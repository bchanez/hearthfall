#include "Item.hpp"

#include <gtest/gtest.h>

// Rarity scaling is pure and deterministic, so it's directly testable.

namespace game {
namespace {

TEST(Item, should_scale_bonuses_and_prefix_name_by_rarity) {
    // given
    Item it{"Great Axe", 6.0f, ItemKind::Weapon, 30, 14, 0};

    // when
    applyRarity(it, Rarity::Rare);  // x2

    // then
    EXPECT_EQ(it.rarity, Rarity::Rare);
    EXPECT_EQ(it.bonusDamage, 28);
    EXPECT_EQ(it.value, 60);
    EXPECT_EQ(it.name, "Rare Great Axe");
}

TEST(Item, should_leave_common_items_unchanged) {
    // given
    Item it{"Rusty Sword", 3.0f, ItemKind::Weapon, 8, 6, 0};

    // when
    applyRarity(it, Rarity::Common);  // x1, no prefix

    // then
    EXPECT_EQ(it.name, "Rusty Sword");
    EXPECT_EQ(it.bonusDamage, 6);
    EXPECT_EQ(it.rarity, Rarity::Common);
}

TEST(Item, should_triple_epic_bonuses) {
    // given
    Item it{"Iron Shield", 8.0f, ItemKind::Armor, 20, 0, 40};

    // when
    applyRarity(it, Rarity::Epic);  // x3

    // then
    EXPECT_EQ(it.bonusMaxHp, 120);
    EXPECT_EQ(it.name, "Epic Iron Shield");
}

TEST(Item, should_scale_affix_count_with_rarity) {
    EXPECT_EQ(affixCountFor(Rarity::Common), 0);
    EXPECT_EQ(affixCountFor(Rarity::Uncommon), 1);
    EXPECT_EQ(affixCountFor(Rarity::Rare), 2);
    EXPECT_EQ(affixCountFor(Rarity::Epic), 3);
}

TEST(Item, should_format_affix_labels) {
    EXPECT_EQ(affixLabel({AffixType::Crit, 8}), "+8% Crit");
    EXPECT_EQ(affixLabel({AffixType::MaxHp, 25}), "+25 HP");
}

TEST(Item, should_fold_base_bonus_into_stat_total) {
    // given a weapon with a base damage bonus AND a Damage affix
    Item it{"Sword", 3.0f, ItemKind::Weapon, 10, 12, 0};
    it.affixes = {{AffixType::Damage, 5}, {AffixType::Crit, 8}};

    // then Damage folds the base bonus in; Crit is affix-only; others are zero
    EXPECT_EQ(itemStatTotal(it, AffixType::Damage), 17);  // 12 base + 5 affix
    EXPECT_EQ(itemStatTotal(it, AffixType::Crit), 8);
    EXPECT_EQ(itemStatTotal(it, AffixType::MaxHp), 0);
}

TEST(Item, should_restrict_affixes_by_slot) {
    // Offense on weapons, defense on body armour, either on the wildcard slots.
    EXPECT_TRUE(slotAllowsAffix(EquipSlot::MainHand, AffixType::Damage));
    EXPECT_FALSE(slotAllowsAffix(EquipSlot::MainHand, AffixType::MaxHp));
    EXPECT_TRUE(slotAllowsAffix(EquipSlot::Chest, AffixType::MaxHp));
    EXPECT_FALSE(slotAllowsAffix(EquipSlot::Chest, AffixType::Crit));
    EXPECT_TRUE(slotAllowsAffix(EquipSlot::Ring1, AffixType::Crit));    // jewellery: any
    EXPECT_TRUE(slotAllowsAffix(EquipSlot::Ring1, AffixType::MaxHp));
    EXPECT_TRUE(slotAllowsAffix(EquipSlot::OffHand, AffixType::SpellPower));  // a focus
}

TEST(Item, should_scale_affix_magnitude_with_rarity) {
    EXPECT_FLOAT_EQ(rarityAffixMul(Rarity::Uncommon), 1.0f);
    EXPECT_GT(rarityAffixMul(Rarity::Rare), rarityAffixMul(Rarity::Uncommon));
    EXPECT_GT(rarityAffixMul(Rarity::Epic), rarityAffixMul(Rarity::Rare));
}

TEST(Item, should_rank_a_stronger_weapon_higher_by_power) {
    Item weak{"Dagger", 1.0f, ItemKind::Weapon, 0, 8, 0};
    Item strong{"Greatsword", 5.0f, ItemKind::Weapon, 0, 14, 0};
    strong.affixes = {{AffixType::Crit, 8}};

    // 8*2=16  vs  14*2 + 8*2 = 44
    EXPECT_EQ(itemPower(weak), 16);
    EXPECT_EQ(itemPower(strong), 44);
    EXPECT_GT(itemPower(strong), itemPower(weak));
}

}  // namespace
}  // namespace game
