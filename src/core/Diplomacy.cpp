// Rebirth of Civilisation - diplomacy: opinions, treaties and deals
//
// Every civilisation keeps an opinion of every other one, built from visible reasons: its own
// personality, shared borders, wars, remembered wrongs, gifts, treaties and the company the
// other keeps. Opinion decides how the AI trades, whom it befriends and whom it attacks.
//
// Treaties (Agreements) last a fixed number of turns:
//   open borders (military units may enter each other's lands), research pacts (+15% science
//   each), defensive pacts (an attack on one is an attack on both), declarations of friendship,
//   resource sharing (a strategic or luxury resource counts as the receiver's own) and gold per
//   turn. Making peace brings a peace treaty during which neither side may declare war again.
// Deals bundle items from both sides; the AI accepts a deal when it values it at zero or more.
// SPDX-License-Identifier: MIT
#include "Game.h"
#include <algorithm>
#include <cmath>

static bool mutualKind(int k) { return k == AG_RESEARCH || k == AG_DEFENSIVE || k == AG_FRIENDSHIP; }

const char* Game::attitudeName(int a) {
    switch (a) {
        case -2: return "Hostile";
        case -1: return "Unfriendly";
        case 1: return "Friendly";
        case 2: return "Close friends";
        default: return "Neutral";
    }
}

// ------------------------------------------------------------ agreements
const Agreement* Game::findAgreement(int kind, int a, int b, int value) const {
    for (auto& ag : agreements) {
        if (ag.kind != kind || ag.until <= turn) continue;
        if (value >= 0 && ag.value != value) continue;
        if (ag.a == a && ag.b == b) return &ag;
        if (mutualKind(kind) && ag.a == b && ag.b == a) return &ag;
    }
    return nullptr;
}

bool Game::hasAgreement(int kind, int a, int b) const { return findAgreement(kind, a, b) != nullptr; }

int Game::agreementTurnsLeft(int kind, int a, int b) const {
    const Agreement* ag = findAgreement(kind, a, b);
    return ag ? ag->until - turn : 0;
}

void Game::endAgreements(int a, int b) {
    for (auto& ag : agreements)
        if ((ag.a == a && ag.b == b) || (ag.a == b && ag.b == a)) ag.until = turn;
    markResourcesDirty();
}

bool Game::lentResource(int c, int r) const {
    for (auto& ag : agreements)
        if (ag.kind == AG_RESOURCE && ag.b == c && ag.value == r && ag.until > turn && civs[ag.a].alive) return true;
    return false;
}

int Game::diploSciPct(int c) const {
    int n = 0;
    for (auto& ag : agreements)
        if (ag.kind == AG_RESEARCH && ag.until > turn && (ag.a == c || ag.b == c)) n++;
    return std::min(2, n) * 15;
}

bool Game::embargoed(int a, int b) const {
    if (a < FIRST_PRIMATE_CIV || b < FIRST_PRIMATE_CIV || a == b) return false;
    return (civs[a].embargo.size() > (size_t)b && civs[a].embargo[b]) || (civs[b].embargo.size() > (size_t)a && civs[b].embargo[a]);
}

bool Game::canEmbargo(int a, int b, std::string* why) const {
    auto fail = [&](const std::string& m) { if (why) *why = m; return false; };
    if (a == b || b < FIRST_PRIMATE_CIV || !civs[b].alive) return fail("Not possible");
    if (!civs[a].met[b]) return fail("You have not met them");
    if (!hasTech(a, ID.tLaw)) return fail("Requires Tribal Law");
    if (civs[a].embargo[b]) return fail("Already under your embargo");
    return true;
}

void Game::setEmbargo(int a, int b, bool on) {
    if (on == (bool)civs[a].embargo[b]) return;
    if (on && !canEmbargo(a, b)) return;
    civs[a].embargo[b] = on ? 1 : 0;
    if (on) {
        civs[b].grievance[a] += 10;
        for (auto& ag : agreements)  // every trade between them stops
            if (((ag.a == a && ag.b == b) || (ag.a == b && ag.b == a)) &&
                (ag.kind == AG_RESOURCE || ag.kind == AG_GOLD_PER_TURN || ag.kind == AG_RESEARCH))
                ag.until = turn;
        cancelRoutesBetween(a, b, "the " + civs[a].name + " imposed an embargo on the " + civs[b].name);  // and the caravans turn back
        markResourcesDirty();
    } else {
        civs[b].goodwill[a] += 5;
    }
    std::string what = on ? " imposed an embargo on " : " lifted their embargo on ";
    if (a == player) notify(player, std::string(on ? "You imposed an embargo on the " : "You lifted your embargo on the ") + civs[b].name + ".");
    else if (b == player) notify(player, "The " + civs[a].name + what + "you." + (on ? " No trade is possible between you." : ""));
    else if (civs[player].met[a] && civs[player].met[b]) notify(player, "The " + civs[a].name + what + "the " + civs[b].name + ".");
}

