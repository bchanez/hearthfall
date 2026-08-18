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
    // The first wave is 6 enemies; idle world mobs (zone population) are extra.
    const auto& es = sim.world().enemies;
    const auto waveCount = std::count_if(es.begin(), es.end(),
                                         [](const Entity& e) { return !e.worldMob; });
    EXPECT_EQ(waveCount, 6);
}

TEST(Simulation, should_default_player_zero_to_the_human_class) {
    // given / when
    const Simulation sim;

    // then — everyone starts as a neutral Human (classless direction)
    EXPECT_EQ(sim.world().players[0].cls.id, ClassId::Human);
    EXPECT_EQ(sim.world().players[0].entity.maxHp, 130);
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
    // given — an unarmed archer (ranged unarmed style)
    Simulation sim;
    sim.applyCommand({CommandType::SelectClass, 0, {}, 2});  // index 2 == Archer

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
    sim.applyCommand({CommandType::SelectClass, 0, {}, 1});  // index 1 == Tank
    sim.step(kTick);

    // then
    EXPECT_EQ(sim.world().players[0].cls.id, ClassId::Tank);
    EXPECT_EQ(sim.world().players[0].entity.maxHp, 220);
    EXPECT_EQ(sim.world().players[0].cls.attackStyle, AttackStyle::Melee);
}

TEST(Simulation, should_add_a_second_player_that_moves_independently) {
    // given
    Simulation sim;
    const int p2 = sim.addPlayer(0);  // Human
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
    sim.applyCommand({CommandType::SelectClass, 0, {}, 2});  // P1 → Archer (index 2)
    const int tank = sim.addPlayer(1);  // index 1 == Tank
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
    sim.applyCommand({CommandType::SelectClass, 0, {}, 1});  // Tank (index 1, melee)

    // when — hold attack for a while so waves are cleared
    for (int i = 0; i < 2000; ++i) {
        sim.applyCommand({CommandType::Attack, 0, {1.0f, 0.0f}, 0});
        sim.step(1.0f / 60.0f);
    }

    // then — the tank has gained levels...
    const int tankLevel = sim.world().players[0].level;
    EXPECT_GE(tankLevel, 2);

    // ...and a newly joining player is level-matched to the party.
    const int p2 = sim.addPlayer(2);  // Archer
    EXPECT_EQ(sim.world().players[p2].level, tankLevel);
}

TEST(Simulation, should_bank_a_point_on_level_up_and_spend_it_into_a_stat) {
    // given — a tank that levels up by clearing waves
    Simulation sim;
    sim.applyCommand({CommandType::SelectClass, 0, {}, 1});  // Tank (index 1)
    for (int i = 0; i < 1200; ++i) {
        sim.applyCommand({CommandType::Attack, 0, {1.0f, 0.0f}, 0});
        sim.step(1.0f / 60.0f);
    }
    ASSERT_GE(sim.world().players[0].level, 2);

    // then — leveling banked spendable points (one per level gained)
    const int points = sim.world().players[0].unspentPoints;
    EXPECT_GE(points, 1);
    const int maxHpBefore = sim.world().players[0].entity.maxHp;

    // when — spend one point into VIT (stat index 3)
    sim.applyCommand({CommandType::AllocStat, 0, {}, 3});
    sim.step(1.0f / 60.0f);

    // then — the point is consumed and max HP grew
    EXPECT_EQ(sim.world().players[0].unspentPoints, points - 1);
    EXPECT_GT(sim.world().players[0].entity.maxHp, maxHpBefore);
    EXPECT_EQ(sim.world().players[0].stats.vit, 1);
}

TEST(Simulation, should_raise_the_melee_skill_by_swinging) {
    // given — a melee tank
    Simulation sim;
    sim.applyCommand({CommandType::SelectClass, 0, {}, 1});  // Tank (melee)
    ASSERT_EQ(sim.world().players[0].skills.melee.level, 1);

    // when — swing a lot (each hit trains the blade)
    for (int i = 0; i < 600; ++i) {
        sim.applyCommand({CommandType::Attack, 0, {1.0f, 0.0f}, 0});
        sim.step(1.0f / 60.0f);
    }

    // then — Melee mastery has climbed above the starting level
    EXPECT_GT(sim.world().players[0].skills.melee.level, 1);
}

