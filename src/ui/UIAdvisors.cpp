// Rebirth of Civilisation - advisors
// Four advisors read the state of your people every turn and suggest what to do next, most urgent first:
// the Elder of the Hearth (growth and cities), the Keeper of Knowledge (science and evolution), the War
// Chief (war and defence) and the Far-Traveller (neighbours, tribes and trade). Their screen lists all
// their advice; with advisor tips on, the most urgent new matter also pops up at the start of a turn.
// SPDX-License-Identifier: MIT
#include "GameUI.h"
#include <algorithm>
static int dur5(const Game& g) { return std::max(1, g.dur(5)); }
#include <cctype>
#include <cmath>

namespace {
const int TOP = 36;  // matches GameUI.cpp
const Color C_TEXT = {234, 238, 242};
const Color C_DIM = {150, 160, 174};
const Color C_GOLD = {240, 200, 70};
const char* ADVISOR_NAMES[4] = {"Elder of the Hearth", "Keeper of Knowledge", "War Chief", "Far-Traveller"};
const char* ADVISOR_ROLES[4] = {"Growth and cities", "Science and evolution", "War and defence", "Neighbours and trade"};
const Color ADVISOR_COLORS[4] = {{120, 200, 110}, {110, 170, 250}, {230, 100, 80}, {240, 200, 90}};
}  // namespace

void GameUI::drawAdvisor(int who, int cx, int cy, int r) {
    const Game& g = *game;
    drawPortrait(g.civs[g.player].primate, cx, cy, r, ADVISOR_COLORS[who]);
    // an emblem for each: a sprouting seed, an eye, a spear, a winding road
    int ex = cx + r * 3 / 4, ey = cy + r * 3 / 4, er = std::max(6, r / 3);
    cv->fillCircle(ex, ey, er + 1, rgb(14, 14, 16));
    cv->fillCircle(ex, ey, er, ADVISOR_COLORS[who]);
    Color ink = rgb(24, 24, 28);
    int lw = std::max(1, er / 4);
    switch (who) {
        case 0:
            cv->line(ex, ey + er / 2, ex, ey - er / 3, ink, lw);
            cv->fillEllipseA(ex - er / 3, ey - er / 3, er / 3, er / 6, ink, 255);
            cv->fillEllipseA(ex + er / 3, ey - er / 2, er / 3, er / 6, ink, 255);
            break;
        case 1:
            cv->fillEllipseA(ex, ey, er * 2 / 3, er / 3, ink, 255);
            cv->fillCircle(ex, ey, std::max(1, er / 4), ADVISOR_COLORS[who]);
            break;
        case 2:
            cv->line(ex - er / 2, ey + er / 2, ex + er / 2, ey - er / 2, ink, lw);
            {
                int tip[6] = {ex + er / 2, ey - er / 2, ex + er / 8, ey - er / 3, ex + er / 3, ey - er / 8};
                cv->fillPoly(tip, 3, ink);
            }
            break;
        default: {
            int pts[8] = {ex - er / 2, ey + er / 2, ex + er / 3, ey + er / 6, ex - er / 3, ey - er / 6, ex + er / 2, ey - er / 2};
            cv->polylineA(pts, 4, ink, lw, 255);
            break;
        }
    }
}

