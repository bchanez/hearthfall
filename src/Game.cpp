#include "Game.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

#include "Command.hpp"
#include "ScriptEngine.hpp"

namespace game {

namespace {
constexpr float kMoveDeadZone = 0.20f;
constexpr float kAimDeadZone = 0.25f;
constexpr const char* kSavePath = "save/bank.lua";

float axis(SDL_Gamepad* pad, SDL_GamepadAxis a) {
    return SDL_GetGamepadAxis(pad, a) / 32767.0f;
}

// `arrows` includes the arrow keys in movement. It's turned off while the bank
// overlay is open, so the arrows drive the item cursor instead of the player.
Vec2 keyboardMove(bool arrows = true) {
    Vec2 move{};
    const bool* keys = SDL_GetKeyboardState(nullptr);
    if (keys[SDL_SCANCODE_W] || (arrows && keys[SDL_SCANCODE_UP])) move.y -= 1.0f;
    if (keys[SDL_SCANCODE_S] || (arrows && keys[SDL_SCANCODE_DOWN])) move.y += 1.0f;
    if (keys[SDL_SCANCODE_A] || (arrows && keys[SDL_SCANCODE_LEFT])) move.x -= 1.0f;
    if (keys[SDL_SCANCODE_D] || (arrows && keys[SDL_SCANCODE_RIGHT])) move.x += 1.0f;
    return move;
}
}  // namespace

Game::Game(GameContent content, NetConfig net)
    : netCfg_(std::move(net)), sim_(std::move(content)) {}

Game::~Game() {
    for (auto& pp : pads_) {
        if (pp.pad) SDL_CloseGamepad(pp.pad);
    }
    for (auto& seat : seats_) {
        if (seat.pad) SDL_CloseGamepad(seat.pad);
    }
    if (sdlRenderer_) SDL_DestroyRenderer(sdlRenderer_);
    if (window_) SDL_DestroyWindow(window_);
    SDL_Quit();
}

bool Game::init() {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return false;
    }

    const char* title = netCfg_.mode == NetMode::Host     ? "jeu — HOST"
                        : netCfg_.mode == NetMode::Client  ? "jeu — CLIENT"
                                                           : "jeu";
    if (!SDL_CreateWindowAndRenderer(title, kWindowWidth, kWindowHeight, 0, &window_,
                                     &sdlRenderer_)) {
        SDL_Log("SDL_CreateWindowAndRenderer failed: %s", SDL_GetError());
        return false;
    }

    if (netCfg_.mode == NetMode::Client) {
        // One connection for the keyboard seat, plus one per connected gamepad,
        // so a joining PC can also have local co-op.
        addClientSeat(nullptr, 0);  // keyboard
        if (seats_.empty()) return false;  // couldn't reach the host
        int count = 0;
        if (SDL_JoystickID* ids = SDL_GetGamepads(&count)) {
            for (int i = 0; i < count; ++i) {
                if (SDL_Gamepad* pad = SDL_OpenGamepad(ids[i])) addClientSeat(pad, ids[i]);
            }
            SDL_free(ids);
        }
    } else {
        // Local / Host: local players run against our own sim.
        int count = 0;
        if (SDL_JoystickID* ids = SDL_GetGamepads(&count)) {
            for (int i = 0; i < count; ++i) addGamepad(ids[i]);
            SDL_free(ids);
        }
        if (netCfg_.mode == NetMode::Host && !netHost_.start(netCfg_.port)) return false;

        // Restore the persistent shared bank (host/local own the world).
        SaveState save = ScriptEngine::loadState(kSavePath);
        // Always start with a few Health Potions so healing is possible from the
        // very first fight (dropped potions top this up; they bypass the weight cap).
        int pots = 0;
        for (const auto& it : save.items)
            if (it.kind == game::ItemKind::Potion) ++pots;
        for (int i = pots; i < 3; ++i)
            save.items.push_back({"Health Potion", 0.5f, game::ItemKind::Potion, 25});
        sim_.setBank(save.gold, save.items);
        SDL_Log("Bank loaded: %d gold, %zu items.", save.gold, save.items.size());
    }

