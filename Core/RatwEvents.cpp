// Staged events and causes (Docs/Design/79-a-staged-economy.md, 5.3, 8.4-8.6 and T13, T16, D25; Phase 7). World members,
// beside the roads' show (RatwTradeShow.cpp) and the port (RatwPort.cpp), whose machinery they reuse.
//
// On a small world player trade rarely moves anything, so each market has its own visible happenings: a seeded scheduler
// rolls 1 to 3 events a market a week (the rule eventRate, percent) from market.json's `events` list, each one the market
// can show (its shops sell or buy the goods; "city": the realm's largest market; "port": a ship on its timetable, which
// it comes in with or loses; "cart": a staffed farm's cart to it, which it holds; "festival": the days before one).
// Events are committed on the Market Board a week ahead (as the ships are), so they are saved, journalled and restored
// with it and a restart shows the same ones; an event's words are filled as it is rolled, so a host's later edit of the
// list changes none already committed. Each has a visible cause: its notice (pinned ahead where its kind says, else as it
// begins), a crier's line at the square, a crowd where it says, and talk's world.event. While on, its goods are pressed
// halfway toward their targets as it begins and each morning (inside the clamp: the Board's own setPressure; cause
// "event"), and its Times points count in its market's score (5.3). Events never touch money, bands or posts.
//
// The slider's cause (8.5): a move of the DM's slider comes with words (the DM's, else the seeded list's for its way:
// Society::setProsperity); each market is told once (a notice on its board, a crier's line), and the cause is
// world.times_cause and the Mind's Means while it is the largest part of the Times. The DM's Times nudges and harvest
// overrides post their own notices, unless the rule overrideNotices is 0.
#include "RatwCalendar.h"
#include "RatwItems.h"
#include "RatwJsonDoc.h"
#include "RatwMarket.h"
#include "RatwSchedules.h"
#include "RatwSeeded.h"
#include "RatwWorld.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>

