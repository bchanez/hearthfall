#include "Simulation.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>

#include "PlayerStats.hpp"

namespace game {

namespace {
constexpr float kEnemyContactCooldown = 0.6f;  // player i-frames after a hit
constexpr float kProjectileSpeed = 620.0f;
constexpr float kMeleeWeaponRange = 78.0f;    // reach of an equipped melee weapon
constexpr float kRangedWeaponRange = 520.0f;  // travel of an equipped bow's bolt

// New monster tuning (ranged / caster / venom / telegraph).
constexpr float kEnemyBoltSpeed = 340.0f;
constexpr int kArcherBoltDamage = 8;
constexpr float kArcherFireCooldown = 1.4f;
constexpr float kArcherKeepDistance = 300.0f;  // archer backs off if a player is closer
constexpr float kSorcererWindup = 1.1f;         // telegraph time before the blast lands
constexpr float kSorcererFireCooldown = 2.8f;
constexpr int kSorcererBoltDamage = 16;
constexpr int kSpitterPoisonDps = 6;
constexpr float kSpitterPoisonTime = 3.0f;
constexpr float kSpitterSlowTime = 1.5f;
constexpr float kStatusSlowFactor = 0.55f;  // a slowed body moves at 55% speed
constexpr float kPowerShotSpeed = 780.0f;
constexpr float kRespawnInvuln = 1.0f;

// Juice tuning.
constexpr float kHitFlash = 0.09f;         // how long a struck enemy flashes white
constexpr float kKnockback = 260.0f;       // impulse speed imparted by a hit
constexpr float kKnockbackDecay = 6.0f;    // per-second decay of the knockback push

// Wave / difficulty tuning.
constexpr int kBossEveryNWaves = 5;
constexpr float kHpScalePerWave = 0.08f;   // enemies get +8% HP per wave cleared

// Co-op scaling: a bigger party gets a bigger, beefier wave so it isn't trivial.
constexpr int kPartyExtraPerPlayer = 2;      // +2 wave enemies per extra player
constexpr float kPartyHpPerPlayer = 0.5f;    // +50% wave HP per extra player

// Zone tuning: the map is the difficulty curve. Distance from spawn → level,
// level → HP/damage multipliers (reused from the same "scale up" idea as waves).
constexpr float kHpScalePerLevel = 0.12f;  // +12% HP per zone level over 1
constexpr float kDmgScalePerLevel = 0.06f; // +6% contact damage per zone level over 1

// Progression tuning. A level grants a point to spend into a characteristic
// (the class only sets the level-1 baseline); the point is what makes you
// stronger, per the classless / Mabinogi-style specialization.
constexpr int kXpPerKill = 20;
constexpr int kPointsPerLevel = 1;
constexpr int kHpPerVit = 8;     // max HP per VIT point
constexpr int kDmgPerStat = 2;   // attack damage per point of the primary stat
constexpr int kHealPerInt = 3;   // Mend heal per INT point

// Skills rise by use. Small XP per action; the matching skill's level scales the
// action, and a threshold unlocks a stronger ability.
constexpr int kSkillXpMeleeHit = 4;
constexpr int kSkillXpRangedShot = 3;
constexpr int kSkillXpHeal = 8;
constexpr int kSkillXpDodge = 5;
constexpr int kWhirlwindSkill = 5;  // Melee ≥ this → ability becomes Whirlwind
constexpr int kPowerShotSkill = 5;  // Ranged ≥ this → ability becomes Power Shot

// Consumables.
constexpr float kPotionHealPct = 0.5f;   // a potion restores half of max HP
constexpr float kPotionCooldown = 1.0f;  // seconds between quaffs

// Dash / dodge roll.
constexpr float kDashSpeed = 1150.0f;    // burst speed while dashing
constexpr float kDashDuration = 0.16f;   // how long the burst lasts (~184px)
constexpr float kDashCooldown = 1.1f;    // until the next dash is ready

// Bump a characteristic by stat index (0 STR, 1 DEX, 2 INT, 3 VIT, 4 AGI).
// Returns false for an out-of-range index.
bool addStat(Stats& s, int idx) {
    switch (idx) {
        case 0: ++s.str; return true;
        case 1: ++s.dex; return true;
        case 2: ++s.intel; return true;
        case 3: ++s.vit; return true;
        case 4: ++s.agi; return true;
    }
    return false;
}

// Aggro tuning.
constexpr float kThreatDecayPerSec = 0.05f;  // gentle decay so aggro stays dynamic
constexpr float kHealThreatFactor = 0.6f;    // healing draws aggro
constexpr float kTauntBonus = 60.0f;         // Taunt puts the tank on top + this
constexpr float kTauntRadius = 280.0f;

// Behavioural aggro tuning.
constexpr float kPackWakeRadius = 260.0f;      // an aggroed pack member wakes mates within this
constexpr float kAggressiveDeaggroMul = 2.0f;  // Aggressive chases until 2x its aggro radius
constexpr float kDefensiveLeashMul = 2.6f;     // Defensive leashes home past 2.6x radius from home
constexpr float kHomeEpsilon = 6.0f;           // "close enough" to home to stop drifting

// Predator / alpha tuning. A rare mob hunts OTHER mobs, eating their XP + loot
// and snowballing into an alpha. Kept deliberately slow (scarce spawns, modest
// per-kill growth) so the outer world thins gradually instead of collapsing.
constexpr float kPredatorHuntRadius = 620.0f;  // how far a predator senses prey
constexpr float kPredatorBiteReach = 6.0f;     // a bite connects at contact + this
constexpr float kPredatorBiteCooldown = 0.7f;  // seconds between bites on prey
constexpr float kPredatorHpGrowth = 1.18f;     // +18% max HP per monster eaten
constexpr float kPredatorDmgGrowth = 1.12f;    // +12% contact damage per kill
constexpr float kPredatorMaxRadius = 40.0f;    // an alpha caps at boss-ish size
constexpr std::size_t kMaxHoard = 6;           // cap on scavenged loot per predator

float clampToZero(float v) { return v < 0.0f ? 0.0f : v; }

// The tank grabs aggro; everyone else is baseline.
float classThreatMultiplier(const PlayerClass& cls) {
    return cls.id == ClassId::Tank ? 5.0f : 1.0f;
}

float threatOf(const Entity& enemy, int playerId) {
    if (playerId < 0 || playerId >= static_cast<int>(enemy.threat.size())) return 0.0f;
    return enemy.threat[playerId];
}
}  // namespace

Simulation::Simulation(GameContent content) : content_(std::move(content)) {
    if (content_.classes.empty()) content_ = defaultContent();  // never run without a kit
    addPlayer(0);  // player 0 spawns Human — you become who you play
    world_.wave = 1;
    spawnWave();
    spawnWorldMobs();  // populate the outer world so exploration has stakes
}

const PlayerClass& Simulation::classAt(int index) const {
    const int n = static_cast<int>(content_.classes.size());
    return content_.classes[((index % n) + n) % n];
}

Item Simulation::lootRandom() {
    const auto& table = content_.lootTable;
    if (table.empty()) return {};

    // Weighted pick over dropWeight, so drops feel genuinely random instead of a
    // predictable cycle. Still driven by the seedable PRNG → reproducible tests.
    float total = 0.0f;
    for (const auto& it : table) total += std::max(0.0f, it.dropWeight);
    if (total <= 0.0f) return table[nextRand() % table.size()];  // all zero → uniform

    // roll ∈ [0, total): scale a 24-bit PRNG draw to keep it deterministic.
    const float roll = static_cast<float>(nextRand() & 0xFFFFFF) / static_cast<float>(0x1000000) *
                       total;
    float acc = 0.0f;
    for (const auto& it : table) {
        acc += std::max(0.0f, it.dropWeight);
        if (roll < acc) return it;
    }
    return table.back();
}

Vec2 Simulation::spawnOffset(int playerId) const {
    const float ox = static_cast<float>((playerId % 3) - 1) * 70.0f;
    const float oy = static_cast<float>((playerId / 3) % 3 - 1) * 70.0f;
    return {world_.width / 2.0f + ox, world_.height / 2.0f + oy};
}

int Simulation::addPlayer(int classIndex) {
    // Level-matching: a joining player starts at the party's current level, so
    // nobody is ever too low to be useful.
    const int level = std::max(1, partyMaxLevel());
    for (int i = 0; i < static_cast<int>(world_.players.size()); ++i) {
        if (!world_.players[i].active) {
            initPlayer(i, classIndex, level);
            return i;
        }
    }
    world_.players.emplace_back();
    const int id = static_cast<int>(world_.players.size()) - 1;
    initPlayer(id, classIndex, level);
    return id;
}

void Simulation::initPlayer(int playerId, int classIndex, int level) {
    Player& p = world_.players[playerId];
    p = Player{};  // reset transient state and timers
    p.level = std::max(1, level);
    p.cls = classAt(classIndex);
    p.entity.radius = 18.0f;
    p.entity.position = spawnOffset(playerId);
    p.active = true;
    // A level-matched drop-in gets its earned points pre-spent along the class's
    // preferred order, so it's immediately competent rather than a blank slate.
    autoAllocate(p, p.level - 1);
    recomputeStats(p);
    p.entity.hp = p.entity.maxHp;
}

void Simulation::reclass(int playerId, int classIndex) {
    // Change class but keep the character's level/xp/position.
    Player& p = world_.players[playerId];
    p.cls = classAt(classIndex);
    p.attackCooldown = 0.0f;
    p.abilityCooldown = 0.0f;
    p.shieldTimer = 0.0f;
    recomputeStats(p);
    p.entity.hp = p.entity.maxHp;
}

void Simulation::recomputeStats(Player& p) {
    // Class base = the level-1 baseline; VIT and gear stack on top.
    p.entity.maxHp = p.cls.maxHp + p.stats.vit * kHpPerVit + gearMaxHp(p);
    p.entity.speed = p.cls.speed * moveSpeedMul(p);  // affixes + AGI
    if (p.entity.hp > p.entity.maxHp) p.entity.hp = p.entity.maxHp;
}

int Simulation::effectiveDamage(const Player& p) const {
    // Class base + the primary characteristic (STR melee / DEX ranged) + gear +
    // the mastery of the skill you're actually using (rises as you practise).
    return p.cls.attackDamage + primaryDamageStat(p) * kDmgPerStat + gearDamage(p) +
           activeAttackSkill(p);
}

void Simulation::allocStat(int playerId, int statIndex) {
    if (playerId < 0 || playerId >= static_cast<int>(world_.players.size())) return;
    Player& p = world_.players[playerId];
    if (!p.active || p.unspentPoints <= 0) return;
    if (!addStat(p.stats, statIndex)) return;  // ignore an out-of-range index
    --p.unspentPoints;
    const int before = p.entity.maxHp;
    recomputeStats(p);
    p.entity.hp += std::max(0, p.entity.maxHp - before);  // a VIT point grants its HP now
}

void Simulation::gainSkill(Skill& s, int amount) {
    s.xp += amount;
    while (s.xp >= skillXpForLevel(s.level)) {
        s.xp -= skillXpForLevel(s.level);
        ++s.level;  // mastery climbs purely from doing
    }
}

void Simulation::autoAllocate(Player& p, int n) {
    if (n <= 0) return;
    // Preferred stat order per class (indices into addStat) so a matched drop-in
    // plays to its class's strengths without the player micromanaging.
    static const int humanOrder[]  = {3, 0, 1, 4, 2};  // VIT, STR, DEX, AGI, INT (balanced)
    static const int tankOrder[]   = {3, 0, 4, 1, 2};  // VIT, STR, AGI, DEX, INT
    static const int archerOrder[] = {1, 4, 3, 0, 2};  // DEX, AGI, VIT, STR, INT
    static const int healerOrder[] = {2, 3, 4, 1, 0};  // INT, VIT, AGI, DEX, STR
    const int* order = p.cls.id == ClassId::Tank     ? tankOrder
                       : p.cls.id == ClassId::Archer ? archerOrder
                       : p.cls.id == ClassId::Healer ? healerOrder
                                                     : humanOrder;
    for (int i = 0; i < n; ++i) addStat(p.stats, order[i % 5]);
}

void Simulation::equipBest(int playerId) {
    equipBestOfKind(playerId, ItemKind::Weapon);
    equipBestOfKind(playerId, ItemKind::Armor);
}

void Simulation::equipBestOfKind(int playerId, ItemKind kind) {
    Player& p = world_.players[playerId];
    const bool isWeapon = kind == ItemKind::Weapon;
    const int current = isWeapon ? (p.hasWeapon ? p.weapon.bonusDamage : -1)
                                 : (p.hasArmor ? p.armor.bonusMaxHp : -1);

    // Find the best matching item in the bank that beats what's equipped.
    const auto& items = world_.inventory.items();
    int bestIdx = -1;
    int bestVal = current;
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        if (items[i].kind != kind) continue;
        const int v = isWeapon ? items[i].bonusDamage : items[i].bonusMaxHp;
        if (v > bestVal) {
            bestVal = v;
            bestIdx = i;
        }
    }
    if (bestIdx < 0) return;  // nothing better available

