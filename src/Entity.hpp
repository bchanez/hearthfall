#pragma once

#include <vector>

#include "Item.hpp"
#include "Vec2.hpp"

namespace game {

// Enemy archetypes. Each has a distinct stat profile (see Simulation::makeEnemy)
// so a wave is a mix of threats — fast swarmers, beefy brutes, the odd boss —
// which makes positioning and target priority matter. Unused for players.
enum class EnemyType { Grunt, Swarmer, Brute, Boss, Slime, Archer, Spitter, Sorcerer };

// How an enemy decides to chase a player. Gated by an aggroRadius so the world
// stops sprinting at you — idle mobs create safe zones until you step close.
//   Passive    — never chases on sight; only fights back once attacked.
//   Defensive  — chases inside its radius, but leashes back home if pulled far.
//   Aggressive — chases inside its radius and stays sticky until you break away.
//   Pack       — like Aggressive, and one member waking wakes nearby pack-mates.
enum class AggroBehavior { Passive, Defensive, Aggressive, Pack };

// A single actor in the world (player or enemy).
// Step 1 keeps this deliberately simple: position, velocity, a collision
// radius and hit points. It will grow (or be replaced by an ECS) as the
// game gains classes, stats and abilities — see GAME_DESIGN.md build order.
struct Entity {
    Vec2 position{};
    Vec2 velocity{};
    float radius = 16.0f;
    float speed = 200.0f;  // pixels per second
    int hp = 100;
    int maxHp = 100;
    bool alive = true;

    // Enemy-only fields (unused for players).
    int id = -1;  // stable across snapshots so the client can interpolate enemies
    EnemyType type = EnemyType::Grunt;
    int contactDamage = 10;      // damage dealt to a player on contact
    float hitFlash = 0.0f;       // brief white flash timer when damaged (juice)
    Vec2 knockback{};            // decaying push velocity applied when hit (juice)

    // Status effects (apply to players and enemies alike). Timers in seconds; the
    // damage-over-time ticks each step while active, the slow scales speed. This
    // is the shared substrate for enemy venom, boss telegraphs and on-hit affixes.
    float burnTime = 0.0f, poisonTime = 0.0f, slowTime = 0.0f;
    int burnDps = 0, poisonDps = 0;  // damage per second while the timer runs
    float dotAccum = 0.0f;           // fractional damage carried between steps

    // Ranged / caster enemies: cadence between shots, and a telegraph wind-up
    // (>0 = charging a heavy attack the player can read and dodge).
    float attackTimer = 0.0f;
    float windup = 0.0f;

    // Aggro. An enemy stays idle (or drifts home) until provoked; once aggroed
    // it chases its target. `home` is the spawn point Defensive mobs leash to.
    AggroBehavior behavior = AggroBehavior::Aggressive;
    float aggroRadius = 300.0f;
    Vec2 home{};
    bool aggroed = false;

    // Zone level: the map is the difficulty curve. Wave enemies stay near spawn
    // (low level); scattered world mobs scale by distance from spawn. `worldMob`
    // marks the idle outer-world population so it's excluded from wave-clear.
    int level = 1;
    bool worldMob = false;
    bool elite = false;  // golden-aura rare: tougher, drops guaranteed rare loot

    // Predator: a rare, hyper-aggressive mob that HUNTS other monsters — biting
    // them for XP and scavenging their corpses. Left alone it snowballs into an
    // "alpha" (predatorKills climbs, it grows and hits harder), turning the outer
    // world into a living food chain rather than a static roster. Beat one and you
    // inherit everything it accumulated: the hoard it scavenged and the levels
    // (→ XP) it ate. Predators can eat each other, so alphas escalate on their own.
    bool predator = false;
    int predatorKills = 0;    // monsters eaten → size/damage + alpha status
    float huntTimer = 0.0f;   // bite cadence while chewing on prey
    std::vector<Item> hoard;  // scavenged gear, spilled on death (or stolen up the chain)

    // How much "threat" each player has generated on this enemy (indexed by
    // playerId). The enemy chases whoever holds the most threat.
    std::vector<float> threat;
    int targetPlayer = -1;  // playerId this enemy is currently chasing (for display)
};

// A predator that has eaten enough to dominate its patch: the emergent "alpha".
// Both the Simulation (for tuning) and the Renderer (for its menacing aura) read
// this so a fearsome-looking mob and a genuinely fearsome one stay in lockstep.
constexpr int kAlphaKills = 3;
inline bool isAlpha(const Entity& e) { return e.predator && e.predatorKills >= kAlphaKills; }

// Circle-vs-circle overlap test.
inline bool overlaps(const Entity& a, const Entity& b) {
    const float r = a.radius + b.radius;
    return distanceSquared(a.position, b.position) <= r * r;
}

}  // namespace game
