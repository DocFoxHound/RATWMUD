// The Market Board (Docs/Design/79-a-staged-economy.md, 5 and 7; Phase 1; Core/RatwMarket.*), in each test's own
// world: pressure from players' trade at the tills, its decay, the
// per-piece ladder and the per-holder cap, the morning's ripples through recipes, the shown dozen, the budget, take
// allowance and sell total with their reasons, the morning restock, the Board saved and restored, the markets the world
// finds, the faction discount's weekly cap. And the one path: since Phase 3 the Board is the only price path, with no
// switch to turn it off.
#include "test_purse.h"
#include "RatwCheckpoint.h"
#include "RatwGame.h"
#include "RatwItems.h"
#include "RatwJournal.h"
#include "RatwJsonDoc.h"
#include "RatwMarket.h"
#include "RatwSections.h"
#include "RatwSociety.h"
#include "RatwWire.h"
#include "RatwWorld.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>

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
bool near(double a, double b, double eps = 1e-9) { return std::abs(a - b) <= eps; }
std::string str(double x)
{
    std::ostringstream o;
    o << x;
    return o.str();
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

struct Shop
{
    std::string id, work, market;
};
// A world of these shops (and two townsfolk); its markets as given, each with
// its catchment; `rules`, more of the world's own.
Society world(const std::vector<Shop>& shops, const std::map<std::string, double>& catchments,
              const std::map<std::string, int>& rules = {})
{
    AuthoredRoster roster;
    int n = 0;
    for (const auto& s : shops)
        roster.residents.push_back(person(s.id, s.work, {"room" + std::to_string(n++), 2.5, 2.5}));
    roster.residents.push_back(person("farmer", "farms the valley fields", {"fields", 4.5, 4.5}, "civilian"));
    roster.residents.push_back(person("idler", "-", {"square", 3.5, 3.5}, "civilian"));
    roster.economy.treasury = 100000;
    roster.economy.needsPaused = true;
    roster.economy.rules = rules;
    Society s(Roster::None);
    s.configure(roster);
    if (!catchments.empty())
    {
        market::Topology t;
        for (const auto& [id, catchment] : catchments)
        {
            market::TradePoint m;
            m.id = id;
            m.catchment = catchment;
            for (const auto& shop : shops)
                if (shop.market == id)
                {
                    m.tills.push_back(s.tillOf(shop.id));
                    for (const auto& w : s.wares(shop.id))
                        m.sold.insert(items::baseOf(w));
                    if (const auto* b = items::businessFor(shop.work))
                        market::madeBy(b->id, m.made);
                }
            t.markets.push_back(std::move(m));
        }
        for (auto& m : t.markets)
            for (const auto& o : t.markets)
                if (o.id != m.id)
                    m.routes.push_back(o.id);
        s.setTradePoints(std::move(t));
    }
    s.setMarketClock(10. + 10. / 24);              // A working morning of day 10.
    return s;
}
std::string player(Society& s, const std::string& id, std::int64_t purse = 5000)
{
    s.addPlayer(id);
    expect(testPurse(s, id, purse), "a purse for " + id);
    return id;
}
void stock(Society& s, const std::string& merchant, const std::string& item, int n)
{
    const auto till = s.tillOf(merchant);
    const int have = Society::stock(*s.account(till), item);
    if (n > have)
        expect(s.create(till, item, n - have, "test stock"), "stock " + item + " at " + merchant);
}

void everyGoodHasADepthGroup()
{
    // Every catalogue good is mapped to a group by itself or its category (market.json), never the fallback alone.
    expect(market::data().loaded, "market.json read");
    int goods = 0;
    for (const auto& g : items::allGoods())
    {
        std::string how;
        const auto group = market::groupOf(g.id, &how);
        expect(how != "fallback" && market::data().groups.count(group), g.id + " has a depth group: " + how);
        ++goods;
    }
    for (const char* own : {"meal", "herbs", "sword"})
        expect(market::data().groups.count(market::groupOf(own)), std::string(own) + " too");
    expect(goods > 400, "over the whole catalogue: " + std::to_string(goods));
    expect(market::groupOf("bread") == "staple" && market::groupOf("iron_ore") == "metal" && market::groupOf("silk") == "luxury" &&
               market::groupOf("hide") == "meat",
           "the doc's groups");
}

void thePieces()
{
    // The arithmetic: decay exact at any step, the clamp, NaN read 1, half-lives by keeping.
    expect(near(market::decayed(.4, 0, 2, 2), .2) && near(market::decayed(.4, 0, 1, 2) * std::pow(.5, .5), .2, 1e-12),
           "decay: half at the half-life, the same in two steps");
    expect(near(market::factor(2, .5), market::FactorHigh) && near(market::factor(.1, -.5), market::FactorLow) &&
               near(market::factor(std::nan(""), 0), 1) && near(market::factor(1, std::numeric_limits<double>::infinity()), 1) && near(market::factor(1, 9), market::FactorHigh) && near(market::factor(1, .1), std::exp(.1)),
           "the clamp holds and a NaN factor reads 1");
    const market::Rules r;
    expect(near(market::halfLife("bread", r), 2) && near(market::halfLife("cheese", r), 5) && near(market::halfLife("iron_ore", r), 8),
           "perishables 2 days, keeping food 5, metals 8");
    expect(market::timesOf(-100) == 0 && market::timesOf(0) == 2 && market::timesOf(100) == 4, "Times from the slider");
}

void breadInRidgemere()
{
    auto s = world({{"baker", "baker at The Loaf", "ridgemere"}}, {{"ridgemere", 620}});
    stock(s, "baker", "bread", 2000);
    const auto till = s.tillOf("baker");
    const double depth = s.depthOf("ridgemere", "bread");
    expect(near(depth, 2 * std::pow(620., .6), 1e-6), "bread's depth in Ridgemere about 95: " + str(depth));
    const auto ada = player(s, "player-ada");
    const auto before = s.listing(ada, "baker", "bread");
    expect(before.listCount == 12 && before.trend == 0, "bread listed by the dozen, no marker at rest");
    const auto ten = s.trade(ada, "baker", "bread", 10, true);
    expect(ten.ok, "ten loaves bought: " + ten.message);
    expect(near(s.pressure("ridgemere", "bread"), 10 / depth, 1e-9), "P up by 10/depth: " + str(s.pressure("ridgemere", "bread")));
    const auto bo = player(s, "player-bo");
    const auto after = s.listing(bo, "baker", "bread");
    expect(after.listPrice == before.listPrice + 1 && after.trend == 1 && after.cause == "travellers buying",
           "one player's ten loaves move the dozen by a penny, with its marker: " + std::to_string(before.listPrice) + " to " +
               std::to_string(after.listPrice));
    // Twenty players buying: the shown dozen rises, inside the clamp.
    for (int i = 0; i < 20; ++i)
    {
        const auto who = player(s, "player-crowd" + std::to_string(i), 200);
        expect(s.trade(who, "baker", "bread", 12, true).ok, "a crowd buys");
    }
    const auto crowd = s.listing(bo, "baker", "bread");
    const double base = s.baseFactor(till, "bread");
    expect(crowd.listPrice > after.listPrice && crowd.listPrice <= std::llround(12 * std::min(market::FactorHigh, base * std::exp(.5))),
           "twenty players' buying: dearer, inside the clamp: " + std::to_string(crowd.listPrice));
    // The decay: half at bread's half-life; the lazy read and the nightly sweep agree.
    const double P = s.pressure("ridgemere", "bread");
    s.setMarketClock(s.marketClock() + 2);
    const double lazy = s.pressure("ridgemere", "bread");
    s.marketMorning(s.marketDay());
    expect(near(lazy, P / 2, 1e-9) && near(s.pressure("ridgemere", "bread"), lazy, 1e-12),
           "half in two days, lazily and swept: " + str(P) + " to " + str(lazy));
}

void aLonePlayersDayInColdwater()
{
    // A lone account holder moves no good past ±0.15 in a day, whatever it buys, sells or buys again; a second character
    // on the same account shares the cap.
    auto s = world({{"baker", "baker at The Loaf", "coldwater"}, {"tanner", "tanner at The Hide Yard", "coldwater"}},
                   {{"coldwater", 40}});
    stock(s, "baker", "bread", 2000);
    const auto ada = player(s, "player-ada"), alt = player(s, "player-alt");
    expect(near(s.depthOf("coldwater", "bread"), market::DepthLeast) && near(s.depthOf("coldwater", "hide"), market::DepthLeast),
           "every good on the depth's floor in a village");
    for (int i = 0; i < 3; ++i)
        expect(s.trade(ada, "baker", "bread", 40, true, "acct:ada").ok, "Ada buys bread");
    expect(s.trade(alt, "baker", "bread", 40, true, "acct:ada").ok, "and her second character");
    const double P = s.pressure("coldwater", "bread");
    expect(P <= .15 + 1e-9 && P > .149, "capped at 0.15 for the account: " + str(P));
    s.create(ada, "hide", 60, "test: hunted");
    for (int i = 0; i < 6; ++i)
        s.trade(ada, "tanner", "hide", 5, false, "acct:ada");
    expect(s.pressure("coldwater", "hide") >= -.15 - 1e-9, "and selling: " + str(s.pressure("coldwater", "hide")));
    for (const auto& [m, goods] : s.board().cell)
        for (const auto& [g, c] : goods)
            expect(std::abs(c.P) <= .15 + 1e-9, "no good past 0.15: " + g + " " + str(c.P));
}

void hidesAtColdwater()
{
    // The doc's worked example (7.3): the first piece at P = 0, then -0.05 a piece to the cap; then flat.
    auto s = world({{"tanner", "tanner at The Hide Yard", "coldwater"}}, {{"coldwater", 40}});
    const auto ada = player(s, "player-ada");
    s.create(ada, "hide", 20, "test: hunted");
    const auto till = s.tillOf("tanner");
    const double base = s.baseFactor(till, "hide"), each = double(Society::catalogPrice("hide"));
    double three = 0;
    for (int i = 0; i < 3; ++i)
        three += each * market::factor(base, -.05 * i) * market::SellShare;
    const auto q3 = s.quote(ada, "tanner", "hide", 3, false);
    expect(q3.ok && q3.total == std::int64_t(std::floor(three + 1e-9)), "three hides pay the ladder's floor: " + std::to_string(q3.total) +
                                                                            " for " + str(three));
    const auto q4 = s.quote(ada, "tanner", "hide", 4, false);
    expect(q4.total == std::int64_t(std::floor(three + each * market::factor(base, -.15) * market::SellShare + 1e-9)),
           "the fourth at the capped P: " + std::to_string(q4.total));
    // Each further piece pays the same (levelled at the cap), never more than the pieces before.
    std::int64_t last = 0;
    double lastStep = 1e9;
    for (int q = 1; q <= 12; ++q)
    {
        const auto t = s.quote(ada, "tanner", "hide", q, false).total;
        const double step = double(t - last);
        expect(t >= last && step <= lastStep + 1.0001, "a dump pays less a piece, then levels: " + std::to_string(q));
        last = t;
        lastStep = std::max(step, 1.);
    }
}

void anOreDumpRipples()
{
    // A 30-ore dump at the smithy: less a piece, then level at the cap; the next morning bars at half and nails at a
    // quarter, no third step, each step at most six goods.
    auto s = world({{"smith", "the smith at the forge", "coldwater"}}, {{"coldwater", 40}});
    expect(std::find(s.wares("smith").begin(), s.wares("smith").end(), "iron_ore") != s.wares("smith").end(),
           "the smithy buys iron ore in the staged economy (D5)");
    const auto ada = player(s, "player-ada");
    s.create(ada, "iron_ore", 30, "test: mined");
    const auto sold = s.trade(ada, "smith", "iron_ore", 30, false);
    expect(sold.ok && sold.total >= 1 && sold.total < 30, "thirty ore sold, for less than a penny a piece: " + sold.message);
    expect(near(s.pressure("coldwater", "iron_ore"), -.15, 1e-9), "ore's P at the holder's cap: " + str(s.pressure("coldwater", "iron_ore")));
    const auto targets = market::rippleTargets("iron_ore", true);
    int first = 0, second = 0;
    bool bars = false, nails = false;
    for (const auto& [g, step] : targets)
    {
        (step == 1 ? first : second) += 1;
        bars = bars || (g == "iron_bar" && step == 1);
        nails = nails || (g == "nails" && step == 2);
        expect(step <= 2, "no third step");
    }
    expect(first <= 6 && second <= 6 && bars && nails, "ore -> bars, then nails, six at most a step: " + std::to_string(first) + "+" +
                                                         std::to_string(second));
    // The next morning.
    s.setMarketClock(std::floor(s.marketClock()) + 1 + market::DawnHour / 24);
    s.marketTick(s.marketClock());
    expect(near(s.pressure("coldwater", "iron_bar"), -.075, 1e-9) && near(s.pressure("coldwater", "nails"), -.0375, 1e-9),
           "bars at half and nails at a quarter: " + str(s.pressure("coldwater", "iron_bar")) + ", " + str(s.pressure("coldwater", "nails")));
    for (const auto& [g, step] : market::rippleTargets("iron_bar", true))
        if (std::none_of(targets.begin(), targets.end(), [&](const auto& t) { return t.first == g; }) && g != "iron_ore")
            expect(near(s.pressure("coldwater", g), 0), "nothing at a third step: " + g);
    expect(s.board().recent.size() >= 1 && s.board().recent.back().cause == "ripple", "the ripple logged with its cause");
    // Every good's graph ends (cycles in the recipes are safe), at most 12 targets.
    for (const auto& g : items::allGoods())
        for (const bool down : {true, false})
            expect(market::rippleTargets(g.id, down).size() <= 12, "a ripple ends: " + g.id);
}

void churnLeavesNoRipple()
{
    // Buying and selling the same good the same day nets to nothing downstream.
    auto s = world({{"smith", "the smith at the forge", "coldwater"}}, {{"coldwater", 40}});
    stock(s, "smith", "nails", 200);
    const auto ada = player(s, "player-ada");
    expect(s.trade(ada, "smith", "nails", 6, true).ok, "nails bought");
    const auto back = s.trade(ada, "smith", "nails", 6, false);
    expect(back.ok, "and sold back: " + back.message);
    s.setMarketClock(std::floor(s.marketClock()) + 1 + market::DawnHour / 24);
    s.marketTick(s.marketClock());
    for (const auto& [g, step] : market::rippleTargets("nails", false))
        expect(near(s.pressure("coldwater", g), 0), "no upstream pressure: " + g);
}

void churnAcrossHolders()
{
    // The market's net over all holders ripples (7.4), not each holder's own: one sells thirty ore and another buys thirty
    // back, the market's net is nothing, and nothing ripples up or down. With a net left over, its ripple is charged to the
    // holders on its side by their shares.
    auto s = world({{"smith", "the smith at the forge", "coldwater"}}, {{"coldwater", 40}});
    const auto ada = player(s, "player-ada"), bo = player(s, "player-bo");
    s.create(ada, "iron_ore", 30, "test: mined");
    expect(s.trade(ada, "smith", "iron_ore", 30, false, "acct:a").ok, "Ada sells thirty ore");
    const auto bought = s.trade(bo, "smith", "iron_ore", 30, true, "acct:b");
    expect(bought.ok, "Bo buys them back: " + bought.message);
    s.setMarketClock(std::floor(s.marketClock()) + 1 + market::DawnHour / 24);
    s.marketTick(s.marketClock());
    for (const bool down : {true, false})
        for (const auto& [g, step] : market::rippleTargets("iron_ore", down))
            expect(near(s.pressure("coldwater", g), 0), std::string("no ") + (down ? "downstream" : "upstream") + " pressure: " + g + " " +
                                                           str(s.pressure("coldwater", g)));
    // Thirty sold, twenty-eight bought: a net glut of two, Ada's alone; its ripple charged to her, none to Bo.
    auto t = world({{"smith", "the smith at the forge", "coldwater"}}, {{"coldwater", 40}});
    const auto cy = player(t, "player-cy"), di = player(t, "player-di");
    t.create(cy, "iron_ore", 30, "test: mined");
    expect(t.trade(cy, "smith", "iron_ore", 30, false, "acct:c").ok && t.trade(di, "smith", "iron_ore", 28, true, "acct:d").ok,
           "thirty sold and twenty-eight bought");
    t.setMarketClock(std::floor(t.marketClock()) + 1 + market::DawnHour / 24);
    t.marketTick(t.marketClock());
    const double depth = t.depthOf("coldwater", "iron_ore");
    expect(near(t.pressure("coldwater", "iron_bar"), -.5 * 2 / depth, 1e-9) && near(t.pressure("coldwater", "nails"), -.25 * 2 / depth, 1e-9),
           "the net's ripple: bars " + str(t.pressure("coldwater", "iron_bar")) + ", nails " + str(t.pressure("coldwater", "nails")));
    expect(near(t.pushedBy("coldwater", "acct:c", "iron_bar"), t.pressure("coldwater", "iron_bar"), 1e-9) &&
               near(t.pushedBy("coldwater", "acct:d", "iron_bar"), 0),
           "charged to the seller's cap, not the buyer's");
    // Two sellers of one glut: the ripple shared between their caps by their shares.
    auto u = world({{"smith", "the smith at the forge", "coldwater"}}, {{"coldwater", 4000}});
    const auto ed = player(u, "player-ed"), fa = player(u, "player-fa");
    u.create(ed, "iron_ore", 30, "test: mined");
    u.create(fa, "iron_ore", 10, "test: mined");
    expect(u.trade(ed, "smith", "iron_ore", 12, false, "acct:e").ok && u.trade(fa, "smith", "iron_ore", 4, false, "acct:f").ok, "two sell");
    u.setMarketClock(std::floor(u.marketClock()) + 1 + market::DawnHour / 24);
    u.marketTick(u.marketClock());
    const double e = u.pushedBy("coldwater", "acct:e", "iron_bar"), f = u.pushedBy("coldwater", "acct:f", "iron_bar");
    expect(e < 0 && f < 0 && near(e, 3 * f, 1e-9) && near(e + f, u.pressure("coldwater", "iron_bar"), 1e-9),
           "three to one: " + str(e) + ", " + str(f));
}

void theShopsLimits()
{
    // The take allowance, the budget and the holder's sell total, each with its reason; a restart keeps the budget.
    auto s = world({{"tanner", "tanner at The Hide Yard", "coldwater"}, {"keeper", "shopkeeper of general goods", "coldwater"}},
                   {{"coldwater", 40}});
    const auto ada = player(s, "player-ada");
    s.create(ada, "stone", 80, "test: quarried");
    s.create(ada, "hide", 60, "test: hunted");
    const auto till = s.tillOf("keeper");
    expect(s.budgetLeft(till) == 40 && s.takeAllowance(till, "stone") == 30, "a 40p budget and 30 pieces of a raw good a day");
    // Raw by what the good is (7.5), not by whether some shop buys it: gathered, hunted and land goods 30; made goods 6,
    // a pawnbroker's cookpots and a foundry's iron bars among them; a wearable or a sword 2.
    for (const char* rawGood : {"stone", "herbs", "hide", "bones", "raw_meat", "fish", "iron_ore", "berries"})
        expect(s.takeAllowance(till, rawGood) >= 30, std::string("a raw good's allowance: ") + rawGood + " " + std::to_string(s.takeAllowance(till, rawGood)));
    for (const char* made : {"cookpot", "iron_bar", "nails", "bread", "leather", "mirror"})
        expect(s.takeAllowance(till, made) == 6, std::string("a made good's allowance: ") + made + " " + std::to_string(s.takeAllowance(till, made)));
    expect(s.takeAllowance(till, "sword") == 2, "a sword's 2");
    // What a market makes (7.1's region): its shops' crafts' outputs and what they supply (crafts.json `supplies`), in
    // Society's own market as in the world's; a good made there prices ×0.95, one made nowhere near ×1.08.
    auto own = world({{"keeper", "shopkeeper of general goods", "x"}}, {});
    const auto& here = own.tradePoints().markets.front();
    expect(!items::suppliesFor("general").empty(), "a general store supplies goods");
    for (const auto& g : items::suppliesFor("general"))
        expect(here.made.count(items::baseOf(g)) == 1, "a supplied good made here: " + g);
    int sold = 0;
    for (int i = 0; i < 6; ++i)
        if (s.trade(ada, "keeper", "stone", 5, false).ok)
            sold += 5;
    expect(sold == 30, "thirty stone taken: " + std::to_string(sold));
    const auto more = s.quote(ada, "keeper", "stone", 5, false);
    expect(!more.ok && more.message == "I've no room for more today.", "then no room: " + more.message);
    const auto tanner = s.tillOf("tanner");
    std::string why;
    for (int i = 0; i < 30 && why.empty(); ++i)
    {
        const auto r = s.trade(ada, "tanner", "hide", 1, false);
        if (!r.ok)
            why = r.message;
    }
    expect(why == "I've spent all I'll spend today." && s.budgetLeft(tanner) < Society::catalogPrice("hide"), "the budget spent: " + why);
    // Player purchases never raise a shop's budget.
    stock(s, "tanner", "leather", 20);
    const auto left = s.budgetLeft(tanner);
    expect(s.trade(ada, "tanner", "leather", 3, true).ok && s.budgetLeft(tanner) == left, "buying from it adds nothing to its budget");
    // A restart keeps the budget and the counts (the Board saved with the society).
    auto saved = wire::readSociety(wire::society(s.state()));
    expect(saved.nextEntry > 0 && !saved.board.empty(), "the Board saved: " + json::dump(wire::society(s.state())).substr(0, 0));
    auto t = world({{"tanner", "tanner at The Hide Yard", "coldwater"}, {"keeper", "shopkeeper of general goods", "coldwater"}},
                   {{"coldwater", 40}});
    expect(t.restore(saved), "restored");
    t.setMarketClock(s.marketClock());
    expect(t.budgetLeft(tanner) == s.budgetLeft(tanner) && t.takenToday(till, "stone") == 30 &&
               near(t.pressure("coldwater", "hide"), s.pressure("coldwater", "hide")),
           "the budget, the take and the pressure kept across a restart");
    // The holder's daily sell total (the rule playerSellMost).
    auto u = world({{"tanner", "tanner at The Hide Yard", "coldwater"}}, {{"coldwater", 40}}, {{"playerSellMost", 6}});
    const auto bo = player(u, "player-bo");
    u.create(bo, "hide", 10, "test: hunted");
    expect(u.trade(bo, "tanner", "hide", 2, false, "acct:bo").ok, "a first sale");
    const auto capped = u.quote(bo, "tanner", "hide", 3, false, "acct:bo");
    expect(!capped.ok && capped.message == "No one here is buying more from you today.", "then the day's sell total: " + capped.message);
    // The next market day, all of it back.
    u.setMarketClock(std::floor(u.marketClock()) + 1 + market::DawnHour / 24);
    u.marketTick(u.marketClock());
    expect(u.quote(bo, "tanner", "hide", 2, false, "acct:bo").ok && u.budgetLeft(u.tillOf("tanner")) == u.budgetOf(u.tillOf("tanner")),
           "a new day: a new budget and sell total");
}

void worthlessLotsAndTheDozen()
{
    // A 1p good: one loaf sold is worth nothing, refused in words, moving nothing; the list names the smallest lot.
    auto s = world({{"baker", "baker at The Loaf", "ridgemere"}}, {{"ridgemere", 620}});
    const auto ada = player(s, "player-ada");
    s.create(ada, "bread", 30, "test: baked");
    stock(s, "baker", "bread", 50);
    const auto one = s.trade(ada, "baker", "bread", 1, false);
    expect(!one.ok && one.message.rfind("Not worth the counting. I buy those by ", 0) == 0, "one loaf refused: " + one.message);
    expect(s.board().cell.empty() && s.board().netToday.empty(), "a refused lot moves no pressure");
    const auto row = s.listing(ada, "baker", "bread");
    expect(row.sellLot >= 2 && row.sell.ok && row.sell.total >= 1 && row.listCount == 12 && row.buyCount == 12,
           "the list: sold by the " + std::to_string(row.sellLot) + ", bought by the dozen");
    // A dear good is listed one at a time.
    expect(s.listing(ada, "baker", "meat_pie").listCount == 1, "a 5p pie listed alone");
}

void seasonsFitTheClamp()
{
    // Fruit's 80-140 (×1.75) rescaled under the span (7.1), the Board's price following market::seasonFactor.
    auto s = world({{"keeper", "shopkeeper of general goods", "realm"}}, {{"realm", 100}});
    const auto* fruit = market::seasonOf("apples");
    expect(fruit != nullptr, "apples are a season good");
    double lo = 9, hi = 0;
    for (int d = 0; d < 365; d += 3)
    {
        s.setYearDay(d);
        const double f = s.baseFactor(s.tillOf("keeper"), "apples") / s.baseFactor(s.tillOf("keeper"), "stone");
        expect(near(f, market::seasonFactor(fruit->price, d), 1e-9), "the Board's fruit on day " + std::to_string(d) + ": " + str(f));
        lo = std::min(lo, f), hi = std::max(hi, f);
    }
    expect(hi / lo <= market::data().seasonSpanMost + 1e-6 && hi / lo > 1.02, "fruit's swing rescaled, still a swing: " + str(hi / lo));
    // The seven groups and the shelves are market.json's (7.1, 7.7: moved from city_loop.json, gone since Phase 3).
    const auto& d = market::data();
    expect(d.seasons.size() == 7 && market::seasonOf("bread") && market::seasonOf("bread")->id == "grain" &&
               market::seasonOf("apples")->id == "fruit" && !market::seasonOf("stone"),
           "the seven season groups in market.json: " + std::to_string(d.seasons.size()));
    for (const auto& g : d.seasons)
        expect(!g.goods.empty() && market::seasonOf(g.goods.front()) == &g, g.id + ": its goods its own");
    expect(d.shelves.count("theriac") && d.shelves.at("theriac") == 1, "theriac's shelf moved: one");
    // The season's arithmetic by day (season_tests' theFactorsByDay, doc 79, 17): each season's value at its middle day
    // (Spring 46, Summer 138, Autumn 229.5, Winter 320), a straight line between, Winter running on into Spring.
    {
        constexpr double Spring = 46, Summer = 138, Autumn = 229.5, Winter = 320;
        const std::array<int, 4> by{10, 20, 30, 40};
        expect(near(market::seasonal(by, Spring), .1) && near(market::seasonal(by, Summer), .2) &&
                   near(market::seasonal(by, Autumn), .3) && near(market::seasonal(by, Winter), .4),
               "each season's value at its middle day");
        expect(near(market::seasonal(by, (Spring + Summer) / 2), .15), "halfway between two middles, halfway between them");
        expect(near(market::seasonal(by, 0), .4 + (.1 - .4) * (365 - Winter) / (Spring + 365 - Winter)) &&
                   near(market::seasonal(by, 364.9), .4 + (.1 - .4) * (364.9 - Winter) / (Spring + 365 - Winter)),
               "winter runs on to spring across the year's turn");
        expect(near(market::seasonal(by, Winter + 365), .4), "a later year is the same");
        expect(market::seasonal(by, -1) == 1, "no date: no season");
        for (double day = 0; day < 365; day += 7.3)
            expect(near(market::seasonal({100, 100, 100, 100}, day), 1), "a flat group stays at 1");
        expect(std::abs(market::seasonal({110, 95, 85, 115}, 46) - 1.1) < 1e-12, "grain's spring");
    }
    // A world's season record lays its price over market.json's group.
    auto t = world({{"keeper", "shopkeeper of general goods", "realm"}}, {{"realm", 100}});
    AuthoredRoster roster = t.authored();
    roster.economy.seasons["fruit"].price = {100, 100, 100, 100};
    t.configure(roster);
    t.setYearDay(320);
    s.setYearDay(320);
    expect(near(t.baseFactor(t.tillOf("keeper"), "apples"), t.baseFactor(t.tillOf("keeper"), "stone")) &&
               s.baseFactor(s.tillOf("keeper"), "apples") > s.baseFactor(s.tillOf("keeper"), "stone"),
           "the world's own fruit season (flat) over market.json's (dear in winter)");
}

void seasonsLeavePressureRoom()
{
    // The user's price decisions (2026-11-08, doc 79's Progress): at a season's ends, with each region factor (made here,
    // a neighbour's, far), players' pressure always keeps at least x1.10 of room up and x0.90 down inside the fixed
    // 0.80-1.28 clamp, for every season good; the region factors stay 0.95, 1.0 and 1.08. (The season's ends are its
    // groups' dearest and cheapest seasons' middle days; every day is checked too.)
    constexpr double Up = 1.10, Down = .90;
    const auto& d = market::data();
    expect(near(d.regionMade, .95) && near(d.regionNear, 1) && near(d.regionFar, 1.08),
           "the region factors kept: " + str(d.regionMade) + ", " + str(d.regionNear) + ", " + str(d.regionFar));
    expect(near(market::FactorLow, .80) && near(market::FactorHigh, 1.28), "the clamp fixed at 0.80-1.28");
    double leastUp = 9, leastDown = 9;
    std::string tightestUp, tightestDown;
    for (const auto& g : d.seasons)
    {
        std::vector<double> days;
        for (double day = 0; day < 365; day += .5)
            days.push_back(day);
        for (const double middle : {46., 138., 229.5, 320.})
            days.push_back(middle);
        double lo = 9, hi = 0;
        for (const double day : days)
        {
            const double f = market::seasonFactor(g.price, day);
            lo = std::min(lo, f), hi = std::max(hi, f);
        }
        for (const double region : {d.regionMade, d.regionNear, d.regionFar})
        {
            const double up = market::FactorHigh / (hi * region), down = market::FactorLow / (lo * region);
            expect(up >= Up - 1e-12, g.id + " at its dearest (x" + str(hi) + ", region " + str(region) + "): room up x" + str(up));
            expect(down <= Down + 1e-12, g.id + " at its cheapest (x" + str(lo) + ", region " + str(region) + "): room down x" + str(down));
            if (up < leastUp)
                tightestUp = g.id;
            if (1 / down < leastDown)
                tightestDown = g.id;
            leastUp = std::min(leastUp, up), leastDown = std::min(leastDown, 1 / down);
        }
        // Each good of the group is priced by that group (and so by these numbers).
        for (const auto& good : g.goods)
            expect(market::seasonOf(good) == &g, good + " priced by " + g.id + "'s season");
    }
    std::cout << "  room for pressure at the seasons' ends: up x" << leastUp << " (" << tightestUp << "), down x"
              << 1 / leastDown << " (" << tightestDown << ")\n";
}

void theMorningRestock()
{
    // A keeper first at work in the market day sets its wares out to their shelf targets: what it sells, not what it only
    // buys; once a market day. A market with no route refills everything each morning.
    auto s = world({{"baker", "baker at The Loaf", "ridgemere"}}, {});
    const auto till = s.tillOf("baker");
    const auto ada = player(s, "player-ada");
    const int target = s.shelfTarget(till, "bread");
    expect(target >= 1, "a shelf target: " + std::to_string(target));
    expect(!s.settingOut("baker"), "not setting out before the restock");
    expect(s.stagedRestock("baker"), "the restock set something out");
    const int out = Society::stock(*s.account(till), "bread");
    expect(out >= target, "set out this morning: " + std::to_string(out));
    // The keeper is seen setting out the wares for a few minutes (7.7), then not.
    expect(s.settingOut("baker"), "the keeper setting out the wares");
    const double opened = s.marketClock();
    s.setMarketClock(opened + market::SettingOutDays / 2);
    expect(s.settingOut("baker"), "still, a few minutes on");
    s.setMarketClock(opened + market::SettingOutDays + 1. / 1440);
    expect(!s.settingOut("baker"), "done after a quarter of an hour");
    expect(s.trade(ada, "baker", "bread", out, true).ok && Society::stock(*s.account(till), "bread") == 0, "bought out");
    expect(!s.stagedRestock("baker"), "nothing set out again");
    expect(Society::stock(*s.account(till), "bread") == 0, "not again the same market day (it set out this morning)");
    s.setMarketClock(std::floor(s.marketClock()) + 1 + 9. / 24);
    s.stagedRestock("baker");
    const int after = Society::stock(*s.account(till), "bread");
    expect(after == s.shelfTarget(till, "bread") && after >= 1, "the next morning, to its target: " + std::to_string(after));
    expect(s.realBalanced(), "no money moved");
}

void theBoardRoundTrips()
{
    auto s = world({{"baker", "baker at The Loaf", "ridgemere"}}, {{"ridgemere", 620}});
    stock(s, "baker", "bread", 200);
    const auto ada = player(s, "player-ada");
    expect(s.trade(ada, "baker", "bread", 30, true, "acct:ada").ok, "bought");
    s.setProsperity(-40);
    s.discountPaid("chapter:c1", 12);
    const auto j = market::encode(s.board());
    const auto back = market::decode(j);
    expect(json::dump(market::encode(back)) == json::dump(j), "the Board encodes and decodes the same");
    // Lenient: garbage is dropped, numbers clamped, never refused.
    json::Value bad;
    std::string error;
    json::parse(R"({"slider": 9999, "cell": {"ridgemere": {"bread": {"P": 7, "touched": 1}, "no_such_good": {"P": 0.2}}},
                   "budgetLeft": {"till:x": "abc"}, "recent": [1, {"market": "m", "good": "bread", "day": 1, "delta": "x"}]})",
                bad, error);
    const auto lenient = market::decode(bad);
    expect(lenient.slider == 100 && lenient.cell.at("ridgemere").at("bread").P == market::PressureMost &&
               !lenient.cell.at("ridgemere").count("no_such_good") && lenient.budgetLeft.empty(),
           "garbage read leniently: " + json::dump(market::encode(lenient)));
    expect(market::decode(json::Value()).empty(), "nothing reads as an empty Board");
    // The slider heads to the DM's by ten a day; "now" skips it.
    s.setMarketClock(std::floor(s.marketClock()) + 1 + market::DawnHour / 24);
    s.marketTick(s.marketClock());
    expect(s.board().slider == -10, "ten a day: " + std::to_string(s.board().slider));
    s.setProsperity(60, true);
    expect(s.board().slider == 60, "now");
}