std::vector<GameUI::Advice> GameUI::gatherAdvice() {
    Game& g = *game;
    int p = g.player;
    const Civ& me = g.civs[p];
    std::vector<Advice> out;
    auto add = [&](int who, int pri, const std::string& text, std::function<void()> go = nullptr, const std::string& goLabel = "Show me") {
        out.push_back({who, pri, text, go, goLabel});
    };
    auto showCity = [this](int id) { return [this, id] { openCity(id); }; };
    auto showTile = [this](int x, int y) { return [this, x, y] { overlay = OV_NONE; centerOn(x, y); }; };
    auto open = [this](Overlay o) { return [this, o] { overlay = o; }; };
    {  // veterans waiting for their promotion
        int n = 0, first = -1;
        for (auto& u : g.units)
            if (u.alive && u.owner == g.player && g.canPromote(u)) { n++; if (first < 0) first = u.id; }
        if (n > 0)
            add(2, 40, n == 1 ? "One of our warriors has earned a promotion." : std::to_string(n) + " of our warriors have earned promotions.",
                [this, first] { overlay = OV_NONE; selectUnit(first, true); promoteFor = first; }, "Promote");
    }
    std::vector<const City*> mine;
    for (auto& c : g.cities) if (c.alive && c.owner == p) mine.push_back(&c);
    const Unit* band = nullptr;
    int workers = 0, elders = 0, military = 0;
    for (auto& u : g.units) {
        if (!u.alive || u.owner != p) continue;
        const UnitDef& d = g_units[u.type];
        if (d.cls == UnitClass::Band && !band) band = &u;
        if (d.cls == UnitClass::Worker) workers++;
        if (d.cls == UnitClass::Settler) elders++;
        if (d.landMilitary()) military++;
    }
    bool atWarAny = false;
    for (int o = FIRST_PRIMATE_CIV; o < (int)g.civs.size(); o++) if (o != p && g.civs[o].alive && me.war[o]) atWarAny = true;

    // ---- the Elder of the Hearth: growth and cities
    if (mine.empty()) {
        if (g.canSettle(p) && band) {
            int bid = band->id;
            add(0, 90, "We can settle down at last! Choose a spot with food, fresh water and perhaps a luxury nearby, then found a city (B).",
                [this, bid] { overlay = OV_NONE; selectUnit(bid, true); }, "Select band");
        } else if (band) {
            int bid = band->id;
            add(0, 30, "We are wanderers still. Keep the band on fresh land - forest, jungle and grassland - so it eats well and grows.",
                [this, bid] { overlay = OV_NONE; selectUnit(bid, true); }, "Select band");
        }
    }
    for (const City* c : mine) {
        int m = g.moodOf(*c);
        if (c->buildKind == BK_NONE) add(0, 95, c->name + " has nothing to build.", showCity(c->id), "Open city");
        if (c->foodSurplus < 0) add(0, 85, c->name + " is starving! Work more food tiles, or build farms and a Food Cache.", showCity(c->id), "Open city");
        if (m == MOOD_UNREST) add(0, 90, c->name + " is in unrest: it does not grow and its output has fallen. It needs luxuries, a Temple, an Arena - or peace.", showCity(c->id), "Open city");
        else if (m == MOOD_UNHAPPY) add(0, 75, c->name + " is unhappy and has stopped growing. More luxuries or a happiness building would help.", showCity(c->id), "Open city");
        else if (c->happy <= c->unhappy && c->pop >= 5) add(0, 40, c->name + " cannot grow: it has no happiness to spare.", showCity(c->id), "Open city");
        else if (m == MOOD_CELEBRATING) add(0, 8, c->name + " is celebrating (+10% Food and Gold).", showCity(c->id), "Open city");
    }
    if (!mine.empty() && workers == 0) add(0, 60, "We have no Gatherers. Train one to farm, mine and build roads.", showCity(mine[0]->id), "Open city");
    if (!mine.empty() && elders == 0 && (int)mine.size() < 4 + g.settings.mapSize * 2 && g.turn > 0) {
        bool building = false, can = false;
        for (const City* c : mine) {
            if (c->buildKind == BK_UNIT && g_units[c->buildId].cls == UnitClass::Settler) building = true;
            if (g.canBuild(*c, BK_UNIT, ID.uElder)) can = true;
        }
        if (!building && can) add(0, 50, "There is still room to spread out. An Elder Troop can found another city.", showCity(mine[0]->id), "Open city");
    }

    for (auto& u : g.units)
        if (u.alive && u.owner == p && g.greatKind(u) >= 0 && u.order == ORD_NONE) {
            int uid = u.id;
            add(g.greatKind(u) == GK_WARLEADER ? 2 : g.greatKind(u) == GK_MERCHANT ? 3 : g.greatKind(u) == GK_THINKER || g.greatKind(u) == GK_LAST_HUMAN ? 1 : 0, 70,
                u.name + " awaits your word: use their gift now, or settle them in a city.", [this, uid] { overlay = OV_NONE; selectUnit(uid, true); }, "Select");
        }
    // faith: founding a religion, idle missionaries (the Elder of the Hearth)
    if (g.canFoundReligion(p)) add(0, 80, "Our people's faith is strong enough to found a religion of our own, while there is still room for one.", open(OV_RELIGION), "Religion");
    else if (g.canEnhanceReligion(p)) add(0, 45, "Our religion can be enhanced with two more beliefs.", open(OV_RELIGION), "Religion");
    for (auto& u : g.units)
        if (u.alive && u.owner == p && u.type == ID.uMissionary && u.order == ORD_NONE) {
            int uid = u.id;
            add(0, 55, "A Missionary waits. Lead it to a city that does not yet follow our faith and preach.", [this, uid] { overlay = OV_NONE; selectUnit(uid, true); }, "Select");
            break;
        }
    // the World Council (the Far-Traveller's business, but urgent before a session)
    if (g.council.formed) {
        const Council& k = g.council;
        int left = k.nextSession - g.turn;
        bool pending = false, unvoted = false;
        for (auto& pr : k.agenda) {
            if (pr.kind < 0 && pr.proposer == p) pending = true;
            if (pr.kind >= 0 && (p >= (int)pr.votes.size() || pr.votes[p] == 0)) unvoted = true;
        }
        bool noBallot = k.electionNext && (p >= (int)k.ballot.size() || k.ballot[p] < 0);
        if (pending) add(3, left <= 3 ? 80 : 50, "We may put a resolution before the World Council (session in " + std::to_string(left) + " turns).", open(OV_COUNCIL), "Council");
        else if ((unvoted || noBallot) && left <= dur5(g)) add(3, 65, "The World Council meets in " + std::to_string(left) + " turns and we have not cast all our votes" +
                                                               (noBallot ? ", nor chosen a chair" : "") + ".", open(OV_COUNCIL), "Council");
        if (k.chair == p && p < (int)k.abuse.size() && k.abuse[p] > 0)
            add(3, 40, "Our abuse of the chair's powers stands at " + std::to_string(k.abuse[p]) + " of " + std::to_string(ABUSE_BAR) + ". At " +
                std::to_string(ABUSE_BAR) + " we could not stand for re-election.", open(OV_COUNCIL), "Council");
    }
    // ---- the Keeper of Knowledge: science and evolution
    if (me.zealPenalty >= 10)
        add(1, me.zealPenalty >= 25 ? 85 : 60, "Our people's zeal costs us " + std::to_string(me.zealPenalty) + "% of our Science: they resist ideas that contradict "
            "their faith. Fewer new houses of worship, more science, scholarly beliefs or the Free Thought tradition would temper it.", open(OV_RELIGION), "Religion");
    bool canRes = false;
    for (size_t t = 0; t < g_techs.size(); t++) if (g.canResearch(p, (int)t)) canRes = true;
    if (!g.hasTrait(p, TR_NEOCORTEX))
        add(1, 35, "Our minds cannot yet learn on purpose. Science will come when we evolve an Enlarged Neocortex.", open(OV_EVO), "Evolution");
    else if (me.research < 0 && canRes) add(1, 95, "We are not researching anything.", open(OV_TECH), "Choose research");
    else if (me.research >= 0) add(1, 12, "We are researching " + g_techs[me.research].name + " (" + std::to_string(g.researchTurns(p)) + " turns).", open(OV_TECH), "Tech tree");
    bool canEvo = false;
    for (int t = 0; t < TR_COUNT; t++) if (g.canEvolve(p, t)) canEvo = true;
    if (me.evoGoal < 0 && canEvo) add(1, 85, "Our species could evolve further: choose a trait to evolve towards.", open(OV_EVO), "Evolution");
    else if (me.evoGoal >= 0 && g.evolveTurns(p, me.evoGoal) < 0)
        add(1, 55, "Evolution towards " + g_traits[me.evoGoal].name + " has stalled: our people lack a pressure it needs.", open(OV_EVO), "Evolution");
    for (size_t q = 0; q < g_policies.size(); q++)
        if (g.canAdopt(p, (int)q)) { add(1, 80, "We have the culture to adopt a new Tradition.", open(OV_POLICY), "Traditions"); break; }
    {
        int ruins = 0, rx = -1, ry = -1;
        for (int y = 0; y < g.H; y++)
            for (int x = 0; x < g.W; x++) {
                const Tile& t = g.at(x, y);
                if (t.ruin == RUIN_NONE || t.ruinDone || t.owner != p || g.excavateTierFor(p, x, y) < 0) continue;
                if (rx < 0) { rx = x; ry = y; }
                ruins++;
            }
        if (ruins) add(1, 50, std::to_string(ruins) + (ruins == 1 ? " ruin in our lands is" : " ruins in our lands are") + " ready to be excavated (move a unit there, then E).",
                       showTile(rx, ry), "Show");
    }

    // ---- the War Chief
    for (const City* c : mine) {
        int foes = 0, ferals = 0;
        bool garrison = false;
        for (auto& u : g.units) {
            if (!u.alive) continue;
            int d = g.dist(u.x, u.y, c->x, c->y);
            if (u.owner == p && d == 0 && g_units[u.type].landMilitary()) garrison = true;
            if (d > 3 || !g_units[u.type].military()) continue;
            if (u.owner == CIV_FERAL) ferals++;
            else if (g.atWar(p, u.owner)) foes++;
        }
        if (foes) add(2, 95, std::to_string(foes) + (foes == 1 ? " enemy unit is" : " enemy units are") + " closing on " + c->name + "!", showTile(c->x, c->y), "Show");
        else if (ferals) add(2, 70, "Feral beasts prowl near " + c->name + ".", showTile(c->x, c->y), "Show");
        if (!garrison && (foes || ferals || atWarAny)) add(2, 65, c->name + " has no defenders inside its walls.", showCity(c->id), "Open city");
    }
    if (band && mine.empty())
        for (auto& u : g.units)
            if (u.alive && u.owner == CIV_FERAL && g.dist(u.x, u.y, band->x, band->y) <= 3) {
                add(2, 75, "Feral " + g_units[u.type].name + " are close to our band. Move away, or fight them with the warband.", showTile(u.x, u.y), "Show");
                break;
            }
    int myPow = g.militaryPower(p) + 1;
    for (int o = FIRST_PRIMATE_CIV; o < (int)g.civs.size(); o++) {
        if (o == p || !g.civs[o].alive || !me.met[o]) continue;
        int their = g.militaryPower(o) + 1;
        std::string who = g.isMinor(o) ? g.tribeTitle(o) : "the " + g.civs[o].name;
        if (me.war[o])
            add(2, g.isMinor(o) ? 45 : 60, "We are at war with " + who + ". Our strength " + std::to_string(myPow) + ", theirs " + std::to_string(their) + ".",
                open(g.isMinor(o) ? OV_TRIBES : OV_DIPLO), g.isMinor(o) ? "Tribes" : "Diplomacy");
        else if (g.isMajor(o) && g.attitude(o, p) < 0 && their > myPow * 3 / 2)
            add(2, 55, "The " + g.civs[o].name + " are far stronger than us and do not like us. Build defences and walls.", open(OV_DIPLO), "Diplomacy");
    }
    {
        int up = 0, first = -1;
        for (auto& u : g.units)
            if (u.alive && u.owner == p && g.canUpgrade(u)) { up++; if (first < 0) first = u.id; }
        if (up) add(2, 40, std::to_string(up) + (up == 1 ? " unit" : " units") + " can be upgraded (U).",
                    [this, first] { overlay = OV_NONE; selectUnit(first, true); }, "Select");
    }
    if (!atWarAny && military > 0) add(2, 6, "We are at peace. " + std::to_string(military) + " troops stand ready.");

    // ---- the Far-Traveller: neighbours, tribes and trade
    int metMajors = 0;
    for (int o = FIRST_PRIMATE_CIV; o < (int)g.civs.size(); o++) {
        if (o == p || !g.civs[o].alive || !me.met[o]) continue;
        if (g.isMajor(o)) {
            metMajors++;
            const Civ& ov = g.civs[o];
            if (ov.policiesAdopted >= CULTURE_WIN_POLICIES - 3)
                add(3, 70, "The " + ov.name + " have adopted " + std::to_string(ov.policiesAdopted) + " Traditions: close to a Cultural victory!", open(OV_DIPLO), "Diplomacy");
            for (auto& c : g.cities)
                if (c.alive && c.owner == o && c.buildKind == BK_BUILDING && g_buildings[c.buildId].special == BSpecial::Victory) {
                    add(3, 80, "The " + ov.name + " are building the Rebirth Project in " + c.name + "!", open(OV_DIPLO), "Diplomacy");
                    break;
                }
            continue;
        }
        // a tribe
        const Civ& tv = g.civs[o];
        if (me.war[o]) continue;
        std::string title = g.tribeTitle(o);
        std::string Title = title.empty() ? title : (char)std::toupper((unsigned char)title[0]) + title.substr(1);
        if (tv.questKind != TQ_NONE && !(p < (int)tv.questDone.size() && tv.questDone[p]))
            add(3, 50, Title + " ask for help: " + g.questText(o) + ". It would win us 40 influence.", open(OV_TRIBES), "Tribes");
        int inf = g.influenceFor(p, o);
        if (g.hasGold(p) && me.gold >= 100) {
            if (inf < TRIBE_FRIEND && inf >= TRIBE_FRIEND - 15)
                add(3, 40, "A gift of gold would make " + title + " our friends (influence " + std::to_string(inf) + " of " + std::to_string(TRIBE_FRIEND) + ").",
                    open(OV_TRIBES), "Tribes");
            else if (tv.ally < 0 && inf >= TRIBE_ALLY - 15 && inf < TRIBE_ALLY)
                add(3, 45, "A gift of gold would make " + title + " our allies (influence " + std::to_string(inf) + " of " + std::to_string(TRIBE_ALLY) + ").",
                    open(OV_TRIBES), "Tribes");
            else if (tv.ally >= 0 && tv.ally != p && inf >= TRIBE_ALLY - 15 && g.influenceFor(tv.ally, o) - inf <= 30)
                add(3, 45, "A gift of gold could win " + title + " away from the " + g.civs[tv.ally].name + " (their influence " +
                               std::to_string(g.influenceFor(tv.ally, o)) + ", ours " + std::to_string(inf) + ").", open(OV_TRIBES), "Tribes");
        }
    }
    if (metMajors == 0 && g.turn > 10) add(3, 25, "We have not met another species yet. Send scouts to explore.");
    int cap = g.tradeCapacity(p), used = g.tradeUsed(p);
    if (cap > used) {
        int idle = -1;
        for (auto& u : g.units)
            if (u.alive && u.owner == p && g_units[u.type].cls == UnitClass::Trader && u.order != ORD_GOTO) { idle = u.id; break; }
        if (idle >= 0) add(3, 65, "A caravan is waiting for orders: move it into a city and choose a Trade Route.",
                           [this, idle] { overlay = OV_NONE; selectUnit(idle, true); }, "Select");
        else if (!mine.empty()) add(3, 45, "We could run " + std::to_string(cap - used) + " more trade route" + (cap - used == 1 ? "" : "s") + ": train Caravans.",
                                    showCity(mine[0]->id), "Open city");
    }
    for (int r = 1; r < R_COUNT; r++) {
        if (g_resources[r].kind != RK_LUXURY || g.resourceCount(p, r) < 2) continue;
        for (int o = FIRST_PRIMATE_CIV; o < (int)g.civs.size(); o++)
            if (g.isMajor(o) && o != p && g.civs[o].alive && me.met[o] && !me.war[o] && !g.hasResource(o, r)) {
                add(3, 40, "We have spare " + g_resources[r].name + ". The " + g.civs[o].name + " lack it - trade it for a luxury we do not have.",
                    [this, o] { diploSel = o; overlay = OV_DIPLO; }, "Diplomacy");
                r = R_COUNT;
                break;
            }
    }
    std::stable_sort(out.begin(), out.end(), [](const Advice& a, const Advice& b) { return a.pri > b.pri; });
    return out;
}

