#include "ScriptEngine.hpp"

#include <gtest/gtest.h>

#include <cstdio>

// The shared bank must survive a save -> load cycle.

namespace game {
namespace {

TEST(Save, should_round_trip_the_bank_through_a_file) {
    const std::string path = "/tmp/jeu_save_test.lua";
    std::remove(path.c_str());

    // given
    SaveState in;
    in.gold = 250;
    in.items = {
        {"Iron Shield", 8.0f, ItemKind::Armor, 20},
        {"Health Potion", 0.5f, ItemKind::Potion, 25},
    };

    // when
    ScriptEngine::saveState(path, in);
    const SaveState out = ScriptEngine::loadState(path);

    // then
    EXPECT_EQ(out.gold, 250);
    ASSERT_EQ(out.items.size(), 2u);
    EXPECT_EQ(out.items[0].name, "Iron Shield");
    EXPECT_EQ(out.items[0].kind, ItemKind::Armor);
    EXPECT_FLOAT_EQ(out.items[1].weight, 0.5f);

    std::remove(path.c_str());
}

TEST(Save, should_return_empty_state_when_file_is_missing) {
    const SaveState out = ScriptEngine::loadState("/tmp/does_not_exist_jeu.lua");
    EXPECT_EQ(out.gold, 0);
    EXPECT_TRUE(out.items.empty());
}

}  // namespace
}  // namespace game