TEST(Simulation, should_split_a_slime_into_two_smaller_ones_on_death) {
    // given — a tank with a single low-HP slime injected right next to it
    Simulation sim;
    sim.applyCommand({CommandType::SelectClass, 0, {}, 1});  // Tank (melee)
    World& w = const_cast<World&>(sim.world());
    const Vec2 tankPos = w.players[0].entity.position;
    w.enemies.clear();
    Entity slime;
    slime.type = EnemyType::Slime;
    slime.radius = 18.0f;  // big enough to split
    slime.position = tankPos + Vec2{40.0f, 0.0f};  // inside melee reach
    slime.hp = slime.maxHp = 5;  // dies in one swing
    slime.aggroed = true;
    slime.id = 999;
    w.enemies.push_back(slime);

    // when — one swing (a few steps, well under the attack cooldown) kills it and
    // triggers the split; we stop before a second swing can clean up the children.
    for (int i = 0; i < 5; ++i) {
        sim.applyCommand({CommandType::Attack, 0, {1.0f, 0.0f}, 0});
        sim.step(1.0f / 60.0f);
    }

    // then — the big slime is gone and two smaller slimes took its place
    const auto& es = sim.world().enemies;
    const int small = static_cast<int>(std::count_if(es.begin(), es.end(), [](const Entity& e) {
        return e.alive && e.type == EnemyType::Slime && e.radius <= 11.0f;
    }));
    EXPECT_EQ(small, 2);
}

TEST(Simulation, should_let_an_enemy_bolt_hurt_a_player) {
    // given — a player and a hostile projectile aimed at them (built via the sim's
    // own contact path would be indirect; here we assert the substrate: a hostile
    // bolt in the world reduces the player's HP as it passes through).
    Simulation sim;
    Player& p = const_cast<World&>(sim.world()).players[0];
    p.entity.position = {500.0f, 500.0f};
    p.invuln = 0.0f;
    const int hpBefore = p.entity.hp;

    // A hostile bolt sitting on the player.
    Projectile bolt;
    bolt.position = {500.0f, 500.0f};
    bolt.velocity = {1.0f, 0.0f};
    bolt.damage = 12;
    bolt.radius = 6.0f;
    bolt.life = 1.0f;
    bolt.hostile = true;
    const_cast<World&>(sim.world()).projectiles.push_back(bolt);

    // when
    sim.step(1.0f / 60.0f);

    // then — the hostile bolt took a bite out of the player
    EXPECT_LT(sim.world().players[0].entity.hp, hpBefore);
}

TEST(Simulation, should_ignore_alloc_when_no_points_banked) {
    // given — a fresh level-1 player (no banked points)
    Simulation sim;
    ASSERT_EQ(sim.world().players[0].unspentPoints, 0);
    const int strBefore = sim.world().players[0].stats.str;

    // when — trying to spend a point anyway
    sim.applyCommand({CommandType::AllocStat, 0, {}, 0});  // STR
    sim.step(1.0f / 60.0f);

    // then — nothing changes
    EXPECT_EQ(sim.world().players[0].stats.str, strBefore);
}

