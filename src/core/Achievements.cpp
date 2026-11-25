// Rebirth of Civilisation - achievements
// SPDX-License-Identifier: MIT
#include "Achievements.h"
#include "Game.h"
#include <cstdio>
#include <ctime>
#include <fstream>
#include <sstream>

const std::vector<AchievementDef> g_achievements = {
    // the wandering age
    {"SPARK_OF_GENIUS", "Spark of Genius", "Discover Fire.", "The Wandering Age"},
    {"BIG_BRAIN_ENERGY", "Big Brain Energy", "Evolve the Neocortex.", "The Wandering Age"},
    {"USE_YOUR_WORDS", "Use Your Words", "Evolve Speech.", "The Wandering Age"},
    {"BORN_TO_ROAM", "Born to Roam", "Grow a roaming band to 8 members.", "The Wandering Age"},
    {"SEPARATE_WAYS", "Going Our Separate Ways", "Split a band in two.", "The Wandering Age"},
    {"HOME_SWEET_HOME", "Home Sweet Home", "Settle down and found your first city.", "The Wandering Age"},
    // evolution and the old world
    {"PEAK_PRIMATE", "Peak Primate", "Evolve a capstone trait.", "Evolution and the Old World"},
    {"WHAT_DOESNT_KILL_YOU", "What Doesn't Kill You", "Survive 5 disasters in one game.", "Evolution and the Old World"},
    {"DIG_IT", "Dig It", "Excavate a layer of a human ruin.", "Evolution and the Old World"},
    {"ARCHAEOLOGIST", "Archaeologist", "Excavate 25 ruin layers in one game.", "Evolution and the Old World"},
    {"CLEAN_UP_CREW", "Clean-Up Crew", "Decontaminate fallout 10 times in one game.", "Evolution and the Old World"},
    {"FRIENDS_IN_LOW_PLACES", "Friends in Low Places", "Be allied with 3 minor tribes at once.", "Evolution and the Old World"},
    {"GOOD_NEIGHBOURS", "Good Neighbours", "Complete 5 requests from minor tribes in one game.", "Evolution and the Old World"},
    {"SWEET_TOOTH", "Sweet Tooth", "Enjoy 6 different luxuries at once.", "Evolution and the Old World"},
    {"PARTY_ANIMALS", "Party Animals", "Have 4 cities celebrating at once.", "Evolution and the Old World"},
    {"NATURE_WATCH", "Nature Watch", "Discover 3 natural wonders in one game.", "Evolution and the Old World"},
    // building a civilisation
    {"MONEY_TALKS", "Money Talks", "Discover Currency.", "Civilisation"},
    {"GROWING_EMPIRE", "Growing Empire", "Control 6 cities at once.", "Civilisation"},
    {"CONCRETE_JUNGLE", "Concrete Jungle", "Grow a city to size 15.", "Civilisation"},
    {"WONDERFUL", "Wonderful", "Complete a wonder.", "Civilisation"},
    {"WONDERS_OF_THE_NEW_WORLD", "Wonders of the New World", "Own 5 wonders at once.", "Civilisation"},
    {"OLD_WAYS", "The Old Ways", "Adopt 8 traditions.", "Civilisation"},
    {"SILK_ROAD", "Silk Road", "Run 5 trade routes at once.", "Civilisation"},
    {"ALL_ROADS", "All Roads Lead Home", "Build a Tarmac Road.", "Civilisation"},
    {"HIGHWAY_ROBBERY", "Highway Robbery", "Plunder an enemy trade route.", "War"},
    // war
    {"GONE_BANANAS", "Gone Bananas", "Declare war.", "War"},
    {"SPOILS_OF_WAR", "Spoils of War", "Capture or raze a city.", "War"},
    {"WARLORD", "Warlord", "Capture or raze 10 cities in one game.", "War"},
    {"PARADISE_LOST", "Paradise Lost", "Lose a city.", "War"},
    {"SEA_LEGS", "Sea Legs", "Launch your first warship.", "War"},
    {"UP_THE_CREEK", "Up the Creek", "Sail boats 20 tiles along rivers in one game.", "War"},
    {"RULER_OF_THE_WAVES", "Ruler of the Waves", "Sink 10 enemy ships in one game.", "War"},
    {"ALL_ABOARD", "All Aboard", "Carry a land unit aboard a ship.", "War"},
    {"OUT_WITH_THE_OLD", "Out With the Old", "Upgrade a unit.", "War"},
    {"HERE_WE_GO_AGAIN", "Here We Go Again", "Detonate a nuclear weapon.", "War"},
    {"GLASS_HOUSES", "Glass Houses", "Have one of your cities hit by an enemy nuclear weapon.", "War"},
    {"MUTUALLY_ASSURED", "Mutually Assured Destruction", "In one game, nuke an enemy and be nuked yourself.", "War"},
    // diplomacy
    {"BEST_FRIENDS", "Best Friends", "Sign a defensive pact.", "Diplomacy"},
    {"MONKEY_BUSINESS", "Monkey Business", "Make 10 deals with other civilisations in one game.", "Diplomacy"},
    {"ET_TU", "Et Tu?", "Declare war on a civilisation you are sworn friends with.", "Diplomacy"},
    // victory
    {"BORN_AGAIN", "Born Again", "Win a Rebirth victory.", "Victory"},
    {"APEX_PRIMATE", "Apex Primate", "Win a Domination victory.", "Victory"},
    {"RENAISSANCE_APE", "Renaissance Ape", "Win a Cultural victory.", "Victory"},
    {"STILL_STANDING", "Still Standing", "Win on score when time runs out.", "Victory"},
    {"OUTSMARTED", "Outsmarted", "Win on Hard or Expert.", "Victory"},
    {"GRAND_MASTER", "Grand Master", "Win on Expert.", "Victory"},
    {"MAKE_LOVE_NOT_WAR", "Make Love, Not War", "Win without ever declaring war or launching a nuke.", "Victory"},
    {"EVOLUTION_REVOLUTION", "Evolution Revolution", "Win before 60% of the turns have passed.", "Victory"},
    {"FULL_TROOP", "Full Troop", "Win with every primate.", "Victory"},
};

