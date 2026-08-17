#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "GameContent.hpp"
#include "Net.hpp"
#include "Renderer.hpp"
#include "Simulation.hpp"

namespace game {

enum class NetMode { Local, Host, Client };

struct NetConfig {
    NetMode mode = NetMode::Local;
    std::string host = "127.0.0.1";
    uint16_t port = 7777;
};

// The application host: owns the window, the SDL renderer and the gamepads, and
// runs the main loop. Turns OS input into Commands, drives the Simulation at a
// fixed timestep, and renders.
//
// Netmodes:
//  - Local : single machine, keyboard + gamepads (as before).
//  - Host  : same as Local, but also accepts network clients (authoritative).
//  - Client: renders snapshots from a host and sends its input; no local sim.
class Game {
public:
    Game(GameContent content, NetConfig net);
    ~Game();

    Game(const Game&) = delete;
    Game& operator=(const Game&) = delete;

    bool init();
    void run();

private:
    struct PadPlayer {
        SDL_Gamepad* pad = nullptr;
        SDL_JoystickID id = 0;
        int playerId = 0;
        int statCursor = 0;  // highlighted stat in the character sheet (0..4)
    };

    // A local player on a client machine: one network connection per input
    // source (keyboard, or a gamepad). This lets a joining PC also have local
    // co-op — each seat becomes its own player on the host.
    struct ClientSeat {
        std::unique_ptr<net::Client> client;
        SDL_Gamepad* pad = nullptr;  // null == keyboard + mouse
        SDL_JoystickID padId = 0;
    };

    void processEvents();
    void feedKeyboardCommands();
    void feedGamepadCommands();

    // Client mode.
    net::InputState collectSeatInput(const ClientSeat& seat);
    void addClientSeat(SDL_Gamepad* pad, SDL_JoystickID padId);  // pad null = keyboard
    void runClientFrame();
    // Host mode.
    void pumpHostNetwork();
    void applyRemoteInput(int playerId, const net::InputState& in);

    void saveBank();  // persist the shared bank (host/local)

    void addGamepad(SDL_JoystickID id);
    void removeGamepad(SDL_JoystickID id);
    PadPlayer* findPad(SDL_JoystickID id);

    Vec2 mouseAim(const Vec2& fromPos) const;
    static Vec2 stickAim(SDL_Gamepad* pad, const Vec2& fallback);

    SDL_Window* window_ = nullptr;
    SDL_Renderer* sdlRenderer_ = nullptr;
    std::vector<PadPlayer> pads_;

    bool running_ = false;
    bool showBank_ = false;
    bool showChar_ = false;         // character sheet overlay (non-pausing)
    int pendingClassSelect_ = -1;  // client: class key pressed this frame
    int pendingAllocStat_ = -1;    // client: stat point spent this frame (keyboard seat)

    NetConfig netCfg_;
    net::Host netHost_;
    std::vector<ClientSeat> seats_;              // client: one per local input source
    std::unordered_map<int, int> connToPlayer_;  // host: connection fd -> playerId

    Simulation sim_;
    std::optional<Renderer> renderer_;

    static constexpr int kKeyboardPlayer = 0;
    static constexpr int kWindowWidth = 1280;
    static constexpr int kWindowHeight = 720;
};

}  // namespace game
