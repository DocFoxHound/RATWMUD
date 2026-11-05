// Pulling the Country into the world, Phase 5 (Docs/Design/72-pulling-the-country-into-the-world.md, 4.6 and the
// Siting's Phase 5 rows): the country workshops, real businesses in the communities at the map's fixed building ids
// (Data/Economy/workshops.json), each a House's; the resale path's purchases from the city's makers and the workshops
// (Society::buyFromCity from `!w.craft`, at the store room's price); and cheese, a comic luxury for wolves (the user's
// decision): never food, every household's want, bought only from what is over its keep after its food and needs, at a
// price that climbs in its own band.
//
// A workshop: it crafts (fine soap and tallow candles of its tallow, herbs and cord), sells to the city's general store
// till to till at the store room's price (no toll, no carriage, nothing from the store room: the Country no longer
// sells the good), pays its crew from its till and sends what is over to its House's estate (the Country's purse,
// booked by House), money conserved; with the workshop out of soap, or the rule `suppliers` off, the store room
// supplies it again.
//
// The catalogue is the real Data (RATW_SOURCE_DIR).
#include "RatwCalendar.h"
#include "RatwItems.h"
#include "RatwJsonDoc.h"
#include "RatwSociety.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>

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

ResidentSpec person(const std::string& id, const std::string& role, const std::string& work, Spot job,
                    const std::string& home, int age = 30, std::int64_t purse = 60)
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
    // (And the Wharf's valve shut, doc 73: the Country's flows as these tests have them.)
    r.economy.rules = {{"services", 0}, {"patrons", 0}, {"surplus", 0}, {"wageDrift", 0}, {"treasuryDrift", 0}, {"wharfValve", 0}};
    if (change)
        change(r);
    Society s(Roster::None);
    s.configure(r);
    LifeDay day;
    day.communityOf = [](const std::string& cell) {
        return cell.rfind("cn_coldwater_", 0) == 0 ? std::string("coldwater") : std::string("city");
    };
    s.setDay(day);
    CareerWorld world;
    world.alive = [](const std::string&) { return true; };
    world.age = [&s](const std::string& id) { return s.spec(id) ? s.spec(id)->age : 30; };
    world.regard = [](const std::string&, const std::string&) { return 50.; };
    s.tendCareers(1, world);
    return s;
}

// The society in ten-second ticks, everyone at work, from day 1.
struct Town
{
    static constexpr double Step = 10;
    Society s;
    std::map<std::string, LifeBody> bodies;
    double now = 1;
    explicit Town(Society society) : s(std::move(society))
    {
        for (const auto& r : s.authored().residents)
        {
            LifeBody b{r.work.cell, r.work.x, r.work.y};
            b.age = r.age;
            bodies[r.id] = b;
        }
    }
    void until(double day)
    {
        for (; now < day; now += Step / calendar::SecondsPerDay)
            s.tick(Step, now, 0, bodies);
    }
};

void empty(Society& s, const std::string& till, const std::string& item)
{
    if (const int n = Society::stock(*s.account(till), item); n > 0)
        s.consume(till, item, n, "test: none of it");
}

// The nine workshops as data: each at its fixed building, merged into its community's market as its business, its
// keeper's label its business's, its hands' no business's, at most 40 characters, owned by its land's House.
void theWorkshopsData()
{
    const auto& people = items::farmPeople();
    const std::map<std::string, std::pair<std::string, std::string>> want = {
        {"cn_coldwater_soapworks", {"country_soapworks", "the Crown"}},
        {"cn_coldwater_weavers", {"country_wool_weavers", "the Crown"}},
        {"cn_larkfield_weavers", {"country_linen_weavers", "the Crown"}},
        {"cn_larkfield_press", {"country_press", "the Crown"}},
        {"cn_larkfield_smithy", {"country_smithy", "the Crown"}},
        {"cn_fenshore_pottery", {"country_pottery", "Greatly"}},
        {"cn_southmeadow_pottery", {"country_pottery", "Greatly"}},
        {"cn_oakhaven_press", {"country_press", "Timberland"}},
        {"cn_coldwater_still", {"country_still", "Fell"}}};    // (Since iron6, doc 72: "The producers and the fixes".)
    expect(people.workshops.size() == want.size(), "nine workshops: " + std::to_string(people.workshops.size()));
    int posts = 0;
    for (const auto& [area, what] : want)
    {
        const auto* w = people.workshop(area);
        expect(w && w->business == what.first && w->house == what.second,
               area + ": " + what.first + ", " + what.second + "'s");
        const auto* c = people.community(w->community);
        const auto m = c ? std::find_if(c->market.begin(), c->market.end(),
                                        [&](const items::MarketPlace& x) { return x.area == area; })
                         : std::vector<items::MarketPlace>::const_iterator{};
        expect(c && m != c->market.end() && m->business == w->business && m->name == w->name && !m->work.empty(),
               area + ": in " + w->community + "'s market");
        expect(!items::craftsFor(w->business).empty(), w->business + " makes something");
        for (const auto& [post, n] : w->posts)
        {
            posts += n;
            const auto rule = people.market.find(post);
            expect(rule != people.market.end(), area + ": a rule for " + post);
            auto label = rule->second.label;
            // (The striker's label names no workshop: "Larkfield Smithy" would make it the city smithy's.)
            if (const auto at = label.find("{name}"); at != std::string::npos)
                label.replace(at, 6, w->name);
            const auto* business = items::businessFor(label);
            expect(label.size() <= 40, "at most 40 characters: " + label);
            if (rule->second.role == "merchant")
                expect(business && business->id == w->business && n == 1, label + ": its keeper, of " + w->business);
            else
                expect(!business, label + ": no business's");
        }
    }
    expect(posts == 30, "30 posts (32 before doc 73's round 6, 18 before country5b's carry-overs, 35 before the still, 37 before its two hands of doc 73's round 3, 39 "
                        "before round 4's: Oakhaven's press, the soapworks and Coldwater's looms two hands fewer each, "
                        "Larkfield's looms one): " +
                           std::to_string(posts));
    // Where the map has laid a workshop (communities.json, a community's `workshops`), its work spots are the map's (by
    // its stations), never workshops.json's own.
    std::ifstream in(fs::path(RATW_SOURCE_DIR) / "Data" / "Economy" / "communities.json", std::ios::binary);
    std::stringstream text;
    text << in.rdbuf();
    json::Value places;
    std::string error;
    expect(json::parse(text.str(), places, error), "communities.json parses: " + error);
    for (const auto& x : places.array("communities"))
        for (const auto& laid : x.array("workshops"))
        {
            const auto area = laid.string("area", laid.string("id"));
            const auto* c = people.community(x.string("id"));
            const auto m = std::find_if(c->market.begin(), c->market.end(),
                                        [&](const items::MarketPlace& y) { return y.area == area; });
            if (!people.workshop(area) || m == c->market.end())
                continue;
            const auto& spots = laid.array("work");
            expect(m->work.size() == spots.size(), area + ": the map's work spots");
            for (std::size_t i = 0; i < spots.size(); ++i)
                expect(m->work[i].first == std::floor(spots[i].number("x", 0)) + .5 &&
                           m->work[i].second == std::floor(spots[i].number("y", 0)) + .5,
                       area + ": the map's work spot " + std::to_string(i));
        }
    // Their wares: what their crafts make.
    expect(items::business("country_soapworks") && Society(Roster::None).citySellers("general", "fine_soap") ==
                                                       std::vector<std::string>{"country_soapworks"},
           "the general store's fine soap from the soapworks");
}