const AchievementDef* findAchievement(const std::string& id) {
    for (auto& a : g_achievements) if (id == a.id) return &a;
    return nullptr;
}

bool AchievementStore::load(const std::string& path) {
    std::ifstream f(path);
    if (!f) return false;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        size_t eq = line.find('=');
        if (eq == std::string::npos || line[0] == '#') continue;
        std::string k = line.substr(0, eq), v = line.substr(eq + 1);
        if (k == "primates_won") {
            std::stringstream ss(v);
            std::string tok;
            while (std::getline(ss, tok, ',')) if (!tok.empty()) primatesWon.insert(std::atoi(tok.c_str()));
        } else if (findAchievement(k)) {
            unlocked[k] = v;
        }
    }
    return true;
}

bool AchievementStore::save(const std::string& path) const {
    if (path.empty()) return false;
    std::ofstream f(path, std::ios::trunc);
    if (!f) return false;
    f << "# Rebirth of Civilisation achievements\n";
    for (auto& a : g_achievements) {
        auto it = unlocked.find(a.id);
        if (it != unlocked.end()) f << a.id << "=" << it->second << "\n";
    }
    f << "primates_won=";
    bool first = true;
    for (int p : primatesWon) { f << (first ? "" : ",") << p; first = false; }
    f << "\n";
    return (bool)f;
}

static std::string today() {
    std::time_t t = std::time(nullptr);
    char buf[16];
    std::tm* tm = std::localtime(&t);
    if (!tm || !std::strftime(buf, sizeof(buf), "%Y-%m-%d", tm)) return "";
    return buf;
}

