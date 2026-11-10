// The port (Docs/Design/79-a-staged-economy.md, 8.3, 8.6 and D8; Phase 5): ships as a committed timetable, never seen.
// A port is a market with posts of the Wharf's job words. Each port's ships are rolled a week ahead (the rule
// shipsPerWeek, one more or fewer by the slider and the season, seeded by the week and the port) and kept on the Market
// Board (market::Board::ships), so they are saved, journalled and restored with it: a restart shows the same ships. On
// its day a ship docks at its hour (its cargo cheaper at the port's market toward a baseline, what it takes dearer, the
// goods the port waits for ships for refilled, the Wharf's posts unloading it, the Wharf a crowd) and sails that evening
// (what the shops hold over their shelves of what it takes goes with it); a storm keeps one out (its cargo dearer, a
// trouble for the market's Times, "No ship this week"). The timetable is a notice on the port's board (Game::portNotices),
// and talk's `world.ships` and `world.ship_name` (World::talkTown). A world with no port has none of it, and its goods
// refill each morning as ever; a port deleted loses its ships and its notice at the next load or tick.
#include "RatwCalendar.h"
#include "RatwItems.h"
#include "RatwMarket.h"
#include "RatwWorld.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <set>

namespace ratw
{
namespace
{
// (RatwTradeShow.cpp's seeded lots, the same here: a ship is as fixed as a wagon.)
std::uint64_t lot(const std::string& a, std::int64_t b)
{
    std::uint64_t h = std::hash<std::string>{}(a) * 1099511628211ULL + std::uint64_t(b) * 2654435761ULL;
    h ^= h >> 29;
    h *= 0xBF58476D1CE4E5B9ULL;
    h ^= h >> 32;
    return h;
}
double unit(const std::string& a, std::int64_t b)
{
    return double(lot(a, b) % 100000) / 100000.0;
}
std::string lowered(std::string s)
{
    for (auto& c : s)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
std::string listed(const std::vector<std::string>& words)
{
    std::string out;
    for (std::size_t i = 0; i < words.size(); ++i)
        out += (i == 0 ? "" : i + 1 == words.size() ? " and " : ", ") + words[i];
    return out;
}
// "the Gull" at a sentence's start: "The Gull".
std::string capital(std::string s)
{
    if (!s.empty())
        s[0] = char(std::toupper(static_cast<unsigned char>(s[0])));
    return s;
}
// The Wharf's job words (8.3): the sea trade's posts and the harbour's heads (Society::postRules).
const std::vector<std::string>& wharfWords()
{
    static const std::vector<std::string> words = [] {
        std::vector<std::string> out{"overseeing the grayrock wharf", "harbourmaster"};
        for (const auto& rule : Society::postRules())
            if (rule.name == "the sea trade")
                out.insert(out.end(), rule.words.begin(), rule.words.end());
        return out;
    }();
    return words;
}
constexpr std::int64_t Week = calendar::DaysPerWeek;
// A ship's cargo is pulled halfway toward the ship baseline, only down; what it takes halfway toward its own, only up; a
// stormbound ship's cargo a step dearer (8.3, 8.6).
constexpr double ShipBaseline = -.1, TakesBaseline = .1, StormNudge = .1;
constexpr int BringsMost = 4, TakesMost = 3;
constexpr double BackRooms = 2;                      // (A ship fills two shelves' worth behind: the next is days off.)
constexpr std::int64_t SailedKeptDays = 1, StormKeptDays = 3;
constexpr std::size_t NoticeRoom = 270;              // (A notice's 280, less a little.)
double stormOdds(calendar::Season season)
{
    return season == calendar::Season::Winter ? .15 : season == calendar::Season::Autumn ? .06 : .03;
}
} // namespace

const World::Port* World::port(const std::string& market) const
{
    for (const auto& p : ports_)
        if (p.market == market)
            return &p;
    return nullptr;
}

void World::findPorts(std::map<std::string, std::set<std::string>>& imports)
{
    // The ports (8.3), found again with the road markets: every post with the Wharf's job words, by the market it works
    // in (else its holder's home's, else the nearest by path); and each port's goods that wait for a ship (D27), unless
    // the DM's rule stops the ships (shipsPerWeek 0), when they refill each morning as in a world with no port.
    ports_.clear();
    dockHands_.clear();
    std::map<std::string, Port> found;
    std::map<std::string, std::map<std::string, int>> crowd;      // Port -> cell -> its posts there.
    std::map<std::string, const Town*> nearest;
    const auto& held = society_.state().careers.positions;
    for (const auto& p : society_.positions())
    {
        if (p.role == "merchant" || p.role == "guard")
            continue;
        const auto title = lowered(p.title);
        if (std::none_of(wharfWords().begin(), wharfWords().end(), [&](const std::string& w) { return title.find(w) != std::string::npos; }))
            continue;
        std::string holder;
        if (const auto h = held.find(p.id); h != held.end())
            holder = h->second.holder;
        const auto* body = holder.empty() ? nullptr : entity(holder);
        const auto* life = holder.empty() ? nullptr : society_.resident(holder);
        const bool manned = body && !body->dead && life;
        const auto* m = roadMarketOf(p.work.cell);
        if (!m && manned)
            m = roadMarketOf(life->homeCell);
        if (!m && cells_.count(p.work.cell))
        {
            const auto [slot, fresh] = nearest.try_emplace(p.work.cell, nullptr);
            if (fresh)
            {
                std::size_t best = std::numeric_limits<std::size_t>::max();
                for (const auto& candidate : roadMarkets_)
                    if (const auto route = routeBetween(p.work.cell, candidate.market); !route.empty() && route.size() < best)
                        best = route.size(), slot->second = &candidate;
            }
            m = slot->second;
        }
        if (!m)
            continue;
        auto& port = found[m->id];
        port.market = m->id;
        ++port.posts;
        port.held += manned;
        if (cells_.count(p.work.cell))
        {
            port.cells.insert(p.work.cell);
            ++crowd[m->id][p.work.cell];
        }
        if (manned)
            dockHands_[holder] = {m->id, p.title,
                                  title.find("unloading") != std::string::npos || title.find("cranes") != std::string::npos ||
                                      title.find("carrying") != std::string::npos};
    }
    const bool shipsRun = society_.rule("shipsPerWeek") > 0;
    for (auto& [id, port] : found)
    {
        int most = 0;
        for (const auto& [cellId, n] : crowd[id])
            if (n > most)
                most = n, port.cell = cellId;
        if (port.cell.empty())
            if (const auto* m = roadMarket(id))
                port.cell = m->market;
        if (const auto* tp = society_.tradePoints().find(id); tp && shipsRun)
            for (const auto& g : market::data().shipBrings)
                if (tp->sold.count(g) && !tp->crafted.count(g))
                {
                    port.imports.push_back(g);
                    imports[id].insert(g);
                }
        ports_.push_back(std::move(port));
    }
}

int World::shipsInWeek(const std::string& market, std::int64_t week) const
{
    // 1-3 ships a week by slider and season at the rule's own 2 (8.3): the rule, one more or fewer by a seeded score the
    // slider and the season lean (fewer in the winter's storms, never more then); none while the rule is 0.
    const int rule = society_.rule("shipsPerWeek");
    if (rule <= 0 || !port(market))
        return 0;
    const auto season = calendar::calendarAt(double(week * Week) + 3.5).season;
    const double lean = season == calendar::Season::Winter ? -.3 : season == calendar::Season::Autumn ? -.05
                        : season == calendar::Season::Summer ? .1 : 0;
    const double score = unit("ships:" + market, week) - .5 + double(society_.board().slider) / 400. + lean;
    return std::clamp(rule + (score < -.2 ? -1 : score > .2 ? 1 : 0), 1, int(Week));
}

void World::rollShips(const Port& port, std::int64_t week, std::int64_t today)
{
    // A week of a port's ships, committed (8.3): their days (never Restday while another day is free), their hours, their
    // names from market.json's list (none twice on the timetable), and what each brings and takes, in rotation by its
    // number: of the port's goods that wait for ships, and of market.json's `wharf.takes` its shops sell, buy or make.
    auto& b = society_.stagedBoard();
    const int count = shipsInWeek(port.market, week);
    if (count <= 0)
        return;
    std::vector<std::pair<std::uint64_t, int>> order;
    for (int weekday = 0; weekday < int(Week); ++weekday)
        order.push_back({(weekday == calendar::Restday ? 1ULL << 63 : 0) | lot("shipday:" + port.market, week * Week + weekday) >> 1, weekday});
    std::sort(order.begin(), order.end());
    std::vector<std::int64_t> days;
    for (int i = 0; i < count && i < int(order.size()); ++i)
        days.push_back(week * Week + order[std::size_t(i)].second);
    std::sort(days.begin(), days.end());
    const auto* tp = society_.tradePoints().find(port.market);
    std::vector<std::string> takes;
    if (tp)
        for (const auto& g : market::data().shipTakes)
            if (tp->sold.count(g) || tp->bought.count(g) || tp->made.count(g))
                takes.push_back(g);
    const auto& names = market::data().shipNames;
    for (const auto day : days)
    {
        // (Only what a notice can promise ahead: never today's. And bounded a port, so no port crowds out another's.)
        if (day <= today || b.ships.size() >= market::ShipsMost ||
            std::size_t(std::count_if(b.ships.begin(), b.ships.end(), [&](const market::Ship& o) { return o.port == port.market; })) >= market::ShipsAPort)
            continue;
        market::Ship s;
        const auto number = b.nextShip++;
        s.id = "s" + std::to_string(number);
        s.port = port.market;
        s.day = day;
        const auto pick = lot("ship:" + port.market, day);
        s.hour = 7 + double(pick % 7) * .5;
        s.sails = std::min(19.5, s.hour + 8.5 + double(pick / 7 % 4) * .5);
        if (!names.empty())
        {
            const auto taken = [&](const std::string& name) {
                return std::any_of(b.ships.begin(), b.ships.end(), [&](const market::Ship& o) { return o.port == port.market && o.name == name; });
            };
            auto at = std::size_t(pick / 31 % names.size());
            for (std::size_t tries = 0; tries < names.size() && taken(names[at]); ++tries)
                at = (at + 1) % names.size();
            s.name = names[at];
        }
        if (s.name.empty())
            s.name = "a ship";
        const auto fill = [&](const std::vector<std::string>& from, int most, std::vector<std::pair<std::string, int>>& into) {
            for (std::size_t i = 0; i < from.size() && int(into.size()) < most; ++i)
            {
                const auto& g = from[(std::size_t(number) * std::size_t(most) + i) % from.size()];
                into.push_back({g, std::clamp(society_.marketShelf(port.market, g), 1, 999)});
            }
        };
        fill(port.imports, BringsMost, s.brings);
        fill(takes, TakesMost, s.takes);
        s.posted = double(today);                   // (The day, never the instant: the same world posts the same timetable.)
        recordEvent({"ship posted", s.id, port.market, port.cell, 0, 0, s.name, int(s.brings.size()), 0,
                     capital(s.name) + ", due " + calendar::weekdayName(calendar::weekdayOf(double(day))) + "."});
        b.ships.push_back(std::move(s));
    }
}

void World::portMorning(std::int64_t today)
{
    // The port's day (8.3): ships long sailed or stormbound drop off the timetable; each port's weeks rolled through the
    // next (so the next seven days are always committed); the notice posted again.
    auto& b = society_.stagedBoard();
    b.ships.erase(std::remove_if(b.ships.begin(), b.ships.end(), [&](const market::Ship& s) {
                      return (s.status == "sailed" && today > s.day + SailedKeptDays) || (s.status == "storm" && today > s.day + StormKeptDays);
                  }),
                  b.ships.end());
    rollPorts(today);
    shipsPosted_.clear();                           // (Each morning's timetable is posted afresh: tendPort's.)
}

void World::rollPorts(std::int64_t today)
{
    // Each port's weeks through the next, those not yet rolled (a week is rolled once: what is posted stays as posted).
    if (society_.rule("shipsPerWeek") <= 0)
        return;
    auto& b = society_.stagedBoard();
    const auto week = today / Week;
    for (const auto& port : ports_)
    {
        const auto through = b.shipsThrough.find(port.market);
        for (auto w = std::max(week, through == b.shipsThrough.end() ? week : through->second + 1); w <= week + 1; ++w)
            rollShips(port, w, today);
        b.shipsThrough[port.market] = week + 1;
    }
}

void World::shipStormbound(market::Ship& s)
{
    // A storm keeps it out (8.3, 8.6): what it was bringing a step dearer, a trouble for the market's Times, and word of it.
    s.status = "storm";
    for (const auto& [g, n] : s.brings)
        society_.setPressure(s.port, g, society_.pressure(s.port, g) + StormNudge, "ship late");
    society_.stagedBoard().troubles.push_back({calendarDays_, s.port, "late"});
    const auto* p = port(s.port);
    recordEvent({"ship stormbound", s.id, s.port, p ? p->cell : std::string(), 0, 0, s.name, int(s.brings.size()), 0,
                 capital(s.name) + " is stormbound and will not come."});
}

void World::shipDocks(market::Ship& s)
{
    // Ship day (8.3, 8.6): its cargo pulled halfway toward the ship baseline at the port's market (never up), what it
    // takes halfway toward its own (never down: the ship's buyers are about), and every good the port waits for ships for
    // refilled, shelf and back room, its keepers unloading.
    s.status = "in";
    for (const auto& [g, n] : s.brings)
        if (const double P = society_.pressure(s.port, g); P > ShipBaseline + 1e-4)
            society_.setPressure(s.port, g, (P + ShipBaseline) / 2, "ship");
    for (const auto& [g, n] : s.takes)
        if (const double P = society_.pressure(s.port, g); P < TakesBaseline - 1e-4)
            society_.setPressure(s.port, g, (P + TakesBaseline) / 2, "ship");
    const auto* p = port(s.port);
    if (p)
        for (const auto& g : p->imports)
            society_.arrivalRefill(s.port, g, 1, BackRooms);
    shipsIn_[s.port] = s.name;
    std::vector<std::string> goods;
    for (const auto& [g, n] : s.brings)
        goods.push_back(goodWords(g));
    recordEvent({"ship docks", s.id, s.port, p ? p->cell : std::string(), 0, 0, s.name, int(s.brings.size()), 0,
                 capital(s.name) + " is in" + (goods.empty() ? std::string() : ", unloading " + listed(goods)) + "."});
}

void World::shipSails(market::Ship& s)
{
    // It sails that evening (8.3): what the port's shops hold over their shelves of the goods it takes goes with it (7.5:
    // goods move on).
    s.status = "sailed";
    int pieces = 0;
    for (const auto& [g, n] : s.takes)
        pieces += society_.sendOn(s.port, g);
    if (const auto in = shipsIn_.find(s.port); in != shipsIn_.end() && in->second == s.name)
        shipsIn_.erase(in);
    std::vector<std::string> goods;
    for (const auto& [g, n] : s.takes)
        goods.push_back(goodWords(g));
    const auto* p = port(s.port);
    recordEvent({"ship sails", s.id, s.port, p ? p->cell : std::string(), 0, 0, s.name, pieces, 0,
                 capital(s.name) + " sailed" + (goods.empty() ? std::string() : " with " + listed(goods)) + "."});
}

void World::tendPort()
{
    // Each pass of the roads: ships whose port is gone (a host's edit) or whose rule stops them (shipsPerWeek 0: those
    // still due) are dropped; a due ship's day begun, a storm may keep it out (seeded by the ship; or the DM's); at its
    // hour it docks; at its evening hour it sails. All by the saved timetable, so a restart at any hour catches up.
    auto& b = society_.stagedBoard();
    if (b.ships.empty() && b.shipsThrough.empty() && ports_.empty())
        return;
    const bool shipsRun = society_.rule("shipsPerWeek") > 0;
    b.ships.erase(std::remove_if(b.ships.begin(), b.ships.end(), [&](const market::Ship& s) {
                      return !port(s.port) || (!shipsRun && s.status == "due");
                  }),
                  b.ships.end());
    for (auto it = b.shipsThrough.begin(); it != b.shipsThrough.end();)
        it = !port(it->first) || !shipsRun ? b.shipsThrough.erase(it) : std::next(it);
    // (A port with no week rolled yet: a save from before the ships, a port a host just made, the rule turned on again.
    // Its timetable from today, not from tomorrow's morning.)
    if (shipsRun && std::any_of(ports_.begin(), ports_.end(), [&](const Port& p) { return !b.shipsThrough.count(p.market); }))
        rollPorts(std::int64_t(std::floor(calendarDays_)));
    shipsIn_.clear();
    for (auto& s : b.ships)
    {
        if (s.status == "due" && calendarDays_ >= double(s.day))
        {
            const auto season = calendar::calendarAt(double(s.day) + .5).season;
            if (stormsCalled_.count(s.id) || unit("storm:" + s.port + ":" + s.name, s.day) < stormOdds(season))
                shipStormbound(s);
            else if (calendarDays_ >= double(s.day) + s.hour / 24)
                shipDocks(s);
        }
        if (s.status == "in" && calendarDays_ >= double(s.day) + s.sails / 24)
            shipSails(s);
        if (s.status == "in")
            shipsIn_[s.port] = s.name;
    }
    for (auto it = stormsCalled_.begin(); it != stormsCalled_.end();)
        it = std::none_of(b.ships.begin(), b.ships.end(), [&](const market::Ship& s) { return s.id == *it && s.status == "due"; })
                 ? stormsCalled_.erase(it) : std::next(it);
    postShips();
}

bool World::stormShip(const std::string& id)
{
    auto& b = society_.stagedBoard();
    for (auto& s : b.ships)
        if (s.id == id && s.status == "due")
        {
            stormsCalled_.erase(id);
            shipStormbound(s);
            postShips();
            return true;
        }
    return false;
}

bool World::portErrand(const std::string& resident, const ResidentLife& life, std::string& task, std::string& reason,
                       std::string& goalCell, Vec2& goal) const
{
    // The Wharf's posts while a ship is in (8.3: "the Wharf's own posts are the dock hands"): each at its work as ever,
    // where its post is, and seen at the ship's cargo, by name. Only while it would be working anyway.
    if (shipsIn_.empty())
        return false;
    const auto hand = dockHands_.find(resident);
    if (hand == dockHands_.end() || life.task != hand->second.title || life.goalCell.empty())
        return false;
    const auto in = shipsIn_.find(hand->second.market);
    if (in == shipsIn_.end())
        return false;
    const auto title = lowered(hand->second.title);
    task = hand->second.hauls                               ? "unloading " + in->second
           : title.find("tallying") != std::string::npos ? "tallying " + in->second + "'s cargo"
                                                          : "seeing to " + in->second + "'s cargo";
    reason = "A ship is in.";
    goalCell = life.goalCell;
    goal = {life.goalX, life.goalY};
    return true;
}

const market::Ship* World::shipIn(const std::string& market) const
{
    for (const auto& s : society_.board().ships)
        if (s.port == market && s.status == "in")
            return &s;
    return nullptr;
}

void World::shipFacts(const std::string& market, std::map<std::string, std::string>& facts) const
{
    // Talk's `world.ships` and `world.ship_name` (8.3, 9.1): the ship in; else one a storm kept out these two days; else
    // one due today; else one sailed since yesterday; else one due within two days; else "no". Nothing without a port.
    if (!port(market))
        return;
    const auto today = std::int64_t(std::floor(calendarDays_));
    const market::Ship* best = nullptr;
    int rank = 9;
    for (const auto& s : society_.board().ships)
    {
        if (s.port != market)
            continue;
        const int r = s.status == "in" ? 0
                      : s.status == "storm" && today - s.day <= 2 ? 1
                      : s.status == "due" && s.day == today ? 2
                      : s.status == "sailed" && today - s.day <= 1 ? 3
                      : s.status == "due" && s.day - today <= 2 ? 4 : 9;
        if (r < rank)
            rank = r, best = &s;
    }
    facts["world.ships"] = rank == 0 ? "in" : rank == 1 ? "late" : rank == 3 ? "sailed" : rank < 9 ? "due" : "no";
    if (best)
        facts["world.ship_name"] = best->name;
}

bool World::wharfCrowd(const std::string& cell) const
{
    for (const auto& p : ports_)
        if (shipsIn_.count(p.market) && p.cells.count(cell))
            return true;
    return false;
}

std::string World::shipsNotice(const std::string& market) const
{
    // The timetable in words, with the calendar's weekday names (8.3): the ship in and when it sails, one just sailed,
    // those due in the next seven days, one a storm kept out ("No ship this week" where no other came or is due that
    // week). "" with nothing to tell.
    const auto today = std::int64_t(std::floor(calendarDays_));
    std::vector<const market::Ship*> ships;
    for (const auto& s : society_.board().ships)
        if (s.port == market)
            ships.push_back(&s);
    if (ships.empty())
        return {};
    std::stable_sort(ships.begin(), ships.end(), [](const market::Ship* a, const market::Ship* b) {
        return a->day != b->day ? a->day < b->day : a->hour < b->hour;
    });
    const auto words = [&](const std::vector<std::pair<std::string, int>>& goods, std::size_t most) {
        std::vector<std::string> out;
        for (std::size_t i = 0; i < goods.size() && i < most; ++i)
            out.push_back(goodWords(goods[i].first));
        return listed(out);
    };
    const auto when = [&](const market::Ship& s) {
        return s.day == today ? std::string("today") : calendar::weekdayName(calendar::weekdayOf(double(s.day)));
    };
    std::string text = "Ships at " + marketWords(market) + ".";
    std::string storms;
    for (const auto* s : ships)
    {
        if (s->status == "in")
        {
            text += " " + capital(s->name) + " is in";
            if (!s->brings.empty())
                text += ", unloading " + words(s->brings, BringsMost);
            text += s->takes.empty() ? ", and sails this evening." : ", and sails this evening with " + words(s->takes, TakesMost) + ".";
        }
        else if (s->status == "sailed")
            text += " " + capital(s->name) + " sailed " + (s->day == today ? std::string("today") : "on " + when(*s)) +
                    (s->takes.empty() ? std::string() : " with " + words(s->takes, TakesMost)) + ".";
        else if (s->status == "storm")
        {
            const auto week = s->day / Week;
            const bool another = std::any_of(ships.begin(), ships.end(), [&](const market::Ship* o) {
                return o != s && o->status != "storm" && o->day / Week == week;
            });
            storms += " " + capital(s->name) + " is stormbound and will not come." + (another ? "" : " No ship this week.");
        }
    }
    bool any = false;
    for (const auto* s : ships)
    {
        if (s->status != "due" || s->day >= today + Week)
            continue;
        std::string one = s->name + (s->day == today ? " today" : " on " + when(*s));
        if (!s->brings.empty())
            one += " with " + words(s->brings, 3);
        if (!s->takes.empty())
            one += ", taking " + words(s->takes, 2);
        const std::string lead = any ? "; " : " Due: ";
        if (text.size() + lead.size() + one.size() + 1 + storms.size() > NoticeRoom)
        {
            // (A full week's ships: the rest by name and day only, while they fit.)
            one = s->name + (s->day == today ? " today" : " on " + when(*s));
            if (text.size() + lead.size() + one.size() + 1 + storms.size() > NoticeRoom)
                break;
        }
        text += lead + one;
        any = true;
    }
    if (any)
        text += ".";
    text += storms;
    return text == "Ships at " + marketWords(market) + "." ? std::string() : text;
}

void World::postShips()
{
    // The notice (8.3): each port's timetable, posted whenever its words change and afresh each morning (a notice stays up
    // seven days); taken down where there is nothing to tell, or the port is gone.
    const auto today = std::int64_t(std::floor(calendarDays_));
    for (const auto& p : ports_)
    {
        auto words = shipsNotice(p.market);
        const auto last = shipsPosted_.find(p.market);
        if (last != shipsPosted_.end() && last->second.first == today && last->second.second == words)
            continue;
        if (words.empty() && last == shipsPosted_.end() && !society_.board().shipsThrough.count(p.market))
        {
            shipsPosted_[p.market] = {today, words};   // (Never any ships here: nothing to take down either.)
            continue;
        }
        recordEvent({words.empty() ? "ships cleared" : "ships posted", p.market, p.market, p.cell, 0, 0, {}, 0, 0, words});
        shipsPosted_[p.market] = {today, std::move(words)};
    }
    for (auto it = shipsPosted_.begin(); it != shipsPosted_.end();)
    {
        if (port(it->first))
        {
            ++it;
            continue;
        }
        recordEvent({"ships cleared", it->first, it->first, {}, 0, 0, {}, 0, 0, {}});
        it = shipsPosted_.erase(it);
    }
}

void World::dropOrphanedShips()
{
    // After a load (5.1: "Delete the Wharf and there are no ships"): a port's ships whose port is gone are dropped with
    // its week's roll and its notice; nothing else of the save is touched. What happened while the world was down (a ship's
    // hour passed) is caught up at the first pass of the roads (tendPort).
    ensureRoadMarkets();
    auto& b = society_.stagedBoard();
    std::set<std::string> gone;
    for (const auto& s : b.ships)
        if (!port(s.port))
            gone.insert(s.port);
    for (const auto& [market, week] : b.shipsThrough)
        if (!port(market))
            gone.insert(market);
    b.ships.erase(std::remove_if(b.ships.begin(), b.ships.end(), [&](const market::Ship& s) { return gone.count(s.port) > 0; }), b.ships.end());
    for (const auto& market : gone)
    {
        b.shipsThrough.erase(market);
        recordEvent({"ships cleared", market, market, {}, 0, 0, {}, 0, 0, "its port is gone"});
    }
    shipsIn_.clear();
    shipsPosted_.clear();
    stormsCalled_.clear();
    for (const auto& s : b.ships)
        if (s.status == "in")
            shipsIn_[s.port] = s.name;
}
} // namespace ratw
