// Loads a streamed world export (python3 tools/world_build.py export DIR writes DEV's newest build) the way the game
// server does, then brings every cell into memory so the server's own checks run on all of it.
//
//   world_check EXPORT_DIR [--simulate FROM_HOUR TO_HOUR [--players N [--at-seams]] [--no-check] [--full] [--deterministic] [--events FILE]
//                                                       [--threads N] [--economy FILE [--economy-detail FILE]] [--day N] [--sample] [--no-profile] [--no-births]
//                                                       [--fast] [--unseen-step N] [--straight] [--coarse-ticks]]
//
// --simulate then runs the world the way the game server does (20 ticks a second) between two hours of the day and
// reports how long ticks take, so a region's population can be checked against the server's 50 ms tick budget.
// --players N (default 1; 0 with --economy) adds walking players: the first at the spawn, the rest beside residents
// spread over the world, each observed every tick and sent a view five times a second as the server does (--at-seams:
// the rest by the seams between outdoor cells instead, walking over them and back, for sight and the local map across
// seams: Docs/Design/70-the-realm.md). --no-check
// skips loading every place first (the check), which is slow and not needed to measure ticks. --deterministic does the
// work by counts, never the clock (World::setDeterministic), so a run repeats exactly. --full turns the simulation tiers
// off, simulating every NPC in full wherever it is (as before tiers), for comparison. --threads N (default 8; 1: the main
// thread alone) spreads the world's work over threads as the server's pool does (Docs/Design/31, "Fast-forward"); a
// deterministic run ends the same on any number.
//
// An economy run (Docs/Design/69-simulation-speed.md): --economy FILE writes the city's loop, a JSON line a game day
// (doc 68, 10), and has no player unless --players says. With no player the harness samples nothing tick by tick (the
// held residents, the road tally, the crowds, each tick's time): --sample keeps them. --no-profile turns World::tick's
// clocks off (World::setProfiling). Each game day's wall time is printed as it ends. Births and arrivals are made as the
// game server makes them for a world from files (World::makeRequestedResidents, every tick); --no-births only asks for
// them, as before. Deaths are the world's own (old age, World::tendOldAge), the same here as on the server. --day N starts
// the run on day N of the year (1-365: Spring 1-92, Summer 93-184, Autumn 185-275, Winter 276-365), for the seasons in
// the Country's supply; the economy line names the season and the day, and each season group's supply and price.
// --economy-detail FILE (with --economy) writes a second JSON line a game day, per business (its trade, crew, wages owed,
// till, stock, and what it took in and paid out by kind: sales to residents and to the Country, draws, carriage, wages,
// profit) and per household (its posts, members, cash, hungry hours, income by kind and spending on food and goods), and
// the treasury's and the Country's flows, all from the ledger (World::setEventWatcher): only read, so the --economy file
// is the same with or without it. RATW_DETAIL_TRACE=HOME,... adds those households hour by hour. tools/econ_detail.py
// reads it (doc 68, "The first simulations").
// --fast (opt-in: it changes results) is --unseen-step 30 --straight --coarse-ticks: the unseen decide every N seconds,
// each decision N seconds' worth (Society::setUnseenStep; the game's is 5); offstage legs are timed by their straight
// line (World::setStraightOffstage); and while nobody is onstage the harness ticks half a second at a time, not 0.05 s.
#include "RatwCalendar.h"
#include "RatwItems.h"
#include "RatwJsonDoc.h"
#include "RatwPool.h"
#include "RatwWire.h"
#include "RatwWorld.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <tuple>
#include <sstream>
#include <set>
#include <string>
#include <unordered_map>

using namespace ratw;
namespace fs = std::filesystem;

namespace
{
std::string slurp(const fs::path& path)
{
    std::ifstream file(path, std::ios::binary);
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
}
// Keeping to the roads: the outdoor world as one grid of whether a road is within a tile (read from the export's cells,
// by the game's own rule, roadTerrain: a tile the catalog marks a way), and how far residents walked on it
// and off it, sampled every second along the straight line between where they were and where they are.
struct RoadTally
{
    int size = 0;
    std::vector<std::uint8_t> near;                 // 1: a road within a tile; 2: open ground; 0: no outdoor cell.
    std::map<std::string, std::pair<double, double>> offsets;   // Outdoor cell -> its world position.
    std::map<std::string, Vec2> last;               // Resident -> where it was last (world tiles); NaN when indoors.
    double on[2] = {0, 0}, off[2] = {0, 0};         // By [onstage, offstage].
    std::map<std::string, double> offBy;            // Off-road distance by what the walker was doing,
    std::map<std::string, double> offIn;            // and by where (the cell; for travellers, those "on the road").
    double travel[2] = {0, 0};                      // Travellers' ("on the road", "going ...") [on, off] the road.

