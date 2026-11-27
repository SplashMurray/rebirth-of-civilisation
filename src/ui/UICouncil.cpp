// Rebirth of Civilisation - the World Council screen
// SPDX-License-Identifier: MIT
#include "GameUI.h"
#include <algorithm>

namespace {
const int TOP = 36;  // matches GameUI.cpp
const Color C_TEXT = {234, 238, 242};
const Color C_DIM = {150, 160, 174};
const Color C_PANEL2 = {40, 48, 62};
const Color C_GOLD = {240, 200, 70};
const Color C_ON = {110, 90, 40};
const Color C_BAD = {240, 140, 100};
}  // namespace

void GameUI::renderCouncil() {
    Game& g = *game;
    int p = g.player;
    const Council& k = g.council;
    int pw = std::min(1180, W - 30), ph = H - TOP - 20;
    int px = (W - pw) / 2, py = TOP + 8;
    panel(px, py, pw, ph);
    cv->text(px + 20, py + 12, "World Council", 24, C_TEXT, AL_LEFT, true);
    button(px + pw - 40, py + 10, 28, 28, "X", true, [this] { overlay = OV_NONE; }, "Close (Esc)");
    if (!k.formed) {
        textBlock(px + 20, py + 56, pw - 40,
                  "There is no World Council yet. A people that knows Philosophy - asking why the humans destroyed themselves - and has met every "
                  "other living species can call them together; failing that, the first people with Mass Media will, over the airwaves. It will meet every " + std::to_string(g.dur(30)) + " turns to vote on resolutions, and every second session it will "
                  "elect a chair - who wins nothing by it, but gains real powers, and loses the right to stand again by abusing them.",
                  14, C_DIM, 4);
        return;
    }
    auto civName = [&](int c) { return c == p ? std::string("You") : g.civs[p].met[c] ? "The " + g.civs[c].name : std::string("An unknown people"); };
    int lw = std::min(600, pw / 2 + 10);
    int x = px + 20, y = py + 46;
    int left = g.council.nextSession - g.turn;
    cv->text(x, y, "Founded by " + (k.founder == p ? std::string("you") : "the " + g.civs[k.founder].name) + " on turn " + std::to_string(k.founded) +
                       ".  Next session: turn " + std::to_string(k.nextSession) + " (in " + std::to_string(left) + (left == 1 ? " turn)" : " turns)") +
                       (k.electionNext ? ", with an election." : "."),
             13, C_DIM);
    y += 22;
    {
        std::string chair = k.chair < 0 ? "No chair yet" : "Chair: " + civName(k.chair) + (k.chair == p ? " (since turn " : " (since turn ") + std::to_string(k.termStart) + ")";
        cv->text(x, y, chair, 16, k.chair == p ? C_GOLD : C_TEXT, AL_LEFT, true);
        hot(x, y - 2, 420, 20, nullptr,
            "The chair has two more delegates in every vote, puts a resolution on every agenda, enjoys +10% Culture and +1 Happiness in every "
            "city, and may use each of four powers once a term. Being chair wins nothing by itself. A chair whose abuse of power reaches " +
                std::to_string(ABUSE_BAR) + " may not stand for re-election.");
        y += 22;
        int allies = g.delegates(p, true) - 1;
        cv->text(x, y, "Your delegates: " + std::to_string(g.delegates(p)) + "  (1" + (allies > 0 ? " + " + std::to_string(allies) + " for allied tribes" : std::string()) +
                           (k.chair == p ? " + 2 as chair" : "") + ")",
                 13, C_DIM);
        y += 26;
    }
    // ---- the election
    if (k.electionNext) {
        cv->text(x, y, "Vote for the chair", 15, C_TEXT, AL_LEFT, true);
        y += 22;
        int mine = p < (int)k.ballot.size() ? k.ballot[p] : -1;
        int bx = x;
        for (int c = FIRST_PRIMATE_CIV; c < (int)g.civs.size(); c++) {
            if (!g.isMajor(c) || !g.civs[c].alive || (c != p && !g.civs[p].met[c])) continue;
            std::string why;
            bool ok = g.canStand(c, &why);
            std::string label = c == p ? "Yourselves" : g.civs[c].name;
            int w = cv->textWidth(label, 12, true) + 18;
            if (bx + w > x + lw - 10) { bx = x; y += 30; }
            int cc = c;
            button(bx, y, w, 26, label, ok, [this, cc] { game->setBallot(game->player, cc); },
                   ok ? std::to_string(g.delegates(c, true)) + " delegates of their own" : why, mine == c ? C_ON : C_PANEL2, 12);
            bx += w + 5;
        }
        int w = cv->textWidth("Abstain", 12, true) + 18;
        if (bx + w > x + lw - 10) { bx = x; y += 30; }
        button(bx, y, w, 26, "Abstain", true, [this] { game->setBallot(game->player, -1); }, "", mine < 0 ? C_ON : C_PANEL2, 12);
        y += 40;
    }
    // ---- the agenda
    cv->text(x, y, "The agenda", 15, C_TEXT, AL_LEFT, true);
    y += 22;
    if (k.agenda.empty()) { cv->text(x, y, "Nothing has been proposed.", 13, C_DIM); y += 20; }
    for (int slot = 0; slot < (int)k.agenda.size(); slot++) {
        const Proposal& pr = k.agenda[slot];
        if (pr.kind < 0) {
            if (pr.proposer != p) {
                cv->text(x, y, civName(pr.proposer) + " will put forward a resolution.", 13, C_DIM);
                y += 22;
                continue;
            }
            // our turn to propose
            cv->text(x, y, "Your proposal:", 13, C_GOLD, AL_LEFT, true);
            y += 20;
            int bx = x;
            for (int kd = 0; kd < RS_COUNT; kd++) {
                std::string label = g_resolutions[kd].name;
                int w = cv->textWidth(label, 11, true) + 14;
                if (bx + w > x + lw - 10) { bx = x; y += 26; }
                int kk = kd;
                button(bx, y, w, 23, label, true, [this, kk] { councilKind = kk; councilT1 = councilT2 = -1; }, g_resolutions[kd].desc,
                       councilKind == kd ? C_ON : C_PANEL2, 11);
                bx += w + 4;
            }
            y += 30;
            if (councilKind >= 0 && g_resolutions[councilKind].targets > 0) {
                int need = g_resolutions[councilKind].targets;
                cv->text(x, y + 4, need == 1 ? "On:" : "Between:", 12, C_DIM);
                int tx = x + 60;
                for (int c = FIRST_PRIMATE_CIV; c < (int)g.civs.size(); c++) {
                    if (!g.isMajor(c) || !g.civs[c].alive || (c != p && !g.civs[p].met[c])) continue;
                    std::string label = c == p ? "You" : g.civs[c].name;
                    int w = cv->textWidth(label, 11, true) + 14;
                    if (tx + w > x + lw - 10) { tx = x + 60; y += 26; }
                    int cc = c;
                    bool on = councilT1 == c || councilT2 == c;
                    button(tx, y, w, 23, label, true, [this, cc, need] {
                        if (councilT1 == cc) { councilT1 = councilT2; councilT2 = -1; }
                        else if (councilT2 == cc) councilT2 = -1;
                        else if (councilT1 < 0 || need == 1) councilT1 = cc;
                        else councilT2 = cc;
                    }, "", on ? C_ON : C_PANEL2, 11);
                    tx += w + 4;
                }
                y += 30;
            }
            Proposal draft = pr;
            draft.kind = councilKind; draft.t1 = councilT1; draft.t2 = councilT2;
            std::string why;
            bool ok = councilKind >= 0 && g.validProposal(draft, &why);
            int sl = slot;
            button(x, y, 160, 28, "Propose", ok, [this, sl] {
                if (game->setProposal(sl, councilKind, councilT1, councilT2)) councilKind = councilT1 = councilT2 = -1;
            }, ok ? g.proposalText(draft, p) : councilKind < 0 ? "Choose a resolution" : why, rgb(60, 110, 60), 13);
            if (councilKind >= 0) textBlock(x + 170, y + 2, lw - 180, g_resolutions[councilKind].desc, 11, C_DIM, 2);
            y += 40;
            continue;
        }
        cv->text(x, y, g.proposalText(pr, p), 14, C_TEXT, AL_LEFT, true);
        y += 18;
        cv->text(x, y, "Proposed by " + (pr.proposer == p ? std::string("you") : civName(pr.proposer)) + ".  " + g_resolutions[pr.kind].desc + ".", 11, C_DIM);
        y += 20;
        int v = p < (int)pr.votes.size() ? pr.votes[p] : 0;
        const char* labels[3] = {"Yes", "No", "Abstain"};
        int vals[3] = {1, -1, 0};
        for (int i = 0; i < 3; i++) {
            int vv = vals[i], sl = slot;
            button(x + i * 92, y, 86, 26, labels[i], true, [this, sl, vv] { game->setVote(game->player, sl, vv); }, "", v == vals[i] ? C_ON : C_PANEL2, 12);
        }
        // how the others lean (only those you have met)
        std::string lean;
        int yes = 0, no = 0;
        for (int c = FIRST_PRIMATE_CIV; c < (int)g.civs.size(); c++) {
            if (!g.isMajor(c) || !g.civs[c].alive || c == p || g.human(c)) continue;
            int a = g.aiVoteOn(c, pr);
            if (a > 0) yes += g.delegates(c); else if (a < 0) no += g.delegates(c);
        }
        lean = "The others lean " + std::to_string(yes) + " for, " + std::to_string(no) + " against";
        cv->text(x + 290, y + 6, lean, 11, C_DIM);
        y += 38;
    }
    // ---- right: resolutions in force, members, powers, last session
    int rx = px + 20 + lw + 16, rw = pw - (rx - px) - 20, ry = py + 46;
    int bottom = py + ph - 12;
    cv->text(rx, ry, "In force", 15, C_TEXT, AL_LEFT, true);
    ry += 20;
    bool any = false;
    for (auto& r : k.active) {
        if (r.until <= g.turn) continue;
        any = true;
        Proposal tmp;
        tmp.kind = r.kind; tmp.t1 = r.t1; tmp.t2 = r.t2;
        std::string s = g.proposalText(tmp, p) + "  (" + std::to_string(r.until - g.turn) + " turns)";
        cv->text(rx, ry, s, 12, rgb(170, 220, 170));
        hot(rx, ry - 2, rw, 16, nullptr, g_resolutions[r.kind].desc);
        ry += 17;
    }
    if (!any) { cv->text(rx, ry, "No resolutions.", 12, C_DIM); ry += 17; }
    ry += 10;
    cv->text(rx, ry, "Members", 15, C_TEXT, AL_LEFT, true);
    ry += 20;
    for (int c = FIRST_PRIMATE_CIV; c < (int)g.civs.size(); c++) {
        if (!g.isMajor(c) || !g.civs[c].alive) continue;
        int ab = c < (int)k.abuse.size() ? k.abuse[c] : 0;
        cv->fillCircle(rx + 6, ry + 7, 5, g.civs[p].met[c] || c == p ? g.civs[c].color : rgb(90, 90, 90));
        std::string s = civName(c) + "  -  " + std::to_string(g.delegates(c)) + (g.delegates(c) == 1 ? " delegate" : " delegates");
        if (c == k.chair) s += ", chair";
        cv->text(rx + 16, ry, s, 12, c == k.chair ? C_GOLD : C_TEXT, AL_LEFT, c == k.chair);
        if (ab > 0) {
            std::string as = "abuse " + std::to_string(ab) + "/" + std::to_string(ABUSE_BAR) + (ab >= ABUSE_BAR ? " - barred" : "");
            cv->text(rx + rw - 4, ry, as, 12, C_BAD, AL_RIGHT, true);
        }
        ry += 17;
    }
    ry += 10;
    if (k.chair == p) {
        cv->text(rx, ry, "Your powers as chair (each once a term)", 15, C_GOLD, AL_LEFT, true);
        ry += 22;
        int myAbuse = p < (int)k.abuse.size() ? k.abuse[p] : 0;
        static const char* DESC[CP_COUNT] = {
            "Order two warring peoples to make peace (for 10 turns). Fair between others - it even mends your name; self-serving if it is your own war.",
            "Every member pays you a tenth of their treasury. Pure self-enrichment: abuse of power.",
            "Sanctions without a vote, for 15 turns. Mild abuse against a recent aggressor, serious against anyone else.",
            "Condemn a people before the world: every member resents them. Fair against a recent aggressor, a smear otherwise."};
        for (int pwr = 0; pwr < CP_COUNT; pwr++) {
            bool used = k.powersUsed & (1 << pwr);
            cv->text(rx, ry, CHAIR_POWER_NAMES[pwr], 13, used ? C_DIM : C_TEXT, AL_LEFT, true);
            hot(rx, ry - 2, 160, 18, nullptr, DESC[pwr]);
            if (used) { cv->text(rx + 170, ry, "used this term", 12, C_DIM); ry += 22; continue; }
            int need = pwr == CP_MEDIATE ? 2 : pwr == CP_LEVY ? 0 : 1;
            int tx = rx + 160;
            int rowY = ry;  // the Use button stays on the first line
            if (need == 2) {
                bool anyWar = false;
                for (int a2 = FIRST_PRIMATE_CIV; a2 < (int)g.civs.size(); a2++)
                    for (int b2 = FIRST_PRIMATE_CIV; b2 < (int)g.civs.size(); b2++)
                        if (g.isMajor(a2) && g.isMajor(b2) && g.civs[a2].war[b2]) anyWar = true;
                if (!anyWar) cv->text(tx, ry, "no wars to end", 11, C_DIM);
            }
            if (need > 0) {
                for (int c = FIRST_PRIMATE_CIV; c < (int)g.civs.size(); c++) {
                    if (!g.isMajor(c) || !g.civs[c].alive || (c != p && !g.civs[p].met[c])) continue;
                    if (need == 1 && c == p) continue;
                    if (need == 2) {  // only peoples at war with another
                        bool war = false;
                        for (int o = FIRST_PRIMATE_CIV; o < (int)g.civs.size(); o++) if (g.isMajor(o) && g.civs[c].war[o]) war = true;
                        if (!war) continue;
                    }
                    std::string label = c == p ? "You" : g.civs[c].name;
                    int w = cv->textWidth(label, 10, true) + 10;
                    if (tx + w > rx + rw - 70) { tx = rx + 160; ry += 22; }
                    int cc = c, pp = pwr;
                    bool on = councilPower == pwr && (councilPT1 == c || councilPT2 == c);
                    button(tx, ry - 3, w, 20, label, true, [this, cc, pp, need] {
                        if (councilPower != pp) { councilPower = pp; councilPT1 = councilPT2 = -1; }
                        if (councilPT1 == cc) { councilPT1 = councilPT2; councilPT2 = -1; }
                        else if (councilPT2 == cc) councilPT2 = -1;
                        else if (councilPT1 < 0 || need == 1) councilPT1 = cc;
                        else councilPT2 = cc;
                    }, "", on ? C_ON : C_PANEL2, 10);
                    tx += w + 3;
                }
            }
            int t1 = councilPower == pwr ? councilPT1 : -1, t2 = councilPower == pwr ? councilPT2 : -1;
            std::string why;
            bool ok = g.canUsePower(p, pwr, t1, t2, &why);
            int ab = g.powerAbuse(pwr, t1, t2);
            std::string tip = std::string(DESC[pwr]) + "\n" +
                              (ab > 0 ? "Abuse of power +" + std::to_string(ab) + (myAbuse + ab >= ABUSE_BAR ? ": you could not stand for re-election!" : ".")
                                      : ab < 0 ? "Fair use: it lessens your abuse of power." : "Fair use: no abuse of power.");
            int pp = pwr;
            button(rx + rw - 64, rowY - 4, 62, 22, "Use", ok, [this, pp, t1, t2] {
                if (game->usePower(game->player, pp, t1, t2)) councilPower = councilPT1 = councilPT2 = -1;
            }, ok ? tip : why, ab > 0 ? rgb(120, 60, 50) : rgb(60, 100, 60), 11);
            ry += 26;
        }
        ry += 6;
    }
    if (!k.lastResults.empty() && ry + 40 < bottom) {
        cv->text(rx, ry, "Last session", 15, C_TEXT, AL_LEFT, true);
        ry += 20;
        for (auto& l : k.lastResults) {
            if (ry + 14 > bottom) break;
            ry = textBlock(rx, ry, rw, l, 11, C_DIM, 2) + 4;
        }
    }
}
