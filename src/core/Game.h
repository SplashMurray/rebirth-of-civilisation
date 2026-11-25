// Rebirth of Civilisation - game state and rules engine
// SPDX-License-Identifier: MIT
#pragma once
#include "Data.h"
#include <algorithm>
#include <string>
#include <vector>

static const int CIV_HUMANS = 0;   // humanity: alive until the Fall, never playable
static const int CIV_FERAL = 1;    // feral packs and rogue machines (barbarians)
static const int FIRST_PRIMATE_CIV = 2;
static const int PROLOGUE_TURNS = 6;
static const int CULTURE_WIN_POLICIES = 16;  // traditions adopted for a Cultural victory (there are more to choose from)  // the war happens at the end of turn 6 (2034)
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
    uint8_t foraged = 0;    // 0..3: how picked-over the land is by roaming bands
    uint8_t road = 0;       // RoadLevel
    uint8_t river = 0;      // bitmask of the directions (0..5) a river joins this tile to its neighbours
    int8_t riverOut = -1;   // the direction the river flows on towards the sea (-1: none)
    uint8_t riverSize = 0;  // RiverSize
    uint8_t feature = F_NONE;
    int8_t natural = -1;    // natural wonder (index into g_naturals) when feature == F_WONDER
};

// A natural wonder on the map and which species have found it.
struct NaturalSite {
    int type = 0, x = 0, y = 0;
    std::vector<uint8_t> found;  // per civ
};

// A caravan's trade route: from one of the owner's cities to another city, for a set number of turns.
struct TradeRoute {
    int owner = -1, from = -1, to = -1;  // civ, origin city, destination city
    int start = 0, until = 0;            // turns
    bool sea = false;
    std::vector<int> path;               // tile indices from origin to destination
};

// One possible destination for a caravan, with what the route would earn.
struct RouteOption {
    int city = -1;
    bool sea = false;
    bool ok = false;
    std::string why;      // why not, when !ok
    int length = 0;       // tiles
    int roadPct = 0;      // bonus from roads (or the sea)
    Yields origin, dest;  // per turn, before city modifiers
    std::vector<int> path;
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
    int carrier = -1;   // id of the ship carrying this unit, or -1
    int ferryX = -1, ferryY = -1;  // AI: where a ferried unit (or its ship) is heading
    int band = 0;       // Roaming Band: members
    int bandFood = 0;   // Roaming Band: food store towards the next member
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
    int happy = 0, unhappy = 0;  // derived: citizens' mood (see Happiness.cpp)
    int8_t moodWas = 0;     // mood last turn, to report changes
};

// City moods, from worst to best.
enum CityMood { MOOD_UNREST = -2, MOOD_UNHAPPY = -1, MOOD_CONTENT = 0, MOOD_CELEBRATING = 1 };
static const int CONTENT_BASE = 5;  // citizens content with nothing at all

