// The households' life with their bands (Docs/Design/79-a-staged-economy.md, sections 6.5-6.7, 9 and 16.2; Phase 2): a
// household's members share its band (its pups among them); losing a post lowers its score; food from the shelf, a bare
// shelf and short days (a comfortable household never goes hungry, a destitute one does on its short days); holds, lean
// days and the goods trim; the faucet door and real money balanced; staged payers topped up; the welcome once per account
// holder per world per game week (D23: an exploit test); the staged day settled across a restart; the Mind's Means.
// Seeded and offline; the catalogue is the real Data/Items (RATW_SOURCE_DIR).
#include "RatwCalendar.h"
#include "RatwItems.h"
#include "RatwJournal.h"
#include "RatwJsonDoc.h"
#include "RatwMarket.h"
#include "RatwSociety.h"
#include "RatwStatus.h"
#include "RatwWire.h"

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <set>
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

ResidentSpec person(const std::string& id, const std::string& role, const std::string& work, int age, Spot home, Spot job)
{
    ResidentSpec r;
    r.id = id;
    r.name = id;
    r.role = role;
    r.workLabel = work;
    r.age = age;
    r.appearance.sex = id.size() % 2 ? "male" : "female";
    r.home = home;
    r.work = job;
    r.evening = home;
    r.serve = job;
    r.purse = 40;
    return r;
}
// The docks (as people_flow's): two dockers and their pup in the Sump's rents, a stall keeper, a docker couple, the
// Heights' lady; with the staged economy on.
AuthoredRoster docks()
{
    AuthoredRoster roster;
    roster.residents = {person("ivar", "civilian", "unloading ships at the piers", 40, {"sump_rents", 2.5, 2.5}, {"piers", 3.5, 3.5}),
                        person("una", "civilian", "unloading ships at the piers", 38, {"sump_rents", 3.5, 2.5}, {"piers", 4.5, 3.5}),
                        person("tam", "civilian", "at lessons", 12, {"sump_rents", 4.5, 2.5}, {"school", 2.5, 2.5}),
                        person("moss", "merchant", "keeping the stall", 45, {"stall_up", 2.5, 2.5}, {"stall", 2.5, 4.5}),
                        person("birch", "civilian", "unloading ships at the piers", 32, {"ash_house", 2.5, 2.5}, {"piers", 5.5, 3.5}),
                        person("wren", "civilian", "unloading ships at the piers", 30, {"ash_house", 3.5, 2.5}, {"piers", 6.5, 3.5}),
                        person("lady", "civilian", "at home in the Heights", 50, {"vance_house", 2.5, 2.5}, {"vance_house", 2.5, 2.5})};
    roster.kin = {{"ivar", "una", "spouse"}, {"ivar", "tam", "parent"}, {"una", "tam", "parent"}, {"birch", "wren", "spouse"}};
    return roster;
}
CareerWorld careers(const Society& s)
{
    CareerWorld w;
    w.alive = [](const std::string&) { return true; };
    w.age = [&s](const std::string& id) { const auto* r = s.spec(id); return r ? r->age : 30; };
    w.regard = [](const std::string&, const std::string&) { return 0.0; };
    return w;
}
double food(const Society& s, const std::string& id)
{
    double n = 0;
    for (const auto& [item, count] : s.account(id)->stock)
        n += double(Society::nourishment(item)) * count;
    return n;
}
void emptyLarder(Society& s, const std::string& id)
{
    const auto copy = s.account(id)->stock;
    for (const auto& [item, c] : copy)
        if (c > 0 && Society::nourishment(item) > 0)
            s.consume(id, item, c, "test: emptied");
}
json::Value parse(const std::string& text)
{
    json::Value v;
    std::string error;
    if (!json::parse(text, v, error))
        throw std::runtime_error("bad test json: " + error);
    return v;
}
Society docksSociety()
{
    Society s(Roster::None);
    s.configure(docks());
    s.tendCareers(1.3, careers(s));
    return s;
}
// Who is dead, for the careers (an estate settled, a post emptied).
struct Fates
{
    std::set<std::string> dead;
    CareerWorld world(const Society& s)
    {
        CareerWorld w;
        w.alive = [this](const std::string& id) { return !dead.count(id); };
        w.age = [&s](const std::string& id) { const auto* r = s.spec(id); return r ? r->age : 30; };
        w.regard = [](const std::string&, const std::string&) { return 0.0; };
        return w;
    }
};
// The docks with the Country beside them (as people_flow's): the pup grown (17) or not there, and the unplaced of three
// classes (a fisher and a servant lower, a town's baker middle, a Heights cousin upper).
ResidentSpec away(const std::string& id, const std::string& role, const std::string& work, int age, std::int64_t purse)
{
    auto r = person(id, role, work, age, {}, {});
    r.purse = purse;
    r.description = "A wolf of the Country.";
    return r;
}
AuthoredRoster docksAndCountry(bool grownPup)
{
    auto roster = docks();
    for (auto it = roster.residents.begin(); it != roster.residents.end(); ++it)
        if (it->id == "tam")
        {
            if (grownPup)
                it->age = 17;
            else
                roster.residents.erase(it);
            break;
        }
    if (!grownPup)
        roster.kin.erase(std::remove_if(roster.kin.begin(), roster.kin.end(), [](const auto& k) { return k.b == "tam" || k.a == "tam"; }),
                         roster.kin.end());
    roster.unplaced = {away("cty_fisher", "civilian", "fishes the waters off Greywater", 30, 49),
                       away("rm_servant", "civilian", "in service to the Ashbys", 31, 36),
                       away("tn_baker", "merchant", "baker at Downs Bakery", 33, 86),
                       away("rm_cousin", "civilian", "at home in the Heights", 46, 97)};
    return roster;
}
bool hasEvent(const Society& s, const std::string& id, const std::string& kind)
{
    const auto found = s.state().staged.events.find(id);
    return found != s.state().staged.events.end() &&
           std::any_of(found->second.begin(), found->second.end(), [&](const auto& e) { return e.kind == kind; });
}

void householdsShareABand()
{
    auto s = docksSociety();
    s.stagedBands(2, true);
    for (const auto& [id, life] : s.state().residents)
    {
        const auto* st = s.statusOf(id);
        expect(st != nullptr, id + " has a status");
        for (const auto& [other, l2] : s.state().residents)
            if (s.householdOf(other) == st->household)
                expect(s.statusOf(other)->band == st->band, "a household's members share its band: " + id + ", " + other);
    }
    expect(s.householdOf("tam") == s.householdOf("ivar"), "the pup is in its parents' household, and takes its band");
    // Losing a post lowers the household's score (the lost post noted at the next pass).
    const double before = s.statusOf("birch")->score;
    s.died("tam", 3.1);                             // (The pup's place at lessons empty: birch put there, his post gone.)
    expect(s.appoint("job:tam", "birch", careers(s), 3.2).kind == "succession", "birch moved to no post that pays");
    s.stagedBands(3, true);
    expect(s.jobOf("birch") && s.postClass(*s.jobOf("birch")).empty(), "birch has lost his post");
    expect(s.statusOf("birch")->score < before - .5 && s.statusOf("birch")->reason,
           "losing a post lowers the score and is a reason: " + std::to_string(before) + " -> " + std::to_string(s.statusOf("birch")->score));
}

