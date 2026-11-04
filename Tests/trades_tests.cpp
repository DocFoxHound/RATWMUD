// Trades and the follow-ups of Ridgemere 2.0's economy (Docs/Design/68-the-country-and-the-city-loop.md, Progress,
// "Trades: medicine, patrons, office heads, and the follow-ups"): the heads of offices at a keeper's wage (the Watch's
// commander too); the city's suppliers (a keeper's craft inputs from the city's makers first, till to till: the cooper's
// casks for the wine makers), with the rule off all from the store room; the trades' patrons (the treasury for the
// Watch, the Country for the Houses' retainers: the armoury's mending and new kit), never kept; the households'
// medicines; buying ahead before a Restday; the float rule's wages kept before profit; the carriage subsidy only to the
// carters' need; and the Country's winter stores of preserved fruit. Money is conserved in every one.
//
// The catalogue is the real Data/Items (RATW_SOURCE_DIR).
#include "RatwCalendar.h"
#include "RatwItems.h"
#include "RatwSociety.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>

using namespace ratw;
namespace fs = std::filesystem;
namespace
{
int checks = 0;
void expect(bool condition, const std::string& message)
{
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}

void catalogue()
{
    setenv("RATW_DATA_DIR", (fs::path(RATW_SOURCE_DIR) / "Data").string().c_str(), 1);
    std::string error;
    expect(items::load(&error), "the catalogue loads: " + error);
}

ResidentSpec person(const std::string& id, const std::string& role, const std::string& work, Spot job, const std::string& home,
                    int age = 30, std::int64_t purse = 60)
{
    ResidentSpec r;
    r.id = id;
    r.name = id;
    r.role = role;
    r.workLabel = work;
    r.home = {home, 2.5, 2.5};
    r.work = job;
    r.evening = r.home;
    r.serve = job;
    r.purse = purse;
    r.age = age;
    r.meals = 0;
    r.startHour = 7;
    r.endHour = 19;
    return r;
}

Society city(std::vector<ResidentSpec> residents, const std::function<void(AuthoredRoster&)>& change = {})
{
    AuthoredRoster r;
    r.residents = std::move(residents);
    r.economy.treasury = 10000;
    r.economy.storeRule = true;
    if (change)
        change(r);
    Society s(Roster::None);
    s.configure(r);
    LifeDay day;
    day.communityOf = [](const std::string&) { return std::string("city"); };
    s.setDay(day);
    CareerWorld world;
    world.alive = [](const std::string&) { return true; };
    world.age = [&s](const std::string& id) { return s.spec(id) ? s.spec(id)->age : 30; };
    world.regard = [](const std::string&, const std::string&) { return 50.; };
    s.tendCareers(1, world);
    return s;
}

std::map<std::string, LifeBody> atWork(const Society& s)
{
    std::map<std::string, LifeBody> bodies;
    for (const auto& r : s.authored().residents)
    {
        LifeBody b{r.work.cell, r.work.x, r.work.y};
        b.age = r.age;
        bodies[r.id] = b;
    }
    return bodies;
}

// Runs the society from `from` to `to` (game days), handing each day's journal to `seen`.
void run(Society& s, double from, double to, const std::function<void(const std::vector<EconomyEntry>&)>& seen = {})
{
    const auto bodies = atWork(s);
    double day = std::floor(from);
    for (double now = from; now < to; now += 1. / calendar::SecondsPerDay)
    {
        s.tick(1, now, 0, bodies);
        if (std::floor(now) != day)
        {
            day = std::floor(now);
            if (seen)
                seen(s.takeJournal());
        }
    }
    if (seen)
        seen(s.takeJournal());
}

void theOfficeHeads()
{
    // The user, 2026-10-31: "office heads get a head's (keeper's) pay, 14p".
    const std::vector<std::pair<std::string, std::string>> heads = {
        {"harbour", "harbourmaster of the Wharf Office"}, {"counting", "overseeing the Counting House"},
        {"rolls", "keeping the rolls at the Records Office"}, {"customs", "collecting the Crown's customs"},
        {"warden", "warden of the Market Hall"}, {"abbot", "abbot of the cathedral"}, {"chancellor", "chancellor of the Council"},
        {"chancellor2", "Chancellor of Ridgemere"}, {"magistrate", "hearing cases"}, {"secretary", "royal secretary"},
        {"treasurer", "the Crown's treasurer"}, {"king", "reigning at the Great Palace"}, {"washhouse", "keeping the Washhouse"},
        {"bathhouse", "keeping the Bathhouse"}};
    std::vector<ResidentSpec> folk;
    double x = 2.5;
    for (const auto& [id, label] : heads)
        folk.push_back(person(id, "civilian", label, {"office_" + id, x += 2, 2.5}, "home_" + id));
    folk.push_back(person("commander", "guard", "commanding the Watch", {"keep", 2.5, 2.5}, "home_c"));
    folk.push_back(person("sergeant", "guard", "sergeant of the Watch", {"keep", 4.5, 2.5}, "home_s"));
    folk.push_back(person("clerk", "civilian", "clerking at the Records Office", {"rolls", 2.5, 2.5}, "home_k"));
    folk.push_back(person("washer", "civilian", "washing at the Washhouse", {"wash", 2.5, 2.5}, "home_w"));
    folk.push_back(person("docker", "civilian", "clerking at the Wharf Office", {"wharf", 2.5, 2.5}, "home_d"));
    auto s = city(folk);
    const auto pay = [&](const std::string& id) { return s.payOf(*s.position("job:" + id)); };
    for (const auto& [id, label] : heads)
    {
        const bool country = id == "harbour" || id == "counting";
        expect(pay(id).wage == 14 && pay(id).payer == (country ? std::string(Society::CountryFund) : std::string("treasury")),
               label + ": a head's wage, 14p, from " + (country ? "the Country" : "the treasury") + "; it has " +
                   std::to_string(pay(id).wage) + "p from " + pay(id).payer);
    }
    expect(pay("commander").kind == "watch" && pay("commander").wage == 14, "the Watch's commander: the watch's post, a head's wage");
    expect(pay("sergeant").kind == "watch" && pay("sergeant").wage == 8, "the rest of the watch as before");
    expect(pay("clerk").wage == 8 && pay("washer").wage == 8 && pay("docker").wage == 8 && pay("docker").payer == Society::CountryFund,
           "the offices' clerks and hands as before");
}

// A brewery whose shelves lack only wine, with the grapes for a cask, and a cooper with barrels.
std::vector<ResidentSpec> wineAndCasks(std::int64_t cooperPurse = 100)
{
    return {person("brewer", "merchant", "brewer at The Hopyard", {"brewery", 2.5, 2.5}, "home_b", 40, 500),
            person("cooper", "merchant", "cooper at Staves & Hoops", {"cooperage", 2.5, 2.5}, "home_c", 40, cooperPurse)};
}

void theCitySuppliers()
{
    expect(Society(Roster::None).citySellers("press", "barrel") == std::vector<std::string>{"cooperage"} &&
               Society(Roster::None).citySellers("brewery", "barrel") == std::vector<std::string>{"cooperage"} &&
               Society(Roster::None).citySellers("brewery", "grapes").empty(),
           "the wine makers' casks come from the cooper (country.json's suppliers)");
    const auto* brewery = items::business("brewery");
    // Bought till to till, at the cooper's price.
    {
        auto s = city(wineAndCasks());
        const auto cooper = s.tillOf("cooper"), brewer = s.tillOf("brewer");
        if (const int start = Society::stock(*s.account(cooper), "barrel"); start > 0)
            s.consume(cooper, "barrel", start, "test: its starting casks gone");
        s.create(cooper, "barrel", 3, "test: casks on the shelf");
        const auto money = s.moneySupply();
        const auto c0 = s.cash(cooper), b0 = s.cash(brewer), country0 = s.cash(Society::CountryFund);
        const int had = Society::stock(*s.account(brewer), "barrel");
        const int got = s.buyFromCity(brewer, brewery, "barrel", 2);
        const auto price = Society::catalogPrice("barrel");
        expect(got == 2 && s.cash(cooper) - c0 == 2 * price && b0 - s.cash(brewer) == 2 * price &&
                   Society::stock(*s.account(brewer), "barrel") == had + 2 && Society::stock(*s.account(cooper), "barrel") == 1,
               "two casks from the cooper's till at its price: " + std::to_string(got) + ", cooper " + std::to_string(s.cash(cooper) - c0) +
                   "p, brewer " + std::to_string(b0 - s.cash(brewer)) + "p");
        expect(s.cash(Society::CountryFund) == country0 && s.conserved() && s.moneySupply() == money,
               "the Country takes nothing (inside the walls: no toll, no carriage); conserved");
        expect(s.loopToday().cityTrade == 2 * price && s.loopToday().cityTradePieces == 2, "counted");
        expect(s.buyFromCity(brewer, brewery, "barrel", 5) == 1, "only what the cooper holds");
        expect(s.buyFromCity(brewer, brewery, "barrel", 1) == 0, "none when the cooper has none (the store room's then)");
        expect(s.buyFromCity(brewer, brewery, "grapes", 1) == 0, "a good no city maker supplies: none");
    }
    // The rule off: nothing from the city.
    {
        auto s = city(wineAndCasks(), [](AuthoredRoster& r) { r.economy.rules["suppliers"] = 0; });
        expect(Society::stock(*s.account(s.tillOf("cooper")), "barrel") > 0, "the cooper holds casks");
        expect(s.buyFromCity(s.tillOf("brewer"), brewery, "barrel", 2) == 0, "suppliers off: none bought from the city");
    }
    // The restocking rule: wine made by the cask, the cask bought from the cooper first, the store room only when it has none.
    const auto restocked = [&](bool casksInTown) {
        auto s = city(wineAndCasks());
        const auto brewer = s.tillOf("brewer"), cooper = s.tillOf("cooper");
        for (const auto& ware : s.wares("brewer"))
            if (ware != "wine")
                s.create(brewer, ware, 40, "test: the shelves full");
        s.create(brewer, "grapes", 16, "test: a cask's grapes");
        for (const char* none : {"barrel", "wine"})
            if (const int n = Society::stock(*s.account(brewer), none); n > 0)
                s.consume(brewer, none, n, "test: none of its own");
        if (const int n = Society::stock(*s.account(cooper), "barrel"); !casksInTown && n > 0)
            s.consume(cooper, "barrel", n, "test: the cooper sold out");
        const auto money = s.moneySupply();
        for (int h = 0; h < 40; ++h)
            s.restockFromStore("brewer", 1 + h / 24.);
        expect(s.conserved() && s.moneySupply() == money, "conserved");
        std::map<std::string, int> from;
        for (const auto& e : s.takeJournal())
            if (e.item == "barrel" && e.kind == "from a city maker" && e.from == brewer && e.to == cooper)
                from["cooper"] += e.quantity;
            else if (e.item == "barrel" && e.kind == "drawn for wine" && e.from == "made" && e.to == brewer)
                from["store"] += e.quantity;
        expect(Society::stock(*s.account(brewer), "wine") >= 8, "a cask of wine made: " + std::to_string(Society::stock(*s.account(brewer), "wine")));
        return from;
    };
    auto town = restocked(true);
    expect(town["cooper"] == 1 && town["store"] == 0, "the cask from the cooper, none from the store room");
    auto none = restocked(false);
    expect(none["cooper"] == 0 && none["store"] == 1, "with no cask in the city, the store room's");
}

// Doc 72, Phase 4 (4.5): the craft inputs the Country's store sold come from the city's makers first. A tailor whose shelves
// lack only neckerchiefs (a linen cloth makes four) and a weaver.
void thePhase4Suppliers()
{
    const auto sellers = [](const char* buyer, const char* good) { return Society(Roster::None).citySellers(buyer, good); };
    using V = std::vector<std::string>;
    // (Phase 5: the country workshops beside them, Larkfield's linen weavers, its village smithy, the presses.)
    const V linen{"weaver", "country_linen_weavers"};
    expect(sellers("tailor", "linen_cloth") == linen && sellers("apothecary", "linen_cloth") == linen &&
               sellers("papermill", "linen_cloth") == linen && sellers("scriptorium", "linen_cloth") == linen,
           "linen cloth from the weaver");
    expect(sellers("carpenter", "iron_hoops") == V{"smithy", "cooperage", "country_smithy"} &&
               sellers("cartwright", "iron_hoops") == V{"smithy", "cooperage", "country_smithy"},
           "iron hoops from the smithies and the cooperage");
    // (After country5b, doc 72: the builders' works and the carpenters beside the sawmill; the ropewalk's cord beside the
    // chandlery's, the chandlery's own candles' cord from the ropewalk before the store room.)
    expect(sellers("cooperage", "planks") == V{"sawmill", "builders_works", "carpenter"} &&
               sellers("cartwright", "planks") == V{"sawmill", "builders_works", "carpenter"} &&
               sellers("carpenter", "planks") == V{"sawmill", "builders_works"},
           "planks from the Timberland Sawmill, the builders' works and the carpenters");
    expect(sellers("ropewalk", "cord") == V{"chandlery"} && sellers("jeweler", "cord") == V{"chandlery", "ropewalk"} &&
               sellers("chandlery", "cord") == V{"ropewalk"},
           "cord from the chandlery and the ropewalk (the chandlery's own candles' cord the ropewalk's)");
    expect(sellers("printing_house", "paper") == V{"papermill", "scriptorium", "country_press"} && sellers("scriptorium", "leather") == V{"tannery"} &&
               sellers("cartographer", "ink") == V{"scriptorium", "printing_house"},
           "paper, leather and ink from their makers");
    expect(sellers("general", "nails") == V{"country_smithy", "smithy", "tinker"} &&
               sellers("rag_and_bone", "cookpot") == V{"country_smithy", "smithy", "tinker"} &&
               sellers("barber", "bandages") == V{"apothecary", "herbalist"} && sellers("cooperage", "barrel").empty() &&
               sellers("brewery", "barrel") == V{"cooperage"},
           "the resale goods from their makers since Phase 5, the casks as they were");
    const auto made = [&](bool linenInTown) {
        auto s = city({person("tailor", "merchant", "tailor at The Needle", {"tailor", 2.5, 2.5}, "home_t", 40, 500),
                       person("weaver", "merchant", "weaver at The Loom", {"weaver", 2.5, 2.5}, "home_w", 40, 100)});
        const auto tailor = s.tillOf("tailor"), weaver = s.tillOf("weaver");
        for (const auto& ware : s.wares("tailor"))
            if (ware != "linen_neckerchief")
                s.create(tailor, ware, 40, "test: the shelves full");
        for (const char* none : {"linen_cloth", "linen_neckerchief"})
            if (const int n = Society::stock(*s.account(tailor), none); n > 0)
                s.consume(tailor, none, n, "test: none of its own");
        if (const int n = Society::stock(*s.account(weaver), "linen_cloth"); n > 0)
            s.consume(weaver, "linen_cloth", n, "test: the weaver's linen gone");
        if (linenInTown)
            s.create(weaver, "linen_cloth", 3, "test: linen on the weaver's shelf");
        const auto money = s.moneySupply();
        const auto country0 = s.cash(Society::CountryFund), treasury0 = s.cash("treasury"), carriage0 = s.cash(Society::CarriageFund);
        const auto weaver0 = s.cash(weaver), tailor0 = s.cash(tailor);
        for (int h = 0; h < 40; ++h)
            s.restockFromStore("tailor", 1 + h / 24.);
        expect(s.conserved() && s.moneySupply() == money, "conserved");
        std::map<std::string, int> from;
        for (const auto& e : s.takeJournal())
            if (e.item == "linen_cloth" && e.kind == "from a city maker" && e.from == tailor && e.to == weaver)
                from["weaver"] += e.quantity;
            else if (e.item == "linen_cloth" && e.kind == "drawn for linen_neckerchief" && e.from == "made" && e.to == tailor)
                from["store"] += e.quantity;
        expect(Society::stock(*s.account(tailor), "linen_neckerchief") > 0, "neckerchiefs made");
        if (linenInTown)
        {
            const auto paid = s.cash(weaver) - weaver0;
            expect(from["weaver"] > 0 && from["store"] == 0 && paid == from["weaver"] * Society::catalogPrice("linen_cloth") &&
                       tailor0 - s.cash(tailor) == paid,
                   "the linen from the weaver's till at its price, none from the store room: " + std::to_string(from["weaver"]) +
                       " pieces, " + std::to_string(paid) + "p");
            expect(s.cash(Society::CountryFund) == country0 && s.cash("treasury") == treasury0 && s.cash(Society::CarriageFund) == carriage0,
                   "till to till inside the walls: the Country takes nothing, no toll, no carriage");
        }
        else
            expect(from["weaver"] == 0 && from["store"] > 0 && s.cash(Society::CountryFund) > country0,
                   "the weaver out of linen: the store room's, the Country paid for it");
        return from;
    };
    made(true);
    made(false);
}

// An armourer and its hand, three of the Watch, and the Houses' retainers (a number): mending every day at 3p each, a new
// piece every day, so a day shows them.
void thePatrons()
{
    const auto day = [](const std::function<void(AuthoredRoster&)>& change) {
        auto s = city({person("armourer", "merchant", "armourer at The Rivet and Rain", {"armoury", 2.5, 2.5}, "home_a", 40, 100),
                       person("hand", "civilian", "helping at The Rivet and Rain", {"armoury", 4.5, 2.5}, "home_h"),
                       person("g1", "guard", "on patrol", {"gate", 2.5, 2.5}, "barracks_watch"),
                       person("g2", "guard", "walking the wall", {"gate", 4.5, 2.5}, "barracks_watch"),
                       person("g3", "guard", "holding the north gate", {"gate", 6.5, 2.5}, "barracks_watch")},
                      [&](AuthoredRoster& r) {
                          for (const char* id : {"armour_mended", "dent_beaten_out", "straps_replaced", "blade_ground"})
                              r.economy.services[id] = {true, 3, 1};
                          r.economy.rules["armourEvery"] = 1;
                          if (change)
                              change(r);
                      });
        s.create(s.tillOf("armourer"), "kettle_helm", 200, "test: plenty of helms");
        s.shift("treasury", Society::CountryFund, "", 0, 3000, "test: the estates' money");
        const auto money = s.moneySupply();
        std::vector<EconomyEntry> journal;
        run(s, 1 + 7.9 / 24, 1 + 17.5 / 24, [&](const std::vector<EconomyEntry>& j) { journal.insert(journal.end(), j.begin(), j.end()); });
        expect(s.conserved() && s.moneySupply() == money, "conserved");
        return std::pair<Society, std::vector<EconomyEntry>>{std::move(s), std::move(journal)};
    };
    auto [s, journal] = day({});
    const auto till = s.tillOf("armourer");
    std::map<std::string, std::int64_t> work, kit;
    std::int64_t kitPaid = 0;
    for (const auto& e : journal)
    {
        if (e.kind == "a patron's work")
        {
            expect(e.to == till && e.coins == 3, "the patrons' work: from the armoury, at the world's price");
            work[e.from] += 1;
        }
        if (e.kind == "a patron's new kit")
        {
            const auto& pieces = items::country().patrons.front().pieces;
            expect(e.to == till && std::find(pieces.begin(), pieces.end(), e.item) != pieces.end() && e.coins == Society::catalogPrice(e.item),
                   "new kit, one of the pieces, at its price: " + e.item);
            if (e.from == "treasury")
                kitPaid += e.coins;
            kit[e.from] += 1;
        }
    }
    const int houses = items::country().patrons.front().groups.back().units;
    expect(work["treasury"] == 3 * 4 && kit["treasury"] == 3, "the Watch: four kinds of mending and a piece for each of its three: " +
                                                                   std::to_string(work["treasury"]) + ", " + std::to_string(kit["treasury"]));
    expect(work[Society::CountryFund] > 0 && kit[Society::CountryFund] > 0 && work[Society::CountryFund] <= 4 * houses,
           "the Houses' retainers, paid by their estates (the Country), as far as it can spare: " + std::to_string(work[Society::CountryFund]));
    for (const auto& [id, a] : s.state().accounts)
        for (const char* v : {"armour_mended", "dent_beaten_out", "straps_replaced", "blade_ground"})
            expect(Society::stock(a, v) == 0, std::string("the work is never kept: ") + v + " in " + id);
    expect(s.loopToday().patronsTreasury == 3 * 4 * 3 + kitPaid && s.loopToday().patronJobs > 12,
           "counted: " + std::to_string(s.loopToday().patronsTreasury));
    // Never a household's: the households buy none of them.
    for (const auto& e : journal)
        expect(e.kind != "a service" || (e.item != "armour_mended" && e.item != "blade_ground"), "no household buys a patron's work");
    // Off.
    auto [off, offJournal] = day([](AuthoredRoster& r) { r.economy.rules["patrons"] = 0; });
    for (const auto& e : offJournal)
        expect(e.kind != "a patron's work" && e.kind != "a patron's new kit", "patrons off: none");
    // The treasury spends only what it can spare.
    auto [poor, poorJournal] = day([](AuthoredRoster& r) { r.economy.treasury = Society::TreasuryFloat; });
    for (const auto& e : poorJournal)
        expect(e.from != "treasury" || (e.kind != "a patron's work" && e.kind != "a patron's new kit"), "a treasury at its float buys none");
}

void theMedicines()
{
    // The households' medicines (doc 68, "Trades"), from the apothecary.
    std::set<std::string> needed;
    for (const auto& n : items::households().needs)
        for (const auto& g : n.any)
            needed.insert(g);
    for (const char* m : {"willow_tea", "thyme_syrup", "paw_balm", "flea_powder", "dewormer", "garlic_salve", "bandages"})
        expect(needed.count(m), std::string("a household need: ") + m);
    // The apothecary makes and keeps what players want: the simple remedies, salves, stitch kits, splints, smelling salts,
    // and theriac (one on the shelf, from the Country).
    auto s = city({person("apothecary", "merchant", "apothecary at Hollis Physic", {"shop", 2.5, 2.5}, "home_a", 40, 200)});
    const auto wares = s.wares("apothecary");
    for (const char* m : {"bandages", "willow_tea", "flea_powder", "yarrow_poultice", "paw_balm", "thyme_syrup", "comfrey_salve",
                          "garlic_salve", "dewormer", "stitch_kit", "splint", "smelling_salts", "theriac"})
        expect(std::find(wares.begin(), wares.end(), m) != wares.end(), std::string("the apothecary sells ") + m);
    expect(s.storeUse("apothecary", "theriac") == Society::StoreUse::Resell && Society::startingStock("theriac") == 1 &&
               Society::startingStock("bandages") == Society::GoodsKept,
           "theriac from the Country, one on the shelf");
    // A household with money buys each over its days (a need every 60-120 days: run 121).
    auto t = city({person("apothecary", "merchant", "apothecary at Hollis Physic", {"shop", 2.5, 2.5}, "home_a", 40, 200),
                   person("buyer", "civilian", "keeps the house", {"home_b", 2.5, 2.5}, "home_b", 40, 3000)});
    const auto till = t.tillOf("apothecary");
    for (const char* m : {"willow_tea", "thyme_syrup", "paw_balm", "flea_powder", "dewormer", "garlic_salve", "comfrey_salve"})
        t.create(till, m, 99, "test: the shelves full");
    t.create("buyer", "bread", 99, "test: fed");
    const auto money = t.moneySupply();
    std::map<std::string, int> bought;
    run(t, 1, 1 + 121, [&](const std::vector<EconomyEntry>& j) {
        for (const auto& e : j)
            if (e.kind == "market" && e.to == till && e.from == "buyer")
                bought[e.item] += e.quantity;
    });
    expect(t.conserved() && t.moneySupply() == money, "conserved");
    for (const char* m : {"willow_tea", "thyme_syrup", "paw_balm", "flea_powder", "dewormer"})
        expect(bought[m] >= 1, std::string("bought for the household: ") + m + " " + std::to_string(bought[m]));
    expect(bought["garlic_salve"] + bought["comfrey_salve"] >= 1, "a salve for the household");
    expect(bought["willow_tea"] <= 3 && bought["dewormer"] <= 2, "now and then, not daily: willow tea " + std::to_string(bought["willow_tea"]));
}

// The day before a Restday: a household's market hour buys a day more.
void buyingAhead()
{
    int before = 1;
    while (calendar::weekdayOf(before + 1.5) != calendar::Restday || calendar::festivalDay(before + 1.5))
        ++before;
    const auto fed = [&](int days, int when) {
        auto s = city({person("baker", "merchant", "baker at The Loaf", {"bakery", 2.5, 2.5}, "home_baker", 40, 100),
                       person("eater", "civilian", "keeps the house", {"home_e", 2.5, 2.5}, "home_e", 40, 500)},
                      [&](AuthoredRoster& r) { r.economy.rules["buyAheadDays"] = days; });
        s.create(s.tillOf("baker"), "bread", 900, "test: plenty of bread");
        const auto money = s.moneySupply();
        run(s, when + 7.9 / 24, when + 17.9 / 24);
        expect(s.conserved() && s.moneySupply() == money, "conserved");
        double n = 0;
        for (const auto& [item, count] : s.account("eater")->stock)
            n += double(Society::nourishment(item)) * count;
        return n / (.0035 * 14400 / 1.1);
    };
    const double ahead = fed(1, before), usual = fed(0, before), weekday = fed(1, before - 1);
    expect(ahead >= 2.9 && usual < 2.9 && usual >= 1.9, "a day more before the Restday: " + std::to_string(ahead) + " against " + std::to_string(usual));
    expect(weekday < 2.9, "on other days, as before: " + std::to_string(weekday));
}

// A rich barber's till pays its keeper the profit over its float and floatWagesDays of its wages.
void theFloatRule()
{
    const auto profit = [](int days) {
        auto s = city({person("barber", "merchant", "barber at The Close Trim", {"shop", 2.5, 2.5}, "home_barber", 40, 1000),
                       person("helper", "civilian", "helping at the barber's", {"shop", 4.5, 2.5}, "home_helper", 30, 30)},
                      [&](AuthoredRoster& r) {
                          r.economy.rules["services"] = 0;
                          r.economy.rules["floatWagesDays"] = days;
                      });
        const auto money = s.moneySupply();
        run(s, 1 + 17.9 / 24, 1 + 18.5 / 24);
        expect(s.conserved() && s.moneySupply() == money, "conserved");
        return s.loopToday().profit;
    };
    // 1,000p, less 22p of wages, kept back: its 20p float and the days' wages (it drew nothing: the float is 20p).
    expect(profit(7) == 1000 - 22 - (20 + 7 * 22), "a week of wages kept: " + std::to_string(profit(7)));
    expect(profit(3) == 1000 - 22 - (20 + 3 * 22), "three days' kept: " + std::to_string(profit(3)));
    expect(profit(0) == 1000 - 22 - 20, "none kept: all over its float");
}

// The carriage subsidy only while the carters lack their crew's wages.
void theSubsidyToNeed()
{
    const auto subsidy = [](std::int64_t carterPurse, int toNeed) {
        auto s = city({person("smith", "merchant", "the smith at the forge", {"forge", 2.5, 2.5}, "home_smith", 40, 5000),
                       person("carter", "merchant", "running the Carters' Yard", {"yard", 2.5, 2.5}, "home_carter", 40, carterPurse),
                       person("driver", "civilian", "driving for the Carters' Yard", {"yard", 4.5, 2.5}, "home_driver")},
                      [&](AuthoredRoster& r) { r.economy.rules["subsidyToNeed"] = toNeed; });
        const auto money = s.moneySupply();
        expect(s.drawFromStore("smith", "iron_bar", 400).ok, "the draw");
        expect(s.conserved() && s.moneySupply() == money, "conserved");
        return s.loopToday().subsidy;
    };
    // 2,000 weight at 6p a load: 12p of carriage, a quarter of it the treasury's.
    expect(subsidy(1000, 0) == 3, "as it was: a quarter of the carriage");
    expect(subsidy(1000, 1) == 0, "the carters' till holds its crew's wages and more: none");
    expect(subsidy(0, 1) == 3, "a till that lacks them: the subsidy, up to what it lacks");
}

// Winter: a preserver with no fresh fruit to be had draws its preserved fruit from the Country's stores.
void theWinterStores()
{
    auto s = city({person("preserver", "merchant", "preserver at The Honey Crock", {"shop", 2.5, 2.5}, "home_p", 40, 500)},
                  [](AuthoredRoster& r) { r.economy.seasons["fruit"] = {{0, 0, 0, 0}, {100, 100, 100, 100}, {100, 100, 100, 100}, 950}; });
    s.setYearDay(320);                              // (Winter's middle.)
    const auto till = s.tillOf("preserver");
    const auto money = s.moneySupply();
    for (int h = 0; h < 4; ++h)
        s.restockFromStore("preserver", 320 + h / 24.);
    expect(s.conserved() && s.moneySupply() == money, "conserved");
    expect(Society::stock(*s.account(till), "dried_apples") >= Society::GoodsKept &&
               Society::stock(*s.account(till), "fruit_preserves") >= Society::GoodsKept,
           "the shelves from the Country's stores: " + std::to_string(Society::stock(*s.account(till), "dried_apples")));
    expect(s.priceFactor("dried_apples") < 1, "and a tenth cheaper in winter");
    // With fruit to be had, the preserver makes them, drawing none.
    auto t = city({person("preserver", "merchant", "preserver at The Honey Crock", {"shop", 2.5, 2.5}, "home_p", 40, 500)});
    t.setYearDay(229);
    for (int h = 0; h < 4; ++h)
        t.restockFromStore("preserver", 229 + h / 24.);
    for (const auto& e : t.takeJournal())
        expect(!(e.kind == "restocked from the store room" && (e.item == "dried_apples" || e.item == "fruit_preserves")),
               "in autumn they are made, not drawn");
}
} // namespace

int main()
{
    try
    {
        catalogue();
        theOfficeHeads();
        theCitySuppliers();
        thePhase4Suppliers();
        thePatrons();
        theMedicines();
        buyingAhead();
        theFloatRule();
        theSubsidyToNeed();
        theWinterStores();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Trades tests passed: " << checks << " checks.\n";
    return 0;
}