bool Game::canDeclareWar(int a, int b, std::string* why) const {
    auto fail = [&](const std::string& m) { if (why) *why = m; return false; };
    if (a == b || b < FIRST_PRIMATE_CIV || !civs[b].alive) return fail("Not possible");
    if (civs[a].war[b]) return fail("Already at war");
    if (!civs[a].met[b]) return fail("You have not met them");
    if (civs[a].peaceUntil.size() > (size_t)b && civs[a].peaceUntil[b] > turn)
        return fail("Your peace treaty forbids war for " + std::to_string(civs[a].peaceUntil[b] - turn) + " more turns");
    return true;
}

// ------------------------------------------------------------ opinion
int Game::opinion(int c, int o, std::vector<std::pair<std::string, int>>* why) const {
    if (c < FIRST_PRIMATE_CIV || o < FIRST_PRIMATE_CIV || c == o) return 0;
    const Civ& cv = civs[c];
    const Civ& ov = civs[o];
    const PrimateDef* pd = primate(c);
    const PrimateDef* po = primate(o);
    int total = 0;
    auto add = [&](const std::string& label, int v) {
        if (!v) return;
        total += v;
        if (why) why->push_back({label, v});
    };
    if (pd) add(pd->persona.empty() ? "Personality" : pd->persona, pd->friendliness);
    if (pd && po && pd->ape == po->ape) add(pd->ape ? "Fellow apes" : "Fellow monkeys", 4);
    if (cv.war[o]) add("We are at war", -50);
    // neighbours
    int closest = 99;
    for (auto& a : cities) {
        if (!a.alive || a.owner != c) continue;
        for (auto& b : cities) if (b.alive && b.owner == o) closest = std::min(closest, dist(a.x, a.y, b.x, b.y));
    }
    if (closest <= 6) add("Our lands border each other", -(2 + (pd ? pd->territorial : 5)));
    if (cv.grievance.size() > (size_t)o && cv.grievance[o] > 0) add("Remembered wrongs", -std::min(60, cv.grievance[o]));
    if (cv.goodwill.size() > (size_t)o && cv.goodwill[o] > 0) add("Gifts and fair dealing", std::min(30, cv.goodwill[o]));
    if (hasAgreement(AG_FRIENDSHIP, c, o)) add("Declaration of friendship", 15);
    if (hasAgreement(AG_DEFENSIVE, c, o)) add("Defensive pact", 15);
    if (hasAgreement(AG_OPEN_BORDERS, c, o) || hasAgreement(AG_OPEN_BORDERS, o, c)) add("Open borders", 4);
    if (ov.embargo.size() > (size_t)c && ov.embargo[c]) add("They embargo us", -15);
    if (hasAgreement(AG_RESEARCH, c, o)) add("Research pact", 6);
    int trades = 0;
    for (auto& ag : agreements)
        if ((ag.kind == AG_RESOURCE || ag.kind == AG_GOLD_PER_TURN) && ag.until > turn && ((ag.a == c && ag.b == o) || (ag.a == o && ag.b == c))) trades++;
    for (auto& r : routes)  // caravans between us
        if (r.to >= 0 && r.to < (int)cities.size() && ((r.owner == c && cities[r.to].owner == o) || (r.owner == o && cities[r.to].owner == c))) trades++;
    if (trades) add("Trading partners", std::min(12, trades * 3) * (pd && pd->name == "Capuchin" ? 2 : 1));
    bool common = false, hitsFriends = false;
    for (int x = FIRST_PRIMATE_CIV; x < (int)civs.size(); x++) {
        if (x == c || x == o || !civs[x].alive) continue;
        if (cv.war[x] && ov.war[x]) common = true;
        if (ov.war[x] && (hasAgreement(AG_FRIENDSHIP, c, x) || hasAgreement(AG_DEFENSIVE, c, x))) hitsFriends = true;
    }
    if (common) add("Fighting a common enemy", 12);
    if (hitsFriends) add("At war with our friends", -12);
    if (ov.denouncedUntil.size() > (size_t)c && ov.denouncedUntil[c] > turn) add("They denounced us", -20);
    for (int x = FIRST_PRIMATE_CIV; x < (int)civs.size(); x++)
        if (x != c && x != o && civs[x].denouncedUntil.size() > (size_t)o && civs[x].denouncedUntil[o] > turn && hasAgreement(AG_FRIENDSHIP, c, x)) {
            add("Denounced by our friends", -8);
            break;
        }
    if (ov.warsDeclared > 0) add("Warmonger", -std::min(15, ov.warsDeclared * 3) * (pd && pd->name == "Gorilla" ? 2 : 1));
    if (ov.nukesLaunched > 0) add("Uses nuclear weapons", -20);
    if (ov.backstabs > 0) add("Betrayed a friend", -15);
    if (pd && (pd->name == "Japanese Macaque" || pd->name == "Baboon")) {
        int mp = militaryPower(c) + 1, op = militaryPower(o) + 1;
        if (op > mp * 13 / 10) add("Respects your strength", 6);
        else if (op * 13 / 10 < mp) add("Looks down on the weak", -6);
    }
    if (era(o) >= 6 && score(o) > score(c) * 115 / 100) add("Racing us to the rebirth", -8);
    if (pd && pd->name == "Gibbon") {  // one partner for life
        for (auto& ag : agreements)
            if (ag.kind == AG_DEFENSIVE && ag.until > turn && (ag.a == c || ag.b == c) && ag.a != o && ag.b != o) { add("Already has a life partner", -10); break; }
    }
    return total;
}

