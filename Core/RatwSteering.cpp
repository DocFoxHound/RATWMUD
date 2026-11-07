// Steering the economy (Docs/Design/74-steering-the-economy.md; Docs/Design/75-the-economy-controller.md): the DM's
// slider, read by a table of levers at its place (the centre the world's own); the two actuators doc 75 gave the ends,
// the Levelling tithe (left) and the Crown's charters on middle keepers' tills (right); the skim, which takes idle money in
// a set order of payers; and the two floors it pays for, Never Hungry and Never Broke (credit toward necessities). Every
// coin moves by shift between accounts that exist (fund:relief only passes money through), so the money is conserved;
// nothing here is random.
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

// A Gini of values each with a weight (doc 75, 3.4: a household's cash a head, weighted by its heads): the mean absolute
// difference over pairs of heads, over twice the mean.
double giniWeighted(std::vector<std::pair<double, double>> v)
{
    std::sort(v.begin(), v.end());
    double w = 0, sum = 0;
    for (const auto& [x, n] : v)
    {
        w += n;
        sum += x * n;
    }
    if (w <= 0 || sum <= 0)
        return 0;
    // Sum over pairs |xi - xj| ni nj, in one pass over the sorted values: each xi less those below it.
    double below = 0, belowSum = 0, diff = 0;
    for (const auto& [x, n] : v)
    {
        diff += n * (x * below - belowSum);
        below += n;
        belowSum += x * n;
    }
    return diff / (w * sum);
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
        {"levelling", "the Levelling tithe"}, {"charters", "the Crown's charters"}, {"postsShare", "the hands' share"},
        {"surplusKeep", "the treasury's keep"}, {"treasuryCap", "the treasury's cap"}, {"treasuryTarget", "the treasury's target"},
        {"surplusAlmsTo", "the poorest raised to"}, {"groundRent", "the ground rent"}, {"dividend", "the Houses' dividend"},
        {"richAbove", "the rich hold"}, {"spendAbove", "the spending keep"}};
    const auto found = names.find(lever);
    return found == names.end() ? "a lever" : found->second;
}
// The levers a change of the slider names in its line (each lever of the table, in this order).
const char* const Levers[] = {"levelling", "charters", "postsShare", "surplusKeep", "treasuryCap", "treasuryTarget",
                              "surplusAlmsTo", "groundRent", "dividend", "richAbove", "spendAbove"};
} // namespace

bool Society::steering() const
{
    return state_.loop.steer.on && ownRule("steer") > 0;
}

double Society::steerSlider() const
{
    return std::clamp((ownRule("steerLean") - 100) / 100., -1., 1.);
}

double Society::steerLever(const std::string& lever, bool table) const
{
    double own = 0;
    if (lever == "surplusKeep" || lever == "treasuryCap" || lever == "treasuryTarget" || lever == "surplusAlmsTo")
        own = ownRule(lever);
    else if (lever == "richAbove" || lever == "spendAbove")
        own = comfortsAbove();
    const auto& t = items::cityLoop().steering;
    const auto found = t.levers.find(lever);
    if (!steering() || found == t.levers.end())
        return own;
    double v = items::steerAt(found->second, state_.loop.steer.push, own);
    if (std::isnan(v))
        v = own;
    // The dividend rises with the charters (doc 75, 4.2), so the Houses' new profit moves on to upper purses.
    if (lever == "dividend")
        v += t.dividendPerCharter * steerLever("charters", true);
    // The supervisor's back-off (doc 75, 3.6): toward the world's own value, never past it.
    if (!table)
    {
        const auto& sup = state_.loop.steer.sup;
        if (lever == "levelling" && sup.levelling > 0)
            v = std::max(std::min(v, own), v - sup.levelling);
        else if (lever == "richAbove" && sup.richAbove > 0 && v > own)
            v = std::max(own, v - sup.richAbove);
    }
    return v;
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
        if (!tillHouse(&p).empty())
            upper += cash("till:" + p.id);   // (A House's till, by the catalogue or by charter.)
    return 100. * double(upper) / double(all);
}

double Society::giniHeads() const
{
    std::vector<std::pair<double, double>> v;
    for (const auto& [home, members] : households())
    {
        if (members.empty())
            continue;
        double held = 0;
        for (const auto& m : members)
            held += double(cash(m));
        v.emplace_back(held / double(members.size()), double(members.size()));
    }
    return giniWeighted(std::move(v));
}

