#include "RatwSociety.h"

#include "RatwCalendar.h"
#include "RatwItems.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>

// Crafting by shopkeepers (Docs/Design/35-items-crafting-industry.md, Phase 5; Data/Items/crafts.json).
// A maker at work makes a batch of whatever has run low on their shelves, from materials in their own stock; when the
// materials run short they buy more, at the catalog's price, from a shop in the same community that supplies them.
// Suppliers restock from the workshops that make their goods (a mill's flour) and from producers who bring goods in
// from the land (a farmer's wheat). What a community can't find at home is fetched from another, dearer.
// Nothing is made from nothing: goods appear only from producers' work and the starting materials (stockMaterials).
namespace ratw
{
int Society::stockMaterials(const std::string& id)
{
    const auto* r = spec(id);
    const auto* business = r && r->role == "merchant" ? items::businessFor(r->workLabel) : nullptr;
    const auto found = state_.accounts.find(tillOf(id));   // (A house's business: its till, doc 42.)
    if (!business || found == state_.accounts.end())
        return 0;
    auto& stockHeld = found->second.stock;
    int added = 0;
    const auto topUp = [&](const std::string& item, int level) {
        const int held = stock(found->second, item);
        if (held >= level || (!stockHeld.count(item) && stockHeld.size() >= MaxGoodsKinds))
            return;
        stockHeld[item] = std::min(level, StockCap);
        added += stockHeld[item] - held;
    };
    for (const auto* craft : items::craftsFor(business->id))
        for (const auto& [item, count] : craft->in)
            topUp(item, count * MaterialBatches);
    for (const auto& item : items::suppliesFor(business->id))
        topUp(item, SuppliesKept);
    if (added > 0)
        record("starting materials", "outside", found->first, "", 0, 0);
    return added;
}

std::string Society::communityOfResident(const std::string& id) const
{
    const auto* r = spec(id);
    if (!r)
        return {};
    // The day's plans know the communities; without them, the store a cell's merchants restock from. A producer who
    // works out in the country (a farmer in the fields) belongs to the town they live in.
    const auto of = [&](const std::string& cell) { return day_.communityOf ? day_.communityOf(cell) : storeFor(cell); };
    auto here = of(r->work.cell);
    return here.empty() ? of(r->home.cell) : here;
}

const std::vector<Society::Seller>* Society::sellersOf(const std::string& item) const
{
    if (suppliers_.empty())
        for (const auto& r : authored_.residents)
        {
            if (const auto* producer = items::producerFor(r.workLabel))
                for (const auto& [made, n] : producer->out)
                    suppliers_[made].push_back({r.id, 2});
            if (r.role != "merchant")
                continue;
            if (const auto* business = items::businessFor(r.workLabel))
            {
                for (const auto& supply : items::suppliesFor(business->id))
                    suppliers_[supply].push_back({r.id, 0});
                for (const auto* craft : items::craftsFor(business->id))
                    for (const auto& [made, n] : craft->out)
                        if (std::none_of(suppliers_[made].begin(), suppliers_[made].end(),
                                         [&](const Seller& s) { return s.id == r.id; }))
                            suppliers_[made].push_back({r.id, 1});
            }
        }
    const auto found = suppliers_.find(item);
    return found == suppliers_.end() ? nullptr : &found->second;
}

void Society::missedAt(const std::string& town, const std::string& item, int n)
{
    const auto* sellers = n > 0 ? sellersOf(item) : nullptr;
    if (!sellers)
        return;
    std::vector<std::string> producers;
    for (const auto& s : *sellers)
        if (s.kind == 2 && state_.residents.count(s.id) && communityOfResident(s.id) == town)
            producers.push_back(tillOf(s.id));
    // (Shared out whole, the odd ones to the first. With the buyers who found none, not with its sales: a producer that
    // sold its last that day counts its missed sales or its sold-out half again, the more, not both. Society::spoil.)
    for (std::size_t k = 0; k < producers.size(); ++k)
        if (const int share = n / int(producers.size()) + (int(k) < n % int(producers.size()) ? 1 : 0); share > 0)
            missedToday_[producers[k]][item] += share;
}

int Society::buyMaterials(const std::string& id, const std::string& workCell, const std::string& item, int wanted,
                          bool forSupplier, bool credit)
{
    const auto* good = items::good(item);
    if (!good || wanted < 1)
        return 0;
    const auto till = tillOf(id);                   // The shop's money and shelves (its house's till, doc 42).
    const auto* sellers = sellersOf(item);
    if (!sellers)
        return 0;
    const auto of = [&](const std::string& cell) { return day_.communityOf ? day_.communityOf(cell) : storeFor(cell); };
    auto here = of(workCell);
    if (here.empty())
        here = communityOfResident(id);
    int bought = 0;
    bool more = false;                              // A seller in its town had more it would sell.
    // What it bought of it today, this call's with those before it. And none left to buy in its town (doc 64, Phase 13;
    // the model's asks): what it went without of a day's use of it, once a day, is a sale its town's producers missed.
    // (Not what it was short of money for, nor what a ledger entry's 99 left behind: only when its town's sellers had no
    // more, keeping back what they keep. Counted the first time it is, not the first time it looked: a call short only
    // for money stamped the day, and the want found later went uncounted. Against all it bought that day: a mill that
    // laid in two days' wheat from one farm, calling again for more, found the other's barn empty, and told it of a
    // day's sales it missed.)
    const auto tally = [&](int n, bool left) {
        if (n <= 0 && left)
            return n;
        auto& today = unmetOn_[till + "|" + item];
        if (today.day != state_.budgetDay)
            today.day = state_.budgetDay, today.bought = 0;
        const int before = today.bought;
        today.bought += n;
        if (!left && today.counted != state_.budgetDay)
            if (const int missed = std::min(before + wanted, int(std::ceil(useRate(till, item)))) - today.bought; missed > 0)
            {
                today.counted = state_.budgetDay;
                missedAt(here, item, missed);
            }
        return n;
    };
    // Close to home first, at the price; then anywhere, carted in (in a world of towns, only the caravans bring what a
    // town lacks: doc 42, Phase 7).
    for (const bool local : {true, false})
        for (int kind = forSupplier ? 1 : 0; kind <= 2 && bought < wanted && (local || !tradeByCaravan_); ++kind)
            for (const auto& seller : *sellers)
            {
                if (seller.kind != kind || seller.id == id || (communityOfResident(seller.id) == here) != local)
                    continue;
                const auto from = account(tillOf(seller.id));
                const auto* buyer = account(till);
                if (!from || !buyer)
                    continue;
                // Any quality it has (doc 35, Part 4), at that quality's price. A workshop keeps a few common ones for
                // its own shelf, and of what it works with too, its own batches' (keepsBack); a supplier or producer
                // sells all it has.
                const int keeps = kind == 1 ? keepsBack(seller.id, item) : 0;
                for (const auto& sort : kindsHeld(*from, item))
                {
                    const auto* sortGood = items::good(sort);
                    const auto* buyerNow = account(till);
                    if (!sortGood || !buyerNow)
                        continue;
                    const int spare = stock(*from, sort) - (sort == item ? keeps : 0);
                    // At the seller's own price (doc 64, Phase 4); carted in from another town, its carriage besides.
                    const double each = priceAt(seller.id, sort);
                    const std::int64_t price = pennies(each + (local ? 0. : carriage(sort, CarriageLegs, here)));
                    const int n = int(std::min<std::int64_t>({wanted - bought, spare, 99, spendable(till) / price}));
                    if (n > 0 && transfer(tillOf(seller.id), till, sort, n, price, local ? "materials bought" : "materials carted in"))
                        bought += n;
                    // Short of money: the rest on credit, up to a week of its turnover (doc 64, Phase 5).
                    int owing = 0;
                    if (const int rest = int(std::min<std::int64_t>({wanted - bought, spare - std::max(0, n), 99, creditLeft(till) / price}));
                        credit && rest > 0 && till.rfind("till:", 0) == 0 && onCredit(tillOf(seller.id), till, sort, rest, price))
                        bought += rest, owing = rest;
                    if (bought >= wanted)
                        return tally(bought, more);
                    // (What it has left of what it would sell, as it keeps back: left for want of money or by a ledger
                    // entry's 99 is no sale missed.)
                    more = more || (local && spare - std::max(0, n) - owing > 0);
                }
            }
    return tally(bought, more);
}

int Society::keepsBack(const std::string& seller, const std::string& item) const
{
    // (A workshop that works with it too, a chandler's cord for its candles, keeps back what its own batches want before
    // it buys more: two chandlers stripped each other. Doc 64, Phase 13. Not a store of them: a butcher held back two
    // dozen raw meat that spoiled, a plasterworks four dozen lime.)
    const auto* r = spec(seller);
    const auto* business = r ? items::businessFor(r->workLabel) : nullptr;
    if (!business || rots(items::good(item)))
        return GoodsKept;
    // (Its crafts' need of each material, worked out once a business: each thread remembers.)
    thread_local std::unordered_map<std::string, std::unordered_map<std::string, int>> needs;
    auto known = needs.find(business->id);
    if (known == needs.end())
    {
        known = needs.emplace(business->id, std::unordered_map<std::string, int>{}).first;
        for (const auto* k : items::craftsFor(business->id))
            for (const auto& [in, count] : k->in)
                known->second[in] += count * MaterialsLow;
    }
    const auto need = known->second.find(item);
    return need == known->second.end() ? GoodsKept : std::max(GoodsKept, need->second);
}

bool Society::produce(const std::string& id, double absoluteDay, double hours)
{
    const auto* r = spec(id);
    const auto* producer = r ? items::producerFor(r->workLabel) : nullptr;
    const auto till = tillOf(id);
    if (!producer || !account(till))
        return false;
    const bool inSeason = producer->seasons.empty() ||
                          std::find(producer->seasons.begin(), producer->seasons.end(), season_) != producer->seasons.end();
    if (!inSeason && producer->offSeason.empty())
        return false;                               // Nothing grows in the fields this season.
    // Its work in hand (doc 64, Phase 3): this spell's, and any a hand put in (RatwOddJobs.cpp); improved premises
    // (better tools, a bigger barn: RatwOddJobs.cpp) make an hour go further.
    const auto* job = jobOf(id);
    const double improved = 1 - ImprovementPace * (job ? improvement(job->id) : 0);
    creditWork(id, hours / improved);
    auto& banked = workBanked_[id];
    // Its tools worn by the hours its work takes, not those it stands idle (doc 64, Phase 13: a farmer between sowing and
    // the harvest bought sickles for nothing): what goes out of its work in hand, as real hours (an improved farm's hour
    // in hand took less than an hour), its hands' with its own: they work with its tools. (With work by the hours: at the
    // seconds' pace, its own spells', as it was.)
    const double before = banked;
    const auto wear = [&] { wearTools(till, producer->id, workByHours() ? std::max(0., before - banked) * improved : hours); };
    bool any = false;
    // Seasonal fields (doc 64, Phase 3b): sown in spring, brought in at the harvest, sold from the store all year. (Until
    // it has a week of sales to plan by, as before: doc 64, Phase 13.)
    if (fieldsOn() && fieldWork(producer->id) && (state_.memory.fields.count(till) || foundingRate(till, *producer) > 0))
    {
        if (!state_.memory.fields.count(till))
            planField(id, till, *producer, absoluteDay, true);
        auto& f = state_.memory.fields[till];
        const auto window = fieldWindow(absoluteDay);
        const auto today = std::int64_t(std::floor(absoluteDay));
        // (The harvest over: next year's acres. Its founding spring is looked at in the day's pass: Society::tendLabour.)
        if (today % calendar::DaysPerYear >= HarvestEnd && f.year < today / calendar::DaysPerYear)
            planField(id, till, *producer, absoluteDay, false);
        // (Dry when any of what it is planned by runs out, not only its first good: a farm's wheat held while its oats
        // were gone.)
        if (const auto* a = account(till))
            for (const auto& [item, n] : producer->out)
                if (plannedBy(*producer, item) && stockAll(*a, item) <= 0)
                    f.dry = true;
        const double acre = hoursFor(*producer) * FieldYields;
        if (window == 1 && f.sown < f.acres)
        {
            const double done = std::min(f.acres - f.sown, banked / (acre * SowShare));
            f.sown += done;
            f.toHarvest += done;
            banked -= done * acre * SowShare;
        }
        else if (window == 2 && f.toHarvest > 0)
        {
            const double done = std::min(f.toHarvest, banked / (acre * (1 - SowShare)));
            f.toHarvest -= done;
            banked -= done * acre * (1 - SowShare);
            f.part += done * FieldYields;
            const int yields = int(std::floor(f.part));
            f.part -= yields;
            for (const auto& [item, count] : producer->out)
            {
                int brought = yields * count;
                if (items::townGoodOf(communityOfResident(id), item))
                    brought = int(std::lround(brought * items::tradeYield()));
                for (; brought > 0; brought -= 99)           // (A ledger entry holds 99 at most.)
                {
                    addCost(till, item, stock(*account(till), item), std::min(brought, 99), normalCost(id, item));
                    any = create(till, item, std::min(brought, 99), "brought in") || any;
                }
            }
        }
        else if (window == 0)
            // Between: what the work brings in out of season (an orchard's prunings for firewood), not the fields' goods.
            // (Its hours taken only for a yield that brought something in: with nothing wanted they stay in hand, and
            // are lost. Doc 64, Phase 13.)
            for (bool made = true; made && banked >= hoursFor(*producer);)
            {
                made = false;
                for (const auto& [item, count] : producer->offSeason)
                    if (std::none_of(producer->out.begin(), producer->out.end(), [&](const auto& o) { return o.first == item; }))
                        if (const int held = stock(*account(till), item); held < shelfTarget(till, item, ProducerKept))
                        {
                            addCost(till, item, held, count, normalCost(id, item));
                            made = create(till, item, count, "brought in") || made;
                        }
                if (made)
                    banked -= hoursFor(*producer);
                any = any || made;
            }
        wear();
        return any;
    }
    // A town's trade (doc 61): its producers bring in more of its goods, the fraction by a lot fixed by the producer and
    // the spell (so a run repeats), and keep more of them in store to send away. (A yield's hours are taken only when it
    // brought something in: with its store full they stay in hand, and are lost. Doc 64, Phase 13; with work by the
    // hours: at the seconds' pace, taken whatever it brought, as they were.)
    const auto town = communityOfResident(id);
    for (int yield = 0; banked >= hoursFor(*producer); ++yield)
    {
        bool made = false;
        const auto spell = std::uint64_t(std::floor(absoluteDay * 96)) * 16 + std::uint64_t(yield);
        // Out of season, what the work brings in instead (threshing the barn's grain in winter, doc 42).
        for (const auto& [item, count] : inSeason ? producer->out : producer->offSeason)
        {
            int brought = count, kept = ProducerKept;
            if (items::townGoodOf(town, item))
            {
                const double more = count * items::tradeYield();
                const double lot = double(std::hash<std::string>{}(id + "|" + item + "|" + std::to_string(spell)) % 1000) / 1000;
                brought = int(std::floor(more)) + (lot < more - std::floor(more) ? 1 : 0);
                kept = int(std::lround(ProducerKept * items::tradeKeep()));
            }
            kept = shelfTarget(till, item, kept);   // (By what it sells, and the roads' orders on it: doc 64, Phase 3b.)
            if (const int held = stock(*account(till), item); held < kept)
            {
                addCost(till, item, held, std::min(brought, kept - held), normalCost(id, item));
                made = create(till, item, std::min(brought, kept - held), "brought in") || made;
            }
        }
        if (!made && workByHours())
            break;
        banked -= hoursFor(*producer);
        any = any || made;
    }
    wear();
    return any;
}

void Society::wearTools(const std::string& till, const std::string& producer, double hours)
{
    // Its tools worn by the work (crafts.json `tools`: a tool lasts so many days of spells; bought when worn: tradeUpkeep).
    for (const auto& t : items::toolNeeds())
        if (t.producer == producer && t.everyDays > 0 && hours > 0)
            toolsWorn_[till][t.item] += hours / (PaidSpells * t.everyDays);
}

double Society::hoursFor(const items::Craft& k) const
{
    // The old pace, and with work by the hours the realm's way from it to the calibrated hours (memory.pace).
    const double old = k.seconds / 3600.;
    return workByHours_ ? old + std::clamp(state_.memory.pace, 0., 1.) * (k.hours - old) : old;
}

double Society::hoursFor(const items::Producer& p) const
{
    // (At the old pace a producer brought in a yield a spell at most: it looked once a spell, however short its seconds.)
    const double old = std::max(1., p.seconds / 3600.);
    return workByHours_ ? old + std::clamp(state_.memory.pace, 0., 1.) * (p.hours - old) : old;
}

void Society::creditWork(const std::string& account, double hours)
{
    if (hours <= 0)
        return;
    auto& banked = workBanked_[account];
    const double lost = std::max(0., banked + hours - WorkBankedMost);   // (Hands with nothing to make: hours that go nowhere.)
    workCredited_ += hours;
    workLost_ += lost;
    auto& by = workBy_[account];
    by.first += hours;
    by.second += lost;
    banked = std::min(WorkBankedMost, banked + hours);
}

double Society::useRate(const std::string& till, const std::string& item) const
{
    const auto* p = till.rfind("till:", 0) == 0 ? position(till.substr(5)) : nullptr;
    const auto* business = p ? items::businessFor(p->title) : nullptr;
    const auto rates = sellRate_.find(till);
    if (!business || rates == sellRate_.end())
        return 0;
    const auto rate = [&](const std::string& good) {
        const auto found = rates->second.find(good);
        return found == rates->second.end() ? 0. : found->second;
    };
    double use = 0;
    for (const auto* k : items::craftsFor(business->id))
    {
        const auto in = std::find_if(k->in.begin(), k->in.end(), [&](const auto& i) { return i.first == item; });
        if (in != k->in.end() && !k->out.empty())
            use += rate(k->out.front().first) / std::max(1, k->out.front().second) * in->second;
    }
    const auto& supplies = items::suppliesFor(business->id);
    if (std::find(supplies.begin(), supplies.end(), item) != supplies.end())
        use += rate(item);
    return use;
}

bool Society::rots(const items::Item* good)
{
    return good && good->keeps > 0 && good->nourish > 0;
}

int Society::inputStore(const std::string& till, const items::Craft& k, const std::string& item, int least) const
{
    if (k.out.empty() || (!edible(k.out.front().first) && k.out.front().first != "flour"))
        return least;
    const auto& made = k.out.front().first;
    const auto* out = items::good(made);
    const auto* in = items::good(item);
    double days = out && out->staple ? StapleInputDays : InputDays;
    if (rots(in))
        days = std::min(days, std::max(1., in->keeps - 1));   // (Fish two days, not a week of it rotting.)
    return std::max(least, std::min(ShelfMost, int(std::ceil(useRate(till, item) * days))));
}

int Society::shelfTarget(const std::string& till, const std::string& item, int least, bool orders) const
{
    const auto base = items::baseOf(item);
    const auto* good = items::good(base);
    double rate = 0;
    if (const auto rates = sellRate_.find(till); rates != sellRate_.end())
        if (const auto r = rates->second.find(base); r != rates->second.end())
            rate = r->second;
    // (Kept whole: a shelf for a day's keeping holds a whole one more.)
    const double slack = good && good->staple ? StapleSlack : 1.;
    // Wood laid in for winter (doc 64, Phase 3b): from a month before winter, what households burn faster then (crafts.json
    // `winterDays`, firewood and peat) is kept at its winter rate, so the store is there when the cold comes.
    if (const auto d = state_.budgetDay % calendar::DaysPerYear; d >= WinterStart - MonthDays && d < WinterStart)
        for (const auto& need : items::householdNeeds())
            if (need.winterDays > 0 && need.winterDays < need.everyDays && std::find(need.any.begin(), need.any.end(), base) != need.any.end())
                rate *= need.everyDays / need.winterDays;
    // Food is kept over the days ahead the shops aren't properly open too (Restday, a festival), as far as it keeps
    // (doc 64, Phase 13): the eve's makers hold the shut day's sales, so the larders laying in don't empty the shelves.
    double days = keepDays(base);
    if (good && good->nourish > 0 && !good->drink)
        if (const int shut = shutAhead(state_.budgetDay); shut > 0)
            days += good->keeps > 0 ? std::min<double>(shut, std::max(0., good->keeps - 1 - days)) : shut;
    int target = std::max(least, int(std::ceil(rate * days * slack)));
    // A week of the roads' orders on it and half a supplier's store, kept for the caravan, as a smelter keeps bars to
    // send (doc 61). (Not of what rots before the week is out; but of what keeps a day yet never rots, milk, its share:
    // doc 64, Phase 13.)
    if (const auto* p = orders && !rots(good) && till.rfind("till:", 0) == 0 ? position(till.substr(5)) : nullptr)
        if (const auto ordered = ordersFrom_.find({communityOfResident(p->founder), base}); ordered != ordersFrom_.end())
            target = std::max(target, ordered->second + SuppliesKept / 2);
    return std::min(target, ShelfMost);
}

void Society::setStandingOrders(const std::map<std::pair<std::string, std::string>, int>& perWeek,
                                std::map<std::string, std::int64_t> costs)
{
    // Each of a town's makers of the good keeps its share: kept whole by every one, Ridgemere's six smiths each made
    // the city's week of nails for the towns, and sat on them.
    // (And its producers': a farm's grain goes on the roads too, doc 64, Phase 3b.)
    std::map<std::pair<std::string, std::string>, int> makers;
    for (const auto& p : positions_)
    {
        std::set<std::string> made;
        if (const auto* business = p.role == "merchant" ? items::businessFor(p.title) : nullptr)
        {
            for (const auto* k : items::craftsFor(business->id))
                if (!k->out.empty())
                    made.insert(k->out.front().first);
        }
        else if (const auto* producer = items::producerFor(p.title))
            for (const auto& [item, n] : producer->out)
                made.insert(item);
        const auto town = communityOfResident(p.founder);
        for (const auto& item : made)
            if (perWeek.count({town, item}))
                ++makers[{town, item}];
    }
    ordersFrom_.clear();
    for (const auto& [key, n] : perWeek)
    {
        const int shares = std::max(1, makers.count(key) ? makers.at(key) : 0);
        ordersFrom_[key] = (n + shares - 1) / shares;
    }
    ordersCost_ = std::move(costs);
}

void Society::craft(const std::string& id, const std::string& workCell, double absoluteDay)
{
    const auto* r = spec(id);
    const auto* business = r ? items::businessFor(r->workLabel) : nullptr;
    const auto till = tillOf(id);                   // The shop's shelves (its house's till, doc 42).
    if (!r || !account(till))
        return;
    if (const auto next = craftNext_.find(id); next != craftNext_.end() && absoluteDay < next->second)
        return;
    const auto crafts = business ? items::craftsFor(business->id) : std::vector<const items::Craft*>{};
    const auto& supplies = business ? items::suppliesFor(business->id) : std::vector<std::string>{};
    const auto supplied = [&](const std::string& item) { return std::find(supplies.begin(), supplies.end(), item) != supplies.end(); };
    // What it makes itself (a craft's first good: a chandler's cord), it buys as a supplier does (doc 64, Phase 13).
    const auto madeHere = [&](const std::string& item) {
        return std::any_of(crafts.begin(), crafts.end(), [&](const items::Craft* k) { return !k->out.empty() && k->out.front().first == item; });
    };
    // Preserving (salting, smoking: doc 42, "Pressure"): a shop that sells a fresh food it also preserves keeps GoodsKept
    // of it fresh on the counter, for hot meals and those who eat it the day they buy it; only what's over is cured.
    const auto fresh = [&](const items::Craft& k, const std::string& item) {
        const auto* in = items::good(item);
        const auto* out = items::good(k.out.front().first);
        if (!business || !rots(in) || !out || out->keeps <= in->keeps)
            return 0;
        const auto sold = items::goodsSold(*business, 1 << 20);
        return std::find(sold.begin(), sold.end(), item) != sold.end() ? GoodsKept : 0;
    };
    const auto has = [&](const items::Craft& k) {
        return std::all_of(k.in.begin(), k.in.end(),
                           [&](const auto& i) { return stockAll(*account(till), i.first) >= i.second + fresh(k, i.first); });
    };
    // A batch finished: its materials are used and its goods are on the shelf (if the materials are still there).
    if (const auto atWork = craftAtWork_.find(id); atWork != craftAtWork_.end())
    {
        const auto k = std::find_if(crafts.begin(), crafts.end(), [&](const auto* c) { return c->id == atWork->second; });
        craftAtWork_.erase(atWork);
        if (k != crafts.end() && has(**k))
        {
            // The materials, plain ones first (common, then crude, then the better), and how good they were on the whole.
            double qualities = 0, materials = 0;
            int used = 0;
            // A paw lent (doc 53, 3): one batch in four, one input of its first material isn't used up.
            bool saved = false;
            if (const auto paw = pawLent_.find(id); paw != pawLent_.end() && paw->second.second >= absoluteDay - 1)
                saved = std::hash<std::string>{}(id + (*k)->id + std::to_string(std::int64_t(absoluteDay * 1440))) % 4 == 0;
            for (const auto& [item, count] : (*k)->in)
            {
                int left = count - (saved ? 1 : 0);
                saved = false;
                for (const auto& sort : kindsHeld(*account(till), item))
                {
                    const double each = inputCost(till, sort);
                    const int took = consume(till, sort, std::min(left, stock(*account(till), sort)), "used in crafting");
                    materials += took * each;
                    qualities += took * items::qualityOf(sort);
                    used += took;
                    left -= took;
                    if (left <= 0)
                        break;
                }
            }
            // The batch's quality (doc 35, Part 4): the maker's skill at their trade, the materials (a crude one drags
            // it down, a fine one lifts it) and the day's luck. Fine needs a skill of 60 (and is common only past 75), masterwork 85.
            const auto* job = jobOf(id);
            // (Against its trade's level, as its pace is: a hand at the level crafts as a 50 did. Doc 64, Phase 8.)
            const double craft = job ? skill(id, job->id) - job->level + 50 : 30;
            const double fromMaterials = used > 0 ? (qualities / used - 1) * 15 : 0;
            const double luck = (double(std::hash<std::string>{}(id + "|" + std::to_string(std::int64_t(absoluteDay * 86400))) % 1000) / 1000 - .5) * 40;
            // A Gift lent to it (doc 43): the forge kept hot, the clay true, the flaws heard; once, for this batch.
            double lifted = 0;
            if (const auto lent = giftLift_.find(id); lent != giftLift_.end())
            {
                lifted = absoluteDay <= lent->second.second ? lent->second.first : 0;
                giftLift_.erase(lent);
            }
            const double score = craft + fromMaterials + luck + lifted;
            // Masterwork is rare even for a master: a good day, and a further one in seven besides.
            const bool inspired = std::hash<std::string>{}(id + "|muse|" + std::to_string(std::int64_t(absoluteDay * 86400))) % 7 == 0;
            const int quality = craft >= 85 && score >= 100 && inspired ? 3 : craft >= 60 && score >= 85 ? 2 : score < 20 ? 0 : 1;
            // A masterwork carries its maker's mark (doc 35, Part 4).
            // What the batch cost (doc 64, Phase 4): its hours at the town's wage for the trade, and its materials at what
            // they cost; shared among its goods by their worth.
            double worth = 0;
            for (const auto& [item, count] : (*k)->out)
                worth += count * items::worthOf(item);
            const double batch = (*k)->hours * (1 + (*k)->skill / items::ladder().skillWage) * labourDay(communityOfResident(id)) / PaidSpells + materials;
            for (const auto& [item, count] : (*k)->out)
            {
                const auto made = quality == 3 ? items::withMaker(items::withQuality(item, 3), id) : items::withQuality(item, quality);
                addCost(till, made, stock(*account(till), made), count, worth > 0 ? batch * items::worthOf(item) / worth : 0);
                create(till, made, count, "crafted");
            }
            madeToday_[till][(*k)->out.front().first] += (*k)->out.front().second;   // (What it can make: doc 64, Phase 13.)
        }
    }
    // A supplier's shelves: what has run below a quarter of its store is bought back up from workshops and producers.
    // (Its store by what it sells, doc 64, Phase 3b: SuppliesKept at least.)
    for (const auto& item : supplies)
        if (const int store = shelfTarget(till, item, SuppliesKept); stockAll(*account(till), item) < store / 4)
            buyMaterials(id, workCell, item, store - stockAll(*account(till), item), true);
    // A food maker's materials (doc 64, Phase 13; the model's lay_in): once a day, whatever its shelves hold, laid in
    // to days of their use (inputStore), so a Restday or a short caravan doesn't empty its kitchen the day after; what
    // is over its batches' store only with what its till can spare, never on credit.
    if (auto& laid = laidIn_.try_emplace(till, -1).first->second; laid != state_.budgetDay)
    {
        laid = state_.budgetDay;
        for (const auto* k : crafts)
            for (const auto& [item, count] : k->in)
                if (const int store = inputStore(till, *k, item, count * MaterialBatches), held = stockAll(*account(till), item);
                    store > count * MaterialBatches && held < store / 2)
                    // (A seller's 99 at a time, as long as some come: doc 64, Phase 13. What it makes too, it buys as a
                    // supplier does, as for its batches below, and not back from the town's shops: the cord loop.)
                    for (int left = store + fresh(*k, item) - held, got = 1; left > 0 && got > 0; left -= got)
                        got = buyMaterials(id, workCell, item, left, supplied(item) || madeHere(item), false);
    }
    // The keeper's own time at work since it last looked (doc 64, Phase 3): a gap over two game hours is a new day's
    // work begun, not work done. The skilled get more done in an hour (workPace), and so do improved premises and a
    // player's paw lent; it goes to the business's work in hand, beside its help's spells (RatwResidents.cpp).
    double worked = 0;
    if (const auto last = lastAtWork_.find(id); last != lastAtWork_.end())
        if (const double since = (absoluteDay - last->second) * 24; since > 0 && since <= 2)
            worked = since;
    lastAtWork_[id] = absoluteDay;
    if (const auto* job = jobOf(id); job && worked > 0)
        worked /= workPace(skill(id, job->id), job->level) * (1 - ImprovementPace * improvement(job->id));
    if (const auto paw = pawLent_.find(id); paw != pawLent_.end() && paw->second.second >= absoluteDay)
        worked *= std::max(1., paw->second.first);   // (A player's paw at the work: doc 53, 3.)
    // A shopkeeper who also brings goods in (a stables' stock-breeding) does so with its time; one who makes things,
    // its time goes to the business's work in hand.
    if (items::producerFor(r->workLabel))
        produce(id, absoluteDay, worked);
    else
        creditWork(till, worked);
    double wait = 1. / 96;                          // Nothing to make: look again in a quarter of an hour.
    // What sells fastest first (and what was asked for and missed), so scarce materials (iron) go where they're wanted,
    // not into goods that sit on the shelf; otherwise in file order (doc 42, "Pressure").
    auto order = crafts;
    if (const auto rates = sellRate_.find(till); rates != sellRate_.end())
            std::stable_sort(order.begin(), order.end(), [&](const items::Craft* a, const items::Craft* b) {
                const auto rate = [&](const items::Craft* k) {
                    const auto sold = rates->second.find(k->out.front().first);
                    return sold == rates->second.end() ? 0. : sold->second;
                };
                return rate(a) > rate(b);
            });
    for (const auto* k : order)
    {
        // Low, judged by what the batch is for (its first good; a butcher's meat, not the hides that come with it):
        // under the few a shop keeps, or, for what other trades work with (a mill's flour), under half a supplier's store.
        // (What other trades work with and keeps (a mill's flour, not a baker's bread): a full store, so there is some to
        // spare for other towns' makers, doc 42, "Food between towns". Their standing orders take what is over half.)
        const auto& made = k->out.front().first;
        const auto* madeGood = items::good(made);
        const bool keeps = !rots(madeGood);
        // A shop curing food (what keeps longer than what it's made from) keeps half a store of it, not a few: more
        // fresh meat and fish cured before it spoils, more salt and firewood bought (doc 42, "Pressure").
        const bool cures = !k->in.empty() && madeGood && madeGood->nourish > 0 && [&] {
            const auto* in = items::good(k->in.front().first);
            return in && in->keeps > 0 && (madeGood->keeps <= 0 || madeGood->keeps > in->keeps);
        }();
        // A town's trade (doc 61): its makers keep a bigger store of its goods, the spare that goes out on the roads.
        const auto town = communityOfResident(id);
        const double trade = items::townGoodOf(town, made) ? items::tradeKeep() : 1;
        const double least = items::traded(made) && keeps ? SuppliesKept * trade
                             : supplied(made) || items::traded(made) || cures ? SuppliesKept / 2 * trade : GoodsKept;
        // Its store follows its sales (doc 64, Phase 3b): what it sells in its days' keeping, and its share of a week of
        // what the roads' standing orders take of it from this town (doc 61: kept at its usual store, a smelter had 60
        // bars to send whatever was ordered of it); the old store a floor.
        const int kept = shelfTarget(till, made, int(least));
        const bool low = stockAll(*account(till), made) < kept;
        if (!low)
            continue;
        // Materials for the batches that fill it, MaterialBatches at least. A seasonal good (grapes, apples) is laid in
        // at the harvest, a store to last the year: SeasonalStore times as much.
        const int perBatch = std::max(1, k->out.front().second);
        const int batches = std::max(MaterialBatches, (kept + perBatch - 1) / perBatch);
        for (const auto& [item, count] : k->in)
        {
            const int store = count * batches * (items::seasonal(item) ? SeasonalStore : 1) + fresh(*k, item);
            // (What it sells as a supplier too, a fishmonger's fish, or makes itself, a chandler's cord, it restocks as a
            // supplier does, from the makers and the land, never from the town's other suppliers: they'd only buy it back,
            // round and round.)
            if (const int held = stockAll(*account(till), item); held < count * MaterialsLow || (store > count * MaterialBatches && held < store / 2))
                buyMaterials(id, workCell, item, store - held, supplied(item) || madeHere(item));
        }
        if (!has(*k))
        {
            shortToday_[till].insert(k->id);        // (Its hands wanted by what it could make: doc 64, Phase 13.)
            continue;                               // Short of materials, and nobody sells them: something else.
        }
        // A batch takes its crafts.json `hours` of the business's work in hand (doc 64, Phase 3): begun when there are
        // enough, and on the shelf at the next look; until then the hands work on, and it waits for them.
        auto& banked = workBanked_[till];
        if (banked + 1e-9 >= hoursFor(*k))
        {
            banked -= hoursFor(*k);
            craftAtWork_[id] = k->id;
            wait = 1. / 1440;                       // (Done at the next look, a minute on.)
        }
        else                                        // Look again when the keeper's own time would cover it (the help's
            wait = std::clamp((hoursFor(*k) - banked) / 24, 1. / 1440, 1. / 24);   // may bring it sooner), within the hour.
        break;
    }
    craftNext_[id] = absoluteDay + wait;
}

void Society::lendGift(const std::string& maker, double lift, double untilDay)
{
    auto& lent = giftLift_[maker];
    lent.first = std::min(30.0, (lent.second >= untilDay - 1 ? lent.first : 0) + lift);   // (Several Gifts help, to a point.)
    lent.second = untilDay;
}

void Society::lendPaw(const std::string& maker, double rate, double untilDay)
{
    pawLent_[maker] = {rate, untilDay};
}

double Society::giftLiftOf(const std::string& maker) const
{
    const auto lent = giftLift_.find(maker);
    return lent == giftLift_.end() ? 0 : lent->second.first;
}
} // namespace ratw