int Game::attitude(int c, int o) const {
    int v = opinion(c, o);
    if (v <= -30) return -2;
    if (v < -8) return -1;
    if (v < 12) return 0;
    if (v < 30) return 1;
    return 2;
}

// ------------------------------------------------------------ deals
std::vector<int> Game::tradeableResources(int giver, int receiver) const {
    std::vector<int> out;
    for (int r = 1; r < R_COUNT; r++) {
        if (r == R_SCRAP) continue;
        const ResourceDef& rd = g_resources[r];
        if (rd.kind != RK_STRATEGIC && rd.kind != RK_LUXURY) continue;
        if (resourceCount(giver, r) <= 0) continue;          // only what you really own
        if (hasResource(receiver, r)) continue;
        if (findAgreement(AG_RESOURCE, giver, receiver, r)) continue;
        out.push_back(r);
    }
    return out;
}

bool Game::dealItemValid(const Deal& d, const DealItem& it, std::string* why) const {
    auto fail = [&](const std::string& m) { if (why) *why = m; return false; };
    int a = d.a, b = d.b;
    int giver = it.from, taker = it.from == a ? b : a;
    bool atWarNow = civs[a].war[b];
    if ((it.kind == DI_GOLD || it.kind == DI_GPT || it.kind == DI_RESOURCE || it.kind == DI_RESEARCH) && embargoed(a, b))
        return fail("Trade is under embargo");
    switch (it.kind) {
        case DI_GOLD:
            if (!hasGold(giver) || !hasGold(taker)) return fail("Both sides need Currency to exchange gold");
            if (it.value <= 0 || civs[giver].gold < it.value) return fail("Not enough gold");
            return true;
        case DI_GPT:
            if (!hasGold(giver) || !hasGold(taker)) return fail("Both sides need Currency to exchange gold");
            if (it.value <= 0 || civs[giver].goldPerTurn < it.value) return fail("Not enough income");
            return true;
        case DI_RESOURCE: {
            auto ok = tradeableResources(giver, taker);
            if (std::find(ok.begin(), ok.end(), it.value) == ok.end()) return fail("That resource cannot be traded");
            if (atWarNow) return fail("Not while at war");
            return true;
        }
        case DI_OPEN_BORDERS:
            if (atWarNow) return fail("Not while at war");
            if (giver < 0) return fail("Open borders are granted by one side");
            if (!bordersClosed(giver)) return fail(giver == player ? "Your borders are open to all until you discover Tribal Law"
                                                                   : "Their borders are open to all until they discover Tribal Law");
            if (hasAgreement(AG_OPEN_BORDERS, giver, taker)) return fail("Those borders are already open");
            return true;
        case DI_RESEARCH:
            if (atWarNow) return fail("Not while at war");
            if (hasAgreement(AG_RESEARCH, a, b)) return fail("A research pact is already in force");
            if (!hasTrait(a, TR_NEOCORTEX) || !hasTrait(b, TR_NEOCORTEX)) return fail("Both sides must be capable of science");
            return true;
        case DI_DEFENSIVE:
            if (atWarNow) return fail("Not while at war");
            if (hasAgreement(AG_DEFENSIVE, a, b)) return fail("A defensive pact is already in force");
            return true;
        case DI_FRIENDSHIP:
            if (atWarNow) return fail("Not while at war");
            if (hasAgreement(AG_FRIENDSHIP, a, b)) return fail("You are already declared friends");
            return true;
        case DI_PEACE:
            if (!atWarNow) return fail("You are not at war");
            return true;
        case DI_WAR_ON: {
            int x = it.value;
            if (x == a || x == b || x < FIRST_PRIMATE_CIV || !civs[x].alive) return fail("Not possible");
            if (!civs[a].met[x] || !civs[b].met[x]) return fail("Both sides must know them");
            if (civs[giver].war[x]) return fail("Already at war with them");
            std::string w;
            if (!canDeclareWar(giver, x, &w)) return fail(w);
            return true;
        }
    }
    return fail("Unknown item");
}

