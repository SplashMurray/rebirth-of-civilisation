// Rebirth of Civilisation - static game data (rules tables)
// SPDX-License-Identifier: MIT
#pragma once
#include "Types.h"

// ---------------------------------------------------------------- terrain
enum Terrain : uint8_t { T_OCEAN, T_COAST, T_GRASS, T_PLAINS, T_DESERT, T_TUNDRA, T_SNOW, T_COUNT };
enum Vegetation : uint8_t { V_NONE, V_FOREST, V_JUNGLE };

struct TerrainDef {
    std::string name;
    int food, prod, gold;
    Color color;
    bool water;
};

enum Resource : uint8_t {
    R_NONE, R_FRUIT, R_FISH, R_GAME, R_STONE, R_IRON, R_HORSES, R_OIL, R_SCRAP,
    R_COPPER, R_TIN, R_COAL, R_URANIUM, R_GOLD, R_WHEAT, R_RICE, R_MAIZE, R_COUNT
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
enum class UnitClass { Settler, Worker, Recon, Melee, Ranged, Mounted, Siege, Armor };

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
    bool civilian() const { return cls == UnitClass::Settler || cls == UnitClass::Worker; }
    bool military() const { return !civilian(); }
    bool ranged() const { return rstr > 0; }
};

// ---------------------------------------------------------------- buildings
enum class BSpecial { None, Palace, FreeTech, RuinScience, RuinProd, RuinBonus, Victory, CombatBonus, HealBonus, GrowthBonus };

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
enum TraitId { TR_NEOCORTEX, TR_GESTURES, TR_PROTO_SPEECH, TR_SPEECH, TR_ADV_SPEECH, TR_ABSTRACT, TR_CHILDHOOD, TR_COLLECTIVE, TR_COUNT };
struct TraitDef {
    std::string name;
    int cost;
    std::string effect;
    std::string flavor;
};
extern const char* ERA_NAMES[];
static const int NUM_ERAS = 9;

// ---------------------------------------------------------------- traditions (policies)
enum class PolicyEffect { Food, Prod, Sci, Culture, Gold, Combat, RuinBonus, SettlerCost, UnitCost, CityDefense, Borders, Heal, FlatSci, FlatCult };
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
    int uElder, uGatherer, uScout, uWarband, uTinkerer;
    int bPalace, bRebirth;
    int tCurrency, tRafts, tNavigation, tRadiology, tSalvaging, tDeciphering, tReverse, tComputing, tRebirth;
    int tCooking;
    int tCultivation, tDigging, tWoodcraft, tIndustrial, tChemistry;
};
extern DataIds ID;
