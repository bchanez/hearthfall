#pragma once

#include "Vec2.hpp"

namespace game {

// A flying bullet/arrow/bolt. Owned by the simulation; lives until it hits an
// enemy or its lifetime runs out.
struct Projectile {
    Vec2 position{};
    Vec2 velocity{};
    float radius = 6.0f;
    int damage = 0;
    float life = 0.0f;  // seconds remaining before it despawns
    bool alive = true;
    int owner = 0;       // playerId that fired it (for threat attribution)
    bool hostile = false;  // fired by an enemy → hits players instead of enemies
};

}  // namespace game
