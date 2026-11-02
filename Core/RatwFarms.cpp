// The Country's food farms (Docs/Design/71-the-country-s-industries.md, 4, Phases A and B; the user, 2026-11-01: "Yes to
// all your suggestions"): modelled producers with virtual crews, before any NPC is placed. Each farm is a till of its own
// and its crew a household that shops in the city; an area switched on takes its goods from the Country, its farms
// selling them to the shops' store draws from their stock. Every coin moves by shift or sale but the founding (a farm's
// float and its crew's settled purse, made as the world's founding money was and counted as minted), so the money stays
// conserved; goods are grown on the farms (create) and leave by being sold, eaten at the Country's table or by the crews,
// spoiling, or being used at the farm (the basket).
#include "RatwItems.h"
#include "RatwSociety.h"

#include <algorithm>
#include <cmath>

namespace ratw
{
namespace
{
// A world's own setting where it has one (0 or more), else the next level's.
int orElse(int own, int otherwise)
{
    return own >= 0 ? own : otherwise;
}
} // namespace

void Society::buildFarms()
{
    // The farms as this world has them: country_farms.json's, with the world's own changes (EconomySpec::farms); a farm's
    // own setting, then its area's, then the defaults. What each carries (its fractions of a piece) is kept.
    const auto& defaults = items::farms();
    const auto& own = authored_.economy.farms;
    std::map<std::string, FarmNow> was;
    for (auto& f : farms_)
        was.emplace(f.def->id, std::move(f));
    farms_.clear();
    farmOf_.clear();
    areaOfGood_.clear();
    for (std::size_t a = 0; a < defaults.areas.size(); ++a)
    {
        const auto& area = defaults.areas[a];
        for (const auto& good : area.goods)
            areaOfGood_.emplace(good, a);
        const auto oa = own.areas.find(area.id);
        const EconomySpec::Farms::Area none{};
        const auto& ownArea = oa == own.areas.end() ? none : oa->second;
        const int areaYield = orElse(ownArea.yieldScale, area.yieldScale), areaCrew = orElse(ownArea.crewScale, area.crewScale);
        // The defaults' wages, then the area's (as the file has them, then the world's own).
        const int keeperOfArea = orElse(ownArea.keeper, orElse(area.keeper, orElse(own.keeper, defaults.keeper)));
        const int handOfArea = orElse(ownArea.hand, orElse(area.hand, orElse(own.hand, defaults.hand)));
        for (const auto& def : area.farms)
        {
            const auto of = own.farms.find(def.id);
            const EconomySpec::Farms::Farm noFarm{};
            const auto& ownFarm = of == own.farms.end() ? noFarm : of->second;
            auto found = was.find(def.id);
            FarmNow f = found == was.end() ? FarmNow{} : std::move(found->second);
            f.def = &def;
            f.area = a;
            f.till = farmTill(def.id);
            f.crew = farmCrew(def.id);
            f.yieldScale = areaYield / 100. * orElse(ownFarm.yieldScale, 100) / 100.;
            f.keepers = def.keepers;
            // Its hands times its area's crew scale and its own, rounded half up (doc 71, 4).
            f.hands = int(std::floor(def.hands * (areaCrew / 100.) * (orElse(ownFarm.crewScale, 100) / 100.) + .5));
            f.keeper = orElse(ownFarm.keeper, orElse(def.keeper, keeperOfArea));
            f.hand = orElse(ownFarm.hand, orElse(def.hand, handOfArea));
            // Its float: floatDays of its wages now, and the design's lean season's shortfall on top (what its reserve is
            // over floatDays of the design's wages: none has one at the defaults). Its crew's settled purse, by its size.
            const std::int64_t designWages = std::int64_t(def.keepers) * orElse(def.keeper, orElse(area.keeper, defaults.keeper)) +
                                             std::int64_t(def.hands) * orElse(def.hand, orElse(area.hand, defaults.hand));
            const auto lean = std::max<std::int64_t>(0, def.reserve - defaults.floatDays * designWages);
            f.reserve = defaults.floatDays * f.wages() + lean;
            const int designHeads = def.keepers + def.hands;
            f.purse = designHeads > 0 ? std::int64_t(std::llround(double(def.purse) * f.heads() / designHeads)) : 0;
            farmOf_[f.till] = farms_.size();
            farms_.push_back(std::move(f));
        }
    }
    refreshAreas(-1, false);                         // (Which are on as of the day it knows; none founded here.)
}

void Society::refreshAreas(std::int64_t day, bool found)
{
    // Today's areas (the schedule moves on with the day: an area's place in the order, `every` days apart from `start`;
    // an area's own switch over it), and, with `found`, the farms of an area on founded where they are not yet. Without a
    // day, the last the farms were tended (or the society's, once it knows the date).
    if (day >= 0)
        farmsDay_ = day;
    else
        day = farmsDay_ >= 0 ? farmsDay_ : seasonDay_;
    const auto& defaults = items::farms();
    const auto& own = authored_.economy.farms;
    areaOn_.assign(defaults.areas.size(), 0);
    const int start = own.start == -2 ? defaults.start : own.start;
    const int every = orElse(own.every, defaults.every);
    const auto& order = own.order.empty() ? defaults.order : own.order;
    for (std::size_t a = 0; a < defaults.areas.size(); ++a)
    {
        const auto& area = defaults.areas[a];
        const auto oa = own.areas.find(area.id);
        int on = oa != own.areas.end() && oa->second.on >= 0 ? oa->second.on : area.on;
        if (on < 0)
        {
            const auto at = std::find(order.begin(), order.end(), area.id);
            on = start >= 0 && day >= 0 && at != order.end() && day >= start + std::int64_t(at - order.begin()) * every;
        }
        areaOn_[a] = char(on > 0);
    }
    if (!found || roster_ != Roster::Authored)
        return;
    for (auto& f : farms_)
        if (farmAreaOn(f.area) && (!account(f.till) || !account(f.crew)))
            foundFarm(f);
}

void Society::foundFarm(FarmNow& f)
{
    // A farm's founding (doc 71, 3, "The money the switch holds"; the user, 2026-11-01): its till opened with its float and
    // its crew's household with its settled purse, made as the world's founding money was ("initial funding") and counted
    // as minted, so the money stays conserved; and its store a week of its yields (at most what a perishable good keeps).
    const bool fresh = !account(f.till);
    if ((!account(f.till) && !openAccount(f.till)) || (!account(f.crew) && !openAccount(f.crew)))
        return;
    if (!fresh)
        return;                                     // (Its crew's household alone was missing: a save's; nothing made.)
    const auto mint = [&](const std::string& id, std::int64_t coins, const char* kind) {
        if (coins <= 0)
            return;
        state_.accounts.at(id).cash += coins;
        state_.minted += coins;
        loopToday_.minted += coins;
        farmsMinted_ += coins;
        record(kind, "made", id, "", 0, coins);
    };
    mint(f.till, f.reserve, "a farm's founding money");
    mint(f.crew, f.purse, "a farm crew's founding purse");
    const auto& defaults = items::farms();
    for (const auto& [good, middle] : f.def->yield)
    {
        const auto* item = items::good(good);
        const double keeps = item ? item->keeps : 0;
        const double days = keeps > 0 ? std::min<double>(defaults.openingStockDays, keeps) : defaults.openingStockDays;
        const int n = int(std::floor(seasonalValue(middle, yearDay_) * f.yieldScale * days + 1e-9));
        for (int left = n; left > 0; left -= 99)
            if (!create(f.till, good, std::min(left, 99), "grown at " + f.def->id))
                break;
    }
}

void Society::tendFarms(std::int64_t day)
{
    // The farms' day (doc 71, 4, Phase B: "Production"), at the first loop hour of each day: the areas as of today; each
    // on farm grows its yields (the season's, blended, times its yield scale; fractions carried), the Country's own people
    // eat at its table (its `table`: no dependants until families are placed), and what is past keeping spoils (more than
    // its keeping days of the good's yield); and its crew eats a day's food from what its household holds.
    refreshAreas(day);
    if (roster_ != Roster::Authored)
        return;
    for (auto& f : farms_)
    {
        if (!farmAreaOn(f.area) || !account(f.till))
            continue;
        auto& today = loopToday_.farms[f.def->id];
        const auto& name = f.def->id;
        for (const auto& [good, middle] : f.def->yield)
        {
            double& carry = f.grown[good];
            carry += seasonalValue(middle, yearDay_) * f.yieldScale;
            const int n = int(std::floor(carry + 1e-9));
            if (n < 1)
                continue;
            carry -= n;
            int made = 0;
            for (int left = n; left > 0; left -= 99)
                if (const int lot = std::min(left, std::min(99, StockCap - stock(*account(f.till), good)));
                    lot > 0 && create(f.till, good, lot, "grown at " + name))
                    made += lot;
                else
                    break;
            today.grown += made;
            today.spoiled += n - made;              // (Its store full: what it couldn't keep.)
        }
        for (const auto& [good, middle] : f.def->table)
        {
            double& carry = f.eaten[good];
            carry += seasonalValue(middle, yearDay_);
            const int n = int(std::floor(carry + 1e-9));
            if (n < 1)
                continue;
            carry -= n;
            const int got = consume(f.till, good, n, "eaten in the Country");
            today.eaten += got;
            today.tableShort += n - got;
        }
        for (const auto& [good, middle] : f.def->yield)
        {
            const auto* item = items::good(good);
            if (!item || item->keeps <= 0)
                continue;
            const double mean = (middle[0] + middle[1] + middle[2] + middle[3]) / 4 * f.yieldScale;
            const int keep = std::max(1, int(std::ceil(item->keeps * mean - 1e-9)));
            if (const int have = stock(*account(f.till), good); have > keep)
                today.spoiled += consume(f.till, good, have - keep, "spoiled");
        }
        // The crew eats (a household of its size: NourishADay a head), the soonest-spoiling food first; what it can't eat
        // for want of food is owed to tomorrow, a day's at most (it eats a piece over, rather than leave a want).
        if (const auto* larder = account(f.crew); larder && f.heads() > 0)
        {
            const double aDay = f.heads() * NourishADay;
            f.hunger = std::min(aDay, f.hunger) + aDay;
            while (f.hunger > 0)
            {
                const auto food = eatFirst(*account(f.crew));
                const int each = food.empty() ? 0 : nourishment(food);
                if (each < 1)
                    break;
                const int n = std::clamp(int(std::floor(f.hunger / each)), 1, stock(*account(f.crew), food));
                const int got = consume(f.crew, food, n, "eaten by a farm's crew");
                if (got < 1)
                    break;
                f.hunger -= double(got) * each;
            }
            if (f.hunger > 0)
                today.crewHungry += f.hunger;
        }
    }
}

void Society::payFarms()
{
    // The farms' wages at 18:00 (doc 71, 4; the user: fixed, a keeper 16p and a hand 10p, editable per farm and area), to
    // their crews' households, from all the till holds (its float is for them): what it can't pay goes to the treasury's
    // wage support with the city's short tills' (supportWages).
    for (const auto& f : farms_)
    {
        if (!farmAreaOn(f.area) || !account(f.till) || !account(f.crew))
            continue;
        const auto owed = f.wages();
        if (owed <= 0)
            continue;
        auto& today = loopToday_.farms[f.def->id];
        auto paid = std::min(owed, cash(f.till));
        if (paid > 0 && !shift(f.till, f.crew, "", 0, paid, "wages"))
            paid = 0;
        today.wagesOwed += owed;
        today.wagesPaid += paid;
        if (paid < owed)
            tillsShort_[f.till] = owed - paid;      // (For the treasury's wage support, with the city's short tills.)
    }
}

void Society::farmsBuy(int hod)
{
    // The farms' basket (doc 71, 2.6: what each takes in over its crew's wages, about half its takings): each shop hour,
    // what its till holds over its float and tonight's wages, spread over the hours left, on the city's goods a farm uses
    // (country_farms.json `basket`: carts, barrels and hoops, tools, rope and nets, building goods), from their makers'
    // open shops, as the Country buys; it pays the carriage out.
    const auto& goods = items::farms().basket;
    if (goods.empty() || std::none_of(farms_.begin(), farms_.end(), [&](const FarmNow& f) { return farmAreaOn(f.area) && account(f.till); }))
        return;
    std::vector<std::pair<const std::string*, double>> basket;
    for (const auto& [item, share] : goods)
        basket.push_back({&item, double(share)});
    const auto open = openShops();
    for (const auto& f : farms_)
    {
        if (!farmAreaOn(f.area) || !account(f.till))
            continue;
        const auto keep = f.reserve + (hod < PayHour ? f.wages() : 0);
        const auto spare = cash(f.till) - keep;
        if (spare <= 0)
            continue;
        auto& today = loopToday_.farms[f.def->id];
        today.basket += basketBuys(f.till, basket, double(spare) / double(ShopShut - hod), keep, hod, open, "to a farm",
                                   "used at the farm", &today.basketCarriage);
    }
}

int Society::farmAreaOf(const std::string& item) const
{
    if (areaOfGood_.empty())
        return -1;
    const auto found = areaOfGood_.find(items::baseOf(item));
    return found != areaOfGood_.end() && farmAreaOn(found->second) ? int(found->second) : -1;
}

int Society::farmsHold(const std::string& item) const
{
    const int area = farmAreaOf(item);
    if (area < 0)
        return 0;
    int held = 0;
    for (const auto& f : farms_)
        if (f.area == std::size_t(area))
            if (const auto* till = account(f.till))
                held += stock(*till, item);
    return held;
}

double Society::farmPriceFactor(const std::string& item) const
{
    const int area = farmAreaOf(item);
    if (area < 0)
        return 0;
    const int f = items::farms().areas[std::size_t(area)].priceFactor;
    const auto own = authored_.economy.farms.areas.find(items::farms().areas[std::size_t(area)].id);
    const int factor = own != authored_.economy.farms.areas.end() && own->second.priceFactor >= 0 ? own->second.priceFactor : f;
    return factor > 0 ? factor / 100. : 0;
}

double Society::countryBasketShift() const
{
    // The share of the food areas' takings (the design's, country_farms.json) whose areas are on.
    const auto& areas = items::farms().areas;
    double on = 0, all = 0;
    for (std::size_t a = 0; a < areas.size(); ++a)
    {
        all += areas[a].takings;
        if (farmAreaOn(a))
            on += areas[a].takings;
    }
    return all > 0 ? std::clamp(on / all, 0., 1.) : 0.;
}

bool Society::farmSwitchTill(const std::string& till) const
{
    // A till the switch moves orders from or to (doc 71, 2.6): a farm's, or a shop of a trade that loses the Country's.
    if (farmOf_.count(till))
        return true;
    const auto& trades = items::farms().switchTrades;
    if (trades.empty() || till.rfind("till:", 0) != 0)
        return false;
    const auto* p = position(till.substr(5));
    const auto* business = p ? items::businessFor(p->title) : nullptr;
    return business && std::find(trades.begin(), trades.end(), business->id) != trades.end();
}
} // namespace ratw
