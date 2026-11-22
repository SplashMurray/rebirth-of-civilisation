// Rebirth of Civilisation - game state and rules engine
// SPDX-License-Identifier: MIT
#pragma once
#include "Data.h"
#include <string>
#include <vector>

static const int CIV_HUMANS = 0;   // humanity: alive until the Fall, never playable
static const int CIV_FERAL = 1;    // feral packs and rogue machines (barbarians)
static const int FIRST_PRIMATE_CIV = 2;
static const int PROLOGUE_TURNS = 6;  // the war happens at the end of turn 6 (2034)
static const int MAX_STRANDS = 2;      // evolutionary strands a species may commit to

struct Tile {
    uint8_t terrain = T_OCEAN;
    uint8_t veg = V_NONE;
    bool hills = false;
    bool mountain = false;
    uint8_t res = R_NONE;
    uint8_t imp = IMP_NONE;
    int8_t owner = -1;      // civ that owns the tile
    int16_t city = -1;      // city that owns the tile
    uint8_t ruin = RUIN_NONE;
    uint8_t ruinDone = 0;   // bitmask of excavated tiers
    int16_t site = -1;      // index into Game::sites
    uint8_t fallout = 0;    // 0..3
};

struct HumanSite {
    std::string name;
    int x = 0, y = 0;
    int type = RUIN_CITY;
    int population = 0;  // thousands (flavour)
};

enum Order : uint8_t { ORD_NONE, ORD_FORTIFY, ORD_SLEEP, ORD_GOTO, ORD_BUILD, ORD_EXCAVATE, ORD_DECON, ORD_SKIP };

struct Unit {
    int id = 0;
    int type = 0;
    int owner = 0;
    int x = 0, y = 0;
    int hp = 100;
    int moves = 0;
    uint8_t order = ORD_NONE;
    int gx = -1, gy = -1;
    int work = 0;       // turns left on build/excavate/decon
    int buildImp = 0;
    int fortTurns = 0;
    bool acted = false;
    bool attacked = false;
    bool alive = true;
};

enum BuildKind { BK_NONE = 0, BK_UNIT = 1, BK_BUILDING = 2 };

struct City {
    int id = 0;
    std::string name;
    int owner = 0, origOwner = 0;
    bool capital = false, origCapital = false;
    int x = 0, y = 0;
    int pop = 1;
    int food = 0;
    int prod = 0;
    int buildKind = BK_NONE, buildId = -1;
    std::vector<uint8_t> has;   // per building
    int borderCulture = 0, claims = 0;
    int hp = 200;
    bool alive = true;
    int founded = 0;
    // derived each turn
    Yields yields;
    int foodSurplus = 0;
    std::vector<int> worked;
    int droughtTurns = 0;   // drought: -33% food while > 0
};

// evolutionary pressure that lingers for a few turns after a disaster
struct TempPressure {
    std::string name;
    int cat = 0;
    int value = 0;
    int turns = 0;
};

struct Civ {
    int id = 0;
    int primate = -1;
    bool player = false;
    bool alive = true;
    bool humans = false;
    bool feral = false;
    std::string name, leader, adjective;
    Color color;
    std::vector<uint8_t> techs;
    std::vector<int> techProgress;
    int research = -1, goal = -1;
    int scienceBank = 0;
    int sciencePerTurn = 0, culturePerTurn = 0, goldPerTurn = 0;
    int culture = 0, cultureTotal = 0, gold = 0;
    std::vector<uint8_t> policies;
    int policiesAdopted = 0;
    std::vector<uint8_t> explored, visible;
    std::vector<uint8_t> war, met;
    std::vector<int> warSince;
    int capital = -1;
    int ruinsExcavated = 0;
    int nameIdx = 0;
    int yieldBonusPct = 0;  // difficulty handicap for AI
    // evolution
    std::vector<uint8_t> traits;            // per trait: gained?
    int evoGoal = -1;                       // trait being evolved towards
    std::vector<int> evoPool, evoRate;      // per pressure category
    int battles = 0;                        // fights this turn (conflict pressure)
    TraitFx fx;                             // summed trait effects (derived, not saved)
    std::vector<TempPressure> tempPressures;  // lingering pressure from disasters
    int lastDisaster = -100;
};

struct Notification {
    int turn = 0;
    std::string text;
    int x = -1, y = -1;
};

enum PopupKind { POP_INFO, POP_NUKE, POP_INTRO, POP_TECH, POP_RUIN, POP_WAR, POP_VICTORY, POP_DEFEAT, POP_WONDER, POP_EVOLUTION, POP_DISASTER };
struct Popup {
    int kind = POP_INFO;
    std::string title, body;
    int x = -1, y = -1;
};

struct GameSettings {
    int primate = 0;
    int mapSize = 1;     // 0 small, 1 standard, 2 large
    int difficulty = 1;  // 0 easy, 1 normal, 2 hard
    uint64_t seed = 1;
    bool allAI = false;  // simulation / test mode
    int speed = 1;       // 0 quick, 1 standard, 2 epic
};

