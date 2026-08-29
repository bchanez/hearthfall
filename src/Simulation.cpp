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

// Hit-stop: a brief whole-sim freeze on impact. This is what sells "weight" — the
// game hangs for a few frames the instant a hit connects, then snaps back. Kept
// short (a few frames at 60fps) so it reads as punch, not lag; capped so chained
// AoE kills never lock the game up.
constexpr float kHitStopBigKill = 0.11f;   // a boss/elite/alpha kill hangs the frame
constexpr float kHitStopHeavyHit = 0.03f;  // a hit that takes a big bite of max HP
constexpr float kHitStopMax = 0.13f;       // ceiling so multi-kills never freeze long

// Per-hit screen-shake: a light tremble on every landed blow (see hurtEnemy), so
// connecting feels weighty even when it doesn't kill. Small + capped well under
// the kill/reward shake so it never overwhelms them; decays fast in step.
constexpr float kHitShakePerHit = 1.4f;    // shake added by one landed hit
constexpr float kHitShakeCap = 4.5f;       // ceiling on the accumulated per-hit shake
constexpr float kHitShakeDecay = 22.0f;    // per-second linear decay

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

// Threat is uniform now that classes are gone — you hold aggro by dealing damage,
// not by being "the tank". (Kept as a hook in case a threat boon returns.)
float classThreatMultiplier(const PlayerClass&) { return 1.0f; }

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