double Society::foodGini() const
{
    std::vector<std::pair<double, double>> v;
    for (const auto& [home, members] : households())
    {
        if (members.empty())
            continue;
        double food = 0;
        for (const auto& m : members)
            if (const auto* a = account(m))
                food += eatable(*a);
        v.emplace_back(food / (NourishADay * double(members.size())), double(members.size()));
    }
    return giniWeighted(std::move(v));
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

std::int64_t Society::comfortableAHead() const
{
    // A household's line for the skim (doc 74, 6.1, "The skim reworked"): what it holds under `comfortable` times its
    // necessities a head is its spending money, never taken; and never under the comforts' keep.
    const auto nec = double(necessitiesAHead());
    return std::max<std::int64_t>(comfortsAbove(), std::int64_t(std::ceil(items::cityLoop().steering.comfortable * nec - 1e-9)));
}

bool Society::skimGuardOn() const
{
    // The tills short's guard (doc 74, 6.2): with Never Broke on, once its mean has had 28 nights, while the tills short's
    // 28-night mean is over the centre's level by more than tillsShortOver percent.
    const auto& st = state_.loop.steer;
    const auto& t = items::cityLoop().steering;
    return ownRule("neverBroke") > 0 && st.tillsDays >= 28 && st.tillsShort > t.tillsShortNormal * (1 + t.tillsShortOver / 100.);
}

std::vector<Society::Spare> Society::skimStep(int step, bool treasury)
{
    // The skim's payers at one step (doc 74, 6.1), worked out fresh: never a till, a farm, a player's purse, the beyond's
    // accounts, the Wharf's credit or fund:relief itself.
    std::vector<Spare> out;
    if (step == 1)
    {
        // The estates: each its spare over its keep (its posts' nights, the Wharf's reserve, the Crown's grants), and all
        // of them together never under the one purse's keep. (Idle by its nature: what the day's basket left.)
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
                        out.push_back({f, "estates: " + e.house, spare, {}, spare});
                        pool -= spare;
                    }
        }
        else if (account(WharfFund))
            if (const auto spare = estateSpare(WharfFund, estateOwed); spare > 0)
                out.push_back({WharfFund, "estates", spare, {}, spare});
    }
    else if (step == 2)
    {
        // The treasury: its spare over its cap and tonight's wage support; only once that is known (18:00), so never in
        // Never Hungry's 17:00 skim, before the night's wage support has had what is over the cap.
        // (Doc 75, 3.6, rule 1: while the treasury is under its floor, the skim leaves it be.)
        if (treasury && account("treasury") && !state_.loop.steer.sup.on[0])
            if (const auto spare = cash("treasury") - std::max<std::int64_t>(rule("treasuryCap"), TreasuryFloat) - loopToday_.supportWanted;
                spare > 0)
                out.push_back({"treasury", "treasury", spare, {}, spare});
    }
    else if (step >= 3 && step <= SkimSteps)
    {
        // The households over the comfortable line: 3 upper-class, 4 a keeper's (its shop's profit in its purses), 5 the
        // other middle-class (and any other class but the lower, which never pays). Each by its idle spare: what it has held
        // over the line for weeks (the lower of its spare now and its idle spare, which rises a 28th of the way a night).
        std::set<std::string> keepers;
        for (const auto& p : positions_)
            if (const auto held = state_.careers.positions.find(p.id);
                held != state_.careers.positions.end() && !held->second.holder.empty() && !dead_.count(held->second.holder) &&
                payOf(p).kind == "keeper" && charterOf("till:" + p.id).empty())
                keepers.insert(held->second.holder);   // (A chartered shop's keeper is paid a wage: no profit.)
        const auto line = comfortableAHead();
        const auto& st = state_.loop.steer;
        const bool seeded = st.idleDay >= 0;
        for (const auto& [home, members] : households())
        {
            const auto& cls = homeClass(home, members);
            if (cls == "lower")
                continue;
            const bool keeper = std::any_of(members.begin(), members.end(), [&](const std::string& m) { return keepers.count(m) > 0; });
            const int at = cls == "upper" ? 3 : keeper ? 4 : 5;
            if (at != step)
                continue;
            std::int64_t held = 0;
            for (const auto& m : members)
                held += cash(m);
            const auto spare = held - line * std::int64_t(members.size());
            if (spare <= 0)
                continue;
            const auto id = "home " + home;
            const auto found = st.idle.find(id);
            const auto idle = !seeded ? spare : found == st.idle.end() ? 0 : std::min(spare, found->second);
            out.push_back({id, at == 4 ? std::string("keepers") : "households: " + cls, spare, members, idle});
        }
    }
    std::sort(out.begin(), out.end(), [](const Spare& a, const Spare& b) {
        return a.idle != b.idle ? a.idle > b.idle : a.spare != b.spare ? a.spare > b.spare : a.id < b.id;
    });
    return out;
}

void Society::skimIdleNight(std::int64_t day)
{
    // Each household's idle spare, followed at the night's end (doc 74, 6.2): down at once to its spare, up toward it by
    // 1/idleDays of the gap (rounded up); the first night, each at its spare. A household with no spare is dropped (0).
    auto& st = state_.loop.steer;
    if (st.idleDay == day)
        return;
    const auto days = std::int64_t(items::cityLoop().steering.idleDays);
    std::map<std::string, std::int64_t> next;
    for (int step = 3; step <= SkimSteps; ++step)
        for (const auto& h : skimStep(step, false))
        {
            const auto found = st.idle.find(h.id);
            const auto was = st.idleDay < 0 ? h.spare : found == st.idle.end() ? 0 : found->second;
            next[h.id] = h.spare <= was ? h.spare : was + (h.spare - was + days - 1) / days;
        }
    st.idle = std::move(next);
    st.idleDay = day;
}

