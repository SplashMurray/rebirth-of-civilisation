// Rebirth of Civilisation - core types
// SPDX-License-Identifier: MIT
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <algorithm>

struct Color {
    uint8_t r = 0, g = 0, b = 0;
};

inline Color rgb(int r, int g, int b) {
    Color c;
    c.r = (uint8_t)std::clamp(r, 0, 255);
    c.g = (uint8_t)std::clamp(g, 0, 255);
    c.b = (uint8_t)std::clamp(b, 0, 255);
    return c;
}
inline Color blend(Color a, Color b, float t) {
    return rgb((int)(a.r + (b.r - a.r) * t), (int)(a.g + (b.g - a.g) * t), (int)(a.b + (b.b - a.b) * t));
}
inline Color darken(Color c, float f) { return rgb((int)(c.r * f), (int)(c.g * f), (int)(c.b * f)); }
inline Color lighten(Color c, float f) { return blend(c, rgb(255, 255, 255), f); }

struct Yields {
    int food = 0, prod = 0, sci = 0, cult = 0, gold = 0, faith = 0;
    Yields& operator+=(const Yields& o) {
        food += o.food; prod += o.prod; sci += o.sci; cult += o.cult; gold += o.gold; faith += o.faith;
        return *this;
    }
};

// percentage helper: v * (100 + pct) / 100, never below 0
inline int applyPct(int v, int pct) {
    int r = v * (100 + pct) / 100;
    return r < 0 ? 0 : r;
}

// Simple deterministic RNG (xorshift) so saves/replays are stable.
struct Rng {
    uint64_t s = 88172645463325252ull;
    void seed(uint64_t v) { s = v ? v : 88172645463325252ull; next(); next(); }
    uint64_t next() {
        s ^= s << 13; s ^= s >> 7; s ^= s << 17;
        return s;
    }
    int range(int n) { return n <= 0 ? 0 : (int)(next() % (uint64_t)n); }   // [0,n)
    int between(int a, int b) { return a + range(b - a + 1); }              // [a,b]
    double unit() { return (next() >> 11) * (1.0 / 9007199254740992.0); }  // [0,1)
    bool chance(double p) { return unit() < p; }
};
