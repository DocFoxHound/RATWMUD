// The base the economy's rules were erased to (Docs/Design/66-worlds-atlas-and-dm.md, Phase 5): the catalogue's goods
// and qualities, shops that hold stock at catalogue prices in their tills, players buying from them and selling to
// them, money conserved; and residents who go about their days with nothing bought, made, paid or eaten up, their
// hunger paused (EconomySpec::needsPaused, doc 68's switch). An older save, with the economy's state in it, still loads.
#include "RatwCalendar.h"
#include "RatwItems.h"
#include "RatwJsonDoc.h"
#include "RatwSociety.h"
#include "RatwWire.h"

#include <cmath>
#include <iostream>
#include <limits>
#include <map>
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

// A baker, a tanner, a smith, and two townsfolk.
Society town(std::int64_t treasury = 500)
{
    AuthoredRoster roster;
    roster.residents = {person("baker", "baker at The Loaf", {"bakery", 2.5, 2.5}), person("tanner", "tanner at The Hide Yard", {"yard", 2.5, 2.5}),
                        person("smith", "the smith at the forge", {"forge", 2.5, 2.5}), person("farmer", "farms the valley fields", {"fields", 4.5, 4.5}, "civilian"),
                        person("idler", "-", {"square", 3.5, 3.5}, "civilian")};
    roster.economy.treasury = treasury;
    roster.economy.needsPaused = true;              // (Phase 5's pause, a switch since doc 68: the loop is country_tests'.)
    Society s(Roster::None);
    s.configure(roster);
    return s;
}

std::map<std::string, LifeBody> atWork(const Society& s)
{
    std::map<std::string, LifeBody> bodies;
    for (const auto& r : s.authored().residents)
        bodies[r.id] = {r.work.cell, r.work.x, r.work.y};
    return bodies;
}

void theCatalogue()
{
    // The catalog's kinds (doc 35, Part 4).
    const auto* fine = items::good("hide~fine");
    const auto* plain = items::good("hide");
    expect(fine && plain && fine->price == int(std::lround(plain->price * 1.6)) && fine->name == "Fine hide", "a fine hide: dearer, and named so");
    expect(items::good("hide~crude") && items::good("hide~masterwork") && !items::good("hide~shoddy"), "crude, fine, masterwork; nothing else");
    expect(!items::good("meal~fine") && !items::good("herbs~fine"), "meals and herbs have no qualities");
    expect(items::good("sword~fine") && items::good("sword~fine")->name == "Fine bronze sword", "swords do: a fine bronze sword");
    expect(items::qualityDamage(2) > 1 && items::qualityDamage(0) < 1, "a finer blade cuts deeper");
    expect(items::good("sword~fine")->durability > items::good("sword")->durability &&
               items::good("sword~crude")->durability < items::good("sword")->durability, "and lasts longer");
    expect(items::makerOf("hide~masterwork@sorrel") == "sorrel" && items::baseOf("hide~masterwork@sorrel") == "hide" &&
               items::qualityOf("hide~masterwork@sorrel") == 3 && items::good("hide~masterwork@sorrel"), "a mark is read, and the good found");
    expect(items::withMaker("hide~masterwork", "sorrel") == "hide~masterwork@sorrel", "and made");
    const auto* vest = items::wearable("quilted_vest");
    const auto* fineVest = items::wearable("quilted_vest~fine");
    expect(vest && fineVest && fineVest->protect > vest->protect && fineVest->slot == vest->slot, "fine armour protects more");
    expect(!items::craftsFor("bakery").empty() && items::producerFor("farms the valley fields"), "the crafts and producers are still the catalogue's");
    expect(Society::catalogPrice("hide~fine") == fine->price && Society::catalogPrice("meal") == 6 && Society::catalogPrice("herbs") == 2,
           "every shop's price is the catalogue's");
}

void shopsHoldTheirStock()
{
    auto s = town();
    const auto till = s.tillOf("baker");
    expect(till.rfind("till:", 0) == 0 && s.account(till), "the bakery keeps a till of its own: " + till);
    const auto wares = s.wares("baker");
    int stocked = 0;
    for (const auto& ware : wares)
        stocked += Society::stock(*s.account(till), ware) > 0;
    expect(stocked > 0, "with its wares on its shelves from the start");
    expect(s.account(till)->cash == 200 && s.account("baker")->cash == 200, "its money its founder's purse's worth, made; the keeper's own kept");
    expect(s.conserved(), "money is conserved");
    // A player buys at the catalogue's price, and sells at SellBackShare of it.
    s.addPlayer("player-ada");
    const auto start = s.account("player-ada")->cash;
    std::string ware;
    for (const auto& w : wares)
        if (w != "meal" && Society::stock(*s.account(till), w) > 0)
            ware = w;
    expect(!ware.empty(), "something besides meals on the bakery's shelves");
    const auto bought = s.trade("player-ada", "baker", ware, 1, true);
    expect(bought.ok && bought.unitPrice == Society::catalogPrice(ware) && s.account("player-ada")->cash == start - bought.unitPrice,
           "a player buys one at the catalogue's price: " + bought.message);
    s.create("player-ada", "hide~fine", 2, "test: hunted");
    const auto fine = s.quote("player-ada", "tanner", "hide~fine", 1, false);
    const auto common = s.quote("player-ada", "tanner", "hide", 1, false);
    expect(fine.ok && fine.unitPrice == std::llround(Society::catalogPrice("hide~fine") * Society::SellBackShare) &&
               fine.unitPrice > common.unitPrice,
           "a tanner buys a fine hide at a little over half its price, dearer than a common one: " + fine.message);
    const auto sold = s.trade("player-ada", "tanner", "hide~fine", 2, false);
    expect(sold.ok && Society::stock(*s.account(s.tillOf("tanner")), "hide~fine") == 2, "sold, onto the tanner's shelves: " + sold.message);
    expect(!s.quote("player-ada", "tanner", "hide~fine", 99, false).ok, "nobody sells what they haven't got");
    expect(s.conserved(), "money is conserved");
}