std::int64_t Society::skim(std::int64_t sum, const std::string& purpose, bool treasury)
{
    if (sum <= 0)
        return 0;
    if (!account(ReliefFund) && !openAccount(ReliefFund))
        return 0;
    // Idle money first (doc 74, 6.2): the steps in order, the richest first within each. The first pass: at most skimMost
    // percent of each holder's idle spare, step by step; the second, short of it: up to the whole of each spare, step by
    // step again. With the tills short's guard on, the first pass takes the estates' and the treasury's whole spare.
    const int most = std::clamp(rule("skimMost"), 1, 100);
    const bool guard = skimGuardOn();
    loopToday_.skimGuard = loopToday_.skimGuard || guard;
    std::int64_t taken = 0;
    for (int pass = 0; pass < 2 && taken < sum; ++pass)
        for (const int step : items::cityLoop().steering.skimOrder)   // (Doc 75, D1: the order a table value.)
        {
            if (taken >= sum)
                break;
            auto holders = skimStep(step, treasury);
            if (holders.empty())
                continue;
            std::vector<std::int64_t> caps;
            for (auto& h : holders)
            {
                const bool whole = pass == 1 || (guard && step <= 2);
                if (!whole)
                    h.spare = h.idle;               // (Water-filled on the idle spare: the money that has sat.)
                caps.push_back(whole ? h.spare : h.spare * most / 100);
            }
            std::vector<std::size_t> order(holders.size());
            for (std::size_t i = 0; i < order.size(); ++i)
                order[i] = i;
            std::sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
                return holders[a].spare != holders[b].spare ? holders[a].spare > holders[b].spare : holders[a].id < holders[b].id;
            });
            std::vector<Spare> ranked;
            std::vector<std::int64_t> rankedCaps;
            for (const auto i : order)
            {
                ranked.push_back(holders[i]);
                rankedCaps.push_back(caps[i]);
            }
            const auto take = waterFill(ranked, rankedCaps, sum - taken);
            for (std::size_t i = 0; i < ranked.size(); ++i)
            {
                auto left = take[i];
                if (left <= 0)
                    continue;
                const auto& h = ranked[i];
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
                (pass == 0 ? loopToday_.skimIdle : loopToday_.skimBusy) += gave;
                loopToday_.skimBy[h.group] += gave;
                loopToday_.skimPayers[h.id] += gave;
            }
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
    // bill is worked out first, the skim raises it from the richest, and fund:relief pays the tills. (Doc 75, 3.7: the
    // farms' crews too, each a household of its heads in one account, as they shop.)
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
    std::map<std::string, int> wharfPlanned;                       // Food -> the Wharf's pieces already in the batch.
    std::int64_t bill = 0;
    auto homes = households();
    std::map<std::string, double> heads;            // (A crew's line is its heads' days; a resident's, one.)
    for (const auto& f : farms_)
        if (farmAreaOn(f.area) && account(f.crew) && f.heads() > 0)
        {
            homes[f.crew] = {f.crew};
            heads[f.crew] = double(f.heads());
        }
    const auto lineOf = [&](const std::string& home) {
        const auto found = heads.find(home);
        return found == heads.end() ? line : line * found->second;
    };
    for (const auto& [home, members] : homes)
    {
        const double line = lineOf(home);
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
                    // The cheapest of the Wharf's foods it can still land today, with this hour's lots counted (Tune round
                    // 1: steer-centre-floors' few households left short after it were lots of the cheapest food over the
                    // Wharf's day's room, refused at the order, the next food never tried).
                    const std::string* food = nullptr;
                    int n = 0;
                    for (const auto& f : wharfFoods)
                    {
                        const int want = std::max(1, int(std::ceil((line - have) / nourishment(f) - 1e-9)));
                        const int room = (farmAreaOf(f) < 0 ? supplyLeft(f) : wharfRoom(f)) - wharfPlanned[f];
                        if (want <= room && want <= StoreDrawMost)
                        {
                            food = &f;
                            n = want;
                            break;
                        }
                    }
                    if (!food)
                        break;
                    wharfPlanned[*food] += n;
                    wharfLots.push_back({m, "", *food, n, 0});
                    bill += stablePrice(*food, n) + carriageSplit(ReliefFund, *food, n, false).first;
                    have += double(n) * nourishment(*food);
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
        after += std::any_of(members.begin(), members.end(), [&, line = lineOf(home)](const std::string& m) {
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
    // Never Broke (doc 74, 7.2, "The skim reworked"): at 18:00, after the day's wages and the city's alms, a household whose
    // purses together are under its next day's necessities (necessitiesAHead a head) is given credit for exactly what the
    // alms left it short: not cash, but credit toward its necessities, drawn from fund:relief as it buys its food and
    // needs at the shops tomorrow. Last night's credit lapses; what of it was not spent is still in the fund, so tonight's
    // skim raises only the rest. The poorest a head first, as far as the fund then holds.
    auto& st = state_.loop.steer;
    st.tillsDays = std::min(st.tillsDays + 1, 3650);   // (The tills short's mean, for the skim's guard.)
    const double rate = 1. / std::min(st.tillsDays, 28), tills = loopToday_.tillsShort;
    st.tillsShort = st.tillsShort < 0 ? tills : st.tillsShort + (tills - st.tillsShort) * rate;
    st.credit.clear();
    loopToday_.skimGuard = loopToday_.skimGuard || skimGuardOn();
    const auto nec = necessitiesAHead();
    struct Short
    {
        std::string home;
        std::int64_t held, heads;
    };
    std::vector<Short> shorts;
    std::int64_t bill = 0;
    for (const auto& [home, members] : households())
    {
        std::int64_t held = 0;
        for (const auto& m : members)
            held += cash(m);
        const auto heads = std::int64_t(members.size());
        if (held < nec * heads)
        {
            shorts.push_back({home, held, heads});
            bill += nec * heads - held;
        }
    }
    if (bill <= 0)
        return;
    skim(bill - cash(ReliefFund), "never broke");
    std::sort(shorts.begin(), shorts.end(), [](const Short& a, const Short& b) {
        return a.held * b.heads != b.held * a.heads ? a.held * b.heads < b.held * a.heads : a.home < b.home;
    });
    auto left = cash(ReliefFund);
    for (const auto& s : shorts)
    {
        const auto credit = std::min(nec * s.heads - s.held, left);
        if (credit <= 0)
            break;
        st.credit[s.home] = credit;
        left -= credit;
        loopToday_.neverBrokeCredit += credit;
        ++loopToday_.neverBrokeHomes;
    }
}

std::int64_t Society::neverBrokeOwed() const
{
    std::int64_t owed = 0;
    for (const auto& [home, credit] : state_.loop.steer.credit)
        owed += credit;
    return owed;
}

std::vector<std::int64_t> Society::tithed(const std::vector<std::int64_t>& cash, const std::vector<std::int64_t>& heads, double r)
{
    // Exactly, in whole numbers: r in millionths (R), the heads N and the total T, each want is
    // (cash (10^6 - R) N + R T heads) / (10^6 N), its floor and its remainder exact, so ties are true ties.
    std::vector<std::int64_t> out = cash;
    constexpr std::int64_t Parts = 1000000;
    const auto R = std::int64_t(std::llround(std::clamp(r, 0., 1.) * double(Parts)));
    std::int64_t total = 0, n = 0;
    for (std::size_t i = 0; i < cash.size() && i < heads.size(); ++i)
    {
        total += cash[i];
        n += std::max<std::int64_t>(0, heads[i]);
    }
    if (R <= 0 || n <= 0 || cash.size() != heads.size())
        return out;
    const __int128 den = __int128(Parts) * n;
    std::vector<__int128> part(cash.size(), 0);
    std::int64_t placed = 0;
    for (std::size_t i = 0; i < cash.size(); ++i)
    {
        const __int128 num = __int128(cash[i]) * (Parts - R) * n + __int128(R) * total * std::max<std::int64_t>(0, heads[i]);
        out[i] = num > 0 ? std::int64_t(num / den) : 0;
        part[i] = num > 0 ? num % den : 0;
        placed += out[i];
    }
    // The pennies the rounding left over (fewer than the households), one each, the largest remainders first, ties in
    // the order given.
    std::vector<std::size_t> order(cash.size());
    for (std::size_t i = 0; i < order.size(); ++i)
        order[i] = i;
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) { return part[a] > part[b]; });
    for (std::size_t k = 0; placed < total && k < order.size(); ++k)
    {
        ++out[order[k]];
        ++placed;
    }
    return out;
}

