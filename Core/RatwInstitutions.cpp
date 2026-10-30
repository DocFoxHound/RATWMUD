// Households and institutions (Docs/Design/64-economy-migration.md, Phase 7; doc 63's model): income, tax and tithe on
// it, the household rule, institutions' weekly budgets, daily relief and bakers' leftovers. See RatwSociety.h.
#include "RatwItems.h"
#include "RatwSociety.h"

#include <algorithm>
#include <cmath>
#include <functional>

namespace ratw
{
namespace
{
double lot(const std::string& words)
{
    return double(std::hash<std::string>{}(words) % 100000) / 100000;
}
bool townAccount(const std::string& id)
{
    return id == "treasury" || id.rfind("stores:", 0) == 0;
}
bool building(const std::string& item)
{
    return item == "bricks" || item == "planks" || item == "nails" || item == "stone" || item == "roof_tiles" || item == "timber" ||
           item == "lime" || item == "fittings" || item == "plaster";
}
bool finery(const std::string& item)
{
    const auto* g = items::good(item);
    const auto c = g ? g->category : std::string();
    return c == "jewelry" || c == "luxury" || c == "hat" || c == "scarf" || c == "shawl";
}
}

bool Society::unearnedKind(const std::string& kind)
{
    return kind == "inheritance" || kind == "operator transfer" || kind == "the shop's till" || kind == "sale of a business" ||
           kind == "starting money" || kind == "a child's first pennies" || kind == "a child's stipend" || kind == "the household purse" ||
           kind == "the church's dole" || kind == "the church's share" || kind == "a gift" || kind == "a letter's enclosure" ||
           kind == "an owner's investment" || kind == "relief" || kind == "a relief grant" || kind == "a town's rebate" || kind.rfind("test:", 0) == 0;   // (A test's money, put there.)
}

double Society::monthIncomeOf(const std::string& account) const
{
    const auto found = state_.memory.monthIncome.find(account);
    return found == state_.memory.monthIncome.end() ? 0 : found->second;
}

void Society::tendIncome()
{
    auto& memory = state_.memory;
    // Each town's stores' mean holding, a week's, day by day (round 10, TaxEase): its caravans' road money out and their
    // takings home again swing it by a week's trade.
    if (workByHours() && wageRule(TaxEase))
        for (auto it = state_.accounts.lower_bound("stores:"); it != state_.accounts.end() && it->first.rfind("stores:", 0) == 0; ++it)
        {
            // (Its holdings tax not yet spent back left out: round 12, lane B, HoldingsBite.)
            const double cash = std::max(0., double(it->second.cash) - biteHeld(it->first));
            const auto [held, fresh] = memory.heldMean.try_emplace(it->first, cash);
            if (!fresh)
                held->second += (cash - held->second) / HeldMeanDays;
        }
    for (auto it = memory.monthIncome.begin(); it != memory.monthIncome.end();)
    {
        it->second *= double(MonthDays - 1) / MonthDays;
        // (A town's month can fall below nothing, its caravans' road money out before their takings come home: round 8.)
        it = std::abs(it->second) < .5 && !earnedToday_.count(it->first) ? memory.monthIncome.erase(it) : std::next(it);
    }
    flushEarnings();
}

void Society::easeTownTax(const std::map<std::string, double>& fullTax)
{
    // A town's stores over their target lower its tax (round 10, the user, 2026-10-27: over grants, sending it to the
    // capital, or capping its caravans' profit). The target is the model's: TaxEaseMonths of its income, what its tax
    // brings in a month at the full rate (a running month, a quarter of the way to four of this week's a week), not at
    // the eased one, or easing it would lower the target and ease it further. The rate wanted is the full one at the
    // target and below it, falling as the mean holding rises over it, to nothing at twice the target; the rate moves a
    // quarter of the way to it a week. Never above the full rate: a town short of money keeps its posts to its tax
    // (TownPosts) instead.
    auto& memory = state_.memory;
    for (auto it = state_.accounts.lower_bound("stores:"); it != state_.accounts.end() && it->first.rfind("stores:", 0) == 0; ++it)
    {
        const auto& id = it->first;
        const auto found = fullTax.find(id);
        const double month = found == fullTax.end() ? 0. : 4 * found->second;
        auto base = memory.taxBase.find(id);
        if (base == memory.taxBase.end())
        {
            if (month <= 0)
                continue;
            base = memory.taxBase.emplace(id, month).first;
        }
        else
            base->second += (month - base->second) / TaxEaseWeeks;
        const double target = TaxEaseMonths * base->second;
        if (target < 1)
            continue;
        const auto held = memory.heldMean.find(id);
        const double want = std::clamp(1 - ((held == memory.heldMean.end() ? double(it->second.cash) : held->second) - target) / target, 0., 1.);
        const double share = townTaxShare(id) + (want - townTaxShare(id)) / TaxEaseWeeks;
        if (share > .999)
            memory.townTax.erase(id);
        else
            memory.townTax[id] = share;
    }
    ++memory.revision;
}

void Society::rebateTownTax(const std::map<std::string, std::vector<std::pair<std::string, double>>>& taxpayers)
{
    // Round 10 (the user, 2026-10-27: on, by income; TaxRebate): a town whose stores hold over twice their target
    // (where the rate it wants is nothing) pays its taxpayers back half of what its stores hold over that a week (the
    // lesser of what they hold and their week's mean), each by its share of the week's income: "a town's rebate".
    // Oakhaven's caravans bring in several times its tax, so easing its tax alone can't bring it down.
    for (const auto& [id, payers] : taxpayers)
    {
        const auto* stores = account(id);
        // (Its holdings tax not yet spent back isn't its to rebate: round 12, HoldingsBite.)
        const double target = townTaxTarget(id), held = std::min(double(stores ? stores->cash : 0) - biteHeld(id), townHeldMean(id));
        double incomes = 0;
        for (const auto& [payer, income] : payers)
            incomes += income;
        if (target < 1 || held <= 2 * target || incomes < 1)
            continue;
        // (Round 13, lane 2, SurplusByNeed: less its pull as its surplus was sent on this week. While the city's homes are
        // poorer than its own, what it holds over two targets goes to the city's poor before it goes back to its own
        // taxpayers; as the need falls, or the capital's pot fills, the rebate comes back. In round 12's run Larkfield's
        // stores took 1,257,623p of holdings tax and paid 1,297,271p of rebate, mostly to the same payers.)
        double pool = (held - 2 * target) / 2;
        if (surplusByNeed())
            if (const auto pull = pullSent_.find(id); pull != pullSent_.end())
                pool *= 1 - std::clamp(pull->second, 0., 1.);
        for (const auto& [payer, income] : payers)
            if (const auto back = std::min<std::int64_t>(account(id)->cash, std::llround(pool * income / incomes));
                back > 0 && shift(id, payer, "", 0, back, "a town's rebate") && noteTowns_)
                rebated_[id] += back;
    }
}

std::int64_t Society::roadMoneyFor(const std::string& trader) const
{
    if (!roadBudgeted() || trader.rfind("stores:", 0) != 0)
        return -1;
    const auto found = state_.memory.roadLeft.find(trader);
    return found == state_.memory.roadLeft.end() ? -1 : std::int64_t(std::max(0., found->second));
}

void Society::flushEarnings()
{
    auto& memory = state_.memory;
    for (const auto& [id, coins] : earnedToday_)
    {
        memory.monthIncome[id] += coins;
        memory.weekIncome[id] += coins;
    }
    earnedToday_.clear();
}

Spot Society::townSpot(const std::string& community, const std::string& salt) const
{
    // Its square, or else before one of its shops.
    if (const auto plan = day_.plans.find(community); plan != day_.plans.end() && !plan->second.crowd.empty())
        return plan->second.crowd[std::hash<std::string>{}(salt) % plan->second.crowd.size()];
    std::vector<Spot> about;
    for (const auto& p : positions_)
        if (p.role == "merchant" && communityOfResident(p.founder) == community)
            about.push_back(p.serve);
    return about.empty() ? Spot{} : about[std::hash<std::string>{}(salt) % about.size()];
}

std::int64_t Society::hireForWeek(const std::string& payer, const std::string& community, const std::string& kind, const std::string& what,
                                  const Spot& at, int slots, std::int64_t wage, int days, std::int64_t day)
{
    if (slots < 1 || wage < 1 || at.cell.empty() || !account(payer) || spendable(payer) < wage * slots)
        return 0;
    OddJob j;
    j.id = "odd" + std::to_string(++nextOddJob_);
    j.payer = payer;
    j.community = community;
    j.kind = kind;
    j.what = what.substr(0, 80);
    j.from = j.to = at;
    j.slots = std::min(slots, 12);
    j.pay = wage * j.slots;
    j.forChildren = false;
    j.until = day + days - 1;
    oddJobs_.push_back(std::move(j));
    ++oddVersion_;
    return oddJobs_.back().pay;
}

int Society::estateMost() const
{
    // The model's `len(self.workers) // 10`: the grown wolves of working age at the last labour pass (before the first
    // after a restart, by their written ages).
    int grown = grown_;
    if (grown <= 0)
        for (const auto& [id, life] : state_.residents)
            if (const auto* r = spec(id); r && r->age >= 16 && r->age < RetireAge)
                ++grown;
    return grown / EstateShare;
}

std::int64_t Society::houseCrewForWeek(const std::string& house, const std::string& community, const std::string& kind, const std::string& what,
                                       const std::vector<Spot>& spots, int slots, std::int64_t wage, std::int64_t day)
{
    // Its crew in gangs, as many as its purse pays a day of (the model's staff_to: its posts are what its budget pays).
    // Each gang's places are taken as a business's hire's are (oddJobFor's bestOf), by its pay.
    if (wage < 1 || spots.empty() || !account(house))
        return 0;
    slots = int(std::min<std::int64_t>(slots, spendable(house) / wage));
    std::int64_t cost = 0;
    for (std::size_t k = 0; slots > 0; ++k)
    {
        const auto& at = spots[k % spots.size()];
        const int gang = std::min(slots, CrewGang);
        slots -= gang;
        if (at.cell.empty())
            continue;
        OddJob j;
        j.id = "odd" + std::to_string(++nextOddJob_);
        j.payer = house;
        j.community = community;
        j.kind = kind;
        j.what = what.substr(0, 80);
        j.from = j.to = at;
        j.slots = gang;
        j.pay = wage * gang;
        j.forChildren = false;
        j.until = day + HireDays - 1;
        cost += j.pay;
        oddJobs_.push_back(std::move(j));
        ++oddVersion_;
    }
    return cost;
}

std::int64_t Society::buyAndUse(const std::string& payer, const std::string& community, const std::function<bool(const std::string&)>& is,
                                std::int64_t budget, const std::string& what, bool keep)
{
    if (budget < pennies(2 / 12., community))
        return 0;
    std::vector<std::string> shops;
    for (const auto& p : positions_)
        if (p.role == "merchant")
            if (const auto held = state_.careers.positions.find(p.id); held != state_.careers.positions.end() &&
                                                                     !held->second.holder.empty() && communityOfResident(held->second.holder) == community)
                shops.push_back(held->second.holder);
    std::map<std::string, int> got;
    const auto spent = buyForSurplus(payer, shops, is, budget, keep ? what : "bought for " + what, &got);
    if (!keep)
        for (const auto& [item, n] : got)
            consume(payer, item, n, "used in " + what);
    return spent;
}

void Society::institutionsWeek(std::int64_t day)
{
    auto& memory = state_.memory;
    // Each institution's week's budget: a quarter of a month's income, and its share of what it holds over k months of
    // it. In the realm's first month, before it has a month's income, it reckons that a k+1-th of what it holds.
    const auto budgetOf = [&](const std::string& id, double k, double a2) {
        const auto* purse = account(id);
        if (!purse)
            return std::int64_t(0);
        // (A town's holdings tax not yet spent back isn't its holding: it is spent at the week's end, on its works.)
        const double held = double(purse->cash) - biteHeld(id);
        const double month = day < MonthDays ? held / (k + 1) : monthIncomeOf(id);
        return std::int64_t(std::clamp((month + a2 * (held - k * month)) / 4, 0., held));
    };
    // Where a crew's gangs of CrewGang work, with work by the hours: about its town, each at its own spot.
    const auto gangsAbout = [&](const std::string& community, const std::string& salt, int most) {
        std::vector<Spot> spots;
        for (int k = 0; k * CrewGang < std::min(most, estateMost()); ++k)
            spots.push_back(townSpot(community, salt + "|" + std::to_string(k)));
        return spots;
    };
    // The wages it paid this week are its own posts' (the watch, the clergy, the Town Works): out of its budget first.
    const auto wagesPaid = [&](const std::string& id) {
        const auto found = memory.wagesWeek.find(id);
        return found == memory.wagesWeek.end() ? std::int64_t(0) : std::int64_t(found->second);
    };
    std::vector<std::string> towns;
    for (const auto& [id, a] : state_.accounts)
        if (townAccount(id))
            towns.push_back(id);
    for (const auto& id : towns)
    {
        const auto community = id == "treasury" ? capital() : id.substr(7);
        // (A town's month is its tax; in round 8, with work by the hours, its caravans' net takings too: Society::record.)
        auto budget = budgetOf(id, 2, .4);
        // The capital kept to its income (round 12, lane B; CapitalByIncome; the user, 2026-10-29), with work by the hours:
        // a quarter of its month's income a week, and CapitalDraw of what it holds over CapitalReserveMonths of it; never
        // its savings under that floor. Less what went out last week outside its parts (its caravans' unsold goods bought
        // back, rewards set aside: Society::record). In round 11's year its budget was already about its income under two
        // months of it, but its watch was kept past its posts' share, its own Town Works' day labour was paid every spell
        // whatever its budget (3k a week in the spring, 12-17k in the last month as its wolves fell out of work) and its
        // caravans' unsold goods came on top (3-8k a week): 315k to 144k. Below, its week's parts in the order of the cuts.
        const bool byIncome = id == "treasury" && workByHours() && wageRule(CapitalByIncome);
        if (byIncome)
        {
            const double held = std::max(0., double(account(id)->cash) - biteHeld(id));
            const double month = day < MonthDays ? held / 3 : std::max(0., monthIncomeOf(id));
            double other = 0;
            if (const auto out = memory.spendWeek.find("treasury|other"); out != memory.spendWeek.end())
            {
                other = out->second;
                memory.spendWeek.erase(out);
            }
            budget = std::int64_t(std::clamp(month / 4 + std::max(0., held - CapitalReserveMonths * month) * CapitalDraw - other, 0., held));
        }
        // Since round 9 its caravans' net takings since its last week's budget come on top, spent as they come (the user,
        // 2026-10-27), and its month is its tax alone: it holds two months of its tax, not of its trade. A week below
        // nothing (its road money out, the takings not yet home) adds nothing, and is netted against the takings to come.
        // (A town's stores only: the capital's caravans are its Houses'.)
        const auto taxed = budget;
        std::int64_t trade = 0;
        if (const auto found = memory.caravanWeek.find(id); found != memory.caravanWeek.end() && found->second > 0)
        {
            trade = std::int64_t(found->second);
            budget = std::min(budget + trade, account(id)->cash);
            memory.caravanWeek.erase(found);
        }
        // The week's relief, set aside: what it gave last week and a third over, and a twentieth of its budget besides.
        // (With CapitalByIncome as much of the budget as that comes to: relief is the last cut.)
        const auto relief = std::min<std::int64_t>(byIncome ? budget : budget * 3 / 10, std::int64_t(memory.reliefWeek[id] * 1.33) + budget / 20);
        memory.reliefLeft[id] = double(relief);
        memory.reliefWeek[id] = 0;
        // A small town's day labour from its own tax first (round 11, the user, 2026-10-29; TownLabourFirst, with the
        // capital carrying it): its week's allowance for it is its tax's budget after its relief and its other posts' wages
        // last week (its watch's: the day labour it paid itself is its own measure, not counted twice), and the capital
        // carries only what that doesn't cover. Its caravans' week isn't counted: that is its works'. What it pays is in
        // its wages paid next week, as its watch's are (postsFirst's need, below). Before, the capital paid all of it:
        // Oakhaven's too, holding 50-72k.
        if (id != "treasury" && workByHours() && wageRule(CapitalLabour) && wageRule(TownLabourFirst))
        {
            std::int64_t own = 0;
            if (const auto paid = memory.labourWeek.find(id); paid != memory.labourWeek.end())
            {
                own = std::int64_t(paid->second);
                memory.labourWeek.erase(paid);
            }
            memory.labourLeft[id] = double(std::max<std::int64_t>(0, taxed - relief - std::max<std::int64_t>(0, wagesPaid(id) - own)));
        }
        // Its caravans' money pays its own posts first (round 9, the user, 2026-10-27): what its posts and relief came to
        // over its tax budget, this week and before, is made good from its caravans' takings, and its works get only
        // what's left of them, with what its tax budget leaves over its posts. (Before, its posts were taken from the tax
        // budget and the caravans' week together each week, and a week's takings went to its works while the weeks with
        // its caravans on the road paid its posts from its purse: Frosthollow's ran dry.)
        // Since round 10 (the user, 2026-10-27; TownPosts) a town keeps its own posts to its tax as the capital does, below:
        // its caravans' money then pays only what its watch comes to over that, and its buyers, its road money and its works.
        const bool townPosts = workByHours() && wageRule(TownPosts) && id != "treasury";
        const bool postsFirst = !townPosts && workByHours() && wageRule(CaravansAsTheyCome) && wageRule(PostsFirst) && id != "treasury";
        if (postsFirst)
        {
            const auto need = relief + wagesPaid(id);
            auto owed = memory.postsOwed[id] + double(std::max<std::int64_t>(0, need - taxed));
            const auto repaid = std::min(double(trade), owed);
            owed -= repaid;
            if (owed > 0)
                memory.postsOwed[id] = owed;
            else
                memory.postsOwed.erase(id);
            budget = std::min(std::max<std::int64_t>(0, taxed - need) + trade - std::int64_t(repaid), account(id)->cash);
        }
        if (workByHours() && (id == "treasury" || townPosts))
        {
            // The capital, with work by the hours (doc 64, Phase 13, round 6; the model's treasury_week): its posts first,
            // the watch and then its own, to half its week's budget, its own let go if they come to more; then its buyers'
            // funds inside what's left (what they drew last week and a third over, and a twentieth of the rest besides;
            // its first week, all the rest); then its works, as a town's. Before, it paid its posts every spell and funded
            // its buyers day by day whatever its budget, and it ran dry in three months.
            // A town since round 10 (TownPosts) the same, its posts to half its tax's budget (its caravans' week not
            // counted: Frosthollow's posts came to 290p a day on 248p of tax). And its road money (RoadBudget) inside
            // what's left after its buyers, as theirs is, its caravans' takings counted in its week as they come home.
            budget -= relief;
            // (The capital's week, since round 10 (the user, 2026-10-28; CapitalLabour), counts first the small towns'
            // day labour it paid since its last: its posts and buyers get what's left of it.)
            // Since round 11 (the user, 2026-10-29; CarriedPosts) it is counted among its posts instead, after its watch
            // and before its own: what it asked last week (paid or not) and a tenth, inside its posts' share of the budget,
            // and what its own posts leave of the share on top (so the whole share comes off its budget: its buyers and
            // works get the rest, as the model's staff_to(watch, budget * 0.5)). Past that it is let go as its own posts
            // are, the day labour unpaid for the rest of the week (the wolves to relief), its own posts closed first. Counted off the budget whole, it was paid every spell without limit,
            // 125-165k a quarter, and the treasury fell from 266k to 11k in round 10's year.
            // (Since round 12, CapitalByIncome, its own Town Works' day labour is in the allowance and the ask too, and the
            // posts' share is all of its budget after relief: the watch, the day labour (at most CapitalLabourRoom over
            // what it asked), its own posts; its buyers' funds get what's left, the first cut.)
            const bool carriedPosts = id == "treasury" && ((wageRule(CapitalLabour) && wageRule(CarriedPosts)) || byIncome);
            double carried = 0;
            if (id == "treasury")
            {
                const auto asked = memory.labourWeek.find("treasury|asked");
                carried = asked != memory.labourWeek.end() ? asked->second : memory.labourWeek[id];
                if (!carriedPosts)
                    budget -= std::int64_t(memory.labourWeek[id]);
                memory.labourWeek.erase(id);
                memory.labourWeek.erase("treasury|asked");
                carried *= CarriedGrowth;
            }
            const double room = byIncome ? carried * CapitalLabourRoom : -1;
            budget -= keepPostsTo(id, double(std::max<std::int64_t>(0, id == "treasury" ? budget : taxed - relief)) * (byIncome ? 1. : TreasuryStaff),
                                  day, carriedPosts ? &carried : nullptr, room);
            if (carriedPosts)
                memory.labourLeft[id] = carried;
            // (A town's buyers and caravans that drew nothing since the rule began get a twentieth of the rest: the
            // capital's first week, saved before round 6, all of it.)
            const auto allow = [&](std::map<std::string, double>& left, std::map<std::string, double>& week) {
                const auto rest = std::max<std::int64_t>(0, budget);
                const auto drew = week.find(id);
                const auto most = drew == week.end() && id == "treasury"
                                      ? rest
                                      : std::min(rest, std::int64_t((drew == week.end() ? 0. : drew->second) * 1.33) + rest / 20);
                left[id] = double(most);
                week[id] = 0;
                budget -= most;
            };
            allow(memory.fundsLeft, memory.fundsWeek);
            if (townPosts && roadBudgeted())
                allow(memory.roadLeft, memory.roadWeek);
        }
        else if (!postsFirst)
            budget -= relief + wagesPaid(id);
        // Its holdings tax not yet spent back (round 12, lane B, HoldingsBite): on its works' crew this week, on top of
        // its budget, not saved. (It was counted in nothing above: not its income, nor its holding.)
        if (workByHours() && wageRule(HoldingsBite))
            if (const auto bite = memory.biteLeft.find(id); bite != memory.biteLeft.end())
            {
                auto spend = std::min<std::int64_t>(std::int64_t(std::max(0., bite->second)), account(id)->cash);
                // Round 13, lane 2 (SurplusByNeed; the user, 2026-10-29). A town: its pull's part of what's left of its bite
                // (its relief past its allowance and its day labour past its own were paid from it this week: its own poor
                // first) goes on to the capital's pot, inside the room it has ("holdings tax sent on"); the rest to its
                // works as before. In round 12's run the works' places were taken 37% of the time in Larkfield and 46% in
                // Oakhaven in the last quarter (the capital's all of them): what wasn't paid out became the stores' own and
                // went back to the payers by the rebate.
                if (surplusByNeed() && id != "treasury")
                    if (const auto send = std::min<std::int64_t>(std::llround(double(spend) * surplusPull(community)), std::int64_t(capitalPotRoom()));
                        send > 0 && shift(id, "treasury", "", 0, send, "holdings tax sent on"))
                    {
                        memory.biteLeft["treasury"] += double(send);
                        spend -= send;
                        if (noteTowns_)
                            sentOn_[id] += send;
                    }
                // The capital: on its works' crew only what its places can take on top of its budget's crew (estateMost
                // at the most); the rest kept in its pot, not erased, for its relief past its allowance, its own day labour
                // past its own (Society's day labour, bitePays) and next week's crew.
                if (surplusByNeed() && id == "treasury")
                {
                    // (What brings its crew, a week of 80% of the budget at the going rate a place, to estateMost: the
                    // places its budget's crew leaves, and the part of one its budget already pays.)
                    const auto wage = std::int64_t(std::ceil(goingRate(community) - 1e-9));
                    const auto base = std::max<std::int64_t>(0, budget);
                    const auto most = std::int64_t(estateMost()) * std::max<std::int64_t>(1, wage * 7) * 10;
                    const auto foldable = std::max<std::int64_t>(0, (most - 8 * base + 7) / 8);
                    spend = std::min(spend, foldable);
                    budget = base + spend;
                    if (noteTowns_)
                        biteSpent_[id] += spend;
                    bite->second -= double(spend);
                    if (bite->second < 1)
                        memory.biteLeft.erase(bite);
                }
                else
                {
                    budget = std::max<std::int64_t>(0, budget) + spend;
                    if (noteTowns_)
                        biteSpent_[id] += spend;
                    memory.biteLeft.erase(bite);
                }
            }
        if (budget <= 0 || community.empty())
            continue;
        // Its works: a crew hired for the week with most of the rest, materials with what's left. With work by the hours,
        // by a House's crew rules (doc 64, Phase 13, round 7; the model's treasury_week staffs its works to its budget):
        // as many as that pays, up to a tenth of the realm's workers, in gangs about its town. (It was twelve at most, in
        // one place: Oakhaven's was full from day 77 and the rest of its budget went unspent.)
        const auto wage = std::int64_t(std::ceil(goingRate(community) - 1e-9));
        const int crew = int(budget * 8 / 10 / std::max<std::int64_t>(1, wage * 7));
        if (workByHours())
            budget -= 7 * houseCrewForWeek(id, community, "building work", "the town's works", gangsAbout(community, id + "|works", crew),
                                           std::min(crew, estateMost()), wage, day);
        else
            budget -= 7 * hireForWeek(id, community, "building work", "the town's works", townSpot(community, id + "|works"), crew, wage, 7, day);
        buyAndUse(id, community, building, budget, "the town's works", false);
    }
    // The church: its relief (at least a tenth of its budget), its clergy, its works, and its own masons.
    if (account(SharedChurch))
    {
        auto budget = budgetOf(SharedChurch, 2, .4);
        // With work by the hours (round 10, lane A, the user, 2026-10-27): never below a month of its income (its floor,
        // which spendable keeps for every coin it pays out), and its alms bought for the hungry inside its week's relief
        // with its relief (Society::record counts them; the alms ask what's left). Before, its relief was set aside but its
        // alms were bought on top, as many as the hungry asked: it ran dry from day 170 of the year's run.
        if (workByHours() && wageRule(ChurchBudget))
        {
            const double held = double(account(SharedChurch)->cash);
            const auto floor = std::int64_t(day < MonthDays ? held / 3 : std::max(0., monthIncomeOf(SharedChurch)));
            memory.spendLeft[std::string(SharedChurch) + "|floor"] = double(floor);
            budget = std::min<std::int64_t>(budget, std::max<std::int64_t>(0, account(SharedChurch)->cash - floor));
        }
        const auto relief = std::min<std::int64_t>(budget, std::max<std::int64_t>(std::int64_t(memory.reliefWeek[SharedChurch] * 1.33), budget / 10));
        memory.reliefLeft[SharedChurch] = double(relief);
        memory.reliefWeek[SharedChurch] = 0;
        budget -= relief + wagesPaid(SharedChurch);
        if (budget > 0)
        {
            const auto community = capital();
            budget -= buyAndUse(SharedChurch, community, building, budget / 2, "the church's works", false);
            const auto wage = std::int64_t(std::ceil(goingRate(community) - 1e-9));
            const int masons = int(budget / std::max<std::int64_t>(1, wage * 7));
            // (With work by the hours, as a House's crew, up to a tenth of the realm's workers: the model's fabric. It was
            // twelve every week, and the rest of its budget stayed in its purse: round 7.)
            if (workByHours())
                houseCrewForWeek(SharedChurch, community, "building work", "the church's masons", gangsAbout(community, "church|masons", masons),
                                 std::min(masons, estateMost()), wage, day);
            else
                hireForWeek(SharedChurch, community, "building work", "the church's masons", townSpot(community, "church|masons"), masons,
                            wage, 7, day);
        }
    }
    // The Houses: retainers, finery, works and an estate crew.
    for (const auto& h : houses())
    {
        auto budget = budgetOf(h.id, 4, .3);
        if (workByHours() && wageRule(HouseBudget) && account(h.id))
            budget = houseParts(h, day);
        const auto* seat = position(h.head);
        if (budget <= 0 || !seat)
            continue;
        const auto wage = std::int64_t(std::ceil(goingRate(h.community) - 1e-9));
        const int retainers = std::min(12, int(budget * 3 / 10 / std::max<std::int64_t>(1, wage * 7)));
        if (workByHours())
            budget -= 7 * houseCrewForWeek(h.id, h.community, "about the home", "a retainer of " + h.name, {seat->work}, retainers, wage, day);
        else
            budget -= 7 * hireForWeek(h.id, h.community, "about the home", "a retainer of " + h.name, seat->work, retainers, wage, 7, day);
        // (Finery kept, a few pieces a House: what it has worn out goes in Phase 9.)
        int pieces = 0;
        for (const auto& [item, n] : account(h.id)->stock)
            pieces += finery(item) ? n : 0;
        if (pieces < 12)
            budget -= buyAndUse(h.id, h.community, finery, budget * 3 / 10, "a House's finery", true);
        budget -= buyAndUse(h.id, h.community, building, budget * 4 / 10, "a House's works", false);
        const int crew = int(std::max<std::int64_t>(0, budget) / std::max<std::int64_t>(1, wage * 7));
        if (!workByHours())
        {
            hireForWeek(h.id, h.community, "building work", "the estate of " + h.name, seat->work, crew, wage, 7, day);
            continue;
        }
        // With work by the hours, the model's size (doc 64, Phase 13, round 6): as many as what's left pays, up to a
        // tenth of the realm's workers, in gangs about its town, keeping its fabric (CrewMend). (It was twelve at most, in
        // its hall, and they went mostly untaken.)
        const int most = std::min(crew, estateMost());
        houseCrewForWeek(h.id, h.community, "building work", "the estate of " + h.name, gangsAbout(h.community, h.id + "|estate", most), most,
                         wage, day);
    }
    memory.wagesWeek.clear();
    ++memory.revision;
}

std::int64_t Society::houseParts(const House& h, std::int64_t day)
{
    // A House's week's budget, by the model's rule (house_week: k 4, a2 0.3), over all it pays out (round 10, lane A, the
    // user, 2026-10-27). Its month's income is its rent and its businesses' profit (its first month, a fifth of what it
    // holds). What its caravans brought home since its last week's budget comes on top, spent as it comes, as a town's
    // since round 9: their road money draws on its budget, so their takings pay for the next. (Counted in its month, it
    // held four months of its trade, the trial's Mossgrave Court 300,000p of target on 14,000p a month of rent and profit:
    // the Houses held 0.19 of the money at day 154 against the model's 0.105, and the realm went hungry from day 112.)
    // Never below its floor: a week's wages for HouseFloorHands at its town's going rate (spendable keeps it for every coin
    // it pays out). Then its parts, in the order they are cut when it holds less than its target (four months' income):
    // its posts' wages first, then its caravans, each what it paid out last week and a third over (a tenth of what's left
    // at least: the model's church_week relief; its caravans at least what they brought home); then its investments in
    // its businesses, the same, only while it holds its target (below it they pause first); and what's left is its works
    // and crews' (retainers, finery, materials, its estate crew: below its target they shrink next, its caravans last).
    // Each part paid out stops for the week when it's spent (houseMay); what's left of a part stays in its purse.
    auto& memory = state_.memory;
    const double held = double(account(h.id)->cash);
    const double month = day < MonthDays ? held / 5 : monthIncomeOf(h.id);
    auto& takings = memory.spendWeek[h.id + "|takings"];
    const auto trade = std::int64_t(takings);
    takings = 0;
    const auto floor = 7 * HouseFloorHands * std::int64_t(std::ceil(goingRate(h.community) - 1e-9));
    memory.spendLeft[h.id + "|floor"] = double(floor);
    auto budget = std::int64_t(std::clamp((month + .3 * (held - 4 * month)) / 4 + double(trade), 0., std::max(0., held - double(floor))));
    // What it didn't pay out of last week's budget (CarryParts; the user, 2026-10-27): what's left of each part, and of
    // its works and crews' (a crew's places untaken, materials the shops hadn't), goes to this week's works and crews on
    // top, as far as it holds over its floor. (Kept, a House paid out 75-85% of its budget in round 10's year, and held
    // 0.025-0.065 of the money over the model's from day 140.) It is money still in its purse, not counted twice: what's
    // left of this week's carries on to the next in its turn.
    std::int64_t carried = 0;
    if (wageRule(CarryParts))
        for (const auto* name : {"|posts", "|road", "|tops", "|works"})
            if (const auto left = memory.spendLeft.find(h.id + name); left != memory.spendLeft.end())
                carried += std::max<std::int64_t>(0, std::int64_t(left->second));
    const auto own = budget, room = std::max<std::int64_t>(0, std::int64_t(held) - floor - budget);
    const auto part = [&](const char* name, bool open, std::int64_t least) {
        auto& last = memory.spendWeek[h.id + name];
        const auto give = open ? std::min<std::int64_t>(budget, std::max({std::int64_t(last * PartGrowth), std::int64_t(double(budget) * PartLeast), least}))
                               : std::int64_t(0);
        memory.spendLeft[h.id + name] = double(give);
        last = 0;
        budget -= give;
    };
    part("|posts", true, 0);
    part("|road", true, trade);
    part("|tops", held >= 4 * month, 0);
    // (At most this week's own budget again: a House whose purse fell since, by its tax or a thief, doesn't spend the
    // rest of it to its floor in a week on what it had set aside.)
    budget += std::min({carried, room, own});
    // (Its works and crews' part, counted down as they are paid, for the next week's carry.)
    memory.spendLeft[h.id + "|works"] = double(budget);
    return budget;
}

std::int64_t Society::keepPostsTo(const std::string& treasury, double share, std::int64_t day, double* carried, double over)
{
    // Its posts that draw a wage (an unpaid one too, held by a grown wolf: doc 42, Phase 3; not a beggar's, nor a great
    // house's head's), each with its employer as the labour market finds it (its holder's, else its founder's).
    struct Post
    {
        const Position* p;
        bool guard;
        double order, week;
    };
    std::vector<Post> posts;
    for (const auto& p : positions_)
    {
        if (!p.paid && (p.role != "civilian" || houseHead(p.title) || p.title.find("beg") != std::string::npos))
            continue;
        const auto held = state_.careers.positions.find(p.id);
        const auto holder = held == state_.careers.positions.end() ? std::string() : held->second.holder;
        const auto* r = holder.empty() ? nullptr : spec(holder);
        const auto key = postEmployer(p, holder, r ? r->age : 30);
        const bool guard = key == treasury + "|guard";
        if (guard || key == treasury + "|labour")
            posts.push_back({&p, guard, lot(p.id + "|kept"), employerWage(key) * (guard ? 7 : PostWeekDays)});
    }
    std::sort(posts.begin(), posts.end(), [](const Post& a, const Post& b) {
        return a.guard != b.guard ? a.guard : a.order != b.order ? a.order < b.order : a.p->id < b.p->id;
    });
    double left = share;
    bool room = true;
    // (The day labour it carries, after its watch: what it asks, as far as what's left goes; and after its own posts,
    // what's left of the share on top, so it is cut only when the posts' share runs short, not because the out of work
    // grew faster than last week's ask: in a 30-day trial an allowance of the ask alone left Frosthollow's day labour unpaid on a
    // treasury of 300k. Round 11, CarriedPosts.)
    double* const given = carried;
    const auto carry = [&] {
        if (carried && *carried >= 0)
        {
            *carried = std::clamp(left, 0., *carried);
            left -= *carried;
            carried = nullptr;
        }
    };
    for (const auto& post : posts)
    {
        if (!post.guard)
            carry();
        room = room && (post.guard || left >= post.week);
        if (room)
        {
            left -= post.week;
            if (state_.memory.closed.erase(post.p->id))
            {
                record("taken on again", treasury, post.p->id, "", 0, 0);
                ++state_.memory.revision;
            }
            continue;
        }
        auto& ps = state_.careers.positions[post.p->id];
        if (!ps.holder.empty())
        {
            record("laid off", treasury, ps.holder, "", 0, 0);
            ps.lastHolder = ps.holder;
            ps.holder.clear();
            ps.vacantSince = double(day);
            forgetCareers();
        }
        if (state_.memory.closed.insert(post.p->id).second)
            ++state_.memory.revision;
    }
    carry();
    // (With `over` (CapitalByIncome) what's left goes on top only so far: the rest is its buyers' funds'.)
    if (given && left > 0)
    {
        const double more = over >= 0 ? std::min(left, over) : left;
        *given += more;
        left -= more;
    }
    return std::int64_t(std::ceil(share - left));
}

void Society::dailyRelief(std::int64_t day, const std::map<std::string, LifeBody>& bodies)
{
    (void)day;
    auto& memory = state_.memory;
    // Each household short of a day's food money (or hungry with no food in): a day's, from the church, then its town.
    std::map<std::string, std::vector<std::string>> homes;
    for (const auto& [id, life] : state_.residents)
        if (!life.homeCell.empty() && bodies.count(id))
            homes[life.homeCell].push_back(id);
    for (const auto& [home, members] : homes)
    {
        std::int64_t pot = 0;
        bool hungry = false;
        for (const auto& id : members)
        {
            const auto* a = account(id);
            pot += a ? a->cash : 0;
            hungry |= state_.residents.at(id).hunger >= 60 && a && !hasFood(*a);
        }
        const auto need = householdNeed(members.size(), communityOfResident(members.front()));
        if (pot >= need && !hungry)
            continue;
        const auto eldest = *std::max_element(members.begin(), members.end(), [&](const std::string& a, const std::string& b) {
            return bodies.at(a).age < bodies.at(b).age;
        });
        auto want = std::max<std::int64_t>(1, need - std::max<std::int64_t>(0, pot));
        const auto town = treasuryOf(communityOfResident(eldest));
        for (const auto& payer : {std::string(SharedChurch), town})
        {
            // (Before its first reckoning, what its first week's budget would set aside: a k+1-th of what it holds, a
            // quarter of that, and a tenth of it for the church, a twentieth for a town.)
            if (!memory.reliefLeft.count(payer) && account(payer))
                memory.reliefLeft[payer] = double(account(payer)->cash) / 3 / 4 * (payer == SharedChurch ? .1 : .05);
            auto& left = memory.reliefLeft[payer];
            const auto give = std::min<std::int64_t>(want, std::int64_t(left));
            if (give > 0 && account(payer) && spendable(payer) >= give && shift(payer, eldest, "", 0, give, "relief"))
            {
                left -= double(give);
                memory.reliefWeek[payer] += double(give);
                want -= give;
            }
            if (want <= 0)
                break;
        }
        // (Round 12, lane B, HoldingsBite: past its week's relief, its town's holdings tax not yet spent back gives it.)
        if (want > 0)
            if (const auto bite = std::min<std::int64_t>({want, std::int64_t(biteHeld(town)), spendable(town)});
                bite > 0 && shift(town, eldest, "", 0, bite, "relief"))
            {
                memory.biteLeft[town] -= double(bite);
                if (noteTowns_)
                    biteSpent_[town] += bite;
            }
    }
}

std::int64_t Society::holdingsTax(const std::vector<std::string>& members, std::int64_t pot, std::int64_t need)
{
    // The holdings tax (round 11, lane 2, HoldingsTax; the user, 2026-10-29), with work by the hours: a seventh a day of
    // holdingsTaxRate() (a week's, 1% to start) of what the home holds over HoldingsMonths of its needs (six 28-day
    // months of householdNeed: the realm's measure of what a home must keep, not its income, which would spare just the
    // owners' households whose profit piled up: Wetmeadow Dairy's keeper 445p -> 150,262p in round 10's year), paid by
    // its members richest first to the town where the richest pays its tax (treasuryOfResident: stores:<town>, or the
    // capital's treasury), as its town tax is, and counted as the town's income for its week's budget (record). A keeper's
    // purse that is its own shop's till isn't the home's (tendHouseholds' pot leaves it out too).
    // Since round 12 (lane B, HoldingsBite; the user, 2026-10-29) graduated over the target (holdingsOwed), and what it
    // brings kept to be spent back in the town in the week (biteLeft). In round 11's year it took 131k (the towns 67k, the
    // capital 64k) while the residents over their targets held 25k over them on day 84 and 530k on day 364, a dairy's
    // keeper 99k: 1% a week of the excess grows slower than an owner's profit.
    const bool bite = workByHours() && wageRule(HoldingsBite);
    const double target = double(HoldingsMonths) * MonthDays * double(need);
    const auto owed = std::int64_t(std::floor(holdingsOwed(double(pot), target, holdingsTaxRate(), bite) / 7));
    if (owed < 1)
        return 0;
    std::vector<std::string> payers;
    for (const auto& id : members)
        if (const auto* r = spec(id); account(id) && account(id)->cash > 0 && !(r && r->role == "merchant" && tillOf(id) == id))
            payers.push_back(id);
    std::sort(payers.begin(), payers.end(), [&](const std::string& a, const std::string& b) {
        return account(a)->cash > account(b)->cash || (account(a)->cash == account(b)->cash && a < b);
    });
    if (payers.empty())
        return 0;
    const auto treasury = treasuryOfResident(payers.front());
    openAccount(treasury);
    std::int64_t paid = 0;
    for (const auto& id : payers)
        if (const auto give = std::min(owed - paid, account(id)->cash); give > 0 && shift(id, treasury, "", 0, give, "holdings tax"))
            paid += give;
    if (noteTowns_ && paid > 0)
        holdingsTaxed_[treasury] += paid;
    if (bite && paid > 0)
        state_.memory.biteLeft[treasury] += double(paid);
    return paid;
}

double Society::holdingsOwed(double pot, double target, double rate, bool graduated)
{
    // A week's: flat (round 11), the rate on what's held over the target. Graduated (round 12, HoldingsBite): the rate on
    // the part up to BiteFrom targets, BiteMiddle times it on the part up to BiteTop, BiteHigh times it above (each at
    // most MostHoldingsTax), so a working home over its target by a little pays as before and a fortune many times it
    // pays most of its week's growth. At the 1% lever, for a lodger's target of about 7,000p (six months of a wolf's
    // food at a 100p day): 1% of the next 7,000p, 3% of the next 14,000p, 6% over 28,000p; Whiteleas Dairy's keeper at
    // 99k about 4,800p a week against round 11's 920p. Never more than what's held over the target.
    const double over = pot - target;
    if (over <= 0 || rate <= 0 || target < 0)
        return 0;
    if (!graduated)
        return over * rate;
    const double t = std::max(1., target);
    const double low = std::min(over, (BiteFrom - 1) * t), mid = std::clamp(over - (BiteFrom - 1) * t, 0., (BiteTop - BiteFrom) * t),
                 high = std::max(0., over - (BiteTop - 1) * t);
    const double owed = low * rate + mid * std::min(MostHoldingsTax, rate * BiteMiddle) + high * std::min(MostHoldingsTax, rate * BiteHigh);
    return std::min(owed, over);
}

int Society::homePlaces(const std::vector<std::string>& members, const char* what, std::int64_t day, bool untaken) const
{
    // A home's places on a hire of its own still running (RichHomes): all of them, or those no one has taken.
    int n = 0;
    for (const auto& j : oddJobs_)
        if (!j.ended && j.until >= day && j.what == what && std::find(members.begin(), members.end(), j.payer) != members.end())
            n += untaken ? std::max(0, j.slots - int(j.stage.size())) : j.slots;
    return n;
}

void Society::householdRule(const std::string& home, const std::vector<std::string>& members, std::int64_t pot, std::int64_t day,
                            bool lodger)
{
    auto& memory = state_.memory;
    double month = 0;
    for (const auto& id : members)
        month += monthIncomeOf(id);
    const auto community = communityOfResident(members.front());
    const auto need = householdNeed(members.size(), community);
    // (Round 11, lane 2: the holdings tax first, on what it holds today; the rule on what's left.)
    if (workByHours() && wageRule(HoldingsTax))
        pot -= holdingsTax(members, pot, need);
    // A rich home spends with its budget (round 11, lane 2, RichHomes; the user, 2026-10-29), with work by the hours. A
    // lodger (a bunkhouse's, quarters') is a home of its own for help and servants too: the richest of round 10's year
    // were keepers living in bunkhouses, whose whole spare went to a want fund only things could spend.
    const bool rich = workByHours() && wageRule(RichHomes);
    const bool helps = !lodger || rich;
    // The one rule: a share of what's held over a month's income and HomeNeedDays of its needs, half towards things
    // and half on help about the home.
    const double target = month + HomeNeedDays * double(need);
    const double a2 = HomeShareLeast + (HomeShareMost - HomeShareLeast) * lot(home + "|a2");
    const double spare = (double(pot) - target) * a2 / MonthDays;
    auto& fund = memory.wantFund[home];
    auto& help = memory.helpBudget[home];
    if (spare <= 0)
        help *= .9;
    else if (!helps)
        fund += spare;
    else
    {
        fund += spare / 2;
        help = help * .9 + spare / 2;
    }
    // (Never more than what it holds over the target: a fund left unspent would count the same money again each day.
    // Doc 64, Phase 13, with work by the hours; at the old pace, as it was, only on a day with nothing to spare.)
    if (fieldsOn() || spare <= 0)
        fund = std::min(fund, std::max(0., double(pot) - target));
    if (!helps)
    {
        memory.helpBudget.erase(home);
        if (fund <= 0)
            memory.wantFund.erase(home);
        return;
    }
    // Help about the home: a hand for the week, at a little under the going rate, once the budget runs to it (a week's
    // hire, not a day's: fewer jobs on the board, the same work). With RichHomes as many hands as the budget pays a week
    // of, a post a day at most (hireForWeek's 12 places), and none while a place it posted stands untaken (the model's
    // home_help hires only the idle there are: a place no one takes would spend the budget on nothing). Its takers are
    // the out of work, children and the idle, who take a business's best-paid hire before chores (oddJobFor); one with a
    // post leaves it only for better pay than its own, and help pays .8 of the going rate.
    const auto pay = std::int64_t(std::ceil(.8 * goingRate(community) - 1e-9));
    const auto* where = state_.residents.count(members.front()) ? &state_.residents.at(members.front()) : nullptr;
    const auto richest = *std::max_element(members.begin(), members.end(), [&](const std::string& a, const std::string& b) {
        return (account(a) ? account(a)->cash : 0) < (account(b) ? account(b)->cash : 0);
    });
    if (where && help >= double(7 * pay))
    {
        const int hands = !rich ? 1
                          : homePlaces(members, "help about the home", day, true) > 0
                              ? 0
                              : int(std::min<double>({help / double(7 * pay), 12., double(spendable(richest)) / double(std::max<std::int64_t>(1, 7 * pay))}));
        const auto daily = hands > 0 ? hireForWeek(richest, community, "about the home", "help about the home",
                                                   Spot{where->homeCell, where->homeX, where->homeY}, hands, pay, 7, day)
                                     : std::int64_t(0);
        if (daily > 0)
            help -= double(7 * daily);
    }
    // Servants: holding over half a year of its income (an inheritance, a windfall), posts of its own for the week, as many
    // as a tenth of the excess pays for a month. (At most three, on one day in seven; with RichHomes as many as that pays
    // for, topped up to it on any day, in posts of hireForWeek's 12.)
    const double excess = double(pot) - ServantMonths * std::max(month, 7. * double(need));
    if (where && excess > 0 && (rich || std::int64_t(std::hash<std::string>{}(home + "|servants") % 7) == day % 7))
    {
        const auto wage = std::int64_t(std::ceil(.9 * goingRate(community) - 1e-9));
        const Spot spot{where->homeCell, where->homeX, where->homeY};
        const int wanted = int(excess * .1 / double(wage * MonthDays));
        if (!rich)
            hireForWeek(richest, community, "about the home", "a servant of the house", spot, std::min(3, wanted), wage, 7, day);
        else
            for (int more = wanted - homePlaces(members, "a servant of the house", day, false); more > 0;)
            {
                const auto posted = hireForWeek(richest, community, "about the home", "a servant of the house", spot, std::min(more, 12), wage, 7, day);
                if (posted <= 0)
                    break;
                more -= int(posted / wage);
            }
    }
    if (fund <= 0)
        memory.wantFund.erase(home);
    if (help <= .5)
        memory.helpBudget.erase(home);
}

bool Society::builds(const std::string& shop) const
{
    // (The building trades by name: a smithy's nails and fittings, a tinker's nails, a potter's bricks are a few of their
    // wares, and their hands are for their tools, blades and pots. Doc 64, Phase 13.)
    static const char* Trades[] = {"carpenter", "sawmill", "mason", "brickworks", "plasterworks"};
    const auto* keeper = spec(shop);
    const auto* business = keeper ? items::businessFor(keeper->workLabel) : nullptr;
    if (!business || std::find(std::begin(Trades), std::end(Trades), business->id) == std::end(Trades))
        return false;
    for (const auto* k : items::craftsFor(business->id))
        if (!k->out.empty() && building(k->out.front().first))
            return true;
    return false;
}

std::int64_t Society::worksOnHome(const std::string& payer, const std::vector<std::string>& builders, std::int64_t fund, std::int64_t most,
                                  std::int64_t day)
{
    // A few of the town's builders, by lot (the model's buy: up to WantSample sellers, the cheapest first), each with its
    // quote a unit: WorksHours at its town's hour, and a batch of the building good it has most batches of, at what they
    // cost it; with the ladder's markup, as a mend's (mendQuote).
    struct Quote
    {
        double each;
        std::string till, item;
        int batch;
    };
    std::vector<std::pair<std::size_t, const std::string*>> order;
    for (const auto& shop : builders)
        order.push_back({std::hash<std::string>{}(payer + "|works|" + std::to_string(day) + "|" + shop), &shop});
    std::sort(order.begin(), order.end(), [](const auto& a, const auto& b) { return a.first < b.first || (a.first == b.first && *a.second < *b.second); });
    std::vector<Quote> quotes;
    for (const auto& [l, shop] : order)
    {
        const auto* keeper = *shop == payer || quotes.size() >= WantSample ? nullptr : spec(*shop);
        const auto* business = keeper ? items::businessFor(keeper->workLabel) : nullptr;
        const auto till = tillOf(*shop);
        const auto* shelves = account(till);
        if (!business || !shelves || till == payer)
            continue;
        const items::Craft* best = nullptr;
        int batches = 0;
        for (const auto* k : items::craftsFor(business->id))
            if (!k->out.empty() && building(k->out.front().first))
                if (const int b = stockAll(*shelves, k->out.front().first) / std::max(1, k->out.front().second); b > batches)
                    best = k, batches = b;
        if (!best)
            continue;
        const auto& [item, n] = best->out.front();
        const double hour = labourDay(communityOfResident(*shop)) / PaidSpells;
        quotes.push_back({(WorksHours * hour + std::max(1, n) * costOf(till, item)) * (1 + items::ladder().markup), till, item, std::max(1, n)});
    }
    std::sort(quotes.begin(), quotes.end(), [](const Quote& a, const Quote& b) { return a.each < b.each || (a.each == b.each && a.till < b.till); });
    if (quotes.empty() || double(fund) < quotes.front().each)
        return 0;                                   // (Not yet a unit's worth in the fund.)
    // By the amount, up to half the fund (the model's), the cheapest first that has the hours: booked into its work in
    // hand, worked out of what it banks next, while what's booked is under half a day of its hands' time (as a mend).
    double left = std::min(double(fund) / 2, double(most));
    std::int64_t paid = 0;
    for (const auto& q : quotes)
    {
        if (left < 1)
            break;
        auto& banked = workBanked_[q.till];
        const double units = std::min(left / q.each, (banked + .5 * PaidSpells * std::max(1, handsOf(q.till))) / WorksHours);
        const auto price = std::llround(units * q.each);
        // Its goods: the units' batches, whole ones (a part by lot).
        const int goods = int(std::floor(units * q.batch + lot(payer + "|works|" + std::to_string(day))));
        if (price < 1 || stockAll(*account(q.till), q.item) < goods || !shift(payer, q.till, "", 0, price, "works on the home"))
            continue;
        int need = goods;
        for (const auto& sort : kindsHeld(*account(q.till), q.item))
            if (need > 0)
                need -= consume(q.till, sort, std::min(need, stock(*account(q.till), sort)), "used in works on the home");
        // (Its goods, as sold: its shelf's plan and its hands follow its works, as they follow its sales, and a shelf the
        // works emptied is a sell-out. Society::spoil.)
        if (goods - need > 0)
        {
            soldToday_[q.till][q.item] += goods - need;
            if (stockAll(*account(q.till), q.item) == 0)
                ranOut_[q.till].insert(q.item);
        }
        banked -= units * WorksHours;
        worksRate_[q.till] += units * WorksHours / MonthDays;
        left -= double(price);
        paid += price;
    }
    // (Wanted, and too few hours to be had: each builder it asked hears of it, a unit at most between them (the model's
    // asks), and takes hands on for it.)
    if (left >= 1)
        for (const auto& q : quotes)
            worksRate_[q.till] += std::min(1., left / quotes.front().each) * WorksHours / MonthDays / 4 / double(quotes.size());
    return paid;
}

void Society::leftovers(std::int64_t day)
{
    (void)day;
    // At the day's end a baker gives what would spoil overnight to the households that need it: the hungry first, then
    // the poor (under three days' food money). Goods, not money: into their larders.
    std::vector<std::pair<int, std::string>> hungry, poor;     // (Members, home.)
    std::map<std::string, std::pair<int, std::int64_t>> homes;
    std::map<std::string, bool> starving;
    for (const auto& [id, life] : state_.residents)
        if (!life.homeCell.empty())
        {
            auto& h = homes[life.homeCell];
            ++h.first;
            h.second += account(id) ? account(id)->cash : 0;
            starving[life.homeCell] = starving[life.homeCell] || (life.hunger >= 60 && account(id) && !hasFood(*account(id)));
        }
    for (const auto& [home, h] : homes)
        if (starving[home])
            hungry.push_back({h.first, home});
        else if (h.second < 3 * householdNeed(std::size_t(h.first)))
            poor.push_back({h.first, home});
    std::vector<std::pair<int, std::string>> queue = hungry;
    queue.insert(queue.end(), poor.begin(), poor.end());
    std::size_t next = 0;
    for (auto& [till, goods] : batches_)
    {
        if (till.rfind("till:", 0) != 0 && till.rfind("house:", 0) != 0)
            continue;
        for (auto& [item, lots] : goods)
        {
            const auto* good = items::good(item);
            if (!good || good->keeps <= 0 || items::baseOf(item) != "bread" && items::baseOf(item) != "fine_bread")
                continue;
            // What would be past keeping by tomorrow.
            int going = 0;
            for (const auto& [made, n] : lots)
                if (double(state_.budgetDay + 1 - made) > good->keeps)
                    going += n;
            going = std::min(going, account(till) ? stock(*account(till), item) : 0);
            while (going > 0 && next < queue.size())
            {
                const auto& [members, home] = queue[next];
                const auto larder = homeStore(home, "larder");
                openAccount(larder);
                const int give = std::min(going, std::max(1, members * 2));
                if (shift(till, larder, item, give, 0, "a baker's leftovers"))
                    going -= give;
                ++next;
            }
        }
    }
}
} // namespace ratw