const char* const Boiler = "soap boiler at Coldwater Soapworks";
const char* const Works = "cn_coldwater_soapworks";

std::vector<ResidentSpec> soapworks(std::int64_t purse = 300)
{
    return {
        person("boiler", "merchant", Boiler, {Works, 3.5, 3.5}, "cn_coldwater_cottage_1", 40, purse),
        person("hand", "civilian", "soap hand at Coldwater Soapworks", {Works, 5.5, 3.5}, "cn_coldwater_cottage_1", 25,
               20),
        person("grocer", "merchant", "shopkeeper at The Corner Stores", {"store", 2.5, 2.5}, "home_grocer", 40, 800)};
}

void aCommunityWorkshop()
{
    // Its wares and its crafts.
    {
        auto s = city(soapworks());
        const auto wares = s.wares("boiler");
        expect(std::find(wares.begin(), wares.end(), "fine_soap") != wares.end() &&
                   std::find(wares.begin(), wares.end(), "candle_tallow") != wares.end(),
               "the soapworks sells fine soap and tallow candles");
        const auto till = s.tillOf("boiler");
        expect(till.rfind("till:", 0) == 0 && s.account(till), "its own till");
        for (const char* g : {"fine_soap", "candle_tallow"})
            empty(s, till, g);
        s.create(till, "tallow", 30, "test: the farms' tallow");
        s.create(till, "herbs", 10, "test: herbs");
        s.create(till, "cord", 10, "test: cord");
        const auto money = s.moneySupply();
        for (int h = 0; h < 10; ++h)
            s.restockFromStore("boiler", 1 + (8 + h) / 24.);
        expect(Society::stock(*s.account(till), "fine_soap") >= 3 &&
                   Society::stock(*s.account(till), "candle_tallow") >= 2,
               "made fine soap and candles: " + std::to_string(Society::stock(*s.account(till), "fine_soap")) + ", " +
                   std::to_string(Society::stock(*s.account(till), "candle_tallow")));
        expect(s.conserved() && s.moneySupply() == money, "making: conserved");
    }
    // It sells to the general store, till to till at the store room's price; nothing of it from the store room.
    const auto sold = [](bool soapInStock, bool suppliersOn) {
        auto s = city(soapworks(), [&](AuthoredRoster& r) { r.economy.rules["suppliers"] = suppliersOn ? 1 : 0; });
        const auto works = s.tillOf("boiler"), store = s.tillOf("grocer");
        empty(s, works, "fine_soap");
        empty(s, store, "fine_soap");
        if (soapInStock)
            s.create(works, "fine_soap", 12, "test: soap on the soapworks' shelf");
        const auto money = s.moneySupply();
        const auto works0 = s.cash(works), store0 = s.cash(store), country0 = s.cash(Society::WharfFund),
                   treasury0 = s.cash("treasury");
        s.takeJournal();
        // (Only the soap wanted: every other ware of the store's on its shelf.)
        for (const auto& ware : s.wares("grocer"))
            if (ware != "fine_soap")
                s.create(store, ware, 40, "test: the shelves full");
        s.takeJournal();
        s.restockFromStore("grocer", 1 + 9 / 24.);
        int fromWorks = 0, fromStore = 0;
        std::int64_t paid = 0;
        for (const auto& e : s.takeJournal())
        {
            if (e.item == "fine_soap" && e.kind == "from a maker, to sell on" && e.from == store && e.to == works)
                fromWorks += e.quantity, paid += e.coins;
            else if (e.item.empty() && e.kind == "from a maker, to sell on: the rest of its price" && e.from == store &&
                     e.to == works)
                paid += e.coins;
            else if (e.item == "fine_soap" && e.kind == "restocked from the store room")
                fromStore += e.quantity;
        }
        expect(s.conserved() && s.moneySupply() == money, "selling: conserved");
        if (soapInStock && suppliersOn)
        {
            const auto price =
                std::int64_t(std::ceil(double(s.seasonalCost("fine_soap", fromWorks)) * 75 / 100. - 1e-9));
            expect(fromWorks > 0 && fromStore == 0,
                   "the store's soap from the soapworks, none from the store room: " + std::to_string(fromWorks));
            expect(paid == price && s.cash(works) - works0 == paid && store0 - s.cash(store) == paid,
                   "at the store room's price (75%): " + std::to_string(paid) + "p for " + std::to_string(fromWorks));
            expect(s.cash(Society::WharfFund) == country0 && s.cash("treasury") == treasury0,
                   "the Country sells none of it, and no toll (till to till)");
            expect(s.loopToday().cityResalePieces == fromWorks && s.loopToday().cityResale == paid &&
                       s.loopToday().cityResaleGoods.at("fine_soap") == fromWorks,
                   "counted as the resale trade");
        }
        else
            expect(fromWorks == 0 && fromStore > 0 && s.cash(Society::WharfFund) > country0,
                   std::string(suppliersOn ? "the soapworks out of soap" : "suppliers off") +
                       ": the store room's, the Country paid");
        return fromWorks;
    };
    sold(true, true);
    sold(false, true);
    sold(true, false);
    // Its crew paid from its till at 18:00, at the city's wages; what is over its float and a week of wages to its
    // House's estate (its own purse: doc 72, Phase 6), booked by House; nothing to its keeper as profit; conserved.
    {
        auto t = Town(city(soapworks(1000)));
        auto& s = t.s;
        const auto till = s.tillOf("boiler");
        t.until(1 + 17.9 / 24);
        s.takeJournal();
        const auto money = s.moneySupply();
        const auto country0 = s.cash(Society::WharfFund);
        t.until(1 + 18.5 / 24);
        std::map<std::string, std::int64_t> wages;
        std::int64_t toHouse = 0, profit = 0;
        for (const auto& e : s.takeJournal())
        {
            if (e.kind == "wages" && e.from == till)
                wages[e.to] += e.coins;
            else if (e.kind == "a workshop's takings to its House: the Crown" && e.from == till &&
                     e.to == "estate:crown")
                toHouse += e.coins;
            else if (e.kind == "profit" && e.from == till)
                profit += e.coins;
        }
        const auto& e = s.authored().economy;
        expect(wages["boiler"] == e.wageKeeper && wages["hand"] == e.wageHand,
               "the keeper's and the hand's wages from the till: " + std::to_string(wages["boiler"]) + ", " +
                   std::to_string(wages["hand"]));
        const auto& day = s.loopToday();
        const auto w = day.workshops.find(Works);
        expect(toHouse > 0 && profit == 0 && w != day.workshops.end() && w->second.toHouse == toHouse &&
                   w->second.paid == w->second.owed && day.rents.count("the Crown") &&
                   day.rents.at("the Crown") == toHouse,
               "what is over to the Crown's estate, booked by House, none to its keeper: " + std::to_string(toHouse));
        // (Three days of them, workshopWagesDays: doc 72, after country5, a week's held its House's share back for
        // nothing; after country5b, two left Larkfield's smithy, its float a day's iron bars, short on its Restdays;
        // after country6a, four: three still left it short on some, 98.0%.)
        const auto kept = s.rule("workshopWagesDays") * (e.wageKeeper + e.wageHand);
        expect(s.rule("workshopWagesDays") == 4 && s.rule("floatWagesDays") == 7 && s.cash(till) >= kept &&
                   s.cash(till) <= kept + 100,
               "the till keeps its float and four days' wages (a city till a week's): " + std::to_string(s.cash(till)));
        (void)country0;
        expect(s.conserved() && s.moneySupply() == money, "wages and the House's share: conserved");
    }
    // Its wages drift at most workshopWageMost over the world's (doc 72, after country6a: the drift, judged on its House's
    // share, raised the crews' wages ~130p a day over their base), a keeper 16p and a hand 10p, the works' fixed wages;
    // down as any till's; with the rule at the drift's own bound, as a city till.
    {
        const auto paidAt = [](int step, int most) {
            auto t = Town(city(soapworks(1000)));
            auto& s = t.s;
            const auto till = s.tillOf("boiler");
            t.until(1 + 1. / 24);                   // (A society restores once it has begun its days.)
            auto saved = s.state();
            saved.loop.wages[till].step = step;
            expect(s.restore(saved), "restored with a step of " + std::to_string(step));
            auto economy = s.authored().economy;
            economy.rules["wageDrift"] = 1;     // (Off in the tests' city.)
            economy.rules["workshopWageMost"] = most;
            s.adoptEconomy(economy);
            t.until(1 + 17.9 / 24);
            s.takeJournal();
            t.until(1 + 18.5 / 24);
            std::map<std::string, std::int64_t> wages;
            for (const auto& e : s.takeJournal())
                if (e.kind == "wages" && e.from == till)
                    wages[e.to] += e.coins;
            return std::pair{wages["boiler"], wages["hand"]};
        };
        const auto& e = Society(Roster::None).authored().economy;
        expect(Society(Roster::None).rule("workshopWageMost") == 2, "workshopWageMost 2");
        const auto capped = paidAt(6, 2), city = paidAt(6, 12), down = paidAt(-1, 2);
        expect(capped.first == e.wageKeeper + 2 && capped.second == e.wageHand + 2,
               "a step of 6 pays the keeper and the hand 2p over the world's: " + std::to_string(capped.first) + ", " +
                   std::to_string(capped.second));
        expect(city.first == e.wageKeeper + 6 && city.second == e.wageHand + 6, "the rule at 12: the step's 6p");
        expect(down.first == e.wageKeeper - 1 && down.second == e.wageHand - 1, "down as any till's: a penny under");
    }
    // The butchers' tallow (doc 72, after country5: their cuts' by-product piled in their tills, 39,300, while the
    // soapworks went short ~4,700 a season): the soapworks, the chandlery and the perfumer buy it of them first, till to
    // till at the butcher's price, the store room only what they lack.
    {
        for (const char* buyer : {"country_soapworks", "chandlery", "perfumer"})
            expect(Society(Roster::None).citySellers(buyer, "tallow") == std::vector<std::string>{"butcher"},
                   std::string(buyer) + " buys the butchers' tallow");
        auto people = soapworks();
        people.push_back(person("butcher", "merchant", "butcher at the Shambles", {"shambles", 2.5, 2.5}, "home_butcher", 40, 400));
        auto s = city(people);
        const auto works = s.tillOf("boiler"), shambles = s.tillOf("butcher");
        for (const char* g : {"fine_soap", "candle_tallow", "tallow"})
            empty(s, works, g);
        empty(s, shambles, "tallow");
        s.create(shambles, "tallow", 40, "test: the butcher's tallow");
        s.create(works, "herbs", 10, "test: herbs");
        s.create(works, "cord", 10, "test: cord");
        const auto money = s.moneySupply();
        const auto butcher0 = s.cash(shambles);
        s.takeJournal();
        s.restockFromStore("boiler", 1 + 9 / 24.);
        int fromButcher = 0, fromStore = 0;
        std::int64_t paid = 0;
        for (const auto& e : s.takeJournal())
            if (e.item == "tallow" && e.from == works && e.to == shambles)
                fromButcher += e.quantity, paid += e.coins;
            else if (e.item.empty() && e.from == works && e.to == shambles)
                paid += e.coins;
            else if (e.item == "tallow" && e.kind.rfind("drawn for", 0) == 0)
                fromStore += e.quantity;
        expect(fromButcher > 0 && fromStore == 0 && paid == s.seasonalCost("tallow", fromButcher) &&
                   s.cash(shambles) - butcher0 == paid,
               "the soapworks' tallow the butcher's, at its price, none drawn: " + std::to_string(fromButcher) + " for " +
                   std::to_string(paid) + "p");
        expect(s.conserved() && s.moneySupply() == money, "the butcher's tallow: conserved");
    }
    // A world without the building: the general store's soap the store room's, as before Phase 5.
    {
        auto s = city({person("grocer", "merchant", "shopkeeper at The Corner Stores", {"store", 2.5, 2.5},
                              "home_grocer", 40, 800)});
        const auto store = s.tillOf("grocer");
        empty(s, store, "fine_soap");
        for (const auto& ware : s.wares("grocer"))
            if (ware != "fine_soap")
                s.create(store, ware, 40, "test: the shelves full");
        s.takeJournal();
        s.restockFromStore("grocer", 1 + 9 / 24.);
        int fromStore = 0;
        for (const auto& e : s.takeJournal())
            fromStore += e.item == "fine_soap" && e.kind == "restocked from the store room" ? e.quantity : 0;
        expect(fromStore > 0 && s.loopToday().cityResalePieces == 0, "no workshop: the store room's soap");
    }
}

