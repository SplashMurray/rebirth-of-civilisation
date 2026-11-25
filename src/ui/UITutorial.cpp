// Rebirth of Civilisation - the tutorial
// A guide that walks a new player through the first game one step at a time: each step explains one
// idea, points at the control or the unit concerned, and moves on by itself once the player has done
// what it asks (or when they press Next). Its progress is saved with the game.
// SPDX-License-Identifier: MIT
#include "GameUI.h"
#include <algorithm>
#include <cmath>

namespace {
const int TOP = 36;  // matches GameUI.cpp
const Color C_TEXT = {234, 238, 242};
const Color C_DIM = {150, 160, 174};
const Color C_GOLD = {240, 200, 70};
}  // namespace

enum TutStep {
    TS_WELCOME, TS_SELECT_BAND, TS_MOVE_BAND, TS_END_TURN, TS_SCOUT, TS_FALL, TS_OPEN_EVO, TS_EVO_EXPLAINED, TS_WANDER, TS_RESEARCH,
    TS_TOWARD_SETTLING, TS_FOUND_CITY, TS_PRODUCTION, TS_GATHERER, TS_HAPPINESS, TS_TRADITIONS, TS_NEIGHBOURS, TS_RUINS_TRADE,
    TS_ADVISORS, TS_WINNING, TS_COUNT
};

void GameUI::markRect(const std::string& id, int x, int y, int w, int h) { uiRects[id] = {x, y, w, h}; }

int GameUI::tutorialSteps() const { return TS_COUNT; }