double Game::peaceValue(int c, int o) const {
    // positive: c would like peace with o
    int len = turn - civs[c].warSince[o];
    double my = militaryPower(c) + 5, th = militaryPower(o) + 5;
    const PrimateDef* pd = primate(c);
    int aggr = pd ? pd->aggression : 5;
    double v = (th / my - 1.0) * 120 + (len - dur(8)) * 3 - aggr * 6;
    // losing cities hurts; taking theirs encourages continuing
    for (auto& ci : cities) {
        if (!ci.alive) continue;
        if (ci.owner == o && ci.origOwner == c) v += 40;
        if (ci.owner == c && ci.origOwner == o) v -= 25;
    }
    if (pd && pd->name == "Bonobo") v += 60;
    if (len < dur(4)) v -= 80;  // too soon
    return v;
}

double Game::dealValue(int c, const Deal& d, std::string* why) const {
    int o = d.a == c ? d.b : d.a;
    const PrimateDef* pd = primate(c);
    int greed = pd ? pd->greed : 5, pactLove = pd ? pd->pactLove : 5, territorial = pd ? pd->territorial : 5, loyalty = pd ? pd->loyalty : 5;
    int att = attitude(c, o);
    int op = opinion(c, o);
    int turns = dealTurns();
    double v = 0;
    auto refuse = [&](const std::string& m) { if (why) *why = m; return -1e9; };
    bool friendshipInDeal = false;
    for (auto& it : d.items) if (it.kind == DI_FRIENDSHIP) friendshipInDeal = true;
    int myPow = militaryPower(c) + 5, theirPow = militaryPower(o) + 5;
    bool bordersTouch = false;
    for (auto& a : cities)
        if (a.alive && a.owner == c)
            for (auto& b : cities) if (b.alive && b.owner == o && dist(a.x, a.y, b.x, b.y) <= 7) bordersTouch = true;
    for (auto& it : d.items) {
        std::string w;
        if (!dealItemValid(d, it, &w)) return refuse(w);
        bool gets = it.from == o, gives = it.from == c;
        switch (it.kind) {
            case DI_GOLD: v += gets ? it.value : -it.value * (1.0 + greed / 20.0); break;
            case DI_GPT: v += gets ? it.value * turns * 0.7 : -it.value * turns * (0.8 + greed / 20.0); break;
            case DI_RESOURCE: {
                int r = it.value;
                bool strategic = g_resources[r].kind == RK_STRATEGIC;
                if (gets) {
                    bool useful = !strategic;
                    for (auto& u : g_units) for (int need : u.needs) if (need == r && hasTech(c, u.techId) && unitBelongsTo(c, unitIndex(u.name))) useful = true;
                    for (auto& b : g_buildings) for (int need : b.needs) if (need == r && hasTech(c, b.techId)) useful = true;
                    v += useful ? (strategic ? 140 : 90) : 15;
                } else if (gives) {
                    if (att <= -2) return refuse("We will not supply our enemies");
                    v -= 25 + (strategic && att < 1 ? 70 : 0) + greed * 3;
                }
                break;
            }
            case DI_OPEN_BORDERS:
                if (gets) { v += 20; break; }
                if (att <= -1) return refuse("We do not trust you enough to open our borders");
                v += 5 + (pd ? pd->friendliness : 0) - territorial;
                if (bordersTouch && territorial >= 7 && theirPow > myPow * 13 / 10) v -= 90;
                break;
            case DI_RESEARCH:
                if (att < 1 && !(pd && (pd->name == "Orangutan" || pd->name == "Bonobo" || pd->name == "Capuchin") && att >= 0))
                    return refuse("Only friends share their knowledge with us");
                {
                    int mine = 0, theirs = 0;
                    for (size_t t = 0; t < g_techs.size(); t++) { mine += civs[c].techs[t]; theirs += civs[o].techs[t]; }
                    v += theirs * 10 >= mine * 8 ? 60 : 25;
                }
                break;
            case DI_DEFENSIVE: {
                if (att < 1) return refuse("We are not close enough for that");
                if (!hasAgreement(AG_FRIENDSHIP, c, o) && !friendshipInDeal) return refuse("Declare friendship first");
                if (pd && pd->name == "Gibbon")
                    for (auto& ag : agreements)
                        if (ag.kind == AG_DEFENSIVE && ag.until > turn && (ag.a == c || ag.b == c)) return refuse("We already have a partner for life");
                v += pactLove * 9 - 40;
                // do they drag us into a war we cannot win?
                for (int x = FIRST_PRIMATE_CIV; x < (int)civs.size(); x++)
                    if (x != c && x != o && civs[x].alive && civs[o].war[x] && militaryPower(x) > myPow) v -= 60;
                // do we need protection?
                for (int x = FIRST_PRIMATE_CIV; x < (int)civs.size(); x++)
                    if (x != c && x != o && civs[x].alive && civs[c].met[x] && militaryPower(x) > myPow * 3 / 2 && attitude(c, x) < 0) { v += 50; break; }
                break;
            }
            case DI_FRIENDSHIP:
                if (op < 8) return refuse("We hardly know you well enough");
                v += 10 + (pd ? pd->friendliness : 0);
                break;
            case DI_PEACE: v += peaceValue(c, o); break;
            case DI_WAR_ON: {
                int x = it.value;
                if (gives) {
                    if (hasAgreement(AG_FRIENDSHIP, c, x) || hasAgreement(AG_DEFENSIVE, c, x)) return refuse("We would never betray the " + civs[x].name);
                    if (attitude(c, x) >= 1) return refuse("The " + civs[x].name + " are our friends");
                    if (militaryPower(x) > myPow * 13 / 10) return refuse("The " + civs[x].name + " are too strong for us");
                    double cost = 220 + opinion(c, x) * 4 - (loyalty < 4 ? 40 : 0);
                    v -= std::max(0.0, cost);
                } else {
                    v += civs[c].war[x] ? 140 : attitude(c, x) <= -1 ? 50 : 0;
                }
                break;
            }
        }
    }
    v += op * 1.0;  // friends are generous, rivals stingy
    if (d.demand && d.a == o) {  // a tribute demand: weigh it against their threat
        double ratio = (double)theirPow / myPow;
        v += ratio >= 2.5 ? 400 : ratio >= 1.8 ? 120 : -50;
    }
    if (why && v < 0) *why = "That is not worth our while";
    return v;
}