// The resale path's prices: a supplier's own `price` (the smiths' nails and cookpots at the catalogue's), else the
// store room's resaleMarkup; a craft input at the seller's price as before.
void theResalePrices()
{
    Society s(Roster::None);
    expect(s.supplierPrice("general", "country_soapworks", "fine_soap", true) == 75,
           "a good sold on: the store room's 75%");
    expect(s.supplierPrice("general", "smithy", "nails", true) == 100 &&
               s.supplierPrice("rag_and_bone", "tinker", "cookpot", true) == 100 &&
               s.supplierPrice("barber", "apothecary", "bandages", true) == 100,
           "the smiths' nails and cookpots and the bandages at the catalogue's (at 75% under their inputs' cost)");
    expect(s.supplierPrice("tailor", "weaver", "linen_cloth", false) == 100, "a craft input at the seller's price");
    // Nails for the general store from a smith, at the catalogue's price.
    auto t = city(
        {person("smith", "merchant", "the smith at the forge", {"forge", 2.5, 2.5}, "home_smith", 40, 100),
         person("grocer", "merchant", "shopkeeper at The Corner Stores", {"store", 2.5, 2.5}, "home_grocer", 40, 800)});
    const auto smith = t.tillOf("smith"), store = t.tillOf("grocer");
    empty(t, smith, "nails");
    t.create(smith, "nails", 6, "test: nails on the smith's shelf");
    const auto money = t.moneySupply();
    const auto s0 = t.cash(smith);
    expect(t.buyFromCity(store, items::business("general"), "nails", 4, true) == 4 &&
               t.cash(smith) - s0 == t.seasonalCost("nails", 4),
           "four nails at the catalogue's (the season's) price");
    expect(t.conserved() && t.moneySupply() == money, "conserved");
}

