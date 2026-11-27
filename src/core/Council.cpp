// Rebirth of Civilisation - the World Council
// A people that knows Philosophy ("Why did they destroy themselves?") and has met every other living species calls
// the great peoples together to form a World Council - or, if none can, the first with Mass Media does, over the
// airwaves, so that the mistakes of the humans are not made again. It meets every 45 turns at standard speed. Each
// people has one delegate, plus one for every minor tribe allied to it.
//
// Every session votes on an agenda of two resolutions, put forward by the chair and by the next strongest delegation:
// treaties against nuclear weapons, shared archives, trade charters, secular accords, protection for the tribes,
// wasteland reclamation, sanctions against one people, or a call for peace between two. A resolution passes with
// more yes than no votes, and holds for a while.
//
// Every second session elects a chair - the first at the council's first session. Being chair wins nothing by
// itself, but it is worth having: two extra delegates in every vote, a place on every agenda, prestige at home
// (+10% Culture, +1 Happiness in every city), and four powers, each usable once a term: mediating peace, levying a
// contribution from every member, emergency sanctions and censure. Used fairly they cost nothing; used for the
// chair's own gain they are an abuse of power - and a chair whose abuse reaches 3 may not stand for re-election
// (abuse fades by 1 every 45 turns at standard speed, but only once out of office). Declaring war on a member or launching a nuclear weapon while chair is abuse too.
// SPDX-License-Identifier: MIT
#include "Game.h"
#include <algorithm>

const ResolutionDef g_resolutions[RS_COUNT] = {
    {"Non-Proliferation Treaty", "No one may build nuclear warheads, and whoever launches one is condemned by every member", 0, 60},
    {"Shared Archives", "+10% Science for every member", 0, 60},
    {"Trade Charter", "Every member may run one more trade route", 0, 60},
    {"Secular Accord", "-10 zeal for every member, but 15% less Faith", 0, 60},
    {"Protection of the Tribes", "No one may declare war on a minor tribe, and wars with tribes end", 0, 60},
    {"Wasteland Reclamation", "Decontaminating fallout takes half as long", 0, 60},
    {"Sanctions", "No one may trade with them: their trade routes and trade deals end, and they earn 10% less Gold", 1, 30},
    {"Call for Peace", "The two peoples must make peace, and keep it for 20 turns", 2, 30},
};
const char* CHAIR_POWER_NAMES[CP_COUNT] = {"Mediate Peace", "Council Levy", "Emergency Sanctions", "Censure"};

static int techPhilosophy() { static int t = techIndex("Philosophy"); return t; }
static int techMassMedia() { static int t = techIndex("Mass Media"); return t; }

// ------------------------------------------------------------ queries
int Game::delegates(int c, bool election) const {
    if (!isMajor(c) || !civs[c].alive) return 0;
    int n = 1;
    for (int t = FIRST_PRIMATE_CIV; t < (int)civs.size(); t++)
        if (isMinor(t) && civs[t].alive && civs[t].ally == c) n++;
    if (!election && council.chair == c) n += 2;
    return n;
}

bool Game::resolutionActive(int kind) const {
    if (!council.formed) return false;
    for (auto& r : council.active) if (r.kind == kind && r.until > turn) return true;
    return false;
}

bool Game::sanctioned(int c) const {
    if (!council.formed) return false;
    for (auto& r : council.active) if (r.kind == RS_SANCTIONS && r.t1 == c && r.until > turn) return true;
    return false;
}

bool Game::validProposal(const Proposal& p, std::string* why) const {
    auto fail = [&](const std::string& w) { if (why) *why = w; return false; };
    if (p.kind < 0 || p.kind >= RS_COUNT) return fail("Nothing proposed");
    const ResolutionDef& d = g_resolutions[p.kind];
    for (auto& r : council.active)
        if (r.kind == p.kind && r.until > turn && (d.targets == 0 || (r.t1 == p.t1 && r.t2 == p.t2))) return fail("Already in force");
    if (d.targets >= 1) {
        if (!isMajor(p.t1) || !civs[p.t1].alive) return fail("Choose a people");
        if (p.kind == RS_SANCTIONS && p.t1 == p.proposer) return fail("You cannot sanction yourselves");
    }
    if (d.targets == 2) {
        if (!isMajor(p.t2) || !civs[p.t2].alive || p.t1 == p.t2) return fail("Choose two peoples");
        if (!civs[p.t1].war[p.t2]) return fail("They are not at war");
    }
    return true;
}

