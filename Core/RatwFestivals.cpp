// Festivals that draw players (Docs/Design/54-gathering-places.md, 6; Phase 6). On a town's festival day (the season's
// 46th, or one a DM called: World::dayPlan's rule) the programme runs at its square:
// - 12:00 the feast: a wolf at the square is given the meal residents are, from the town's store, once. Every game hour
//   a wolf spends at the square from 12 to 23 builds rested practice (half a day's worth, within its most).
// - Contests, each signed up for at the square, the board or the inn until it starts, 1p into its pot. Two or three
//   residents enter too (from purses of 10p or more), and the town adds 2p an entrant, up to 10p, from its own purse
//   (the user, 2026-10-08). Two thirds of the pot to the winner, a third to the second; with fewer than two entrants,
//   or no winner, the pennies go back to whoever put them in.
//   - 13:00 races: a loop of four marks round the square and back, timed from where the server has the racers; residents
//     are timed from their DEX and age. Three minutes.
//   - 14:00 tug-of-war: two teams; PULL on the beat (every 1.2 s) counts STR × the stamina left; residents pull too. The
//     marker moves by the difference; 3 tiles or a minute decides it.
//   - 15:00 howling: in turn, 15 s each; a howl's carry from stamina and howling skill, times a roll, and the crowd's
//     cheers (+5% each, at most +30%; one cheer a wolf).
//   - 16:00 the sparring tourney: a bracket of bouts to a yield, a round every half minute, decided from strength,
//     dexterity and a roll: nothing lasting (not yet real duels).
//   - 12:00-17:00 the hunting contest: an entrant's best single kill, the species' health × how clean it was.
//   - 19:00 storytelling: entrants take the middle in turn, 5 minutes each; the winner is starred (doc 51's stars) by
//     the most different wolves at the square, none counting themself. No stars, no winner.
//   - 20:00 the crier (doc 56's slot, empty); 21:00 games at the inn (the tables, doc 54's Phase 5).
// Winners are called by the steward to the square, recorded (`festival won`), and known to the town's residents that day.
#include "RatwGame.h"

#include "RatwCalendar.h"
#include "RatwPractice.h"
#include "RatwWild.h"

#include <algorithm>
#include <cmath>
#include <functional>

