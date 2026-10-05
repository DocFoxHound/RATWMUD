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
    const std::string* best = nullptr;
    int most = 0;
    for (const auto& [item, n] : account.stock)
        if (n > 0)
            if (const auto* good = items::good(item); good && !good->drink && good->nourish > most)
            {
                most = good->nourish;
                best = &item;
            }
    return best ? *best : std::string();
}

bool Society::hasFood(const EconomyAccount& account)
{
    for (const auto& [item, n] : account.stock)
        if (n > 0)
            if (const auto* good = items::good(item); good && !good->drink && good->nourish > 0)
                return true;
    return false;
}

bool Society::shopHasFood(const std::string& merchant) const
{
    const auto* a = account(tillOf(merchant));       // (A house's business: its till, doc 42.)
    if (!a)
        return false;
    // (Its wares as wares() keeps them, not a copy: asked of every open shop every second.)
    std::vector<std::string> made;
    const std::vector<std::string>* sold = nullptr;
    if (const auto* r = roster_ == Roster::Demo ? nullptr : spec(merchant))
        if (const auto kept = waresCache_.find(merchant); kept != waresCache_.end() && kept->second.first == r->workLabel)
            sold = &kept->second.second;
    if (!sold)
        made = wares(merchant), sold = &made;
    for (const auto& [item, n] : a->stock)
        if (n > 0 && edible(item) && std::find(sold->begin(), sold->end(), items::baseOf(item)) != sold->end())
            return true;
    return false;
}

std::int64_t Society::shopPrice(const std::string& shop, const std::string& item) const
{
    const auto* good = items::good(item);
    if (!good)
        return 1;
    const auto base = items::baseOf(item);
    double factor = priceFactor(shop, base);
    if (const auto* a = account(tillOf(shop)))
    {
        const double kept = items::traded(base) ? SuppliesKept / 2 : GoodsKept;
        const double r = double(stockAll(*a, base)) / kept;
        factor *= r <= .5 ? 1.2 : r >= 3 ? .8 : 1.2 - .4 * (r - .5) / 2.5;
        // A shop flush with money sells cheaper, the more so the more it holds: its surplus back to its customers.
        if (const auto* job = jobOf(shop))
        {
            const auto flush = double(a->cash) / double(4 * floatOf(job->id) + 200);
            if (flush > 1)
                factor *= std::max(.7, 1 - .1 * std::min(3.0, flush));
        }
    }
    return std::max<std::int64_t>(1, std::int64_t(std::ceil(good->price * factor)));
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
        const auto asked = shopPrice(seller, item);   // (Supply and demand at this shop, the town's scarcity.)
        const int price = int(std::max<std::int64_t>(1, stall ? std::int64_t(std::floor(double(asked) * .9)) : asked));
        const double taste = .6 + .8 * unit(resident + "|" + items::baseOf(item));
        choices.push_back({item, price, good->nourish, double(good->nourish) / price * taste});
    }
    std::sort(choices.begin(), choices.end(), [](const Choice& a, const Choice& b) { return a.score > b.score; });
    // Enough for now (a meal's worth), and, where there is a larder to keep it in, the household's habit of days more
    // (stockingDays: 5 to 8) for everyone at home, by a grown wolf (the user, 2026-10-05: the children eat from the
    // larder, not their own pennies), as far as the purse goes. A child stocks only a couple of days for itself.
    int household = 1, days = 2;
    if (const auto* me = state_.residents.count(resident) ? &state_.residents.at(resident) : nullptr; stocking && me && !me->homeCell.empty())
    {
        const auto* r = spec(resident);
        if (r && r->age >= 16)
        {
            household = 0;
            for (const auto& [id, life] : state_.residents)
                household += life.homeCell == me->homeCell;
            household = std::clamp(household, 1, 8);
            days = stockingDays(me->homeCell);
        }
    }
    int wanted = 50 + (stocking ? 50 * days * household : 0), bought = 0;
    for (const auto& c : choices)
    {
        if (wanted <= 0)
            break;
        const auto* purse = account(resident);
        // Laying in a store takes at most half of what the shop has left beyond today's meal (the user, 2026-10-05: the
        // next customer finds some too).
        const int have = stock(*account(till), c.item);
        const int meal = bought == 0 ? (50 + c.nourish - 1) / c.nourish : 0;
        const int n = int(std::min<std::int64_t>({(wanted + c.nourish - 1) / c.nourish, std::min(have, meal + (have - meal) / 2), 99,
                                                  purse ? purse->cash / c.price : 0}));
        if (n > 0 && transfer(till, resident, c.item, n, c.price, "resident food purchase"))
        {
            bought += n;
            wanted -= n * c.nourish;
        }
    }
    return bought;
}