    renderer_.emplace(sdlRenderer_);
    // Hand the Renderer display strings for the level-up boon chooser, so it can
    // label each offered choice without knowing about GameContent/Lua.
    {
        // Combined choice-label space: passive boons first, then auto-cast abilities
        // — matching the id layout the Simulation offers in Player::boonChoices.
        std::vector<std::string> labels;
        for (const auto& u : sim_.upgrades())
            labels.push_back(u.desc.empty() ? u.name : u.name + " (" + u.desc + ")");
        for (const auto& a : sim_.abilities())
            labels.push_back("* " + a.name + (a.desc.empty() ? "" : " (" + a.desc + ")"));
        renderer_->setUpgradeLabels(std::move(labels));
    }
    // Art intentionally disabled: the world renders as primitives (coloured
    // squares/rings, flat ground) while we rebuild the sprite pipeline from
    // scratch. The Renderer/ScriptEngine sprite plumbing is kept and simply
    // falls back to shapes whenever a sprite is missing — re-enable by feeding
    // it a fresh sprite set here once the new art pass lands.
    running_ = true;
    return true;
}

void Game::saveBank() {
    if (netCfg_.mode == NetMode::Client) return;  // client's bank is remote
    SaveState save;
    save.gold = sim_.world().gold;
    save.items = sim_.world().inventory.items();
    ScriptEngine::saveState(kSavePath, save);
    SDL_Log("Bank saved: %d gold, %zu items.", save.gold, save.items.size());
}

void Game::run() {
    constexpr double kFixedDt = 1.0 / 60.0;

    Uint64 previous = SDL_GetTicks();
    double accumulator = 0.0;

    while (running_) {
        const Uint64 now = SDL_GetTicks();
        double frameTime = static_cast<double>(now - previous) / 1000.0;
        previous = now;
        frameTime = std::min(frameTime, 0.25);

        processEvents();

        if (netCfg_.mode == NetMode::Client) {
            runClientFrame();
            continue;
        }

        // Local / Host: authoritative simulation.
        accumulator += frameTime;
        feedKeyboardCommands();
        feedGamepadCommands();
        if (netCfg_.mode == NetMode::Host) pumpHostNetwork();

        while (accumulator >= kFixedDt) {
            sim_.step(static_cast<float>(kFixedDt));
            accumulator -= kFixedDt;
        }

        if (netCfg_.mode == NetMode::Host)
            netHost_.broadcast(net::MSG_SNAPSHOT, net::encodeSnapshot(sim_.world()));

        // Couch (Local) shares one screen → frame the whole party; Host keeps its
        // own local seat as the desktop follow-cam anchor.
        const CameraMode cam =
            netCfg_.mode == NetMode::Local ? CameraMode::FrameParty : CameraMode::FollowLocal;
        renderer_->draw(sim_.world(), showBank_, kKeyboardPlayer, cam, showChar_,
                        showBank_ ? bankCursor_ : -1);
    }

    saveBank();  // persist on a clean quit (Esc / window close)
}

// --- input -------------------------------------------------------------------

