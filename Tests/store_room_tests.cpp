// The store room (Docs/Design/67-ridgemere2-city.md, 6): a business's craft inputs and resale goods, without end, at the
// catalogue's price times a markup, drawn by its keeper into the shop's till; the coins kept in the world (the Country's
// purse since doc 68, here with no toll and no carriage);
// the world's switches and each business's own (the "store" and "nostore" records); a refusal changes nothing; the
// restocking rule, off unless switched on; the fund in a save. And the business labels and store goods shared with
// tools/item_catalog.py (Tests/fixtures/business_labels.json), so the two cannot drift.
#include "RatwCalendar.h"
#include "RatwGame.h"
#include "RatwItems.h"
#include "RatwJsonDoc.h"
#include "RatwSociety.h"
#include "RatwWire.h"
#include "RatwWorld.h"

#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace ratw;
namespace
{
int checks = 0;
void expect(bool condition, const std::string& message)
{
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}

ResidentSpec person(const std::string& id, const std::string& work, Spot job, const std::string& role = "merchant")
{
    ResidentSpec r;
    r.id = id;
    r.name = id;
    r.role = role;
    r.workLabel = work;
    r.home = {"homes", 2.5, 2.5};
    r.work = job;
    r.evening = r.home;
    r.serve = job;
    r.purse = 200;
    r.startHour = 8;
    r.endHour = 17;
    return r;
}

// A smith, a food stall, a baker, a shipwright (nothing to draw), a clerk, a vintner, and one unplaced.
Society city(const std::function<void(AuthoredRoster&)>& change = {})
{
    AuthoredRoster roster;
    roster.residents = {person("smith", "the smith at the forge", {"forge", 2.5, 2.5}),
                        person("stall", "keeping a food stall", {"square", 2.5, 2.5}),
                        person("baker", "baker at The Loaf", {"bakery", 2.5, 2.5}),
                        person("wright", "shipwright at the slipway", {"slip", 2.5, 2.5}),
                        person("clerk", "clerking at the City Office", {"office", 2.5, 2.5}, "civilian"),
                        person("vintner", "vintner at the Fell Wine Vaults", {"vaults", 2.5, 2.5}),
                        person("helper", "helping at the bakery", {"bakery", 3.5, 2.5}, "civilian"),
                        person("helper2", "helping at the forge", {"forge", 3.5, 2.5}, "civilian")};
    roster.unplaced = {person("wanderer", "the smith at the far forge", {})};
    roster.economy.treasury = 500;
    // (The store room alone: the whole cost to the Country, no toll, no carriage, the city's loop paused: doc 68.)
    roster.economy.gateToll = 0;
    roster.economy.carriage = 0;
    roster.economy.needsPaused = true;
    roster.economy.rules["wharfValve"] = 0;         // (The Wharf's valve shut, doc 73: the Country's flows as these tests have them; wharf_tests and farms_tests open it.)
    if (change)
        change(roster);
    Society s(Roster::None);
    s.configure(roster);
    return s;
}

std::int64_t cost(std::int64_t price, int quantity, int markup)
{
    return markup == 0 ? 0 : std::max<std::int64_t>(1, (price * quantity * markup + 99) / 100);
}

void drawingFromTheStore()
{
    auto s = city();
    expect(s.account(Society::WharfFund) && s.cash(Society::WharfFund) == 0, "the store room's fund is opened, empty");
    expect(s.storeOpen("smith"), "the smith may draw: " + s.storeClosed("smith"));
    const auto& goods = s.storeGoods("smith");
    expect(goods == std::vector<std::string>({"bronze_bar", "charcoal", "iron_bar", "leather", "timber"}), "the smith's goods are its crafts' inputs");
    expect(s.storeUse("smith", "iron_bar") == Society::StoreUse::Make, "iron bars are a making good");
    const auto till = s.tillOf("smith");
    const auto price = Society::catalogPrice("iron_bar");
    const auto want = cost(price, 3, 100);
    expect(want == price * 3 && s.storeCost("smith", "iron_bar", 3) == want, "three iron bars cost ceil(" + std::to_string(price) + " × 3 × 100 / 100)");
    const auto before = s.cash(till);
    const auto drawn = s.drawFromStore("smith", "iron_bar", 3);
    expect(drawn.ok && drawn.total == want, "the smith draws three iron bars: " + drawn.message);
    expect(s.cash(till) == before - want && s.cash(Society::WharfFund) == want, "the till pays it, and the fund holds it");
    expect(Society::stock(*s.account(till), "iron_bar") == 3, "the bars are on the shelves");
    expect(s.conserved(), "money is conserved");
    // A vendor's resale goods at the resale markup.
    expect(s.storeUse("stall", "flour") == Society::StoreUse::Resell, "flour is the stall's to resell");
    const auto stallTill = s.tillOf("stall");
    const auto flour = Society::catalogPrice("flour");
    const auto paid = s.cash(stallTill);
    const auto resold = s.drawFromStore("stall", "flour", 10);
    expect(resold.ok && resold.total == cost(flour, 10, 75) && s.cash(stallTill) == paid - cost(flour, 10, 75),
           "a stall draws ten flour at 75%: " + resold.message);
    expect(Society::stock(*s.account(stallTill), "flour") >= 10, "and has it");
    expect(s.conserved(), "money is conserved");
    // A draw that comes to less than a penny still costs one.
    auto cheap = city([](AuthoredRoster& r) { r.economy.resaleMarkup = 1; });
    expect(cheap.storeCost("stall", "flour", 1) == 1 && cheap.drawFromStore("stall", "flour", 1).total == 1, "at least 1p above a markup of 0");
}

void freeAtMarkupZero()
{
    auto s = city([](AuthoredRoster& r) { r.economy.storeMarkup = 0; });
    const auto till = s.tillOf("smith");
    const auto before = s.cash(till);
    const auto drawn = s.drawFromStore("smith", "iron_bar", 5);
    expect(drawn.ok && drawn.total == 0 && s.cash(till) == before && s.cash(Society::WharfFund) == 0, "at markup 0 the goods are free: " + drawn.message);
    expect(Society::stock(*s.account(till), "iron_bar") == 5, "and arrive");
    expect(s.conserved(), "money is conserved");
}

// Every refusal leaves the till and the fund as they were.
void refusals()
{
    const auto unchanged = [](Society& s, const std::string& keeper, const std::string& item, int n, const std::string& why) {
        const auto till = s.tillOf(keeper);
        const auto* account = s.account(till);
        const auto cash = account ? account->cash : 0;
        const auto stock = account ? account->stock : std::map<std::string, int>{};
        const auto fund = s.cash(Society::WharfFund);
        const auto journal = s.pendingJournal().size();
        const auto r = s.drawFromStore(keeper, item, n);
        expect(!r.ok, why + ": refused");
        expect(s.cash(till) == cash && (!s.account(till) || s.account(till)->stock == stock) && s.cash(Society::WharfFund) == fund &&
                   s.pendingJournal().size() == journal,
               why + ": nothing changed (" + r.message + ")");
        expect(s.conserved(), why + ": money conserved");
    };
    {
        auto s = city([](AuthoredRoster& r) { r.economy.internalStore = false; });
        unchanged(s, "smith", "iron_bar", 1, "the world's making switch off");
        expect(s.storeClosed("smith") == "off for the whole world", "said so: " + s.storeClosed("smith"));
        expect(s.drawFromStore("stall", "flour", 1).ok, "resale is its own switch");
    }
    {
        auto s = city([](AuthoredRoster& r) { r.economy.storeResale = false; });
        unchanged(s, "stall", "flour", 1, "the world's resale switch off");
        expect(s.drawFromStore("smith", "iron_bar", 1).ok, "making is its own switch");
        expect(s.storeUse("baker", "meal") == Society::StoreUse::None && s.storeUse("baker", "flour") == Society::StoreUse::Make,
               "a baker keeps its making goods without resale");
    }
    {
        auto s = city([](AuthoredRoster& r) { r.residents[0].internalStore = false; });
        unchanged(s, "smith", "iron_bar", 1, "the business off (nostore)");
        expect(s.storeClosed("smith") == "off for this business", "said so: " + s.storeClosed("smith"));
        expect(s.drawFromStore("baker", "flour", 1).ok, "the others are on");
    }
    auto s = city();
    unchanged(s, "smith", "flour", 1, "an item not among its goods");
    unchanged(s, "smith", "no_such_thing", 1, "an unknown item");
    unchanged(s, "clerk", "iron_bar", 1, "a civilian");
    unchanged(s, "wanderer", "iron_bar", 1, "an unplaced NPC");
    expect(s.storeClosed("wanderer").find("unplaced") != std::string::npos, "said so: " + s.storeClosed("wanderer"));
    unchanged(s, "nobody", "iron_bar", 1, "no such resident");
    unchanged(s, "wright", "planks", 1, "a business with nothing to draw");
    expect(s.storeClosed("wright") == "this business has nothing to draw", "said so: " + s.storeClosed("wright"));
    unchanged(s, "smith", "iron_bar", 0, "a quantity of 0");
    unchanged(s, "smith", "iron_bar", 1001, "a quantity over 1,000");
    // Short of cash: a till that can't pay.
    {
        const auto till = s.tillOf("smith");
        const auto taken = s.cash(till) - 5;
        expect(s.shift(till, "treasury", "", 0, taken, "test: emptied"), "the smithy's till down to 5p");
        unchanged(s, "smith", "iron_bar", 1, "a till short of cash");
        expect(s.shift("treasury", till, "", 0, taken, "test: refilled"), "and refilled");
    }
    // Over its stock cap, and over its kinds.
    {
        auto free = city([](AuthoredRoster& r) { r.economy.storeMarkup = 0; });
        expect(free.drawFromStore("smith", "iron_bar", 1000).ok, "a thousand bars at once");
        for (int k = 0; k < 9; ++k)
            free.drawFromStore("smith", "iron_bar", 1000);
        expect(Society::stock(*free.account(free.tillOf("smith")), "iron_bar") == 10000, "ten thousand: the cap");
        unchanged(free, "smith", "iron_bar", 1, "a till at its stock cap of a good");
        const auto till = s.tillOf("stall");
        const auto& mine = s.storeGoods("stall");
        for (const auto& good : items::allGoods())
            if (s.account(till)->stock.size() < MaxGoodsKinds && !s.account(till)->stock.count(good.id) &&
                std::find(mine.begin(), mine.end(), good.id) == mine.end())
                s.create(till, good.id, 1, "test: a full counter");
        expect(s.account(till)->stock.size() == MaxGoodsKinds, "the stall's till holds as many kinds as it may");
        std::string fresh;
        for (const auto& good : s.storeGoods("stall"))
            if (!s.account(till)->stock.count(good))
                fresh = good;
        expect(!fresh.empty(), "a good of the stall's it doesn't hold yet");
        unchanged(s, "stall", fresh, 1, "a till over its kinds");
    }
    // The fund at MoneyCap.
    {
        auto t = city([](AuthoredRoster& r) { r.economy.treasury = 1000000; });
        auto& state = const_cast<SocietyState&>(t.state());   // (A fund as full as money may be: made, as minted.)
        state.accounts.at(Society::WharfFund).cash = 1000000000 - 1;
        state.minted += 1000000000 - 1;
        expect(t.conserved(), "the fund nearly full, conserved");
        unchanged(t, "smith", "iron_bar", 1, "the fund at its cap");
    }
    // The goods fail to arrive after the till has paid (forced): the money comes back.
    {
        auto t = city();
        const auto till = t.tillOf("smith");
        const auto cash = t.cash(till);
        t.failNextStoreCreate();
        const auto r = t.drawFromStore("smith", "iron_bar", 2);
        expect(!r.ok && t.cash(till) == cash && t.cash(Society::WharfFund) == 0 && Society::stock(*t.account(till), "iron_bar") == 0,
               "a failed create after the shift is paid back: " + r.message);
        expect(t.conserved(), "money conserved");
    }
}

void theFlagFollowsTheBusiness()
{
    // The baker's business is off; it dies and its helper takes the post: still off. The smith's is on; its helper keeps it on.
    auto s = city([](AuthoredRoster& r) { r.residents[2].internalStore = false; });
    CareerWorld world;
    world.alive = [](const std::string& id) { return id != "baker" && id != "smith"; };
    world.age = [](const std::string&) { return 30; };
    world.regard = [](const std::string&, const std::string&) { return 50.0; };
    s.died("baker", 1);
    s.died("smith", 1);
    expect(s.appoint("job:baker", "helper", world, 1).kind == "succession", "the helper takes the bakery");
    expect(s.appoint("job:smith", "helper2", world, 1).kind == "succession", "and the other the smithy");
    expect(s.tillOf("helper") == "till:job:baker", "the bakery's till is the helper's to keep");
    expect(s.storeClosed("helper") == "off for this business", "the bakery's switch followed the post: " + s.storeClosed("helper"));
    expect(s.drawFromStore("helper2", "iron_bar", 1).ok, "the smithy's new keeper draws");
    expect(!s.storeOpen("baker") && !s.storeOpen("smith"), "and the dead keep no shop");
}

void theDefaults()
{
    auto s = city();
    const auto& e = s.authored().economy;
    expect(e.internalStore && e.storeMarkup == 100 && e.storeResale && e.resaleMarkup == 75 && !e.storeRule, "on, 100, on, 75, rule off");
    for (const auto& r : s.authored().residents)
        expect(r.internalStore, "every business on: " + r.id);
}

// A tiny world from files: one outdoor cell, the people, and whatever records `extra` adds.
std::map<std::string, std::string> files(const std::string& extra, const std::string& afterPeople = "")
{
    std::ostringstream cell;
    cell << "id: town\nname: Town\ndescription: A town.\nworld: 0 0 0\nsize: 24 24\noutdoors: true\nweather: clear\ngrid:\n";
    for (int y = 0; y < 24; ++y)
        cell << std::string(24, '.') << '\n';
    const auto resident = [](const std::string& id, const std::string& role, const std::string& label, const std::string& home, double x) {
        std::ostringstream line;
        line << "resident \"" << id << "\" \"" << id << "\" \"" << role << "\" \"" << label << "\" \"Someone.\" \"Hello.\" 38 \"timber\" \"female\" "
             << "\"average\" \"piebald\" 4 3 0 8 1 8 17 \"-\" 30 0 1 ";
        if (home.empty())
            line << "\"-\" 0 0 \"-\" 0 0 \"-\" 0 0\n";
        else
            line << '"' << home << "\" " << x << " 5.5 \"" << home << "\" " << x << " 8.5 \"" << home << "\" " << x << " 11.5\n";
        return line.str();
    };
    std::string manifest = "RATW_WORLD 2\ncell \"town\" \"cells/town.cell\"\nspawn \"town\" 12.5 12.5\neconomy 1000 100 50 10 12\n" + extra +
                           resident("smith", "merchant", "the smith at the forge", "town", 3.5) +
                           resident("stall", "merchant", "keeping a food stall", "town", 9.5) +
                           resident("wanderer", "merchant", "the smith at the far forge", "", 0) + afterPeople;
    return {{"world.ratw", manifest}, {"cells/town.cell", cell.str()}};
}

void theRecords()
{
    {
        World w;
        const auto r = w.loadWorldFiles(files(""), "test");
        expect(r.ok, "a world without store records loads: " + r.message);
        const auto& e = w.society().authored().economy;
        expect(e.internalStore && e.storeMarkup == 100 && e.storeResale && e.resaleMarkup == 75 && !e.storeRule, "with the defaults");
        expect(w.society().storeOpen("smith"), "and the smith may draw");
    }
    {
        World w;
        const auto r = w.loadWorldFiles(files("store off 40 resale on 0 rule on\n", "nostore \"stall\"\nnostore \"wanderer\"\n"), "test");
        expect(r.ok, "a store record and nostore records load: " + r.message);
        const auto& e = w.society().authored().economy;
        expect(!e.internalStore && e.storeMarkup == 40 && e.storeResale && e.resaleMarkup == 0 && e.storeRule, "the record is read");
        expect(w.society().storeClosed("stall") == "off for this business", "the stall is off: " + w.society().storeClosed("stall"));
        expect(w.society().storeClosed("smith") == "off for the whole world", "and the smith's making with the world");
        expect(w.society().unplaced("wanderer") && w.society().unplaced("wanderer")->internalStore, "a nostore for an unplaced founder is set aside");
    }
    const auto refused = [](const std::string& extra, const std::string& after, const std::string& why) {
        World w;
        const auto r = w.loadWorldFiles(files(extra, after), "test");
        expect(!r.ok, why + ": refused");
    };
    refused("store on 100 resale on 75 rule off\nstore on 100 resale on 75 rule off\n", "", "a duplicate store record");
    refused("store maybe 100 resale on 75 rule off\n", "", "a store switch that isn't on or off");
    refused("store on 1001 resale on 75 rule off\n", "", "a markup over 1,000");
    refused("store on 100 resale on -1 rule off\n", "", "a negative markup");
    refused("store on 100 resale on 75\n", "", "a short store record");
    refused("store on 100 resale on 75 rule off extra\n", "", "a long store record");
    refused("", "nostore \"nobody\"\n", "a nostore for an unknown ID");
    refused("", "nostore \"stall\"\nnostore \"stall\"\n", "a duplicate nostore");
    // A streamed build's manifest keeps its own store records out, so the people's manifest brings them once.
    const auto build = files("store on 50 resale on 75 rule off\n", "nostore \"stall\"\n").at("world.ratw");
    const auto stripped = game::Game::withoutPeople(build);
    expect(stripped.find("store ") == std::string::npos && stripped.find("nostore") == std::string::npos, "the build's store records are stripped");
    const auto people = std::string("economy 1000 100 50 10 12\nstore on 50 resale on 75 rule off\n") +
                        build.substr(build.find("resident \"smith\"")) ;
    std::istringstream lines(stripped + people);
    int stores = 0;
    for (std::string line; std::getline(lines, line);)
        stores += line.rfind("store ", 0) == 0;
    expect(stores == 1, "and the two together hold one store record");
    World w;
    auto both = files("");
    both["world.ratw"] = stripped + people;
    const auto r = w.loadWorldFiles(both, "test");
    expect(r.ok && w.society().authored().economy.storeMarkup == 50 && w.society().storeClosed("stall") == "off for this business",
           "and load: " + r.message);
}

std::map<std::string, LifeBody> atWork(const Society& s)
{
    std::map<std::string, LifeBody> bodies;
    for (const auto& r : s.authored().residents)
        bodies[r.id] = {r.work.cell, r.work.x, r.work.y};
    return bodies;
}

void theRestockingRule()
{
    for (const bool on : {false, true})
    {
        auto s = city([&](AuthoredRoster& r) { r.economy.storeRule = on; });
        LifeDay day;
        day.communityOf = [](const std::string&) { return std::string("town"); };
        s.setDay(day);
        s.addPlayer("player-ada");
        s.shift("treasury", "player-ada", "", 0, 300, "test: a purse");
        const auto bakery = s.tillOf("baker"), stall = s.tillOf("stall");
        const int bread = Society::stock(*s.account(bakery), "bread");
        expect(bread == Society::GoodsKept, "the bakery starts with its loaves");
        expect(s.trade("player-ada", "baker", "bread", 3, true).ok, "Ada buys three loaves");
        const int flour = Society::stock(*s.account(stall), "flour");
        expect(s.trade("player-ada", "stall", "flour", 2, true).ok, "and two flour");
        double now = 10. / 24;
        const auto bodies = atWork(s);
        for (int i = 0; i < 120; ++i)
            s.tick(1, now += 1. / calendar::SecondsPerDay, 0, bodies);
        const int loaves = Society::stock(*s.account(bakery), "bread");
        const int sacks = Society::stock(*s.account(stall), "flour");
        if (!on)
        {
            expect(loaves == bread - 3 && sacks == flour - 2 && s.cash(Society::WharfFund) == 0, "rule off: nothing restocked, nothing drawn");
            continue;
        }
        expect(loaves == bread - 3 + 6, "rule on: the baker made one batch (" + std::to_string(loaves) + " loaves)");
        expect(sacks == flour, "and the stall drew back up to its starting stock (" + std::to_string(sacks) + ")");
        expect(s.cash(Society::WharfFund) > 0 && s.conserved(), "paid into the fund, money conserved");
        const auto fund = s.cash(Society::WharfFund);
        for (int i = 0; i < 120; ++i)
            s.tick(1, now += 1. / calendar::SecondsPerDay, 0, bodies);
        expect(s.cash(Society::WharfFund) == fund, "at most once a game hour");
        const auto spot = s.authored().residents[2].work;
        (void)spot;
    }
    // A till that can't pay for all its shelf lacks draws what it can pay for (doc 72, after country3b: all or nothing
    // left a poor vintner without wine for good, and its vineyards without a buyer).
    {
        auto s = city([](AuthoredRoster& r) { r.economy.storeRule = true; });
        LifeDay day;
        day.communityOf = [](const std::string&) { return std::string("town"); };
        s.setDay(day);
        s.addPlayer("player-ada");
        s.shift("treasury", "player-ada", "", 0, 300, "test: a purse");
        const auto stall = s.tillOf("stall");
        const int flour = Society::stock(*s.account(stall), "flour");
        expect(s.trade("player-ada", "stall", "flour", 4, true).ok, "Ada buys four flour");
        const auto taken = s.cash(stall) - 5;
        expect(s.shift(stall, "treasury", "", 0, taken, "test: emptied"), "the stall's till down to 5p");
        const auto money = s.moneySupply();
        double now = 10. / 24;
        const auto bodies = atWork(s);
        for (int i = 0; i < 120; ++i)
            s.tick(1, now += 1. / calendar::SecondsPerDay, 0, bodies);
        const int drawn = Society::stock(*s.account(stall), "flour") - (flour - 4);
        expect(drawn >= 1 && drawn < 4, "the stall drew what its 5p paid for: " + std::to_string(drawn) + " of the 4 it lacked");
        expect(s.cash(stall) >= 0 && s.cash(stall) < 5 && s.conserved() && s.moneySupply() == money, "its till spent, money conserved");
    }
}

void theFundInASave()
{
    auto s = city();
    expect(s.drawFromStore("smith", "iron_bar", 4).ok, "a draw");
    const auto fund = s.cash(Society::WharfFund);
    const auto saved = wire::readSociety(wire::society(s.state()));
    auto again = city();
    expect(again.restore(saved) && again.cash(Society::WharfFund) == fund && again.conserved(), "a save keeps the fund");
    auto older = wire::society(city().state());
    auto old = json::Value::object();
    for (const auto& [key, v] : older.fields())
        if (key != "accounts")
            old.add(key, v);
        else
        {
            auto accounts = json::Value::object();
            for (const auto& [id, a] : v.fields())
                if (id != Society::WharfFund)
                    accounts.add(id, a);
            old.add(key, accounts);
        }
    const auto read = wire::readSociety(old);
    expect(!read.accounts.count(Society::WharfFund), "an older save has no fund");
    auto fresh = city();
    expect(fresh.restore(read) && fresh.account(Society::WharfFund) && fresh.cash(Society::WharfFund) == 0, "and gets an empty one");
}

std::string readFile(const std::string& path)
{
    std::ifstream in(path, std::ios::binary);
    std::ostringstream all;
    all << in.rdbuf();
    if (!in.good() && !in.eof())
        throw std::runtime_error("cannot read " + path);
    return all.str();
}

void theSharedFixture()
{
    json::Value fixture;
    std::string error;
    expect(json::parse(readFile(std::string(RATW_SOURCE_DIR) + "/Tests/fixtures/business_labels.json"), fixture, error), "the fixture reads: " + error);
    const auto& labels = fixture.array("labels");
    expect(labels.size() > 20, "with its labels");
    for (const auto& row : labels)
    {
        const auto* business = items::businessFor(row.string("label"));
        const auto want = row["business"].isNull() ? std::string() : row.string("business");
        expect((business ? business->id : std::string()) == want, "\"" + row.string("label") + "\" is " + (want.empty() ? "no business" : want) +
                                                                     ", not " + (business ? business->id : "none"));
    }
    AuthoredRoster roster;
    double x = 2.5;
    for (const auto& k : fixture.array("keepers"))
        roster.residents.push_back(person(k.string("keeper"), k.string("label"), {"shops", x += 2, 2.5}));
    Society s(Roster::None);
    s.configure(roster);
    for (const auto& k : fixture.array("keepers"))
    {
        std::vector<std::string> want;
        for (const auto* part : {"making", "resale"})
            for (const auto& item : k.array(part))
                want.push_back(item.asString());
        std::string got;
        for (const auto& g : s.storeGoods(k.string("keeper")))
            got += g + " ";
        expect(s.storeGoods(k.string("keeper")) == want, k.string("keeper") + "'s store goods as Python reckons them; Core has: " + got);
        for (const auto& item : k.array("making"))
            expect(s.storeUse(k.string("keeper"), item.asString()) == Society::StoreUse::Make, item.asString() + " is a making good");
    }
}
} // namespace

int main()
{
    try
    {
        drawingFromTheStore();
        freeAtMarkupZero();
        refusals();
        theFlagFollowsTheBusiness();
        theDefaults();
        theRecords();
        theRestockingRule();
        theFundInASave();
        theSharedFixture();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Store room tests passed: " << checks << " checks.\n";
    return 0;
}