// No cap on what a single piece costs (the user, 2026-10-30): a dear price is paid when the buyer can pay; only a total
// that would overflow is refused.
void noCapOnAPiece()
{
    auto s = town(100000);
    s.addPlayer("player-ada");
    expect(s.shift("treasury", "player-ada", "", 0, 20000, "test: a purse") && s.create("smith", "sword", 3, "test: forged"),
           "Ada has 20,000p and the smith three swords");
    const auto before = s.account("player-ada")->cash;
    expect(s.sale("smith", "player-ada", "sword", 1, 12000, "test: dear") && s.account("player-ada")->cash == before - 12000,
           "a piece at 12,000p, paid: she can pay");
    expect(!s.sale("smith", "player-ada", "sword", 2, 12000, "test: dearer"), "not what she can't pay");
    expect(!s.sale("smith", "player-ada", "sword", 2, std::numeric_limits<std::int64_t>::max() / 2 + 1, "test: overflow"),
           "a price whose total would overflow is refused");
    expect(s.conserved(), "money is conserved");
}

void residentsGoAboutTheirDays()
{
    auto s = town();
    LifeDay day;
    day.communityOf = [](const std::string&) { return std::string("town"); };
    s.setDay(day);
    auto before = s.state();
    double now = 10. / 24;                          // A working morning.
    const auto bodies = atWork(s);
    s.tick(1, now, 0, bodies);
    s.takeJournal();
    for (int i = 0; i < int(1.5 * calendar::SecondsPerDay); ++i)   // A day and a half, a second at a time.
    {
        now += 1. / calendar::SecondsPerDay;
        s.tick(1, now, 0, bodies);
    }
    expect(s.conserved(), "money is conserved");
    const auto journal = s.takeJournal();
    expect(journal.empty(), "nobody bought, made, paid or used anything (" + std::to_string(journal.size()) + " entries)");
    for (const auto& [id, a] : s.state().accounts)
        expect(before.accounts.count(id) && before.accounts.at(id).cash == a.cash && before.accounts.at(id).stock == a.stock,
               "every account as it was: " + id);
    for (const auto& [id, life] : s.state().residents)
        expect(life.hunger == before.residents.at(id).hunger, "hunger paused: " + id + " at " + std::to_string(life.hunger));
    // At work in their hours, asleep at night.
    auto at = [&](double hour) {
        double t = std::floor(now) + 1 + hour / 24;
        for (int i = 0; i < 3; ++i)
            s.tick(1, t += 1. / calendar::SecondsPerDay, 0, bodies);
    };
    at(10);
    expect(s.resident("baker")->task == "trade", "the baker keeps shop in its hours: " + s.resident("baker")->task);
    expect(s.resident("farmer")->task == "farms the valley fields", "the farmer at its post: " + s.resident("farmer")->task);
    expect(s.resident("idler")->task == "looking for work", "one without a post spends the day out of work: " + s.resident("idler")->task);
    at(23);
    expect(s.resident("baker")->task == "sleep" && s.resident("farmer")->task == "sleep", "and everyone sleeps at night");
}

void anOlderSaveLoads()
{
    // A save from before doc 66's Phase 5 carries the economy's state (books, houses, memory, the monitor, the crafting
    // grant): it is read past, and the rest loads.
    auto s = town();
    auto saved = wire::society(s.state());
    auto old = json::Value::object();
    for (const auto& [key, v] : saved.fields())
        old.add(key, v);
    old.add("craftingStocked", 3);
    old.add("books", json::Value::object());
    old.add("houses", json::Value::object());
    old.add("memory", json::Value::object());
    old.add("orchestrator", json::Value::object());
    const auto read = wire::readSociety(old);
    expect(read.minted >= 0 && read.accounts.size() == s.state().accounts.size(), "the older save is read");
    expect(s.restore(read), "and restores");
    const auto again = wire::readSociety(saved);
    expect(again.minted >= 0, "and a save of now reads too");
}
} // namespace

int main()
{
    try
    {
        theCatalogue();
        shopsHoldTheirStock();
        noCapOnAPiece();
        residentsGoAboutTheirDays();
        anOlderSaveLoads();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Shops tests passed: " << checks << " checks.\n";
    return 0;
}