void theFactionDiscountCap()
{
    auto s = world({{"baker", "baker at The Loaf", "ridgemere"}}, {{"ridgemere", 620}});
    expect(s.discountRoom("c1") == 30, "30p a Chapter a week");
    s.discountPaid("c1", 25);
    expect(s.discountRoom("c1") == 5 && s.discountRoom("c2") == 30, "a Chapter's own");
    s.discountPaid("c1", 25);
    expect(s.discountRoom("c1") == 0, "never under nothing");
    s.setMarketClock(s.marketClock() + 7);
    expect(s.discountRoom("c1") == 30, "a new week");
}

void theOnePath()
{
    // The Board's prices are the only ones (doc 79, Phase 3: the rule `staged` and the conserving economy's shopFactor are
    // gone): a quote is the ladder's, a trade moves the Board and saves it, and the smithy buys ore (D5) in any world.
    auto s = world({{"baker", "baker at The Loaf", "ridgemere"}}, {});
    stock(s, "baker", "bread", 50);
    const auto ada = player(s, "player-ada");
    const auto q = s.quote(ada, "baker", "bread", 12, true);
    const auto l = s.listing(ada, "baker", "bread");
    expect(q.ok && l.listCount == 12 && q.total == l.listPrice, "the quote is the Board's dozen: " + std::to_string(q.total));
    expect(s.trade(ada, "baker", "bread", 12, true).ok && !s.board().empty(), "a trade moves the Board");
    expect(wire::society(s.state()).has("board"), "and it is saved");
    auto t = world({{"smith", "the smith at the forge", "ridgemere"}}, {});
    const auto w = t.wares("smith");
    expect(std::find(w.begin(), w.end(), "iron_ore") != w.end(), "the smithy buys ore (market.json `buys`)");
    // A world's old rule record of the conserving economy's is skipped, never refused (doc 79, 14).
    AuthoredRoster roster = s.authored();
    roster.economy.rules["staged"] = 0;
    roster.economy.rules["steerLean"] = 150;
    Society u(Roster::None);
    u.configure(roster);
    expect(u.rule("steerLean") == -1 && u.rule("staged") == -1 && u.rule("ripple") == 50, "the old rules are no rules now");
}

