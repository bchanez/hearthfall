#pragma once

#include <cmath>
#include <vector>

#include "Entity.hpp"
#include "Inventory.hpp"
#include "Item.hpp"
#include "PlayerClass.hpp"
#include "Projectile.hpp"
#include "Vec2.hpp"

namespace game {

// An item lying on the ground, waiting to be picked up.
struct GroundItem {
    Vec2 position{};
    Item item{};
    float radius = 10.0f;
};

// Character characteristics, raised by spending points earned on level-up. The
// class only sets the *starting* baseline (base HP/damage/style); everything you
// grow into comes from these. This is the seed of the classless / Mabinogi-style
// specialization — what you pump is who you become. Stat index order (used by
// the alloc command + UI): 0 STR, 1 DEX, 2 INT, 3 VIT, 4 AGI.
struct Stats {
    int str = 0;    // melee damage (+ carry weight, later)
    int dex = 0;    // ranged damage + attack speed + crit
    int intel = 0;  // spell / heal power
    int vit = 0;    // max HP
    int agi = 0;    // move speed (+ dash, later)
};

// Skills that rise by *doing* (Mabinogi model): swing a melee weapon and Melee
// climbs; fire a bow and Ranged climbs; heal, dodge, likewise. A skill's level
// scales its action and, past thresholds, unlocks stronger abilities — so what
// you practise is what you become, independent of the level-up stat points.
struct Skill {
    int level = 1;
    int xp = 0;  // toward the next skill level
};
struct Skills {
    Skill melee;
    Skill ranged;
    Skill heal;
    Skill dodge;
};

// One player character. Everything here is per-player; the bank (inventory +
// gold) is shared and lives on the World, matching the "shared household bank"
// from DESIGN.md.
struct Player {
    Entity entity;
    PlayerClass cls;

    int level = 1;
    int xp = 0;  // toward the next level

    // Specialization: characteristics + the points banked on level-up, spent
    // into stats through a non-pausing command (allocating mid-fight is risky).
    Stats stats;
    int unspentPoints = 0;

    // Use-based mastery — rises as you perform the matching action.
    Skills skills;

    // Equipment drawn from the shared bank. When the player leaves, this gear
    // flows back into the bank ("fluid loot").
    bool hasWeapon = false;
    Item weapon{};
    bool hasArmor = false;
    Item armor{};

    Vec2 moveIntent{};
    Vec2 aim{1.0f, 0.0f};

    // Per-frame input, consumed each step.
    bool attackQueued = false;
    bool abilityQueued = false;
    bool dashQueued = false;

    // Dash / dodge roll: a short burst along dashDir with i-frames.
    float dashTimer = 0.0f;     // >0 while dashing
    float dashCooldown = 0.0f;  // until the next dash is ready
    Vec2 dashDir{1.0f, 0.0f};

    // Timers (seconds).
    float attackCooldown = 0.0f;
    float attackFlash = 0.0f;
    float abilityCooldown = 0.0f;
    float shieldTimer = 0.0f;
    float healFlash = 0.0f;
    float invuln = 0.0f;
    float downedFlash = 0.0f;
    float potionCooldown = 0.0f;  // gate on chugging the whole bank at once

    bool active = true;  // inactive slots are hidden and skipped (e.g. pad unplugged)
};

// The complete, self-contained game state. Owned by the Simulation and rendered
// (read-only) by the Renderer. Free of any SDL / rendering types.
struct World {
    // World extent, decoupled from the screen size. The camera (see Renderer)
    // shows a screen-sized window into this and follows the local player, so the
    // world can be walked around rather than fitting on one screen.
    int width = 3840;
    int height = 2160;

    std::vector<Player> players;

    // Shared household bank.
    Inventory inventory{50.0f};
    int gold = 0;

    // World contents.
    std::vector<Entity> enemies;
    std::vector<GroundItem> loot;
    std::vector<Projectile> projectiles;

    int wave = 0;
};

// Concentric biome/difficulty ring radii, measured from the world centre. They
// scale with the map (fractions of its half-diagonal) so a bigger world spreads
// the zones out instead of drowning in the deadly outer ring. The fractions
// reproduce the original hand-tuned 550/850/1150 at the old 2560x1440 size.
// BOTH the Renderer (ground bands) and the Simulation (zoneLevel) read this so
// the visible biome and its difficulty stay in lockstep — change it in one place.
struct RingRadii {
    float r0;  // safe inner ring (levels 1..5)
    float r1;  // level 10 band
    float r2;  // level 20 band; beyond r2 is the level 40 endgame ring
};
inline RingRadii ringRadii(const World& w) {
    const float halfDiag =
        0.5f * std::sqrt(static_cast<float>(w.width) * static_cast<float>(w.width) +
                         static_cast<float>(w.height) * static_cast<float>(w.height));
    return {0.375f * halfDiag, 0.579f * halfDiag, 0.783f * halfDiag};
}

}  // namespace game