void GameUI::renderAdvisors() {
    int pw = std::min(1180, W - 30), ph = H - TOP - 20;
    int px = (W - pw) / 2, py = TOP + 8;
    panel(px, py, pw, ph);
    cv->text(px + 20, py + 12, "Advisors", 24, C_TEXT, AL_LEFT, true);
    cv->text(px + 150, py + 20, "Most urgent first. Below: progress towards your next Great Primates.", 13, C_DIM);
    button(px + pw - 190, py + 10, 140, 28, advisorTips ? "Turn tips off" : "Turn tips on", true, [this] { advisorTips = !advisorTips; }, "", rgb(46, 56, 72), 12);
    button(px + pw - 40, py + 10, 28, 28, "X", true, [this] { overlay = OV_NONE; });
    auto all = gatherAdvice();
    // progress towards the next Great Primate of each kind
    {
        Game& g = *game;
        int p = g.player, bw = (pw - 40 - 4 * 10) / GK_COUNT, by = py + 50;
        for (int k = 0; k < GK_COUNT; k++) {
            int bx = px + 20 + k * (bw + 10);
            int have = g.civs[p].greatPts[k], cost = g.greatCost(p, k);
            Unit probe;
            probe.type = ID.uGreat[k];
            probe.owner = p;
            drawUnitToken(probe, bx + 12, by + 14, 11);
            cv->text(bx + 30, by, GREAT_KIND_NAMES[k], 12, C_TEXT, AL_LEFT, true);
            bar(bx + 30, by + 18, bw - 34, 6, std::min(1.0, (double)have / std::max(1, cost)), rgb(230, 190, 80));
            hot(bx, by - 2, bw, 30, nullptr,
                std::string(GREAT_KIND_NAMES[k]) + ": " + std::to_string(have) + " of " + std::to_string(cost) + " points. Earned from " + GREAT_SOURCE[k] +
                    ". Born in your capital; each one takes more points than the last.");
        }
    }
    int gap = 12, cw = (pw - 40 - gap) / 2, ch = (ph - 110 - gap) / 2;
    for (int who = 0; who < 4; who++) {
        int x = px + 20 + (who % 2) * (cw + gap), y = py + 96 + (who / 2) * (ch + gap);
        cv->fillRoundRectGrad(x, y, cw, ch, 8, rgb(38, 44, 58), rgb(26, 30, 40));
        cv->roundRect(x, y, cw, ch, 8, darken(ADVISOR_COLORS[who], 0.7f), 1);
        drawAdvisor(who, x + 40, y + 40, 26);
        cv->text(x + 80, y + 14, ADVISOR_NAMES[who], 17, C_TEXT, AL_LEFT, true);
        cv->text(x + 80, y + 38, ADVISOR_ROLES[who], 12, ADVISOR_COLORS[who], AL_LEFT, true);
        int ly = y + 78, shown = 0;
        for (auto& a : all) {
            if (a.who != who) continue;
            auto lines = wrap(a.text, cw - 150, 13);
            int h = (int)lines.size() * 16 + 8;
            if (ly + h > y + ch - 8) break;
            Color dot = a.pri >= 80 ? rgb(240, 90, 70) : a.pri >= 50 ? rgb(240, 180, 80) : a.pri >= 20 ? rgb(170, 180, 190) : rgb(120, 200, 120);
            cv->fillCircle(x + 20, ly + 8, 5, dot);
            int ty = ly;
            for (auto& l : lines) { cv->text(x + 34, ty, l, 13, C_TEXT); ty += 16; }
            if (a.go) {
                auto go = a.go;
                button(x + cw - 108, ly - 2, 94, 24, a.goLabel, true, [go] { go(); }, "", rgb(46, 56, 72), 12);
            }
            ly += std::max(h, 28);
            shown++;
        }
        if (!shown) cv->text(x + 20, ly, "Nothing to report.", 13, C_DIM);
    }
}