bool Game::canStand(int c, std::string* why) const {
    auto fail = [&](const std::string& w) { if (why) *why = w; return false; };
    if (!isMajor(c) || !civs[c].alive) return fail("Not a member");
    int a = c < (int)council.abuse.size() ? council.abuse[c] : 0;
    if (a >= ABUSE_BAR) return fail("Barred for abuse of power (" + std::to_string(a) + ")");
    return true;
}

std::string Game::proposalText(const Proposal& p, int viewer) const {
    if (p.kind < 0) return "(not yet chosen)";
    std::string s = g_resolutions[p.kind].name;
    auto nm = [&](int c) { return c == viewer ? std::string("you") : "the " + civs[c].name; };
    if (p.kind == RS_SANCTIONS && p.t1 >= 0) s += " on " + nm(p.t1);
    if (p.kind == RS_PEACE && p.t1 >= 0 && p.t2 >= 0) s += " between " + nm(p.t1) + " and " + nm(p.t2);
    return s;
}

// ------------------------------------------------------------ human choices
bool Game::setProposal(int slot, int kind, int t1, int t2) {
    if (slot < 0 || slot >= (int)council.agenda.size()) return false;
    Proposal p = council.agenda[slot];
    p.kind = kind; p.t1 = t1; p.t2 = t2;
    if (!validProposal(p)) return false;
    for (int i = 0; i < (int)council.agenda.size(); i++)
        if (i != slot && council.agenda[i].kind == kind && council.agenda[i].t1 == t1 && council.agenda[i].t2 == t2) return false;
    council.agenda[slot] = p;
    return true;
}

void Game::setVote(int c, int slot, int vote) {
    if (slot < 0 || slot >= (int)council.agenda.size() || c < 0 || c >= (int)civs.size()) return;
    auto& v = council.agenda[slot].votes;
    if ((int)v.size() < (int)civs.size()) v.resize(civs.size(), 0);
    v[c] = std::clamp(vote, -1, 1);
}

void Game::setBallot(int c, int candidate) {
    if (c < 0 || c >= (int)civs.size()) return;
    if ((int)council.ballot.size() < (int)civs.size()) council.ballot.resize(civs.size(), -1);
    council.ballot[c] = candidate;
}

// ------------------------------------------------------------ the chair's powers
int Game::powerAbuse(int power, int t1, int t2) const {
    int ch = council.chair;
    bool recentAggressor = t1 >= 0 && t1 < (int)civs.size() && civs[t1].lastAggression >= turn - dur(30);
    switch (power) {
        case CP_MEDIATE: return (t1 == ch || t2 == ch) ? 3 : -1;  // ending one's own war on the council's authority is self-serving
        case CP_LEVY: return 3;                                    // enriching oneself
        case CP_SANCTION: return recentAggressor ? 1 : 3;          // without a vote; worse against a people that has done nothing
        case CP_CENSURE: return recentAggressor ? 0 : 2;           // fair against an aggressor, a smear otherwise
    }
    return 0;
}

bool Game::canUsePower(int c, int power, int t1, int t2, std::string* why) const {
    auto fail = [&](const std::string& w) { if (why) *why = w; return false; };
    if (!council.formed || council.chair != c) return fail("Only the chair of the World Council");
    if (council.powersUsed & (1 << power)) return fail("Already used this term");
    switch (power) {
        case CP_MEDIATE:
            if (!isMajor(t1) || !isMajor(t2) || t1 == t2 || !civs[t1].alive || !civs[t2].alive) return fail("Choose two peoples at war");
            if (!civs[t1].war[t2]) return fail("They are not at war");
            break;
        case CP_LEVY: break;
        case CP_SANCTION:
        case CP_CENSURE:
            if (!isMajor(t1) || t1 == c || !civs[t1].alive) return fail("Choose another people");
            if (power == CP_SANCTION && sanctioned(t1)) return fail("Already under sanctions");
            break;
        default: return fail("No such power");
    }
    return true;
}

