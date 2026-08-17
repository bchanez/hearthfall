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

}  // namespace
}  // namespace game
