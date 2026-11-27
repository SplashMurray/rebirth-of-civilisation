// Rebirth of Civilisation - rules tables
// SPDX-License-Identifier: MIT
#include "Data.h"
#include <stdexcept>

using UC = UnitClass;

std::vector<TerrainDef> g_terrain;
std::vector<ResourceDef> g_resources;
std::vector<ImprovementDef> g_improvements;
std::vector<RoadDef> g_roads;
std::vector<FeatureDef> g_features;
std::vector<NaturalDef> g_naturals;
std::vector<RuinDef> g_ruins;
std::vector<UnitDef> g_units;
std::vector<BuildingDef> g_buildings;
std::vector<TechDef> g_techs;
std::vector<PolicyDef> g_policies;
std::vector<PrimateDef> g_primates;
std::vector<TribeDef> g_tribes;
const char* GREAT_KIND_NAMES[GK_COUNT + 1] = {"Great Thinker", "Great Artisan", "Great Merchant", "Great Storyteller", "Great Warleader", "The Last Human"};
const char* GREAT_SOURCE[GK_COUNT] = {"Science", "Production", "Gold and trade routes", "Culture", "Battles won"};
std::vector<std::vector<std::string>> g_greatNames = {
    {"Ishi the Stargazer", "Tamba Knot-Tier", "Grey Ula", "Bakari Who-Counts", "Sefu the Patient", "Nyota of the Long Look", "Old Ponderer", "Kesi Seed-Reader",
     "Lumo Fire-Keeper", "Wise Adaeze"},
    {"Mosi Stone-Shaper", "Chuma the Smith", "Zola Weaver", "Hodari Bridge-Builder", "Kito Many-Hands", "Imara the Potter", "Jengo Wheelwright",
     "Asha Kiln-Mother", "Baraka Who-Mends", "Tumo the Tinker"},
    {"Amani Far-Trader", "Dalia Salt-Seller", "Faraji of the Caravans", "Neema Sharp-Eye", "Obi Pearl-Diver", "Rehema Gold-Purse", "Simba Honey-Merchant",
     "Tisa the Broker", "Zuri Long-Road", "Juma Fair-Weigh"},
    {"Hadithi Fire-Talker", "Wanjiru Song-Keeper", "Kaya Who-Remembers", "Nia Dream-Teller", "Omari the Laughing", "Sauti Loud-Voice", "Tendai Mask-Maker",
     "Ayo Drum-Beater", "Malaika the Dancer", "Shani Moon-Singer"},
    {"Jabari the Fearless", "Shaka Scar-Brow", "Kifo Iron-Grip", "Moto Fire-Eyes", "Nguvu the Unbroken", "Rafiki Shield-Wall", "Tau the Ambusher",
     "Vita Spear-Mother", "Askari Ten-Battles", "Kali the Swift"},
};
const char* TRIBE_KIND_NAMES[TK_COUNT] = {"Cultured", "Learned", "Mercantile", "Maritime", "Militant"};
const char* TRIBE_KIND_GIFTS[TK_COUNT] = {"Culture", "Science", "Gold", "Food", "Troops"};
std::vector<TraitDef> g_traits;
std::vector<std::string> g_humanCityNames;
DataIds ID;

const char* PRESSURE_NAMES[PC_COUNT] = {"Social", "Ecological", "Conflict", "Mutation"};
const char* PRESSURE_ABBR[PC_COUNT] = {"Soc", "Eco", "Con", "Mut"};
const char* STRAND_NAMES[ST_COUNT] = {"Brain and Language", "Tool Brain", "Social Brain", "Predator Brain", "Mutant Line"};
const char* SPEED_NAMES[3] = {"Quick", "Standard", "Epic"};
const int SPEED_PCT[3] = {100, 150, 225};
const int SPEED_TURNS[3] = {330, 500, 750};
// costs rise faster than the turn count: on longer games empires grow to full size relatively earlier
const int SPEED_COST[3] = {100, 170, 250};
const char* ERA_NAMES[] = {"Awakening", "Tribal", "Ancient", "Classical", "Kingdoms",
                           "Reclamation", "Industrial", "Modern", "Rebirth"};
const char* RUIN_TIER_TECH[RUIN_TIERS] = {"Salvaging", "Deciphering", "Reverse Engineering", "Computing"};
const char* RUIN_TIER_NAME[RUIN_TIERS] = {"Scrap", "Books", "Machines", "Data Archives"};
int g_ruinTierTech[RUIN_TIERS];

static Yields Y(int f, int p, int s, int c, int g) {
    Yields y;
    y.food = f; y.prod = p; y.sci = s; y.cult = c; y.gold = g;
    return y;
}

int techIndex(const std::string& n) {
    for (size_t i = 0; i < g_techs.size(); i++) if (g_techs[i].name == n) return (int)i;
    return -1;
}
int unitIndex(const std::string& n) {
    for (size_t i = 0; i < g_units.size(); i++) if (g_units[i].name == n) return (int)i;
    return -1;
}
int buildingIndex(const std::string& n) {
    for (size_t i = 0; i < g_buildings.size(); i++) if (g_buildings[i].name == n) return (int)i;
    return -1;
}
int resourceIndex(const std::string& n) {
    for (size_t i = 0; i < g_resources.size(); i++) if (g_resources[i].name == n) return (int)i;
    return -1;
}
bool isCrop(int r) { return r > 0 && r < (int)g_resources.size() && g_resources[r].kind == RK_CROP; }
bool isMineral(int r) {
    return r == R_STONE || r == R_IRON || r == R_OIL || r == R_SCRAP || r == R_COPPER || r == R_TIN || r == R_COAL || r == R_URANIUM || r == R_GOLD ||
           r == R_SALT || r == R_GEMS;
}
std::string needsText(const std::vector<int>& needs) {
    std::string s;
    for (size_t i = 0; i < needs.size(); i++) s += (i ? (i + 1 == needs.size() ? " and " : ", ") : "") + g_resources[needs[i]].name;
    return s;
}
int policyIndex(const std::string& n) {
    for (size_t i = 0; i < g_policies.size(); i++) if (g_policies[i].name == n) return (int)i;
    return -1;
}

static int needTech(const std::string& n) {
    if (n.empty()) return -1;
    int t = techIndex(n);
    if (t < 0) throw std::runtime_error("Unknown tech in data: " + n);
    return t;
}

