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
    const auto* a = account(merchant);
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
    const auto* shop = account(seller);
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
        const int have = stock(*account(seller), c.item);
        const int n = int(std::min<std::int64_t>({(wanted + c.nourish - 1) / c.nourish, have, 20, purse ? purse->cash / c.price : 0}));
        if (n > 0 && transfer(seller, resident, c.item, n, c.price, "resident food purchase"))
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
                    const auto* s = account(shop);
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
                            if (transfer(shop, buyer, kind, 1, price, "household purchase"))
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
        std::vector<std::string> shops;
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
    }
    const double days = items::institutionDays();
    for (const auto& [community, town] : towns)
    {
        if (town.residents < 5)
            continue;                                   // (A hamlet's needs are its households'.)
        for (const auto& in : list)
        {
            const double scale = in.perGuard ? town.guards : town.residents / 100.0;
            if (scale <= 0)
                continue;
            const auto acct = "town:" + community + ":" + in.id;
            openAccount(acct);
            if (!account(acct))
                continue;
            // A day's funds from the treasury: what its basket costs, never more than a twentieth of the treasury.
            double cost = 0;
            for (const auto& [item, rate] : in.basket)
                if (const auto* good = items::good(item))
                    cost += rate * scale * good->price;
            // Funds from the treasury: enough to keep twice its days' worth in hand (to buy, and to put up a contract's
            // reward), never more than a twentieth of the treasury a day.
            const auto& treasury = *account("treasury");
            const auto grant = std::min<std::int64_t>(std::int64_t(std::ceil(cost * days * 2 * items::contractPremium())) - account(acct)->cash,
                                                      treasury.cash / 20);
            if (grant > 0)
                shift("treasury", acct, "", 0, grant, "town funds");
            for (const auto& [item, rate] : in.basket)
            {
                const double daily = rate * scale;
                // The day's use, from its stock (what it lacked waits, up to its days' worth).
                auto& owed = owed_[acct + "|" + item];
                owed = std::min(owed + daily, daily * days);
                for (const auto& kind : kindsHeld(*account(acct), item))
                {
                    const int n = std::min(int(owed), stock(*account(acct), kind));
                    if (n > 0)
                        owed -= consume(acct, kind, n, "used by " + in.name);
                }
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
                    for (const auto& kind : kindsHeld(*account(shop), item))
                    {
                        const auto* good = items::good(kind);
                        const std::int64_t price = std::max(1, good ? good->price : 1);
                        const int n = int(std::min<std::int64_t>({want, stock(*account(shop), kind), 99, account(acct)->cash / price}));
                        if (n > 0 && transfer(shop, acct, kind, n, price, "bought by " + in.name))
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
        }
    }
}
} // namespace ratw