// ----------------------------------------------------------------------------------------- The world finds its markets

#include "town_fixture.h"

void theWorldsMarkets()
{
    const auto root = fs::temp_directory_path() / "ratw-p12stage-market-tests";
    const auto worldPath = writeStrip(root / "world");
    fs::remove_all(root / "save");
    Town town(worldPath, (root / "save").string());
    auto& w = town.g.world();
    // Found at load (5.1): the world's turn finds them before any player trades or stands at a shop, so the dawn's
    // ripples and the morning restock read the real markets, never Society's one-market stand-in.
    town.run(3);
    expect(w.society().tradePoints().fromWorld, "the markets found by the world's own turn, before any trade");
    expect(w.society().stagedHooked(), "the Board's Times, pressure and shelves feed the bands in play (World::ensureTradePoints)");
    w.ensureTradePoints();
    const auto& t = w.society().tradePoints();
    expect(t.fromWorld && t.markets.size() == 3 && t.find("ridgemere") && t.find("larkfield") && t.find("oakhaven"),
           "three markets, each its community's id: " + std::to_string(t.markets.size()));
    const auto* r = t.find("ridgemere");
    expect(r->tills.size() == 2 && r->routes.size() == 2 && r->routes.front() == "larkfield",
           "Ridgemere's two shops, its routes nearest first: " + (r->routes.empty() ? std::string() : r->routes.front()));
    expect(near(r->catchment, double(w.society().households(true).size()) / 3 + market::PassingTrade, 1.01),
           "its catchment its households and passing trade: " + str(r->catchment));
    expect(r->made.count("flour") && r->made.count("eggs"), "the stall's supplies made in Ridgemere (crafts.json `supplies`)");
    // A trade through the world moves its own market's Board.
    town.enter(town.ash, 1, "player-ash", "Ash", 1, 8.5);
    auto& s = w.society();
    const auto till = s.tillOf("ui");
    s.create(till, "meal", 20, "test");
    const auto bought = s.trade(town.ash.entityId, "ui", "meal", 5, true, "acct:ash");
    expect(bought.ok && s.pressure("ridgemere", "meal") > 0 && near(s.pressure("larkfield", "meal"), 0), "Ridgemere's meal dearer, not Larkfield's: " + bought.message);
    // The shop list a player sees (7.1): BUYING TODAY, the dozen, the sell lot, the trend. At ten in the morning, beside
    // the inn's keeper at its post.
    const double now = w.calendarDays();
    w.advanceCalendar(std::floor(now) + 1 + 10. / 24 - now);
    town.run(3);
    const auto* keeper = w.entity("ui");
    expect(keeper != nullptr, "the inn's keeper");
    town.place(town.ash, 1, keeper->position.x, keeper->position.y + 1);
    town.run(1);
    const auto self = town.selfOf(town.ash);
    const auto& m = town.ash.snapshots.back()["merchant"];
    expect(m.isObject() && m.boolean("staged") && m.number("buyingToday", -1) >= 0,
           "the keeper's list is the staged one, with its budget: " + json::dump(m).substr(0, 300));
    bool meal = false;
    for (const auto& item : m.array("items"))
        if (item.string("id") == "meal")
        {
            meal = true;
            const auto row = s.listing(town.ash.entityId, "ui", "meal", town.g.accountOf(town.ash.entityId));
            expect(item.number("listCount", 0) == 1 && item.number("buyLotPrice", 0) == double(row.buy.total) &&
                       item.number("sellLot", 0) == row.sellLot && item.number("sellLotPrice", -1) == double(row.sell.total) &&
                       item.number("trend", 9) == row.trend && item.string("trendCause") == row.cause && row.sellLot >= 1,
                   "a meal's row, the society's listing for this wolf: " + json::dump(item));
        }
    expect(meal, "meals on the list");
    (void)self;
    // The faction discount's weekly cap through the Game's own path (7.8, D28: Game::afterFactionTrade): a Chapter sworn
    // to the keeper's faction gets 25% back, but never more than factionDiscountMost (30p) a week, however much it buys.
    town.enter(town.bo, 2, "player-bo", "Bo", 1, 9.5);
    town.enter(town.cy, 3, "player-cy", "Cy", 1, 10.5);
    auto& ch = town.g.chapters();
    ch.propose(town.ash.entityId, {town.bo.entityId, town.cy.entityId}, "Ashen Lodge", "#5b8bd9", "", "scene-x", 0);
    ch.agree(town.bo.entityId, 0);
    const auto lodge = ch.agree(town.cy.entityId, 0).message;
    expect(!lodge.empty() && ch.of(town.ash.entityId), "the Lodge founded");
    town.g.factions().define({"guild", "The Innkeepers' Guild", "#9aa9c0", "city"});
    town.g.factions().setMember("ui", "guild", "", true);
    town.g.factions().change("guild", lodge, 90, "test", w.calendarDays(), 0, false);
    s.create(till, "meal", 400, "test");
    expect(testPurse(s, town.ash.entityId, std::min<std::int64_t>(400, s.cash("treasury"))) &&
               s.cash(town.ash.entityId) >= 300,
           "a purse for the Lodge's buying: " + std::to_string(s.cash(town.ash.entityId)) + " (the treasury " + std::to_string(s.cash("treasury")) + ")");
    const auto room = s.discountRoom(lodge);
    expect(room == s.marketRules().discountMost && room == 30, "a whole week's cap: " + std::to_string(room));
    town.ash.events.clear();
    std::int64_t told = 0;
    int trades = 0;
    for (int i = 0; i < 12; ++i)
    {
        town.g.command(&town.ash, cmd({{"type", "trade"}, {"target", "ui"}, {"item", "meal"}, {"quantity", 6}, {"buy", true}}));
        town.run(.25);
        const auto said = town.ash.said();
        for (auto at = said.find(", "); at != std::string::npos; at = said.find(", ", at + 1))
            if (const auto end = said.find(" back.", at); end != std::string::npos && end - at < 8)
                told += std::stoll(said.substr(at + 2, end - at - 2));
        trades += contains(said, "Bought 6");
        town.ash.events.clear();
    }
    const auto& week = s.board().discountWeek;
    expect(trades >= 5, "the Lodge's buying went through: " + std::to_string(trades) + " (each a lot of " +
                            std::to_string(s.quote(town.ash.entityId, "ui", "meal", 6, true).total) + "p)");
    expect(week.count(lodge) && week.at(lodge) == told && told == 30 && s.discountRoom(lodge) == 0,
           "25% back, capped at the week's 30p (told " + std::to_string(told) + ", paid " +
               std::to_string(week.count(lodge) ? week.at(lodge) : -1) + ")");
    fs::remove_all(root);
}

