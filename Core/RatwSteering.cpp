// Steering the economy (Docs/Design/74-steering-the-economy.md): one number, the push (-1..1), that a few levers follow
// toward where the DM's slider says the money should rest; the skim, which takes money off the top of whoever is richest
// at that moment; and the two floors it pays for, Never Hungry and Never Broke. Every coin moves by shift between
// accounts that exist (fund:relief only passes money through), so the money is conserved; nothing here is random.
#include "RatwItems.h"
#include "RatwSociety.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>

namespace ratw
{
namespace
{
constexpr double None = std::numeric_limits<double>::quiet_NaN();

// What an account holds to eat, in nourishment (as the city's loop counts it).
double eatable(const EconomyAccount& a)
{
    double n = 0;
    for (const auto& [item, count] : a.stock)
        if (count > 0)
            n += double(Society::nourishment(item)) * count;
    return n;
}

// The House a till's post works for: a country workshop's, or a House's city business's ("" a keeper's own).
std::string houseOf(const Position* p)
{
    if (!p)
        return {};
    if (const auto* w = items::farmPeople().workshop(p->work.cell))
        return w->house;
    const auto& owned = items::cityLoop().houseTills;
    const auto found = owned.find(p->work.cell);
    return found == owned.end() ? std::string() : found->second;
}

double gini(std::vector<double> v)
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

std::string fixed(double v, int places)
{
    std::ostringstream o;
    o << std::fixed << std::setprecision(places) << v;
    return o.str();
}

// The levers' names in words (the DM tab's and the reasons').
const char* leverName(const std::string& lever)
{
    static const std::map<std::string, const char*> names = {
        {"shareOut", "the share-out"},       {"postsShare", "the hands' share"},    {"tollLean", "the toll lean"},
        {"surplusKeep", "the treasury's keep"}, {"treasuryCap", "the treasury's cap"}, {"surplusAlmsTo", "the poorest raised to"},
        {"groundRent", "the ground rent"},   {"rentScale", "the rent scale"},        {"dividend", "the Houses' dividend"},
        {"richAbove", "the rich hold"}};
    const auto found = names.find(lever);
    return found == names.end() ? "a lever" : found->second;
}
} // namespace

bool Society::steering() const
{
    return state_.loop.steer.on && ownRule("steer") > 0;
}

double Society::steerSlider() const
{
    return std::clamp((ownRule("steerLean") - 100) / 100., -1., 1.);
}

std::pair<double, double> Society::steerBand() const
{
    const double x = steerSlider();
    const double mid = ownRule("steerWidthMid") / 100., end = ownRule("steerWidthEnd") / 100.;
    const double w = mid + (end - mid) * std::abs(x);
    return {std::max(-1., x - w), std::min(1., x + w)};
}

double Society::steerLever(const std::string& lever) const
{
    double own = 0;
    if (lever == "surplusKeep" || lever == "treasuryCap" || lever == "surplusAlmsTo")
        own = ownRule(lever);
    else if (lever == "richAbove")
        own = comfortsAbove();
    else if (lever == "rentScale")
        own = 100;
    const auto& levers = items::cityLoop().steering.levers;
    const auto found = levers.find(lever);
    if (!steering() || found == levers.end())
        return own;
    const double v = items::steerAt(found->second, state_.loop.steer.push, own);
    return std::isnan(v) ? own : v;
}

double Society::steerTarget(const std::string& target) const
{
    const auto& targets = items::cityLoop().steering.targets;
    const auto found = targets.find(target);
    return found == targets.end() ? None : items::steerAt(found->second, steerSlider(), None);
}

double Society::giniEarners() const
{
    // As world_check's gini_earners: the residents' homes (each alone in a communal one) with a member holding a post that
    // pays, by what their members hold.
    std::set<std::string> earners;
    for (const auto& p : positions_)
        if (const auto held = state_.careers.positions.find(p.id);
            held != state_.careers.positions.end() && !held->second.holder.empty() && !payOf(p).kind.empty())
            earners.insert(held->second.holder);
    std::map<std::string, std::vector<std::string>> homes;
    for (const auto& [id, life] : state_.residents)
        if (!dead_.count(id) && spec(id))
            homes[communalHome(life.homeCell) ? life.homeCell + "|" + id : life.homeCell].push_back(id);
    std::vector<double> wealth;
    for (const auto& [home, members] : homes)
        if (std::any_of(members.begin(), members.end(), [&](const std::string& m) { return earners.count(m) > 0; }))
        {
            double w = 0;
            for (const auto& m : members)
                w += double(cash(m));
            wealth.push_back(w);
        }
    return gini(wealth);
}

double Society::upperShare()
{
    const auto all = moneySupply();
    if (all <= 0)
        return 0;
    std::int64_t upper = 0;
    for (const auto& [home, members] : households())
        if (homeClass(home, members) == "upper")
            for (const auto& m : members)
                upper += cash(m);
    if (estatesOn())
    {
        for (const auto& e : items::cityLoop().estates)
            if (const auto f = estateFund(e); account(f))
                upper += cash(f);
    }
    else
        upper += cash(WharfFund);
    for (const auto& p : positions_)
        if (!houseOf(&p).empty())
            upper += cash("till:" + p.id);
    return 100. * double(upper) / double(all);
}

double Society::top10Share() const
{
    std::vector<std::int64_t> purses;
    std::int64_t total = 0;
    for (const auto& [home, members] : households())
    {
        std::int64_t held = 0;
        for (const auto& m : members)
            held += cash(m);
        purses.push_back(held);
        total += held;
    }
    if (purses.empty() || total <= 0)
        return 0;
    std::sort(purses.rbegin(), purses.rend());
    const auto n = (purses.size() + 9) / 10;
    std::int64_t top = 0;
    for (std::size_t i = 0; i < n; ++i)
        top += purses[i];
    return 100. * double(top) / double(total);
}

std::int64_t Society::necessitiesAHead() const
{
    // The alms line (a day's food money), the reserve a household keeps for food, and a day's share of its needs (each
    // need's cheapest good at the stable price, over its days), in whole pennies, rounded up. (Worked out each time: a
    // DM's change to the alms mid-day, or a save read back, gives the same line as an unbroken run.)
    const auto& h = items::households();
    double needs = 0;
    for (const auto& need : h.needs)
    {
        std::int64_t least = 0;
        for (const auto& g : need.any)
            if (const auto p = stablePrice(g, 1); p > 0 && (least == 0 || p < least))
                least = p;
        if (least > 0 && need.everyDays > 0)
            needs += double(least) / need.everyDays;
    }
    return almsNow() + h.reserve + std::int64_t(std::ceil(needs - 1e-9));
}

std::vector<Society::Spare> Society::spares(bool deep, bool treasury)
{
    // Every holder's spare over its own needs, now (doc 74, 6.1): never a player's purse, the beyond's accounts, the
    // Wharf's credit or fund:relief itself. `deep`: the second pass, a household keeping one day's necessities a head.
    std::vector<Spare> out;
    const auto nec = necessitiesAHead();
    const std::int64_t keepHead = deep ? nec : std::max<std::int64_t>(comfortsAbove(), std::int64_t(rule("skimKeepDays")) * nec);
    for (const auto& [home, members] : households())
    {
        std::int64_t held = 0;
        for (const auto& m : members)
            held += cash(m);
        if (const auto spare = held - keepHead * std::int64_t(members.size()); spare > 0)
            out.push_back({"home " + home, "households: " + homeClass(home, members), spare, members});
    }
    // Tonight's wages owed by each payer, as payWages counts them.
    std::map<std::string, std::int64_t> owed;
    for (const auto& p : positions_)
        if (const auto held = state_.careers.positions.find(p.id);
            held != state_.careers.positions.end() && !held->second.holder.empty() && !dead_.count(held->second.holder))
            if (const auto pay = payOf(p); !pay.kind.empty() && pay.wage > 0)
                owed[pay.payer] += pay.wage;
    const auto owedOf = [&](const std::string& id) {
        const auto found = owed.find(id);
        return found == owed.end() ? std::int64_t(0) : found->second;
    };
    for (const auto& [id, a] : state_.accounts)
    {
        if (id.rfind("till:", 0) != 0 || farmOf_.count(id) || a.cash <= 0)
            continue;
        const auto house = houseOf(position(id.substr(5)));
        const std::int64_t days = house.empty() ? rule("floatWagesDays") : rule("workshopWagesDays");
        if (const auto spare = a.cash - tillFloat(id) - (days + 1) * owedOf(id); spare > 0)
            out.push_back({id, house.empty() ? "shops" : "House businesses", spare, {}});
    }
    for (const auto& f : farms_)
        if (account(f.till) && farmAreaOn(f.area))
            if (const auto spare = cash(f.till) - f.reserve - std::max<std::int64_t>(0, farmBasketMost(f)) - f.wages() - owedOf(f.till);
                spare > 0)
                out.push_back({f.till, "farms", spare, {}});
    // The estates: each its spare over its keep (its posts' nights, the Wharf's reserve, the Crown's grants), and all of
    // them together never under the one purse's keep.
    const auto estateOwed = estatePostsOwed();
    if (estatesOn())
    {
        std::int64_t all = 0, posts = 0;
        for (const auto& e : items::cityLoop().estates)
            if (const auto f = estateFund(e); account(f))
            {
                all += cash(f);
                if (const auto found = estateOwed.find(f); found != estateOwed.end())
                    posts += found->second;
            }
        auto pool = all - authored_.economy.wharfReserve - std::int64_t(std::max(1, rule("estatePostsDays"))) * posts;
        for (const auto& e : items::cityLoop().estates)
            if (const auto f = estateFund(e); account(f) && pool > 0)
                if (const auto spare = std::min(pool, estateSpare(f, estateOwed)); spare > 0)
                {
                    out.push_back({f, "estates: " + e.house, spare, {}});
                    pool -= spare;
                }
    }
    else if (account(WharfFund))
        if (const auto spare = estateSpare(WharfFund, estateOwed); spare > 0)
            out.push_back({WharfFund, "estates", spare, {}});
    // The treasury: its spare over its cap and tonight's wage support; only once that is known (18:00), so never in Never
    // Hungry's 17:00 skim, before the night's wage support has had what is over the cap.
    if (treasury && account("treasury"))
        if (const auto spare = cash("treasury") - std::max<std::int64_t>(rule("treasuryCap"), TreasuryFloat) - loopToday_.supportWanted;
            spare > 0)
            out.push_back({"treasury", "treasury", spare, {}});
    std::sort(out.begin(), out.end(), [](const Spare& a, const Spare& b) { return a.spare != b.spare ? a.spare > b.spare : a.id < b.id; });
    return out;
}

namespace
{
// What each holder gives so that the most it gives (`caps`) and the level `L` take `sum` from the top down: the holders
// over L each down to max(L, its spare less its cap), the last pennies in rank order. Short: all the caps.
std::vector<std::int64_t> waterFill(const std::vector<Society::Spare>& holders, const std::vector<std::int64_t>& caps, std::int64_t sum)
{
    const auto at = [&](std::int64_t level) {
        std::int64_t t = 0;
        for (std::size_t i = 0; i < holders.size(); ++i)
            t += std::min(caps[i], std::max<std::int64_t>(0, holders[i].spare - level));
        return t;
    };
    std::vector<std::int64_t> take(holders.size(), 0);
    if (at(0) <= sum)
    {
        for (std::size_t i = 0; i < holders.size(); ++i)
            take[i] = std::min(caps[i], holders[i].spare);
        return take;
    }
    std::int64_t low = 0, high = holders.empty() ? 0 : holders.front().spare;   // (at(low) > sum; at(high) == 0.)
    while (high - low > 1)
    {
        const auto mid = low + (high - low) / 2;
        (at(mid) > sum ? low : high) = mid;
    }
    // At `high` it takes no more than the sum; at `low`, more: each holder at most a penny more.
    std::int64_t taken = 0;
    for (std::size_t i = 0; i < holders.size(); ++i)
        taken += (take[i] = std::min(caps[i], std::max<std::int64_t>(0, holders[i].spare - high)));
    for (std::size_t i = 0; i < holders.size() && taken < sum; ++i)
        if (std::min(caps[i], std::max<std::int64_t>(0, holders[i].spare - low)) > take[i])
        {
            ++take[i];
            ++taken;
        }
    return take;
}
} // namespace

std::int64_t Society::skim(std::int64_t sum, const std::string& purpose, bool treasury)
{
    if (sum <= 0)
        return 0;
    if (!account(ReliefFund) && !openAccount(ReliefFund))
        return 0;
    // The first pass: at most skimMost percent of each holder's spare; the second, short of it, the whole of the spare
    // (a household's line down to a day's necessities a head).
    auto holders = spares(false, treasury);
    const int most = std::clamp(rule("skimMost"), 1, 100);
    std::vector<std::int64_t> caps;
    std::int64_t room = 0;
    for (const auto& h : holders)
        room += caps.emplace_back(h.spare * most / 100);
    if (room < sum)
    {
        holders = spares(true, treasury);
        caps.clear();
        for (const auto& h : holders)
            caps.push_back(h.spare);
    }
    const auto take = waterFill(holders, caps, sum);
    std::int64_t taken = 0;
    for (std::size_t i = 0; i < holders.size(); ++i)
    {
        auto left = take[i];
        if (left <= 0)
            continue;
        const auto& h = holders[i];
        std::int64_t gave = 0;
        if (!h.members.empty())
        {
            // A household pays from the member with the most first.
            auto members = h.members;
            std::sort(members.begin(), members.end(), [&](const std::string& a, const std::string& b) {
                return cash(a) != cash(b) ? cash(a) > cash(b) : a < b;
            });
            for (const auto& m : members)
                if (const auto c = std::min(left - gave, cash(m)); c > 0 && shift(m, ReliefFund, "", 0, c, "skim: " + purpose))
                    gave += c;
        }
        else if (shift(h.id, ReliefFund, "", 0, left, "skim: " + purpose))
            gave = left;
        if (gave <= 0)
            continue;
        taken += gave;
        loopToday_.skimBy[h.group] += gave;
        loopToday_.skimPayers[h.id] += gave;
    }
    loopToday_.skim += taken;
    loopToday_.skimFor[purpose] += taken;
    loopToday_.skimShort += std::max<std::int64_t>(0, sum - taken);
    return taken;
}

void Society::neverHungry(std::int64_t day)
{
    // Never Hungry (doc 74, 7.1): at the shops' last hour, after everyone's own buying, each member of a household holding
    // under a day's food (shortDays: the line that marks a household short) is given food up to it and no more, in its own
    // stock, bought at the stable price from an open shop's real stock in its community (else any open one), the cheapest
    // nourishment first; with no shop's food in reach, ordered from the Wharf within its valve's rules. The whole hour's
    // bill is worked out first, the skim raises it from the richest, and fund:relief pays the tills.
    (void)day;
    const double line = items::cityLoop().shortDays * NourishADay;
    struct Lot
    {
        std::string member, till, item;
        int n = 0;
        std::int64_t each = 0;
    };
    std::vector<Lot> lots;
    std::vector<std::string> shortHomes;
    const auto open = openShops();
    std::map<std::string, std::vector<std::string>> byCommunity;   // Community -> its open tills (with food).
    std::vector<std::string> all;
    for (const auto& [till, business] : open)
    {
        const auto* a = account(till);
        if (!a || !hasFood(*a))
            continue;
        const auto* p = position(till.substr(5));
        byCommunity[p && day_.communityOf ? day_.communityOf(p->work.cell) : std::string()].push_back(till);
        all.push_back(till);
    }
    std::map<std::pair<std::string, std::string>, int> planned;    // (Till, food) -> pieces already in the batch.
    // The Wharf's food, the cheapest nourishment first (only what it lands now).
    std::vector<std::string> wharfFoods;
    for (const auto& [good, w] : items::cityLoop().wharf.imports)
        if (edible(good) && !luxury(good) && nourishment(good) > 0 && wharfImports(good))
            wharfFoods.push_back(good);
    const auto worth = [&](const std::string& g) { return double(stablePrice(g, 1)) / std::max(1, nourishment(g)); };
    std::sort(wharfFoods.begin(), wharfFoods.end(), [&](const std::string& a, const std::string& b) {
        return worth(a) != worth(b) ? worth(a) < worth(b) : a < b;
    });
    std::vector<Lot> wharfLots;
    std::int64_t bill = 0;
    const auto homes = households();
    for (const auto& [home, members] : homes)
    {
        const auto& community = day_.communityOf && state_.residents.count(members.front())
                                    ? day_.communityOf(state_.residents.at(members.front()).homeCell) : std::string();
        const auto here = byCommunity.find(community);
        const auto& shops = here != byCommunity.end() && !here->second.empty() ? here->second : all;
        bool helped = false;
        for (const auto& m : members)
        {
            const auto* wallet = account(m);
            if (!wallet)
                continue;
            double have = eatable(*wallet);
            for (int k = 0; k < 8 && have < line; ++k)
            {
                // The cheapest nourishment on these shops' shelves, not yet in the batch.
                const std::string* bestTill = nullptr;
                std::string bestFood;
                double bestWorth = 0;
                int bestLeft = 0;
                for (const auto& till : shops)
                    for (const auto& [item, n] : account(till)->stock)
                    {
                        if (n <= 0 || !edible(item) || luxury(item) || nourishment(item) <= 0)
                            continue;
                        const auto it = planned.find({till, item});
                        const int left = n - (it == planned.end() ? 0 : it->second);
                        const double w = worth(item);
                        if (left > 0 && (!bestTill || w < bestWorth || (w == bestWorth && (item < bestFood || (item == bestFood && till < *bestTill)))))
                        {
                            bestTill = &till;
                            bestFood = item;
                            bestWorth = w;
                            bestLeft = left;
                        }
                    }
                if (!bestTill)
                {
                    if (wharfFoods.empty())
                        break;
                    const auto& food = wharfFoods.front();
                    const int n = std::max(1, int(std::ceil((line - have) / nourishment(food) - 1e-9)));
                    wharfLots.push_back({m, "", food, n, 0});
                    bill += stablePrice(food, n) + carriageSplit(ReliefFund, food, n, false).first;
                    have += double(n) * nourishment(food);
                    helped = true;
                    break;
                }
                const int n = std::min(bestLeft, std::max(1, int(std::ceil((line - have) / nourishment(bestFood) - 1e-9))));
                const auto each = stablePrice(bestFood, 1);
                lots.push_back({m, *bestTill, bestFood, n, each});
                planned[{*bestTill, bestFood}] += n;
                bill += each * n;
                have += double(n) * nourishment(bestFood);
                helped = true;
            }
        }
        if (helped)
            shortHomes.push_back(home);
    }
    if (bill > 0)
        skim(bill - cash(ReliefFund), "never hungry", false);
    std::set<std::string> given;
    for (const auto& l : lots)
    {
        if (cash(ReliefFund) < l.each * l.n || !sale(l.till, ReliefFund, l.item, l.n, l.each, "relief: food at the stable price"))
            continue;
        if (!shift(ReliefFund, l.member, l.item, l.n, 0, "relief: food"))
            continue;
        loopToday_.neverHungryPieces += l.n;
        loopToday_.neverHungryPennies += l.each * l.n;
        loopToday_.relief["food"] += l.each * l.n;
        given.insert(l.member);
    }
    for (const auto& l : wharfLots)
    {
        const auto before = cash(ReliefFund);
        if (!orderFromWharf(ReliefFund, l.item, l.n, "relief: food from the Wharf").ok)
            continue;
        const auto paid = before - cash(ReliefFund);
        if (!shift(ReliefFund, l.member, l.item, l.n, 0, "relief: food"))
            continue;
        loopToday_.neverHungryPieces += l.n;
        loopToday_.neverHungryPennies += paid;
        loopToday_.neverHungryWharfPieces += l.n;
        loopToday_.neverHungryWharfPennies += paid;
        loopToday_.relief["food"] += paid;
        given.insert(l.member);
    }
    // The households still short after it (none, unless the skim or the shops fell short).
    int after = 0;
    for (const auto& [home, members] : homes)
        after += std::any_of(members.begin(), members.end(), [&](const std::string& m) {
            const auto* a = account(m);
            return a && eatable(*a) < line;
        });
    loopToday_.householdsShortAfter = after;
    for (const auto& home : shortHomes)
    {
        const auto& members = homes.at(home);
        if (std::any_of(members.begin(), members.end(), [&](const std::string& m) { return given.count(m) > 0; }))
            ++loopToday_.neverHungryHomes;
    }
}

void Society::neverBroke()
{
    // Never Broke (doc 74, 7.2): at 18:00, after the day's wages and the city's alms, a household whose purses together are
    // under its next day's necessities (necessitiesAHead a head) is topped up to exactly that, paid to the member with the
    // least; the night's top-ups added up first, raised by the skim from the richest, and paid out of fund:relief.
    const auto nec = necessitiesAHead();
    struct Short
    {
        std::string home;
        const std::vector<std::string>* members;
        std::int64_t held, heads;
    };
    const auto homes = households();
    std::vector<Short> shorts;
    std::int64_t bill = 0;
    for (const auto& [home, members] : homes)
    {
        std::int64_t held = 0;
        for (const auto& m : members)
            held += cash(m);
        const auto heads = std::int64_t(members.size());
        if (held < nec * heads)
        {
            shorts.push_back({home, &members, held, heads});
            bill += nec * heads - held;
        }
    }
    if (bill <= 0)
        return;
    skim(bill - cash(ReliefFund), "never broke");
    std::sort(shorts.begin(), shorts.end(), [](const Short& a, const Short& b) {
        return a.held * b.heads != b.held * a.heads ? a.held * b.heads < b.held * a.heads : a.home < b.home;
    });
    for (const auto& s : shorts)
    {
        const auto coins = std::min(nec * s.heads - s.held, cash(ReliefFund));
        if (coins <= 0)
            break;
        const auto poorest = *std::min_element(s.members->begin(), s.members->end(), [&](const std::string& a, const std::string& b) {
            return cash(a) != cash(b) ? cash(a) < cash(b) : a < b;
        });
        if (shift(ReliefFund, poorest, "", 0, coins, "relief: necessities"))
        {
            loopToday_.neverBrokePennies += coins;
            ++loopToday_.neverBrokeHomes;
            loopToday_.relief["necessities"] += coins;
        }
    }
}

void Society::shareOut()
{
    // The share-out (doc 74, 5.1; the push left of the centre): each household over the mean purse a head gives r percent
    // of what it holds over the mean (never under skimKeepDays of its necessities a head), and the richest tenth of the
    // other holders r percent of their spare (at most skimMost percent of it); all of it into fund:relief, then out at
    // once to the poorest households, per head, poorest first, with no cap. What it can't place stays for tomorrow.
    const double r = std::min(steerLever("shareOut"), 100.);
    if (r <= 0)
        return;
    if (!account(ReliefFund) && !openAccount(ReliefFund))
        return;
    const auto homes = households();
    std::int64_t held = 0, heads = 0;
    for (const auto& [home, members] : homes)
    {
        for (const auto& m : members)
            held += cash(m);
        heads += std::int64_t(members.size());
    }
    if (heads <= 0)
        return;
    const double mean = double(held) / double(heads);
    const auto keep = std::int64_t(rule("skimKeepDays")) * necessitiesAHead();
    std::int64_t raised = 0;
    for (const auto& [home, members] : homes)
    {
        std::int64_t own = 0;
        for (const auto& m : members)
            own += cash(m);
        const auto n = std::int64_t(members.size());
        const double over = double(own) - mean * double(n);
        if (over <= 0)
            continue;
        auto give = std::min(std::int64_t(std::floor(over * r / 100.)), own - std::max<std::int64_t>(keep * n, std::int64_t(std::ceil(mean * double(n)))));
        if (give <= 0)
            continue;
        auto order = members;
        std::sort(order.begin(), order.end(), [&](const std::string& a, const std::string& b) {
            return cash(a) != cash(b) ? cash(a) > cash(b) : a < b;
        });
        std::int64_t gave = 0;
        for (const auto& m : order)
            if (const auto c = std::min(give - gave, cash(m)); c > 0 && shift(m, ReliefFund, "", 0, c, "skim: share-out"))
                gave += c;
        if (gave > 0)
        {
            raised += gave;
            loopToday_.skimBy["households: " + homeClass(home, members)] += gave;
            loopToday_.skimPayers["home " + home] += gave;
        }
    }
    auto others = spares(false);
    others.erase(std::remove_if(others.begin(), others.end(), [](const Spare& s) { return !s.members.empty(); }), others.end());
    const int most = std::clamp(rule("skimMost"), 1, 100);
    for (std::size_t i = 0, top = (others.size() + 9) / 10; i < top; ++i)
    {
        const auto& h = others[i];
        if (const auto c = std::min(std::int64_t(std::floor(double(h.spare) * r / 100.)), h.spare * most / 100);
            c > 0 && shift(h.id, ReliefFund, "", 0, c, "skim: share-out"))
        {
            raised += c;
            loopToday_.skimBy[h.group] += c;
            loopToday_.skimPayers[h.id] += c;
        }
    }
    loopToday_.skim += raised;
    loopToday_.skimFor["share-out"] += raised;
    loopToday_.shareOut += topUpPoorest(cash(ReliefFund), ReliefFund, -1);
}

void Society::housesDividend()
{
    // The Houses' dividend (doc 74, 5.7; the push right of the centre): d percent of each estate's spare over its keep (its
    // posts' nights, the Wharf's reserve, the Crown's grants) to the upper-class households, per head, to the member with
    // the most; nothing while any estate's posts went short in the last 28 nights.
    const double d = std::min(steerLever("dividend"), 100.);
    if (d <= 0)
        return;
    const auto day = loopToday_.day;
    for (const auto& [id, e] : loopToday_.estates)
        if (e.postsPaid < e.postsOwed)
            state_.loop.steer.estateShort = day;
    if (state_.loop.steer.estateShort >= 0 && day - state_.loop.steer.estateShort < 28)
        return;
    std::vector<std::pair<std::string, const std::vector<std::string>*>> upper;
    const auto homes = households();
    std::int64_t heads = 0;
    for (const auto& [home, members] : homes)
        if (homeClass(home, members) == "upper")
        {
            upper.push_back({home, &members});
            heads += std::int64_t(members.size());
        }
    if (heads <= 0)
        return;
    std::vector<std::string> funds;
    if (estatesOn())
    {
        for (const auto& e : items::cityLoop().estates)
            if (const auto f = estateFund(e); account(f))
                funds.push_back(f);
    }
    else if (account(WharfFund))
        funds.push_back(WharfFund);
    const auto owed = estatePostsOwed();
    std::set<std::string> paidHomes;
    for (const auto& f : funds)
    {
        const auto spare = estateSpare(f, owed);
        const auto pay = spare > 0 ? std::int64_t(std::floor(double(spare) * d / 100.)) : 0;
        if (pay < heads)
            continue;
        for (const auto& [home, members] : upper)
        {
            const auto coins = pay * std::int64_t(members->size()) / heads;
            const auto richest = *std::max_element(members->begin(), members->end(), [&](const std::string& a, const std::string& b) {
                return cash(a) != cash(b) ? cash(a) < cash(b) : a > b;
            });
            if (coins > 0 && shift(f, richest, "", 0, coins, "the Houses' dividend"))
            {
                loopToday_.dividend += coins;
                paidHomes.insert(home);
            }
        }
    }
    loopToday_.dividendHomes += int(paidHomes.size());
}

void Society::steerSet(double push, std::int64_t day, const std::string& why, bool judging)
{
    // The push moved (or the toll lean's guard judged): the levers at the new push, the toll lean with its guard (doc 74,
    // 5.3: not lowered under 0 while the treasury's 28-night mean is under treasuryFloor; under treasuryLow, back up half
    // a point a judging), and a line in the history naming what moved.
    auto& st = state_.loop.steer;
    static const char* const Levers[] = {"shareOut", "postsShare", "tollLean", "surplusKeep", "treasuryCap", "surplusAlmsTo",
                                         "groundRent", "rentScale", "dividend", "richAbove"};
    std::map<std::string, double> before;
    for (const char* l : Levers)
        before[l] = steerLever(l);
    const double was = st.push;
    st.push = std::clamp(push, -1., 1.);
    const auto& t = items::cityLoop().steering;
    const double want = steerLever("tollLean");
    const double treasury = st.treasury >= 0 ? st.treasury : double(cash("treasury"));   // (Before its mean: tonight's.)
    double lean = want;
    if (treasury < t.treasuryLow && st.tollLean < 0)
        lean = std::min(0., st.tollLean + .5);
    else if (treasury < t.treasuryFloor && want < 0)
        lean = std::max(want, std::min(st.tollLean, 0.));
    const double leanWas = st.tollLean;
    st.tollLean = steering() ? lean : 0.;
    std::string moved;
    for (const char* l : Levers)
    {
        const double now = std::string(l) == "tollLean" ? st.tollLean : steerLever(l);
        const double then = std::string(l) == "tollLean" ? leanWas : before[l];
        if (std::abs(now - then) >= .05)
            moved += std::string(moved.empty() ? "" : "; ") + leverName(l) + " " + fixed(then, 1) + " -> " + fixed(now, 1);
    }
    LoopState::Steer::Judging j;
    j.day = day;
    j.before = was;
    j.after = st.push;
    j.why = why + (moved.empty() ? "" : " (" + moved + ")");
    if (!judging)
    {
        st.history.push_back(j);
        if (st.history.size() > 24)
            st.history.erase(st.history.begin());
    }
    loopToday_.steerWhy = j.why;
}

void Society::steerFollow(std::int64_t day)
{
    // The DM's settings followed at once (doc 74, 8.2): turned on, the push at the slider; the slider moved, the push to
    // it and the judging's days started again; turned off, the push 0 and every rule the world's own (the means kept).
    auto& st = state_.loop.steer;
    const int lean = ownRule("steerLean");
    const double x = steerSlider();
    const auto said = [](int l) { return std::to_string(l - 100); };
    day = std::max<std::int64_t>(0, day);
    if (ownRule("steer") <= 0)
    {
        if (st.on)
        {
            steerSet(0, day, "Day " + std::to_string(day) + ": the steering turned off; every rule is the world's own again", false);
            st.on = false;
            st.push = 0;
            st.tollLean = 0;
        }
        return;
    }
    if (!st.on)
    {
        st.on = true;
        st.lean = lean;
        st.judged = day;
        st.push = 0;
        steerSet(x, day, "Day " + std::to_string(day) + ": the steering turned on, where the money rests at " + said(lean), false);
    }
    else if (st.lean != lean)
    {
        const int was = st.lean;
        st.lean = lean;
        st.judged = day;                            // (The move tells before it is trimmed.)
        steerSet(x, day, "Day " + std::to_string(day) + ": where the money rests moved from " + said(was) + " to " + said(lean), false);
    }
}

void Society::steerEconomy(std::int64_t day)
{
    // The controller (doc 74, 8): last of 18:00's money steps. The slider sets the push at once (feed-forward); then,
    // once its means have had steerMeanDays nights, every steerJudgeDays nights it trims the push a step toward the
    // slider's target (the earners' Gini left of the centre, the upper share right of it, both at it), within its band,
    // waiting while a measure is already coming back; the hunger guard (with Never Hungry off) stops it leaning right
    // while hunger is over its ceiling and pulls it back toward the centre (never past it) at twice the ceiling.
    auto& st = state_.loop.steer;
    steerFollow(day);
    if (!st.on)
        return;
    const double x = steerSlider();
    // The measures tonight, and their means.
    const double g = giniEarners(), s = upperShare(), t10 = top10Share();
    const double h = loopToday_.hungryHours > 0 ? double(loopToday_.hungrySum) / loopToday_.hungryHours : 0;
    const double k = loopToday_.householdsShort, tr = double(cash("treasury"));
    loopToday_.mGini = g;
    loopToday_.mUpper = s;
    loopToday_.mTop10 = t10;
    loopToday_.mHungry = h;
    st.days = std::min(st.days + 1, 3650);
    const double slow = 1. / std::min(st.days, std::max(1, ownRule("steerMeanDays"))), fast = 1. / std::min(st.days, 28);
    const auto follow = [&](double& mean, double now, double rate) { mean = mean < 0 ? now : mean + (now - mean) * rate; };
    follow(st.gini, g, slow);
    follow(st.upper, s, slow);
    follow(st.top10, t10, slow);
    follow(st.hungry, h, fast);
    follow(st.shortHomes, k, fast);
    follow(st.treasury, tr, fast);
    if (st.days < ownRule("steerMeanDays") || day - st.judged < ownRule("steerJudgeDays"))
        return;
    // Judging.
    LoopState::Steer::Judging j;
    j.day = day;
    j.before = st.push;
    const double gT = steerTarget("gini"), gB = steerTarget("giniBand"), sT = steerTarget("upper"), sB = steerTarget("upperBand");
    const bool useG = x <= 0 && !std::isnan(gT) && gB > 0, useS = x >= 0 && !std::isnan(sT) && sB > 0;
    const double eG = useG ? (gT - st.gini) / gB : 0, eS = useS ? (sT - st.upper) / sB : 0;   // (+: too equal, push right.)
    const auto inWords = [&](bool gi) {
        return gi ? "the earners' Gini " + fixed(st.gini, 3) + " against its target " + fixed(gT, 3) + " (band " + fixed(gB, 3) + ")"
                  : "the upper share " + fixed(st.upper, 1) + "% against its target " + fixed(sT, 1) + "% (band " + fixed(sB, 1) + ")";
    };
    bool onG = useG && (!useS || std::abs(eG) > 1);
    double err = 0;
    std::string why;
    if (useG && useS)
    {
        const bool outG = std::abs(eG) > 1, outS = std::abs(eS) > 1;
        if (outG && outS && (eG > 0) != (eS > 0))
        {
            why = inWords(true) + " and " + inWords(false) + " disagree: no move";
            onG = true;
        }
        else if (outG || outS)
        {
            onG = outG && (!outS || std::abs(eG) >= std::abs(eS));
            err = onG ? eG : eS;
        }
        else
            why = inWords(true) + " and " + inWords(false) + ": both in their bands";
        j.measure = "both";
    }
    else if (useG || useS)
    {
        err = useG ? eG : eS;
        j.measure = useG ? "gini" : "upper";
        if (std::abs(err) <= 1)
            why = inWords(useG) + ": in its band";
    }
    if (!j.measure.empty() && j.measure == "both" && err != 0)
        j.measure = onG ? "gini" : "upper";
    j.value = onG ? st.gini : st.upper;
    j.target = onG ? gT : sT;
    j.band = onG ? gB : sB;
    j.error = err;
    const double unit = ownRule("steerStep") / 100., most = ownRule("steerStepMost") / 100.;
    double step = std::abs(err) > 1 ? (std::abs(err) > 2 ? most : unit) * (err > 0 ? 1 : -1) : 0;
    if (step != 0)
    {
        const double then = onG ? st.giniThen : st.upperThen;
        if (then >= 0 && std::abs(then - j.target) - std::abs(j.value - j.target) >= .25 * std::abs(j.value - j.target))
        {
            why = inWords(onG) + ", already coming back: waiting";
            step = 0;
        }
        else
            why = inWords(onG) + (err > 0 ? ": too equal, leaning right" : ": too unequal, leaning left");
    }
    // The hunger guard (with Never Hungry off: on, the floor is that toggle's).
    bool pulled = false;
    if (ownRule("neverHungry") <= 0)
    {
        const double hMost = steerTarget("hungerMost"), kMost = steerTarget("shortMost");
        const bool over = (hMost >= 0 && st.hungry > hMost) || (kMost >= 0 && st.shortHomes > kMost);
        const bool far = (hMost >= 0 && st.hungry > 2 * hMost) || (kMost >= 0 && st.shortHomes > 2 * kMost);
        const std::string hunger = "hungry " + fixed(st.hungry, 3) + " an hour (ceiling " + fixed(hMost, 2) + "), households short " +
                                   fixed(st.shortHomes, 2) + " (ceiling " + fixed(kMost, 1) + ")";
        if (far && st.push > 0)
        {
            step = -std::min(st.push, unit);
            pulled = true;
            why = hunger + ": the hunger guard pulls the push back toward the centre";
            j.measure = "hunger";
        }
        else if (over && step > 0)
        {
            step = 0;
            why = hunger + ": the hunger guard holds the push from leaning right";
            j.measure = "hunger";
        }
    }
    const auto [lo, hi] = steerBand();
    double next = st.push + std::clamp(step, -most, most);
    next = pulled ? std::max(0., next) : std::clamp(next, std::min(lo, st.push), std::max(hi, st.push));
    if (!pulled && step != 0 && (next == hi || next == lo) && std::abs(next - st.push) < 1e-9)
        why += ", but the push is at its band's edge";
    j.step = next - st.push;
    st.judged = day;
    st.giniThen = st.gini;
    st.upperThen = st.upper;
    const std::string line = "Day " + std::to_string(day) + ": " + (why.empty() ? std::string("no measure to steer on") : why) +
                             (j.step != 0 ? "; the push went " + std::string(j.step > 0 ? "+" : "") + fixed(j.step, 2) + ", to " + fixed(next, 2)
                                          : "; the push stays at " + fixed(st.push, 2));
    steerSet(next, day, line, true);
    j.after = st.push;
    j.why = loopToday_.steerWhy;
    st.history.push_back(j);
    if (st.history.size() > 24)
        st.history.erase(st.history.begin());
    loopToday_.steerJudged = true;
}
} // namespace ratw