// country5b's carry-overs (doc 72): Larkfield's smith alone made no nails in year 3 (3.4 hours a batch against the
// hoops' and cookpots' 2.1: each hour's bank went on a hoop as soon as it held 2.1); now a sold-out ware's longer craft
// has the crew's hours saved for it, and the hoops and cookpots are still made. A sword's day at the forge (over six
// hours a post) is not saved for. The neckerchiefs at Larkfield's looms, the cord at the ropewalk, the planks of the
// builders' works and the carpenters, as data.
void aSmithAlone()
{
    const auto made = [](const std::string& label, const std::string& cell, int hours, const std::vector<std::string>& sold,
                         const std::map<std::string, int>& start = {}) {
        auto s = city({person("smith", "merchant", label, {cell, 3.5, 3.5}, "home_smith", 40, 300)});
        const auto till = s.tillOf("smith");
        for (const auto& ware : s.wares("smith"))
            empty(s, till, ware);
        for (const auto& ware : s.wares("smith"))
            if (std::find(sold.begin(), sold.end(), ware) == sold.end())
                s.create(till, ware, 40, "test: the shelf full");
        for (const auto& [ware, n] : start)
            s.create(till, ware, n, "test: a little on the shelf");
        s.create(till, "iron_bar", 40, "test: iron bars");
        s.create(till, "charcoal", 40, "test: charcoal");
        s.create(till, "bronze_bar", 40, "test: bronze bars");
        const auto money = s.moneySupply();
        for (int h = 0; h < hours; ++h)
            s.restockFromStore("smith", 1 + (7 + h) / 24.);
        expect(s.conserved() && s.moneySupply() == money, label + ": making, conserved");
        std::map<std::string, int> out;
        for (const auto& ware : sold)
            out[ware] = Society::stock(*s.account(till), ware);
        return out;
    };
    const auto village = made("village smith at Larkfield Smithy", "cn_larkfield_smithy", 12, {"nails", "iron_hoops", "cookpot"});
    expect(village.at("nails") >= 6, "the village smith alone makes nails: " + std::to_string(village.at("nails")));
    expect(village.at("iron_hoops") >= 3 && village.at("cookpot") >= 1,
           "and its hoops and cookpots still: " + std::to_string(village.at("iron_hoops")) + ", " +
               std::to_string(village.at("cookpot")));
    // A city smith alone, its iron swords sold out (19.4 hours a sword) and its nails low: the sword not saved for, the
    // nails made.
    const auto city0 = made("the smith at the forge", "forge", 12, {"iron_sword", "nails"}, {{"nails", 1}});
    expect(city0.at("iron_sword") == 0 && city0.at("nails") >= 6,
           "an iron sword's 19 hours not saved for: the nails made (" + std::to_string(city0.at("nails")) + ")");

    const auto makes = [](const std::string& business, const std::string& craft) {
        const auto crafts = items::craftsFor(business);
        return std::any_of(crafts.begin(), crafts.end(), [&](const items::Craft* k) { return k->id == craft; });
    };
    const auto has = [](const std::vector<std::string>& v, const std::string& x) {
        return std::find(v.begin(), v.end(), x) != v.end();
    };
    expect(makes("country_linen_weavers", "linen_neckerchief"), "Larkfield's looms make neckerchiefs");
    expect(makes("ropewalk", "cord") && makes("chandlery", "cord"), "the ropewalk makes cord beside the chandlery");
    Society s(Roster::None);
    for (const char* buyer : {"country_soapworks", "chandlery", "general"})
        expect(has(s.citySellers(buyer, "cord"), "ropewalk"), std::string(buyer) + ": cord from the ropewalk");
    for (const char* buyer : {"cooperage", "cartwright", "carpenter"})
        expect(has(s.citySellers(buyer, "planks"), "sawmill") && has(s.citySellers(buyer, "planks"), "builders_works") &&
                   (std::string(buyer) == "carpenter" || has(s.citySellers(buyer, "planks"), "carpenter")),
               std::string(buyer) + ": planks from the sawmill, the builders' works and (but its own) the carpenters");
}

