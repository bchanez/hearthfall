#pragma once

#include <SDL3/SDL.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "Sprite.hpp"
#include "World.hpp"

namespace game {

// How the camera frames the world:
//   FollowLocal — centre on one (local) player. Desktop / online horizon.
//   FrameParty  — centre on the whole party's centroid. Couch / shared-screen:
//                 everyone shares one TV, so nobody has "their own" camera.
enum class CameraMode { FollowLocal, FrameParty };

// Draws a read-only World. Holds no game state and never mutates the world —
// the mirror image of Simulation.
class Renderer {
public:
    explicit Renderer(SDL_Renderer* sdl) : sdl_(sdl) {}
    ~Renderer();

    Renderer(const Renderer&) = delete;             // owns SDL textures
    Renderer& operator=(const Renderer&) = delete;

    // Bake decoded sprites (from ScriptEngine::loadSprites) into GPU textures,
    // keyed by name. Call once after construction; safe to call with an empty
    // list, in which case draw() falls back to solid squares everywhere.
    void loadSprites(const std::vector<SpritePixels>& defs);

    // followPlayer: the local player the camera centres on in FollowLocal mode.
    // If it's out of range (e.g. a client not yet welcomed), the camera centres
    // on the world instead. In FrameParty mode followPlayer is ignored.
    void draw(const World& world, bool showBank = false, int followPlayer = 0,
              CameraMode mode = CameraMode::FollowLocal, bool showChar = false);

    // How much the world view is magnified so sprites/tiles read bigger. The
    // world is drawn through an SDL render scale of this factor; the HUD is drawn
    // unzoomed. Both the camera framing and screenToWorld account for it.
    static constexpr float kCameraZoom = 1.6f;

    // Convert a screen-space point (e.g. the mouse) into world space using the
    // camera offset from the most recent draw. Divides out the zoom first, since
    // the world is drawn magnified. Used to aim toward the cursor now that the
    // player is no longer at a fixed screen position.
    Vec2 screenToWorld(const Vec2& screen) const {
        return screen * (1.0f / kCameraZoom) + lastCam_;
    }

private:
    void drawHud(const World& world);
    void drawBankOverlay(const World& world, int screenW, int screenH);
    // The character sheet: each active player's characteristics, banked points,
    // and how to spend them. Non-pausing (drawn over a live world), like the bank.
    void drawCharSheet(const World& world, int screenW, int screenH);
    // A corner minimap of the whole world: enemies, allies, and the camera view
    // box. Makes a bigger-than-screen world navigable and shows where allies are.
    void drawMinimap(const World& world, int screenW, int screenH);

    // Top-left world coordinate the camera shows this frame: the follow target
    // (a player, or the party centroid) centred on screen, clamped so we never
    // scroll past the world edges.
    Vec2 cameraOffset(const World& world, int followPlayer, CameraMode mode, int screenW,
                      int screenH) const;
    // Screen-shake kicked by kills/damage. Returns the pixel offset to draw the
    // world at this frame, and decays over time. Purely visual — never touches
    // the world, so it also works when rendering a remote snapshot.
    void updateShake(const World& world, float& offX, float& offY);

    // Tile the ground across the visible world and scatter props, so the map is
    // a living place rather than a black void. The ground type is chosen by
    // distance from spawn, mirroring the sim's difficulty rings (grass centre →
    // cracked stone in the deadly outer zones). Purely render-side + deterministic
    // (hashed per cell), so it also works when drawing a remote snapshot.
    void drawGround(const World& world, const Vec2& cam, int screenW, int screenH);

    struct BakedSprite {
        SDL_Texture* tex = nullptr;
        int w = 0, h = 0;
    };