    const Item picked = items[bestIdx];
    world_.inventory.removeAt(static_cast<std::size_t>(bestIdx));
    if (isWeapon) {
        if (p.hasWeapon) world_.inventory.tryAdd(p.weapon);  // old gear returns to the pool
        p.weapon = picked;
        p.hasWeapon = true;
    } else {
        if (p.hasArmor) world_.inventory.tryAdd(p.armor);
        p.armor = picked;
        p.hasArmor = true;
    }
    recomputeStats(p);
}

void Simulation::unequip(int playerId) {
    Player& p = world_.players[playerId];
    if (p.hasWeapon) {
        world_.inventory.tryAdd(p.weapon);
        p.hasWeapon = false;
        p.weapon = Item{};
    }
    if (p.hasArmor) {
        world_.inventory.tryAdd(p.armor);
        p.hasArmor = false;
        p.armor = Item{};
    }
    recomputeStats(p);
}

int Simulation::partyMaxLevel() const {
    int best = 0;
    for (const auto& p : world_.players) {
        if (p.active) best = std::max(best, p.level);
    }
    return best;
}

void Simulation::awardXp(int playerId, int amount) {
    if (playerId < 0 || playerId >= static_cast<int>(world_.players.size())) return;
    Player& p = world_.players[playerId];
    if (!p.active) return;
    p.xp += amount;
    while (p.xp >= xpForLevel(p.level)) {
        p.xp -= xpForLevel(p.level);
        ++p.level;
        p.unspentPoints += kPointsPerLevel;  // you choose where the growth goes
        p.entity.hp = p.entity.maxHp;         // level-up fully heals — a reward moment
    }
}

void Simulation::setPlayerActive(int playerId, bool active) {
    if (playerId < 0 || playerId >= static_cast<int>(world_.players.size())) return;
    if (!active) unequip(playerId);  // fluid loot: gear returns to the shared bank
    world_.players[playerId].active = active;
}

void Simulation::setBank(int gold, const std::vector<Item>& items) {
    world_.gold = gold;
    for (const auto& item : items) world_.inventory.tryAdd(item);
}

