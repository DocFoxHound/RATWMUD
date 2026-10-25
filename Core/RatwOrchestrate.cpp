// The society's side of the economy's monitor (Docs/Design/46-economy-orchestrator.md; doc 64, Phase 12): what it counts
// between snapshots, the snapshot it takes once a day at SnapshotHour, and the measures it keeps when the day turns. And
// the Dungeon Master's three levers (doc 64, decision 5): the tax rate, the mint on or off, and a relief grant from the
// capital's treasury, each logged in the ledger.
#include "RatwItems.h"
#include "RatwSociety.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace ratw
{
namespace
{
// Money a resident is given rather than earns (for "idle": a wolf that earned nothing in three days).
bool unearned(const std::string& kind)
{
    static const char* const words[] = {"inheritance", "operator", "the shop's till", "sale of a business", "starting money",
                                        "a child's", "stipend", "household purse", "dole", "the church's share", "alms",
                                        "beggar", "money for the road", "welcome grant", "world's money", "for the poor",
                                        "a gift", "enclosure", "an owner's investment"};   // (Doc 55: given, not earned.)
    for (const auto* w : words)
        if (kind.find(w) != std::string::npos)
            return true;
    return false;
}
} // namespace

bool Society::orchestrating() const
{
    return roster_ == Roster::Authored;
}

void Society::forgetOrchestra()
{
    measured_.reset();                               // (Measures for the old society are dropped.)
    snapshotDay_ = countingFrom_ = -1;
    accountTown_.clear();
    townFlow_.clear();
    playerFlow_.clear();
    earned_.clear();
}

std::vector<orchestra::Brief> Society::takeBriefs()
{
    std::vector<orchestra::Brief> out;
    out.swap(briefs_);
    return out;
}

void Society::noteForOrchestra(const std::string& kind, const std::string& from, const std::string& to, std::int64_t coins,
                               const std::string& item, int quantity)
{
    if (accountTown_.empty())
        return;
    // Players' help (doc 57, 5): coins between a player and a town's account (a shop's till, its treasury, its works, its
    // projects); and goods a player sold to a shop ("local trade", whose entry runs buyer to seller) or handed in there.
    const bool fromPlayer = playerAccountId(from), toPlayer = playerAccountId(to);
    if (fromPlayer != toPlayer)
    {
        const auto& other = fromPlayer ? to : from;
        std::string town;
        if (const auto t = accountTown_.find(other); t != accountTown_.end())
            town = t->second;
        else if (const auto p = projectTowns_.find(other); p != projectTowns_.end())
            town = p->second;
        if (!town.empty())
        {
            auto& flow = playerFlow_[town];
            if (coins > 0)
                (fromPlayer ? flow.in : flow.out) += coins;
            const bool sold = kind == "local trade" && toPlayer;
            const bool handed = fromPlayer && (kind == "contract delivery" || kind == "handed in to a town project");
            if (!item.empty() && quantity > 0 && (sold || handed))
                flow.goods += std::int64_t(std::llround(quantity * townPrice(town, item)));
        }
    }
    if (coins <= 0)
        return;
    // Coins that go from one town to another (an account of no town, the road's or the land church's, doesn't count: what
    // the church takes in tithes it gives back as alms and wages in every town).
    const auto a = accountTown_.find(from), b = accountTown_.find(to);
    if (a != accountTown_.end() && b != accountTown_.end() && !a->second.empty() && !b->second.empty() && a->second != b->second)
    {
        townFlow_[a->second].second += coins;
        townFlow_[b->second].first += coins;
    }
    // What a resident earns.
    if (!unearned(kind))
        if (const auto e = earned_.find(to); e != earned_.end())
        {
            const auto slot = std::size_t(((state_.budgetDay % 7) + 7) % 7);
            if (e->second.day[slot] != state_.budgetDay)
                e->second.day[slot] = state_.budgetDay, e->second.coins[slot] = 0;
            e->second.coins[slot] += coins;
        }
}

bool Society::wantsSnapshot(double absoluteDay) const
{
    if (!orchestrating())
        return false;
    const auto day = std::int64_t(std::floor(absoluteDay));
    return (absoluteDay - double(day)) * 24 >= SnapshotHour && snapshotDay_ < day + 1;
}

void Society::orchestrate(double absoluteDay, const std::map<std::string, LifeBody>& bodies)
{
    if (!orchestrating())
        return;
    const auto day = std::int64_t(std::floor(absoluteDay));
    // The day turns: the measures taken for it last evening are the ones shown (so every run sees them at the same moment).
    if (day > state_.budgetDay && measured_)
    {
        auto [brief, memory] = std::move(*measured_);
        measured_.reset();
        if (brief.day == day)                        // (Measures for a day already gone are dropped.)
        {
            state_.orchestrator.memory = std::move(memory);
            state_.orchestrator.last = brief;
            if (brief.decided)
                state_.orchestrator.decision = brief;
            if (keepBriefs_)
                briefs_.push_back(std::move(brief));
        }
    }
    // An hour before the day turns: the snapshot for tomorrow, and its measures.
    if (wantsSnapshot(absoluteDay) && !bodies.empty())
    {
        snapshotDay_ = day + 1;
        auto memory = state_.orchestrator.memory;
        auto brief = orchestra::measure(orchestraSnapshot(day + 1, bodies), memory);
        measured_.emplace(std::move(brief), std::move(memory));
    }
}

orchestra::Snapshot Society::orchestraSnapshot(std::int64_t forDay, const std::map<std::string, LifeBody>& bodies)
{
    orchestra::Snapshot s;
    s.day = forDay;
    s.moneySupply = moneySupply();
    if (countingFrom_ < 0)
        countingFrom_ = forDay;
    s.counted = int(std::min<std::int64_t>(1000, forDay - countingFrom_));
    // The week is weighed the evening of the weekly reckoning, once a day has been counted.
    s.decide = reckonedDay_ == forDay - 1 && s.counted >= 1;
    const auto of = [&](const std::string& cell) { return day_.communityOf ? day_.communityOf(cell) : storeFor(cell); };

    // Residents (the living: those the world gave a body), and where each lives.
    std::unordered_map<std::string, std::string> nextTown;
    std::map<std::string, int> people;
    std::map<std::string, std::vector<const std::string*>> homes;
    for (const auto& [id, life] : state_.residents)
    {
        const auto body = bodies.find(id);
        if (body == bodies.end())
            continue;
        orchestra::ResidentSnap r;
        r.id = id;
        r.home = life.homeCell;
        r.town = life.homeCell.empty() ? communityOfResident(id) : of(life.homeCell);
        r.age = body->second.age;
        r.hunger = life.hunger;
        if (const auto* a = account(id))
            r.cash = a->cash;
        const auto* job = jobOf(id);
        // (Not looking for work: a child, the retired, one keeping the house, or one in an unpaid post: a lord, a student.)
        r.dependent = r.age < 16 || r.age >= RetireAge || (job && (homemaking(job->title) || !job->paid));
        if (const auto e = earned_.find(id); e != earned_.end())
            for (int i = 0; i < 7; ++i)
            {
                const auto ago = forDay - 1 - e->second.day[i];
                if (e->second.day[i] >= 0 && ago >= 0 && ago < 7)
                {
                    r.earned7 += e->second.coins[i];
                    if (ago < 3)
                        r.earned3 += e->second.coins[i];
                }
            }
        nextTown[id] = r.town;
        if (!r.town.empty())
            ++people[r.town];
        if (!life.homeCell.empty())
            homes[life.homeCell].push_back(&id);
        s.residents.push_back(std::move(r));
    }
    // Households: what they hold, in purse and food.
    const auto nourishIn = [&](const EconomyAccount& a, const std::string& holder) {
        std::int64_t n = 0;
        for (const auto& [item, q] : a.stock)
            if (q > 0 && nourishment(item) > 0 && (holder.empty() || !forSale(holder, item)))
                n += std::int64_t(q) * nourishment(item);
        return n;
    };
    for (const auto& [home, members] : homes)
    {
        orchestra::HomeSnap h;
        h.home = home;
        h.town = of(home);
        h.members = int(members.size());
        if (const auto* larder = account(homeStore(home, "larder")))
            h.nourishment += nourishIn(*larder, {});
        for (const auto* id : members)
            if (const auto* a = account(*id))
            {
                const bool till = merchant(*id) && tillOf(*id) == *id;
                if (!till)
                    h.cash += a->cash;
                h.nourishment += nourishIn(*a, *id);
            }
        s.homes.push_back(std::move(h));
    }

    // The accounts that belong to a town, for the coins between towns: its treasury, its buyers, its great houses, its
    // businesses' tills, and the owner-run shops' and producers' own purses (the capital's treasury is the capital's; the
    // land's church is of no town).
    std::map<std::string, std::string> houseTown;
    for (const auto& house : houses())
        houseTown[house.id] = house.community;
    std::vector<std::pair<std::string, std::string>> placed;   // Account -> its town ("": of none).
    for (const auto& [id, a] : state_.accounts)
    {
        if (id == "treasury")
            placed.push_back({id, capital_});
        else if (id.rfind("stores:", 0) == 0)
            placed.push_back({id, id.substr(7)});
        else if (id == SharedChurch)
            placed.push_back({id, {}});
        else if (id.rfind("town:", 0) == 0)
        {
            const auto colon = id.find(':', 5);
            if (colon != std::string::npos && id.compare(colon, std::string::npos, ":church") != 0 &&
                id.compare(colon, std::string::npos, ":granary") != 0)
                placed.push_back({id, id.substr(5, colon - 5)});
        }
        else if (id.rfind("house:", 0) == 0)
            placed.push_back({id, houseTown[id]});
        else if (id.rfind("till:", 0) == 0)
        {
            // A business's till: a shop's, or a farm's or a site's, a house's or its own.
            const auto* p = position(id.substr(5));
            const bool farm = p && p->role != "merchant" && items::producerFor(p->title);
            placed.push_back({id, p ? (farm ? communityOfResident(p->founder) : of(p->work.cell)) : std::string()});
        }
    }
    for (const auto& r : s.residents)
    {
        const auto* job = jobOf(r.id);
        const auto* sp = spec(r.id);
        if (tillOf(r.id) != r.id)
            continue;
        if (job && merchant(r.id))
            placed.push_back({r.id, of(job->work.cell)});
        else if (sp && items::producerFor(sp->workLabel))
            placed.push_back({r.id, r.town});
    }

    // The shops: today's takings against their running, and the food each has in, and asks.
    std::map<std::pair<std::string, std::string>, orchestra::GoodSnap> goods;
    std::map<std::pair<std::string, std::string>, int> priced;
    for (const auto& r : authored_.residents)
    {
        if (r.role != "merchant" || !state_.residents.count(r.id) || !bodies.count(r.id))
            continue;
        const auto* job = jobOf(r.id);
        const auto till = tillOf(r.id);
        const auto* a = account(till);
        if (!job || !a)
            continue;
        const auto town = of(job->work.cell);
        orchestra::ShopSnap shop;
        shop.till = till;
        shop.town = town;
        if (const auto t = takings_.find(till); t != takings_.end())
            shop.takings = t->second;
        shop.running = floatOf(job->id) / FloatDays;
        s.shops.push_back(shop);
        // What it sells: its wares, what it supplies to the makers, and any food it has in.
        auto sold = wares(r.id);
        const auto* business = items::businessFor(r.workLabel);
        const auto supplies = business ? items::suppliesFor(business->id) : std::vector<std::string>{};
        for (const auto& item : supplies)
            if (std::find(sold.begin(), sold.end(), item) == sold.end())
                sold.push_back(item);
        for (const auto& [item, q] : a->stock)
            if (q > 0 && nourishment(item) > 0 && std::find(sold.begin(), sold.end(), item) == sold.end())
                sold.push_back(item);
        for (const auto& item : sold)
        {
            const auto base = items::baseOf(item);
            const auto* good = storePriced(base) ? nullptr : items::good(base);
            if (!good || good->price <= 0 || nourishment(base) <= 0)
                continue;
            auto& g = goods[{town, base}];
            g.town = town;
            g.item = base;
            g.nourish = nourishment(base);
            g.stock += stockAll(*a, base);
            g.price += double(shopPrice(r.id, base));
            ++priced[{town, base}];
        }
    }
    for (auto& [key, g] : goods)
    {
        g.price /= std::max(1, priced[key]);
        s.goods.push_back(g);
    }

    // The towns: coins in and out since the last snapshot, and players' help.
    std::map<std::string, orchestra::TownSnap> towns;
    for (const auto& [town, n] : people)
        towns[town].id = town;
    for (const auto& [town, flow] : townFlow_)
        if (towns.count(town))
            towns[town].inflow = flow.first, towns[town].outflow = flow.second;
    for (const auto& [town, flow] : playerFlow_)    // (Players' help: doc 57, 5.)
        if (const auto t = towns.find(town); t != towns.end())
            t->second.playerIn = flow.in, t->second.playerOut = flow.out, t->second.playerGoods = flow.goods;
    for (auto& [town, t] : towns)
        s.towns.push_back(t);

    // What is counted until the next snapshot: coins between these towns, these residents' earnings (kept for the week).
    townFlow_.clear();
    playerFlow_.clear();
    accountTown_ = std::move(nextTown);
    for (const auto& [id, town] : placed)
        if (!town.empty())
            accountTown_[id] = town;
        else
            accountTown_.erase(id);                  // (The land church's: of no town.)
    for (const auto& r : s.residents)
        earned_.try_emplace(r.id);
    for (auto it = earned_.begin(); it != earned_.end();)
        it = accountTown_.count(it->first) || state_.residents.count(it->first) ? std::next(it) : earned_.erase(it);
    return s;
}

// --- The Dungeon Master's levers (doc 64, decision 5) --------------------------------------------------------------------
namespace
{
std::string percent(double rate)
{
    char text[16];
    std::snprintf(text, sizeof text, "%g%%", std::round(rate * 1000) / 10);
    return text;
}
} // namespace

EconomyResult Society::setTaxRate(double rate, const std::string& by)
{
    if (!std::isfinite(rate) || rate < 0 || rate > MostTaxRate)
        return {false, "The tax rate is 0% to " + percent(MostTaxRate) + " of a week's income."};
    const double was = state_.memory.taxRate;
    state_.memory.taxRate = rate;
    ++state_.memory.revision;
    record("the tax rate set from " + percent(was) + " to " + percent(rate), by.empty() ? "a Dungeon Master" : by, {}, {}, 0, 0);
    return {true, "From the next reckoning, " + percent(rate) + " of a week's income."};
}

EconomyResult Society::setMint(bool on, const std::string& by)
{
    auto& mint = state_.memory.mint;
    if (mint.on == on)
        return {true, on ? "The mint is on already." : "The mint is off already."};
    mint.on = on;
    ++state_.memory.revision;
    record(on ? "the mint turned on" : "the mint turned off", by.empty() ? "a Dungeon Master" : by, {}, {}, 0, 0);
    return {true, on ? "The mint is on: it strikes and melts from its next week." : "The mint is off: nothing more is struck or melted."};
}

EconomyResult Society::reliefGrant(const std::string& town, std::int64_t coins)
{
    const auto from = treasuryOf(capital_), to = treasuryOf(town);
    if (town.empty() || to == from || !account(to))
        return {false, "Name a town with a treasury of its own, not the capital."};
    const auto* purse = account(from);
    if (coins <= 0 || !purse || coins > purse->cash)
        return {false, "The capital's treasury holds " + std::to_string(purse ? purse->cash : 0) + "p: grant 1p to that."};
    if (!shift(from, to, "", 0, coins, "a relief grant"))
        return {false, "The grant couldn't be made."};
    return {true, std::to_string(coins) + "p from the capital's treasury to " + town + "'s."};
}
} // namespace ratw