void initData() {
    if (!g_techs.empty()) return;

    // ------------------------------------------------------------ terrain
    g_terrain = {
        {"Ocean", 1, 0, 0, rgb(28, 58, 112), true},
        {"Coast", 1, 0, 1, rgb(52, 104, 162), true},
        {"Grassland", 2, 0, 0, rgb(92, 148, 62), false},
        {"Plains", 1, 1, 0, rgb(166, 156, 82), false},
        {"Desert", 0, 0, 0, rgb(218, 196, 138), false},
        {"Tundra", 1, 0, 0, rgb(134, 138, 116), false},
        {"Snow", 0, 0, 0, rgb(232, 238, 244), false},
    };

    // name, abbr, food, prod, gold, reveal tech, colour, kind, hint
    g_resources = {
        {"", "", 0, 0, 0, "", rgb(0, 0, 0), RK_BONUS, ""},
        {"Wild Fruit", "Fr", 1, 0, 1, "", rgb(240, 200, 40), RK_BONUS, ""},
        {"Fish", "Fi", 2, 0, 1, "", rgb(150, 210, 240), RK_BONUS, ""},
        {"Game", "Gm", 1, 1, 0, "", rgb(160, 100, 60), RK_BONUS, ""},
        {"Stone", "St", 0, 1, 0, "", rgb(170, 170, 170), RK_BONUS, "Mine: +2 Production"},
        {"Iron", "Fe", 0, 2, 0, "Smelting", rgb(120, 130, 150), RK_STRATEGIC, "Mine: +2 Production"},
        {"Horses", "Ho", 1, 1, 0, "Animal Taming", rgb(200, 150, 100), RK_STRATEGIC, "Pasture: +1 Food, +1 Production"},
        {"Oil", "Oi", 0, 3, 1, "Combustion", rgb(30, 30, 30), RK_STRATEGIC, "Mine: +2 Production"},
        {"Scrap Metal", "Sm", 0, 1, 1, "Salvaging", rgb(190, 120, 70), RK_STRATEGIC, "Mine: +2 Production. Counts as Iron."},
        {"Copper", "Cu", 0, 1, 1, "Digging", rgb(204, 112, 60), RK_STRATEGIC, "Mine: +2 Production"},
        {"Tin", "Sn", 0, 1, 0, "Bronze Working", rgb(176, 186, 196), RK_STRATEGIC, "Mine: +2 Production"},
        {"Coal", "Co", 0, 2, 0, "Steam Power", rgb(54, 50, 50), RK_STRATEGIC, "Mine: +2 Production"},
        {"Uranium", "U", 0, 1, 1, "Radiology", rgb(120, 230, 80), RK_STRATEGIC, "Mine: +2 Production"},
        {"Gold Ore", "Au", 0, 0, 3, "Currency", rgb(250, 214, 60), RK_LUXURY, "Mine: +2 Production"},
        {"Wheat", "Wh", 1, 0, 0, "", rgb(232, 206, 120), RK_CROP, "Farm: +2 Food"},
        {"Rice", "Ri", 1, 0, 0, "", rgb(236, 240, 220), RK_CROP, "Farm: +2 Food"},
        {"Maize", "Mz", 1, 0, 0, "", rgb(248, 190, 40), RK_CROP, "Farm: +2 Food"},
        // luxuries: each kind you have makes every city happier
        {"Honey", "Hn", 1, 0, 1, "", rgb(240, 170, 30), RK_LUXURY, ""},
        {"Cacao", "Ca", 0, 0, 2, "Foraging", rgb(120, 70, 40), RK_LUXURY, "Trading Post: +2 Gold"},
        {"Spices", "Sp", 1, 0, 1, "Bartering", rgb(220, 90, 40), RK_LUXURY, "Trading Post: +2 Gold"},
        {"Grapes", "Gp", 1, 0, 1, "Cultivation", rgb(140, 60, 150), RK_LUXURY, "Trading Post: +2 Gold"},
        {"Salt", "Sa", 1, 0, 1, "Digging", rgb(236, 236, 236), RK_LUXURY, "Mine: +2 Production"},
        {"Pearls", "Pe", 1, 0, 2, "Rafts", rgb(250, 240, 250), RK_LUXURY, ""},
        {"Furs", "Fu", 0, 1, 1, "Spearcraft", rgb(170, 120, 80), RK_LUXURY, "Lumber Camp or Trading Post"},
        {"Gems", "Ge", 0, 0, 3, "Masonry", rgb(90, 220, 210), RK_LUXURY, "Mine: +2 Production"},
    };

    // name, tech, Gatherer turns, movement cost (in MP), trade bonus % per road tile
    g_roads = {
        {"", "", 0, MP, 0},
        {"Worn Track", "", 2, 8, 10},
        {"Dirt Road", "Bartering", 3, 6, 20},
        {"Cobblestone Road", "Construction", 4, 4, 32},
        {"Paved Road", "Industrialisation", 4, 3, 45},
        {"Tarmac Road", "Combustion", 5, 2, 60},
    };
    g_improvements = {
        {"", 0, 0, 0, "", 0},
        {"Farm", 1, 0, 0, "Cultivation", 5},
        {"Mine", 0, 2, 0, "Digging", 5},
        {"Lumber Camp", 0, 1, 0, "Woodcraft", 5},
        {"Trading Post", 0, 0, 2, "Currency", 5},
        {"Pasture", 1, 1, 0, "Animal Taming", 4},
    };

    // minor tribes: name, adjective, home, elder, kind, colour
    g_tribes = {
        {"Ring-tailed Lemurs", "Lemur", "Isalo", "Matriarch Vola", TK_CULTURED, rgb(190, 190, 200)},
        {"Mandrills", "Mandrill", "Moukalaba", "Old Bluenose", TK_MILITANT, rgb(70, 110, 200)},
        {"Howler Monkeys", "Howler", "Barro Colorado", "Loudest Voice", TK_CULTURED, rgb(150, 70, 40)},
        {"Spider Monkeys", "Spider Monkey", "Calakmul", "Long-Arms", TK_MERCANTILE, rgb(60, 50, 40)},
        {"Tarsiers", "Tarsier", "Bohol", "Wide-Eyes", TK_LEARNED, rgb(200, 160, 110)},
        {"Proboscis Monkeys", "Proboscis", "Klias", "Great Nose", TK_MARITIME, rgb(230, 150, 90)},
        {"Geladas", "Gelada", "Guassa", "Bleeding-Heart", TK_MILITANT, rgb(170, 40, 50)},
        {"Colobus Monkeys", "Colobus", "Kakamega", "White Mantle", TK_LEARNED, rgb(235, 235, 235)},
        {"Marmosets", "Marmoset", "Atlantica", "Tufted Ear", TK_MERCANTILE, rgb(210, 200, 120)},
        {"Snub-nosed Monkeys", "Snub-nosed", "Shennongjia", "Golden Coat", TK_LEARNED, rgb(240, 170, 40)},
        {"Uakaris", "Uakari", "Jau", "Red-Face", TK_MARITIME, rgb(220, 60, 60)},
        {"Hanuman Langurs", "Langur", "Ranthambore", "Grey Sage", TK_CULTURED, rgb(150, 150, 140)},
        {"Drills", "Drill", "Bioko", "Black-Mask", TK_MILITANT, rgb(40, 40, 60)},
        {"Squirrel Monkeys", "Squirrel Monkey", "Pacaya", "Quick-Tail", TK_MARITIME, rgb(240, 200, 60)},
    };

    // name, food, prod, gold (added to the tile), description
    g_features = {
        {"", 0, 0, 0, ""},
        {"Floodplain", 2, 0, 0, "Silt left by the river's yearly floods: rich, easily farmed soil."},
        {"Marsh", 1, -1, 0, "Reeds and standing water. Slow going; a farm drains it."},
        {"Oasis", 3, 0, 1, "A spring in the desert. Fresh water for a city beside it; cannot be improved."},
        {"Volcano", 0, 0, 0, "A smoking mountain. Its ash makes the land around it fertile - and it may erupt."},
        {"Reef", 1, 1, 0, "Coral shallows teeming with fish."},
        {"Lake", 1, 0, 1, "Fresh water. Boats that keep to the coast can sail it; cities beside it grow faster."},
        {"", 0, 0, 0, ""},
    };

    // natural wonders: name, yields {food, prod, sci, cult, gold}, where, description
    g_naturals = {
        {"Thundering Falls", Yields{0, 2, 0, 2, 2}, WS_FALLS,
         "A river plunges off a cliff in a roar of spray. Boats cannot pass the falls: cargo must be carried around."},
        {"Elder Baobab", Yields{2, 0, 0, 3, 1}, WS_FLAT, "A tree so old and vast that whole troops have sheltered in its hollow trunk."},
        {"Crystal Caverns", Yields{0, 1, 3, 1, 0}, WS_HILLS, "Caves glittering with crystals as long as a gorilla is tall."},
        {"Coral Gardens", Yields{3, 0, 1, 0, 2}, WS_REEF, "A vast living reef in warm shallow water, swarming with fish."},
        {"Mirror Salt Flats", Yields{0, 1, 1, 1, 3}, WS_SALT, "A dry lake of white salt that mirrors the sky after rain."},
        {"Steaming Springs", Yields{2, 0, 1, 2, 1}, WS_COLD, "Hot pools in the frozen north, where the snow monkeys bathe."},
        {"Ember Crown", Yields{0, 0, 2, 3, 0}, WS_PEAK, "A great volcano, its crater glowing at night. Nothing can climb it."},
        {"Sky Mesa", Yields{0, 2, 1, 2, 0}, WS_MESA, "A flat-topped mountain of red rock rising sheer from the plains."},
    };

    g_ruins = {
        {"", "", 0, 0},
        {"Human City", "Ruined City", 0xF, 3},
        {"Human Town", "Ruined Town", 0x3, 1},
        {"Military Base", "Ruined Military Base", 0x5, 2},
        {"Research Laboratory", "Ruined Laboratory", 0xA, 1},
        {"Nuclear Power Plant", "Ruined Power Plant", 0x5, 2},
    };

    g_humanCityNames = {"Lagos", "Kinshasa", "Jakarta", "Mumbai", "Tokyo", "London", "New York",
                        "Sao Paulo", "Cairo", "Shanghai", "Moscow", "Mexico City", "Sydney", "Paris",
                        "Delhi", "Los Angeles", "Istanbul", "Nairobi", "Buenos Aires", "Seoul",
                        "Chicago", "Berlin", "Manila", "Lima"};

    // ------------------------------------------------------------ technologies
    g_techs = {
        {"Fire", 0, {}, "A lightning-struck tree, a flame carefully carried home and kept alive through the night."},
        {"Foraging", 0, {}, "Knowing which fruit is ripe, which roots are safe, and where to find them."},
        {"Stone Tools", 0, {}, "A stick to fish for termites. A sharp stone to crack a nut or cut a vine."},
        {"Cooking", 1, {"Fire", "Foraging"}, "Food held over the fire is easier to eat and feeds a bigger brain."},
        {"Woodcraft", 1, {"Stone Tools"}, "Branches shaped with purpose."},
        {"Storytelling", 2, {"Cooking"}, "Around the fire, the elders speak of what came before."},
        {"Cultivation", 2, {"Foraging"}, "Seeds dropped by the band sprout where it camped last season. A garden must be tended - and so, for the first time, the band can stay."},
        {"Digging", 2, {"Stone Tools", "Fire"}, "Beneath the soil lie stone, metal and secrets."},
        {"Spearcraft", 2, {"Fire", "Woodcraft"}, "Fire-hardened points."},
        {"Rafts", 2, {"Woodcraft"}, "Lashed logs carry the troop across the shallows."},
        {"Symbols", 3, {"Storytelling"}, "Marks on bark that mean something."},
        {"Salvaging", 3, {"Digging"}, "The dead cities are full of strange, useful scrap."},
        {"Archery", 3, {"Spearcraft"}, "Strike from afar."},
        {"Animal Taming", 3, {"Cultivation", "Spearcraft"}, "Feral horses and great dogs learn to carry riders."},
        {"Masonry", 3, {"Digging"}, "Stacked stone outlasts any nest."},
        {"Bronze Working", 3, {"Digging"}, "Copper and tin, melted together, make something harder than either."},
        {"Tribal Law", 4, {"Symbols", "Cultivation"},
         "Marks on the trees say where our land ends. Strangers with spears must ask first. Your borders close to foreign armies, and you can impose embargoes."},
        {"Writing", 4, {"Symbols"}, "Knowledge outlives the one who knew it."},
        {"Smelting", 4, {"Salvaging", "Bronze Working"}, "Melted scrap pours into new shapes."},
        {"Bartering", 4, {"Cultivation", "Symbols"}, "Two bananas for a sharp stone."},
        {"Currency", 5, {"Bartering", "Writing"}, "Bottle caps from the old world become money. Unlocks Gold income."},
        {"Mathematics", 5, {"Writing", "Masonry"}, "Counting beyond fingers and toes."},
        {"Deciphering", 6, {"Writing", "Salvaging"}, "We can finally read the humans' books."},
        {"Philosophy", 6, {"Writing", "Currency"}, "Why did they destroy themselves?"},
        {"Construction", 6, {"Mathematics"}, "Arches, aqueducts and roads."},
        {"Steel", 6, {"Smelting", "Mathematics"}, "Stronger than the scrap it came from."},
        {"Navigation", 6, {"Rafts", "Mathematics"}, "Stars guide boats across the open ocean."},
        {"Theology", 7, {"Philosophy"}, "Were the humans gods, or warnings?"},
        {"Education", 7, {"Philosophy", "Deciphering"}, "Every child of the troop learns to read."},
        {"Mechanics", 7, {"Construction", "Steel"}, "Gears, levers and springs."},
        {"Chemistry", 8, {"Education"}, "The substances of the world, named and measured."},
        {"Printing", 8, {"Education"}, "One book becomes a thousand."},
        {"Reverse Engineering", 8, {"Mechanics", "Deciphering"}, "Taking apart human machines to learn how they worked."},
        {"Gunpowder", 9, {"Chemistry", "Reverse Engineering"}, "Rusted rifles from the armouries are made to fire again."},
        {"Radiology", 9, {"Chemistry", "Reverse Engineering"}, "Understanding the invisible poison that killed the humans."},
        {"Banking", 9, {"Printing", "Currency"}, "Gold stored, lent and multiplied."},
        {"Steam Power", 10, {"Reverse Engineering"}, "Boiling water turns wheels."},
        {"Rifling", 10, {"Gunpowder"}, "Spiralled barrels for accuracy."},
        {"Industrialisation", 11, {"Steam Power", "Banking"}, "Factories rise beside the old ruins."},
        {"Electricity", 11, {"Radiology", "Steam Power"}, "The lights come back on."},
        {"Combustion", 11, {"Steam Power", "Chemistry"}, "Oil from the earth powers engines."},
        {"Ballistics", 12, {"Rifling", "Industrialisation"}, "The mathematics of the shell."},
        {"Mass Media", 12, {"Electricity", "Printing"}, "Voices carried across the whole world."},
        {"Computing", 13, {"Electricity", "Mass Media"}, "The humans' silicon minds wake again. Their data archives can be read."},
        {"Motorised Warfare", 13, {"Combustion", "Ballistics"}, "The humans' tanks roll once more."},
        {"Nuclear Fission", 13, {"Radiology", "Electricity"}, "The power that ended the old world. Now it is yours."},
        {"Genetics", 14, {"Computing", "Radiology"}, "We understand what we are, and what we were."},
        {"Rocketry", 14, {"Computing", "Combustion"}, "The sky is no longer the limit."},
        {"Rebirth", 15, {"Genetics", "Rocketry"}, "Civilisation reborn - wiser than the last."},
    };
    static const int ERA_BY_COL[] = {0, 0, 1, 1, 2, 2, 3, 3, 4, 5, 5, 6, 6, 7, 7, 8};
    static const int COST_BY_COL[] = {25, 50, 90, 150, 420, 700, 900, 1300, 1800, 2450, 3300, 4200, 5200, 6300, 7500, 9400};
    {
        int rowCount[16] = {0};
        for (auto& t : g_techs) {
            t.era = ERA_BY_COL[t.col];
            t.cost = COST_BY_COL[t.col];
            t.row = rowCount[t.col]++;
            // evolution gates: no real science without language
            t.trait = t.era == 0 ? TR_NEOCORTEX : t.era == 1 ? TR_SPEECH : t.era == 2 ? TR_ADV_SPEECH : TR_ABSTRACT;
        }
        for (auto& t : g_techs)
            for (auto& p : t.prereqNames) t.prereqs.push_back(needTech(p));
    }
    for (auto& r : g_resources) r.revealTechId = needTech(r.revealTech);
    for (auto& im : g_improvements) im.techId = needTech(im.tech);
    for (auto& r : g_roads) r.techId = needTech(r.tech);
    for (int i = 0; i < RUIN_TIERS; i++) g_ruinTierTech[i] = needTech(RUIN_TIER_TECH[i]);

    // ------------------------------------------------------------ primates
    // name, plural, leader, ability, abilityDesc, flavor, color,
    // food, prod, sci, cult, gold, combat, ruin, extraMoves, forestWalker, snow, heal, border, milCost, excavate, fallout, aggression, startBias
    g_primates = {
        {"Chimpanzee", "Chimpanzees", "Alpha Kibo", "Tool Masters",
         "+15% Production, +10% combat strength. Unique unit: Hunting Party.",
         "Cunning, ambitious and quick to war. The chimpanzees were using tools long before the humans fell.",
         rgb(176, 64, 40), 0, 15, 0, 0, 0, 10, 0, 0, false, false, 0, 0, 0, 0, false, 8, 0,
         {"Kibale", "Gombe", "Mahale", "Budongo", "Tai", "Bossou", "Fongoli", "Ngogo", "Kalinzu", "Lope", "Goualougo", "Sonso", "Kanyawara", "Issa"}},
        {"Bonobo", "Bonobos", "Matriarch Lomako", "Make Love, Not War",
         "+25% Culture, +15% Food, -10% combat strength. Unique unit: Peacekeepers.",
         "Peaceful, social and led by their matriarchs. Bonobos settle quarrels with affection rather than violence.",
         rgb(214, 84, 164), 15, 0, 0, 25, 0, -10, 0, 0, false, false, 0, 0, 0, 0, false, 1, 0,
         {"Wamba", "Lomako", "Lui Kotale", "Salonga", "Kokolopori", "Lola ya Bonobo", "Iyondji", "Bolobo", "Malebo", "Lukuru", "Tshuapa", "Maringa"}},
        {"Orangutan", "Orangutans", "Elder Tuan Batu", "Patient Minds",
         "+25% Science, forest and jungle cost 1 move, -10% Food. Unique unit: Canopy Stalkers.",
         "Solitary thinkers of the canopy. Slow to grow, but nothing escapes their patient study.",
         rgb(232, 128, 28), -10, 0, 25, 0, 0, 0, 0, 0, true, false, 0, 0, 0, 0, false, 3, 0,
         {"Tanjung Puting", "Sebangau", "Gunung Leuser", "Kutai", "Suaq", "Ketambe", "Batang Toru", "Danau Sentarum", "Kinabatangan", "Sabangau", "Tuanan", "Gunung Palung"}},
        {"Gorilla", "Gorillas", "Silverback Mzee", "Silverback Strength",
         "+25% combat strength, units heal +10 HP per turn, -10% Science. Unique unit: Silverback Guard.",
         "The mightiest of the great apes. Gentle until threatened, and then unstoppable.",
         rgb(78, 84, 108), 0, 0, -10, 0, 0, 25, 0, 0, false, false, 10, 0, 0, 0, false, 6, 1,
         {"Virunga", "Bwindi", "Karisoke", "Mgahinga", "Kahuzi", "Odzala", "Dzanga", "Lossi", "Mbeli", "Nouabale", "Cross River", "Sangha"}},
        {"Baboon", "Baboons", "Warlord Kaba", "Savanna Troops",
         "+1 movement for military units, military units cost 20% less. Unique unit: Baboon Raiders.",
         "Disciplined troops of the open savanna, they march in formation and fear nothing.",
         rgb(222, 186, 40), 0, 0, 0, 0, 0, 0, 0, 1, false, false, 0, 0, -20, 0, false, 9, 2,
         {"Amboseli", "Okavango", "Moremi", "Serengeti", "Kruger", "Laikipia", "Awash", "Simien", "Gashaka", "Tsaobis", "Mikumi", "Chobe"}},
        {"Capuchin", "Capuchins", "Tinker Pico", "Clever Hands",
         "+75% rewards from human ruins, excavation 1 turn faster, +20% Gold. Unique unit: Tinkerer.",
         "Small, clever and endlessly curious, capuchins pick apart the remains of humanity faster than anyone.",
         rgb(40, 168, 170), 0, 0, 0, 0, 20, 0, 75, 0, false, false, 0, 0, 0, 1, false, 4, 3,
         {"Serra da Capivara", "Boa Vista", "Corcovado", "Manu", "Tayrona", "Iguazu", "Pantanal", "Santa Rosa", "Cocha Cashu", "Fazenda", "Tikal", "Yasuni"}},
        {"Gibbon", "Gibbons", "Songmaster Wau", "Songs of the Canopy",
         "+20% Culture, +10% Science, borders expand 50% faster. Unique unit: Song Scouts.",
         "Their dawn songs carry for miles. Gibbon culture spreads through the forests like music.",
         rgb(64, 170, 80), 0, 0, 10, 20, 0, 0, 0, 0, false, false, 0, 50, 0, 0, false, 2, 1,
         {"Khao Yai", "Hoolock", "Cao Vit", "Danum", "Phong Nha", "Gunung Halimun", "Hainan", "Ujung Kulon", "Chiang Mai", "Siberut", "Bukit Barisan", "Mount Kinabalu"}},
        {"Japanese Macaque", "Snow Monkeys", "Matriarch Yuki", "Snow Monkeys",
         "Tundra and snow give +1 Food and +1 Production, units take half fallout damage, +10% Science. Unique unit: Onsen Guard.",
         "Hardy monkeys of the frozen north, warming themselves in hot springs while the world freezes.",
         rgb(150, 190, 230), 0, 0, 10, 0, 0, 0, 0, 0, false, true, 0, 0, 0, 0, true, 5, 4,
         {"Jigokudani", "Shiga", "Yakushima", "Koshima", "Arashiyama", "Shimokita", "Kinkazan", "Takasakiyama", "Hakusan", "Nikko", "Kamikochi", "Shodoshima"}},
    };

    // ------------------------------------------------------------ evolution
    // name, strand, tier, cost {Social, Ecology, Conflict, Mutation}, prereqs, capstone, effect, flavour
    g_traits = {
        // the trunk: brain and language. Every species must climb it for science.
        {"Enlarged Neocortex", ST_TRUNK, 0, {0, 15, 0, 0}, {}, false,
         "Science can now be earned. Unlocks Awakening-era technologies",
         "Generation by generation the forebrain grows. For the first time your primates can learn deliberately - to experiment, "
         "to remember what worked and to plan ahead."},
        {"Expressive Gestures", ST_TRUNK, 1, {10, 0, 0, 0}, {TR_NEOCORTEX}, false, "+1 Culture in every city",
         "Pointing, beckoning, a raised hand for silence. The troop begins to share what it means."},
        {"Proto-Speech", ST_TRUNK, 2, {20, 0, 0, 0}, {TR_GESTURES}, false, "+10% Science",
         "Calls grow more varied: a cry for snake, another for eagle, another for food. Each sound means one thing."},
        {"Speech", ST_TRUNK, 3, {40, 10, 0, 0}, {TR_PROTO_SPEECH}, false, "Unlocks Tribal-era technologies. +1 Science in every city",
         "The throat and the brain have changed. Sounds combine into words, and words name everything. Ideas can now be taught."},
        {"Advanced Speech", ST_TRUNK, 4, {75, 0, 0, 0}, {TR_SPEECH}, false, "Unlocks Ancient-era technologies. +10% Culture",
         "Grammar, tense and question. The troop can speak of yesterday and tomorrow, of what is and what might be."},
        {"Abstract Thought", ST_TRUNK, 5, {110, 50, 0, 0}, {TR_ADV_SPEECH}, false, "Unlocks Classical-era technologies and beyond. +10% Science",
         "Numbers without objects, rules without examples. Minds that can reason about the reasoning of others."},
        // the tool brain: shaped by the land itself
        {"Precision Grip", ST_TOOL, 1, {0, 30, 0, 0}, {TR_NEOCORTEX}, false, "+10% Production. Gatherers work 1 turn faster",
         "An opposable thumb that can pinch as well as grasp. Fine work becomes possible."},
        {"Spatial Reasoning", ST_TOOL, 2, {20, 90, 0, 0}, {TR_GRIP, TR_SPEECH}, false, "+1 Production in every city, +10% Production",
         "Seeing the tool inside the stone before the first blow is struck."},
        {"Master Toolmakers", ST_TOOL, 3, {40, 220, 0, 0}, {TR_SPATIAL, TR_ABSTRACT}, true, "+20% Production, +2 Production in every city",
         "Tools that make tools. Your species becomes the great builders of the new world."},
        // the social brain: shaped by living together
        {"Pair Bonding", ST_SOCIAL, 1, {40, 0, 0, 0}, {TR_GESTURES}, false, "+10% Food",
         "Partners share food and raise young together. Families become the bedrock of the troop."},
        {"Extended Childhood", ST_SOCIAL, 2, {120, 20, 0, 0}, {TR_PAIRBOND, TR_SPEECH}, false, "+10% Food, +5% Culture",
         "Young ones stay with their parents for years, learning everything the troop knows."},
        {"Collective Learning", ST_SOCIAL, 3, {280, 0, 0, 0}, {TR_CHILDHOOD, TR_ABSTRACT}, true, "+12% Science, +10% Culture",
         "Knowledge now grows faster than any single mind. Each generation begins where the last left off."},
        // the predator brain: shaped by violence
        {"Pack Hunters", ST_PREDATOR, 1, {0, 0, 20, 0}, {TR_NEOCORTEX}, false, "+10% combat strength",
         "Hunting together, flanking and driving prey. Every member knows their part."},
        {"Ambush Instinct", ST_PREDATOR, 2, {0, 20, 60, 0}, {TR_PACK, TR_SPEECH}, false, "+10% combat strength, units heal +5 HP per turn",
         "Patience, stillness and the sudden strike. Wounds are licked and warriors return."},
        {"Apex Predators", ST_PREDATOR, 3, {0, 0, 150, 0}, {TR_AMBUSH, TR_ADV_SPEECH}, true, "+15% combat strength, military units cost 15% less",
         "Nothing on Earth hunts your kind any longer. Your kind hunts everything."},
        // the mutant line: shaped by the poison of the old world
        {"Radiation Tolerance", ST_MUTANT, 1, {0, 0, 0, 12}, {TR_NEOCORTEX}, false, "Units take half damage from fallout",
         "Those who survived the poison passed something on to their children."},
        {"Accelerated Healing", ST_MUTANT, 2, {0, 0, 0, 40}, {TR_RADTOL}, false, "Units heal +10 HP per turn",
         "Cells that repair themselves at astonishing speed. A strange gift of the radiation."},
        {"Fallout-Adapted", ST_MUTANT, 3, {0, 40, 0, 110}, {TR_HEALING}, true, "Fallout no longer reduces tile yields. +10% Food and Production",
         "Your people thrive where the humans died. The glowing wastelands are now your gardens."},
    };
    {
        auto fx = [](int t) -> TraitFx& { return g_traits[t].fx; };
        fx(TR_GESTURES).flatCult = 1;
        fx(TR_PROTO_SPEECH).sci = 10;
        fx(TR_SPEECH).flatSci = 1;
        fx(TR_ADV_SPEECH).cult = 10;
        fx(TR_ABSTRACT).sci = 10;
        fx(TR_GRIP).prod = 10; fx(TR_GRIP).workFaster = 1;
        fx(TR_SPATIAL).flatProd = 1; fx(TR_SPATIAL).prod = 10;
        fx(TR_TOOLMAKERS).prod = 20; fx(TR_TOOLMAKERS).flatProd = 2;
        fx(TR_PAIRBOND).food = 10;
        fx(TR_CHILDHOOD).food = 10; fx(TR_CHILDHOOD).cult = 5;
        fx(TR_COLLECTIVE).sci = 12; fx(TR_COLLECTIVE).cult = 10;
        fx(TR_PACK).combat = 10;
        fx(TR_AMBUSH).combat = 10; fx(TR_AMBUSH).heal = 5;
        fx(TR_APEX).combat = 15; fx(TR_APEX).milCost = 15;
        fx(TR_RADTOL).falloutHalf = true;
        fx(TR_HEALING).heal = 10;
        fx(TR_FALLOUT_ADAPTED).falloutYields = true; fx(TR_FALLOUT_ADAPTED).food = 10; fx(TR_FALLOUT_ADAPTED).prod = 10;
    }

    // ------------------------------------------------------------ units
    // name, abbr, cost, str, rstr, range, moves, tech, class, replaces, primate, desc
    g_units = {
        {"Roaming Band", "RB", 0, 0, 0, 0, 2, "", UC::Band, "", -1, "Your wandering people. Forages the land around it; grows, splits and raises war parties. Settles once you have Speech and Cultivation."},
        {"Elder Troop", "ET", 45, 0, 0, 0, 2, "", UC::Settler, "", -1, "Founds a new settlement. City must be size 2; costs 1 population."},
        {"Gatherer", "Ga", 35, 0, 0, 0, 2, "", UC::Worker, "", -1, "Builds tile improvements, excavates human ruins and clears fallout."},
        {"Tinkerer", "Ti", 35, 0, 0, 0, 2, "", UC::Worker, "Gatherer", 5, "Capuchin Gatherer. Works 2 turns faster."},
        {"Scout", "Sc", 22, 5, 0, 0, 3, "", UC::Recon, "", -1, "Fast explorer. Sees further."},
        {"Song Scout", "SS", 22, 7, 0, 0, 4, "", UC::Recon, "Scout", 6, "Gibbon Scout. Faster and tougher."},
        {"Warband", "Wb", 30, 8, 0, 0, 2, "", UC::Melee, "", -1, "Basic melee troop."},
        {"Silverback Guard", "SG", 30, 12, 0, 0, 2, "", UC::Melee, "Warband", 3, "Gorilla Warband. Much stronger."},
        {"Stone Thrower", "ST", 35, 5, 7, 2, 2, "Stone Tools", UC::Ranged, "", -1, "Ranged attack at 2 tiles."},
        {"Spear Troop", "Sp", 50, 12, 0, 0, 2, "Spearcraft", UC::Melee, "", -1, "Disciplined spear-wielders."},
        {"Hunting Party", "HP", 50, 15, 0, 0, 3, "Spearcraft", UC::Melee, "Spear Troop", 0, "Chimpanzee Spear Troop. Stronger and faster."},
        {"Bronze Phalanx", "BP", 60, 15, 0, 0, 2, "Bronze Working", UC::Melee, "", -1, "A shield wall of bronze-armed troops."},
        {"Bow Troop", "Bw", 50, 7, 10, 2, 2, "Archery", UC::Ranged, "", -1, "Archers."},
        {"Peacekeepers", "PK", 50, 11, 12, 2, 2, "Archery", UC::Ranged, "Bow Troop", 1, "Bonobo Bow Troop. Tougher in defence."},
        {"Beast Riders", "BR", 70, 14, 0, 0, 4, "Animal Taming", UC::Mounted, "", -1, "Fast mounted raiders."},
        {"Baboon Raiders", "Rd", 70, 17, 0, 0, 5, "Animal Taming", UC::Mounted, "Beast Riders", 4, "Baboon Beast Riders. Stronger and faster."},
        {"Iron Blades", "IB", 75, 17, 0, 0, 2, "Smelting", UC::Melee, "", -1, "Wielders of re-forged human steel."},
        {"Onsen Guard", "OG", 75, 20, 0, 0, 2, "Smelting", UC::Melee, "Iron Blades", 7, "Macaque Iron Blades. Stronger."},
        {"Siege Sling", "SL", 85, 7, 14, 2, 2, "Mathematics", UC::Siege, "", -1, "Siege engine. Double damage against cities."},
        {"Crossbows", "Cb", 110, 13, 18, 2, 2, "Mechanics", UC::Ranged, "", -1, "Powerful ranged troop."},
        {"Canopy Stalkers", "CS", 110, 15, 21, 2, 3, "Mechanics", UC::Ranged, "Crossbows", 2, "Orangutan Crossbows. Stronger and faster."},
        {"Armoured Riders", "AR", 120, 24, 0, 0, 4, "Steel", UC::Mounted, "", -1, "Riders in salvaged armour plate."},
        {"Salvaged Rifles", "SR", 150, 28, 0, 0, 2, "Gunpowder", UC::Melee, "", -1, "Troops carrying restored human rifles."},
        {"Bombard", "Bd", 170, 14, 28, 2, 2, "Chemistry", UC::Siege, "", -1, "Explosive siege weapon."},
        {"Riflemen", "Rf", 210, 36, 0, 0, 2, "Rifling", UC::Melee, "", -1, "Line infantry with modern rifles."},
        {"Field Guns", "FG", 280, 20, 40, 3, 2, "Ballistics", UC::Siege, "", -1, "Long range artillery."},
        {"Infantry", "If", 300, 50, 0, 0, 2, "Combustion", UC::Melee, "", -1, "Motorised infantry."},
        {"Rebuilt Tanks", "Tk", 380, 66, 0, 0, 5, "Motorised Warfare", UC::Armor, "", -1, "Human tanks, restored and crewed by primates."},
        {"Nuclear Warhead", "NW", 600, 0, 0, 0, 0, "Nuclear Fission", UC::Nuke, "", -1,
         "The weapon that ended the old world. Kept in your silos: launch it at any tile within 12 of one of your cities. "
         "Destroys everything nearby, wrecks cities and leaves fallout - and the whole world will remember."},
        // ships: built in coastal cities, move on water (early ones keep to the coast)
        {"Dugout Canoe", "DC", 40, 6, 0, 0, 3, "Rafts", UC::Naval, "", -1, "A hollowed log paddled by warriors. Keeps to coastal waters, lakes and rivers."},
        {"War Galley", "WG", 65, 12, 0, 0, 3, "Bronze Working", UC::Naval, "", -1, "Oars and a bronze ram. Keeps to coastal waters, lakes and rivers."},
        {"Archer Galley", "AG", 70, 8, 11, 2, 3, "Archery", UC::Naval, "", -1, "Archers on a galley: attacks ships and the shore 2 tiles away. Keeps to coastal waters, lakes and rivers."},
        {"Caravel", "Cv", 95, 16, 0, 0, 5, "Navigation", UC::Naval, "", -1, "A fast sailing ship that can cross the open ocean."},
        {"Frigate", "Fr", 160, 22, 28, 2, 4, "Gunpowder", UC::Naval, "", -1, "Broadsides of salvaged cannon. Bombards ships and coasts 2 tiles away."},
        {"Salvaged Freighter", "SF", 140, 10, 0, 0, 5, "Steam Power", UC::Naval, "", -1,
         "A rusting human cargo ship, patched and steaming again. Carries 4 land units across the ocean."},
        {"Ironclad", "Ic", 220, 40, 0, 0, 4, "Steam Power", UC::Naval, "", -1, "A steam-driven, iron-plated ram, shallow enough to steam up rivers. Needs Coal."},
        {"River Gunboat", "RG", 170, 18, 26, 2, 4, "Steam Power", UC::Naval, "", -1,
         "A shallow-draught steamer with a deck gun. Patrols rivers and coasts and shells the banks 2 tiles away. Needs Coal."},
        {"Salvaged Destroyer", "Ds", 330, 58, 0, 0, 6, "Combustion", UC::Naval, "", -1, "A human destroyer raised from the harbour mud and made to sail again. Needs Oil."},
        {"Salvaged Battleship", "BB", 420, 50, 68, 3, 5, "Motorised Warfare", UC::Naval, "", -1,
         "The humans' greatest warship, restored. Its guns reach 3 tiles inland. Needs Oil."},
        {"Exo-Primates", "EP", 520, 90, 0, 0, 3, "Genetics", UC::Melee, "", -1, "Soldiers in powered exoskeletons."},
        {"Caravan", "Cv", 50, 0, 0, 0, 2, "Currency", UC::Trader, "", -1,
         "Sets up a trade route from the city it stands in to another city - yours or another species' - for 30 turns (more at slower speeds), then comes home. "
         "Earns gold and science abroad, food and production at home. Roads make routes richer. Walks through closed borders, but embargoes and wars end its routes."},
        // Great Primates (never built: they are born of a people's deeds)
        {"Great Thinker", "GT", 0, 0, 0, 0, 2, "", UC::Great, "", -2,
         "A mind ahead of its time. Share an insight for a burst of Science, or settle in a city to add +3 Science there for good."},
        {"Great Artisan", "GA", 0, 0, 0, 0, 2, "", UC::Great, "", -2,
         "A master maker. Throw their skill into what a city is building, or settle there to add +3 Production for good."},
        {"Great Merchant", "GM", 0, 0, 0, 0, 2, "", UC::Great, "", -2,
         "A trader whose name opens every gate. A trade mission to another people's city brings gold and goodwill (and much influence "
         "with a tribe); or settle in your city for +4 Gold."},
        {"Great Storyteller", "GS", 0, 0, 0, 0, 2, "", UC::Great, "", -2,
         "Their tales are told at every fire. Tell a great tale for Culture and a season of festivals (+2 Happiness in every city), or "
         "settle in a city for +3 Culture."},
        {"Great Warleader", "GW", 0, 0, 0, 0, 3, "", UC::Great, "", -2,
         "A legendary commander. Rallies the troops around them: full health and +20% strength for a while."},
        {"Missionary", "Mi", 0, 0, 0, 0, 3, "", UC::Great, "", -2,
         "Bought with Faith. Preaches the faith in a city (its own or another people's, if not at war with them), "
         "winning many followers there at once. Used up when it preaches."},
        {"The Last Human", "LH", 0, 0, 0, 0, 2, "", UC::Great, "", -2,
         "Cassandra, the last human, who fell from a blue box in the sky. She remembers the old world - and insists she is, technically, "
         "a primate."},
        // hostile wildlife and leftovers of humanity (never buildable, primate == -2)
        {"Feral Dogs", "FD", 0, 7, 0, 0, 2, "", UC::Melee, "", -2, "Packs of former pets, gone wild."},
        {"Mutant Hounds", "MH", 0, 20, 0, 0, 3, "", UC::Melee, "", -2, "Radiation has made them huge."},
        {"Rogue Machines", "RM", 0, 42, 0, 0, 2, "", UC::Armor, "", -2, "Human war machines still following their last orders."},
    };
    for (auto& u : g_units) u.techId = needTech(u.tech);
    for (auto& u : g_units) u.replacesId = u.replaces.empty() ? -1 : unitIndex(u.replaces);

    // ------------------------------------------------------------ buildings & wonders
    // name, cost, tech, flat, pct, defense, hp, wonder, special, desc
    g_buildings = {
        {"Nest Hall", 0, "", Y(0, 2, 3, 2, 3), Y(0, 0, 0, 0, 0), 3, 25, false, BSpecial::Palace, "Seat of the troop. Free in the capital."},
        {"Food Cache", 40, "Foraging", Y(2, 0, 0, 0, 0), Y(0, 0, 0, 0, 0), 0, 0, false, BSpecial::None, ""},
        {"Totem", 30, "Storytelling", Y(0, 0, 0, 2, 0), Y(0, 0, 0, 0, 0), 0, 0, false, BSpecial::None, ""},
        {"Elder Circle", 45, "Cooking", Y(0, 0, 2, 0, 0), Y(0, 0, 0, 0, 0), 0, 0, false, BSpecial::None, ""},
        {"Tool Shed", 55, "Woodcraft", Y(0, 2, 0, 0, 0), Y(0, 0, 0, 0, 0), 0, 0, false, BSpecial::None, ""},
        {"Palisade", 50, "Digging", Y(0, 0, 0, 0, 0), Y(0, 0, 0, 0, 0), 4, 50, false, BSpecial::None, ""},
        {"Training Ground", 60, "Spearcraft", Y(0, 0, 0, 0, 0), Y(0, 0, 0, 0, 0), 0, 0, false, BSpecial::Veterans,
         "Military units trained here start with 15 experience: a promotion at once"},
        {"Shrine", 60, "Symbols", Y(0, 0, 0, 2, 0), Y(0, 0, 0, 0, 0), 0, 0, false, BSpecial::None, ""},
        {"Salvage Yard", 70, "Salvaging", Y(0, 1, 0, 0, 0), Y(0, 10, 0, 0, 0), 0, 0, false, BSpecial::RuinProd, "+1 Production per ruin in territory"},
        {"Knowledge Hut", 75, "Writing", Y(0, 0, 3, 0, 0), Y(0, 0, 10, 0, 0), 0, 0, false, BSpecial::None, ""},
        {"Stone Walls", 80, "Masonry", Y(0, 0, 0, 0, 0), Y(0, 0, 0, 0, 0), 8, 75, false, BSpecial::None, ""},
        {"Market", 90, "Currency", Y(0, 0, 0, 0, 2), Y(0, 0, 0, 0, 25), 0, 0, false, BSpecial::None, ""},
        {"Archive of Humanity", 120, "Deciphering", Y(0, 0, 3, 1, 0), Y(0, 0, 0, 0, 0), 0, 0, false, BSpecial::RuinScience, "+2 Science per ruin in territory"},
        {"Aqueduct", 120, "Construction", Y(2, 0, 0, 0, 0), Y(15, 0, 0, 0, 0), 0, 0, false, BSpecial::None, ""},
        {"Temple", 110, "Theology", Y(0, 0, 0, 3, 0), Y(0, 0, 0, 0, 0), 0, 0, false, BSpecial::None, ""},
        {"University", 180, "Education", Y(0, 0, 4, 0, 0), Y(0, 0, 25, 0, 0), 0, 0, false, BSpecial::None, ""},
        {"Printing House", 200, "Printing", Y(0, 0, 2, 3, 0), Y(0, 0, 10, 10, 0), 0, 0, false, BSpecial::None, ""},
        {"Bank", 170, "Banking", Y(0, 0, 0, 0, 3), Y(0, 0, 0, 0, 25), 0, 0, false, BSpecial::None, ""},
        {"Hospital", 220, "Radiology", Y(3, 0, 0, 0, 0), Y(0, 0, 0, 0, 0), 0, 0, false, BSpecial::HealBonus, "Units in or next to the city heal fully"},
        {"Cathedral", 200, "Mechanics", Y(0, 0, 0, 3, 0), Y(0, 0, 0, 0, 0), 0, 0, false, BSpecial::None, "Vaulted halls raised to the sky"},
        {"Arena", 140, "Construction", Y(0, 0, 0, 1, 0), Y(0, 0, 0, 0, 0), 0, 0, false, BSpecial::None, "Games and contests"},
        {"Steam Workshop", 200, "Steam Power", Y(0, 3, 0, 0, 0), Y(0, 15, 0, 0, 0), 0, 0, false, BSpecial::None, ""},
        {"Bastion", 260, "Rifling", Y(0, 0, 0, 0, 0), Y(0, 0, 0, 0, 0), 14, 100, false, BSpecial::None, ""},
        {"Factory", 300, "Industrialisation", Y(0, 4, 0, 0, 0), Y(0, 25, 0, 0, 0), 0, 0, false, BSpecial::None, ""},
        {"Power Station", 350, "Electricity", Y(0, 2, 0, 0, 0), Y(0, 20, 0, 0, 0), 0, 0, false, BSpecial::None, ""},
        {"Stadium", 280, "Industrialisation", Y(0, 0, 0, 1, 1), Y(0, 0, 0, 0, 0), 0, 0, false, BSpecial::None, "Crowds cheering their team"},
        {"Broadcast Tower", 340, "Mass Media", Y(0, 0, 0, 5, 0), Y(0, 0, 0, 25, 0), 0, 0, false, BSpecial::None, ""},
        {"Research Lab", 450, "Computing", Y(0, 0, 6, 0, 0), Y(0, 0, 40, 0, 0), 0, 0, false, BSpecial::None, ""},
        {"Gene Clinic", 500, "Genetics", Y(4, 0, 2, 0, 0), Y(20, 0, 0, 0, 0), 0, 0, false, BSpecial::None, ""},
        // wonders
        {"Great Canopy", 150, "Woodcraft", Y(2, 0, 0, 3, 0), Y(0, 0, 0, 0, 0), 0, 0, true, BSpecial::GrowthBonus, "+10% Food in every city"},
        {"Festival Grounds", 260, "Bartering", Y(0, 0, 0, 3, 1), Y(0, 0, 0, 0, 0), 0, 0, true, BSpecial::HappyAll, "+1 Happiness in every city"},
        {"Monolith of Bones", 220, "Masonry", Y(0, 0, 0, 5, 0), Y(0, 0, 0, 0, 0), 0, 0, true, BSpecial::CombatBonus, "+10% combat strength for all units"},
        {"The First Library", 260, "Writing", Y(0, 0, 5, 2, 0), Y(0, 0, 0, 0, 0), 0, 0, true, BSpecial::FreeTech, "Grants a free technology"},
        {"Hall of Human Relics", 420, "Deciphering", Y(0, 0, 4, 4, 0), Y(0, 0, 0, 0, 0), 0, 0, true, BSpecial::RuinBonus, "+100% rewards from human ruins"},
        {"Restored Observatory", 650, "Education", Y(0, 0, 10, 0, 0), Y(0, 0, 25, 0, 0), 0, 0, true, BSpecial::None, "+10 Science, +25% Science here"},
        {"Rebirth Project", 3000, "Rebirth", Y(0, 0, 0, 0, 0), Y(0, 0, 0, 0, 0), 0, 0, true, BSpecial::Victory, "Rebuild civilisation. Completing it wins the game."},
    };
    for (auto& b : g_buildings) b.techId = needTech(b.tech);
    // happiness from buildings
    for (auto& hb : std::vector<std::pair<const char*, int>>{{"Totem", 1}, {"Shrine", 1}, {"Market", 1}, {"Temple", 2}, {"Cathedral", 2}, {"Arena", 3},
                                                             {"Hospital", 1}, {"Stadium", 3}, {"Broadcast Tower", 2}, {"Nest Hall", 1}})
        g_buildings[buildingIndex(hb.first)].happy = hb.second;
    // strategic resource requirements (unique units inherit those of the unit they replace)
    auto needU = [](const char* u, std::vector<int> r) { g_units[unitIndex(u)].needs = r; };
    auto needB = [](const char* b, std::vector<int> r) { g_buildings[buildingIndex(b)].needs = r; };
    needU("Bronze Phalanx", {R_COPPER, R_TIN});
    needU("Beast Riders", {R_HORSES});
    needU("Iron Blades", {R_IRON});
    needU("Armoured Riders", {R_HORSES, R_IRON});
    needU("Bombard", {R_IRON});
    needU("Rebuilt Tanks", {R_OIL});
    needU("Exo-Primates", {R_URANIUM});
    needU("Nuclear Warhead", {R_URANIUM});
    needU("War Galley", {R_COPPER});
    needU("Ironclad", {R_COAL});
    needU("River Gunboat", {R_COAL});
    needU("Salvaged Destroyer", {R_OIL});
    needU("Salvaged Battleship", {R_OIL});
    for (const char* n : {"Caravel", "Frigate", "Salvaged Freighter", "Ironclad", "Salvaged Destroyer", "Salvaged Battleship"}) g_units[unitIndex(n)].ocean = true;
    // transport capacity
    for (const char* n : {"Dugout Canoe", "War Galley", "Archer Galley", "Ironclad", "River Gunboat"}) g_units[unitIndex(n)].river = true;
    auto cargo = [](const char* u, int n) { g_units[unitIndex(u)].cargo = n; };
    cargo("Dugout Canoe", 1);
    cargo("War Galley", 1);
    cargo("Caravel", 2);
    cargo("Frigate", 1);
    cargo("Salvaged Freighter", 4);
    // upgrade lines (unique units follow the line of the unit they replace)
    auto upg = [](const char* a, const char* b) { g_units[unitIndex(a)].upgradeId = unitIndex(b); };
    upg("Warband", "Spear Troop");
    upg("Spear Troop", "Iron Blades");
    upg("Bronze Phalanx", "Iron Blades");
    upg("Iron Blades", "Salvaged Rifles");
    upg("Salvaged Rifles", "Riflemen");
    upg("Riflemen", "Infantry");
    upg("Infantry", "Exo-Primates");
    upg("Stone Thrower", "Bow Troop");
    upg("Bow Troop", "Crossbows");
    upg("Crossbows", "Field Guns");
    upg("Siege Sling", "Bombard");
    upg("Bombard", "Field Guns");
    upg("Beast Riders", "Armoured Riders");
    upg("Armoured Riders", "Rebuilt Tanks");
    upg("Dugout Canoe", "War Galley");
    upg("War Galley", "Caravel");
    upg("Caravel", "Ironclad");
    upg("Ironclad", "Salvaged Destroyer");
    upg("Archer Galley", "Frigate");
    upg("Frigate", "Salvaged Battleship");
    for (auto& u : g_units) if (u.replacesId >= 0 && u.needs.empty()) u.needs = g_units[u.replacesId].needs;
    needB("Factory", {R_COAL});
    needB("Power Station", {R_COAL});
    // global production pacing
    for (auto& u : g_units) if (u.cls != UC::Recon && u.cls != UC::Worker) u.cost = u.cost * 3 / 2;
    for (auto& b : g_buildings) if (b.special != BSpecial::Victory) b.cost = b.cost * 3 / 2;

    // ------------------------------------------------------------ traditions
    g_policies = {
        {"Grooming Circles", "+15% Culture", PolicyEffect::Culture, 15, ""},
        {"Communal Feeding", "+15% Food", PolicyEffect::Food, 15, ""},
        {"Craft Lore", "+15% Production", PolicyEffect::Prod, 15, ""},
        {"Elder Wisdom", "+15% Science", PolicyEffect::Sci, 15, ""},
        {"Alpha Hierarchy", "+15% combat strength", PolicyEffect::Combat, 15, ""},
        {"Scavenger Code", "+50% ruin rewards", PolicyEffect::RuinBonus, 50, ""},
        {"Nomad Spirit", "Elder Troops cost 33% less", PolicyEffect::SettlerCost, 33, ""},
        {"Fortified Nests", "+50% city defence", PolicyEffect::CityDefense, 50, ""},
        {"Spreading Canopy", "Borders expand 50% faster", PolicyEffect::Borders, 50, ""},
        {"Warrior Bands", "Military units cost 20% less", PolicyEffect::UnitCost, 20, ""},
        {"Merchant Troops", "+30% Gold", PolicyEffect::Gold, 30, "Currency"},
        {"Free Thought", "+3 Science in every city", PolicyEffect::FlatSci, 3, "Philosophy"},
        {"Festival Songs", "+3 Culture in every city", PolicyEffect::FlatCult, 3, "Theology"},
        {"Mutual Grooming", "+1 Happiness in every city", PolicyEffect::Happy, 1, ""},
        {"Public Festivals", "+2 Happiness in every city", PolicyEffect::Happy, 2, "Construction"},
        {"Rally Calls", "War weariness builds half as fast", PolicyEffect::Weariness, 50, "Mass Media"},
        {"Field Medicine", "Units heal +10 HP per turn", PolicyEffect::Heal, 10, "Radiology"},
        {"Industrial Will", "+20% Production", PolicyEffect::Prod, 20, "Industrialisation"},
        {"Open Archives", "+25% Science", PolicyEffect::Sci, 25, "Computing"},
    };
    for (auto& p : g_policies) p.techId = needTech(p.tech);
    // faith from the houses of worship
    g_buildings[buildingIndex("Shrine")].flat.faith = 2;
    g_buildings[buildingIndex("Temple")].flat.faith = 4;
    g_buildings[buildingIndex("Cathedral")].flat.faith = 6;

    // evolutionary aptitude: social and vocal species evolve language faster
    static const int EVO[] = {10, 20, 10, 0, 0, 10, 25, 0};
    static const int LEAN[] = {ST_TOOL, ST_SOCIAL, ST_TOOL, ST_PREDATOR, ST_PREDATOR, ST_TOOL, ST_SOCIAL, ST_MUTANT};
    for (size_t i = 0; i < g_primates.size() && i < 8; i++) {
        g_primates[i].evoPct = EVO[i];
        g_primates[i].evoLean = LEAN[i];
    }
    ID.tCooking = techIndex("Cooking");
    ID.uElder = unitIndex("Elder Troop");
    ID.uBand = unitIndex("Roaming Band");
    // diplomatic personalities
    auto persona = [](const char* name, const char* title, const char* desc, int friendly, int loyal, int greed, int terr, int pacts, bool ape) {
        for (auto& p : g_primates)
            if (p.name == name) {
                p.persona = title; p.personaDesc = desc;
                p.friendliness = friendly; p.loyalty = loyal; p.greed = greed; p.territorial = terr; p.pactLove = pacts; p.ape = ape;
            }
    };
    persona("Chimpanzee", "Territorial Politician", "Builds coalitions with whoever is useful, but guards its range jealously and never forgets a slight.",
            -2, 5, 6, 9, 6, true);
    persona("Bonobo", "Peacemaker", "Warm towards almost everyone. Loves agreements, hates war, and forgives quickly.", 10, 7, 3, 2, 8, true);
    persona("Orangutan", "Solitary Sage", "Polite but aloof. Trades knowledge happily, but rarely binds itself with pacts.", 1, 6, 4, 4, 2, true);
    persona("Gorilla", "Protector", "Slow to anger and fiercely loyal to friends. Despises warmongers and those who attack the weak.", 2, 9, 3, 6, 8, true);
    persona("Baboon", "Opportunist", "Respects only strength. Drives hard bargains, demands tribute, and may turn on a friend who looks weak.",
            -5, 2, 8, 8, 4, false);
    persona("Capuchin", "Trader", "Will deal with anyone. Values fair trades highly and grows fond of good trading partners.", 4, 5, 8, 3, 5, false);
    persona("Gibbon", "Faithful Partner", "Chooses one close partner and stays loyal for life. Distant with everyone else.", 0, 10, 4, 5, 9, true);
    persona("Japanese Macaque", "Hierarchist", "Ranks everyone by strength and status. Respects the powerful; looks down on the weak.", 0, 6, 5, 5, 5, false);
    // piety: how much each people cares for faith, and how much science it will give up for it
    for (auto& pp : std::vector<std::pair<const char*, int>>{{"Chimpanzee", 4}, {"Bonobo", 7}, {"Orangutan", 2}, {"Gorilla", 5}, {"Baboon", 6},
                                                             {"Capuchin", 3}, {"Gibbon", 8}, {"Japanese Macaque", 6}})
        for (auto& p : g_primates) if (p.name == pp.first) p.piety = pp.second;
    ID.uMissionary = unitIndex("Missionary");
    ID.uNuke = unitIndex("Nuclear Warhead");
    ID.tFission = techIndex("Nuclear Fission");
    ID.uGatherer = unitIndex("Gatherer");
    ID.uTinkerer = unitIndex("Tinkerer");
    ID.uScout = unitIndex("Scout");
    ID.uWarband = unitIndex("Warband");
    ID.bPalace = buildingIndex("Nest Hall");
    ID.bRebirth = buildingIndex("Rebirth Project");
    ID.tCurrency = techIndex("Currency");
    ID.tRafts = techIndex("Rafts");
    ID.tNavigation = techIndex("Navigation");
    ID.tRadiology = techIndex("Radiology");
    ID.tSalvaging = techIndex("Salvaging");
    ID.tDeciphering = techIndex("Deciphering");
    ID.tReverse = techIndex("Reverse Engineering");
    ID.tComputing = techIndex("Computing");
    ID.tRebirth = techIndex("Rebirth");
    ID.tCultivation = techIndex("Cultivation");
    ID.tForaging = techIndex("Foraging");
    ID.tLaw = techIndex("Tribal Law");
    ID.tBanking = techIndex("Banking");
    ID.tBartering = techIndex("Bartering");
    ID.uCaravan = unitIndex("Caravan");
    for (int k = 0; k <= GK_COUNT; k++) ID.uGreat[k] = unitIndex(GREAT_KIND_NAMES[k]);
    ID.tDigging = techIndex("Digging");
    ID.tWoodcraft = techIndex("Woodcraft");
    ID.tIndustrial = techIndex("Industrialisation");
    ID.tChemistry = techIndex("Chemistry");
}