Entity Simulation::makeEnemy(EnemyType type, const Vec2& pos) const {
    Entity e;
    e.type = type;
    e.position = pos;
    e.home = pos;
    switch (type) {
        case EnemyType::Swarmer:  // fast, fragile, low damage — swarms and flanks
            e.radius = 10.0f; e.speed = 190.0f; e.hp = 18; e.contactDamage = 6;
            e.behavior = AggroBehavior::Pack; e.aggroRadius = 300.0f;  // hunt in packs
            break;
        case EnemyType::Grunt:  // the baseline threat
            e.radius = 14.0f; e.speed = 110.0f; e.hp = 40; e.contactDamage = 10;
            e.behavior = AggroBehavior::Aggressive; e.aggroRadius = 320.0f;
            break;
        case EnemyType::Brute:  // slow wall of HP that hits hard — a priority target
            e.radius = 24.0f; e.speed = 70.0f; e.hp = 150; e.contactDamage = 22;
            e.behavior = AggroBehavior::Defensive; e.aggroRadius = 200.0f;  // guards its patch
            break;
        case EnemyType::Boss:  // the wave-5 spike: huge, dangerous, shrugs off knockback
            e.radius = 40.0f; e.speed = 85.0f; e.hp = 900; e.contactDamage = 35;
            e.behavior = AggroBehavior::Aggressive; e.aggroRadius = 700.0f;  // sees you from afar
            break;
        case EnemyType::Slime:  // slow blob that SPLITS into two smaller slimes on death
            e.radius = 18.0f; e.speed = 80.0f; e.hp = 55; e.contactDamage = 8;
            e.behavior = AggroBehavior::Aggressive; e.aggroRadius = 260.0f;
            break;
        case EnemyType::Archer:  // ranged: keeps its distance and shoots — punishes standing still
            e.radius = 13.0f; e.speed = 120.0f; e.hp = 32; e.contactDamage = 6;
            e.behavior = AggroBehavior::Aggressive; e.aggroRadius = 460.0f;
            break;
        case EnemyType::Spitter:  // melee that leaves you POISONED + slowed on contact
            e.radius = 15.0f; e.speed = 130.0f; e.hp = 46; e.contactDamage = 9;
            e.behavior = AggroBehavior::Aggressive; e.aggroRadius = 340.0f;
            break;
        case EnemyType::Sorcerer:  // TELEGRAPHS a heavy blast you can dash out of
            e.radius = 16.0f; e.speed = 95.0f; e.hp = 60; e.contactDamage = 8;
            e.behavior = AggroBehavior::Aggressive; e.aggroRadius = 520.0f;
            break;
    }
    e.maxHp = e.hp;
    return e;
}

void Simulation::spawnWave() {
    const int wave = world_.wave;
    const bool bossWave = (wave % kBossEveryNWaves) == 0;

    // Party size drives co-op scaling: more bodies and tougher enemies so a full
    // group doesn't trivialise the wave.
    int party = 0;
    for (const auto& p : world_.players)
        if (p.active) ++party;
    party = std::max(1, party);
    const int extra = kPartyExtraPerPlayer * (party - 1);

    // Compose the roster for this wave.
    std::vector<EnemyType> roster;
    if (bossWave) {
        roster.push_back(EnemyType::Boss);
        for (int i = 0; i < 4 + extra; ++i) roster.push_back(EnemyType::Swarmer);  // adds
    } else {
        const int total = 5 + wave + extra;
        for (int i = 0; i < total; ++i) {
            EnemyType t = EnemyType::Grunt;
            if (i % 4 == 0) t = EnemyType::Swarmer;                  // a quarter are swarmers
            else if (wave >= 3 && i % 6 == 5) t = EnemyType::Brute;   // brutes from wave 3
            else if (wave >= 2 && i % 7 == 3) t = EnemyType::Slime;   // splitters from wave 2
            else if (wave >= 3 && i % 8 == 2) t = EnemyType::Archer;  // ranged from wave 3
            else if (wave >= 4 && i % 9 == 4) t = EnemyType::Spitter; // venom from wave 4
            else if (wave >= 5 && i % 11 == 7) t = EnemyType::Sorcerer;  // casters from wave 5
            roster.push_back(t);
        }
    }

    // A wave is an assault on the spawn point, so we place enemies around the
    // edges of a screen-sized band centred on the world (not the far world
    // edges — the outer world is for zone content, backlog #3) and mark them
    // already aggroed. Aggro *behaviour* (leash / passive / pack) still governs
    // how they act once you break away or for future idle world mobs.
    const float cx = world_.width / 2.0f, cy = world_.height / 2.0f;
    // Band kept inside the enemies' give-up ranges (corner ≈ 466px) so a far
    // spawn doesn't instantly de-aggro before it can march in.
    constexpr float kBandHalfW = 400.0f, kBandHalfH = 240.0f;
    const float x0 = cx - kBandHalfW, y0 = cy - kBandHalfH;
    const float bandW = kBandHalfW * 2.0f, bandH = kBandHalfH * 2.0f;

    // Scale HP with the wave number (ramp) and the party size (co-op balance).
    const float scale = (1.0f + kHpScalePerWave * static_cast<float>(wave - 1)) *
                        (1.0f + kPartyHpPerPlayer * static_cast<float>(party - 1));
    const int count = static_cast<int>(roster.size());
    for (int i = 0; i < count; ++i) {
        const bool topBottom = (i % 2) == 0;
        const float t = static_cast<float>(i + 1) / static_cast<float>(count + 1);
        Vec2 pos;
        if (topBottom) {
            pos = {x0 + bandW * t, i < count / 2 ? y0 : y0 + bandH};
        } else {
            pos = {i < count / 2 ? x0 : x0 + bandW, y0 + bandH * t};
        }
        Entity e = makeEnemy(roster[i], pos);
        e.id = nextEnemyId_++;
        e.home = {cx, cy};  // a wave's "post" is the objective, so it never leashes off
        e.aggroed = true;   // waves engage on arrival
        e.maxHp = static_cast<int>(static_cast<float>(e.maxHp) * scale);
        e.hp = e.maxHp;
        world_.enemies.push_back(e);
    }
}

int Simulation::zoneLevel(const Vec2& pos) const {
    const Vec2 center{world_.width / 2.0f, world_.height / 2.0f};
    const float d = (pos - center).length();
    const RingRadii ring = ringRadii(world_);
    // Near spawn is a gentle 1–5 ramp; the outer rings jump to the endgame tiers.
    if (d < ring.r0) return 1 + static_cast<int>(d / (ring.r0 / 5.0f));  // 1..5
    if (d < ring.r1) return 10;
    if (d < ring.r2) return 20;
    return 40;
}

void Simulation::scaleEnemyToLevel(Entity& e, int level) const {
    e.level = level;
    const float hpMul = 1.0f + kHpScalePerLevel * static_cast<float>(level - 1);
    const float dmgMul = 1.0f + kDmgScalePerLevel * static_cast<float>(level - 1);
    e.maxHp = static_cast<int>(static_cast<float>(e.maxHp) * hpMul);
    e.hp = e.maxHp;
    e.contactDamage = static_cast<int>(static_cast<float>(e.contactDamage) * dmgMul);
}

