// The port (Docs/Design/79-a-staged-economy.md, 8.3, 8.6 and D8; Phase 5 and its repair): ships as a timetable, never
// seen. A port is a market with a post of the Wharf's own words (market.json's `wharf.posts`). Each port's ships are
// rolled a week ahead (the rule shipsPerWeek, one more or fewer by the slider and the season, seeded by the week and the
// port; never Restday's) and kept on the Market Board (market::Board::ships), so they are saved, journalled and restored
// with it: a restart shows the same ships. On its day a ship docks at its hour (its cargo cheaper at the port's market
// toward a baseline, what it takes dearer, the goods the port waits for ships for refilled, the Wharf's posts at its
// cargo, the Wharf a crowd while they are) and sails that evening (what the shops hold over their shelves of what it
// takes goes with it); a storm keeps one out (its cargo dearer, a trouble for the market's Times, "No ship this week"
// where that is so). The timetable is a notice on the port's board (Game::portNotices), and talk's `world.ships` and
// `world.ship_name` (World::talkTown). A world with no port has none of it, and its goods refill each morning as ever; a
// port deleted loses its ships and its notice at the next load or tick. Ships are their own timetable, not the roads'
// `committed` departures: nothing travels, so there is no route, no wagon and no carter to commit.
#include "RatwCalendar.h"
#include "RatwItems.h"
#include "RatwJsonDoc.h"
#include "RatwMarket.h"
#include "RatwSeeded.h"
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
using seeded::listed;
using seeded::lot;
using seeded::lowered;
using seeded::unit;
// "the Gull" at a sentence's start: "The Gull".
std::string capital(std::string s)
{
    if (!s.empty())
        s[0] = char(std::toupper(static_cast<unsigned char>(s[0])));
    return s;
}
// The sea trade's posts (Society::postRules): a port's own where it has a post of the Wharf's words (findPorts).
const std::vector<std::string>& seaTradeWords()
{
    static const std::vector<std::string> words = [] {
        std::vector<std::string> out;
        for (const auto& rule : Society::postRules())
            if (rule.name == "the sea trade")
                out.insert(out.end(), rule.words.begin(), rule.words.end());
        return out;
    }();
    return words;
}
bool hasAny(const std::string& text, const std::vector<std::string>& words)
{
    return std::any_of(words.begin(), words.end(), [&](const std::string& w) { return text.find(w) != std::string::npos; });
}
constexpr std::int64_t Week = calendar::DaysPerWeek;
// A ship's cargo is pulled halfway toward the ship baseline, only down; what it takes halfway toward its own, only up; a
// stormbound ship's cargo a step dearer (8.3, 8.6). (How far is Phase 10's to tune with play.)
constexpr double ShipBaseline = -.1, TakesBaseline = .1, StormNudge = .1;
constexpr int BringsMost = 4, TakesMost = 3;
constexpr double BackRooms = 2;                      // (A ship fills two shelves' worth behind: the next is days off.)
// A sailed ship is told of today and tomorrow, and kept on the timetable through its week's end (so "No ship this week"
// is never said of a week one came in); a stormbound one three days.
constexpr std::int64_t SailedKeptDays = 1, StormKeptDays = 3;
constexpr std::size_t NoticeMost = 280;              // (A notice's own most: Game's TextMost.)
const char* const NoShipCause = "No ship is in.";
double stormOdds(calendar::Season season)
{
    return season == calendar::Season::Winter ? .15 : season == calendar::Season::Autumn ? .06 : .03;
}
bool stormSeeded(const market::Ship& s)
{
    return unit("storm:" + s.port + ":" + s.name, s.day) < stormOdds(calendar::calendarAt(double(s.day) + .5).season);
}
bool stale(const market::Ship& s, std::int64_t today)
{
    if (s.status == "sailed")
        return today > std::max(s.day + SailedKeptDays, s.day / Week * Week + Week - 1);
    return s.status == "storm" && today > s.day + StormKeptDays;
}
// "this evening", by the hour it sails.
const char* partOfDay(double hour)
{
    return hour >= 17 ? "this evening" : hour >= 12 ? "this afternoon" : "this morning";
}
} // namespace

const World::Port* World::port(const std::string& market) const
{
    for (const auto& p : ports_)
        if (p.market == market)
            return &p;
    return nullptr;
}

