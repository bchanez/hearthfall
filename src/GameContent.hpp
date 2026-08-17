#pragma once

#include <vector>

#include "Item.hpp"
#include "PlayerClass.hpp"

namespace game {

// All data-driven content the Simulation needs: the playable classes (index
// order = selection order) and the loot table. Loaded from Lua at startup by
// ScriptEngine, or falls back to defaultContent() below (used in tests and if
// the data files are missing). The Simulation itself never touches Lua.
// A rollable affix: a stat and the magnitude range it draws from.
struct AffixSpec {
    AffixType type = AffixType::MaxHp;
    int minMag = 1;
    int maxMag = 1;
};

struct GameContent {
    std::vector<PlayerClass> classes;
    std::vector<Item> lootTable;
    std::vector<AffixSpec> affixPool;
};

// Built-in fallback, mirroring the Lua data files. Keeping this in C++ means the
// game (and the tests) still run with no data/ directory present.
inline GameContent defaultContent() {
    GameContent c;
    c.classes = {
        makeClass(ClassId::Human),
        makeClass(ClassId::Tank),
        makeClass(ClassId::Archer),
        makeClass(ClassId::Healer),
    };
    //                name             weight  kind              value +dmg +hp  style                  rarity          affixes
    c.lootTable = {
        {"Gold Coins", 0.0f, ItemKind::Gold, 15, 0, 0, AttackStyle::Melee, Rarity::Common, {}},
        {"Health Potion", 0.5f, ItemKind::Potion, 25, 0, 0, AttackStyle::Melee, Rarity::Common, {}},
        {"Rusty Sword", 3.0f, ItemKind::Weapon, 8, 6, 0, AttackStyle::Melee, Rarity::Common, {}},
        {"Short Bow", 2.0f, ItemKind::Weapon, 10, 7, 0, AttackStyle::Ranged, Rarity::Common, {}},
        {"Leather Armor", 5.0f, ItemKind::Armor, 12, 0, 20, AttackStyle::Melee, Rarity::Common, {}},
        {"Iron Shield", 8.0f, ItemKind::Armor, 20, 0, 40, AttackStyle::Melee, Rarity::Common, {}},
    };
    c.affixPool = {
        {AffixType::MaxHp, 10, 40},
        {AffixType::Damage, 3, 10},
        {AffixType::AttackSpeed, 5, 20},
        {AffixType::MoveSpeed, 5, 15},
        {AffixType::Crit, 3, 12},
        {AffixType::Lifesteal, 3, 10},
    };
    return c;
}

}  // namespace game