void Simulation::spawnWorldMobs() {
    // Scatter idle mobs across the outer world on a jittered grid, scaled by their
    // zone level. They wait (aggro system) until a player wanders close — so
    // walking outward is voluntarily raising the stakes. Deterministic placement
    // (index-based jitter, no RNG) keeps the loot PRNG stream untouched.
    const Vec2 center{world_.width / 2.0f, world_.height / 2.0f};
    constexpr float kMargin = 180.0f;
    constexpr float kStep = 340.0f;
    constexpr float kClearRadius = 560.0f;  // keep the central wave arena empty

    int idx = 0;
    for (float y = kMargin; y < world_.height - kMargin; y += kStep) {
        for (float x = kMargin; x < world_.width - kMargin; x += kStep) {
            const float jx = static_cast<float>((idx * 73) % 140 - 70);
            const float jy = static_cast<float>((idx * 151) % 140 - 70);
            const Vec2 pos{x + jx, y + jy};
            ++idx;
            if ((pos - center).length() < kClearRadius) continue;

            const int lvl = zoneLevel(pos);
            // Tougher archetypes dominate the deeper rings.
            EnemyType type;
            if (lvl >= 40) type = (idx % 5 == 0) ? EnemyType::Boss : EnemyType::Brute;
            else if (lvl >= 20) type = (idx % 2 == 0) ? EnemyType::Brute : EnemyType::Swarmer;
            else if (lvl >= 10) type = (idx % 3 == 0) ? EnemyType::Brute : EnemyType::Grunt;
            else type = (idx % 2 == 0) ? EnemyType::Swarmer : EnemyType::Grunt;

            Entity e = makeEnemy(type, pos);
            e.id = nextEnemyId_++;
            e.worldMob = true;   // excluded from wave-clear; never respawns
            e.aggroed = false;   // idle until approached
            scaleEnemyToLevel(e, lvl);

            // Roughly one in seven is an elite: a golden-aura target of desire —
            // beefier and hits harder, but drops guaranteed rare loot.
            if (idx % 7 == 0) {
                e.elite = true;
                e.maxHp = static_cast<int>(e.maxHp * 2.2f);
                e.hp = e.maxHp;
                e.contactDamage = static_cast<int>(e.contactDamage * 1.5f);
                e.radius += 3.0f;
            }

            // Rare predators: hyper-aggressive hunters that prey on other mobs
            // and snowball into alphas. Kept scarce (and never also elite) so the
            // world thins slowly instead of eating itself. Deterministic pick
            // (index-based) so the loot PRNG stream stays untouched at spawn.
            if (!e.elite && idx % 13 == 5) {
                e.predator = true;
                e.behavior = AggroBehavior::Aggressive;  // still comes for the player too
                e.aggroRadius = std::max(e.aggroRadius, 360.0f);
                e.contactDamage = static_cast<int>(e.contactDamage * 1.2f);
            }
            world_.enemies.push_back(e);
        }
    }
}

void Simulation::applyCommand(const Command& cmd) {
    if (cmd.playerId < 0 || cmd.playerId >= static_cast<int>(world_.players.size())) return;
    Player& p = world_.players[cmd.playerId];
    if (!p.active) return;

    switch (cmd.type) {
        case CommandType::Move:
            p.moveIntent = cmd.dir;
            break;
        case CommandType::Attack:
            p.attackQueued = true;
            if (cmd.dir.length() > 0.0f) p.aim = cmd.dir.normalized();
            break;
        case CommandType::Ability:
            p.abilityQueued = true;
            if (cmd.dir.length() > 0.0f) p.aim = cmd.dir.normalized();
            break;
        case CommandType::SelectClass:
            reclass(cmd.playerId, cmd.classIndex);
            break;
        case CommandType::Equip:
            equipBest(cmd.playerId);
            break;
        case CommandType::Unequip:
            unequip(cmd.playerId);
            break;
        case CommandType::UsePotion:
            performUsePotion(cmd.playerId);
            break;
        case CommandType::Dash:
            p.dashQueued = true;
            if (cmd.dir.length() > 0.0f) p.dashDir = cmd.dir.normalized();
            break;
        case CommandType::AllocStat:
            allocStat(cmd.playerId, cmd.classIndex);  // classIndex carries the stat index
            break;
    }
}

void Simulation::step(float dt) {
    for (int i = 0; i < static_cast<int>(world_.players.size()); ++i) {
        Player& p = world_.players[i];
        if (!p.active) continue;
        advancePlayerTimers(p, dt);
        // Status effects tick on the player too (enemy venom): DoT + down check.
        tickStatus(p.entity, dt);
        if (p.entity.hp <= 0) downPlayer(i);
        if (p.dashQueued) performDash(i);
        updatePlayer(p, dt);
        if (p.attackQueued) performAttack(i);
        if (p.abilityQueued) performAbility(i);
    }

    updateProjectiles(dt);
    updateEnemies(dt);
    // Append any enemies spawned mid-step (slime splits) now that the loops are done.
    if (!pendingSpawns_.empty()) {
        for (auto& e : pendingSpawns_) world_.enemies.push_back(std::move(e));
        pendingSpawns_.clear();
    }
    resolveSeparation();

    for (int i = 0; i < static_cast<int>(world_.players.size()); ++i) {
        if (world_.players[i].active) resolveEnemyContact(i);
    }

    resolvePickups();
    respawnWaveIfClear();

    for (auto& p : world_.players) {
        p.attackQueued = false;
        p.abilityQueued = false;
        p.dashQueued = false;
    }
}

void Simulation::advancePlayerTimers(Player& p, float dt) {
    p.attackCooldown = clampToZero(p.attackCooldown - dt);
    p.attackFlash = clampToZero(p.attackFlash - dt);
    p.abilityCooldown = clampToZero(p.abilityCooldown - dt);
    p.shieldTimer = clampToZero(p.shieldTimer - dt);
    p.healFlash = clampToZero(p.healFlash - dt);
    p.invuln = clampToZero(p.invuln - dt);
    p.downedFlash = clampToZero(p.downedFlash - dt);
    p.potionCooldown = clampToZero(p.potionCooldown - dt);
    p.dashTimer = clampToZero(p.dashTimer - dt);
    p.dashCooldown = clampToZero(p.dashCooldown - dt);
}

void Simulation::updatePlayer(Player& p, float dt) {
    Entity& e = p.entity;
    // While dashing, override normal movement with the burst; otherwise walk.
    if (p.dashTimer > 0.0f) {
        e.velocity = p.dashDir * kDashSpeed;  // dash powers through a slow
    } else {
        const float spd = e.speed * (e.slowTime > 0.0f ? kStatusSlowFactor : 1.0f);
        e.velocity = p.moveIntent.normalized() * spd;
    }
    e.position += e.velocity * dt;
    e.position.x = std::clamp(e.position.x, e.radius, world_.width - e.radius);
    e.position.y = std::clamp(e.position.y, e.radius, world_.height - e.radius);
}

void Simulation::performDash(int playerId) {
    Player& p = world_.players[playerId];
    if (p.dashCooldown > 0.0f || p.dashTimer > 0.0f) return;
    // Dash where you're heading; if standing still, dash where you're aiming.
    const Vec2 dir = p.moveIntent.length() > 0.0f ? p.moveIntent.normalized() : p.aim.normalized();
    if (dir.length() <= 0.0f) return;
    p.dashDir = dir;
    p.dashTimer = kDashDuration;
    // Dodge mastery shortens the cooldown (down to a floor) — practise makes you nimble.
    const float cdMul = std::max(0.55f, 1.0f - static_cast<float>(p.skills.dodge.level - 1) * 0.03f);
    p.dashCooldown = kDashCooldown * cdMul;
    p.invuln = std::max(p.invuln, kDashDuration);  // dodge: i-frames through the roll
    gainSkill(p.skills.dodge, kSkillXpDodge);
}

void Simulation::addThreat(Entity& enemy, int playerId, float amount) {
    if (playerId < 0) return;
    if (static_cast<int>(enemy.threat.size()) <= playerId)
        enemy.threat.resize(playerId + 1, 0.0f);
    enemy.threat[playerId] += amount;
}

int Simulation::highestThreatPlayer(const Entity& enemy) const {
    int best = -1;
    float bestThreat = 0.0f;
    for (int i = 0; i < static_cast<int>(world_.players.size()); ++i) {
        if (!world_.players[i].active) continue;
        const float t = threatOf(enemy, i);
        if (t > bestThreat) {
            bestThreat = t;
            best = i;
        }
    }
    return best;  // -1 if nobody has generated threat yet
}