// At the start of each turn the most urgent new matter, if any, is raised by its advisor.
void GameUI::updateAdvisorTip() {
    Game& g = *game;
    if (tipTurn == g.turn) return;
    tipTurn = g.turn;
    tip.active = false;
    if (!advisorTips || g.tutorialStep >= 0 || g.gameOver || g.turn < 2) return;
    for (auto& a : gatherAdvice()) {
        if (a.pri < 75) break;
        auto it = tipShown.find(a.text);
        if (it != tipShown.end() && g.turn - it->second < 8) continue;
        tipShown[a.text] = g.turn;
        tip.advice = a;
        tip.active = true;
        break;
    }
}

void GameUI::renderAdvisorTip() {
    Game& g = *game;
    updateAdvisorTip();
    if (!tip.active || overlay != OV_NONE || !g.popups.empty() || nukeT >= 0 || g.tutorialStep >= 0) return;
    const Advice& a = tip.advice;
    int pw = std::min(360, W / 3), px = 10, py = TOP + 10;
    auto lines = wrap(a.text, pw - 92, 13);
    int ph = std::max(96, 44 + (int)lines.size() * 16 + 40);
    cv->fillRoundRectGrad(px + 3, py + 4, pw, ph, 10, rgb(0, 0, 0), rgb(0, 0, 0), 90);
    cv->fillRoundRectGrad(px, py, pw, ph, 10, rgb(40, 46, 60), rgb(22, 26, 34), 245);
    cv->roundRect(px, py, pw, ph, 10, ADVISOR_COLORS[a.who], 2);
    hot(px, py, pw, ph, nullptr);
    drawAdvisor(a.who, px + 36, py + 38, 22);
    cv->text(px + 70, py + 10, ADVISOR_NAMES[a.who], 13, ADVISOR_COLORS[a.who], AL_LEFT, true);
    int y = py + 30;
    for (auto& l : lines) { cv->text(px + 70, y, l, 13, C_TEXT); y += 16; }
    int by = py + ph - 32;
    if (a.go) {
        auto go = a.go;
        button(px + 70, by, 100, 24, a.goLabel, true, [this, go] { tip.active = false; go(); }, "", rgb(60, 100, 60), 12);
    }
    button(px + 176, by, 76, 24, "Dismiss", true, [this] { tip.active = false; }, "", rgb(46, 56, 72), 12);
    button(px + 258, by, 90, 24, "Tips off", true, [this] { tip.active = false; advisorTips = false; },
           "Stop advisors speaking up at the start of turns (turn back on from the Advisors screen)", rgb(46, 56, 72), 12);
}