void Society::childrenAndStipends(std::int64_t day, const std::map<std::string, LifeBody>& bodies)
{
    // Friend groups: each town's children, by age, in fours.
    {
        friendGroups_.clear();
        std::map<std::string, std::vector<std::pair<int, std::string>>> byTown;
        for (const auto& [id, life] : state_.residents)
            if (const auto body = bodies.find(id); body != bodies.end() && body->second.age < 16 && body->second.age >= 4)
                byTown[communityOfResident(id)].push_back({body->second.age, id});
        for (auto& [town, kids] : byTown)
        {
            std::sort(kids.begin(), kids.end());
            for (std::size_t k = 0; k < kids.size(); ++k)
                friendGroups_[kids[k].second] = town + "|friends|" + std::to_string(k / 4);
        }
    }
    // Each home's household: its grown members and its children (those here, alive).
    std::map<std::string, std::pair<std::vector<std::string>, std::vector<std::string>>> homes;
    for (const auto& [id, life] : state_.residents)
        if (const auto body = bodies.find(id); body != bodies.end() && !life.homeCell.empty())
            (body->second.age < 16 ? homes[life.homeCell].second : homes[life.homeCell].first).push_back(id);
    std::map<std::string, std::vector<std::string>> shops;
    for (const auto& r : authored_.residents)
        if (r.role == "merchant" && state_.residents.count(r.id))
            shops[communityOfResident(r.id)].push_back(r.id);
    const auto& books = state_.books;
    for (const auto& [home, members] : homes)
    {
        const auto& [grown, children] = members;
        if (grown.empty() || children.empty())
            continue;
        // The household's purse, and what it must keep: a week's food for all at home, the tax and tithe on the week's
        // profit so far, and its rent (households pay none yet).
        std::int64_t purse = 0, owed = 0;
        const std::string* richest = nullptr;
        for (const auto& who : grown)
        {
            const auto* a = account(who);
            if (!a)
                continue;
            purse += spendable(who);                // (A keeper's food money and its shop's till aside: spendable.)
            if (!richest || spendable(who) > spendable(*richest))
                richest = &who;
            if (const auto start = books.start.find(who); start != books.start.end())
            {
                const auto unearned = books.unearned.count(who) ? books.unearned.at(who) : 0;
                if (const auto profit = a->cash - start->second - unearned; profit >= std::max(TaxShare, TitheShare))
                    owed += profit / TaxShare + profit / TitheShare;
            }
        }
        // Only what it can safely afford (the user, 2026-10-05: never a given): two weeks' food kept back for all at
        // home, and nothing while the household has lately been poor.
        const std::int64_t rent = 0;
        if (const auto streak = state_.memory.comfort.find(home); streak != state_.memory.comfort.end() && streak->second < 0)
            continue;
        const auto spare = purse - FoodADay * 14 * std::int64_t(grown.size() + children.size()) - owed - rent;
        // The more to spare, the more each child is given: a penny a day for every two weeks' worth it could have, up to
        // MostStipend and a penny more for every hundred to spare for each child (a rich family's children have plenty).
        const auto kids = std::int64_t(children.size());
        const auto each = std::min<std::int64_t>(MostStipend + std::max<std::int64_t>(0, spare) / (100 * kids), spare / (14 * kids));
        if (each <= 0 || !richest)
            continue;
        for (const auto& child : children)
            if (spendable(*richest) > each)
                shift(*richest, child, "", 0, each, "a child's stipend");
    }
    // Children spend freely: up to half of what they have, on something cheap they like (a treat, eaten when hungry, or a
    // trinket, kept), at their town's shops.
    for (const auto& [home, members] : homes)
        for (const auto& child : members.second)
        {
            const auto* purse = account(child);
            if (!purse || purse->cash < 2)
                continue;
            std::map<std::string, int> got;
            buyForSurplus(child, shops[communityOfResident(child)], [](const std::string& item) {
                const auto* good = items::good(item);
                return good && good->price <= 3;
            }, purse->cash / 2, "a child's spending", &got);
            for (const auto& [item, n] : got)
                if (!edible(item))
                    consume(child, item, n, "kept by a child");
        }
}