void Simulation::performAttack(int playerId) {
    Player& p = world_.players[playerId];
    if (p.attackCooldown > 0.0f) return;
    const PlayerClass& cls = p.cls;
    p.attackCooldown = cls.attackCooldown * attackCooldownMul(p);  // attack-speed affixes

    int damage = effectiveDamage(p);
    if (static_cast<int>(nextRand() % 100) < critChancePct(p)) damage *= 2;  // crit affix

    // The equipped weapon decides how you fight; unarmed falls back to the class
    // style. Ranged reach comes from the weapon (long) or the class's unarmed poke.
    const AttackStyle style = effectiveStyle(p);
    const float range = p.hasWeapon ? (style == AttackStyle::Ranged ? kRangedWeaponRange
                                                                     : kMeleeWeaponRange)
                                     : cls.attackRange;

    if (style == AttackStyle::Melee) {
        p.attackFlash = 0.12f;
        gainSkill(p.skills.melee, kSkillXpMeleeHit);  // practise the blade
        const float reach = range + p.entity.radius;
        const float reachSq = reach * reach;
        for (auto& enemy : world_.enemies) {
            if (!enemy.alive) continue;
            if (distanceSquared(p.entity.position, enemy.position) > reachSq) continue;
            const Vec2 dir = (enemy.position - p.entity.position).normalized();
            hurtEnemy(enemy, damage, dir, playerId, damage * classThreatMultiplier(cls));
        }
        return;
    }

    // Ranged: fire a bolt toward the aim direction.
    gainSkill(p.skills.ranged, kSkillXpRangedShot);  // practise the bow
    Projectile bolt;
    bolt.position = p.entity.position;
    bolt.velocity = p.aim * kProjectileSpeed;
    bolt.damage = damage;
    bolt.radius = 6.0f;
    bolt.life = range / kProjectileSpeed;
    bolt.owner = playerId;
    world_.projectiles.push_back(bolt);
}

void Simulation::performAbility(int playerId) {
    Player& p = world_.players[playerId];
    if (p.abilityCooldown > 0.0f) return;
    const PlayerClass& cls = p.cls;
    p.abilityCooldown = cls.abilityCooldown;

    // Skill unlocks: once you've practised a style, your ability upgrades to a
    // signature move regardless of class — mastery comes from doing, not picking.
    const AttackStyle style = effectiveStyle(p);
    if (style == AttackStyle::Melee && p.skills.melee.level >= kWhirlwindSkill) {
        // Whirlwind: hit every enemy around you; damage scales with Melee mastery.
        gainSkill(p.skills.melee, kSkillXpMeleeHit);
        const float radius = p.entity.radius + 90.0f;
        const float radiusSq = radius * radius;
        const int dmg = effectiveDamage(p) * 2;
        for (auto& enemy : world_.enemies) {
            if (!enemy.alive) continue;
            if (distanceSquared(p.entity.position, enemy.position) > radiusSq) continue;
            const Vec2 dir = (enemy.position - p.entity.position).normalized();
            hurtEnemy(enemy, dmg, dir, playerId, dmg * classThreatMultiplier(cls));
        }
        p.attackFlash = 0.2f;
        return;
    }
    if (style == AttackStyle::Ranged && p.skills.ranged.level >= kPowerShotSkill) {
        // Power Shot: a heavy bolt, unlocked by Ranged mastery.
        gainSkill(p.skills.ranged, kSkillXpRangedShot);
        Projectile bolt;
        bolt.position = p.entity.position;
        bolt.velocity = p.aim * kPowerShotSpeed;
        bolt.damage = effectiveDamage(p) * 3;
        bolt.radius = 11.0f;
        bolt.life = (kRangedWeaponRange * 1.3f) / kPowerShotSpeed;
        bolt.owner = playerId;
        world_.projectiles.push_back(bolt);
        return;
    }

    switch (cls.id) {
        case ClassId::Human: {
            // Second Wind: a modest self-heal — the neutral starter's safety net.
            const int heal = p.entity.maxHp / 4 + p.stats.intel * kHealPerInt + p.skills.heal.level;
            p.entity.hp = std::min(p.entity.maxHp, p.entity.hp + heal);
            p.healFlash = 0.35f;
            gainSkill(p.skills.heal, kSkillXpHeal);
            break;
        }
        case ClassId::Tank: {
            // Taunt: yank aggro onto the tank for nearby enemies, + brief shield.
            const float radiusSq = kTauntRadius * kTauntRadius;
            for (auto& enemy : world_.enemies) {
                if (!enemy.alive) continue;
                if (distanceSquared(p.entity.position, enemy.position) > radiusSq) continue;
                float top = 0.0f;
                for (int i = 0; i < static_cast<int>(world_.players.size()); ++i)
                    top = std::max(top, threatOf(enemy, i));
                if (static_cast<int>(enemy.threat.size()) <= playerId)
                    enemy.threat.resize(playerId + 1, 0.0f);
                enemy.threat[playerId] = top + kTauntBonus;
            }
            p.shieldTimer = 3.0f;
            break;
        }
        case ClassId::Archer: {
            Projectile bolt;  // Power Shot: big, fast, heavy-hitting bolt
            bolt.position = p.entity.position;
            bolt.velocity = p.aim * kPowerShotSpeed;
            bolt.damage = effectiveDamage(p) * 3;
            bolt.radius = 11.0f;
            bolt.life = (cls.attackRange * 1.3f) / kPowerShotSpeed;
            bolt.owner = playerId;
            world_.projectiles.push_back(bolt);
            break;
        }
        case ClassId::Healer: {
            // Mend: heal a chunk of max HP, boosted by INT + Heal mastery. Draws aggro.
            const int heal =
                p.entity.maxHp * 2 / 5 + p.stats.intel * kHealPerInt + p.skills.heal.level * 2;
            p.entity.hp = std::min(p.entity.maxHp, p.entity.hp + heal);
            p.healFlash = 0.35f;
            gainSkill(p.skills.heal, kSkillXpHeal);
            for (auto& enemy : world_.enemies) {
                if (enemy.alive) addThreat(enemy, playerId, heal * kHealThreatFactor);
            }
            break;
        }
    }
}

void Simulation::performUsePotion(int playerId) {
    Player& p = world_.players[playerId];
    if (p.potionCooldown > 0.0f) return;
    if (p.entity.hp >= p.entity.maxHp) return;  // don't waste a potion at full HP

    // Pull the first potion out of the shared bank.
    const auto& items = world_.inventory.items();
    int idx = -1;
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        if (items[i].kind == ItemKind::Potion) {
            idx = i;
            break;
        }
    }
    if (idx < 0) return;  // none carried

    world_.inventory.removeAt(static_cast<std::size_t>(idx));
    const int heal = std::max(1, static_cast<int>(p.entity.maxHp * kPotionHealPct));
    p.entity.hp = std::min(p.entity.maxHp, p.entity.hp + heal);
    p.healFlash = 0.35f;  // reuse the heal juice
    p.potionCooldown = kPotionCooldown;
}

void Simulation::updateProjectiles(float dt) {
    for (auto& bolt : world_.projectiles) {
        if (!bolt.alive) continue;
        bolt.position += bolt.velocity * dt;
        bolt.life -= dt;
        if (bolt.life <= 0.0f) {
            bolt.alive = false;
            continue;
        }
        if (bolt.hostile) {
            // Enemy fire: hits the first player it overlaps (respecting i-frames).
            for (auto& p : world_.players) {
                if (!p.active || p.invuln > 0.0f) continue;
                const float r = bolt.radius + p.entity.radius;
                if (distanceSquared(bolt.position, p.entity.position) > r * r) continue;
                p.entity.hp -= bolt.damage;
                p.entity.hitFlash = std::max(p.entity.hitFlash, 0.1f);
                bolt.alive = false;
                if (p.entity.hp <= 0) {
                    const int pid = static_cast<int>(&p - world_.players.data());
                    downPlayer(pid);
                }
                break;
            }
            continue;
        }
        for (auto& enemy : world_.enemies) {
            if (!enemy.alive) continue;
            const float r = bolt.radius + enemy.radius;
            if (distanceSquared(bolt.position, enemy.position) > r * r) continue;
            hurtEnemy(enemy, bolt.damage, bolt.velocity.normalized(), bolt.owner,
                      static_cast<float>(bolt.damage));
            bolt.alive = false;
            break;
        }
    }
    world_.projectiles.erase(
        std::remove_if(world_.projectiles.begin(), world_.projectiles.end(),
                       [](const Projectile& p) { return !p.alive; }),
        world_.projectiles.end());
}