const char* const Stall = "keeping the stall in the square";

// Cheese, the luxury: the poor household buys its food, never cheese, and keeps its money; the rich one buys a piece
// at the stall's high price, within its luxuries' share; the stall's price for it climbs past the city's band while it
// sells out, and stays out of the city's price level. By class, not cash (doc 72, after country5): a household of the
// Houses buys from what is over the comforts' keep (40p a head), any other only over luxuryAbove (200p a head), so the
// stallholder's middling household (200p) buys none, the Houses' lady (120p) one at the season's price.
void theCheese()
{
    const auto& cfg = items::cityLoop();
    expect(Society(Roster::None).rule("luxuryAbove") == 200 && Society(Roster::None).rule("luxuryPriceBand") == 700,
           "the luxuries' keep 200p a head, their band eight times");
    expect(cfg.luxuries == std::vector<std::string>{"cheese"} && Society::luxury("cheese") && !Society::luxury("bread"),
           "cheese the luxury");
    expect(std::find(cfg.comforts.begin(), cfg.comforts.end(), "cheese") == cfg.comforts.end(),
           "and no longer a comfort");
    const auto day = [](double factor, int cheese) {
        auto t = Town(city({person("stall", "merchant", Stall, {"square", 2.5, 2.5}, "home_stall", 40, 200),
                            person("pam", "civilian", "keeps the house", {"home_pam", 2.5, 2.5}, "home_pam", 30, 30),
                            person("rex", "civilian", "keeps the house", {"home_rex", 2.5, 2.5}, "home_rex", 50, 2000),
                            person("lady", "civilian", "at home in the Heights", {"home_lady", 2.5, 2.5}, "home_lady", 45, 120)},
                           [](AuthoredRoster& r) {
                               r.economy.wharf = false;
                               r.economy.storeRule = false; // (Its shelves only what the test puts there.)
                               r.economy.rules["stockPrices"] = 1;
                               r.economy.rules["shelfFollow"] = 0;
                               r.economy.rules["wageSupport"] = 0;
                           }));
        auto& s = t.s;
        const auto till = s.tillOf("stall");
        for (const auto& [item, n] :
             std::map<std::string, int>(s.account(till)->stock.begin(), s.account(till)->stock.end()))
            if (n > 0)
                s.consume(till, item, n, "test: an empty stall");
        s.create(till, "bread", 60, "test: bread");
        if (cheese > 0)
            s.create(till, "cheese", cheese, "test: cheese");
        t.until(1 + 1. / 24);
        auto saved = s.state();
        saved.loop.prices[till]["cheese"] = factor;
        expect(s.restore(saved), "restored with the stall's cheese at " + std::to_string(factor));
        s.takeJournal();
        const auto money = s.moneySupply();
        const auto rex0 = s.cash("rex"), pam0 = s.cash("pam");
        expect(s.classOf("lady") == "upper" && s.classOf("rex") != "upper" && s.classOf("stall") != "upper",
               "the lady the Houses', the others not: " + s.classOf("lady") + ", " + s.classOf("rex") + ", " + s.classOf("stall"));
        t.until(1 + 17.5 / 24);
        std::map<std::string, std::map<std::string, int>> bought;
        for (const auto& e : s.takeJournal())
            if (e.kind == "market" && e.to == till)
                bought[e.from][e.item] += e.quantity;
        expect(s.conserved() && s.moneySupply() == money, "conserved");
        int pamFood = 0;
        for (const auto& [item, n] : bought["pam"])
            pamFood += item != "cheese" && Society::edible(item) ? n : 0;
        expect(pamFood > 0 && bought["pam"]["cheese"] == 0, "the poor household buys its food, never cheese");
        double held = 0;
        for (const auto& [item, n] : s.account("pam")->stock)
            held += double(n) * Society::nourishment(item);
        expect(held >= items::cityLoop().shortDays * Society::NourishADay && s.cash("pam") > 0,
               "fed (no hunger), and money kept: " + std::to_string(s.cash("pam")));
        expect(bought["rex"]["cheese"] <= 1, "at most a piece a head a day");
        const int left = Society::stock(*s.account(till), "cheese");
        // Another day's turn: the cheese sold out climbs, past the city's 25% band; nothing else moved by it.
        t.until(2 + 1. / 24);
        return std::tuple{bought, rex0 - s.cash("rex"),          pam0 - s.cash("pam"),
                          left,   s.stockFactor(till, "cheese"), s.stockFactor(till, "bread")};
    };
    {
        const auto [bought, rexSpent, pamSpent, left, cheeseF, breadF] = day(3.0, 1);
        expect(bought.at("rex").count("cheese") && bought.at("rex").at("cheese") == 1,
               "the rich household buys its piece at three times the price");
        const auto price = std::int64_t(std::ceil(Society::catalogPrice("cheese") * 3.0 - 1e-9));
        expect(rexSpent >= price && price <= (2000 - 200) * 10 / 100,
               "within its luxuries' share: " + std::to_string(price));
        expect(left == 0 && cheeseF > 3.0 && cheeseF <= 4.0,
               "sold out: its price climbs on, in its own band: " + std::to_string(cheeseF) + ", left " +
                   std::to_string(left));
        expect(std::abs(breadF - 1) < .3, "the bread's factor the city's own: " + std::to_string(breadF));
        (void)pamSpent;
    }
    {
        // At the season's price (5p) the Houses' lady (120p: a share of ~7p over her 40p keep) buys one too; the
        // stallholder's household (200p, under its 200p keep after its food), and the poor, none.
        const auto [bought, rexSpent, pamSpent, left, cheeseF, breadF] = day(1.0, 5);
        expect(bought.at("rex").at("cheese") == 1 && bought.at("lady").at("cheese") == 1 && left == 3 &&
                   bought.at("pam").at("cheese") == 0 && (!bought.count("stall") || !bought.at("stall").count("cheese") ||
                                                          bought.at("stall").at("cheese") == 0),
               "at the season's price the rich and the Houses' buy one each, the middling and the poor none: left " +
                   std::to_string(left));
        (void)rexSpent, (void)pamSpent, (void)cheeseF, (void)breadF;
    }
    {
        // At four times (20p) only the rich: the lady's share is under the price.
        const auto [bought, rexSpent, pamSpent, left, cheeseF, breadF] = day(4.0, 5);
        expect(bought.at("rex").at("cheese") == 1 && left == 4,
               "at four times its price only the rich buys: left " + std::to_string(left));
        expect(cheeseF <= 8.0 + 1e-9, "never over its band: " + std::to_string(cheeseF));
        (void)rexSpent, (void)pamSpent, (void)breadF;
    }
    {
        // At its band's top (eight times, 40p) the rich still buy within their share, and it climbs no further.
        const auto [bought, rexSpent, pamSpent, left, cheeseF, breadF] = day(8.0, 1);
        expect(bought.at("rex").at("cheese") == 1 && left == 0 && rexSpent >= 40 && rexSpent <= (2000 - 200) * 10 / 100,
               "at eight times the rich buys its piece: " + std::to_string(rexSpent));
        expect(std::abs(cheeseF - 8.0) < 1e-9, "sold out at the top: no further: " + std::to_string(cheeseF));
        (void)pamSpent, (void)breadF;
    }
}