// ============================================================ the blue box
// A box tumbles out of the sky, lands upright on the tile, a figure steps out, and the box flickers and
// fades away. Drawn as a plain blue box: an affectionate nod, nothing more.
void GameUI::renderBlueBox() {
    Game& g = *game;
    int tx, ty;
    tileCenter(g.cassandraX, g.cassandraY, tx, ty);
    double t = boxT / 1000.0;
    int bw = std::max(18, hs * 7 / 10), bh = bw * 16 / 10;
    double fall = std::min(1.0, t / 1.4), ease = fall * fall;
    int groundY = ty - bh / 2 + hs / 6;
    int cy = (int)(-bh * 2 + (groundY + bh * 2) * ease);
    int cx = tx - hs / 4 + (int)((1 - ease) * hs * 2);
    double ang = t < 1.4 ? (1.4 - t) * 7.0 : 0.0;  // tumbling, upright on landing
    double alpha = 1.0;
    if (t > 2.3) alpha = std::max(0.0, (3.4 - t) / 1.1) * (0.55 + 0.45 * std::sin(t * 38.0));  // fading in and out of the world
    // a shadow growing as it falls
    cv->glowEllipse(tx - hs / 4, ty + hs / 3, (int)(bw * (0.3 + 0.7 * ease)), bw / 4, rgb(0, 0, 0), (int)(140 * ease * alpha));
    auto rot = [&](double fx, double fy, int& ox, int& oy) {
        double c = std::cos(ang), s = std::sin(ang);
        ox = cx + (int)std::lround(fx * c - fy * s);
        oy = cy + (int)std::lround(fx * s + fy * c);
    };
    auto quad = [&](double x0, double y0, double x1, double y1, Color col, int a) {
        int p[8];
        rot(x0, y0, p[0], p[1]); rot(x1, y0, p[2], p[3]); rot(x1, y1, p[4], p[5]); rot(x0, y1, p[6], p[7]);
        cv->fillPolyA(p, 4, col, a);
    };
    int A = (int)(255 * alpha);
    if (A > 4) {
        double hw = bw / 2.0, hh = bh / 2.0;
        quad(-hw, -hh, hw, hh, rgb(18, 40, 96), A);                          // the box
        quad(-hw + 3, -hh + 3, hw - 3, hh - 3, rgb(30, 64, 140), A);         // its face
        quad(-hw * 0.12, -hh + 3, hw * 0.12, hh - 3, rgb(18, 40, 96), A);    // the door's seam
        quad(-hw - 2, -hh - 4, hw + 2, -hh + 2, rgb(14, 30, 74), A);         // the roof
        quad(-hw + 6, -hh * 0.6, -hw * 0.25, -hh * 0.25, rgb(200, 220, 250), A * 3 / 4);  // two little windows
        quad(hw * 0.25, -hh * 0.6, hw - 6, -hh * 0.25, rgb(200, 220, 250), A * 3 / 4);
        if (t > 1.4 && t < 2.2) cv->glowEllipse(cx, cy, bw, bh * 2 / 3, rgb(220, 235, 255), (int)(120 * (2.2 - t) / 0.8));  // the door opens
    }
    if (t > 1.6) {  // the figure who fell out
        double f = std::min(1.0, (t - 1.6) / 0.5);
        int fx = tx + hs / 3, fy = ty + hs / 10, s = std::max(5, hs / 5);
        int a = (int)(255 * f);
        cv->fillCircleA(fx, fy - s * 2, s * 2 / 3, rgb(236, 210, 190), a);
        int body[8] = {fx - s / 3, fy - s * 3 / 2, fx + s / 3, fy - s * 3 / 2, fx + s, fy + s, fx - s, fy + s};
        cv->fillPolyA(body, 4, rgb(240, 236, 228), a);
    }
    if (t > 0.3 && t < 3.3) {
        std::string cap = t < 1.4 ? "Something is falling out of the sky..." : t < 2.3 ? "A figure tumbles out onto the grass!" : "...and the box fades away.";
        int w = cv->textWidth(cap, 18, true) + 40;
        cv->fillRoundRectGrad(W / 2 - w / 2, H - 120, w, 36, 8, rgb(14, 22, 44), rgb(8, 12, 26), 220);
        cv->text(W / 2, H - 112, cap, 18, rgb(220, 232, 255), AL_CENTER, true);
    }
}