void Game::processEvents() {
    const bool client = netCfg_.mode == NetMode::Client;
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_EVENT_QUIT:
                running_ = false;
                break;
            case SDL_EVENT_KEY_DOWN: {
                const SDL_Keycode k = e.key.key;
                // A pending level-up boon claims the number row first: keys 1/2/3
                // pick the offered choice, overriding class-select / stat-alloc.
                int kbPending = 0;
                int bankSize = 0;
                if (client) {
                    if (!seats_.empty() && seats_[0].client->hasSnapshot()) {
                        const World& w = seats_[0].client->world();
                        const int id = seats_[0].client->myId();
                        if (id >= 0 && id < static_cast<int>(w.players.size()))
                            kbPending = w.players[id].pendingBoons;
                        bankSize = static_cast<int>(w.inventory.size());
                    }
                } else {
                    if (kKeyboardPlayer < static_cast<int>(sim_.world().players.size()))
                        kbPending = sim_.world().players[kKeyboardPlayer].pendingBoons;
                    bankSize = static_cast<int>(sim_.world().inventory.size());
                }
                // Keep the bank cursor in range as items come and go.
                if (bankSize > 0) bankCursor_ = std::clamp(bankCursor_, 0, bankSize - 1);
                else bankCursor_ = 0;

                if (k == SDLK_ESCAPE) {
                    running_ = false;
                } else if (k == SDLK_TAB) {
                    showBank_ = !showBank_;
                } else if (k == SDLK_C) {
                    showChar_ = !showChar_;  // non-pausing character sheet
                } else if (showBank_ && (k == SDLK_UP || k == SDLK_DOWN) && bankSize > 0) {
                    // Bank open: arrows move the item cursor (movement uses WASD only).
                    bankCursor_ = std::clamp(bankCursor_ + (k == SDLK_UP ? -1 : 1), 0, bankSize - 1);
                } else if (showBank_ && (k == SDLK_RETURN || k == SDLK_KP_ENTER) && bankSize > 0) {
                    if (client) pendingEquipItem_ = bankCursor_;
                    else sim_.applyCommand({CommandType::EquipItem, kKeyboardPlayer, {}, bankCursor_});
                } else if (showBank_ && k == SDLK_V && bankSize > 0) {
                    // Sell the selected item for gold (into the shared bank).
                    if (client) pendingSellItem_ = bankCursor_;
                    else sim_.applyCommand({CommandType::SellItem, kKeyboardPlayer, {}, bankCursor_});
                } else if (showBank_ && k >= SDLK_1 && k <= SDLK_9) {
                    // Bank open: a number key equips that listed item onto P1. The
                    // world keeps running, so gearing up is an exposed, co-op moment.
                    const int idx = static_cast<int>(k - SDLK_1);
                    if (client)
                        pendingEquipItem_ = idx;
                    else
                        sim_.applyCommand({CommandType::EquipItem, kKeyboardPlayer, {}, idx});
                } else if (kbPending > 0 && k >= SDLK_1 && k <= SDLK_3) {
                    const int slot = static_cast<int>(k - SDLK_1);
                    if (client)
                        pendingChooseUpgrade_ = slot;
                    else
                        sim_.applyCommand({CommandType::ChooseUpgrade, kKeyboardPlayer, {}, slot});
                } else if (showChar_ && k >= SDLK_1 && k <= SDLK_5) {
                    // Sheet open: number keys spend a banked point into a stat.
                    const int statIndex = static_cast<int>(k - SDLK_1);
                    if (client)
                        pendingAllocStat_ = statIndex;
                    else
                        sim_.applyCommand({CommandType::AllocStat, kKeyboardPlayer, {}, statIndex});
                }
                break;
            }
            case SDL_EVENT_GAMEPAD_ADDED:
                if (client) {
                    if (SDL_Gamepad* pad = SDL_OpenGamepad(e.gdevice.which))
                        addClientSeat(pad, e.gdevice.which);
                } else {
                    addGamepad(e.gdevice.which);
                }
                break;
            case SDL_EVENT_GAMEPAD_REMOVED:
                if (client) {
                    for (std::size_t i = 0; i < seats_.size(); ++i) {
                        if (seats_[i].pad && seats_[i].padId == e.gdevice.which) {
                            SDL_CloseGamepad(seats_[i].pad);
                            seats_.erase(seats_.begin() + i);  // client dtor disconnects
                            break;
                        }
                    }
                } else {
                    removeGamepad(e.gdevice.which);
                }
                break;
            case SDL_EVENT_GAMEPAD_BUTTON_DOWN: {
                if (client) break;
                PadPlayer* pp = findPad(e.gbutton.which);
                if (!pp) break;
                // A pending level-up boon claims the D-pad first: Left/Up/Right pick
                // the offered choice, overriding the sheet cursor and class-select.
                if (pp->playerId < static_cast<int>(sim_.world().players.size()) &&
                    sim_.world().players[pp->playerId].pendingBoons > 0) {
                    int slot = -1;
                    if (e.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_LEFT) slot = 0;
                    else if (e.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_UP) slot = 1;
                    else if (e.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_RIGHT) slot = 2;
                    if (slot >= 0) {
                        sim_.applyCommand({CommandType::ChooseUpgrade, pp->playerId, {}, slot});
                        break;
                    }
                }
                if (showChar_) {
                    // Sheet open: the D-pad drives a per-pad cursor + spends points,
                    // so class-select is suppressed (you're in the menu).
                    if (e.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_UP)
                        pp->statCursor = (pp->statCursor + 4) % 5;
                    else if (e.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_DOWN)
                        pp->statCursor = (pp->statCursor + 1) % 5;
                    else if (e.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_RIGHT)
                        sim_.applyCommand({CommandType::AllocStat, pp->playerId, {}, pp->statCursor});
                    break;
                }
                break;
            }
            default:
                break;
        }
    }
}