void Society::wants(std::int64_t day, const std::map<std::string, LifeBody>& bodies)
{
    std::map<std::string, std::vector<std::string>> shops;
    for (const auto& r : authored_.residents)
        if (r.role == "merchant" && state_.residents.count(r.id))
            shops[communityOfResident(r.id)].push_back(r.id);
    const auto category = [](const std::string& item) {
        const auto* good = items::good(item);
        return good ? good->category : std::string();
    };
    // The kinds of thing a wolf may want, each with a taste of its own for it.
    struct Want
    {
        const char* name;
        std::function<bool(const std::string&)> is;
    };
    const std::vector<Want> kinds = {
        {"a treat", [](const std::string& i) { const auto* g = items::good(i); return edible(i) && g && g->price >= 3; }},
        {"a drink", [](const std::string& i) { const auto* g = items::good(i); return g && g->drink; }},
        {"jewellery", [&](const std::string& i) { return category(i) == "jewelry"; }},
        {"finery", [&](const std::string& i) { const auto c = category(i); return c == "hat" || c == "scarf" || c == "shawl" || c == "paw_wear"; }},
        {"soap and scent", [&](const std::string& i) { return category(i) == "care"; }},
        {"a pastime", [&](const std::string& i) { const auto c = category(i); return c == "luxury" || c == "instrument" || i == "broadsheet" || i == "pamphlet" || i == "printed_book"; }},
        {"something for the home", [&](const std::string& i) { return category(i) == "household"; }},
    };
    const auto& books = state_.books;
    for (const auto& [id, life] : state_.residents)
    {
        const auto body = bodies.find(id);
        const auto* r = spec(id);
        const auto* purse = account(id);
        // (A keeper whose shop is its own spends its surplus as a keeper: RatwSurplus.cpp.)
        if (body == bodies.end() || body->second.age < 16 || !purse || !r || (r->role == "merchant" && tillOf(id) == id))
            continue;
        std::int64_t owed = 0;
        if (const auto start = books.start.find(id); start != books.start.end())
        {
            const auto unearned = books.unearned.count(id) ? books.unearned.at(id) : 0;
            if (const auto profit = purse->cash - start->second - unearned; profit >= std::max(TaxShare, TitheShare))
                owed = profit / TaxShare + profit / TitheShare;
        }
        const auto budget = (purse->cash - FoodADay * 7 - owed) / 10;
        if (budget < 2)
            continue;
        // Today's fancy: the kind it likes best, as it feels today.
        const Want* fancy = nullptr;
        double best = -1;
        for (const auto& k : kinds)
            if (const double w = (.2 + 1.6 * unit(id + "|want|" + k.name)) * unit(id + "|" + std::to_string(day) + "|" + k.name); w > best)
                best = w, fancy = &k;
        std::map<std::string, int> got;
        buyForSurplus(id, shops[communityOfResident(id)], fancy->is, budget, std::string("a want: ") + fancy->name, &got);
        int finery = 0;
        for (const auto& [item, n] : account(id)->stock)
            if (const auto c = category(item); c == "jewelry" || c == "hat" || c == "scarf" || c == "shawl" || c == "paw_wear")
                finery += n;
        for (const auto& [item, n] : got)
        {
            const auto* good = items::good(item);
            const auto c = category(item);
            if (edible(item))
                continue;                           // Kept, to eat.
            if (good && good->drink)
                consume(id, item, n, "drunk");
            else if ((c == "jewelry" || c == "hat" || c == "scarf" || c == "shawl" || c == "paw_wear") && finery <= 6)
                continue;                           // Kept: a few pieces of finery.
            else
                consume(id, item, n, "used");
        }
    }
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
        // The larder (the user, 2026-10-05): kept stocked for everyone at home by its best-off grown wolf, whoever is
        // hungry. Under two days' food for the household, it lays in a trip's worth (the household's habit: 5 to 8 days),
        // so the children and the others who earn nothing eat even when the earners eat elsewhere.
        std::string shopper;
        std::int64_t richest = -1;
        for (const auto& id : members)
            if (const auto body = bodies.find(id); body != bodies.end() && body->second.age >= 16)
                if (const auto* a = account(id); a && spendable(id) > richest)
                    richest = spendable(id), shopper = id;   // (A keeper's food money stays its own.)
        const auto larder = homeStore(home, "larder");
        if (const auto* store = account(larder); store && !shopper.empty())
        {
            int held = 0;
            for (const auto& [item, n] : store->stock)
                if (edible(item))
                    held += n * nourishment(item);
            // (A household with someone keeping the house sends it to the shop: RatwResidents.cpp; unless the larder is
            // all but bare.)
            const bool kept = state_.memory.keeper.count(home) > 0;
            if (held < 50 * (kept ? 1 : 2) * int(members.size()))
                for (const auto& shop : shops->second)
                {
                    if (shop == shopper || !shopHasFood(shop) || buyFood(shopper, shop, true) == 0)
                        continue;
                    const auto keep = bestFood(*account(shopper));
                    std::vector<std::pair<std::string, int>> food;
                    for (const auto& [item, n] : account(shopper)->stock)
                        if (n > 0 && edible(item))
                            food.push_back({item, item == keep ? n - 1 : n});
                    for (const auto& [item, n] : food)
                        if (n > 0)
                            shift(shopper, larder, item, std::min(n, 99), 0, "put away in the larder");
                    break;
                }
        }
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
                            const std::int64_t price = shopPrice(shop, kind);
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
        if (const auto* purse = account(r.id); purse && purse->cash < 12 && !hasFood(*purse))
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
                // The church lives on its tithes. It is founded with its starting money (RatwFounding.cpp): four weeks
                // of its basket, or ChurchHead a resident if more.
                if (founded)
                    startingMoney(acct, std::max<std::int64_t>(std::int64_t(std::ceil(cost * MonthDays)), ChurchHead * town.residents));
            }
            else
            {
                // Founded with its working funds as starting money (RatwFounding.cpp), the treasury tops them up after.
                if (founded)
                    startingMoney(acct, std::int64_t(std::ceil(cost * days * 2 * items::contractPremium())));
                // A town's budget (doc 42, 2026-10-05): its treasury first keeps PayrollDays of its wages (2p a spell,
                // PaidSpells a day, for each of its watch and its civic posts, roughly), and funds its buyers only from
                // what is above that, a month's share a day. A poor town's buyers go short (its buildings wear, its
                // watch eats plainly) rather than its wages going unpaid.
                const auto reserve = std::int64_t(PayrollDays) * 2 * PaidSpells * (town.guards + town.residents / 10);
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
                // The watch's bread, the mines' and quarries': rations its own people take when hungry (RatwResidents.cpp),
                // not used up here (the user, 2026-10-05: the watch eats from its mess).
                if (!in.tithes && edible(item))
                    owed = 0;
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