std::string World::cargoWords(const std::string& base)
{
    // A good as a ship's cargo: its plain words, a thing counted by the piece in the plural ("hides", "small pelts",
    // "wool blankets", "handbells"), a thing by the measure as it is ("butter", "wheat", "copper ore"). The catalogue
    // has no plurals, so: a packed good (its name has its pack) and words already plural stay; the kinds of thing sold
    // by the piece, and a few words that are, take an s; market.json's `wharf.cargoWords` says the rest.
    const auto& own = market::data().cargoWords;
    if (const auto found = own.find(base); found != own.end())
        return found->second;
    auto words = goodWords(base);
    const auto* item = items::good(base);
    if (!item || words.size() < 2 || item->name.find(" (") != std::string::npos || words.back() == 's')
        return words;
    for (const char* joined : {" with ", " and ", " or ", " on a "})
        if (words.find(joined) != std::string::npos)
            return words;
    const auto cut = words.find_last_of(" -");
    const auto last = cut == std::string::npos ? words : words.substr(cut + 1);
    static const std::set<std::string> uncounted{"gear", "dice", "tack", "harness"};
    static const std::set<std::string> byThePiece{"container", "tool", "instrument", "household", "luxury", "stationery", "document"};
    static const std::set<std::string> counted{"pelt", "hide", "sheepskin", "horn", "axle", "beam", "wheel", "millstone",
                                               "antler", "horse", "pearl", "gemstone", "garnet"};
    if (uncounted.count(last) || (!byThePiece.count(item->category) && !counted.count(last)))
        return words;
    const auto ends = [&](const char* tail) {
        const std::string t = tail;
        return words.size() >= t.size() && words.compare(words.size() - t.size(), std::string::npos, t) == 0;
    };
    const auto vowel = [](char c) { return c == 'a' || c == 'e' || c == 'i' || c == 'o' || c == 'u'; };
    if (ends("x") || ends("ch") || ends("sh"))
        return words + "es";
    if (words.back() == 'y' && !vowel(words[words.size() - 2]))
        return words.substr(0, words.size() - 1) + "ies";
    return words + "s";
}

void World::findPorts(std::map<std::string, std::set<std::string>>& imports)
{
    // The ports (8.3), found again with the road markets. A market is a port where a post of the Wharf's own words
    // (market.json's `wharf.posts`: its office, its cranes, its harbourmaster) works; its posts are those and the sea
    // trade's others there (its carriers, its store keepers and talliers: a town with a "long store" and no wharf is no
    // port). A post is of the market its work room is in, else of the nearest by path: one Wharf, one port, wherever
    // its hands live. And each port's goods that wait for a ship (7.7, D27), unless the DM's rule stops the ships
    // (shipsPerWeek 0), when they refill each morning as in a world with no port.
    ports_.clear();
    dockHands_.clear();
    shipsChanged_ = true;
    struct Post
    {
        std::size_t at = 0;
        const Town* market = nullptr;
        bool sure = false;
        std::string title;
    };
    std::vector<Post> posts;
    std::set<std::string> sure;
    std::map<std::string, const Town*> nearest;
    const auto& all = society_.positions();
    for (std::size_t i = 0; i < all.size(); ++i)
    {
        const auto& p = all[i];
        if (p.role == "merchant" || p.role == "guard")
            continue;
        Post post{i, nullptr, false, lowered(p.title)};
        post.sure = hasAny(post.title, market::data().wharfPosts);
        if (!post.sure && !hasAny(post.title, seaTradeWords()))
            continue;
        post.market = roadMarketOf(p.work.cell);
        if (!post.market && cells_.count(p.work.cell))
        {
            const auto [slot, fresh] = nearest.try_emplace(p.work.cell, nullptr);
            if (fresh)
            {
                std::size_t best = std::numeric_limits<std::size_t>::max();
                for (const auto& candidate : roadMarkets_)
                    if (const auto route = routeBetween(p.work.cell, candidate.market); !route.empty() && route.size() < best)
                        best = route.size(), slot->second = &candidate;
            }
            post.market = slot->second;
        }
        if (!post.market)
            continue;
        if (post.sure)
            sure.insert(post.market->id);
        posts.push_back(std::move(post));
    }
    std::map<std::string, Port> found;
    std::map<std::string, std::map<std::string, int>> crowd;      // Port -> cell -> its posts there.
    const auto& held = society_.state().careers.positions;
    for (const auto& post : posts)
    {
        const auto& id = post.market->id;
        if (!sure.count(id))
            continue;
        const auto& p = all[post.at];
        std::string holder;
        if (const auto h = held.find(p.id); h != held.end())
            holder = h->second.holder;
        const auto* body = holder.empty() ? nullptr : entity(holder);
        const bool manned = body && !body->dead && society_.resident(holder);
        auto& port = found[id];
        port.market = id;
        ++port.posts;
        port.held += manned;
        if (cells_.count(p.work.cell))
        {
            port.cells.insert(p.work.cell);
            ++crowd[id][p.work.cell];
        }
        if (!manned)
            continue;
        DockHand hand{id, p.title,
                      post.title.find("unloading") != std::string::npos || post.title.find("cranes") != std::string::npos ||
                          post.title.find("carrying") != std::string::npos,
                      {}};
        // (A post named for the ships it unloads is seen minding the cargo between them: portErrand.)
        static const std::string Unloading = "unloading ships";
        if (const auto at = post.title.find(Unloading); at != std::string::npos)
            hand.idle = p.title.substr(0, at) + "minding the cargo" + p.title.substr(at + Unloading.size());
        dockHands_[holder] = std::move(hand);
    }
    const bool shipsRun = society_.rule("shipsPerWeek") > 0;
    // (What the realm makes: any market's workshops and areas, any farm or works.)
    std::set<std::string> realmMakes;
    if (shipsRun && !found.empty())
    {
        for (const auto& tp : society_.tradePoints().markets)
            realmMakes.insert(tp.crafted.begin(), tp.crafted.end());
        for (const auto& farm : society_.farms())
            if (farm.def)
                realmMakes.insert(farm.def->yields.begin(), farm.def->yields.end());
    }
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
        {
            const auto& d = market::data();
            const auto waits = [&](const std::string& g) {
                return tp->sold.count(g) && !tp->crafted.count(g) && items::good(g) && !d.shipNever.count(g);
            };
            // market.json's imports first (the ships' own goods), then whatever else its shops set out that nothing in
            // the realm makes and no wagon or cart brings: only a ship could.
            for (const auto& g : d.shipBrings)
                if (waits(g) && std::find(port.imports.begin(), port.imports.end(), g) == port.imports.end())
                    port.imports.push_back(g);
            const auto wagons = imports.find(id);
            for (const auto& g : tp->sold)
                if (waits(g) && !realmMakes.count(g) && !(wagons != imports.end() && wagons->second.count(g)) &&
                    std::find(port.imports.begin(), port.imports.end(), g) == port.imports.end())
                    port.imports.push_back(g);
            if (!port.imports.empty())
                imports[id].insert(port.imports.begin(), port.imports.end());
        }
        ports_.push_back(std::move(port));
    }
}