void foodFromTheShelf()
{
    // Three days with bodies (each at home, unseen: they eat as they grow hungry). The lady's household comfortable, the
    // dockers' destitute with every day short: the comfortable never go hungry, the destitute do.
    auto s = docksSociety();
    s.setStatusOverride(std::make_unique<json::Value>(parse(R"({"rows":{"destitute":{"short":100}}})")).get());
    s.stagedBands(2, true);
    s.overrideBand(s.householdOf("ivar"), status::Destitute, 100);
    s.overrideBand(s.householdOf("lady"), status::Comfortable, 100);
    for (const auto& id : {"ivar", "una", "tam", "lady"})
        emptyLarder(s, id);
    std::map<std::string, LifeBody> bodies;
    for (const auto& r : s.authored().residents)
    {
        LifeBody b;
        b.cell = r.home.cell;
        b.x = r.home.x;
        b.y = r.home.y;
        b.age = r.age;
        b.unseen = true;
        bodies[r.id] = b;
    }
    double ladyMost = 0, ivarMost = 0;
    for (int sec = 0; sec < 3 * 14400; sec += 10)
    {
        s.tick(10., 2 + 8. / 24 + sec / 14400., 0, bodies);
        if (std::getenv("LIFE_DEBUG") && sec % 3600 == 0)
            std::cerr << sec / 14400. << " ivar " << s.resident("ivar")->hunger << " food " << food(s, "ivar") << " band "
                      << s.statusOf("ivar")->band << " lady " << s.resident("lady")->hunger << " " << food(s, "lady") << "\n";
        ladyMost = std::max(ladyMost, s.resident("lady")->hunger);
        ivarMost = std::max(ivarMost, s.resident("ivar")->hunger);
    }
    expect(ladyMost < 70, "a comfortable household never goes hungry: " + std::to_string(ladyMost));
    expect(ivarMost >= 70, "a destitute household goes hungry on its short days: " + std::to_string(ivarMost));
    // A bare shelf (the Board's hook, here the till's own stock): the poor it serves go short; the comfortable don't. (The
    // stall's meals, the poor's first food it sets out: it sells no bread, so loaves it held would make it no bread shop.)
    auto shop = docksSociety();
    shop.setStagedHooks({nullptr, nullptr, &Society::tillShelf});
    const auto till = shop.tillOf("moss");
    for (const auto& [item, n] : std::map<std::string, int>(shop.account(till)->stock))
        if (n > 0 && Society::nourishment(item) > 0)
            shop.consume(till, item, n, "test: the shelf cleared");
    expect(!till.empty() && shop.create(till, "meal", 6, "test: the shelf"), "the stall's shelf holds 6 meals");
    shop.stagedBands(2, true);
    shop.overrideBand(shop.householdOf("ivar"), status::Poor, 100);
    for (const auto& id : {"ivar", "una", "tam"})
        emptyLarder(shop, id);
    for (int h = 0; h < 24; ++h)
        shop.tick(1., 3 + h / 24. + 1e-4, 0, {});
    const double fedPoor = food(shop, "ivar") + food(shop, "una") + food(shop, "tam");
    expect(Society::stock(*shop.account(till), "meal") < 6, "the household's meals came off the shelf: " +
                                                                std::to_string(Society::stock(*shop.account(till), "meal")));
    expect(fedPoor <= 6 * Society::nourishment("meal") + 1e-9, "the poor fed only from what the shelf held: " + std::to_string(fedPoor));
    expect(food(shop, "lady") >= Society::NourishADay, "the comfortable fed from elsewhere when the shelf is bare");
    const auto provisions = shop.takeProvisions();
    expect(std::any_of(provisions.begin(), provisions.end(), [](const auto& p) { return p.second == "moss"; }),
           "the provision's familiarity step between a member and the keeper (no WorldEvent)");
}

void playersEmptyTheShelf()
{
    // The two phases joined (doc 79, 6.5, 7.5, 7.7, T2): with the Board's own hooks, the morning restock sets the stall's
    // meals out to a target that covers its households' provision as well as players; a player who buys the shelf bare
    // leaves the poor household it serves short today. The keeper's buying budget follows its band. The trade is counted
    // once at the door.
    struct Run
    {
        int provision = 0, target = 0, onShelf = 0;
        double fedPoor = 0, fedLady = 0;
        std::int64_t paid = 0, sunk = 0, budget = 0, budgetRich = 0, budgetExpected = 0;
    };
    const auto run = [](bool buyOut) {
        Run out;
        auto s = docksSociety();
        s.setStatusOverride(std::make_unique<json::Value>(parse(R"({"rows":{"poor":{"short":0}}})")).get());
        s.setStagedHooks(Society::boardHooks());
        s.setHolder("player-ada", "acct:ada");
        s.addPlayer("player-ada");
        s.stagedBands(2, true);
        s.overrideBand(s.householdOf("ivar"), status::Poor, 100);
        const auto till = s.tillOf("moss");
        out.provision = s.provisionFor(till, "meal");
        out.target = s.shelfTarget(till, "meal");
        const auto* mossBand = s.statusOf("moss");
        out.budget = s.budgetOf(till);
        out.budgetExpected = std::llround(market::data().budget * s.statusTable().rows[std::size_t(mossBand ? mossBand->band : 2)].budget);
        // The morning's restock (the stall's wares set out to their targets: its meals, the poor household's food here, as
        // the stall sells no bread).
        s.setMarketClock(2 + 9. / 24);
        s.stagedRestock("moss");
        out.onShelf = Society::stock(*s.account(till), "meal");
        for (const auto& id : {"ivar", "una", "tam", "lady"})
            emptyLarder(s, id);
        if (buyOut)
        {
            s.shift("treasury", "player-ada", "", 0, 5000, "test purse");
            for (int left; (left = Society::stock(*s.account(till), "meal")) > 0;)
            {
                const auto bought = s.trade("player-ada", "moss", "meal", std::min(99, left), true, "acct:ada");
                if (!bought.ok)
                    break;
                out.paid += bought.total;
            }
            for (const auto& [day, holders] : s.state().staged.faucets)
                if (const auto h = holders.find("acct:ada"); h != holders.end())
                    for (const auto& [reason, t] : h->second)
                        if (reason == "local trade")
                            out.sunk += t.sunk;
        }
        s.stagedFood(s.householdOf("ivar"), {"ivar", "una", "tam"}, "lower");
        s.stagedFood(s.householdOf("lady"), {"lady"}, "upper");
        out.fedPoor = food(s, "ivar") + food(s, "una") + food(s, "tam");
        out.fedLady = food(s, "lady");
        s.overrideBand(s.householdOf("moss"), status::Rich, 100);
        out.budgetRich = s.budgetOf(till);
        return out;
    };
    const auto stocked = run(false);
    expect(stocked.provision > 0, "the stall feeds the poor household: its day's provision counted");
    expect(stocked.target >= stocked.provision && stocked.onShelf >= stocked.provision,
           "the shelf target covers the provision: " + std::to_string(stocked.target) + " for " + std::to_string(stocked.provision));
    expect(stocked.fedPoor >= 3 * 15, "a stocked shelf feeds every member of the poor household: " + std::to_string(stocked.fedPoor));
    expect(stocked.budget == stocked.budgetExpected, "the keeper's buying budget by its band: " + std::to_string(stocked.budget));
    expect(stocked.budgetRich == std::llround(market::data().budget * 1.5), "a rich keeper's budget 1.5 times: " + std::to_string(stocked.budgetRich));
    const auto bare = run(true);
    expect(bare.paid > 0, "the player bought the shelf");
    expect(bare.fedPoor == 0, "the shelf bought bare: the poor household goes short today: " + std::to_string(bare.fedPoor));
    expect(bare.fedLady >= Society::NourishADay, "the comfortable fed from elsewhere");
    expect(bare.sunk == bare.paid, "each purchase counted once at the door: " + std::to_string(bare.sunk) + " of " + std::to_string(bare.paid));
}

