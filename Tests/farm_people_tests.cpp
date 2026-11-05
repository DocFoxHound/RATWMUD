// The farms' people (Docs/Design/71-the-country-s-industries.md, Phase C; the user, 2026-11-03): real NPCs replace the
// farms' virtual crews. With every area on and its farms founded, a day's careers place them, by rule, from the unplaced
// NPCs (never making one): each farm's keeper by old trade (the oldest of its trades, with family among the unplaced
// where one has), the keeper's family with them at the farmhouse and nobody else; the hands of the farm's trades (the
// fallback trades for what they can't fill), housed in the farm's community at most six to a home, or near the city as
// doc 68's arrivals are; each community's market from the towns' trades and shops. Each real post replaces one virtual
// post and is paid as it was; the crew's founding purse goes to those who take its posts (no money made but a market
// shop's float, counted as the farms' founding); a post left empty is a hand short and the farm yields in proportion;
// the community's store sells to its households. Doc 72's works (the woods' and the peat's): their crews by the same rule,
// the woods' old trade first, keepers of those with no family among the unplaced first and alone (no farmhouse), living
// with the hands in their community or the city, their posts out of doors at the works; Oakhaven's market staffed with
// its works. Phase 3: the stone's works of the mines, stone and clay (a clay pit's one hand, no keeper), and a works'
// empty post filled again, its holders judged near from its crew's home ground.
//
// The catalogue is the real Data (RATW_SOURCE_DIR): country_farms.json, communities.json and farm_people.json with it.
#include "RatwCalendar.h"
#include "RatwItems.h"
#include "RatwSociety.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
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
    {
        r.home = r.work = r.evening = {home, 2.5, 2.5};
        r.workLabel = label;
    }
    return r;
}

// A place's community: a community's interiors (cn_<id>_...), a farm's steading (its community), a works' cell out of
// doors the realm's own region (as the world's cells are, doc 72), else the city.
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
    std::set<std::string> unplacedAtStart, residentsAtStart;
};