int World::shipsInWeek(const std::string& market, std::int64_t week) const
{
    // 1-3 ships a week by slider and season at the rule's own 2 (8.3): the rule, one more or fewer by a seeded score the
    // slider and the season lean (fewer in the winter's storms, never more then); none while the rule is 0. Never more
    // than a week's six working days: no ship comes in on Restday, when nobody is at the Wharf to unload it.
    const int rule = society_.rule("shipsPerWeek");
    if (rule <= 0 || !port(market))
        return 0;
    const auto season = calendar::calendarAt(double(week * Week) + 3.5).season;
    const double lean = season == calendar::Season::Winter ? -.3 : season == calendar::Season::Autumn ? -.05
                        : season == calendar::Season::Summer ? .1 : 0;
    const double score = unit("ships:" + market, week) - .5 + double(society_.board().slider) / 400. + lean;
    return std::clamp(rule + (score < -.2 ? -1 : score > .2 ? 1 : 0), 1, int(Week) - 1);
}

void World::rollShips(const Port& port, std::int64_t week, std::int64_t today)
{
    // A week of a port's ships, committed (8.3): their days (never Restday), their hours (in by late morning, out that
    // evening), their names from market.json's list (none twice on the timetable), and what each brings and takes, in
    // rotation by its day (the same port and day, the same cargo, whatever else was rolled): of the port's goods that
    // wait for ships (market.json's own first), and of market.json's `wharf.takes` its shops sell, buy or make.
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
    const auto& d = market::data();
    std::vector<std::string> listedImports, otherImports, takes;
    for (const auto& g : port.imports)
        (std::find(d.shipBrings.begin(), d.shipBrings.end(), g) != d.shipBrings.end() ? listedImports : otherImports).push_back(g);
    if (tp)
        for (const auto& g : d.shipTakes)
            if ((tp->sold.count(g) || tp->bought.count(g) || tp->made.count(g)) &&
                std::find(port.imports.begin(), port.imports.end(), g) == port.imports.end())
                takes.push_back(g);
    const auto& names = d.shipNames;
    const auto ofPort = [&] {
        return std::size_t(std::count_if(b.ships.begin(), b.ships.end(), [&](const market::Ship& o) { return o.port == port.market; }));
    };
    for (const auto day : days)
    {
        // (Only what a notice can promise ahead: never today's. One ship a port a day, so a week rolled again, after
        // the rule was off, adds only what is missing.)
        if (day <= today ||
            std::any_of(b.ships.begin(), b.ships.end(), [&](const market::Ship& o) { return o.port == port.market && o.day == day; }))
            continue;
        // (Bounded a port, so no port crowds out another's: at its most, the oldest ship sailed or stormbound makes
        // room for the one to come, which a notice needs more.)
        if (ofPort() >= market::ShipsAPort)
        {
            auto oldest = b.ships.end();
            for (auto it = b.ships.begin(); it != b.ships.end(); ++it)
                if (it->port == port.market && (it->status == "sailed" || it->status == "storm") &&
                    (oldest == b.ships.end() || it->day < oldest->day))
                    oldest = it;
            if (oldest != b.ships.end())
                b.ships.erase(oldest);
        }
        if (b.ships.size() >= market::ShipsMost || ofPort() >= market::ShipsAPort)
            continue;
        market::Ship s;
        s.id = "s" + std::to_string(b.nextShip++);
        s.port = port.market;
        s.day = day;
        const auto pick = lot("ship:" + port.market, day);
        s.hour = 8.5 + double(pick % 6) * .5;                   // 8:30 to 11:00: the Wharf is at work.
        s.sails = 17 + double(pick / 7 % 6) * .5;               // 17:00 to 19:30: "this evening".
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
        const auto fill = [&](const std::vector<std::string>& from, int step, int most, std::vector<std::pair<std::string, int>>& into) {
            if (from.empty())
                return;
            const auto start = std::size_t(lot("cargo:" + port.market, 0) % from.size()) + std::size_t(day) * std::size_t(step);
            for (std::size_t i = 0; i < from.size() && int(into.size()) < most; ++i)
            {
                const auto& g = from[(start + i) % from.size()];
                if (std::none_of(into.begin(), into.end(), [&](const auto& have) { return have.first == g; }))
                    into.push_back({g, std::clamp(society_.marketShelf(port.market, g), 1, 999)});
            }
        };
        // (Half of market.json's own where there are others, and the others first on some days, so a notice reads of
        // more than butter, and not of butter first every time.)
        const int ownMost = otherImports.empty() ? BringsMost : std::min(BringsMost / 2, int(listedImports.size()));
        const int otherMost = std::min(BringsMost - ownMost, int(otherImports.size()));
        if (otherMost > 0 && lot("lead:" + port.market, day) % 2 == 1)
        {
            fill(otherImports, otherMost, otherMost, s.brings);
            fill(listedImports, std::max(1, ownMost), BringsMost, s.brings);
        }
        else
        {
            fill(listedImports, std::max(1, ownMost), ownMost, s.brings);
            fill(otherImports, std::max(1, otherMost), BringsMost, s.brings);
            fill(listedImports, std::max(1, ownMost), BringsMost, s.brings);   // (Few others: more of its own.)
        }
        fill(takes, TakesMost, TakesMost, s.takes);
        s.posted = double(today);                   // (The day, never the instant: the same world posts the same timetable.)
        recordEvent({"ship posted", s.id, port.market, port.cell, 0, 0, s.name, int(s.brings.size()), 0,
                     capital(s.name) + ", due " + calendar::weekdayName(calendar::weekdayOf(double(day))) + "."});
        b.ships.push_back(std::move(s));
        shipsChanged_ = true;
    }
}