bool Game::aiAccepts(int c, const Deal& d, std::string* why) const {
    if (d.items.empty()) { if (why) *why = "There is nothing to agree on"; return false; }
    double v = dealValue(c, d, why);
    int need = skillOf(c) >= 2 ? 5 : 0;
    return v >= need;
}

bool Game::executeDeal(const Deal& d) {
    for (auto& it : d.items) if (!dealItemValid(d, it)) return false;
    int a = d.a, b = d.b;
    int until = turn + dealTurns();
    for (auto& it : d.items) {
        int giver = it.from, taker = it.from == a ? b : a;
        switch (it.kind) {
            case DI_GOLD:
                civs[giver].gold -= it.value;
                civs[taker].gold += it.value;
                if (taker >= 0) civs[taker].goodwill[giver] += it.value / 15;
                break;
            case DI_GPT: agreements.push_back({AG_GOLD_PER_TURN, giver, taker, until, it.value}); break;
            case DI_RESOURCE: agreements.push_back({AG_RESOURCE, giver, taker, until, it.value}); break;
            case DI_OPEN_BORDERS: agreements.push_back({AG_OPEN_BORDERS, giver, taker, until, 0}); break;
            case DI_RESEARCH: agreements.push_back({AG_RESEARCH, a, b, until, 0}); break;
            case DI_DEFENSIVE: agreements.push_back({AG_DEFENSIVE, a, b, turn + dur(50), 0}); break;
            case DI_FRIENDSHIP: agreements.push_back({AG_FRIENDSHIP, a, b, turn + dur(50), 0}); break;
            case DI_PEACE: makePeace(a, b); break;
            case DI_WAR_ON: declareWar(giver, it.value); break;
        }
    }
    markResourcesDirty();
    civs[a].dealsMade++;
    civs[b].dealsMade++;
    if (!d.demand) {
        civs[a].goodwill[b] += 3;
        civs[b].goodwill[a] += 3;
    }
    return true;
}