namespace ratw::game
{
using json::Value;

namespace
{
constexpr std::int64_t Entry = 1, TownPerEntrant = 2, TownMost = 10, ResidentPurseLeast = 10;
constexpr double SquareReach = 12, RaceSeconds = 180, BeatSeconds = 1.2, BeatSlack = .35, TugSeconds = 60, TugWins = 3,
                 HowlSeconds = 15, BoutSeconds = 30, TellSeconds = 300, StarsSeconds = 120;

struct Slot
{
    double hour;
    const char* kind;
    const char* what;
};
const Slot Programme[] = {
    {12, "feast", "The feast; the hunting contest opens"},
    {13, "race", "Races"},
    {14, "tug", "Tug-of-war"},
    {15, "howl", "Howling"},
    {16, "tourney", "The sparring tourney"},
    {17, "hunt", "The hunting contest judged"},
    {19, "story", "The storytelling contest"},
    {20, "crier", "The crier"},
    {21, "games", "Games at the inn"},
};
const char* const Contests[] = {"race", "tug", "howl", "tourney", "hunt", "story"};
double startHour(const std::string& kind) { return kind == "hunt" ? 12 : kind == "race" ? 13 : kind == "tug" ? 14 : kind == "howl" ? 15 : kind == "tourney" ? 16 : 19; }
bool isContest(const std::string& kind) { return std::any_of(std::begin(Contests), std::end(Contests), [&](const char* k) { return kind == k; }); }
std::string contestWords(const std::string& kind)
{
    return kind == "race" ? "the race" : kind == "tug" ? "the tug-of-war" : kind == "howl" ? "the howling" : kind == "tourney" ? "the sparring tourney"
         : kind == "hunt" ? "the hunting contest" : "the storytelling";
}
double unit(const std::string& key) { return double(std::hash<std::string>{}(key) % 100000) / 100000; }
double hourOf(double days) { return (days - std::floor(days)) * 24; }
template <class F, class C> std::string pot(const F& f, const C& c) { return "fest:" + f.community + ":" + std::to_string(f.day) + ":" + c.kind; }
} // namespace

std::string Game::festivalOn(const std::string& community, std::int64_t day) const
{
    for (const auto& called : world_.calledFestivals())
        if (called.day == day && called.community == community)
            return called.name.empty() ? world_.festivalName(community, int(calendar::calendarAt(double(day)).season)) : called.name;
    return calendar::festivalDay(double(day) + .5) ? world_.festivalName(community, int(calendar::calendarAt(double(day) + .5).season)) : std::string();
}

Game::Fair* Game::fairToday(const std::string& community)
{
    if (community.empty())
        return nullptr;
    const auto today = std::int64_t(std::floor(world_.calendarDays()));
    const auto key = community + "|" + std::to_string(today);
    if (const auto found = fairs_.find(key); found != fairs_.end())
        return &found->second;
    const auto name = festivalOn(community, today);
    if (name.empty() || !world_.marketSpot(community))
        return nullptr;
    auto& f = fairs_[key];
    f.community = community, f.name = name, f.day = today;
    for (const char* kind : Contests)
        f.contests[kind].kind = kind;
    return &f;
}

bool Game::atSquare(const Entity& e, const std::string& community)
{
    const auto* at = world_.marketSpot(community);
    return at && e.cellId == at->cell && std::hypot(e.position.x - at->x, e.position.y - at->y) <= SquareReach;
}

void Game::festivalSay(const std::string& community, const std::vector<std::string>& also, const std::function<std::string(const std::string&)>& line)
{
    // Everyone at the square, and these besides, told the line (each naming wolves as they know them).
    for (auto* c : clients_)
        if (c)
            if (const auto* e = world_.entity(c->entityId);
                e && (atSquare(*e, community) || std::find(also.begin(), also.end(), c->entityId) != also.end()))
                system(c, line(c->entityId));
}

void Game::endContest(Fair& f, Contest& c, const std::vector<std::string>& ranked, const std::vector<std::string>& losers)
{
    // The pot: two thirds to the winner (a tug's winning team, shared), a third to the second (the losing team); with no
    // winner, back to those who put it in.
    auto& society = world_.society();
    const auto account = pot(f, c);
    const auto* held = society.account(account);
    const auto coins = held ? held->cash : 0;
    c.done = true;
    const auto treasury = society.treasuryOf(f.community);
    if (ranked.empty())
    {
        for (const auto& [who, n] : c.paid)
            society.shift(account, who.empty() ? treasury : who, "", 0, std::min(n, society.spendable(account)), "festival entry returned");
        if (c.result.empty())
            c.result = "No winner in " + contestWords(c.kind) + " this year.";
        const auto line = c.result;
        festivalSay(f.community, c.entrants, [&](const std::string&) { return line; });
    }
    else
    {
        const bool team = !losers.empty();
        const std::vector<std::string> first = team ? ranked : std::vector<std::string>{ranked[0]};
        std::vector<std::string> second = team ? losers : ranked.size() > 1 ? std::vector<std::string>{ranked[1]} : std::vector<std::string>{};
        const auto toSecond = second.empty() ? 0 : coins / 3;
        const auto toFirst = coins - toSecond;
        const auto share = [&](const std::vector<std::string>& who, std::int64_t n) {
            if (who.empty() || n <= 0)
                return;
            const auto each = n / std::int64_t(who.size());
            auto rest = n - each * std::int64_t(who.size());
            for (const auto& w : who)
            {
                const auto give = each + (rest > 0 ? 1 : 0);
                rest -= rest > 0;
                if (give > 0)
                    society.shift(account, w, "", 0, give, "a festival prize");
            }
        };
        share(first, toFirst);
        share(second, toSecond);
        c.winners = {ranked[0]};
        if (!team && ranked.size() > 1)
            c.winners.push_back(ranked[1]);
        world_.recordEvent({"festival won", ranked[0], {}, world_.marketSpot(f.community)->cell, 0, 0, c.kind, int(c.entrants.size()), toFirst,
                            f.name});
        festivalSay(f.community, c.entrants, [&](const std::string& viewer) {
            std::string line = "The steward calls: " + names::capitalised(ranked[0] == viewer ? std::string("you") : labelFor(viewer, ranked[0]));
            if (team)
                line += (ranked.size() > 1 ? "'s team" : "") + std::string(ranked[0] == viewer ? " win " : " wins ") + contestWords(c.kind);
            else
            {
                line += std::string(ranked[0] == viewer ? " win " : " wins ") + contestWords(c.kind);
                if (ranked.size() > 1)
                    line += ", " + (ranked[1] == viewer ? std::string("you") : labelFor(viewer, ranked[1])) + " second";
            }
            return line + " at " + f.name + (toFirst > 0 ? " (" + std::to_string(toFirst) + "p)" : "") + "!";
        });
        for (const auto& w : c.entrants)
            record(Economy, w);
    }
    if (const auto* left = society.account(account); left && left->cash > 0)
        society.shift(account, treasury, "", 0, left->cash, "festival pot remainder");
    society.closeAccount(account);
}

void Game::beginContest(Fair& f, Contest& c)
{
    auto& society = world_.society();
    c.begun = true;
    c.begunAt = c.turnAt = world_.time();
    const auto account = pot(f, c);
    society.openAccount(account);
    // Residents enter: two or three grown townsfolk who can spare a penny (none in the storytelling: stars are players').
    if (c.kind != "story")
    {
        std::vector<std::pair<double, std::string>> folk;
        for (const auto& [id, life] : society.state().residents)
        {
            const auto* e = world_.entity(id);
            const auto* purse = society.account(id);
            if (!e || e->dead || e->age < 16 || !purse || purse->cash < ResidentPurseLeast || life.task == "sleep" ||
                world_.lawTown(life.homeCell) != f.community || world_.seatedResident(id) || world_.guardOnDuty(id))
                continue;
            double order = unit(id + "|" + std::to_string(f.day) + "|" + c.kind);
            if (c.kind == "tourney" && life.role == "guard")
                order -= 1;                         // (Off-duty guards first into the ring.)
            folk.push_back({order, id});
        }
        std::sort(folk.begin(), folk.end());
        const int wanted = 2 + int(unit(f.community + std::to_string(f.day) + c.kind) * 2);
        for (int i = 0; i < wanted && i < int(folk.size()); ++i)
            if (society.shift(folk[std::size_t(i)].second, account, "", 0, Entry, "festival entry"))
            {
                c.entrants.push_back(folk[std::size_t(i)].second);
                c.paid[folk[std::size_t(i)].second] += Entry;
            }
    }
    // The town adds to the pot.
    const auto treasury = society.treasuryOf(f.community);
    const auto add = std::min({TownMost, TownPerEntrant * std::int64_t(c.entrants.size()), society.spendable(treasury)});
    if (add > 0 && society.shift(treasury, account, "", 0, add, "the town's festival purse"))
        c.paid[""] += add;
    if (c.entrants.size() < 2)
    {
        c.result = "Too few for " + contestWords(c.kind) + " this year.";
        endContest(f, c, {});
        return;
    }
    const auto* at = world_.marketSpot(f.community);
    const auto names = [&](const std::string& viewer) {
        std::string out;
        for (std::size_t i = 0; i < c.entrants.size(); ++i)
            out += (i ? ", " : "") + (c.entrants[i] == viewer ? std::string("you") : labelFor(viewer, c.entrants[i]));
        return out;
    };
    std::string how;
    if (c.kind == "race")
    {
        // Four marks round the square: the farthest crowd spot in each quarter, in a loop.
        std::map<int, std::pair<double, Spot>> quarters;
        for (const auto& s : world_.crowdSpots(f.community))
        {
            const double dx = s.x - at->x, dy = s.y - at->y, d = std::hypot(dx, dy);
            const int q = dx >= 0 ? (dy < 0 ? 0 : 1) : (dy >= 0 ? 2 : 3);
            if (!quarters.count(q) || d > quarters[q].first)
                quarters[q] = {d, s};
        }
        double loop = 0;
        Spot last = *at;
        for (const auto& [q, s] : quarters)
        {
            c.marks.push_back(s.second);
            loop += std::hypot(s.second.x - last.x, s.second.y - last.y);
            last = s.second;
        }
        loop += std::hypot(at->x - last.x, at->y - last.y);
        for (const auto& id : c.entrants)
            if (const auto* e = world_.entity(id); e && e->npc)
            {
                const double speed = std::max(3., 5 + e->dexterity / 25 - std::max(0, e->age - 40) * .05) * (.9 + .2 * unit(id + "|race|" + std::to_string(f.day)));
                c.score[id] = loop / speed + 2;
            }
        how = "Round the four marks and back to the middle; go!";
    }
    else if (c.kind == "tug")
    {
        // Two teams, wolves spread between them.
        int players = 0, folk = 0;
        for (const auto& id : c.entrants)
        {
            const auto* e = world_.entity(id);
            c.team[id] = e && !e->npc ? players++ % 2 : (folk++ + 1) % 2;
        }
        how = "PULL on the beat!";
    }
    else if (c.kind == "howl")
        how = "Each howls in turn; cheer the one you favour.";
    else if (c.kind == "tourney")
    {
        c.bracket = c.entrants;
        std::sort(c.bracket.begin(), c.bracket.end(), [&](const std::string& a, const std::string& b) {
            return unit(a + std::to_string(f.day)) < unit(b + std::to_string(f.day));
        });
        if (c.bracket.size() > 8)
            c.bracket.resize(8);
        how = "Bouts to a yield, a round every half minute.";
    }
    else if (c.kind == "hunt")
    {
        for (const auto& id : c.entrants)
            if (const auto* e = world_.entity(id); e && e->npc)
                c.score[id] = (10 + 50 * unit(id + "|hunt|" + std::to_string(f.day))) * (.6 + unit(id + "|clean|" + std::to_string(f.day)));
        how = "The best single kill by five o'clock wins.";
    }
    else if (c.kind == "story")
        how = "Each takes the middle in turn; star the teller you liked best.";
    festivalSay(f.community, c.entrants, [&](const std::string& viewer) {
        return names::capitalised(contestWords(c.kind)) + " at " + f.name + " begins: " + names(viewer) + ". " + how;
    });
    if (c.kind == "story")
    {
        const auto first = c.entrants.front();
        festivalSay(f.community, c.entrants, [&](const std::string& viewer) {
            return names::capitalised(first == viewer ? std::string("you take") : labelFor(viewer, first) + " takes") + " the middle of the square to tell a story.";
        });
    }
}

void Game::runContest(Fair& f, Contest& c)
{
    const double t = world_.time() - c.begunAt;
    const auto rankBy = [&](bool lowFirst) {
        std::vector<std::string> ranked;
        for (const auto& [id, s] : c.score)
            ranked.push_back(id);
        std::stable_sort(ranked.begin(), ranked.end(), [&](const std::string& a, const std::string& b) {
            return lowFirst ? c.score[a] < c.score[b] : c.score[a] > c.score[b];
        });
        return ranked;
    };
    if (c.kind == "race")
    {
        bool running = false;
        for (const auto& id : c.entrants)
        {
            const auto* e = world_.entity(id);
            if (!e || e->npc || c.score.count(id))
                continue;
            int& next = c.mark[id];
            const Spot target = next < int(c.marks.size()) ? c.marks[std::size_t(next)] : *world_.marketSpot(f.community);
            if (e->cellId == target.cell && std::hypot(e->position.x - target.x, e->position.y - target.y) <= 1.5)
            {
                if (++next > int(c.marks.size()))
                {
                    c.score[id] = t;
                    if (auto* cl = clientOf(id))
                        system(cl, "Home, in " + std::to_string(int(std::round(t))) + " seconds.");
                    continue;
                }
                if (auto* cl = clientOf(id))
                    system(cl, next < int(c.marks.size()) ? "A mark! On to the next." : "The last mark! Back to the middle.");
            }
            running = true;
        }
        if (!running || t > RaceSeconds)
            endContest(f, c, rankBy(true));
    }
    else if (c.kind == "tug")
    {
        // Beats as they pass: each counts its pulls, players' (pulled on it) and residents' (three in four).
        const long beats = long(std::floor(t / BeatSeconds));
        while (c.beat < beats && !c.done)
        {
            double force[2] = {0, 0};
            for (const auto& id : c.entrants)
            {
                const auto* e = world_.entity(id);
                if (!e)
                    continue;
                const int side = c.team[id];
                if (e->npc)
                {
                    if (unit(id + "|beat|" + std::to_string(c.beat) + std::to_string(f.day)) < .75)
                        force[side] += e->strength * .9;
                }
                else if (const auto p = c.pulledBeat.find(id); p != c.pulledBeat.end() && p->second == c.beat)
                    force[side] += e->strength * std::clamp(e->stamina / 100, .2, 1.);
            }
            c.marker += (force[0] - force[1]) * .005;
            ++c.beat;
            if (std::abs(c.marker) >= TugWins)
                break;
        }
        if (std::abs(c.marker) >= TugWins || t >= TugSeconds)
        {
            const int won = c.marker >= 0 ? 0 : 1;
            std::vector<std::string> winners, losers;
            for (const auto& id : c.entrants)
                (c.team[id] == won ? winners : losers).push_back(id);
            if (std::abs(c.marker) < 1e-9 || winners.empty())
            {
                c.result = "The rope doesn't move: no winner in the tug-of-war.";
                endContest(f, c, {});
            }
            else
                endContest(f, c, winners, losers);
        }
    }
    else if (c.kind == "howl")
    {
        if (c.turn >= c.entrants.size())
        {
            // The crowd's cheers, then the carry ranks them.
            std::map<std::string, int> cheers;
            for (const auto& [by, whom] : c.cheered)
                ++cheers[whom];
            for (auto& [id, s] : c.score)
                s *= 1 + std::min(.3, .05 * cheers[id]);
            endContest(f, c, rankBy(false));
            return;
        }
        const auto& who = c.entrants[c.turn];
        const auto* e = world_.entity(who);
        const double since = world_.time() - c.turnAt;
        if (e && e->npc && since >= 3)
        {
            const double skill = tavern::residentSkill(who + "|howl", e->age);
            c.score[who] = (60 + skill * .4) * (.75 + .5 * unit(who + "|howl|" + std::to_string(f.day)));
            festivalSay(f.community, c.entrants, [&](const std::string& viewer) {
                return names::capitalised(labelFor(viewer, who)) + " howls; it rolls round the square.";
            });
        }
        if (c.score.count(who) || !e || since >= HowlSeconds)
        {
            c.score.emplace(who, 0.);
            ++c.turn;
            c.turnAt = world_.time();
            if (c.turn < c.entrants.size())
                if (auto* next = clientOf(c.entrants[c.turn]))
                    system(next, "Your turn to howl!");
        }
    }
    else if (c.kind == "tourney")
    {
        if (world_.time() - c.turnAt < BoutSeconds)
            return;
        c.turnAt = world_.time();
        // A round: pairs in the bracket's order, an odd one out through by itself.
        std::vector<std::string> through;
        std::string lastLoser;
        for (std::size_t i = 0; i + 1 < c.bracket.size(); i += 2)
        {
            const auto &a = c.bracket[i], &b = c.bracket[i + 1];
            const auto* ea = world_.entity(a);
            const auto* eb = world_.entity(b);
            const auto roll = [&](const Entity* e, const std::string& id) {
                return e ? (e->strength + e->dexterity) / 2 + 50 * unit(id + "|bout|" + std::to_string(c.bracket.size()) + std::to_string(f.day)) : 0.;
            };
            const bool aWins = roll(ea, a) >= roll(eb, b);
            const auto& winner = aWins ? a : b;
            const auto& loser = aWins ? b : a;
            through.push_back(winner);
            lastLoser = loser;
            festivalSay(f.community, c.entrants, [&](const std::string& viewer) {
                return "In the ring: " + names::capitalised(loser == viewer ? std::string("you yield") : labelFor(viewer, loser) + " yields") + " to " +
                       (winner == viewer ? std::string("you") : labelFor(viewer, winner)) + ".";
            });
        }
        if (c.bracket.size() % 2)
            through.push_back(c.bracket.back());
        c.bracket = through;
        if (c.bracket.size() <= 1)
            endContest(f, c, c.bracket.empty() ? std::vector<std::string>{} : std::vector<std::string>{c.bracket[0], lastLoser});
    }
    else if (c.kind == "hunt")
    {
        if (hourOf(world_.calendarDays()) >= 17)
        {
            std::vector<std::string> ranked;
            for (const auto& id : rankBy(false))
                if (c.score[id] > 0)
                    ranked.push_back(id);
            endContest(f, c, ranked);
        }
    }
    else if (c.kind == "story")
    {
        const double since = world_.time() - c.turnAt;
        if (c.turn < c.entrants.size())
        {
            if (since < TellSeconds)
                return;
            ++c.turn;
            c.turnAt = world_.time();
            if (c.turn < c.entrants.size())
            {
                const auto next = c.entrants[c.turn];
                festivalSay(f.community, c.entrants, [&](const std::string& viewer) {
                    return names::capitalised(next == viewer ? std::string("you take") : labelFor(viewer, next) + " takes") + " the middle of the square.";
                });
            }
            else
                festivalSay(f.community, c.entrants, [&](const std::string&) {
                    return "The tales are told. Star the teller you liked best (not yourself): two minutes.";
                });
            return;
        }
        if (since < StarsSeconds && hourOf(world_.calendarDays()) < 21)
            return;
        std::vector<std::pair<std::string, std::size_t>> tally;
        for (const auto& id : c.entrants)
            if (const auto g = c.givers.find(id); g != c.givers.end() && !g->second.empty())
                tally.push_back({id, g->second.size()});
        std::stable_sort(tally.begin(), tally.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
        std::vector<std::string> ranked;
        for (const auto& [id, n] : tally)
            ranked.push_back(id);
        if (ranked.empty())
            c.result = "Too quiet a crowd this year: no winner in the storytelling.";
        endContest(f, c, ranked);
    }
}

void Game::tendFestivals(double dt)
{
    fairsAccumulator_ += dt;
    if (fairsAccumulator_ < 1)
        return;
    fairsAccumulator_ = 0;
    const double days = world_.calendarDays(), hour = hourOf(days);
    const auto today = std::int64_t(std::floor(days));
    auto& society = world_.society();
    // Fairs where wolves are on the day.
    for (auto* c : clients_)
        if (c)
            if (const auto* e = world_.entity(c->entityId))
                fairToday(world_.lawTown(e->cellId));
    // Kills on hunts, for the hunting contest.
    const auto kills = world_.takeHunted();
    for (auto it = fairs_.begin(); it != fairs_.end();)
    {
        auto& f = it->second;
        if (f.day != today)
        {
            for (auto& [kind, c] : f.contests)
                if (c.begun && !c.done)
                {
                    c.result = "The day is over: no winner in " + contestWords(kind) + ".";
                    endContest(f, c, {});
                }
                else if (!c.begun && !c.paid.empty())
                    endContest(f, c, {});
            it = fairs_.erase(it);
            continue;
        }
        // The feast, and rested time at the square.
        if (hour >= 12 && hour < 23)
            for (auto* cl : clients_)
                if (cl)
                    if (auto* e = world_.entity(cl->entityId); e && !e->npc && atSquare(*e, f.community))
                    {
                        if (f.fed.insert(e->id).second)
                        {
                            const auto store = society.storeFor(world_.marketSpot(f.community)->cell);
                            if (!store.empty() && society.shift(store, e->id, "meal", 1, 0, "festival feast"))
                            {
                                system(cl, "The feast for " + f.name + ": the town gives you a meal. Eat with the others.");
                                record(Economy, e->id);
                            }
                        }
                        if (f.rested.insert(e->id + "|" + std::to_string(int(hour))).second)
                        {
                            const auto& r = practice::rules();
                            e->practice->rested = std::min(r.restedMost, e->practice->rested + .5 * r.restedPerDay);
                        }
                    }
        for (const auto& k : kills)
            if (auto c = f.contests.find("hunt"); c != f.contests.end() && c->second.begun && !c->second.done &&
                                                    std::find(c->second.entrants.begin(), c->second.entrants.end(), k.killer) != c->second.entrants.end())
                if (const auto* s = wild::speciesById(k.species))
                {
                    const double clean = k.grade == "a clean kill" ? 1.5 : k.grade == "a good kill" ? 1. : k.grade == "a rough kill" ? .8 : .6;
                    c->second.score[k.killer] = std::max(c->second.score[k.killer], s->health * clean);
                }
        for (auto& [kind, c] : f.contests)
        {
            if (c.done)
                continue;
            if (!c.begun && hour >= startHour(kind))
                beginContest(f, c);
            else if (c.begun)
                runContest(f, c);
        }
        ++it;
    }
}

bool Game::festivalCommand(Connection* c, const json::Value& j, Result& result)
{
    // {"verb": "enter", "contest"} / "pull" / "howl" / "cheer" {target} / "star" {target}.
    const auto me = c->entityId;
    auto* e = world_.entity(me);
    if (!e)
        return result = {false, "No such character.", {}}, true;
    const auto community = world_.lawTown(e->cellId);
    auto* f = fairToday(community);
    if (!f)
        return result = {false, "There's no festival here today.", {}}, true;
    const auto verb = j.string("verb");
    if (verb == "enter")
    {
        const auto kind = j.string("contest");
        if (!isContest(kind))
            return result = {false, "Enter what?", {}}, true;
        refreshLetterPlaces();
        if (!atSquare(*e, community) && boardNear(me).empty() && !innCells_.count(e->cellId))
            return result = {false, "Sign up at the square, the board or the inn.", {}}, true;
        auto& ct = f->contests[kind];
        if (ct.begun)
            return result = {false, names::capitalised(contestWords(kind)) + " has begun.", {}}, true;
        if (std::find(ct.entrants.begin(), ct.entrants.end(), me) != ct.entrants.end())
            return result = {false, "You have entered already.", {}}, true;
        auto& society = world_.society();
        society.openAccount(pot(*f, ct));
        if (society.spendable(me) < Entry || !society.shift(me, pot(*f, ct), "", 0, Entry, "festival entry"))
            return result = {false, "Entry is a penny.", {}}, true;
        ct.entrants.push_back(me);
        ct.paid[me] += Entry;
        world_.recordEvent({"festival entered", me, {}, e->cellId, 0, 0, kind, 0, Entry, f->name});
        record(Economy, me);
        return result = {true, "You enter " + contestWords(kind) + " (a penny into the pot). It begins at " + std::to_string(int(startHour(kind))) + ":00.", {}}, true;
    }
    if (verb == "pull")
    {
        auto& ct = f->contests["tug"];
        if (!ct.begun || ct.done || std::find(ct.entrants.begin(), ct.entrants.end(), me) == ct.entrants.end())
            return result = {false, "You aren't on the rope.", {}}, true;
        const double t = world_.time() - ct.begunAt;
        const long beat = long(std::lround(t / BeatSeconds));
        if (std::abs(t - beat * BeatSeconds) > BeatSlack || beat < ct.beat)
            return result = {true, "Off the beat!", {}}, true;
        ct.pulledBeat[me] = beat;
        return result = {true, "", {}}, true;
    }
    if (verb == "howl")
    {
        auto& ct = f->contests["howl"];
        if (!ct.begun || ct.done || ct.turn >= ct.entrants.size() || ct.entrants[ct.turn] != me || ct.score.count(me))
            return result = {false, "It isn't your turn to howl.", {}}, true;
        const double skill = e->gameSkills.count("howl") ? e->gameSkills.at("howl") : 0.;
        ct.score[me] = std::clamp(e->stamina / 100, .2, 1.) * (60 + skill * .4) * (.75 + .5 * unit(me + "|howl|" + std::to_string(world_.time())));
        e->gameSkills["howl"] = std::min(100., skill + 1);
        festivalSay(community, ct.entrants, [&](const std::string& viewer) {
            return names::capitalised(viewer == me ? std::string("you howl") : labelFor(viewer, me) + " howls") + "; it rolls round the square.";
        });
        return result = {true, "", {}}, true;
    }
    if (verb == "cheer")
    {
        auto& ct = f->contests["howl"];
        const auto target = j.string("target");
        if (!ct.begun || ct.done || std::find(ct.entrants.begin(), ct.entrants.end(), target) == ct.entrants.end() || !atSquare(*e, community))
            return result = {false, "Cheer a howler, at the square, while the howling goes on.", {}}, true;
        if (target == me)
            return result = {false, "Not for yourself.", {}}, true;
        if (ct.cheered.count(me))
            return result = {false, "You have cheered already.", {}}, true;
        ct.cheered[me] = target;
        return result = {true, "You cheer " + labelFor(me, target) + " on.", {}}, true;
    }
    if (verb == "star")
    {
        // The storytelling: one star a wolf, for a teller other than itself (a doc 51 star, of the kind "festival").
        auto& ct = f->contests["story"];
        const auto target = j.string("target");
        if (!ct.begun || ct.done || std::find(ct.entrants.begin(), ct.entrants.end(), target) == ct.entrants.end() || !atSquare(*e, community))
            return result = {false, "Star a teller, at the square, while the storytelling goes on.", {}}, true;
        if (target == me || accountKey(target) == accountKey(me))
            return result = {false, "Not your own tale.", {}}, true;
        for (const auto& [teller, givers] : ct.givers)
            if (givers.count(me))
                return result = {false, "You have given your star.", {}}, true;
        recordStar("festival", "festival:" + f->community + ":" + std::to_string(f->day), me, target, 0);   // (Calls festivalStar.)
        if (auto* other = clientOf(target))
            system(other, names::capitalised(labelFor(target, me)) + " stars your tale.");
        return result = {true, "You star " + labelFor(me, target) + "'s tale.", {}}, true;
    }
    return result = {false, "That isn't something a festival does.", {}}, true;
}

void Game::festivalStar(const std::string& giver, const std::string& recipient)
{
    // A star at the storytelling: one a wolf, at the square, for a teller other than itself.
    const auto* e = world_.entity(giver);
    if (!e || giver == recipient)
        return;
    auto* f = fairToday(world_.lawTown(e->cellId));
    if (!f || !atSquare(*e, f->community))
        return;
    auto& ct = f->contests["story"];
    if (!ct.begun || ct.done || std::find(ct.entrants.begin(), ct.entrants.end(), recipient) == ct.entrants.end())
        return;
    for (const auto& [teller, givers] : ct.givers)
        if (givers.count(giver))
            return;
    ct.givers[recipient].insert(giver);
}

std::string Game::festivalBriefing(const std::string& npc, const std::string& player)
{
    (void)player;
    const auto* life = world_.society().state().residents.count(npc) ? &world_.society().state().residents.at(npc) : nullptr;
    if (!life)
        return {};
    const auto today = std::int64_t(std::floor(world_.calendarDays()));
    const auto found = fairs_.find(world_.lawTown(life->homeCell) + "|" + std::to_string(today));
    if (found == fairs_.end())
        return {};
    std::string out;
    for (const auto& [kind, c] : found->second.contests)
        if (c.done && !c.winners.empty())
            out += (out.empty() ? "" : "; ") + labelFor(npc, c.winners[0]) + " won " + contestWords(kind);
    return out.empty() ? std::string() : " At today's " + found->second.name + ": " + out + ".";
}

json::Value Game::festivalBoard(const std::string& community)
{
    // The board's festival notice: from three days before.
    const auto today = std::int64_t(std::floor(world_.calendarDays()));
    for (int ahead = 0; ahead <= 3; ++ahead)
        if (const auto name = festivalOn(community, today + ahead); !name.empty())
        {
            auto o = Value::object();
            o.add("name", name);
            o.add("inDays", ahead);
            auto lines = Value::array();
            for (const auto& s : Programme)
                lines.push(std::to_string(int(s.hour)) + ":00 " + s.what);
            o.add("programme", lines);
            return o;
        }
    return {};
}

json::Value Game::festivalSelf(const std::string& viewer)
{
    auto* e = world_.entity(viewer);
    if (!e)
        return {};
    const auto community = world_.lawTown(e->cellId);
    if (community.empty())
        return {};
    const auto today = std::int64_t(std::floor(world_.calendarDays()));
    auto* f = fairToday(community);
    if (!f)
    {
        for (int ahead = 1; ahead <= 3; ++ahead)
            if (const auto name = festivalOn(community, today + ahead); !name.empty())
            {
                auto o = Value::object();
                o.add("name", name);
                o.add("inDays", ahead);
                return o;
            }
        return {};
    }
    const double hour = hourOf(world_.calendarDays());
    auto o = Value::object();
    o.add("name", f->name);
    o.add("inDays", 0);
    o.add("here", atSquare(*e, community));
    auto programme = Value::array();
    for (const auto& s : Programme)
    {
        auto p = Value::object();
        p.add("hour", s.hour);
        p.add("what", s.what);
        p.add("kind", s.kind);
        if (const auto found = f->contests.find(s.kind == std::string("hunt") ? "hunt" : s.kind); found != f->contests.end())
        {
            const auto& ct = found->second;
            p.add("state", ct.done ? "done" : ct.begun ? "under way" : "open");
            p.add("entered", std::find(ct.entrants.begin(), ct.entrants.end(), viewer) != ct.entrants.end());
            if (!ct.winners.empty())
                p.add("winner", names::capitalised(ct.winners[0] == viewer ? std::string("you") : labelFor(viewer, ct.winners[0])));
            else if (ct.done && !ct.result.empty())
                p.add("winner", "none");
        }
        else
            p.add("state", hour >= s.hour + 1 ? "done" : hour >= s.hour ? "under way" : "");
        programme.push(p);
    }
    o.add("programme", programme);
    // What one does now.
    for (const auto& [kind, ct] : f->contests)
    {
        if (!ct.begun || ct.done)
            continue;
        const bool entered = std::find(ct.entrants.begin(), ct.entrants.end(), viewer) != ct.entrants.end();
        auto now = Value::object();
        now.add("kind", kind);
        if (kind == "race" && entered && !ct.score.count(viewer))
        {
            const int next = ct.mark.count(viewer) ? ct.mark.at(viewer) : 0;
            const Spot target = next < int(ct.marks.size()) ? ct.marks[std::size_t(next)] : *world_.marketSpot(community);
            now.add("mark", next + 1);
            now.add("marks", int(ct.marks.size()) + 1);
            now.add("x", target.x);
            now.add("y", target.y);
            now.add("seconds", std::round(world_.time() - ct.begunAt));
        }
        else if (kind == "tug" && entered)
        {
            const double t = world_.time() - ct.begunAt;
            now.add("team", ct.team.count(viewer) ? ct.team.at(viewer) : 0);
            now.add("marker", std::round(ct.marker * 10) / 10);
            now.add("beatIn", std::round((BeatSeconds - std::fmod(t, BeatSeconds)) * 100) / 100);
        }
        else if (kind == "howl" && ct.turn < ct.entrants.size())
        {
            const auto& who = ct.entrants[ct.turn];
            now.add("whose", names::capitalised(who == viewer ? std::string("you") : labelFor(viewer, who)));
            now.add("mine", who == viewer && !ct.score.count(viewer));
            now.add("cheered", ct.cheered.count(viewer) > 0);
            auto field = Value::array();
            for (const auto& id : ct.entrants)
                if (id != viewer)
                {
                    auto h = Value::object();
                    h.add("id", id);
                    h.add("name", names::capitalised(labelFor(viewer, id)));
                    field.push(h);
                }
            now.add("field", field);
        }
        else if (kind == "story")
        {
            if (ct.turn < ct.entrants.size())
                now.add("whose", names::capitalised(ct.entrants[ct.turn] == viewer ? std::string("you") : labelFor(viewer, ct.entrants[ct.turn])));
            bool starred = false;
            for (const auto& [teller, givers] : ct.givers)
                starred |= givers.count(viewer) > 0;
            now.add("starred", starred);
            auto field = Value::array();
            for (const auto& id : ct.entrants)
                if (id != viewer)
                {
                    auto h = Value::object();
                    h.add("id", id);
                    h.add("name", names::capitalised(labelFor(viewer, id)));
                    field.push(h);
                }
            now.add("field", field);
        }
        else
            continue;
        o.add("now", now);
        break;
    }
    return o;
}
} // namespace ratw::game
