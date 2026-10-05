#include "RatwSociety.h"

#include "RatwItems.h"

#include <algorithm>
#include <cmath>
#include <functional>

// What townsfolk buy and use up (Docs/Design/35-items-crafting-industry.md, Part 7: households).
//
// Food is any good of the catalog that feeds (`nourish`; drinks aside): a hungry resident eats the best it carries,
// and buys at an open shop what gives the most for its money, by its own taste (bread and porridge are cheap filling, a
// meal or a pie a treat), enough for now and, with a larder at home, a couple of days more. Besides food, a household
// uses up firewood (more in winter), candles, a pot now and then, a scarf or hat for each grown wolf in a while: once a
// day whoever in it has the most money buys what is due from a shop in town that has it, keeping a little back for food.
// Every penny goes to the shop: money is only moved, never made.
namespace ratw
{
namespace
{
double unit(const std::string& a)
{
    return double(std::hash<std::string>{}(a) % 10000) / 10000;
}
} // namespace

bool Society::edible(const std::string& item)
{
    const auto* good = items::good(item);
    return good && good->nourish > 0 && !good->drink;
}

int Society::nourishment(const std::string& item)
{
    const auto* good = items::good(item);
    return good && !good->drink ? good->nourish : 0;
}

std::string Society::bestFood(const EconomyAccount& account)
{
    std::string best;
    int most = 0;
    for (const auto& [item, n] : account.stock)
        if (n > 0 && edible(item) && nourishment(item) > most)
        {
            most = nourishment(item);
            best = item;
        }
    return best;
}

bool Society::shopHasFood(const std::string& merchant) const
{
    const auto* a = account(tillOf(merchant));       // (A house's business: its till, doc 42.)
    if (!a)
        return false;
    const auto sold = wares(merchant);
    for (const auto& [item, n] : a->stock)
        if (n > 0 && edible(item) && std::find(sold.begin(), sold.end(), items::baseOf(item)) != sold.end())
            return true;
    return false;
}

int Society::buyFood(const std::string& resident, const std::string& seller, bool stocking)
{
    const auto till = tillOf(seller);
    const auto* shop = account(till);
    if (!shop || !account(resident))
        return 0;
    const auto sold = wares(seller);
    const bool stall = atStall_.count(seller) > 0;
    // What the shop has to eat, best for the money by this wolf's taste (the same each day: some like their porridge).
    struct Choice
    {
        std::string item;
        int price = 1, nourish = 0;
        double score = 0;
    };
    std::vector<Choice> choices;
    for (const auto& [item, n] : shop->stock)
    {
        if (n <= 0 || !edible(item) || std::find(sold.begin(), sold.end(), items::baseOf(item)) == sold.end())
            continue;
        const auto* good = items::good(item);
        const int price = std::max(1, stall ? int(std::floor(good->price * .9)) : good->price);
        const double taste = .6 + .8 * unit(resident + "|" + items::baseOf(item));
        choices.push_back({item, price, good->nourish, double(good->nourish) / price * taste});
    }
    std::sort(choices.begin(), choices.end(), [](const Choice& a, const Choice& b) { return a.score > b.score; });
    // Enough for now (a meal's worth), and for a couple of days more where there is a larder to keep it in.
    int wanted = 50 + (stocking ? 100 : 0), bought = 0;
    for (const auto& c : choices)
    {
        if (wanted <= 0)
            break;
        const auto* purse = account(resident);
        const int have = stock(*account(till), c.item);
        const int n = int(std::min<std::int64_t>({(wanted + c.nourish - 1) / c.nourish, have, 20, purse ? purse->cash / c.price : 0}));
        if (n > 0 && transfer(till, resident, c.item, n, c.price, "resident food purchase"))
        {
            bought += n;
            wanted -= n * c.nourish;
        }
    }
    return bought;
}

void Society::householdShopping(std::int64_t day, int season, const std::map<std::string, LifeBody>& bodies)
{
    const auto& needs = items::householdNeeds();
    if (needs.empty() || roster_ != Roster::Authored)
        return;
    // The households (by home), and the shops of each community.
    std::map<std::string, std::vector<std::string>> homes;
    for (const auto& [id, life] : state_.residents)
        if (!life.homeCell.empty() && bodies.count(id))
            homes[life.homeCell].push_back(id);
    std::map<std::string, std::vector<std::string>> shopsOf;
    for (const auto& r : authored_.residents)
        if (r.role == "merchant" && state_.residents.count(r.id))
            shopsOf[communityOfResident(r.id)].push_back(r.id);
    const int reserve = items::householdReserve();
    for (const auto& [home, members] : homes)
    {
        // Who goes shopping: the one with the most money; how many grown wolves there are.
        std::string buyer;
        std::int64_t most = -1;
        int grown = 0;
        for (const auto& id : members)
        {
            if (const auto* a = account(id); a && a->cash > most)
                most = a->cash, buyer = id;
            const auto body = bodies.find(id);
            grown += body != bodies.end() && body->second.age >= 16;
        }
        if (buyer.empty())
            continue;
        const auto community = day_.communityOf ? day_.communityOf(home) : storeFor(home);
        const auto shops = shopsOf.find(community);
        if (shops == shopsOf.end())
            continue;
        const double offset = unit(home) * 30;          // (Not every household on the same day.)
        for (std::size_t k = 0; k < needs.size(); ++k)
        {
            const auto& need = needs[k];
            const double every = season == 3 && need.winterDays > 0 ? need.winterDays : need.everyDays;
            const double at = double(day) + offset + double(k) * 7.3;
            int count = int(std::floor(at / every) - std::floor((at - 1) / every));
            if (need.perPerson)
                count *= std::max(1, grown);
            for (int left = count; left > 0;)
            {
                // A shop in town with any of it, at its price, and the purse to spare beyond food money.
                bool got = false;
                // A shopkeeper's own household uses its own stock first: nothing is bought.
                if (const auto* own = account(buyer))
                    for (const auto& want : need.any)
                        if (!got && std::find(shops->second.begin(), shops->second.end(), buyer) != shops->second.end())
                            for (const auto& kind : kindsHeld(*own, want))
                                if (!got && consume(buyer, kind, 1, "used at home") > 0)
                                    got = true;
                for (const auto& shop : shops->second)
                {
                    if (got)
                        break;
                    const auto* s = account(tillOf(shop));
                    if (!s || shop == buyer)
                        continue;
                    const auto sold = wares(shop);
                    for (const auto& want : need.any)
                    {
                        if (std::find(sold.begin(), sold.end(), want) == sold.end())
                            continue;
                        for (const auto& kind : kindsHeld(*s, want))
                        {
                            const auto* good = items::good(kind);
                            const auto* purse = account(buyer);
                            if (!good || !purse)
                                continue;
                            const std::int64_t price = std::max(1, good->price);
                            if (purse->cash - price < std::int64_t(reserve) * std::int64_t(members.size()))
                                continue;
                            if (transfer(tillOf(shop), buyer, kind, 1, price, "household purchase"))
                            {
                                consume(buyer, kind, 1, "used at home");
                                got = true;
                                break;
                            }
                        }
                        if (got)
                            break;
                    }
                    if (got)
                        break;
                }
                if (!got)
                    break;                          // Nobody here has it, or there is no money for it: it goes without.
                --left;
            }
        }
    }
}

std::vector<Society::Procurement> Society::takeProcurements()
{
    std::vector<Procurement> out;
    out.swap(procurements_);
    return out;
}

void Society::townBuyers(std::int64_t day, const std::map<std::string, LifeBody>& bodies)
{
    const auto& list = items::institutions();
    if (list.empty() || roster_ != Roster::Authored)
        return;
    (void)day;
    // Each community: how many live there, how many keep the watch, and its shops.
    struct Town
    {
        int residents = 0, guards = 0;
        std::vector<std::string> shops, poor;         // poor: no food and under 12p, for the church's alms.
        std::map<std::string, int> workers;           // By producer (a mine's, a fishery's... doc 42, Phase 5).
    };
    std::map<std::string, Town> towns;
    for (const auto& r : authored_.residents)
    {
        if (!state_.residents.count(r.id) || !bodies.count(r.id))
            continue;
        const auto community = communityOfResident(r.id);
        if (community.empty())
            continue;
        auto& t = towns[community];
        ++t.residents;
        t.guards += r.role == "guard";
        if (r.role == "merchant")
            t.shops.push_back(r.id);
        if (const auto* p = items::producerFor(r.workLabel))
            ++t.workers[p->id];
        if (const auto* purse = account(r.id); purse && purse->cash < 12 && bestFood(*purse).empty())
            t.poor.push_back(r.id);
    }
    const double days = items::institutionDays();
    for (auto& [community, town] : towns)
    {
        if (town.residents < 5)
            continue;                                   // (A hamlet's needs are its households'.)
        for (const auto& in : list)
        {
            const auto workers = in.perProducer.empty() ? 0 : town.workers.count(in.perProducer) ? town.workers.at(in.perProducer) : 0;
            double scale = !in.perProducer.empty() ? workers : in.perGuard ? town.guards : town.residents / 100.0;
            // The Town Works keeps the town's buildings (doc 42, Phase 5): they wear a point a day (two in winter); a
            // town fallen into disrepair needs more mending to catch up.
            const bool works = in.id == "works";
            double* condition = nullptr;
            if (works)
            {
                condition = &state_.memory.condition.try_emplace(community, 100.0).first->second;
                *condition = std::max(0.0, *condition - (season_ == 3 ? 2 : 1));
                scale *= 1 + (100 - *condition) / 100;
            }
            double needed = 0, used = 0;
            if (scale <= 0 || town.residents < in.minResidents)
                continue;
            const auto acct = "town:" + community + ":" + in.id;
            const auto treasuryId = treasuryOf(community);      // The town's own purse (doc 42).
            const bool founded = openAccount(acct);
            if (!account(acct) || !account(treasuryId))
                continue;
            // A day's funds from the treasury: what its basket costs, never more than a twentieth of the treasury.
            double cost = 0;
            for (const auto& [item, rate] : in.basket)
                if (const auto* good = items::good(item))
                    cost += rate * scale * good->price;
            // Funds from the treasury: enough to keep twice its days' worth in hand (to buy, and to put up a contract's
            // reward), never more than a twentieth of the treasury a day.
            const auto& treasury = *account(treasuryId);
            if (in.tithes)
            {
                // The church lives on its tithes; the town gives it four weeks of its basket when it is founded.
                if (founded)
                    shift(treasuryId, acct, "", 0, std::min<std::int64_t>(std::int64_t(std::ceil(cost * MonthDays)), treasury.cash / 10),
                          "church foundation");
            }
            else
            {
                // A town's budget (doc 42, 2026-10-05): its treasury first keeps PayrollDays of its wages (6p a day for each
                // of its watch and its civic posts, roughly), and funds its buyers only from what is above that, a
                // month's share a day. A poor town's buyers go short (its buildings wear, its watch eats plainly)
                // rather than its wages going unpaid.
                const auto reserve = std::int64_t(PayrollDays) * 6 * (town.guards + town.residents / 10);
                const auto spare = std::max<std::int64_t>(0, treasury.cash - reserve) / MonthDays;
                if (const auto grant = std::min<std::int64_t>(std::int64_t(std::ceil(cost * days * 2 * items::contractPremium())) - account(acct)->cash,
                                                              spare);
                    grant > 0)
                    shift(treasuryId, acct, "", 0, grant, "town funds");
            }
            for (const auto& [item, rate] : in.basket)
            {
                const double daily = rate * scale;
                // The day's use, from its stock (what it lacked waits, up to its days' worth).
                auto& owed = owed_[acct + "|" + item];
                owed = std::min(owed + daily, daily * days);
                needed += daily;
                const double owedBefore = owed;
                for (const auto& kind : kindsHeld(*account(acct), item))
                {
                    int n = std::min(int(owed), stock(*account(acct), kind));
                    // The church's bread goes to the hungry poor first, given (doc 42, Phase 5); the rest is eaten at
                    // its table.
                    if (in.tithes && edible(kind))
                        for (auto& who : town.poor)
                            if (n > 0 && !who.empty() && shift(acct, who, kind, 1, 0, "alms"))
                            {
                                --n;
                                owed -= 1;
                                who.clear();          // (One each.)
                            }
                    if (n > 0)
                        owed -= consume(acct, kind, n, "used by " + in.name);
                }
                used += std::max(0.0, owedBefore - owed);
                // Stock back up to its days' worth, from the town's shops.
                const int target = int(std::ceil(daily * days));
                int want = target - stockAll(*account(acct), item);
                for (const auto& shop : town.shops)
                {
                    if (want <= 0)
                        break;
                    const auto sold = wares(shop);
                    if (std::find(sold.begin(), sold.end(), item) == sold.end())
                        continue;
                    const auto till = tillOf(shop);
                    for (const auto& kind : kindsHeld(*account(till), item))
                    {
                        const auto* good = items::good(kind);
                        const std::int64_t price = std::max(1, good ? good->price : 1);
                        const int n = int(std::min<std::int64_t>({want, stock(*account(till), kind), 99, account(acct)->cash / price}));
                        if (n > 0 && transfer(till, acct, kind, n, price, "bought by " + in.name))
                            want -= n;
                        if (want <= 0)
                            break;
                    }
                }
                // What the town couldn't supply, it asks for: a contract for goods (the world posts it).
                if (want >= std::max(2, target / 3))
                    if (const auto* good = items::good(item))
                        procurements_.push_back({acct, community, in.name, item, want, std::max(1, good->price)});
            }
            if (condition && needed > 0)
            {
                // Mended by what was used: a full day's materials mends two points (and a backlog more).
                const double was = *condition;
                *condition = std::min(100.0, *condition + 2 * used / needed * (1 + (100 - was) / 100));
                if ((was >= 50) != (*condition >= 50))
                    townNews_.push_back({community, *condition >= 50 ? "mended" : "disrepair"});
                ++state_.memory.revision;
            }
        }
    }
}

void Society::tradeUpkeep(std::int64_t day, const std::map<std::string, LifeBody>& bodies)
{
    if (roster_ != Roster::Authored)
        return;
    // The shops of each community, and who sells what.
    std::map<std::string, std::vector<std::string>> shopsOf;
    for (const auto& r : authored_.residents)
        if (r.role == "merchant" && state_.residents.count(r.id))
            shopsOf[communityOfResident(r.id)].push_back(r.id);
    const auto buy = [&](const std::string& who, const std::string& item, const std::string& kind) {
        const auto shops = shopsOf.find(communityOfResident(who));
        if (shops == shopsOf.end())
            return false;
        for (const auto& shop : shops->second)
        {
            const auto sold = wares(shop);
            if (shop == who || std::find(sold.begin(), sold.end(), item) == sold.end())
                continue;
            const auto till = tillOf(shop);
            for (const auto& sort : kindsHeld(*account(till), item))
            {
                const auto* good = items::good(sort);
                if (good && transfer(till, who, sort, 1, std::max(1, good->price), kind))
                {
                    consume(who, sort, 1, "worn out at work");
                    return true;
                }
            }
        }
        return false;
    };
    // Tools a trade wears out (crafts.json `tools`): every so often, out of the worker's own purse.
    const auto& tools = items::toolNeeds();
    for (const auto& r : authored_.residents)
    {
        if (tools.empty())
            break;
        const auto body = bodies.find(r.id);
        const auto* p = items::producerFor(r.workLabel);
        if (!p || body == bodies.end() || body->second.age < 16 || !state_.residents.count(r.id))
            continue;
        for (const auto& t : tools)
        {
            if (t.producer != p->id)
                continue;
            const double offset = unit(r.id + "|" + t.item) * t.everyDays;          // (Not everyone on the same day.)
            const double at = double(day) + offset;
            if (std::floor(at / t.everyDays) != std::floor((at - 1) / t.everyDays))
                buy(r.id, t.item, "tools for the work");
        }
    }
    // Businesses' upkeep (crafts.json `upkeep`): a stables' horses eat, from the till, bought like its materials.
    for (const auto& r : authored_.residents)
    {
        if (r.role != "merchant" || !state_.residents.count(r.id))
            continue;
        const auto* business = items::businessFor(r.workLabel);
        const auto* upkeep = business ? items::upkeepFor(business->id) : nullptr;
        const auto* job = jobOf(r.id);
        if (!upkeep || !job)
            continue;
        const auto till = tillOf(r.id);
        for (const auto& [item, rate] : *upkeep)
        {
            auto& owed = owed_[till + "|upkeep|" + item];
            owed += rate;
            if (const int want = int(owed) - stockAll(*account(till), item); want > 0)
                buyMaterials(r.id, job->work.cell, item, want + int(std::ceil(rate * 3)));
            for (const auto& sort : kindsHeld(*account(till), item))
                if (int(owed) > 0)
                    owed -= consume(till, sort, std::min(int(owed), stock(*account(till), sort)), "eaten by the horses");
            owed = std::min(owed, rate * 3);       // (What couldn't be had waits a few days, no more.)
        }
    }
    // Beggars (doc 42, Phase 5): a few of their town's better-off spare them a penny each day.
    std::map<std::string, std::vector<std::string>> wellOff;
    for (const auto& r : authored_.residents)
        if (const auto* purse = account(r.id); purse && purse->cash > 100 && state_.residents.count(r.id))
            wellOff[communityOfResident(r.id)].push_back(r.id);
    for (const auto& r : authored_.residents)
    {
        if (r.workLabel.find("beg") == std::string::npos || !state_.residents.count(r.id))
            continue;
        const auto& givers = wellOff[communityOfResident(r.id)];
        for (std::size_t k = 0; k < std::min<std::size_t>(3, givers.size()); ++k)
        {
            const auto& giver = givers[std::size_t(unit(r.id + std::to_string(day) + std::to_string(k)) * double(givers.size())) % givers.size()];
            if (giver != r.id)
                shift(giver, r.id, "", 0, 1, "a penny for a beggar");
        }
    }
}
} // namespace ratw