std::string Game::dealItemText(const DealItem& it, int viewer) const {
    std::string who = it.from < 0 ? "" : it.from == viewer ? "You give " : "They give ";
    int t = dealTurns();
    switch (it.kind) {
        case DI_GOLD: return who + std::to_string(it.value) + " gold";
        case DI_GPT: return who + std::to_string(it.value) + " gold per turn (" + std::to_string(t) + " turns)";
        case DI_RESOURCE: return who + "access to " + g_resources[it.value].name + " (" + std::to_string(t) + " turns)";
        case DI_OPEN_BORDERS:
            return (it.from == viewer ? std::string("You open your borders to them") : std::string("They open their borders to you")) + " (" + std::to_string(t) + " turns)";
        case DI_RESEARCH: return "Research pact: +15% science each (" + std::to_string(t) + " turns)";
        case DI_DEFENSIVE: return "Defensive pact (" + std::to_string(dur(50)) + " turns)";
        case DI_FRIENDSHIP: return "Declaration of friendship (" + std::to_string(dur(50)) + " turns)";
        case DI_PEACE: return "Peace, with a " + std::to_string(dur(10)) + "-turn treaty";
        case DI_WAR_ON:
            return (it.from == viewer ? std::string("You declare war on the ") : std::string("They declare war on the ")) + civs[it.value].name;
    }
    return "?";
}

// ------------------------------------------------------------ events
void Game::denounce(int a, int b) {
    if (a == b || civs[a].denouncedUntil[b] > turn) return;
    civs[a].denouncedUntil[b] = turn + dur(30);
    civs[b].grievance[a] += 15;
    endAgreements(a, b);
    if (b == player) notify(player, civs[a].leader + " of the " + civs[a].name + " has publicly denounced you.");
    else if (a == player) notify(player, "You have denounced the " + civs[b].name + ".");
    else if (civs[player].met[a] && civs[player].met[b]) notify(player, "The " + civs[a].name + " have denounced the " + civs[b].name + ".");
}

void Game::expelUnits(int a, int b) {
    for (auto& u : units) {
        if (!u.alive || u.owner != a || !g_units[u.type].military() || g_units[u.type].cls == UnitClass::Recon) continue;
        if (!inMap(u.x, u.y) || at(u.x, u.y).owner != b || u.carrier >= 0) continue;
        bool moved = false;
        for (int r = 1; r <= 12 && !moved; r++)
            for (auto& p : ring(u.x, u.y, r)) {
                if (dist(p.x, p.y, u.x, u.y) != r || at(p.x, p.y).owner == b || at(p.x, p.y).mountain) continue;
                if (g_units[u.type].naval() ? !shipCanStand(u, p.x, p.y) : isWater(p.x, p.y)) continue;  // e.g. a coastal boat cannot be pushed onto the ocean
                if (!g_units[u.type].naval() && moveCost(u, p.x, p.y, p.x, p.y) < 0) continue;
                if (cityAt(p.x, p.y) && cityAt(p.x, p.y)->owner != a) continue;
                if (!canEnter(u, p.x, p.y)) continue;
                u.x = p.x; u.y = p.y;
                u.order = ORD_NONE;
                for (auto& o : units) if (o.alive && o.carrier == u.id) { o.x = p.x; o.y = p.y; }
                moved = true;
                break;
            }
        if (!moved) killUnit(u);
    }
}

void Game::answerOffer(int idx, bool accept) {
    if (idx < 0 || idx >= (int)offers.size()) return;
    Deal d = offers[idx];
    offers[idx].items.clear();
    int ai = d.a;
    if (d.items.empty() || !civs[ai].alive) return;
    if (accept) {
        if (!executeDeal(d)) notify(player, "The deal with the " + civs[ai].name + " could no longer be completed.");
        else notify(player, "Deal agreed with the " + civs[ai].name + ".");
        return;
    }
    if (d.demand) {
        civs[ai].grievance[player] += 20;
        notify(player, "You refused the " + civs[ai].name + "'s demand. " + civs[ai].leader + " will not forget it.");
        const PrimateDef* pd = primate(ai);
        std::string w;
        if (pd && rng.chance(pd->aggression * 0.06) && canDeclareWar(ai, player, &w)) declareWar(ai, player);
    } else {
        civs[ai].goodwill[player] = std::max(0, civs[ai].goodwill[player] - 2);
    }
}

