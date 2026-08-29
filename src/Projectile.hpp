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
    int pierce = 0;      // Pierce boon: enemies this bolt can pass through before dying
    int lastHit = -1;    // id of the last enemy hit, so a piercing bolt doesn't re-hit it

    // Optional on-hit status rider carried by ability projectiles (burn/slow/stun/
    // knock). statusType is AbilityStatus's integer value; 0 = none. Applied to
    // whatever the bolt strikes. Purely host-side, so it isn't networked.
    int statusType = 0;
    float statusDur = 0.0f;
    int statusPower = 0;

    // Visual element, purely cosmetic (see World::SpellElement): tints the bolt's
    // glow/core so a fireball reads orange, a frost dart cyan, an arcane missile
    // violet — instead of every projectile sharing one yellow sprite. Not networked.
    int element = 0;
};

}  // namespace game
