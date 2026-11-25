// Rebirth of Civilisation - what the game sounds like: which effect, music and ambience when
// SPDX-License-Identifier: MIT
#include "GameUI.h"
#include <algorithm>

namespace {
const Color C_TEXT = {234, 238, 242};
const Color C_GOLD = {240, 200, 70};
const Color C_PANEL2 = {38, 46, 60};
}  // namespace

void GameUI::sfx(Sfx s, float vol, int screenX, int delayMs) {
    float pan = screenX < 0 || W <= 0 ? 0.0f : std::max(-0.8f, std::min(0.8f, (screenX - W / 2) / (float)(W / 2)));
    snd->play(s, vol, pan, delayMs);
}

void GameUI::sfxAt(Sfx s, int tx, int ty, float vol, int delayMs) {
    int sx, sy;
    tileCenter(tx, ty, sx, sy);
    if (sx < -200 || sx > W + 200 || sy < -200 || sy > H + 200) vol *= 0.35f;  // off screen: distant
    sfx(s, vol, sx, delayMs);
}

// A call of the species that civ c is, from tile (tx, ty) or the middle of the screen.
void GameUI::primateCall(int c, int tx, int ty, float vol, int delayMs) {
    if (!game || c < 0 || c >= (int)game->civs.size() || game->civs[c].primate < 0) return;
    int sx = W / 2, sy;
    if (tx >= 0) tileCenter(tx, ty, sx, sy);
    float pan = W > 0 ? std::max(-0.8f, std::min(0.8f, (sx - W / 2) / (float)(W / 2))) : 0.0f;
    snd->call(game->civs[c].primate, vol, pan, delayMs);
}

void GameUI::selectSound(const Unit& u) {
    const UnitDef& d = g_units[u.type];
    Sfx s = d.naval() ? Sfx::Ship : d.cls == UnitClass::Mounted ? Sfx::Horse : d.cls == UnitClass::Siege ? Sfx::SiegeSelect : Sfx::Step;
    sfxAt(s, u.x, u.y, d.cls == UnitClass::Mounted ? 0.45f : 0.6f);
    // the troop answers: always for a band, now and then for others (so it never grates)
    bool primateUnit = !d.naval() && d.cls != UnitClass::Siege && d.cls != UnitClass::Armor && d.cls != UnitClass::Nuke;
    if (primateUnit && (d.cls == UnitClass::Band || callCooldownMs <= 0)) {
        primateCall(u.owner, u.x, u.y, 0.5f, 60);
        callCooldownMs = 4000;
    }
}

void GameUI::moveSound(const Unit& u) {
    if (moveSndMs > 0) return;  // one footstep at a time
    moveSndMs = 180;
    const UnitDef& d = g_units[u.type];
    const Tile& t = game->at(u.x, u.y);
    Sfx s = d.naval() || game->isWater(u.x, u.y) ? Sfx::ShipMove
          : t.terrain == T_SNOW || t.terrain == T_TUNDRA ? Sfx::StepSnow
          : t.terrain == T_DESERT || t.terrain == T_PLAINS ? Sfx::StepSand : Sfx::Step;
    sfxAt(s, u.x, u.y, 0.55f);
}