// The bands name markets as the Board does (doc 79, 5.1 and 6.5): in a world of three towns, a household in each town
// has its town's market, and one living out in the wilds between towns (lawTown "") the nearest market by path, so the
// pressure players put on its staple reaches its short days.
void householdsInTheBoardsMarkets()
{
    const auto root = fs::temp_directory_path() / "ratw-p12stage-market-homes";
    const auto worldPath = writeStrip(root / "world");
    {
        std::ofstream manifest(fs::path(worldPath) / "world.ratw", std::ios::app);
        manifest << resident("wx", "Cotter x", "civilian", "working", 6, 4.5, 6, 4.5);   // (The wilds between Larkfield and Oakhaven.)
    }
    fs::remove_all(root / "save");
    // Through a restart (9.2): straight after the load, before any turn, the households have the Board's markets again
    // and each food shop's provision sits on its own till, never on Society's one-market stand-in ("realm") until midnight.
    const auto provisions = [](const Society& so) {
        std::map<std::string, int> out;
        std::set<std::string> foods;
        for (const auto& row : so.statusTable().rows)
            foods.insert(row.food.begin(), row.food.end());
        for (const auto& m : so.tradePoints().markets)
            for (const auto& till : m.tills)
                for (const auto& f : foods)
                    if (const int n = so.provisionFor(till, f); n > 0)
                        out[m.id + "/" + till + "/" + f] = n;
        return out;
    };
    const auto describe = [](const std::map<std::string, int>& p) {
        std::string out;
        for (const auto& [k, n] : p)
            out += k + "=" + std::to_string(n) + " ";
        return out;
    };
    std::map<std::string, int> before;
    {
        Town town(worldPath, (root / "save").string());
        auto& w = town.g.world();
        auto& s = w.society();
        town.run(3);
        expect(w.towns().size() == 3 && w.lawTown(cellId(6)).empty() && w.lawTown(cellId(4)) == "larkfield",
               "three towns, the wilds in none: " + std::to_string(w.towns().size()));
        const auto marketOf = [&](const std::string& id) {
            const auto* st = s.statusOf(id);
            return st ? st->market : std::string("(no band)");
        };
        expect(marketOf("u1") == "ridgemere" && marketOf("s1") == "larkfield" && marketOf("r1") == "oakhaven",
               "each town's households its own market: " + marketOf("u1") + " " + marketOf("s1") + " " + marketOf("r1"));
        expect(marketOf("wx") == "larkfield", "the cotter in the wilds the nearest market by path: " + marketOf("wx"));
        expect(marketOf("um") == s.marketOf(s.tillOf("um")), "a keeper's household and its till in one market");
        s.setPressure("larkfield", "meal", .5, "test");
        expect(Society::boardHooks().pressure(s, marketOf("wx"), "meal") > .4,
               "the pressure on the cotter's staple reaches its short days");
        before = provisions(s);
        expect(!before.empty(), "the food shops' provisions before the save");
        town.g.save();
    }
    {
        Town again(worldPath, (root / "save").string());
        auto& s2 = again.g.world().society();
        const auto marketAgain = [&](const std::string& id) {
            const auto* st = s2.statusOf(id);
            return st ? st->market : std::string("(no band)");
        };
        expect(s2.tradePoints().fromWorld, "the world's markets at once after the load");
        expect(marketAgain("u1") == "ridgemere" && marketAgain("s1") == "larkfield" && marketAgain("r1") == "oakhaven" &&
                   marketAgain("wx") == "larkfield",
               "after a restart each household its market at once: " + marketAgain("u1") + " " + marketAgain("s1") + " " +
                   marketAgain("r1") + " " + marketAgain("wx"));
        const auto after = provisions(s2);
        expect(after == before, "and each food shop's provision on its own till: " + describe(after) + "(was " + describe(before) + ")");
        again.run(3);
        expect(marketAgain("s1") == "larkfield" && provisions(s2) == before, "and still so after the world's turns");
    }
    fs::remove_all(root);
}

