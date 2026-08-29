#include "Inventory.hpp"

#include <gtest/gtest.h>

#include "Item.hpp"

namespace game {
namespace {

Item weapon(float weight) { return {"Test Weapon", weight, ItemKind::Weapon, 1}; }

TEST(Inventory, should_start_empty_with_zero_weight) {
    // given
    const Inventory inv{10.0f};

    // then
    EXPECT_EQ(inv.size(), 0u);
    EXPECT_FLOAT_EQ(inv.currentWeight(), 0.0f);
}

TEST(Inventory, should_accept_item_when_it_fits_under_the_cap) {
    // given
    Inventory inv{10.0f};

    // when
    const bool added = inv.tryAdd(weapon(4.0f));

    // then
    EXPECT_TRUE(added);
    EXPECT_EQ(inv.size(), 1u);
    EXPECT_FLOAT_EQ(inv.currentWeight(), 4.0f);
}

TEST(Inventory, should_reject_item_when_it_would_exceed_the_cap) {
    // given
    Inventory inv{10.0f};
    inv.tryAdd(weapon(7.0f));

    // when
    const bool added = inv.tryAdd(weapon(5.0f));  // 7 + 5 > 10

    // then
    EXPECT_FALSE(added);
    EXPECT_EQ(inv.size(), 1u);
    EXPECT_FLOAT_EQ(inv.currentWeight(), 7.0f);
}

TEST(Inventory, should_accept_item_when_it_fills_the_cap_exactly) {
    // given
    Inventory inv{10.0f};

    // when
    const bool added = inv.tryAdd(weapon(10.0f));

    // then
    EXPECT_TRUE(added);
    EXPECT_FLOAT_EQ(inv.currentWeight(), 10.0f);
}

TEST(Inventory, should_force_add_past_the_cap) {
    // given — a bank already filled to its cap
    Inventory inv{10.0f};
    inv.tryAdd(weapon(10.0f));
    ASSERT_FALSE(inv.tryAdd(weapon(1.0f)));  // no room via the normal path

    // when — a consumable is force-added (potions bypass the cap on pickup)
    inv.forceAdd({"Health Potion", 0.5f, ItemKind::Potion, 25});

    // then — it went in regardless of the cap
    EXPECT_EQ(inv.size(), 2u);
    EXPECT_FLOAT_EQ(inv.currentWeight(), 10.5f);
}

TEST(Inventory, should_stack_identical_potions_into_one_slot) {
    // given
    Inventory inv{100.0f};
    const Item potion{"Health Potion", 0.5f, ItemKind::Potion, 25};

    // when — three identical potions are added
    inv.forceAdd(potion);
    inv.tryAdd(potion);
    inv.forceAdd(potion);

    // then — one slot, count 3, weight counts every unit
    ASSERT_EQ(inv.size(), 1u);
    EXPECT_EQ(inv.items()[0].count, 3);
    EXPECT_FLOAT_EQ(inv.currentWeight(), 1.5f);  // 3 x 0.5

    // when — consumed one at a time
    inv.consumeOne(0);
    EXPECT_EQ(inv.items()[0].count, 2);
    inv.consumeOne(0);
    inv.consumeOne(0);  // the last one drops the slot
    EXPECT_EQ(inv.size(), 0u);
}

TEST(Inventory, should_not_stack_potions_of_different_rarity) {
    Inventory inv{100.0f};
    Item common{"Health Potion", 0.5f, ItemKind::Potion, 25};
    Item epic = common;
    epic.rarity = Rarity::Epic;
    inv.forceAdd(common);
    inv.forceAdd(epic);
    EXPECT_EQ(inv.size(), 2u);  // different rarity → different stacks
}

}  // namespace
}  // namespace game