struct CombatResult {
    bool happened = false;
    int dmgToDef = 0, dmgToAtt = 0;
    bool defenderDied = false, attackerDied = false, cityCaptured = false;
};

struct Pt { int x, y; };

struct Pressure {
    std::string name;
    int value;
    int cat;  // PressureCat
    std::string why;
};

class Game {
public:
    // ---- state
    int W = 0, H = 0;
    std::vector<Tile> tiles;
    std::vector<Civ> civs;
    std::vector<Unit> units;
    std::vector<City> cities;
    std::vector<HumanSite> sites;
    std::vector<int> wonderCity;   // per building index, city id or -1
    int turn = 1;
    int player = -1;
    Rng rng;
    GameSettings settings;
    bool warHappened = false;
    bool gameOver = false;
    int winner = -1;
    std::string victoryType;
    std::vector<Notification> log;
    std::vector<Popup> popups;
    bool nukeAnimPending = false;
    // strategic resources owned by each civ (cached; rebuilt when territory or techs change)
    mutable std::vector<std::vector<int>> resCache;
    mutable bool resDirty = true;
    void markResourcesDirty() { resDirty = true; }
    int resourceCount(int c, int r) const;
    bool hasResource(int c, int r) const;   // Scrap Metal also counts as Iron
    bool hasNeeds(int c, const std::vector<int>& needs) const;
    bool unitBelongsTo(int c, int u) const;
    // game speed: everything that costs points or lasts a number of turns is scaled by speedPct
    int speedPct = 150;   // length of time: durations, calendar, fallout decay
    int costPct = 180;    // cost of everything: science, production, growth, culture, evolution
    int maxTurns = 500;
    int sc(int v) const { return (int)((long long)v * costPct / 100); }
    int dur(int turns) const { return turns * speedPct / 100; }
    int techCost(int t) const { return sc(g_techs[t].cost); }
    int traitCost(int t, int k) const { return sc(g_traits[t].cost[k]); }
    int statWars = 0, statCaptures = 0, statRazes = 0;  // diagnostics only (not saved)

    // ---- setup (MapGen.cpp)
    void newGame(const GameSettings& s);
    void generateMap();
    void placeHumanity();
    std::vector<Pt> chooseStarts(int count);

    // ---- map helpers
    bool inMap(int x, int y) const { return x >= 0 && y >= 0 && x < W && y < H; }
    int idx(int x, int y) const { return y * W + x; }
    Tile& at(int x, int y) { return tiles[idx(x, y)]; }
    const Tile& at(int x, int y) const { return tiles[idx(x, y)]; }
    int neighbors(int x, int y, Pt out[6]) const;
    bool neighborDir(int x, int y, int dir, Pt& out) const;  // dir 0..5 = E, NE, NW, W, SW, SE
    int dist(int x1, int y1, int x2, int y2) const;
    std::vector<Pt> ring(int x, int y, int radius) const;  // all tiles within radius
    bool isWater(int x, int y) const { return g_terrain[at(x, y).terrain].water; }
    bool isLand(int x, int y) const { return !isWater(x, y); }

    // ---- civ queries
    Civ& civ(int i) { return civs[i]; }
    bool hasTech(int c, int t) const { return t < 0 || civs[c].techs[t]; }
    bool hasGold(int c) const { return hasTech(c, ID.tCurrency); }
    bool hasPolicy(int c, int p) const { return civs[c].policies[p] != 0; }
    int policyPct(int c, PolicyEffect e) const;
    bool ownsWonderSpecial(int c, BSpecial s) const;
    bool atWar(int a, int b) const;
    bool isPrimateCiv(int c) const { return c >= FIRST_PRIMATE_CIV; }
    const PrimateDef* primate(int c) const { return civs[c].primate >= 0 ? &g_primates[civs[c].primate] : nullptr; }
    int era(int c) const;
    int score(int c) const;
    int numCities(int c) const;
    int militaryPower(int c) const;
    std::string dateString(int t) const;
    int yearAD(int t) const;

    // ---- techs (Game.cpp)
    bool canResearch(int c, int t) const;
    void setResearch(int c, int t);
    void setGoal(int c, int t);
    int nextTowardGoal(int c, int goal) const;
    void grantTech(int c, int t, bool announce = true);
    void addScience(int c, int amount);
    int researchTurns(int c) const;

