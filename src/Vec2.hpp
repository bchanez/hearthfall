#pragma once

#include <cmath>

namespace game {

// Minimal 2D vector. Enough for movement, distances and normalization.
struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }

    Vec2& operator+=(const Vec2& o) {
        x += o.x;
        y += o.y;
        return *this;
    }

    Vec2& operator-=(const Vec2& o) {
        x -= o.x;
        y -= o.y;
        return *this;
    }

    float length() const { return std::sqrt(x * x + y * y); }

    // Returns a unit-length copy, or {0,0} if the vector is (near) zero.
    Vec2 normalized() const {
        const float len = length();
        if (len <= 1e-6f) return {0.0f, 0.0f};
        return {x / len, y / len};
    }
};

// Squared distance — cheaper than distance() when you only compare.
inline float distanceSquared(const Vec2& a, const Vec2& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return dx * dx + dy * dy;
}

}  // namespace game