bool Game::usePower(int c, int power, int t1, int t2) {
    if (!canUsePower(c, power, t1, t2)) return false;
    int ab = powerAbuse(power, t1, t2);
    if ((int)council.abuse.size() < (int)civs.size()) council.abuse.resize(civs.size(), 0);
    council.abuse[c] = std::max(0, council.abuse[c] + ab);
    council.powersUsed |= 1 << power;
    std::string who = "The " + civs[c].name + ", chair of the World Council,";
    std::string msg;
    switch (power) {
        case CP_MEDIATE:
            makePeace(t1, t2);
            civs[t1].peaceUntil[t2] = civs[t2].peaceUntil[t1] = std::max(civs[t1].peaceUntil[t2], turn + dur(10));
            msg = who + " mediated peace between the " + civs[t1].name + " and the " + civs[t2].name + ".";
            break;
        case CP_LEVY: {
            int total = 0;
            for (int o = FIRST_PRIMATE_CIV; o < (int)civs.size(); o++) {
                if (o == c || !isMajor(o) || !civs[o].alive) continue;
                int g = std::min(civs[o].gold / 10, sc(150));
                civs[o].gold -= g;
                total += g;
                civs[o].grievance[c] += 10;
            }
            civs[c].gold += total;
            msg = who + " levied " + std::to_string(total) + " Gold from the members \"for the council's work\".";
            break;
        }
        case CP_SANCTION: {
            ActiveResolution r;
            r.kind = RS_SANCTIONS; r.t1 = t1; r.until = turn + dur(15);
            council.active.push_back(r);
            passResolution(r);
            msg = who + " imposed emergency sanctions on the " + civs[t1].name + ".";
            civs[t1].grievance[c] += 20;
            break;
        }
        case CP_CENSURE:
            for (int o = FIRST_PRIMATE_CIV; o < (int)civs.size(); o++)
                if (o != t1 && isMajor(o) && civs[o].alive) civs[o].grievance[t1] += 15;
            civs[t1].grievance[c] += 15;
            msg = who + " censured the " + civs[t1].name + " before the world.";
            break;
    }
    if (ab > 0) msg += " (Abuse of power: " + std::to_string(council.abuse[c]) + " of " + std::to_string(ABUSE_BAR) + ".)";
    for (int v : viewers()) notify(v, v == c ? "You used your power as chair: " + std::string(CHAIR_POWER_NAMES[power]) + "." + (ab > 0 ? " Abuse of power is now " + std::to_string(council.abuse[c]) + " of " + std::to_string(ABUSE_BAR) + "." : "") : msg);
    // every member who sees power abused remembers it
    if (ab > 0)
        for (int o = FIRST_PRIMATE_CIV; o < (int)civs.size(); o++)
            if (o != c && isMajor(o) && civs[o].alive) civs[o].grievance[c] += 4 * ab;
    return true;
}

// ------------------------------------------------------------ resolutions taking effect
void Game::passResolution(const ActiveResolution& r) {
    switch (r.kind) {
        case RS_PEACE:
            if (civs[r.t1].war[r.t2]) makePeace(r.t1, r.t2);
            civs[r.t1].peaceUntil[r.t2] = civs[r.t2].peaceUntil[r.t1] = std::max(civs[r.t1].peaceUntil[r.t2], turn + dur(20));
            break;
        case RS_SANCTIONS:
            for (int o = FIRST_PRIMATE_CIV; o < (int)civs.size(); o++) {
                if (o == r.t1 || !isMajor(o)) continue;
                for (auto& ag : agreements)
                    if (((ag.a == o && ag.b == r.t1) || (ag.a == r.t1 && ag.b == o)) &&
                        (ag.kind == AG_RESOURCE || ag.kind == AG_GOLD_PER_TURN || ag.kind == AG_RESEARCH) && ag.until > turn)
                        ag.until = turn;
                cancelRoutesBetween(o, r.t1, "the World Council's sanctions on the " + civs[r.t1].name);
            }
            markResourcesDirty();
            break;
        case RS_TRIBES:
            for (int a = FIRST_PRIMATE_CIV; a < (int)civs.size(); a++)
                for (int t = FIRST_PRIMATE_CIV; t < (int)civs.size(); t++)
                    if (isMajor(a) && isMinor(t) && civs[a].alive && civs[t].alive && civs[a].war[t]) makePeace(a, t);
            break;
        default: break;
    }
}