std::vector<std::string> checkAchievements(const Game& g, AchievementStore& st) {
    std::vector<std::string> out;
    if (g.settings.allAI || g.player < FIRST_PRIMATE_CIV || g.player >= (int)g.civs.size()) return out;
    int p = g.player;
    const Civ& cv = g.civs[p];
    std::string date;
    auto give = [&](const char* id, bool cond) {
        if (!cond || st.has(id)) return;
        if (date.empty()) date = today();
        st.unlocked[id] = date.empty() ? "yes" : date;
        out.push_back(id);
    };
    int cities = 0, biggest = 0, wonders = 0;
    bool founded = false;
    for (auto& ci : g.cities) {
        if (ci.origOwner == p) founded = true;
        if (!ci.alive || ci.owner != p) continue;
        cities++;
        biggest = std::max(biggest, ci.pop);
        for (size_t b = 0; b < g_buildings.size(); b++) if (ci.has[b] && g_buildings[b].wonder) wonders++;
    }
    bool capstone = false;
    for (size_t t = 0; t < g_traits.size(); t++) if (g_traits[t].capstone && g.hasTrait(p, (int)t)) capstone = true;

    give("SPARK_OF_GENIUS", g.hasTech(p, techIndex("Fire")));
    give("BIG_BRAIN_ENERGY", g.hasTrait(p, TR_NEOCORTEX));
    give("USE_YOUR_WORDS", g.hasTrait(p, TR_SPEECH));
    give("BORN_TO_ROAM", cv.maxBand >= Game::BAND_MAX);
    give("SEPARATE_WAYS", cv.bandSplits > 0);
    give("HOME_SWEET_HOME", founded);
    give("PEAK_PRIMATE", capstone);
    give("WHAT_DOESNT_KILL_YOU", cv.alive && cv.disastersSuffered >= 5);
    give("DIG_IT", cv.ruinsExcavated >= 1);
    give("ARCHAEOLOGIST", cv.ruinsExcavated >= 25);
    give("CLEAN_UP_CREW", cv.falloutCleaned >= 10);
    give("NATURE_WATCH", cv.naturalsFound >= 3);
    give("SWEET_TOOTH", cv.mostLuxuries >= 6);
    give("FRIENDS_IN_LOW_PLACES", cv.mostAllies >= 3);
    give("GOOD_NEIGHBOURS", cv.questsCompleted >= 5);
    give("PARTY_ANIMALS", cv.mostCelebrating >= 4);
    give("MONEY_TALKS", g.hasGold(p));
    give("GROWING_EMPIRE", cities >= 6);
    give("CONCRETE_JUNGLE", biggest >= 15);
    give("WONDERFUL", wonders >= 1);
    give("WONDERS_OF_THE_NEW_WORLD", wonders >= 5);
    give("OLD_WAYS", cv.policiesAdopted >= 8);
    give("SILK_ROAD", g.routesOf(p) >= 5);
    give("ALL_ROADS", cv.bestRoad >= ROAD_TARMAC);
    give("HIGHWAY_ROBBERY", cv.routesPlundered > 0);
    give("GONE_BANANAS", cv.warsDeclared > 0);
    give("SPOILS_OF_WAR", cv.citiesCaptured > 0);
    give("WARLORD", cv.citiesCaptured >= 10);
    give("PARADISE_LOST", cv.citiesLost > 0);
    bool ship = false;
    for (auto& u : g.units) if (u.alive && u.owner == p && g_units[u.type].naval()) ship = true;
    give("SEA_LEGS", ship);
    give("RULER_OF_THE_WAVES", cv.shipsSunk >= 10);
    give("UP_THE_CREEK", cv.riverSailed >= 20);
    give("ALL_ABOARD", cv.unitsFerried > 0);
    give("OUT_WITH_THE_OLD", cv.unitsUpgraded > 0);
    give("HERE_WE_GO_AGAIN", cv.nukesLaunched > 0);
    bool pact = false;
    for (auto& ag : g.agreements) if (ag.kind == AG_DEFENSIVE && ag.until > g.turn && (ag.a == p || ag.b == p)) pact = true;
    give("BEST_FRIENDS", pact);
    give("MONKEY_BUSINESS", cv.dealsMade >= 10);
    give("ET_TU", cv.backstabs > 0);
    give("GLASS_HOUSES", cv.timesNuked > 0);
    give("MUTUALLY_ASSURED", cv.nukesLaunched > 0 && cv.timesNuked > 0);

    if (g.gameOver && g.winner == p) {
        const std::string& v = g.victoryType;
        give("BORN_AGAIN", v == "Rebirth");
        give("APEX_PRIMATE", v == "Domination");
        give("RENAISSANCE_APE", v == "Cultural");
        give("STILL_STANDING", v == "Score");
        give("OUTSMARTED", g.settings.difficulty >= 2);
        give("GRAND_MASTER", g.settings.difficulty >= 3);
        give("MAKE_LOVE_NOT_WAR", cv.warsDeclared == 0 && cv.nukesLaunched == 0);
        give("EVOLUTION_REVOLUTION", g.turn <= g.maxTurns * 6 / 10);
        if (cv.primate >= 0) st.primatesWon.insert(cv.primate);
        give("FULL_TROOP", (int)st.primatesWon.size() >= (int)g_primates.size());
    }
    return out;
}
