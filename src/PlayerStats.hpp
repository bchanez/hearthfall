#pragma once

#include <algorithm>

#include "World.hpp"

namespace game {

// Derived stats aggregated from a player's equipped gear (weapon + armor
// affixes). Pure functions so they're easy to unit-test and reason about.

inline int totalAffix(const Player& p, AffixType type) {
    int sum = 0;
    if (p.hasWeapon)
        for (const auto& a : p.weapon.affixes)
            if (a.type == type) sum += a.magnitude;
    if (p.hasArmor)
        for (const auto& a : p.armor.affixes)
            if (a.type == type) sum += a.magnitude;
    return sum;
}

// Flat max-HP from gear (armor's base bonus + any MaxHp affixes).
inline int gearMaxHp(const Player& p) {
    int hp = totalAffix(p, AffixType::MaxHp);
    if (p.hasArmor) hp += p.armor.bonusMaxHp;
    return hp;
}

// Flat damage from gear (weapon's base bonus + any Damage affixes).
inline int gearDamage(const Player& p) {
    int dmg = totalAffix(p, AffixType::Damage);
    if (p.hasWeapon) dmg += p.weapon.bonusDamage;
    return dmg;
}

// Attack cooldown multiplier (<1 = faster). Attack-speed affixes *and* DEX
// (+1%/pt) fold in, capped so cooldown never collapses to zero.
inline float attackCooldownMul(const Player& p) {
    const int aspd = std::min(75, totalAffix(p, AffixType::AttackSpeed) + p.stats.dex);
    return 1.0f - static_cast<float>(aspd) / 100.0f;
}

// Move-speed multiplier (>1 = faster). MoveSpeed affixes + AGI (+2%/pt).
inline float moveSpeedMul(const Player& p) {
    return 1.0f + static_cast<float>(totalAffix(p, AffixType::MoveSpeed) + p.stats.agi * 2) / 100.0f;
}

// Crit chance in percent, capped at 100. Crit affixes + DEX (+0.5%/pt).
inline int critChancePct(const Player& p) {
    return std::min(100, totalAffix(p, AffixType::Crit) + p.stats.dex / 2);
}

// How this player fights right now: the equipped weapon's style, or the class's
// unarmed style when weaponless. The weapon drives identity — grab a bow, become
// an archer on the spot.
inline AttackStyle effectiveStyle(const Player& p) {
    return p.hasWeapon ? p.weapon.style : p.cls.attackStyle;
}

// Which characteristic drives your attack damage: melee → STR, ranged → DEX.
inline int primaryDamageStat(const Player& p) {
    return effectiveStyle(p) == AttackStyle::Melee ? p.stats.str : p.stats.dex;
}

// XP required to advance FROM `level` to the next. Linear ramp; shared by the
// sim (leveling), the netcode (snapshot) and the HUD (progress bar).
inline int xpForLevel(int level) { return level * 100; }

// XP to raise a skill FROM `level` to the next. Skills climb faster early, then
// slow — so a newcomer feels progress but mastery is a long road.
inline int skillXpForLevel(int level) { return 40 + level * 25; }

// The skill that governs your current attack (melee weapon → Melee, bow → Ranged).
inline int activeAttackSkill(const Player& p) {
    return effectiveStyle(p) == AttackStyle::Melee ? p.skills.melee.level : p.skills.ranged.level;
}

// Emergent "class" title from the highest-trained skill — the visible payoff of
// specializing by playing. Tiered by that skill's level.
inline const char* playerTitle(const Player& p) {
    const int m = p.skills.melee.level, r = p.skills.ranged.level;
    const int h = p.skills.heal.level, d = p.skills.dodge.level;
    const int top = std::max(std::max(m, r), std::max(h, d));
    if (top < 2) return "Novice";
    // Which skill leads (ties break melee > ranged > heal > dodge).
    const char* base = m >= top ? "Warrior" : r >= top ? "Archer" : h >= top ? "Cleric" : "Rogue";
    if (top >= 12) {
        return m >= top ? "Master Warrior" : r >= top ? "Master Archer"
                      : h >= top ? "Master Cleric" : "Master Rogue";
    }
    if (top >= 7) {
        return m >= top ? "Adept Warrior" : r >= top ? "Adept Archer"
                      : h >= top ? "Adept Cleric" : "Adept Rogue";
    }
    return base;
}

// Lifesteal in percent of damage dealt, capped so it can't fully sustain.
inline int lifestealPct(const Player& p) {
    return std::min(60, totalAffix(p, AffixType::Lifesteal));
}

}  // namespace game