void foodShopsTheRestockFills()
{
    // A household's food shop is a till the Board's morning restock fills (doc 79, 6.5 and 7.7: stagedShelfGoods): a
    // ropewalk that only holds meals and loaves (its starting stock, leftovers) is never one, though the stall sells no
    // bread; the stall is. And a recruited keeper's shop still sets out its wares in its hours, so the households it
    // feeds don't go short while it's away.
    auto roster = docks();
    roster.residents.push_back(person("kari", "merchant", "at the rope walk", 36, {"rope_loft", 2.5, 2.5}, {"ropewalk", 2.5, 2.5}));
    Society s(Roster::None);
    s.configure(roster);
    s.tendCareers(1.3, careers(s));
    const auto rope = s.tillOf("kari"), stall = s.tillOf("moss");
    expect(rope != "kari" && s.account(rope) && s.merchant("kari"), "the ropewalk's till");
    expect(s.create(rope, "meal", 12, "test: leftovers") && s.create(rope, "bread", 12, "test: leftovers"), "the ropewalk holds food");
    const auto shelf = s.stagedShelfGoods("kari");
    expect(std::find(shelf.begin(), shelf.end(), "meal") == shelf.end() && std::find(shelf.begin(), shelf.end(), "bread") == shelf.end(),
           "the ropewalk sets out no food");
    s.stagedBands(2, true);
    std::set<std::string> foods;
    for (const auto& row : s.statusTable().rows)
        foods.insert(row.food.begin(), row.food.end());
    int atRope = 0, atStall = 0;
    for (const auto& f : foods)
        atRope += s.provisionFor(rope, f), atStall += s.provisionFor(stall, f);
    expect(atRope == 0, "no household's food shop is the ropewalk: " + std::to_string(atRope));
    expect(atStall > 0, "the stall feeds households: " + std::to_string(atStall));
    // Recruited (a party's, a Hold's): moss's stall still set out today.
    const auto copy = s.account(stall)->stock;
    for (const auto& [item, n] : copy)
        if (n > 0 && Society::nourishment(item) > 0)
            s.consume(stall, item, n, "test: bought bare");
    std::map<std::string, LifeBody> bodies;
    for (const auto& r : s.authored().residents)
    {
        LifeBody b{r.work.cell, r.work.x, r.work.y};
        b.age = r.age;
        b.companion = r.id == "moss";
        bodies[r.id] = b;
    }
    for (int sec = 0; sec < 600; sec += 10)
        s.tick(10., 3 + 10. / 24 + sec / 86400., 0, bodies);
    expect(s.resident("moss")->task == "companion", "moss is away with whoever recruited it: " + s.resident("moss")->task);
    const auto restocked = s.board().restocked.find(stall);
    expect(restocked != s.board().restocked.end() && restocked->second == s.marketDay(),
           "the recruited keeper's stall set out today all the same");
}

void stakesAndTillsAtPayout()
{
    // The table's payout (Game::endTable) puts a player's winnings of residents' stakes through the stakes cap whoever
    // invited them (doc 79, 9.3): of a pot of 50p the residents staked, a player takes at most stakesADay (30p) a day, the
    // rest back to the house. And a till pays what it holds (payable agrees with topUp: a till is never topped up).
    auto s = docksSociety();
    s.setHolder("player-ada", "acct:ada");
    s.addPlayer("player-ada");
    s.stagedBands(2, true);
    const auto cap = s.statusTable().cap("stakesADay");
    expect(s.openAccount("table:t1"), "a table's pot");
    for (const auto& id : {"ivar", "una", "birch", "wren", "moss"})
        expect(s.shift(id, "table:t1", "", 0, 10, "a table stake"), std::string("a resident's stake: ") + id);
    const auto house = s.tillOf("moss");
    const auto houseBefore = s.cash(house);
    const auto won = s.cappedPayout("table:t1", "player-ada", 50, "stakesADay", house);
    expect(won == cap, "the player's winnings of residents' stakes capped at payout: " + std::to_string(won));
    expect(s.cash(house) == houseBefore + 50 - cap, "the rest back to the house");
    expect(s.shift("table:t1", "player-ada", "", 0, won, "a game won") && s.faucetRoom("player-ada", "stakesADay") == 0,
           "the day's stakes room used up");
    expect(s.cappedPayout("table:t1", "player-ada", 0, "stakesADay", house) == 0, "(nothing left in the pot)");
    // A till pays what it holds.
    s.stagedSet(house, 7);
    expect(s.payable(house, 100) == 7, "a till's payable is its cash: " + std::to_string(s.payable(house, 100)));
    expect(s.payable("moss", 100) == 100, "a resident's purse is topped up on demand");
}