TEST(Simulation, should_keep_level_when_switching_class) {
    // given — level the player up a bit
    Simulation sim;
    sim.applyCommand({CommandType::SelectClass, 0, {}, 1});  // Tank (index 1)
    for (int i = 0; i < 1200; ++i) {
        sim.applyCommand({CommandType::Attack, 0, {1.0f, 0.0f}, 0});
        sim.step(1.0f / 60.0f);
    }
    const int level = sim.world().players[0].level;
    ASSERT_GE(level, 2);

    // when — switch class
    sim.applyCommand({CommandType::SelectClass, 0, {}, 2});  // Archer (index 2)
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
    sim.applyCommand({CommandType::SelectClass, 0, {}, 2});  // Archer (index 2)
    const int baseMax = sim.world().players[0].entity.maxHp;
    Item shield{"Warded Shield", 8.0f, ItemKind::Armor, 20, 0, 40, AttackStyle::Melee, Rarity::Rare};
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
    sim.applyCommand({CommandType::SelectClass, 0, {}, 2});  // Archer (index 2)
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

TEST(Simulation, should_engage_the_sole_player_with_every_wave_enemy) {
    // given — a fresh sim: one player, one wave of enemies.
    Simulation sim;

    // when — a single step lets aggro resolve.
    sim.step(kTick);

    // then — wave enemies engage on arrival: every live enemy targets P0 and
    // has a home to leash back to (aggro system wired end to end).
    const auto& enemies = sim.world().enemies;
    ASSERT_FALSE(enemies.empty());
    for (const auto& e : enemies) {
        if (!e.alive || e.worldMob) continue;  // idle world mobs stay unaggroed by design
        EXPECT_TRUE(e.aggroed);
        EXPECT_EQ(e.targetPlayer, 0);
    }
}

TEST(Simulation, should_dash_a_burst_with_iframes_then_cooldown) {
    // given — an archer at rest.
    Simulation sim;
    const Vec2 start = sim.world().players[0].entity.position;

    // when — dash to the right for a couple of frames.
    sim.applyCommand({CommandType::Dash, 0, {1.0f, 0.0f}, 0});
    sim.step(kTick);
    const Vec2 afterOne = sim.world().players[0].entity.position;

    // then — it moved much further than a normal walk step, and granted i-frames.
    const float dashStep = (afterOne - start).length();
    EXPECT_GT(dashStep, sim.world().players[0].entity.speed * kTick * 3.0f);
    EXPECT_GT(sim.world().players[0].invuln, 0.0f);

    // and — a second dash is on cooldown right after the first.
    for (int i = 0; i < 20; ++i) sim.step(kTick);  // let the dash burst end
    const Vec2 before = sim.world().players[0].entity.position;
    sim.applyCommand({CommandType::Dash, 0, {1.0f, 0.0f}, 0});
    sim.step(kTick);
    const float moved = (sim.world().players[0].entity.position - before).length();
    EXPECT_LT(moved, sim.world().players[0].entity.speed * kTick * 3.0f);  // no burst
}

TEST(Simulation, should_scale_the_wave_up_with_party_size) {
    // given — a two-player party, both tanks cleaving the first wave.
    Simulation sim;
    sim.applyCommand({CommandType::SelectClass, 0, {}, 0});  // Tank
    const int p2 = sim.addPlayer(0);                          // second Tank
    ASSERT_EQ(p2, 1);

    // when — clear wave 1 so wave 2 spawns under party scaling.
    for (int i = 0; i < 6000 && sim.world().wave < 2; ++i) {
        sim.applyCommand({CommandType::Attack, 0, {1.0f, 0.0f}, 0});
        sim.applyCommand({CommandType::Attack, p2, {-1.0f, 0.0f}, 0});
        sim.step(kTick);
    }
    ASSERT_EQ(sim.world().wave, 2);

    // then — wave 2 for two players is bigger than the solo baseline (5 + wave).
    const auto& es = sim.world().enemies;
    const auto waveCount = std::count_if(es.begin(), es.end(),
                                         [](const Entity& e) { return !e.worldMob; });
    EXPECT_GT(waveCount, 5 + 2);  // solo wave-2 would be 7; party adds more
}

TEST(Simulation, should_spawn_elite_world_mobs) {
    // given / when — a fresh sim populates the outer world.
    const Simulation sim;

    // then — at least one world mob is an elite (golden-aura target of desire).
    const auto& es = sim.world().enemies;
    const bool anyElite =
        std::any_of(es.begin(), es.end(), [](const Entity& e) { return e.worldMob && e.elite; });
    EXPECT_TRUE(anyElite);
}

TEST(Simulation, should_spawn_predators_that_hunt_and_eat_other_mobs) {
    // given — a fresh world. Some rare mobs are predators: hyper-aggressive
    // hunters that stalk OTHER monsters rather than waiting for the player.
    Simulation sim;
    const auto& es = sim.world().enemies;
    const bool anyPredator =
        std::any_of(es.begin(), es.end(), [](const Entity& e) { return e.predator; });
    ASSERT_TRUE(anyPredator);

    // when — the world runs autonomously for a while (player stays put at spawn,
    // far from the outer-world predators), letting the food chain play out.
    for (int i = 0; i < 3000; ++i) sim.step(1.0f / 60.0f);

    // then — at least one predator has eaten a mob and grown from it: proof the
    // environment is alive, not a static roster. An eater carries what it's eaten.
    const auto& after = sim.world().enemies;
    const auto eater = std::find_if(after.begin(), after.end(),
                                    [](const Entity& e) { return e.predator && e.predatorKills > 0; });
    ASSERT_NE(eater, after.end());
    EXPECT_GT(eater->maxHp, 0);
}

TEST(Simulation, should_spill_a_predators_hoard_when_it_is_killed) {
    // given — an alpha carrying a hoard (built directly to keep the test focused
    // on the drop, not on the emergent hunt which the test above covers).
    Simulation sim;
    // Reach into the sim only through its public step/command surface: instead we
    // rely on the emergent hunt to build a hoard, then verify a kill spills it.
    Entity* predator = nullptr;
    for (int i = 0; i < 4000 && predator == nullptr; ++i) {
        sim.step(1.0f / 60.0f);
        for (auto& e : const_cast<std::vector<Entity>&>(sim.world().enemies)) {
            if (e.predator && !e.hoard.empty()) { predator = &e; break; }
        }
    }
    ASSERT_NE(predator, nullptr);  // an alpha eventually scavenges gear

    // when — the predator dies (simulate a killing blow via a fresh damage source).
    const std::size_t lootBefore = sim.world().loot.size();
    const std::size_t hoardSize = predator->hoard.size();
    predator->hp = 0;
    sim.step(1.0f / 60.0f);  // the DoT/HP<=0 sweep in updateEnemies reaps it

    // then — every hoarded item is spilled onto the ground for the player to claim.
    EXPECT_GE(sim.world().loot.size(), lootBefore + hoardSize);
}

TEST(Simulation, should_drop_random_but_reproducible_loot) {
    // A tank stands at the centre and cleaves waves; we collect the base-item
    // names that end up in the shared bank. Same seed twice must match (weighted
    // pick still runs off the seedable PRNG), and it must not be one item on repeat.
    auto run = [] {
        Simulation sim;
        sim.applyCommand({CommandType::SelectClass, 0, {}, 0});  // Tank (melee)
        for (int i = 0; i < 3000; ++i) {
            sim.applyCommand({CommandType::Attack, 0, {1.0f, 0.0f}, 0});
            sim.step(1.0f / 60.0f);
        }
        std::vector<std::string> names;
        for (const auto& it : sim.world().inventory.items()) names.push_back(it.name);
        std::sort(names.begin(), names.end());
        return names;
    };

    const std::vector<std::string> a = run();
    const std::vector<std::string> b = run();
    EXPECT_EQ(a, b);  // reproducible

    // Variety: more than one distinct base item made it into the bank.
    std::vector<std::string> distinct = a;
    distinct.erase(std::unique(distinct.begin(), distinct.end()), distinct.end());
    EXPECT_GE(distinct.size(), 2u);
}

TEST(Simulation, should_push_overlapping_players_apart) {
    // given — two players both driving into the exact same spot (world centre).
    Simulation sim;
    const int p2 = sim.addPlayer(1);
    ASSERT_EQ(p2, 1);
    const Vec2 c{sim.world().width / 2.0f, sim.world().height / 2.0f};
    for (int i = 0; i < 400; ++i) {
        const Vec2 d0 = c - sim.world().players[0].entity.position;
        const Vec2 d1 = c - sim.world().players[1].entity.position;
        sim.applyCommand({CommandType::Move, 0, d0, 0});
        sim.applyCommand({CommandType::Move, p2, d1, 0});
        sim.step(kTick);
    }

    // then — separation keeps them from fully stacking on one tile.
    const auto& a = sim.world().players[0].entity;
    const auto& b = sim.world().players[1].entity;
    const float minDist = a.radius + b.radius;
    EXPECT_GT((a.position - b.position).length(), minDist * 0.5f);
}

TEST(Simulation, should_heal_by_consuming_a_potion_from_the_bank) {
    // given — a hurt player and one potion in the shared bank.
    Simulation sim;
    sim.setBank(0, {{"Health Potion", 0.5f, ItemKind::Potion, 25}});
    ASSERT_EQ(sim.world().inventory.size(), 1u);
    Simulation& s = sim;

    // Take a wound: walk into the nearest wave enemy until it lands a contact hit
    // (there's no public "damage me" hook, so we earn the damage honestly).
    const int maxHp = sim.world().players[0].entity.maxHp;
    for (int i = 0; i < 240 && sim.world().players[0].entity.hp == maxHp; ++i) {
        const auto& es = sim.world().enemies;
        const Vec2 pp = sim.world().players[0].entity.position;
        const Entity* near = nullptr;
        float best = 1e18f;
        for (const auto& e : es) {
            if (!e.alive) continue;
            const float d = distanceSquared(pp, e.position);
            if (d < best) { best = d; near = &e; }
        }
        if (near) {
            const Vec2 dir = (near->position - pp).normalized();
            s.applyCommand({CommandType::Move, 0, dir, 0});
        }
        s.step(kTick);
    }
    const int wounded = sim.world().players[0].entity.hp;
    ASSERT_LT(wounded, maxHp);  // we actually took a hit

    // when — quaff a potion.
    s.applyCommand({CommandType::UsePotion, 0, {}, 0});
    s.step(kTick);

    // then — it healed and left the bank, never over max.
    EXPECT_GT(sim.world().players[0].entity.hp, wounded);
    EXPECT_LE(sim.world().players[0].entity.hp, maxHp);
    EXPECT_EQ(sim.world().inventory.size(), 0u);
}

TEST(Simulation, should_scale_world_mobs_by_distance_from_spawn) {
    // given — a fresh sim populates the outer world with idle, zone-scaled mobs.
    const Simulation sim;
    const Vec2 center{sim.world().width / 2.0f, sim.world().height / 2.0f};

    // then — the map is the difficulty curve: the farthest mob out-levels the
    // nearest one, and outer mobs stay idle until you approach.
    int nearLvl = 1 << 30, farLvl = 0;
    float nearD = 1e9f, farD = 0.0f;
    bool anyWorldMob = false;
    for (const auto& e : sim.world().enemies) {
        if (!e.worldMob) continue;
        anyWorldMob = true;
        EXPECT_FALSE(e.aggroed);
        const float d = (e.position - center).length();
        if (d < nearD) { nearD = d; nearLvl = e.level; }
        if (d > farD) { farD = d; farLvl = e.level; }
    }
    ASSERT_TRUE(anyWorldMob);
    EXPECT_GT(farLvl, nearLvl);
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