namespace ratw
{
namespace
{
using seeded::listed;
using seeded::lot;
using seeded::unit;
constexpr std::int64_t Week = calendar::DaysPerWeek;
constexpr double OverKeptDays = 2, CancelledKeptDays = 1;   // (An event over is talked of two days; one cancelled, gone.)
constexpr std::size_t GoodsMost = market::EventGoodsMost;   // An event's goods, at most.

std::string capital(std::string s)
{
    if (!s.empty())
        s[0] = char(std::toupper(static_cast<unsigned char>(s[0])));
    return s;
}
// "{market}" and "{Market}" (capitalised) and the rest, filled.
std::string fill(const std::string& text, const std::map<std::string, std::string>& with)
{
    return market::fillWords(text, with);
}
double startsAt(const market::StagedEvent& e)
{
    return double(e.day) + e.hour / 24;
}
double endsAt(const market::StagedEvent& e)
{
    return startsAt(e) + double(e.days);
}
// (Over two days past its end, whatever it says of itself: one due or on then came and went while the world was down, or
// its calendar was moved on, and it makes no room for the weeks to come; a cancelled one a day past its start.)
bool stale(const market::StagedEvent& e, double now)
{
    return (e.status != "cancelled" && now > endsAt(e) + OverKeptDays) || (e.status == "cancelled" && now > startsAt(e) + CancelledKeptDays) ||
           (e.status == "cancelled" && now > double(e.posted) + 7);
}
std::string subjectId(const std::string& subject, const char* kind)
{
    const std::string head = std::string(kind) + ":";
    return subject.rfind(head, 0) == 0 ? subject.substr(head.size()) : std::string();
}
} // namespace

std::string World::cityMarket() const
{
    // The realm's largest market by its catchment (its households, its farm and works crews, passing trade): the city's,
    // where a House's wedding is.
    const market::TradePoint* best = nullptr;
    for (const auto& m : society_.tradePoints().markets)
        if (roadMarket(m.id) && (!best || m.catchment > best->catchment + 1e-9))
            best = &m;
    return best ? best->id : std::string();
}

const market::StagedEvent* World::stagedEvent(const std::string& id) const
{
    for (const auto& e : society_.board().events)
        if (e.id == id)
            return &e;
    return nullptr;
}

int World::eventsInWeek(const std::string& market, std::int64_t week) const
{
    // 1 to 3 at the default (8.4: the list's perWeek 2, one more or fewer by a seeded score), times the rule eventRate
    // (percent, 0 to 300: the DM's 0-3x); none while it is 0. Never more than one a day.
    const int rate = society_.rule("eventRate");
    const double mid = society_.market().eventsPerWeek * double(std::max(0, rate)) / 100.;
    if (mid <= 0 || !roadMarket(market))
        return 0;
    int n = int(std::floor(mid)) + (unit("events:" + market, week) < mid - std::floor(mid) ? 1 : 0);
    const double score = unit("eventsLean:" + market, week) - .5;
    n += score < -.25 ? -1 : score > .25 ? 1 : 0;
    return std::clamp(n, mid >= 1 ? 1 : 0, int(Week) - 1);
}

bool World::makeEvent(const std::string& marketId, const market::Data::EventKind& kind, std::int64_t day, const std::string& seed,
                      market::StagedEvent& e, std::string& problem) const
{
    // One event of `kind` at a market on a day, its words filled, its goods those the market's shops deal in (the kind's
    // own first, then its ship's or its cart's), its subject (a ship, a farm) found as its kind needs; false and why where
    // the market can't show it: its needs (the realm's largest market, a port, a staffed farm's cart, a festival the next
    // day) and its seasons, for the scheduler and the Dungeon Master alike.
    const auto* tp = society_.tradePoints().find(marketId);
    const auto* m = roadMarket(marketId);
    if (!tp || !m)
        return problem = "no such market", false;
    if (kind.needs == "city" && cityMarket() != marketId)
        return problem = "only at the realm's largest market", false;
    if (kind.needs == "festival")
    {
        bool called = calendar::festivalDay(double(day + 1) + .5);
        for (const auto& f : festivals_)
            called |= f.day == day + 1 && f.community == marketId;
        if (!called)
            return problem = "no festival there the next day", false;
    }
    if (kind.needs == "port" && !port(marketId))
        return problem = "no port", false;
    const auto& b = society_.board();
    const auto deals = [&](const std::string& g) { return tp->sold.count(g) > 0 || tp->bought.count(g) > 0; };
    e = {};
    e.market = marketId;
    e.kind = kind.id;
    e.day = day;
    e.hour = kind.hour + double(lot("eventhour:" + seed, day) % 3) * .5;
    e.days = kind.days;
    e.times = kind.times;
    e.ahead = kind.ahead;
    e.crowd = kind.crowd;
    std::map<std::string, std::string> words{{"market", marketWords(marketId)},
                                             {"day", calendar::weekdayName(calendar::weekdayOf(double(day) + .5))}};
    const auto add = [&](const std::string& g, double target) {
        if (deals(g) && e.goods.size() < GoodsMost && std::abs(target) >= .01 &&
            std::none_of(e.goods.begin(), e.goods.end(), [&](const auto& have) { return have.first == g; }))
            e.goods.push_back({g, std::clamp(target, -market::EventTargetMost, market::EventTargetMost)});
    };
    const market::Ship* ship = nullptr;
    const std::set<std::string>* yields = nullptr;
    if (!kind.ship.empty())
    {
        // (The first of the port's ships still due on or after the day, that no other event has already.)
        if (!port(marketId))
            return problem = "no port", false;
        for (const auto& s : b.ships)
            if (s.port == marketId && s.status == "due" && s.day >= day && (!ship || s.day < ship->day) &&
                std::none_of(b.events.begin(), b.events.end(), [&](const market::StagedEvent& o) {
                    return o.status != "cancelled" && (o.subject == "ship:" + s.id || o.subject == "lost:" + s.id);
                }))
                ship = &s;
        if (!ship || ship->day / Week != day / Week)
            return problem = "no ship due that week", false;
        e.day = ship->day;
        e.hour = kind.ship == "lost" ? 0 : ship->hour;
        e.subject = (kind.ship == "lost" ? "lost:" : "ship:") + ship->id;
        words["ship"] = ship->name;
        words["day"] = calendar::weekdayName(calendar::weekdayOf(double(ship->day) + .5));
    }
    if (kind.needs == "cart")
    {
        // (A staffed farm whose cart comes to this market, seeded; its goods the ones its cart brings.)
        const auto& farms = society_.farms();
        std::vector<const CartEnd*> ends;
        for (const auto& end : cartEnds_)
            if (end.market == marketId && end.farm < farms.size() && farms[end.farm].heldKeepers + farms[end.farm].heldHands > 0 &&
                farms[end.farm].def && std::any_of(farms[end.farm].def->yields.begin(), farms[end.farm].def->yields.end(), deals))
                ends.push_back(&end);
        if (ends.empty())
            return problem = "no farm's cart comes there", false;
        const auto* end = ends[std::size_t(lot("eventcart:" + seed, day) % ends.size())];
        e.subject = "farm:" + end->farmId;
        e.hour = std::min(e.hour, 5.);              // (Before the carts roll: World::rollCarts.)
        words["from"] = end->name.empty() ? marketWords(end->farmId) : end->name;
        yields = &farms[end->farm].def->yields;
    }
    if (!(kind.seasons & market::seasonBit(double(e.day) + .5)))
        return problem = "not in this season", false;
    // (The kind's own goods first, so a rich cargo is of silk and spices where its market deals in them, not only the
    // ship's everyday cargo; then its ship's, then its cart's.)
    for (const auto& [g, target] : kind.goods)
        add(g, target);
    if (ship && std::abs(kind.brings) >= .01)
        for (const auto& [g, n] : ship->brings)
            add(g, kind.brings);
    if (yields && std::abs(kind.yields) >= .01)
        for (const auto& g : *yields)
            add(g, kind.yields);
    if (e.goods.empty())
        return problem = "its shops deal in none of its goods", false;
    std::vector<std::string> goods;
    for (const auto& [g, target] : e.goods)
        goods.push_back(cargoWords(g));   // ("hides", "handbells": Phase 8's wording.)
    words["goods"] = listed(goods);
    e.words = json::utf8Prefix(fill(kind.words, words), 80);
    e.notice = json::utf8Prefix(fill(kind.notice, words), 280);
    e.cry = json::utf8Prefix(fill(kind.cry, words), 200);
    if (!kind.noticeOn.empty())
        e.noticeOn = json::utf8Prefix(fill(kind.noticeOn, words), 280);
    if (e.words.empty() || !market::plainWords(e.words, 80) || !market::plainWords(e.notice, 280) || !market::plainWords(e.cry, 200) ||
        !market::plainWords(e.noticeOn, 280))
        return problem = "its words are not plain", false;
    e.posted = std::floor(calendarDays_);
    return true;
}

bool World::rollEvents(const Town& m, std::int64_t week, std::int64_t today)
{
    // A week of a market's events, committed (8.4): how many (eventsInWeek), which (market.json's kinds it can show, by
    // weight, none twice in the week nor within a week of another of its kind there), and when (a ship's day, the day
    // before a festival, else a seeded day of the week, one event a day): only days still to come (a week rolled again
    // adds what is missing on days still free). False where the Board's bound stopped it short (the week is rolled
    // again the next morning, when those over have made room).
    auto& b = society_.stagedBoard();
    const int count = eventsInWeek(m.id, week);
    if (count <= 0)
        return true;
    const auto& d = society_.market();
    std::set<std::int64_t> taken;
    std::set<std::string> used;
    for (const auto& e : b.events)
        if (e.market == m.id && e.day / Week == week && e.status != "cancelled")
            taken.insert(e.day), used.insert(e.kind);
    int have = int(std::count_if(b.events.begin(), b.events.end(), [&](const market::StagedEvent& e) {
        return e.market == m.id && e.day / Week == week && e.by == "seeded";
    }));
    // (The days the festivals of the week fall on, here.)
    std::vector<std::int64_t> festivals;
    for (std::int64_t day = week * Week; day < (week + 1) * Week; ++day)
    {
        bool called = calendar::festivalDay(double(day) + .5);
        for (const auto& f : festivals_)
            called |= f.day == day && f.community == m.id;
        if (called)
            festivals.push_back(day);
    }
    // (The days a festival's buying ahead, or a ship's event, would fall on.)
    std::set<std::int64_t> reserved;
    for (const auto f : festivals)
        reserved.insert(f - 1);
    for (const auto& s : b.ships)
        if (s.port == m.id && s.status == "due" && s.day / Week == week)
            reserved.insert(s.day);
    for (int i = 0; have < count && i < count * 3; ++i)
    {
        const auto seed = m.id + ":" + std::to_string(week) + ":" + std::to_string(i);
        // The kinds it could show this week, by weight.
        std::vector<std::pair<const market::Data::EventKind*, std::int64_t>> candidates;
        double total = 0;
        for (const auto& kind : d.events)
        {
            if (kind.weight <= 0 || used.count(kind.id) || (kind.needs == "city" && cityMarket() != m.id))
                continue;
            std::int64_t day = -1;
            if (!kind.ship.empty())
                day = std::max(today + 1, week * Week); // (Its ship's day: the first due that week no event has, below.)
            else if (kind.needs == "festival")
            {
                for (const auto f : festivals)
                    if (f - 1 > today && !taken.count(f - 1))
                    {
                        day = f - 1;
                        break;
                    }
            }
            else
            {
                // (A seeded day of the week still to come and free, keeping clear of the days a festival's or a ship's
                // event would need where another will do; a ship's kind takes its ship's day below.)
                const auto start = std::int64_t(lot("eventday:" + seed + ":" + kind.id, week) % std::uint64_t(Week));
                for (int pass = 0; pass < 2 && day < 0; ++pass)
                    for (std::int64_t k = 0; k < Week && day < 0; ++k)
                        if (const auto at = week * Week + (start + k) % Week; at > today && !taken.count(at) && (pass == 1 || !reserved.count(at)))
                            day = at;
            }
            if (day < 0)
                continue;
            market::StagedEvent trial;
            std::string why;
            if (day / Week != week || !makeEvent(m.id, kind, day, seed, trial, why) || trial.day <= today ||
                (trial.day != day && taken.count(trial.day)) || (!kind.ship.empty() && taken.count(trial.day)) ||
                std::any_of(b.events.begin(), b.events.end(), [&](const market::StagedEvent& o) {
                    return o.market == m.id && o.kind == kind.id && o.status != "cancelled" && std::abs(o.day - trial.day) < Week;
                }))
                continue;
            candidates.push_back({&kind, trial.day});
            total += kind.weight;
        }
        if (candidates.empty() || total <= 0)
            break;
        double pick = unit("eventkind:" + seed, week) * total;
        auto chosen = candidates.back();
        for (const auto& c : candidates)
            if ((pick -= c.first->weight) < 0)
            {
                chosen = c;
                break;
            }
        market::StagedEvent e;
        std::string why;
        if (!makeEvent(m.id, *chosen.first, chosen.second, seed, e, why))
            break;
        // (At the Board's bound the oldest over or cancelled go first, the market's own, then any; none to go, the week
        // is short and rolled again tomorrow.)
        const auto makeRoom = [&](bool own) {
            auto out = b.events.end();
            for (auto it = b.events.begin(); it != b.events.end(); ++it)
                if ((!own || it->market == m.id) && (it->status == "over" || it->status == "cancelled") && (out == b.events.end() || it->day < out->day))
                    out = it;
            if (out == b.events.end())
                return false;
            b.events.erase(out);
            return true;
        };
        const auto ofMarket = [&] {
            return std::size_t(std::count_if(b.events.begin(), b.events.end(), [&](const market::StagedEvent& o) { return o.market == m.id; }));
        };
        if ((ofMarket() >= market::EventsAMarket && !makeRoom(true)) || (b.events.size() >= market::EventsMost && !makeRoom(false)))
            return false;
        e.id = "e" + std::to_string(b.nextEvent++);
        used.insert(e.kind);
        taken.insert(e.day);
        ++have;
        recordEvent({"event rolled", e.id, m.id, m.market, 0, 0, e.kind, int(e.goods.size()), 0, e.words});
        b.events.push_back(std::move(e));
    }
    return true;
}

void World::eventsMorning(std::int64_t today)
{
    // The events' day (8.4): those long over or cancelled drop off; each market's weeks rolled through the next (so the
    // next seven days are always committed); a notice posted ahead where an event's kind says (its `ahead` days); and each
    // event on pressed again toward its goods' targets (its days: "for a few days").
    auto& b = society_.stagedBoard();
    const double now = calendarDays_;
    b.events.erase(std::remove_if(b.events.begin(), b.events.end(), [&](const market::StagedEvent& e) { return stale(e, now); }), b.events.end());
    if (society_.rule("eventRate") > 0)
    {
        const auto week = today / Week;
        for (const auto& m : roadMarkets_)
        {
            // (A week is marked rolled only when it was rolled whole: one the Board's bound cut short is rolled again.)
            const auto through = b.eventsThrough.find(m.id);
            for (auto w = std::max(week, through == b.eventsThrough.end() ? week : through->second + 1); w <= week + 1; ++w)
            {
                if (!rollEvents(m, w, today))
                    break;
                b.eventsThrough[m.id] = w;
            }
        }
    }
    for (auto& e : b.events)
    {
        if (e.status == "on")
            pressEvent(e);
        if (e.status == "due" && !e.announced && e.ahead > 0 && !e.notice.empty() && double(today) >= double(e.day - e.ahead))
        {
            e.announced = true;
            const auto* m = roadMarket(e.market);
            recordEvent({"event posted", e.id, e.market, m ? m->market : std::string(), 0, 0, e.kind, 0, 0, e.notice});
        }
    }
}

void World::pressEvent(const market::StagedEvent& e)
{
    // Halfway toward each good's target, never past it and never the other way (8.4: "pushes P toward a target ... inside
    // the clamp, with its own cause word"; the Board's setPressure holds P inside its range, and quote the factor inside
    // the clamp, whatever P is).
    for (const auto& [g, target] : e.goods)
    {
        const double P = society_.pressure(e.market, g);
        if (target > 0 ? P < target - 1e-4 : P > target + 1e-4)
            society_.setPressure(e.market, g, P + (target - P) / 2, "event");
    }
}

void World::eventBegins(market::StagedEvent& e)
{
    // It begins (8.6's row): its goods pressed, its shelves as it says (a good catch's fish set out: the kind's `refill`),
    // its notice up if it was not posted ahead, its crier's line, its crowd.
    e.status = "on";
    if (!e.noticeOn.empty())
    {
        e.notice = std::move(e.noticeOn);           // (A rich cargo's "is due on Riverday" becomes "is in".)
        e.noticeOn.clear();
    }
    pressEvent(e);
    if (const auto* kind = society_.market().eventKind(e.kind); kind && kind->refill)
        for (const auto& [g, target] : e.goods)
            if (target < 0)
                society_.arrivalRefill(e.market, g);
    const auto* m = roadMarket(e.market);
    const bool fresh = !e.announced;
    e.announced = e.announced || !e.notice.empty();
    recordEvent({"event begins", e.id, e.market, m ? m->market : std::string(), 0, 0, e.kind, fresh ? 1 : 0, 0,
                 e.notice.empty() ? e.words : e.notice});
}

void World::eventEnds(market::StagedEvent& e, const char* why)
{
    const bool cancelled = std::string(why) != "over";
    e.status = cancelled ? "cancelled" : "over";
    const auto* m = roadMarket(e.market);
    recordEvent({cancelled ? "event cancelled" : "event over", e.id, e.market, m ? m->market : std::string(), 0, 0, e.kind, 0, 0, why});
}

void World::tendEvents()
{
    // Each pass of the roads, all by the saved list, so a restart at any hour catches up: an event whose market is gone is
    // cancelled; one whose days all passed unseen (the world was down) is over, quietly; one whose hour has come begins
    // (one of a ship's: when its ship docks, or as it is lost; cancelled if its ship never came); one past its days is over.
    tendCauses();
    auto& b = society_.stagedBoard();
    if (b.events.empty())
        return;
    const double now = calendarDays_;
    const bool seededOff = society_.rule("eventRate") <= 0;
    // (Over without its days pressed, its notice down if it was up: one whose days passed while the world was down, or in
    // a jump of the calendar.)
    const auto overUnseen = [&](market::StagedEvent& e) {
        if (e.announced)
            eventEnds(e, "over");
        else
            e.status = "over";
    };
    for (auto& e : b.events)
    {
        if (e.status != "due" && e.status != "on")
            continue;
        if (seededOff && e.status == "due" && e.by == "seeded")
        {
            eventEnds(e, "no events now");      // (The rule at 0: the seeded ones still to come are called off; the DM's stay.)
            continue;
        }
        if (!roadMarket(e.market))
        {
            eventEnds(e, "its market is gone");
            continue;
        }
        if (e.status == "due")
        {
            const auto docks = subjectId(e.subject, "ship"), lost = subjectId(e.subject, "lost");
            if (!docks.empty() || !lost.empty())
            {
                const market::Ship* ship = nullptr;
                for (const auto& s : b.ships)
                    if (s.id == (docks.empty() ? lost : docks))
                        ship = &s;
                const bool in = ship && (ship->status == "in" || ship->status == "sailed");
                const bool gone = ship && ship->status == "storm";
                if (!ship || (!docks.empty() && gone) || (!lost.empty() && in) || (!lost.empty() && gone && !ship->lost))
                {
                    eventEnds(e, ship ? "its ship never came" : "its ship is gone");
                    continue;
                }
                if ((!docks.empty() && in) || (!lost.empty() && gone && ship->lost))
                {
                    if (now >= endsAt(e))
                        overUnseen(e);             // (Came and went while the world was down: nothing pressed now.)
                    else
                        eventBegins(e);
                }
                continue;
            }
            if (now >= endsAt(e))
                overUnseen(e);
            else if (now >= startsAt(e))
                eventBegins(e);
            continue;
        }
        if (now >= endsAt(e))
            eventEnds(e, "over");
    }
}

void World::tendCauses()
{
    // The slider's cause told in every market (8.5: a board notice and a crier's line each), once; the DM's Times nudges
    // and harvest overrides posted where they bear (unless the rule overrideNotices is 0), once, and a nudge's notice
    // down when it lapses.
    auto& b = society_.stagedBoard();
    if (!b.sliderCause.told)
    {
        b.sliderCause.told = true;
        if (!b.sliderCause.words.empty())
            for (const auto& m : roadMarkets_)
                recordEvent({"times cause", m.id, m.id, m.market, 0, 0, b.sliderCause.dir < 0 ? "down" : "up", int(b.sliderCause.n), 0,
                             b.sliderCause.notice.empty() ? capital(b.sliderCause.words) + "." : b.sliderCause.notice});
    }
    const bool notices = society_.rule("overrideNotices") > 0;
    for (auto it = b.nudges.begin(); it != b.nudges.end();)
    {
        auto& [market, nudge] = *it;
        const auto* m = roadMarket(market);
        if (calendarDays_ >= nudge.until || !m)
        {
            recordEvent({"nudge over", market, market, m ? m->market : std::string(), 0, 0, {}, 0, 0, {}});
            it = b.nudges.erase(it);
            continue;
        }
        if (!nudge.told)
        {
            nudge.told = true;
            if (notices)
            {
                const auto words = !nudge.cause.empty() ? capital(nudge.cause) + "."
                                   : nudge.points > 0 ? "Trade is brisk in " + marketWords(market) + "."
                                                      : "Trade is slack in " + marketWords(market) + ".";
                recordEvent({"times nudged", market, market, m->market, 0, 0, {}, nudge.points, 0, words});
            }
        }
        ++it;
    }
    for (auto& [area, o] : b.harvestOverride)
    {
        if (o.told)
            continue;
        o.told = true;
        if (!notices || calendarDays_ >= o.until)
            continue;
        // (In the words of what its farms grow, at most three: an area's id is no place's name, and on some maps it is a
        // good's: "a good season for wheat and oats".)
        std::vector<std::string> grown;
        for (const auto& farm : society_.farms())
            if (farm.def && farm.def->area == area)
                for (const auto& g : farm.def->yields)
                    if (grown.size() < 3 && std::find(grown.begin(), grown.end(), cargoWords(g)) == grown.end())
                        grown.push_back(cargoWords(g));
        const char* const Words[] = {"poor", "fair", "good"};
        const auto words = (o.cause.empty() ? std::string() : capital(o.cause) + ": ") + std::string(o.cause.empty() ? "A " : "a ") +
                           Words[std::clamp(o.word, 0, 2)] + " season for " + (grown.empty() ? std::string("the farms") : listed(grown)) + ".";
        // (On the boards of the markets its farms' carts come to; with none staffed, every market's.)
        std::set<std::string> markets;
        for (const auto& end : cartEnds_)
            if (end.area == area)
                markets.insert(end.market);
        if (markets.empty())
            for (const auto& m : roadMarkets_)
                markets.insert(m.id);
        for (const auto& market : markets)
            if (const auto* m = roadMarket(market))
                recordEvent({"harvest called", area, market, m->market, 0, 0, {}, o.word, 0, words});
    }
}

bool World::cartHeld(const std::string& farmId, const std::string& market, std::int64_t day) const
{
    // A staged event holding a farm's cart that day (a lame ox: 8.4's "delaying the Coldwater cart").
    for (const auto& e : society_.board().events)
        if (e.market == market && e.day == day && (e.status == "due" || e.status == "on") && subjectId(e.subject, "farm") == farmId)
            if (const auto* kind = society_.market().eventKind(e.kind); !kind || kind->holdsCart)
                return true;
    return false;
}

// The Dungeon Master's hooks (Phase 9's commands and the Prosperity tab call them): addStagedEvent, cancelStagedEvent,
// nudgeTimes and overrideHarvest change the Market Board and record their step, but write no journal record of their
// own: every command that calls one journals the Board after it, before anyone is told (Game::applyEconomyCommand,
// RatwGameEconomy.cpp).
std::string World::addStagedEvent(const std::string& market, const std::string& kindId, std::int64_t day, double hour, std::string& problem)
{
    // (Its kind's needs and seasons held as the scheduler's are, makeEvent's: a House wedding only at the realm's largest
    // market, a festival's buying only the day before one, a ship's only at a port.)
    auto& b = society_.stagedBoard();
    const auto* kind = society_.market().eventKind(kindId);
    const auto today = std::int64_t(std::floor(calendarDays_));
    if (!kind)
        return problem = "No such event in the list: \"" + kindId + "\".", std::string();
    if (!roadMarket(market))
        return problem = "No such market: \"" + market + "\".", std::string();
    if (day < today || day > (today / Week + 2) * Week - 1)
        return problem = "An event is for a day of this week or the next.", std::string();   // (The weeks committed: a restore keeps no later.)
    if (b.events.size() >= market::EventsMost ||
        std::size_t(std::count_if(b.events.begin(), b.events.end(), [&](const market::StagedEvent& o) { return o.market == market; })) >=
            market::EventsAMarket)
        return problem = "That market has as many events as it can hold.", std::string();
    market::StagedEvent e;
    if (!makeEvent(market, *kind, day, "dm:" + std::to_string(b.nextEvent), e, problem))
        return problem = "It can't happen there: " + problem + ".", std::string();
    if (kind->ship.empty())
    {
        e.day = day;
        if (std::isfinite(hour))
            e.hour = std::clamp(hour, 0., 23.99);
    }
    e.id = "e" + std::to_string(b.nextEvent++);
    e.by = "dm";
    const bool post = e.ahead > 0 && !e.notice.empty() && double(today) >= double(e.day - e.ahead);
    e.announced = post;
    b.events.push_back(std::move(e));
    const auto& added = b.events.back();
    recordEvent({"event added", added.id, market, {}, 0, 0, added.kind, int(added.goods.size()), 0, added.words});
    if (post)                                       // (On the Board first: the game's notice reads it.)
    {
        const auto* m = roadMarket(market);
        recordEvent({"event posted", added.id, market, m ? m->market : std::string(), 0, 0, added.kind, 0, 0, added.notice});
    }
    problem.clear();
    return added.id;
}

bool World::cancelStagedEvent(const std::string& id)
{
    for (auto& e : society_.stagedBoard().events)
        if (e.id == id && (e.status == "due" || e.status == "on"))
        {
            eventEnds(e, "called off");
            return true;
        }
    return false;
}

bool World::nudgeTimes(const std::string& market, int points, double days, const std::string& cause)
{
    auto& b = society_.stagedBoard();
    auto words = json::utf8Prefix(cause, 80);
    if (!roadMarket(market) || !std::isfinite(days) || !market::plainWords(words, 80))
        return false;
    points = std::clamp(points, -100, 100);
    if (points == 0 || days <= 0)
    {
        if (const auto* m = roadMarket(market); b.nudges.erase(market))
            recordEvent({"nudge over", market, market, m->market, 0, 0, {}, 0, 0, {}});
        return true;
    }
    b.nudges[market] = {points, calendarDays_ + std::min(days, 366.), words, false};
    return true;
}

bool World::overrideHarvest(const std::string& area, int word, double days, const std::string& cause)
{
    auto words = json::utf8Prefix(cause, 80);
    if (area.empty() || !std::isfinite(days) || days <= 0 || !market::plainWords(words, 80))
        return false;
    bool known = false;
    for (const auto& f : society_.farms())
        known |= f.def && f.def->area == area;
    for (const auto& end : cartEnds_)
        known |= end.area == area;
    if (!known)
        return false;
    society_.stagedBoard().harvestOverride[area] = {std::clamp(word, 0, 2), calendarDays_ + std::min(days, 366.), words, false};
    return true;
}

void World::eventFacts(const std::string& market, std::map<std::string, std::string>& facts) const
{
    // Talk's world.times and world.times_cause (5.3, 8.5, T16), and its world.event (8.4, T13): the event on (the latest
    // begun), else one posted and due within two days, else one over these two days; its words, and when.
    // (A cause only where the times are other than ordinary: nobody says why things are as they always are.)
    const int times = society_.marketTimes(market);
    facts["world.times"] = market::timesName(times + 2);
    if (auto cause = times != 0 ? society_.timesCause(market) : std::string(); !cause.empty())
        facts["world.times_cause"] = std::move(cause);
    const double now = calendarDays_;
    const market::StagedEvent* best = nullptr;
    int rank = 9;
    for (const auto& e : society_.board().events)
    {
        if (e.market != market)
            continue;
        const int r = e.status == "on" ? 0
                      : e.status == "due" && e.announced && startsAt(e) - now <= 2 ? 1
                      : e.status == "over" && now - endsAt(e) <= OverKeptDays ? 2 : 9;
        if (r < rank || (r == rank && r < 9 && best && (r == 1 ? e.day < best->day : e.day > best->day)))
            rank = r, best = &e;
    }
    if (!best || rank == 9)
        return;
    facts["world.event"] = best->kind;
    facts["world.event_words"] = best->words;
    // (Its goods as a line says them, "brandy, wine and chamomile": a rich cargo is of what its market deals in, never
    // silk where none is sold.)
    std::vector<std::string> goods;
    for (const auto& [g, target] : best->goods)
        goods.push_back(cargoWords(g));
    if (!goods.empty())
        facts["world.event_goods"] = listed(goods);
    facts["world.event_when"] = rank == 0 ? "now" : rank == 1 ? "soon" : "past";
}

bool World::eventCrowd(const std::string& cell) const
{
    for (const auto& e : society_.board().events)
        if (e.status == "on" && e.crowd)
        {
            const auto* p = (!subjectId(e.subject, "ship").empty() || !subjectId(e.subject, "lost").empty()) ? port(e.market) : nullptr;
            const auto* m = roadMarket(e.market);
            if ((p && p->cell == cell) || (!p && m && m->market == cell))
                return true;
        }
    return false;
}

void World::dropOrphanedEvents()
{
    // After a load: the events of a market that is gone are dropped (their notices come down with the boards' mending,
    // Game::mendRoadNotices); the list held to what the game could have rolled (market::mendEvents), and to this calendar:
    // no event of a day past the weeks ever rolled (this week and the next), no week rolled further than that.
    ensureRoadMarkets();
    auto& b = society_.stagedBoard();
    const auto week = std::int64_t(std::floor(calendarDays_)) / Week;
    b.events.erase(std::remove_if(b.events.begin(), b.events.end(), [&](const market::StagedEvent& e) {
                       return !roadMarket(e.market) || e.day > (week + 2) * Week - 1;
                   }),
                   b.events.end());
    market::mendEvents(b);
    for (auto it = b.eventsThrough.begin(); it != b.eventsThrough.end();)
        if (!roadMarket(it->first))
            it = b.eventsThrough.erase(it);
        else
        {
            it->second = std::min(it->second, week + 1);
            ++it;
        }
    for (auto it = b.nudges.begin(); it != b.nudges.end();)
        it = !roadMarket(it->first) ? b.nudges.erase(it) : std::next(it);
    // (A nudge's or a harvest override's expiry held to this calendar, as the levers set it: a year at most from now, so a
    // damaged save's "until" of 10^12 pins no market's Times for ever; and no ship lost on a day still to come.)
    for (auto& [m, nudge] : b.nudges)
        nudge.until = std::min(nudge.until, calendarDays_ + 366);
    for (auto& [area, o] : b.harvestOverride)
        o.until = std::min(o.until, calendarDays_ + 366);
    for (auto& [ship, day] : b.lostShips)
        day = std::min(day, calendarDays_);
}
} // namespace ratw
