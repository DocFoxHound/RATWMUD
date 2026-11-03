// The city's loop (Docs/Design/68-the-country-and-the-city-loop.md): residents buy their food, needs and comforts from the
// shops; the shops restock from the store room, paying the Country (less the gate toll, the treasury's) and the carters;
// the Country spends what it takes in on the city's goods; tills, the treasury and the Country pay their posts' wages.
// Every coin moves by shift or sale, so the money is conserved; goods come only from the store room and leave by being
// eaten, used up at home or carried away to the Country.
#include "RatwCalendar.h"
#include "RatwItems.h"
#include "RatwSociety.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <functional>
#include <limits>
#include <map>
#include <set>

namespace ratw
{
namespace
{
std::uint64_t fnv(const std::string& text, std::uint64_t h = 1469598103934665603ULL)
{
    for (const unsigned char c : text)
        h = (h ^ c) * 1099511628211ULL;
    return h;
}
std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return s;
}
// What an account holds to eat, in nourishment.
double onHand(const EconomyAccount& a)
{
    double n = 0;
    for (const auto& [item, count] : a.stock)
        if (count > 0)
            n += double(Society::nourishment(item)) * count;
    return n;
}
// One of `weights` picked by `roll` (0..1), in proportion; -1 if they are all 0.
int weighted(const std::vector<double>& weights, double roll)
{
    double total = 0;
    for (const double w : weights)
        total += std::max(0., w);
    if (total <= 0)
        return -1;
    double at = roll * total;
    for (std::size_t i = 0; i < weights.size(); ++i)
    {
        if (weights[i] <= 0)
            continue;
        if (at < weights[i])
            return int(i);
        at -= weights[i];
    }
    for (std::size_t i = weights.size(); i-- > 0;)
        if (weights[i] > 0)
            return int(i);
    return -1;
}
double roll(const std::string& key)
{
    return double(fnv(key) % 1000003ULL) / 1000003.;
}
// Whether a till's wage step lowered a penny lowers some post (the third self-correcting rule): a keeper's within
// wageDriftMost, a hand's within wageDriftHandMost, neither under wageDriftFloor.
bool wageFalls(const Society& s, int step)
{
    const auto& e = s.authored().economy;
    const int floor = s.rule("wageDriftFloor");
    const auto falls = [&](int wage, const char* most) { return step > -s.rule(most) && wage + step - 1 >= floor; };
    return falls(e.wageKeeper, "wageDriftMost") || falls(e.wageHand, "wageDriftHandMost");
}
} // namespace

std::string Society::marketFor(const std::string& item) const
{
    (void)item;                                     // Every market supplies every good for now: the first takes it.
    return items::country().markets.front().id;
}

Society::Pay Society::payOf(const Position& p) const
{
    const auto& e = authored_.economy;
    // A till's posts (its keeper and hands) at its wage step (doc 68, Progress, "Self-correcting rules"); the treasury's
    // and the Country's posts keep their fixed wages.
    if (p.role == "merchant")
        return {"keeper", "till:" + p.id, driftedWage("till:" + p.id, e.wageKeeper)};
    const auto title = lower(p.title);
    // A rule may pay another of the world's wages than its payer's (a head's post: a keeper's; doc 68, "First fixes").
    const auto wageOf = [&](const std::string& kind, int otherwise) {
        return kind == "keeper" ? e.wageKeeper : kind == "hand" ? e.wageHand : kind == "public" ? e.wagePublic
               : kind == "country" ? e.wageCountry : otherwise;
    };
    if (p.role == "guard")
    {
        // The watch: the treasury's, at a public wage, but for its head (a treasury rule that names a wage: "commanding
        // the watch", doc 68, Progress, "Trades").
        for (const auto& rule : items::country().wages)
            if (rule.payer == "treasury" && !rule.wage.empty())
                for (const auto& word : rule.words)
                    if (title.find(word) != std::string::npos)
                        return {"watch", "treasury", wageOf(rule.wage, e.wagePublic)};
        return {"watch", "treasury", e.wagePublic};
    }
    // A hand: a post whose work room holds exactly one shop.
    int shops = 0;
    const Position* shop = nullptr;
    for (const auto& q : positions_)
        if (q.role == "merchant" && q.work.cell == p.work.cell)
            ++shops, shop = &q;
    if (shops == 1)
        return {"hand", "till:" + shop->id, driftedWage("till:" + shop->id, e.wageHand, true)};
    for (const auto& rule : items::country().wages)
        for (const auto& word : rule.words)
            if (title.find(word) != std::string::npos)
                return rule.payer == "treasury" ? Pay{"public", "treasury", wageOf(rule.wage, e.wagePublic)}
                                                : Pay{"country", marketFund(rule.payer), wageOf(rule.wage, e.wageCountry)};
    return {};
}

int Society::crewOf(const std::string& till) const
{
    if (crewsRevision_ != rosterRevision_ || crews_.empty())
    {
        crews_.clear();
        for (const auto& p : positions_)
        {
            const auto held = state_.careers.positions.find(p.id);
            if (held == state_.careers.positions.end() || held->second.holder.empty())
                continue;
            const auto pay = payOf(p);
            if (pay.kind == "keeper" || pay.kind == "hand")
                ++crews_[pay.payer];
        }
        crews_[""] = 0;                             // (Built, even where there are no shops.)
        crewsRevision_ = rosterRevision_;
    }
    const auto found = crews_.find(till);
    return found == crews_.end() ? 1 : std::max(1, found->second);
}

std::string Society::carters() const
{
    std::string best;
    for (const auto& p : positions_)
    {
        if (p.role != "merchant")
            continue;
        const auto* business = items::businessFor(p.title);
        const auto till = "till:" + p.id;
        if (business && business->id == "carters" && account(till) && (best.empty() || cash(till) < cash(best)))
            best = till;
    }
    return best.empty() ? std::string(CarriageFund) : best;
}

std::int64_t Society::carriageOn(const std::string& payer, const std::string& item, int quantity, bool commit, double share)
{
    const int rate = std::clamp(authored_.economy.carriage, 0, 1000);
    const auto* good = items::good(items::baseOf(item));
    if (rate == 0 || !good || quantity < 1 || share <= 0)
        return 0;
    const double owed = (carriageOwed_.count(payer) ? carriageOwed_[payer] : 0.) + good->weight * quantity * rate * share / CarriageLoad;
    const auto whole = std::int64_t(std::floor(owed + 1e-9));
    if (commit)
        carriageOwed_[payer] = owed - double(whole);
    return whole;
}

std::int64_t Society::gateToll(const std::string& till, std::int64_t cost, bool commit)
{
    // The gate toll on a draw's cost, its fraction of a penny kept against the till until it makes one.
    const double owed = (tollOwed_.count(till) ? tollOwed_[till] : 0.) + double(cost) * gateTollNow() / 100.;
    const auto whole = std::min(cost, std::int64_t(std::floor(owed + 1e-9)));
    if (commit)
        tollOwed_[till] = std::min(1., owed - double(whole));
    return whole;
}

int Society::subsidyShare(const std::string& service) const
{
    const auto found = authored_.economy.subsidies.find(service);
    return found == authored_.economy.subsidies.end() ? 0 : std::clamp(found->second, 0, 100);
}

std::int64_t Society::subsidyRoom()
{
    if (treasuryOwed_ < 0 || subsidyLeft_ < 0)
    {
        treasuryOwed_ = 0;
        for (const auto& p : positions_)
            if (const auto held = state_.careers.positions.find(p.id);
                held != state_.careers.positions.end() && !held->second.holder.empty())
                if (const auto pay = payOf(p); pay.payer == "treasury")
                    treasuryOwed_ += pay.wage;
        if (subsidyLeft_ < 0)
        {
            subsidyLeft_ = std::int64_t(std::floor(double(cash("treasury")) * SubsidyDay));
            // Only what the carters lack (the rule subsidyToNeed; doc 68, "The follow-ups"): their crew's wages tonight
            // and their float, less what their till holds when the day's first carriage is reckoned; nothing to their
            // keeper's profit.
            if (rule("subsidyToNeed") > 0)
            {
                const auto carter = carters();
                std::int64_t owed = 0;
                if (carter != CarriageFund)
                    for (const auto& p : positions_)
                        if (const auto held = state_.careers.positions.find(p.id);
                            held != state_.careers.positions.end() && !held->second.holder.empty())
                            if (const auto pay = payOf(p); pay.payer == carter)
                                owed += pay.wage;
                const auto lack = owed > 0 ? std::max<std::int64_t>(0, owed + tillFloat(carter) - cash(carter)) : 0;
                subsidyLeft_ = std::min(subsidyLeft_, lack);
            }
        }
    }
    return std::max<std::int64_t>(0, std::min(subsidyLeft_, cash("treasury") - TreasuryFloat - treasuryOwed_));
}

std::pair<std::int64_t, std::int64_t> Society::carriageSplit(const std::string& payer, const std::string& item, int quantity, bool commit)
{
    // The treasury's share (doc 68, 5), while it has room today; else the payer pays it all.
    const double share = subsidyShare("carriage") / 100.;
    if (share > 0)
    {
        const auto city = carriageOn("subsidy:carriage", item, quantity, false, share);
        if (city <= subsidyRoom())
        {
            const auto own = carriageOn(payer, item, quantity, commit, 1 - share);
            if (commit)
            {
                carriageOn("subsidy:carriage", item, quantity, true, share);
                subsidyLeft_ -= city;
            }
            return {own, city};
        }
    }
    return {carriageOn(payer, item, quantity, commit), 0};
}

int Society::shelfOf(const std::string& till, const std::string& ware) const
{
    int shelf = startingStock(ware);
    if (rule("shelfFollow") > 0)
    {
        // Shelves follow sales (doc 68, Progress, "Self-correcting rules"): shelfCover percent of the ware's smoothed sales
        // a day, never under its starting stock.
        if (const auto t = state_.loop.sales.find(till); t != state_.loop.sales.end())
            if (const auto w = t->second.find(ware); w != t->second.end())
                shelf = std::max(shelf, int(std::min<double>(StoreDrawMost, std::ceil(w->second * rule("shelfCover") / 100. - 1e-9))));
    }
    else if (const auto t = shelf_.find(till); t != shelf_.end())
        if (const auto w = t->second.find(ware); w != t->second.end())
            shelf = std::max(shelf, w->second);
    // A season's shelf (a maker stocking up ahead of a lean season): the shelf times its group's factor.
    if (const double f = shelfFactor(ware); f != 1)
        shelf = std::clamp(int(std::ceil(shelf * f - 1e-9)), 1, StoreDrawMost);
    return shelf;
}

// The seasons in the Country's supply (doc 68, Progress, "Seasons and the DM's rules").
void Society::buildSeasons()
{
    seasons_ = items::country().seasons;
    for (auto& g : seasons_)
        if (const auto own = authored_.economy.seasons.find(g.id); own != authored_.economy.seasons.end())
        {
            g.supply = own->second.supply;
            g.price = own->second.price;
            g.shelf = own->second.shelf;
            g.normal = own->second.normal;
        }
    seasonOf_.clear();
    for (std::size_t i = 0; i < seasons_.size(); ++i)
        for (const auto& item : seasons_[i].goods)
            seasonOf_.emplace(item, i);
    // The loop's rules and the city's services as this world has them (doc 68, Progress, "First fixes").
    rules_.clear();
    for (const auto& info : items::Rules)
    {
        int value = info.fallback;
        if (const auto c = items::country().rules.find(info.name); c != items::country().rules.end())
            value = c->second;
        if (const auto own = authored_.economy.rules.find(info.name); own != authored_.economy.rules.end())
            value = own->second;
        rules_[info.name] = std::clamp(value, info.low, info.high);
    }
    services_.clear();
    for (const auto& def : items::country().services)
    {
        ServiceNow s{def.id, def.business, true, catalogPrice(def.id), def.every, def.patron};
        if (const auto own = authored_.economy.services.find(def.id); own != authored_.economy.services.end())
        {
            s.on = own->second.on;
            s.price = std::max(1, own->second.price);
            s.every = std::max(1, own->second.every);
        }
        services_.push_back(std::move(s));
    }
    buildFarms();                                   // (The Country's food farms as this world has them: doc 71.)
}

