#include "Renderer.hpp"

#include "PlayerStats.hpp"  // xpForLevel (character-sheet XP bar)

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace game {

namespace {

struct Color {
    Uint8 r, g, b;
};

// One colour per enemy archetype, so a wave reads at a glance.
Color enemyColor(EnemyType type) {
    switch (type) {
        case EnemyType::Swarmer:  return {230, 120, 90};   // small & fast: orange
        case EnemyType::Grunt:    return {200, 60, 60};     // baseline: red
        case EnemyType::Brute:    return {150, 45, 95};     // heavy: purple-red
        case EnemyType::Boss:     return {245, 60, 130};    // menace: magenta
        case EnemyType::Slime:    return {110, 200, 120};   // gooey green
        case EnemyType::Archer:   return {200, 170, 80};    // ranged: tan
        case EnemyType::Spitter:  return {150, 200, 60};    // venom: sickly yellow-green
        case EnemyType::Sorcerer: return {150, 100, 220};   // caster: violet
    }
    return {200, 60, 60};
}

// One colour per player slot, so couch players can tell themselves apart.
Color playerColor(int index) {
    static const Color palette[] = {
        {80, 200, 100},   // P1 green
        {80, 180, 220},   // P2 cyan
        {230, 150, 70},   // P3 orange
        {180, 120, 220},  // P4 purple
    };
    return palette[index % 4];
}

void setColor(SDL_Renderer* r, Color c) { SDL_SetRenderDrawColor(r, c.r, c.g, c.b, 255); }
void setColor(SDL_Renderer* r, Uint8 red, Uint8 green, Uint8 blue) {
    SDL_SetRenderDrawColor(r, red, green, blue, 255);
}

void fillSquare(SDL_Renderer* r, const Vec2& c, float half) {
    SDL_FRect rect{c.x - half, c.y - half, half * 2.0f, half * 2.0f};
    SDL_RenderFillRect(r, &rect);
}

void drawRing(SDL_Renderer* r, const Vec2& c, float radius) {
    SDL_FRect ring{c.x - radius, c.y - radius, radius * 2.0f, radius * 2.0f};
    SDL_RenderRect(r, &ring);
}

void drawHpBar(SDL_Renderer* r, const Vec2& c, float halfWidth, float yOffset, int hp, int maxHp,
               Color fill) {
    const float w = halfWidth * 2.0f;
    const float frac = maxHp > 0 ? static_cast<float>(hp) / static_cast<float>(maxHp) : 0.0f;
    SDL_FRect bg{c.x - halfWidth, c.y - halfWidth - yOffset, w, 4.0f};
    setColor(r, 60, 60, 60);
    SDL_RenderFillRect(r, &bg);
    SDL_FRect fg{bg.x, bg.y, w * frac, 4.0f};
    setColor(r, fill);
    SDL_RenderFillRect(r, &fg);
}

Color rarityColor(Rarity r) {
    switch (r) {
        case Rarity::Common:   return {200, 200, 210};  // grey
        case Rarity::Uncommon: return {90, 210, 90};    // green
        case Rarity::Rare:     return {80, 150, 240};    // blue
        case Rarity::Epic:     return {190, 110, 240};   // purple
    }
    return {200, 200, 210};
}

// Ground colour: gear shows its rarity; gold/potions keep a fixed colour.
Color lootColor(const Item& item) {
    switch (item.kind) {
        case ItemKind::Gold:   return {240, 200, 60};
        case ItemKind::Potion: return {220, 60, 160};
        case ItemKind::Weapon:
        case ItemKind::Armor:  return rarityColor(item.rarity);
    }
    return {255, 255, 255};
}

// Ground sprite key for a drop — must match names in data/tiles.lua. Weapons
// reuse the in-hand art (wpn_sword / wpn_bow) so a bow reads as a bow on the
// floor too. A few named items get their own look via the override below.
const char* lootSprite(const Item& item) {
    if (item.name == "Ancient Relic") return "loot_relic";
    switch (item.kind) {
        case ItemKind::Gold:   return "loot_gold";
        case ItemKind::Potion: return "loot_potion";
        case ItemKind::Armor:  return "loot_armor";
        case ItemKind::Weapon:
            return item.style == AttackStyle::Ranged ? "wpn_bow" : "wpn_sword";
    }
    return "loot_gold";
}

const char* className(ClassId) {
    return "Adventurer";  // classless: everyone's a neutral adventurer, title comes from play
}

// Sprite keys — must match the `name` fields in data/sprites.lua.
const char* enemySprite(EnemyType t) {
    switch (t) {
        case EnemyType::Swarmer:  return "swarmer";
        case EnemyType::Grunt:    return "grunt";
        case EnemyType::Brute:    return "brute";
        case EnemyType::Boss:     return "boss";
        case EnemyType::Slime:    return "slime";
        case EnemyType::Archer:   return "enemy_archer";
        case EnemyType::Spitter:  return "spitter";
        case EnemyType::Sorcerer: return "sorcerer";
    }
    return "grunt";
}

// Sprites are drawn a touch larger than the collision circle so they read.
constexpr float kSpriteScale = 1.7f;

}  // namespace

Renderer::~Renderer() {
    for (auto& [name, s] : sprites_) SDL_DestroyTexture(s.tex);
}

void Renderer::loadSprites(const std::vector<SpritePixels>& defs) {
    for (const auto& d : defs) {
        if (d.width <= 0 || d.height <= 0 || d.rgba.empty()) continue;
        SDL_Texture* tex =
            SDL_CreateTexture(sdl_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, d.width, d.height);
        if (!tex) continue;
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(tex, SDL_SCALEMODE_NEAREST);  // crisp pixels when scaled up
        SDL_UpdateTexture(tex, nullptr, d.rgba.data(), d.width * 4);
        const BakedSprite baked{tex, d.width, d.height};
        sprites_[d.name] = baked;

        // Group animated frames named "base.state.frame" into anims_.
        const auto p1 = d.name.find('.');
        const auto p2 = d.name.rfind('.');
        if (p1 != std::string::npos && p2 != std::string::npos && p2 > p1) {
            const std::string base = d.name.substr(0, p1);
            const std::string state = d.name.substr(p1 + 1, p2 - p1 - 1);
            const int frame = std::atoi(d.name.c_str() + p2 + 1);
            CharAnim& ca = anims_[base];
            std::vector<BakedSprite>* v =
                state == "walk" ? &ca.walk : state == "attack" ? &ca.attack : &ca.idle;
            if (static_cast<int>(v->size()) <= frame) v->resize(frame + 1);
            (*v)[frame] = baked;
        }
    }
}

namespace {
// A cheap, stable per-cell hash so tile choice + prop scatter are deterministic
// (same everywhere, every frame) without storing a map. Mixes two coords.
std::uint32_t hashCell(int x, int y) {
    std::uint32_t h = static_cast<std::uint32_t>(x) * 73856093u ^ static_cast<std::uint32_t>(y) * 19349663u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    h ^= h >> 15;
    return h;
}
constexpr int kTileSize = 64;  // world pixels per ground tile
}  // namespace

void Renderer::drawGround(const World& world, const Vec2& cam, int screenW, int screenH) {
    if (sprites_.find("grass") == sprites_.end()) return;  // tiles not loaded → keep the flat clear
    if (world.width <= 0 || world.height <= 0) return;

    const Vec2 center{world.width / 2.0f, world.height / 2.0f};
    const char* ground[4] = {"grass", "dirt", "rock", "stone"};

    // Only the tiles under the camera window (+1 cell of margin for shake).
    const int tx0 = std::max(0, static_cast<int>(std::floor(cam.x / kTileSize)) - 1);
    const int ty0 = std::max(0, static_cast<int>(std::floor(cam.y / kTileSize)) - 1);
    const int tx1 = std::min(world.width / kTileSize,
                             static_cast<int>(std::floor((cam.x + screenW) / kTileSize)) + 1);
    const int ty1 = std::min(world.height / kTileSize,
                             static_cast<int>(std::floor((cam.y + screenH) / kTileSize)) + 1);

    const RingRadii ring = ringRadii(world);
    auto bandFor = [&](int tx, int ty, std::uint32_t h) {
        const float wx = tx * kTileSize + kTileSize * 0.5f;
        const float wy = ty * kTileSize + kTileSize * 0.5f;
        const float d = (Vec2{wx, wy} - center).length();
        int b = d < ring.r0 ? 0 : d < ring.r1 ? 1 : d < ring.r2 ? 2 : 3;
        if ((h % 6u) == 0u && b < 3) ++b;  // ragged, eroded band edges
        return b;
    };

    // Pass 1: the ground carpet.
    for (int ty = ty0; ty < ty1; ++ty) {
        for (int tx = tx0; tx < tx1; ++tx) {
            const std::uint32_t h = hashCell(tx, ty);
            const int b = bandFor(tx, ty, h);
            const Vec2 c{tx * kTileSize + kTileSize * 0.5f, ty * kTileSize + kTileSize * 0.5f};
            drawSprite(ground[b], c, kTileSize * 0.5f, (h & 1u) != 0u);  // random mirror = less repeat
        }
    }

    // Pass 1.5: overlay layer — the MIDDLE layer, blended over the base carpet.
    // (a) Directional edge transitions: where a cell touches a HARDER material,
    //     that neighbour's fringe (a torn, top-anchored band) is rotated to face
    //     it and drawn across the shared border — so a straight ring boundary
    //     tears into an organic, interlocking edge instead of a hard grid line.
    // (b) Material detail (moss/mud/water/rubble/corruption): sparse and
    //     block-sized so a patch spans several tiles instead of one puddle per
    //     cell.
    auto bandAt = [&](int gx, int gy) { return bandFor(gx, gy, hashCell(gx, gy)); };
    const char* fringeFor[4] = {nullptr, "fringe_dirt", "fringe_rock", "fringe_stone"};
    const int dirDx[4] = {0, 1, 0, -1};
    const int dirDy[4] = {-1, 0, 1, 0};
    const double dirAng[4] = {0.0, 90.0, 180.0, 270.0};  // fringe authored facing North
    auto drawRot = [&](const char* name, const Vec2& p, float half, double ang, bool flip) {
        auto it = sprites_.find(name);
        if (it != sprites_.end()) drawBaked(it->second, p, half, flip, 255, ang, SDL_BLENDMODE_BLEND);
    };
    // (a) edge transitions
    for (int ty = ty0; ty < ty1; ++ty) {
        for (int tx = tx0; tx < tx1; ++tx) {
            const int b = bandAt(tx, ty);
            const Vec2 c{tx * kTileSize + kTileSize * 0.5f, ty * kTileSize + kTileSize * 0.5f};
            const bool flip = (hashCell(tx, ty) & 8u) != 0u;  // mirror the tear so long borders don't repeat
            for (int dir = 0; dir < 4; ++dir) {
                const int nb = bandAt(tx + dirDx[dir], ty + dirDy[dir]);
                if (nb > b) drawRot(fringeFor[nb], c, kTileSize * 0.5f, dirAng[dir], flip);
            }
        }
    }
    // (b) block-sized detail patches
    const char* detail[4][3] = {
        {"overlay_moss", "overlay_water", "overlay_leaves"},
        {"overlay_mud", "overlay_leaves", "overlay_rubble"},
        {"overlay_moss", "overlay_water", "overlay_rubble"},
        {"overlay_corrupt", "overlay_cracks", "overlay_rubble"},
    };
    for (int ty = ty0; ty < ty1; ++ty) {
        for (int tx = tx0; tx < tx1; ++tx) {
            if ((tx & 1) || (ty & 1)) continue;  // one patch per 2x2 block, anchored on its corner cell
            const std::uint32_t hb = hashCell(tx >> 1, ty >> 1);
            if (hb % 3u != 0u) continue;  // ~1 in 3 blocks
            const int bb = bandAt(tx, ty);
            const char* ov = detail[bb][(hb >> 6) % 3u];
            const Vec2 bc{tx * kTileSize + static_cast<float>(kTileSize),
                          ty * kTileSize + static_cast<float>(kTileSize)};  // block centre
            if (std::strcmp(ov, "overlay_corrupt") == 0)
                drawGlow(bc, kTileSize * 0.7f, 150, 40, 180, 12);  // faint violet bloom
            drawSprite(ov, bc, kTileSize * 0.85f, (hb & 4u) != 0u);  // larger than a cell → spans the seam
        }
    }

    // Pass 2: props on top, so a neighbour's ground never clips them. Sparse.
    for (int ty = ty0; ty < ty1; ++ty) {
        for (int tx = tx0; tx < tx1; ++tx) {
            const std::uint32_t h = hashCell(tx, ty);
            if ((h >> 8) % 9u != 0u) continue;  // ~1 in 9 cells gets a prop
            const int b = bandFor(tx, ty, h);
            const char* prop;
            if (b == 0) {  // safe centre: glow-shrooms, bones, dead scrub
                const unsigned pick = (h >> 16) % 4u;
                prop = pick == 0u ? "prop_flower" : pick == 1u ? "prop_mushroom" : "prop_bush";
            } else if (((h >> 12) & 7u) == 0u) {
                prop = "prop_torch";  // ~1 in 8 outer props is a lit torch
            } else {
                prop = b >= 2 ? "prop_rock" : "prop_bush";  // outer: rubble and dead scrub
            }
            // Jitter within the cell so props don't sit on a perfect grid.
            const float jx = static_cast<float>((h >> 20) % kTileSize) - kTileSize * 0.5f;
            const float jy = static_cast<float>((h >> 26) % kTileSize) - kTileSize * 0.5f;
            const Vec2 c{tx * kTileSize + kTileSize * 0.5f + jx * 0.5f,
                         ty * kTileSize + kTileSize * 0.5f + jy * 0.5f};
            // Light pools first (under the prop): warm for torches, cold for the
            // magic mushrooms. A slow flicker keeps torchlight alive.
            if (std::strcmp(prop, "prop_torch") == 0) {
                const float flick = 1.0f + 0.12f * std::sin((SDL_GetTicks() + tx * 90 + ty * 40) * 0.006f);
                drawGlow({c.x, c.y - kTileSize * 0.15f}, kTileSize * 1.15f * flick, 255, 150, 60, 22);
            } else if (std::strcmp(prop, "prop_flower") == 0) {
                drawGlow(c, kTileSize * 0.55f, 70, 210, 220, 16);
            }
            drawSprite(prop, c, kTileSize * 0.28f, (h & 2u) != 0u);
        }
    }
}

void Renderer::drawBaked(const BakedSprite& s, const Vec2& center, float half, bool flipX,
                         Uint8 alpha, double angleDeg, SDL_BlendMode blend, float squashX,
                         float squashY) {
    const float scale = (2.0f * half) / static_cast<float>(std::max(s.w, s.h));
    const float dw = s.w * scale, dh = s.h * scale;
    const float dw2 = dw * squashX, dh2 = dh * squashY;
    // Anchor at the feet (bottom centre) so squash-&-stretch stays planted. With
    // the default 1,1 squash this reduces exactly to the old centre anchor.
    SDL_FRect dst{center.x - dw2 * 0.5f, center.y + dh * 0.5f - dh2, dw2, dh2};
    SDL_SetTextureBlendMode(s.tex, blend);
    SDL_SetTextureAlphaMod(s.tex, alpha);
    SDL_RenderTextureRotated(sdl_, s.tex, nullptr, &dst, angleDeg, nullptr,
                             flipX ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
    if (blend != SDL_BLENDMODE_BLEND) SDL_SetTextureBlendMode(s.tex, SDL_BLENDMODE_BLEND);
}

const Renderer::BakedSprite* Renderer::effectFrame(const std::string& base) {
    auto it = anims_.find(base);
    if (it == anims_.end() || it->second.idle.empty()) return nullptr;
    const auto& frames = it->second.idle;
    // Flicker fast (~90ms) so a bolt shimmers in flight instead of looking pasted.
    const int n = static_cast<int>(frames.size());
    const int f = n > 1 ? static_cast<int>((SDL_GetTicks() / 90) % n) : 0;
    return &frames[f];
}

void Renderer::drawProjectiles(const World& world) {
    const BakedSprite* fr = effectFrame("bolt");
    for (const auto& bolt : world.projectiles) {
        if (!bolt.alive) continue;
        if (!fr) {  // no effect art loaded → the old solid square
            setColor(sdl_, 250, 240, 150);
            fillSquare(sdl_, bolt.position, bolt.radius);
            continue;
        }
        // Face travel; atan2 in SDL's y-down screen space already matches world y.
        const double ang = std::atan2(bolt.velocity.y, bolt.velocity.x) * 180.0 / M_PI;
        const Vec2 dir = bolt.velocity.normalized();
        const float half = bolt.radius * 1.6f;  // art reads a touch bigger than the hitbox

        // 1) Halo: a big, dim, additive copy — the light the bolt throws.
        drawBaked(*fr, bolt.position, half * 2.2f, false, 90, ang, SDL_BLENDMODE_ADD);
        // 2) Streak: shrinking additive copies trailing back up the velocity.
        for (int i = 1; i <= 3; ++i) {
            const Vec2 p = bolt.position - dir * (half * 0.7f * i);
            const Uint8 a = static_cast<Uint8>(120 - i * 30);
            drawBaked(*fr, p, half * (1.0f - 0.18f * i), false, a, ang, SDL_BLENDMODE_ADD);
        }
        // 3) Hot core on top, crisp.
        drawBaked(*fr, bolt.position, half, false, 255, ang, SDL_BLENDMODE_BLEND);
    }
}

bool Renderer::drawSprite(const char* name, const Vec2& center, float half, bool flipX, Uint8 alpha) {
    auto it = sprites_.find(name);
    if (it == sprites_.end()) return false;
    drawBaked(it->second, center, half, flipX, alpha);
    return true;
}

bool Renderer::drawCharacter(const std::string& base, Motion motion, int phase, const Vec2& center,
                             float half, bool flipX, Uint8 alpha) {
    auto it = anims_.find(base);
    if (it == anims_.end()) return drawSprite(base.c_str(), center, half, flipX, alpha);
    const CharAnim& a = it->second;

    // Pick the state's frame list, falling back toward idle when a state is empty.
    const std::vector<BakedSprite>* v = nullptr;
    if (motion == Motion::Attack && !a.attack.empty()) v = &a.attack;
    else if (motion == Motion::Walk && !a.walk.empty()) v = &a.walk;
    if (!v && !a.idle.empty()) v = &a.idle;
    if (!v && !a.walk.empty()) v = &a.walk;
    if (!v || v->empty()) return false;

    const int n = static_cast<int>(v->size());
    int frame = 0;
    if (n > 1) {
        // Cycle on a wall-clock (ms) so speed is independent of framerate, and
        // slow enough that each pose reads clearly to the eye: a walk step every
        // ~240ms, a calm ~800ms idle breath, a snappy attack. The phase offset
        // keeps a crowd from animating in perfect unison.
        const std::uint64_t ms = SDL_GetTicks();
        if (motion == Motion::Walk) frame = static_cast<int>((ms / 240 + phase) % n);
        else if (motion == Motion::Attack) frame = static_cast<int>((ms / 90) % n);
        else frame = static_cast<int>((ms / 800 + phase) % n);
    }

    // Procedural "juice": the DCSS art is a single static frame, so we fake life
    // with time-based squash-&-stretch + a bob, anchored at the feet. The `phase`
    // offset keeps a crowd from pulsing in unison.
    const float t = SDL_GetTicks() / 1000.0f + static_cast<float>(phase) * 0.31f;
    float bobY = 0.0f, sx = 1.0f, sy = 1.0f, lean = 0.0f;
    if (motion == Motion::Walk) {
        const float s = std::sin(t * 15.0f);       // ~2.4 steps/sec
        const float hop = std::fabs(s);
        bobY = -half * 0.14f * hop;                 // spring up off the ground
        sy = 1.0f + 0.10f * hop;                    // stretch tall mid-hop
        sx = 1.0f - 0.08f * hop;                    // pinch narrow
        lean = 5.0f * s;                            // rock side to side
    } else if (motion == Motion::Attack) {
        const float env = std::fabs(std::sin(SDL_GetTicks() / 70.0f));
        sx = 1.0f + 0.20f * env;                    // snappy anticipation pop
        sy = 1.0f + 0.14f * env;
        bobY = -half * 0.05f * env;
    } else {
        const float b = std::sin(t * 2.4f);         // calm idle breathing
        sy = 1.0f + 0.035f * b;
        sx = 1.0f - 0.025f * b;
        bobY = -half * 0.02f * (b * 0.5f + 0.5f);
    }
    drawBaked((*v)[frame], {center.x, center.y + bobY}, half, flipX, alpha,
              static_cast<double>(flipX ? -lean : lean), SDL_BLENDMODE_BLEND, sx, sy);
    return true;
}

void Renderer::drawShadow(const Vec2& feet, float radius) {
    // Fake a soft ellipse with a few stacked translucent bars — grounds the
    // sprite without needing a texture or per-pixel blending.
    SDL_SetRenderDrawBlendMode(sdl_, SDL_BLENDMODE_BLEND);
    for (int i = 0; i < 3; ++i) {
        const float w = radius * (1.0f - static_cast<float>(i) * 0.26f);
        SDL_SetRenderDrawColor(sdl_, 0, 0, 0, static_cast<Uint8>(70 - i * 12));
        SDL_FRect r{feet.x - w, feet.y - static_cast<float>(i) * 1.5f, w * 2.0f, 3.0f};
        SDL_RenderFillRect(sdl_, &r);
    }
    SDL_SetRenderDrawBlendMode(sdl_, SDL_BLENDMODE_NONE);
}

void Renderer::drawGlow(const Vec2& center, float radius, Uint8 r, Uint8 g, Uint8 b,
                        Uint8 perLayer) {
    SDL_SetRenderDrawBlendMode(sdl_, SDL_BLENDMODE_ADD);
    SDL_SetRenderDrawColor(sdl_, r, g, b, perLayer);
    const int layers = 6;
    for (int i = layers; i >= 1; --i) {  // largest first; centre overlaps most → brightest
        const float rad = radius * static_cast<float>(i) / layers;
        SDL_FRect rect{center.x - rad, center.y - rad, rad * 2.0f, rad * 2.0f};
        SDL_RenderFillRect(sdl_, &rect);
    }
    SDL_SetRenderDrawBlendMode(sdl_, SDL_BLENDMODE_NONE);
}

void Renderer::draw(const World& world, bool showBank, int followPlayer, CameraMode mode,
                    bool showChar, int bankCursor) {
    ++frameCounter_;  // advances walk/idle animation cycles
    SDL_SetRenderViewport(sdl_, nullptr);
    setColor(sdl_, 16, 17, 27);  // deep cool void, matches the graded shadows
    SDL_RenderClear(sdl_);

    int screenW = world.width, screenH = world.height;
    SDL_GetCurrentRenderOutputSize(sdl_, &screenW, &screenH);

    // Shake the *world* (not the HUD) by offsetting the viewport a few pixels.
    float offX = 0.0f, offY = 0.0f;
    updateShake(world, offX, offY);

    // Camera: place the viewport origin at -cam so world-space draws land where
    // the camera looks. The viewport keeps the world's full size so nothing is
    // clipped early; only the screen-sized window is visible. This also carries
    // the screen-shake offset, exactly as the fixed-origin version used to.
    // Zoom the world so characters/tiles read bigger. Everything drawn from here
    // is magnified by an SDL render scale; the visible world span therefore
    // shrinks to screen/zoom, so the camera frames on that reduced window. The
    // viewport origin (in the scaled coord space) still carries -cam + shake.
    SDL_SetRenderScale(sdl_, kCameraZoom, kCameraZoom);
    const int viewW = static_cast<int>(screenW / kCameraZoom);
    const int viewH = static_cast<int>(screenH / kCameraZoom);
    const Vec2 cam = cameraOffset(world, followPlayer, mode, viewW, viewH);
    lastCam_ = cam;
    SDL_Rect worldVp{static_cast<int>(-cam.x + offX), static_cast<int>(-cam.y + offY), world.width,
                     world.height};
    SDL_SetRenderViewport(sdl_, &worldVp);

    // Living ground: tiled terrain + scattered props under everything else.
    drawGround(world, cam, screenW, screenH);

    // Loot on the ground: a pixel sprite per item, over a rarity-coloured glow
    // for Rare+ gear so a good drop calls to you across the arena. A coloured
    // square is the fallback when the sprite isn't loaded.
    for (const auto& drop : world.loot) {
        if (static_cast<int>(drop.item.rarity) >= static_cast<int>(Rarity::Rare)) {
            const Color rc = rarityColor(drop.item.rarity);
            drawGlow(drop.position, drop.radius * 3.0f, rc.r, rc.g, rc.b, 20);
        }
        if (!drawSprite(lootSprite(drop.item), drop.position, drop.radius * kSpriteScale, false)) {
            setColor(sdl_, lootColor(drop.item));
            fillSquare(sdl_, drop.position, drop.radius);
        }
    }

    // Projectiles: glowing, velocity-oriented bolts (see drawProjectiles).
    drawProjectiles(world);

    // Enemies (red), with a thin HP bar when damaged and an aggro pip showing
    // which player they're chasing (in that player's colour).
    for (const auto& enemy : world.enemies) {
        if (!enemy.alive) continue;
        // Soft shadow first, so the sprite sits on the ground.
        drawShadow({enemy.position.x, enemy.position.y + enemy.radius}, enemy.radius * 0.9f);
        // Animated sprite by archetype: walk when moving, mirrored to face travel.
        // A solid square is the fallback when no sprite is loaded.
        const bool eFlip = enemy.velocity.x < -0.01f;
        const Motion eMotion = enemy.velocity.length() > 8.0f ? Motion::Walk : Motion::Idle;
        if (!drawCharacter(enemySprite(enemy.type), eMotion, enemy.id & 7, enemy.position,
                           enemy.radius * kSpriteScale, eFlip)) {
            setColor(sdl_, enemyColor(enemy.type));
            fillSquare(sdl_, enemy.position, enemy.radius);
        }
        // Hit juice: a white flash punched over the body (works over sprite or
        // square) that fades with the hit timer.
        if (enemy.hitFlash > 0.0f) {
            SDL_SetRenderDrawBlendMode(sdl_, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(sdl_, 255, 255, 255,
                                   static_cast<Uint8>(200 * std::min(1.0f, enemy.hitFlash)));
            fillSquare(sdl_, enemy.position, enemy.radius * kSpriteScale);
            SDL_SetRenderDrawBlendMode(sdl_, SDL_BLENDMODE_NONE);
        }
        // Bosses get a bright double outline so they read as a threat.
        if (enemy.type == EnemyType::Boss) {
            setColor(sdl_, 255, 220, 120);
            drawRing(sdl_, enemy.position, enemy.radius + 4.0f);
            drawRing(sdl_, enemy.position, enemy.radius + 7.0f);
        }
        // Elites wear a golden aura — a little target of desire.
        if (enemy.elite) {
            setColor(sdl_, 250, 210, 70);
            drawRing(sdl_, enemy.position, enemy.radius + 5.0f);
            drawRing(sdl_, enemy.position, enemy.radius + 9.0f);
        }
        // Predators wear a blood-red aura that thickens with every mob they eat,
        // so a fattened alpha reads as a threat from across the screen. An alpha
        // (enough kills) pulses, marking it as the patch's apex target.
        if (enemy.predator) {
            const int rings = 1 + std::min(3, enemy.predatorKills);
            SDL_SetRenderDrawBlendMode(sdl_, SDL_BLENDMODE_BLEND);
            const float beat =
                isAlpha(enemy) ? std::sin(SDL_GetTicks() * 0.012f) * 3.0f : 0.0f;
            SDL_SetRenderDrawColor(sdl_, 190, 25, 30, 210);
            for (int k = 0; k < rings; ++k)
                drawRing(sdl_, enemy.position, enemy.radius + 4.0f + 3.0f * k + beat);
            SDL_SetRenderDrawBlendMode(sdl_, SDL_BLENDMODE_NONE);
        }
        // Telegraph: a caster winding up flashes an expanding red warning ring —
        // your cue to dash out of the way before the blast lands.
        if (enemy.windup > 0.0f) {
            const float pulse = 8.0f + std::sin(SDL_GetTicks() * 0.02f) * 3.0f;
            SDL_SetRenderDrawBlendMode(sdl_, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(sdl_, 255, 70, 70, 220);
            drawRing(sdl_, enemy.position, enemy.radius + pulse);
            drawRing(sdl_, enemy.position, enemy.radius + pulse + 3.0f);
            SDL_SetRenderDrawBlendMode(sdl_, SDL_BLENDMODE_NONE);
        }
        if (enemy.targetPlayer >= 0) {
            setColor(sdl_, playerColor(enemy.targetPlayer));
            fillSquare(sdl_, {enemy.position.x, enemy.position.y - enemy.radius - 4.0f}, 3.0f);
        }
        if (enemy.hp < enemy.maxHp) {
            drawHpBar(sdl_, enemy.position, enemy.radius, 10.0f, enemy.hp, enemy.maxHp,
                      {90, 210, 90});
        }
        // Zone stakes: tag tougher mobs with their level so "further out = harder"
        // reads at a glance. Colour warms as the level climbs.
        if (enemy.level > 1) {
            // Warmer (redder) as the level climbs, so danger reads at a glance.
            const Uint8 g = static_cast<Uint8>(230 - std::min(160, enemy.level * 4));
            setColor(sdl_, 255, g, 90);
            char tag[8];
            std::snprintf(tag, sizeof(tag), "L%d", enemy.level);
            SDL_RenderDebugText(sdl_, enemy.position.x - enemy.radius,
                                enemy.position.y - enemy.radius - 20.0f, tag);
        }
    }

    // Players.
    for (int i = 0; i < static_cast<int>(world.players.size()); ++i) {
        const Player& p = world.players[i];
        if (!p.active) continue;
        const Entity& e = p.entity;

        // Soft shadow grounds the character.
        drawShadow({e.position.x, e.position.y + e.radius}, e.radius * 0.9f);

        // Team-colour ground ring under the character, so couch players can still
        // tell "which one is me" even though the sprite carries the class art.
        setColor(sdl_, playerColor(i));
        SDL_FRect base{e.position.x - e.radius, e.position.y + e.radius * kSpriteScale, e.radius * 2.0f,
                       3.0f};
        SDL_RenderFillRect(sdl_, &base);

        // Classless look: everyone is the neutral human body; your identity reads
        // from the weapon in hand and your title, not a class costume.
        const bool pFlip = p.aim.x < -0.01f;
        const Uint8 pAlpha = p.invuln > 0.0f ? 130 : 255;
        const Motion pMotion = p.attackFlash > 0.0f ? Motion::Attack
                               : (e.velocity.length() > 8.0f || p.moveIntent.length() > 0.1f)
                                   ? Motion::Walk
                                   : Motion::Idle;
        if (!drawCharacter("human", pMotion, i, e.position, e.radius * kSpriteScale, pFlip, pAlpha)) {
            setColor(sdl_, p.invuln > 0.0f ? Color{240, 240, 240} : playerColor(i));
            fillSquare(sdl_, e.position, e.radius);
        }
        // Weapon in hand — a bow if ranged, else a blade — on the facing side.
        if (p.hasWeapon()) {
            const AttackStyle ws = p.weapon().style;
            const char* wsprite = ws == AttackStyle::Ranged ? "wpn_bow"
                                : ws == AttackStyle::Magic  ? "wpn_staff"
                                                            : "wpn_sword";
            const float side = pFlip ? -1.0f : 1.0f;
            const Vec2 hand{e.position.x + side * e.radius * 0.95f, e.position.y + 2.0f};
            drawSprite(wsprite, hand, e.radius * 0.85f, pFlip, pAlpha);
        }

        // Emergent title above the head — who you've *become* by playing.
        setColor(sdl_, playerColor(i));
        const char* title = playerTitle(p);
        SDL_RenderDebugText(sdl_, e.position.x - std::strlen(title) * 4.0f,
                            e.position.y - e.radius * kSpriteScale - 12.0f, title);

        // Aim tick.
        setColor(sdl_, 230, 230, 120);
        fillSquare(sdl_, e.position + p.aim * (e.radius + 10.0f), 3.0f);

        // Melee swing ring.
        if (p.attackFlash > 0.0f) {
            setColor(sdl_, 255, 255, 255);
            drawRing(sdl_, e.position, p.cls.attackRange + e.radius);
        }
        // Tank shield ring.
        if (p.shieldTimer > 0.0f) {
            setColor(sdl_, 90, 170, 255);
            drawRing(sdl_, e.position, e.radius + 8.0f);
        }
        // Heal flash ring.
        if (p.healFlash > 0.0f) {
            setColor(sdl_, 120, 240, 140);
            drawRing(sdl_, e.position, e.radius + 6.0f);
        }

        drawHpBar(sdl_, e.position, e.radius, 8.0f, e.hp, e.maxHp, {220, 80, 80});
    }

    // Floating damage numbers, still in world space so they scroll with the map.
    updateAndDrawPopups(world);

    // HUD and overlays draw unshaken, unzoomed, at the fixed origin.
    SDL_SetRenderViewport(sdl_, nullptr);
    SDL_SetRenderScale(sdl_, 1.0f, 1.0f);
    drawHud(world);
    drawMinimap(world, screenW, screenH);
    drawBoonChooser(world, screenW, screenH);
    if (showBank) drawBankOverlay(world, screenW, screenH, bankCursor, followPlayer);
    if (showChar) drawCharSheet(world, screenW, screenH);
    SDL_RenderPresent(sdl_);
}

Vec2 Renderer::cameraOffset(const World& world, int followPlayer, CameraMode mode, int screenW,
                            int screenH) const {
    // Centre on the follow target, or the world's middle if there isn't one.
    Vec2 center{world.width / 2.0f, world.height / 2.0f};
    if (mode == CameraMode::FrameParty) {
        // Average the active players so the shared screen frames the whole party.
        Vec2 sum{};
        int n = 0;
        for (const auto& p : world.players) {
            if (!p.active) continue;
            sum += p.entity.position;
            ++n;
        }
        if (n > 0) center = sum * (1.0f / static_cast<float>(n));
    } else if (followPlayer >= 0 && followPlayer < static_cast<int>(world.players.size()) &&
               world.players[followPlayer].active) {
        center = world.players[followPlayer].entity.position;
    }

    float camX = center.x - screenW / 2.0f;
    float camY = center.y - screenH / 2.0f;

    // Don't scroll past the edges. If the world is smaller than the screen on an
    // axis, pin to 0 (the world is centred/top-left, never over-scrolled).
    const float maxX = std::max(0.0f, static_cast<float>(world.width - screenW));
    const float maxY = std::max(0.0f, static_cast<float>(world.height - screenH));
    camX = std::clamp(camX, 0.0f, maxX);
    camY = std::clamp(camY, 0.0f, maxY);
    return {camX, camY};
}

void Renderer::updateAndDrawPopups(const World& world) {
    // Spawn a popup wherever an enemy's HP dropped since last frame (matched by
    // its stable id). Works identically for local sim and remote snapshots.
    std::unordered_map<int, int> cur;
    cur.reserve(world.enemies.size());
    for (const auto& e : world.enemies) {
        if (!e.alive) continue;
        cur[e.id] = e.hp;
        const auto it = lastEnemyHp_.find(e.id);
        if (it != lastEnemyHp_.end() && e.hp < it->second) {
            popups_.push_back({{e.position.x - 4.0f, e.position.y - e.radius - 6.0f},
                               it->second - e.hp, 1.0f});
        }
    }
    lastEnemyHp_.swap(cur);

    // Rise + fade, then draw. Bigger hits skew red; all fade to black on the
    // dark background as their life runs out.
    for (auto& pu : popups_) {
        pu.pos.y -= 0.7f;
        pu.life -= 0.028f;
        const float b = std::max(0.0f, pu.life);
        const bool big = pu.amount >= 60;
        setColor(sdl_, static_cast<Uint8>(255 * b), static_cast<Uint8>((big ? 140 : 230) * b),
                 static_cast<Uint8>((big ? 90 : 150) * b));
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%d", pu.amount);
        SDL_RenderDebugText(sdl_, pu.pos.x, pu.pos.y, buf);
    }
    popups_.erase(std::remove_if(popups_.begin(), popups_.end(),
                                 [](const Popup& p) { return p.life <= 0.0f; }),
                  popups_.end());
}

void Renderer::updateShake(const World& world, float& offX, float& offY) {
    int alive = 0;
    for (const auto& e : world.enemies)
        if (e.alive) ++alive;

    // A drop in the alive count means kills (respawns raise it, so we ignore
    // those); gold ticking up means a pickup. Both add a little shake.
    const int killed = prevAliveEnemies_ - alive;
    if (killed > 0) shake_ += 2.5f * static_cast<float>(killed);
    if (world.gold > prevGold_) shake_ += 1.0f;
    prevAliveEnemies_ = alive;
    prevGold_ = world.gold;

    // Reward juice: a level-up or a fresh rare+ drop punches the shake harder.
    for (int i = 0; i < static_cast<int>(world.players.size()); ++i) {
        const int lvl = world.players[i].level;
        auto it = lastPlayerLevel_.find(i);
        if (it != lastPlayerLevel_.end() && lvl > it->second) shake_ += 6.0f;
        lastPlayerLevel_[i] = lvl;
    }
    int rareOnGround = 0;
    for (const auto& g : world.loot)
        if (static_cast<int>(g.item.rarity) >= static_cast<int>(Rarity::Rare)) ++rareOnGround;
    if (rareOnGround > prevRareLoot_) shake_ += 4.0f;
    prevRareLoot_ = rareOnGround;

    shake_ = std::min(shake_, 12.0f);  // cap so it never nauseates
    if (shake_ > 0.05f) {
        ++shakeTick_;
        const float t = static_cast<float>(shakeTick_);
        offX = std::sin(t * 1.7f) * shake_;
        offY = std::cos(t * 2.3f) * shake_;
        shake_ *= 0.85f;  // decay per frame (~60fps)
    } else {
        shake_ = 0.0f;
    }
}

void Renderer::drawHud(const World& world) {
    // SDL3's built-in 8x8 debug font — rendered at 2x for readability.
    SDL_SetRenderScale(sdl_, 2.0f, 2.0f);
    setColor(sdl_, 235, 235, 240);

    bool bossAlive = false;
    for (const auto& e : world.enemies)
        if (e.alive && e.type == EnemyType::Boss) {
            bossAlive = true;
            break;
        }

    int potions = 0;
    for (const auto& it : world.inventory.items())
        if (it.kind == ItemKind::Potion) potions += it.count;  // sum stack sizes

    char line[224];
    std::snprintf(line, sizeof(line), "Wave %d%s   Gold %d   Pots %d   Bank %zu items %.1f/%.0f",
                  world.wave, bossAlive ? "  *** BOSS ***" : "", world.gold, potions,
                  world.inventory.size(), world.inventory.currentWeight(),
                  world.inventory.maxWeight());
    setColor(sdl_, bossAlive ? Color{255, 90, 140} : Color{235, 235, 240});
    SDL_RenderDebugText(sdl_, 6.0f, 6.0f, line);
    setColor(sdl_, 235, 235, 240);

    float y = 18.0f;
    for (int i = 0; i < static_cast<int>(world.players.size()); ++i) {
        const Player& p = world.players[i];
        if (!p.active) continue;
        const char* ability = p.abilityCooldown > 0.0f ? "..." : "READY";
        char gear[48] = "";
        const int gdmg = gearDamage(p), ghp = gearMaxHp(p);
        if (gdmg > 0 || ghp > 0) {
            std::snprintf(gear, sizeof(gear), "  [W+%d A+%d]", gdmg, ghp);
        }
        // Boon count: the stacking per-run power, so the god-build reads at a glance.
        if (p.boons.count > 0) {
            char boon[16];
            std::snprintf(boon, sizeof(boon), "  B%d", p.boons.count);
            std::strncat(gear, boon, sizeof(gear) - std::strlen(gear) - 1);
        }
        // Auto-cast abilities acquired this run.
        if (!p.abilities.empty()) {
            char ab[16];
            std::snprintf(ab, sizeof(ab), "  A%zu", p.abilities.size());
            std::strncat(gear, ab, sizeof(gear) - std::strlen(gear) - 1);
        }
        // Encumbrance cue: warns that heavy gear is dragging your speed.
        if (encumbranceMul(p) < 0.98f)
            std::strncat(gear, "  (heavy)", sizeof(gear) - std::strlen(gear) - 1);
        // Nudge to draft a pending level-up boon; blinks so it catches the eye.
        char pts[24] = "";
        if (p.pendingBoons > 0 && (SDL_GetTicks() / 400) % 2 == 0)
            std::snprintf(pts, sizeof(pts), "  LEVEL UP!");
        std::snprintf(line, sizeof(line), "P%d %-8s Lv%d  HP %d/%d   %s: %s%s%s", i + 1,
                      playerTitle(p), p.level, p.entity.hp, p.entity.maxHp,
                      p.cls.abilityName.c_str(), ability, gear, pts);
        SDL_RenderDebugText(sdl_, 6.0f, y, line);
        y += 12.0f;
    }

    y += 4.0f;
    SDL_RenderDebugText(
        sdl_, 6.0f, y,
        "Move WASD  aim mouse/R  atk Space/A  abil E/X  dash Shift/RB  equip F/Y  pot Q/LB");
    SDL_RenderDebugText(sdl_, 6.0f, y + 12.0f, "Char sheet C   Bank Tab   Quit Esc");

    SDL_SetRenderScale(sdl_, 1.0f, 1.0f);
}

void Renderer::drawBoonChooser(const World& world, int screenW, int screenH) {
    // Any active player with a pending level-up choice? Collect them first so we
    // can size a bottom-centre panel that stacks one offer block per player.
    std::vector<int> choosing;
    for (int i = 0; i < static_cast<int>(world.players.size()); ++i) {
        const Player& p = world.players[i];
        if (p.active && p.pendingBoons > 0 && p.boonChoices[0] >= 0) choosing.push_back(i);
    }
    if (choosing.empty()) return;

    auto label = [&](int id) -> const char* {
        if (id >= 0 && id < static_cast<int>(upgradeLabels_.size())) return upgradeLabels_[id].c_str();
        return "Upgrade";
    };

    // Panel geometry in the 2x text space (all text below is drawn at scale 2).
    SDL_SetRenderScale(sdl_, 2.0f, 2.0f);
    const float vw = screenW / 2.0f, vh = screenH / 2.0f;  // virtual size at 2x
    const float lineH = 12.0f;
    const float blockH = lineH * 5.0f;  // header + 3 options + spacer per player
    const float panelH = 14.0f + blockH * static_cast<float>(choosing.size());
    const float panelW = 320.0f;
    const float x0 = (vw - panelW) * 0.5f;
    float y = vh - panelH - 10.0f;  // pinned near the bottom, out of the action

    // Dim backdrop so the offer reads over a busy fight, without pausing it.
    SDL_SetRenderScale(sdl_, 1.0f, 1.0f);
    SDL_SetRenderDrawBlendMode(sdl_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(sdl_, 0, 0, 0, 170);
    SDL_FRect bg{x0 * 2.0f - 12.0f, y * 2.0f - 8.0f, panelW * 2.0f + 24.0f, panelH * 2.0f};
    SDL_RenderFillRect(sdl_, &bg);
    SDL_SetRenderDrawBlendMode(sdl_, SDL_BLENDMODE_NONE);
    setColor(sdl_, 240, 220, 120);
    SDL_RenderRect(sdl_, &bg);

    SDL_SetRenderScale(sdl_, 2.0f, 2.0f);
    char line[128];
    for (int idx : choosing) {
        const Player& p = world.players[idx];
        setColor(sdl_, playerColor(idx));
        // P1 (the keyboard seat) picks with number keys; pads pick with the D-pad.
        const char* keys = idx == 0 ? "keys 1/2/3" : "D-pad L/U/R";
        std::snprintf(line, sizeof(line), "P%d LEVEL UP!  choose a boon  (%s)", idx + 1, keys);
        SDL_RenderDebugText(sdl_, x0, y, line);
        y += lineH;
        static const char* slotTag[3] = {"[1|<]", "[2|^]", "[3|>]"};
        for (int s = 0; s < 3; ++s) {
            setColor(sdl_, 235, 235, 240);
            std::snprintf(line, sizeof(line), "  %s %s", slotTag[s], label(p.boonChoices[s]));
            SDL_RenderDebugText(sdl_, x0, y, line);
            y += lineH;
        }
        y += lineH;  // spacer between players
    }
    SDL_SetRenderScale(sdl_, 1.0f, 1.0f);
}

void Renderer::drawMinimap(const World& world, int screenW, int screenH) {
    if (world.width <= 0 || world.height <= 0) return;

    // A 16:9-ish box pinned to the top-right corner.
    const float mmW = 220.0f;
    const float mmH = mmW * static_cast<float>(world.height) / static_cast<float>(world.width);
    const float x0 = static_cast<float>(screenW) - mmW - 12.0f;
    const float y0 = 12.0f;
    const float sx = mmW / static_cast<float>(world.width);
    const float sy = mmH / static_cast<float>(world.height);
    auto mapX = [&](float wx) { return x0 + wx * sx; };
    auto mapY = [&](float wy) { return y0 + wy * sy; };

    // Dim backdrop + border.
    SDL_SetRenderDrawBlendMode(sdl_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(sdl_, 0, 0, 0, 150);
    SDL_FRect bg{x0, y0, mmW, mmH};
    SDL_RenderFillRect(sdl_, &bg);
    SDL_SetRenderDrawBlendMode(sdl_, SDL_BLENDMODE_NONE);
    setColor(sdl_, 120, 120, 140);
    SDL_RenderRect(sdl_, &bg);

    // Enemies: small dots (elites gold so they pop as targets of desire).
    for (const auto& e : world.enemies) {
        if (!e.alive) continue;
        setColor(sdl_, e.elite ? Color{250, 210, 70} : Color{210, 80, 80});
        SDL_FRect d{mapX(e.position.x) - 1.0f, mapY(e.position.y) - 1.0f, 2.0f, 2.0f};
        SDL_RenderFillRect(sdl_, &d);
    }

    // Allies: bigger dots in their player colour — see where everyone is.
    for (int i = 0; i < static_cast<int>(world.players.size()); ++i) {
        const Player& p = world.players[i];
        if (!p.active) continue;
        setColor(sdl_, playerColor(i));
        SDL_FRect d{mapX(p.entity.position.x) - 2.0f, mapY(p.entity.position.y) - 2.0f, 4.0f, 4.0f};
        SDL_RenderFillRect(sdl_, &d);
    }

    // The current camera view, so you can place yourself in the wider world.
    SDL_FRect view{mapX(lastCam_.x), mapY(lastCam_.y), screenW * sx, screenH * sy};
    setColor(sdl_, 230, 230, 235);
    SDL_RenderRect(sdl_, &view);
}

void Renderer::drawBankOverlay(const World& world, int screenW, int screenH, int bankCursor,
                               int refPlayer) {
    // Dim the screen, then draw the shared household bank contents.
    SDL_SetRenderDrawBlendMode(sdl_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(sdl_, 0, 0, 0, 180);
    SDL_FRect dim{0.0f, 0.0f, static_cast<float>(screenW), static_cast<float>(screenH)};
    SDL_RenderFillRect(sdl_, &dim);
    SDL_SetRenderDrawBlendMode(sdl_, SDL_BLENDMODE_NONE);

    SDL_SetRenderScale(sdl_, 2.0f, 2.0f);
    setColor(sdl_, 240, 235, 200);

    char line[160];
    float x = 60.0f, y = 40.0f;
    SDL_RenderDebugText(sdl_, x, y, "=== SHARED BANK ===");
    y += 16.0f;
    std::snprintf(line, sizeof(line), "Gold: %d     Weight: %.1f / %.0f     Items: %zu", world.gold,
                  world.inventory.currentWeight(), world.inventory.maxWeight(),
                  world.inventory.size());
    SDL_RenderDebugText(sdl_, x, y, line);
    y += 18.0f;

    // Equipped gear per active player, so you can SEE what's on before you swap.
    // (Equipping a bank item auto-returns the old piece, so a swap is one keypress.)
    setColor(sdl_, 200, 220, 245);
    SDL_RenderDebugText(sdl_, x, y, "EQUIPPED:");
    y += 12.0f;
    for (int i = 0; i < static_cast<int>(world.players.size()); ++i) {
        const Player& pl = world.players[i];
        if (!pl.active) continue;
        setColor(sdl_, playerColor(i));
        if (i == refPlayer) {
            // The cursor's player: show the whole paperdoll, one line per slot, so
            // empty slots read as targets to fill.
            std::snprintf(line, sizeof(line), " P%d  (Dmg+%d  HP+%d)", i + 1, gearDamage(pl),
                          gearMaxHp(pl));
            SDL_RenderDebugText(sdl_, x, y, line);
            y += 11.0f;
            for (int s = 0; s < kEquipSlotCount; ++s) {
                const EquipSlot es = static_cast<EquipSlot>(s);
                const bool has = pl.equipment[s].has;
                std::snprintf(line, sizeof(line), "   %-5s %s", slotName(es),
                              has ? pl.equipment[s].item.name.c_str() : "-");
                setColor(sdl_, has ? rarityColor(pl.equipment[s].item.rarity)
                                   : Color{120, 120, 130});
                SDL_RenderDebugText(sdl_, x, y, line);
                y += 10.0f;
            }
        } else {
            // Other players: a compact one-liner (main hand + totals).
            const bool hw = pl.hasWeapon();
            std::snprintf(line, sizeof(line), " P%d  Main:%s  (Dmg+%d HP+%d)", i + 1,
                          hw ? pl.weapon().name.c_str() : "-", gearDamage(pl), gearMaxHp(pl));
            SDL_RenderDebugText(sdl_, x, y, line);
            y += 11.0f;
        }
    }
    y += 8.0f;
    setColor(sdl_, 240, 235, 200);

    const auto& items = world.inventory.items();
    if (items.empty()) {
        SDL_RenderDebugText(sdl_, x, y, "(empty - go kill something)");
    } else {
        const int count = static_cast<int>(items.size());
        // Scroll window: fit as many rows as the space allows (2x text space, and
        // leave room for the two hint lines), then slide it to keep the cursor in
        // view — so a bank bigger than the screen stays fully navigable.
        const float bottom = screenH / 2.0f - 42.0f;
        int maxVisible = std::max(1, static_cast<int>((bottom - y) / 11.0f) - 1);
        const int cursor = (bankCursor >= 0 && bankCursor < count) ? bankCursor : 0;
        int start = 0;
        if (count > maxVisible) start = std::clamp(cursor - maxVisible / 2, 0, count - maxVisible);
        const int end = std::min(count, start + maxVisible);

        if (start > 0) {
            setColor(sdl_, 170, 170, 185);
            SDL_RenderDebugText(sdl_, x, y, "   ^ more above");
            y += 11.0f;
        }
        for (int i = start; i < end; ++i) {
            const Item& it = items[i];
            std::string affixStr;
            for (const auto& a : it.affixes) {
                affixStr += (affixStr.empty() ? "  " : " ");
                affixStr += affixLabel(a);
            }
            const bool gear = it.kind == ItemKind::Weapon || it.kind == ItemKind::Armor;
            const bool selected = i == cursor;
            setColor(sdl_, selected ? Color{255, 255, 255}
                                    : (gear ? rarityColor(it.rarity) : Color{240, 235, 200}));
            char slot[6] = "  -";
            if (i < 9) std::snprintf(slot, sizeof(slot), "[%d]", i + 1);
            // Potions show what they heal (scales with rarity); gear shows its affixes.
            char extra[32] = "";
            if (it.kind == ItemKind::Potion)
                std::snprintf(extra, sizeof(extra), "  x%d  heal %d%%", it.count,
                              potionHealPercent(it.rarity));
            else if (gear)
                std::snprintf(extra, sizeof(extra), "  pow%d", itemPower(it));
            std::snprintf(line, sizeof(line), "%s%s %-15s %.1fkg  %dg%s%s", selected ? ">" : " ",
                          slot, it.name.c_str(), it.weight, it.value, affixStr.c_str(), extra);
            SDL_RenderDebugText(sdl_, x, y, line);
            y += 11.0f;
        }
        if (end < count) {
            setColor(sdl_, 170, 170, 185);
            SDL_RenderDebugText(sdl_, x, y, "   v more below");
            y += 11.0f;
        }
        setColor(sdl_, 240, 235, 200);
    }

    // Comparison panel (right column): the selected gear vs. what the cursor's
    // player has equipped, stat by stat, so "is this an upgrade?" is answerable at
    // a glance — green = better, red = worse. Weight inverts (lighter is better).
    if (!items.empty()) {
        const int count = static_cast<int>(items.size());
        const int cursor = (bankCursor >= 0 && bankCursor < count) ? bankCursor : 0;
        const Item& sel = items[cursor];
        if (sel.slot != EquipSlot::None) {
            // Reference player = the seat driving the cursor; fall back to the first
            // active player if that index is somehow inactive/out of range.
            const Player* rp = nullptr;
            if (refPlayer >= 0 && refPlayer < static_cast<int>(world.players.size()) &&
                world.players[refPlayer].active)
                rp = &world.players[refPlayer];
            else
                for (const auto& pl : world.players)
                    if (pl.active) { rp = &pl; break; }

            // Compare against what's in the item's slot. For a ring, if the first
            // finger is free but the second is taken, compare against that one.
            EquipSlot cmpSlot = sel.slot;
            if (cmpSlot == EquipSlot::Ring1 && rp && !rp->hasEquip(EquipSlot::Ring1) &&
                rp->hasEquip(EquipSlot::Ring2))
                cmpSlot = EquipSlot::Ring2;
            const bool hasCur = rp && rp->hasEquip(cmpSlot);
            const Item equipped = hasCur ? rp->equip(cmpSlot) : Item{};

            float cx = screenW / 2.0f * 0.58f, cy = 70.0f;
            setColor(sdl_, 200, 220, 245);
            std::snprintf(line, sizeof(line), "COMPARE (%s)", slotName(sel.slot));
            SDL_RenderDebugText(sdl_, cx, cy, line);
            cy += 12.0f;
            setColor(sdl_, 255, 255, 255);
            std::snprintf(line, sizeof(line), "%.20s", sel.name.c_str());
            SDL_RenderDebugText(sdl_, cx, cy, line);
            cy += 11.0f;
            setColor(sdl_, 170, 175, 185);
            std::snprintf(line, sizeof(line), "vs %.17s", hasCur ? equipped.name.c_str() : "(nothing)");
            SDL_RenderDebugText(sdl_, cx, cy, line);
            cy += 13.0f;

            // One row per stat where either side is non-zero.
            const AffixType order[] = {AffixType::Damage,   AffixType::MaxHp,
                                       AffixType::AttackSpeed, AffixType::MoveSpeed,
                                       AffixType::Crit,      AffixType::Lifesteal,
                                       AffixType::SpellPower};
            for (AffixType t : order) {
                const int a = itemStatTotal(equipped, t);
                const int b = itemStatTotal(sel, t);
                if (a == 0 && b == 0) continue;
                const int d = b - a;
                setColor(sdl_, d > 0 ? Color{90, 210, 90}
                                     : d < 0 ? Color{225, 90, 90} : Color{190, 190, 195});
                const char* pct = affixIsPercent(t) ? "%" : "";
                std::snprintf(line, sizeof(line), "  %-5s %d%s -> %d%s  %s%d%s", affixShortName(t),
                              a, pct, b, pct, d > 0 ? "+" : "", d, pct);
                SDL_RenderDebugText(sdl_, cx, cy, line);
                cy += 11.0f;
            }
            // Weight — lighter is better, so the colour is inverted.
            const float wa = hasCur ? equipped.weight : 0.0f, wb = sel.weight, dw = wb - wa;
            setColor(sdl_, dw < -0.05f ? Color{90, 210, 90}
                                       : dw > 0.05f ? Color{225, 90, 90} : Color{190, 190, 195});
            std::snprintf(line, sizeof(line), "  %-5s %.1f -> %.1f kg", "Wt", wa, wb);
            SDL_RenderDebugText(sdl_, cx, cy, line);
            cy += 11.0f;
            // Overall power score.
            const int pa = hasCur ? itemPower(equipped) : 0, pb = itemPower(sel), dp = pb - pa;
            setColor(sdl_, dp > 0 ? Color{90, 210, 90}
                                  : dp < 0 ? Color{225, 90, 90} : Color{190, 190, 195});
            std::snprintf(line, sizeof(line), "  %-5s %d -> %d  %s%d", "Pow", pa, pb,
                          dp > 0 ? "+" : "", dp);
            SDL_RenderDebugText(sdl_, cx, cy, line);
            cy += 11.0f;
            // Style swap is a play-style change, not just a number — flag it.
            if (sel.slot == EquipSlot::MainHand && hasCur && sel.style != equipped.style) {
                setColor(sdl_, 240, 200, 90);
                const char* sn = sel.style == AttackStyle::Melee    ? "MELEE"
                                 : sel.style == AttackStyle::Ranged ? "RANGED"
                                                                    : "MAGIC";
                std::snprintf(line, sizeof(line), "  ! becomes %s", sn);
                SDL_RenderDebugText(sdl_, cx, cy, line);
            }
        }
    }

    setColor(sdl_, 200, 220, 245);
    SDL_RenderDebugText(sdl_, x, screenH / 2.0f - 24.0f,
                        "Up/Down: select   Enter: use/equip (drink potions)   V: sell   X: sell junk   G: unequip");
    setColor(sdl_, 240, 235, 200);
    SDL_RenderDebugText(sdl_, x, screenH / 2.0f - 12.0f,
                        "(1-9 quick-use   Q: quick-heal   move with WASD   Tab to close)");
    SDL_SetRenderScale(sdl_, 1.0f, 1.0f);
}

void Renderer::drawCharSheet(const World& world, int screenW, int screenH) {
    // Dim the screen, then list each active player's characteristics + points.
    SDL_SetRenderDrawBlendMode(sdl_, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(sdl_, 0, 0, 0, 180);
    SDL_FRect dim{0.0f, 0.0f, static_cast<float>(screenW), static_cast<float>(screenH)};
    SDL_RenderFillRect(sdl_, &dim);
    SDL_SetRenderDrawBlendMode(sdl_, SDL_BLENDMODE_NONE);

    SDL_SetRenderScale(sdl_, 2.0f, 2.0f);
    setColor(sdl_, 235, 235, 245);

    char line[160];
    float x = 60.0f, y = 40.0f;
    SDL_RenderDebugText(sdl_, x, y, "=== CHARACTER ===");
    y += 18.0f;

    static const char* kStatNames[5] = {"STR", "DEX", "INT", "VIT", "AGI"};
    for (int i = 0; i < static_cast<int>(world.players.size()); ++i) {
        const Player& p = world.players[i];
        if (!p.active) continue;
        setColor(sdl_, playerColor(i));
        std::snprintf(line, sizeof(line), "P%d  %s (%s)  Lv%d  XP %d/%d  boons: %d", i + 1,
                      playerTitle(p), className(p.cls.id), p.level, p.xp, xpForLevel(p.level),
                      p.boons.count);
        SDL_RenderDebugText(sdl_, x, y, line);
        y += 12.0f;
        const int vals[5] = {p.stats.str, p.stats.dex, p.stats.intel, p.stats.vit, p.stats.agi};
        setColor(sdl_, 210, 210, 220);
        std::snprintf(line, sizeof(line), "   %s %d   %s %d   %s %d   %s %d   %s %d", kStatNames[0],
                      vals[0], kStatNames[1], vals[1], kStatNames[2], vals[2], kStatNames[3], vals[3],
                      kStatNames[4], vals[4]);
        SDL_RenderDebugText(sdl_, x, y, line);
        y += 12.0f;
        setColor(sdl_, 170, 190, 210);
        std::snprintf(line, sizeof(line),
                      "   skills: Melee %d  Ranged %d  Arcane %d  Heal %d  Dodge %d",
                      p.skills.melee.level, p.skills.ranged.level, p.skills.arcane.level,
                      p.skills.heal.level, p.skills.dodge.level);
        SDL_RenderDebugText(sdl_, x, y, line);
        y += 12.0f;
        // Mastery of the weapon in hand — what gates that weapon's spell discovery.
        if (p.hasWeapon() && !p.weapon().weaponClass.empty()) {
            setColor(sdl_, 200, 180, 120);
            std::snprintf(line, sizeof(line), "   weapon mastery: %s Lv %d",
                          p.weapon().weaponClass.c_str(),
                          p.masteryOf(p.weapon().weaponClass));
            SDL_RenderDebugText(sdl_, x, y, line);
            y += 12.0f;
        }
        // Drafted auto-cast spells: name + rank + how close to firing. Re-drafting a
        // spell you already own ranks it up (faster cooldown); this is where you see it.
        // Spells persist through weapon swaps — the weapon only gates what's *offered*.
        if (!p.abilities.empty()) {
            setColor(sdl_, 180, 150, 220);
            for (const auto& a : p.abilities) {
                const char* name = (a.specId >= 0 && a.specId < static_cast<int>(abilityNames_.size()))
                                       ? abilityNames_[a.specId].c_str()
                                       : "Spell";
                const char* state = a.cooldown > 0.0f ? "charging" : "ready";
                std::snprintf(line, sizeof(line), "   spell: %s  Rank %d  (%s)", name, a.rank, state);
                SDL_RenderDebugText(sdl_, x, y, line);
                y += 12.0f;
            }
        }
        y += 4.0f;
    }

    y += 4.0f;
    setColor(sdl_, 200, 200, 210);
    SDL_RenderDebugText(sdl_, x, y, "Stats grow automatically toward how you play (no menu to manage).");
    SDL_RenderDebugText(sdl_, x, y + 12.0f, "Your build comes from the boons/abilities you draft on level-up.");
    SDL_RenderDebugText(sdl_, x, y + 28.0f, "(C to close - the world keeps running, so watch your back)");
    SDL_SetRenderScale(sdl_, 1.0f, 1.0f);
}

}  // namespace game