void holdsLeanAndTheTrim()
{
    auto s = docksSociety();
    s.addPlayer("player-thief");
    s.stagedBands(2, true);
    for (int h = 0; h < 2; ++h)
        s.tick(1., 2 + h / 24. + 1e-4, 0, {});
    // A theft: the victim robbed (an event), lean, held; not refilled tonight.
    const auto before = s.cash("una");
    expect(s.shift("una", "player-thief", "", 0, 3, "stolen"), "a theft");
    expect(s.leanNow("una") && s.heldNow("una"), "the victim lean and held");
    // A gift from a player: helped, held.
    s.shift("player-thief", "wren", "", 0, 5, "a gift");
    expect(s.heldNow("wren"), "a gift's recipient held");
    // Goods in a staged wallet trimmed nightly: food above two days, other goods above the band's.
    s.create("birch", "bread", 60, "test: a hoard");
    s.create("birch", "iron_ore", 40, "test: a hoard");
    const auto wrenCash = s.cash("wren");
    for (int h = 2; h <= 26; ++h)
        s.tick(1., 2 + h / 24. + 1e-4, 0, {});
    expect(s.cash("una") == before - 3, "a lean, held purse not refilled: " + std::to_string(s.cash("una")));
    expect(s.cash("wren") == wrenCash, "a held purse keeps its change");
    const auto& row = s.statusTable().rows[std::size_t(s.statusOf("birch")->band)];
    expect(food(s, "birch") <= 3 * Society::NourishADay + 1e-9, "a hoard of bread trimmed: " + std::to_string(food(s, "birch")));
    expect(Society::stock(*s.account("birch"), "iron_ore") <= row.goodsMost, "a hoard of ore trimmed to the band's");
    // The event weighs on the score while it lasts.
    expect(s.statusOf("una")->score < s.statusOf("ivar")->score + 1e-9 || s.householdOf("una") == s.householdOf("ivar"),
           "a theft weighs on the household");
}

void theDoorAndRealMoney()
{
    auto s = docksSociety();
    s.openRealBalance();
    s.setHolder("player-ada", "account:ada");
    s.addPlayer("player-ada");
    expect(s.realBalanced() && s.realFaucets() == 20, "the welcome counted at the door: " + std::to_string(s.realFaucets()));
    // A reward from a treasury that has nothing: topped up first, paid, counted.
    s.stagedSet("treasury", 0);
    expect(s.shift("treasury", "player-ada", "", 0, 50, "faction mission reward"), "a dry treasury still pays a reward");
    expect(s.realBalanced() && s.faucetToday("account:ada", "reward") == 50, "a reward is a faucet for the holder");
    expect(s.faucetRoom("player-ada", "rewardsADay") == s.statusTable().cap("rewardsADay") - 50, "the rewards cap's room");
    // Escrow: a resident's coins into a contract are staged-sourced; paid to a player, a faucet then; a player's coins
    // into one and out to a resident, a sink for the real part only.
    expect(s.openAccount("contract:k1"), "a contract's escrow");
    s.shift("moss", "contract:k1", "", 0, 10, "reward set aside");
    expect(s.realBalanced(), "staged into escrow: no real money made");
    s.shift("contract:k1", "player-ada", "", 0, 10, "a contract's reward");
    expect(s.realBalanced() && s.faucetToday("account:ada", "reward") == 60, "escrow's staged part a faucet at payout");
    s.shift("player-ada", "contract:k1", "", 0, 7, "reward set aside");
    s.shift("moss", "contract:k1", "", 0, 3, "reward set aside");
    s.shift("contract:k1", "wren", "", 0, 10, "a contract's reward");
    expect(s.realBalanced() && s.realSinks() == 7, "escrow back to the staged side: its real part a sink: " + std::to_string(s.realSinks()));
    // A player's purchase and a sale to a shop (a sale moves goods one way and coins the other).
    const auto till = s.tillOf("moss");
    s.create(till, "bread", 5, "test: the shelf");
    expect(s.sale(till, "player-ada", "bread", 2, 1, "local trade"), "a player buys bread");
    expect(s.realBalanced(), "a purchase is a sink, balanced");
    // A till is never topped up by the door: it pays players from its own budget (Phase 1's), so a dry one can't pay.
    const auto tillCash = s.cash(till);
    s.stagedSet(till, 0);
    expect(!s.shift(till, "player-ada", "", 0, 5, "local trade: the odd pennies") && s.cash(till) == 0,
           "a dry till paying a player: not topped up");
    s.stagedSet(till, tillCash);
    // Many random moves: real money always balanced.
    s.addPlayer("player-bo");
    const std::vector<std::string> ids{"player-ada", "player-bo", "moss", "wren", "treasury", "contract:k1", till};
    for (int i = 0; i < 2000; ++i)
    {
        const auto& a = ids[status::hash("a" + std::to_string(i)) % ids.size()];
        const auto& b = ids[status::hash("b" + std::to_string(i)) % ids.size()];
        if (a != b)
            s.shift(a, b, "", 0, std::int64_t(status::hash("c" + std::to_string(i)) % 9), "test move " + std::to_string(i % 7));
        expect(s.realBalanced(), "real money balanced after move " + std::to_string(i));
    }
    // The gate's point (doc 79, 9.4): a penny made from nothing in a player's purse, or in escrow's real part, by any path
    // the door doesn't see (a raw set here, as a duplication bug would), fails it; set back, it holds again.
    const auto ada = s.cash("player-ada");
    s.stagedSet("player-ada", ada + 1);
    expect(!s.realBalanced(), "a penny made from nothing in a player's purse is caught");
    s.stagedSet("player-ada", ada);
    expect(s.realBalanced(), "and set back, balanced again");
    s.stagedSet("contract:k1", s.cash("contract:k1") + 1);
    expect(!s.realBalanced(), "a penny made from nothing in escrow is caught");
}

void theWelcomeOncePerHolderPerWeek()
{
    // D23 (exploit): character churn on one account is no faucet. The first wolf of an account holder in a game week has
    // the welcome's 20p and herbs; a second that week only its meals; another account's wolf the welcome; a week on, the
    // welcome again. Saved, so a restart doesn't reopen it.
    auto s = docksSociety();
    for (int h = 0; h < 2; ++h)
        s.tick(1., 7 + h / 24. + 1e-4, 0, {});      // (Day 7: the first day of game week 1.)
    s.setHolder("player-a1", "central:alice");
    s.setHolder("player-a2", "central:alice");
    s.setHolder("player-b1", "central:bob");
    s.addPlayer("player-a1");
    s.addPlayer("player-a2");
    s.addPlayer("player-b1");
    const auto welcomed = [&](const std::string& id) { return s.cash(id) == 20 && Society::stock(*s.account(id), "herbs") == 2; };
    expect(welcomed("player-a1"), "the account's first wolf welcomed: " + std::to_string(s.cash("player-a1")));
    expect(s.cash("player-a2") == 0 && Society::stock(*s.account("player-a2"), "herbs") == 0 &&
               Society::stock(*s.account("player-a2"), "meal") == Society::StartingMeals,
           "a second wolf on the same account that week: meals only");
    expect(welcomed("player-b1"), "another account's wolf welcomed");
    expect(s.faucetToday("central:alice", "welcome") == 20 + 2 * Society::catalogPrice("herbs") +
                                                            2 * Society::StartingMeals * Society::catalogPrice("meal"),
           "the welcome's faucet counted once for the holder (and the meals of both)");
    // Across a restart: still once.
    const auto state = wire::readSociety(parse(json::dump(wire::society(s.state()))));
    Society again(Roster::None);
    again.configure(docks());
    expect(again.restore(state), "restored");
    again.setHolder("player-a3", "central:alice");
    again.addPlayer("player-a3");
    expect(again.cash("player-a3") == 0, "the cap survives a restart");
    // A week on: welcomed again.
    for (int h = 0; h < 2; ++h)
        again.tick(1., 14 + h / 24. + 1e-4, 0, {});
    again.setHolder("player-a4", "central:alice");
    again.addPlayer("player-a4");
    expect(again.cash("player-a4") == 20, "a week on, the account's next wolf welcomed");
    // A hundred wolves on one account in a week: 20p in all.
    std::int64_t made = 0;
    for (int i = 0; i < 100; ++i)
    {
        const auto id = "player-churn" + std::to_string(i);
        again.setHolder(id, "central:carol");
        again.addPlayer(id);
        made += again.cash(id);
    }
    expect(made == 20, "a hundred characters on one account in a week: one welcome's coins, " + std::to_string(made) + "p");
}

