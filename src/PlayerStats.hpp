#pragma once

#include <algorithm>

#include "World.hpp"

namespace game {

// Derived stats aggregated from a player's equipped gear (weapon + armor
// affixes). Pure functions so they're easy to unit-test and reason about.

// Sum an affix type across every equipped paperdoll slot (weapon hands, armour,
// jewellery) — the whole build contributes, not just weapon + chest.
inline int totalAffix(const Player& p, AffixType type) {
    int sum = 0;
    for (int i = 0; i < kEquipSlotCount; ++i) {
        const auto& s = p.equipment[i];
        if (!s.has) continue;
        for (const auto& a : s.item.affixes)
            if (a.type == type) sum += a.magnitude;
    }
    return sum;
}

// Flat max-HP from gear: every slot's base bonusMaxHp + any MaxHp affixes.
inline int gearMaxHp(const Player& p) {
    int hp = totalAffix(p, AffixType::MaxHp);
    for (int i = 0; i < kEquipSlotCount; ++i)
        if (p.equipment[i].has) hp += p.equipment[i].item.bonusMaxHp;
    return hp;
}

// Flat damage from gear: every slot's base bonusDamage + any Damage affixes.
inline int gearDamage(const Player& p) {
    int dmg = totalAffix(p, AffixType::Damage);
    for (int i = 0; i < kEquipSlotCount; ++i)
        if (p.equipment[i].has) dmg += p.equipment[i].item.bonusDamage;
    return dmg;
}

// Attack cooldown multiplier (<1 = faster). Attack-speed affixes *and* DEX
// (+1%/pt) fold in, capped so cooldown never collapses to zero.
inline float attackCooldownMul(const Player& p) {
    const int aspd =
        std::min(75, totalAffix(p, AffixType::AttackSpeed) + p.stats.dex + p.boons.attackSpeedPct);
    return 1.0f - static_cast<float>(aspd) / 100.0f;
}

// Encumbrance: your equipped gear (weapon + armor) has weight, weighed against a
// STR-based carry allowance. Returns a move-speed multiplier <=1 (1 = unburdened).
// Gentle and floored so a loaded body is slower and vulnerable — the seed of the
// co-op "cover me while I gear up" moment — but never frozen. A light archer kit
// stays fast; heavy plate on a low-STR body drags.
inline float encumbranceMul(const Player& p) {
    constexpr float kCarryFreeBase = 6.0f;  // kg carried free of any penalty
    constexpr float kCarryPerStr = 1.5f;    // + allowance per STR point
    constexpr float kPenaltyPerKg = 0.03f;  // -3% move speed per excess kg
    constexpr float kFloor = 0.6f;          // never slower than 60% of base
    float w = 0.0f;
    for (int i = 0; i < kEquipSlotCount; ++i)
        if (p.equipment[i].has) w += p.equipment[i].item.weight;
    const float allowance = kCarryFreeBase + static_cast<float>(p.stats.str) * kCarryPerStr;
    const float excess = w - allowance;
    if (excess <= 0.0f) return 1.0f;
    return std::max(kFloor, 1.0f - excess * kPenaltyPerKg);
}

// Move-speed multiplier (>1 = faster). MoveSpeed affixes + AGI (+2%/pt) + boons.
inline float moveSpeedMul(const Player& p) {
    return 1.0f + static_cast<float>(totalAffix(p, AffixType::MoveSpeed) + p.stats.agi * 2 +
                                     p.boons.moveSpeedPct) /
                      100.0f;
}

// Crit chance in percent, capped at 100. Crit affixes + DEX (+0.5%/pt) + boons.
inline int critChancePct(const Player& p) {
    return std::min(100, totalAffix(p, AffixType::Crit) + p.stats.dex / 2 + p.boons.critPct);
}

// How this player fights right now: the equipped weapon's style, or the class's
// unarmed style when weaponless. The weapon drives identity — grab a bow, become
// an archer on the spot.
inline AttackStyle effectiveStyle(const Player& p) {
    return p.hasWeapon() ? p.weapon().style : p.cls.attackStyle;
}

// Which characteristic drives your attack damage: melee → STR, ranged → DEX,
// magic → INT (a staff/wand makes you cast off your mind).
inline int primaryDamageStat(const Player& p) {
    switch (effectiveStyle(p)) {
        case AttackStyle::Melee:  return p.stats.str;
        case AttackStyle::Ranged: return p.stats.dex;
        case AttackStyle::Magic:  return p.stats.intel;
    }
    return p.stats.str;
}

// XP required to advance FROM `level` to the next. Linear ramp; shared by the
// sim (leveling), the netcode (snapshot) and the HUD (progress bar).
inline int xpForLevel(int level) { return level * 100; }

// XP to raise a skill FROM `level` to the next. Skills climb faster early, then
// slow — so a newcomer feels progress but mastery is a long road.
inline int skillXpForLevel(int level) { return 40 + level * 25; }

// The skill that governs your current attack (melee weapon → Melee, bow → Ranged).
inline int activeAttackSkill(const Player& p) {
    switch (effectiveStyle(p)) {
        case AttackStyle::Melee:  return p.skills.melee.level;
        case AttackStyle::Ranged: return p.skills.ranged.level;
        case AttackStyle::Magic:  return p.skills.arcane.level;  // casting mastery
    }
    return p.skills.melee.level;
}

// Emergent "class" title from the highest-trained skill — the visible payoff of
// specializing by playing. Tiered by that skill's level.
inline const char* playerTitle(const Player& p) {
    const int m = p.skills.melee.level, r = p.skills.ranged.level, a = p.skills.arcane.level;
    const int h = p.skills.heal.level, d = p.skills.dodge.level;
    const int top = std::max(std::max(std::max(m, r), a), std::max(h, d));
    if (top < 2) return "Novice";
    // Which skill leads (ties break melee > ranged > arcane > heal > dodge).
    const int lead = m >= top ? 0 : r >= top ? 1 : a >= top ? 2 : h >= top ? 3 : 4;
    static const char* const kNames[3][5] = {
        {"Warrior", "Archer", "Mage", "Cleric", "Rogue"},                    // base
        {"Adept Warrior", "Adept Archer", "Adept Mage", "Adept Cleric", "Adept Rogue"},
        {"Master Warrior", "Master Archer", "Master Mage", "Master Cleric", "Master Rogue"},
    };
    const int tier = top >= 12 ? 2 : top >= 7 ? 1 : 0;
    return kNames[tier][lead];
}

// Lifesteal in percent of damage dealt, capped so it can't fully sustain.
inline int lifestealPct(const Player& p) {
    return std::min(60, totalAffix(p, AffixType::Lifesteal) + p.boons.lifestealPct);
}

// Spell power in percent — how much your auto-cast abilities hit ABOVE their base
// magnitude. Fed by INT (+2%/pt, the caster characteristic), SpellPower affixes
// (staves, focuses, jewellery) and the SpellPower boon. A wand+tome INT build
// turns modest spells into artillery. Pure so castAbility can fold it in.
inline int spellPowerPct(const Player& p) {
    return p.stats.intel * 2 + totalAffix(p, AffixType::SpellPower) + p.boons.spellPowerPct;
}

// Incoming-damage reduction in percent, capped so you can't become invulnerable.
// The tanky playstyle is now a BUILD (the Ironhide boon) rather than a class.
inline int armorPct(const Player& p) { return std::min(75, p.boons.armorPct); }

}  // namespace game