int Society::rule(const std::string& name) const
{
    if (const auto found = rules_.find(name); found != rules_.end())
        return found->second;
    const auto* info = items::ruleInfo(name);
    if (!info)
        return -1;
    const auto c = items::country().rules.find(name);
    return c != items::country().rules.end() ? c->second : info->fallback;
}

double Society::workBankMost(const std::string& till, const items::Business* business) const
{
    const int crew = crewOf(till);
    if (const int hours = rule("workBankHours"); hours > 0)
        return double(hours) * crew;
    // The business's longest craft and an hour of the crew more (at least two hours' worth, as it always was): so a
    // craft longer than two hours of its crew can be made over two days.
    double most = 2. * crew;
    if (business)
        for (const auto* k : items::craftsFor(business->id))
            most = std::max(most, k->hours + crew);
    return most;
}

const items::SeasonGroup* Society::seasonGroup(const std::string& item) const
{
    const auto found = seasonOf_.find(items::baseOf(item));
    return found == seasonOf_.end() ? nullptr : &seasons_[found->second];
}

void Society::setYearDay(double absoluteDay)
{
    if (!std::isfinite(absoluteDay) || absoluteDay < 0)
        return;
    const auto day = std::int64_t(std::floor(absoluteDay));
    if (day != seasonDay_)
    {
        seasonDay_ = day;
        seasonDrawn_.clear();                       // (A new day: the Country's supply starts again.)
    }
    yearDay_ = std::fmod(absoluteDay, double(calendar::DaysPerYear));
}

double Society::seasonal(const std::array<int, 4>& by, double yearDay)
{
    if (yearDay < 0)
        return 1;
    return seasonalValue({double(by[0]), double(by[1]), double(by[2]), double(by[3])}, yearDay) / 100.;
}

double Society::seasonalValue(const std::array<double, 4>& by, double yearDay)
{
    // Each season's value stands at its middle day (Spring 92 days, Summer 92, Autumn 91, Winter 90: RatwCalendar.h);
    // between two middles, a straight line; Winter's middle runs on to Spring's across the year's turn.
    static constexpr double Middle[4] = {46, 92 + 46, 184 + 45.5, 275 + 45};
    if (yearDay < 0)
        return (by[0] + by[1] + by[2] + by[3]) / 4;    // (Before the date is known: the year's mean.)
    const double d = std::fmod(yearDay, double(calendar::DaysPerYear));
    int a = 3;
    for (int i = 0; i < 4; ++i)
        if (d >= Middle[i])
            a = i;
    const int b = (a + 1) % 4;
    const double from = Middle[a], to = b == 0 ? Middle[0] + calendar::DaysPerYear : Middle[b];
    const double at = d < from ? d + calendar::DaysPerYear : d;
    const double t = std::clamp((at - from) / (to - from), 0., 1.);
    return by[std::size_t(a)] + (by[std::size_t(b)] - by[std::size_t(a)]) * t;
}

double Society::priceFactor(const std::string& item) const
{
    const auto* g = yearDay_ < 0 ? nullptr : seasonGroup(item);
    return g ? seasonal(g->price, yearDay_) : 1.;
}

double Society::shelfFactor(const std::string& item) const
{
    const auto* g = yearDay_ < 0 ? nullptr : seasonGroup(item);
    return g ? seasonal(g->shelf, yearDay_) : 1.;
}

double Society::supplyFactor(const items::SeasonGroup& group) const
{
    return yearDay_ < 0 ? 1. : seasonal(group.supply, yearDay_);
}

int Society::groupCap(const items::SeasonGroup& group) const
{
    if (yearDay_ < 0 || group.normal <= 0)
        return std::numeric_limits<int>::max();
    return int(std::floor(group.normal * supplyFactor(group) + 1e-9));
}

int Society::groupDrawn(const std::string& group) const
{
    const auto found = seasonDrawn_.find(group);
    return found == seasonDrawn_.end() ? 0 : found->second;
}

int Society::countryLeft(const std::string& item) const
{
    // A good of an area the farms have taken over: what they hold (doc 71, 4: their stock, not the season's cap).
    if (farmAreaOf(item) >= 0)
        return farmsHold(item);
    const auto* g = seasonGroup(item);
    if (!g || yearDay_ < 0 || g->normal <= 0)
        return std::numeric_limits<int>::max();
    return std::max(0, groupCap(*g) - groupDrawn(g->id));
}

std::int64_t Society::seasonalCost(const std::string& item, int quantity) const
{
    const auto each = catalogPrice(item);
    if (quantity < 1)
        return 0;
    const double f = priceFactor(item);
    if (f == 1)
        return each * quantity;
    // (The most it may come to: the catalogue's price is paid first and the difference after, its fractions carried.)
    return std::max<std::int64_t>(each * quantity, std::int64_t(std::ceil(double(each) * quantity * f - 1e-9)));
}

int Society::comfortsAbove() const
{
    const int own = authored_.economy.comfortsAbove;
    return own >= 0 ? own : items::country().comfortsAbove;
}

double Society::comfortsShare() const
{
    const int own = authored_.economy.comfortsShare;
    return own >= 0 ? std::clamp(own, 0, 100) / 100. : items::country().comfortsShare;
}

void Society::adoptEconomy(const EconomySpec& economy)
{
    auto next = economy;
    const auto& now = authored_.economy;
    next.treasury = now.treasury;                   // (Starting amounts: spent long ago.)
    next.storeHerbs = now.storeHerbs;
    next.storeMeals = now.storeMeals;
    authored_.economy = std::move(next);
    buildSeasons();
    storeCache_.clear();                            // (What each keeper may draw follows the world's switches.)
    crews_.clear();
    treasuryOwed_ = -1;                             // (Tonight's public wages, at the new rates; today's subsidy left stands.)
    refreshAreas(-1);                               // (An area switched on now: its farms founded at once, doc 71.)
}

void Society::loopHourly(std::int64_t hour, int season)
{
    const int hod = int(hour % 24);
    const auto day = hour / 24;
    if (loopToday_.day < 0)
        loopToday_.day = day;
    if (hod == 0)
        turnShelves();
    loopToday_.day = day;
    if (day != farmsDay_)
        tendFarms(day);                             // (The farms' day: the areas on, the growing, the table: doc 71.)
    if (hod >= ShopOpen && hod < ShopShut)
    {
        householdsShop(hod, day, season);           // (The farms' crews among the households.)
        patronsBuy(hod, day);                       // (Before the Country: the patrons' orders come first.)
        if (authored_.economy.country)
            countryBuys(hod);
        farmsBuy(hod);                              // (The farms' baskets, as the Country's.)
    }
    if (hod == PayHour)
    {
        payWages();
        payFarms();                                 // (The farms' crews; short, the wage support as a till's.)
        supportWages();                             // (What the treasury holds over its cap, to the short tills' posts.)
        giveAlms();                                 // (After the day's wages: the treasury's posts come first.)
        spendSurplus();                             // (Last: what the treasury still holds over its cap and reserve.)
        loopToday_.treasuryNight = cash("treasury");
        followTreasury(day);                        // (Its level tonight moves the toll and the alms, slowly.)
    }
    // The least the treasury held at an hour's end today (doc 71, 4, the treasury's gate).
    if (const auto t = cash("treasury"); loopToday_.treasuryLow < 0 || t < loopToday_.treasuryLow)
        loopToday_.treasuryLow = t;
}

void Society::turnShelves()
{
    // A ware's shelf is a quarter more than what sold of it yesterday (doc 68, 7). With buying ahead (doc 68, "The
    // follow-ups"), a Restday's or festival's few sales don't empty the next day's shelves (households ate what they
    // bought ahead and shop as usual again): after one, the larger of its sales and the day before's.
    const auto ended = loopToday_.day;
    const bool rested = ended >= 0 && rule("buyAheadDays") > 0 &&
                        (calendar::weekdayOf(double(ended) + .5) == calendar::Restday || calendar::festivalDay(double(ended) + .5));
    if (rested)
        for (const auto& [till, sold] : soldBefore_)
            for (const auto& [ware, n] : sold)
                if (auto& now = soldToday_[till][ware]; now < n)
                    now = n;
    // The self-correcting rules' shelves and prices (doc 68, Progress, "Self-correcting rules"), from the day's sales.
    if (ended >= 0)
        followStock(calendar::weekdayOf(double(ended) + .5) == calendar::Restday || calendar::festivalDay(double(ended) + .5));
    soldOut_.clear();
    shelf_.clear();
    for (const auto& [till, sold] : soldToday_)
        for (const auto& [ware, n] : sold)
            if (const int shelf = std::min(StoreDrawMost, int(std::ceil(n * 1.25))); shelf > startingStock(ware))
                shelf_[till][ware] = shelf;
    soldBefore_ = std::move(soldToday_);
    soldToday_.clear();
    subsidyLeft_ = treasuryOwed_ = -1;              // (Today's subsidy: a twentieth of the treasury, worked out when first asked.)
    // Each till's draws a day, the last 28 (the float rule's mean; a till that drew nothing today, a 0).
    for (const auto& [till, n] : drawnToday_)
        drawnDays_[till];
    for (auto& [till, days] : drawnDays_)
    {
        const auto found = drawnToday_.find(till);
        days.push_back(found == drawnToday_.end() ? 0 : found->second);
        if (days.size() > 28)
            days.erase(days.begin());
    }
    drawnYesterday_ = std::move(drawnToday_);
    drawnToday_.clear();
    crews_.clear();                                 // (Who holds the posts may have changed.)
    loopYesterday_ = loopToday_;
    loopToday_ = {};
}

namespace
{
// The shops open this hour (their keeper keeping shop), what they hold, and their communities.
struct OpenShop
{
    std::string till, community;
    const items::Business* business = nullptr;
    const EconomyAccount* account = nullptr;        // The till's (no account is opened or closed while households shop).
};
} // namespace

std::map<std::string, std::vector<std::string>> Society::households() const
{
    // A household: the residents sharing a home (in a communal home, each alone), with a child under 16 and a spouse who
    // live elsewhere (a child given a room of its own upstairs): whoever earns feeds the family.
    std::map<std::string, std::string> keyOf;          // Resident -> its home's key.
    for (const auto& [id, life] : state_.residents)
        if (spec(id) && !dead_.count(id) && !life.homeCell.empty())
            keyOf[id] = communalHome(life.homeCell) ? life.homeCell + "|" + id : life.homeCell;
    std::map<std::string, std::string> joined;         // Home key -> the key it shops with (union by kin).
    const std::function<std::string(const std::string&)> root = [&](const std::string& k) {
        const auto found = joined.find(k);
        return found == joined.end() || found->second == k ? k : (joined[k] = root(found->second));
    };
    const auto join = [&](const std::string& a, const std::string& b) {
        const auto ka = keyOf.find(a), kb = keyOf.find(b);
        if (ka == keyOf.end() || kb == keyOf.end())
            return;
        const auto ra = root(ka->second), rb = root(kb->second);
        if (ra != rb)
            joined[std::max(ra, rb)] = std::min(ra, rb);
    };
    for (const auto& [child, parents] : state_.careers.parents)
        if (const auto* c = spec(child); c && c->age < 16)
            for (const auto& parent : parents)
                join(child, parent);
    for (const auto& [a, b] : state_.careers.spouses)
        join(a, b);
    std::map<std::string, std::vector<std::string>> homes;
    for (const auto& [id, key] : keyOf)
        homes[root(key)].push_back(id);
    return homes;
}

