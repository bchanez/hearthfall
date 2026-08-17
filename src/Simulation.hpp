#pragma once

#include <cstdint>

#include "Command.hpp"
#include "GameContent.hpp"
#include "World.hpp"

namespace game {

// Owns all game state and rules. It never draws and never touches SDL.
//
// Usage each frame (see Game):
//   sim.applyCommand(cmd);   // feed player intentions (cmd carries a playerId)
//   sim.step(dt);            // advance the world by a fixed timestep
//   render(sim.world());     // draw the read-only state
class Simulation {
public:
    // Content (classes, loot) is injected — from Lua in the real game, or the
    // built-in defaults (used by tests and when data files are absent).
    explicit Simulation(GameContent content = defaultContent());

    void applyCommand(const Command& cmd);
    void step(float dt);

    const World& world() const { return world_; }

    // Player management (local co-op / drop-in). addPlayer reuses an inactive
    // slot if one exists and returns the player's id.
    int addPlayer(int classIndex);
    void setPlayerActive(int playerId, bool active);
    int playerCount() const { return static_cast<int>(world_.players.size()); }

    // Seed the shared bank from a save (host/local only).
    void setBank(int gold, const std::vector<Item>& items);

private:
    void initPlayer(int playerId, int classIndex, int level);
    void reclass(int playerId, int classIndex);  // change class, keep level/xp/stats
    void recomputeStats(Player& p);              // maxHp/speed from class + stats + gear
    void allocStat(int playerId, int statIndex);  // spend a banked point (non-pausing)
    void autoAllocate(Player& p, int n);          // sensible defaults for level-matched joins
    void gainSkill(Skill& s, int amount);         // use-based skill XP + level-ups
    void spawnWave();                            // builds the current wave's roster
    void spawnWorldMobs();                        // scatters idle, zone-scaled mobs
    int zoneLevel(const Vec2& pos) const;         // difficulty by distance from spawn
    void scaleEnemyToLevel(Entity& e, int level) const;  // HP/damage for a zone level
    Entity makeEnemy(EnemyType type, const Vec2& pos) const;  // archetype stat block
    const PlayerClass& classAt(int index) const;
    Item lootRandom();  // weighted base-item pick from the loot table (uses the PRNG)

    void advancePlayerTimers(Player& p, float dt);
    void updatePlayer(Player& p, float dt);
    void performAttack(int playerId);
    void performAbility(int playerId);
    void performUsePotion(int playerId);  // consume a bank potion to heal
    void performDash(int playerId);       // burst move + i-frames
    void resolveEnemyContact(int playerId);
    void downPlayer(int playerId);
    Vec2 spawnOffset(int playerId) const;

    // Progression.
    int effectiveDamage(const Player& p) const;
    void awardXp(int playerId, int amount);
    int partyMaxLevel() const;

    // Equipment (drawn from / returned to the shared bank).
    void equipBest(int playerId);
    void equipBestOfKind(int playerId, ItemKind kind);
    void unequip(int playerId);

    void updateProjectiles(float dt);
    void updateEnemies(float dt);
    void tickStatus(Entity& e, float dt);  // burn/poison DoT + slow timers
    // Ranged/caster enemies: keep distance, fire, or telegraph a heavy blast.
    void enemyRangedAct(Entity& enemy, int target, float dt, bool& holdPosition);
    void applyStatusToPlayer(Player& p, const Entity& source);  // e.g. spitter venom
    void resolveSeparation();  // push overlapping same-kind bodies apart
    void resolvePickups();
    void respawnWaveIfClear();
    void dropLoot(EnemyType from, const Vec2& at, Rarity minRarity = Rarity::Common);
    Rarity rollRarity(EnemyType from);       // tougher enemies roll higher
    void rollAffixes(Item& it, Rarity r);    // rolls affixCountFor(r) from the pool
    uint32_t nextRand();                     // deterministic LCG (reproducible)
    void killEnemy(Entity& enemy, int killerId);
    // Damage an enemy: HP, threat, plus hit-flash + knockback juice.
    void hurtEnemy(Entity& enemy, int damage, const Vec2& fromDir, int killerId, float threat);

    // Threat / aggro.
    void addThreat(Entity& enemy, int playerId, float amount);
    int highestThreatPlayer(const Entity& enemy) const;
    // Decide whether an enemy is chasing this frame, from its behavior + range.
    void updateAggro(Entity& enemy, int target, bool hasThreat);

    int nearestActivePlayerId(const Vec2& from) const;

    // Predator / alpha ecology. A rare mob hunts other mobs; when not chasing a
    // player it stalks the nearest prey, bites on a cadence, and eats it whole.
    int nearestPreyIndex(std::size_t self) const;         // closest other mob in hunt range
    void absorbPrey(Entity& predator, Entity& prey);      // eat: grow + inherit its hoard/level

    GameContent content_;
    World world_;
    uint32_t rngState_ = 0x9e3779b9u;  // fixed seed → reproducible loot rolls
    int nextEnemyId_ = 0;              // hands out stable per-enemy ids
    // Enemies spawned mid-step (e.g. slime splits) are queued here and appended
    // after the enemy loops finish, so we never reallocate a vector we're iterating.
    std::vector<Entity> pendingSpawns_;
};

}  // namespace game