void Game::feedKeyboardCommands() {
    // While the bank is open the arrow keys drive the item cursor, not the player.
    const Vec2 move = keyboardMove(!showBank_);
    sim_.applyCommand({CommandType::Move, kKeyboardPlayer, move, 0});

    const Vec2 aim = mouseAim(sim_.world().players[kKeyboardPlayer].entity.position);
    const bool* keys = SDL_GetKeyboardState(nullptr);
    if (keys[SDL_SCANCODE_SPACE]) sim_.applyCommand({CommandType::Attack, kKeyboardPlayer, aim, 0});
    if (keys[SDL_SCANCODE_E]) sim_.applyCommand({CommandType::Ability, kKeyboardPlayer, aim, 0});
    if (keys[SDL_SCANCODE_F]) sim_.applyCommand({CommandType::Equip, kKeyboardPlayer, {}, 0});
    if (keys[SDL_SCANCODE_G]) sim_.applyCommand({CommandType::Unequip, kKeyboardPlayer, {}, 0});
    if (keys[SDL_SCANCODE_Q]) sim_.applyCommand({CommandType::UsePotion, kKeyboardPlayer, {}, 0});
    if (keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT])
        sim_.applyCommand({CommandType::Dash, kKeyboardPlayer, move, 0});
}

void Game::feedGamepadCommands() {
    for (auto& pp : pads_) {
        SDL_Gamepad* pad = pp.pad;
        Vec2 move{axis(pad, SDL_GAMEPAD_AXIS_LEFTX), axis(pad, SDL_GAMEPAD_AXIS_LEFTY)};
        if (std::abs(move.x) < kMoveDeadZone) move.x = 0.0f;
        if (std::abs(move.y) < kMoveDeadZone) move.y = 0.0f;
        sim_.applyCommand({CommandType::Move, pp.playerId, move, 0});

        const Vec2 aim = stickAim(pad, sim_.world().players[pp.playerId].aim);
        if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_SOUTH))
            sim_.applyCommand({CommandType::Attack, pp.playerId, aim, 0});
        if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_WEST))
            sim_.applyCommand({CommandType::Ability, pp.playerId, aim, 0});
        if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_NORTH))
            sim_.applyCommand({CommandType::Equip, pp.playerId, {}, 0});
        if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_EAST))
            sim_.applyCommand({CommandType::Unequip, pp.playerId, {}, 0});
        if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER))
            sim_.applyCommand({CommandType::UsePotion, pp.playerId, {}, 0});
        if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER))
            sim_.applyCommand({CommandType::Dash, pp.playerId, move, 0});
    }
}

// --- client ------------------------------------------------------------------

void Game::addClientSeat(SDL_Gamepad* pad, SDL_JoystickID padId) {
    auto client = std::make_unique<net::Client>();
    if (!client->connect(netCfg_.host, netCfg_.port)) {
        if (pad) SDL_CloseGamepad(pad);
        return;
    }
    seats_.push_back({std::move(client), pad, padId});
    SDL_Log("Local seat joined (%s).", pad ? "gamepad" : "keyboard");
}

