#pragma once

#include <vector>

#include "Item.hpp"

namespace game {

// Weight-limited carrying. This is the seed of the "shared household bank"
// from GAME_DESIGN.md — for now it's a single solo inventory with a hard
// weight cap, so picking loot is a real trade-off.
class Inventory {
public:
    explicit Inventory(float maxWeight) : maxWeight_(maxWeight) {}

    float maxWeight() const { return maxWeight_; }

    float currentWeight() const {
        float total = 0.0f;
        for (const auto& item : items_) total += item.weight * static_cast<float>(item.count);
        return total;
    }

    bool canCarry(const Item& item) const {
        return currentWeight() + item.weight * static_cast<float>(item.count) <= maxWeight_;
    }

    // Adds the item if it fits under the weight cap. Returns false otherwise.
    // Stackable consumables merge into an existing stack (never blocked, since a
    // merge adds no new slot — only a bigger count).
    bool tryAdd(const Item& item) {
        if (tryStack(item)) return true;
        if (!canCarry(item)) return false;
        items_.push_back(item);
        return true;
    }

    // Adds unconditionally, ignoring the weight cap. For consumables (potions) that
    // should never be blocked by a bank full of heavy gear.
    void forceAdd(const Item& item) {
        if (!tryStack(item)) items_.push_back(item);
    }

    const std::vector<Item>& items() const { return items_; }
    std::size_t size() const { return items_.size(); }

    // Consume one unit from a (stackable) slot: decrement its count, dropping the
    // slot only when the last one is used. Returns false on a bad index.
    bool consumeOne(std::size_t i) {
        if (i >= items_.size()) return false;
        if (items_[i].count > 1) {
            --items_[i].count;
            return true;
        }
        return removeAt(i);
    }

    // Removes the item at index i (used when equipping from the bank).
    bool removeAt(std::size_t i) {
        if (i >= items_.size()) return false;
        items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(i));
        return true;
    }

private:
    // Merge a stackable item into an existing identical stack (same name + rarity).
    // Returns true if merged, false if it needs its own slot.
    bool tryStack(const Item& item) {
        if (!itemStacks(item)) return false;
        for (auto& it : items_) {
            if (itemStacks(it) && it.name == item.name && it.rarity == item.rarity) {
                it.count += item.count;
                return true;
            }
        }
        return false;
    }

    std::vector<Item> items_;
    float maxWeight_;
};

}  // namespace game
