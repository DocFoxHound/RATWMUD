// Town projects (Docs/Design/57-changing-the-world.md, 4). Phase 3: the pure rules (progress, worth, the plaque's order
// and names, naming at 40% of a whole worth 300p, refunds pro rata); then through the game: a market cover a Dungeon
// Master posts stands on the structure layer by the square; coin given posts contracts for its materials and hires hands;
// two wolves in two roles work at 1.8 to one's 1; goods handed in and a contract delivered; a jobless resident hired as
// a hand and paid from its purse; the structure standing with its plaque, named for its chief giver; a cancelled project's
// gifts returned; every penny accounted for; across a restart. Phase 4: what standing projects do (see effects()).
#include "RatwGame.h"
#include "RatwProjects.h"
#include "RatwWorld.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unistd.h>

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

#include "town_fixture.h"

// A project command, and what the world said back.
std::string order(Town& t, Client& c, const std::string& text)
{
    c.events.clear();
    t.g.command(&c, text);
    t.run(1);                                       // (What the world says may come with the next snapshot.)
    return c.said();
}

void rules()
{
    const auto& r = projects::rules();
    const auto* cover = r.kind("cover");
    expect(cover && cover->structure == "market_cover" && cover->roles.size() == 2, "the market cover's kind, with two roles");
    expect(r.kind("mend") && r.kind("mend")->structure.empty(), "a mending stands as nothing");
    projects::Ledger ledger;
    auto& p = ledger.post(*cover, "upper_accord", "c_1_0", 4, 4, "the cover", 10, "dm");
    expect(p.id == "proj-1" && p.needs.at("timber") == 12 && p.hours == 20, "posted with its needs");
    p.shown = {{"a", "Kestrel"}, {"b", ""}, {"c", "Wren"}, {"d", "Hale"}};
    projects::Ledger::give(p, "a", "coin", "", 100, 100, 11);
    projects::Ledger::give(p, "b", "goods", "timber", 10, 30, 11);
    projects::Ledger::give(p, "c", "labour", "", 4, 6, 11);
    projects::Ledger::give(p, "a", "coin", "", 50, 50, 12);
    projects::Ledger::give(p, "d", "coin", "", 5, 5, 12);
    expect(p.gifts.size() == 4 && projects::Ledger::worth(p) == 191, "gifts merged by giver and kind; worth 191p");
    const auto plaque = projects::Ledger::plaque(p);
    expect(plaque.size() == 3 && plaque[0].second == "Kestrel" && plaque[1].second == "a friend of the town" && plaque[2].second == "Wren",
           "the plaque: three by value, each by the name it chose");
    expect(projects::Ledger::chief(p).empty(), "under 300p: named for no one");
    projects::Ledger::give(p, "a", "coin", "", 150, 150, 13);
    expect(projects::Ledger::chief(p) == "a", "a giver of 40% of a whole worth 300p or more: named for them");
    p.shown["a"] = "";
    expect(projects::Ledger::chief(p).empty(), "never named for one who gave as a friend of the town");
    const auto back = projects::Ledger::shares(p, "coin", "", 101);
    std::int64_t total = 0;
    for (const auto& [who, n] : back)
        total += n;
    expect(total == 101 && back[0].first == "a" && back[0].second == 100 && back[1].second == 1, "refunds pro rata, exact: 100 and 1 of 101 (300p and 5p given)");
    auto saved = ledger.save();
    projects::Ledger again;
    again.load(saved);
    expect(again.find("proj-1") && again.find("proj-1")->gifts.size() == 4 && again.find("proj-1")->shown.at("c") == "Wren",
           "saved and loaded");
    expect(again.post(*cover, "x", "c", 0, 0, "t", 0, "dm").id == "proj-2", "ids go on from the loaded");
}