net::InputState Game::collectSeatInput(const ClientSeat& seat) {
    net::InputState in;
    const net::Client& c = *seat.client;

    Vec2 pos{kWindowWidth / 2.0f, kWindowHeight / 2.0f};
    Vec2 lastAim{1.0f, 0.0f};
    bool pendingBoon = false;
    if (c.hasSnapshot() && c.myId() >= 0 && c.myId() < static_cast<int>(c.world().players.size())) {
        pos = c.world().players[c.myId()].entity.position;
        lastAim = c.world().players[c.myId()].aim;
        pendingBoon = c.world().players[c.myId()].pendingBoons > 0;
    }

    if (seat.pad) {
        SDL_Gamepad* pad = seat.pad;
        Vec2 move{axis(pad, SDL_GAMEPAD_AXIS_LEFTX), axis(pad, SDL_GAMEPAD_AXIS_LEFTY)};
        if (std::abs(move.x) < kMoveDeadZone) move.x = 0.0f;
        if (std::abs(move.y) < kMoveDeadZone) move.y = 0.0f;
        in.move = move;
        in.aim = stickAim(pad, lastAim);
        in.attack = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_SOUTH);
        in.ability = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_WEST);
        in.equip = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_NORTH);
        in.unequip = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_EAST);
        in.usePotion = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
        in.dash = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
        // A pending boon claims the D-pad for the choice; otherwise it selects class.
        if (pendingBoon) {
            if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_LEFT)) in.chooseUpgrade = 0;
            else if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_UP)) in.chooseUpgrade = 1;
            else if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) in.chooseUpgrade = 2;
        } else {
            if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_LEFT)) in.classSelect = 0;
            else if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_UP)) in.classSelect = 1;
            else if (SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT)) in.classSelect = 2;
        }
    } else {
        const bool* keys = SDL_GetKeyboardState(nullptr);
        in.move = keyboardMove(!showBank_);  // arrows drive the bank cursor while Tab is open
        in.aim = mouseAim(pos);
        in.attack = keys[SDL_SCANCODE_SPACE];
        in.ability = keys[SDL_SCANCODE_E];
        in.equip = keys[SDL_SCANCODE_F];
        in.unequip = keys[SDL_SCANCODE_G];
        in.usePotion = keys[SDL_SCANCODE_Q];
        in.dash = keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT];
        in.classSelect = pendingClassSelect_;
        in.allocStat = pendingAllocStat_;          // set by the C-sheet number keys
        in.chooseUpgrade = pendingChooseUpgrade_;  // set by the boon-chooser number keys
        in.equipItem = pendingEquipItem_;          // set by the bank-overlay equip keys
        in.sellItem = pendingSellItem_;            // set by the bank-overlay sell key (V)
    }
    return in;
}

void Game::runClientFrame() {
    const Uint64 now = SDL_GetTicks();

    for (auto& seat : seats_) {
        seat.client->tryReconnect(now);  // no-op while connected
        if (seat.client->connected()) seat.client->sendInput(collectSeatInput(seat));
        seat.client->poll(now);
    }
    pendingClassSelect_ = -1;
    pendingAllocStat_ = -1;
    pendingChooseUpgrade_ = -1;
    pendingEquipItem_ = -1;
    pendingSellItem_ = -1;

    // Render an interpolated view slightly in the past for smooth motion.
    constexpr Uint64 kInterpDelayMs = 100;
    const Uint64 renderMs = now > kInterpDelayMs ? now - kInterpDelayMs : 0;

    if (!seats_.empty() && seats_[0].client->hasSnapshot()) {
        renderer_->draw(seats_[0].client->view(renderMs), showBank_, seats_[0].client->myId(),
                        CameraMode::FollowLocal, showChar_, showBank_ ? bankCursor_ : -1);
    } else {
        World empty;
        renderer_->draw(empty, showBank_, 0, CameraMode::FollowLocal, showChar_);
    }
    // Seats auto-reconnect, so we no longer quit on a dropped link — only Esc
    // quits. This survives a host restart or a brief network blip.
}

// --- host --------------------------------------------------------------------