// Where a city's happiness and unhappiness come from.
struct MoodParts {
    int luxuries = 0, buildings = 0, wonders = 0, naturals = 0, traditions = 0;  // happiness
    int crowding = 0, empire = 0, war = 0;                                      // unhappiness
    int happy() const { return luxuries + buildings + wonders + naturals + traditions; }
    int unhappy() const { return crowding + empire + war; }
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
    int yieldBonusPct = 0;  // unused: difficulty no longer gives the AI free yields
    int aiSkill = 1;        // how well this civ is played by the AI: 0 Easy, 1 Normal, 2 Hard, 3 Expert
    // evolution
    std::vector<uint8_t> traits;            // per trait: gained?
    int evoGoal = -1;                       // trait being evolved towards
    std::vector<int> evoPool, evoRate;      // per pressure category
    int battles = 0;                        // fights this turn (conflict pressure)
    TraitFx fx;                             // summed trait effects (derived, not saved)
    std::vector<TempPressure> tempPressures;  // lingering pressure from disasters
    int warheads = 0;       // nuclear warheads in the silos
    // lifetime counters (achievements, statistics)
    int nukesLaunched = 0, timesNuked = 0, citiesCaptured = 0, citiesLost = 0, bandSplits = 0;
    int disastersSuffered = 0, warsDeclared = 0, falloutCleaned = 0, maxBand = 0, shipsSunk = 0;
    int unitsUpgraded = 0, unitsFerried = 0;
    int routesMade = 0, routesPlundered = 0, bestRoad = 0;
    int naturalsFound = 0, riverSailed = 0;
    int weariness = 0;       // war weariness: grows with war and losses, fades in peace
    int mostCelebrating = 0, mostLuxuries = 0;  // natural wonders seen, river tiles sailed
    int lastDisaster = -100;
    // diplomacy: memories of how others treated us, and treaties
    std::vector<int> grievance;       // wrongs done to us by each civ (decays)
    std::vector<int> goodwill;        // gifts and fair dealing (decays)
    std::vector<int> peaceUntil;      // no war with them before this turn
    std::vector<int> denouncedUntil;  // we have denounced them until this turn
    std::vector<int> lastOffer;       // AI: last turn we proposed something to them
    std::vector<int> embargo;         // 1: we refuse all trade with them
    int dealsMade = 0, backstabs = 0;
    // minor tribes: the tribe's own state...
    bool minor = false;
    int tribe = -1;                   // index into g_tribes
    std::vector<int> influence;       // each major civ's standing with this tribe
    int ally = -1;                    // the major civ it is allied with
    int questKind = 0, questTarget = -1, questUntil = 0;
    std::vector<uint8_t> questDone;   // who has completed the current quest
    std::vector<int> lastUnitGift;    // militant tribes: last turn each major was sent troops
    int firstMet = -1;                // the first major to meet them
    // ...and a major civ's dealings with tribes
    int tribeFoodCapital = 0, tribeFoodAll = 0;  // maritime tribes' gifts (derived each turn)
    int questsCompleted = 0, mostAllies = 0;
};

// Influence thresholds with a minor tribe.
static const int TRIBE_FRIEND = 30, TRIBE_ALLY = 60;
enum TribeStatus { TS_WAR = -1, TS_NEUTRAL = 0, TS_FRIEND = 1, TS_ALLY = 2 };
enum TribeQuest { TQ_NONE, TQ_ROUTE, TQ_FERALS };

// ---- diplomacy
enum AgreementKind { AG_OPEN_BORDERS, AG_RESEARCH, AG_DEFENSIVE, AG_FRIENDSHIP, AG_RESOURCE, AG_GOLD_PER_TURN, AG_COUNT };
struct Agreement {
    int kind = 0;
    int a = -1, b = -1;  // mutual kinds: the two parties; open borders, resource, gold: a gives to b
    int until = 0;       // the turn it ends
    int value = 0;       // resource id, or gold per turn
};
enum DealItemKind { DI_GOLD, DI_GPT, DI_RESOURCE, DI_OPEN_BORDERS, DI_RESEARCH, DI_DEFENSIVE, DI_FRIENDSHIP, DI_PEACE, DI_WAR_ON, DI_COUNT };
struct DealItem {
    int kind = 0;
    int from = -1;   // the side giving it; -1 for mutual agreements
    int value = 0;   // gold, gold per turn, resource id, or the civ to declare war on
};
struct Deal {
    int a = -1, b = -1;  // a proposes to b
    std::vector<DealItem> items;
    bool demand = false; // a tribute demand: refusing it angers them
};

struct Notification {
    int turn = 0;
    std::string text;
    int x = -1, y = -1;
};

enum PopupKind { POP_INFO, POP_NUKE, POP_INTRO, POP_TECH, POP_RUIN, POP_WAR, POP_VICTORY, POP_DEFEAT, POP_WONDER, POP_EVOLUTION, POP_DISASTER, POP_SETTLE, POP_DEAL, POP_NATURAL };
struct Popup {
    int kind = POP_INFO;
    std::string title, body;
    int x = -1, y = -1;
    int deal = -1;  // POP_DEAL: index into Game::offers
};