// A city of a few working households, and an unplaced pool of every old trade (more than the farms want of most, two
// fewer of the orchards and vines than they want, so the fallback is used), with a family or two and a House's.
void build(Fixture& x)
{
    AuthoredRoster r;
    // The city: a stall to keep its own shops, and lower households in the Rents.
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
    // The pool.
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
    add("keeps the house", 39, 20);                // (The workshops' 39 posts since the still's hands, doc 73: 37 before.)
    add("serves at The Lantern", 12, 20);
    add("helps at The Stores", 10, 20);
    add("logging at Cedarfall", 5, 20);
    add("logging at Oakfell", 3, 22);
    add("cutting stone at Greyscar", 4, 24);         // (Doc 72, Phase 3: the granite quarry's 1 + 1 since country3,)
    add("digs clay at Clayholt", 3, 22);             // (and the clay pits' one hand each.)
    add("digs ore at Deepvein", 3, 30);              // (The Frosthollow mine's 1 + 1, and one over,)
    add("works the lime kilns at Greyscar", 1, 40);  // (the lime quarry's one hand,)
    add("logging at Highpine", 3, 26);               // (and the Upper hearths' 1 + 2: doc 72, iron and lime.)
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
    one("still_cw", "brewer at The Copper Worm", 44, "male");   // (The still's keeper: doc 72, after iron6.)
    // A family: the oldest of the fields (57), the spouse who keeps the house, a child, a grown son of the fields (a hand).
    one("rook", "farms the downs", 57, "male");
    one("iris", "keeps the house", 55);
    one("tor", "plays about the town", 11, "male");
    one("hob", "works the fields at Wheatley", 25, "male");
    r.kin = {{"iris", "rook", "spouse"}, {"rook", "tor", "parent"}, {"iris", "tor", "parent"}, {"rook", "hob", "parent"}};
    // The oldest of the woods, with a spouse among the unplaced: a works' keeper is one with no family first (doc 72).
    one("ash", "logging at Snowpine", 55, "male");
    one("wren", "keeps the house", 53);
    r.kin.push_back({"ash", "wren", "spouse"});
    // A House's: oldest of the orchards and vines, but the Heights' own (a spouse at home in the Heights).
    one("bo", "selling Fell wine", 56, "male");
    one("nessa", "at home in the Heights", 50);
    r.kin.push_back({"bo", "nessa", "spouse"});
    // Protected: never placed.
    auto guarded = person("guarded", "works the fields at Wheatley", 58);
    guarded.protectedNpc = true;
    r.unplaced.push_back(guarded);
    r.economy.treasury = 20000;
    r.economy.storeRule = true;
    r.economy.rules = {{"stockPrices", 0}, {"shelfFollow", 0}, {"wageDrift", 0}, {"treasuryDrift", 0}, {"surplus", 0}, {"patrons", 0}};
    for (const auto& a : items::farms().areas)
        r.economy.farms.areas[a.id].on = 1;   // (Every area on from the first day.)
    for (const auto& u : r.unplaced)
        x.unplacedAtStart.insert(u.id);
    for (const auto& u : r.residents)
        x.residentsAtStart.insert(u.id);
    x.s.configure(r);
    LifeDay day;
    day.communityOf = communityOf;
    x.s.setDay(day);
    auto& s = x.s;
    x.world.alive = [&s](const std::string& id) { return s.resident(id) != nullptr; };
    x.world.age = [&s](const std::string& id) { return s.spec(id) ? s.spec(id)->age : 30; };
    x.world.regard = [](const std::string&, const std::string&) { return 50.; };
    x.world.near = [](const std::string& work, const std::string& home) { return communityOf(work) == communityOf(home); };
    // (The city's lodging house, within reach of work in the city only: as the world's are, by region.)
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

void theData()
{
    const auto& people = items::farmPeople();
    expect(people.loaded && people.farms.size() == 42 && people.communities.size() == 7,
           "communities.json and farm_people.json read: 31 farms and doc 72's 11 works, 7 communities: " + std::to_string(people.farms.size()));
    int works = 0;
    for (const auto& f : people.farms)
        if (f.works)
        {
            ++works;
            expect(f.home.area.empty() && f.workplaces.size() == 1 && f.workplaces[0].area.rfind("w_ridgemere_", 0) == 0 &&
                       f.workplaces[0].spots.size() >= 6,
                   f.farm + ": a works, no farmhouse, its work out of doors in its cell");
        }
    expect(works == 11, "eleven works: the fuel's five, the stone's three (doc 72, Phase 3), Frosthollow's three (iron and lime)");
    // Every farm's labels: at most 40 characters, no business's word, no "dog".
    Fixture x;
    build(x);
    for (const auto& f : x.s.farms())
        for (int post = 0; post < 3; ++post)
        {
            const auto label = x.s.farmLabel(f, post);
            expect(!label.empty() && label.size() <= 40, f.def->id + "'s label: " + label);
            expect(!items::businessFor(label), f.def->id + "'s label is no business's: " + label);
            expect(label.find("dog") == std::string::npos, "no dogs: " + label);
        }
    for (const auto& c : people.communities)
        for (const auto& m : c.market)
            for (const auto& [post, count] : m.posts)
            {
                const auto rule = people.market.find(post);
                expect(rule != people.market.end(), "a rule for the market's post " + post);
                std::string label = rule->second.label;
                if (const auto at = label.find("{name}"); at != std::string::npos)   // (The striker's has none.)
                    label.replace(at, 6, m.name);
                expect(label.size() <= 40, "a market label: " + label);
                const auto* business = items::businessFor(label);
                if (rule->second.role == "merchant")
                    expect(business && business->id == m.business, label + " is its business's (" + m.business + ")");
                else
                    expect(!business, label + " is no business's");
            }
    // No community is a town yet (`town` false: the user, 2026-11-03): each its own region, kept plain in a world with
    // its places (World::setupTowns), none in a world without them (the old map's towns of the same names); one made a
    // town (a later step) is no longer kept plain.
    for (const auto& c : people.communities)
        expect(!c.town && c.region == c.id, c.id + " is no town yet, its own region");
    const auto all = people.plainRegions([](const std::string&) { return true; });
    expect(all == std::set<std::string>({"coldwater", "larkfield", "southmeadow", "fenshore", "saltwell", "oakhaven", "frosthollow"}), "the seven kept plain");
    expect(people.plainRegions([](const std::string&) { return false; }).empty(), "none without their places");
    expect(people.plainRegions([](const std::string& area) { return area == "cn_saltwell_inn"; }) == std::set<std::string>({"saltwell"}),
           "a community by any of its places");
    auto later = people;
    later.communities.front().town = true;
    expect(!later.plainRegions([](const std::string&) { return true; }).count(later.communities.front().region),
           "a community made a town is no longer kept plain");
}

void thePlacement()
{
    Fixture x;
    build(x);
    auto& s = x.s;
    run(s, 1 - .5 / 24, 1 + .5 / 24);               // (The farms founded at the day's turn.)
    for (const auto& f : s.farms())
        expect(s.account(f.till) && s.account(f.crew) && f.heads() == f.crewSize(), f.def->id + " founded, all its crew virtual");
    const auto mintedBefore = s.state().minted;
    std::int64_t crewPurses = 0;
    for (const auto& f : s.farms())
        crewPurses += s.cash(f.crew);
    const auto residentsBefore = s.state().residents.size();
    const auto unplacedBefore = s.unplacedCount();
    s.tendCareers(2, x.world);
    auto requests = s.takeRequests();
    expect(s.state().residents.size() == residentsBefore, "asking makes nobody");
    std::set<std::string> ids;
    for (const auto& r : requests)
    {
        expect(r.kind == "farm", "each a farm placement");
        expect(x.unplacedAtStart.count(r.templateId) && s.unplaced(r.templateId), "from the unplaced pool: " + r.templateId);
        expect(ids.insert(r.templateId).second, "each once: " + r.templateId);
        expect(r.templateId != "guarded" && r.templateId != "bo" && r.templateId != "nessa", "never the protected or the Heights' own");
    }
    // The host carries them out.
    std::map<std::string, std::string> oldLabel;
    for (const auto& r : requests)
        oldLabel[r.templateId] = s.unplaced(r.templateId)->workLabel;
    int placed = 0;
    for (const auto& r : requests)
        if (s.arrive(r))
        {
            s.welcome(r, r.templateId);
            ++placed;
        }
    expect(placed == int(requests.size()), "every placement carried out");
    expect(s.unplacedCount() == unplacedBefore - requests.size() && s.state().residents.size() == residentsBefore + requests.size(),
           "they leave the pool and become residents, one for one: nobody made");
    for (const auto& [id, life] : s.state().residents)
        expect(x.residentsAtStart.count(id) || x.unplacedAtStart.count(id), "no resident but the authored and the pool: " + id);
    // Every post filled: each real post replaced one virtual post.
    int keepers = 0, hands = 0, market = 0, family = 0;
    for (const auto& f : s.farms())
    {
        expect(f.postKeepers == f.keepers && f.postHands == f.hands && f.heldKeepers == f.keepers && f.heldHands == f.hands,
               f.def->id + "'s posts all real and held: " + std::to_string(f.postKeepers) + "+" + std::to_string(f.postHands));
        expect(f.heads() == 0 && f.virtualWages() == 0 && std::abs(f.working() - 1) < 1e-9, f.def->id + ": no virtual post left");
        keepers += f.keepers, hands += f.hands;
    }
    expect(keepers == 39 && hands == 93, "39 keepers and 93 hands placed (doc 72's works' 8 and 17 among them): " +
                                              std::to_string(keepers) + ", " + std::to_string(hands));
    // Where they live: a farmhouse its keeper's family's alone; hands in their farm's community (or the city's homes), at
    // most six to a home.
    const auto& people = items::farmPeople();
    std::map<std::string, std::vector<std::string>> homes;
    for (const auto& [id, life] : s.state().residents)
        homes[life.homeCell].push_back(id);
    for (const auto& [home, members] : homes)
        expect(members.size() <= 6 || Society::communalHome(home), "at most six to a home: " + home + " " + std::to_string(members.size()));
    for (const auto& site : people.farms)
    {
        if (site.works)
            continue;                               // (Doc 72's works: theirKeepersAndCrews.)
        const auto& f = farm(s, site.farm);
        std::string keeper;
        for (const auto& p : s.positions())
        {
            bool isKeeper = false;
            if (s.farmOfPost(p, &isKeeper) >= 0 && s.farms()[std::size_t(s.farmOfPost(p))].def == f.def && isKeeper)
                keeper = s.state().careers.positions.at(p.id).holder;
            if (s.farmOfPost(p) >= 0 && s.farms()[std::size_t(s.farmOfPost(p))].def == f.def)
            {
                const auto pay = s.payOf(p);
                expect(pay.kind == "farm" && pay.payer == f.till && pay.wage == (isKeeper ? f.keeper : f.hand),
                       p.title + ": paid by its farm's till at its fixed wage");
            }
        }
        expect(!keeper.empty() && s.resident(keeper)->homeCell == site.home.area, site.farm + "'s keeper lives at its farmhouse");
        for (const auto& id : homes[site.home.area])
            expect(id == keeper || s.family(id, keeper) || s.authored().kin.end() != std::find_if(s.authored().kin.begin(), s.authored().kin.end(),
                                                                                                    [&](const Kin& k) { return (k.a == id && k.b == keeper) || (k.b == id && k.a == keeper) ||
                                                                                                                               (k.kind == "spouse" && (k.a == keeper || k.b == keeper)); }),
                   site.farm + "'s farmhouse: its keeper and family only (" + id + ")");
        family += int(homes[site.home.area].size()) - 1;
        for (const auto& p : s.positions())
            if (s.farmOfPost(p) >= 0 && s.farms()[std::size_t(s.farmOfPost(p))].def == f.def)
            {
                const auto& holder = s.state().careers.positions.at(p.id).holder;
                if (holder == keeper)
                    continue;
                const auto& home = s.resident(holder)->homeCell;
                if (site.community == "ridgemere")
                    expect(communityOf(home) == "ridgemere" && home.rfind("fs_", 0) != 0, site.farm + "'s hand lives in the city: " + home);
                else
                    expect(home.rfind("cn_" + site.community + "_", 0) == 0 || home == site.home.area,
                           site.farm + "'s hand lives in " + site.community + " (or is family at the farmhouse): " + home);
            }
    }
    // The family: the oldest of the fields keeps a grain farm, the spouse and child with them, the grown son a hand.
    const auto* rook = s.resident("rook");
    expect(rook && rook->homeCell.rfind("fs_grain_", 0) == 0 && s.jobOf("rook") && s.jobOf("rook")->title.rfind("farms ", 0) == 0,
           "the oldest of the fields keeps a grain farm, at its farmhouse");
    for (const auto* id : {"iris", "tor", "hob"})
        expect(s.resident(id) && s.resident(id)->homeCell == rook->homeCell, std::string(id) + " lives with them");
    expect(!s.jobOf("tor") && !s.jobOf("iris") && s.jobOf("hob") && s.farmOfPost(*s.jobOf("hob")) >= 0, "the grown son a hand there");
    expect(family >= 3, "families at the farmhouses: " + std::to_string(family));
    // The fallback: two of the orchards' and vineyards' posts from those who keep the house or serve.
    int fallback = 0;
    for (const auto& r : requests)
        if ((r.farm.rfind("orchard", 0) == 0 || r.farm.rfind("vineyard", 0) == 0) && r.workLabel != "-")
        {
            const auto trade = people.tradeOf(oldLabel.at(r.templateId));
            expect(trade == "orchards and vines" || trade == "keeps the house" || trade == "service and labour",
                   "an orchard's or vineyard's hand of its trade or the fallback's: " + oldLabel.at(r.templateId));
            fallback += trade != "orchards and vines";
        }
    expect(fallback == 2, "two of the orchards' and vines' posts from the fallback trades: " + std::to_string(fallback));
    // The grain farms' hemp and flax (doc 72, Phase 1): a hand more each, 1 + 5, all of the fields and mills (17 of them
    // in the pool, the 12 posts theirs), the extra hands housed in Larkfield and Southmeadow.
    for (const auto* id : {"grain_farm_1", "grain_farm_2"})
    {
        int posts = 0, ofFields = 0;
        for (const auto& r : requests)
            if (r.farm == id && r.workLabel != "-")
            {
                ++posts;
                ofFields += people.tradeOf(oldLabel.at(r.templateId)) == "fields and mills";
            }
        expect(posts == 6 && ofFields == 6, std::string(id) + ": six posts, all of the fields and mills: " + std::to_string(posts) + ", " +
                                                std::to_string(ofFields));
        const auto& g = farm(s, id);
        expect(g.hands == 5 && g.heldHands == 5, std::string(id) + "'s five hands held");
    }
    // Each farm's people of its own trades but for the fallback's (a keeper's family apart).
    for (const auto& r : requests)
        if (const auto* site = people.farm(r.farm); site && r.workLabel != "-")
        {
            const auto& trades = people.kinds.at(farm(s, site->farm).def->kind).trades;   // (A works' site kind is the map's.)
            const auto trade = people.tradeOf(oldLabel.at(r.templateId));
            expect(std::find(trades.begin(), trades.end(), trade) != trades.end() ||
                       std::find(people.fallback.begin(), people.fallback.end(), trade) != people.fallback.end(),
                   r.farm + " takes " + oldLabel.at(r.templateId));
        }
    // The markets: each community's businesses staffed, a shop's keeper of the old town of its name first.
    // (Doc 72, Phase 5: the country workshops' posts apart, every building of theirs standing in this world.)
    std::map<std::string, std::string> keeperOf;
    std::map<std::string, int> atWorkshop;
    for (const auto& p : s.positions())
        if (p.work.cell.rfind("cn_", 0) == 0)
        {
            ++(people.workshop(p.work.cell) ? atWorkshop[p.work.cell] : market);
            if (p.role == "merchant")
                keeperOf[p.work.cell] = s.state().careers.positions.at(p.id).holder;
        }
    expect(market == 21, "the seven markets' 21 posts (Oakhaven's two and Frosthollow's two with their works, doc 72): " + std::to_string(market));
    int workshopPosts = 0;
    for (const auto& w : people.workshops)
    {
        int want = 0;
        for (const auto& [post, n] : w.posts)
            want += n;
        expect(atWorkshop[w.area] == want, w.area + ": its " + std::to_string(want) + " posts: " + std::to_string(atWorkshop[w.area]));
        workshopPosts += atWorkshop[w.area];
    }
    expect(people.workshops.size() == 9 && workshopPosts == 39, "the nine workshops' 39 posts (the still since iron6, doc 72; its two more hands, doc 73): " + std::to_string(workshopPosts));
    expect(!keeperOf["cn_oakhaven_store"].empty(), "Oakhaven's store kept, with its works");
    // (The still's keeper the brewer, of its words: doc 72, after iron6.)
    expect(keeperOf["cn_larkfield_store"] == "shop_larkfield" && keeperOf["cn_coldwater_store"] == "shop_coldwater" &&
               keeperOf["cn_coldwater_still"] == "still_cw",
           "Larkfield Stores kept by Larkfield General's shopkeeper, Coldwater Stores by Coldwater Sundries', the still by the brewer: " + keeperOf["cn_larkfield_store"] + ", " + keeperOf["cn_coldwater_store"] + ", the still " + keeperOf["cn_coldwater_still"]);
    // (Frosthollow Stores by the old map's Frosthollow Stores' shopkeeper: Fenshore, staffed before it, leaves another
    // community's namesake to it.)
    expect(keeperOf["cn_frosthollow_store"] == "shop_frost" && keeperOf["cn_fenshore_store"] != "shop_frost",
           "Frosthollow Stores kept by its old shopkeeper, never Fenshore's: " + keeperOf["cn_frosthollow_store"]);
    for (const auto& [cell, keeper] : keeperOf)
    {
        expect(s.account("till:job:" + keeper), cell + "'s till opened");
        expect(s.resident(keeper)->homeCell.rfind("cn_", 0) == 0, cell + "'s keeper lives in its community");
    }
    // Money: the crews' founding purses went to those who took their posts (moved); only the market shops' floats were
    // made, counted with the farms' founding.
    std::int64_t made = 0, shared = 0;
    for (const auto& [cell, keeper] : keeperOf)
        made += s.spec(keeper)->purse;
    expect(s.state().minted == mintedBefore + made && s.conserved(), "only the market shops' floats made, counted: conserved");
    for (const auto& f : s.farms())
        shared += s.cash(f.crew);
    std::int64_t theirs = 0;
    for (const auto& r : requests)
        if (r.purseFrom.rfind("crew:", 0) == 0)
            theirs += s.cash(r.templateId);
    expect(theirs + shared == crewPurses, "the crews' purses are their people's now, no more and no less: " + std::to_string(theirs) +
                                              " + " + std::to_string(shared) + " = " + std::to_string(crewPurses));
    // A day's work: wages from the farms' tills to their holders at the fixed wages; the crews' last pennies and larders to
    // the keepers; the community stores selling; conserved.
    run(s, 2 + .5 / 24, 2 + .6 / 24);              // (The hours since the day's turn caught up first: day 1's wages.)
    s.takeJournal();
    run(s, 2 + .6 / 24, 3 + .5 / 24);
    const auto journal = s.takeJournal();       // (Every entry since: the saved ledger keeps only 128.)
    std::map<std::string, std::int64_t> wages;
    std::map<std::string, int> sold;                // By community store: its sales to the realm's households.
    std::map<std::string, std::string> tillAt;
    for (const auto& [cell, keeper] : keeperOf)
        tillAt["till:job:" + keeper] = cell;
    for (const auto& e : journal)
    {
        if (e.kind == "wages" && e.from.rfind("farm:", 0) == 0)
            wages[e.to] += e.coins;
        // (A sale: the buyer's coins to the till; a buyer of the community's own or its farms'.)
        if (const auto at = tillAt.find(e.to); e.kind == "market" && at != tillAt.end() && s.resident(e.from) &&
                                               communityOf(s.resident(e.from)->homeCell) == communityOf(at->second))
            ++sold[at->second];
    }
    for (const auto& f : s.farms())
        for (const auto& p : s.positions())
            if (s.farmOfPost(p) >= 0 && s.farms()[std::size_t(s.farmOfPost(p))].def == f.def)
            {
                bool isKeeper = false;
                s.farmOfPost(p, &isKeeper);
                const auto& holder = s.state().careers.positions.at(p.id).holder;
                expect(wages[holder] == (isKeeper ? f.keeper : f.hand), p.title + " paid " + std::to_string(wages[holder]));
            }
    for (const auto& f : s.farms())
        expect(s.cash(f.crew) == 0, f.def->id + "'s crew holds nothing now (its people's)");
    // (A community's market is staffed with its first farm or works: Oakhaven's with the Oakhaven cut, doc 72.)
    for (const auto& c : people.communities)
        if (std::any_of(people.farms.begin(), people.farms.end(), [&](const auto& f) { return f.community == c.id; }))
            expect(sold["cn_" + c.id + "_store"] > 0, c.name + "'s store sold to its own households: " + std::to_string(sold["cn_" + c.id + "_store"]));
    expect(s.conserved(), "a day on: conserved");
    // Placed once: the next day asks for nobody.
    s.tendCareers(3, x.world);
    const auto again = s.takeRequests();
    expect(std::none_of(again.begin(), again.end(), [](const ResidentRequest& r) { return r.kind == "farm"; }),
           "nobody more asked for once the posts are real");
    // A hand gone (died): its post stands empty, the farm a hand short, and it yields in proportion.
    const auto& pigs = farm(s, "pig_farm_1");
    std::string gone;
    for (const auto& p : s.positions())
        if (bool k = false; s.farmOfPost(p, &k) >= 0 && s.farms()[std::size_t(s.farmOfPost(p))].def == pigs.def && !k)
            gone = s.state().careers.positions.at(p.id).holder;
    s.died(gone, 3.2);
    run(s, 3.9, 4 + .5 / 24);
    const auto& after = farm(s, "pig_farm_1");
    expect(after.heldHands == after.hands - 1 && after.heads() == 0, "its post stands empty, and no virtual post comes back");
    expect(std::abs(after.working() - double(after.crewSize() - 1) / after.crewSize()) < 1e-9,
           "a hand short: it yields in proportion, " + std::to_string(after.working()));
}

void theWorksCrews()
{
    // Doc 72, Phase 2: the fuel area's works crewed by the farms' rule. The woods' four of the woods' old trade first (9 in
    // the pool, 13 posts: the rest of the fallback), the peat cutting of those who keep the house; each keeper of those
    // with no family among the unplaced first, placed alone (no farmhouse), living with the hands: in the works'
    // community, or for the Rainwood's in the city; every post at the works' cell out of doors, paid by its till. Phase 3:
    // the stone area's granite quarry (1 + 1 since country3, in the city) of the stone cutters, and the two clay pits (one hand each and
    // no keeper, in Fenshore and Southmeadow) of the clay diggers, of the mines, stone and clay.
    Fixture x;
    build(x);
    auto& s = x.s;
    run(s, 1 - .5 / 24, 1 + .5 / 24);
    s.tendCareers(2, x.world);
    const auto requests = s.takeRequests();
    std::map<std::string, std::string> oldLabel;
    for (const auto& r : requests)
        oldLabel[r.templateId] = s.unplaced(r.templateId)->workLabel;
    for (const auto& r : requests)
        if (s.arrive(r))
            s.welcome(r, r.templateId);
    const auto& people = items::farmPeople();
    int woodsPosts = 0, ofTheWoods = 0;
    for (const auto& site : people.farms)
    {
        if (!site.works)
            continue;
        const auto& f = farm(s, site.farm);
        expect(f.outdoors && f.heldKeepers == f.keepers && f.heldHands == f.hands,
               site.farm + "'s crew all placed: " + std::to_string(f.heldKeepers) + " + " + std::to_string(f.heldHands));
        int keepers = 0;
        for (const auto& p : s.positions())
        {
            bool isKeeper = false;
            if (s.farmOfPost(p, &isKeeper) < 0 || s.farms()[std::size_t(s.farmOfPost(p))].def != f.def)
                continue;
            keepers += isKeeper;
            expect(p.work.cell == site.workplaces[0].area, p.title + ": at its works' cell");
            const auto pay = s.payOf(p);
            expect(pay.kind == "farm" && pay.payer == f.till && pay.wage == (isKeeper ? 16 : 10), p.title + ": paid by the works' till");
            const auto& holder = s.state().careers.positions.at(p.id).holder;
            const auto& home = s.resident(holder)->homeCell;
            if (site.community == "ridgemere")
                expect(communityOf(home) == "ridgemere" && home.rfind("fs_", 0) != 0, site.farm + "'s crew lives in the city: " + home);
            else
                expect(home.rfind("cn_" + site.community + "_", 0) == 0, site.farm + "'s crew lives in " + site.community + ": " + home);
            const auto trade = people.tradeOf(oldLabel.at(holder));
            if (isKeeper && site.community == "frosthollow")
                expect(home == (site.farm == "mine_frost_iron" ? "cn_frosthollow_mine_house" : "cn_frosthollow_collier_house"),
                       site.farm + "'s keeper in the keeper's house the map built for it: " + home);
            if (site.farm == "peat_fens")
                expect(trade == "keeps the house" || trade == "service and labour", "the peat's crew of those who keep the house: " + oldLabel.at(holder));
            else if (f.house == "Greatly" || site.farm == "mine_frost_iron")
            {
                const auto& old = oldLabel.at(holder);
                const char* word = site.farm == "quarry_granite" ? "cutting stone" : site.farm == "mine_frost_iron" ? "digs ore"
                                 : site.farm == "quarry_lime" ? "lime kilns" : "digs clay";
                expect(trade == "mines, stone and clay" && old.find(word) != std::string::npos,
                       site.farm + "'s crew of its own work, of the mines, stone and clay: " + old);
            }
            else
            {
                ++woodsPosts;
                ofTheWoods += trade == "woods";
                if (isKeeper)
                    expect(trade == "woods", site.farm + "'s keeper of the woods: " + oldLabel.at(holder));
            }
        }
        expect(keepers == f.keepers, site.farm + ": its keeper's post, if it has one: " + std::to_string(keepers));
        // No family with a works' keeper.
        for (const auto& r : requests)
            expect(!(r.farm == site.farm && r.workLabel == "-"), site.farm + ": no family placed with its keeper");
    }
    expect(woodsPosts == 16 && ofTheWoods == 12, "the woods' 13 posts and the Upper hearths' 3, all 12 of the woods' trade among them: " + std::to_string(ofTheWoods));
    // The oldest logger has a spouse among the unplaced: no works' keeper, but a hand; the spouse stays unplaced.
    const auto* ash = s.jobOf("ash");
    bool ashKeeps = true;
    expect(ash && s.farmOfPost(*ash, &ashKeeps) >= 0 && !ashKeeps, "the logger with family is a hand, not a keeper");
    // (Or, since doc 72's Phase 5, a country workshop's hand of those who keep the house: never as the logger's family.)
    const auto* wrenJob = s.jobOf("wren");
    expect(s.unplaced("wren") != nullptr || (wrenJob && people.workshop(wrenJob->work.cell)),
           "and the spouse stays in the pool, or took a workshop's post");
    // The stone's works (Phase 3): a clay pit with no keeper works with its one hand.
    for (const auto* id : {"clay_mirebridge", "clay_fens"})
    {
        const auto& pit = farm(s, id);
        expect(pit.keepers == 0 && pit.hands == 1 && pit.postKeepers == 0 && pit.heldHands == 1 && pit.heads() == 0 &&
                   std::abs(pit.working() - 1) < 1e-9 && pit.house == "Greatly",
               std::string(id) + ": no keeper, its one hand placed and working, House Greatly's");
    }
    expect(s.conserved(), "conserved");
}

void aWorksPostFilledAgain()
{
    // Doc 72, Phase 3 (country2: the Rainwood camp held 2 of its 3 posts for 655 days, a hand gone and never replaced): a
    // works' post out of doors is in the realm's own region, near no home, so every way an empty post is filled (a local
    // out of work, the city's wolves, an arrival from the Country) found nobody. Its holders are judged near from its
    // crew's home ground now (Society::reachOf), as a farm's are: the city's for the Rainwood's, the community's for a pit.
    Fixture x;
    build(x);
    auto& s = x.s;
    run(s, 1 - .5 / 24, 1 + .5 / 24);
    s.tendCareers(2, x.world);
    for (const auto& r : s.takeRequests())
        if (s.arrive(r))
            s.welcome(r, r.templateId);
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
        const auto* p = s.position(post[works]);
        expect(p && s.reachOf(*p) != p->work.cell, std::string(works) + ": its post judged from its crew's home ground: " + s.reachOf(*p));
        s.died(s.state().careers.positions.at(post[works]).holder, 2.2);
    }
    expect(communityOf(s.reachOf(*s.position(post["logging_rainwood"]))) == "ridgemere", "the Rainwood's, the city's");
    expect(communityOf(s.reachOf(*s.position(post["clay_fens"]))) == "southmeadow", "the Sallow pits', Southmeadow's");
    run(s, 2.9, 3 + .5 / 24);
    expect(farm(s, "logging_rainwood").heldHands == 1 && farm(s, "clay_fens").heldHands == 0, "two works' posts stand empty");
    // The days go by: the posts are filled again as a farm's are (a local, the city, or one come in from the Country).
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
           "both filled again within " + std::to_string(day - 2) + " days");
    for (const auto& [works, id] : post)
    {
        const auto& holder = s.state().careers.positions.at(id).holder;
        expect(!holder.empty() && s.resident(holder), works + "'s post held again: " + holder);
        const auto& home = s.resident(holder)->homeCell;
        expect(communityOf(home) == (works == "clay_fens" ? "southmeadow" : "ridgemere"), works + "'s new hand lives with its crew: " + home);
        expect(home.rfind("fs_", 0) != 0, works + "'s new hand not in a farmhouse (its keeper's family's alone): " + home);
    }
    expect(s.conserved(), "conserved");
}