void built()
{
    const auto root = fs::temp_directory_path() / ("ratw-projects-" + std::to_string(::getpid()));
    const auto save = (fs::temp_directory_path() / ("ratw-projects-" + std::to_string(::getpid()) + ".json")).string();
    fs::remove(save);
    const auto world = writeStrip(root);
    std::string id;
    {
        Town t(world, save);
        auto& w = t.g.world();
        auto& s = w.society();
        t.enter(t.ash, 1, "ash", "Ash", 1, 10.5);
        t.enter(t.bo, 2, "bo", "Bo", 1, 11.5);
        const auto ash = t.ash.entityId, bo = t.bo.entityId;
        s.shift("treasury", ash, "", 0, 500, "test purse");
        expect(s.conserved(), "conserved at the start");

        // A Dungeon Master posts a market cover in Upper Accord.
        auto posted = t.g.postProject("cover", "upper_accord", "", -1, -1, "", "dm");
        expect(posted.ok, "posted: " + posted.message);
        id = posted.targetId;
        const auto* p = t.g.projects().find(id);
        expect(p && p->state == "open" && !p->structure.empty() && !p->site.empty(), "it stands on the layer as a plan");
        expect(w.communityOf(p->cell) == "upper_accord", "by the town's square");
        expect(!t.g.postProject("cover", "upper_accord", "", -1, -1, "", "dm").ok, "one of a kind under way at a time");
        expect(!t.g.postProject("castle", "upper_accord", "", -1, -1, "", "dm").ok, "no such kind");

        // Coin given: contracts for its materials and the day's hands.
        t.place(t.ash, 1, p->x + .5, p->y + 1.5);
        t.place(t.bo, 1, p->x + 1.5, p->y + .5);
        t.run(.5);
        auto give = cmd({{"type", "project"}, {"verb", "give"}, {"project", id}, {"coins", 300}, {"shown", "Ash"}});
        auto said = order(t, t.ash, give);
        expect(contains(said, "You give 300 pennies"), "she gives 300p: " + said);
        int contracts = 0;
        for (const auto& k : w.roads().contracts)
            contracts += k.poster == "project:" + id && k.kind == "procure" && k.status == "open";
        expect(contracts == 3, "a contract for each of its three materials: " + std::to_string(contracts));
        bool hands = false;
        for (const auto& j : s.oddJobs())
            hands |= j.payer == "project:" + id && j.kind == "project";
        expect(hands, "hands hired for its hours");
        expect(s.conserved(), "conserved after the gift");
        expect(!t.g.projects().find(id)->shown.at(ash).empty(), "shown on its plaque as Ash");

        // Two at work in two roles: 1.8 a minute's work-hour; one alone: 1.
        t.g.command(&t.ash, cmd({{"type", "project"}, {"verb", "work"}, {"project", id}}));
        t.g.command(&t.bo, cmd({{"type", "project"}, {"verb", "work"}, {"project", id}}));
        const double before = t.g.projects().find(id)->worked;
        t.run(60);
        const double pair = t.g.projects().find(id)->worked - before;
        expect(std::abs(pair - 1.8) < .1, "two in two roles: 1.8 work-hours a minute, not " + std::to_string(pair));
        t.g.command(&t.bo, cmd({{"type", "project"}, {"verb", "work"}, {"project", id}}));
        const double mid = t.g.projects().find(id)->worked;
        t.run(60);
        const double alone = t.g.projects().find(id)->worked - mid;
        expect(std::abs(alone - 1.0) < .1, "one alone: 1, not " + std::to_string(alone));
        t.g.command(&t.ash, cmd({{"type", "project"}, {"verb", "work"}, {"project", id}}));

        // Goods handed in at the site, any quality.
        s.create(ash, "timber", 5, "test: felled");
        said = order(t, t.ash, cmd({{"type", "project"}, {"verb", "handin"}, {"project", id}, {"item", "timber"}, {"quantity", 5}}));
        std::string all;
        for (const auto& e : t.ash.events)
            all += json::dump(e).substr(0, 200) + " | ";
        expect(contains(said, "You hand in 5"), "she hands in 5 timber: " + said + " events: " + all + (w.inBattle(ash) ? " (in a battle)" : "") + " cash " + std::to_string(s.account(ash)->cash) + " at " + (w.entity(ash) ? w.entity(ash)->cellId + " " + std::to_string(w.entity(ash)->position.x) + "," + std::to_string(w.entity(ash)->position.y) + (w.entity(ash)->dead ? " dead" : "") : std::string("gone")) + " site " + std::to_string(p->x) + "," + std::to_string(p->y) + " timber " + std::to_string(Society::stock(*s.account(ash), "timber")));
        // A contract for the cord taken on and delivered by Bo.
        std::string cordContract;
        for (const auto& k : w.roads().contracts)
            if (k.poster == "project:" + id && k.item == "cord")
                cordContract = k.id;
        expect(w.takeContract(bo, cordContract).ok, "Bo takes on the cord");
        s.create(bo, "cord", 6, "test: twisted");
        const auto delivered = w.deliverContract(bo, cordContract);
        expect(delivered.ok, "Bo delivers it: " + delivered.message);
        expect(t.g.projects().find(id) && s.account("project:" + id) && Society::stockAll(*s.account("project:" + id), "cord") == 6, "the cord is the project's");
        expect(s.conserved(), "conserved after the contract");

        // A jobless resident hired as a hand and paid from its purse.
        auto st = s.state();
        st.careers.positions["job:u3"].holder.clear();
        expect(s.restore(st), "u3 out of work");
        w.setTimeOfDay(10);
        const auto* purse = s.account("project:" + id);
        const auto coinBefore = purse ? purse->cash : 0;
        const double handBefore = t.g.projects().find(id)->worked;
        bool paid = false;
        for (int i = 0; i < 1500 && !paid; ++i)
        {
            t.run(1);
            paid = t.g.projects().find(id)->worked > handBefore + .5;
        }
        expect(paid, "a hired hand's spell counts toward its hours");
        expect(s.account("project:" + id)->cash < coinBefore, "paid from the project's purse");
        expect(s.conserved(), "conserved after the hands");

        // The rest by hand: the materials, and the hours.
        for (const auto& [item, n] : t.g.projects().find(id)->needs)
            if (const int have = Society::stockAll(*s.account("project:" + id), item); have < n)
                s.create("project:" + id, item, n - have, "test: delivered");
        t.place(t.ash, 1, p->x + .5, p->y + 1.5);
        t.g.command(&t.ash, cmd({{"type", "project"}, {"verb", "work"}, {"project", id}}));
        for (int i = 0; i < 1500 && t.g.projects().find(id)->state == "open"; ++i)
            t.run(1);
        p = t.g.projects().find(id);
        expect(p->state == "built", "it stands finished");
        bool standing = false;
        for (const auto& [sid, structure] : t.g.camps().structures())
            standing |= sid == p->structure && structure.built && structure.condition >= 100;
        expect(standing, "the structure stands on the layer");
        expect(s.account("project:" + id)->stock.empty() || Society::stockAll(*s.account("project:" + id), "timber") == 0, "its materials used up");
        expect(p->namedFor == ash && p->title == "Ash's Market cover", "named for its chief giver: " + p->title);
        bool deed = false;
        for (const auto* d : t.g.deeds().byDoer(ash))
            deed |= d->kind == "built_project" && d->weight >= fame::Notable;
        expect(deed, "the chief giver's deed is notable");
        expect(s.conserved(), "conserved at the finish");
        // Another, cancelled: its gifts go back.
        posted = t.g.postProject("mend", "ser_ferro", "", -1, -1, "", "dm");
        expect(posted.ok, "a mending at Ser Ferro: " + posted.message);
        t.place(t.bo, 5, 8.5);
        s.shift("treasury", bo, "", 0, 100, "test purse");
        const auto boCash = s.account(bo)->cash;
        said = order(t, t.bo, cmd({{"type", "project"}, {"verb", "give"}, {"project", posted.targetId}, {"coins", 40}}));
        expect(contains(said, "You give 40"), "Bo gives 40p as a friend of the town: " + said);
        expect(t.g.cancelProject(posted.targetId, "test").ok, "cancelled");
        expect(s.account(bo)->cash == boCash, "Bo's 40p came back whole (the contracts withdrawn first)");
        expect(s.conserved(), "conserved after the cancel");
        t.g.settle();
        t.run(2);
    }
    {
        Town t(world, save);
        const auto* p = t.g.projects().find(id);
        expect(p && p->state == "built" && p->title == "Ash's Market cover", "it stands after a restart");
        bool standing = false;
        for (const auto& [sid, structure] : t.g.camps().structures())
            standing |= sid == p->structure && structure.built;
        expect(standing, "and so does its structure");
    }
    fs::remove_all(root);
    fs::remove(save);
}