int Simulation::nearestActivePlayerId(const Vec2& from) const {
    int best = -1;
    float bestSq = 0.0f;
    for (int i = 0; i < static_cast<int>(world_.players.size()); ++i) {
        if (!world_.players[i].active) continue;
        const float d = distanceSquared(from, world_.players[i].entity.position);
        if (best < 0 || d < bestSq) {
            best = i;
            bestSq = d;
        }
    }
    return best;
}

int Simulation::nearestPreyIndex(std::size_t self) const {
    const Entity& hunter = world_.enemies[self];
    const float huntSq = kPredatorHuntRadius * kPredatorHuntRadius;
    int best = -1;
    float bestSq = huntSq;
    for (std::size_t j = 0; j < world_.enemies.size(); ++j) {
        if (j == self) continue;
        const Entity& other = world_.enemies[j];
        if (!other.alive) continue;
        const float d = distanceSquared(hunter.position, other.position);
        if (d < bestSq) {
            bestSq = d;
            best = static_cast<int>(j);
        }
    }
    return best;
}

void Simulation::absorbPrey(Entity& predator, Entity& prey) {
    prey.alive = false;
    ++predator.predatorKills;
    // It eats the prey's strength: grows bigger, hits harder, and — like a
    // player's level-up — heals to full as a reward for the kill.
    predator.level += std::max(1, prey.level);
    predator.maxHp = static_cast<int>(static_cast<float>(predator.maxHp) * kPredatorHpGrowth);
    predator.hp = predator.maxHp;
    predator.contactDamage =
        static_cast<int>(static_cast<float>(predator.contactDamage) * kPredatorDmgGrowth) + 1;
    predator.radius = std::min(predator.radius + 1.5f, kPredatorMaxRadius);
    predator.aggroRadius += 20.0f;  // a bigger beast senses (and hunts) farther

    // Scavenge: inherit whatever the prey had hoarded (so loot flows UP the food
    // chain into ever-fatter alphas), then roll a fresh item off its corpse.
    for (auto& it : prey.hoard) predator.hoard.push_back(std::move(it));
    prey.hoard.clear();
    Item scavenged = lootRandom();
    if (scavenged.kind == ItemKind::Weapon || scavenged.kind == ItemKind::Armor) {
        const Rarity r = rollRarity(prey.type);
        applyRarity(scavenged, r);
        rollAffixes(scavenged, r);
        predator.hoard.push_back(std::move(scavenged));
    }
    // Cap the hoard so a runaway alpha can't accumulate unbounded loot; keep the
    // most recently scavenged (which trend richer, being rolled off tougher prey).
    if (predator.hoard.size() > kMaxHoard)
        predator.hoard.erase(predator.hoard.begin(),
                             predator.hoard.end() - static_cast<std::ptrdiff_t>(kMaxHoard));
}

void Simulation::updateAggro(Entity& enemy, int target, bool hasThreat) {
    // Being attacked provokes anything, even a passive mob.
    if (hasThreat) {
        enemy.aggroed = true;
        return;
    }
    if (target < 0) {  // nobody active to chase
        enemy.aggroed = false;
        return;
    }

    const float distSq = distanceSquared(enemy.position, world_.players[target].entity.position);
    const float rSq = enemy.aggroRadius * enemy.aggroRadius;

    switch (enemy.behavior) {
        case AggroBehavior::Passive:
            // Only fights when hit (handled above); proximity alone never aggros.
            enemy.aggroed = false;
            break;
        case AggroBehavior::Aggressive:
            // Sticky: aggro inside the radius, let go only once you break well away.
            if (distSq <= rSq)
                enemy.aggroed = true;
            else if (enemy.aggroed &&
                     distSq > rSq * kAggressiveDeaggroMul * kAggressiveDeaggroMul)
                enemy.aggroed = false;
            break;
        case AggroBehavior::Defensive: {
            // Aggro in range, but leash back home if dragged too far from it.
            if (distSq <= rSq) enemy.aggroed = true;
            const float leash = enemy.aggroRadius * kDefensiveLeashMul;
            if (distanceSquared(enemy.position, enemy.home) > leash * leash) enemy.aggroed = false;
            break;
        }
        case AggroBehavior::Pack:
            // Aggro in range; pack cohesion (waking mates) is a second pass below.
            if (distSq <= rSq) enemy.aggroed = true;
            break;
    }
}

void Simulation::tickStatus(Entity& e, float dt) {
    if (e.burnTime > 0.0f) {
        e.burnTime = clampToZero(e.burnTime - dt);
        e.dotAccum += static_cast<float>(e.burnDps) * dt;
    }
    if (e.poisonTime > 0.0f) {
        e.poisonTime = clampToZero(e.poisonTime - dt);
        e.dotAccum += static_cast<float>(e.poisonDps) * dt;
    }
    if (e.slowTime > 0.0f) e.slowTime = clampToZero(e.slowTime - dt);
    if (e.dotAccum >= 1.0f) {  // apply whole points of accumulated damage-over-time
        const int dmg = static_cast<int>(e.dotAccum);
        e.dotAccum -= static_cast<float>(dmg);
        e.hp -= dmg;
        e.hitFlash = std::max(e.hitFlash, 0.1f);
    }
}

void Simulation::applyStatusToPlayer(Player& p, const Entity& source) {
    if (source.type != EnemyType::Spitter) return;  // only venom spitters poison
    p.entity.poisonTime = std::max(p.entity.poisonTime, kSpitterPoisonTime);
    p.entity.poisonDps = std::max(p.entity.poisonDps, kSpitterPoisonDps);
    p.entity.slowTime = std::max(p.entity.slowTime, kSpitterSlowTime);
}

void Simulation::enemyRangedAct(Entity& enemy, int target, float dt, bool& holdPosition) {
    holdPosition = false;
    if (target < 0) return;
    const Vec2 targetPos = world_.players[target].entity.position;
    const Vec2 to = targetPos - enemy.position;
    const float dist = to.length();
    const Vec2 dir = dist > 1.0f ? to * (1.0f / dist) : Vec2{1.0f, 0.0f};
    enemy.attackTimer = clampToZero(enemy.attackTimer - dt);

    if (enemy.type == EnemyType::Archer) {
        // Kite: hold ground (and back off) inside keep-distance, and shoot on cadence.
        if (dist < kArcherKeepDistance) holdPosition = true;
        if (dist <= enemy.aggroRadius && enemy.attackTimer <= 0.0f) {
            enemy.attackTimer = kArcherFireCooldown;
            Projectile bolt;
            bolt.position = enemy.position;
            bolt.velocity = dir * kEnemyBoltSpeed;
            bolt.damage = kArcherBoltDamage;
            bolt.radius = 6.0f;
            bolt.life = enemy.aggroRadius / kEnemyBoltSpeed;
            bolt.hostile = true;
            world_.projectiles.push_back(bolt);
        }
        return;
    }

    if (enemy.type == EnemyType::Sorcerer) {
        holdPosition = true;  // a caster stands and channels
        if (enemy.windup > 0.0f) {
            enemy.windup = clampToZero(enemy.windup - dt);
            if (enemy.windup <= 0.0f) {  // the telegraph resolves into a heavy blast
                Projectile bolt;
                bolt.position = enemy.position;
                bolt.velocity = dir * (kEnemyBoltSpeed * 1.15f);
                bolt.damage = kSorcererBoltDamage;
                bolt.radius = 12.0f;
                bolt.life = enemy.aggroRadius / kEnemyBoltSpeed;
                bolt.hostile = true;
                world_.projectiles.push_back(bolt);
                enemy.attackTimer = kSorcererFireCooldown;
            }
        } else if (dist <= enemy.aggroRadius && enemy.attackTimer <= 0.0f) {
            enemy.windup = kSorcererWindup;  // start the readable wind-up
        }
        return;
    }
}