// The Houses' own tables (doc 72, after country5, the user's intent: "only the rich and noble being able to afford
// it"): once a day the Country's purse buys luxuries with luxuryHouses percent of what it holds over its reserve and its
// posts' nights, at the shop's own (dear) price, at most luxuryHousesMost pieces, never into its reserve; conserved.
void theHousesCheese()
{
    const auto run = [](int share, int cheese, double factor) {
        auto t = Town(city({person("stall", "merchant", Stall, {"square", 2.5, 2.5}, "home_stall", 40, 200),
                            person("rex", "civilian", "keeps the house", {"home_rex", 2.5, 2.5}, "home_rex", 50, 3000)},
                           [&](AuthoredRoster& r) {
                               r.economy.storeRule = false;
                               r.economy.rules["stockPrices"] = 1;
                               r.economy.rules["shelfFollow"] = 0;
                               r.economy.rules["wageSupport"] = 0;
                               r.economy.rules["luxuryShare"] = 0;   // (The households none: the Houses' alone.)
                               r.economy.rules["luxuryHouses"] = share;
                           }));
        auto& s = t.s;
        const auto till = s.tillOf("stall");
        empty(s, till, "cheese");
        s.create(till, "cheese", cheese, "test: cheese");
        // The Houses' estates in funds (from a rich wolf: nothing made).
        expect(s.shift("rex", Society::WharfFund, "", 0, 1500, "test: the estates' rents"), "the Country's purse");
        t.until(1 + 1. / 24);
        auto saved = s.state();
        saved.loop.prices[till]["cheese"] = factor;
        expect(s.restore(saved), "restored");
        s.takeJournal();
        const auto money = s.moneySupply();
        const auto each = s.shopCost(till, "cheese", 1);   // (The stall's own price: the season's, times its factor.)
        t.until(1 + 17.5 / 24);
        int pieces = 0;
        std::int64_t paid = 0;
        for (const auto& e : s.takeJournal())
            if (e.from == Society::WharfFund && e.to == till && e.kind.rfind("a luxury for the Houses' tables", 0) == 0)
                pieces += e.quantity, paid += e.coins;
        expect(s.conserved() && s.moneySupply() == money, "the Houses' cheese: conserved");
        expect(s.loopToday().luxuriesHouses == pieces && s.loopToday().luxuriesHousesSpent == paid &&
                   s.loopToday().luxuriesBought == 0,
               "counted as the Houses', none as a household's: " + std::to_string(pieces));
        return std::tuple{pieces, paid, each, s.cash(Society::WharfFund)};
    };
    {
        // 40% of ~1,300p over the reserve: ~520p, at three times (~16p a piece) all ten, each at the stall's own price.
        const auto [pieces, paid, each, country1] = run(40, 10, 3.0);
        expect(pieces == 10 && each >= 15 && paid == 10 * each,
               "the Houses buy the stall's ten at three times: " + std::to_string(pieces) + " for " + std::to_string(paid) + "p");
        expect(country1 >= 200, "never into the Country's reserve: " + std::to_string(country1));
    }
    {
        // At most luxuryHousesMost (24) a day, however much is spare.
        const auto [pieces, paid, country0, country1] = run(100, 40, 1.0);
        expect(pieces == 24, "at most 24 a day: " + std::to_string(pieces));
        (void)paid, (void)country0, (void)country1;
    }
    {
        // Within the share: 1% of ~1,300p is 13p, under one piece at three times.
        const auto [pieces, paid, country0, country1] = run(1, 10, 3.0);
        expect(pieces == 0 && paid == 0, "a share under a piece's price buys none: " + std::to_string(pieces));
        (void)country0, (void)country1;
    }
    {
        // Off: none.
        const auto [pieces, paid, country0, country1] = run(0, 10, 1.0);
        expect(pieces == 0, "luxuryHouses 0: none");
        (void)paid, (void)country0, (void)country1;
    }
}
} // namespace

