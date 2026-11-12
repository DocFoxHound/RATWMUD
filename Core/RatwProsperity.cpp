// The staged economy's DM side (Docs/Design/79-a-staged-economy.md, 12; Phase 9): the per-world market layer and the
// host's overrides, grants and fines on players, "reset prices", the DM's harvest override lifted, and the Prosperity
// report the DM's tab reads (written with the checkpoint: SocietyState::report).
//
// Nothing here is a money loop: a grant is a registered faucet (the door's group "grant", capped a holder a day by
// status.json's caps.grantsADay, a host's at most 2500), a fine a sink of no more than the player holds; a price reset
// only eases pressure toward 0 (inside the clamp, as the decay does); the overrides are checked as the loaders take them.
#include "RatwCalendar.h"
#include "RatwSociety.h"
#include "RatwWorld.h"

#include <algorithm>
#include <cmath>

namespace ratw
{
namespace
{
constexpr std::size_t RetiredKept = 8;               // Market layers replaced and kept alive (a reader may hold one).
constexpr std::size_t PressuresShown = 30;
constexpr double HungryAt = 60;
constexpr const char* HarvestWords[] = {"poor", "fair", "good"};
}

// ---- The overrides (12; 1.4 of the plan) ----

const market::Data& Society::market() const { return marketLayer_ ? *marketLayer_ : market::data(); }

void Society::adoptOverrides(const EconomySpec::Overrides& o, std::vector<std::string>* warnings)
{
    // status.json's: only a host's keys, the share table whole or not at all, the caps held to their ceilings; read
    // over the shipped table (status::read: lenient, every value clamped). None: the shipped table.
    if (o.status.isObject() && o.status.size() > 0)
    {
        const auto checked = status::hostOverride(o.status, warnings);
        setStatusOverride(&checked, warnings);
    }
    else
        setStatusOverride(nullptr);
    // market.json's events, causes and ships' names, and the goods no ship brings: a new layer, swapped in whole; the old
    // one kept alive a while (a reader on another thread may hold it through its pass).
    std::shared_ptr<const market::Data> next;
    if ((o.market.isObject() && o.market.size() > 0) || o.neverSet)
        next = std::make_shared<const market::Data>(market::layered(market::data(), o.market, o.neverSet ? &o.neverImported : nullptr, warnings));
    if (marketLayer_)
    {
        marketRetired_.push_back(marketLayer_);
        if (marketRetired_.size() > RetiredKept)
            marketRetired_.erase(marketRetired_.begin());
    }
    marketLayer_ = std::move(next);
}

// ---- Grants and fines (12: "grant or fine a player"; D16) ----

bool Society::grantPlayer(const std::string& player, std::int64_t coins, const std::string& reason, std::string& problem,
                          std::int64_t* moved)
{
    if (moved)
        *moved = 0;
    const auto words = json::utf8Prefix(reason, 80);
    if (!playerAccountId(player))
        return problem = "Only a player's account can be granted or fined.", false;
    if (coins == 0)
        return problem = "Nothing to grant or fine.", false;
    if (words.empty() || words.size() != reason.size() || !market::plainWords(words, 80))
        return problem = "A reason is needed: plain words, 80 at most.", false;
    const auto* a = account(player);
    if (!a)
        return problem = "That player has no purse yet.", false;
    const std::string bank = account("treasury") ? "treasury" : "made";
    if (!account(bank))
        return problem = "This world has no treasury to pay from.", false;
    if (coins > 0)
    {
        const auto room = std::max<std::int64_t>(0, statusTable().cap("grantsADay") - faucetToday(holderOf(player), "grant"));
        if (coins > room)
            return problem = "Over the day's grants for that player (" + std::to_string(room) + "p left today).", false;
        if (!shift(bank, player, "", 0, coins, "dm grant: " + words))
            return problem = "The grant could not be paid (the purse is full).", false;
        if (moved)
            *moved = coins;
        return true;
    }
    const auto fine = std::min(-coins, a->cash);
    if (fine <= 0)
        return problem = "That player has nothing to fine.", false;
    if (!shift(player, bank, "", 0, fine, "dm fine: " + words))
        return problem = "The fine could not be taken.", false;
    if (moved)
        *moved = fine;
    return true;
}

// ---- Reset prices (12; 7.3) ----

bool Society::resetPrices(const std::string& market, double now, const std::string& words)
{
    if (!std::isfinite(now) || now < 0)
        return false;
    auto said = json::utf8Prefix(words, 80);
    if (said.empty() || !market::plainWords(said, 80))
        said = "fresh stock everywhere";
    std::vector<std::string> markets;
    if (market.empty())
        for (const auto& m : tradePoints().markets)
            markets.push_back(m.id);
    else if (tradePoints().find(market))
        markets.push_back(market);
    if (markets.empty())
        return false;
    for (const auto& m : markets)
        state_.board.resets[m] = {now, now + 1, now, said};
    return true;
}

bool Society::tendResets()
{
    auto& b = state_.board;
    if (b.resets.empty())
        return false;
    const double now = marketClock();
    for (auto it = b.resets.begin(); it != b.resets.end();)
    {
        auto& r = it->second;
        const auto cells = b.cell.find(it->first);
        if (now >= r.until)
        {
            if (cells != b.cell.end())
                b.cell.erase(cells);                // (Every pressure there at 0: normal prices.)
            it = b.resets.erase(it);
            continue;
        }
        // (Linear to 0 at `until`: each pass scales what is there by what is left of the reset since the last pass, so the
        // passes compose to the straight line, and a cell a trade pushed since eases from its touch to 0 at the same end.
        // The decay goes on as ever: scaling P scales its decayed value alike.)
        if (now > r.last && r.until > r.last && cells != b.cell.end())
            for (auto& [good, cell] : cells->second)
                cell.P *= std::clamp((r.until - now) / (r.until - r.last), 0., 1.);
        r.last = std::max(r.last, now);
        ++it;
    }
    return true;
}

void Society::bandsChanged()
{
    if (stagedDay_ >= 0)
        stagedBands(stagedDay_);
}

// ---- The report (12: the Prosperity tab) ----

std::map<std::string, Society::MarketCount> Society::marketCounts() const
{
    std::map<std::string, MarketCount> out;
    const auto day = state_.budgetDay;
    for (const auto& [key, home] : stagedHomes_)
    {
        auto& c = out[home.market];
        c.bands[std::size_t(std::clamp(home.band, 0, status::Bands - 1))]++;
        bool overridden = false, hungry = false;
        for (const auto& m : home.members)
        {
            if (const auto* s = statusOf(m); s && s->overridden)
                overridden = true;
            if (const auto r = state_.residents.find(m); r != state_.residents.end() && r->second.hunger >= HungryAt)
                hungry = true;
        }
        c.overridden += overridden;
        c.hungry += hungry;
        c.shortToday += foodShort(key) > 0;
        if (const auto d = state_.staged.dole.find(key); d != state_.staged.dole.end() && d->second > day - calendar::DaysPerWeek)
            ++c.dole;
    }
    return out;
}

bool World::clearHarvest(const std::string& area)
{
    auto& overrides = society_.stagedBoard().harvestOverride;
    const auto found = overrides.find(area);
    if (found == overrides.end())
        return false;
    overrides.erase(found);
    recordEvent({"harvest over", area, {}, {}, 0, 0, {}, 0, 0, {}});
    return true;
}

void World::noteBeggars()
{
    // After each hour's placing (tendStreet): in the begging hours of a begging day (10.2: Marketday and Restday, 8:00 to
    // 18:00) each market's count is raised to the most seen that day, a new day starting from none (a boom's empty
    // Marketday then reads 0, not last Restday's). Outside those hours nothing changes: the last begging day's stands.
    // Everyone placed is counted: the postless in want and, on Restday, the working poor on their day off (decided
    // 2026-11-12), each under its own household's market, as placeBeggars draws them and holds them to that market's
    // most (beggarsInAMarket), so a market's count here is never over it; one with no market on record under the market
    // of the cell it stands in.
    const int weekday = calendar::weekdayOf(calendarDays_);
    const double hour = (calendarDays_ - std::floor(calendarDays_)) * 24;
    if ((weekday != calendar::Marketday && weekday != calendar::Restday) || hour < 8 || hour >= 18 || !society_.stagedReady())
        return;
    if (const auto today = std::int64_t(std::floor(calendarDays_)); today != beggarsSeenDay_)
    {
        beggarsSeen_.clear();
        beggarsSeenDay_ = today;
    }
    std::map<std::string, int> now;
    for (const auto& [id, spot] : beggars_)
    {
        const auto* s = society_.statusOf(id);
        if (s && !s->market.empty())
            ++now[s->market];
        else if (const auto* m = society_.tradePoints().ofCellId(spot.cell))
            ++now[m->id];
    }
    for (const auto& [market, n] : now)
        beggarsSeen_[market] = std::max(beggarsSeen_[market], n);
}

json::Value World::prosperityReport()
{
    using json::Value;
    ensureTradePoints();
    const auto& b = society_.board();
    auto report = Value::object();
    report.add("v", 1);
    report.add("day", double(society_.state().budgetDay));
    report.add("at", calendarDays_);
    {
        auto p = Value::object();
        p.add("slider", b.slider);
        p.add("target", b.sliderTarget);
        p.add("cause", b.sliderCause.words);
        p.add("by", b.sliderCause.words.empty() ? "" : b.sliderCause.dm ? "dm" : "seeded");
        p.add("neverHungry", society_.rule("neverHungry") > 0);
        p.add("neverBroke", society_.rule("neverBroke") > 0);
        report.add("prosperity", std::move(p));
    }
    const auto counts = society_.marketCounts();
    const auto& thefts = theftPlan();
    // Beggars today (Phase 9's last repair): the most out at once on the last begging day (noteBeggars), never the count
    // at this moment, which at midnight is always none. A server that has seen no begging day since it started keeps the
    // last report's count and day.
    std::map<std::string, int> beggarsAt = beggarsSeen_;
    double beggarsDay = double(beggarsSeenDay_);
    if (beggarsSeenDay_ < 0 && society_.report().isObject())
    {
        const auto& last = society_.report();
        if (const auto* day = last.find("beggarsDay"); day && day->isNumber())
            beggarsDay = day->asNumber(-1);
        if (const auto* list = last.find("markets"); list && list->isArray() && beggarsDay >= 0)
            for (const auto& m : list->items())
                if (m.isObject() && m.find("beggars") && m.find("beggars")->isNumber())
                    beggarsAt[m.string("id")] = int(m.number("beggars"));
    }
    report.add("beggarsDay", beggarsDay);           // (-1: no begging day seen yet.)
    auto markets = Value::array();
    for (const auto& m : society_.tradePoints().markets)
    {
        if (m.id == "outside")
            continue;
        auto j = Value::object();
        j.add("id", m.id);
        j.add("name", marketWords(m.id));
        {
            const int score = society_.timesScore(m.id);
            auto t = Value::object();
            t.add("word", market::timesName(market::timesOf(score)));
            t.add("score", score);
            t.add("cause", society_.timesCause(m.id));
            j.add("times", std::move(t));
        }
        const auto c = counts.count(m.id) ? counts.at(m.id) : Society::MarketCount{};
        auto h = Value::object();
        for (int band = 0; band < status::Bands; ++band)
            h.add(status::bandName(band), c.bands[std::size_t(band)]);
        j.add("households", std::move(h));
        j.add("overridden", c.overridden);
        j.add("hungry", c.hungry);
        j.add("short", c.shortToday);
        j.add("thieves", thefts.expected.count(m.id) ? std::round(thefts.expected.at(m.id) * 100) / 100 : 0.);
        j.add("beggars", beggarsAt.count(m.id) ? beggarsAt.at(m.id) : 0);
        j.add("dole", c.dole);
        if (const auto n = b.nudges.find(m.id); n != b.nudges.end() && calendarDays_ < n->second.until)
        {
            auto nudge = Value::object();
            nudge.add("points", n->second.points);
            nudge.add("until", n->second.until);
            nudge.add("words", n->second.cause);
            j.add("nudge", std::move(nudge));
        }
        else
            j.add("nudge", Value());
        if (const auto r = b.resets.find(m.id); r != b.resets.end())
        {
            auto reset = Value::object();
            reset.add("until", r->second.until);
            j.add("resetting", std::move(reset));
        }
        else
            j.add("resetting", Value());
        markets.push(std::move(j));
    }
    report.add("markets", std::move(markets));
    {
        // The largest pressures with their causes (7.3: the price board): the 30 furthest from 0 now.
        std::vector<std::tuple<double, std::string, std::string, std::string>> cells;
        for (const auto& [m, goods] : b.cell)
            for (const auto& [good, cell] : goods)
                if (const double P = society_.pressure(m, good); std::abs(P) >= .005)
                    cells.push_back({P, m, good, cell.cause});
        std::sort(cells.begin(), cells.end(), [](const auto& a, const auto& c) {
            return std::abs(std::get<0>(a)) != std::abs(std::get<0>(c)) ? std::abs(std::get<0>(a)) > std::abs(std::get<0>(c))
                                                                          : std::tie(std::get<1>(a), std::get<2>(a)) < std::tie(std::get<1>(c), std::get<2>(c));
        });
        auto pressures = Value::array();
        for (std::size_t i = 0; i < cells.size() && i < PressuresShown; ++i)
        {
            auto j = Value::object();
            j.add("market", std::get<1>(cells[i]));
            j.add("good", std::get<2>(cells[i]));
            j.add("P", std::round(std::get<0>(cells[i]) * 1000) / 1000);
            j.add("cause", std::get<3>(cells[i]));
            pressures.push(std::move(j));
        }
        report.add("pressures", std::move(pressures));
    }
    {
        auto areas = Value::array();
        for (const auto& a : items::farms().areas)
        {
            auto j = Value::object();
            j.add("id", a.id);
            j.add("name", a.name.empty() ? a.id : a.name);
            j.add("harvest", std::string(HarvestWords[std::size_t(std::clamp(harvestOf(a.id), 0, 2))]));
            if (const auto o = b.harvestOverride.find(a.id); o != b.harvestOverride.end() && calendarDays_ < o->second.until)
            {
                auto over = Value::object();
                over.add("word", std::string(HarvestWords[std::size_t(std::clamp(o->second.word, 0, 2))]));
                over.add("until", o->second.until);
                over.add("words", o->second.cause);
                j.add("override", std::move(over));
            }
            else
                j.add("override", Value());
            areas.push(std::move(j));
        }
        report.add("areas", std::move(areas));
    }
    {
        auto kinds = Value::array();
        for (const auto& k : society_.market().events)
        {
            auto j = Value::object();
            j.add("id", k.id);
            j.add("words", k.words);
            auto needs = Value::array();
            if (!k.needs.empty())
                needs.push(k.needs);
            j.add("needs", std::move(needs));
            kinds.push(std::move(j));
        }
        report.add("eventKinds", std::move(kinds));
        // The days an event may be added on (World::addStagedEvent: this calendar week and the next, from today).
        auto days = Value::array();
        const auto today = std::int64_t(std::floor(calendarDays_));
        for (auto d = today; d < (today / calendar::DaysPerWeek + 2) * calendar::DaysPerWeek; ++d)
            days.push(double(d));
        report.add("weekDays", std::move(days));
        report.add("nextWeek", double((today / calendar::DaysPerWeek + 1) * calendar::DaysPerWeek));   // (Its first day.)
    }
    {
        // The DM's bands for a while (band.override), each household's with its end and its members, so the DM tool can
        // show what is in force on an NPC and when it lapses.
        auto held = Value::array();
        const auto& st = society_.state();
        for (const auto& [household, o] : st.staged.overrides)
        {
            if (o.until <= st.budgetDay)
                continue;
            auto j = Value::object();
            j.add("household", household);
            j.add("band", status::bandName(o.band));
            j.add("until", double(o.until));
            auto members = Value::array();
            if (const auto* m = society_.membersOf(household))
                for (const auto& id : *m)
                    members.push(id);
            j.add("members", std::move(members));
            held.push(std::move(j));
        }
        report.add("bandOverrides", std::move(held));
    }
    return report;
}
} // namespace ratw