void World::portMorning(std::int64_t today)
{
    // The port's day (8.3): ships long sailed or stormbound drop off the timetable; each port's weeks rolled through the
    // next (so the next seven days are always committed); the notice posted again.
    auto& b = society_.stagedBoard();
    b.ships.erase(std::remove_if(b.ships.begin(), b.ships.end(), [&](const market::Ship& s) { return stale(s, today); }), b.ships.end());
    rollPorts(today);
    shipsPosted_.clear();                           // (Each morning's timetable is posted afresh: tendPort's.)
    shipsChanged_ = true;
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
    shipsChanged_ = true;
    for (const auto& [g, n] : s.brings)
        society_.setPressure(s.port, g, society_.pressure(s.port, g) + StormNudge, "ship late");
    society_.stagedBoard().troubles.push_back({calendarDays_, s.port, "late"});
    const auto* p = port(s.port);
    recordEvent({"ship stormbound", s.id, s.port, p ? p->cell : std::string(), 0, 0, s.name, int(s.brings.size()), 0,
                 capital(s.name) + " is stormbound and will not come."});
}

void World::shipDocks(market::Ship& s)
{
    // Ship day (8.3, 8.6): its cargo pulled halfway toward the ship baseline at the port's market (never up; the cause
    // "ship"), what it takes halfway toward its own (never down: the ship's buyers are about; "ship takes"), and every
    // good the port waits for ships for refilled, shelf and back room.
    s.status = "in";
    shipsChanged_ = true;
    for (const auto& [g, n] : s.brings)
        if (const double P = society_.pressure(s.port, g); P > ShipBaseline + 1e-4)
            society_.setPressure(s.port, g, (P + ShipBaseline) / 2, "ship");
    for (const auto& [g, n] : s.takes)
        if (const double P = society_.pressure(s.port, g); P < TakesBaseline - 1e-4)
            society_.setPressure(s.port, g, (P + TakesBaseline) / 2, "ship takes");
    const auto* p = port(s.port);
    if (p)
        for (const auto& g : p->imports)
            society_.arrivalRefill(s.port, g, 1, BackRooms);
    shipsIn_[s.port] = s.name;
    std::vector<std::string> goods;
    for (const auto& [g, n] : s.brings)
        goods.push_back(cargoWords(g));
    recordEvent({"ship docks", s.id, s.port, p ? p->cell : std::string(), 0, 0, s.name, int(s.brings.size()), 0,
                 capital(s.name) + " is in" + (goods.empty() ? std::string() : " with " + listed(goods)) + "."});
}