// The sounds of one attack: the blow or the shot, then the hit, then a death.
void GameUI::combatSound(const CombatFx& c) {
    Game& g = *game;
    const UnitDef& d = g_units[c.atkType];
    int era = d.techId >= 0 ? g_techs[d.techId].era : 0;
    bool mine = c.atkOwner == g.player;
    for (auto& u : g.units)
        if (u.alive && u.owner == g.player && u.x == c.tx && u.y == c.ty) mine = true;
    if (const City* ci = g.cityAt(c.tx, c.ty)) mine = mine || ci->owner == g.player;
    if (mine) lastCombatTurn = g.turn;
    float v = mine ? 1.0f : 0.6f;
    int hit = c.ranged ? 340 : 170;  // matches the animation
    Sfx shot, impact;
    if (d.primate == -2) { shot = Sfx::Beast; impact = Sfx::Death; }
    else if (d.cls == UnitClass::Siege) { shot = era >= 5 ? Sfx::Cannon : (d.name.find("Sling") != std::string::npos ? Sfx::Catapult : Sfx::Ballista); impact = Sfx::SiegeHit; }
    else if (d.naval()) { shot = d.ranged() ? (era >= 5 ? Sfx::Cannon : Sfx::Bow) : Sfx::ShieldHit; impact = d.ranged() ? (era >= 5 ? Sfx::SiegeHit : Sfx::ArrowHit) : Sfx::SwordHit; }
    else if (d.ranged()) {
        bool sling = d.name.find("Stone") != std::string::npos || d.name.find("Sling") != std::string::npos;
        shot = era >= 5 ? Sfx::Musket : sling ? Sfx::Sling : Sfx::Bow;
        impact = era >= 5 ? Sfx::ShieldHit : Sfx::ArrowHit;
    } else if (era >= 5 && d.cls != UnitClass::Mounted) { shot = Sfx::Musket; impact = Sfx::ShieldHit; }
    else {
        shot = d.name.find("Spear") != std::string::npos || d.name.find("Phalanx") != std::string::npos ? Sfx::Spear : Sfx::Sword;
        impact = Sfx::SwordHit;
        if (d.cls == UnitClass::Mounted) sfxAt(Sfx::Horse, c.ax, c.ay, v * 0.6f);
        if (c.atkOwner >= FIRST_PRIMATE_CIV && callCooldownMs <= 0 && (c.seq & 1)) {  // a war cry, sometimes
            primateCall(c.atkOwner, c.ax, c.ay, v * 0.45f);
            callCooldownMs = 2500;
        }
    }
    sfxAt(shot, c.ax, c.ay, v);
    sfxAt(impact, c.tx, c.ty, v * 0.9f, hit);
    if (c.defDied) {
        if (c.city) sfxAt(Sfx::Collapse, c.tx, c.ty, v * 0.7f, hit + 150);
        else if (g.isWater(c.tx, c.ty)) sfxAt(Sfx::ShipSinks, c.tx, c.ty, v, hit + 100);
        else sfxAt(Sfx::Death, c.tx, c.ty, v, hit + 120);
    }
    if (c.attDied) sfxAt(d.naval() ? Sfx::ShipSinks : d.cls == UnitClass::Mounted ? Sfx::HorseDeath : Sfx::Death, c.ax, c.ay, v, hit + 200);
}

void GameUI::buildSound(int imp) {
    sfx(imp == IMP_FARM || imp == IMP_PASTURE ? Sfx::Hoe : imp == IMP_MINE || imp == IMP_ROAD ? Sfx::Dig : Sfx::Wood, 0.7f);
}