void theStagedDaySettledAtRestore()
{
    // A restart across 0:00 still runs that night's set (Phase 0's open item 10): the same purses as a run never stopped.
    const auto run = [](bool restart) {
        auto s = docksSociety();
        for (int h = 0; h <= 22; ++h)
            s.tick(1., 2 + h / 24. + 1e-4, 0, {});
        if (restart)
        {
            const auto state = wire::readSociety(parse(json::dump(wire::society(s.state()))));
            Society again(Roster::None);
            again.configure(docks());
            if (!again.restore(state))
                throw std::runtime_error("restore failed");
            s = again;
        }
        for (int h = 23; h <= 26; ++h)
            s.tick(1., 2 + h / 24. + 1e-4, 0, {});
        return s;
    };
    const auto straight = run(false);
    const auto restarted = run(true);
    for (const auto& [id, life] : straight.state().residents)
        expect(straight.cash(id) == restarted.cash(id) && straight.statusOf(id)->band == restarted.statusOf(id)->band,
               "the night's set ran across the restart: " + id + " " + std::to_string(straight.cash(id)) + "p and " +
                   std::to_string(restarted.cash(id)) + "p");
    expect(restarted.state().staged.hour == straight.state().staged.hour, "the staged hour as saved");
}

void theMeansNote()
{
    auto s = docksSociety();
    s.stagedBands(2, true);
    for (const auto& [id, life] : s.state().residents)
    {
        const auto note = s.meansNote(id);
        expect(!note.empty() && note.size() <= 300 && note.rfind("Means: ", 0) == 0 &&
                   note.find(status::bandName(s.statusOf(id)->band)) != std::string::npos,
               "the Means note, under 300 characters, with the band: " + note);
        if (s.payWord(id) > 0)
            expect(note.find("pence a day") != std::string::npos, "the pay word in it: " + note);
        expect(note.find("dog") == std::string::npos, "never the forbidden word");
    }
}

void bornAndArrivedTakeTheirBand()
{
    // 16.2: a born child takes the household's band at once; an arrival placed in a post takes its household's.
    auto s = docksSociety();
    s.stagedBands(2, true);
    s.overrideBand(s.householdOf("ivar"), status::Destitute, 100);   // (A band the default could not give by chance.)
    auto child = *s.spec("una");
    child.id = "born_1";
    child.name = "Pip Nailham";
    child.age = 0;
    child.role = "civilian";
    child.workLabel = "-";
    child.paid = false;
    expect(s.addResident(child, 0), "the child added (as the world adds a birth)");
    ResidentRequest birth;
    birth.kind = "birth";
    birth.templateId = "una";
    birth.parents = {"ivar", "una"};
    expect(s.welcome(birth, "born_1").kind == "birth", "the child welcomed");
    const auto* born = s.statusOf("born_1");
    expect(born && born->band == status::Destitute && s.householdOf("born_1") == s.householdOf("ivar"),
           "a born child takes the household's band at once");
    s.stagedBands(3);
    expect(s.householdOf("born_1") == s.householdOf("ivar") && s.statusOf("born_1")->band == s.statusOf("ivar")->band,
           "and keeps it at the next day's pass, in its parents' household");
    // An arrival placed in a post (Ivar's, nobody in the city free): he lodges with Una and takes her household's band.
    Society a(Roster::None);
    a.configure(docksAndCountry(false));
    Fates f;
    a.tendCareers(1.3, f.world(a));
    a.stagedBands(2, true);
    a.overrideBand(a.householdOf("una"), status::Poor, 100);
    f.dead.insert("ivar");
    a.died("ivar", 1.5);
    std::string arrived;
    for (int day = 2; day <= 14 && arrived.empty(); ++day)
    {
        a.tendCareers(day + .3, f.world(a));
        for (auto& r : a.takeRequests())
            if (r.kind == "arrival" && a.arrive(r) && a.welcome(r, r.templateId).kind == "newcomer")
                arrived = r.templateId;
    }
    expect(arrived == "cty_fisher" && a.jobOf(arrived) && a.jobOf(arrived)->id == "job:ivar", "the fisher came for the docker's post");
    expect(a.statusOf(arrived) && a.householdOf(arrived) == a.householdOf("una") && a.statusOf(arrived)->band == status::Poor,
           "an arrival placed in a post takes its household's band at once");
}

void theClassesByPostKind()
{
    // people_flow's theClasses (:115) re-based on postKind, in the staged economy.
    Society s(Roster::None);
    s.configure(docksAndCountry(true));
    Fates f;
    s.tendCareers(1.3, f.world(s));
    const auto kind = [&](const std::string& post) { return s.postKind(*s.position(post)).kind; };
    const auto cls = [&](const std::string& post) { return s.postClass(*s.position(post)); };
    expect(kind("job:moss") == "keeper" && cls("job:moss") == "middle", "the stall: a keeper's post, middle");
    expect(kind("job:ivar") == "estate" && cls("job:ivar") == "lower", "a docker: the sea trade's, lower");
    expect(kind("job:lady") == "estate" && cls("job:lady") == "upper", "at home in the Heights: the Houses' people, upper");
    expect(kind("job:tam").empty() && cls("job:tam").empty(), "lessons: no kind, no post that pays");
    for (const auto& p : s.positions())
        expect(s.postKind(p).kind.empty() == s.postClass(p).empty(), "a class exactly where postKind finds a kind: " + p.id);
    expect(s.classOf("ivar") == "lower" && s.classOf("moss") == "middle" && s.classOf("lady") == "upper", "a founder: its post's");
    expect(s.classOf("tam") == "lower", "a pup at lessons: its parents'");
    expect(s.classOf("cty_fisher") == "lower" && s.classOf("rm_servant") == "lower" && s.classOf("tn_baker") == "middle" &&
               s.classOf("rm_cousin") == "upper",
           "the unplaced by their own work as written");
}

