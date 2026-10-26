// Wear and mending for NPCs (Docs/Design/64-economy-migration.md, Phase 9; doc 63's model). Every grown wolf owns what
// it wears and carries: a set of clothes, the finery and blades it holds, and a guard the sword it's issued. They wear by
// days of use. Worn to MendAt, the owner sees to a piece: an heirloom is mended; the rich may move up to something
// better; "fashion" sometimes buys new; otherwise it's mended if the maker's quote is under MendIf of a new one. A
// mend takes the maker's hours and its own materials (a share of the piece's), and counts toward its hands. Mending never
// shortens a piece's life (doc 35): a mended piece is whole again. Founded pieces start at random condition. With work
// by the hours (Society::fieldsOn); daily, in the day's pass.
#include "RatwSociety.h"

#include "RatwItems.h"

#include <algorithm>
#include <cmath>
#include <functional>

namespace ratw
{
namespace
{
double unit(const std::string& a)
{
    return double(std::hash<std::string>{}(a) % 10000) / 10000;
}

// The model's kinds: days of use whole to worn out, a mend's share of the piece's hours and materials, the chance a
// piece is an heirloom, of fashion buying new, and what the rich move up to.
struct Repair
{
    double days, hours, materials, heirloom, fashion;
    const char* better;
};
const Repair& repairOf(const std::string& kind)
{
    static const Repair clothes{150, .5, .3, 0, .15, nullptr}, finery{240, .6, .4, .05, .35, nullptr},
        sword{360, .3, .1, .2, .02, "rapier"}, rapier{540, .3, .1, .4, .02, nullptr};
    return kind == "clothes" ? clothes : kind == "finery" ? finery : kind == "rapier" ? rapier : sword;
}
} // namespace

std::string Society::belongingKind(const std::string& item)
{
    const auto* good = items::good(items::baseOf(item));
    if (!good)
        return {};
    const auto& c = good->category;
    if (c == "jewelry" || c == "hat" || c == "scarf" || c == "shawl" || c == "paw_wear")
        return "finery";
    const auto base = items::baseOf(item);
    if (c == "weapon" && base == "rapier")
        return "rapier";
    if (c == "weapon" && (base == "sword" || base == "iron_sword" || base == "steel_sword"))
        return "sword";
    return {};
}

void Society::tendBelongings(std::int64_t day, const std::map<std::string, LifeBody>& bodies)
{
    if (!fieldsOn() || roster_ != Roster::Authored)
        return keepRates();
    // A set of clothes: what a household buys a head of (crafts.json `households`, `perPerson`).
    std::string clothes;
    for (const auto& need : items::householdNeeds())
        if (need.perPerson && !need.any.empty())
            clothes = need.any.front();
    std::map<std::string, std::vector<std::string>> shopsOf;
    for (const auto& r : authored_.residents)
        if (r.role == "merchant" && state_.residents.count(r.id))
            shopsOf[communityOfResident(r.id)].push_back(r.id);
    auto& all = state_.memory.belongings;
    for (const auto& [id, life] : state_.residents)
    {
        const auto body = bodies.find(id);
        const auto* purse = account(id);
        if (body == bodies.end() || body->second.age < 16 || !purse)
            continue;
        const auto* job = jobOf(id);
        const bool guard = job && job->role == "guard";
        auto& mine = all[id];
        const bool founding = mine.empty();
        const auto fresh = [&](const std::string& item, bool held) {
            const auto kind = held ? belongingKind(item) : item == clothes ? std::string("clothes") : std::string("sword");
            Belonging b;
            b.item = item;
            b.held = held;
            b.cond = founding ? MendAt + (1 - MendAt) * unit(id + "|" + item + "|founded|" + std::to_string(mine.size())) : 1.;
            b.heirloom = unit(id + "|" + item + "|heirloom|" + std::to_string(day)) < repairOf(kind).heirloom;
            mine.push_back(b);
        };
        // What it holds of finery and blades, piece by piece: new ones bought whole, those gone (sold, given) dropped.
        std::map<std::string, int> held;
        for (const auto& [item, n] : purse->stock)
            if (n > 0 && !belongingKind(item).empty())
                held[item] = n;
        std::map<std::string, int> counted;
        mine.erase(std::remove_if(mine.begin(), mine.end(), [&](const Belonging& b) {
                       return b.held && ++counted[b.item] > (held.count(b.item) ? held.at(b.item) : 0);
                   }),
                   mine.end());
        for (const auto& [item, n] : held)
            for (int k = counted[item]; k < n; ++k)
                fresh(item, true);
        // Its clothes, and a guard its issued sword (not in its purse: the watch's or its House's).
        if (!clothes.empty() && std::none_of(mine.begin(), mine.end(), [&](const Belonging& b) { return !b.held && b.item == clothes; }))
            fresh(clothes, false);
        const bool armed = std::any_of(mine.begin(), mine.end(), [&](const Belonging& b) {
            const auto k = belongingKind(b.item);
            return k == "sword" || k == "rapier";
        });
        if (guard && !armed)
            fresh("sword", false);
        // A day's wear; a piece worn to MendAt seen to (one a day).
        bool seen = false;
        for (auto& b : mine)
        {
            const auto kind = b.held ? belongingKind(b.item) : b.item == clothes ? std::string("clothes") : std::string("sword");
            b.cond = std::max(0., b.cond - 1. / (guard && !b.held && kind == "sword" ? WatchWearDays : repairOf(kind).days));
            if (!seen && b.cond < MendAt)
            {
                seen = true;
                seeTo(id, b, kind, guard && !b.held && kind == "sword", day, shopsOf[communityOfResident(id)]);
            }
        }
    }
    for (auto it = all.begin(); it != all.end();)
        it = state_.residents.count(it->first) ? std::next(it) : all.erase(it);
    for (auto& [till, rate] : mendRate_)
        rate *= 1 - 1. / MonthDays;                  // (A month's running average of mending hours a day.)
    keepRates();
}

void Society::keepRates()
{
    // Makers' mending and builders' works, into the saved memory at the day pass's end (doc 64, Phase 13: most of a
    // builder's hands; a restarted world's would be gone, a month to come back). What has worn to nothing, forgotten.
    auto& memory = state_.memory;
    for (auto* rates : {&mendRate_, &worksRate_})
        for (auto it = rates->begin(); it != rates->end();)
            it = it->second < 1e-4 ? rates->erase(it) : std::next(it);
    memory.mendRate = {mendRate_.begin(), mendRate_.end()};
    memory.worksRate = {worksRate_.begin(), worksRate_.end()};
}

void Society::loadRates()
{
    mendRate_ = {state_.memory.mendRate.begin(), state_.memory.mendRate.end()};
    worksRate_ = {state_.memory.worksRate.begin(), state_.memory.worksRate.end()};
}

double Society::mendQuote(const std::string& maker, const items::Craft& k, const Belonging& b, const std::string& kind) const
{
    const auto& r = repairOf(kind);
    const auto till = tillOf(maker);
    const double each = std::max(1, k.out.front().second);
    const double work = r.hours * hoursFor(k) / each * labourDay(communityOfResident(maker)) / PaidSpells;
    double materials = 0;
    for (const auto& [in, q] : k.in)
        materials += q / each * inputCost(till, in);
    return (1 - b.cond) * (work + r.materials * materials) * (1 + items::ladder().markup);
}

bool Society::seeTo(const std::string& id, Belonging& b, const std::string& kind, bool issued, std::int64_t day,
                    const std::vector<std::string>& shops)
{
    const auto& r = repairOf(kind);
    // Who pays: an issued sword, its guard's payer (the watch, a House); otherwise the wolf, beyond its food money (its
    // want fund's, for finery and blades).
    const auto* job = jobOf(id);
    const auto payer = issued && job ? payerOf(id, *job, 30).account : id;
    const auto* purse = account(payer);
    if (!purse)
        return false;
    const auto fundKey = state_.memory.wantFund.count(id) ? id : state_.residents.at(id).homeCell;
    const auto fund = state_.memory.wantFund.find(fundKey);
    std::int64_t budget = purse->cash - (payer == id ? FoodADay * 8 : 0);
    if (payer == id && kind != "clothes")
        budget = std::min<std::int64_t>(budget, fund == state_.memory.wantFund.end() ? 0 : std::int64_t(fund->second));
    if (budget <= 0)
        return false;
    const auto spend = [&](std::int64_t before) {
        if (const auto spent = before - account(payer)->cash; spent > 0 && payer == id && kind != "clothes" && fund != state_.memory.wantFund.end())
            fund->second = std::max(0., fund->second - double(spent));
    };
    // A new one at a shop in town: the cheapest that has one, at its price.
    const auto buyNew = [&](const std::string& item) {
        const auto before = account(payer)->cash;
        std::string from, sort;
        std::int64_t best = 0;
        for (const auto& shop : shops)
            if (shop != id)
                if (const auto* a = account(tillOf(shop)))
                    for (const auto& s : kindsHeld(*a, item))
                        if (const auto price = shopPrice(shop, s); price <= budget && (best == 0 || price < best))
                            best = price, from = shop, sort = s;
        if (best == 0 || !transfer(tillOf(from), payer, sort, 1, best, "a piece replaced"))
            return false;
        // (Worn, not kept in a purse: clothes, and an issued sword, go straight into use.)
        if (!b.held || payer != id)
            consume(payer, sort, 1, "worn");
        else
            consume(id, b.item, 1, "worn out");
        b.item = b.held ? sort : b.item;
        b.cond = 1;
        b.heirloom = unit(id + "|" + sort + "|heirloom|" + std::to_string(day)) < repairOf(belongingKind(sort).empty() ? kind : belongingKind(sort)).heirloom;
        spend(before);
        return true;
    };
    if (!b.heirloom)
    {
        // The rich may move up: a sword for a rapier, with a few months of its income to spare.
        if (r.better && b.held && unit(id + "|better|" + std::to_string(day)) < .3 &&
            budget > 3 * std::int64_t(state_.memory.monthIncome.count(id) ? state_.memory.monthIncome.at(id) : 0.) &&
            buyNew(r.better))
            return true;
        if (unit(id + "|fashion|" + b.item + "|" + std::to_string(day)) < r.fashion && buyNew(b.item))
            return true;
    }
    // Mended, by the cheapest maker in town with the hands' time and the materials, if under MendIf of new (an heirloom
    // whatever it costs).
    std::int64_t newPrice = 0;
    for (const auto& shop : shops)
        if (const auto* a = account(tillOf(shop)); a && shop != id && !kindsHeld(*a, b.item).empty())
            newPrice = newPrice == 0 ? shopPrice(shop, b.item) : std::min(newPrice, shopPrice(shop, b.item));
    std::string maker;
    const items::Craft* craft = nullptr;
    double quote = 0;
    for (const auto& shop : shops)
    {
        const auto* keeper = spec(shop);
        const auto* business = keeper ? items::businessFor(keeper->workLabel) : nullptr;
        if (!business || shop == id)
            continue;
        for (const auto* k : items::craftsFor(business->id))
            if (!k->out.empty() && k->out.front().first == items::baseOf(b.item))
                if (const double q = mendQuote(shop, *k, b, kind); maker.empty() || q < quote)
                    maker = shop, craft = k, quote = q;
    }
    if (craft && (b.heirloom || newPrice == 0 || quote < MendIf * double(newPrice)))
    {
        const auto till = tillOf(maker);
        const double each = std::max(1, craft->out.front().second);
        const double hours = (1 - b.cond) * r.hours * hoursFor(*craft) / each;
        // Its materials: a share of the piece's, whole ones (a part by lot).
        std::vector<std::pair<std::string, int>> used;
        bool short_ = false;
        for (const auto& [in, q] : craft->in)
        {
            const double m = (1 - b.cond) * r.materials * q / each;
            const int n = int(std::floor(m + unit(id + "|" + in + "|mend|" + std::to_string(day))));
            if (n > 0 && stockAll(*account(till), in) < n)
                short_ = true;
            if (n > 0)
                used.push_back({in, n});
        }
        const auto price = std::max<std::int64_t>(1, std::llround(quote));
        // (Booked into its hours: worked out of what it banks next, while its mending booked is under half a day of its
        // hands' time. Doc 63's model: yesterday's mending comes out of today's hours.)
        auto& banked = workBanked_[till];
        if (!short_ && banked - hours >= -.5 * PaidSpells * std::max(1, handsOf(till)) && price <= budget)
        {
            const auto before = account(payer)->cash;
            if (shift(payer, till, "", 0, price, "a piece mended"))
            {
                for (const auto& [in, n] : used)
                {
                    int left = n;
                    for (const auto& sort : kindsHeld(*account(till), in))
                        if (left > 0)
                            left -= consume(till, sort, std::min(left, stock(*account(till), sort)), "used mending");
                }
                banked -= hours;
                mendRate_[till] += hours / MonthDays;
                b.cond = 1;
                spend(before);
                return true;
            }
        }
        // (Too busy or short: the maker hears it was asked, and takes hands on for it.)
        mendRate_[till] += hours / MonthDays / 4;
    }
    return buyNew(b.item);
}
} // namespace ratw