void World::shipSails(market::Ship& s)
{
    // It sails that evening (8.3): what the port's shops hold over their shelves of the goods it takes goes with it (7.5:
    // goods move on).
    s.status = "sailed";
    shipsChanged_ = true;
    int pieces = 0;
    for (const auto& [g, n] : s.takes)
        pieces += society_.sendOn(s.port, g);
    if (const auto in = shipsIn_.find(s.port); in != shipsIn_.end() && in->second == s.name)
        shipsIn_.erase(in);
    std::vector<std::string> goods;
    for (const auto& [g, n] : s.takes)
        goods.push_back(cargoWords(g));
    const auto* p = port(s.port);
    recordEvent({"ship sails", s.id, s.port, p ? p->cell : std::string(), 0, 0, s.name, pieces, 0,
                 capital(s.name) + " sailed" + (goods.empty() ? std::string() : " with " + listed(goods)) + "."});
}

void World::tendPort()
{
    // Each pass of the roads, all by the saved timetable, so a restart at any hour catches up. Ships whose port is gone
    // (a host's edit) or whose rule stops them (shipsPerWeek 0: those still due) are dropped. A ship whose day is more
    // than a day gone (the world was down, or its calendar was moved on) came and went unseen: its goods are on the
    // shelves and what it took is gone, quietly, with no word of it and no crowd. Then, a due ship's day begun, a storm
    // may keep it out (seeded by the ship; or called: stormShip); at its hour it docks; at its evening hour it sails.
    auto& b = society_.stagedBoard();
    if (b.ships.empty() && b.shipsThrough.empty() && ports_.empty())
        return;
    const bool shipsRun = society_.rule("shipsPerWeek") > 0;
    const auto today = std::int64_t(std::floor(calendarDays_));
    const auto week = today / Week;
    if (shipsStoppedDay_ != today)
        shipsStopped_.clear();
    // (The rule set back the day it stopped them: the ships it dropped that are still to come are back as they were,
    // the same ships under the same ids, and the weeks are rolled over them, which adds only what is missing. One whose
    // hour passed while the rule was off did not come.)
    if (shipsRun && !shipsStopped_.empty())
    {
        for (auto& s : shipsStopped_)
            if (port(s.port) && calendarDays_ < double(s.day) + s.hour / 24 && b.ships.size() < market::ShipsMost &&
                std::none_of(b.ships.begin(), b.ships.end(), [&](const market::Ship& o) { return o.id == s.id || (o.port == s.port && o.day == s.day); }) &&
                std::size_t(std::count_if(b.ships.begin(), b.ships.end(), [&](const market::Ship& o) { return o.port == s.port; })) < market::ShipsAPort)
            {
                b.ships.push_back(std::move(s));
                shipsChanged_ = true;
            }
        shipsStopped_.clear();
    }
    std::set<std::string> refilled;
    for (auto& s : b.ships)
    {
        const auto* p = port(s.port);
        if (!p || (!shipsRun && s.status == "due"))
        {
            if (p)
            {
                shipsStopped_.push_back(s);
                shipsStoppedDay_ = today;
            }
            s.status = "gone";
            continue;
        }
        if ((s.status != "due" && s.status != "in") || today <= s.day + SailedKeptDays)
            continue;
        if (s.status == "in" || !stormSeeded(s))
        {
            if (s.status == "due" && refilled.insert(s.port).second)
                for (const auto& g : p->imports)
                    society_.arrivalRefill(s.port, g, 1, BackRooms);
            for (const auto& [g, n] : s.takes)
                society_.sendOn(s.port, g);
        }
        s.status = "gone";
    }
    const auto before = b.ships.size();
    b.ships.erase(std::remove_if(b.ships.begin(), b.ships.end(), [&](const market::Ship& s) { return s.status == "gone" || stale(s, today); }),
                  b.ships.end());
    shipsChanged_ = shipsChanged_ || b.ships.size() != before;
    // (A port gone loses its week's roll. The rule at 0 keeps it, a week back, until its notice is down (postShips), and
    // so that the timetable is rolled again from the day the rule is set back.)
    for (auto it = b.shipsThrough.begin(); it != b.shipsThrough.end();)
    {
        if (!port(it->first))
        {
            it = b.shipsThrough.erase(it);
            continue;
        }
        if (!shipsRun)
            it->second = std::min(it->second, week - 1);
        ++it;
    }
    // (A port with its weeks not rolled: a save from before the ships, a port a host just made, the rule turned on
    // again. Its timetable from today, not from tomorrow's morning.)
    if (shipsRun && std::any_of(ports_.begin(), ports_.end(), [&](const Port& p) {
            const auto through = b.shipsThrough.find(p.market);
            return through == b.shipsThrough.end() || through->second < week + 1;
        }))
        rollPorts(today);
    shipsIn_.clear();
    for (auto& s : b.ships)
    {
        if (s.status == "due" && calendarDays_ >= double(s.day))
        {
            if (stormSeeded(s))
                shipStormbound(s);
            else if (calendarDays_ >= double(s.day) + s.hour / 24)
                shipDocks(s);
        }
        if (s.status == "in" && calendarDays_ >= double(s.day) + s.sails / 24)
            shipSails(s);
        if (s.status == "in")
            shipsIn_[s.port] = s.name;
    }
    shipsAtWork();
    postShips();
}