void successionWithTheLoopOff()
{
    // 16.2: with the city's loop gone, succession still fills a post emptied for 8 days (from the city: the grown pup).
    Society s(Roster::None);
    s.configure(docksAndCountry(true));
    Fates f;
    s.tendCareers(1.3, f.world(s));
    f.dead.insert("ivar");
    s.died("ivar", 1.5);
    for (int day = 2; day <= 12; ++day)
    {
        s.tendCareers(day + .3, f.world(s));
        const bool now = s.jobOf("tam") && s.jobOf("tam")->id == "job:ivar";
        expect(!now || day >= 1.5 + Society::PostWaitDays, "not before eight days: day " + std::to_string(day));
    }
    expect(s.jobOf("tam") && s.jobOf("tam")->id == "job:ivar", "the grown pup takes the docker's post at eight days");
}

void playersPayAndTheCaps()
{
    auto s = docksSociety();
    s.stagedBands(2, true);
    s.openRealBalance();
    s.setHolder("player-ada", "central:ada");
    s.addPlayer("player-ada");
    // Lodging rent or a repair paid into a keeper's till: earning from players for the keeper's household (6.3, 6.7).
    const auto till = s.tillOf("moss");
    expect(!hasEvent(s, "moss", "earning"), "nothing earned yet");
    s.create(till, "bread", 4, "test: the shelf");
    expect(s.sale(till, "player-ada", "bread", 1, 1, "local trade") && !hasEvent(s, "moss", "earning"), "a purchase is not earning");
    // A lot whose total doesn't divide: trade's odd pennies, coin-only, from the player into the till. Still a purchase.
    expect(s.sale(till, "player-ada", "bread", 3, 1, "local trade") &&
               s.shift("player-ada", till, "", 0, 1, "local trade: the odd pennies") && !hasEvent(s, "moss", "earning") &&
               !s.heldNow("moss"),
           "a purchase with odd pennies: no earning event, no hold on the keeper");
    expect(s.shift("player-ada", "moss", "", 0, 4, "stall sale") && s.shift("player-ada", "moss", "", 0, 2, "faction surcharge") &&
               !hasEvent(s, "moss", "earning") && !s.heldNow("moss"),
           "a stall sale's or a surcharge's coins to a resident: not earning");
    expect(s.shift("player-ada", till, "", 0, 5, "rent"), "rent paid into the till");
    expect(hasEvent(s, "moss", "earning") && s.heldNow("moss"), "rent to the keeper's till: earning, and the steady hold");
    // The rewards cap on an escrow's payout (9.3): the staged-sourced part only within the holder's room, the rest back.
    const auto cap = s.statusTable().cap("rewardsADay");
    expect(cap > 0 && s.openAccount("contract:prize"), "a contract's escrow");
    expect(s.shift("moss", "contract:prize", "", 0, cap + 10, "reward set aside"), "a resident's reward set aside");
    const auto mossBefore = s.cash("moss");
    const auto paid = s.cappedPayout("contract:prize", "player-ada", cap + 10, "rewardsADay", "moss");
    expect(paid == cap && s.cash("contract:prize") == cap && s.cash("moss") == mossBefore + 10,
           "held to the day's cap, the rest back to its poster: " + std::to_string(paid) + "p");
    expect(s.shift("contract:prize", "player-ada", "", 0, paid, "contract reward"), "the reward paid");
    expect(s.faucetToday("central:ada", "reward") == cap && s.realBalanced(), "a day's rewards at the cap, balanced");
    // A second: its real part whole, its staged part nothing (no room left).
    expect(s.openAccount("contract:two"), "another escrow");
    s.shift("player-ada", "contract:two", "", 0, 5, "reward set aside");
    s.shift("moss", "contract:two", "", 0, 5, "reward set aside");
    const auto second = s.cappedPayout("contract:two", "player-ada", 10, "rewardsADay", "moss");
    expect(second == 5 && s.cash("contract:two") == 5, "past the cap: only the real coins paid: " + std::to_string(second));
    expect(s.shift("contract:two", "player-ada", "", 0, second, "contract reward") && s.realBalanced() &&
               s.faucetToday("central:ada", "reward") == cap,
           "nothing past the cap counted");
    // D16: an unregistered faucet logged (once a day), never refused; a registered one not.
    s.takeDoorNotes();
    expect(s.shift("treasury", "player-ada", "", 0, 1, "a mysterious windfall") &&
               s.shift("treasury", "player-ada", "", 0, 1, "a mysterious windfall"),
           "an unregistered faucet still pays");
    s.shift("treasury", "player-ada", "", 0, 1, "faction mission reward");
    const auto notes = s.takeDoorNotes();
    expect(notes.size() == 1 && notes[0].find("a mysterious windfall") != std::string::npos, "logged once, by its reason: " +
                                                                                               std::to_string(notes.size()));
    expect(s.stagedTopUps() >= 0, "the top-ups counted");
}

void inheritanceMovesGoodsOnly()
{
    // 6.5: between NPCs bequeath moves goods only; the coins are staged and drop.
    auto s = docksSociety();
    Fates f;
    s.stagedSet("ivar", 250);
    s.create("ivar", "iron_ore", 4, "test: a dead man's goods");
    const auto una = s.cash("una"), tam = s.cash("tam");
    f.dead.insert("ivar");
    s.died("ivar", 3.1);
    s.tendCareers(4.5, f.world(s));
    expect(s.cash("ivar") == 0 && s.cash("una") == una && s.cash("tam") == tam,
           "the estate's coins dropped, none to the heirs: " + std::to_string(s.cash("una")) + "p");
    expect(Society::stock(*s.account("una"), "iron_ore") + Society::stock(*s.account("tam"), "iron_ore") == 4, "its goods to the heirs");
}

void theWelcomeAcrossACrash()
{
    // D23 across a crash: the welcome and the day's tallies are journalled with it (wire::stagedChanges, as Game::record
    // writes them), so a checkpoint from before and the journal replayed over it still know the holder was welcomed.
    auto s = docksSociety();
    for (int h = 0; h < 2; ++h)
        s.tick(1., 7 + h / 24. + 1e-4, 0, {});
    auto document = json::Value::object();
    document.add("society", wire::society(s.state()));   // (The checkpoint, before.)
    s.takeStagedChanged();
    s.setHolder("player-a1", "central:alice");
    s.addPlayer("player-a1");
    expect(s.cash("player-a1") == 20, "welcomed");
    const auto changes = wire::stagedChanges(s.state().staged, s.takeStagedChanged());
    expect(changes.size() >= 2, "the welcome and the day's tallies journalled");
    std::string problem;
    expect(journal::apply(document, changes, problem), "the journal replayed: " + problem);
    Society again(Roster::None);
    again.configure(docks());
    expect(again.restore(wire::readSociety(document["society"])), "restored from the checkpoint and the journal");
    again.setHolder("player-a2", "central:alice");
    again.addPlayer("player-a2");
    expect(again.cash("player-a2") == 0, "after the crash, the same holder's next wolf that week: no second welcome");
    expect(again.faucetToday("central:alice", "welcome") >= 20, "and the day's tally kept");
}