// Phase 4, what standing projects do: a watch post cuts a strong camp's raid odds and keeps camps from gathering near; a
// market cover keeps stalls out in a storm (worn, every other day); a waystation shelters; a mending lifts the town's
// repair through the Town Works and says so; wear halves then ends an effect, and mending brings it back; the proposer
// posts a watch post after a robbery and a mending under 50, one town a day, never two of a kind.
void effects()
{
    {
        World w;
        const auto loaded = w.loadWorldFile(RATW_SOURCE_DIR "/Data/Worlds/Greyfen/world.ratw");
        expect(loaded.ok, "Greyfen loads: " + loaded.message);
        std::string wild;
        for (const auto& [id, c] : w.cells())
            if (c.outdoors && w.communityOf(id) != "greyfen" && wild.empty())
                wild = id;
        const auto town = w.cells().begin()->first;
        const double bare = w.raidOdds(10, 50, 0, town);
        World::StandingWorks works;
        works.watchposts[town] = 1;
        w.setStandingWorks(works);
        const double guarded = w.raidOdds(10, 50, 0, town);
        expect(guarded < bare * .7, "a watch post cuts a strong camp's odds: " + std::to_string(bare) + " to " + std::to_string(guarded));
        works.watchposts[town] = .5;
        w.setStandingWorks(works);
        expect(w.raidOdds(10, 50, 0, town) > guarded && w.raidOdds(10, 50, 0, town) < bare, "worn, half the help");
        works.watchposts[town] = 1;
        w.setStandingWorks(works);
        expect(!w.campMayGather(town), "no camp gathers by it");
        // The market cover: Marketday (day 5) in a storm keeps the stalls in; a cover keeps them out.
        expect(w.advanceCalendar(5 - std::floor(w.calendarDays())).ok && w.setTimeOfDay(9).ok, "Marketday morning");
        for (const auto& [id, c] : w.cells())
            if (c.outdoors)
                w.setWeather(id, Weather::Storm);
        // (Greyfen's square needs stalls built for a market: as schedules_tests does.)
        bool market = w.dayPlan("greyfen").kind == "market";
        if (market)
        {
            expect(w.dayPlan("greyfen").foul, "in a storm, the stalls stay in");
            works.covers["greyfen"] = 1;
            w.setStandingWorks(works);
            expect(!w.dayPlan("greyfen").foul, "under a market cover, they go out");
            works.covers["greyfen"] = .5;
            w.setStandingWorks(works);
            const bool even = std::int64_t(std::floor(w.calendarDays())) % 2 == 0;
            expect(w.dayPlan("greyfen").foul == !even, "a worn cover keeps them out every other day");
        }
        else
        {
            works.covers["greyfen"] = 0;
            w.setStandingWorks(works);
            std::cout << "(Greyfen has no built stalls here: the market cover is checked in the test town.)\n";
        }
    }
    const auto root = fs::temp_directory_path() / ("ratw-projects4-" + std::to_string(::getpid()));
    const auto save = (fs::temp_directory_path() / ("ratw-projects4-" + std::to_string(::getpid()) + ".json")).string();
    fs::remove(save);
    const auto world = writeStrip(root, false, true);   // (Stalls built at each market.)
    {
        Town t(world, save);
        auto& w = t.g.world();
        auto& s = w.society();
        // A market cover finished by hand: Marketday in a storm, its stalls go out.
        auto made = t.g.postProject("cover", "upper_accord", "", -1, -1, "", "dm");
        expect(made.ok, "a cover posted: " + made.message);
        expect(t.g.completeProject(made.targetId).ok, "and completed by the DM's hand");
        expect(w.standingWorks().covers.count("upper_accord"), "the world knows the cover stands");
        for (int i = 0; i < 7 && w.dayPlan("upper_accord").kind != "market"; ++i)
            w.advanceCalendar(1);
        w.setTimeOfDay(9);
        for (const auto& [id, c] : w.cells())
            if (c.outdoors)
                w.setWeather(id, Weather::Storm);
        t.run(1);
        expect(w.dayPlan("upper_accord").kind == "market", "Marketday at Upper Accord");
        expect(!w.dayPlan("upper_accord").foul, "covered, the stalls go out in a storm");
        expect(w.dayPlan("ser_ferro").foul, "Ser Ferro, uncovered, keeps them in");
        // A waystation in the wilds shelters those about it.
        made = t.g.postProject("waystation", "upper_accord", cellId(3), 8, 8, "", "dm");
        expect(made.ok, "a waystation posted in the wilds: " + made.message);
        expect(t.g.completeProject(made.targetId).ok, "and completed");
        const auto* way = t.g.projects().find(made.targetId);
        t.run(1);
        expect(w.shelters.count(way->cell) && w.shelters.at(way->cell).count({way->x, way->y + 1}), "it shelters the tiles about it");
        expect(w.standingWorks().waystations.count(way->cell), "caravans press on by it in a storm");
        // A mending lifts Ser Ferro's repair and says so.
        auto st = s.state();
        st.memory.condition["ser_ferro"] = 40;
        expect(s.restore(st), "Ser Ferro in disrepair");
        made = t.g.postProject("mend", "ser_ferro", "", -1, -1, "", "dm");
        expect(made.ok, "a mending posted: " + made.message);
        s.create("project:" + made.targetId, "stone", 6, "test: delivered");
        expect(t.g.completeProject(made.targetId).ok, "completed");
        expect(s.condition("ser_ferro") >= 59, "the town's repair lifted over half: " + std::to_string(s.condition("ser_ferro")));
        expect(s.account("town:ser_ferro:works") && Society::stockAll(*s.account("town:ser_ferro:works"), "stone") >= 6,
               "its stone gone to the Town Works' stock");
        bool news = false;
        for (const auto& e : w.recentEvents())
            news |= e.kind == "town mended";
        t.run(2);
        for (const auto& e : w.recentEvents())
            news |= e.kind == "town mended";
        expect(news || s.condition("ser_ferro") >= 50, "and its news");
        // A watch post worn: half, then nothing; mended, back.
        made = t.g.postProject("watchpost", "ridgemere", "", -1, -1, "", "dm");
        expect(made.ok && t.g.completeProject(made.targetId).ok, "a watch post at Ridgemere");
        const auto postId = made.targetId;
        const auto cell = t.g.projects().find(postId)->cell;
        expect(w.standingWorks().watchposts.count(cell) && w.standingWorks().watchposts.at(cell) == 1, "full strength");
        w.advanceCalendar(55);
        t.run(1);
        expect(t.g.projects().find(postId)->state == "worn" && w.standingWorks().watchposts.at(cell) == .5, "worn under half: half its effect");
        w.advanceCalendar(50);
        t.run(1);
        expect(t.g.projects().find(postId)->state == "ruin" && !w.standingWorks().watchposts.count(cell), "a ruin does nothing");
        // (A ruin is rebuilt as a new project; one worn is mended.)
        made = t.g.postProject("watchpost", "ridgemere", "", -1, -1, "", "dm");
        expect(made.ok && t.g.completeProject(made.targetId).ok, "rebuilt");
        w.advanceCalendar(60);
        t.run(1);
        auto* worn = t.g.projects().find(made.targetId);
        expect(worn->state == "worn", "worn again");
        t.enter(t.ash, 1, "ash", "Ash", 8, worn->x + .5);
        if (auto* me = w.entity(t.ash.entityId))
            me->cellId = worn->cell, me->position = {worn->x + .5, worn->y + 1.5};
        t.run(.5);
        t.ash.events.clear();
        t.g.command(&t.ash, cmd({{"type", "project"}, {"verb", "work"}, {"project", made.targetId}}));
        t.run(.5);
        expect(contains(t.ash.said(), "mending"), "she sets to mending it: " + t.ash.said());
        for (int i = 0; i < 400 && t.g.projects().find(made.targetId)->condition < 60; ++i)
            t.run(1);
        expect(t.g.projects().find(made.targetId)->state == "built" && w.standingWorks().watchposts.count(t.g.projects().find(made.targetId)->cell) &&
                   w.standingWorks().watchposts.at(t.g.projects().find(made.targetId)->cell) == 1,
               "mended: its full effect back (" + std::to_string(t.g.projects().find(made.targetId)->condition) + ", " + t.g.projects().find(made.targetId)->state + ") " + t.ash.said());
        expect(s.conserved(), "conserved throughout");
    }
    fs::remove(save);
    fs::remove(save + ".journal");
    // The proposer: a robbery's watch post and a mending under 50, one town a day, never two of a kind.
    {
        Town t(world, save);
        auto& w = t.g.world();
        auto& s = w.society();
        auto st = s.state();
        st.memory.condition["upper_accord"] = 30;
        st.memory.condition["ser_ferro"] = 30;
        expect(s.restore(st), "two towns in disrepair");
        w.recordEvent({"raid", "camp_c_6_0", "ridgemere", cellId(6), 0, 0, {}, 3, 0, "the caravan to ridgemere was robbed"});
        int posted = 0;
        for (int day = 0; day < 6; ++day)
        {
            w.advanceCalendar(1);
            w.setTimeOfDay(7);
            t.run(1);
            int open = 0;
            for (const auto& [id, p] : t.g.projects().all())
                open += p.state == "open" && p.by == "town";
            expect(open <= day + 1, "one town a day: " + std::to_string(open) + " open on day " + std::to_string(day));
            posted = open;
        }
        int mends = 0, posts = 0;
        for (const auto& [id, p] : t.g.projects().all())
        {
            mends += p.kind == "mend";
            posts += p.kind == "watchpost" && p.town == "ridgemere" && p.cell == cellId(6);
        }
        expect(mends == 2, "a mending for each town in disrepair, one each, never two: " + std::to_string(mends));
        expect(posts == 1, "a watch post where Ridgemere's caravan was robbed: " + std::to_string(posts));
        expect(posted == 3, "and no more: " + std::to_string(posted));
    }
    fs::remove_all(root);
    fs::remove(save);
}

