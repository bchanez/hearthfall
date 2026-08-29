#pragma once

#include <cmath>
#include <string>
#include <unordered_map>
#include <vector>

#include "Entity.hpp"
#include "Inventory.hpp"
#include "Item.hpp"
#include "PlayerClass.hpp"
#include "Projectile.hpp"
#include "Vec2.hpp"

namespace game {

// Cosmetic element id carried by projectiles + spell VFX. Purely a tint hint the
// Renderer maps to a colour; the sim never branches on it. Kept as a plain int on
// the wire-facing structs so it costs nothing to ignore. (See Renderer::elementColor.)
enum SpellElement { ElemNeutral = 0, ElemFire, ElemFrost, ElemArcane, ElemLightning };

// A short-lived visual flourish the simulation emits and the Renderer draws +
// fades. Covers the spell shapes that leave no lasting world state — a Chain
// bolt's arc between foes, a Nova's expanding shock ring — so they read as
// something instead of an invisible instant. Decayed in Simulation::step; host-
// side only (not networked), matching the status-VFX policy.
struct SpellVfx {
    enum Kind { Arc, Ring };
    Kind kind = Arc;
    Vec2 a{};          // Arc: start point / Ring: centre
    Vec2 b{};          // Arc: end point (unused by Ring)
    float radius = 0.0f;  // Ring: peak radius it expands to
    float life = 0.0f;    // seconds remaining, counts toward 0
    float maxLife = 0.0f;  // initial life, so the Renderer can fade on life/maxLife
    int element = 0;
};

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
    Skill arcane;  // rises by casting with a magic weapon (staff/wand) — INT combat
    Skill heal;
    Skill dodge;
};

// Per-run level-up boons: the accumulated bonuses from the "pick 1 of 3 on
// level-up" choices. Separate from gear affixes (which come and go with
// equipment) and from stat points (the long-term RPG growth) — this is the
// survivor-like build layer that stacks toward the god fantasy. All are additive
// and fold into the derived-stat helpers in PlayerStats.hpp / the combat code.
struct Boons {
    int damagePct = 0;         // +% attack damage
    int attackSpeedPct = 0;    // +% attack speed
    int moveSpeedPct = 0;      // +% move speed
    int maxHpPct = 0;          // +% max HP
    int critPct = 0;           // +% crit chance
    int lifestealPct = 0;      // +% lifesteal
    int spellPowerPct = 0;     // +% auto-cast ability damage (caster scaling)
    int armorPct = 0;          // +% incoming damage reduction (the tanky build)
    int extraProjectiles = 0;  // +N ranged bolts per shot
    int pierce = 0;            // ranged bolts pass through +N enemies
    int regen = 0;             // passive HP regenerated per second
    int dodgePct = 0;          // +% chance to avoid an incoming hit
    int count = 0;             // how many boons taken (for the HUD)
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

    // Use-based mastery — rises as you perform the matching action. These broad
    // STYLE skills drive damage, the emergent title, stat drift and dodge/heal.
    Skills skills;

    // Per-weapon-class mastery (Mabinogi): each weapon FAMILY you swing ("sword",
    // "axe", "staff"…) climbs on its own, independent of the style skill above.
    // This is what gates spell discovery — pick up a fresh axe and you start its
    // mastery at 1, so only its early spells are offered until you practise it.
    // Keyed by Item::weaponClass. Not networked (shared-screen host owns it).
    std::unordered_map<std::string, Skill> weaponSkills;

    // Mastery level of a weapon family (1 if never trained; 0 for "no class").
    int masteryOf(const std::string& cls) const {
        if (cls.empty()) return 0;
        auto it = weaponSkills.find(cls);
        return it == weaponSkills.end() ? 1 : it->second.level;
    }

    // Per-run boons and the pending level-up choice. On level-up, pendingBoons is
    // bumped and boonChoices is rolled with 3 upgrade-pool ids to pick from; the
    // player's ChooseUpgrade command applies one and re-rolls or clears the offer.
    Boons boons;
    int pendingBoons = 0;                 // queued level-up choices awaiting a pick
    int boonChoices[3] = {-1, -1, -1};    // the 3 currently-offered choice ids

    // Auto-casting abilities drafted on level-up. Each fires on its own cooldown
    // (no key press) and grows with rank (re-picking the same one ranks it up).
    struct AbilityInst {
        int specId = -1;      // index into GameContent::abilityPool
        int rank = 1;         // stacks: re-picking the same ability raises this
        float cooldown = 0.0f;  // seconds until the next auto-cast
    };
    std::vector<AbilityInst> abilities;

    // Equipment drawn from the shared bank — the full paperdoll (see EquipSlot):
    // two weapon hands, five armour pieces, three jewellery slots. Each is
    // optional. When the player leaves, this gear flows back into the bank
    // ("fluid loot"). Index with EquipSlot; the two ring slots are Ring1/Ring2.
    struct Slot {
        bool has = false;
        Item item{};
    };
    Slot equipment[kEquipSlotCount];

    Slot& slot(EquipSlot s) { return equipment[static_cast<int>(s)]; }
    const Slot& slot(EquipSlot s) const { return equipment[static_cast<int>(s)]; }
    bool hasEquip(EquipSlot s) const { return equipment[static_cast<int>(s)].has; }
    const Item& equip(EquipSlot s) const { return equipment[static_cast<int>(s)].item; }

    // The main-hand weapon defines how you fight (style + damage characteristic).
    bool hasWeapon() const { return hasEquip(EquipSlot::MainHand); }
    const Item& weapon() const { return equip(EquipSlot::MainHand); }

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
    float regenBucket = 0.0f;     // accumulates fractional passive regen (Regen boon)

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

    // Transient spell flourishes (chain arcs, nova rings) the Renderer draws + the
    // sim fades each step. Purely cosmetic, host-side (see SpellVfx).
    std::vector<SpellVfx> vfx;

    int wave = 0;

    // Hit-stop: seconds the whole simulation is frozen so a heavy hit or a kill
    // lands with weight (the classic "juice" freeze-frame). Set by the damage
    // path, bled down in Simulation::step, which advances nothing while it's >0.
    // Purely a timing effect — snapshots simply stop changing, so clients see the
    // freeze for free without any netcode change.
    float hitStop = 0.0f;

    // Per-hit screen-shake accumulator: bumped by every landed blow (hurtEnemy),
    // decayed each step, folded into the Renderer's shake so connecting always
    // trembles the view a little — not only kills. Purely cosmetic.
    float hitShake = 0.0f;
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