// The Houses' city businesses (doc 72, Progress, "The producers and the fixes"; city_loop.json `houseBusinesses`): the
// Timberland Ironworks pays its crew from its till at 18:00 and sends what is over its float and workshopWagesDays of
// its wages to House Timberland's estate, booked by House, nothing to its keeper as profit; its wages drift at most
// workshopWageMost; conserved. An ironworks at a building not on the list keeps the old way (its keeper's profit).
void theHousesCityBusinesses()
{
    const auto& owned = items::cityLoop().houseTills;
    expect(owned.size() == 2 && owned.at("rm_the_grayrock_ironworks") == "Timberland" &&
               owned.at("rm_the_grayrock_foundry") == "Timberland",
           "the Timberland Ironworks and the foundry are House Timberland's");
    const auto works = [](const std::string& cell, std::int64_t purse) {
        return std::vector<ResidentSpec>{
            person("ironmaster", "merchant", "runs The Timberland Ironworks", {cell, 3.5, 3.5}, "home_iron", 50, purse),
            person("puddler", "civilian", "works at The Timberland Ironworks", {cell, 5.5, 3.5}, "home_iron", 30, 20)};
    };
    const auto night = [&](const std::string& cell) {
        auto t = Town(city(works(cell, 1500)));
        auto& s = t.s;
        const auto till = s.tillOf("ironmaster");
        t.until(1 + 17.9 / 24);
        s.takeJournal();
        const auto money = s.moneySupply();
        t.until(1 + 18.5 / 24);
        std::map<std::string, std::int64_t> wages;
        std::int64_t toHouse = 0, profit = 0;
        for (const auto& e : s.takeJournal())
        {
            if (e.kind == "wages" && e.from == till)
                wages[e.to] += e.coins;
            else if (e.kind == "a House's business's takings to its House: Timberland" && e.from == till &&
                     e.to == "estate:timberland")
                toHouse += e.coins;
            else if (e.kind == "profit" && e.from == till)
                profit += e.coins;
        }
        const auto& e = s.authored().economy;
        const auto* b = items::businessFor("runs The Timberland Ironworks");
        expect(b && b->id == "ironworks", "an ironworks");
        expect(wages["ironmaster"] == e.wageKeeper && wages["puddler"] == e.wageHand,
               cell + ": the keeper's and the hand's wages from the till: " + std::to_string(wages["ironmaster"]) + ", " +
                   std::to_string(wages["puddler"]));
        expect(s.conserved() && s.moneySupply() == money, cell + ": conserved");
        return std::tuple{toHouse, profit, s.loopToday(), s.cash(till), s.rule("workshopWagesDays") * (e.wageKeeper + e.wageHand)};
    };
    {
        const auto [toHouse, profit, day, held, kept] = night("rm_the_grayrock_ironworks");
        const auto w = day.workshops.find("rm_the_grayrock_ironworks");
        expect(toHouse > 0 && profit == 0 && w != day.workshops.end() && w->second.toHouse == toHouse &&
                   w->second.paid == w->second.owed && day.rents.count("Timberland") && day.rents.at("Timberland") == toHouse &&
                   day.estates.count("timberland") && day.estates.at("timberland").rents >= toHouse,
               "the Timberland Ironworks' takings over its float to Timberland's estate, booked, none to its keeper: " +
                   std::to_string(toHouse));
        expect(held >= kept && held <= kept + 200, "it keeps its float and four days' wages: " + std::to_string(held));
    }
    {
        const auto [toHouse, profit, day, held, kept] = night("rm_a_private_ironworks");
        (void)held, (void)kept;
        expect(toHouse == 0 && profit > 0 && !day.workshops.count("rm_a_private_ironworks") && !day.rents.count("Timberland"),
               "an ironworks not on the list: its keeper's profit, as before: " + std::to_string(profit));
    }
    // Its wage step capped at workshopWageMost, as a country workshop's.
    {
        auto t = Town(city(works("rm_the_grayrock_ironworks", 1500)));
        auto& s = t.s;
        const auto till = s.tillOf("ironmaster");
        t.until(1 + 1. / 24);
        auto saved = s.state();
        saved.loop.wages[till].step = 6;
        expect(s.restore(saved), "restored with a step of 6");
        auto economy = s.authored().economy;
        economy.rules["wageDrift"] = 1;
        s.adoptEconomy(economy);
        t.until(1 + 17.9 / 24);
        s.takeJournal();
        t.until(1 + 18.5 / 24);
        std::map<std::string, std::int64_t> wages;
        for (const auto& e : s.takeJournal())
            if (e.kind == "wages" && e.from == till)
                wages[e.to] += e.coins;
        const auto& e = s.authored().economy;
        expect(wages["ironmaster"] == e.wageKeeper + 2 && wages["puddler"] == e.wageHand + 2,
               "a step of 6 pays the ironworks' keeper and hand 2p over the world's: " + std::to_string(wages["ironmaster"]));
    }
}

