#include "Simulation.hpp"

#include <gtest/gtest.h>

#include <algorithm>

// The simulation is pure C++ (no SDL), so it's directly unit-testable — one of
// the payoffs of the sim/renderer split.

namespace game {
namespace {

constexpr float kTick = 1.0f / 60.0f;

TEST(Simulation, should_start_with_one_player_and_a_first_wave) {
    // given / when
    const Simulation sim;

    // then
    EXPECT_EQ(sim.playerCount(), 1);
    EXPECT_EQ(sim.world().wave, 1);
    EXPECT_EQ(sim.world().enemies.size(), 6u);
}

TEST(Simulation, should_default_player_zero_to_the_archer_class) {
    // given / when
    const Simulation sim;

    // then
    EXPECT_EQ(sim.world().players[0].cls.id, ClassId::Archer);
    EXPECT_EQ(sim.world().players[0].entity.maxHp, 90);
}

TEST(Simulation, should_move_the_player_when_a_move_command_is_applied) {
    // given
    Simulation sim;
    const float startX = sim.world().players[0].entity.position.x;

    // when — push right for a bit
    sim.applyCommand({CommandType::Move, 0, {1.0f, 0.0f}, 0});
    for (int i = 0; i < 10; ++i) sim.step(kTick);

    // then
    EXPECT_GT(sim.world().players[0].entity.position.x, startX);
}

TEST(Simulation, should_spawn_a_projectile_when_archer_attacks) {
    // given
    Simulation sim;  // archer by default

    // when
    sim.applyCommand({CommandType::Attack, 0, {1.0f, 0.0f}, 0});
    sim.step(kTick);

    // then
    EXPECT_FALSE(sim.world().projectiles.empty());
}

TEST(Simulation, should_switch_stats_when_selecting_the_tank_class) {
    // given
    Simulation sim;

    // when
    sim.applyCommand({CommandType::SelectClass, 0, {}, 0});  // index 0 == Tank
    sim.step(kTick);

    // then
    EXPECT_EQ(sim.world().players[0].cls.id, ClassId::Tank);
    EXPECT_EQ(sim.world().players[0].entity.maxHp, 220);
    EXPECT_EQ(sim.world().players[0].cls.attackStyle, AttackStyle::Melee);
}

TEST(Simulation, should_add_a_second_player_that_moves_independently) {
    // given
    Simulation sim;
    const int p2 = sim.addPlayer(0);  // Tank
    ASSERT_EQ(p2, 1);
    const float p1StartX = sim.world().players[0].entity.position.x;
    const float p2StartX = sim.world().players[1].entity.position.x;

    // when — only player 2 moves right
    sim.applyCommand({CommandType::Move, 1, {1.0f, 0.0f}, 0});
    for (int i = 0; i < 10; ++i) sim.step(kTick);

    // then — player 2 moved, player 1 stayed put
    EXPECT_GT(sim.world().players[1].entity.position.x, p2StartX);
    EXPECT_FLOAT_EQ(sim.world().players[0].entity.position.x, p1StartX);
}

TEST(Simulation, should_pull_enemy_aggro_to_the_tank_when_it_taunts) {
    // given — P1 archer, P2 tank. The archer shoots to build some threat.
    Simulation sim;
    const int tank = sim.addPlayer(0);  // index 0 == Tank
    ASSERT_EQ(tank, 1);
    sim.applyCommand({CommandType::Attack, 0, {0.0f, -1.0f}, 0});  // archer fires up
    for (int i = 0; i < 30; ++i) sim.step(1.0f / 60.0f);

    // when — the tank taunts
    sim.applyCommand({CommandType::Ability, tank, {}, 0});
    sim.step(1.0f / 60.0f);

    // then — at least one enemy is now chasing the tank
    const auto& enemies = sim.world().enemies;
    const bool anyOnTank = std::any_of(enemies.begin(), enemies.end(), [&](const Entity& e) {
        return e.alive && e.targetPlayer == tank;
    });
    EXPECT_TRUE(anyOnTank);
}

TEST(Simulation, should_level_up_by_killing_and_match_a_joining_player) {
    // given — a tank standing at the centre; enemies walk in and get cleaved.
    Simulation sim;
    sim.applyCommand({CommandType::SelectClass, 0, {}, 0});  // Tank (melee)

    // when — hold attack for a while so waves are cleared
    for (int i = 0; i < 2000; ++i) {
        sim.applyCommand({CommandType::Attack, 0, {1.0f, 0.0f}, 0});
        sim.step(1.0f / 60.0f);
    }

    // then — the tank has gained levels...
    const int tankLevel = sim.world().players[0].level;
    EXPECT_GE(tankLevel, 2);

    // ...and a newly joining player is level-matched to the party.
    const int p2 = sim.addPlayer(1);  // Archer
    EXPECT_EQ(sim.world().players[p2].level, tankLevel);
}

TEST(Simulation, should_keep_level_when_switching_class) {
    // given — level the player up a bit
    Simulation sim;
    sim.applyCommand({CommandType::SelectClass, 0, {}, 0});  // Tank
    for (int i = 0; i < 1200; ++i) {
        sim.applyCommand({CommandType::Attack, 0, {1.0f, 0.0f}, 0});
        sim.step(1.0f / 60.0f);
    }
    const int level = sim.world().players[0].level;
    ASSERT_GE(level, 2);

    // when — switch class
    sim.applyCommand({CommandType::SelectClass, 0, {}, 1});  // Archer
    sim.step(1.0f / 60.0f);

    // then — level is preserved, class changed
    EXPECT_EQ(sim.world().players[0].level, level);
    EXPECT_EQ(sim.world().players[0].cls.id, ClassId::Archer);
}

TEST(Simulation, should_equip_gear_from_the_bank_and_return_it_on_leave) {
    // given — an archer and a weapon in the shared bank
    Simulation sim;
    const int baseDamage = sim.world().players[0].cls.attackDamage;
    sim.setBank(0, {{"Great Axe", 6.0f, ItemKind::Weapon, 30, 14, 0}});
    ASSERT_EQ(sim.world().inventory.size(), 1u);

    // when — equip best from the bank
    sim.applyCommand({CommandType::Equip, 0, {}, 0});
    sim.step(1.0f / 60.0f);

    // then — weapon left the bank and the player is stronger
    EXPECT_TRUE(sim.world().players[0].hasWeapon);
    EXPECT_EQ(sim.world().inventory.size(), 0u);
    // (effective damage isn't public, but the bonus is on the equipped item)
    EXPECT_EQ(sim.world().players[0].weapon.bonusDamage, 14);
    EXPECT_GT(baseDamage + sim.world().players[0].weapon.bonusDamage, baseDamage);

    // when — the player leaves
    sim.setPlayerActive(0, false);

    // then — the gear flows back to the shared bank (fluid loot)
    EXPECT_FALSE(sim.world().players[0].hasWeapon);
    EXPECT_EQ(sim.world().inventory.size(), 1u);
}

TEST(Simulation, should_apply_affix_max_hp_when_equipping) {
    // given — armor with a base bonus AND a MaxHp affix
    Simulation sim;
    sim.applyCommand({CommandType::SelectClass, 0, {}, 1});  // Archer
    const int baseMax = sim.world().players[0].entity.maxHp;
    Item shield{"Warded Shield", 8.0f, ItemKind::Armor, 20, 0, 40, Rarity::Rare};
    shield.affixes = {{AffixType::MaxHp, 25}};
    sim.setBank(0, {shield});

    // when
    sim.applyCommand({CommandType::Equip, 0, {}, 0});
    sim.step(1.0f / 60.0f);

    // then — base bonus (40) + affix (25)
    EXPECT_EQ(sim.world().players[0].entity.maxHp, baseMax + 65);
}

TEST(Simulation, should_add_armor_max_hp_when_equipped) {
    // given
    Simulation sim;
    sim.applyCommand({CommandType::SelectClass, 0, {}, 1});  // Archer
    const int baseMax = sim.world().players[0].entity.maxHp;
    sim.setBank(0, {{"Iron Shield", 8.0f, ItemKind::Armor, 20, 0, 40}});

    // when
    sim.applyCommand({CommandType::Equip, 0, {}, 0});
    sim.step(1.0f / 60.0f);

    // then
    EXPECT_EQ(sim.world().players[0].entity.maxHp, baseMax + 40);
}

TEST(Simulation, should_spawn_a_mixed_enemy_roster) {
    // given / when — a fresh sim's first wave
    const Simulation sim;

    // then — it isn't a single uniform blob type; variety makes fights tactical
    const auto& enemies = sim.world().enemies;
    ASSERT_FALSE(enemies.empty());
    const bool anyNonGrunt = std::any_of(enemies.begin(), enemies.end(),
                                         [](const Entity& e) { return e.type != EnemyType::Grunt; });
    EXPECT_TRUE(anyNonGrunt);
}

TEST(Simulation, should_flash_and_knock_back_an_enemy_when_hit) {
    // given — the default archer, aiming at whichever enemy is closest each frame
    Simulation sim;

    // when — fire repeatedly; a bolt eventually connects
    bool sawFlash = false;
    bool sawKnockback = false;
    for (int i = 0; i < 300 && !(sawFlash && sawKnockback); ++i) {
        const auto& es = sim.world().enemies;
        const auto nearest = std::min_element(es.begin(), es.end(), [&](const Entity& a, const Entity& b) {
            const Vec2 pp = sim.world().players[0].entity.position;
            return distanceSquared(pp, a.position) < distanceSquared(pp, b.position);
        });
        if (nearest != es.end()) {
            const Vec2 aim = (nearest->position - sim.world().players[0].entity.position).normalized();
            sim.applyCommand({CommandType::Attack, 0, aim, 0});
        }
        sim.step(1.0f / 60.0f);
        for (const auto& e : sim.world().enemies) {
            if (e.hitFlash > 0.0f) sawFlash = true;
            if (e.knockback.length() > 0.0f) sawKnockback = true;
        }
    }

    // then — hits produce the visible juice
    EXPECT_TRUE(sawFlash);
    EXPECT_TRUE(sawKnockback);
}

TEST(Simulation, should_reuse_an_inactive_slot_when_a_player_rejoins) {
    // given
    Simulation sim;
    const int p2 = sim.addPlayer(0);
    sim.setPlayerActive(p2, false);

    // when
    const int rejoined = sim.addPlayer(1);

    // then — same slot reused, no new one allocated
    EXPECT_EQ(rejoined, p2);
    EXPECT_EQ(sim.playerCount(), 2);
    EXPECT_TRUE(sim.world().players[p2].active);
}

}  // namespace
}  // namespace game
