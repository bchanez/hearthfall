#include "PlayerStats.hpp"

#include <gtest/gtest.h>

// Derived stats aggregate a player's equipped affixes. Pure and testable.

namespace game {
namespace {

// Put an item onto a paperdoll slot for the test (bypasses the wield rules).
void give(Player& p, EquipSlot s, const Item& it) { p.slot(s) = Player::Slot{true, it}; }

Player withGear() {
    Player p;
    Item wpn;
    wpn.bonusDamage = 10;
    wpn.affixes = {{AffixType::Damage, 5}, {AffixType::AttackSpeed, 20}, {AffixType::Crit, 10}};
    give(p, EquipSlot::MainHand, wpn);
    Item arm;
    arm.bonusMaxHp = 40;
    arm.affixes = {{AffixType::MaxHp, 25}, {AffixType::Crit, 5}, {AffixType::MoveSpeed, 10}};
    give(p, EquipSlot::Chest, arm);
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
    Item wpn;
    wpn.affixes = {{AffixType::Crit, 200}, {AffixType::AttackSpeed, 200}};
    give(p, EquipSlot::MainHand, wpn);
    EXPECT_EQ(critChancePct(p), 100);             // capped
    EXPECT_FLOAT_EQ(attackCooldownMul(p), 0.25f);  // capped at 75% faster
}

TEST(PlayerStats, should_title_a_caster_as_mage) {
    Player p;
    p.skills.arcane.level = 8;  // arcane leads → Mage, Adept tier (>=7)
    EXPECT_STREQ(playerTitle(p), "Adept Mage");
    p.skills.melee.level = 9;  // melee now leads → Warrior
    EXPECT_STREQ(playerTitle(p), "Adept Warrior");
}

TEST(PlayerStats, should_sum_spell_power_from_int_affixes_and_boon) {
    Player p;
    p.stats.intel = 5;  // +2%/pt = +10%
    Item staff;
    staff.affixes = {{AffixType::SpellPower, 25}};  // +25%
    give(p, EquipSlot::MainHand, staff);
    p.boons.spellPowerPct = 15;  // +15%
    EXPECT_EQ(spellPowerPct(p), 50);
}

TEST(PlayerStats, should_sum_and_cap_lifesteal) {
    Player p;
    Item wpn;
    wpn.affixes = {{AffixType::Lifesteal, 8}};
    give(p, EquipSlot::MainHand, wpn);
    Item arm;
    arm.affixes = {{AffixType::Lifesteal, 5}};
    give(p, EquipSlot::Chest, arm);
    EXPECT_EQ(lifestealPct(p), 13);  // 8 + 5

    Item big;
    big.affixes = {{AffixType::Lifesteal, 90}};
    give(p, EquipSlot::MainHand, big);
    p.slot(EquipSlot::Chest) = Player::Slot{};  // unequip armour
    EXPECT_EQ(lifestealPct(p), 60);  // capped
}

}  // namespace
}  // namespace game