// ------------------------------------------------------------------- The Board through the Game's saves (16.7)

// The Board's fields a trade moves: what the journal must carry and a save keep.
const char* const TradeFields[] = {"cell", "budgetLeft", "takenToday", "pushed", "netToday", "soldToday", "discountWeek"};

std::string slurpText(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    std::ostringstream o;
    o << in.rdbuf();
    return o.str();
}

json::Value parseText(const std::string& text, const std::string& what)
{
    json::Value v;
    std::string error;
    if (!json::parse(text, v, error))
        throw std::runtime_error(what + " is not JSON: " + error);
    return v;
}

struct BoardSave
{
    std::string replayed, expected;   // The checkpoint with its journal replayed over it; the live Board's encoding.
};

// A world saved; then players' trades on the Board (pressure, the budget and allowance drawn,
// the holder's push, the day's net and sell total, a Chapter's weekly discount), journalled after the save and never
// saved whole. The journal replayed over the checkpoint gives the live Board back, and so does a server started from
// the save and its journal.
BoardSave aBoardThroughTheGame()
{
    const auto root = fs::temp_directory_path() / ("ratw-p12stage-board-" + std::to_string(::getpid()));
    fs::remove_all(root);
    const auto worldPath = writeStrip(root / "world");
    const auto save = (root / "save.json").string();
    BoardSave out;
    std::string savedText;
    {
        Town town(worldPath, save);
        auto& w = town.g.world();
        auto& s = w.society();
        town.enter(town.ash, 1, "player-ash", "Ash", 1, 8.5);
        w.setTimeOfDay(12);
        // The keepers come to their posts and set out the day's wares, seen doing it (7.7), for a few minutes only.
        bool seen = false;
        for (int i = 0; i < 120 && !seen; ++i)
        {
            town.run(.25);
            const auto* keeper = w.entity("ui");
            seen = keeper && contains(keeper->activity, "Setting out the wares");
        }
        expect(seen, "the inn's keeper seen setting out the wares: " + (w.entity("ui") ? w.entity("ui")->activity : std::string("none")));
        w.advanceCalendar(1. / 24);
        town.run(30);
        expect(!contains(w.entity("ui")->activity, "Setting out the wares"), "and an hour on, done: " + w.entity("ui")->activity);
        s.create(s.tillOf("ui"), "meal", 30, "test stock");
        s.create(town.ash.entityId, "berries", 20, "test: foraged");
        town.g.save();                              // The checkpoint the journal follows.
        savedText = slurpText(save);
        const auto* keeper = w.entity("ui");
        town.place(town.ash, 1, keeper->position.x + 1, keeper->position.y);
        town.ash.events.clear();
        town.g.command(&town.ash, cmd({{"type", "trade"}, {"target", "ui"}, {"item", "meal"}, {"quantity", 4}, {"buy", true}}));
        town.g.settle();
        s.discountPaid("chapter-test", 7);          // (Journalled with the next trade's record.)
        town.g.command(&town.ash, cmd({{"type", "trade"}, {"target", "ui"}, {"item", "berries"}, {"quantity", 12}, {"buy", false}}));
        town.g.settle();
        const auto said = town.ash.said();
        const auto live = market::encode(s.board());
        for (const char* k : TradeFields)
            expect(live[k].isObject() && !live[k].fields().empty(), std::string("the trades moved the Board's ") + k + ": " + said +
                                                                         json::dump(live).substr(0, 600));
        out.expected = json::dump(live);
        expect(slurpText(save) == savedText, "no save was taken after the trades (the journal is the tail)");
        // (a) The journal replayed over the checkpoint, as the package tool and the server read it.
        std::vector<journal::Record> records;
        std::string problem;
        expect(journal::readFile(save + ".journal", 0, records, problem), "the journal reads back: " + problem);
        auto document = parseText(savedText, "the save");
        std::uint64_t last = 0;
        expect(journal::replay(document, records, std::uint64_t(document.number("journal")), last, problem),
               "the journal replays over the checkpoint: " + problem);
        PersistedWorld saved;
        checkpoint::ServerState server;
        expect(checkpoint::decode(document, saved, server, problem), "the replayed save decodes: " + problem);
        const auto replayed = market::encode(saved.society.board);
        for (const char* k : TradeFields)
            expect(json::dump(replayed[k]) == json::dump(live[k]), std::string("the journal carried the Board's ") + k + ": " +
                                                                       json::dump(replayed[k]).substr(0, 300) + " vs " +
                                                                       json::dump(live[k]).substr(0, 300));
        out.replayed = json::dump(checkpoint::encode(saved, server, saved.npcs, saved.time));
    }
    {
        // (b) A server started from the save and its journal (Game::start: replayed, decoded, restored).
        Town again(worldPath, save);
        const auto loaded = market::encode(again.g.world().society().board());
        const auto live = parseText(out.expected, "the live Board");
        for (const char* k : TradeFields)
            expect(json::dump(loaded[k]) == json::dump(live[k]), std::string("a server from the save and journal has the Board's ") + k);
    }
    fs::remove_all(root);
    return out;
}