    static bool road(char code) { return roadTerrain(code); }
    void load(const fs::path& root)
    {
        std::vector<std::tuple<int, int, std::vector<std::string>>> grids;
        for (const auto& file : fs::directory_iterator(root / "cells"))
        {
            const std::string text = slurp(file.path());
            if (text.find("\noutdoors: true") == std::string::npos)
                continue;
            std::istringstream lines(text);
            std::string line, id;
            int wx = 0, wy = 0;
            std::vector<std::string> rows;
            bool grid = false;
            while (std::getline(lines, line))
            {
                if (grid)
                    rows.push_back(line);
                else if (line.rfind("id: ", 0) == 0)
                    id = line.substr(4);
                else if (line.rfind("world: ", 0) == 0)
                    std::istringstream(line.substr(7)) >> wx >> wy;
                else if (line == "grid:")
                    grid = true;
            }
            offsets[id] = {wx, wy};
            for (std::size_t y = 0; y < rows.size(); ++y)
                size = std::max({size, wx + int(rows[y].size()), wy + int(y) + 1});
            grids.emplace_back(wx, wy, std::move(rows));
        }
        std::vector<std::uint8_t> isRoad(std::size_t(size) * std::size_t(size), 0);
        near.assign(isRoad.size(), 0);
        for (const auto& [wx, wy, rows] : grids)
            for (std::size_t y = 0; y < rows.size(); ++y)
                for (std::size_t x = 0; x < rows[y].size(); ++x)
                {
                    const auto at = std::size_t(wy + int(y)) * std::size_t(size) + std::size_t(wx + int(x));
                    near[at] = 2;
                    isRoad[at] = road(rows[y][x]);
                }
        auto spread = near;
        for (int y = 0; y < size; ++y)
            for (int x = 0; x < size; ++x)
                if (isRoad[std::size_t(y) * std::size_t(size) + std::size_t(x)])
                    for (int dy = -1; dy <= 1; ++dy)
                        for (int dx = -1; dx <= 1; ++dx)
                            if (x + dx >= 0 && y + dy >= 0 && x + dx < size && y + dy < size &&
                                near[std::size_t(y + dy) * std::size_t(size) + std::size_t(x + dx)])
                                spread[std::size_t(y + dy) * std::size_t(size) + std::size_t(x + dx)] = 1;
        near = std::move(spread);
    }
    void sample(const World& world)
    {
        for (const auto& [id, e] : world.entities())
        {
            if (!e.npc || e.dead)
                continue;
            const auto cell = offsets.find(e.cellId);
            const Vec2 now = cell == offsets.end() ? Vec2{NAN, NAN}
                                                   : Vec2{cell->second.first + e.position.x, cell->second.second + e.position.y};
            auto& before = last[id];
            const Vec2 then = before;
            before = now;
            if (!std::isfinite(now.x) || !std::isfinite(then.x))
                continue;
            const double length = std::hypot(now.x - then.x, now.y - then.y);
            if (length < .25 || length > 400)
                continue;
            const int steps = std::max(1, int(std::ceil(length)));
            double onRoad = 0, offRoad = 0;
            for (int s = 0; s < steps; ++s)
            {
                const double f = (s + .5) / steps;
                const int x = int(std::floor(then.x + (now.x - then.x) * f)), y = int(std::floor(then.y + (now.y - then.y) * f));
                if (x < 0 || y < 0 || x >= size || y >= size)
                    continue;
                const auto n = near[std::size_t(y) * std::size_t(size) + std::size_t(x)];
                (n == 1 ? onRoad : offRoad) += length / steps;
            }
            on[e.offstage] += onRoad;
            off[e.offstage] += offRoad;
            if (e.activity.rfind("on the road", 0) == 0 || e.activity.rfind("going ", 0) == 0)
                travel[0] += onRoad, travel[1] += offRoad;
            if (offRoad > 0)
            {
                offBy[e.activity.substr(0, e.activity.find(" — "))] += offRoad;
                if (e.activity.rfind("on the road", 0) == 0)
                    offIn[e.cellId + (e.offstage ? " (offstage)" : "")] += offRoad;
            }
        }
    }
    void report() const
    {
        const auto share = [](double a, double b) { return a + b > 0 ? 100.0 * a / (a + b) : 0.0; };
        std::cout << "  keeping to the roads, outdoors: onstage " << long(on[0] + off[0]) << " tiles walked, " << share(on[0], off[0])
                  << "% within a tile of a road; offstage " << long(on[1] + off[1]) << " tiles, " << share(on[1], off[1]) << "%\n";
        std::vector<std::pair<double, std::string>> most;
        for (const auto& [what, d] : offBy)
            most.emplace_back(d, what);
        std::sort(most.rbegin(), most.rend());
        std::cout << "    off the road most, by what they were doing (tiles):";
        for (std::size_t i = 0; i < most.size() && i < 8; ++i)
            std::cout << " " << most[i].second << " " << long(most[i].first) << ";";
        std::cout << "\n";
        most.clear();
        for (const auto& [where, d] : offIn)
            most.emplace_back(d, where);
        std::sort(most.rbegin(), most.rend());
        std::cout << "    travellers (on the road to somewhere, going somewhere): " << long(travel[0] + travel[1]) << " tiles, "
                  << share(travel[0], travel[1]) << "% within a tile of a road\n";
        std::cout << "    travellers off the road, by where (tiles):";
        for (std::size_t i = 0; i < most.size() && i < 8; ++i)
            std::cout << " " << most[i].second << " " << long(most[i].first) << ";";
        std::cout << "\n";
    }
};

// Where the money is (doc 42): each kind of holder's total, how many hold it, and the median of the people among them.
void moneyReport(const World& server, const char* when)
{
    struct Pile
    {
        std::int64_t total = 0;
        std::vector<std::int64_t> each;
    };
    std::map<std::string, Pile> piles;
    const auto& society = server.society();
    for (const auto& [id, account] : society.state().accounts)
    {
        std::string kind;
        if (id == "treasury")
            kind = "capital's treasury";
        else if (id.rfind("stores:", 0) == 0)
            kind = "town treasuries";
        else if (id.rfind("town:", 0) == 0)
            kind = "town " + id.substr(id.rfind(':') + 1);
        else if (id.rfind("home:", 0) == 0)
            kind = "home stores";
        else if (playerAccountId(id))
            kind = "players";
        else if (facilityAccount(id))
            kind = id.substr(0, id.find(':'));
        else if (const auto* spec = society.spec(id))
        {
            const auto* job = society.jobOf(id);
            const auto* e = server.entity(id);
            kind = job ? (job->role == "merchant" ? "merchants" : job->role == "guard" ? "guards"
                          : job->paid ? "paid civilians" : "unpaid civilians")
                 : society.apprenticedTo(id) ? "apprentices"
                 : e && e->age < 16          ? "children"
                                             : "adults without work";
            (void)spec;
        }
        else
            kind = "other";
        auto& pile = piles[kind];
        pile.total += account.cash;
        pile.each.push_back(account.cash);
    }
    int short_ = 0;
    for (const auto& [id, life] : society.state().residents)
        if (const auto* a = society.account(id); a && a->cash < 6)
            ++short_;
    std::cout << "  money " << when << " (" << society.moneySupply() << "p in all; " << short_
              << " residents short of a day's food money): holder, total, how many, median\n";
    for (auto& [kind, pile] : piles)
    {
        std::sort(pile.each.begin(), pile.each.end());
        std::cout << "    " << kind << ": " << pile.total << "p, " << pile.each.size() << ", " << pile.each[pile.each.size() / 2]
                  << "p\n";
    }
}
// --economy-detail FILE (doc 68, "The first simulations"): a JSON line a game day, per business and per household, from
// the society's ledger (every coin moves by shift or sale, and World::setEventWatcher sees each one). Only read: the
// simulation goes on exactly as without it, and the --economy file is the same with or without it.
struct EconomyDetail
{
    // Account -> what it took in and paid out since the last line, by kind (a short name of the ledger's reason).
    std::unordered_map<std::string, std::map<std::string, std::int64_t>> in, out;
    std::unordered_map<std::string, std::map<std::string, std::int64_t>> pieces;   // Account -> kind -> pieces bought or sold.
    std::unordered_map<std::string, int> hungryHours;                              // Resident -> hours hungry (80+).
    // A ledger reason, shortened.
    static std::string kindOf(const std::string& reason, const std::string& item)
    {
        if (reason == "market")
            return Society::edible(item) ? "food" : "goods";
        if (reason == "market: the season's price")
            return "food";                          // (The seasons' groups are all food.)
        if (reason == "to the Country")
            return "country";
        if (reason == "a service")
            return "services";
        if (reason == "the city's share of a service")
            return "services city";
        if (reason.rfind("from a city maker", 0) == 0)
            return "city trade";                    // (A maker's input bought from another: the seller's takings, the buyer's cost.)
        if (reason == "a patron's work" || reason == "a patron's new kit")
            return "patrons";
        if (reason == "a public works order")
            return "works";                         // (The treasury's surplus: doc 68, Progress, "The treasury's surplus".)
        if (reason == "restocked from the store room" || reason.rfind("drawn for ", 0) == 0 || reason.rfind("gate toll: ", 0) == 0)
            return "draws";
        if (reason.rfind("refunded", 0) == 0)
            return "draws refunded";
        return reason.substr(0, std::min<std::size_t>(reason.size(), 40));
    }
    static std::string payerKind(const std::string& id)
    {
        return id == "treasury" ? "treasury" : id.rfind("outside:", 0) == 0 ? "country" : id.rfind("till:", 0) == 0 ? "till" : "other";
    }
    void event(const WorldEvent& e)
    {
        if (e.kind != "economy" || e.coins <= 0 || e.actor == e.target)
            return;
        const std::string k = kindOf(e.detail, e.item);
        // A season's difference paid back to the buyer runs till -> buyer: less taken, not income.
        if (e.detail == "market: the season's price" && e.actor.rfind("till:", 0) == 0)
        {
            in[e.actor]["residents"] -= e.coins;
            out[e.target]["food"] -= e.coins;
            return;
        }
        if (k == "wages")
        {
            out[e.actor]["wages"] += e.coins;
            in[e.target]["wages:" + payerKind(e.actor)] += e.coins;
            return;
        }
        if (k == "wage support")
        {
            out[e.actor]["wage support"] += e.coins;
            in[e.target]["wages:support"] += e.coins;
            return;
        }
        out[e.actor][k] += e.coins;
        // A till's takings from residents (food and goods both) are "residents"; the Country's purchases "country".
        const bool till = e.target.rfind("till:", 0) == 0;
        const std::string inKind = till && (k == "food" || k == "goods") ? "residents" : k;
        in[e.target][inKind] += e.coins;
        if (till && e.quantity > 0 && (k == "food" || k == "goods" || k == "country"))
            pieces[e.target][inKind] += e.quantity;
        if (e.quantity > 0 && (k == "food" || k == "goods"))
            pieces[e.actor][k] += e.quantity;
    }
    // RATW_DETAIL_TRACE=HOME,HOME...: those households hour by hour (each member's hunger, cash, food in nourishment, task).
    std::set<std::string> traced;
    void hour(const World& server, int absoluteHour, std::ostream* o)
    {
        for (const auto& [id, life] : server.society().state().residents)
            if (const auto* e = server.entity(id); e && !e->dead && life.hunger >= 80)
                ++hungryHours[id];
        if (traced.empty() || !o)
            return;
        const auto& society = server.society();
        for (const auto& [home, members] : society.households())
            if (traced.count(home))
            {
                *o << "{\"trace\": " << quoted(home) << ", \"hour\": " << absoluteHour << ", \"members\": [";
                for (std::size_t i = 0; i < members.size(); ++i)
                {
                    const auto* life = society.resident(members[i]);
                    double food = 0;
                    if (const auto* a = society.account(members[i]))
                        for (const auto& [item, n] : a->stock)
                            if (n > 0)
                                food += double(Society::nourishment(item)) * n;
                    *o << (i ? ", " : "") << "[" << quoted(members[i]) << ", " << (life ? int(life->hunger) : -1) << ", "
                       << society.cash(members[i]) << ", " << food << ", " << quoted(life ? life->task : std::string()) << "]";
                }
                *o << "]}\n";
            }
    }
    static void flows(std::ostream& o, const char* name, const std::map<std::string, std::int64_t>& m)
    {
        o << ", \"" << name << "\": {";
        bool first = true;
        for (const auto& [k, v] : m)
            if (v != 0)
            {
                o << (first ? "" : ", ") << '"' << k << "\": " << v;
                first = false;
            }
        o << '}';
    }
    static std::string quoted(const std::string& s)
    {
        std::string q = "\"";
        for (const char c : s)
            q += c == '"' || c == '\\' ? std::string("\\") + c : std::string(1, c);
        return q + '"';
    }
    void day(const World& server, int dayNumber, std::ostream& o)
    {
        const auto& society = server.society();
        static const std::map<std::string, std::int64_t> none;
        const auto of = [&](const auto& m, const std::string& id) -> const std::map<std::string, std::int64_t>& {
            const auto f = m.find(id);
            return f == m.end() ? none : f->second;
        };
        // Each post's holder and pay; each till's wages owed a day.
        std::map<std::string, std::int64_t> tillOwes;
        std::unordered_map<std::string, std::string> postKind, postTitle;
        for (const auto& p : society.positions())
        {
            const auto held = society.state().careers.positions.find(p.id);
            if (held == society.state().careers.positions.end() || held->second.holder.empty())
                continue;
            const auto pay = society.payOf(p);
            postTitle[held->second.holder] = p.title;
            if (pay.kind.empty())
                continue;
            postKind[held->second.holder] = pay.kind;
            if (pay.payer.rfind("till:", 0) == 0)
                tillOwes[pay.payer] += pay.wage;
        }
        o << "{\"day\": " << dayNumber << ", \"tills\": {";
        bool first = true;
        for (const auto& p : society.positions())
        {
            if (p.role != "merchant")
                continue;
            const auto till = "till:" + p.id;
            const auto* a = society.account(till);
            if (!a)
                continue;
            const auto held = society.state().careers.positions.find(p.id);
            const std::string keeper = held == society.state().careers.positions.end() ? std::string() : held->second.holder;
            const auto* business = items::businessFor(p.title);
            std::int64_t stockValue = 0;
            int stockPieces = 0;
            for (const auto& [item, n] : a->stock)
                if (n > 0)
                {
                    stockValue += Society::catalogPrice(item) * n;
                    stockPieces += n;
                }
            o << (first ? "" : ", ") << quoted(till) << ": {\"trade\": " << quoted(business ? business->id : std::string())
              << ", \"title\": " << quoted(p.title) << ", \"keeper\": " << quoted(keeper) << ", \"crew\": " << society.crewOf(till)
              << ", \"owes\": " << tillOwes[till] << ", \"cash\": " << a->cash << ", \"stock_value\": " << stockValue
              << ", \"stock_pieces\": " << stockPieces;
            flows(o, "in", of(in, till));
            flows(o, "out", of(out, till));
            flows(o, "pieces", of(pieces, till));
            o << '}';
            first = false;
        }
        o << "}, \"households\": {";
        first = true;
        for (const auto& [home, members] : society.households())
        {
            std::map<std::string, std::int64_t> hin, hout, hpieces;
            std::int64_t cash = 0;
            int hungry = 0, hours = 0, adults = 0;
            std::string posts, ids;
            for (const auto& m : members)
            {
                for (const auto& [k, v] : of(in, m))
                    hin[k] += v;
                for (const auto& [k, v] : of(out, m))
                    hout[k] += v;
                for (const auto& [k, v] : of(pieces, m))
                    hpieces[k] += v;
                cash += society.cash(m);
                const auto* life = society.resident(m);
                hungry += life && life->hunger >= 80;
                if (const auto h = hungryHours.find(m); h != hungryHours.end())
                    hours += h->second;
                const auto* sp = society.spec(m);
                adults += sp && sp->age >= 16;
                const auto kind = postKind.find(m);
                const auto title = postTitle.find(m);
                posts += (posts.empty() ? "" : ",") + (kind != postKind.end() ? kind->second
                                                      : title != postTitle.end() ? std::string("unpaid") : std::string("-"));
                ids += (ids.empty() ? "" : ",") + m;
            }
            o << (first ? "" : ", ") << quoted(home) << ": {\"n\": " << members.size() << ", \"adults\": " << adults
              << ", \"posts\": " << quoted(posts) << ", \"ids\": " << quoted(ids) << ", \"cash\": " << cash << ", \"hungry\": " << hungry
              << ", \"hungry_hours\": " << hours;
            flows(o, "in", hin);
            flows(o, "out", hout);
            flows(o, "pieces", hpieces);
            o << '}';
            first = false;
        }
        o << "}, \"accounts\": {";
        first = true;
        for (const std::string id : {std::string("treasury"), std::string(Society::CountryFund), std::string(Society::CarriageFund)})
            if (society.account(id))
            {
                o << (first ? "" : ", ") << quoted(id) << ": {\"cash\": " << society.cash(id);
                flows(o, "in", of(in, id));
                flows(o, "out", of(out, id));
                o << '}';
                first = false;
            }
        o << "}}" << std::endl;
        in.clear();
        out.clear();
        pieces.clear();
        hungryHours.clear();
    }
};
// The city's loop (Docs/Design/68-the-country-and-the-city-loop.md, 10): every game hour, the residents hungry (80 or
// more); at noon, the households with nobody hungry and the shops holding goods; at 23:00 a JSON line for the day.
struct EconomyWatch
{
    int hours = 0, hungrySum = 0, hungryMost = 0, fedHouseholds = 0, households = 0, shops = 0, shopsStocked = 0,
        foodShops = 0, foodShopsStocked = 0, residents = 0;
    std::int64_t money0 = -1;
    int born = 0, arrivals = 0;                     // Made since the last line (the harness counts them: makeRequestedResidents).
    int deadAtLine = -1, deadNow = 0;               // Residents dead at the last line, and now.
    static double gini(std::vector<double> v)
    {
        if (v.size() < 2)
            return 0;
        std::sort(v.begin(), v.end());
        double sum = 0, weighted = 0;
        for (std::size_t i = 0; i < v.size(); ++i)
        {
            sum += v[i];
            weighted += double(i + 1) * v[i];
        }
        return sum <= 0 ? 0 : (2 * weighted) / (double(v.size()) * sum) - (double(v.size()) + 1) / double(v.size());
    }
    EconomyDetail* detail = nullptr;                // --economy-detail: per business and household (EconomyDetail).
    std::ostream* detailOut = nullptr;
    // --loop-state FILE: what the self-correcting rules have drifted to (the society's saved "loop": each shop's price
    // factors and smoothed sales, each till's wage step, the treasury's level), a JSON line every 28 days and the last.
    std::ostream* loopOut = nullptr;
    int lastDay = -1;
    void hour(const World& server, int absoluteHour, std::ostream& out)
    {
        const auto& society = server.society();
        if (detail)
            detail->hour(server, absoluteHour, detailOut);
        if (money0 < 0)
            money0 = society.moneySupply();
        const int hod = ((absoluteHour % 24) + 24) % 24;
        std::map<std::string, std::vector<std::string>> homes;
        int hungry = 0;
        residents = 0;
        deadNow = 0;
        for (const auto& [id, life] : society.state().residents)
        {
            const auto* e = server.entity(id);
            deadNow += e && e->dead;
            if (!e || e->dead)
                continue;
            ++residents;
            hungry += life.hunger >= 80;
            homes[Society::communalHome(life.homeCell) ? life.homeCell + "|" + id : life.homeCell].push_back(id);
        }
        if (deadAtLine < 0)
            deadAtLine = deadNow;
        ++hours;
        hungrySum += hungry;
        hungryMost = std::max(hungryMost, hungry);
        if (hod == 12)
        {
            households = int(homes.size());
            fedHouseholds = 0;
            for (const auto& [home, members] : homes)
                fedHouseholds += std::all_of(members.begin(), members.end(), [&](const std::string& m) { return society.resident(m)->hunger < 80; });
            shops = shopsStocked = foodShops = foodShopsStocked = 0;
            for (const auto& p : society.positions())
            {
                if (p.role != "merchant")
                    continue;
                const auto held = society.state().careers.positions.find(p.id);
                if (held == society.state().careers.positions.end() || held->second.holder.empty())
                    continue;
                const auto* till = society.account("till:" + p.id);
                if (!till)
                    continue;
                ++shops;
                const auto sold = society.wares(held->second.holder);
                shopsStocked += std::any_of(till->stock.begin(), till->stock.end(), [&](const auto& g) {
                    return g.second > 0 && std::find(sold.begin(), sold.end(), items::baseOf(g.first)) != sold.end();
                });
                if (society.foodShop(held->second.holder))
                {
                    ++foodShops;
                    foodShopsStocked += Society::hasFood(*till);
                }
            }
        }
        if (hod != 23)
            return;
        // Gini of cash over earning households (a household with a paid post; doc 68, 10).
        std::set<std::string> earners;
        for (const auto& p : society.positions())
            if (const auto held = society.state().careers.positions.find(p.id);
                held != society.state().careers.positions.end() && !held->second.holder.empty() && !society.payOf(p).kind.empty())
                earners.insert(held->second.holder);
        std::vector<double> wealth;
        for (const auto& [home, members] : homes)
            if (std::any_of(members.begin(), members.end(), [&](const std::string& m) { return earners.count(m) > 0; }))
            {
                double w = 0;
                for (const auto& m : members)
                    w += double(society.cash(m));
                wealth.push_back(w);
            }
        std::int64_t tills = 0;
        std::vector<std::int64_t> tillCash;
        for (const auto& [id, a] : society.state().accounts)
            if (id.rfind("till:", 0) == 0)
            {
                tills += a.cash;
                tillCash.push_back(a.cash);
            }
        std::sort(tillCash.begin(), tillCash.end());
        std::int64_t purses = 0;
        for (const auto& [id, life] : society.state().residents)
            purses += society.cash(id);
        // Who is hungry now (at most twelve): their post and what they hold, to see why.
        std::string who;
        int listed = 0;
        for (const auto& [id, life] : society.state().residents)
            if (const auto* e = server.entity(id); e && !e->dead && life.hunger >= 80 && listed++ < 12)
            {
                const auto* job = society.jobOf(id);
                who += (who.empty() ? "" : "; ") + id + " (" + (job ? job->title : std::string("no post")) + ", " +
                       std::to_string(society.cash(id)) + "p, home " + life.homeCell + ")";
            }
        // The seasons (doc 68, Progress, "Seasons and the DM's rules"): where in the year, and by season group the
        // Country's supply and price factors, its cap today, the pieces drawn, sold to residents, on the shops' shelves
        // now, and the mean price residents paid a piece.
        std::map<std::string, std::int64_t> onShelves;
        for (const auto& [id, a] : society.state().accounts)
            if (id.rfind("till:", 0) == 0)
                for (const auto& [item, n] : a.stock)
                    if (const auto* g = society.seasonGroup(item); g && n > 0)
                        onShelves[g->id] += n;
        const auto date = calendar::calendarAt(server.calendarDays());
        std::ostringstream seasons;
        seasons << ", \"season\": \"" << calendar::seasonName(date.season) << "\", \"year_day\": " << date.dayOfYear;
        const auto& d = society.loopToday();
        for (const auto& g : society.seasonTable())
        {
            const auto found = d.groups.find(g.id);
            const auto day = found == d.groups.end() ? Society::LoopDay::Group{} : found->second;
            const int cap = society.groupCap(g);
            seasons << ", \"" << g.id << "_supply\": " << society.supplyFactor(g) << ", \"" << g.id << "_price\": "
                    << Society::seasonal(g.price, society.yearDay()) << ", \"" << g.id << "_shelf\": "
                    << Society::seasonal(g.shelf, society.yearDay()) << ", \"" << g.id << "_cap\": "
                    << (cap == std::numeric_limits<int>::max() ? -1 : cap) << ", \"" << g.id << "_drawn\": " << day.drawn
                    << ", \"" << g.id << "_sold\": " << day.sold << ", \"" << g.id << "_stock\": " << onShelves[g.id]
                    << ", \"" << g.id << "_paid\": " << (day.sold ? double(day.spent) / day.sold : 0);
        }
        // The first fixes (doc 68, Progress): the treasury's wage support, and the city's services by trade.
        seasons << ", \"wage_support\": " << d.wageSupport << ", \"services_sold\": " << d.servicesSold << ", \"services_spent\": "
                << d.servicesSpent << ", \"services_city\": " << d.servicesCity;
        std::set<std::string> trades;
        for (const auto& s : society.services())
            trades.insert(s.business);
        for (const auto& t : trades)
        {
            const auto found = d.serviceTrades.find(t);
            const auto day = found == d.serviceTrades.end() ? Society::LoopDay::Trade{} : found->second;
            seasons << ", \"svc_" << t << "_sold\": " << day.sold << ", \"svc_" << t << "_spent\": " << day.spent;
        }
        // Trades and the follow-ups (doc 68, Progress): what keepers bought from the city's makers, the patrons' work and
        // kit (by the treasury, by the Country, by trade), buying ahead.
        seasons << ", \"city_trade\": " << d.cityTrade << ", \"city_trade_pieces\": " << d.cityTradePieces << ", \"patron_jobs\": "
                << d.patronJobs << ", \"patron_pieces\": " << d.patronPieces << ", \"patrons_treasury\": " << d.patronsTreasury
                << ", \"patrons_country\": " << d.patronsCountry << ", \"bought_ahead\": " << d.boughtAhead;
        for (const auto& trade : items::country().patrons)
        {
            const auto found = d.patronTrades.find(trade.business);
            const auto day = found == d.patronTrades.end() ? Society::LoopDay::Trade{} : found->second;
            seasons << ", \"patron_" << trade.business << "_jobs\": " << day.sold << ", \"patron_" << trade.business
                    << "_spent\": " << day.spent;
        }
        // The treasury's surplus (doc 68, Progress, "The treasury's surplus: works and alms"): what it held over its cap
        // and reserve tonight, the poorest households' share, the public works' orders (and by trade).
        seasons << ", \"surplus\": " << d.surplus << ", \"surplus_alms\": " << d.surplusAlms << ", \"surplus_households\": "
                << d.surplusHouseholds << ", \"works_spent\": " << d.worksSpent << ", \"works_pieces\": " << d.worksPieces;
        for (const auto& w : items::country().works)
        {
            const auto found = d.worksTrades.find(w.business);
            const auto day = found == d.worksTrades.end() ? Society::LoopDay::Trade{} : found->second;
            seasons << ", \"works_" << w.business << "_pieces\": " << day.sold << ", \"works_" << w.business << "_spent\": " << day.spent;
        }
        // The city's people (doc 68, Progress, "the long runs"): by age, pups (under 16), of working age (16 to their own
        // age to retire) and the retired; their mean age; the posts and those held; the largest household.
        {
            int pups = 0, working = 0, retired = 0, posts = 0, held = 0;
            double ages = 0;
            for (const auto& [id, life] : society.state().residents)
                if (const auto* e = server.entity(id); e && !e->dead)
                {
                    ages += e->age;
                    (e->age < 16 ? pups : e->age >= Society::retireAge(id) ? retired : working) += 1;
                }
            for (const auto& p : society.positions())
            {
                ++posts;
                const auto h = society.state().careers.positions.find(p.id);
                held += h != society.state().careers.positions.end() && !h->second.holder.empty();
            }
            std::size_t most = 0, alone = 0;
            for (const auto& [home, members] : homes)
                most = std::max(most, members.size()), alone += members.size() == 1;
            seasons << ", \"pups\": " << pups << ", \"working_age\": " << working << ", \"retired\": " << retired
                    << ", \"age_mean\": " << (residents ? ages / residents : 0) << ", \"posts\": " << posts
                    << ", \"posts_held\": " << held << ", \"household_most\": " << most << ", \"households_alone\": " << alone;
            // Births follow deaths; arrivals from the Country (doc 68, Progress): the posts that pay and those held (the
            // longest empty, in days), the children still owed for the dead, the unplaced NPCs left in the world's list,
            // how the empty posts were filled so far, and the players (there are none: no player is simulated).
            int earning = 0, earningHeld = 0;
            double emptiest = 0;
            for (const auto& p : society.positions())
            {
                if (society.postClass(p).empty())
                    continue;
                ++earning;
                const auto h = society.state().careers.positions.find(p.id);
                if (h != society.state().careers.positions.end() && !h->second.holder.empty())
                    ++earningHeld;
                else if (h != society.state().careers.positions.end() && h->second.vacantSince >= 0)
                    emptiest = std::max(emptiest, server.calendarDays() - h->second.vacantSince);
            }
            int players = 0;
            for (const auto& [id, e] : server.entities())
                players += !e.npc;
            for (const auto& [id, account] : society.state().accounts)
                players += playerAccountId(id) && !server.entity(id);
            const auto fill = [&](const char* how) {
                const auto f = society.fills().find(how);
                return f == society.fills().end() ? 0 : f->second;
            };
            seasons << ", \"earning_posts\": " << earning << ", \"earning_held\": " << earningHeld << ", \"empty_longest\": " << emptiest
                    << ", \"births_owed\": " << society.state().careers.birthsOwed.size() << ", \"unplaced\": " << society.unplacedCount()
                    << ", \"filled_city\": " << fill("from the city") + fill("from the city, of the next class")
                    << ", \"filled_city_next\": " << fill("from the city, of the next class")
                    << ", \"filled_country\": " << fill("from the Country") + fill("from the Country, of the next class")
                    << ", \"filled_country_next\": " << fill("from the Country, of the next class")
                    << ", \"filled_local\": " << fill("someone local takes it on") << ", \"filled_best\": "
                    << fill("the best-trained takes it on, to work toward mastership") << ", \"players\": " << players;
        }
        // The self-correcting rules (doc 68, Progress, "Self-correcting rules"), where they stand tonight: the shops' price
        // factors (those not 1: how many, their mean, spread and range; and the price level, those factors weighted by their
        // wares' smoothed sales), the shelves the sales give (their mean and spread), the tills' wage steps and the mean wage of
        // a keeper and a hand, the gate toll and the alms as the treasury's level has moved them, and that level.
        {
            const auto& loop = society.state().loop;
            const auto spread = [](const std::vector<double>& xs) {
                double sum = 0, squares = 0;
                for (const double x : xs)
                    sum += x;
                const double mean = xs.empty() ? 0 : sum / double(xs.size());
                for (const double x : xs)
                    squares += (x - mean) * (x - mean);
                return std::pair{mean, xs.empty() ? 0 : std::sqrt(squares / double(xs.size()))};
            };
            std::vector<double> factors, shelves;
            double low = 1, high = 1, weighted = 0, weights = 0;
            for (const auto& [till, byWare] : loop.prices)
                for (const auto& [ware, f] : byWare)
                {
                    factors.push_back(f);
                    low = std::min(low, f);
                    high = std::max(high, f);
                    if (const auto t = loop.sales.find(till); t != loop.sales.end())
                        if (const auto m = t->second.find(ware); m != t->second.end())
                        {
                            weighted += m->second * society.stockFactor(till, ware);
                            weights += m->second;
                        }
                }
            for (const auto& [till, byWare] : loop.sales)
                for (const auto& [ware, m] : byWare)
                    shelves.push_back(society.shelfOf(till, ware));
            std::vector<double> steps, keepers, hands;
            std::set<std::string> paying;
            for (const auto& p : society.positions())
                if (const auto held = society.state().careers.positions.find(p.id);
                    held != society.state().careers.positions.end() && !held->second.holder.empty())
                    if (const auto pay = society.payOf(p); pay.payer.rfind("till:", 0) == 0)
                    {
                        (pay.kind == "keeper" ? keepers : hands).push_back(pay.wage);
                        paying.insert(pay.payer);
                    }
            int up = 0, down = 0, top = 0, foot = 0;
            for (const auto& till : paying)
            {
                const auto found = loop.wages.find(till);
                const int step = found == loop.wages.end() ? 0 : found->second.step;
                steps.push_back(step);
                up += step > 0;
                down += step < 0;
                const int bound = society.wageAtBound(till);   // (At the top bound, or the foot: the floor or the bound.)
                top += bound > 0;
                foot += bound < 0;
            }
            const auto [priceMean, priceSd] = spread(factors);
            const auto [shelfMean, shelfSd] = spread(shelves);
            const auto [stepMean, stepSd] = spread(steps);
            seasons << ", \"price_moved\": " << factors.size() << ", \"price_mean\": " << (factors.empty() ? 1 : priceMean)
                    << ", \"price_sd\": " << priceSd << ", \"price_low\": " << low << ", \"price_high\": " << high
                    << ", \"price_level\": " << (weights > 0 ? weighted / weights : 1) << ", \"shelves\": " << shelves.size()
                    << ", \"shelf_mean\": " << shelfMean << ", \"shelf_sd\": " << shelfSd << ", \"wage_step_mean\": " << stepMean
                    << ", \"wage_step_sd\": " << stepSd << ", \"wage_up\": " << up << ", \"wage_down\": " << down
                    << ", \"wage_top\": " << top << ", \"wage_foot\": " << foot << ", \"wage_keeper\": " << spread(keepers).first
                    << ", \"wage_keeper_sd\": " << spread(keepers).second << ", \"wage_hand\": " << spread(hands).first
                    << ", \"wage_hand_sd\": " << spread(hands).second
                    << ", \"toll_now\": " << society.gateTollNow() << ", \"alms_now\": " << society.almsNow()
                    << ", \"treasury_level\": " << loop.level;
        }
        out << "{\"day\": " << absoluteHour / 24 << ", \"residents\": " << residents << ", \"hungry_mean\": "
            << (hours ? double(hungrySum) / hours : 0) << ", \"hungry_most\": " << hungryMost << ", \"households\": " << households
            << ", \"households_fed\": " << fedHouseholds << ", \"shops\": " << shops << ", \"shops_stocked\": " << shopsStocked
            << ", \"food_shops\": " << foodShops << ", \"food_shops_stocked\": " << foodShopsStocked
            << ", \"residents_spent\": " << d.residentsSpent << ", \"food_bought\": " << d.foodBought << ", \"needs_bought\": "
            << d.needsBought << ", \"comforts_bought\": " << d.comfortsBought << ", \"households_short\": " << d.householdsShort
            << ", \"country_spent\": " << d.countrySpent << ", \"country_unspent\": " << d.countryUnspent << ", \"drawn\": " << d.drawn
            << ", \"toll\": " << d.toll << ", \"carriage\": " << d.carriage << ", \"wages_owed\": " << d.wagesOwed
            << ", \"wages_paid\": " << d.wagesPaid << ", \"tills_short\": " << d.tillsShort << ", \"profit\": " << d.profit << ", \"subsidy\": " << d.subsidy << ", \"alms\": " << d.alms
            << ", \"country\": " << society.cash(Society::CountryFund) << ", \"treasury\": " << society.cash("treasury")
            << ", \"carters\": " << society.cash(society.carters()) << ", \"tills\": " << tills << ", \"till_median\": "
            << (tillCash.empty() ? 0 : tillCash[tillCash.size() / 2]) << ", \"purses\": " << purses << ", \"money\": "
            << society.moneySupply() << ", \"money_start\": " << money0 << ", \"conserved\": " << (society.conserved() ? "true" : "false")
            << ", \"gini_earners\": " << gini(wealth) << ", \"earning_households\": " << wealth.size() << ", \"births\": " << born
            << ", \"arrivals\": " << arrivals << ", \"deaths\": " << deadNow - deadAtLine << seasons.str() << ", \"hungry_who\": \""
            << who << "\"}" << std::endl;
        if (detail && detailOut)
            detail->day(server, int(absoluteHour / 24), *detailOut);
        if (const int day = int(absoluteHour / 24); loopOut && (day % 28 == 27 || day == lastDay))
        {
            const auto saved = wire::society(society.state());
            *loopOut << "{\"day\": " << day << ", \"loop\": " << json::dump(saved["loop"]) << "}" << std::endl;
        }
        hours = hungrySum = hungryMost = 0;
        born = arrivals = 0;
        deadAtLine = deadNow;
    }
};
} // namespace