void Game::pumpHostNetwork() {
    for (const auto& ev : netHost_.poll()) {
        switch (ev.type) {
            case net::Host::Event::Join: {
                const int pid = sim_.addPlayer(0);
                connToPlayer_[ev.conn] = pid;
                netHost_.send(ev.conn, net::MSG_WELCOME, net::encodeWelcome(pid));
                SDL_Log("Player %d joined over the network.", pid + 1);
                break;
            }
            case net::Host::Event::Leave: {
                auto it = connToPlayer_.find(ev.conn);
                if (it != connToPlayer_.end()) {
                    sim_.setPlayerActive(it->second, false);
                    connToPlayer_.erase(it);
                }
                break;
            }
            case net::Host::Event::Input: {
                auto it = connToPlayer_.find(ev.conn);
                if (it != connToPlayer_.end()) applyRemoteInput(it->second, ev.input);
                break;
            }
        }
    }
}

void Game::applyRemoteInput(int playerId, const net::InputState& in) {
    sim_.applyCommand({CommandType::Move, playerId, in.move, 0});
    if (in.attack) sim_.applyCommand({CommandType::Attack, playerId, in.aim, 0});
    if (in.ability) sim_.applyCommand({CommandType::Ability, playerId, in.aim, 0});
    if (in.equip) sim_.applyCommand({CommandType::Equip, playerId, {}, 0});
    if (in.unequip) sim_.applyCommand({CommandType::Unequip, playerId, {}, 0});
    if (in.usePotion) sim_.applyCommand({CommandType::UsePotion, playerId, {}, 0});
    if (in.dash) sim_.applyCommand({CommandType::Dash, playerId, in.move, 0});
    if (in.classSelect >= 0)
        sim_.applyCommand({CommandType::SelectClass, playerId, {}, in.classSelect});
    if (in.allocStat >= 0)
        sim_.applyCommand({CommandType::AllocStat, playerId, {}, in.allocStat});
    if (in.chooseUpgrade >= 0)
        sim_.applyCommand({CommandType::ChooseUpgrade, playerId, {}, in.chooseUpgrade});
    if (in.equipItem >= 0)
        sim_.applyCommand({CommandType::EquipItem, playerId, {}, in.equipItem});
    if (in.sellItem >= 0)
        sim_.applyCommand({CommandType::SellItem, playerId, {}, in.sellItem});
}

// --- gamepad bookkeeping -----------------------------------------------------

Game::PadPlayer* Game::findPad(SDL_JoystickID id) {
    for (auto& pp : pads_) {
        if (pp.id == id) return &pp;
    }
    return nullptr;
}

void Game::addGamepad(SDL_JoystickID id) {
    if (findPad(id)) return;
    SDL_Gamepad* pad = SDL_OpenGamepad(id);
    if (!pad) return;
    const int playerId = sim_.addPlayer(0);  // gamepad players default to Tank
    pads_.push_back({pad, id, playerId});
    SDL_Log("Player %d joined (gamepad: %s)", playerId + 1, SDL_GetGamepadName(pad));
}

void Game::removeGamepad(SDL_JoystickID id) {
    PadPlayer* pp = findPad(id);
    if (!pp) return;
    sim_.setPlayerActive(pp->playerId, false);
    if (pp->pad) SDL_CloseGamepad(pp->pad);
    pads_.erase(pads_.begin() + (pp - pads_.data()));
}

// --- aim helpers -------------------------------------------------------------

Vec2 Game::mouseAim(const Vec2& fromPos) const {
    float mx = 0.0f, my = 0.0f;
    SDL_GetMouseState(&mx, &my);
    // The cursor is in screen space but the player lives in world space; map the
    // mouse back through the camera before aiming.
    const Vec2 worldMouse =
        renderer_ ? renderer_->screenToWorld({mx, my}) : Vec2{mx, my};
    const Vec2 toMouse = worldMouse - fromPos;
    if (toMouse.length() > 1.0f) return toMouse.normalized();
    return {1.0f, 0.0f};
}

Vec2 Game::stickAim(SDL_Gamepad* pad, const Vec2& fallback) {
    const Vec2 stick{axis(pad, SDL_GAMEPAD_AXIS_RIGHTX), axis(pad, SDL_GAMEPAD_AXIS_RIGHTY)};
    if (stick.length() > kAimDeadZone) return stick.normalized();
    return fallback;
}

}  // namespace game