const PlayerClass& Simulation::classAt(int /*index*/) const {
    // Classless: everyone is the same neutral adventurer (the first kit). Identity
    // comes from what you wield, pump and practise — not a picked class.
    return content_.classes.front();
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

void Simulation::recomputeStats(Player& p) {
    // Class base = the level-1 baseline; VIT and gear stack on top.
    // Base HP (class + VIT + gear), then the MaxHp boon scales the whole pool.
    const int baseHp = p.cls.maxHp + p.stats.vit * kHpPerVit + gearMaxHp(p);
    p.entity.maxHp = baseHp * (100 + p.boons.maxHpPct) / 100;
    // Speed = class base, scaled up by affixes/AGI/boons, then down by the weight
    // of the gear on your back (encumbrance) — heavy plate is a real trade-off.
    p.entity.speed = p.cls.speed * moveSpeedMul(p) * encumbranceMul(p);
    if (p.entity.hp > p.entity.maxHp) p.entity.hp = p.entity.maxHp;
}

int Simulation::effectiveDamage(const Player& p) const {
    // Class base + the primary characteristic (STR melee / DEX ranged) + gear +
    // the mastery of the skill you're actually using (rises as you practise).
    const int base = p.cls.attackDamage + primaryDamageStat(p) * kDmgPerStat + gearDamage(p) +
                     activeAttackSkill(p);
    return base * (100 + p.boons.damagePct) / 100;  // Damage boon scales the total
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

void Simulation::driftStat(Player& p) {
    // Drift: on level-up your identity grows toward HOW you play — no menu to open.
    // The characteristic behind your strongest skill climbs (melee→STR, ranged→DEX,
    // arcane/heal→INT, dodge→AGI), plus a point of VIT so survivability keeps pace.
    // What you practise is who you become; the big, varied bonuses come from boons.
    int statIdx = 0;  // STR (melee) by default
    int best = p.skills.melee.level;
    if (p.skills.ranged.level > best) { best = p.skills.ranged.level; statIdx = 1; }  // DEX
    if (p.skills.arcane.level > best) { best = p.skills.arcane.level; statIdx = 2; }  // INT
    if (p.skills.heal.level > best)   { best = p.skills.heal.level;   statIdx = 2; }  // INT
    if (p.skills.dodge.level > best)  { best = p.skills.dodge.level;  statIdx = 4; }  // AGI
    addStat(p.stats, statIdx);
    addStat(p.stats, 3);  // VIT: baseline HP growth every level
    const int before = p.entity.maxHp;
    recomputeStats(p);
    p.entity.hp += std::max(0, p.entity.maxHp - before);
}

int Simulation::dodgePctFor(const Player& p) const {
    return std::min(60, p.boons.dodgePct);  // capped so you can't become untouchable
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
    // A matched drop-in gets a balanced spread (VIT, STR, DEX, AGI, INT) — no class
    // to bias it. Players still drift toward how they play via driftStat.
    static const int balancedOrder[] = {3, 0, 1, 4, 2};
    for (int i = 0; i < n; ++i) addStat(p.stats, balancedOrder[i % 5]);
}

EquipSlot Simulation::landingSlot(const Player& p, const Item& it) const {
    const EquipSlot s = resolvedSlot(it);
    // A ring fills the first free finger; both taken → it replaces Ring1.
    if (s == EquipSlot::Ring1)
        return (p.hasEquip(EquipSlot::Ring1) && !p.hasEquip(EquipSlot::Ring2)) ? EquipSlot::Ring2
                                                                               : EquipSlot::Ring1;
    // A second 1H weapon over a 1H main hand (with a free off-hand) dual-wields.
    if (s == EquipSlot::MainHand && it.hands == 1 && p.hasEquip(EquipSlot::MainHand) &&
        p.equip(EquipSlot::MainHand).hands == 1 && !p.hasEquip(EquipSlot::OffHand))
        return EquipSlot::OffHand;
    return s;
}

void Simulation::equipResolved(Player& p, const Item& raw) {
    // Normalise: fill in a slot for items built without one (tests/legacy) and
    // default a weapon to one-handed so the wield rules always have real data.
    Item picked = raw;
    picked.slot = resolvedSlot(picked);
    if (picked.kind == ItemKind::Weapon && picked.hands == 0) picked.hands = 1;

    // Send whatever is in a slot back to the shared bank and clear it.
    auto stash = [&](EquipSlot s) {
        if (p.hasEquip(s)) {
            world_.inventory.tryAdd(p.slot(s).item);
            p.slot(s) = Player::Slot{};
        }
    };
    auto place = [&](EquipSlot s, const Item& it) { p.slot(s) = Player::Slot{true, it}; };

    switch (picked.slot) {
        case EquipSlot::MainHand:
            if (picked.hands >= 2) {  // two-hander claims both hands
                stash(EquipSlot::OffHand);
                stash(EquipSlot::MainHand);
                place(EquipSlot::MainHand, picked);
            } else if (!p.hasEquip(EquipSlot::MainHand)) {
                place(EquipSlot::MainHand, picked);
            } else if (p.equip(EquipSlot::MainHand).hands == 1 && !p.hasEquip(EquipSlot::OffHand)) {
                place(EquipSlot::OffHand, picked);  // dual wield into the free hand
            } else {
                stash(EquipSlot::MainHand);
                place(EquipSlot::MainHand, picked);
            }
            break;
        case EquipSlot::OffHand:
            // A shield/focus/off-hand weapon needs a free hand: a two-hander in the
            // main hand must come off first.
            if (p.hasEquip(EquipSlot::MainHand) && p.equip(EquipSlot::MainHand).hands >= 2)
                stash(EquipSlot::MainHand);
            stash(EquipSlot::OffHand);
            place(EquipSlot::OffHand, picked);
            break;
        case EquipSlot::Ring1: {
            const EquipSlot dst = landingSlot(p, picked);
            stash(dst);
            place(dst, picked);
            break;
        }
        case EquipSlot::None:
            world_.inventory.tryAdd(picked);  // not equippable — belt and braces
            return;
        default:  // Head/Chest/Hands/Legs/Feet/Amulet: a plain one-per-slot swap
            stash(picked.slot);
            place(picked.slot, picked);
            break;
    }

    // Gear can raise max HP; grant the gained HP now so equipping feels rewarding.
    const int before = p.entity.maxHp;
    recomputeStats(p);
    p.entity.hp += std::max(0, p.entity.maxHp - before);
}

void Simulation::equipBest(int playerId) {
    if (playerId < 0 || playerId >= static_cast<int>(world_.players.size())) return;
    Player& p = world_.players[playerId];
    if (!p.active) return;
    // Greedily equip the single most-improving bank item, then repeat. Each equip
    // strictly raises the slot's power, so this terminates. Fluid loot: displaced
    // gear returns to the bank and can itself be re-picked for another slot.
    for (bool improved = true; improved;) {
        improved = false;
        const auto& items = world_.inventory.items();
        int bestIdx = -1, bestGain = 0;
        for (int i = 0; i < static_cast<int>(items.size()); ++i) {
            const Item& it = items[i];
            if (resolvedSlot(it) == EquipSlot::None) continue;  // gold/potions/junk
            const EquipSlot dst = landingSlot(p, it);
            const int cur = p.hasEquip(dst) ? itemPower(p.equip(dst)) : 0;
            const int gain = itemPower(it) - cur;
            if (gain > bestGain) {
                bestGain = gain;
                bestIdx = i;
            }
        }
        if (bestIdx >= 0) {
            const Item picked = world_.inventory.items()[bestIdx];
            world_.inventory.removeAt(static_cast<std::size_t>(bestIdx));
            equipResolved(p, picked);
            improved = true;
        }
    }
}

void Simulation::equipItemAt(int playerId, int bankIndex) {
    if (playerId < 0 || playerId >= static_cast<int>(world_.players.size())) return;
    Player& p = world_.players[playerId];
    if (!p.active) return;
    const auto& items = world_.inventory.items();
    if (bankIndex < 0 || bankIndex >= static_cast<int>(items.size())) return;
    const Item picked = items[bankIndex];
    // "Use" the selected item: a potion is drunk in place, gear is equipped. This
    // makes Enter/number on a potion in the bank do the intuitive thing (heal).
    if (picked.kind == ItemKind::Potion) {
        drinkPotionAt(playerId, bankIndex);
        return;
    }
    if (resolvedSlot(picked) == EquipSlot::None) return;  // not equippable
    // Take it out first (frees its weight), then place it — displaced pieces flow
    // back into the shared pool (fluid loot).
    world_.inventory.removeAt(static_cast<std::size_t>(bankIndex));
    equipResolved(p, picked);
}

void Simulation::sellItemAt(int bankIndex) {
    // Sell a bank item for its gold value (shared bank → shared gold). Gear only —
    // gold/potions aren't "sold". Unequip first (G) to sell something you're wearing.
    const auto& items = world_.inventory.items();
    if (bankIndex < 0 || bankIndex >= static_cast<int>(items.size())) return;
    const Item& it = items[bankIndex];
    world_.gold += std::max(1, it.value) * std::max(1, it.count);  // whole stack sells
    world_.inventory.removeAt(static_cast<std::size_t>(bankIndex));
}

int Simulation::sellJunk() {
    // One-tap declutter: sell every unequipped Common-rarity piece of gear in the
    // shared bank for its gold value. Potions and anything Uncommon+ are spared, so
    // you keep the roll-hunting depth but stop babysitting grey drops. Iterate back
    // to front so removals don't shift indices we haven't visited yet.
    int gained = 0;
    const auto& items = world_.inventory.items();
    for (int i = static_cast<int>(items.size()) - 1; i >= 0; --i) {
        const Item& it = items[i];
        const bool gear = it.kind == ItemKind::Weapon || it.kind == ItemKind::Armor;
        if (!gear || it.rarity != Rarity::Common) continue;
        gained += std::max(1, it.value) * std::max(1, it.count);
        world_.inventory.removeAt(static_cast<std::size_t>(i));
    }
    world_.gold += gained;
    return gained;
}

void Simulation::unequip(int playerId) {
    if (playerId < 0 || playerId >= static_cast<int>(world_.players.size())) return;
    Player& p = world_.players[playerId];
    // Strip every paperdoll slot back into the shared bank.
    for (int i = 0; i < kEquipSlotCount; ++i) {
        if (!p.equipment[i].has) continue;
        world_.inventory.tryAdd(p.equipment[i].item);
        p.equipment[i] = Player::Slot{};
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
        driftStat(p);                  // identity auto-grows toward how you play (no menu)
        ++p.pendingBoons;              // and you draft a build-defining boon / ability (1 of 3)
        p.entity.hp = p.entity.maxHp;  // level-up fully heals — a reward moment
    }
    // If a choice is now open and none is showing yet, roll the 3 offers.
    if (p.pendingBoons > 0 && p.boonChoices[0] < 0) rollBoonChoices(p);
}

void Simulation::rollBoonChoices(Player& p) {
    // The offer is a COMBINED space: ids [0, boonCount) are passive boons; ids from
    // boonCount up are auto-cast abilities. A spell is only eligible if it belongs
    // to the weapon you're WIELDING right now (its weaponClass) AND you've trained
    // that weapon to its mastery gate — so each weapon discovers its own spells the
    // more you play it. Passive boons are always eligible. Deterministic via PRNG.
    p.boonChoices[0] = p.boonChoices[1] = p.boonChoices[2] = -1;
    const int boonCount = static_cast<int>(content_.upgradePool.size());
    const AttackStyle style = effectiveStyle(p);
    const std::string wclass = p.hasWeapon() ? p.weapon().weaponClass : std::string();
    // Gate on the WIELDED weapon's own mastery (per-class), falling back to the
    // broad style skill when unarmed or the weapon has no class.
    const int mastery = !wclass.empty() ? p.masteryOf(wclass) : activeAttackSkill(p);

    std::vector<int> eligible;
    for (int i = 0; i < boonCount; ++i) eligible.push_back(i);  // boons: always offered
    for (int i = 0; i < static_cast<int>(content_.abilityPool.size()); ++i) {
        const AbilitySpec& spec = content_.abilityPool[i];
        if (abilityMatchesWeapon(spec, style, wclass) && mastery >= spec.minSkill)
            eligible.push_back(boonCount + i);  // ability ids live above the boons
    }
    if (eligible.empty()) return;

    const int want = std::min(3, static_cast<int>(eligible.size()));
    int filled = 0, guard = 0;
    while (filled < want && guard++ < 200) {
        const int id = eligible[nextRand() % static_cast<uint32_t>(eligible.size())];
        bool dup = false;
        for (int i = 0; i < filled; ++i)
            if (p.boonChoices[i] == id) dup = true;
        if (!dup) p.boonChoices[filled++] = id;
    }
}

void Simulation::chooseUpgrade(int playerId, int slot) {
    if (playerId < 0 || playerId >= static_cast<int>(world_.players.size())) return;
    Player& p = world_.players[playerId];
    if (!p.active || p.pendingBoons <= 0) return;
    if (slot < 0 || slot > 2) return;
    const int id = p.boonChoices[slot];
    const int boonCount = static_cast<int>(content_.upgradePool.size());
    const int total = boonCount + static_cast<int>(content_.abilityPool.size());
    if (id < 0 || id >= total) return;

    if (id >= boonCount) {
        // An ability offer: grant it (or rank it up if already owned).
        grantAbility(p, id - boonCount);
    } else {
        const UpgradeSpec& up = content_.upgradePool[id];
        const int m = up.magnitude;
        switch (up.effect) {
            case UpgradeEffect::DamagePct:      p.boons.damagePct += m; break;
            case UpgradeEffect::AttackSpeedPct: p.boons.attackSpeedPct += m; break;
            case UpgradeEffect::MoveSpeedPct:   p.boons.moveSpeedPct += m; break;
            case UpgradeEffect::MaxHpPct:       p.boons.maxHpPct += m; break;
            case UpgradeEffect::CritPct:        p.boons.critPct += m; break;
            case UpgradeEffect::LifestealPct:   p.boons.lifestealPct += m; break;
            case UpgradeEffect::MultiShot:      p.boons.extraProjectiles += m; break;
            case UpgradeEffect::Pierce:         p.boons.pierce += m; break;
            case UpgradeEffect::Regen:          p.boons.regen += m; break;
            case UpgradeEffect::Dodge:          p.boons.dodgePct += m; break;
            case UpgradeEffect::SpellPowerPct:  p.boons.spellPowerPct += m; break;
            case UpgradeEffect::ArmorPct:       p.boons.armorPct += m; break;
        }
    }
    ++p.boons.count;
    --p.pendingBoons;

    // A MaxHp boon should feel like a reward *now*: recompute and grant the gained HP.
    const int before = p.entity.maxHp;
    recomputeStats(p);
    p.entity.hp += std::max(0, p.entity.maxHp - before);

    // Show the next offer, or clear the box when there's nothing left to pick.
    if (p.pendingBoons > 0) rollBoonChoices(p);
    else p.boonChoices[0] = p.boonChoices[1] = p.boonChoices[2] = -1;
}

void Simulation::grantAbility(Player& p, int abilityId) {
    if (abilityId < 0 || abilityId >= static_cast<int>(content_.abilityPool.size())) return;
    for (auto& a : p.abilities)
        if (a.specId == abilityId) {
            ++a.rank;  // already owned → stack it stronger
            return;
        }
    p.abilities.push_back({abilityId, 1, 0.0f});  // ready to fire immediately
}

int Simulation::nearestEnemyIndex(const Vec2& from) const {
    int best = -1;
    float bestSq = 0.0f;
    for (int i = 0; i < static_cast<int>(world_.enemies.size()); ++i) {
        const Entity& e = world_.enemies[i];
        if (!e.alive) continue;
        const float d = distanceSquared(from, e.position);
        if (best < 0 || d < bestSq) {
            best = i;
            bestSq = d;
        }
    }
    return best;
}

void Simulation::updateAbilities(Player& p, int playerId, float dt) {
    if (p.abilities.empty()) return;
    // Only auto-cast when there's something to hit, so cooldowns don't idle away
    // between waves.
    bool anyEnemy = false;
    for (const auto& e : world_.enemies)
        if (e.alive) {
            anyEnemy = true;
            break;
        }
    for (auto& a : p.abilities) {
        if (a.specId < 0 || a.specId >= static_cast<int>(content_.abilityPool.size())) continue;
        a.cooldown = clampToZero(a.cooldown - dt);
        if (a.cooldown > 0.0f || !anyEnemy) continue;
        const AbilitySpec& spec = content_.abilityPool[a.specId];
        castAbility(p, playerId, spec, a.rank);
        // Higher rank fires a touch faster, down to a floor so it never machine-guns.
        a.cooldown = std::max(0.4f, spec.cooldown * (1.0f - 0.06f * static_cast<float>(a.rank - 1)));
    }
}

void Simulation::applyAbilityStatus(Entity& enemy, AbilityStatus st, float dur, int power,
                                    const Vec2& dir) {
    constexpr float kAbilityKnockback = 720.0f;  // shove of a Knock rider
    switch (st) {
        case AbilityStatus::Burn:
            enemy.burnTime = std::max(enemy.burnTime, dur);
            enemy.burnDps = std::max(enemy.burnDps, power);
            break;
        case AbilityStatus::Slow: enemy.slowTime = std::max(enemy.slowTime, dur); break;
        case AbilityStatus::Stun: enemy.stunTime = std::max(enemy.stunTime, dur); break;
        case AbilityStatus::Knock: enemy.knockback = dir * kAbilityKnockback; break;
        case AbilityStatus::None: break;
    }
}

// Cosmetic-only: pick the tint a spell's bolts/arc/ring should glow with, from its
// on-hit rider and shape (fire for burn/bleed, frost for chill, lightning for a
// chain, arcane for a plain magic cast). Never affects the sim — see SpellElement.
static int spellElement(const AbilitySpec& spec, AttackStyle style) {
    if (spec.effect == AbilityEffect::Chain) return ElemLightning;
    switch (spec.status) {
        case AbilityStatus::Burn: return ElemFire;
        case AbilityStatus::Slow: return ElemFrost;
        default: break;
    }
    return style == AttackStyle::Magic ? ElemArcane : ElemNeutral;
}

void Simulation::castAbility(Player& p, int playerId, const AbilitySpec& spec, int rank) {
    const int base = effectiveDamage(p) / 2;  // abilities scale partly with your build
    // Spell power lifts every ability above its base magnitude — a staff/focus/INT
    // caster hits far harder than the raw numbers (Phase 2 caster scaling).
    const int sp = spellPowerPct(p);
    auto amp = [sp](int dmg) { return dmg * (100 + sp) / 100; };
    const int element = spellElement(spec, effectiveStyle(p));
    // A projectile carries its spell's status rider so it lands the effect on hit,
    // plus a cosmetic element tint so it reads as fire/frost/arcane in flight.
    auto tag = [&](Projectile& b) {
        b.statusType = static_cast<int>(spec.status);
        b.statusDur = spec.statusDur;
        b.statusPower = spec.statusPower;
        b.element = element;
    };
    switch (spec.effect) {
        case AbilityEffect::Nova: {
            const float radius = p.entity.radius + 70.0f + 12.0f * static_cast<float>(rank);
            const float radiusSq = radius * radius;
            const int dmg = amp(spec.magnitude * rank + base);
            for (auto& enemy : world_.enemies) {
                if (!enemy.alive) continue;
                if (distanceSquared(p.entity.position, enemy.position) > radiusSq) continue;
                const Vec2 dir = (enemy.position - p.entity.position).normalized();
                hurtEnemy(enemy, dmg, dir, playerId, static_cast<float>(dmg));
                applyAbilityStatus(enemy, spec.status, spec.statusDur, spec.statusPower, dir);
            }
            p.attackFlash = std::max(p.attackFlash, 0.12f);
            // A shock ring that expands out to the blast radius, so the instant
            // AoE reads as a wave instead of a silent flash.
            world_.vfx.push_back({SpellVfx::Ring, p.entity.position, {}, radius, 0.3f, 0.3f, element});
            break;
        }
        case AbilityEffect::Volley: {
            const int n = std::max(1, spec.magnitude + (rank - 1) * 2);
            const int dmg = amp(base + rank * 4);
            for (int i = 0; i < n; ++i) {
                const float ang = 6.2831853f * static_cast<float>(i) / static_cast<float>(n);
                Projectile bolt;
                bolt.position = p.entity.position;
                bolt.velocity = Vec2{std::cos(ang), std::sin(ang)} * kProjectileSpeed;
                bolt.damage = dmg;
                bolt.radius = 6.0f;
                bolt.life = kRangedWeaponRange / kProjectileSpeed;
                bolt.owner = playerId;
                tag(bolt);
                world_.projectiles.push_back(bolt);
            }
            break;
        }
        case AbilityEffect::Bolt: {
            const int idx = nearestEnemyIndex(p.entity.position);
            if (idx < 0) break;
            const Vec2 dir = (world_.enemies[idx].position - p.entity.position).normalized();
            Projectile bolt;
            bolt.position = p.entity.position;
            bolt.velocity = dir * kProjectileSpeed;
            bolt.damage = amp(spec.magnitude * rank + base);
            bolt.radius = 8.0f;
            bolt.life = kRangedWeaponRange / kProjectileSpeed;
            bolt.owner = playerId;
            tag(bolt);
            world_.projectiles.push_back(bolt);
            break;
        }
        case AbilityEffect::Chain: {
            // Lightning: strike the nearest foe, then leap to the nearest not-yet-hit
            // enemy within range, up to a jump count that grows with rank. Damage
            // falls off a little per jump. Instant + juicy (hit-flash on each link).
            constexpr float kChainRange = 220.0f;
            const int jumps = 3 + (rank - 1) + spec.magnitude / 12;
            int dmg = amp(spec.magnitude * rank + base);
            Vec2 from = p.entity.position;
            std::vector<int> hit;
            for (int j = 0; j < jumps; ++j) {
                int best = -1;
                float bestSq = kChainRange * kChainRange;
                for (int i = 0; i < static_cast<int>(world_.enemies.size()); ++i) {
                    const Entity& e = world_.enemies[i];
                    if (!e.alive) continue;
                    if (std::find(hit.begin(), hit.end(), i) != hit.end()) continue;
                    const float d = distanceSquared(from, e.position);
                    if (d <= bestSq) { bestSq = d; best = i; }
                }
                if (best < 0) break;
                Entity& e = world_.enemies[best];
                const Vec2 dir = (e.position - from).normalized();
                hurtEnemy(e, dmg, dir, playerId, static_cast<float>(dmg));
                applyAbilityStatus(e, spec.status, spec.statusDur, spec.statusPower, dir);
                e.hitFlash = std::max(e.hitFlash, 0.14f);
                // Draw the arc for this link, so the chain is visible lightning
                // leaping foe-to-foe rather than damage out of nowhere.
                world_.vfx.push_back({SpellVfx::Arc, from, e.position, 0.0f, 0.16f, 0.16f, element});
                hit.push_back(best);
                from = e.position;
                dmg = std::max(1, dmg * 4 / 5);  // -20% per link
            }
            p.attackFlash = std::max(p.attackFlash, 0.12f);
            break;
        }
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
            break;  // classless: no class to pick (kept for wire/command compat)
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
        case CommandType::ChooseUpgrade:
            chooseUpgrade(cmd.playerId, cmd.classIndex);  // classIndex carries the offer slot
            break;
        case CommandType::EquipItem:
            equipItemAt(cmd.playerId, cmd.classIndex);  // classIndex carries the bank index
            break;
        case CommandType::SellItem:
            sellItemAt(cmd.classIndex);  // classIndex carries the bank index
            break;
        case CommandType::SellJunk:
            sellJunk();  // bulk-sell every Common piece of gear
            break;
    }
}

void Simulation::step(float dt) {
    // Hit-stop: while frozen we bleed the timer down and advance nothing else, so
    // the impact that set it hangs on screen for a few frames before play resumes.
    if (world_.hitStop > 0.0f) {
        world_.hitStop = clampToZero(world_.hitStop - dt);
        return;
    }

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
        updateAbilities(p, i, dt);  // drafted auto-cast abilities fire on their cooldowns
    }

    updateProjectiles(dt);
    // Fade transient spell VFX (chain arcs / nova rings) and bleed the per-hit
    // shake down — both purely cosmetic, advanced only while the sim runs.
    for (auto& fx : world_.vfx) fx.life -= dt;
    world_.vfx.erase(std::remove_if(world_.vfx.begin(), world_.vfx.end(),
                                    [](const SpellVfx& f) { return f.life <= 0.0f; }),
                     world_.vfx.end());
    world_.hitShake = clampToZero(world_.hitShake - kHitShakeDecay * dt);
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

    // Passive regeneration (Regen boon): heal fractional HP per second, banked in a
    // bucket so a small per-second value still adds up at a fixed timestep.
    if (p.boons.regen > 0 && p.entity.hp > 0 && p.entity.hp < p.entity.maxHp) {
        p.regenBucket += static_cast<float>(p.boons.regen) * dt;
        while (p.regenBucket >= 1.0f && p.entity.hp < p.entity.maxHp) {
            ++p.entity.hp;
            p.regenBucket -= 1.0f;
        }
    }
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
    const bool projectileStyle = style == AttackStyle::Ranged || style == AttackStyle::Magic;
    const float range = p.hasWeapon() ? (projectileStyle ? kRangedWeaponRange
                                                          : kMeleeWeaponRange)
                                     : cls.attackRange;

    // Per-weapon mastery (Mabinogi): the specific weapon family you're swinging
    // climbs, gating that weapon's spell discovery independently of the style.
    if (p.hasWeapon() && !p.weapon().weaponClass.empty())
        gainSkill(p.weaponSkills[p.weapon().weaponClass], kSkillXpMeleeHit);

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

    // Ranged & magic both fire toward the aim. The MultiShot boon adds bolts, fanned
    // symmetrically around the aim; the Pierce boon lets each pass through enemies.
    // Practise the matching mastery: a bow trains Ranged, a wand/staff trains
    // Arcane — the casting skill that gates magic spell discovery.
    gainSkill(style == AttackStyle::Magic ? p.skills.arcane : p.skills.ranged, kSkillXpRangedShot);
    const int bolts = 1 + std::max(0, p.boons.extraProjectiles);
    constexpr float kFanStep = 0.12f;  // radians between adjacent bolts in the fan
    for (int i = 0; i < bolts; ++i) {
        // Centre the fan on the aim: offsets -(n-1)/2 .. +(n-1)/2 times the step.
        const float off = (static_cast<float>(i) - static_cast<float>(bolts - 1) * 0.5f) * kFanStep;
        const float c = std::cos(off), s = std::sin(off);
        const Vec2 dir{p.aim.x * c - p.aim.y * s, p.aim.x * s + p.aim.y * c};
        Projectile bolt;
        bolt.position = p.entity.position;
        bolt.velocity = dir * kProjectileSpeed;
        bolt.damage = damage;
        bolt.radius = 6.0f;
        bolt.life = range / kProjectileSpeed;
        bolt.owner = playerId;
        bolt.pierce = std::max(0, p.boons.pierce);
        bolt.element = style == AttackStyle::Magic ? ElemArcane : ElemNeutral;
        world_.projectiles.push_back(bolt);
    }
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

    // Below the mastery threshold, the manual ability is a neutral safety net for
    // everyone — Second Wind, a modest self-heal scaling with INT + Heal mastery.
    // (Classless: your damage comes from weapons + drafted spells, not a class move.)
    const int heal = p.entity.maxHp / 4 + p.stats.intel * kHealPerInt + p.skills.heal.level;
    p.entity.hp = std::min(p.entity.maxHp, p.entity.hp + heal);
    p.healFlash = 0.35f;
    gainSkill(p.skills.heal, kSkillXpHeal);
}

void Simulation::performUsePotion(int playerId) {
    // Quick-heal (Q / LB): drink the FIRST potion in the shared bank.
    const auto& items = world_.inventory.items();
    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        if (items[i].kind == ItemKind::Potion) {
            drinkPotionAt(playerId, i);
            return;
        }
    }
}

