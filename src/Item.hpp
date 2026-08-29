#pragma once

#include <cstdio>
#include <string>
#include <vector>

#include "PlayerClass.hpp"  // AttackStyle (a weapon declares how it's wielded)

namespace game {

// A rolled modifier on an item. MaxHp/Damage are flat; the rest are percentages.
enum class AffixType { MaxHp, Damage, AttackSpeed, MoveSpeed, Crit, Lifesteal, SpellPower };

struct Affix {
    AffixType type = AffixType::MaxHp;
    int magnitude = 0;
};

inline bool affixIsPercent(AffixType t) {
    return t == AffixType::AttackSpeed || t == AffixType::MoveSpeed || t == AffixType::Crit ||
           t == AffixType::Lifesteal || t == AffixType::SpellPower;
}

inline const char* affixShortName(AffixType t) {
    switch (t) {
        case AffixType::MaxHp:       return "HP";
        case AffixType::Damage:      return "Dmg";
        case AffixType::AttackSpeed: return "ASpd";
        case AffixType::MoveSpeed:   return "MSpd";
        case AffixType::Crit:        return "Crit";
        case AffixType::Lifesteal:   return "LS";
        case AffixType::SpellPower:  return "Spell";
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

// Paperdoll slots — the full Diablo/PoE-style equipment layout. Rings appear
// twice physically (Ring1/Ring2); loot declares "ring" and the equip code fills
// the first free ring. `Count` is the fixed size of a player's equipment array.
// Order matters: it's the on-wire/UI order and indexes the array directly.
enum class EquipSlot {
    None = -1,
    MainHand = 0,
    OffHand,
    Head,
    Chest,
    Hands,
    Legs,
    Feet,
    Amulet,
    Ring1,
    Ring2,
    Count
};

inline constexpr int kEquipSlotCount = static_cast<int>(EquipSlot::Count);  // 10

inline const char* slotName(EquipSlot s) {
    switch (s) {
        case EquipSlot::MainHand: return "Main";
        case EquipSlot::OffHand:  return "Off";
        case EquipSlot::Head:     return "Head";
        case EquipSlot::Chest:    return "Chest";
        case EquipSlot::Hands:    return "Hands";
        case EquipSlot::Legs:     return "Legs";
        case EquipSlot::Feet:     return "Feet";
        case EquipSlot::Amulet:   return "Amul";
        case EquipSlot::Ring1:    return "Ring";
        case EquipSlot::Ring2:    return "Ring";
        default:                  return "-";
    }
}

inline const char* slotKey(EquipSlot s) {  // loot.lua / save token
    switch (s) {
        case EquipSlot::MainHand: return "mainhand";
        case EquipSlot::OffHand:  return "offhand";
        case EquipSlot::Head:     return "head";
        case EquipSlot::Chest:    return "chest";
        case EquipSlot::Hands:    return "hands";
        case EquipSlot::Legs:     return "legs";
        case EquipSlot::Feet:     return "feet";
        case EquipSlot::Amulet:   return "amulet";
        case EquipSlot::Ring1:
        case EquipSlot::Ring2:    return "ring";
        default:                  return "none";
    }
}

inline EquipSlot slotFromKey(const std::string& s) {
    if (s == "mainhand" || s == "weapon") return EquipSlot::MainHand;
    if (s == "offhand") return EquipSlot::OffHand;
    if (s == "head") return EquipSlot::Head;
    if (s == "chest" || s == "armor") return EquipSlot::Chest;
    if (s == "hands") return EquipSlot::Hands;
    if (s == "legs") return EquipSlot::Legs;
    if (s == "feet") return EquipSlot::Feet;
    if (s == "amulet") return EquipSlot::Amulet;
    if (s == "ring") return EquipSlot::Ring1;  // canonical ring; equip picks Ring1/Ring2
    return EquipSlot::None;
}

// Which characteristic index drives a weapon's damage (0 STR / 1 DEX / 2 INT),
// from how it's wielded. Mirrors Stats' index order in World.hpp.
inline int governingStatIndex(AttackStyle style) {
    switch (style) {
        case AttackStyle::Melee:  return 0;  // STR
        case AttackStyle::Ranged: return 1;  // DEX
        case AttackStyle::Magic:  return 2;  // INT
    }
    return 0;
}

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
    EquipSlot slot = EquipSlot::None;  // paperdoll slot; None = not equippable
    int hands = 0;                     // weapons: 1 or 2 hands occupied; 0 = n/a
    Rarity rarity = Rarity::Common;
    std::vector<Affix> affixes;  // rolled modifiers (count grows with rarity)

    // Loot-table only: relative frequency of this base item in the weighted drop
    // pick. Ignored once the item is a concrete drop / in the bank.
    float dropWeight = 1.0f;

    // Weapons: which family this belongs to ("sword", "axe", "mace", "greatsword",
    // "bow", "staff"…). Drives spell discovery — each class unlocks its own
    // auto-cast abilities on level-up (see abilities.lua). Empty for non-weapons.
    std::string weaponClass;

    // Stack size for stackable consumables (potions): several identical vials
    // occupy ONE bank slot as "Health Potion x5". Always 1 for gear (each piece is
    // unique — its own rolled affixes). See Inventory's stacking logic.
    int count = 1;
};

// Stackable = identical copies merge into one slot. Only consumables (potions);
// gear never stacks because each piece carries its own rolled affixes.
inline bool itemStacks(const Item& it) { return it.kind == ItemKind::Potion; }

// Affix families. Offense sits on weapons, defense on armour; jewellery and the
// off-hand are wildcards that take either. Keeps rolled gear coherent (no +HP on a
// sword, no +crit on boots) — the loot chase reads cleanly (PoE-style).
enum class AffixGroup { Offense, Defense };

inline AffixGroup affixGroupOf(AffixType t) {
    switch (t) {
        case AffixType::MaxHp:
        case AffixType::MoveSpeed:
            return AffixGroup::Defense;
        default:  // Damage, AttackSpeed, Crit, Lifesteal, SpellPower
            return AffixGroup::Offense;
    }
}

// Can an affix of this type roll on the given slot?
//   weapon (main hand)                → offense only
//   body armour (head/chest/…/feet)   → defense only
//   off-hand & jewellery (amulet/ring)→ either (wildcard slots)
inline bool slotAllowsAffix(EquipSlot slot, AffixType t) {
    switch (slot) {
        case EquipSlot::MainHand:
            return affixGroupOf(t) == AffixGroup::Offense;
        case EquipSlot::OffHand:
        case EquipSlot::Amulet:
        case EquipSlot::Ring1:
        case EquipSlot::Ring2:
            return true;  // wildcard
        case EquipSlot::None:
            return false;
        default:  // Head, Chest, Hands, Legs, Feet
            return affixGroupOf(t) == AffixGroup::Defense;
    }
}

// Rarity scales an affix's rolled magnitude, so a higher tier is bigger, not just
// more numerous. (Affixes only appear on Uncommon+.) Tunable by feel.
inline float rarityAffixMul(Rarity r) {
    switch (r) {
        case Rarity::Common:   return 1.0f;
        case Rarity::Uncommon: return 1.0f;
        case Rarity::Rare:     return 1.4f;
        case Rarity::Epic:     return 1.9f;
    }
    return 1.0f;
}

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

// How much of your max HP a potion restores, by rarity — a rarer vial is a
// bigger heal (an Epic tops you off entirely). Pure so it's easy to test/tune.
inline int potionHealPercent(Rarity r) {
    switch (r) {
        case Rarity::Common:   return 35;
        case Rarity::Uncommon: return 55;
        case Rarity::Rare:     return 75;
        case Rarity::Epic:     return 100;
    }
    return 35;
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

// The paperdoll slot an item actually occupies. Falls back to a sensible slot by
// kind when `slot` wasn't set (items built in code/tests, or legacy save data):
// a plain weapon -> main hand, a plain armour -> chest.
inline EquipSlot resolvedSlot(const Item& it) {
    if (it.slot != EquipSlot::None) return it.slot;
    if (it.kind == ItemKind::Weapon) return EquipSlot::MainHand;
    if (it.kind == ItemKind::Armor) return EquipSlot::Chest;
    return EquipSlot::None;
}

// Total contribution of one stat from a single item, with the base bonus folded
// into Damage/MaxHp so weapons/armor read on the same axis as their affixes. Lets
// the bank show a clean per-stat "equipped -> candidate" delta. Pure/testable.
inline int itemStatTotal(const Item& it, AffixType t) {
    int sum = 0;
    for (const auto& a : it.affixes)
        if (a.type == t) sum += a.magnitude;
    if (t == AffixType::Damage) sum += it.bonusDamage;
    if (t == AffixType::MaxHp) sum += it.bonusMaxHp;
    return sum;
}

// Rough "worth" of a single affix, in a common currency, so gear can be ranked at
// a glance. Weights reflect how much a point of each stat is worth in practice
// (flat HP is cheap; a % of attack speed or crit is dear). Tunable by feel.
inline int affixPower(const Affix& a) {
    switch (a.type) {
        case AffixType::MaxHp:       return a.magnitude / 2;      // flat HP: cheap
        case AffixType::Damage:      return a.magnitude * 2;      // flat dmg: strong
        case AffixType::AttackSpeed: return a.magnitude * 5 / 2;  // %: ~2.5 each
        case AffixType::MoveSpeed:   return a.magnitude * 3 / 2;  // %
        case AffixType::Crit:        return a.magnitude * 2;      // %
        case AffixType::Lifesteal:   return a.magnitude * 2;      // %
        case AffixType::SpellPower:  return a.magnitude * 2;      // %: caster power
    }
    return 0;
}

// A single-number "power" for a piece of gear — the weighted sum of its combat
// stats (base bonus + affixes). A scanning aid only, NOT the whole story: it
// ignores weight/encumbrance and your characteristics (which depend on who
// equips it), so the per-stat delta stays the real decider. Pure/testable.
inline int itemPower(const Item& it) {
    int p = it.bonusDamage * 2 + it.bonusMaxHp / 2;
    for (const auto& a : it.affixes) p += affixPower(a);
    return p;
}

}  // namespace game