bool World::stormShip(const std::string& id)
{
    auto& b = society_.stagedBoard();
    for (auto& s : b.ships)
        if (s.id == id && s.status == "due")
        {
            shipStormbound(s);
            postShips();
            return true;
        }
    return false;
}

bool World::dockShip(const std::string& id)
{
    // A ship still due brought in now, whatever its day and hour were: it sails this evening as posted, or in two hours
    // where that hour is past. Not while another of its port's is in, or was today.
    auto& b = society_.stagedBoard();
    const auto today = std::int64_t(std::floor(calendarDays_));
    const double hour = (calendarDays_ - double(today)) * 24;
    for (auto& s : b.ships)
    {
        if (s.id != id || s.status != "due" || !port(s.port))
            continue;
        if (std::any_of(b.ships.begin(), b.ships.end(), [&](const market::Ship& o) {
                return &o != &s && o.port == s.port && (o.status == "in" || o.day == today);
            }))
            return false;
        s.day = today;
        s.hour = hour;
        if (s.sails < hour + 1)
            s.sails = std::min(23.75, hour + 2);
        shipDocks(s);
        shipsAtWork();
        postShips();
        return true;
    }
    return false;
}

void World::shipsAtWork()
{
    // The ports whose hands are at work on a ship in (the crowd at the Wharf, here.crowd): a ship in at night, or on a
    // day nobody works, is a ship at a quiet pier.
    shipsWorked_.clear();
    if (shipsIn_.empty())
        return;
    for (const auto& [who, hand] : dockHands_)
        if (shipsIn_.count(hand.market) && !shipsWorked_.count(hand.market))
            if (const auto* life = society_.resident(who); life && life->task == hand.title)
                shipsWorked_.insert(hand.market);
}

bool World::portErrand(const std::string& resident, const ResidentLife& life, std::string& task, std::string& reason,
                       std::string& goalCell, Vec2& goal) const
{
    // The Wharf's posts (8.3: "the Wharf's own posts are the dock hands"): each at its work as ever, where its post is.
    // While a ship is in, seen at the ship's cargo, by name; with none in, a post named for the ships it unloads is
    // seen minding the cargo, never unloading a ship that isn't there. Only while it would be working anyway.
    if (dockHands_.empty())
        return false;
    const auto hand = dockHands_.find(resident);
    if (hand == dockHands_.end() || life.task != hand->second.title || life.goalCell.empty())
        return false;
    const auto in = shipsIn_.find(hand->second.market);
    if (in != shipsIn_.end())
    {
        task = hand->second.hauls                                                   ? "unloading " + in->second
               : lowered(hand->second.title).find("tallying") != std::string::npos ? "tallying " + in->second + "'s cargo"
                                                                                    : "seeing to " + in->second + "'s cargo";
        reason = "A ship is in.";
    }
    else if (!hand->second.idle.empty())
    {
        task = hand->second.idle;
        reason = NoShipCause;
    }
    else
        return false;
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
    // one due today; else one sailed since yesterday; else one due within two days; else "no". Of two of a kind, the
    // latest that sailed or was kept out, the first that is due. Nothing without a port.
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
        if (r < rank || (r == rank && r < 9 && best && (s.status == "due" ? s.day < best->day : s.day > best->day)))
            rank = r, best = &s;
    }
    facts["world.ships"] = rank == 0 ? "in" : rank == 1 ? "late" : rank == 3 ? "sailed" : rank < 9 ? "due" : "no";
    if (best)
        facts["world.ship_name"] = best->name;
}