void theBoardThroughTheGame()
{
    (void)aBoardThroughTheGame();
}

void theHolderBeforeTheWelcome()
{
    // D23 through the Game (doc 79): character creation (makeWolf) and entry (enterCharacter) set the account holder
    // before the wolf is added, so the welcome is counted by the account: one account's two wolves made in one week get
    // one welcome's coins between them, tallied under the account, never under a wolf. The holder isn't saved, so a
    // server started again sets it at entry.
    const auto root = fs::temp_directory_path() / ("ratw-p12stage-holder-" + std::to_string(::getpid()));
    fs::remove_all(root);
    const auto worldPath = writeStrip(root / "world");
    const auto save = (root / "save.json").string();
    const auto options = [&] {
        game::Options o;
        o.hiddenNames = false;
        o.forkSnapshots = false;
        o.tiesOptional = true;
        o.oneWolfPerAccount = false;
        o.worldExport = worldPath;
        o.savePath = save;
        return o;
    };
    const auto appearance = parseText(R"({"species": "timber", "sex": "female", "stature": "average", "pattern": "solid", "baseColor": 3,
                                          "gradientColor": 1, "markingColor": 5, "gradientAmount": 0.5, "patternAmount": 0.5})",
                                      "an appearance");
    std::vector<std::string> wolves;
    {
        game::Game g(options());
        std::string problem;
        expect(g.start(problem), "starts: " + problem);
        auto& s = g.world().society();
        Client c;
        c.id = 1;
        g.connect(&c);
        g.command(&c, cmd({{"type", "auth_register"}, {"username", "bob"}, {"password", "a long enough password"}}));
        g.settle();
        for (int i = 0; i < 2; ++i)
            g.command(&c, cmd({{"type", "character_create"}, {"name", i ? "Bo Two" : "Bo One"}, {"age", 24}, {"appearance", appearance},
                               {"commandId", "bob" + std::to_string(i)}}));
        g.settle();
        expect(c.last("lobby") != nullptr, "a lobby for bob");
        for (const auto& ch : c.last("lobby")->array("characters"))
            wolves.push_back(ch.string("id"));
        expect(wolves.size() == 2, "two wolves for bob: " + c.last("lobby")->string("message"));
        // (The welcome's coins as the door tallied them today, by holder: faucetToday adds the goods' worth too.)
        const auto welcomeCoins = [&s](const std::string& holder) {
            std::int64_t n = 0;
            for (const auto& [day, holders] : s.state().staged.faucets)
                if (const auto h = holders.find(holder); h != holders.end())
                    if (const auto w = h->second.find("welcome"); w != h->second.end())
                        n += w->second.made;
            return n;
        };
        std::int64_t coins = 0;
        for (const auto& id : wolves)
        {
            expect(s.holderOf(id) == "bob", "made: the wolf's holder is its account: " + s.holderOf(id));
            expect(s.faucetToday(id, "welcome") == 0 && welcomeCoins(id) == 0, "no welcome tallied under the wolf itself: " + id);
            coins += s.cash(id);
        }
        const auto welcome = s.statusTable().caps.count("welcomeCoins") ? s.statusTable().cap("welcomeCoins") : Society::PlayerWelcome;
        expect(welcomeCoins("bob") == welcome, "one welcome's coins tallied under the account: " + std::to_string(welcomeCoins("bob")));
        expect(coins == welcome, "one welcome's coins between the account's two wolves: " + std::to_string(coins));
        g.command(&c, cmd({{"type", "character_enter"}, {"id", wolves[0]}}));
        g.settle();
        expect(g.world().entity(wolves[0]) && s.holderOf(wolves[0]) == "bob", "entered: still the account's");
        g.save();
    }
    {
        game::Game g(options());
        std::string problem;
        expect(g.start(problem), "starts again: " + problem);
        auto& s = g.world().society();
        expect(s.holderOf(wolves[1]) == wolves[1], "(the holder isn't saved)");
        Client c;
        c.id = 1;
        g.connect(&c);
        g.command(&c, cmd({{"type", "auth_login"}, {"username", "bob"}, {"password", "a long enough password"}}));
        g.settle();
        g.command(&c, cmd({{"type", "character_enter"}, {"id", wolves[1]}}));
        g.settle();
        expect(g.world().entity(wolves[1]) != nullptr, "the second wolf entered after the restart");
        expect(s.holderOf(wolves[1]) == "bob", "entry sets the holder again: " + s.holderOf(wolves[1]));
        expect(s.faucetToday(wolves[1], "welcome") == 0, "and nothing welcomed under the wolf at entry");
    }
    fs::remove_all(root);
}