void Game::diplomacyTurn() {
    // gold per turn, expiries
    for (auto& ag : agreements) {
        if (ag.until <= turn - 1) continue;  // ended earlier
        if (ag.kind == AG_GOLD_PER_TURN && ag.until > turn) {
            if (!civs[ag.a].alive || civs[ag.a].gold < ag.value) {
                ag.until = turn;
                civs[ag.b].grievance[ag.a] += 10;
                if (ag.a == player || ag.b == player) notify(player, "A gold-per-turn deal between the " + civs[ag.a].name + " and the " + civs[ag.b].name + " collapsed: the payer ran out of gold.");
            } else {
                civs[ag.a].gold -= ag.value;
                civs[ag.b].gold += ag.value;
            }
        }
        if (ag.until == turn) {
            if (ag.kind == AG_OPEN_BORDERS && !civs[ag.b].war[ag.a]) expelUnits(ag.b, ag.a);
            if (ag.a == player || ag.b == player) {
                int other = ag.a == player ? ag.b : ag.a;
                static const char* NAMES[AG_COUNT] = {"An open-borders agreement", "The research pact", "The defensive pact", "The declaration of friendship",
                                                      "Resource sharing", "A gold-per-turn deal"};
                if (civs[other].alive && !civs[player].war[other]) notify(player, std::string(NAMES[ag.kind]) + " with the " + civs[other].name + " has ended.");
            }
            markResourcesDirty();
        }
    }
    agreements.erase(std::remove_if(agreements.begin(), agreements.end(), [&](const Agreement& ag) { return ag.until < turn; }), agreements.end());
    // memories fade
    bool fade = turn % std::max(1, speedPct / 100) == 0;
    if (fade)
        for (auto& cv : civs)
            for (size_t o = 0; o < cv.grievance.size(); o++) {
                if (cv.grievance[o] > 0) cv.grievance[o]--;
                if (cv.goodwill[o] > 0 && turn % 2 == 0) cv.goodwill[o]--;
            }
}

// ------------------------------------------------------------ AI
// treaties between computer players
void Game::aiPacts(int c) {
    if (skillOf(c) == 0 && !rng.chance(0.5)) return;
    for (int o = FIRST_PRIMATE_CIV; o < (int)civs.size(); o++) {
        if (o == c || o == player || !civs[o].alive || !civs[c].met[o] || civs[c].war[o]) continue;
        if (turn - civs[c].lastOffer[o] < dur(6)) continue;
        if (!rng.chance(0.15)) continue;
        civs[c].lastOffer[o] = turn;
        std::vector<Deal> cands;
        auto mk = [&](std::initializer_list<DealItem> items) {
            Deal d; d.a = c; d.b = o;
            for (auto& it : items) if (dealItemValid(d, it)) d.items.push_back(it);
            if (!d.items.empty()) cands.push_back(d);
        };
        mk({{DI_FRIENDSHIP, -1, 0}});
        mk({{DI_OPEN_BORDERS, c, 0}, {DI_OPEN_BORDERS, o, 0}});
        mk({{DI_RESEARCH, -1, 0}});
        mk({{DI_DEFENSIVE, -1, 0}});
        auto mine = tradeableResources(c, o), theirs = tradeableResources(o, c);
        if (!mine.empty() && !theirs.empty()) mk({{DI_RESOURCE, c, mine[0]}, {DI_RESOURCE, o, theirs[0]}});
        for (auto& d : cands) {
            bool valid = true;
            for (auto& it : d.items) if (!dealItemValid(d, it)) valid = false;
            if (!valid) continue;
            if (dealValue(c, d) >= 0 && aiAccepts(o, d)) {
                executeDeal(d);
                if (civs[player].met[c] && civs[player].met[o]) {
                    std::string what = dealItemText(d.items[0], -1);
                    if (d.items[0].kind == DI_RESOURCE) what = "a resource trade";
                    if (d.items[0].kind == DI_DEFENSIVE || d.items[0].kind == DI_FRIENDSHIP)
                        notify(player, "The " + civs[c].name + " and the " + civs[o].name + " signed: " + what + ".");
                }
                break;
            }
        }
    }
    // embargo those we cannot stand; lift it once tempers cool
    if (hasTech(c, ID.tLaw))
        for (int o = FIRST_PRIMATE_CIV; o < (int)civs.size(); o++) {
            if (o == c || !civs[o].alive || !civs[c].met[o]) continue;
            int op = opinion(c, o);
            if (!civs[c].embargo[o] && op <= -35 && !civs[c].war[o] && rng.chance(0.03)) setEmbargo(c, o, true);
            else if (civs[c].embargo[o] && op > -10 && rng.chance(0.1)) setEmbargo(c, o, false);
        }
    // denounce those we despise
    for (int o = FIRST_PRIMATE_CIV; o < (int)civs.size(); o++) {
        if (o == c || !civs[o].alive || !civs[c].met[o] || civs[c].denouncedUntil[o] > turn) continue;
        if (opinion(c, o) <= -40 && rng.chance(0.04)) denounce(c, o);
    }
}