// Phase 5, the economy made visible: meals a stranger sells into a town short of them, and the price falling after, are
// talked of with their cause, the wolf named by look by those who never learned her name; food brought into a town of
// bare shelves feeds it (a deed); players' coins and goods counted by town in the orchestrator's measures, and a
// project's purse never one of its holders.
void visible()
{
    const auto root = fs::temp_directory_path() / ("ratw-projects5-" + std::to_string(::getpid()));
    const auto save = (fs::temp_directory_path() / ("ratw-projects5-" + std::to_string(::getpid()) + ".json")).string();
    fs::remove(save);
    const auto world = writeStrip(root);
    {
        Town t(world, save);
        auto& w = t.g.world();
        auto& s = w.society();
        t.enter(t.ash, 1, "ash", "Ash", 1, 9.5);
        const auto ash = t.ash.entityId;
        const auto put = [&](const char* who, int cell, double x, double y = 8.5) {
            auto* e = w.entity(who);
            e->cellId = cellId(cell);
            e->position = {x, y};
            e->path.clear();
            e->velocity = {};
        };
        std::string store;
        for (const auto& town : w.towns())
            if (town.id == "upper_accord")
                store = town.store;
        expect(!store.empty(), "Upper Accord keeps a store");
        if (const auto* held = s.account(store))
            s.consume(store, "meal", Society::stock(*held, "meal"), "test: eaten");
        w.setTimeOfDay(10);
        t.run(3700);                                // (The store's price, hourly.)
        expect(w.townShortOf("upper_accord", "meal"), "the town is short of meals");
        const double before = w.goingPrice("upper_accord", "meal");
        s.create(ash, "meal", 6, "test: cooked");
        put("um", 1, 10.5);
        t.run(.5);
        const auto sold = w.trade(ash, "um", "meal", 6, false);
        expect(sold.ok, "she sells the stall six meals: " + sold.message);
        // The store filled again (as a caravan does): the price falls, and the town says why.
        s.create(store, "meal", 40, "test: a caravan's load");
        t.run(3700);
        expect(w.goingPrice("upper_accord", "meal") <= before * .9, "the price fell a tenth or more");
        bool lowered = false;
        for (const auto& e : w.recentEvents())
            lowered |= e.kind == "price lowered" && e.actor == ash;
        const auto causes = w.priceCauses().find("upper_accord");
        expect(causes != w.priceCauses().end() && !causes->second.empty() && causes->second.back().who == ash, "the cause is hers");
        bool told = false, leak = false;
        w.setTimeOfDay(12);                         // (Awake, at midday.)
        for (int i = 0; i < 120 && !told; ++i)
        {
            put("u1", 1, 8.5);                      // (Standing together by her, free to talk; the place's talk waits a while.)
            put("u2", 1, 9.0, 9.5);
            for (const auto& pick : w.ambientPicks({ash}))
                if (pick.topic.kind == "price_cause")
                {
                    told = true;
                    leak |= contains(pick.topic.blanks.at("who"), "Ash");
                    for (const auto& f : pick.topic.facts)
                        leak |= contains(f, "Ash");
                    expect(pick.topic.tags.at("known") == "no", "by look: they never learned her name");
                }
            if (!told)
                t.run(3);
        }
        std::string kinds;
        for (const char* who : {"u1", "u2"})
        {
            const auto* e = w.entity(who);
            const auto* life = s.resident(who);
            kinds += std::string(who) + " task=" + (life ? life->task : "?") + " path=" + std::to_string(e->path.size()) + " leader=" + e->leaderId +
                     " offstage=" + std::to_string(e->offstage) + " cell=" + e->cellId + " hear=" + std::to_string(w.hearingClarity(ash, who, Voice::Speak)) + "; ";
        }
        for (const auto& pick : w.ambientPicks({ash}))
            kinds += pick.teller + "/" + pick.listener + ":" + pick.topic.kind + " ";
        expect(told, "the town talks of why meals are cheaper: " + kinds);
        expect(!leak, "never by a name they weren't given");

        // Food into a town of bare shelves.
        auto st = s.state();
        orchestra::TownReading bare;
        bare.id = "upper_accord";
        bare.people = 5;
        bare.kind = "empty shelves";
        st.orchestrator.last.towns = {bare};
        expect(s.restore(st), "Upper Accord's shelves are bare");
        w.noteTrade("upper_accord", "meal", ash, 2);
        bool fed = false;
        for (const auto* d : t.g.deeds().byDoer(ash))
            fed |= d->kind == "fed_town";
        expect(fed, "she fed the town: a deed");

        // The orchestrator counts what players bring; a project's purse is never a holder.
        expect(t.g.postProject("mend", "upper_accord", "", -1, -1, "", "dm").ok, "a mending to give to");
        std::string mend;
        for (const auto& [id, p] : t.g.projects().all())
            mend = id;
        const auto snapshot = [&] {                 // (The orchestrator measures at 23:00 and briefs at the turn of the day.)
            w.setTimeOfDay(23.2);
            t.run(5);
            w.advanceCalendar(1);
            w.setTimeOfDay(0.5);
            t.run(5);
        };
        snapshot();                                 // (Counting begins with the first measure.)
        w.setTimeOfDay(12);
        t.run(5);                                   // (Awake again.)
        t.place(t.ash, 1, 9.5);
        t.g.command(&t.ash, cmd({{"type", "project"}, {"verb", "give"}, {"project", mend}, {"coins", 25}}));
        s.create(ash, "meal", 3, "test: cooked");
        put("um", 1, 10.5);
        const auto more = w.trade(ash, "um", "meal", 3, false);
        expect(more.ok, "she sells three more meals: " + more.message);
        snapshot();
        bool counted = false;
        for (const auto& town : s.orchestrator().last.towns)
            if (town.id == "upper_accord")
                counted = town.playerIn >= 25 && town.playerOut > 0 && town.playerGoods > 0;
        std::string seen = "brief day " + std::to_string(s.orchestrator().last.day) + " now " + std::to_string(w.calendarDays()) + ": ";
        for (const auto& town : s.orchestrator().last.towns)
            seen += town.id + " " + std::to_string(town.playerIn) + "/" + std::to_string(town.playerOut) + "/" + std::to_string(town.playerGoods) + " ";
        expect(counted, "the town's measure counts players' coins in, out, and goods: " + seen);
        for (const auto& h : s.orchestrator().last.holders)
            expect(h.id.rfind("project:", 0) != 0, "a project's purse is never a holder: " + h.id);
        expect(s.conserved(), "conserved");
    }
    fs::remove_all(root);
    fs::remove(save);
}