// ------------------------------------------------------------ sessions
void Game::prepareAgenda() {
    council.agenda.clear();
    std::vector<int> proposers;
    int first = council.chair >= 0 && civs[council.chair].alive ? council.chair : council.founder;
    if (first >= 0 && civs[first].alive) proposers.push_back(first);
    int best = -1;
    for (int c = FIRST_PRIMATE_CIV; c < (int)civs.size(); c++) {
        if (!isMajor(c) || !civs[c].alive || c == first) continue;
        if (best < 0 || delegates(c, true) > delegates(best, true) || (delegates(c, true) == delegates(best, true) && score(c) > score(best))) best = c;
    }
    if (best >= 0) proposers.push_back(best);
    for (int c : proposers) {
        Proposal p;
        p.proposer = c;
        if (!human(c)) {
            p = aiPropose(c);
            p.proposer = c;
            for (auto& q : council.agenda) if (q.kind == p.kind && q.t1 == p.t1 && q.t2 == p.t2) p.kind = -2;  // the same idea twice: drop it
            if (p.kind == -2) continue;
            if (p.kind < 0) continue;
        } else {
            notify(c, "You may put a resolution before the World Council at its next session (turn " + std::to_string(council.nextSession) + "). See the Council screen (N).");
        }
        p.votes.assign(civs.size(), 0);
        council.agenda.push_back(p);
    }
    council.ballot.assign(civs.size(), -1);
}

void Game::holdSession() {
    council.sessions++;
    council.lastResults.clear();
    // an election first, so the new chair's delegates count in the votes
    if (council.electionNext) {
        std::vector<int> tally(civs.size(), 0);
        for (int c = FIRST_PRIMATE_CIV; c < (int)civs.size(); c++) {
            if (!isMajor(c) || !civs[c].alive) continue;
            int cand = human(c) ? (c < (int)council.ballot.size() ? council.ballot[c] : -1) : aiCandidate(c);
            if (cand >= 0 && canStand(cand)) tally[cand] += delegates(c, true);
        }
        int win = -1;
        for (int c = FIRST_PRIMATE_CIV; c < (int)civs.size(); c++) {
            if (tally[c] <= 0) continue;
            if (win < 0 || tally[c] > tally[win] || (tally[c] == tally[win] && score(c) > score(win))) win = c;
        }
        council.lastElection = tally;
        int old = council.chair;
        if (win >= 0) {
            council.chair = win;
            council.termStart = turn;
            council.powersUsed = 0;
            civs[win].termsAsChair++;
            std::string line = win == old ? "The " + civs[win].name + " were re-elected chair of the World Council (" + std::to_string(tally[win]) + " delegates)."
                                          : "The " + civs[win].name + " were elected chair of the World Council (" + std::to_string(tally[win]) + " delegates).";
            if (old >= 0 && old != win && old < (int)council.abuse.size() && council.abuse[old] >= ABUSE_BAR)
                line += " The " + civs[old].name + " could not stand again after abusing their power.";
            council.lastResults.push_back(line);
        } else council.lastResults.push_back("No chair was elected: no delegate voted.");
    }
    // the resolutions
    for (auto& p : council.agenda) {
        if (p.kind < 0) continue;
        std::string why;
        if (!validProposal(p, &why)) {
            council.lastResults.push_back(proposalText(p, -1) + ": withdrawn (" + why + ").");
            continue;
        }
        int yes = 0, no = 0;
        for (int c = FIRST_PRIMATE_CIV; c < (int)civs.size(); c++) {
            if (!isMajor(c) || !civs[c].alive) continue;
            int v = human(c) ? (c < (int)p.votes.size() ? p.votes[c] : 0) : aiVoteOn(c, p);
            if (v > 0) yes += delegates(c);
            if (v < 0) no += delegates(c);
        }
        bool pass = yes > no;
        std::string line = proposalText(p, -1) + " (proposed by the " + civs[p.proposer].name + "): " + (pass ? "PASSED" : "rejected") + ", " +
                           std::to_string(yes) + " for, " + std::to_string(no) + " against.";
        council.lastResults.push_back(line);
        if (pass) {
            ActiveResolution r;
            r.kind = p.kind; r.t1 = p.t1; r.t2 = p.t2; r.until = turn + dur(g_resolutions[p.kind].duration);
            council.active.push_back(r);
            passResolution(r);
        }
    }
    // tell everyone
    std::string body;
    for (auto& l : council.lastResults) body += l + "\n\n";
    for (int v : viewers()) {
        for (auto& l : council.lastResults) notify(v, "World Council: " + l);
        popup(v, POP_INFO, "The World Council", body + "The council meets again on turn " + std::to_string(turn + dur(30)) + ".");
    }
    council.electionNext = !council.electionNext;
    council.nextSession = turn + dur(30);
    prepareAgenda();
}