void nothingWithoutThePlaces()
{
    // A world without the realm's places (the old map, a test town) places nobody: the farms keep their virtual crews.
    Fixture x;
    build(x);
    x.world.place = [](const std::string& cell) {
        return cell.rfind("fs_", 0) != 0 && cell.rfind("cn_", 0) != 0 && cell.rfind("w_ridgemere_", 0) != 0;
    };
    run(x.s, 1 - .5 / 24, 1 + .5 / 24);
    x.s.tendCareers(2, x.world);
    expect(x.s.takeRequests().empty(), "no places, nobody placed");
    for (const auto& f : x.s.farms())
        expect(f.heads() == f.crewSize(), "the crews stay virtual");
    // And without the world's answer at all (Society alone, as the farms' own tests run): nobody.
    Fixture y;
    build(y);
    y.world.place = nullptr;
    run(y.s, 1 - .5 / 24, 1 + .5 / 24);
    y.s.tendCareers(2, y.world);
    expect(y.s.takeRequests().empty(), "no world to ask, nobody placed");
}

void nothingUntilSwitched()
{
    // Before a farm's area is on (and its till founded) nobody is placed for it.
    Fixture x;
    build(x);
    auto e = x.s.authored().economy;
    for (auto& [id, a] : e.farms.areas)
        a.on = id == "grain" ? 1 : 0;
    x.s.adoptEconomy(e);
    run(x.s, 1 - .5 / 24, 1 + .5 / 24);
    x.s.tendCareers(2, x.world);
    const auto requests = x.s.takeRequests();
    std::set<std::string> farms;
    for (const auto& r : requests)
        farms.insert(r.farm);
    expect(!requests.empty() && farms.size() == 4 && farms.count("grain_farm_1") && farms.count("grain_farm_2") &&
               farms.count("market:larkfield") && farms.count("market:southmeadow"),
           "only the grain farms' people, and their two communities' markets: " + std::to_string(farms.size()));
}
} // namespace

int main()
{
    try
    {
        setenv("RATW_DATA_DIR", (fs::path(RATW_SOURCE_DIR) / "Data").string().c_str(), 1);
        std::string error;
        expect(items::load(&error), "the catalogue loads: " + error);
        theData();
        thePlacement();
        theWorksCrews();
        aWorksPostFilledAgain();
        nothingWithoutThePlaces();
        nothingUntilSwitched();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Farm people tests passed: " << checks << " checks.\n";
    return 0;
}