    // Draw one baked texture centred on `center`, scaled so its longer side spans
    // 2*half pixels (aspect kept), optionally mirrored, at `alpha`. `angleDeg`
    // rotates it clockwise (used to point projectiles along their velocity) and
    // `blend` swaps in additive blending for glow/streak passes.
    // `squashX`/`squashY` non-uniformly scale the sprite (1,1 = none) anchored at
    // the feet, so procedural squash-&-stretch stays grounded (see drawCharacter).
    void drawBaked(const BakedSprite& s, const Vec2& center, float half, bool flipX, Uint8 alpha,
                   double angleDeg = 0.0, SDL_BlendMode blend = SDL_BLENDMODE_BLEND,
                   float squashX = 1.0f, float squashY = 1.0f);
    // Glowing, velocity-oriented projectiles: a dim additive halo + a short streak
    // up the tail + a crisp hot core, from the "bolt" effect sheet. Falls back to
    // the old solid square when no effect art is loaded.
    void drawProjectiles(const World& world);
    // The current frame of an effect's idle cycle (fast flicker), or nullptr if the
    // effect isn't loaded. Effects animate on a wall clock like characters do.
    const BakedSprite* effectFrame(const std::string& base);
    // Look up a baked sprite by name and draw it. Returns false if none is loaded,
    // so callers fall back to a solid square.
    bool drawSprite(const char* name, const Vec2& center, float half, bool flipX,
                    Uint8 alpha = 255);

    // Character motion states, chosen from the entity each frame.
    enum class Motion { Idle, Walk, Attack };
    // Draw an animated character by base name ("tank", "grunt", …), selecting the
    // state's frame from a render clock (walk/idle cycle) offset by `phase` so
    // actors don't all animate in lockstep. Falls back to a static sprite / false.
    bool drawCharacter(const std::string& base, Motion motion, int phase, const Vec2& center,
                       float half, bool flipX, Uint8 alpha = 255);
    // A soft translucent blob under an actor's feet — grounds sprites so they
    // don't look like they float. Purely cosmetic.
    void drawShadow(const Vec2& feet, float radius);
    // A soft ADDITIVE light pool (torchlight, magic glow). Concentric squares
    // summed by additive blending brighten toward the centre. Purely cosmetic;
    // works on remote snapshots since it's derived from render-side positions.
    void drawGlow(const Vec2& center, float radius, Uint8 r, Uint8 g, Uint8 b, Uint8 perLayer = 26);

    SDL_Renderer* sdl_;

    // Baked textures keyed by full name ("grass", "tank.walk.0", …). Owns the
    // textures (destroyed in the dtor). Empty until loadSprites runs; the game
    // still draws (squares) without them.
    std::unordered_map<std::string, BakedSprite> sprites_;

    // Grouped animations: base name → per-state frame lists, built from the
    // "base.state.frame" names at load. Frames reference textures owned by
    // sprites_, so this holds no ownership.
    struct CharAnim {
        std::vector<BakedSprite> idle, walk, attack;
    };
    std::unordered_map<std::string, CharAnim> anims_;
    std::uint64_t frameCounter_ = 0;  // advances animation cycles, one per draw

    // Shake state (render-only). We detect "something violent happened" by
    // watching kills and gold gained between frames.
    float shake_ = 0.0f;
    int prevAliveEnemies_ = 0;
    int prevGold_ = 0;
    int shakeTick_ = 0;

    // Camera offset from the most recent draw, so input code can map the mouse
    // back into world space (see screenToWorld).
    Vec2 lastCam_{};

    // Floating damage numbers. Derived purely on the render side by diffing enemy
    // HP per stable id between frames, so it works on remote snapshots too.
    struct Popup {
        Vec2 pos;
        int amount = 0;
        float life = 1.0f;  // 1 → 0, drives rise + fade
    };
    std::vector<Popup> popups_;
    std::unordered_map<int, int> lastEnemyHp_;  // enemy id → hp last frame
    void updateAndDrawPopups(const World& world);

    // Reward juice: watch for level-ups / rare drops to punch the screen-shake.
    std::unordered_map<int, int> lastPlayerLevel_;  // player index → level
    int prevRareLoot_ = 0;                          // rare+ items on the ground last frame
};

}  // namespace game
