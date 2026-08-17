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
        for (const auto& item : items_) total += item.weight;
        return total;
    }

    bool canCarry(const Item& item) const {
        return currentWeight() + item.weight <= maxWeight_;
    }

    // Adds the item if it fits under the weight cap. Returns false otherwise.
    bool tryAdd(const Item& item) {
        if (!canCarry(item)) return false;
        items_.push_back(item);
        return true;
    }

    const std::vector<Item>& items() const { return items_; }
    std::size_t size() const { return items_.size(); }

    // Removes the item at index i (used when equipping from the bank).
    bool removeAt(std::size_t i) {
        if (i >= items_.size()) return false;
        items_.erase(items_.begin() + static_cast<std::ptrdiff_t>(i));
        return true;
    }

private:
    std::vector<Item> items_;
    float maxWeight_;
};

}  // namespace game