bool World::wharfCrowd(const std::string& cell) const
{
    for (const auto& p : ports_)
        if (shipsWorked_.count(p.market) && p.cells.count(cell))
            return true;
    return false;
}

std::string World::shipsNotice(const std::string& market) const
{
    // The timetable in words, with the calendar's weekday names (8.3): the ship in and when it sails, one sailed today or
    // yesterday, those due in the next seven days, those a storm kept out ("No ship this week." where none came or is due
    // in this week), and with nothing due in seven days, the next after. "" with nothing to tell. Always inside a
    // notice's 280 characters, and never with half a ship's cargo: where the whole doesn't fit, the later ships lose
    // what they take, then what they bring, then are counted ("and 2 more by Marketday"); then the ship in and the
    // ships sailed lose their cargo; every ship due, in and stormbound is told of in each.
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
    const auto cargo = [](const std::vector<std::pair<std::string, int>>& goods) {
        std::vector<std::string> out;
        for (const auto& g : goods)
            out.push_back(cargoWords(g.first));
        return listed(out);
    };
    const auto weekday = [](std::int64_t day) { return calendar::weekdayName(calendar::weekdayOf(double(day))); };
    const auto when = [&](const market::Ship& s) { return s.day == today ? std::string(" today") : " on " + weekday(s.day); };
    std::vector<const market::Ship*> due, storms;
    const market::Ship* next = nullptr;
    for (const auto* s : ships)
    {
        if (s->status == "storm")
            storms.push_back(s);
        else if (s->status == "due" && s->day < today + Week)
            due.push_back(s);
        else if (s->status == "due" && !next)
            next = s;
    }
    const std::string head = "Ships at " + marketWords(market) + ".";
    // level 0: everything; 1: the ship in and those sailed without their cargo; 2: and the stormbound counted.
    // mode 0: the first k due with what they bring and take, the rest with what they bring; 1: the first k with what
    // they bring, the rest by name and day; 2: the first k by name and day, the rest counted.
    const auto compose = [&](int level, int mode, std::size_t k) {
        std::string text = head;
        for (const auto* s : ships)
        {
            const bool goods = level == 0;
            if (s->status == "in")
                text += " " + capital(s->name) + " is in" + (goods && !s->brings.empty() ? " with " + cargo(s->brings) : std::string()) +
                        ", and sails " + partOfDay(s->sails) + (goods && !s->takes.empty() ? " with " + cargo(s->takes) : std::string()) + ".";
            else if (s->status == "sailed" && today - s->day <= SailedKeptDays && today >= s->day)
                text += " " + capital(s->name) + " sailed " + (s->day == today ? "today" : "yesterday") +
                        (goods && !s->takes.empty() ? " with " + cargo(s->takes) : std::string()) + ".";
        }
        for (std::size_t i = 0; i < due.size(); ++i)
        {
            const auto& s = *due[i];
            if (mode == 2 && i >= k)
            {
                const auto more = due.size() - k;
                text += k == 0 ? " Due: " + std::to_string(more) + (more == 1 ? " ship by " : " ships by ") + weekday(due.back()->day)
                               : ", and " + std::to_string(more) + " more by " + weekday(due.back()->day);
                break;
            }
            text += (i == 0 ? " Due: " : "; ") + s.name + when(s);
            const bool brings = mode == 0 || (mode == 1 && i < k), takes = mode == 0 && i < k;
            if (brings && !s.brings.empty())
                text += " with " + cargo(s.brings);
            if (takes && !s.takes.empty())
                text += (brings && !s.brings.empty() ? ", taking " : " taking ") + cargo(s.takes);
        }
        if (!due.empty())
            text += ".";
        if (!storms.empty())
        {
            if (level < 2)
            {
                std::vector<std::string> names;
                for (const auto* s : storms)
                    names.push_back(s->name);
                text += " " + capital(listed(names)) + (names.size() == 1 ? " is" : " are") + " stormbound and will not come.";
            }
            else
                text += " " + std::to_string(storms.size()) + (storms.size() == 1 ? " ship is" : " ships are") + " stormbound and will not come.";
            // ("No ship this week": only of this week, and only where no ship of it came in, is in or is still due.)
            const bool thisWeek = std::any_of(storms.begin(), storms.end(), [&](const market::Ship* s) { return s->day / Week == today / Week; });
            const bool another = std::any_of(ships.begin(), ships.end(), [&](const market::Ship* o) {
                return o->status != "storm" && o->day / Week == today / Week;
            });
            if (thisWeek && !another)
                text += " No ship this week.";
        }
        if (due.empty() && next)
            text += " The next is " + next->name + ", on " + weekday(next->day) +
                    (next->day / Week == today / Week + 1 ? " of next week." : ", after next week.");
        return text;
    };
    std::string text;
    for (int level = 0; level < 3; ++level)
        for (int mode = 0; mode < 3; ++mode)
            for (std::size_t k = due.size() + 1; k-- > 0;)
            {
                // (Each mode's k at its most is the mode before's at 0: said once.)
                if ((mode > 0 && k == due.size()) || (due.empty() && mode > 0))
                    continue;
                text = compose(level, mode, k);
                if (text.size() <= NoticeMost)
                    return text == head ? std::string() : text;
            }
    return json::utf8Prefix(text, NoticeMost);
}