int main(int argc, char** argv)
{
    const bool simulate = argc >= 5 && std::string(argv[2]) == "--simulate";
    int playerCount = -1;                           // (-1: 1, or 0 for an economy run.)
    bool sample = false;                            // --sample: the per-tick sampling even with no player.
    bool profile = true;                            // --no-profile: World::tick's clocks off.
    bool births = true;                             // --no-births: births and arrivals only asked for.
    bool fast = false, straight = false, coarse = false;   // --fast, --straight, --coarse-ticks.
    int unseenStep = 0;                             // --unseen-step N (0: the game's, Society::UnseenStep).
    bool unseenStepSet = false;
    bool check = true;
    bool full = false;                              // --full: every NPC in full simulation (no tiers).
    bool deterministic = false;                     // --deterministic: work by counts, never the clock (World::setDeterministic).
    std::string eventsFile;                         // --events FILE: what happened, and everyone's names, as JSON.
    unsigned threads = 8;                           // --threads N.
    std::string economyFile;                        // --economy FILE: the city's loop, a JSON line a game day (doc 68, 10).
    std::string loopFile;                           // --loop-state FILE: the self-correcting rules' drift (doc 68, Progress).
    std::string detailFile;                         // --economy-detail FILE: per business and household a day (with --economy).
    int startDay = 0;                               // --day N: start on day N of the year (1-365; 0: the world's own).
    bool atSeams = false;                           // --at-seams: the players after the first by seams (doc 70).
    bool usage = argc != 2 && !simulate;
    for (int i = 5; simulate && i < argc; ++i)
    {
        const std::string flag = argv[i];
        if (flag == "--players" && i + 1 < argc)
            playerCount = std::max(0, std::atoi(argv[++i]));
        else if (flag == "--at-seams")
            atSeams = true;
        else if (flag == "--sample")
            sample = true;
        else if (flag == "--no-profile")
            profile = false;
        else if (flag == "--no-births")
            births = false;
        else if (flag == "--fast")
            fast = true;
        else if (flag == "--straight")
            straight = true;
        else if (flag == "--coarse-ticks")
            coarse = true;
        else if (flag == "--unseen-step" && i + 1 < argc)
        {
            unseenStep = std::clamp(std::atoi(argv[++i]), 1, 600);
            unseenStepSet = true;
        }
        else if (flag == "--no-check")
            check = false;
        else if (flag == "--full")
            full = true;
        else if (flag == "--deterministic")
            deterministic = true;
        else if (flag == "--events" && i + 1 < argc)
            eventsFile = argv[++i];
        else if (flag == "--threads" && i + 1 < argc)
            threads = unsigned(std::max(1, std::atoi(argv[++i])));
        else if (flag == "--economy" && i + 1 < argc)
            economyFile = argv[++i];
        else if (flag == "--economy-detail" && i + 1 < argc)
            detailFile = argv[++i];
        else if (flag == "--loop-state" && i + 1 < argc)
            loopFile = argv[++i];
        else if (flag == "--day" && i + 1 < argc)
        {
            startDay = std::atoi(argv[++i]);
            usage = usage || startDay < 1 || startDay > calendar::DaysPerYear;
        }
        else
            usage = true;
    }
    if (usage)
    {
        std::cerr << "usage: world_check EXPORT_DIR [--simulate FROM_HOUR TO_HOUR [--players N [--at-seams]] [--no-check] [--full] [--deterministic]"
                     " [--events FILE] [--threads N] [--economy FILE [--economy-detail FILE]] [--day N] [--sample] [--no-profile] [--no-births] [--fast]"
                     " [--unseen-step N] [--straight] [--coarse-ticks]]\n";
        return 2;
    }
    if (playerCount < 0)
        playerCount = economyFile.empty() ? 1 : 0;  // An economy run has nobody watching (doc 69).
    if (fast)
    {
        unseenStep = unseenStepSet ? unseenStep : 30;
        straight = coarse = true;
    }
    // Per-tick sampling (held residents, the road tally, the crowds, each tick's time) when someone plays, or asked.
    const bool sampling = playerCount > 0 || sample;
    if (coarse && sampling)
    {
        std::cerr << "--coarse-ticks (and --fast) need no player and no --sample: ticking at 0.05 s.\n";
        coarse = false;
    }
    const fs::path root = argv[1];
    std::map<std::string, std::string> files;
    for (const auto& entry : fs::recursive_directory_iterator(root))
        if (entry.is_regular_file())
            files[fs::relative(entry.path(), root).generic_string()] = slurp(entry.path());
    auto find = [&](const std::string& path) -> const std::string* {
        const auto it = files.find(path);
        return it == files.end() ? nullptr : &it->second;
    };
    World world;
    world.setCellSource({
        [&](const std::string& id, std::string& header) {
            const auto* cell = find("cells/" + id + ".cell");
            if (!cell)
                return std::string("no cell file for ") + id;
            header = cell->substr(0, cell->find("grid:") + 5);
            return std::string();
        },
        [&](const std::string& id, std::string& cell, std::string& seams) {
            const auto* text = find("cells/" + id + ".cell");
            if (!text)
                return std::string("no cell file for ") + id;
            cell = *text;
            const auto* sides = find("seams/" + id);
            seams = sides ? *sides : std::string();
            return std::string();
        }});
    std::map<std::string, std::string> manifest;
    for (const auto& [path, text] : files)
        if (path.rfind("cells/", 0) != 0 && path.rfind("seams/", 0) != 0)
            manifest[path] = text;
    const auto loaded = world.loadWorldFiles(manifest, root.string());
    if (!loaded.ok)
    {
        std::cerr << "World failed to load: " << loaded.message << '\n';
        return 1;
    }
    std::size_t tiles = 0, cells = 0, failures = 0;
    double loadTotal = 0, loadWorst = 0;
    std::string slowest;
    for (const auto& [id, cell] : world.cells())
    {
        if (!check)
            break;
        const auto loadBegin = std::chrono::steady_clock::now();
        const auto result = world.ensureLoaded(id);
        const double took = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - loadBegin).count();
        loadTotal += took;
        if (took > loadWorst)
        {
            loadWorst = took;
            slowest = id;
        }
        if (!result.ok)
        {
            std::cerr << id << ": " << result.message << '\n';
            ++failures;
            continue;
        }
        ++cells;
        tiles += world.cell(id)->tiles.size();
    }
    if (check && cells)
        std::cout << "loading a place: mean " << loadTotal / double(cells) << " ms, worst " << loadWorst << " ms ("
                  << slowest << ")\n";
    if (check)
        std::cout << cells << " places loaded (" << tiles << " tiles), " << world.doors().size() << " doors and seams, "
              << failures << " failures\n";
    if (failures || !simulate)
        return failures ? 1 : 0;

    // A fresh world, as a server starts: only the cells someone is in or beside get loaded.
    World server;
    Pool pool(threads - 1);                         // (The main thread takes a share too.)
    if (threads > 1)
        server.setParallel([&](std::size_t count, const std::function<void(std::size_t)>& job) { pool.run(count, job); });
    server.setCellSource({
        [&](const std::string& id, std::string& header) {
            const auto* cell = find("cells/" + id + ".cell");
            if (!cell)
                return std::string("no cell file for ") + id;
            header = cell->substr(0, cell->find("grid:") + 5);
            return std::string();
        },
        [&](const std::string& id, std::string& cell, std::string& seams) {
            const auto* text = find("cells/" + id + ".cell");
            if (!text)
                return std::string("no cell file for ") + id;
            cell = *text;
            const auto* sides = find("seams/" + id);
            seams = sides ? *sides : std::string();
            return std::string();
        }});
    if (!server.loadWorldFiles(manifest, root.string()).ok)
        return 1;
    moneyReport(server, "at the start");
    if (full)
        server.setTiered(false);
    server.setDeterministic(deterministic);
    server.setProfiling(profile);
    if (unseenStep > 0)
        Society::setUnseenStep(unseenStep);
    World::setStraightOffstage(straight);
    if (unseenStep > 0 || straight || coarse)
        std::cout << "  fast steps (they change results): unseen step " << Society::unseenStep() << " s" << (straight ? ", straight offstage legs" : "")
                  << (coarse ? ", 0.5 s ticks while nobody is onstage" : "") << "\n";
    const double from = std::atof(argv[3]), to = std::atof(argv[4]);
    server.setTimeOfDay(from);
    if (startDay > 0)
    {
        // --day N: the run starts on that day of the year (a season's seasons: doc 68, Progress). Nobody ages: an
        // entity's first birthday is reckoned from the day it is first seen.
        server.advanceCalendar(double(startDay - 1) - std::floor(server.calendarDays()));
        const auto date = calendar::calendarAt(server.calendarDays());
        std::cout << "  starting on day " << date.dayOfYear << " of the year (" << calendar::seasonName(date.season) << ", day "
                  << date.dayOfSeason << ")\n";
    }
    // Players walking about, each of whose views is built five times a second as the server does for each client:
    // the first at the spawn, the others beside residents taken evenly through the population.
    std::vector<std::string> players;
    std::vector<std::string> residents;
    for (const auto& [id, e] : server.entities())
        if (e.npc)
            residents.push_back(id);
    for (int p = 0; p < playerCount; ++p)
    {
        const std::string id = p ? "player-bench-" + std::to_string(p) : "player-bench";
        server.addPlayer(id, "Bench " + std::to_string(p));
        players.push_back(id);
        if (p && !residents.empty())
            if (const auto* beside = server.entity(residents[std::size_t(p - 1) * residents.size() / std::size_t(playerCount)]))
            {
                auto* me = server.entity(id);
                me->cellId = beside->cellId;
                me->position = beside->position;
            }
    }
    if (atSeams)
    {
        // Each player after the first a little way in from the east edge of an outdoor cell with another beyond it,
        // taken evenly through them (the players walk east and west, over the seam and back).
        std::vector<std::string> edges;
        for (const auto& [id, c] : server.cells())
            for (const auto& [other, d] : server.cells())
                if (c.outdoors && d.outdoors && std::abs(d.worldX - (c.worldX + c.width)) < .5 && std::abs(d.worldY - c.worldY) < .5 &&
                    std::abs(d.worldZ - c.worldZ) < .01)
                    edges.push_back(id);
        int placed = 0;
        for (int p = 1; p < playerCount && !edges.empty(); ++p)
        {
            const auto& at = edges[std::size_t(p - 1) * edges.size() / std::size_t(playerCount - 1)];
            if (!server.ensureLoaded(at).ok)
                continue;
            const auto* c = server.cell(at);
            bool stood = false;
            for (int r = 0; r < 40 && !stood; ++r)
                for (int dy = -r; dy <= r && !stood; ++dy)
                    stood = server.teleport(players[std::size_t(p)], at, c->width - 12.5, c->height / 2 + dy + .5).ok ||
                            server.teleport(players[std::size_t(p)], at, c->width - 12.5 - r, c->height / 2 + dy + .5).ok;
            placed += stood;
        }
        std::cout << "  " << placed << " players by seams (of " << edges.size() << " cells with one east of them)\n";
    }
    std::vector<double> views, motions;
    const auto npcs = [&] {
        std::size_t n = 0;
        for (const auto& [id, e] : server.entities())
            n += e.npc;
        return n;
    };
    std::set<std::string> deadAtStart;
    for (const auto& [id, e] : server.entities())
        if (e.npc && e.dead)
            deadAtStart.insert(id);
    std::map<std::string, std::string> start;
    for (const auto& [id, e] : server.entities())
        if (e.npc)
            start[id] = e.cellId + " " + std::to_string(int(e.position.x)) + "," + std::to_string(int(e.position.y));
    // In 0.05 s steps of world time: a game hour is 600 real seconds; the server ticks at 20 Hz.
    const int ticks = int((to - from) * 600.0 / 0.05);
    std::vector<ratw::WorldEvent> events;
    struct WorstTick
    {
        double total = 0, streaming = 0, schedules = 0, movement = 0, separation = 0, views = 0;
        std::size_t searches = 0;
        int at = 0;
    } worstSteady;
    std::vector<double> times, steady;           // `steady`: ticks in which no place had to be loaded.
    // Each game day's share, to see whether ticks grow dearer as the world ages: total, schedules, movement,
    // separation (ms), and ticks.
    std::map<int, std::array<double, 5>> byDay;
    // And what grows: the schedules' stages (ms), route searches, and (sampled every 30 s, the most in the day) the
    // wolves in the world, those onstage, the road folk, and the most standing on one tile.
    std::map<int, std::array<double, 7>> stagesByDay;
    std::map<int, std::size_t> searchesByDay;
    std::map<int, std::array<std::size_t, 4>> crowdByDay;
    std::map<int, std::string> biggestCrowd;        // Where each day's biggest crowd stood, and what it was doing.
    std::vector<double> picking;                    // The ambient director's look every five seconds (Phase 10).
    std::size_t picked = 0;
    if (sampling)
        times.reserve(std::size_t(ticks));
    std::size_t mostLoaded = 0;
    // Residents held more than ten game minutes on one tile while bound elsewhere (a game minute is 10 s, 200 ticks).
    struct Held
    {
        std::tuple<std::string, int, int> at;
        int since = 0;
        bool counted = false;
    };
    constexpr int HeldTicks = 10 * 200;
    std::map<std::string, Held> held;
    std::vector<std::string> heldOnce;
    RoadTally roads;
    if (sampling)
        roads.load(root);
    std::ofstream economyOut;
    if (!economyFile.empty())
        economyOut.open(economyFile);
    EconomyWatch watch;
    EconomyDetail detail;
    std::ofstream loopOut;
    if (!loopFile.empty() && economyOut)
    {
        loopOut.open(loopFile);
        watch.loopOut = &loopOut;
        watch.lastDay = (int(to) - 24) / 24;       // (The last day whose line is written, at 23:00.)
    }
    std::ofstream detailOut;
    if (!detailFile.empty() && economyOut)
    {
        detailOut.open(detailFile);
        watch.detail = &detail;
        watch.detailOut = &detailOut;
        server.setEventWatcher([&detail](const WorldEvent& e) { detail.event(e); });
        if (const char* traced = std::getenv("RATW_DETAIL_TRACE"))
        {
            std::istringstream list(traced);
            for (std::string home; std::getline(list, home, ',');)
                detail.traced.insert(home);
        }
    }
    // Each game day's wall time (and, with the profile on, its stages), printed as it ends: the cost of a long run by
    // day, so any growth shows.
    constexpr int DayTicks = 24 * 12000;            // (A run's days are 24 game hours from its start.)
    const auto dayOf = [](int at) { return at / DayTicks; };
    struct DayCost
    {
        double seconds = 0, stages[7] = {};
        int ticks = 0;
    };
    std::map<int, DayCost> costByDay;
    const auto runBegun = std::chrono::steady_clock::now();
    auto dayBegun = runBegun;
    double stagesAtDay[7];
    std::copy(std::begin(server.tickProfile().stages), std::end(server.tickProfile().stages), stagesAtDay);
    int day = dayOf(0), ticksToday = 0, ticksRun = 0, madeBirths = 0, madeArrivals = 0;
    std::vector<ResidentRequest> made;
    const char* streamTrace = std::getenv("RATW_STREAM_TRACE");
    const char* trace = std::getenv("RATW_TRACE");
    const bool ambientLook = !std::getenv("RATW_NO_AMBIENT_LOOK");
    constexpr int CoarseSteps = 10;                 // (--coarse-ticks: half a second, in 0.05 s steps.)
    // Births and arrivals made as the game server makes them for a world from files (Game::makeResidents), after
    // each tick; --no-births leaves them asked for.
    const auto welcome = [&] {
        if (!births)
            return;
        made.clear();
        server.makeRequestedResidents(&made);
        for (const auto& r : made)
        {
            const bool child = r.kind == "birth";
            (child ? watch.born : watch.arrivals) += 1;
            (child ? madeBirths : madeArrivals) += 1;
        }
    };
    for (int i = 0; i < ticks;)
    {
        // Coarser ticks while nobody is onstage, on the half seconds, so the hours fall where they fall in 0.05 s ticks.
        const int stride = coarse && !server.anyoneOnstage() ? std::min(CoarseSteps - i % CoarseSteps, ticks - i) : 1;
        if (economyOut && i % 12000 == 0)          // Each game hour (12,000 ticks): the economy's samples.
            watch.hour(server, int(std::floor(from + i * 0.05 / 600.0 + 1e-9)), economyOut);
        if (!sampling)
        {
            server.tick(0.05 * stride);
            ++ticksRun;
            ++ticksToday;
            welcome();
            if (!eventsFile.empty() && (i % 20 == 0 || stride > 1))
                for (auto& e : server.takeEvents())
                    events.push_back(std::move(e));
            if (i % 200 == 0)                       // Once a game minute (10 s).
                mostLoaded = std::max(mostLoaded, server.loadedCells());
        }
        else
        {
            const auto begin = std::chrono::steady_clock::now();
            const auto loadedBefore = server.loadedCells();
            if (i % 300 == 0)                          // Walk east and west, as a player would.
                for (const auto& p : players)
                    server.move(p, (i / 300) % 2 ? -1 : 1, 0);
            const auto profileBefore = server.tickProfile();
            server.tick(0.05);
            ++ticksRun;
            ++ticksToday;
            welcome();
            const auto& profileAfter = server.tickProfile();
            if (i % 20 == 0)
                roads.sample(server);
            if (i % 20 == 0)
            {
                // Held (Docs/Design/67-ridgemere2-city.md, 8: the Sump's narrow passages): a resident bound somewhere else
                // that stays on one tile, sampled every second.
                for (const auto& [id, e] : server.entities())
                {
                    const auto* life = e.npc && !e.dead ? server.society().resident(id) : nullptr;
                    auto& h = held[id];
                    const std::tuple<std::string, int, int> at{e.cellId, int(std::floor(e.position.x)), int(std::floor(e.position.y))};
                    const bool bound = life && !life->goalCell.empty() &&
                                       (e.cellId != life->goalCell || std::hypot(e.position.x - life->goalX, e.position.y - life->goalY) > 1.5) &&
                                       e.activity.find("route unavailable") == std::string::npos;
                    if (!bound || at != h.at)
                    {
                        h.at = at;
                        h.since = i;
                        continue;
                    }
                    if (i - h.since > HeldTicks && !h.counted)
                    {
                        h.counted = true;
                        std::ostringstream where;
                        where << id << " (in " << e.cellId << " " << std::get<1>(at) << "," << std::get<2>(at) << ", bound for " << life->goalCell
                              << ", at " << int(std::fmod(from + i * 0.05 / 600.0, 24.0)) << ":00)";
                        heldOnce.push_back(where.str());
                    }
                }
            }
            if (i % 100 == 0 && ambientLook)
            {
                // Only looked at, never spoken: the simulation goes on exactly as without it.
                const auto lookBegin = std::chrono::steady_clock::now();
                picked += server.ambientPicks(players).size();
                picking.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - lookBegin).count());
            }
            if (!eventsFile.empty() && i % 20 == 0)
                for (auto& e : server.takeEvents())
                    events.push_back(std::move(e));
            const double tickOnly = (profileAfter.streaming.total - profileBefore.streaming.total) +
                                    (profileAfter.schedules.total - profileBefore.schedules.total) +
                                    (profileAfter.movement.total - profileBefore.movement.total) +
                                    (profileAfter.separation.total - profileBefore.separation.total) +
                                    (profileAfter.views.total - profileBefore.views.total);
            auto& dayTotals = byDay[int(std::floor((from + i * 0.05 / 600.0) / 24.0))];
            dayTotals[0] += tickOnly;
            dayTotals[1] += profileAfter.schedules.total - profileBefore.schedules.total;
            dayTotals[2] += profileAfter.movement.total - profileBefore.movement.total;
            dayTotals[3] += profileAfter.separation.total - profileBefore.separation.total;
            dayTotals[4] += 1;
            {
                const int d = int(std::floor((from + i * 0.05 / 600.0) / 24.0));
                for (int k = 0; k < 7; ++k)
                    stagesByDay[d][std::size_t(k)] += profileAfter.stages[k] - profileBefore.stages[k];
                searchesByDay[d] += profileAfter.routeSearches - profileBefore.routeSearches;
                if (i % 600 == 0)
                {
                    std::size_t all = 0, onstage = 0, transient = 0, pile = 0;
                    std::map<std::tuple<std::string, int, int>, std::size_t> tiles;
                    std::tuple<std::string, int, int> pileAt;
                    for (const auto& [id, e] : server.entities())
                    {
                        ++all;
                        transient += e.transient;
                        if (e.offstage || e.dead)
                            continue;
                        ++onstage;
                        const std::tuple<std::string, int, int> at{e.cellId, int(std::floor(e.position.x)), int(std::floor(e.position.y))};
                        if (++tiles[at] > pile)
                        {
                            pile = tiles[at];
                            pileAt = at;
                        }
                    }
                    auto& c = crowdByDay[d];
                    if (pile > c[3])
                    {
                        // What they were about: each activity (its task, before the reason) and how many.
                        std::map<std::string, int> doing;
                        std::size_t moving = 0, routed = 0;
                        for (const auto& [id, e] : server.entities())
                            if (!e.offstage && !e.dead && e.cellId == std::get<0>(pileAt) && int(std::floor(e.position.x)) == std::get<1>(pileAt) &&
                                int(std::floor(e.position.y)) == std::get<2>(pileAt))
                            {
                                ++doing[e.activity.substr(0, e.activity.find(" — "))];
                                moving += std::hypot(e.velocity.x, e.velocity.y) > 1e-6;
                                routed += !e.path.empty();
                            }
                        std::ostringstream where;
                        where << std::get<0>(pileAt) << " " << std::get<1>(pileAt) << "," << std::get<2>(pileAt) << " at "
                              << int(std::fmod(from + i * 0.05 / 600.0, 24.0)) << ":00, " << moving << " moving, " << routed << " with a route:";
                        for (const auto& [what, n] : doing)
                            where << " " << n << "× " << what << ";";
                        biggestCrowd[d] = where.str();
                    }
                    c = {std::max(c[0], all), std::max(c[1], onstage), std::max(c[2], transient), std::max(c[3], pile)};
                }
            }
            if (i >= int(60 / 0.05) && tickOnly > worstSteady.total)
                worstSteady = {tickOnly,
                               profileAfter.streaming.total - profileBefore.streaming.total,
                               profileAfter.schedules.total - profileBefore.schedules.total,
                               profileAfter.movement.total - profileBefore.movement.total,
                               profileAfter.separation.total - profileBefore.separation.total,
                               profileAfter.views.total - profileBefore.views.total,
                               profileAfter.routeSearches - profileBefore.routeSearches, i};
            for (const auto& p : players)
            {
                // The motion frame the server sends each client every tick: who in the cell it can see.
                const auto motionBegin = std::chrono::steady_clock::now();
                const auto* me = server.entity(p);
                std::size_t seen = 0;
                for (const auto& [id, e] : server.entities())
                    if (me && e.cellId == me->cellId && id != me->id && server.visionClarity(me->id, id) > 0)
                        ++seen;
                motions.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - motionBegin).count());
                (void)seen;
            }
            // Five views a second for each player, a quarter of the players each tick, as the server staggers them;
            // their sight is worked out together first, as the server does.
            std::vector<std::string> due;
            for (std::size_t p = 0; p < players.size(); ++p)
                if ((std::size_t(i) + p) % 4 == 0)
                    due.push_back(players[p]);
            {
                const auto viewBegin = std::chrono::steady_clock::now();
                server.prepareViews(due);
                const double shared = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - viewBegin).count();
                if (!due.empty())
                    views.push_back(shared);
            }
            for (std::size_t p = 0; p < players.size(); ++p)
                if ((std::size_t(i) + p) % 4 == 0)
                {
                    const auto viewBegin = std::chrono::steady_clock::now();
                    const auto view = server.snapshot(players[p]);
                    views.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - viewBegin).count());
                    (void)view;
                }
            if (streamTrace && i % 1200 == 0)
                std::cerr << "STREAM t=" << i * 0.05 << " loaded=" << server.loadedCells() << " offstage="
                          << server.offstageCount() << "\n";
            if (trace && i % 200 == 0)
                if (const auto* e = server.entity(trace))
                    std::cerr << "TRACE t=" << i * 0.05 << " " << e->cellId << " (" << e->position.x << "," << e->position.y
                              << ") path=" << e->path.size() << " state=" << e->state << " activity=" << e->activity << "\n";
            times.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - begin).count());
            if (server.loadedCells() <= loadedBefore)
                steady.push_back(times.back());
            mostLoaded = std::max(mostLoaded, server.loadedCells());
        }
        i += stride;
        if (const int now = dayOf(i); now != day || i >= ticks)
        {
            // A game day done (or the run): its wall time, and its stages where the profile is kept.
            const auto at = std::chrono::steady_clock::now();
            auto& cost = costByDay[day];
            cost.seconds = std::chrono::duration<double>(at - dayBegun).count();
            cost.ticks = ticksToday;
            for (int k = 0; k < 7; ++k)
                cost.stages[k] = (server.tickProfile().stages[k] - stagesAtDay[k]) / 1000.0;
            std::copy(std::begin(server.tickProfile().stages), std::end(server.tickProfile().stages), stagesAtDay);
            std::cout << "  run day " << day + 1 << (i % DayTicks ? " (part)" : "") << ": " << cost.seconds << " s wall, " << ticksToday
                      << " ticks; " << std::chrono::duration<double>(at - runBegun).count() << " s so far" << std::endl;
            dayBegun = at;
            day = now;
            ticksToday = 0;
        }
    }
    {
        const auto& profile = server.tickProfile();
        std::cout << "  inside World::tick, total ms (worst tick):";
        for (const auto& [name, part] : std::vector<std::pair<const char*, World::TickProfile::Part>>{
                 {"streaming", profile.streaming}, {"schedules", profile.schedules}, {"movement", profile.movement},
                 {"separation", profile.separation}, {"views", profile.views}})
            std::cout << ' ' << name << ' ' << long(part.total) << " (" << part.worst << ')';
        std::cout << "; " << profile.routeSearches << " route searches expanding " << profile.routeNodes
                  << " nodes (at most " << profile.largestRoute << " in one)\n";
        std::cout << "  path cache: " << server.pathCacheStats().first << " found, " << server.pathCacheStats().second
                  << " searched\n";
        std::cout << "  slowest route search: " << profile.slowestRoute << " ms in " << profile.slowestRouteCell << ", "
                  << profile.slowestRouteNodes << " nodes, " << profile.slowestRouteWaypoints << " waypoints\n";
        std::cout << "  slowest World::tick after the first minute (tick " << worstSteady.at << "): " << worstSteady.total
                  << " ms = streaming " << worstSteady.streaming << ", schedules " << worstSteady.schedules << " ("
                  << worstSteady.searches << " route searches), movement " << worstSteady.movement << ", separation "
                  << worstSteady.separation << ", views " << worstSteady.views << "\n";
    }
    std::size_t moved = 0;
    for (const auto& [id, e] : server.entities())
        if (e.npc && start.count(id) &&
            start[id] != e.cellId + " " + std::to_string(int(e.position.x)) + "," + std::to_string(int(e.position.y)))
            ++moved;
    double total = 0;
    for (double t : times)
        total += t;
    auto sorted = times;
    std::sort(sorted.begin(), sorted.end());
    const auto pct = [&](double p) { return sorted[std::min(sorted.size() - 1, std::size_t(p * sorted.size()))]; };
    // The first minute includes warming up: every cell someone stands in or beside loads, and every route and
    // region cache is built, at once. Report the steady state after it separately.
    const std::size_t warm = std::min(times.size(), std::size_t(60 / 0.05));
    const auto worstAt = std::size_t(std::max_element(times.begin(), times.end()) - times.begin());
    double warmWorst = 0, afterWorst = 0;
    for (std::size_t t = 0; t < times.size(); ++t)
        (t < warm ? warmWorst : afterWorst) = std::max(t < warm ? warmWorst : afterWorst, times[t]);
    std::vector<double> after(times.begin() + std::ptrdiff_t(warm), times.end());
    std::sort(after.begin(), after.end());
    if (!after.empty())
        std::cout << "  after the first minute: p99 " << after[std::min(after.size() - 1, std::size_t(.99 * after.size()))]
                  << " ms, p99.9 " << after[std::min(after.size() - 1, std::size_t(.999 * after.size()))]
                  << " ms, worst " << afterWorst << " ms (first minute worst " << warmWorst << " ms; worst tick "
                  << worstAt << ")\n";
    const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - runBegun).count();
    if (sampling)
        std::cout << npcs() << " residents, " << players.size() << " player" << (players.size() == 1 ? "" : "s") << ", " << ticks << " ticks from " << from << ":00 to " << to << ":00 ("
                  << total / 1000.0 << " s of work): mean " << total / ticks << " ms, p99 " << pct(.99) << " ms, p99.9 "
                  << pct(.999) << " ms, worst " << sorted.back() << " ms; " << moved << " residents moved; at most "
                  << mostLoaded << " places in memory; " << threads << " thread" << (threads == 1 ? "" : "s") << "\n";
    else
        std::cout << npcs() << " residents, no player, " << ticksRun << " ticks (" << ticks << " of 0.05 s) from " << from << ":00 to " << to
                  << ":00: " << wall << " s wall, " << wall / std::max(1e-9, (to - from) / 24.0) << " s a game day, mean "
                  << 1000.0 * wall / std::max(1, ticksRun) << " ms a tick; " << moved << " residents moved; at most " << mostLoaded
                  << " places in memory; " << threads << " thread" << (threads == 1 ? "" : "s") << "\n";
    if (costByDay.size() > 1 || !sampling)
    {
        std::cout << "  by run day (wall s; ticks; with the profile on, the schedules' stages in s: society bonds roads crime errands"
                     " streaming routes):\n";
        for (const auto& [d, c] : costByDay)
        {
            std::cout << "    day " << d + 1 << ": " << c.seconds << "; " << c.ticks << ";";
            for (const double sec : c.stages)
                std::cout << " " << sec;
            std::cout << "\n";
        }
    }
    // Who is where their day says they should be, and who could find no way there.
    std::size_t arrived = 0, underway = 0;
    std::vector<std::string> lost;
    for (const auto& [id, e] : server.entities())
    {
        const auto* life = server.society().resident(id);
        if (!e.npc || !life || life->goalCell.empty())
            continue;
        if (e.cellId == life->goalCell && std::hypot(e.position.x - life->goalX, e.position.y - life->goalY) <= 1.5)
            ++arrived;
        else if (e.activity.find("route unavailable") != std::string::npos)
            lost.push_back(id + " (in " + e.cellId + ", bound for " + life->goalCell + ")");
        else
            ++underway;
    }
    std::cout << "  " << server.offstageCount() << " residents offstage at the end; money "
              << (server.society().conserved() ? "conserved" : "NOT CONSERVED") << "; " << server.loadedCells()
              << " places in memory at the end; " << server.bonds().count() << " bonds between them\n";
    moneyReport(server, "at the end");
    {
        const auto& roads = server.roads();
        std::size_t travelling = 0, raided = 0, camps = 0, beliefs = 0, open = 0;
        for (const auto& c : roads.caravans)
            (c.status == "raided" ? raided : travelling) += 1;
        for (const auto& b : roads.camps)
            camps += b.active;
        for (const auto& k : roads.contracts)
            open += k.status == "open" || k.status == "taken";
        for (const auto& [id, e] : server.entities())
            if (const auto* mine = server.beliefsOf(id))
                beliefs += mine->size();
        std::cout << "  roads: " << server.towns().size() << " towns (";
        for (const auto& t : server.towns())
            std::cout << t.id << (&t == &server.towns().front() ? "*" : "") << (&t == &server.towns().back() ? "" : ", ");
        std::cout << "); " << travelling << " caravans on the road, " << raided << " robbed; " << camps << " bandit camps; "
                  << open << " open contracts; " << beliefs << " rumours held\n";
    }
    {
        std::map<std::string, int> kinds;
        for (const auto& i : server.crime().incidents)
            ++kinds[i.kind];
        std::cout << "  crime: " << server.crime().incidents.size() << " incidents (";
        for (const auto& [kind, n] : kinds)
            std::cout << n << " " << kind << (kind == kinds.rbegin()->first ? "" : ", ");
        std::cout << "); " << server.crime().warrants.size() << " wanted, " << server.crime().custody.size() << " held\n";
    }
    if (!picking.empty())
    {
        double total = 0;
        for (const double t : picking)
            total += t;
        std::cout << "  ambient director: a look every 5 s takes mean " << total / double(picking.size()) << " ms, worst "
                  << *std::max_element(picking.begin(), picking.end()) << " ms; " << picked << " exchanges it would voice\n";
    }
    if (byDay.size() > 1)
    {
        std::cout << "  by game day (mean ms a tick: all = schedules + movement + separation + ...):\n";
        for (const auto& [d, t] : byDay)
            std::cout << "    day " << d + 1 << ": " << t[0] / t[4] << " = " << t[1] / t[4] << " + " << t[2] / t[4] << " + "
                      << t[3] / t[4] << " + ...\n";
        std::cout << "  by game day (schedules by stage, mean ms a tick: society bonds roads crime errands streaming routes; route"
                     " searches a tick; most wolves, onstage, road folk, on one tile):\n";
        for (const auto& [d, t] : byDay)
        {
            std::cout << "    day " << d + 1 << ":";
            for (const double ms : stagesByDay[d])
                std::cout << " " << ms / t[4];
            const auto& c = crowdByDay[d];
            std::cout << "; " << double(searchesByDay[d]) / t[4] << "; " << c[0] << " " << c[1] << " " << c[2] << " " << c[3] << "\n";
        }
        std::cout << "  each day's biggest crowd on one tile:\n";
        for (const auto& [d, where] : biggestCrowd)
            std::cout << "    day " << d + 1 << ": " << where << "\n";
    }
    if (!eventsFile.empty())
    {
        // For reading chronicles offline (tools/chronicle.py --events): the events as the database would get them.
        for (auto& e : server.takeEvents())
            events.push_back(std::move(e));
        std::ofstream out(eventsFile);
        out << "{\"events\": " << ratw::eventsJson(events) << ", \"names\": {";
        bool first = true;
        for (const auto& [id, e] : server.entities())
        {
            std::string name;
            for (const char ch : e.name)
                if (ch == '"' || ch == '\\') name += std::string("\\") + ch;
                else if (static_cast<unsigned char>(ch) >= 0x20) name += ch;
            out << (first ? "" : ", ") << '"' << id << "\": \"" << name << '"';
            first = false;
        }
        out << "}}\n";
        std::cout << "  events: " << events.size() << " written to " << eventsFile << '\n';
    }
    {
        // The people (births follow deaths: doc 68, Progress): who lives, who married, children born and arrivals asked
        // for. (With --no-births nobody makes them: they wait as requests.)
        int living = 0, oldest = 0;
        for (const auto& [id, e] : server.entities())
            if (e.npc && !e.dead && server.society().resident(id))
                ++living, oldest = std::max(oldest, e.age);
        int asked = 0, askedArrivals = 0, died = 0;
        for (const auto& r : server.takeResidentRequests())
            (r.kind == "birth" ? asked : askedArrivals) += 1;
        for (const auto& [id, e] : server.entities())
            died += e.npc && e.dead && server.society().resident(id) && !deadAtStart.count(id);
        std::cout << "  people: " << living << " residents living (the oldest " << oldest << "), "
                  << server.society().state().careers.spouses.size() / 2 << " married couples; " << madeBirths << " born, "
                  << madeArrivals << " arrived from the Country and " << died << " died in the run" << (births ? "" : " (--no-births)") << "; "
                  << asked << " children and " << askedArrivals << " arrivals asked for and not made; "
                  << server.society().state().careers.birthsOwed.size() << " children still owed for the dead; " << server.society().unplacedCount()
                  << " unplaced NPCs left in the world's list\n";
    }
    if (sampling)
        roads.report();
    std::cout << "  at their scheduled place: " << arrived << ", on their way: " << underway << ", without a route: "
              << lost.size() << '\n';
    for (const auto& who : lost)
        std::cout << "    " << who << '\n';
    if (sampling)
    {
        std::size_t placed = 0;
        for (const auto& [id, e] : server.entities())
            placed += e.npc && server.society().resident(id) != nullptr;
        std::cout << "  held more than 10 game minutes on one tile while bound elsewhere: " << heldOnce.size() << " of " << placed
                  << " residents (" << (placed ? 100.0 * double(heldOnce.size()) / double(placed) : 0.0) << "%)\n";
        for (std::size_t k = 0; k < heldOnce.size() && k < 40; ++k)
            std::cout << "    " << heldOnce[k] << '\n';
        if (heldOnce.size() > 40)
            std::cout << "    ... and " << heldOnce.size() - 40 << " more\n";
    }
    // The store room and the city's loop (docs 67, 68): the Country's purse, the treasury, the carters.
    {
        const auto& society = server.society();
        const auto carters = society.carters();
        std::cout << "  the Country (" << ratw::Society::CountryFund << "): " << society.cash(ratw::Society::CountryFund) << "p"
                  << (society.account(ratw::Society::CountryFund) ? "" : " (no purse: not an authored world)") << "; treasury "
                  << society.cash("treasury") << "p; carriage to " << carters << " " << society.cash(carters) << "p"
                  << (carters == ratw::Society::CarriageFund ? " (no carters' yard in this world)" : "") << "; restocking rule "
                  << (society.authored().economy.storeRule ? "on" : "off") << "; needs " << (society.needsPaused() ? "paused" : "on") << '\n';
    }
    std::sort(motions.begin(), motions.end());
    double motionTotal = 0;
    for (double m : motions)
        motionTotal += m;
    if (!motions.empty())
        std::cout << "  a player's motion frame (every tick): mean " << motionTotal / motions.size() << " ms, worst "
                  << motions.back() << " ms\n";
    std::sort(views.begin(), views.end());
    double viewTotal = 0;
    for (double v : views)
        viewTotal += v;
    if (!views.empty())
        std::cout << "  a player's view (5 a second): mean " << viewTotal / views.size() << " ms, worst " << views.back()
                  << " ms\n";
    // A fingerprint of where everyone ended up, what they were doing, and what each player has seen of the world: an
    // optimisation that changes no behaviour leaves it the same, so compare it before and after.
    std::uint64_t digest = 1469598103934665603ULL;
    const auto mix = [&](const void* data, std::size_t size) {
        for (std::size_t b = 0; b < size; ++b)
            digest = (digest ^ static_cast<const unsigned char*>(data)[b]) * 1099511628211ULL;
    };
    const auto mixText = [&](const std::string& text) { mix(text.data(), text.size() + 1); };
    for (const auto& [id, e] : server.entities())
    {
        mixText(id);
        mixText(e.cellId);
        mix(&e.position.x, sizeof e.position.x);
        mix(&e.position.y, sizeof e.position.y);
        mixText(e.activity);
        mixText(e.posture);
        if (const auto* life = server.society().resident(id))
            mixText(life->task);
    }
    const auto people = digest;
    // And the society's state (doc 69: the people digest above can't tell apart runs whose economies differ): every
    // account's cash and goods, every life's needs, work and goal, the posts' holders, and the legs under way.
    digest = 1469598103934665603ULL;
    for (const auto& [id, account] : server.society().state().accounts)
    {
        mixText(id);
        mix(&account.cash, sizeof account.cash);
        for (const auto& [item, n] : account.stock)
        {
            mixText(item);
            mix(&n, sizeof n);
        }
    }
    for (const auto& [id, life] : server.society().state().residents)
    {
        mixText(id);
        for (const double v : {life.hunger, life.fatigue, life.progress, life.goalX, life.goalY, life.homeX, life.homeY})
            mix(&v, sizeof v);
        mixText(life.task);
        mixText(life.reason);
        mixText(life.goalCell);
        mixText(life.homeCell);
    }
    for (const auto& [id, post] : server.society().state().careers.positions)
    {
        mixText(id);
        mixText(post.holder);
        mixText(post.apprentice);
    }
    const std::uint64_t legs = server.legsDigest();
    mix(&legs, sizeof legs);
    const auto societyDigest = digest;
    digest = 1469598103934665603ULL;
    std::size_t remembered = 0;
    for (const auto& p : players)
        for (const auto& [cellId, memory] : server.memories(p))
        {
            mixText(cellId);
            mix(memory.glyphs.data(), memory.glyphs.size());
            for (const bool seen : memory.observed)
            {
                mix(&seen, 1);
                remembered += seen;
            }
        }
    std::cout << "  digest: people " << std::hex << people << ", society " << societyDigest << ", map memory " << digest << std::dec
              << " (" << remembered << " tiles remembered)\n";
    std::sort(steady.begin(), steady.end());
    if (!steady.empty())
        std::cout << "  without loading a place: p99 " << steady[std::min(steady.size() - 1, std::size_t(.99 * steady.size()))]
                  << " ms, p99.9 " << steady[std::min(steady.size() - 1, std::size_t(.999 * steady.size()))]
                  << " ms, worst " << steady.back() << " ms (" << times.size() - steady.size()
                  << " ticks loaded places)\n";
    return 0;
}