// Called every tick: turn game events into sounds, choose the music, mix the ambience.
void GameUI::updateSound(int ms) {
    if (moveSndMs > 0) moveSndMs -= ms;
    if (callCooldownMs > 0) callCooldownMs -= ms;
    if (sndNotifyMs > 0) sndNotifyMs -= ms;
    if (screen != SCR_GAME || !game) {
        snd->setMood(Mood::Title);
        snd->setAmbient(AMB_WAVES, 0.0f);
        snd->setAmbient(AMB_BIRDS, 0.0f);
        snd->setAmbient(AMB_JUNGLE, 0.35f);
        snd->setAmbient(AMB_WIND, 0.25f);
        sndGame = nullptr;
        return;
    }
    Game& g = *game;
    const Civ& me = g.civs[g.player];
    int techs = 0, traits = 0;
    for (auto t : me.techs) techs += t;
    for (auto t : me.traits) traits += t;
    int cities = g.numCities(g.player), routes = g.routesOf(g.player);
    if (sndGame != game.get()) {  // a new or loaded game: no fanfare for what it already has
        sndGame = game.get();
        sndTechs = techs; sndTraits = traits; sndCities = cities; sndRoutes = routes; sndPolicies = me.policiesAdopted;
        sndAch = (int)achBanners.size();
        sndPopup.clear();
        lastCombatTurn = -1000;
    }
    if (techs > sndTechs) sfx(Sfx::Tech, 0.7f);
    if (traits > sndTraits) sfx(Sfx::Evolve, 0.8f);
    if (cities > sndCities) {
        sfx(Sfx::CityFounded, 0.8f);
        primateCall(g.player, -1, -1, 0.6f, 500);  // the troop celebrates
    }
    if (routes > sndRoutes) sfx(g.routes.empty() || !g.routes.back().sea ? Sfx::Trade : Sfx::TradeSea, 0.8f);
    if (me.policiesAdopted > sndPolicies) sfx(Sfx::Culture, 0.6f);
    sndTechs = techs; sndTraits = traits; sndCities = cities; sndRoutes = routes; sndPolicies = me.policiesAdopted;
    if ((int)achBanners.size() > sndAch) sfx(Sfx::Achievement, 0.8f);
    sndAch = (int)achBanners.size();
    // a new popup
    std::string front = g.popups.empty() ? std::string() : std::to_string(g.popups.front().kind) + g.popups.front().title;
    if (!front.empty() && front != sndPopup) {
        switch (g.popups.front().kind) {
            case POP_WAR: sfx(Sfx::War, 0.9f); break;
            case POP_VICTORY: sfx(Sfx::Victory, 0.9f); break;
            case POP_DEFEAT: sfx(Sfx::Defeat, 0.9f); break;
            case POP_DISASTER: sfx(Sfx::Thunder, 0.7f); break;
            case POP_NATURAL: sfx(Sfx::Culture, 0.8f); break;
            case POP_NUKE: break;  // the explosion speaks for itself
            default: if (sndNotifyMs <= 0) sfx(Sfx::Notify, 0.5f); break;
        }
        sndNotifyMs = 400;
    }
    sndPopup = front;
    // the bombs
    if (nukeT >= 0 && sndNuke < 0) {
        sfx(Sfx::Nuke, 1.0f);
        sfx(Sfx::Thunder, 0.8f, -1, 900);
        sfx(Sfx::Collapse, 0.7f, -1, 1600);
    }
    sndNuke = nukeT;
    if (blastT >= 0 && sndBlast < 0) {
        sfxAt(Sfx::Nuke, blastX, blastY, 1.0f);
        sfxAt(Sfx::Collapse, blastX, blastY, 0.7f, 700);
    }
    sndBlast = blastT;

    // music
    bool atWar = false;
    for (int o = FIRST_PRIMATE_CIV; o < (int)g.civs.size(); o++)
        if (o != g.player && g.civs[o].alive && me.war[o]) atWar = true;
    Mood mood;
    if (g.gameOver) mood = g.winner == g.player ? Mood::Victory : Mood::Defeat;
    else if (nukeT >= 0 || (!g.popups.empty() && g.popups.front().kind == POP_NUKE)) mood = Mood::Fall;
    else {
        int era = g.era(g.player);
        if (atWar && g.turn - lastCombatTurn <= 8) mood = era >= 6 ? Mood::ModernWar : Mood::War;
        else mood = cities == 0 || era <= 1 ? Mood::Wander : era <= 4 ? Mood::Ancient : era == 5 ? Mood::Later : era == 6 ? Mood::Industrial : Mood::Modern;
    }
    snd->setMood(mood);

    // ambience from what is on screen, twice a second
    sndAmbMs -= ms;
    if (sndAmbMs > 0) return;
    sndAmbMs = 500;
    const double SQ3 = 1.7320508075688772;
    int rowH = std::max(1, (int)(hs * 1.5));
    double colW = hs * SQ3;
    int y0 = std::max(0, (int)(camY / rowH)), y1 = std::min(g.H - 1, (int)((camY + H) / rowH));
    int x0 = std::max(0, (int)(camX / colW)), x1 = std::min(g.W - 1, (int)((camX + W) / colW));
    int n = 0, water = 0, jungle = 0, green = 0, harsh = 0;
    for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
            int i = g.idx(x, y);
            if (!me.explored[i]) continue;
            const Tile& t = g.tiles[i];
            n++;
            if (g.isWater(x, y)) water++;
            else if (t.veg == V_JUNGLE) jungle++;
            else if (t.veg == V_FOREST || t.terrain == T_GRASS) green++;
            if (t.terrain == T_SNOW || t.terrain == T_TUNDRA || t.mountain || t.fallout) harsh++;
        }
    float f = n > 0 ? 1.0f / n : 0.0f;
    float quiet = overlay == OV_NONE ? 1.0f : 0.5f;
    snd->setAmbient(AMB_WAVES, std::min(1.0f, water * f * 1.6f) * quiet);
    snd->setAmbient(AMB_JUNGLE, std::min(1.0f, jungle * f * 3.0f) * quiet);
    snd->setAmbient(AMB_BIRDS, std::min(0.8f, green * f * 1.5f) * quiet);
    snd->setAmbient(AMB_WIND, std::min(1.0f, harsh * f * 2.5f + (g.warHappened && g.turn - PROLOGUE_TURNS < 25 ? 0.4f : 0.0f)) * quiet);
}

void GameUI::volumeControl(int x, int y, int w, const std::string& label, int& v) {
    cv->fillRectA(x, y, w, 40, rgb(20, 24, 30), 215);
    cv->text(x + 12, y + 11, label, 15, C_TEXT);
    int* pv = &v;
    button(x + w - 128, y + 5, 30, 30, "-", v > 0, [this, pv] { *pv = std::max(0, *pv - 10); applyVolumes(); }, "", C_PANEL2, 18);
    cv->text(x + w - 64, y + 11, v == 0 ? std::string("Off") : std::to_string(v) + "%", 15, C_GOLD, AL_CENTER, true);
    button(x + w - 34, y + 5, 30, 30, "+", v < 100, [this, pv] { *pv = std::min(100, *pv + 10); applyVolumes(); }, "", C_PANEL2, 18);
}
