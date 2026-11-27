// Rebirth of Civilisation - achievements
//
// Achievements belong to the player's profile, not to a saved game: they are kept in a small
// text file next to the saves and survive between games. The ids are stable API names, so a
// future Steam (or other store) build can map each one to the store's own achievement.
// SPDX-License-Identifier: MIT
#pragma once
#include <map>
#include <set>
#include <string>
#include <vector>

class Game;

struct AchievementDef {
    const char* id;     // stable API name, e.g. "HERE_WE_GO_AGAIN"
    const char* name;
    const char* desc;
    const char* group;  // for the achievements screen
    bool secret = false;  // shown only as "???" until earned
};

extern const std::vector<AchievementDef> g_achievements;
const AchievementDef* findAchievement(const std::string& id);

struct AchievementStore {
    std::map<std::string, std::string> unlocked;  // id -> date unlocked (YYYY-MM-DD)
    std::set<int> primatesWon;                    // primates you have won with, across all games

    bool has(const std::string& id) const { return unlocked.count(id) > 0; }
    bool load(const std::string& path);
    bool save(const std::string& path) const;
};

// Look at the player's game and record anything newly earned. Returns the ids unlocked by this
// call, in display order. Games where every civilisation is computer-controlled earn nothing.
std::vector<std::string> checkAchievements(const Game& g, AchievementStore& store);