// What the current step says, what it points at, and whether it is done.
GameUI::TutInfo GameUI::tutorialInfo() {
    Game& g = *game;
    const Civ& me = g.civs[g.player];
    TutInfo t;
    int step = g.tutorialStep;
    const Unit* band = nullptr;
    const Unit* scout = nullptr;
    for (auto& u : g.units) {
        if (!u.alive || u.owner != g.player) continue;
        if (!band && g_units[u.type].cls == UnitClass::Band) band = &u;
        if (!scout && g_units[u.type].cls == UnitClass::Recon) scout = &u;
    }
    const City* firstCity = nullptr;
    for (auto& c : g.cities) if (c.alive && c.owner == g.player) { firstCity = &c; break; }
    int explored = 0;
    for (auto e : me.explored) explored += e;
    if (tutRefStep != step) {  // remember how things stood when this step began
        tutRefStep = step;
        tutRefTurn = g.turn;
        tutRefX = band ? band->x : -1;
        tutRefY = band ? band->y : -1;
        tutRefCount = explored;
    }
    const PrimateDef& pd = g_primates[me.primate];
    auto pointAtUnit = [&](const Unit* u) { if (u) { t.tileX = u->x; t.tileY = u->y; } };
    switch (step) {
        case TS_WELCOME:
            t.title = "Welcome";
            t.text = "You lead the " + pd.plural + ". It is 2032: humanity still rules the Earth, but not for long. This guide will walk you through "
                     "the first steps of a game. Each step moves on by itself once you have done what it asks - or press Next.\n\n"
                     "Drag the map or use the arrow keys to look around; the mouse wheel zooms. Hover over anything to learn more.";
            t.canNext = true;
            break;
        case TS_SELECT_BAND:
            t.title = "Your Roaming Band";
            t.text = "Your people begin as nomads: a Roaming Band of " + std::string(band ? std::to_string(band->band) : "a few") +
                     " members. Click the band to select it. Its panel shows what it can do.";
            pointAtUnit(band);
            t.done = !band || (sel() && g_units[sel()->type].cls == UnitClass::Band);
            break;
        case TS_MOVE_BAND:
            t.title = "Moving";
            t.text = "Right-click a tile to move there - a path shows how many turns it will take. A band forages the land it stands on and around "
                     "it; picked-over land takes time to recover, so keep moving to fresh forest, jungle and grassland. Stay away from the human "
                     "cities.";
            pointAtUnit(band);
            t.done = !band || band->x != tutRefX || band->y != tutRefY;
            break;
        case TS_END_TURN:
            t.title = "Ending the turn";
            t.text = "When your units have moved, end the turn with the button at the bottom right (or press Enter). If something needs your "
                     "attention first, the button says so - click it to go there.";
            t.rect = "endturn";
            t.done = g.turn > tutRefTurn;
            break;
        case TS_SCOUT:
            t.title = "Exploring";
            t.text = "Your Scout moves fast and sees far. Select it and send it into the unknown: you will find food, resources, ruins, other "
                     "species and the minor tribes.";
            pointAtUnit(scout);
            t.done = !scout || explored >= tutRefCount + 25;
            break;
        case TS_FALL:
            t.title = "The Fall";
            if (g.warHappened)
                t.text = "Humanity is gone, destroyed by its own war. Its cities are ruins glowing with fallout: keep your people out until it "
                         "fades. In time those ruins will reveal the knowledge of the lost civilisation to those clever enough to dig.";
            else
                t.text = "Listen to the human news in your notifications. In August 2034 (turn 6) the humans will destroy themselves, and the "
                         "world will belong to the primates. Keep ending turns.";
            t.canNext = g.warHappened;
            t.done = g.warHappened && g.turn >= PROLOGUE_TURNS + 4;  // gives the news a couple of turns to sink in
            break;
        case TS_OPEN_EVO:
            t.title = "Evolution";
            t.text = "Your species cannot yet learn deliberately: it earns no science until its brain evolves. Open the Evolution screen "
                     "(Evo on the top bar, or press V).";
            t.rect = "evo";
            t.done = overlay == OV_EVO;
            break;
        case TS_EVO_EXPLAINED:
            t.title = "Evolutionary pressure";
            t.text = "Traits are earned through four kinds of pressure - Social, Ecological, Conflict and Mutation - that build up as your "
                     "people live, roam, fight and suffer. Your goal is already set to the Enlarged Neocortex, the first step to science. Later "
                     "you choose which strand to follow: Brain and Language leads to Speech, which you need to settle. Close this screen when "
                     "you are ready.";
            t.canNext = true;
            t.done = overlay != OV_EVO;
            break;
        case TS_WANDER: {
            t.title = "Wandering";
            int tt = me.evoGoal >= 0 ? g.evolveTurns(g.player, me.evoGoal) : -1;
            t.text = "Keep your band moving to fresh food and end turns. Well-fed bands grow; a band of 6 or more can split in two (G) or raise a "
                     "war party (R). Feral beasts roam the world - keep away from them, or fight them with your warband.";
            if (me.evoGoal >= 0)
                t.text += "\n\nEvolving " + g_traits[me.evoGoal].name + (tt >= 0 ? ": about " + std::to_string(tt) + " turns." : ": stalled - see Evolution.");
            t.canNext = true;
            t.done = g.hasTrait(g.player, TR_NEOCORTEX);
            break;
        }
        case TS_RESEARCH:
            t.title = "Research";
            t.text = "Your people can learn at last. Open the Technology tree (Tech, or T) and choose a discovery - Fire, Foraging and Stone Tools "
                     "come first. The tree shows what each technology unlocks.";
            t.rect = "tech";
            t.done = me.research >= 0;
            break;
        case TS_TOWARD_SETTLING: {
            t.title = "Towards settling down";
            bool speech = g.hasTrait(g.player, TR_SPEECH), cult = g.hasTech(g.player, ID.tCultivation);
            t.text = "To found a city your species needs Speech (Evolution) and Cultivation (Technology). ";
            t.text += speech ? "Speech: evolved. " : "Speech: not yet. ";
            t.text += cult ? "Cultivation: discovered." : "Cultivation: not yet.";
            if (!speech && me.evoGoal < 0) {
                t.text += "\n\nYour species is not evolving anything: open Evolution (V) and choose the next trait on the Brain and Language "
                          "strand - it leads to Speech.";
                t.rect = "evo";
            } else if (!cult && me.research >= 0 && me.research != ID.tCultivation) {
                t.text += "\n\nTip: set Cultivation as your research goal in the Technology tree.";
            }
            t.text += "\n\nMeanwhile, look for a good home: grassland or plains with food nearby, a river, lake or oasis (fresh water: +1 food), "
                      "the coast, and luxuries such as Honey or Grapes to keep your people happy.";
            t.canNext = true;
            t.done = g.canSettle(g.player);
            break;
        }
        case TS_FOUND_CITY:
            t.title = "Your first city";
            t.text = "Your people can settle. Move a band to a good spot and press B (or the Settle button) to found a city. A bigger band "
                     "founds a bigger city. Cities cannot be founded within 3 tiles of another.";
            pointAtUnit(band);
            t.done = firstCity != nullptr;
            break;
        case TS_PRODUCTION:
            t.title = "Building";
            t.text = "Click your city to open it. Choose what it builds from the list: warriors to defend it, a Gatherer to improve the land, "
                     "buildings for food, science and happiness, and later Elder Troops to found more cities.";
            if (firstCity) { t.tileX = firstCity->x; t.tileY = firstCity->y; }
            t.done = false;
            for (auto& c : g.cities) if (c.alive && c.owner == g.player && c.buildKind != BK_NONE) t.done = true;
            break;
        case TS_GATHERER: {
            t.title = "Improving the land";
            t.text = "Gatherers build farms, mines, lumber camps, pastures and trading posts on the tiles your cities work, and roads between "
                     "your cities. Select a Gatherer on a tile in your borders to see what it can build there.";
            t.canNext = true;
            t.done = false;
            for (auto& u : g.units) if (u.alive && u.owner == g.player && g_units[u.type].cls == UnitClass::Worker) t.done = true;
            break;
        }
        case TS_HAPPINESS:
            t.title = "Growth and happiness";
            t.text = "Cities grow with surplus food. Each citizen beyond the first 5 wants something to be happy about: luxuries (each kind adds "
                     "1 happiness in every city), Totems, Shrines and Temples. A city grows only while it has happiness to spare; the face by "
                     "its name shows its mood, and the Luxuries count on the top bar warns you of trouble.";
            t.canNext = true;
            break;
        case TS_TRADITIONS:
            t.title = "Traditions";
            t.text = "Culture accumulates every turn. When you have enough, adopt a Tradition (Trad, or P): each gives a lasting bonus, and "
                     "adopting 16 wins a Cultural victory.";
            t.rect = "trad";
            t.canNext = true;
            t.done = me.policiesAdopted >= 1;
            break;
        case TS_NEIGHBOURS:
            t.title = "Neighbours";
            t.text = "Other species share the world. On the Diplomacy screen (D) you can trade, sign pacts or declare war. The minor tribes "
                     "(Tribes, or I) give gifts to their friends - win them over with gold, trade routes and help.";
            t.rect = "diplo";
            t.canNext = true;
            t.done = overlay == OV_DIPLO || overlay == OV_TRIBES;
            break;
        case TS_RUINS_TRADE:
            t.title = "Ruins and trade";
            t.text = "Human ruins hold the knowledge of the old world. With Salvaging (and later Deciphering and Computing) move a unit onto "
                     "a ruin and press E to excavate it. With Currency your cities train Caravans: send them to other cities to set up trade "
                     "routes that pay every turn and lay roads.";
            t.canNext = true;
            break;
        case TS_ADVISORS:
            t.title = "Your advisors";
            t.text = "Four advisors watch over your people and suggest what to do next. Open them any time (Advisors, or A). With advisor "
                     "tips on, they also speak up when something needs you.";
            t.rect = "advisors";
            t.canNext = true;
            t.done = overlay == OV_ADVISORS;
            break;
        case TS_WINNING:
            t.title = "Winning";
            t.text = "Win by building the Rebirth Project (the last technology), adopting 16 Traditions, or taking every original capital. "
                     "If nobody has won when time runs out, the highest score wins. Press F1 at any time for the full guide. Good luck!";
            t.canNext = true;
            t.nextLabel = "Finish";
            break;
        default: break;
    }
    return t;
}