void Game::councilTurn() {
    if (!council.formed) {
        // who can call the world together: a people with Philosophy that has met every other living species -
        // or, failing that, the first with Mass Media, whose broadcasts reach peoples it has never met
        int caller = -1;
        bool radio = false;
        for (int c = FIRST_PRIMATE_CIV; c < (int)civs.size() && caller < 0; c++) {
            if (!isMajor(c) || !civs[c].alive || !hasTech(c, techPhilosophy())) continue;
            bool all = true;
            for (int o = FIRST_PRIMATE_CIV; o < (int)civs.size(); o++)
                if (o != c && isMajor(o) && civs[o].alive && !civs[c].met[o]) all = false;
            if (all) caller = c;
        }
        for (int c = FIRST_PRIMATE_CIV; c < (int)civs.size() && caller < 0; c++)
            if (isMajor(c) && civs[c].alive && hasTech(c, techMassMedia())) { caller = c; radio = true; }
        if (caller >= 0) {
            int c = caller;
            council.formed = true;
            council.founded = turn;
            council.founder = c;
            council.nextSession = turn + dur(10);
            council.electionNext = true;
            council.abuse.assign(civs.size(), 0);
            prepareAgenda();
            for (int v : viewers())
                popup(v, POP_INFO, "The World Council",
                      (radio ? "Over the airwaves, the " + civs[c].name + " have called every people of the world together - even those they have never met. "
                             : "Asking why the humans destroyed themselves, the " + civs[c].name + " have called the great peoples together. ") +
                      "The World Council is formed, so that their mistakes are never made again.\n\n"
                      "It meets every " + std::to_string(dur(30)) + " turns, first on turn " + std::to_string(council.nextSession) +
                      ". Each people has one delegate, and one more for every minor tribe allied to it. Every session votes on two "
                      "resolutions, and every second session elects a chair: two more delegates, a place on every agenda, prestige at home "
                      "and four powers to use once a term. Being chair wins nothing by itself - and a chair who abuses those powers may not "
                      "stand again.\n\nSee the Council screen (N).");
        }
        return;
    }
    // resolutions lapse
    for (auto& r : council.active)
        if (r.until == turn)
            for (int v : viewers()) notify(v, "World Council: " + proposalText(Proposal{r.kind, r.t1, r.t2, -1, {}}, v) + " has lapsed.");
    council.active.erase(std::remove_if(council.active.begin(), council.active.end(), [&](const ActiveResolution& r) { return r.until <= turn; }),
                         council.active.end());
    // abuse fades, slowly - but not while the abuser still holds the chair: it is judged at the next election
    if (turn > council.founded && (turn - council.founded) % std::max(1, dur(30)) == 0)
        for (int c = 0; c < (int)council.abuse.size(); c++)
            if (c != council.chair) council.abuse[c] = std::max(0, council.abuse[c] - 1);
    if (council.chair >= 0 && !civs[council.chair].alive) council.chair = -1;
    if (turn == council.nextSession - dur(5) || (dur(5) == 0 && turn == council.nextSession - 1))
        for (int v : viewers())
            notify(v, "The World Council meets in " + std::to_string(council.nextSession - turn) + " turns" + (council.electionNext ? ", and will elect its chair" : "") +
                          ". Cast your votes on the Council screen (N).");
    if (turn >= council.nextSession) holdSession();
}

