// Rebirth of Civilisation - save games (simple versioned text format)
// SPDX-License-Identifier: MIT
#include "Game.h"
#include <fstream>
#include <sstream>

namespace {
const int SAVE_VERSION = 20;

struct Archive {
    bool writing;
    std::string buf;
    size_t pos = 0;
    bool ok = true;

    explicit Archive(bool w) : writing(w) {}

    long long readNum() {
        while (pos < buf.size() && (buf[pos] == ' ' || buf[pos] == '\n')) pos++;
        size_t start = pos;
        if (pos < buf.size() && buf[pos] == '-') pos++;
        while (pos < buf.size() && buf[pos] >= '0' && buf[pos] <= '9') pos++;
        if (start == pos) { ok = false; return 0; }
        return std::stoll(buf.substr(start, pos - start));
    }
    void num(long long& v) {
        if (writing) { buf += std::to_string(v); buf += ' '; }
        else v = readNum();
    }
    template <class T> void io(T& v) {
        long long x = (long long)v;
        num(x);
        if (!writing) v = (T)x;
    }
    void io(bool& v) {
        long long x = v ? 1 : 0;
        num(x);
        if (!writing) v = x != 0;
    }
    void io(uint64_t& v) {
        // store as two 32-bit halves to avoid sign issues
        long long hi = (long long)(v >> 32), lo = (long long)(v & 0xffffffffull);
        num(hi); num(lo);
        if (!writing) v = ((uint64_t)hi << 32) | (uint64_t)lo;
    }
    void io(std::string& s) {
        long long n = (long long)s.size();
        num(n);
        if (writing) { buf += s; buf += '\n'; }
        else {
            pos++;  // the single space written after the length
            if (n < 0 || pos + (size_t)n > buf.size()) { ok = false; return; }
            s = buf.substr(pos, (size_t)n);
            pos += (size_t)n;
        }
    }
    void io(Color& c) { io(c.r); io(c.g); io(c.b); }
    void io(TempPressure& t) { io(t.name); io(t.cat); io(t.value); io(t.turns); }
    void io(Agreement& ag) { io(ag.kind); io(ag.a); io(ag.b); io(ag.until); io(ag.value); }
    template <class T> void io(std::vector<T>& v) {
        long long n = (long long)v.size();
        num(n);
        if (!writing) {
            if (n < 0 || n > 10000000) { ok = false; return; }
            v.resize((size_t)n);
        }
        for (auto& e : v) io(e);
    }
    void io(std::vector<uint8_t>& v) {
        // compact run-length encoding for big flag arrays
        long long n = (long long)v.size();
        num(n);
        if (writing) {
            size_t i = 0;
            while (i < v.size()) {
                size_t j = i;
                while (j < v.size() && v[j] == v[i] && j - i < 1000000) j++;
                long long val = v[i], run = (long long)(j - i);
                num(val); num(run);
                i = j;
            }
        } else {
            if (n < 0 || n > 10000000) { ok = false; return; }
            v.assign((size_t)n, 0);
            size_t i = 0;
            while (i < v.size() && ok) {
                long long val = readNum(), run = readNum();
                if (run <= 0) { ok = false; break; }
                for (long long k = 0; k < run && i < v.size(); k++) v[i++] = (uint8_t)val;
            }
        }
    }
};

void ioYields(Archive& a, Yields& y) { a.io(y.food); a.io(y.prod); a.io(y.sci); a.io(y.cult); a.io(y.gold); }

void ioGame(Archive& a, Game& g) {
    a.io(g.W); a.io(g.H); a.io(g.turn); a.io(g.player);
    a.io(g.rng.s);
    a.io(g.settings.primate); a.io(g.settings.mapSize); a.io(g.settings.difficulty); a.io(g.settings.seed); a.io(g.settings.allAI); a.io(g.settings.speed); a.io(g.settings.tutorial); a.io(g.tutorialStep);
    a.io(g.speedPct); a.io(g.costPct); a.io(g.maxTurns); a.io(g.yearAF);
    a.io(g.warHappened); a.io(g.gameOver); a.io(g.winner); a.io(g.victoryType);
    // tiles
    long long nt = (long long)g.tiles.size();
    a.num(nt);
    if (!a.writing) g.tiles.assign((size_t)std::max(0LL, nt), Tile());
    for (auto& t : g.tiles) {
        a.io(t.terrain); a.io(t.veg); a.io(t.hills); a.io(t.mountain); a.io(t.res); a.io(t.imp);
        a.io(t.owner); a.io(t.city); a.io(t.ruin); a.io(t.ruinDone); a.io(t.site); a.io(t.fallout); a.io(t.foraged); a.io(t.road);
        a.io(t.river); a.io(t.riverOut); a.io(t.riverSize); a.io(t.feature); a.io(t.natural);
    }
    long long ns = (long long)g.sites.size();
    a.num(ns);
    if (!a.writing) g.sites.assign((size_t)std::max(0LL, ns), HumanSite());
    for (auto& s : g.sites) { a.io(s.name); a.io(s.x); a.io(s.y); a.io(s.type); a.io(s.population); }
    a.io(g.wonderCity);
    long long nn = (long long)g.naturals.size();
    a.num(nn);
    if (!a.writing) g.naturals.assign((size_t)std::max(0LL, nn), NaturalSite());
    for (auto& n : g.naturals) { a.io(n.type); a.io(n.x); a.io(n.y); a.io(n.found); }
    long long nc = (long long)g.civs.size();
    a.num(nc);
    if (!a.writing) g.civs.assign((size_t)std::max(0LL, nc), Civ());
    for (auto& c : g.civs) {
        a.io(c.id); a.io(c.primate); a.io(c.player); a.io(c.alive); a.io(c.humans); a.io(c.feral);
        a.io(c.name); a.io(c.leader); a.io(c.adjective); a.io(c.color);
        a.io(c.techs); a.io(c.techProgress); a.io(c.research); a.io(c.goal); a.io(c.scienceBank);
        a.io(c.sciencePerTurn); a.io(c.culturePerTurn); a.io(c.goldPerTurn);
        a.io(c.culture); a.io(c.cultureTotal); a.io(c.gold);
        a.io(c.policies); a.io(c.policiesAdopted);
        a.io(c.explored); a.io(c.visible); a.io(c.war); a.io(c.met); a.io(c.warSince);
        a.io(c.capital); a.io(c.ruinsExcavated); a.io(c.nameIdx); a.io(c.yieldBonusPct);
        a.io(c.traits); a.io(c.evoGoal); a.io(c.evoPool); a.io(c.evoRate); a.io(c.battles);
        a.io(c.tempPressures); a.io(c.lastDisaster); a.io(c.aiSkill);
        a.io(c.grievance); a.io(c.goodwill); a.io(c.peaceUntil); a.io(c.denouncedUntil); a.io(c.lastOffer); a.io(c.embargo); a.io(c.dealsMade); a.io(c.backstabs);
        a.io(c.warheads); a.io(c.nukesLaunched); a.io(c.timesNuked); a.io(c.citiesCaptured); a.io(c.citiesLost); a.io(c.bandSplits);
        a.io(c.disastersSuffered); a.io(c.warsDeclared); a.io(c.falloutCleaned); a.io(c.maxBand); a.io(c.shipsSunk); a.io(c.unitsUpgraded); a.io(c.unitsFerried);
        a.io(c.routesMade); a.io(c.routesPlundered); a.io(c.bestRoad); a.io(c.naturalsFound); a.io(c.riverSailed); a.io(c.weariness); a.io(c.mostCelebrating); a.io(c.mostLuxuries);
        a.io(c.minor); a.io(c.tribe); a.io(c.influence); a.io(c.ally); a.io(c.questKind); a.io(c.questTarget); a.io(c.questUntil);
        a.io(c.questDone); a.io(c.lastUnitGift); a.io(c.firstMet); a.io(c.tribeFoodCapital); a.io(c.tribeFoodAll); a.io(c.questsCompleted); a.io(c.mostAllies);
    }
    if (!a.writing) g.allyResTurn = -1;
    long long nu = (long long)g.units.size();
    a.num(nu);
    if (!a.writing) g.units.assign((size_t)std::max(0LL, nu), Unit());
    for (auto& u : g.units) {
        a.io(u.id); a.io(u.type); a.io(u.owner); a.io(u.x); a.io(u.y); a.io(u.hp); a.io(u.moves);
        a.io(u.order); a.io(u.gx); a.io(u.gy); a.io(u.work); a.io(u.buildImp); a.io(u.fortTurns);
        a.io(u.acted); a.io(u.attacked); a.io(u.alive); a.io(u.band); a.io(u.bandFood); a.io(u.carrier); a.io(u.ferryX); a.io(u.ferryY);
    }
    long long ncity = (long long)g.cities.size();
    a.num(ncity);
    if (!a.writing) g.cities.assign((size_t)std::max(0LL, ncity), City());
    for (auto& c : g.cities) {
        a.io(c.id); a.io(c.name); a.io(c.owner); a.io(c.origOwner); a.io(c.capital); a.io(c.origCapital);
        a.io(c.x); a.io(c.y); a.io(c.pop); a.io(c.food); a.io(c.prod); a.io(c.buildKind); a.io(c.buildId);
        a.io(c.has); a.io(c.borderCulture); a.io(c.claims); a.io(c.hp); a.io(c.alive); a.io(c.founded);
        ioYields(a, c.yields); a.io(c.foodSurplus); a.io(c.worked); a.io(c.droughtTurns); a.io(c.happy); a.io(c.unhappy); a.io(c.moodWas);
    }
    long long nl = (long long)g.log.size();
    a.num(nl);
    if (!a.writing) g.log.assign((size_t)std::max(0LL, nl), Notification());
    for (auto& n : g.log) { a.io(n.turn); a.io(n.text); a.io(n.x); a.io(n.y); }
    a.io(g.agreements);
    long long nr = (long long)g.routes.size();
    a.num(nr);
    if (!a.writing) g.routes.assign((size_t)std::max(0LL, nr), TradeRoute());
    for (auto& r : g.routes) { a.io(r.owner); a.io(r.from); a.io(r.to); a.io(r.start); a.io(r.until); a.io(r.sea); a.io(r.path); }
    if (!a.writing) g.offers.clear();
}
}  // namespace

bool Game::save(const std::string& path) const {
    Archive a(true);
    a.buf = "REBIRTH-OF-CIVILISATION ";
    long long v = SAVE_VERSION;
    a.num(v);
    ioGame(a, const_cast<Game&>(*this));
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f << a.buf;
    return (bool)f;
}

bool Game::load(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    std::stringstream ss;
    ss << f.rdbuf();
    Archive a(false);
    a.buf = ss.str();
    const std::string magic = "REBIRTH-OF-CIVILISATION ";
    if (a.buf.compare(0, magic.size(), magic) != 0) return false;
    a.pos = magic.size();
    long long v = 0;
    a.num(v);
    if (v != SAVE_VERSION) return false;
    initData();
    Game tmp;
    ioGame(a, tmp);
    if (!a.ok || tmp.W <= 0 || (int)tmp.tiles.size() != tmp.W * tmp.H) return false;
    *this = tmp;
    resDirty = true;
    for (size_t c = 0; c < civs.size(); c++) refreshTraitFx((int)c);
    popups.clear();
    nukeAnimPending = false;
    for (auto& c : cities) if (c.alive) computeCity(c);
    return true;
}