void Simulation::updateEnemies(float dt) {
    const float decay = std::max(0.0f, 1.0f - kThreatDecayPerSec * dt);

    // Pass 1: decay threat and (re)decide each enemy's target + aggro state.
    std::vector<int> targets(world_.enemies.size(), -1);
    for (std::size_t i = 0; i < world_.enemies.size(); ++i) {
        Entity& enemy = world_.enemies[i];
        if (!enemy.alive) continue;
        enemy.hitFlash = clampToZero(enemy.hitFlash - dt);
        tickStatus(enemy, dt);  // burn/poison DoT (from player on-hit affixes, later)
        if (enemy.hp <= 0) {
            killEnemy(enemy, -1);  // died to a damage-over-time effect
            continue;
        }
        for (auto& t : enemy.threat) t *= decay;  // threat fades so aggro stays dynamic

        // Chase the highest-threat player; if nobody has threat yet, the nearest.
        int target = highestThreatPlayer(enemy);
        const bool hasThreat = target >= 0;
        if (target < 0) target = nearestActivePlayerId(enemy.position);
        targets[i] = target;
        updateAggro(enemy, target, hasThreat);
    }

    // Pass 2: pack cohesion — an aggroed pack member wakes nearby pack-mates.
    for (const Entity& waker : world_.enemies) {
        if (!waker.alive || waker.behavior != AggroBehavior::Pack || !waker.aggroed) continue;
        for (auto& other : world_.enemies) {
            if (&other == &waker || !other.alive || other.behavior != AggroBehavior::Pack) continue;
            if (other.aggroed) continue;
            if (distanceSquared(waker.position, other.position) <= kPackWakeRadius * kPackWakeRadius)
                other.aggroed = true;
        }
    }

    // Pass 3: move. Aggroed enemies chase; idle Defensive/Pack drift back home.
    for (std::size_t i = 0; i < world_.enemies.size(); ++i) {
        Entity& enemy = world_.enemies[i];
        if (!enemy.alive) continue;
        const int target = targets[i];

        Vec2 dest{};
        bool moving = false;
        if (enemy.aggroed && target >= 0) {
            enemy.targetPlayer = target;
            const Vec2 tp = world_.players[target].entity.position;
            // Ranged/caster enemies shoot; they hold ground or kite instead of
            // charging in. `hold` means "don't advance" (archer too close / caster
            // channelling); an archer that's too close actively backs away.
            bool hold = false;
            if (enemy.type == EnemyType::Archer || enemy.type == EnemyType::Sorcerer)
                enemyRangedAct(enemy, target, dt, hold);
            if (enemy.type == EnemyType::Archer && hold) {
                dest = enemy.position + (enemy.position - tp);  // retreat to keep range
                moving = true;
            } else if (!hold) {
                dest = tp;
                moving = true;
            }
        } else if (enemy.predator) {
            // No player to chase → hunt the food chain. Stalk the nearest mob,
            // close in, and bite on a cadence; a lethal bite eats it whole.
            enemy.targetPlayer = -1;
            enemy.huntTimer = clampToZero(enemy.huntTimer - dt);
            const int prey = nearestPreyIndex(i);
            if (prey >= 0) {
                Entity& victim = world_.enemies[prey];
                dest = victim.position;
                moving = true;
                const float reach = enemy.radius + victim.radius + kPredatorBiteReach;
                if (enemy.huntTimer <= 0.0f &&
                    distanceSquared(enemy.position, victim.position) <= reach * reach) {
                    enemy.huntTimer = kPredatorBiteCooldown;
                    const Vec2 dir = (victim.position - enemy.position).normalized();
                    victim.hp -= enemy.contactDamage;
                    victim.hitFlash = kHitFlash;
                    const float resist = 14.0f / std::max(victim.radius, 1.0f);
                    victim.knockback = dir * (kKnockback * resist);
                    if (victim.hp <= 0) absorbPrey(enemy, victim);
                }
            } else if (distanceSquared(enemy.position, enemy.home) > kHomeEpsilon * kHomeEpsilon) {
                dest = enemy.home;  // nothing to eat nearby → drift home and wait
                moving = true;
            }
        } else {
            enemy.targetPlayer = -1;
            // Non-aggressive mobs saunter back to their post when they lose interest.
            if (enemy.behavior != AggroBehavior::Aggressive &&
                distanceSquared(enemy.position, enemy.home) > kHomeEpsilon * kHomeEpsilon) {
                dest = enemy.home;
                moving = true;
            }
        }
        if (moving) {
            const Vec2 to = dest - enemy.position;
            if (to.length() > 1.0f) {
                const float spd = enemy.speed * (enemy.slowTime > 0.0f ? kStatusSlowFactor : 1.0f);
                enemy.velocity = to.normalized() * spd;
                enemy.position += enemy.velocity * dt;
            }
        }

        // Apply then bleed off knockback, so a hit visibly shoves the enemy back.
        enemy.position += enemy.knockback * dt;
        enemy.knockback = enemy.knockback * clampToZero(1.0f - kKnockbackDecay * dt);

        // Keep enemies inside the arena (knockback near an edge could eject them).
        enemy.position.x = std::clamp(enemy.position.x, enemy.radius, world_.width - enemy.radius);
        enemy.position.y = std::clamp(enemy.position.y, enemy.radius, world_.height - enemy.radius);
    }
}

void Simulation::hurtEnemy(Entity& enemy, int damage, const Vec2& fromDir, int killerId,
                           float threat) {
    enemy.hp -= damage;
    addThreat(enemy, killerId, threat);

    // Lifesteal affix: the attacker sips back a fraction of the damage dealt.
    if (killerId >= 0 && killerId < static_cast<int>(world_.players.size())) {
        Player& kp = world_.players[killerId];
        if (kp.active && kp.entity.hp < kp.entity.maxHp) {
            const int ls = lifestealPct(kp);
            if (ls > 0) {
                const int heal = std::max(1, damage * ls / 100);
                kp.entity.hp = std::min(kp.entity.maxHp, kp.entity.hp + heal);
                kp.healFlash = std::max(kp.healFlash, 0.15f);
            }
        }
    }
    enemy.hitFlash = kHitFlash;
    // Heavier enemies (bigger radius) resist knockback; bosses barely flinch.
    const float resist = 14.0f / std::max(enemy.radius, 1.0f);
    enemy.knockback = fromDir * (kKnockback * resist);
    if (enemy.hp <= 0) killEnemy(enemy, killerId);
}

void Simulation::resolveEnemyContact(int playerId) {
    Player& p = world_.players[playerId];
    if (p.invuln > 0.0f) return;
    for (const auto& enemy : world_.enemies) {
        if (!enemy.alive) continue;
        if (!overlaps(p.entity, enemy)) continue;

        int dmg = enemy.contactDamage;
        if (p.cls.id == ClassId::Tank) dmg = std::max(1, dmg / 2);  // Tank passive
        if (p.shieldTimer > 0.0f) dmg = std::max(1, dmg / 4);       // Taunt shield
        p.entity.hp -= dmg;
        p.invuln = kEnemyContactCooldown;
        applyStatusToPlayer(p, enemy);  // e.g. a spitter leaves you poisoned + slowed
        if (p.entity.hp <= 0) downPlayer(playerId);
        break;
    }
}