void theRestAcrossACrash()
{
    // 9.2: the rest of the staged state is journalled too (wire::stagedDiff, as Game::record writes it): a pot's
    // staged-sourced part, a theft's lean days, holds and events, the bands. A checkpoint from before and the journal
    // replayed over it restore all of it, so a second theft doesn't pay and the pot's payout is still capped.
    auto s = docksSociety();
    s.stagedBands(2, true);
    s.setHolder("player-ada", "central:ada");
    s.addPlayer("player-ada");
    s.addPlayer("player-thief");
    auto document = json::Value::object();
    document.add("society", wire::society(s.state()));   // (The checkpoint, before.)
    const auto before = s.state().staged;
    s.takeStagedChanged();
    expect(s.openAccount("contract:pot") && s.shift("moss", "contract:pot", "", 0, 12, "reward set aside"), "a resident's reward set aside");
    expect(s.shift("una", "player-thief", "", 0, 3, "stolen") && s.leanNow("una"), "a theft: the victim lean");
    expect(s.overrideBand(s.householdOf("una"), status::Poor, 30), "a DM's band for the victim's household");
    auto changes = wire::stagedChanges(s.state().staged, s.takeStagedChanged());
    auto rest = wire::stagedDiff(before, s.state().staged);
    expect(!rest.items().empty(), "the rest of the staged state journalled");
    for (auto& c : rest.items())
        changes.push(std::move(c));
    std::string problem;
    const bool replayed = journal::apply(document, changes, problem);
    expect(replayed, "the journal replayed: " + problem);
    Society again(Roster::None);
    again.configure(docks());
    expect(again.restore(wire::readSociety(document["society"])), "restored from the checkpoint and the journal");
    const auto& a = again.state().staged;
    const auto& b = s.state().staged;
    expect(a.escrowSource == b.escrowSource && a.escrowSource.count("contract:pot") && a.escrowSource.at("contract:pot") == 12,
           "the pot's staged-sourced part kept");
    expect(a.lean == b.lean && a.holds == b.holds && again.leanNow("una"), "the victim's lean days and the holds kept");
    expect(a.events.size() == b.events.size() && a.events.count("una"), "the robbed event kept");
    expect(a.bands.size() == b.bands.size() && a.overrides.size() == b.overrides.size(), "the bands and the override kept");
    // Something gone since is erased by the journal too.
    const auto mid = s.state().staged;
    s.shift("contract:pot", "moss", "", 0, 12, "reward returned");
    const auto erased = wire::stagedDiff(mid, s.state().staged);
    const auto& list = erased.items();
    expect(!s.state().staged.escrowSource.count("contract:pot") &&
               std::any_of(list.begin(), list.end(), [](const json::Value& c) { return c.has("erase"); }),
           "an escrow emptied of its staged part: erased in the journal");
}

// Farm posts still fill with the city's loop off (16.2): farm_people's fixture and its works' post filled again (doc 72,
// Phase 3), the staged economy on. (A copy of farm_people_tests' builder, which Phase 3 trims.)
namespace farmside
{
ResidentSpec person(const std::string& id, const std::string& label, int age, const std::string& home = {},
                    const std::string& sex = "female", std::int64_t purse = 30)
{
    ResidentSpec r;
    r.id = id;
    r.name = id;
    r.role = "civilian";
    r.workLabel = label;
    r.age = age;
    r.purse = purse;
    r.meals = 0;
    r.appearance.sex = sex;
    if (!home.empty())
        r.home = r.work = r.evening = {home, 2.5, 2.5};
    return r;
}
std::string communityOf(const std::string& cell)
{
    const auto& people = items::farmPeople();
    for (const auto& c : people.communities)
        if (cell.rfind("cn_" + c.id + "_", 0) == 0)
            return c.id;
    if (cell.rfind("w_ridgemere_", 0) == 0)
        return "the realm";
    for (const auto& f : people.farms)
    {
        bool here = cell == f.home.area;
        for (const auto& w : f.workplaces)
            here = here || cell == w.area;
        if (here)
            return f.community;
    }
    return "ridgemere";
}
struct Fixture
{
    Society s{Roster::None};
    CareerWorld world;
    std::set<std::string> dead;                     // (The world's dead are not alive: farm_people's leaves them so.)
};
void build(Fixture& x)
{
    AuthoredRoster r;
    auto stall = person("stall", "keeping the stall in the square", 40, "home_stall");
    stall.role = "merchant";
    stall.work = {"square", 2.5, 2.5};
    stall.serve = {"square", 2.5, 3.5};
    stall.startHour = 7, stall.endHour = 19;
    stall.purse = 400;
    r.residents.push_back(stall);
    for (int i = 0; i < 6; ++i)
    {
        auto w = person("porter_" + std::to_string(i), "carrying from the wharf", 30 + i, "r2_rents_" + std::to_string(i % 3));
        w.work = {"wharf", 2.5, 2.5};
        r.residents.push_back(w);
    }
    int n = 0;
    const auto add = [&](const std::string& label, int count, int age0) {
        for (int i = 0; i < count; ++i)
            r.unplaced.push_back(person("u" + std::to_string(n++), label, age0 + (i * 7) % 30, {}, i % 2 ? "male" : "female"));
    };
    add("works the fields at Wheatley", 12, 20);
    add("works at The Larkmill Water Mill", 3, 30);
    add("tends the orchard at Russet", 18, 20);
    add("tends the vines at Sunvine", 3, 24);
    add("milks the herd at Wetmeadow", 14, 20);
    add("keeps pigs at Mastwood", 15, 20);
    add("keeps rabbits at Coneyhill", 4, 25);
    add("raises sheep at Fleecemoor", 8, 20);
    add("keeps bees at Mossbank", 6, 30);
    add("fishes the waters off Greywater", 18, 20);
    add("traps eels in the fen", 3, 30);
    add("cuts reeds in the marsh", 3, 30);
    add("keeps the house", 39, 20);
    add("serves at The Lantern", 12, 20);
    add("helps at The Stores", 10, 20);
    add("logging at Cedarfall", 5, 20);
    add("logging at Oakfell", 3, 22);
    add("cutting stone at Greyscar", 4, 24);
    add("digs clay at Clayholt", 3, 22);
    add("digs ore at Deepvein", 3, 30);
    add("works the lime kilns at Greyscar", 1, 40);
    add("logging at Highpine", 3, 26);
    add("plays about the town", 6, 6);
    add("sits by the well", 3, 70);
    const auto one = [&](const std::string& id, const std::string& label, int age, const std::string& sex = "female") {
        r.unplaced.push_back(person(id, label, age, {}, sex));
    };
    one("shop_larkfield", "shopkeeper at Larkfield General", 30, "male");
    one("shop_coldwater", "shopkeeper at Coldwater Sundries", 40);
    one("shop_frost", "shopkeeper at Frosthollow Stores", 35);
    one("shop_oak", "shopkeeper at Oakhaven Goods", 30, "male");
    one("prov_fen", "provisioner at Fen Provisions", 31, "male");
    one("inn_lantern", "keeps The Glass Lantern", 45);
    one("inn_lark", "keeps The Singing Lark", 27);
    one("inn_acorn", "keeps The Acorn Inn", 52, "male");
    one("inn_apple", "keeps The Apple Tree", 22, "male");
    one("inn_eel", "keeps The Eel and Lantern", 30);
    one("baker_lake", "baker at The Lake Oven", 50);
    one("baker_wheat", "baker at The Wheatsheaf Bakery", 48);
    one("still_cw", "brewer at The Copper Worm", 44, "male");
    one("rook", "farms the downs", 57, "male");
    one("iris", "keeps the house", 55);
    one("tor", "plays about the town", 11, "male");
    one("hob", "works the fields at Wheatley", 25, "male");
    r.kin = {{"iris", "rook", "spouse"}, {"rook", "tor", "parent"}, {"iris", "tor", "parent"}, {"rook", "hob", "parent"}};
    one("ash", "logging at Snowpine", 55, "male");
    one("wren", "keeps the house", 53);
    r.kin.push_back({"ash", "wren", "spouse"});
    one("bo", "selling Fell wine", 56, "male");
    one("nessa", "at home in the Heights", 50);
    r.kin.push_back({"bo", "nessa", "spouse"});
    r.economy.treasury = 20000;
    x.s.configure(r);
    LifeDay day;
    day.communityOf = communityOf;
    x.s.setDay(day);
    auto& s = x.s;
    x.world.alive = [&s, &x](const std::string& id) { return s.resident(id) != nullptr && !x.dead.count(id); };
    x.world.age = [&s](const std::string& id) { return s.spec(id) ? s.spec(id)->age : 30; };
    x.world.regard = [](const std::string&, const std::string&) { return 50.; };
    x.world.near = [](const std::string& work, const std::string& home) { return communityOf(work) == communityOf(home); };
    x.world.lodgings = [](const std::string& work) {
        return communityOf(work) == "ridgemere" ? std::vector<Spot>{{"r2_lodging_house", 4.5, 4.5}} : std::vector<Spot>{};
    };
    x.world.place = [](const std::string&) { return true; };
}
int seasonOf(double day)
{
    const double d = std::fmod(day, double(calendar::DaysPerYear));
    return d < 92 ? 0 : d < 184 ? 1 : d < 275 ? 2 : 3;
}
void run(Society& s, double from, double to)
{
    std::map<std::string, LifeBody> bodies;
    for (const auto& r : s.authored().residents)
    {
        LifeBody b{r.work.cell, r.work.x, r.work.y};
        b.age = r.age;
        bodies[r.id] = b;
    }
    for (double now = from; now < to; now += 1. / calendar::SecondsPerDay)
        s.tick(1, now, seasonOf(now), bodies);
}
const Society::FarmNow& farm(const Society& s, const std::string& id)
{
    for (const auto& f : s.farms())
        if (f.def->id == id)
            return f;
    throw std::runtime_error("no farm " + id);
}
} // namespace farmside

