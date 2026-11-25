// Rebirth of Civilisation - static game data (rules tables)
// SPDX-License-Identifier: MIT
#pragma once
#include "Types.h"

// ---------------------------------------------------------------- terrain
enum Terrain : uint8_t { T_OCEAN, T_COAST, T_GRASS, T_PLAINS, T_DESERT, T_TUNDRA, T_SNOW, T_COUNT };
enum Vegetation : uint8_t { V_NONE, V_FOREST, V_JUNGLE };

// Terrain features: changes to a tile's land beyond its terrain, vegetation and relief.
enum Feature : uint8_t { F_NONE, F_FLOODPLAIN, F_MARSH, F_OASIS, F_VOLCANO, F_REEF, F_LAKE, F_WONDER, F_COUNT };
struct FeatureDef {
    std::string name;
    int food, prod, gold;
    std::string desc;
};
extern std::vector<FeatureDef> g_features;

// Natural wonders: one-of-a-kind places. Discovering one inspires a species; working it pays well.
enum WonderSite { WS_FALLS, WS_FLAT, WS_HILLS, WS_REEF, WS_SALT, WS_COLD, WS_PEAK, WS_MESA };
struct NaturalDef {
    std::string name;
    Yields y;
    int site;           // WonderSite: where it can appear
    std::string desc;
};
extern std::vector<NaturalDef> g_naturals;

// River sizes, from a stream to a great river, by how much land drains into them.
enum RiverSize : uint8_t { RIVER_NONE, RIVER_STREAM, RIVER_RIVER, RIVER_GREAT };

struct TerrainDef {
    std::string name;
    int food, prod, gold;
    Color color;
    bool water;
};

enum Resource : uint8_t {
    R_NONE, R_FRUIT, R_FISH, R_GAME, R_STONE, R_IRON, R_HORSES, R_OIL, R_SCRAP,
    R_COPPER, R_TIN, R_COAL, R_URANIUM, R_GOLD, R_WHEAT, R_RICE, R_MAIZE,
    R_HONEY, R_CACAO, R_SPICES, R_GRAPES, R_SALT, R_PEARLS, R_FURS, R_GEMS, R_COUNT
};
enum ResourceKind { RK_BONUS = 0, RK_STRATEGIC = 1, RK_LUXURY = 2, RK_CROP = 3 };
struct ResourceDef {
    std::string name, abbr;
    int food, prod, gold;
    std::string revealTech;
    Color color;
    int kind;           // ResourceKind
    std::string hint;   // which improvement to build on it
    int revealTechId = -1;
};
bool isCrop(int r);
bool isMineral(int r);

enum Improvement : uint8_t { IMP_NONE, IMP_FARM, IMP_MINE, IMP_LUMBER, IMP_TRADEPOST, IMP_PASTURE, IMP_COUNT };
struct ImprovementDef {
    std::string name;
    int food, prod, gold;
    std::string tech;
    int turns;
    int techId = -1;
};

// Roads, from a path worn by caravans to modern tarmac. Gatherers build and upgrade them one level at a time.
enum RoadLevel : uint8_t { ROAD_NONE, ROAD_TRACK, ROAD_DIRT, ROAD_COBBLE, ROAD_PAVED, ROAD_TARMAC, ROAD_LEVELS };
struct RoadDef {
    std::string name;
    std::string tech;  // needed to build this level ("" = anyone)
    int turns;         // Gatherer work to reach this level
    int cost;          // movement cost along it, in movement points (MP per move)
    int tradePct;      // trade route bonus per tile of this road
    int techId = -1;
};
extern std::vector<RoadDef> g_roads;
const int IMP_ROAD = 100;  // a Gatherer's build order for the next road level
const int MP = 12;         // movement points per move: roads cost fractions of a move

// Remnants of humanity. Before the Fall they are living human structures.
enum RuinType : uint8_t { RUIN_NONE, RUIN_CITY, RUIN_TOWN, RUIN_MILITARY, RUIN_LAB, RUIN_POWER, RUIN_COUNT };
struct RuinDef {
    std::string liveName;   // before the war
    std::string ruinName;   // after the war
    int tierMask;           // which excavation tiers (bit 0 = tier 1 ...)
    int blastRadius;
};
// Excavation tiers and the technology that unlocks them
static const int RUIN_TIERS = 4;
extern const char* RUIN_TIER_TECH[RUIN_TIERS];
extern const char* RUIN_TIER_NAME[RUIN_TIERS];
extern int g_ruinTierTech[RUIN_TIERS];

