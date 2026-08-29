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

    // The draft pools (for the Renderer's choice-box labels). The level-up chooser
    // offers a combined space: boon ids [0, upgrades) then ability ids after them.
    const std::vector<UpgradeSpec>& upgrades() const { return content_.upgradePool; }
    const std::vector<AbilitySpec>& abilities() const { return content_.abilityPool; }

private:
    void initPlayer(int playerId, int classIndex, int level);
    void recomputeStats(Player& p);              // maxHp/speed from class + stats + gear
    void allocStat(int playerId, int statIndex);  // spend a banked point (non-pausing)
    void driftStat(Player& p);  // auto-grow a characteristic toward your playstyle on level-up
    int dodgePctFor(const Player& p) const;  // capped dodge chance from boons
    void chooseUpgrade(int playerId, int slot);   // apply an offered level-up choice
    void rollBoonChoices(Player& p);              // roll 3 distinct offers into boonChoices
    void grantAbility(Player& p, int abilityId);  // add or rank up an auto-cast ability
    void updateAbilities(Player& p, int playerId, float dt);  // tick + auto-cast
    void castAbility(Player& p, int playerId, const AbilitySpec& spec, int rank);
    int nearestEnemyIndex(const Vec2& from) const;  // closest living enemy, or -1
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
    void performUsePotion(int playerId);         // quaff the first bank potion to heal
    void drinkPotionAt(int playerId, int idx);   // drink a specific bank potion (heal by rarity)
    void performDash(int playerId);       // burst move + i-frames
    void resolveEnemyContact(int playerId);
    void downPlayer(int playerId);
    Vec2 spawnOffset(int playerId) const;

    // Progression.
    int effectiveDamage(const Player& p) const;
    void awardXp(int playerId, int amount);
    int partyMaxLevel() const;

    // Equipment (drawn from / returned to the shared bank).
    void equipBest(int playerId);                   // auto-equip every improving item
    void equipItemAt(int playerId, int bankIndex);  // equip a specific bank item
    void sellItemAt(int bankIndex);                 // sell a bank item for gold
    int sellJunk();                                 // bulk-sell all Common gear; returns gold gained
    void unequip(int playerId);                     // strip all gear back to the bank
    // Place an item onto the paperdoll per the wield rules (2H clears the off-hand,
    // a second 1H weapon dual-wields, etc.); displaced gear returns to the bank.
    void equipResolved(Player& p, const Item& picked);
    // Which physical slot `it` would occupy on `p` right now (accounts for the
    // free ring finger and dual-wield). Used to compare against what's equipped.
    EquipSlot landingSlot(const Player& p, const Item& it) const;

    void updateProjectiles(float dt);
    void updateEnemies(float dt);
    void tickStatus(Entity& e, float dt);  // burn/poison/stun DoT + slow timers
    // Apply an ability's on-hit status rider (burn/slow/stun/knock) to an enemy.
    void applyAbilityStatus(Entity& enemy, AbilityStatus st, float dur, int power,
                            const Vec2& dir);
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