void World::postShips()
{
    // The notice (8.3): each port's timetable, posted whenever its words change and afresh each morning (a notice stays up
    // seven days); taken down where there is nothing to tell, the rule stops the ships, or the port is gone.
    const auto today = std::int64_t(std::floor(calendarDays_));
    // (Nothing moved since every port's was posted today: the words are the same, and needn't be made to see so.)
    if (!shipsChanged_ && shipsPosted_.size() == ports_.size() &&
        std::all_of(ports_.begin(), ports_.end(), [&](const Port& p) {
            const auto last = shipsPosted_.find(p.market);
            return last != shipsPosted_.end() && last->second.first == today;
        }))
        return;
    shipsChanged_ = false;
    auto& b = society_.stagedBoard();
    const bool shipsRun = society_.rule("shipsPerWeek") > 0;
    for (const auto& p : ports_)
    {
        auto words = shipsNotice(p.market);
        const auto last = shipsPosted_.find(p.market);
        if (last != shipsPosted_.end() && last->second.first == today && last->second.second == words)
            continue;
        const bool rolled = b.shipsThrough.count(p.market) > 0;
        if (words.empty() && last == shipsPosted_.end() && !rolled)
        {
            shipsPosted_[p.market] = {today, words};   // (Never any ships here: nothing to take down either.)
            continue;
        }
        recordEvent({words.empty() ? "ships cleared" : "ships posted", p.market, p.market, p.cell, 0, 0, {}, 0, 0, words});
        // (The rule at 0 and the last ship's word gone: its notice is down, and the port is as one that never had ships.)
        if (words.empty() && !shipsRun)
            b.shipsThrough.erase(p.market);
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
    // its week's roll and its notice; nothing else of the save is touched. A ship said to be in or sailed on a day still
    // to come (a save edited, or made past a calendar since set back) is due again. The shelves are left as saved: the
    // tills a ship filled or took from are journalled with the Board (Society::takeTillsMoved), so a crash brings back
    // both or neither, and a restart moves nothing. What happened while the world was down (a ship's hour passed) is
    // caught up at the first pass of the roads (tendPort).
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
    // The timetable held to what the game could have rolled (market::mendShips: a ship's name, its cargo, its hours, a
    // port's bound, the next id), and to its calendar: no ship of a day past the weeks ever rolled (this week and the
    // next), whatever it says of itself, and no week rolled further than that, so a save from a calendar since set back
    // gets its timetable from today. A storm on a day still to come, inside those weeks, stays: one called ahead
    // (stormShip) reads the same, its price already moved.
    const auto today = std::int64_t(std::floor(calendarDays_));
    const auto week = today / Week;
    market::mendShips(b);
    b.ships.erase(std::remove_if(b.ships.begin(), b.ships.end(), [&](const market::Ship& s) { return s.day > (week + 2) * Week - 1; }), b.ships.end());
    for (auto& [market, through] : b.shipsThrough)
        through = std::min(through, week + 1);
    shipsIn_.clear();
    shipsPosted_.clear();
    shipsStopped_.clear();
    shipsChanged_ = true;
    for (auto& s : b.ships)
    {
        if ((s.status == "in" || s.status == "sailed") && s.day > today)
            s.status = "due";
        if (s.status != "in" || shipsIn_.count(s.port))
            continue;
        shipsIn_[s.port] = s.name;
    }
    shipsAtWork();
}
} // namespace ratw
