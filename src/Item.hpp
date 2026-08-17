#pragma once

#include <cstdio>
#include <string>
#include <vector>

#include "PlayerClass.hpp"  // AttackStyle (a weapon declares how it's wielded)

namespace game {

// A rolled modifier on an item. MaxHp/Damage are flat; the rest are percentages.
enum class AffixType { MaxHp, Damage, AttackSpeed, MoveSpeed, Crit, Lifesteal };

struct Affix {
    AffixType type = AffixType::MaxHp;
    int magnitude = 0;
};

inline bool affixIsPercent(AffixType t) {
    return t == AffixType::AttackSpeed || t == AffixType::MoveSpeed || t == AffixType::Crit ||
           t == AffixType::Lifesteal;
}

inline const char* affixShortName(AffixType t) {
    switch (t) {
        case AffixType::MaxHp:       return "HP";
        case AffixType::Damage:      return "Dmg";
        case AffixType::AttackSpeed: return "ASpd";
        case AffixType::MoveSpeed:   return "MSpd";
        case AffixType::Crit:        return "Crit";
        case AffixType::Lifesteal:   return "LS";
    }
    return "?";
}

// e.g. "+8% Crit" or "+25 HP" — for the bank overlay / tooltips.
inline std::string affixLabel(const Affix& a) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "+%d%s %s", a.magnitude, affixIsPercent(a.type) ? "%" : "",
                  affixShortName(a.type));
    return buf;
}

// Broad category of an item. Drives colour on the ground and, later, what a
// class can equip. Gold is a weightless currency handled separately from the
// weight-limited inventory.
enum class ItemKind { Gold, Potion, Weapon, Armor };

// Rarity tiers. Tougher enemies roll higher rarities, which scale an item's
// bonuses (and colour it in the UI) — the core of the loot chase.
enum class Rarity { Common, Uncommon, Rare, Epic };

// A single item. Weight is what makes carrying a decision (encumbrance).
// `value` means gold amount for Gold, or a stat magnitude otherwise.
struct Item {
    std::string name;
    float weight = 0.0f;
    ItemKind kind = ItemKind::Potion;
    int value = 0;
    int bonusDamage = 0;  // weapons: added to attack damage when equipped
    int bonusMaxHp = 0;   // armor: added to max HP when equipped
    AttackStyle style = AttackStyle::Melee;  // weapons: how it's wielded (sets your style)
    Rarity rarity = Rarity::Common;
    std::vector<Affix> affixes;  // rolled modifiers (count grows with rarity)

    // Loot-table only: relative frequency of this base item in the weighted drop
    // pick. Ignored once the item is a concrete drop / in the bank.
    float dropWeight = 1.0f;
};

// How many affixes an item of this rarity rolls.
inline int affixCountFor(Rarity r) {
    switch (r) {
        case Rarity::Common:   return 0;
        case Rarity::Uncommon: return 1;
        case Rarity::Rare:     return 2;
        case Rarity::Epic:     return 3;
    }
    return 0;
}

inline float rarityMultiplier(Rarity r) {
    switch (r) {
        case Rarity::Common:   return 1.0f;
        case Rarity::Uncommon: return 1.5f;
        case Rarity::Rare:     return 2.0f;
        case Rarity::Epic:     return 3.0f;
    }
    return 1.0f;
}

inline const char* rarityPrefix(Rarity r) {
    switch (r) {
        case Rarity::Common:   return "";
        case Rarity::Uncommon: return "Uncommon ";
        case Rarity::Rare:     return "Rare ";
        case Rarity::Epic:     return "Epic ";
    }
    return "";
}

// Upgrades an item to the given rarity: scales its bonuses/value and prefixes
// its name. Pure and deterministic so it's directly unit-testable.
inline void applyRarity(Item& it, Rarity r) {
    it.rarity = r;
    const float m = rarityMultiplier(r);
    it.bonusDamage = static_cast<int>(it.bonusDamage * m);
    it.bonusMaxHp = static_cast<int>(it.bonusMaxHp * m);
    it.value = static_cast<int>(it.value * m);
    if (r != Rarity::Common) it.name = std::string(rarityPrefix(r)) + it.name;
}

}  // namespace game