    // ---- evolution
    bool hasTrait(int c, int t) const { return t >= 0 && t < (int)civs[c].traits.size() && civs[c].traits[t]; }
    int traitCount(int c) const;
    bool capstoneTaken(int c) const;
    int strandsStarted(int c) const;
    bool strandStarted(int c, int strand) const;
    bool canEvolve(int c, int t) const;          // prerequisites met (cost aside)
    bool affordable(int c, int t) const;
    int evolveTurns(int c, int t) const;         // -1 = this species lacks a needed pressure
    std::string traitBlocker(int c, int t) const;
    void setEvoGoal(int c, int t);
    void gainTrait(int c, int t);
    void refreshTraitFx(int c);
    int aiChooseTrait(int c);
    int bestFitStrand(int c) const;
    std::vector<Pressure> evolutionPressures(int c) const;
    std::vector<int> evolutionRates(int c) const;
    void processEvolution(int c);

    // ---- traditions
    int policyCost(int c) const;
    bool canAdopt(int c, int p) const;
    void adoptPolicy(int c, int p);

    // ---- tiles & cities
    Yields tileYields(int c, int x, int y) const;
    bool resourceVisible(int c, int x, int y) const;
    void computeCity(City& city);
    int growthThreshold(int pop) const;
    int cityStrength(const City& city) const;
    int cityMaxHP(const City& city) const;
    bool canFoundAt(int c, int x, int y, std::string* why = nullptr) const;
    City* cityAt(int x, int y);
    const City* cityAt(int x, int y) const;
    int foundCity(int c, int x, int y);
    void claimTile(City& city, int x, int y);
    bool canBuild(const City& city, int kind, int id) const;
    int buildCost(int c, int kind, int id) const;
    int buyCost(const City& city) const;
    bool buy(City& city);
    void setProduction(City& city, int kind, int id);
    std::vector<int> availableUnits(int c) const;
    std::vector<int> availableBuildings(const City& city) const;
    int uniqueFor(int c, int baseUnit) const;
    bool unitObsolete(int c, int u) const;
    int turnsToBuild(const City& city) const;
    int turnsToGrow(const City& city) const;
    int ruinTilesInTerritory(const City& city) const;
    void captureCity(City& city, int newOwner);

    // ---- units
    int spawnUnit(int c, int type, int x, int y);
    void killUnit(Unit& u);
    Unit* militaryAt(int x, int y);
    Unit* civilianAt(int x, int y);
    std::vector<Unit*> unitsAt(int x, int y);
    int maxMoves(const Unit& u) const;
    int moveCost(const Unit& u, int fx, int fy, int tx, int ty) const;  // -1 impassable
    bool canEnter(const Unit& u, int x, int y) const;
    bool canEmbark(int c, int x, int y) const;
    std::vector<Pt> findPath(const Unit& u, int tx, int ty, bool ignoreUnits = false) const;
    bool stepUnit(Unit& u, int nx, int ny);
    void continueGoto(Unit& u);
    bool orderMoveOrAttack(Unit& u, int tx, int ty);
    bool canAttack(const Unit& u, int tx, int ty) const;
    bool isEnemyTarget(int c, int tx, int ty) const;
    CombatResult attack(Unit& u, int tx, int ty);
    int unitStrength(const Unit& u, bool attacking, bool rangedAtk, int x, int y) const;
    int combatDamage(double a, double d);
    int sightRange(const Unit& u) const;
    std::vector<int> buildOptions(const Unit& u) const;
    int excavateTier(const Unit& u) const;  // -1 none
    int excavateTierFor(int c, int x, int y) const;
    bool canDecon(const Unit& u) const;
    void startBuild(Unit& u, int imp);
    void startExcavate(Unit& u);
    void startDecon(Unit& u);
    int workTurns(const Unit& u, int base) const;
    bool unitNeedsOrders(const Unit& u) const;
    void excavateReward(int c, int x, int y, int tier);

    // ---- turn flow
    void beginTurn(int c);
    void endTurn();          // called when the player (or sim) ends the turn
    void processCiv(int c);
    void processCity(City& city);
    void processUnitsEndOfTurn(int c);
    void updateVisibility(int c);
    void updateAllVisibility();
    void decayFallout();
    void triggerNuclearWar();
    void humanNews();
    void cityBombard(City& city);
    void checkElimination();
    void checkVictory();
    void declareWar(int a, int b);
    void makePeace(int a, int b);
    bool proposePeace(int from, int to);   // returns accepted
    void meet(int a, int b);

    // ---- disasters (Disasters.cpp)
    void disasterTurn(int c);
    bool triggerDisaster(int c, int type);   // returns true if it happened
    int statDisasters = 0;
    int statDisasterKind[8] = {};

    // ---- ferals
    void feralTurn();

    // ---- AI (AI.cpp)
    void aiTurn(int c);
    void aiCity(City& city);
    void aiUnit(Unit& u);
    void aiResearch(int c);
    void aiDiplomacy(int c);
    int aiSiteScore(int c, int x, int y) const;

    // ---- messages
    void notify(int c, const std::string& text, int x = -1, int y = -1);
    void popup(int c, int kind, const std::string& title, const std::string& body, int x = -1, int y = -1);

    // ---- save/load (Save.cpp)
    bool save(const std::string& path) const;
    bool load(const std::string& path);
};

const char* orderName(int o);
