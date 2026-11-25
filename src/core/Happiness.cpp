// Rebirth of Civilisation - happiness and luxuries
// Every city has a mood. Its first five citizens are content with nothing; each one beyond that wants
// something more, as do the citizens of a sprawling empire and a nation weary of war. Luxuries (each
// kind you own or are lent cheers every city), buildings, wonders, traditions and natural wonders in
// the city's lands make them happy.
//   happy - unhappy >= 3 (size 3+): celebrating, +10% Food and Gold
//   0 .. 2                       : content (a city grows only while it has at least 1 happiness to spare)
//   -1 .. -2                     : unhappy, the city stops growing
//   <= -3                        : unrest, no growth and -25% Production, Science, Culture and Gold
// SPDX-License-Identifier: MIT
#include "Game.h"
#include <algorithm>

std::vector<int> Game::luxuriesOf(int c) const {
    std::vector<int> out;
    if (c < 0 || c >= (int)civs.size()) return out;
    for (int r = 1; r < R_COUNT; r++)
        if (g_resources[r].kind == RK_LUXURY && hasResource(c, r)) out.push_back(r);
    return out;
}

MoodParts Game::moodParts(const City& city) const {
    MoodParts m;
    int c = city.owner;
    if (!isMajor(c)) return m;
    m.luxuries = luxuryCount(c);
    for (size_t b = 0; b < g_buildings.size(); b++)
        if (city.has[b]) (g_buildings[b].wonder ? m.wonders : m.buildings) += g_buildings[b].happy;
    if (ownsWonderSpecial(c, BSpecial::HappyAll)) m.wonders += 1;
    for (auto& s : naturals)
        if (at(s.x, s.y).city == city.id && at(s.x, s.y).owner == c) m.naturals++;
    m.traditions = policyPct(c, PolicyEffect::Happy);
    m.crowding = std::max(0, city.pop - CONTENT_BASE);
    m.empire = std::max(0, (numCities(c) - 1) / 4);
    m.war = std::min(4, civs[c].weariness / 25);
    return m;
}

int Game::moodOf(const City& city) const {
    int d = city.happy - city.unhappy;
    if (d <= -3) return MOOD_UNREST;
    if (d < 0) return MOOD_UNHAPPY;
    if (d >= 3 && city.pop >= 3) return MOOD_CELEBRATING;
    return MOOD_CONTENT;
}

const char* Game::moodName(int mood) {
    switch (mood) {
        case MOOD_UNREST: return "Unrest";
        case MOOD_UNHAPPY: return "Unhappy";
        case MOOD_CELEBRATING: return "Celebrating";
        default: return "Content";
    }
}

void Game::addWeariness(int c, int amount) {
    if (c < FIRST_PRIMATE_CIV || c >= (int)civs.size()) return;
    int pct = 100 - policyPct(c, PolicyEffect::Weariness);
    civs[c].weariness = std::min(100, civs[c].weariness + std::max(1, amount * pct / 100));
}

// War wears a people down; peace lets them recover.
void Game::wearinessTurn(int c) {
    Civ& cv = civs[c];
    bool war = false;
    for (int o = FIRST_PRIMATE_CIV; o < (int)civs.size(); o++)
        if (o != c && civs[o].alive && cv.war[o] && numCities(o) > 0) war = true;
    if (war && numCities(c) > 0 && turn % 2 == 0) addWeariness(c, 1);
    else cv.weariness = std::max(0, cv.weariness - 3);
    // records for achievements
    int lux = luxuryCount(c), party = 0;
    for (auto& ci : cities) if (ci.alive && ci.owner == c && moodOf(ci) == MOOD_CELEBRATING) party++;
    cv.mostLuxuries = std::max(cv.mostLuxuries, lux);
    cv.mostCelebrating = std::max(cv.mostCelebrating, party);
}

void Game::reportMood(City& city) {
    int now = moodOf(city), was = city.moodWas;
    city.moodWas = (int8_t)now;
    if (now == was || city.owner < FIRST_PRIMATE_CIV) return;
    std::string msg;
    if (now == MOOD_UNREST) msg = city.name + " is in unrest! It will not grow and its output has fallen. Find luxuries or build a Temple or Arena.";
    else if (now == MOOD_UNHAPPY && was > now) msg = city.name + " is unhappy and has stopped growing.";
    else if (now == MOOD_CELEBRATING) msg = city.name + " is celebrating!";
    else if (was == MOOD_UNREST) msg = "Order has returned to " + city.name + ".";
    else if (was == MOOD_UNHAPPY && now == MOOD_CONTENT) msg = city.name + " is content again and growing.";
    if (!msg.empty()) notify(city.owner, msg, city.x, city.y);
}
