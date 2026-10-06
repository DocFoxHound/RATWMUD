// Watches the economy of a streamed world export left to its residents alone (no players), for as many game days as
// asked (Docs/Design/42-money-in-circulation.md: "a month-long run"). The world runs as world_check runs it (20 ticks a
// second, simulation tiers on), and every coin and good that moves is tallied from the society's journal.
//
//   econ_watch EXPORT_DIR DAYS OUT_DIR [--from HOUR] [--deterministic] [--threads N]
//
// OUT_DIR gets, for each game day:
//   days.jsonl      money by holder, every collector's and till's purse, residents' purses (median, poorest and richest
//                   tenth, Gini, shares), hunger, contracts, each town's residents (held, median, short, broke, hungry,
//                   starving), and whether money is conserved;
//   flows.csv       day, kind, from-holder, to-holder: entries, coins, goods;
//   wages.csv       day, resident, position title, wages received;
//   materials.csv   day, good: maker-hours short of it, supplier-hours short of it, the stock held world-wide, brought
//                   in, crafted, used in crafting, bought, carted in;
//   events.csv      day, kind, detail: everything the world logged that wasn't a plain ledger entry;
//   food.csv        day, food: brought in, crafted, used in crafting, eaten, spoiled, bought by residents (counts);
//   food_towns.csv  day, town, food: brought in, crafted, used, eaten, spoiled, bought by residents and what they paid
//                   (a good made or used up where its holder is; a sale where its seller is);
//   prices.csv      day, good, kind of sale: units and coins (the average price paid, against the catalog's);
//   larders.csv     day, town: households, people, their days of food on hand (larder and pockets, at 50 a wolf a
//                   day: mean, and how many households hold under 1, 1 to 2, 2 to 5, over 5), and the food (in
//                   nourishment) on its shops' shelves and with its producers;
//   food_moves.csv  day, kind, from-holder, to-holder, food: units (every movement of food, by why and between whom);
//   town_flows.csv  day, town, kind, from-holder, to-holder: coins that move between two of one town's accounts;
//   trade.csv       day, from-town, to-town, kind: entries, coins, goods, for coins that change towns (a resident's town
//                   is its home's; a shop's, where its keeper works; the shared church, the capital's treasury, the great
//                   houses and the roads count as places of their own).
// A line a day goes to stdout, so a long run can be followed. --deterministic does the world's work by counts, never the
// clock (World::setDeterministic): the run repeats exactly, and ends with a digest of where everyone is and what they do.
// --threads N: how many threads the world's work is spread over (8 unless asked: four cores and their twins; 1, none but
// the main one). The same run gives the same digest on any number.
#include "RatwItems.h"
#include "RatwPool.h"
#include "RatwWorld.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

using namespace ratw;
namespace fs = std::filesystem;

namespace
{
std::string slurp(const fs::path& path)
{
    std::ifstream file(path, std::ios::binary);
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
}
std::string csv(const std::string& s)
{
    std::string out = "\"";
    for (const char c : s)
        out += c == '"' ? std::string("\"\"") : std::string(1, c);
    return out + "\"";
}
std::string json(const std::string& s)
{
    std::string out = "\"";
    for (const char c : s)
        if (c == '"' || c == '\\')
            out += std::string("\\") + c;
        else if (static_cast<unsigned char>(c) >= 0x20)
            out += c;
    return out + "\"";
}

// What kind of holder an account is: a resident by its work, or a facility by its prefix.
std::string holder(const World& server, const std::string& id)
{
    const auto& society = server.society();
    if (id == "treasury")
        return "capital treasury";
    if (id == "outside")
        return "outside";
    if (id.rfind("stores:", 0) == 0)
        return "town treasury";
    if (id.rfind("town:", 0) == 0)
        return "town " + id.substr(id.rfind(':') + 1);
    if (id.rfind("home:", 0) == 0)
        return "home stores";
    if (id.rfind("till:", 0) == 0)
        return "house till";
    if (id.rfind("house:", 0) == 0)
        return "great house";
    if (playerAccountId(id))
        return "player";
    if (facilityAccount(id))
        return id.substr(0, id.find(':'));
    if (society.spec(id))
    {
        const auto* job = society.jobOf(id);
        const auto* e = server.entity(id);
        // (A producer, bringing goods in from the land, apart from the rest of the paid: doc 42, "Pressure".)
        const auto* r = society.spec(id);
        return job ? (job->role == "merchant"                       ? "merchant"
                      : job->role == "guard"                        ? "guard"
                      : r && items::producerFor(r->workLabel)       ? "producer"
                      : job->paid                                   ? "paid civilian"
                                                                    : "unpaid civilian")
             : society.apprenticedTo(id) ? "apprentice"
             : e && e->age < 16          ? "child"
                                         : "adult without work";
    }
    return id.empty() ? "nobody" : "other";
}

struct Flow
{
    std::int64_t entries = 0, coins = 0, goods = 0;
};
struct Material
{
    std::int64_t makerShort = 0, supplierShort = 0, held = 0, broughtIn = 0, crafted = 0, used = 0, bought = 0, carted = 0;
};
} // namespace