// ------------------------------------------------------------ the AI
int Game::aiResolutionValue(int c, const Proposal& p) const {
    const PrimateDef* pd = primate(c);
    int aggr = pd ? pd->aggression : 5, piety = pd ? pd->piety : 5, greed = pd ? pd->greed : 5;
    int v = 0;
    switch (p.kind) {
        case RS_NONPROLIF: {
            bool rivalNukes = false;
            for (int o = FIRST_PRIMATE_CIV; o < (int)civs.size(); o++) if (o != c && isMajor(o) && civs[o].warheads > 0) rivalNukes = true;
            v = civs[c].warheads > 0 || (hasTech(c, ID.tFission) && aggr >= 7) ? -25 : 12 + (rivalNukes ? 15 : 0);
            break;
        }
        case RS_ARCHIVES: {  // good for all - less so for the people already ahead
            int ahead = 0;
            for (int o = FIRST_PRIMATE_CIV; o < (int)civs.size(); o++)
                if (o != c && isMajor(o) && civs[o].alive && civs[o].sciencePerTurn > civs[c].sciencePerTurn) ahead++;
            v = ahead == 0 ? -6 : 8 + ahead;
            break;
        }
        case RS_TRADE: v = tradeCapacity(c) > 0 ? 6 + greed : -2; break;
        case RS_SECULAR: v = 10 - piety * 2 + civs[c].zealPenalty / 2; break;
        case RS_TRIBES: {
            int allies = 0;
            for (int t = FIRST_PRIMATE_CIV; t < (int)civs.size(); t++) if (isMinor(t) && civs[t].ally == c) allies++;
            v = 6 - aggr + allies * 4;
            break;
        }
        case RS_RECLAIM: {
            int fallout = 0;
            for (auto& t : tiles) if (t.owner == c && t.fallout > 0) fallout++;
            v = fallout > 0 ? 6 + std::min(10, fallout) : 1;
            break;
        }
        case RS_SANCTIONS:
            if (p.t1 == c) return -100;
            v = -opinion(c, p.t1) / 3 + (civs[c].war[p.t1] ? 20 : 0);
            for (auto& rt : routes)
                if (rt.from >= 0 && rt.to >= 0 && ((rt.owner == c && cities[rt.to].owner == p.t1) || (rt.owner == p.t1 && cities[rt.to].owner == c))) v -= 6;
            for (auto& ag : agreements) if (ag.until > turn && ((ag.a == c && ag.b == p.t1) || (ag.a == p.t1 && ag.b == c))) v -= 5;
            break;
        case RS_PEACE:
            if (p.t1 == c || p.t2 == c) {
                int o = p.t1 == c ? p.t2 : p.t1;
                v = peaceValue(c, o) > 0 ? 20 : -15;
            } else {
                v = 6 - aggr / 2;
                // do not save an enemy from a beating
                for (int x : {p.t1, p.t2}) if (civs[c].war[x]) v -= 15;
            }
            break;
    }
    if (p.proposer >= 0 && p.proposer != c) v += attitude(c, p.proposer) * 2;
    return v;
}

int Game::aiVoteOn(int c, const Proposal& p) const {
    int v = aiResolutionValue(c, p);
    return v > 0 ? 1 : v < 0 ? -1 : 0;
}

Proposal Game::aiPropose(int c) const {
    Proposal best;
    int bv = -1000;
    auto consider = [&](int kind, int t1, int t2) {
        Proposal p;
        p.kind = kind; p.t1 = t1; p.t2 = t2; p.proposer = c;
        if (!validProposal(p)) return;
        // a proposal must be likely to pass: count the delegates of those who would support it
        int yes = 0, no = 0;
        for (int o = FIRST_PRIMATE_CIV; o < (int)civs.size(); o++) {
            if (!isMajor(o) || !civs[o].alive) continue;
            int vv = o == c ? 1 : aiVoteOn(o, p);
            if (vv > 0) yes += delegates(o); else if (vv < 0) no += delegates(o);
        }
        int v = aiResolutionValue(c, p) + (yes > no ? 10 : -10) + const_cast<Game*>(this)->rng.range(4);
        if (v > bv) { bv = v; best = p; }
    };
    for (int k = 0; k < RS_COUNT; k++) {
        if (g_resolutions[k].targets == 0) consider(k, -1, -1);
        else if (k == RS_SANCTIONS) {
            for (int o = FIRST_PRIMATE_CIV; o < (int)civs.size(); o++)
                if (o != c && isMajor(o) && civs[o].alive && civs[c].met[o]) consider(k, o, -1);
        } else {
            for (int a = FIRST_PRIMATE_CIV; a < (int)civs.size(); a++)
                for (int b = a + 1; b < (int)civs.size(); b++)
                    if (isMajor(a) && isMajor(b) && civs[a].alive && civs[b].alive && civs[a].war[b]) consider(k, a, b);
        }
    }
    if (bv <= 0) best.kind = -1;
    return best;
}