void Simulation::resolveSeparation() {
    // Push apart overlapping bodies of the same kind so they read as solid and
    // stop stacking on one tile. Each body takes half the overlap; a small factor
    // keeps dense packs from jittering. Player-vs-enemy is left alone — that's
    // what contact damage / melee are for.
    constexpr float kPush = 0.5f;
    auto separate = [](Entity& a, Entity& b) {
        const Vec2 delta = b.position - a.position;
        const float dist = delta.length();
        const float overlap = (a.radius + b.radius) - dist;
        if (overlap <= 0.0f) return;
        const Vec2 n = dist > 1e-3f ? delta * (1.0f / dist) : Vec2{1.0f, 0.0f};
        const Vec2 shove = n * (overlap * 0.5f * kPush);
        a.position -= shove;
        b.position += shove;
    };

    const int pn = static_cast<int>(world_.players.size());
    for (int i = 0; i < pn; ++i) {
        if (!world_.players[i].active) continue;
        for (int j = i + 1; j < pn; ++j) {
            if (!world_.players[j].active) continue;
            separate(world_.players[i].entity, world_.players[j].entity);
        }
    }

    const int en = static_cast<int>(world_.enemies.size());
    for (int i = 0; i < en; ++i) {
        if (!world_.enemies[i].alive) continue;
        for (int j = i + 1; j < en; ++j) {
            if (!world_.enemies[j].alive) continue;
            separate(world_.enemies[i], world_.enemies[j]);
        }
    }

    // Separation can nudge a body out of bounds; pull everyone back in.
    auto clampIn = [this](Entity& e) {
        e.position.x = std::clamp(e.position.x, e.radius, world_.width - e.radius);
        e.position.y = std::clamp(e.position.y, e.radius, world_.height - e.radius);
    };
    for (auto& p : world_.players)
        if (p.active) clampIn(p.entity);
    for (auto& e : world_.enemies)
        if (e.alive) clampIn(e);
}

void Simulation::resolvePickups() {
    for (auto& drop : world_.loot) {
        for (auto& p : world_.players) {
            if (!p.active) continue;
            const float reach = p.entity.radius + drop.radius;
            if (distanceSquared(p.entity.position, drop.position) > reach * reach) continue;

            if (drop.item.kind == ItemKind::Gold) {
                world_.gold += drop.item.value;
                drop.item.weight = -1.0f;
            } else if (world_.inventory.tryAdd(drop.item)) {
                drop.item.weight = -1.0f;  // into the shared bank
            }
            if (drop.item.weight < 0.0f) break;
        }
    }
    world_.loot.erase(std::remove_if(world_.loot.begin(), world_.loot.end(),
                                     [](const GroundItem& g) { return g.item.weight < 0.0f; }),
                      world_.loot.end());
}

void Simulation::respawnWaveIfClear() {
    // Only the wave counts toward "clear"; idle world mobs stay put forever.
    const bool anyWaveAlive = std::any_of(world_.enemies.begin(), world_.enemies.end(),
                                          [](const Entity& e) { return e.alive && !e.worldMob; });
    if (anyWaveAlive) return;
    // Drop the dead (spent wave enemies + slain world mobs); keep live world mobs.
    world_.enemies.erase(std::remove_if(world_.enemies.begin(), world_.enemies.end(),
                                        [](const Entity& e) { return !e.alive; }),
                         world_.enemies.end());
    ++world_.wave;
    spawnWave();
}

uint32_t Simulation::nextRand() {
    // Numerical Recipes LCG — cheap, deterministic, good enough for loot rolls.
    rngState_ = rngState_ * 1664525u + 1013904223u;
    return rngState_;
}

Rarity Simulation::rollRarity(EnemyType from) {
    const uint32_t r = nextRand() % 100;
    switch (from) {
        case EnemyType::Boss:  // bosses always drop something special
            return r < 60 ? Rarity::Epic : Rarity::Rare;
        case EnemyType::Brute:
            return r < 40 ? Rarity::Rare : (r < 85 ? Rarity::Uncommon : Rarity::Common);
        case EnemyType::Swarmer:
        case EnemyType::Grunt:
        default:
            if (r < 4) return Rarity::Rare;
            if (r < 22) return Rarity::Uncommon;
            return Rarity::Common;
    }
}

void Simulation::rollAffixes(Item& it, Rarity r) {
    if (content_.affixPool.empty()) return;
    const int count = affixCountFor(r);
    for (int i = 0; i < count; ++i) {
        const AffixSpec& spec = content_.affixPool[nextRand() % content_.affixPool.size()];
        const int span = spec.maxMag - spec.minMag;
        const int mag = spec.minMag + (span > 0 ? static_cast<int>(nextRand() % (span + 1)) : 0);
        it.affixes.push_back({spec.type, mag});
    }
}

void Simulation::dropLoot(EnemyType from, const Vec2& at, Rarity minRarity) {
    GroundItem drop;
    drop.position = at;
    Item item = lootRandom();
    // Only gear rolls rarity + affixes; gold/potions stay plain.
    if (item.kind == ItemKind::Weapon || item.kind == ItemKind::Armor) {
        Rarity r = rollRarity(from);
        if (static_cast<int>(r) < static_cast<int>(minRarity)) r = minRarity;  // elite floor
        applyRarity(item, r);
        rollAffixes(item, r);
    }
    drop.item = item;
    world_.loot.push_back(drop);
}

void Simulation::killEnemy(Entity& enemy, int killerId) {
    enemy.alive = false;
    // Elites are targets of desire: an extra drop, floored at Rare.
    if (enemy.elite) {
        dropLoot(enemy.type, enemy.position, Rarity::Rare);
        dropLoot(enemy.type, enemy.position + Vec2{18.0f, 0.0f}, Rarity::Rare);
    } else {
        dropLoot(enemy.type, enemy.position);
    }
    awardXp(killerId, kXpPerKill * std::max(1, enemy.level));  // deeper zones reward more

    // A slain predator spills everything it scavenged: beat the alpha, take its
    // hoard. Combined with the XP from its inflated level (above), killing an
    // alpha pays out all the mobs it ate — the risk *is* the reward.
    if (!enemy.hoard.empty()) {
        int k = 0;
        for (auto& it : enemy.hoard) {
            GroundItem drop;
            drop.item = std::move(it);
            const float ang = static_cast<float>(k) * 1.1f;
            const float rad = 18.0f + 5.0f * static_cast<float>(k);
            drop.position = enemy.position + Vec2{std::cos(ang) * rad, std::sin(ang) * rad};
            world_.loot.push_back(std::move(drop));
            ++k;
        }
        enemy.hoard.clear();
    }

    // Slime split: a dying slime bursts into two smaller, angrier slimes — until
    // they're too small to split again. Queued (not pushed now) so we never
    // reallocate the enemies vector mid-iteration.
    if (enemy.type == EnemyType::Slime && enemy.radius > 11.0f) {
        const Vec2 pos = enemy.position;
        const int lvl = enemy.level;
        for (int k = 0; k < 2; ++k) {
            Entity child = makeEnemy(EnemyType::Slime, pos + Vec2{k == 0 ? -16.0f : 16.0f, 0.0f});
            child.radius *= 0.6f;
            child.hp = std::max(8, child.hp / 3);
            child.maxHp = child.hp;
            child.contactDamage = std::max(3, child.contactDamage / 2);
            child.level = lvl;
            child.aggroed = true;  // spawns already provoked
            child.id = nextEnemyId_++;
            pendingSpawns_.push_back(child);
        }
    }
}

void Simulation::downPlayer(int playerId) {
    Player& p = world_.players[playerId];
    p.downedFlash = 1.0f;
    p.entity.hp = p.entity.maxHp;
    p.invuln = kRespawnInvuln;
    p.entity.position = spawnOffset(playerId);
    // Lose all aggro on death so enemies re-target.
    for (auto& enemy : world_.enemies) {
        if (playerId < static_cast<int>(enemy.threat.size())) enemy.threat[playerId] = 0.0f;
    }
}

}  // namespace game