// ---------------------------------------------------------------- units
enum class UnitClass { Settler, Worker, Recon, Melee, Ranged, Mounted, Siege, Armor, Band, Nuke, Naval, Trader };

struct UnitDef {
    std::string name, abbr;
    int cost, str, rstr, range, moves;
    std::string tech;
    UnitClass cls;
    std::string replaces;   // unique-unit replacement
    int primate = -1;       // >= 0 for unique units
    std::string desc;
    int techId = -1;
    int replacesId = -1;
    std::vector<int> needs;  // strategic resources required to train it
    bool ocean = false;      // ships: can leave the coast for the open ocean
    int cargo = 0;           // ships: land units it can carry
    bool river = false;      // ships: shallow enough to sail up rivers
    int upgradeId = -1;      // the next unit in this unit's line (base units only)
    bool civilian() const { return cls == UnitClass::Settler || cls == UnitClass::Worker || cls == UnitClass::Band || cls == UnitClass::Trader; }
    bool military() const { return !civilian() && cls != UnitClass::Nuke; }
    bool ranged() const { return rstr > 0; }
    bool naval() const { return cls == UnitClass::Naval; }
    bool landMilitary() const { return military() && !naval(); }
};

// ---------------------------------------------------------------- buildings
enum class BSpecial { None, Palace, FreeTech, RuinScience, RuinProd, RuinBonus, Victory, CombatBonus, HealBonus, GrowthBonus, HappyAll };

struct BuildingDef {
    std::string name;
    int cost;
    std::string tech;
    Yields flat;
    Yields pct;
    int defense, hp;
    bool wonder;
    BSpecial special;
    std::string desc;
    int techId = -1;
    std::vector<int> needs;  // strategic resources required to build it
    int happy = 0;           // happiness in this city
};

// ---------------------------------------------------------------- techs
struct TechDef {
    std::string name;
    int col;
    std::vector<std::string> prereqNames;
    std::string flavor;
    int cost = 0;
    int era = 0;
    int row = 0;
    std::vector<int> prereqs;
    int trait = -1;   // evolutionary trait required before this can be researched
};

// ---------------------------------------------------------------- evolution
// Four kinds of evolutionary pressure. Each trait needs points of particular kinds,
// so a species can only follow a strand if its life actually exerts that pressure.
enum PressureCat { PC_SOCIAL, PC_ECOLOGY, PC_CONFLICT, PC_MUTATION, PC_COUNT };
extern const char* PRESSURE_NAMES[PC_COUNT];
extern const char* PRESSURE_ABBR[PC_COUNT];

enum Strand { ST_TRUNK, ST_TOOL, ST_SOCIAL, ST_PREDATOR, ST_MUTANT, ST_COUNT };
extern const char* STRAND_NAMES[ST_COUNT];

enum TraitId {
    TR_NEOCORTEX, TR_GESTURES, TR_PROTO_SPEECH, TR_SPEECH, TR_ADV_SPEECH, TR_ABSTRACT,
    TR_GRIP, TR_SPATIAL, TR_TOOLMAKERS,
    TR_PAIRBOND, TR_CHILDHOOD, TR_COLLECTIVE,
    TR_PACK, TR_AMBUSH, TR_APEX,
    TR_RADTOL, TR_HEALING, TR_FALLOUT_ADAPTED,
    TR_COUNT
};

// Effects of traits (summed per civilisation)
struct TraitFx {
    int food = 0, prod = 0, sci = 0, cult = 0;   // percentages
    int flatSci = 0, flatCult = 0, flatProd = 0; // per city
    int combat = 0, heal = 0, milCost = 0, workFaster = 0;
    bool falloutHalf = false, falloutYields = false;
    TraitFx& operator+=(const TraitFx& o) {
        food += o.food; prod += o.prod; sci += o.sci; cult += o.cult;
        flatSci += o.flatSci; flatCult += o.flatCult; flatProd += o.flatProd;
        combat += o.combat; heal += o.heal; milCost += o.milCost; workFaster += o.workFaster;
        falloutHalf = falloutHalf || o.falloutHalf;
        falloutYields = falloutYields || o.falloutYields;
        return *this;
    }
};

struct TraitDef {
    std::string name;
    int strand;
    int tier;
    int cost[PC_COUNT];
    std::vector<int> prereqs;
    bool capstone;               // only one capstone trait per species
    std::string effect;
    std::string flavor;
    TraitFx fx;
};