struct GameSettings {
    int primate = 0;
    int mapSize = 1;     // 0 small, 1 standard, 2 large
    int difficulty = 1;  // AI skill: 0 easy, 1 normal, 2 hard, 3 expert
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

// A record of one attack, kept briefly so the interface can animate it. Not saved.
struct CombatFx {
    uint64_t seq = 0;
    int attacker = -1, atkType = 0, atkOwner = -1;
    int ax = 0, ay = 0, tx = 0, ty = 0;
    int dmgDef = 0, dmgAtt = 0;
    bool ranged = false, defDied = false, attDied = false, city = false;
};

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
    mutable std::vector<std::vector<uint8_t>> allyRes;  // resources reaching each civ from allied tribes (cache)
    mutable int allyResTurn = -1;
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
    // Bigger maps give each species more room and bigger empires, so (as in Civilization) research,
    // traditions and the Rebirth Project cost more on them. Derived from the map size, not saved.
    int mapCostPct() const { static const int P[] = {100, 130, 170}; return P[std::clamp(settings.mapSize, 0, 2)]; }
    int researchCost(int v) const { return (int)((long long)sc(v) * mapCostPct() / 100); }
    int techCost(int t) const { return researchCost(g_techs[t].cost); }
    int traitCost(int t, int k) const { return sc(g_traits[t].cost[k]); }
    int statWars = 0, statCaptures = 0, statRazes = 0, statNukes = 0;  // diagnostics only (not saved)

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
    std::string dateString(int t) const;     // full date for the current turn (t only matters in the prologue)
    std::string shortDate() const;           // compact form for narrow windows
    long long yearAF = 0;                    // years since the Fall (advances by yearsPerTurn)
    int yearsPerTurn() const;                // depends on how far the most advanced species has come
    std::string calendarPhase() const;

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
    // ---- roaming bands (Bands.cpp): the nomadic age before the first settlement
    static const int BAND_MAX = 8;
    bool canSettle(int c) const;                  // Speech evolved and Cultivation discovered
    bool nomadic(int c) const { return numCities(c) == 0; }
    int numBands(int c) const;
    int bandMembers(int c) const;
    int forageFood(int c, int x, int y) const;    // food a band would gather from one tile now
    int bandForage(const Unit& u, std::vector<int>* tiles = nullptr) const;  // food gathered this turn
    int bandGrowth(int size) const;               // food needed for the next member
    int bandScience(const Unit& u) const;
    int bandCulture(const Unit& u) const;
    int settlePop(const Unit& u) const { return 1 + (u.band - 1) / 2; }
    int settleBand(Unit& u);                      // found a city with the band; city id or -1
    bool splitBand(Unit& u);
    bool raiseWarParty(Unit& u);
    bool sendScout(Unit& u);
    bool hurtBand(Unit& u, int loss, const std::string& why);  // true if the band was wiped out
    bool bandSpawnSpot(const Unit& u, bool military, Pt& out) const;
    void processBands(int c, int& sci, int& cult);
    void regrowForage();
    bool nomadDisaster(int c);
    void aiBand(Unit& u);
    // ---- nuclear weapons (Nukes.cpp)
    static const int NUKE_RANGE = 12;
    bool canNukeAt(int c, int x, int y, std::string* why = nullptr) const;
    bool launchNuke(int c, int x, int y);
    void aiNukes(int c);
    std::vector<Pt> blasts;  // detonations this turn, for the UI to show (not saved)
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
    // A tile -> units index, built for the length of a path search (units cannot move meanwhile), so
    // canEnter and transportAt need not scan every unit for every tile the search looks at.
    mutable std::vector<int> occHead, occNext;
    mutable int occDepth = 0;
    struct OccScope {
        const Game& g;
        explicit OccScope(const Game& gm);
        ~OccScope() { g.occDepth--; }
    };
    template <class F> void forUnitsAt(int x, int y, F f) const {
        if (occDepth > 0) {
            for (int i = occHead[idx(x, y)]; i >= 0; i = occNext[i]) f(units[i]);
        } else {
            for (auto& o : units) if (o.alive && o.x == x && o.y == y) f(o);
        }
    }
    bool canEmbark(int c, int x, int y) const;
    bool isCoastal(int x, int y) const;  // next to water
    void aiNaval(Unit& u);
    // ---- minor tribes (Tribes.cpp)
    bool isMinor(int c) const { return c >= FIRST_PRIMATE_CIV && c < (int)civs.size() && civs[c].minor; }
    bool isMajor(int c) const { return c >= FIRST_PRIMATE_CIV && c < (int)civs.size() && !civs[c].minor; }
    int numMajors() const;
    void placeTribes(const std::vector<Pt>& starts);
    int tribeStatus(int major, int tribe) const;
    int influenceFor(int major, int tribe) const;
    void addInfluence(int major, int tribe, int amount, const std::string& why = "");
    int giftInfluence(int major, int tribe, int gold) const;
    bool giftGold(int major, int tribe, int gold);
    bool allyResource(int c, int r) const;           // a tribe allied with c has it
    std::string tribeTitle(int tribe) const;          // "the Tarsiers of Bohol"
    std::string tribeBonusText(int tribe) const;
    std::string questText(int tribe) const;
    void tribesTurn();
    void aiMinorTurn(int c);
    void aiTribes(int c);
    void tribeDeclaredOn(int major, int tribe);
    void feralKilled(int killer, const Unit& feral);
    bool tribePeaceOk(int major, int tribe, std::string* why = nullptr) const;
    // ---- happiness and luxuries (Happiness.cpp)
    std::vector<int> luxuriesOf(int c) const;   // distinct luxuries you own or are lent
    int luxuryCount(int c) const { return (int)luxuriesOf(c).size(); }
    MoodParts moodParts(const City& city) const;
    int moodOf(const City& city) const;
    static const char* moodName(int mood);
    void wearinessTurn(int c);
    void addWeariness(int c, int amount);
    void reportMood(City& city);
    // ---- rivers and terrain features (Rivers.cpp)
    std::vector<NaturalSite> naturals;
    int dirTo(int fx, int fy, int tx, int ty) const;           // 0..5, or -1 if not neighbours
    bool hasRiver(int x, int y) const { return !isWater(x, y) && at(x, y).river != 0; }
    bool riverLink(int fx, int fy, int tx, int ty) const;      // a river runs straight from one tile into the other
    bool downstream(int fx, int fy, int tx, int ty) const;     // ... and flows that way
    bool isFalls(int x, int y) const;                          // boats cannot pass a waterfall
    bool waterStep(int fx, int fy, int tx, int ty, bool ocean, bool river) const;  // could a boat go this way?
    bool isPort(int x, int y) const;                           // on the coast, a lake or a navigable river
    bool freshWater(int x, int y) const;                       // on or beside a river, lake or oasis
    bool shipCanStand(const Unit& u, int x, int y) const;
    void generateRivers(const std::vector<double>& elev);
    void generateFeatures();
    void discoverNaturals(int c, int x, int y, int r);
    void volcanoTurn();
    // ---- diplomacy (Diplomacy.cpp)
    std::vector<Agreement> agreements;
    // ---- trade and roads (Trade.cpp)
    std::vector<TradeRoute> routes;
    int roadLevelAt(int x, int y) const;               // cities count as roads
    int bestRoad(int c) const;                         // the best road level civ c can build
    int roadOption(const Unit& u) const;               // next road level a Gatherer could build here, or -1
    int tradeCapacity(int c) const;                    // trade routes (and caravans) civ c may have
    int tradeUsed(int c) const;                        // active routes plus caravans in the field
    int routesOf(int c) const;
    std::vector<RouteOption> tradeOptions(const Unit& u) const;
    RouteOption routeOption(int c, int from, int to, bool sea) const;
    bool establishRoute(Unit& u, int toCity, bool sea);
    Yields routeYields(const TradeRoute& r, bool forOrigin) const;
    Yields cityTradeYields(const City& city) const;   // all routes starting or ending here
    bool routeValid(const TradeRoute& r, std::string* why = nullptr) const;
    void endRoute(size_t i, bool comeHome, const std::string& why);
    void tradeTurn();
    void cancelRoutesBetween(int a, int b, const std::string& why);
    void aiTrader(Unit& u);
    std::vector<int> tradePath(int c, int from, int to, bool sea) const;
    const std::vector<uint8_t>& aiRoadWant(int c) const;
    mutable std::vector<std::vector<uint8_t>> roadWant;  // AI: road plans per civ (cache, not saved)
    mutable std::vector<int> roadWantTurn;
    std::vector<Deal> offers;        // AI proposals waiting for the player's answer (not saved)
    int opinion(int c, int o, std::vector<std::pair<std::string, int>>* why = nullptr) const;  // c's view of o
    int attitude(int c, int o) const;  // -2 hostile, -1 unfriendly, 0 neutral, 1 friendly, 2 close
    static const char* attitudeName(int a);
    bool hasAgreement(int kind, int a, int b) const;
    const Agreement* findAgreement(int kind, int a, int b, int value = -1) const;
    int agreementTurnsLeft(int kind, int a, int b) const;
    void endAgreements(int a, int b);  // war breaks every treaty between them
    // borders: open to everyone until the owner discovers Tribal Law, then closed except by treaty
    bool bordersClosed(int owner) const { return owner >= FIRST_PRIMATE_CIV && hasTech(owner, ID.tLaw); }
    bool bordersOpenTo(int owner, int mover) const { return !bordersClosed(owner) || hasAgreement(AG_OPEN_BORDERS, owner, mover); }
    // embargoes: no trade of any kind between the two
    bool embargoed(int a, int b) const;
    bool canEmbargo(int a, int b, std::string* why = nullptr) const;
    void setEmbargo(int a, int b, bool on);
    bool canDeclareWar(int a, int b, std::string* why = nullptr) const;
    int diploSciPct(int c) const;
    bool lentResource(int c, int r) const;
    std::vector<int> tradeableResources(int giver, int receiver) const;
    bool dealItemValid(const Deal& d, const DealItem& it, std::string* why = nullptr) const;
    double dealValue(int c, const Deal& d, std::string* why = nullptr) const;  // how much civ c (AI) likes the deal
    bool aiAccepts(int c, const Deal& d, std::string* why = nullptr) const;
    bool executeDeal(const Deal& d);
    std::string dealItemText(const DealItem& it, int viewer) const;
    double peaceValue(int c, int o) const;
    void denounce(int a, int b);
    void expelUnits(int a, int b);  // a's military leaves b's lands
    void diplomacyTurn();
    void aiPacts(int c);
    void aiOffers(int c);
    void answerOffer(int idx, bool accept);
    int dealTurns() const { return dur(30); }
    // ---- upgrades and transports (Transport.cpp)
    bool canTrain(int c, int unitType) const;     // tech, resources and species allow it
    int upgradeTarget(const Unit& u) const;       // the best unit this one can become now, or -1
    int upgradeGold(const Unit& u, int to) const;
    int upgradeProd(const Unit& u, int to) const; // before Currency: production from the city it stands in
    bool canUpgrade(const Unit& u, std::string* why = nullptr) const;
    bool upgradeUnit(Unit& u);
    int cargoCount(const Unit& ship) const;
    int cargoSpace(const Unit& ship) const;
    const Unit* transportAt(int c, int x, int y, int exclude = -1) const;  // own ship with room
    bool boardShip(Unit& u);     // in a harbour city: go aboard the docked ship
    bool leaveShip(Unit& u);     // in a harbour city: step ashore
    void aiUpgrades(int c);
    bool aiFerry(Unit& u, int gx, int gy);  // AI: get a land unit to a goal across the water
    void aiTransportShip(Unit& u);          // AI: a ship with cargo space acting as a ferry
    bool aiFerryJob(const Unit& ship) const;
    int aiNeedsTransport(const City& port) const;  // unit type to build for waiting passengers, or -1
    int aiShipWant(int c, bool atWar) const;
    int aiBestShip(int c, bool wantRanged, bool riverOnly = false) const;
    std::vector<Pt> findPath(const Unit& u, int tx, int ty, bool ignoreUnits = false) const;
    bool stepUnit(Unit& u, int nx, int ny);
    void continueGoto(Unit& u);
    bool orderMoveOrAttack(Unit& u, int tx, int ty);
    bool canAttack(const Unit& u, int tx, int ty) const;
    bool isEnemyTarget(int c, int tx, int ty) const;
    CombatResult attack(Unit& u, int tx, int ty);
    CombatResult attackImpl(Unit& u, int tx, int ty);
    std::vector<CombatFx> combatFx;  // recent attacks, newest last (capped)
    uint64_t combatSeq = 0;
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
    int skillOf(int c) const { return c >= FIRST_PRIMATE_CIV ? civs[c].aiSkill : 1; }
    double aiTechValue(int c, int t) const;
    bool aiOnRebirthPath(int t) const;
    int aiChoosePolicy(int c);
    // Hard/Expert war planning (recomputed every turn, not saved)
    struct AIPlan { int target = -1; int rx = -1, ry = -1; bool assault = false; int army = 0, need = 0; };
    std::vector<AIPlan> aiPlans;
    void aiPlanWar(int c);
    bool aiArmyUnit(Unit& u);   // Hard/Expert: act as part of the army; true if the unit acted

    // ---- messages
    void notify(int c, const std::string& text, int x = -1, int y = -1);
    void popup(int c, int kind, const std::string& title, const std::string& body, int x = -1, int y = -1);

    // ---- save/load (Save.cpp)
    bool save(const std::string& path) const;
    bool load(const std::string& path);
};

const char* orderName(int o);