double Society::giniOf(std::vector<std::pair<double, double>> values)
{
    return giniWeighted(std::move(values));
}

void Society::levellingTithe()
{
    // The Levelling tithe (doc 75, 4.1; left of the centre): each resident household's cash a head moves r of the way to
    // the head-weighted mean, through fund:relief (what the households over the mean pay, those under it are given, the
    // same pennies tonight). Tills, farms, the estates and the treasury are never touched. G is taken before it moves.
    const double r = std::clamp(steerLever("levelling"), 0., 100.) / 100.;
    if (r <= 0)
        return;
    if (!account(ReliefFund) && !openAccount(ReliefFund))
        return;
    const auto homes = households();
    std::vector<const std::vector<std::string>*> members;
    std::vector<std::string> classes;               // (Each household's class, for the weekly ledger: doc 75, Phase 7.)
    std::vector<std::int64_t> held, heads;
    for (const auto& [home, m] : homes)
    {
        if (m.empty())
            continue;
        classes.push_back(homeClass(home, m));
        std::int64_t c = 0;
        for (const auto& id : m)
            c += cash(id);
        members.push_back(&m);
        held.push_back(c);
        heads.push_back(std::int64_t(m.size()));
    }
    loopToday_.mGBefore = giniHeads();
    const auto next = tithed(held, heads, r);
    std::int64_t raised = 0;
    for (std::size_t i = 0; i < held.size(); ++i)
    {
        const auto give = held[i] - next[i];
        if (give <= 0)
            continue;
        // A household pays from the member with the most first.
        auto order = *members[i];
        std::sort(order.begin(), order.end(), [&](const std::string& a, const std::string& b) {
            return cash(a) != cash(b) ? cash(a) > cash(b) : a < b;
        });
        std::int64_t gave = 0;
        for (const auto& m : order)
            if (const auto c = std::min(give - gave, cash(m)); c > 0 && shift(m, ReliefFund, "", 0, c, "the Levelling tithe"))
                gave += c;
        raised += gave;
        loopToday_.titheGave += gave > 0;
        if (gave > 0)
            loopToday_.titheOut[classes[i]] += gave;
    }
    for (std::size_t i = 0; i < held.size() && raised > 0; ++i)
    {
        const auto get = std::min(next[i] - held[i], raised);
        if (get <= 0)
            continue;
        // To the member with the most (as the dividend's).
        const auto& m = *members[i];
        const auto richest = *std::max_element(m.begin(), m.end(), [&](const std::string& a, const std::string& b) {
            return cash(a) != cash(b) ? cash(a) < cash(b) : a > b;
        });
        if (shift(ReliefFund, richest, "", 0, get, "the Levelling tithe"))
        {
            raised -= get;
            loopToday_.tithe += get;
            ++loopToday_.titheGot;
            loopToday_.titheIn[classes[i]] += get;
        }
    }
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

void Society::steerSet(double x, std::int64_t day, const std::string& why)
{
    // The slider moved (or the steering turned on or off): the levers read at the new place, and a line in the history
    // naming what moved.
    auto& st = state_.loop.steer;
    std::map<std::string, double> before;
    for (const char* l : Levers)
        before[l] = steerLever(l);
    st.push = std::clamp(x, -1., 1.);
    std::string moved;
    for (const char* l : Levers)
        if (const double now = steerLever(l); std::abs(now - before[l]) >= .05)
            moved += std::string(moved.empty() ? "" : "; ") + leverName(l) + " " + fixed(before[l], 1) + " -> " + fixed(now, 1);
    LoopState::Steer::Judging j;
    j.day = day;
    j.after = st.push;
    j.why = why + (moved.empty() ? "" : " (" + moved + ")");
    loopToday_.steerWhy = j.why;
    steerLog(std::move(j));
}

void Society::steerLog(LoopState::Steer::Judging j)
{
    // Doc 75, Phase 7: the log keeps a year of lines (the DM's slider moves, the supervisor's actions), not the last 24, so
    // a busy year of the supervisor no longer pushes the DM's changes out; at most LogMost lines, the oldest first out.
    auto& h = state_.loop.steer.history;
    const auto day = j.day;
    h.push_back(std::move(j));
    h.erase(std::remove_if(h.begin(), h.end(), [&](const auto& x) { return x.day <= day - LoopState::Steer::LogDays; }), h.end());
    if (h.size() > LoopState::Steer::LogMost)
        h.erase(h.begin(), h.begin() + std::ptrdiff_t(h.size() - LoopState::Steer::LogMost));
}

void Society::steerLedgerNight(std::int64_t day)
{
    // The weekly ledger (doc 75, 3.8, Phase 7): tonight's money by lever, who paid it and who was given it, added to this
    // week's (day / 7); the last LedgerWeeks weeks kept. It only counts what the night's steps booked in loopToday_.
    const auto& d = loopToday_;
    auto& weeks = state_.loop.steer.ledger;
    const auto week = day / 7;
    if (weeks.empty() || weeks.back().week != week)
    {
        weeks.push_back({});
        weeks.back().week = week;
    }
    auto& w = weeks.back();
    ++w.nights;
    const auto flow = [&](const char* lever, const std::string& from, const std::string& to, std::int64_t coins) {
        if (coins <= 0)
            return;
        w.paid[lever][from] += coins;
        w.got[lever][to] += coins;
    };
    for (const auto& [cls, coins] : d.titheOut)
        if (coins > 0)
            w.paid["levelling"][cls] += coins;
    for (const auto& [cls, coins] : d.titheIn)
        if (coins > 0)
            w.got["levelling"][cls] += coins;
    flow("charters", "the chartered shops", "the Houses' estates", d.charterProfit);
    flow("groundRent", "the keepers' tills", "the Crown's estate", d.groundRent - d.groundRentTreasury);
    flow("groundRent", "the keepers' tills", "the treasury", d.groundRentTreasury);
    flow("postsShare", "the keepers' tills", "the shops' hands", d.postsShare);
    flow("dividend", "the Houses' estates", "upper", d.dividend);
    flow("wageSupport", "the treasury", "the short tills' posts", d.wageSupport);
    flow("surplusAlms", "the treasury", "the poorest households", d.surplusAlms);
    flow("toll", "the shops' store draws", "the treasury", d.toll);
    for (const auto& [group, coins] : d.skimBy)
        if (coins > 0)
            w.paid["skim"][group] += coins;
    for (const auto& [purpose, coins] : d.skimFor)
        if (coins > 0)
            w.got["skim"][purpose] += coins;
    while (!weeks.empty() && weeks.front().week <= week - LoopState::Steer::LedgerWeeks)
        weeks.erase(weeks.begin());
}

void Society::steerFollow(std::int64_t day)
{
    // The DM's settings followed at once (doc 74, 8.2; doc 75, 3.3): turned on, the levers at the slider; the slider
    // moved, the levers to its new place the same night, the charters' ramp begun toward its new number and the slow means
    // re-seated tonight; turned off, every rule the world's own and every charter handed back (the means kept).
    auto& st = state_.loop.steer;
    const int lean = ownRule("steerLean");
    const double x = steerSlider();
    const auto said = [](int l) { return std::to_string(l - 100); };
    day = std::max<std::int64_t>(0, day);
    const auto ramp = [&] {
        // The number of charters at the new place: its share of the middle keepers' tills (those that may be chartered
        // and those that are), reached over chartersRampDays nights from tonight's.
        const double share = std::clamp(steerLever("charters"), 0., 100.) / 100.;
        st.chartersFrom = int(st.charters.size()) + st.sup.chartersHeld;   // (Without the supervisor's held back.)
        st.chartersTo = int(std::lround(share * double(charterable().size() + st.charters.size())));
        st.chartersNights = st.chartersTo == st.chartersFrom ? -1 : 0;
    };
    if (ownRule("steer") <= 0)
    {
        if (st.on)
        {
            steerSet(0, day, "Day " + std::to_string(day) + ": the steering turned off; every rule is the world's own again" +
                                 (st.charters.empty() ? "" : ", and the " + std::to_string(st.charters.size()) + " charters handed back"));
            loopToday_.chartersBack += int(st.charters.size());
            st.on = false;
            st.push = 0;
            st.charters.clear();
            st.chartersFrom = st.chartersTo = 0;
            st.chartersNights = -1;
            st.sup = {};                            // (The supervisor starts afresh when it is next turned on.)
        }
        return;
    }
    if (!st.on)
    {
        st.on = true;
        st.sup = {};
        st.lean = lean;
        st.judged = day;
        st.push = 0;
        steerSet(x, day, "Day " + std::to_string(day) + ": the steering turned on, where the money rests at " + said(lean));
        ramp();
    }
    else if (st.lean != lean)
    {
        const int was = st.lean;
        st.lean = lean;
        st.judged = day;
        st.gini = st.upper = st.top10 = -1;         // (Re-seated tonight: the first look after a move is at the new place.)
        // (Doc 75, Phase 5: the supervisor's 7-night means too, so no rule judges the new place on the old one's nights for
        // meanNights nights. In the sweep, A9, the centre's 0.9 households short tripped the far left's 0.1 the night of the
        // move.)
        st.sup.nights = 0;
        st.sup.treasury = st.sup.tills = st.sup.hungry = st.sup.shortHomes = st.sup.unpaid = -1;
        steerSet(x, day, "Day " + std::to_string(day) + ": where the money rests moved from " + said(was) + " to " + said(lean));
        ramp();
    }
}

std::string Society::charterOf(const std::string& till) const
{
    for (const auto& [t, house] : state_.loop.steer.charters)
        if (t == till)
            return house;
    return {};
}

std::string Society::tillHouse(const Position* post) const
{
    if (!post)
        return {};
    if (const auto* w = items::farmPeople().workshop(post->work.cell))
        return w->house;
    const auto& owned = items::cityLoop().houseTills;
    if (const auto found = owned.find(post->work.cell); found != owned.end())
        return found->second;
    return charterOf("till:" + post->id);           // (The overlay: a middle keeper's till chartered to a House.)
}

std::vector<std::string> Society::charterable()
{
    // A city keeper's till (a merchant's post, held by the living; not a farm's, not a House's by the catalogue, not
    // chartered), whose keeper's household is not upper class: the six large keepers are upper class already (doc 75,
    // 4.2), so chartering them would only move their profit within S.
    std::map<std::string, std::pair<std::string, const std::vector<std::string>*>> homeOf;   // Resident -> home, members.
    const auto homes = households();
    for (const auto& [home, members] : homes)
        for (const auto& m : members)
            homeOf[m] = {home, &members};
    const auto& st = state_.loop.steer;
    std::vector<std::pair<double, std::string>> ranked;
    for (const auto& p : positions_)
    {
        if (p.role != "merchant")
            continue;
        const auto till = "till:" + p.id;
        if (farmOf_.count(till) || !account(till) || items::farmPeople().workshop(p.work.cell) ||
            items::cityLoop().houseTills.count(p.work.cell) || !charterOf(till).empty())
            continue;
        const auto held = state_.careers.positions.find(p.id);
        if (held == state_.careers.positions.end() || held->second.holder.empty() || dead_.count(held->second.holder))
            continue;
        const auto home = homeOf.find(held->second.holder);
        if (home != homeOf.end() && homeClass(home->second.first, *home->second.second) == "upper")
            continue;
        const auto mean = st.profit.find(till);
        ranked.push_back({mean == st.profit.end() ? 0. : -mean->second, till});
    }
    std::sort(ranked.begin(), ranked.end());
    std::vector<std::string> out;
    for (const auto& [unused, till] : ranked)
        out.push_back(till);
    return out;
}

void Society::chartersNight(std::int64_t day)
{
    // The Crown's charters (doc 75, 4.2), at 18:00 before the wages: tonight's number on the ramp (equal steps from its
    // number at the move to the new one over chartersRampDays of these nights, once the keepers' profit has had its 28
    // nights: until then the ramp waits), the best middle keepers' tills granted, the newest handed back first. A till is
    // chartered to the House whose district it is in (its cell's House), else the Crown's.
    (void)day;
    auto& st = state_.loop.steer;
    if (!steering())
        return;
    // (A chartered till gone, or its post now a House's by the catalogue: its charter lapses.)
    st.charters.erase(std::remove_if(st.charters.begin(), st.charters.end(),
                                     [&](const auto& c) {
                                         const auto* p = account(c.first) ? position(c.first.substr(5)) : nullptr;
                                         return !p || items::farmPeople().workshop(p->work.cell) || items::cityLoop().houseTills.count(p->work.cell);
                                     }),
                      st.charters.end());
    // (The ramp waits for a month of the keepers' profit, so the best tills are known.)
    const bool waiting = st.chartersNights >= 0 && st.profitNights < 28;
    // (The number the ramp or the table wants, before the supervisor's: those chartered and those it holds back.)
    int want = int(st.charters.size()) + st.sup.chartersHeld;
    if (st.chartersNights >= 0 && !waiting)
    {
        const int days = std::max(1, items::cityLoop().steering.chartersRampDays);
        const double t = std::min(1., double(++st.chartersNights) / days);
        want = st.chartersFrom + int(std::lround((st.chartersTo - st.chartersFrom) * t));
        if (t >= 1)
            st.chartersNights = -1;
    }
    // The supervisor's rule 3 (doc 75, 3.6): the charters it holds back, newest first (a whole one at a time).
    st.sup.chartersHeld = std::clamp(int(std::floor(st.sup.charters + 1e-9)), 0, std::max(0, want));
    want -= st.sup.chartersHeld;
    if (int(st.charters.size()) < want)
    {
        const auto& estates = items::cityLoop().estates;
        std::string crown;
        for (const auto& e : estates)
            if (e.grants)
                crown = e.house;
        for (const auto& till : charterable())
        {
            if (int(st.charters.size()) >= want)
                break;
            const auto* p = position(till.substr(5));
            auto house = p && day_.houseOf ? day_.houseOf(p->work.cell) : std::string();
            if (std::none_of(estates.begin(), estates.end(), [&](const items::Estate& e) { return e.house == house; }))
                house = crown.empty() ? std::string("the Crown") : crown;
            st.charters.push_back({till, house});
            ++loopToday_.chartersGranted;
        }
    }
    while (int(st.charters.size()) > std::max(0, want))
    {
        st.charters.pop_back();                     // (Newest first.)
        ++loopToday_.chartersBack;
    }
    loopToday_.charters = int(st.charters.size());
    loopToday_.chartersWant = st.chartersNights >= 0 ? st.chartersTo : want;
}

void Society::steerEconomy(std::int64_t day)
{
    // The steering's night (doc 75, 3.4): last of 18:00's money steps. The slider is followed; the measures tonight and
    // their means (slow, over about steerMeanDays nights; fast, over 28); the middle keepers' profit means for the
    // charters. Nothing is judged here: the table set the levers (doc 75 took the push's trim and the hunger pulls away;
    // the supervisor of Phase 4 backs off a lever that is paying).
    auto& st = state_.loop.steer;
    steerFollow(day);
    if (!st.on)
        return;
    const double g = giniEarners(), s = upperShare(), t10 = top10Share();
    const double h = loopToday_.hungryHours > 0 ? double(loopToday_.hungrySum) / loopToday_.hungryHours : 0;
    const double k = loopToday_.householdsShort, tr = double(cash("treasury"));
    loopToday_.mGini = g;
    loopToday_.mGAll = giniHeads();
    loopToday_.mFood = foodGini();
    loopToday_.mUpper = s;
    if (const auto all = moneySupply(); all > 0)
    {
        std::int64_t chartered = 0;
        for (const auto& [till, house] : st.charters)
            chartered += cash(till);
        loopToday_.mUpperNet = s - 100. * double(chartered) / double(all);
    }
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
    // The middle keepers' profit tonight (none: 0), each till's 28-night mean; the chartered tills' too.
    st.profitNights = std::min(st.profitNights + 1, 3650);
    const double rate = 1. / std::min(st.profitNights, 28);
    std::map<std::string, double> next;
    auto tills = charterable();
    for (const auto& [till, house] : st.charters)
        tills.push_back(till);
    for (const auto& till : tills)
    {
        const auto tonight = loopToday_.keeperProfit.find(till);
        const double now = tonight == loopToday_.keeperProfit.end() ? 0. : double(tonight->second);
        const auto was = st.profit.find(till);
        next[till] = was == st.profit.end() ? now : was->second + (now - was->second) * rate;
    }
    st.profit = std::move(next);
    steerSupervise(day);
}

double Society::treasuryFloorAt(double x, std::int64_t day) const
{
    // The mark's seasonal treasury profile less 2 sigma (the table's months), blended between the marks as a lever is,
    // never under floorLeast; with no floors in the table, floorLeast.
    const auto& t = items::cityLoop().steering;
    if (!t.floorsGiven)
        return t.floorLeast;
    const auto yd = ((day % 365) + 365) % 365;
    const auto month = std::min<std::int64_t>(items::CityLoop::Steering::FloorMonths - 1, yd / 28);
    std::array<double, 5> column{};
    for (std::size_t m = 0; m < 5; ++m)
        column[m] = t.treasuryFloors[m][std::size_t(month)];
    const double v = items::steerAt(column, x, t.floorLeast);
    return std::max(t.floorLeast, std::isnan(v) ? t.floorLeast : v);
}

void Society::steerSupervise(std::int64_t day)
{
    SupNight tonight;
    tonight.treasury = double(cash("treasury"));
    tonight.tills = double(loopToday_.tillsShort);
    tonight.hungry = loopToday_.hungryHours > 0 ? double(loopToday_.hungrySum) / loopToday_.hungryHours : 0;
    tonight.shortHomes = double(loopToday_.householdsShort);
    tonight.unpaid = double(loopToday_.supportUnpaidSwitch + loopToday_.supportUnpaidOrdinary);
    steerSupervise(day, tonight);
}

void Society::steerSupervise(std::int64_t day, const SupNight& tonight)
{
    // The safety supervisor (doc 75, 3.6), at 18:00, last: it judges tonight's 7-night means and moves the levers read from
    // tomorrow. In priority order:
    //   1 the treasury's 7-night mean under its floor: the ground rent to the treasury in proportion to the gap (all of it
    //     at routeAt of the floor under it), the skim leaves the treasury be, and the rich hold eases toward the world's
    //     (at the right the rich holding over the world's keep is the money out of the shops' and the toll's circulation:
    //     doc 74's "200p cost the treasury");
    //   2 the tills short over tillsOver x this mark's, or the wage support unpaid over this mark's most: the tithe eases
    //     (left), the rich hold eases (right);
    //   3 hungry residents or households short over their ceilings (each with its floor off): the tithe eases faster
    //     (left), charters are handed back, newest first (right);
    //   4 a lever backed off edgeNights nights running: "at the envelope's edge", logged, nothing done.
    // A rule fires only where it has a lever to move, and clears when its measure has been back under clearShare of its
    // trigger (for the treasury, over the floor by as much) for clearNights nights running. Each lever moves once a night,
    // at the fastest rate of the rules on it whose trigger is true tonight; with rules on but none over its trigger, it holds;
    // with none on, back toward its table value at returnShare of the rate it left. A back-off never takes a lever past the world's
    // own value.
    auto& st = state_.loop.steer;
    auto& sup = st.sup;
    const auto& t = items::cityLoop().steering;
    const double x = st.push;
    loopToday_.supOn.clear();
    loopToday_.supEdge.clear();
    loopToday_.supWhy.clear();
    // The 7-night means.
    sup.nights = std::min(sup.nights + 1, 3650);
    const double rate = 1. / std::min(double(sup.nights), t.meanNights);
    const auto follow = [&](double& mean, double now) { mean = mean < 0 ? now : mean + (now - mean) * rate; };
    follow(sup.treasury, tonight.treasury);
    follow(sup.tills, tonight.tills);
    follow(sup.hungry, tonight.hungry);
    follow(sup.shortHomes, tonight.shortHomes);
    follow(sup.unpaid, tonight.unpaid);
    // Each lever's room: how far its table value is from the world's here.
    const double roomLevel = std::max(0., steerLever("levelling", true));
    const double roomRich = std::max(0., steerLever("richAbove", true) - double(comfortsAbove()));
    const int chartersBase = int(st.charters.size()) + sup.chartersHeld;
    const double roomCharters = double(std::max(0, std::max(chartersBase, st.chartersNights >= 0 ? st.chartersTo : 0)));
    const bool floors = ownRule("neverHungry") > 0 || ownRule("neverBroke") > 0;
    const double floor = treasuryFloorAt(x, day);
    loopToday_.supFloor = floor;
    const double tillsTarget = steerTarget("tillsShort"), unpaidMost = steerTarget("unpaidMost");
    const double hungerMost = steerTarget("hungerMost"), shortMost = steerTarget("shortMost");
    const bool hungerJudged = ownRule("neverHungry") <= 0 && !std::isnan(hungerMost);
    const bool shortJudged = ownRule("neverBroke") <= 0 && !std::isnan(shortMost);
    // Each rule: its trigger, its clearing, whether it has a lever here, and its name and words.
    const double cs = t.clearShare;
    const bool tillsJudged = !std::isnan(tillsTarget) && tillsTarget > 0, unpaidJudged = !std::isnan(unpaidMost) && unpaidMost > 0;
    struct Rule
    {
        bool trigger, cleared, work;
        const char* name;
        std::string why;
    };
    const std::array<Rule, 3> rules{{
        {sup.treasury < floor, sup.treasury >= floor / cs,
         steerLever("groundRent", true) > 0 || floors || roomRich > 0, "treasury",
         "the treasury under its floor (" + fixed(sup.treasury, 0) + "p a night over 7 nights, its floor " + fixed(floor, 0) + "p)"},
        {(tillsJudged && sup.tills > t.tillsOver * tillsTarget) || (unpaidJudged && sup.unpaid > unpaidMost),
         (!tillsJudged || sup.tills < cs * t.tillsOver * tillsTarget) && (!unpaidJudged || sup.unpaid < cs * unpaidMost),
         roomLevel > 0 || roomRich > 0, "shops",
         "the shops ran short (" + fixed(sup.tills, 1) + " tills short" + (tillsJudged ? " against " + fixed(t.tillsOver * tillsTarget, 1) : "") +
             (unpaidJudged ? ", the wage support " + fixed(sup.unpaid, 0) + "p a night unpaid against " + fixed(unpaidMost, 0) + "p" : "") + ")"},
        {(hungerJudged && sup.hungry > hungerMost) || (shortJudged && sup.shortHomes > shortMost),
         (!hungerJudged || sup.hungry < cs * hungerMost) && (!shortJudged || sup.shortHomes < cs * shortMost),
         roomLevel > 0 || roomCharters > 0, "hunger",
         "hunger (" + fixed(sup.hungry, 3) + " hungry an hour, " + fixed(sup.shortHomes, 2) + " households short)"},
    }};
    std::string said;
    const auto say = [&](const std::string& words) { said += (said.empty() ? "" : "; ") + words; };
    // (No rule fires until its means have had meanNights nights: one night's measure, the steering's first, is not a
    // week's. A first night after a world starts is often its hungriest.)
    const bool judged = sup.nights >= t.meanNights;
    for (std::size_t i = 0; i < rules.size(); ++i)
    {
        const auto& r = rules[i];
        if (!sup.on[i])
        {
            if (judged && r.trigger && r.work)
            {
                sup.on[i] = true;
                sup.clear[i] = 0;
                ++sup.fires[i];
                say("the supervisor acts: " + r.why);
            }
            continue;
        }
        sup.clear[i] = r.cleared ? sup.clear[i] + 1 : 0;
        if (!r.work || sup.clear[i] >= t.clearNights)
        {
            sup.on[i] = false;
            sup.clear[i] = 0;
            say(std::string("the supervisor's ") + r.name + " rule clears" + (r.work ? "" : " (no lever of its here)") +
                ": the levers return toward the table" + (t.returnShare >= 1 ? std::string(" at the rate they left") :
                                                          " at " + fixed(100 * t.returnShare, 0) + "% of the rate they left"));
        }
    }
    for (std::size_t i = 0; i < rules.size(); ++i)
        if (sup.on[i])
            loopToday_.supOn.push_back(rules[i].name);
    // The levers, once each a night.
    // (Phase 4 review: a rule backs its lever off only while its trigger is true, "until the cause clears" (3.6); on but
    // back under its trigger, waiting out its clearing nights, the lever holds where it is, and returns only once every
    // rule on it has cleared. Deepening through the clearing nights made each back-off a standing table change: in the
    // Phase 4 smokes 48-60% of the nights a rule was on its trigger was already false.)
    const auto move = [&](double& off, double& left, double room, double week, bool held, const char* lever, const std::string& unit) {
        const double before = off;
        if (week > 0 && room > 0)
        {
            left = week / 7;
            off = std::min(room, off + left);
        }
        else if (off > 0 && !held)
            off = std::max(0., off - left * t.returnShare);
        off = std::min(off, room);                  // (Never past the world's own: the slider may have moved.)
        if (off != before && (before == 0 || off == 0 || off == room))
            say(std::string(leverName(lever)) + (off > before ? " eased by " : " back to within ") + fixed(off, 1) + unit +
                (off > before ? "" : " of the table"));
    };
    std::array<bool, 3> pull{};                     // (On and its trigger true tonight: it pulls its lever further.)
    for (std::size_t i = 0; i < rules.size(); ++i)
        pull[i] = sup.on[i] && rules[i].trigger;
    const double levelWeek = std::max(pull[1] ? t.titheShopsWeek : 0., pull[2] ? t.titheHungerWeek : 0.);
    const double richWeek = (pull[0] || pull[1]) ? t.richShopsWeek : 0.;
    const double chartersWeek = pull[2] ? t.chartersHungerWeek : 0.;
    move(sup.levelling, sup.levellingRate, roomLevel, roomLevel > 0 ? levelWeek : 0., roomLevel > 0 && (sup.on[1] || sup.on[2]),
         "levelling", " points");
    move(sup.richAbove, sup.richRate, roomRich, roomRich > 0 ? richWeek : 0., roomRich > 0 && (sup.on[0] || sup.on[1]), "richAbove", "p");
    move(sup.charters, sup.chartersRate, roomCharters, roomLevel > 0 ? 0. : chartersWeek, roomLevel <= 0 && sup.on[2], "charters",
         " charters");
    // Rule 1's route: in proportion to the gap while it is on (and held there until it clears), easing back over 14 nights.
    if (sup.on[0])
        sup.route = std::max(sup.route, std::clamp((floor - sup.treasury) / (t.routeAt * floor), 0., 1.));
    else
        sup.route = std::max(0., sup.route - 1. / 14);
    if (steerLever("groundRent", true) <= 0)
        sup.route = 0;
    // Rule 4: the envelope's edge.
    const std::array<std::pair<double, const char*>, 3> backs{{{sup.levelling, "levelling"}, {sup.richAbove, "richAbove"}, {sup.charters, "charters"}}};
    for (std::size_t i = 0; i < backs.size(); ++i)
    {
        sup.edge[i] = backs[i].first > 0 ? std::min(sup.edge[i] + 1, 3650) : 0;
        if (sup.edge[i] >= t.edgeNights)
        {
            loopToday_.supEdge.push_back(backs[i].second);
            if (sup.edge[i] == t.edgeNights)
                say(std::string(leverName(backs[i].second)) + " backed off " + std::to_string(t.edgeNights) +
                    " nights running: at the envelope's edge");
        }
    }
    if (!said.empty())
    {
        loopToday_.supWhy = said;
        LoopState::Steer::Judging j;
        j.day = day;
        j.measure = "supervisor";
        j.after = st.push;
        j.why = "Day " + std::to_string(day) + ": " + said;
        steerLog(std::move(j));
    }
}
} // namespace ratw
