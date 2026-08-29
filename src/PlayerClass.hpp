#pragma once

#include <string>

namespace game {

// Human is the neutral starting "class" for the classless direction: everyone
// begins here and specializes by what they pump and what they wield. Tank/Archer/
// Healer remain as starting stat kits until ClassId is retired entirely (Phase 4).
enum class ClassId { Human, Tank, Archer, Healer };
// How a weapon is wielded, and which characteristic drives its damage:
//   Melee  -> STR   Ranged -> DEX   Magic -> INT
// The equipped main-hand weapon sets this; unarmed falls back to the class style.
enum class AttackStyle { Melee, Ranged, Magic };

// Stat block + one signature ability for a playable class. This is the seed of
// the full roster (berserker, mage, summoner…) in DESIGN.md. Data like this
// will eventually live in Lua; for now it's a small hand-written table.
struct PlayerClass {
    std::string name;
    ClassId id = ClassId::Archer;
    int maxHp = 100;
    float speed = 260.0f;
    AttackStyle attackStyle = AttackStyle::Melee;
    int attackDamage = 20;
    float attackRange = 80.0f;     // melee reach, or projectile travel distance
    float attackCooldown = 0.30f;  // seconds between attacks
    std::string abilityName;
    float abilityCooldown = 6.0f;
};

inline PlayerClass makeClass(ClassId id) {
    switch (id) {
        case ClassId::Human:
            // Neutral adventurer: balanced, unarmed melee. Your weapon and your
            // stats decide who you become. Ability: Second Wind (small self-heal).
            return {"Human", ClassId::Human, 130,  240.0f,        AttackStyle::Melee,
                    14,      65.0f,          0.40f, "Second Wind", 6.0f};
        case ClassId::Tank:
            // Beefy, slow, short melee. Passive: takes half damage + builds huge
            // threat. Ability: Taunt (rips aggro onto itself + brief shield).
            return {"Tank", ClassId::Tank, 220, 200.0f, AttackStyle::Melee,
                    18,     70.0f,         0.45f, "Taunt", 7.0f};
        case ClassId::Archer:
            // Squishy ranged DPS. Ability: Power Shot (heavy bolt).
            return {"Archer", ClassId::Archer, 90,    280.0f,      AttackStyle::Ranged,
                    22,       520.0f,          0.28f, "Power Shot", 5.0f};
        case ClassId::Healer:
            // Support: weak ranged poke. Ability: Mend (self-heal for now).
            return {"Healer", ClassId::Healer, 120,  250.0f, AttackStyle::Ranged,
                    12,       380.0f,          0.50f, "Mend", 6.0f};
    }
    return {};
}

}  // namespace game