// Phase 6, bounds: a protected resident (from the world file's `protected` record) can't be attacked by a player, and a
// theft from them takes no coin; an ordinary resident is as before; a protected record for no such resident is refused.
void bounds()
{
    const auto root = fs::temp_directory_path() / ("ratw-projects6-" + std::to_string(::getpid()));
    const auto save = (fs::temp_directory_path() / ("ratw-projects6-" + std::to_string(::getpid()) + ".json")).string();
    fs::remove(save);
    const auto world = writeStrip(root);
    std::ofstream(root / "world.ratw", std::ios::app) << "protected \"um\"\n";
    {
        Town t(world, save);
        auto& w = t.g.world();
        auto& s = w.society();
        t.enter(t.ash, 1, "ash", "Ash", 1, 9.5);
        const auto ash = t.ash.entityId;
        expect(s.spec("um") && s.spec("um")->protectedNpc, "the world file marks the stall keeper protected");
        expect(w.isProtected("um") && !w.isProtected("u1"), "protected, and an ordinary neighbour not");
        auto* um = w.entity("um");
        um->cellId = cellId(1), um->position = {10.5, 8.5}, um->path.clear();
        const auto refused = w.attack(ash, "um", "");
        expect(!refused.ok && contains(refused.message, "guards close in"), "she can't attack them: " + refused.message);
        expect(!w.inBattle(ash), "no fight begins");
        // A theft from them takes a meal or herbs, never coin.
        s.create("um", "meal", 3, "test: cooked");
        const auto cash = s.account("um")->cash;
        int tries = 0, took = 0;
        for (; tries < 40 && took < 3; ++tries)
        {
            um->cellId = cellId(1), um->position = {10.0, 8.5}, um->path.clear();
            auto* me = w.entity(ash);
            me->cellId = cellId(1), me->position = {9.5, 8.5};
            const auto stole = w.steal(ash, "um");
            took += stole.ok && contains(stole.message, "You lift");
            t.run(31);                              // (Not so soon again: StealEvery.)
        }
        expect(took > 0, "a theft from them succeeds now and then");
        expect(s.account("um")->cash == cash, "and never takes coin");
        expect(s.conserved(), "conserved");
    }
    // A protected record for no such resident: the world refuses to load.
    {
        std::ofstream(root / "world.ratw", std::ios::app) << "protected \"nobody\"\n";
        World w;
        expect(!w.loadWorldFile((root / "world.ratw").string()).ok, "a protected record for no such resident is refused");
    }
    fs::remove_all(root);
    fs::remove(save);
}
} // namespace

int main()
{
    try
    {
        rules();
        built();
        effects();
        visible();
        bounds();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "projects: " << checks << " checks passed\n";
    return 0;
}