// The Coldwater Still (doc 72, Progress, "The producers and the fixes"): House Fell's country workshop presses the
// orchards' apples into cider and stills it into apple brandy, the vineyards' wine into brandy; the vintner buys both
// of it till to till at the store room's price, none from the store room; its crew paid and what is over to Fell's
// estate; apple brandy and brandy among the households' comforts; conserved.
const char* const Still = "cn_coldwater_still";
std::vector<ResidentSpec> still(std::int64_t purse = 300)
{
    return {person("stillmaster", "merchant", "stillmaster at Coldwater Still", {Still, 3.5, 3.5}, "cn_coldwater_cottage_1", 45, purse),
            person("stillhand", "civilian", "still hand at Coldwater Still", {Still, 5.5, 3.5}, "cn_coldwater_cottage_1", 25, 20),
            person("vintner", "merchant", "vintner at the Fell Wine Vaults", {"vaults", 2.5, 2.5}, "home_vintner", 40, 800)};
}

void theStill()
{
    const auto& comforts = items::cityLoop().comforts;
    for (const char* g : {"apple_brandy", "brandy"})
        expect(std::find(comforts.begin(), comforts.end(), g) != comforts.end(), std::string(g) + ": a comfort");
    for (const char* g : {"apple_brandy", "brandy"})
    {
        const auto sellers = Society(Roster::None).citySellers("vintner", g);
        expect(sellers == std::vector<std::string>({"country_still", "distillery"}), std::string(g) + ": the vintner's from the still and the distillery");
    }
    expect(Society(Roster::None).citySellers("weaver", "linen_cloth") == std::vector<std::string>({"country_linen_weavers"}),
           "the city weaver's linen from Larkfield's looms (it drew the Country's)");
    // It makes cider of apples, apple brandy of the cider, brandy of wine.
    {
        auto s = city(still());
        const auto* b = items::businessFor("stillmaster at Coldwater Still");
        expect(b && b->id == "country_still", "the still a country_still");
        expect(!items::businessFor("still hand at Coldwater Still"), "its hand's label no business's");
        const auto wares = s.wares("stillmaster");
        for (const char* g : {"cider", "apple_brandy", "brandy"})
            expect(std::find(wares.begin(), wares.end(), g) != wares.end(), std::string("the still sells ") + g);
        const auto till = s.tillOf("stillmaster");
        for (const auto& g : wares)
            empty(s, till, g);
        s.create(till, "apples", 40, "test: the orchards' apples");
        s.create(till, "wine", 6, "test: the vineyards' wine");
        s.create(till, "firewood", 12, "test: firewood");
        const auto money = s.moneySupply();
        for (int h = 0; h < 36; ++h)
            s.restockFromStore("stillmaster", 1 + (8 + h) / 24.);
        const auto& a = *s.account(till);
        expect(Society::stock(a, "apple_brandy") >= 2 && Society::stock(a, "brandy") >= 1 && Society::stock(a, "apples") < 40,
               "made apple brandy and brandy of the apples and the wine: " + std::to_string(Society::stock(a, "apple_brandy")) +
                   ", " + std::to_string(Society::stock(a, "brandy")));
        expect(s.conserved() && s.moneySupply() == money, "the still's making: conserved");
    }
    // The vintner's apple brandy from the still, at the store room's price, none from the store room.
    {
        auto s = city(still());
        const auto works = s.tillOf("stillmaster"), vaults = s.tillOf("vintner");
        empty(s, works, "apple_brandy");
        empty(s, vaults, "apple_brandy");
        s.create(works, "apple_brandy", 8, "test: apple brandy at the still");
        for (const auto& ware : s.wares("vintner"))
            if (ware != "apple_brandy")
                s.create(vaults, ware, 40, "test: the shelves full");
        const auto money = s.moneySupply();
        const auto works0 = s.cash(works);
        s.takeJournal();
        s.restockFromStore("vintner", 1 + 9 / 24.);
        int fromStill = 0, fromStore = 0;
        for (const auto& e : s.takeJournal())
            if (e.item == "apple_brandy" && e.kind == "from a maker, to sell on" && e.from == vaults && e.to == works)
                fromStill += e.quantity;
            else if (e.item == "apple_brandy" && e.kind == "restocked from the store room")
                fromStore += e.quantity;
        expect(fromStill > 0 && fromStore == 0 && s.cash(works) > works0,
               "the vintner's apple brandy the still's: " + std::to_string(fromStill));
        expect(s.conserved() && s.moneySupply() == money, "the vintner's purchase: conserved");
    }
    // At 18:00 its crew paid, what is over to House Fell's estate.
    {
        auto t = Town(city(still(1000)));
        auto& s = t.s;
        const auto till = s.tillOf("stillmaster");
        t.until(1 + 17.9 / 24);
        s.takeJournal();
        const auto money = s.moneySupply();
        t.until(1 + 18.5 / 24);
        std::int64_t toHouse = 0, profit = 0;
        for (const auto& e : s.takeJournal())
            if (e.kind == "a workshop's takings to its House: Fell" && e.from == till && e.to == "estate:fell")
                toHouse += e.coins;
            else if (e.kind == "profit" && e.from == till)
                profit += e.coins;
        expect(toHouse > 0 && profit == 0 && s.loopToday().rents.count("Fell") && s.loopToday().rents.at("Fell") >= toHouse,
               "the still's takings over its float to Fell's estate: " + std::to_string(toHouse));
        expect(s.conserved() && s.moneySupply() == money, "the still's wages and Fell's share: conserved");
    }
}

int main()
{
    try
    {
        catalogue();
        theWorkshopsData();
        aCommunityWorkshop();
        theHousesCityBusinesses();
        theStill();
        theResalePrices();
        aSmithAlone();
        theCheese();
        theHousesCheese();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Workshops tests passed: " << checks << " checks.\n";
    return 0;
}