// --write-board DIR: the replayed checkpoint (board.json) and the live Board (board.expected.json), for
// Tests/market_pg.py to save through game.save_checkpoint into a throwaway database and load back.
int writeBoard(const fs::path& dir)
{
    const auto b = aBoardThroughTheGame();
    fs::create_directories(dir);
    std::ofstream(dir / "board.json", std::ios::binary) << b.replayed;
    std::ofstream(dir / "board.expected.json", std::ios::binary) << b.expected;
    return 0;
}

// --check-board BACK DIR: the checkpoint as Postgres gave it back (BACK/board.json) decodes to the same Board, and the
// whole checkpoint re-encodes the same.
int checkBoard(const fs::path& back, const fs::path& dir)
{
    PersistedWorld after;
    checkpoint::ServerState server;
    std::string problem;
    const auto text = slurpText(dir / "board.json");
    expect(checkpoint::decode(parseText(slurpText(back / "board.json"), "the checkpoint from Postgres"), after, server, problem),
           "decodes after Postgres: " + problem);
    const auto board = market::encode(after.society.board);
    const auto expected = parseText(slurpText(dir / "board.expected.json"), "the live Board");
    for (const char* k : TradeFields)
        expect(!board[k].fields().empty() && json::dump(board[k]) == json::dump(expected[k]),
               std::string("the Board's ") + k + " the same through Postgres");
    // The same content (Postgres's jsonb keeps no key order, and the sections the server keeps as it was given, the
    // people's and the factions', come back in its order: json::Value's == compares objects whatever their order).
    const auto again = checkpoint::encode(after, server, after.npcs, after.time);
    const auto before = parseText(text, "the checkpoint written");
    if (!(again == before))
    {
        // (Which sections differ, and in the society which fields.)
        std::string differ;
        for (const auto& [k, v] : before.fields())
            if (!(v == again[k]))
            {
                differ += " " + k;
                if (k == "society")
                    for (const auto& [f, x] : v.fields())
                        if (!(x == again[k][f]))
                            differ += "/" + f;
            }
        expect(false, "the whole checkpoint the same through Postgres; differs in" + differ);
    }
    std::cout << "market_tests --check-board: the Board the same through Postgres (" << checks << " checks)\n";
    return 0;
}
} // namespace

int main(int argc, char** argv)
{
    try
    {
        if (argc == 3 && std::string(argv[1]) == "--write-board")
            return writeBoard(argv[2]);
        if (argc == 4 && std::string(argv[1]) == "--check-board")
            return checkBoard(argv[2], argv[3]);
        everyGoodHasADepthGroup();
        thePieces();
        breadInRidgemere();
        aLonePlayersDayInColdwater();
        hidesAtColdwater();
        anOreDumpRipples();
        churnLeavesNoRipple();
        churnAcrossHolders();
        theShopsLimits();
        worthlessLotsAndTheDozen();
        seasonsFitTheClamp();
        seasonsLeavePressureRoom();
        theMorningRestock();
        theBoardRoundTrips();
        theFactionDiscountCap();
        theOnePath();
        theWorldsMarkets();
        householdsInTheBoardsMarkets();
        theBoardThroughTheGame();
        theHolderBeforeTheWelcome();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Market tests passed: " << checks << " checks.\n";
    return 0;
}