void farmPostsStillFill()
{
    using namespace farmside;
    Fixture x;
    build(x);
    auto& s = x.s;
    run(s, 1 - .5 / 24, 1 + .5 / 24);
    s.tendCareers(2, x.world);
    int placed = 0;
    for (const auto& r : s.takeRequests())
        if (s.arrive(r))
            s.welcome(r, r.templateId), ++placed;
    expect(placed > 20, "the farms' posts taken by the unplaced, with the loop off: " + std::to_string(placed));
    run(s, 2 + .1 / 24, 2 + 1.2 / 24);              // (Those in households of their own banded at the next hour's pass.)
    const auto handOf = [&](const std::string& works) {
        for (const auto& p : s.positions())
            if (bool k = false; s.farmOfPost(p, &k) >= 0 && s.farms()[std::size_t(s.farmOfPost(p))].def->id == works && !k)
                return p.id;
        return std::string();
    };
    std::map<std::string, std::string> post;
    for (const auto* works : {"logging_rainwood", "clay_fens"})
    {
        post[works] = handOf(works);
        expect(s.position(post[works]) != nullptr, std::string(works) + " has a hand's post");
        const auto holder = s.state().careers.positions.at(post[works]).holder;
        expect(!holder.empty() && s.statusOf(holder), std::string(works) + "'s hand placed, and banded: " + holder);
        x.dead.insert(holder);
        s.died(holder, 2.2);
    }
    run(s, 2.9, 3 + .5 / 24);
    expect(farm(s, "logging_rainwood").heldHands == 1 && farm(s, "clay_fens").heldHands == 0, "two works' posts stand empty");
    int day = 3;
    for (; day < 30 && (farm(s, "logging_rainwood").heldHands < 2 || farm(s, "clay_fens").heldHands < 1); ++day)
    {
        s.tendCareers(day, x.world);
        for (const auto& r : s.takeRequests())
            if (s.arrive(r))
                s.welcome(r, r.templateId);
        run(s, day + .5 / 24, day + .6 / 24);
    }
    expect(farm(s, "logging_rainwood").heldHands == 2 && farm(s, "clay_fens").heldHands == 1,
           "both filled again within " + std::to_string(day - 2) + " days, the loop off");
    for (const auto& [works, id] : post)
    {
        const auto& holder = s.state().careers.positions.at(id).holder;
        expect(!holder.empty() && s.resident(holder) && s.statusOf(holder), works + "'s post held again, its holder banded: " + holder);
    }
}
} // namespace

int main()
{
    try
    {
        setenv("RATW_DATA_DIR", (std::filesystem::path(RATW_SOURCE_DIR) / "Data").string().c_str(), 1);
        if (!items::load())
            throw std::runtime_error("the catalogue didn't load");
        householdsShareABand();
        foodFromTheShelf();
        playersEmptyTheShelf();
        foodShopsTheRestockFills();
        stakesAndTillsAtPayout();
        holdsLeanAndTheTrim();
        theDoorAndRealMoney();
        theWelcomeOncePerHolderPerWeek();
        theStagedDaySettledAtRestore();
        theMeansNote();
        bornAndArrivedTakeTheirBand();
        theClassesByPostKind();
        successionWithTheLoopOff();
        playersPayAndTheCaps();
        inheritanceMovesGoodsOnly();
        theWelcomeAcrossACrash();
        theRestAcrossACrash();
        farmPostsStillFill();
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAILED after " << checks << " checks: " << e.what() << "\n";
        return 1;
    }
    std::cout << "Life status tests passed: " << checks << " checks.\n";
    return 0;
}
