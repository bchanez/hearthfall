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

}  // namespace
}  // namespace game
