#pragma once

#include <vector>

#include "Item.hpp"
#include "PlayerClass.hpp"

namespace game {

// All data-driven content the Simulation needs: the playable classes (index
// order = selection order) and the loot table. Loaded from Lua at startup by
// ScriptEngine, or falls back to defaultContent() below (used in tests and if
// the data files are missing). The Simulation itself never touches Lua.
// A rollable affix: a stat and the magnitude range it draws from.
struct AffixSpec {
    AffixType type = AffixType::MaxHp;
    int minMag = 1;
    int maxMag = 1;
};

// A per-run level-up boon. On every level-up a player is offered a few of these
// and picks one — the survivor-like "pick 1 of 3" loop that layers build-defining
// power on top of the persistent RPG stats. What each effect does lives in
// Simulation::chooseUpgrade; the magnitude is how much it grants per pick (boons
// stack, so taking the same one twice doubles it).
enum class UpgradeEffect {
    DamagePct,       // +% attack damage
    AttackSpeedPct,  // +% attack speed (faster swings/shots)
    MoveSpeedPct,    // +% move speed
    MaxHpPct,        // +% max HP (and heals the difference on pick)
    CritPct,         // +% crit chance
    LifestealPct,    // +% of damage dealt returned as HP
    MultiShot,       // +N extra ranged bolts per shot (fanned)
    Pierce,          // ranged bolts pass through +N more enemies
    Regen,           // +N passive HP regenerated per second
    Dodge,           // +% chance to avoid an incoming hit
    SpellPowerPct,   // +% auto-cast ability damage (caster scaling)
    ArmorPct,        // +% incoming damage reduction (the tanky build)
};

struct UpgradeSpec {
    std::string name;         // shown in the choice box, e.g. "Sharpened Blade"
    std::string desc;         // one-liner, e.g. "+15% damage"
    UpgradeEffect effect = UpgradeEffect::DamagePct;
    int magnitude = 0;
};

inline UpgradeEffect upgradeEffectFromString(const std::string& s) {
    if (s == "attackSpeed") return UpgradeEffect::AttackSpeedPct;
    if (s == "moveSpeed") return UpgradeEffect::MoveSpeedPct;
    if (s == "maxHp") return UpgradeEffect::MaxHpPct;
    if (s == "crit") return UpgradeEffect::CritPct;
    if (s == "lifesteal") return UpgradeEffect::LifestealPct;
    if (s == "multiShot") return UpgradeEffect::MultiShot;
    if (s == "pierce") return UpgradeEffect::Pierce;
    if (s == "regen") return UpgradeEffect::Regen;
    if (s == "dodge") return UpgradeEffect::Dodge;
    if (s == "spellPower") return UpgradeEffect::SpellPowerPct;
    if (s == "armor") return UpgradeEffect::ArmorPct;
    return UpgradeEffect::DamagePct;
}

// An auto-casting active ability, drafted on level-up (offered alongside the
// passive boons, filtered by the wielder's weapon style). Once acquired it fires
// on its own cooldown — no key to press — so stacking several is the survivor-like
// god-fantasy. Picking one you already own ranks it up (stronger).
enum class AbilityEffect {
    Nova,    // AoE burst around you; magnitude = damage
    Volley,  // fire bolts in a ring; magnitude = bolt count
    Bolt,    // auto-fire a bolt at the nearest enemy; magnitude = damage
    Chain,   // lightning that leaps from the nearest foe to nearby others
};

// An on-hit rider layered on top of the delivery shape, so the same nova/bolt can
// feel wholly different: a mace stuns, a wand chills, an axe makes foes bleed.
enum class AbilityStatus { None, Burn, Slow, Stun, Knock };

struct AbilitySpec {
    std::string name;
    std::string desc;
    // Which weapon may be offered this spell, matched against the wielded weapon:
    //   a class ("sword", "axe", "staff"…) → only that weapon family
    //   a style ("melee", "ranged", "magic") → any weapon of that style
    //   "any" / empty → any weapon (and unarmed)
    std::string weapon = "any";
    AbilityEffect effect = AbilityEffect::Nova;
    float cooldown = 3.0f;  // seconds between auto-casts
    int magnitude = 10;
    // Mastery gate: only offered once the wielder's attack skill reaches this level
    // — the stronger a weapon's spells, the more you must PLAY that weapon first.
    int minSkill = 1;
    // On-hit status rider (see AbilityStatus). `statusDur` is its duration in
    // seconds; `statusPower` is the burn damage-per-second (ignored by the others).
    AbilityStatus status = AbilityStatus::None;
    float statusDur = 2.0f;
    int statusPower = 6;
};

inline AbilityStatus abilityStatusFromString(const std::string& s) {
    if (s == "burn" || s == "bleed") return AbilityStatus::Burn;
    if (s == "slow" || s == "chill") return AbilityStatus::Slow;
    if (s == "stun") return AbilityStatus::Stun;
    if (s == "knock" || s == "knockback") return AbilityStatus::Knock;
    return AbilityStatus::None;
}

// Does `spec` match a wielder of the given effective style + weapon class (empty
// when unarmed)? Class match takes precedence; style keywords broaden it.
inline bool abilityMatchesWeapon(const AbilitySpec& spec, AttackStyle style,
                                 const std::string& weaponClass) {
    const std::string& w = spec.weapon;
    if (w.empty() || w == "any") return true;
    if (w == "melee") return style == AttackStyle::Melee;
    if (w == "ranged") return style == AttackStyle::Ranged;
    if (w == "magic") return style == AttackStyle::Magic;
    return w == weaponClass;  // a specific weapon family
}

inline AbilityEffect abilityEffectFromString(const std::string& s) {
    if (s == "volley") return AbilityEffect::Volley;
    if (s == "bolt") return AbilityEffect::Bolt;
    if (s == "chain") return AbilityEffect::Chain;
    return AbilityEffect::Nova;
}

struct GameContent {
    std::vector<PlayerClass> classes;
    std::vector<Item> lootTable;
    std::vector<AffixSpec> affixPool;
    std::vector<UpgradeSpec> upgradePool;
    std::vector<AbilitySpec> abilityPool;
};

// Built-in fallback, mirroring the Lua data files. Keeping this in C++ means the
// game (and the tests) still run with no data/ directory present.
inline GameContent defaultContent() {
    GameContent c;
    c.classes = {
        makeClass(ClassId::Human),
        makeClass(ClassId::Tank),
        makeClass(ClassId::Archer),
        makeClass(ClassId::Healer),
    };
    //                name           weight kind              value +dmg +hp  style               slot                 hands rarity          affixes
    c.lootTable = {
        {"Gold Coins", 0.0f, ItemKind::Gold, 15, 0, 0, AttackStyle::Melee, EquipSlot::None, 0, Rarity::Common, {}},
        {"Health Potion", 0.5f, ItemKind::Potion, 25, 0, 0, AttackStyle::Melee, EquipSlot::None, 0, Rarity::Common, {}},
        {"Rusty Sword", 3.0f, ItemKind::Weapon, 8, 6, 0, AttackStyle::Melee, EquipSlot::MainHand, 1, Rarity::Common, {}},
        {"Short Bow", 2.0f, ItemKind::Weapon, 10, 7, 0, AttackStyle::Ranged, EquipSlot::MainHand, 2, Rarity::Common, {}},
        {"Leather Armor", 5.0f, ItemKind::Armor, 12, 0, 20, AttackStyle::Melee, EquipSlot::Chest, 0, Rarity::Common, {}},
        {"Iron Shield", 8.0f, ItemKind::Armor, 20, 0, 40, AttackStyle::Melee, EquipSlot::OffHand, 0, Rarity::Common, {}},
    };
    c.affixPool = {
        {AffixType::MaxHp, 10, 40},
        {AffixType::Damage, 3, 10},
        {AffixType::AttackSpeed, 5, 20},
        {AffixType::MoveSpeed, 5, 15},
        {AffixType::Crit, 3, 12},
        {AffixType::Lifesteal, 3, 10},
        {AffixType::SpellPower, 6, 18},
    };
    c.upgradePool = {
        {"Sharpened Blade", "+15% damage", UpgradeEffect::DamagePct, 15},
        {"Frenzy", "+12% attack speed", UpgradeEffect::AttackSpeedPct, 12},
        {"Fleet Feet", "+12% move speed", UpgradeEffect::MoveSpeedPct, 12},
        {"Vitality", "+20% max HP", UpgradeEffect::MaxHpPct, 20},
        {"Deadeye", "+8% crit chance", UpgradeEffect::CritPct, 8},
        {"Vampiric", "+6% lifesteal", UpgradeEffect::LifestealPct, 6},
        {"Split Shot", "+1 bolt per shot", UpgradeEffect::MultiShot, 1},
        {"Piercing Rounds", "bolts pierce +1 enemy", UpgradeEffect::Pierce, 1},
        {"Regeneration", "+3 HP per second", UpgradeEffect::Regen, 3},
        {"Evasion", "+8% dodge chance", UpgradeEffect::Dodge, 8},
        {"Bulwark", "+15% max HP", UpgradeEffect::MaxHpPct, 15},
    };
    c.abilityPool = {
        {"Nova Blast", "AoE burst around you", "any", AbilityEffect::Nova, 3.0f, 18, 1},
        {"Cleave Wave", "wide melee shockwave", "melee", AbilityEffect::Nova, 2.4f, 26, 1},
        {"Arrow Volley", "fire bolts all around", "ranged", AbilityEffect::Volley, 4.0f, 8, 1},
        {"Spirit Bolt", "auto-fire at nearest foe", "any", AbilityEffect::Bolt, 1.4f, 14, 1},
    };
    return c;
}

}  // namespace game
