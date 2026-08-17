#include "Vec2.hpp"

#include <gtest/gtest.h>

// Test naming follows the project convention: should_<outcome>_when_<condition>.
// These double as living documentation of Vec2's contract.

namespace game {
namespace {

TEST(Vec2, should_add_component_wise_when_using_plus_operator) {
    // given
    const Vec2 a{1.0f, 2.0f};
    const Vec2 b{3.0f, 4.0f};

    // when
    const Vec2 sum = a + b;

    // then
    EXPECT_FLOAT_EQ(sum.x, 4.0f);
    EXPECT_FLOAT_EQ(sum.y, 6.0f);
}

TEST(Vec2, should_return_unit_length_when_normalizing_non_zero_vector) {
    // given
    const Vec2 v{3.0f, 4.0f};  // length 5

    // when
    const Vec2 n = v.normalized();

    // then
    EXPECT_FLOAT_EQ(n.length(), 1.0f);
    EXPECT_FLOAT_EQ(n.x, 0.6f);
    EXPECT_FLOAT_EQ(n.y, 0.8f);
}

TEST(Vec2, should_return_zero_when_normalizing_zero_vector) {
    // given
    const Vec2 v{0.0f, 0.0f};

    // when
    const Vec2 n = v.normalized();

    // then — no division by zero, safe default
    EXPECT_FLOAT_EQ(n.x, 0.0f);
    EXPECT_FLOAT_EQ(n.y, 0.0f);
}

TEST(Vec2, should_compute_squared_distance_when_given_two_points) {
    // given
    const Vec2 a{0.0f, 0.0f};
    const Vec2 b{3.0f, 4.0f};

    // when / then — 3² + 4² = 25
    EXPECT_FLOAT_EQ(distanceSquared(a, b), 25.0f);
}

}  // namespace
}  // namespace game