int main(int argc, char** argv)
{
    if (argc < 4)
    {
        std::cerr << "usage: econ_watch EXPORT_DIR DAYS OUT_DIR [--from HOUR] [--deterministic] [--threads N]\n";
        return 2;
    }
    const fs::path root = argv[1];
    const double days = std::atof(argv[2]);
    const fs::path out = argv[3];
    double from = 6;
    bool deterministic = false;
    unsigned threads = 8;
    for (int i = 4; i < argc; ++i)
        if (std::string(argv[i]) == "--from" && i + 1 < argc)
            from = std::atof(argv[++i]);
        else if (std::string(argv[i]) == "--deterministic")
            deterministic = true;
        else if (std::string(argv[i]) == "--threads" && i + 1 < argc)
            threads = unsigned(std::max(1, std::atoi(argv[++i])));
    fs::create_directories(out);

    std::map<std::string, std::string> files;
    for (const auto& entry : fs::recursive_directory_iterator(root))
        if (entry.is_regular_file())
            files[fs::relative(entry.path(), root).generic_string()] = slurp(entry.path());
    auto find = [&](const std::string& path) -> const std::string* {
        const auto it = files.find(path);
        return it == files.end() ? nullptr : &it->second;
    };
    World server;
    Pool pool(threads - 1);                         // (The main thread takes a share too.)
    if (threads > 1)
        server.setParallel([&](std::size_t count, const std::function<void(std::size_t)>& job) { pool.run(count, job); });
    server.setCellSource({
        [&](const std::string& id, std::string& header) {
            const auto* cell = find("cells/" + id + ".cell");
            if (!cell)
                return std::string("no cell file for ") + id;
            header = cell->substr(0, cell->find("grid:") + 5);
            return std::string();
        },
        [&](const std::string& id, std::string& cell, std::string& seams) {
            const auto* text = find("cells/" + id + ".cell");
            if (!text)
                return std::string("no cell file for ") + id;
            cell = *text;
            const auto* sides = find("seams/" + id);
            seams = sides ? *sides : std::string();
            return std::string();
        }});
    std::map<std::string, std::string> manifest;
    for (const auto& [path, text] : files)
        if (path.rfind("cells/", 0) != 0 && path.rfind("seams/", 0) != 0)
            manifest[path] = text;
    if (const auto loaded = server.loadWorldFiles(manifest, root.string()); !loaded.ok)
    {
        std::cerr << "World failed to load: " << loaded.message << '\n';
        return 1;
    }
    server.setTimeOfDay(from);
    server.setDeterministic(deterministic);
    const auto& society = server.society();

    std::ofstream daysOut(out / "days.jsonl"), flowsOut(out / "flows.csv"), wagesOut(out / "wages.csv"),
        materialsOut(out / "materials.csv"), eventsOut(out / "events.csv"), tradeOut(out / "trade.csv"),
        foodOut(out / "food.csv"), foodTownsOut(out / "food_towns.csv"), pricesOut(out / "prices.csv"),
        lardersOut(out / "larders.csv"), townFlowsOut(out / "town_flows.csv"),
        foodMovesOut(out / "food_moves.csv");
    flowsOut << "day,kind,from,to,entries,coins,goods\n";
    wagesOut << "day,resident,title,coins\n";
    materialsOut << "day,item,maker_hours_short,supplier_hours_short,held,brought_in,crafted,used,bought,carted_in\n";
    eventsOut << "day,kind,actor,target,coins,detail\n";
    tradeOut << "day,from,to,kind,entries,coins,goods\n";
    foodOut << "day,item,brought_in,crafted,used,eaten,spoiled,bought\n";
    struct Food
    {
        std::int64_t in = 0, crafted = 0, used = 0, eaten = 0, spoiled = 0, bought = 0;
    };
    std::map<int, std::map<std::string, Food>> foods;
    foodTownsOut << "day,town,item,brought_in,crafted,used,eaten,spoiled,bought,paid\n";
    pricesOut << "day,item,kind,units,coins,catalog\n";
    lardersOut << "day,town,households,people,mean_days,under_1,d1_2,d2_5,over_5,shop_food,producer_food\n";
    townFlowsOut << "day,town,kind,from,to,entries,coins\n";
    foodMovesOut << "day,kind,from,to,item,units\n";
    std::map<int, std::map<std::tuple<std::string, std::string, std::string, std::string>, std::int64_t>> foodMoves;
    struct TownFood
    {
        std::int64_t in = 0, crafted = 0, used = 0, eaten = 0, spoiled = 0, bought = 0, paid = 0;
    };
    std::map<int, std::map<std::pair<std::string, std::string>, TownFood>> townFoods;
    std::map<int, std::map<std::pair<std::string, std::string>, std::pair<std::int64_t, std::int64_t>>> prices;
    std::map<int, std::map<std::tuple<std::string, std::string, std::string, std::string>, Flow>> townFlows;

    // Where each account is (refreshed each day's end): a resident's town, a till's, a town's own purses'.
    std::map<std::string, std::string> places;
    const auto refreshPlaces = [&] {
        places.clear();
        for (const auto& [id, life] : society.state().residents)
            places[id] = server.communityOf(life.homeCell);
        for (const auto& r : society.authored().residents)
            if (r.role == "merchant" && society.state().residents.count(r.id))
                places[society.tillOf(r.id)] = server.communityOf(r.work.cell);
    };
    const auto placeOf = [&](const std::string& id) -> std::string {
        if (const auto found = places.find(id); found != places.end())
            return found->second.empty() ? "(country)" : found->second;
        if (id == Society::SharedChurch)
            return "(church)";
        if (id == "treasury")
            return "(capital)";
        if (id.rfind("stores:", 0) == 0)
            return id.substr(7);
        if (id.rfind("town:", 0) == 0)
            return id.substr(5, id.find(':', 5) - 5);
        if (id.rfind("house:", 0) == 0)
            return "(houses)";
        if (id.rfind("home:", 0) == 0 && id.rfind(':') > 5)
        {
            const auto town = server.communityOf(id.substr(5, id.rfind(':') - 5));
            return town.empty() ? "(country)" : town;
        }
        if (id.rfind("caravan", 0) == 0 || id.rfind("contract", 0) == 0)
            return "(road)";
        return {};
    };
    refreshPlaces();
    std::map<int, std::map<std::tuple<std::string, std::string, std::string>, Flow>> trade;
    {
        // roster.csv: who works at what, where (the producer or business their work makes them).
        std::ofstream roster(out / "roster.csv");
        roster << "resident,town,work_town,title,producer,business\n";
        for (const auto& r : society.authored().residents)
        {
            if (!society.state().residents.count(r.id))
                continue;
            const auto* job = society.jobOf(r.id);
            const auto* p = items::producerFor(r.workLabel);
            const auto* b = r.role == "merchant" ? items::businessFor(r.workLabel) : nullptr;
            roster << csv(r.id) << ',' << csv(placeOf(r.id)) << ',' << csv(server.communityOf(r.work.cell)) << ','
                   << csv(job ? job->title : std::string()) << ',' << csv(p ? p->id : std::string()) << ','
                   << csv(b ? b->id : std::string()) << '\n';
        }
    }

    // The day's tallies.
    // By the day each entry was made (the world stamps it), so the midnight's decisions (a month's reckoning) fall on
    // the new day.
    std::map<int, std::map<std::tuple<std::string, std::string, std::string>, Flow>> flows;
    std::map<int, std::map<std::string, std::int64_t>> wages;      // Resident -> wages that day.
    std::map<int, std::map<std::string, Material>> materials;
    int today = 0;
    std::int64_t hoursSampled = 0;

    // The makers and suppliers, and what each makes from and sells (fixed for the run).
    struct Maker
    {
        std::string id;
        std::vector<const items::Craft*> crafts;
        std::vector<std::string> supplies;
    };
    std::vector<Maker> makers;
    for (const auto& r : society.authored().residents)
        if (r.role == "merchant")
            if (const auto* business = items::businessFor(r.workLabel))
            {
                Maker m{r.id, items::craftsFor(business->id), items::suppliesFor(business->id)};
                if (!m.crafts.empty() || !m.supplies.empty())
                    makers.push_back(std::move(m));
            }
    std::set<std::string> ingredients;
    for (const auto& m : makers)
    {
        for (const auto* k : m.crafts)
            for (const auto& [item, n] : k->in)
                ingredients.insert(item);
        for (const auto& s : m.supplies)
            ingredients.insert(s);
    }

    // An hour's look at the workshops: who wants to make something and lacks what it takes, and which suppliers' shelves
    // are bare, as RatwCrafting.cpp judges them.
    const auto sampleMaterials = [&] {
        ++hoursSampled;
        for (const auto& m : makers)
        {
            const auto* till = society.account(society.tillOf(m.id));
            const auto* e = server.entity(m.id);
            if (!till || !e || e->dead)
                continue;
            for (const auto* k : m.crafts)
            {
                const auto& made = k->out.front().first;
                const bool supplied = std::find(m.supplies.begin(), m.supplies.end(), made) != m.supplies.end();
                const bool low = Society::stockAll(*till, made) < (supplied || items::traded(made) ? Society::SuppliesKept / 2 : Society::GoodsKept);
                if (!low)
                    continue;
                for (const auto& [item, n] : k->in)
                    if (Society::stockAll(*till, item) < n)
                        ++materials[today][item].makerShort;
            }
            for (const auto& s : m.supplies)
                if (Society::stockAll(*till, s) < Society::SuppliesKept / 4)
                    ++materials[today][s].supplierShort;
        }
    };

    const auto absorb = [&] {
        for (auto& e : server.takeEvents())
        {
            const int d = int(std::floor(e.day));
            if (e.kind == "economy")
            {
                auto& f = flows[d][{e.detail, holder(server, e.actor), holder(server, e.target)}];
                ++f.entries;
                f.coins += e.coins;
                f.goods += e.quantity;
                if (e.coins > 0)
                    if (const auto a = placeOf(e.actor), b = placeOf(e.target); !a.empty() && !b.empty() && a != b)
                    {
                        auto& t = trade[d][{a, b, e.detail}];
                        ++t.entries;
                        t.coins += e.coins;
                        t.goods += e.quantity;
                    }
                if (e.detail.find("wage") != std::string::npos && e.coins > 0 && society.spec(e.target))
                    wages[d][e.target] += e.coins;
                if (!e.item.empty() && e.coins > 0 && e.quantity > 0)
                {
                    auto& pr = prices[d][{items::baseOf(e.item), e.detail}];
                    pr.first += e.quantity;
                    pr.second += e.coins;
                }
                if (e.coins > 0)
                    if (const auto a = placeOf(e.actor); !a.empty() && a == placeOf(e.target))
                    {
                        auto& t = townFlows[d][{a, e.detail, holder(server, e.actor), holder(server, e.target)}];
                        ++t.entries;
                        t.coins += e.coins;
                    }
                if (!e.item.empty() && Society::edible(e.item))
                    foodMoves[d][{e.detail, holder(server, e.actor), holder(server, e.target), items::baseOf(e.item)}] += e.quantity;
                if (!e.item.empty() && Society::edible(e.item))
                {
                    // Where: the holder made or used up, else the seller (the target of a sale).
                    const bool made = e.actor == "made" || e.actor == "outside";
                    auto town = placeOf(made ? e.target : e.target == "consumed" ? e.actor : e.target);
                    if (town.empty())
                        town = placeOf(e.actor);
                    auto& tf = townFoods[d][{town.empty() ? "(none)" : town, items::baseOf(e.item)}];
                    const auto& k = e.detail;
                    if (k == "brought in")
                        tf.in += e.quantity;
                    else if (k == "crafted")
                        tf.crafted += e.quantity;
                    else if (k == "used in crafting")
                        tf.used += e.quantity;
                    else if (k == "eat")
                        tf.eaten += e.quantity;
                    else if (k == "spoiled")
                        tf.spoiled += e.quantity;
                    else if (k == "resident food purchase")
                        tf.bought += e.quantity, tf.paid += e.coins;
                    auto& f = foods[d][items::baseOf(e.item)];
                    if (k == "brought in")
                        f.in += e.quantity;
                    else if (k == "crafted")
                        f.crafted += e.quantity;
                    else if (k == "used in crafting")
                        f.used += e.quantity;
                    else if (k == "eat" || k == "eaten")
                        f.eaten += e.quantity;
                    else if (k == "spoiled")
                        f.spoiled += e.quantity;
                    else if (k == "resident food purchase")
                        f.bought += e.quantity;
                }
                if (!e.item.empty())
                {
                    const auto base = items::baseOf(e.item);
                    auto& m = materials[d][base];
                    if (e.detail == "brought in")
                        m.broughtIn += e.quantity;
                    else if (e.detail == "crafted")
                        m.crafted += e.quantity;
                    else if (e.detail == "used in crafting")
                        m.used += e.quantity;
                    else if (e.detail == "materials bought")
                        m.bought += e.quantity;
                    else if (e.detail == "materials carted in")
                        m.carted += e.quantity;
                }
            }
            else if (e.kind != "movement" && e.kind != "conversation" && e.kind != "speech")
                eventsOut << d << ',' << csv(e.kind) << ',' << csv(e.actor) << ',' << csv(e.target) << ',' << e.coins << ','
                          << csv(e.detail) << '\n';
        }
    };

    std::map<std::string, std::int64_t> startCash;   // Each account's cash at the start, for the end's comparison.
    for (const auto& [id, a] : society.state().accounts)
        startCash[id] = a.cash;

    // Each town's larders: how many days of food its households hold (larder and pockets, at 50 a wolf a day), and the
    // food on its shops' shelves and with its producers (in nourishment).
    const auto writeLarders = [&](int day) {
        const auto nourishIn = [](const EconomyAccount& a) {
            std::int64_t n = 0;
            for (const auto& [item, q] : a.stock)
                if (const auto* g = q > 0 ? items::good(item) : nullptr; g && !g->drink && g->nourish > 0)
                    n += std::int64_t(q) * g->nourish;
            return n;
        };
        struct Larders
        {
            int households = 0, people = 0, under1 = 0, d12 = 0, d25 = 0, over5 = 0;
            double days = 0;
            std::int64_t shop = 0, producer = 0;
        };
        std::map<std::string, Larders> towns;
        std::map<std::string, std::vector<std::string>> homes;
        for (const auto& [id, life] : society.state().residents)
            if (const auto* e = server.entity(id); e && !e->dead && !life.homeCell.empty())
                homes[life.homeCell].push_back(id);
        for (const auto& [home, members] : homes)
        {
            std::int64_t food = 0;
            if (const auto* larder = society.account(Society::homeStore(home, "larder")))
                food += nourishIn(*larder);
            for (const auto& id : members)
            {
                const auto* r = society.spec(id);
                if (const auto* a = society.account(id); a && !(r && r->role == "merchant" && society.tillOf(id) == id))
                    food += nourishIn(*a);
            }
            const auto town = server.communityOf(home);
            auto& t = towns[town.empty() ? "(country)" : town];
            const double days = double(food) / (50. * double(members.size()));
            ++t.households;
            t.people += int(members.size());
            t.days += days;
            (days < 1 ? t.under1 : days < 2 ? t.d12 : days <= 5 ? t.d25 : t.over5) += 1;
        }
        for (const auto& r : society.authored().residents)
        {
            if (!society.state().residents.count(r.id))
                continue;
            const auto* e = server.entity(r.id);
            if (!e || e->dead)
                continue;
            if (r.role == "merchant")
            {
                if (const auto* till = society.account(society.tillOf(r.id)))
                    towns[placeOf(society.tillOf(r.id)).empty() ? "(country)" : placeOf(society.tillOf(r.id))].shop += nourishIn(*till);
            }
            else if (items::producerFor(r.workLabel))
                if (const auto* a = society.account(r.id))
                    towns[placeOf(r.id).empty() ? "(country)" : placeOf(r.id)].producer += nourishIn(*a);
        }
        for (const auto& [town, t] : towns)
            lardersOut << day << ',' << csv(town) << ',' << t.households << ',' << t.people << ','
                       << (t.households ? t.days / t.households : 0) << ',' << t.under1 << ',' << t.d12 << ',' << t.d25 << ','
                       << t.over5 << ',' << t.shop << ',' << t.producer << '\n';
        lardersOut.flush();
    };

    const auto snapshot = [&](int day, const char* when) {
        if (std::string(when) == "day end")
            writeLarders(day);
        // Money by kind of holder, and every collector's and till's purse.
        std::map<std::string, std::pair<std::int64_t, std::int64_t>> byHolder;   // total, how many
        std::map<std::string, std::int64_t> purses;
        std::vector<std::pair<std::int64_t, std::string>> all;
        std::vector<std::int64_t> people;
        int short_ = 0, broke = 0, starving = 0, hungry = 0, grownShort = 0;
        std::int64_t residentTotal = 0;
        for (const auto& [id, a] : society.state().accounts)
        {
            const auto kind = holder(server, id);
            auto& h = byHolder[kind];
            h.first += a.cash;
            ++h.second;
            all.push_back({a.cash, id});
            if (id == "treasury" || id.rfind("stores:", 0) == 0 || id.rfind("town:", 0) == 0 || id.rfind("house:", 0) == 0 ||
                id.rfind("till:", 0) == 0)
                purses[id] = a.cash;
        }
        for (const auto& [id, life] : society.state().residents)
        {
            const auto* e = server.entity(id);
            if (!e || e->dead)
                continue;
            const auto* a = society.account(id);
            const std::int64_t cash = a ? a->cash : 0;
            people.push_back(cash);
            residentTotal += cash;
            short_ += cash < 6;
            grownShort += cash < 6 && e->age >= 16;     // (Children spend their stipends freely; the larder feeds them.)
            broke += cash <= 0;
            hungry += life.hunger >= 70;
            starving += life.hunger >= 90;
        }
        // Merchants' own purses are their shops' tills unless a house owns the business; count those too.
        for (const auto& m : makers)
            if (society.tillOf(m.id) == m.id)
                if (const auto* a = society.account(m.id))
                    purses["shop:" + m.id] = a->cash;
        std::sort(people.begin(), people.end());
        std::sort(all.rbegin(), all.rend());
        const auto n = people.size();
        double gini = 0;
        if (n && residentTotal > 0)
        {
            double weighted = 0;
            for (std::size_t i = 0; i < n; ++i)
                weighted += double(i + 1) * double(people[i]);
            gini = 2 * weighted / (double(n) * double(residentTotal)) - double(n + 1) / double(n);
        }
        const auto sumRange = [&](std::size_t a, std::size_t b) {
            std::int64_t s = 0;
            for (std::size_t i = a; i < b && i < n; ++i)
                s += people[i];
            return s;
        };
        const auto supply = society.moneySupply();
        std::int64_t contractsOpen = 0, contractsTaken = 0, contractsDone = 0, contractsExpired = 0;
        for (const auto& k : server.roads().contracts)
            (k.status == "open" ? contractsOpen : k.status == "taken" ? contractsTaken : k.status == "done" ? contractsDone : contractsExpired) += 1;
        std::size_t caravans = 0;
        for (const auto& c : server.roads().caravans)
            caravans += c.status == "travelling" || c.status == "returning";

        daysOut << "{\"day\":" << day << ",\"when\":" << json(when) << ",\"calendar\":" << server.calendarDays()
                << ",\"supply\":" << supply << ",\"conserved\":" << (society.conserved() ? "true" : "false")
                << ",\"minted\":" << society.state().minted << ",\"sunk\":" << society.state().sunk << ",\"residents\":" << n
                << ",\"resident_total\":" << residentTotal << ",\"median\":" << (n ? people[n / 2] : 0)
                << ",\"poorest_tenth\":" << (n ? sumRange(0, n / 10) / std::int64_t(std::max<std::size_t>(1, n / 10)) : 0)
                << ",\"richest_tenth\":" << (n ? sumRange(n - n / 10, n) / std::int64_t(std::max<std::size_t>(1, n / 10)) : 0)
                << ",\"richest_resident\":" << (n ? people.back() : 0) << ",\"gini\":" << gini
                << ",\"top_tenth_share\":" << (residentTotal ? double(sumRange(n - n / 10, n)) / double(residentTotal) : 0)
                << ",\"bottom_half_share\":" << (residentTotal ? double(sumRange(0, n / 2)) / double(residentTotal) : 0)
                << ",\"short\":" << short_ << ",\"grown_short\":" << grownShort << ",\"broke\":" << broke << ",\"hungry\":" << hungry << ",\"starving\":" << starving
                << ",\"contracts\":{\"open\":" << contractsOpen << ",\"taken\":" << contractsTaken << ",\"done\":" << contractsDone
                << ",\"expired\":" << contractsExpired << "},\"caravans\":" << caravans << ",\"holders\":{";
        bool first = true;
        for (const auto& [kind, h] : byHolder)
        {
            daysOut << (first ? "" : ",") << json(kind) << ":[" << h.first << ',' << h.second << ']';
            first = false;
        }
        daysOut << "},\"top_accounts\":[";
        for (std::size_t i = 0; i < std::min<std::size_t>(15, all.size()); ++i)
            daysOut << (i ? "," : "") << "[" << json(all[i].second) << ',' << all[i].first << ']';
        daysOut << "],\"purses\":{";
        first = true;
        for (const auto& [id, cash] : purses)
        {
            daysOut << (first ? "" : ",") << json(id) << ':' << cash;
            first = false;
        }
        // Who is hungry, and why: work, purse, food carried, what it is doing.
        daysOut << "},\"hungry_residents\":[";
        first = true;
        for (const auto& [id, life] : society.state().residents)
        {
            const auto* e = server.entity(id);
            if (!e || e->dead || life.hunger < 70)
                continue;
            const auto* a = society.account(id);
            const auto* job = society.jobOf(id);
            daysOut << (first ? "" : ",") << "{\"id\":" << json(id) << ",\"title\":" << json(job ? job->title : "(none)")
                    << ",\"age\":" << e->age << ",\"hunger\":" << life.hunger << ",\"cash\":" << (a ? a->cash : 0)
                    << ",\"food\":" << json(a ? Society::bestFood(*a) : "") << ",\"task\":" << json(life.task)
                    << ",\"reason\":" << json(life.reason) << ",\"cell\":" << json(e->cellId) << ",\"goal\":" << json(life.goalCell)
                    << ",\"home\":" << json(life.homeCell) << ",\"offstage\":" << (e->offstage ? "true" : "false") << "}";
            first = false;
        }
        // Each town's residents by home (Docs/Design/42, "Where money pools"): how many, what they hold, the median, and
        // how many are short (under 6p), broke, hungry and starving.
        struct Town
        {
            std::vector<std::int64_t> purses;
            int short_ = 0, broke = 0, hungry = 0, starving = 0;
            int foodShops = 0, food = 0;            // Shops holding anything to eat, and how many meals' worth they hold.
            std::int64_t producers = 0, keepers = 0, town = 0;   // Held by its producers, its shops' tills, the town.
        };
        std::map<std::string, Town> towns;
        for (const auto& [id, life] : society.state().residents)
        {
            const auto* e = server.entity(id);
            if (!e || e->dead)
                continue;
            const auto* a = society.account(id);
            const std::int64_t cash = a ? a->cash : 0;
            auto& t = towns[server.communityOf(life.homeCell)];
            t.purses.push_back(cash);
            t.short_ += cash < 6;
            t.broke += cash <= 0;
            t.hungry += life.hunger >= 70;
            t.starving += life.hunger >= 90;
            if (const auto* r = society.spec(id); r && items::producerFor(r->workLabel))
                t.producers += cash;
        }
        refreshPlaces();
        for (const auto& [id, a] : society.state().accounts)
        {
            const bool till = id.rfind("till:", 0) == 0 || (society.spec(id) && society.tillOf(id) == id &&
                                                            society.jobOf(id) && society.jobOf(id)->role == "merchant");
            const bool own = id.rfind("stores:", 0) == 0 || (id.rfind("town:", 0) == 0 && id != Society::SharedChurch);
            if ((till || own) && places.count(id) + own > 0)
            {
                const auto where = own ? placeOf(id) : places.at(id);
                (till ? towns[where].keepers : towns[where].town) += a.cash;
            }
        }
        // And each town's shops' food (by where each keeper works).
        for (const auto& r : society.authored().residents)
            if (r.role == "merchant" && society.state().residents.count(r.id))
                if (const auto* till = society.account(society.tillOf(r.id)))
                {
                    int meals = 0;
                    for (const auto& [item, count] : till->stock)
                        if (count > 0 && Society::edible(item))
                            meals += count;
                    auto& t = towns[server.communityOf(r.work.cell)];
                    t.food += meals;
                    t.foodShops += meals > 0;
                }
        daysOut << "],\"towns\":{";
        first = true;
        for (auto& [community, t] : towns)
        {
            std::sort(t.purses.begin(), t.purses.end());
            std::int64_t held = 0;
            for (const auto c : t.purses)
                held += c;
            daysOut << (first ? "" : ",") << json(community.empty() ? "(none)" : community) << ":{\"residents\":" << t.purses.size()
                    << ",\"held\":" << held << ",\"median\":" << (t.purses.empty() ? 0 : t.purses[t.purses.size() / 2])
                    << ",\"food_shops\":" << t.foodShops << ",\"food\":" << t.food << ",\"short\":" << t.short_
                    << ",\"broke\":" << t.broke << ",\"hungry\":" << t.hungry << ",\"starving\":" << t.starving
                    << ",\"producers_held\":" << t.producers << ",\"keepers_held\":" << t.keepers << ",\"town_held\":" << t.town << "}";
            first = false;
        }
        daysOut << "},\"condition\":{";
        first = true;
        for (const auto& [community, c] : society.state().memory.condition)
        {
            daysOut << (first ? "" : ",") << json(community) << ':' << c;
            first = false;
        }
        daysOut << "}}\n";
        daysOut.flush();
        return std::make_tuple(supply, society.conserved(), n ? people[n / 2] : 0, gini, grownShort, broke, starving);
    };

    const auto writeDay = [&](int day) {
        for (const auto& [key, f] : flows[day])
            flowsOut << day << ',' << csv(std::get<0>(key)) << ',' << csv(std::get<1>(key)) << ',' << csv(std::get<2>(key)) << ','
                     << f.entries << ',' << f.coins << ',' << f.goods << '\n';
        for (const auto& [key, f] : trade[day])
            tradeOut << day << ',' << csv(std::get<0>(key)) << ',' << csv(std::get<1>(key)) << ',' << csv(std::get<2>(key)) << ','
                     << f.entries << ',' << f.coins << ',' << f.goods << '\n';
        tradeOut.flush();
        trade.erase(trade.begin(), trade.upper_bound(day));
        for (const auto& [item, f] : foods[day])
            foodOut << day << ',' << csv(item) << ',' << f.in << ',' << f.crafted << ',' << f.used << ',' << f.eaten << ','
                    << f.spoiled << ',' << f.bought << '\n';
        foodOut.flush();
        foods.erase(foods.begin(), foods.upper_bound(day));
        for (const auto& [key, f] : townFoods[day])
            foodTownsOut << day << ',' << csv(key.first) << ',' << csv(key.second) << ',' << f.in << ',' << f.crafted << ','
                         << f.used << ',' << f.eaten << ',' << f.spoiled << ',' << f.bought << ',' << f.paid << '\n';
        for (const auto& [key, p] : prices[day])
        {
            const auto* good = items::good(key.first);
            pricesOut << day << ',' << csv(key.first) << ',' << csv(key.second) << ',' << p.first << ',' << p.second << ','
                      << (good ? good->price : 0) << '\n';
        }
        for (const auto& [key, f] : townFlows[day])
            townFlowsOut << day << ',' << csv(std::get<0>(key)) << ',' << csv(std::get<1>(key)) << ',' << csv(std::get<2>(key)) << ','
                         << csv(std::get<3>(key)) << ',' << f.entries << ',' << f.coins << '\n';
        for (const auto& [key, q] : foodMoves[day])
            foodMovesOut << day << ',' << csv(std::get<0>(key)) << ',' << csv(std::get<1>(key)) << ',' << csv(std::get<2>(key)) << ','
                         << csv(std::get<3>(key)) << ',' << q << '\n';
        foodMovesOut.flush();
        foodMoves.erase(foodMoves.begin(), foodMoves.upper_bound(day));
        foodTownsOut.flush();
        pricesOut.flush();
        townFlowsOut.flush();
        townFoods.erase(townFoods.begin(), townFoods.upper_bound(day));
        prices.erase(prices.begin(), prices.upper_bound(day));
        townFlows.erase(townFlows.begin(), townFlows.upper_bound(day));
        for (const auto& [id, coins] : wages[day])
        {
            const auto* job = society.jobOf(id);
            wagesOut << day << ',' << csv(id) << ',' << csv(job ? job->title : std::string("(none)")) << ',' << coins << '\n';
        }
        // What is held of each ingredient world-wide, at the day's end.
        auto& mats = materials[day];
        std::map<std::string, std::int64_t> held;
        for (const auto& [id, a] : society.state().accounts)
            for (const auto& [item, q] : a.stock)
                if (ingredients.count(items::baseOf(item)))
                    held[items::baseOf(item)] += q;
        for (const auto& item : ingredients)
            mats[item].held = held[item];
        for (const auto& [item, m] : mats)
            if (ingredients.count(item) || m.crafted || m.broughtIn)
                materialsOut << day << ',' << csv(item) << ',' << m.makerShort << ',' << m.supplierShort << ',' << m.held << ','
                             << m.broughtIn << ',' << m.crafted << ',' << m.used << ',' << m.bought << ',' << m.carted << '\n';
        flowsOut.flush();
        wagesOut.flush();
        materialsOut.flush();
        eventsOut.flush();
        std::int64_t wageTotal = 0;
        for (const auto& [id, c] : wages[day])
            wageTotal += c;
        std::int64_t makerShort = 0;
        for (const auto& [item, m] : mats)
            makerShort += m.makerShort;
        flows.erase(flows.begin(), flows.upper_bound(day));
        wages.erase(wages.begin(), wages.upper_bound(day));
        materials.erase(materials.begin(), materials.upper_bound(day));
        return std::make_pair(wageTotal, makerShort);
    };

    snapshot(int(std::floor(server.calendarDays())), "start");
    std::cout << "residents " << society.state().residents.size() << ", makers and suppliers " << makers.size()
              << ", ingredients " << ingredients.size() << ", from calendar day " << server.calendarDays() << "\n"
              << std::flush;

    const auto wallBegin = std::chrono::steady_clock::now();
    const long ticks = long(days * 24 * 600 / 0.05);
    int day = int(std::floor(server.calendarDays()));
    today = day;
    double nextHour = std::floor(server.calendarDays() * 24 + 1) / 24;
    // RATW_TRACE=id[,id...]: each traced wolf's place, task and activity, whenever one of them changes.
    std::vector<std::string> traced;
    if (const char* t = std::getenv("RATW_TRACE"))
        for (std::stringstream list(t); list.good();)
        {
            std::string id;
            std::getline(list, id, ',');
            if (!id.empty())
                traced.push_back(id);
        }
    std::map<std::string, std::string> lastTrace;
    for (long i = 0; i < ticks; ++i)
    {
        server.tick(0.05);
        absorb();
        for (const auto& id : traced)
            if (const auto* e = server.entity(id))
            {
                const auto* life = society.resident(id);
                std::ostringstream now;
                now << e->cellId << " | " << (life ? life->task + " -> " + life->goalCell : std::string()) << " | " << e->activity
                    << (e->offstage ? " (offstage)" : "");
                for (const auto& k : server.roads().contracts)
                    if (k.taker == id && k.status == "taken")
                        if (const auto* job = society.jobOf(k.source))
                            now << " [contract " << k.id << ": from " << k.source << " at " << (job->role == "merchant" ? job->serve.cell : job->work.cell)
                                << ", carried " << k.carried << "]";
                if (lastTrace[id] != now.str())
                {
                    lastTrace[id] = now.str();
                    std::cerr << "TRACE day " << std::fixed << std::setprecision(4) << server.calendarDays() << std::defaultfloat << " " << id
                              << " @" << int(e->position.x) << "," << int(e->position.y) << ": " << now.str() << "\n";
                }
            }
        if (server.calendarDays() >= nextHour)
        {
            nextHour += 1. / 24;
            sampleMaterials();
        }
        if (const int now = int(std::floor(server.calendarDays())); now != day)
        {
            today = now;
            const auto [wageTotal, makerShort] = writeDay(day);
            const auto [supply, conserved, median, gini, short_, broke, starving] = snapshot(now, "day end");
            const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - wallBegin).count();
            std::cout << "day " << now << " (" << int(wall / 60) << " min): supply " << supply << (conserved ? "" : " NOT CONSERVED")
                      << ", median purse " << median << "p, gini " << std::fixed << std::setprecision(3) << gini
                      << std::defaultfloat << ", grown short " << short_ << ", broke " << broke << ", starving " << starving
                      << ", wages " << wageTotal << "p, maker-hours short " << makerShort << "\n"
                      << std::flush;
            day = now;
        }
    }
    absorb();
    writeDay(day);
    snapshot(int(std::floor(server.calendarDays())), "end");
    // As world_check's: where everyone ended up and what they were doing, and the money.
    std::uint64_t digest = 1469598103934665603ULL;
    const auto mix = [&](const void* data, std::size_t size) {
        for (std::size_t b = 0; b < size; ++b)
            digest = (digest ^ static_cast<const unsigned char*>(data)[b]) * 1099511628211ULL;
    };
    const auto mixText = [&](const std::string& text) { mix(text.data(), text.size() + 1); };
    for (const auto& [id, e] : server.entities())
    {
        mixText(id);
        mixText(e.cellId);
        mix(&e.position.x, sizeof e.position.x);
        mix(&e.position.y, sizeof e.position.y);
        mixText(e.activity);
        if (const auto* a = society.account(id))
            mix(&a->cash, sizeof a->cash);
    }
    const double wall = std::chrono::duration<double>(std::chrono::steady_clock::now() - wallBegin).count();
    std::cout << "done: " << ticks << " ticks, " << wall / 60 << " min (" << wall * 1000 / double(ticks) << " ms a tick); digest "
              << std::hex << digest << std::dec << "\n";
    // Where the ticks went (ms a tick): the world's parts, and the schedules by stage.
    const auto& p = server.tickProfile();
    const auto per = [&](double ms) { return ms / double(std::max<std::int64_t>(1, ticks)); };
    std::cout << std::fixed << std::setprecision(4) << "ms a tick: streaming " << per(p.streaming.total) << ", schedules "
              << per(p.schedules.total) << ", movement " << per(p.movement.total) << ", separation " << per(p.separation.total)
              << ", views " << per(p.views.total) << "\nschedules: society " << per(p.stages[0]) << ", bonds " << per(p.stages[1])
              << ", roads " << per(p.stages[2]) << ", crime " << per(p.stages[3]) << ", errands " << per(p.stages[4])
              << ", streaming's wants " << per(p.stages[5]) << ", route planning " << per(p.stages[6]) << "\n"
              << std::defaultfloat;
    return 0;
}