int Game::aiCandidate(int c) const {
    // the three leading peoples stand themselves; the others back whom they like best, never an abuser of power
    std::vector<int> majors;
    for (int o = FIRST_PRIMATE_CIV; o < (int)civs.size(); o++) if (isMajor(o) && civs[o].alive) majors.push_back(o);
    std::sort(majors.begin(), majors.end(), [&](int a, int b) { return score(a) > score(b); });
    bool contender = false;
    for (size_t i = 0; i < majors.size() && i < 3; i++) if (majors[i] == c) contender = true;
    if (contender && canStand(c)) return c;
    int best = -1, bv = -100000;
    for (size_t i = 0; i < majors.size(); i++) {
        int o = majors[i];
        if (o == c || !canStand(o)) continue;
        int v = opinion(c, o) + (i < 3 ? 15 : 0) - (o < (int)council.abuse.size() ? council.abuse[o] * 20 : 0);
        if (o == council.chair) v += 8;  // a fair chair is trusted
        if (civs[c].war[o]) v -= 60;
        if (v > bv) { bv = v; best = o; }
    }
    if (best < 0 && canStand(c)) best = c;
    return best;
}

void Game::aiChairPowers(int c) {
    if (!council.formed || council.chair != c) return;
    const PrimateDef* pd = primate(c);
    int greed = pd ? pd->greed : 5, loyalty = pd ? pd->loyalty : 5, aggr = pd ? pd->aggression : 5;
    int sk = skillOf(c);
    int abuse = c < (int)council.abuse.size() ? council.abuse[c] : 0;
    // would this keep us electable? A clever people guards its reputation; a greedy or faithless one is tempted.
    auto tempting = [&](int ab) {
        if (abuse + ab < ABUSE_BAR) return true;
        bool shameless = greed >= 8 || loyalty <= 3;
        return shameless && (sk <= 1 || rng.chance(0.3));
    };
    // mediate between two others at war: costs nothing and earns trust
    if (!(council.powersUsed & (1 << CP_MEDIATE))) {
        for (int a = FIRST_PRIMATE_CIV; a < (int)civs.size() && !(council.powersUsed & (1 << CP_MEDIATE)); a++)
            for (int b = a + 1; b < (int)civs.size(); b++) {
                if (!isMajor(a) || !isMajor(b) || !civs[a].war[b] || a == c || b == c) continue;
                if (turn - civs[a].warSince[b] < dur(10) || civs[c].war[a] || civs[c].war[b]) continue;
                if (attitude(c, a) < 0 && attitude(c, b) < 0) continue;
                usePower(c, CP_MEDIATE, a, b);
                break;
            }
        // or end our own losing war on the council's authority
        if (!(council.powersUsed & (1 << CP_MEDIATE)))
            for (int o = FIRST_PRIMATE_CIV; o < (int)civs.size(); o++)
                if (isMajor(o) && civs[c].war[o] && militaryPower(c) * 10 < militaryPower(o) * 7 && tempting(3)) { usePower(c, CP_MEDIATE, c, o); break; }
    }
    // a levy, when the treasury is low and the greed is high
    if (!(council.powersUsed & (1 << CP_LEVY)) && hasGold(c) && greed >= 6 && civs[c].gold < sc(100) && rng.chance(0.1) && tempting(3))
        usePower(c, CP_LEVY, -1, -1);
    // censure an aggressor, sanction an enemy
    for (int o = FIRST_PRIMATE_CIV; o < (int)civs.size(); o++) {
        if (!isMajor(o) || o == c || !civs[o].alive || !civs[c].met[o]) continue;
        if (!(council.powersUsed & (1 << CP_CENSURE)) && civs[o].lastAggression >= turn - dur(10) && attitude(c, o) < 0 && tempting(powerAbuse(CP_CENSURE, o, -1)))
            usePower(c, CP_CENSURE, o, -1);
        if (!(council.powersUsed & (1 << CP_SANCTION)) && civs[c].war[o] && aggr >= 5 && rng.chance(0.15) && tempting(powerAbuse(CP_SANCTION, o, -1)))
            usePower(c, CP_SANCTION, o, -1);
    }
}