// proposals to the human player
void Game::aiOffers(int c) {
    int p = player;
    if (settings.allAI || p < FIRST_PRIMATE_CIV || !civs[p].alive || !civs[c].met[p] || c == p) return;
    for (auto& d : offers) if (!d.items.empty() && d.a == c) return;  // one open offer at a time
    if (turn - civs[c].lastOffer[p] < dur(10)) return;
    const PrimateDef* pd = primate(c);
    int aggr = pd ? pd->aggression : 5;
    std::vector<Deal> cands;
    auto mk = [&](std::vector<DealItem> items, bool demand = false) {
        Deal d; d.a = c; d.b = p; d.demand = demand;
        for (auto& it : items) if (dealItemValid(d, it)) d.items.push_back(it);
        if (d.items.size() == items.size() || (!d.items.empty() && d.items[0].kind == DI_OPEN_BORDERS)) cands.push_back(d);
    };
    if (civs[c].war[p]) {
        if (peaceValue(c, p) > 0 && turn - civs[c].warSince[p] >= dur(6)) {
            std::vector<DealItem> items = {{DI_PEACE, -1, 0}};
            int my = militaryPower(c) + 5, th = militaryPower(p) + 5;
            if (th > my * 3 / 2 && hasGold(c) && hasGold(p) && civs[c].gold >= 40) items.push_back({DI_GOLD, c, civs[c].gold / 5});
            mk(items);
        }
    } else {
        int att = attitude(c, p);
        int my = militaryPower(c) + 5, th = militaryPower(p) + 5;
        if (aggr >= 7 && att <= 0 && my > th * 2 && hasGold(p) && hasGold(c) && civs[p].gold >= 60 && rng.chance(0.3))
            mk({{DI_GOLD, p, std::min(civs[p].gold / 3, sc(120))}}, true);
        if (att >= 1) mk({{DI_FRIENDSHIP, -1, 0}});
        if (att >= 2 && (pd ? pd->pactLove : 5) >= 5) mk({{DI_DEFENSIVE, -1, 0}});
        if (att >= 1) mk({{DI_RESEARCH, -1, 0}});
        if (att >= 0) mk({{DI_OPEN_BORDERS, c, 0}, {DI_OPEN_BORDERS, p, 0}});
        auto mine = tradeableResources(c, p), theirs = tradeableResources(p, c);
        if (!mine.empty() && !theirs.empty() && att >= 0) mk({{DI_RESOURCE, c, mine[rng.range((int)mine.size())]}, {DI_RESOURCE, p, theirs[rng.range((int)theirs.size())]}});
        // help against an enemy
        for (int x = FIRST_PRIMATE_CIV; x < (int)civs.size(); x++) {
            if (x == c || x == p || !civs[x].alive || !civs[c].war[x] || !civs[p].met[x] || civs[p].war[x]) continue;
            std::vector<DealItem> items = {{DI_WAR_ON, p, x}};
            if (hasGold(c) && hasGold(p) && civs[c].gold >= 50) items.push_back({DI_GOLD, c, civs[c].gold / 3});
            mk(items);
            break;
        }
    }
    for (auto& d : cands) {
        if (dealValue(c, d) < 0) continue;
        civs[c].lastOffer[p] = turn;
        offers.push_back(d);
        int idx = (int)offers.size() - 1;
        std::string body = civs[c].leader + " of the " + civs[c].name + (d.demand ? " demands:" : " proposes:") + "\n";
        for (auto& it : d.items) body += "\n  - " + dealItemText(it, p);
        if (d.demand) body += "\n\nRefuse, and the " + civs[c].name + " will remember it - perhaps with spears.";
        Popup pp;
        pp.kind = POP_DEAL;
        pp.title = d.demand ? "A Demand from the " + civs[c].name : "A Proposal from the " + civs[c].name;
        pp.body = body;
        pp.deal = idx;
        popups.push_back(pp);
        return;
    }
}
