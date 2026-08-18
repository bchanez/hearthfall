#include "Net.hpp"

#include <gtest/gtest.h>

// Snapshot serialization must round-trip: what the host encodes is what the
// client decodes. (Sockets themselves aren't unit-tested here.)

namespace game {
namespace {

TEST(Net, should_round_trip_a_world_snapshot) {
    // given — a small hand-built world
    World w;
    w.wave = 3;
    w.gold = 120;

    Player p;
    p.cls = makeClass(ClassId::Healer);
    p.level = 4;
    p.entity.position = {100.0f, 200.0f};
    p.entity.hp = 55;
    p.entity.maxHp = 120;
    p.aim = {0.0f, 1.0f};
    p.shieldTimer = 1.5f;
    w.players.push_back(p);

    Entity e;
    e.alive = true;
    e.id = 42;
    e.position = {300.0f, 400.0f};
    e.hp = 10;
    e.maxHp = 40;
    e.targetPlayer = 0;
    e.level = 12;
    e.elite = true;
    w.enemies.push_back(e);

    GroundItem g;
    g.position = {50.0f, 60.0f};
    g.item.kind = ItemKind::Gold;
    w.loot.push_back(g);

    w.inventory.tryAdd({"Iron Shield", 8.0f, ItemKind::Armor, 20});

    // when
    const std::vector<uint8_t> bytes = net::encodeSnapshot(w);
    World out;
    ASSERT_TRUE(net::decodeSnapshot(bytes, out));

    // then
    EXPECT_EQ(out.wave, 3);
    EXPECT_EQ(out.gold, 120);
    ASSERT_EQ(out.players.size(), 1u);
    EXPECT_EQ(out.players[0].level, 4);
    EXPECT_EQ(out.players[0].cls.id, ClassId::Healer);
    EXPECT_FLOAT_EQ(out.players[0].entity.position.x, 100.0f);
    EXPECT_EQ(out.players[0].entity.hp, 55);
    EXPECT_FLOAT_EQ(out.players[0].shieldTimer, 1.5f);
    ASSERT_EQ(out.enemies.size(), 1u);
    EXPECT_EQ(out.enemies[0].targetPlayer, 0);
    EXPECT_EQ(out.enemies[0].id, 42);
    EXPECT_EQ(out.enemies[0].level, 12);
    EXPECT_TRUE(out.enemies[0].elite);
    ASSERT_EQ(out.loot.size(), 1u);
    EXPECT_EQ(out.loot[0].item.kind, ItemKind::Gold);
    EXPECT_EQ(out.inventory.size(), 1u);
}

TEST(Net, should_round_trip_an_input_state) {
    // given
    net::InputState in;
    in.move = {1.0f, -1.0f};
    in.aim = {0.0f, 1.0f};
    in.attack = true;
    in.ability = false;
    in.classSelect = 2;

    // when
    const std::vector<uint8_t> bytes = net::encodeInput(in);
    net::InputState out;
    ASSERT_TRUE(net::decodeInput(bytes, out));

    // then
    EXPECT_FLOAT_EQ(out.move.x, 1.0f);
    EXPECT_FLOAT_EQ(out.move.y, -1.0f);
    EXPECT_TRUE(out.attack);
    EXPECT_FALSE(out.ability);
    EXPECT_EQ(out.classSelect, 2);
}

}  // namespace
}  // namespace game
