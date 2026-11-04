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
    r.economy.rules = {{"services", 0}, {"patrons", 0}, {"surplus", 0}, {"wageDrift", 0}, {"treasuryDrift", 0}};
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

// The eight workshops as data: each at its fixed building, merged into its community's market as its business, its
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
        {"cn_oakhaven_press", {"country_press", "Timberland"}}};
    expect(people.workshops.size() == want.size(), "eight workshops: " + std::to_string(people.workshops.size()));
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
            label.replace(label.find("{name}"), 6, w->name);
            const auto* business = items::businessFor(label);
            expect(label.size() <= 40, "at most 40 characters: " + label);
            if (rule->second.role == "merchant")
                expect(business && business->id == w->business && n == 1, label + ": its keeper, of " + w->business);
            else
                expect(!business, label + ": no business's");
        }
    }
    expect(posts == 18, "18 posts: " + std::to_string(posts));
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
        const auto works0 = s.cash(works), store0 = s.cash(store), country0 = s.cash(Society::CountryFund),
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
            expect(s.cash(Society::CountryFund) == country0 && s.cash("treasury") == treasury0,
                   "the Country sells none of it, and no toll (till to till)");
            expect(s.loopToday().cityResalePieces == fromWorks && s.loopToday().cityResale == paid &&
                       s.loopToday().cityResaleGoods.at("fine_soap") == fromWorks,
                   "counted as the resale trade");
        }
        else
            expect(fromWorks == 0 && fromStore > 0 && s.cash(Society::CountryFund) > country0,
                   std::string(suppliersOn ? "the soapworks out of soap" : "suppliers off") +
                       ": the store room's, the Country paid");
        return fromWorks;
    };
    sold(true, true);
    sold(false, true);
    sold(true, false);
    // Its crew paid from its till at 18:00, at the city's wages; what is over its float and a week of wages to its
    // House's estate (the Country's purse), booked by House; nothing to its keeper as profit; conserved.
    {
        auto t = Town(city(soapworks(1000)));
        auto& s = t.s;
        const auto till = s.tillOf("boiler");
        t.until(1 + 17.9 / 24);
        s.takeJournal();
        const auto money = s.moneySupply();
        const auto country0 = s.cash(Society::CountryFund);
        t.until(1 + 18.5 / 24);
        std::map<std::string, std::int64_t> wages;
        std::int64_t toHouse = 0, profit = 0;
        for (const auto& e : s.takeJournal())
        {
            if (e.kind == "wages" && e.from == till)
                wages[e.to] += e.coins;
            else if (e.kind == "a workshop's takings to its House: the Crown" && e.from == till &&
                     e.to == Society::CountryFund)
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
        expect(s.cash(till) >= 7 * (e.wageKeeper + e.wageHand) && s.cash(till) <= 7 * (e.wageKeeper + e.wageHand) + 100,
               "the till keeps its float and a week's wages: " + std::to_string(s.cash(till)));
        (void)country0;
        expect(s.conserved() && s.moneySupply() == money, "wages and the House's share: conserved");
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

const char* const Stall = "keeping the stall in the square";

// Cheese, the luxury: the poor household buys its food, never cheese, and keeps its money; the rich one buys a piece
// at the stall's high price, within its luxuries' share; the stall's price for it climbs past the city's band while it
// sells out, and stays out of the city's price level.
void theCheese()
{
    const auto& cfg = items::country();
    expect(cfg.luxuries == std::vector<std::string>{"cheese"} && Society::luxury("cheese") && !Society::luxury("bread"),
           "cheese the luxury");
    expect(std::find(cfg.comforts.begin(), cfg.comforts.end(), "cheese") == cfg.comforts.end(),
           "and no longer a comfort");
    const auto day = [](double factor, int cheese) {
        auto t = Town(city({person("stall", "merchant", Stall, {"square", 2.5, 2.5}, "home_stall", 40, 200),
                            person("pam", "civilian", "keeps the house", {"home_pam", 2.5, 2.5}, "home_pam", 30, 30),
                            person("rex", "civilian", "keeps the house", {"home_rex", 2.5, 2.5}, "home_rex", 50, 2000)},
                           [](AuthoredRoster& r) {
                               r.economy.country = false;
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
        expect(held >= items::country().shortDays * Society::NourishADay && s.cash("pam") > 0,
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
        expect(rexSpent >= price && price <= (2000 - items::country().comfortsAbove) * 10 / 100,
               "within its luxuries' share: " + std::to_string(price));
        expect(left == 0 && cheeseF > 3.0 && cheeseF <= 4.0,
               "sold out: its price climbs on, in its own band: " + std::to_string(cheeseF) + ", left " +
                   std::to_string(left));
        expect(std::abs(breadF - 1) < .3, "the bread's factor the city's own: " + std::to_string(breadF));
        (void)pamSpent;
    }
    {
        // At the season's price (5p) the stallholder's household (200p: a share of 16p) buys one too; the poor none.
        const auto [bought, rexSpent, pamSpent, left, cheeseF, breadF] = day(1.0, 5);
        expect(bought.at("rex").at("cheese") == 1 && left == 3 && bought.at("pam").at("cheese") == 0,
               "at the season's price the rich and the middling buy one each, the poor none: left " +
                   std::to_string(left));
        (void)rexSpent, (void)pamSpent, (void)cheeseF, (void)breadF;
    }
    {
        // At four times (20p) only the rich: the stallholder's share is under the price.
        const auto [bought, rexSpent, pamSpent, left, cheeseF, breadF] = day(4.0, 5);
        expect(bought.at("rex").at("cheese") == 1 && left == 4,
               "at four times its price only the rich buys: left " + std::to_string(left));
        expect(cheeseF <= 4.0 + 1e-9, "never over its band: " + std::to_string(cheeseF));
        (void)rexSpent, (void)pamSpent, (void)breadF;
    }
}
} // namespace

int main()
{
    try
    {
        catalogue();
        theWorkshopsData();
        aCommunityWorkshop();
        theResalePrices();
        theCheese();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Workshops tests passed: " << checks << " checks.\n";
    return 0;
}
