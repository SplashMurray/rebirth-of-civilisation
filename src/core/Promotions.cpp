// Rebirth of Civilisation - unit experience and promotions
// Military units earn experience (XP) in battle: attacking, surviving an attack and winning. Each level
// of experience lets the unit take a promotion of its owner's choosing - stronger in attack or defence,
// at home in the forest or the hills, a city raider, a medic, swifter, keener-eyed or at home at sea.
// Fights with the wild beasts teach only so much. A Training Ground gives new units a head start.
// SPDX-License-Identifier: MIT
#include "Game.h"
#include <algorithm>

namespace {
constexpr int bit(UnitClass c) { return 1 << (int)c; }
const int LAND = bit(UnitClass::Melee) | bit(UnitClass::Ranged) | bit(UnitClass::Mounted) | bit(UnitClass::Recon);
const int ALLMIL = LAND | bit(UnitClass::Siege) | bit(UnitClass::Armor) | bit(UnitClass::Naval);
const int FERAL_XP_CAP = 30;  // beasts teach no more than this
}  // namespace

std::vector<PromotionDef> g_promotions = {
    {"Shock I", "+15% strength when attacking", -1, ALLMIL},
    {"Shock II", "+15% more strength when attacking", PR_SHOCK1, ALLMIL},
    {"Guard I", "+15% strength when defending", -1, ALLMIL & ~bit(UnitClass::Siege)},
    {"Guard II", "+15% more strength when defending", PR_GUARD1, ALLMIL & ~bit(UnitClass::Siege)},
    {"Woodsman", "+25% strength when fighting from forest or jungle", -1, LAND},
    {"Highlander", "+25% strength when fighting from hills", -1, LAND},
    {"City Raider", "+25% strength when attacking cities", PR_SHOCK1,
     bit(UnitClass::Melee) | bit(UnitClass::Mounted) | bit(UnitClass::Armor) | bit(UnitClass::Siege) | bit(UnitClass::Ranged)},
    {"Medic", "This unit and your units next to it heal 10 HP more each turn", PR_GUARD1, bit(UnitClass::Melee) | bit(UnitClass::Recon) | bit(UnitClass::Ranged)},
    {"Swift", "+1 movement", -2, bit(UnitClass::Mounted) | bit(UnitClass::Recon) | bit(UnitClass::Armor) | bit(UnitClass::Naval)},
    {"Keen Eyes", "+1 sight", -1, bit(UnitClass::Recon) | bit(UnitClass::Naval) | bit(UnitClass::Ranged) | bit(UnitClass::Melee)},
    {"Sea Legs", "+20% strength at sea", -1, bit(UnitClass::Naval)},
};

int Game::unitLevel(const Unit& u) {
    int n = 0;
    for (int p = 0; p < PR_COUNT; p++) if (u.promos & (1u << p)) n++;
    return n;
}

int Game::xpForLevel(int level) { return 10 * level * (level + 1) / 2; }  // 10, 30, 60, 100, 150...

bool Game::promotionAllowed(const Unit& u, int p) const {
    if (p < 0 || p >= PR_COUNT || hasPromo(u, p)) return false;
    const UnitDef& d = g_units[u.type];
    if (!d.military() || !(g_promotions[p].classMask & bit(d.cls))) return false;
    int req = g_promotions[p].req;
    if (req == -2) return unitLevel(u) >= 1;
    return req < 0 || hasPromo(u, req);
}

bool Game::canPromote(const Unit& u) const {
    if (!u.alive || !g_units[u.type].military() || u.xp < xpForLevel(unitLevel(u) + 1)) return false;
    for (int p = 0; p < PR_COUNT; p++) if (promotionAllowed(u, p)) return true;
    return false;
}

bool Game::promote(Unit& u, int p) {
    if (!canPromote(u) || !promotionAllowed(u, p)) return false;
    u.promos |= 1u << p;
    u.hp = std::min(100, u.hp + 25);  // a moment to catch breath
    if (u.owner >= FIRST_PRIMATE_CIV) civs[u.owner].promotions++;
    return true;
}

void Game::gainXP(Unit& u, int amount, bool vsFeral) {
    if (!u.alive || amount <= 0 || !g_units[u.type].military() || u.owner < FIRST_PRIMATE_CIV) return;
    if (vsFeral) {
        if (u.xp >= FERAL_XP_CAP) return;
        u.xp = std::min(FERAL_XP_CAP, u.xp + std::max(1, amount / 2));
        return;
    }
    if (const PrimateDef* pd = primate(u.owner)) if (pd->combatPct > 0) amount += 1;  // warlike peoples learn faster
    u.xp += amount;
}

int Game::promoStrengthPct(const Unit& u, bool attacking, int x, int y) const {
    if (!u.promos) return 0;
    int pct = 0;
    if (attacking) pct += 15 * (hasPromo(u, PR_SHOCK1) + hasPromo(u, PR_SHOCK2));
    else pct += 15 * (hasPromo(u, PR_GUARD1) + hasPromo(u, PR_GUARD2));
    if (inMap(x, y)) {
        const Tile& t = at(x, y);
        if (hasPromo(u, PR_WOODSMAN) && t.veg != V_NONE && !isWater(x, y)) pct += 25;
        if (hasPromo(u, PR_HIGHLANDER) && t.hills) pct += 25;
        if (hasPromo(u, PR_SEALEGS) && isWater(x, y)) pct += 20;
    }
    return pct;
}

double Game::cityAttackMult(const Unit& u) const { return hasPromo(u, PR_RAIDER) ? 1.25 : 1.0; }

// The AI promotes as soon as it can, by what suits the unit's work.
void Game::aiPromote(int c) {
    for (auto& u : units) {
        if (!u.alive || u.owner != c) continue;
        for (int guard = 0; guard < 6 && canPromote(u); guard++) {
            const UnitDef& d = g_units[u.type];
            const Tile& t = at(u.x, u.y);
            bool home = cityAt(u.x, u.y) != nullptr || u.order == ORD_FORTIFY;
            int best = -1;
            double bv = -1;
            for (int p = 0; p < PR_COUNT; p++) {
                if (!promotionAllowed(u, p)) continue;
                double v = 1;
                switch (p) {
                    case PR_SHOCK1: case PR_SHOCK2: v = home ? 3 : 6; break;
                    case PR_GUARD1: case PR_GUARD2: v = home ? 7 : 4; break;
                    case PR_WOODSMAN: v = t.veg != V_NONE ? 5 : 2.5; break;
                    case PR_HIGHLANDER: v = t.hills ? 5 : 2; break;
                    case PR_RAIDER: v = d.cls == UnitClass::Siege ? 9 : 5.5; break;
                    case PR_MEDIC: v = 3.5; break;
                    case PR_SWIFT: v = d.cls == UnitClass::Mounted || d.cls == UnitClass::Armor ? 5.5 : 4; break;
                    case PR_SIGHT: v = d.cls == UnitClass::Recon ? 6 : 1.5; break;
                    case PR_SEALEGS: v = 7; break;
                }
                v *= 0.85 + 0.3 * rng.unit();
                if (v > bv) { bv = v; best = p; }
            }
            if (best < 0 || !promote(u, best)) break;
        }
    }
}