extern const char* ERA_NAMES[];
extern const char* SPEED_NAMES[3];
extern const int SPEED_PCT[3];
extern const int SPEED_TURNS[3];
extern const int SPEED_COST[3];
static const int NUM_ERAS = 9;

// ---------------------------------------------------------------- traditions (policies)
enum class PolicyEffect { Food, Prod, Sci, Culture, Gold, Combat, RuinBonus, SettlerCost, UnitCost, CityDefense, Borders, Heal, FlatSci, FlatCult, Happy, Weariness };
struct PolicyDef {
    std::string name, desc;
    PolicyEffect eff;
    int value;
    std::string tech;
    int techId = -1;
};

// ---------------------------------------------------------------- primates
struct PrimateDef {
    std::string name;        // "Chimpanzee"
    std::string plural;      // "Chimpanzees"
    std::string leader;
    std::string ability;     // ability title
    std::string abilityDesc;
    std::string flavor;
    Color color;
    int foodPct, prodPct, sciPct, cultPct, goldPct, combatPct, ruinPct;
    int extraMoves;          // all land units
    bool forestWalker;       // forest/jungle cost 1
    bool snowMonkey;         // tundra/snow bonuses
    int healBonus;
    int borderPct;           // faster borders
    int militaryCostPct;     // negative = cheaper
    int excavateFaster;      // turns less
    bool falloutResist;
    int aggression;          // 0..10 for AI
    int startBias;           // 0 jungle,1 forest,2 plains,3 coast,4 tundra,5 hills
    std::vector<std::string> cityNames;
    int evoPct = 0;          // evolution speed bonus
    int evoLean = 0;         // evolutionary strand this species naturally favours (AI and description)
    // diplomatic personality (0..10 unless noted)
    std::string persona, personaDesc;
    int friendliness = 0;    // -10..10: starting goodwill towards everyone
    int loyalty = 5;         // keeps its word; low loyalty may betray friends
    int greed = 5;           // drives a hard bargain
    int territorial = 5;     // resents neighbours pressing on its lands
    int pactLove = 5;        // keen on pacts and alliances
    bool ape = true;         // great/lesser ape (true) or monkey (false): kin feeling
};

// ---------------------------------------------------------------- tables
extern std::vector<TerrainDef> g_terrain;
extern std::vector<ResourceDef> g_resources;
extern std::vector<ImprovementDef> g_improvements;
extern std::vector<RuinDef> g_ruins;
extern std::vector<UnitDef> g_units;
extern std::vector<BuildingDef> g_buildings;
extern std::vector<TechDef> g_techs;
extern std::vector<PolicyDef> g_policies;
extern std::vector<PrimateDef> g_primates;

// ---------------------------------------------------------------- minor tribes
// Small one-city peoples of other primate species. Befriend them for their gifts; their ally also gets
// their resources and their spears.
enum TribeKind { TK_CULTURED, TK_LEARNED, TK_MERCANTILE, TK_MARITIME, TK_MILITANT, TK_COUNT };
struct TribeDef {
    std::string name;       // "Lemurs"
    std::string adjective;  // "Lemur"
    std::string city;       // their home
    std::string elder;      // who speaks for them
    int kind;               // TribeKind
    Color color;
};
extern std::vector<TribeDef> g_tribes;
extern const char* TRIBE_KIND_NAMES[TK_COUNT];
extern const char* TRIBE_KIND_GIFTS[TK_COUNT];
extern std::vector<TraitDef> g_traits;
extern std::vector<std::string> g_humanCityNames;

void initData();
int techIndex(const std::string& name);
int unitIndex(const std::string& name);
int buildingIndex(const std::string& name);
int policyIndex(const std::string& name);
int resourceIndex(const std::string& name);
std::string needsText(const std::vector<int>& needs);

// commonly referenced ids (filled by initData)
struct DataIds {
    int uElder, uGatherer, uScout, uWarband, uTinkerer, uBand, uNuke, uCaravan;
    int bPalace, bRebirth;
    int tCurrency, tRafts, tNavigation, tRadiology, tSalvaging, tDeciphering, tReverse, tComputing, tRebirth;
    int tCooking;
    int tForaging, tFission, tLaw, tBanking, tBartering;
    int tCultivation, tDigging, tWoodcraft, tIndustrial, tChemistry;
};
extern DataIds ID;
