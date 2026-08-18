#include "PlayerStats.hpp"

#include <gtest/gtest.h>

// Derived stats aggregate a player's equipped affixes. Pure and testable.

namespace game {
namespace {

Player withGear() {
    Player p;
    p.hasWeapon = true;
    p.weapon.bonusDamage = 10;
    p.weapon.affixes = {{AffixType::Damage, 5}, {AffixType::AttackSpeed, 20}, {AffixType::Crit, 10}};
    p.hasArmor = true;
    p.armor.bonusMaxHp = 40;
    p.armor.affixes = {{AffixType::MaxHp, 25}, {AffixType::Crit, 5}, {AffixType::MoveSpeed, 10}};
    return p;
}

TEST(PlayerStats, should_sum_affixes_across_both_slots) {
    const Player p = withGear();
    EXPECT_EQ(totalAffix(p, AffixType::Crit), 15);  // 10 + 5
}

TEST(PlayerStats, should_add_gear_damage_and_max_hp) {
    const Player p = withGear();
    EXPECT_EQ(gearDamage(p), 15);  // weapon bonus 10 + Damage affix 5
    EXPECT_EQ(gearMaxHp(p), 65);   // armor bonus 40 + MaxHp affix 25
}

TEST(PlayerStats, should_convert_percent_affixes_to_multipliers) {
    const Player p = withGear();
    EXPECT_FLOAT_EQ(attackCooldownMul(p), 0.8f);  // 20% attack speed
    EXPECT_FLOAT_EQ(moveSpeedMul(p), 1.1f);       // 10% move speed
    EXPECT_EQ(critChancePct(p), 15);
}

TEST(PlayerStats, should_cap_crit_and_attack_speed) {
    Player p;
    p.hasWeapon = true;
    p.weapon.affixes = {{AffixType::Crit, 200}, {AffixType::AttackSpeed, 200}};
    EXPECT_EQ(critChancePct(p), 100);             // capped
    EXPECT_FLOAT_EQ(attackCooldownMul(p), 0.25f);  // capped at 75% faster
}

TEST(PlayerStats, should_sum_and_cap_lifesteal) {
    Player p;
    p.hasWeapon = true;
    p.weapon.affixes = {{AffixType::Lifesteal, 8}};
    p.hasArmor = true;
    p.armor.affixes = {{AffixType::Lifesteal, 5}};
    EXPECT_EQ(lifestealPct(p), 13);  // 8 + 5

    p.weapon.affixes = {{AffixType::Lifesteal, 90}};
    p.hasArmor = false;
    EXPECT_EQ(lifestealPct(p), 60);  // capped
}

}  // namespace
}  // namespace game