void Society::householdsShop(int hod, std::int64_t day, int season)
{
    const auto& cfg = items::country();
    std::vector<OpenShop> open;
    for (const auto& p : positions_)
    {
        if (p.role != "merchant")
            continue;
        const auto held = state_.careers.positions.find(p.id);
        if (held == state_.careers.positions.end() || held->second.holder.empty())
            continue;
        const auto* life = resident(held->second.holder);
        const auto till = "till:" + p.id;
        const auto* tillAccount = life && life->task == "trade" ? account(till) : nullptr;
        if (!tillAccount)
            continue;
        open.push_back({till, day_.communityOf ? day_.communityOf(p.work.cell) : std::string(), items::businessFor(p.title), tillAccount});
    }
    if (open.empty())
        return;
    const auto homes = households();
    // The shops a household buys from: its community's open ones, or every open one where its community has none.
    std::map<std::string, std::vector<const OpenShop*>> byCommunity;
    for (const auto& s : open)
        byCommunity[s.community].push_back(&s);
    std::vector<const OpenShop*> all;
    for (const auto& s : open)
        all.push_back(&s);
    // Who holds a good: the household's own community's open shops, or, where none of them has it, every open shop.
    const auto sellersOf = [&](const std::vector<const OpenShop*>& shops, const std::string& item, std::vector<const OpenShop*>& out,
                               std::vector<double>& weights) {
        for (const std::vector<const OpenShop*>* list : {&shops, static_cast<const std::vector<const OpenShop*>*>(&all)})
        {
            out.clear();
            weights.clear();
            for (const auto* s : *list)
                if (const int n = stock(*s->account, item); n > 0)
                {
                    out.push_back(s);
                    weights.push_back(n);
                }
            if (!out.empty())
                return;
        }
    };
    std::vector<const OpenShop*> tills;
    std::vector<double> weights;
    // A farm's crew shopping now (doc 71, 4: a household of its farm's crew), whose day is counted apart from the
    // residents' (null: residents).
    LoopDay::Farm* crewDay = nullptr;
    // Pays from whichever member has the most and can pay (keeping `keep` back across the household), or "" for none.
    const auto payerFor = [&](const std::vector<std::string>& members, std::int64_t price, std::int64_t keep) {
        std::int64_t held = 0;
        for (const auto& m : members)
            held += cash(m);
        const std::string* best = nullptr;
        for (const auto& m : members)
            if (cash(m) >= price && (!best || cash(m) > cash(*best)))
                best = &m;
        return best && held - price >= keep ? *best : std::string();
    };
    // Buys `want` of `item` from the shops (sellers by stock), paid by the household; returns how many came.
    // What a lot costs a resident: the catalogue's price, times the season's for a good of a season group (doc 68,
    // Progress, "Seasons"), in whole pennies for the lot; paid as a sale at its whole pennies a piece and the odd pennies.
    const auto buy = [&](const std::vector<const OpenShop*>& shops, const std::vector<std::string>& members, const std::string& to,
                         const std::string& item, int want, std::int64_t keep, const std::string& key) {
        int got = 0;
        const auto* group = seasonGroup(item);
        for (int lot = 0; lot < 4 && got < want; ++lot)
        {
            sellersOf(shops, item, tills, weights);
            const int pick = weighted(weights, roll(key + "|seller|" + std::to_string(lot)));
            if (pick < 0)
                break;
            const auto& till = tills[std::size_t(pick)]->till;
            int n = std::min({want - got, stock(*tills[std::size_t(pick)]->account, item), 99});
            std::string payer;
            while (n > 0 && (payer = payerFor(members, shopCost(till, item, n), keep)).empty())
                n = payerFor(members, shopCost(till, item, 1), keep).empty() ? 0 : std::max(1, n / 2);
            if (n < 1)
                break;
            // At the catalogue's price, then the season's difference (and the shop's, from its stock: doc 68, Progress,
            // "Self-correcting rules"): the payer's fractions of a penny kept against it until they make one (as the gate
            // toll's are), so a penny's good costs its season's price over many buys.
            const auto base = catalogPrice(item) * n;
            if (!sale(till, payer, item, n, catalogPrice(item), "market"))
                break;
            auto cost = base;
            if (const double f = priceFactor(item) * stockFactor(till, item); f != 1)
            {
                double& carry = priceCarry_.try_emplace(payer, .5).first->second;   // (From half a penny: fair rounding.)
                const double owed = carry + double(base) * f;
                cost = std::max<std::int64_t>(0, std::int64_t(std::floor(owed + 1e-9)));
                if (cost > base && !shift(payer, till, "", 0, std::min(cost - base, cash(payer)), "market: the season's price"))
                    cost = base;
                else if (cost < base)
                    shift(till, payer, "", 0, base - cost, "market: the season's price");
                carry = std::clamp(owed - double(cost), 0., 1.);
            }
            if (payer != to && !shift(payer, to, item, n, 0, "for the household"))
                consume(payer, item, n, "used at home");   // (Not reached: the member's purse has room.)
            got += n;
            if (crewDay)
                crewDay->crewSpent += cost;
            else
            {
                loopToday_.residentsSpent += cost;
                if (group)
                {
                    auto& g = loopToday_.groups[group->id];
                    g.sold += n;
                    g.spent += cost;
                }
            }
        }
        return got;
    };
    const auto& households = items::households();
    const bool fairFood = rule("fairFood") > 0;
    const bool stockPricing = rule("stockPrices") > 0;
    const int comfortsMost = rule("comfortsMost");
    const bool servicesOn = rule("services") > 0 && !services_.empty();
    const int cityShare = rule("servicesCityShare");
    // A city service done for a household (doc 68, "First fixes"): by an open shop of its business (the household's
    // community's, else any open one; one picked by a fixed roll), paid by the member with most, done at once (made,
    // sold and used up: nothing kept); the city's share of its price from the till to the treasury, the fraction of a
    // penny kept against the till until it makes one. False if there is no such shop or nobody can pay.
    const auto doService = [&](const std::vector<const OpenShop*>& shops, const std::vector<std::string>& members, const ServiceNow& s,
                               const std::string& key) {
        std::vector<const OpenShop*> by;
        for (const std::vector<const OpenShop*>* list : {&shops, static_cast<const std::vector<const OpenShop*>*>(&all)})
        {
            for (const auto* o : *list)
                if (o->business && o->business->id == s.business)
                    by.push_back(o);
            if (!by.empty())
                break;
        }
        if (by.empty())
            return false;
        const auto& till = by[std::size_t(roll(key + "|shop") * double(by.size())) % by.size()]->till;
        const auto payer = payerFor(members, s.price, 0);
        if (payer.empty() || !create(till, s.id, 1, "a service done"))
            return false;
        if (!sale(till, payer, s.id, 1, s.price, "a service"))
        {
            consume(till, s.id, 1, "a service not done");
            return false;
        }
        consume(payer, s.id, 1, "a service, done at once");
        auto& owed = serviceOwed_[till];
        owed += double(s.price) * cityShare / 100.;
        const auto city = std::min<std::int64_t>(s.price, std::int64_t(std::floor(owed + 1e-9)));
        if (city > 0 && account("treasury") && shift(till, "treasury", "", 0, city, "the city's share of a service"))
        {
            owed -= double(city);
            loopToday_.servicesCity += city;
        }
        if (crewDay)
        {
            ++crewDay->crewServices;
            crewDay->crewSpent += s.price;
            return true;
        }
        ++loopToday_.servicesSold;
        loopToday_.servicesSpent += s.price;
        auto& trade = loopToday_.serviceTrades[s.business];
        ++trade.sold;
        trade.spent += s.price;
        return true;
    };
    // Buying ahead (doc 68, Progress, "The follow-ups"): the day before a Restday or a festival, when the shops keep short
    // hours or none, each household's market hour tops its members up to buyAheadDays days of food more.
    const auto tomorrow = double(day + 1) + .5;
    const bool restAhead = calendar::weekdayOf(tomorrow) == calendar::Restday || calendar::festivalDay(tomorrow);
    const double foodDays = cfg.foodDays + (restAhead ? rule("buyAheadDays") : 0);
    if (restAhead && foodDays > cfg.foodDays && hod == ShopOpen)
        ++loopToday_.boughtAhead;
    // A household's market: its food each hour from its market hour, its needs, services and comforts at that hour. A
    // farm's crew (doc 71, 4) is a household of `heads` wolves in one account (`per` of them to its one member), buying
    // at every open shop, its day counted apart (crewDay).
    const auto shopFor = [&](const std::string& home, const std::vector<std::string>& members, int heads, bool crew) {
        const int marketHour = ShopOpen + int(fnv(home + "|market") % std::uint64_t(ShopShut - ShopOpen));
        if (hod < marketHour)
            return;
        const bool first = hod == marketHour;
        const double per = crew ? double(heads) : 1.;
        const auto community = crew || !day_.communityOf ? std::string() : day_.communityOf(state_.residents.at(members.front()).homeCell);
        const auto here = crew ? byCommunity.end() : byCommunity.find(community);
        const auto& shops = here != byCommunity.end() && !here->second.empty() ? here->second : all;
        // Food: every member topped up to foodDays; one under shortDays again each hour until the shops shut.
        bool shortHere = false;
        const bool fair = fairFood;
        // Tops `m` up to `target` nourishment (at most six buys). The food: among what these shops hold (under the fair
        // rule, only what some member can pay for), the better nourishment for the price the likelier.
        const auto topUp = [&](const std::string& m, double& have, double target, const std::string& tag) {
            for (int k = 0; k < 6 && have < target; ++k)
            {
                std::int64_t most = std::numeric_limits<std::int64_t>::max();
                if (fair)
                {
                    most = 0;
                    for (const auto& x : members)
                        most = std::max(most, cash(x));
                }
                std::vector<std::string> foods;
                std::vector<double> value, held, priced;   // (Pieces on offer, and pieces times their shop's stock factor.)
                for (const std::vector<const OpenShop*>* list : {&shops, static_cast<const std::vector<const OpenShop*>*>(&all)})
                {
                    for (const auto* s : *list)
                    {
                        const auto* factors = stockPricing ? priceFactors(s->till) : nullptr;
                        for (const auto& [item, n] : s->account->stock)
                        {
                            double f = 1.;
                            if (factors)
                                if (const auto own = factors->find(item); own != factors->end())
                                    f = own->second;
                            if (n <= 0 || !edible(item) || (fair && (f == 1 ? seasonalCost(item, 1) : shopCost(s->till, item, 1)) > most))
                                continue;
                            auto at = std::find(foods.begin(), foods.end(), item);
                            if (at == foods.end())
                            {
                                foods.push_back(item);
                                held.push_back(0);
                                priced.push_back(0);
                                at = foods.end() - 1;
                            }
                            const auto i = std::size_t(at - foods.begin());
                            held[i] += n;
                            priced[i] += n * f;
                        }
                    }
                    if (!foods.empty())
                        break;                      // (Its own community's food, where it has some; else anywhere open.)
                }
                // The better nourishment for the price the likelier: the season's price, times its shops' stock factors
                // (their mean, by what each holds; doc 68, Progress, "Self-correcting rules"), so cheaper food sells more.
                for (std::size_t i = 0; i < foods.size(); ++i)
                    value.push_back(double(nourishment(foods[i])) /
                                    (double(std::max<std::int64_t>(1, catalogPrice(foods[i]))) * priceFactor(foods[i]) * (priced[i] / held[i])));
                const auto key = m + "|" + std::to_string(day) + "|" + std::to_string(hod) + "|" + std::to_string(k) + tag;
                const int pick = weighted(value, roll(key));
                if (pick < 0)
                    break;
                const auto& food = foods[std::size_t(pick)];
                const int pieces = int(std::ceil((target - have) / std::max(1, nourishment(food))));
                const int got = buy(shops, members, m, food, std::max(1, pieces), 0, key);
                if (got < 1)
                    break;
                (crewDay ? crewDay->crewFood : loopToday_.foodBought) += got;
                have += double(got) * nourishment(food);
            }
        };
        if (!fair)
            for (const auto& m : members)
            {
                const auto* wallet = account(m);
                if (!wallet)
                    continue;
                double have = onHand(*wallet);
                if (!(first ? have < foodDays * NourishADay * per : have < cfg.shortDays * NourishADay * per))
                    continue;
                topUp(m, have, foodDays * NourishADay * per, "");
                shortHere = shortHere || have < cfg.shortDays * NourishADay * per;
            }
        else
        {
            // Fair food (doc 68, "First fixes"): the members with least food first, everyone to shortDays before anyone
            // to foodDays (that only at the household's market hour; later hours, only the short again).
            std::vector<std::pair<double, std::string>> order;
            for (const auto& m : members)
                if (const auto* wallet = account(m))
                    order.push_back({onHand(*wallet), m});
            std::stable_sort(order.begin(), order.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
            // (Buying ahead: everyone to foodDays before anyone to the days ahead, since each eats what it holds.)
            std::vector<double> targets{cfg.shortDays * NourishADay * per, cfg.foodDays * NourishADay * per};
            if (foodDays > cfg.foodDays)
                targets.push_back(foodDays * NourishADay * per);
            for (const double target : targets)
            {
                if (!first && target > cfg.shortDays * NourishADay * per)
                    break;
                for (const auto& [unused, m] : order)
                {
                    double have = onHand(*account(m));
                    topUp(m, have, target, "|" + std::to_string(int(target)));
                }
            }
            for (const auto& [unused, m] : order)
                shortHere = shortHere || onHand(*account(m)) < cfg.shortDays * NourishADay * per;
        }
        if (shortHere)
            ++(crewDay ? crewDay->crewShort : loopToday_.householdsShort);
        if (shortHere && shortageWatcher_ && hod == ShopShut - 1)
        {
            // (Recorded at the shops' last hour: the meals' worth its members still lack of shortDays.)
            double lack = 0;
            for (const auto& m : members)
                if (const auto* wallet = account(m))
                    lack += std::max(0., cfg.shortDays * NourishADay * per - onHand(*wallet));
            shortageWatcher_({members.front(), "", "food", "", std::max(1, int(std::ceil(lack / std::max(1, nourishment("meal")) - 1e-9)))});
        }
        if (!first)
            return;
        const std::int64_t keep = std::int64_t(households.reserve) * std::int64_t(heads);
        // Needs, when due (a fixed hash of the home and the day: nothing saved), used up at home.
        int grown = crew ? heads : 0;               // (A crew is all of working age.)
        for (const auto& m : members)
            grown += !crew && spec(m) && spec(m)->age >= 16;
        for (std::size_t i = 0; i < households.needs.size(); ++i)
        {
            const auto& need = households.needs[i];
            const double days = season == 3 && need.winterDays > 0 ? need.winterDays : need.everyDays;
            int count = days < 1 ? int(std::lround(1 / days))
                                 : (std::uint64_t(day) + fnv(home + "|need|" + std::to_string(i))) % std::uint64_t(std::max(1L, std::lround(days))) == 0;
            if (need.perPerson)
                count *= grown;
            if (count < 1)
                continue;
            std::vector<std::string> options;
            for (const auto& g : need.any)
            {
                sellersOf(shops, g, tills, weights);
                if (!tills.empty())
                    options.push_back(g);
            }
            if (options.empty() || (shortageWatcher_ && options.size() < need.any.size()))
            {
                // (Recorded: each of the need's goods no open shop holds, and what was taken in its place.)
                const std::string instead =
                    options.empty() ? std::string() : options[fnv(home + "|" + std::to_string(day) + "|" + std::to_string(i)) % options.size()];
                if (shortageWatcher_)
                    for (const auto& g : need.any)
                        if (std::find(options.begin(), options.end(), g) == options.end())
                            shortageWatcher_({members.front(), "", g, instead, count});
                if (options.empty())
                    continue;
            }
            const auto& item = options[fnv(home + "|" + std::to_string(day) + "|" + std::to_string(i)) % options.size()];
            const auto payer = payerFor(members, seasonalCost(item, count), keep);
            if (payer.empty())
                continue;
            if (const int got = buy(shops, members, payer, item, count, keep, home + "|need|" + std::to_string(day) + "|" + std::to_string(i)); got > 0)
            {
                consume(payer, item, got, "used at home");
                (crewDay ? crewDay->crewNeeds : loopToday_.needsBought) += got;
            }
        }
        // The day's comforts budget: a share of what is over `above` pennies a head (after food and needs).
        std::int64_t held = 0;
        for (const auto& m : members)
            held += cash(m);
        auto budget = std::int64_t(std::floor((held - std::int64_t(comfortsAbove()) * std::int64_t(heads)) * comfortsShare()));
        // The city's services first, each about once every `every` days (a fixed roll of the home and the day: nothing
        // saved), when the budget covers it (doc 68, "First fixes": only from money nothing else needs).
        if (servicesOn)
            for (const auto& s : services_)
            {
                if (budget <= 0)
                    break;
                const auto key = home + "|service|" + s.id + "|" + std::to_string(day);
                if (!s.on || s.patron || s.price > budget || roll(key) * double(s.every) >= 1.)
                    continue;
                if (doService(shops, members, s, key))
                    budget -= s.price;
            }
        // Comforts with the rest, used up at once: at most comfortsMost a day.
        for (int k = 0; k < comfortsMost && budget > 0 && !cfg.comforts.empty(); ++k)
        {
            std::vector<std::string> options;
            for (const auto& g : cfg.comforts)
                if (seasonalCost(g, 1) <= budget)
                {
                    sellersOf(shops, g, tills, weights);
                    if (!tills.empty())
                        options.push_back(g);
                }
            if (options.empty())
                break;
            const auto& item = options[fnv(home + "|comfort|" + std::to_string(day) + "|" + std::to_string(k)) % options.size()];
            const auto payer = payerFor(members, seasonalCost(item, 1), 0);
            if (payer.empty() || buy(shops, members, payer, item, 1, 0, home + "|comfort|" + std::to_string(day) + "|" + std::to_string(k)) < 1)
                break;
            consume(payer, item, 1, "used at home");
            budget -= seasonalCost(item, 1);
            ++(crewDay ? crewDay->crewComforts : loopToday_.comfortsBought);
        }
    };
    for (const auto& [home, members] : homes)
        shopFor(home, members, int(members.size()), false);
    // The farms' crews (doc 71, 4), while their area is on: their spending is how the farms' money returns to the city.
    for (const auto& f : farms_)
        if (farmAreaOn(f.area) && account(f.crew) && f.heads() > 0)
        {
            crewDay = &loopToday_.farms[f.def->id];
            shopFor(f.crew, {f.crew}, f.heads(), true);
            crewDay = nullptr;
        }
}

std::vector<std::pair<std::string, const items::Business*>> Society::openShops() const
{
    // The open shops (their keeper at the counter), with their business.
    std::vector<std::pair<std::string, const items::Business*>> open;
    for (const auto& p : positions_)
    {
        if (p.role != "merchant")
            continue;
        const auto held = state_.careers.positions.find(p.id);
        if (held == state_.careers.positions.end() || held->second.holder.empty())
            continue;
        const auto* life = resident(held->second.holder);
        const auto till = "till:" + p.id;
        if (life && life->task == "trade" && account(till))
            open.push_back({till, items::businessFor(p.title)});
    }
    return open;
}

void Society::countryBuys(int hod)
{
    const auto& cfg = items::country();
    // The open shops whose crafts make a good (the Country buys the city's own make, from its makers).
    const auto open = openShops();
    // Its basket as the farms take the food over (doc 71, 2.6; the user, 2026-11-01): re-weighted to the goods the farms'
    // basket doesn't buy and no food or drink, in proportion to the food areas' takings switched on so far.
    const double shift = countryBasketShift();
    for (const auto& market : cfg.markets)
    {
        const auto fund = marketFund(market.id);
        if (!account(fund) || market.basket.empty())
            continue;
        // What it owes its posts tonight stays in the purse with its reserve.
        std::int64_t owed = 0;
        for (const auto& p : positions_)
            if (const auto held = state_.careers.positions.find(p.id);
                held != state_.careers.positions.end() && !held->second.holder.empty())
                if (const auto pay = payOf(p); pay.payer == fund)
                    owed += pay.wage;
        const auto spare = cash(fund) - authored_.economy.countryReserve - owed;
        if (spare <= 0)
            continue;
        const double budget = double(spare) / double(ShopShut - hod);
        std::vector<std::pair<const std::string*, double>> basket;
        const auto& after = items::farms().countryBasket;
        if (shift <= 0 || after.empty() || &market != &cfg.markets.front())
            for (const auto& [item, share] : market.basket)
                basket.push_back({&item, double(share)});
        else
        {
            // Each good its share of the old basket and of the new, by how far the switch has gone.
            double before = 0, now = 0;
            for (const auto& [item, share] : market.basket)
                before += share;
            for (const auto& [item, share] : after)
                now += share;
            std::map<std::string, std::pair<const std::string*, double>> blend;
            for (const auto& [item, share] : market.basket)
                blend[item] = {&item, (1 - shift) * share / before};
            for (const auto& [item, share] : after)
            {
                auto& b = blend.try_emplace(item, std::pair<const std::string*, double>{&item, 0.}).first->second;
                b.second += shift * share / now;
            }
            for (const auto& [item, share] : market.basket)
                if (blend[item].second > 0)
                    basket.push_back(blend[item]);
            for (const auto& [item, share] : after)
                if (std::none_of(market.basket.begin(), market.basket.end(), [&](const auto& b) { return b.first == item; }))
                    basket.push_back(blend[item]);
        }
        loopToday_.countrySpent += basketBuys(fund, basket, budget, authored_.economy.countryReserve + owed, hod, open,
                                              "to the Country", "carried to the Country", nullptr);
        if (hod == ShopShut - 1)
            loopToday_.countryUnspent += std::max<std::int64_t>(0, cash(fund) - authored_.economy.countryReserve - owed);
    }
}

std::int64_t Society::basketBuys(const std::string& fund, const std::vector<std::pair<const std::string*, double>>& basket,
                                 double budget, std::int64_t keep, int hod,
                                 const std::vector<std::pair<std::string, const items::Business*>>& open, const char* saleKind,
                                 const char* usedKind, std::int64_t* carriageOut)
{
    // A basket of the city's goods (doc 68, 2: the Country's; doc 71, 2.6: a farm's): `budget` pennies this hour shared
    // over its goods by their shares, among those its makers' open shops hold now, a piece at a time to a seller picked
    // by stock; carried away (`usedKind`), its carriage out paid by `fund` (less the treasury's share), never taking
    // `fund` under `keep`. What it spent on the goods.
    const auto makes = [](const items::Business* b, const std::string& item) {
        if (!b)
            return false;
        for (const auto* k : items::craftsFor(b->id))
            for (const auto& [out, n] : k->out)
                if (out == item)
                    return true;
        return false;
    };
    // The goods its makers have on their shelves now, and their shares.
    struct Want
    {
        const std::string* item;
        double share;
        std::vector<const std::string*> tills;
    };
    std::vector<Want> wants;
    double shares = 0;
    for (const auto& [item, share] : basket)
    {
        Want w{item, share, {}};
        for (const auto& [till, business] : open)
            if (stock(*account(till), *item) > 0 && makes(business, *item))
                w.tills.push_back(&till);
        if (!w.tills.empty())
        {
            shares += share;
            wants.push_back(std::move(w));
        }
    }
    std::int64_t spent = 0;
    for (const auto& w : wants)
    {
        const auto price = catalogPrice(*w.item);
        // Whole pieces of its share; the odd part of a piece by a fixed roll, so small shares still buy.
        const double pieces = budget * w.share / shares / double(price);
        const auto key = fund + "|" + *w.item + "|" + std::to_string(loopToday_.day) + "|" + std::to_string(hod);
        int n = int(std::floor(pieces)) + (roll(key) < pieces - std::floor(pieces) ? 1 : 0);
        // A piece at a time to a seller picked by stock: spread over the makers, never the first in a list.
        std::map<const std::string*, int> lots;
        std::map<const std::string*, int> left;
        for (const auto* t : w.tills)
            left[t] = stock(*account(*t), *w.item);
        for (int i = 0; i < n; ++i)
        {
            std::vector<double> weights;
            for (const auto* t : w.tills)
                weights.push_back(left[t]);
            const int pick = weighted(weights, roll(key + "|" + std::to_string(i)));
            if (pick < 0)
                break;
            ++lots[w.tills[std::size_t(pick)]];
            --left[w.tills[std::size_t(pick)]];
        }
        for (const auto& [till, count] : lots)
        {
            int q = std::min(count, 99);
            const auto carriage = carriageSplit(fund, *w.item, q, false).first;
            if (cash(fund) - price * q - carriage < keep)
                q = int(std::max<std::int64_t>(0, (cash(fund) - keep) / std::max<std::int64_t>(1, price + 1)));
            if (q < 1 || !sale(*till, fund, *w.item, q, price, saleKind))
                continue;
            consume(fund, *w.item, q, usedKind);
            // Carriage out: the buyer pays it, less the treasury's share (doc 68, 5 and 6).
            if (const auto [own, city] = carriageSplit(fund, *w.item, q, true); own + city > 0)
            {
                const auto carter = carters();
                if (carter == CarriageFund && !account(CarriageFund))
                    openAccount(CarriageFund);
                if (const auto out = std::min(own, cash(fund)); own > 0 && shift(fund, carter, "", 0, out, "carriage out") && carriageOut)
                    *carriageOut += out;
                if (city > 0 && shift("treasury", carter, "", 0, city, "carriage subsidy"))
                    loopToday_.subsidy += city;
                loopToday_.carriage += own + city;
            }
            spent += price * q;
        }
    }
    return spent;
}

void Society::payWages()
{
    // Each payer's posts, held now, in post order.
    struct Due
    {
        const std::string* holder;
        int wage;
    };
    std::map<std::string, std::vector<Due>> byPayer;
    for (const auto& p : positions_)
    {
        const auto held = state_.careers.positions.find(p.id);
        if (held == state_.careers.positions.end() || held->second.holder.empty() || dead_.count(held->second.holder) ||
            !account(held->second.holder))
            continue;
        const auto pay = payOf(p);
        if (pay.kind.empty() || pay.wage <= 0 || !account(pay.payer))
            continue;
        byPayer[pay.payer].push_back({&held->second.holder, pay.wage});
    }
    const auto floorOf = [&](const std::string& payer) -> std::int64_t {
        if (payer == "treasury")
            return TreasuryFloat;
        if (payer.rfind("outside:", 0) == 0)
            return 0;                               // (Its reserve is kept from its buying so that it can pay its posts.)
        return tillFloat(payer);
    };
    const std::int64_t wagesDays = rule("floatWagesDays");
    for (const auto& [payer, dues] : byPayer)
    {
        std::int64_t owed = 0;
        for (const auto& d : dues)
            owed += d.wage;
        const std::int64_t float_ = floorOf(payer);
        const std::int64_t can = std::max<std::int64_t>(0, cash(payer) - float_);
        std::int64_t paid = 0;
        for (const auto& d : dues)
        {
            // Short of the whole, each post the same share of its wage.
            const std::int64_t coins = can >= owed ? d.wage : std::int64_t(d.wage) * can / owed;
            if (coins > 0 && shift(payer, *d.holder, "", 0, coins, "wages"))
                paid += coins;
        }
        loopToday_.wagesOwed += owed;
        loopToday_.wagesPaid += paid;
        if (paid < owed && payer.rfind("till:", 0) == 0)
        {
            ++loopToday_.tillsShort;
            tillsShort_[payer] = owed - paid;       // (For the treasury's wage support, after every payer has paid.)
        }
        // A till over its float and floatWagesDays of its wages (a week: doc 68, "The follow-ups") pays its keeper the
        // rest, as profit.
        if (payer.rfind("till:", 0) == 0)
        {
            const auto held = state_.careers.positions.find(payer.substr(5));
            const auto cap = float_ + wagesDays * owed;
            std::int64_t profit = 0;
            if (const auto excess = cash(payer) - cap; held != state_.careers.positions.end() && !held->second.holder.empty() &&
                                                       excess > 0 && shift(payer, held->second.holder, "", 0, excess, "profit"))
                loopToday_.profit += (profit = excess);
            judgeWages(payer, owed, paid, profit);  // (Its wage step, from its days: doc 68, "Self-correcting rules".)
        }
    }
}

std::int64_t Society::tillFloat(const std::string& till) const
{
    // A till's working float (doc 68, 4; "The follow-ups", the float rule): what it keeps back for tomorrow's draws
    // before its wages and its keeper's profit, the mean of its draws a day over the last floatDrawDays days (1: what it
    // drew yesterday, as it was), and at least TillFloat. A day it drew nothing counts as 0.
    const int days = rule("floatDrawDays");
    std::int64_t drawn = 0;
    if (const auto found = drawnDays_.find(till); days > 1 && found != drawnDays_.end() && !found->second.empty())
    {
        const auto& d = found->second;
        const auto n = std::min<std::size_t>(std::size_t(days), d.size());
        std::int64_t sum = 0;
        for (std::size_t i = d.size() - n; i < d.size(); ++i)
            sum += d[i];
        drawn = sum / std::int64_t(n);
    }
    else if (const auto y = drawnYesterday_.find(till); y != drawnYesterday_.end())
        drawn = y->second;
    return std::max(TillFloat, drawn);
}

void Society::supportWages()
{
    // The treasury's cap (doc 68, "First fixes"; the user's "supplement areas that help businesses"): after every payer
    // has paid tonight, what the treasury holds over treasuryCap pays wageSupport percent of what each short till could
    // not pay its posts, each post the same share of its wage; when it can't cover them all, every till the same share
    // of its shortfall. A farm's till short of its crew's wages is one of them (doc 71, 4), its crew's household its post.
    // What the short tills wanted and what of it went unpaid, the switch's (the farms' and switchTrades' tills) apart from
    // the city's ordinary short tills' (doc 71, 4, Phase B's gate), are counted.
    std::map<std::string, std::int64_t> shorts;
    shorts.swap(tillsShort_);
    const int percent = rule("wageSupport");
    if (percent <= 0 || shorts.empty() || !account("treasury"))
        return;
    std::int64_t want = 0;
    for (auto& [till, short_] : shorts)
        want += (short_ = short_ * percent / 100);
    if (want <= 0)
        return;
    loopToday_.supportWanted += want;
    std::map<std::string, std::int64_t> given;
    const auto unpaid = [&]() {
        for (const auto& [till, short_] : shorts)
            if (const auto left = short_ - given[till]; left > 0)
                (farmSwitchTill(till) ? loopToday_.supportUnpaidSwitch : loopToday_.supportUnpaidOrdinary) += left;
    };
    const auto room = cash("treasury") - std::int64_t(rule("treasuryCap"));
    if (room <= 0)
        return unpaid();
    // Each short till's posts held now, as payWages found them (a farm's: its crew, at its wages).
    std::map<std::string, std::vector<std::pair<const std::string*, int>>> posts;
    for (const auto& p : positions_)
    {
        const auto held = state_.careers.positions.find(p.id);
        if (held == state_.careers.positions.end() || held->second.holder.empty() || dead_.count(held->second.holder) ||
            !account(held->second.holder))
            continue;
        if (const auto pay = payOf(p); pay.wage > 0 && shorts.count(pay.payer))
            posts[pay.payer].push_back({&held->second.holder, pay.wage});
    }
    for (const auto& f : farms_)
        if (shorts.count(f.till) && account(f.crew))
            posts[f.till].push_back({&f.crew, int(f.wages())});
    const auto give = std::min(room, want);
    for (const auto& [till, short_] : shorts)
    {
        const auto share = give >= want ? short_ : short_ * give / want;
        int owed = 0;
        for (const auto& [holder, wage] : posts[till])
            owed += wage;
        const auto farm = farmOf_.find(till);
        for (const auto& [holder, wage] : posts[till])
            if (const auto coins = share * wage / std::max(1, owed);
                coins > 0 && shift("treasury", *holder, "", 0, coins, "wage support"))
            {
                given[till] += coins;
                loopToday_.wageSupport += coins;
                if (farm != farmOf_.end())
                    loopToday_.farms[farms_[farm->second].def->id].support += coins;
                else
                    loopToday_.wagesPaid += coins;
            }
    }
    unpaid();
}

void Society::giveAlms()
{
    // The city's alms (doc 68, 4): after the day's wages, a household still holding less than a day's food money a head
    // (the old who have retired, orphans, the out of work, a family whose shop couldn't pay) is given what makes it up,
    // from what the treasury holds over its float. Paid to the member with the least; whoever buys, buys for all.
    const std::int64_t level = almsNow();            // (Pennies a head: doc 68, "First fixes"; it was 7. The treasury's
                                                     // level moves it: "Self-correcting rules".)
    for (const auto& [home, members] : households())
    {
        std::int64_t held = 0;
        for (const auto& m : members)
            held += cash(m);
        const auto want = level * std::int64_t(members.size()) - held;
        const auto can = cash("treasury") - TreasuryFloat;
        if (want <= 0 || can <= 0)
            continue;
        const auto poorest = *std::min_element(members.begin(), members.end(), [&](const std::string& a, const std::string& b) { return cash(a) < cash(b); });
        if (const auto coins = std::min(want, can); shift("treasury", poorest, "", 0, coins, "alms"))
            loopToday_.alms += coins;
    }
}

void Society::spendSurplus()
{
    // The treasury's surplus (doc 68, Progress, "The treasury's surplus: works and alms"; the user's rule, 2026-10-31):
    // last at 18:00, after the treasury's own posts, the short tills' wages over its cap and the alms, what it still holds
    // over treasuryCap and surplusKeep (kept for the summer's and autumn's fall, when the toll is lower than its posts and
    // alms) goes back out: surplusAlms percent to the poorest households, per head, poorest first, up to surplusAlmsTo
    // pennies a head; surplusWorks percent, and what the alms could not place, to public works orders from the city's
    // trades, till to till; what the works could not place to the alms. What neither places stays. Only what is over
    // them: it never makes money, nor takes the treasury under its cap and reserve.
    if (rule("surplus") <= 0 || !account("treasury"))
        return;
    const auto over = cash("treasury") - std::int64_t(rule("treasuryCap")) - std::int64_t(rule("surplusKeep"));
    if (over <= 0)
        return;
    std::int64_t works = rule("surplusWorks"), alms = rule("surplusAlms");
    if (works + alms > 100)                         // (Shares of more than the whole: the whole, in their proportion.)
    {
        works = works * 100 / (works + alms);
        alms = 100 - works;
    }
    const auto almsShare = over * alms / 100, worksShare = over * works / 100;
    if (almsShare + worksShare <= 0)
        return;
    loopToday_.surplus += over;
    const auto given = topUpPoorest(almsShare);
    const auto worksBudget = worksShare + almsShare - given;
    topUpPoorest(worksBudget - worksOrders(worksBudget));
}

std::int64_t Society::topUpPoorest(std::int64_t budget)
{
    // The poorest households, per head (all their members' purses), raised to one level, poorest first: the highest whole
    // pennies a head, up to surplusAlmsTo, that the budget can bring every household under it to; what is left of the
    // budget the next penny a head, the poorest first. Paid to the member with the least, as the alms are.
    const std::int64_t to = rule("surplusAlmsTo");
    budget = std::min(budget, cash("treasury") - TreasuryFloat);
    if (budget <= 0 || to <= 0)
        return 0;
    struct Home
    {
        const std::string* home;
        const std::vector<std::string>* members;
        std::int64_t held, heads;
    };
    const auto homes = households();
    std::vector<Home> under;
    for (const auto& [home, members] : homes)
    {
        std::int64_t held = 0;
        for (const auto& m : members)
            held += cash(m);
        if (const auto heads = std::int64_t(members.size()); heads > 0 && held < to * heads)
            under.push_back({&home, &members, held, heads});
    }
    if (under.empty())
        return 0;
    std::sort(under.begin(), under.end(), [](const Home& a, const Home& b) {
        return a.held * b.heads != b.held * a.heads ? a.held * b.heads < b.held * a.heads : *a.home < *b.home;
    });
    const auto costTo = [&](std::int64_t level) {
        std::int64_t cost = 0;
        for (const auto& h : under)
            cost += std::max<std::int64_t>(0, level * h.heads - h.held);
        return cost;
    };
    std::int64_t low = 0, high = to;                // (The highest level the budget reaches: costTo(low) <= budget.)
    while (low < high)
    {
        const auto mid = (low + high + 1) / 2;
        if (costTo(mid) <= budget)
            low = mid;
        else
            high = mid - 1;
    }
    std::int64_t left = budget - costTo(low), given = 0;
    for (const auto& h : under)
    {
        auto coins = std::max<std::int64_t>(0, low * h.heads - h.held);
        if (low < to && left > 0)                   // (The next penny a head, the poorest first, while the budget lasts.)
        {
            const auto more = std::min(left, std::max<std::int64_t>(0, (low + 1) * h.heads - h.held) - coins);
            coins += more;
            left -= more;
        }
        if (coins <= 0)
            continue;
        const auto poorest = *std::min_element(h.members->begin(), h.members->end(),
                                               [&](const std::string& a, const std::string& b) { return cash(a) < cash(b); });
        if (shift("treasury", poorest, "", 0, coins, "alms from the surplus"))
        {
            given += coins;
            ++loopToday_.surplusHouseholds;
        }
    }
    loopToday_.surplusAlms += given;
    return given;
}

std::int64_t Society::worksOrders(std::int64_t budget)
{
    // The public works (country.json `works`): the treasury orders upkeep and goods for the public buildings and services
    // from the city's trades, at the catalogue's price, till to till: each trade its `weight`'s share of the budget (of
    // what is left: what one trade can't place goes on to the next), its tills those with most room under their keeper's
    // profit (a till's float and floatWagesDays of its posts' wages, as payWages keeps them) first, so that the orders pay
    // the posts of the tills that need them and spread over a trade's shops rather than heaping one keeper's profit. A
    // service is done at once (nothing kept); a good comes off the shop's shelves (one left for its customers) and is used
    // up in the works, so the shop makes more (its shelves follow its sales). The city fetches what it orders: any shop of
    // the trade with its post held, open or not.
    budget = std::min(budget, cash("treasury") - TreasuryFloat);
    const auto& list = items::country().works;
    if (budget <= 0 || list.empty())
        return 0;
    std::map<std::string, std::int64_t> owed;      // Till -> its posts' wages tonight.
    std::map<std::string, std::vector<std::string>> tills;   // Business -> its tills.
    for (const auto& p : positions_)
    {
        const auto held = state_.careers.positions.find(p.id);
        if (held == state_.careers.positions.end() || held->second.holder.empty() || dead_.count(held->second.holder))
            continue;
        if (const auto pay = payOf(p); pay.wage > 0 && pay.payer.rfind("till:", 0) == 0 && account(held->second.holder))
            owed[pay.payer] += pay.wage;
        if (p.role != "merchant")
            continue;
        if (const auto* b = items::businessFor(p.title); b && account("till:" + p.id))
            tills[b->id].push_back("till:" + p.id);
    }
    const std::int64_t wagesDays = rule("floatWagesDays");
    const auto room = [&](const std::string& till) { return tillFloat(till) + wagesDays * owed[till] - cash(till); };
    const auto service = [](const std::string& item) {
        const auto* g = items::good(item);
        return g && g->category == "service";
    };
    int weights = 0;
    for (const auto& w : list)
        if (tills.count(w.business))
            weights += w.weight;
    std::int64_t spent = 0;
    for (const auto& w : list)
    {
        const auto found = tills.find(w.business);
        if (found == tills.end() || weights <= 0)
            continue;
        std::int64_t left = (budget - spent) * w.weight / weights;
        weights -= w.weight;
        while (left > 0)
        {
            // The till with most room (then by till), and the good it can most supply that the trade's share still pays.
            const std::string *best = nullptr, *good = nullptr;
            std::int64_t bestRoom = 0;
            for (const auto& till : found->second)
            {
                const std::string* pick = nullptr;
                int most = 0;
                for (const auto& g : w.goods)
                    if (const auto price = catalogPrice(g); price >= 1 && price <= left)
                        if (const int n = service(g) ? 99 : stock(*account(till), g) - 1; n > most)
                        {
                            most = n;
                            pick = &g;
                        }
                if (!pick)
                    continue;
                if (const auto r = room(till); !best || r > bestRoom || (r == bestRoom && till < *best))
                {
                    best = &till;
                    good = pick;
                    bestRoom = r;
                }
            }
            if (!best)
                break;
            const auto price = catalogPrice(*good);
            const bool svc = service(*good);
            std::int64_t n = std::min<std::int64_t>({left / price, 99, svc ? 99 : stock(*account(*best), *good) - 1});
            // Up to its room; a till with none (every till of the trade full), a share of what is left, so they take turns.
            n = std::max<std::int64_t>(1, std::min(n, bestRoom >= price ? bestRoom / price
                                                                          : left / price / std::int64_t(found->second.size())));
            if (svc && !create(*best, *good, int(n), "a service done"))
                break;
            if (!sale(*best, "treasury", *good, int(n), price, "a public works order"))
            {
                if (svc)
                    consume(*best, *good, int(n), "a service not done");
                break;
            }
            consume("treasury", *good, int(n), svc ? "a service, done at once" : "used in the city's works");
            left -= price * n;
            spent += price * n;
            auto& day = loopToday_.worksTrades[w.business];
            day.sold += int(n);
            day.spent += price * n;
            loopToday_.worksPieces += int(n);
        }
    }
    loopToday_.worksSpent += spent;
    return spent;
}

// The restocking rule (doc 67, 6; revised by doc 68, 7): shelves follow sales, and making takes the crew's hours.
std::vector<std::string> Society::restockFromStore(const std::string& keeper, double absoluteDay)
{
    std::vector<std::string> done;
    const auto hour = std::int64_t(std::floor(absoluteDay * 24));
    if (const auto [last, fresh] = restockedAt_.emplace(keeper, hour); !fresh)
    {
        if (last->second == hour)
            return done;                            // At most once a game hour.
        last->second = hour;
    }
    if (!storeOpen(keeper))
        return done;
    const auto* job = jobOf(keeper);
    const auto* business = items::businessFor(job->title);
    const auto till = tillOf(keeper);
    const auto sold = spec(job->founder) ? wares(job->founder) : std::vector<std::string>{};
    // The crew's hours this hour, with what was left from the last (at most workBankMost: by default the business's
    // longest craft and an hour more, so a long craft is made over two days; doc 68, "First fixes").
    const int crew = crewOf(till);
    auto& bank = workBank_[till];
    bank = std::min(workBankMost(till, business), bank + crew);
    struct Want
    {
        std::string ware;
        int lack;
        double ratio;
        const items::Craft* craft;
    };
    std::vector<Want> wants;
    for (const auto& ware : sold)
    {
        const int shelf = shelfOf(till, ware);
        const int have = stock(*account(till), ware);
        if (have >= shelf)
            continue;
        const items::Craft* craft = nullptr;
        if (business)
            for (const auto* k : items::craftsFor(business->id))
                if (!craft && std::any_of(k->out.begin(), k->out.end(), [&](const auto& o) { return o.first == ware; }))
                    craft = k;
        if (!craft && storeUse(keeper, ware) != StoreUse::Resell)
            continue;
        wants.push_back({ware, shelf - have, double(have) / shelf, craft});
    }
    // The ware furthest below its shelf first.
    std::stable_sort(wants.begin(), wants.end(), [](const Want& a, const Want& b) { return a.ratio < b.ratio; });
    // A farm's good the shop lacks more of than the farms hold (doc 71, 4: the gate counts the days an area runs out).
    const auto farmsShort = [&](const std::string& item, int lack) {
        if (const int area = farmAreaOf(item); area >= 0 && lack > 0 && farmsHold(item) < lack)
            ++loopToday_.areasOut[items::farms().areas[std::size_t(area)].id];
    };
    for (const auto& w : wants)
    {
        if (!w.craft)
        {
            farmsShort(w.ware, std::min(StoreDrawMost, w.lack));
            // (Within what the Country can still supply of it today: a season's cap.)
            const int n = std::min({StoreDrawMost, w.lack, countryLeft(w.ware)});
            const bool drew = n > 0 && drawFromStore(keeper, w.ware, n, "restocked from the store room").ok;
            if (drew)
                done.push_back("restocked " + w.ware);
            if (const int lack = std::min(StoreDrawMost, w.lack) - (drew ? n : 0); lack > 0 && shortageWatcher_)
                shortageWatcher_({till, till, w.ware, "", lack});   // (The cap, or the farms' stock: the draw cut short.)
            continue;
        }
        // The farms' stores first (doc 71, 2.4): a good of a season group with `stores` that the farms of an area on make
        // (the drying houses' dried apples and preserves) its maker sells on from what they hold before it makes its own,
        // as it sold on the Country's stores when the season left it short (their yields were sized on those draws).
        int wanted = w.lack;
        if (const auto* g = seasonGroup(w.ware);
            g && g->stores && farmAreaOf(w.ware) >= 0 && storeUse(keeper, w.ware) == StoreUse::Resell)
            if (const int n = std::min({StoreDrawMost, wanted, countryLeft(w.ware)});
                n > 0 && drawFromStore(keeper, w.ware, n, "restocked from the store room").ok)
            {
                done.push_back("restocked " + w.ware + " from the farms' stores");
                if ((wanted -= n) < 1)
                    continue;
            }
        const auto& k = *w.craft;
        int per = 1;
        for (const auto& [item, n] : k.out)
            if (item == w.ware)
                per = n;
        int batches = std::min({int(std::ceil(double(wanted) / per)), int(std::floor(bank / k.hours + 1e-9)), 100});
        if (batches < 1)
            continue;
        // Room for what the batches make, and their inputs drawn for what the shelves lack.
        const auto& shelves = *account(till);
        std::size_t kinds = shelves.stock.size();
        bool can = true;
        for (const auto& [item, n] : k.out)
        {
            can = can && itemValid(item) && stock(shelves, item) <= StockCap - n * batches;
            kinds += shelves.stock.count(item) ? 0 : 1;
        }
        for (const auto& [item, n] : k.in)
        {
            kinds += shelves.stock.count(item) ? 0 : 1;
            can = can && (stock(shelves, item) >= n * batches || storeUse(keeper, item) == StoreUse::Make);
        }
        if (!can || kinds > MaxGoodsKinds)
            continue;
        bool capped = false;                        // (An input the Country can't send enough of today: its season.)
        for (const auto& [item, n] : k.in)
        {
            // The city's makers first (the rule "suppliers": the cooper's casks for the wine makers), then the store room.
            int lack = n * batches - stock(*account(till), item);
            if (lack > 0)
                lack -= buyFromCity(till, business, item, lack);
            capped = capped || (lack > 0 && countryLeft(item) < lack);
            farmsShort(item, lack);
            if ((lack = std::min(lack, countryLeft(item))) > 0)
                drawFromStore(keeper, item, std::min(StoreDrawMost, lack), "drawn for " + k.id);
        }
        // As many batches as the inputs on the shelves now allow (a refused draw leaves the rest for the next hour).
        for (const auto& [item, n] : k.in)
            batches = std::min(batches, stock(*account(till), item) / n);
        if (batches < 1)
        {
            // The Country's stores (doc 68, "The follow-ups", winter preserves): a good of a season group with `stores`
            // that its maker can't make for the season's lack of an input it draws itself, made in the Country, where the
            // maker sells it on too (crafts.json `supplies`): the preservers' dried and preserved fruit in winter.
            if (const auto* g = seasonGroup(w.ware); capped && g && g->stores && storeUse(keeper, w.ware) == StoreUse::Resell)
                if (const int n = std::min({StoreDrawMost, wanted, countryLeft(w.ware)});
                    n > 0 && drawFromStore(keeper, w.ware, n, "restocked from the store room").ok)
                    done.push_back("restocked " + w.ware + " from the Country's stores");
            continue;
        }
        for (const auto& [item, n] : k.in)
            consume(till, item, n * batches, "made into " + k.id);
        for (const auto& [item, n] : k.out)
            create(till, item, n * batches, "made from the store room");
        bank -= batches * k.hours;
        done.push_back("made " + std::to_string(batches) + " " + k.id);
    }
    return done;
}

// Trades (doc 68, Progress, "Trades: medicine, the armoury's patrons, office heads").
namespace
{
// Whether a business sells a good: one of its crafts makes it, or it supplies it (crafts.json `supplies`).
bool sells(const std::string& business, const std::string& item)
{
    for (const auto* k : items::craftsFor(business))
        for (const auto& [out, n] : k->out)
            if (out == item)
                return true;
    const auto& supplies = items::suppliesFor(business);
    return std::find(supplies.begin(), supplies.end(), item) != supplies.end();
}
} // namespace

std::vector<std::string> Society::citySellers(const std::string& buyerBusiness, const std::string& item) const
{
    std::vector<std::string> out;
    for (const auto& s : items::country().suppliers)
        if (s.buyer == buyerBusiness &&
            (s.goods.empty() ? sells(s.seller, item) : std::find(s.goods.begin(), s.goods.end(), item) != s.goods.end()) &&
            std::find(out.begin(), out.end(), s.seller) == out.end())
            out.push_back(s.seller);
    return out;
}

int Society::buyFromCity(const std::string& till, const items::Business* business, const std::string& item, int want)
{
    if (want < 1 || !business || rule("suppliers") <= 0 || !account(till))
        return 0;
    const auto sellers = citySellers(business->id, item);
    if (sellers.empty())
        return 0;
    // The sellers' tills holding the good (a post held, whether or not its keeper is at the counter this hour: the buyer
    // fetches it), the one holding most first, then by till, so the order never follows the map's.
    std::vector<std::pair<int, std::string>> held;
    for (const auto& p : positions_)
    {
        if (p.role != "merchant")
            continue;
        const auto h = state_.careers.positions.find(p.id);
        const auto* b = items::businessFor(p.title);
        if (h == state_.careers.positions.end() || h->second.holder.empty() || !b ||
            std::find(sellers.begin(), sellers.end(), b->id) == sellers.end())
            continue;
        const auto seller = "till:" + p.id;
        if (const auto* a = account(seller); a && seller != till)
            if (const int n = stock(*a, item); n > 0)
                held.push_back({n, seller});
    }
    std::sort(held.begin(), held.end(), [](const auto& a, const auto& b) { return a.first != b.first ? a.first > b.first : a.second < b.second; });
    int got = 0;
    for (const auto& [unused, seller] : held)
    {
        // As many as the till can pay for, at the seller's price: the catalogue's, and a dearer season's difference after
        // (a cheaper season's is not passed between shops).
        int n = std::min({want - got, stock(*account(seller), item), 99});
        n = int(std::min<std::int64_t>(n, cash(till) / std::max<std::int64_t>(1, seasonalCost(item, 1))));
        while (n > 0 && cash(till) < seasonalCost(item, n))
            --n;
        if (n < 1)
            break;
        const auto base = catalogPrice(item) * n, cost = seasonalCost(item, n);
        if (!sale(seller, till, item, n, catalogPrice(item), "from a city maker"))
            continue;
        if (cost > base)
            shift(till, seller, "", 0, cost - base, "from a city maker: the season's price");
        drawnToday_[till] += cost;                  // (Its goods' cost, as a draw's: the till's float tomorrow.)
        loopToday_.cityTrade += cost;
        loopToday_.cityTradePieces += n;
        if ((got += n) >= want)
            break;
    }
    return got;
}

int Society::unitsOf(const items::PatronGroup& group) const
{
    if (group.roles.empty())
        return group.units;                         // (The Houses' retainers, the hulls, the buildings: a number.)
    int n = 0;
    for (const auto& p : positions_)
        if (std::find(group.roles.begin(), group.roles.end(), p.role) != group.roles.end())
            if (const auto h = state_.careers.positions.find(p.id);
                h != state_.careers.positions.end() && !h->second.holder.empty() && !dead_.count(h->second.holder))
                ++n;
    return n;
}

void Society::patronsBuy(int hod, std::int64_t day)
{
    // The trades' patrons (doc 68, Progress, "Trades"; the user, 2026-10-31, on the armoury: "high cost items replaced
    // occasionally, but repaired frequently which should be the main source of their business"): once a day, at the
    // first hour one of the trade's shops is open, each of its patrons' units (an armed wolf, a hull, a building) has
    // each of the trade's patron services done about once every so many days, and, where the trade has kit, a new piece
    // bought about once every armourEvery days (one of the kit its shops hold, at the catalogue's price); paid by the
    // unit's patron, the treasury or the Country (the Houses' estates, the wharf), each only from what it can spare (the
    // treasury over its float and tonight's posts, the Country over its reserve and its posts). A fixed roll of the
    // group, the unit, the day and the work: nothing saved.
    if (rule("patrons") <= 0)
        return;
    for (const auto& trade : items::country().patrons)
    {
        if (const auto found = patronsDay_.find(trade.business); found != patronsDay_.end() && found->second == day)
            continue;
        std::vector<std::string> open;
        for (const auto& p : positions_)
        {
            if (p.role != "merchant")
                continue;
            const auto h = state_.careers.positions.find(p.id);
            if (h == state_.careers.positions.end() || h->second.holder.empty())
                continue;
            const auto* life = resident(h->second.holder);
            const auto* b = items::businessFor(p.title);
            if (life && life->task == "trade" && b && b->id == trade.business && account("till:" + p.id))
                open.push_back("till:" + p.id);
        }
        if (open.empty())
        {
            if (hod >= ShopShut - 1)
                patronsDay_[trade.business] = day;  // (None open today: nothing done.)
            continue;
        }
        patronsDay_[trade.business] = day;
        auto& done = loopToday_.patronTrades[trade.business];
        for (const auto& g : trade.groups)
        {
            const bool city = g.payer == "treasury";
            const auto payer = city ? std::string("treasury") : marketFund(g.payer);
            if (!account(payer))
                continue;
            std::int64_t owed = 0;                  // (The Country's posts tonight; the treasury's are subsidyRoom's.)
            if (!city)
                for (const auto& p : positions_)
                    if (const auto h = state_.careers.positions.find(p.id); h != state_.careers.positions.end() && !h->second.holder.empty())
                        if (const auto pay = payOf(p); pay.payer == payer)
                            owed += pay.wage;
            const auto spare = [&]() -> std::int64_t {
                if (city)
                {
                    subsidyRoom();                  // (Works out tonight's public wages, if not yet.)
                    return cash(payer) - TreasuryFloat - std::max<std::int64_t>(0, treasuryOwed_);
                }
                return cash(payer) - authored_.economy.countryReserve - owed;
            };
            auto& spent = city ? loopToday_.patronsTreasury : loopToday_.patronsCountry;
            const auto pay = [&](std::int64_t coins) {
                spent += coins;
                done.spent += coins;
            };
            const int units = unitsOf(g);
            for (int u = 0; u < units; ++u)
            {
                const auto key = trade.business + "|" + g.id + "|" + std::to_string(u) + "|" + std::to_string(day);
                for (const auto& s : services_)
                {
                    if (!s.patron || !s.on || s.business != trade.business || roll(key + "|" + s.id) * double(s.every) >= 1. ||
                        s.price > spare())
                        continue;
                    const auto& till = open[std::size_t(roll(key + "|shop|" + s.id) * double(open.size())) % open.size()];
                    if (!create(till, s.id, 1, "a service done"))
                        continue;
                    if (!sale(till, payer, s.id, 1, s.price, "a patron's work"))
                    {
                        consume(till, s.id, 1, "a service not done");
                        continue;
                    }
                    consume(payer, s.id, 1, "a service, done at once");
                    ++loopToday_.patronJobs;
                    ++done.sold;
                    pay(s.price);
                }
                if (trade.pieces.empty() || roll(key + "|piece") * double(rule("armourEvery")) >= 1.)
                    continue;
                std::vector<std::pair<const std::string*, const std::string*>> kit;   // (Till, piece) on the shelves now.
                for (const auto& till : open)
                    for (const auto& piece : trade.pieces)
                        if (stock(*account(till), piece) > 0)
                            kit.push_back({&till, &piece});
                if (kit.empty())
                    continue;
                const auto [till, piece] = kit[std::size_t(roll(key + "|which") * double(kit.size())) % kit.size()];
                const auto price = catalogPrice(*piece);
                if (price > spare() || !sale(*till, payer, *piece, 1, price, "a patron's new kit"))
                    continue;
                consume(payer, *piece, 1, "kit for an armed wolf");
                ++loopToday_.patronPieces;
                pay(price);
            }
        }
    }
}

// The self-correcting rules (doc 68, Progress, "Self-correcting rules"; the user, 2026-11-01: "It's looking good and
// balanced right now, but it doesn't seem robust enough to survive us adding additional towns and converting the
// 'country' into actual businesses just yet."): a shop's prices follow its stock, its shelves its sales, a till's wages
// its cover, and the gate toll and the alms the treasury's level. Each moves gently, within its bounds, by its own rule
// and switch; what each has drifted to is the society's (LoopState, saved with it). None moves money of itself: the
// prices are paid as the season's are, the wages and the toll by the payers they always had.
const std::map<std::string, double>* Society::priceFactors(const std::string& till) const
{
    const auto found = state_.loop.prices.find(till);
    return found == state_.loop.prices.end() ? nullptr : &found->second;
}

double Society::stockFactor(const std::string& till, const std::string& ware) const
{
    if (rule("stockPrices") <= 0)
        return 1;
    if (const auto* factors = priceFactors(till))
        if (const auto found = factors->find(ware); found != factors->end())
            return found->second;
    return 1;
}

std::int64_t Society::shopCost(const std::string& till, const std::string& item, int quantity) const
{
    // As seasonalCost, times the shop's own factor: the most it may come to (the catalogue's price is paid first).
    const auto each = catalogPrice(item);
    if (quantity < 1)
        return 0;
    const double f = priceFactor(item) * stockFactor(till, item);
    if (f == 1)
        return each * quantity;
    return std::max<std::int64_t>(each * quantity, std::int64_t(std::ceil(double(each) * quantity * f - 1e-9)));
}

double Society::gateTollNow() const
{
    const int base = std::clamp(authored_.economy.gateToll, 0, 90);
    if (rule("treasuryDrift") <= 0 || state_.loop.toll == 0)
        return base;
    const double most = rule("tollDriftMost");      // (The DM may have narrowed the bounds since it moved.)
    return std::clamp(base + std::clamp(state_.loop.toll, -most, most), 0., 90.);
}

std::int64_t Society::almsNow() const
{
    const int base = rule("almsADay");
    if (rule("treasuryDrift") <= 0 || state_.loop.alms == 0)
        return base;
    const int most = rule("almsDriftMost");
    return std::max(0, base + std::clamp(state_.loop.alms, 0, most));
}

double Society::shopFactor(const std::string& till, const std::string& item) const
{
    return priceFactor(item) * stockFactor(till, items::baseOf(item));
}

int Society::driftedWage(const std::string& till, int wage, bool hand) const
{
    if (rule("wageDrift") <= 0)
        return wage;
    const auto found = state_.loop.wages.find(till);
    if (found == state_.loop.wages.end() || found->second.step == 0)
        return wage;
    // The till's step, within the keeper's bound or a hand's (the user, 2026-11-01: "Yeah let the wage limit spread
    // more"; doc 68, Progress, "Players in the shifting market; wider wages; the long runs").
    const int most = rule(hand ? "wageDriftHandMost" : "wageDriftMost");
    const int step = std::clamp(found->second.step, -most, most);
    // Lowered no further than the floor (a day's food), and the floor never raises a wage the world set under it.
    return step > 0 ? wage + step : std::max(std::min(wage, rule("wageDriftFloor")), wage + step);
}

int Society::wageAtBound(const std::string& till) const
{
    if (rule("wageDrift") <= 0)
        return 0;
    const auto found = state_.loop.wages.find(till);
    const int step = found == state_.loop.wages.end() ? 0 : found->second.step;
    if (step > 0 && step >= rule("wageDriftMost") && step >= rule("wageDriftHandMost"))
        return 1;
    return wageFalls(*this, step) ? 0 : -1;         // (At the foot: no post can fall, by its bound or the floor.)
}

void Society::followStock(bool restDay)
{
    // At the day's turn, for each shop open today (it sold something), each ware it sells:
    //  - its sales a day, smoothed over about shelfDays days (the second rule's shelf is shelfCover percent of them, at
    //    most StoreDrawMost): a day it sold out having sold at least its shelf counts as shelfGrow percent more than its
    //    shelf, so a shelf too small to show what would sell grows rather than shrinking with its sales (the small shelf's
    //    spiral; a maker that sold out short of its shelf was short of hands, not of shelf); a Restday's or festival's few
    //    sales never lower it (as buying ahead's fix keeps yesterday's shelves);
    //  - its price factor (the first rule, for what residents buy): up stockPriceStep a day when it sold out, down when
    //    what is left at the day's end is more than GlutDays of the day's sales (the season's stock-up allowed for) and
    //    over its starting stock, and else back toward 1 by a quarter step. Then every factor moved today is scaled so
    //    that their mean, weighted by sales, is 1: what is scarce grows dearer against what is plentiful while the city's
    //    prices as a whole stay the catalogue's (the season's). Without it the food every maker makes as fast as its hands
    //    allow (sold out each day, wherever residents turn) climbs to the band's top together and households pay a
    //    quarter more for the same food: a level that ratchets, where only the prices between goods should move. Within
    //    stockPriceBand of 1, and a ware the store room supplies never under what it charges and a little carriage;
    //    kept as they were over a Restday or festival.
    const bool shelves = rule("shelfFollow") > 0, prices = rule("stockPrices") > 0;
    if (!shelves && !prices)
        return;
    constexpr double GlutDays = 3;
    const double days = rule("shelfDays"), grow = 1 + rule("shelfGrow") / 100., cover = rule("shelfCover") / 100.;
    const double band = rule("stockPriceBand") / 100., step = rule("stockPriceStep") / 1000.;
    const double making = std::clamp(authored_.economy.storeMarkup, 0, 1000) / 100.;
    const double resale = std::clamp(authored_.economy.resaleMarkup, 0, 1000) / 100.;
    // (Only what residents buy has a price of the shop's own: food, needs, comforts; players pay it too, and are offered
    // their share of it, quote. A maker's inputs and by-products go till to till, and the Country's purchases, at the
    // catalogue's price. A player's purchase is a sale like a resident's, counted in the day's sales and selling out; a
    // player's sale to the shop is on its shelf at the day's end, where a glut tells.)
    const auto& needs = items::households().needs;
    const auto& comforts = items::country().comforts;
    const auto residentsBuy = [&](const std::string& ware) {
        return edible(ware) || std::find(comforts.begin(), comforts.end(), ware) != comforts.end() ||
               std::any_of(needs.begin(), needs.end(), [&](const auto& n) { return std::find(n.any.begin(), n.any.end(), ware) != n.any.end(); });
    };
    struct Priced
    {
        std::string till, ware;
        double f, low, high, weight;                // (Its factor before the scaling, its bounds, its sales a day.)
    };
    std::vector<Priced> moved;
    for (const auto& p : positions_)
    {
        if (p.role != "merchant")
            continue;
        const auto till = "till:" + p.id;
        const auto* onShelves = account(till);
        const auto sold = soldToday_.find(till);
        if (!onShelves || sold == soldToday_.end() || sold->second.empty())
            continue;                               // (Shut all day: nothing to go on.)
        const auto held = state_.careers.positions.find(p.id);
        const std::string keeper = held != state_.careers.positions.end() ? held->second.holder : std::string();
        const auto out = soldOut_.find(till);
        for (const auto& ware : wares(p.founder))
        {
            const auto s = sold->second.find(ware);
            const int n = s == sold->second.end() ? 0 : s->second;
            const bool ranOut = out != soldOut_.end() && out->second.count(ware) > 0;
            auto& means = state_.loop.sales[till];
            const auto mean = means.find(ware);
            const int least = startingStock(ware);
            const double shelf = std::min<double>(StoreDrawMost, mean == means.end() ? least
                                                                   : std::max<double>(least, std::ceil(mean->second * cover - 1e-9)));
            double x = n;
            if (ranOut && n >= shelf)
                x = std::max(x, shelf * grow);
            double weight = 0;
            if (mean == means.end())
            {
                if (x > 0)
                    means[ware] = weight = std::min<double>(StoreDrawMost, x);   // (Its first day: what it sold.)
            }
            else
            {
                if (restDay)
                    x = std::max(x, mean->second);
                weight = mean->second = std::min<double>(StoreDrawMost, mean->second + (x - mean->second) / days);
                if (mean->second < .05)
                    means.erase(mean);
            }
            if (means.empty())
                state_.loop.sales.erase(till);
            if (!prices || restDay || !residentsBuy(ware))
                continue;
            double f = stockFactor(till, ware);
            // (Never under what the store room charges the shop for it, and a little for carriage: a ware it resells at
            // resaleMarkup, an input of its crafts it also sells as it is, the fishmonger's fish, at storeMarkup.)
            const auto use = keeper.empty() ? StoreUse::None : storeUse(keeper, ware);
            const double charged = use == StoreUse::Resell ? resale + .05 : use == StoreUse::Make ? making + .05 : 0.;
            const double low = std::min(1., std::max(1 - band, charged));
            const int have = stock(*onShelves, ware);
            if (ranOut)
                f += step;
            else if (have > least && have > GlutDays * n * shelfFactor(ware))
                f -= step;
            else
                f = f > 1 ? std::max(1., f - step / 4) : std::min(1., f + step / 4);
            moved.push_back({till, ware, f, low, 1 + band, weight});
        }
    }
    if (moved.empty())
        return;
    // The city's price level held: today's factors scaled so that, each within its bounds, their mean by sales is 1 (the
    // scale found by halving, the bounds being where a factor stops).
    const auto levelAt = [&](double scale) {
        double level = 0, weights = 0;
        for (const auto& m : moved)
        {
            level += std::clamp(m.f * scale, m.low, m.high) * m.weight;
            weights += m.weight;
        }
        return weights > 0 ? level / weights : 1.;
    };
    double scale = 1;
    if (levelAt(1) != 1)
    {
        double low = .25, high = 4;
        for (int i = 0; i < 60; ++i)
            (levelAt(scale = (low + high) / 2) < 1 ? low : high) = scale;
    }
    for (const auto& m : moved)
    {
        const double f = std::clamp(m.f * scale, m.low, m.high);
        auto& factors = state_.loop.prices[m.till];
        if (std::abs(f - 1) < 1e-9)
            factors.erase(m.ware);
        else
            factors[m.ware] = f;
        if (factors.empty())
            state_.loop.prices.erase(m.till);
    }
}

void Society::judgeWages(const std::string& till, std::int64_t owed, std::int64_t paid, std::int64_t profit)
{
    // A till's wages follow its cover (the third rule), a penny a post at a time: over wageDriftDays days, a till that paid
    // all its posts every day, its keeper's profit at least wageDriftSpare percent of their wages, raises each post's wage
    // a penny; one short of them on more than half the days lowers it a penny (only while that lowers some post: none
    // under wageDriftFloor); at most wageDriftMost pennies either way on a keeper's, wageDriftHandMost on a hand's (12p
    // each, from 6p: the user, 2026-11-01, "Yeah let the wage limit spread more"). Between the two (paid, with little to spare; short
    // now and then) it stays: a band, so a till settles where its takings pay its posts rather than see-saw. Counted
    // before the treasury's wage support: what the till itself could pay.
    if (rule("wageDrift") <= 0 || owed <= 0)
        return;
    auto& w = state_.loop.wages[till];
    ++w.days;
    w.shortDays += paid < owed;
    w.owed += owed;
    w.profit += profit;
    if (w.days < rule("wageDriftDays"))
        return;
    // (The step within the wider of the keeper's bound and a hand's: each post's wage within its own.)
    const int most = std::max(rule("wageDriftMost"), rule("wageDriftHandMost"));
    if (w.shortDays * 2 > w.days)
    {
        if (wageFalls(*this, w.step))
            --w.step;                               // (Only while that lowers some post.)
    }
    else if (w.shortDays == 0 && w.profit * 100 >= w.owed * rule("wageDriftSpare") && w.step < most)
        ++w.step;
    w.step = std::clamp(w.step, -most, most);
    w.days = w.shortDays = 0;
    w.owed = w.profit = 0;
}

void Society::followTreasury(std::int64_t day)
{
    // The gate toll and the alms follow the treasury's level (the fourth rule), after the surplus each night: its level is
    // a slow mean of what it holds at 18:00 (over about treasuryDays days: a season's swing mostly smoothed away), judged
    // every treasuryJudgeDays days against treasuryTarget. Over the target and its band and not falling: the toll eases by
    // tollStep tenths of a point and the alms rise a penny a head; under it and not rising: the toll firms and the alms
    // tighten; inside the band, or already moving back toward it, nothing (so a change has time to tell before the next).
    // The toll within tollDriftMost points of the world's; the alms up to almsDriftMost pennies over almsADay, never under
    // it (the world's alms are the line under hunger: tightened, they come back down to it and no further).
    //
    // How it shares the work with the surplus (spendSurplus): the surplus is the ceiling, each night, at once (what is
    // over the cap and its reserve goes out); this rule is the slow trim of the flows under it, so the treasury sits about
    // its target with the surplus clipping only a good season's top. A treasury that keeps reaching its ceiling eases the
    // toll (less taken in) and raises the alms (more given out) until it no longer does; one that keeps falling short
    // (a shrinking toll as the Country's goods move to the city's own makers) firms the toll before the cap's wage support
    // and the alms run dry.
    if (rule("treasuryDrift") <= 0 || !account("treasury"))
        return;
    auto& loop = state_.loop;
    const double now = double(cash("treasury"));
    if (loop.level < 0 || loop.judged < 0 || day + 3650 < loop.judged)
    {
        // Its first night: the level begins at what it holds, is the plain mean of its nights until it has treasuryDays of
        // them (so a new world's treasury, starting low and filling in its first weeks, weighs no more than any other
        // night), and is first judged once it has had them: not a fall to correct.
        loop.level = loop.levelThen = now;
        loop.levelDays = 1;
        loop.judged = day + std::max(0, rule("treasuryDays") - rule("treasuryJudgeDays"));
        return;
    }
    loop.levelDays = std::min(loop.levelDays + 1, 3650);
    loop.level += (now - loop.level) / std::min(loop.levelDays, rule("treasuryDays"));
    if (day - loop.judged < rule("treasuryJudgeDays"))
        return;
    const double target = rule("treasuryTarget"), band = target * rule("treasuryBand") / 100.;
    int toward = 0;                                 // -1: the toll eases, the alms rise; 1: the toll firms, the alms tighten.
    if (loop.level > target + band && loop.level >= loop.levelThen)
        toward = -1;
    else if (loop.level < target - band && loop.level <= loop.levelThen)
        toward = 1;
    if (toward != 0)
    {
        const double most = rule("tollDriftMost");
        loop.toll = std::clamp(loop.toll + toward * rule("tollStep") / 10., -most, most);
        const int almsMost = rule("almsDriftMost");
        loop.alms = std::clamp(loop.alms - toward, 0, almsMost);
    }
    loop.judged = day;
    loop.levelThen = loop.level;
}
} // namespace ratw