void Simulation::drinkPotionAt(int playerId, int idx) {
    if (playerId < 0 || playerId >= static_cast<int>(world_.players.size())) return;
    Player& p = world_.players[playerId];
    if (!p.active) return;
    if (p.potionCooldown > 0.0f) return;
    if (p.entity.hp >= p.entity.maxHp) return;  // don't waste a potion at full HP
    const auto& items = world_.inventory.items();
    if (idx < 0 || idx >= static_cast<int>(items.size())) return;
    if (items[idx].kind != ItemKind::Potion) return;

    // Heal scales with the potion's rarity — a rarer vial mends more.
    const int pct = potionHealPercent(items[idx].rarity);
    const int heal = std::max(1, p.entity.maxHp * pct / 100);
    world_.inventory.consumeOne(static_cast<std::size_t>(idx));  // one from the stack

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
                // Dodge boon: a roll can slip the bolt (it still fizzles on contact).
                const int dodge = dodgePctFor(p);
                if (dodge > 0 && static_cast<int>(nextRand() % 100) < dodge) {
                    bolt.alive = false;
                    break;
                }
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
            if (enemy.id == bolt.lastHit) continue;  // don't re-hit the one we're passing through
            const float r = bolt.radius + enemy.radius;
            if (distanceSquared(bolt.position, enemy.position) > r * r) continue;
            hurtEnemy(enemy, bolt.damage, bolt.velocity.normalized(), bolt.owner,
                      static_cast<float>(bolt.damage));
            if (bolt.statusType != 0)  // ability bolts land their rider on hit
                applyAbilityStatus(enemy, static_cast<AbilityStatus>(bolt.statusType),
                                   bolt.statusDur, bolt.statusPower, bolt.velocity.normalized());
            bolt.lastHit = enemy.id;
            // Pierce boon: spend one pass-through instead of dying, and fly on.
            if (bolt.pierce > 0) --bolt.pierce;
            else bolt.alive = false;
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
    if (e.stunTime > 0.0f) e.stunTime = clampToZero(e.stunTime - dt);
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
    if (enemy.stunTime > 0.0f) return;  // stunned casters can't fire or wind up
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
        if (moving && enemy.stunTime <= 0.0f) {  // a stunned enemy is rooted
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
    // Every landed hit trembles the screen a touch (not just kills), so spells and
    // swings that connect but don't finish a foe still feel like they bite. A small
    // per-hit pulse the Renderer folds into its shake; capped + decayed in step.
    if (killerId >= 0)
        world_.hitShake = std::min(kHitShakeCap, world_.hitShake + kHitShakePerHit);
    // Heavier enemies (bigger radius) resist knockback; bosses barely flinch.
    const float resist = 14.0f / std::max(enemy.radius, 1.0f);
    enemy.knockback = fromDir * (kKnockback * resist);
    // Hit-stop juice: a hit that bites deep (>=25% of the target's max HP) hangs
    // the frame. A kill adds its own, bigger freeze in killEnemy just below.
    if (enemy.hp > 0 && enemy.maxHp > 0 && damage * 4 >= enemy.maxHp)
        world_.hitStop = std::min(kHitStopMax, std::max(world_.hitStop, kHitStopHeavyHit));
    if (enemy.hp <= 0) killEnemy(enemy, killerId);
}

void Simulation::resolveEnemyContact(int playerId) {
    Player& p = world_.players[playerId];
    if (p.invuln > 0.0f) return;
    for (const auto& enemy : world_.enemies) {
        if (!enemy.alive) continue;
        if (!overlaps(p.entity, enemy)) continue;

        // Dodge boon: a roll can shrug the blow entirely, granting brief i-frames.
        const int dodge = dodgePctFor(p);
        if (dodge > 0 && static_cast<int>(nextRand() % 100) < dodge) {
            p.invuln = kEnemyContactCooldown;
            break;
        }

        int dmg = enemy.contactDamage;
        if (const int ar = armorPct(p); ar > 0) dmg = std::max(1, dmg * (100 - ar) / 100);  // Ironhide boon
        if (p.shieldTimer > 0.0f) dmg = std::max(1, dmg / 4);       // Second Wind / shield
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
            } else if (drop.item.kind == ItemKind::Potion) {
                world_.inventory.forceAdd(drop.item);  // consumables always fit (bypass the cap)
                drop.item.weight = -1.0f;
            } else if (world_.inventory.tryAdd(drop.item)) {
                drop.item.weight = -1.0f;  // gear respects the weight cap (sell to make room)
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
    // Only roll affixes this slot can carry (offense on weapons, defense on armour,
    // either on off-hand/jewellery) — coherent gear, no +HP swords.
    const EquipSlot slot = resolvedSlot(it);
    std::vector<const AffixSpec*> eligible;
    for (const auto& spec : content_.affixPool)
        if (slotAllowsAffix(slot, spec.type)) eligible.push_back(&spec);
    if (eligible.empty()) return;

    const int count = affixCountFor(r);
    const float mul = rarityAffixMul(r);  // rarer → bigger rolls, not just more
    for (int i = 0; i < count; ++i) {
        const AffixSpec& spec = *eligible[nextRand() % eligible.size()];
        const int span = spec.maxMag - spec.minMag;
        const int roll = spec.minMag + (span > 0 ? static_cast<int>(nextRand() % (span + 1)) : 0);
        const int mag = std::max(1, static_cast<int>(static_cast<float>(roll) * mul));
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
    // Hit-stop juice: only *meaty* kills (boss / elite / predator-alpha) freeze the
    // frame — trash mobs don't, so mowing a horde with auto-cast abilities stays
    // fast and fluid instead of stuttering. The big kills still land with weight.
    if (enemy.type == EnemyType::Boss || enemy.elite || enemy.predator) {
        world_.hitStop = std::min(kHitStopMax, std::max(world_.hitStop, kHitStopBigKill));
    }
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
