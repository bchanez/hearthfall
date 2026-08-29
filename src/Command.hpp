#pragma once

#include "Vec2.hpp"

namespace game {

// A player intention, applied by the Simulation. This is the seam DESIGN.md
// mandates: solo, a Command is a local call; online later, the *same* Command
// simply travels over the network first — nothing else changes.
enum class CommandType {
    Move,
    Attack,
    Ability,
    SelectClass,
    Equip,
    Unequip,
    UsePotion,
    Dash,
    AllocStat,      // spend one banked point into a characteristic
    ChooseUpgrade,  // pick a level-up boon: classIndex carries the slot (0..2)
    EquipItem,      // equip a specific bank item: classIndex carries its index
    SellItem,       // sell a bank item for gold: classIndex carries its index
    SellJunk,       // sell every unequipped Common-rarity piece of gear at once
};

struct Command {
    CommandType type = CommandType::Move;
    int playerId = 0;    // which player this intention is for
    Vec2 dir{};          // Move: movement direction; Attack/Ability: aim direction
    int classIndex = 0;  // SelectClass: class index — or AllocStat: stat index (0..4)
};

}  // namespace game