void GameUI::tutorialAdvance() {
    Game& g = *game;
    if (g.tutorialStep < 0) return;
    g.tutorialStep++;
    if (g.tutorialStep >= TS_COUNT) {
        g.tutorialStep = -1;
        g.notify(g.player, "Tutorial complete. Press F1 for the full guide, or A for your advisors.");
    }
    tutRefStep = -1;
}

void GameUI::drawHighlight(int x, int y, int w, int h) {
    double pulse = 0.5 + 0.5 * std::sin(animMs / 220.0);
    int pad = 3 + (int)(pulse * 4);
    cv->roundRect(x - pad, y - pad, w + pad * 2, h + pad * 2, 8, rgb(255, 214, 90), 3, 120 + (int)(pulse * 135));
    cv->roundRect(x - pad - 3, y - pad - 3, w + pad * 2 + 6, h + pad * 2 + 6, 10, rgb(255, 240, 180), 1, 60 + (int)(pulse * 60));
}

void GameUI::renderTutorial() {
    Game& g = *game;
    if (g.tutorialStep < 0 || g.tutorialStep >= TS_COUNT || !g.popups.empty() || nukeT >= 0) return;
    TutInfo t = tutorialInfo();
    if (t.done) {  // done: on to the next step (one per frame, so a step already done is skipped quickly)
        tutorialAdvance();
        return;
    }
    // point at what the step is about
    if (!t.rect.empty() && uiRects.count(t.rect)) {
        const UIRect& r = uiRects[t.rect];
        drawHighlight(r.x, r.y, r.w, r.h);
    } else if (t.tileX >= 0 && overlay == OV_NONE) {
        int sx, sy;
        tileCenter(t.tileX, t.tileY, sx, sy);
        double pulse = 0.5 + 0.5 * std::sin(animMs / 220.0);
        int r = hs + 4 + (int)(pulse * 6);
        cv->circle(sx, sy, r, blend(rgb(150, 110, 40), rgb(255, 214, 90), (float)pulse), 3);
        cv->circle(sx, sy, r + 5, rgb(255, 240, 180), 1);
    }
    // the panel
    int pw = std::min(380, W / 3), px = 10, py = TOP + 10;
    if (overlay != OV_NONE && overlay != OV_EVO) py = H - 260;  // out of the way of open screens
    if (overlay == OV_EVO) { px = W - pw - 14; py = H - 300; }
    auto lines = wrap(t.text, pw - 28, 13);
    int ph = 56 + (int)lines.size() * 17 + 44;
    cv->fillRoundRectGrad(px + 3, py + 4, pw, ph, 10, rgb(0, 0, 0), rgb(0, 0, 0), 90);
    cv->fillRoundRectGrad(px, py, pw, ph, 10, rgb(52, 44, 30), rgb(26, 22, 16), 245);
    cv->roundRect(px, py, pw, ph, 10, rgb(220, 180, 100), 2);
    hot(px, py, pw, ph, nullptr);
    cv->text(px + 14, py + 10, "TUTORIAL  " + std::to_string(g.tutorialStep + 1) + " / " + std::to_string(TS_COUNT), 11, rgb(220, 180, 100), AL_LEFT, true);
    cv->text(px + 14, py + 26, t.title, 17, C_GOLD, AL_LEFT, true);
    int y = py + 52;
    for (auto& l : lines) { cv->text(px + 14, y, l, 13, C_TEXT); y += 17; }
    int by = py + ph - 36;
    button(px + 14, by, 120, 26, "Skip tutorial", true, [this] { game->tutorialStep = -1; tutRefStep = -1; }, "Turn the tutorial off for this game", rgb(60, 54, 44), 12);
    if (t.canNext)
        button(px + pw - 104, by, 90, 26, t.nextLabel, true, [this] { tutorialAdvance(); }, "", rgb(70, 110, 60), 13);
    else
        cv->text(px + pw - 14, by + 6, "Do this to go on", 11, C_DIM, AL_RIGHT);
}
