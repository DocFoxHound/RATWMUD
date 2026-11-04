// The seasons in the Country's supply (Docs/Design/68-the-country-and-the-city-loop.md, Progress, "Seasons and the DM's
// rules"): a season group's factors by day of the year (each season's value at its middle day, a straight line between);
// store draws dearer and capped out of season, cheaper and plentiful in it, money conserved; residents paying the
// season's price and buying more of what is in season; a preserver stocking up in autumn for the winter; the world's
// own seasons and comforts (the "season" and "comforts" records) and the DM's economy.sync (Society::adoptEconomy).
//
// The catalogue is Data/Items with city_loop.json's seasons replaced by a small table of the test's own, copied to a
// scratch directory read through RATW_DATA_DIR.
#include "RatwCalendar.h"
#include "RatwGame.h"
#include "RatwItems.h"
#include "RatwSociety.h"
#include "RatwWorld.h"

#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
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
bool near(double a, double b, double within = 1e-6) { return std::abs(a - b) <= within; }

// The season's middle days (0 at Spring's first day): Spring 46, Summer 138, Autumn 229.5, Winter 320.
constexpr double Spring = 46, Summer = 138, Autumn = 229.5, Winter = 320;

// Grain (flour and bread) dear and short in winter, cheap and plentiful in autumn; the preserver's goods kept three
// times fuller in autumn; flour named again in a later group (the first group keeps it).
const char* Seasons = R"(,
  "seasons": {"groups": [
    {"id": "grain", "name": "grain", "goods": ["flour", "bread", "no_such_good"],
     "supply": [100, 100, 200, 50], "price": [100, 100, 50, 200], "shelf": [100, 100, 100, 100], "normal": 10},
    {"id": "preserved", "name": "preserves", "goods": ["dried_apples", "fruit_preserves"],
     "supply": [100, 100, 100, 100], "price": [100, 100, 100, 100], "shelf": [100, 100, 300, 100], "normal": 0},
    {"id": "fruit", "name": "fruit", "goods": ["apples", "flour"],
     "supply": [100, 100, 100, 100], "price": [100, 100, 100, 100], "shelf": [100, 100, 100, 100], "normal": 0}
  ]}
}
)";

void catalogue()
{
    const fs::path source = fs::path(RATW_SOURCE_DIR) / "Data";
    const fs::path dir = fs::temp_directory_path() / ("ratw_season_tests_" + std::to_string(::getpid()));
    fs::remove_all(dir);
    fs::create_directories(dir / "Items");
    fs::create_directories(dir / "Economy");
    for (const auto& e : fs::directory_iterator(source / "Items"))
        if (e.is_regular_file())
            fs::copy_file(e.path(), dir / "Items" / e.path().filename());
    fs::copy_file(source / "Economy" / "ladder.json", dir / "Economy" / "ladder.json");
    std::ifstream in(dir / "Items" / "city_loop.json");
    std::stringstream text;
    text << in.rdbuf();
    auto body = text.str();
    const auto at = body.find(",\n  \"seasons\"");
    expect(at != std::string::npos, "city_loop.json has its seasons last");
    body = body.substr(0, at) + Seasons;
    std::ofstream(dir / "Items" / "city_loop.json") << body;
    setenv("RATW_DATA_DIR", dir.string().c_str(), 1);
    std::string error;
    expect(items::load(&error), "the catalogue loads: " + error);
    const auto& groups = items::cityLoop().seasons;
    expect(groups.size() == 3 && groups[0].id == "grain" && groups[0].normal == 10, "the test's seasons are read");
    expect(groups[0].goods == std::vector<std::string>{"flour", "bread"}, "an unknown good is left out");
    expect(groups[2].goods == std::vector<std::string>{"apples"}, "a good is in the first group that names it");
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

// A baker, a preserver, a provisioner (fish), the watch, two clerks' households and a child.
Society city(const std::function<void(AuthoredRoster&)>& change = {})
{
    AuthoredRoster r;
    r.residents = {person("baker", "merchant", "baker at The Loaf", {"bakery", 2.5, 2.5}, "home_baker"),
                   person("preserver", "merchant", "keeping the preserving house", {"jars", 2.5, 2.5}, "home_jars"),
                   person("fish", "merchant", "fishmonger at the Fish Hall", {"fishhall", 2.5, 2.5}, "home_fish"),
                   person("guard", "guard", "keeping the watch", {"gate", 2.5, 2.5}, "watch_house_barracks"),
                   person("clerk", "civilian", "clerking at the City Office", {"office", 2.5, 2.5}, "home_clerk"),
                   person("child", "civilian", "at lessons", {"school", 2.5, 2.5}, "home_clerk", 8, 0),
                   person("copyist", "civilian", "copying at the Records Office", {"records", 2.5, 2.5}, "home_copyist"),
                   person("crier", "civilian", "crying wares in the square", {"square", 2.5, 2.5}, "home_crier")};
    r.kin = {{"clerk", "child", "parent"}};
    r.economy.treasury = 10000;
    r.economy.storeRule = true;
    r.economy.wharfReserve = 0;
    r.economy.rules["wharfValve"] = 0;              // (The Wharf's valve shut, doc 73: the Country's flows as these tests have them; wharf_tests and farms_tests open it.)
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
        s.tick(1, now, 0, bodies);
}

void theFactorsByDay()
{
    const std::array<int, 4> by{10, 20, 30, 40};
    expect(near(Society::seasonal(by, Spring), .1) && near(Society::seasonal(by, Summer), .2) &&
               near(Society::seasonal(by, Autumn), .3) && near(Society::seasonal(by, Winter), .4),
           "each season's value at its middle day");
    expect(near(Society::seasonal(by, (Spring + Summer) / 2), .15), "halfway between two middles, halfway between them");
    expect(near(Society::seasonal(by, 0), .4 + (.1 - .4) * (365 - Winter) / (Spring + 365 - Winter)) &&
               near(Society::seasonal(by, 364.9), .4 + (.1 - .4) * (364.9 - Winter) / (Spring + 365 - Winter)),
           "winter runs on to spring across the year's turn");
    expect(near(Society::seasonal(by, Winter + 365), .4), "a later year is the same");
    expect(Society::seasonal(by, -1) == 1, "no date: no season");
    // Flat groups are flat all year.
    for (double d = 0; d < 365; d += 7.3)
        expect(near(Society::seasonal({100, 100, 100, 100}, d), 1), "a flat group stays at 1");
}

void drawsFollowTheSeason()
{
    auto s = city();
    const auto till = s.tillOf("baker");
    s.shift("treasury", till, "", 0, 2000, "test: working money");
    expect(s.seasonGroup("flour") && s.seasonGroup("flour")->id == "grain" && s.seasonGroup("bread")->id == "grain" &&
               !s.seasonGroup("iron_bar"), "a good's group");
    // Before the society knows the date, no season applies.
    expect(s.yearDay() < 0 && s.priceFactor("flour") == 1 && s.supplyLeft("flour") == std::numeric_limits<int>::max(),
           "no date yet: catalogue prices and no cap");
    const auto plain = s.storeCost("baker", "flour", 10);
    // Mid-winter: twice the price, half the normal supply (5 of 10 a day).
    s.setYearDay(Winter);
    expect(near(s.priceFactor("flour"), 2) && near(s.priceFactor("bread"), 2) && s.priceFactor("firewood") == 1, "winter's prices");
    expect(s.storeCost("baker", "flour", 10) == 2 * plain, "a winter draw costs twice the catalogue's: " + std::to_string(s.storeCost("baker", "flour", 10)));
    expect(s.supplyLeft("flour") == 5 && s.supplyLeft("bread") == 5 && s.supplyLeft("firewood") == std::numeric_limits<int>::max(),
           "five a day of the grain group in winter, shared by its goods");
    const auto money = s.moneySupply();
    const auto refused = s.drawFromStore("baker", "flour", 6);
    expect(!refused.ok && refused.message.find("only 5 grain") != std::string::npos, "more than the season sends is refused: " + refused.message);
    const auto t0 = s.cash(till), c0 = s.cash(Society::WharfFund), r0 = s.cash("treasury");
    expect(s.drawFromStore("baker", "flour", 5).ok, "the season's five come");
    expect(t0 - s.cash(till) >= s.storeCost("baker", "flour", 5) && (s.cash(Society::WharfFund) - c0) + (s.cash("treasury") - r0) > 0,
           "the till paid the winter price, to the Country and the toll");
    expect(s.supplyLeft("flour") == 0 && !s.drawFromStore("baker", "flour", 1).ok, "and then no more today");
    expect(s.loopToday().groups.at("grain").drawn == 5, "the day's draws of the group are counted");
    expect(s.conserved() && s.moneySupply() == money, "money conserved");
    // The next day's supply starts again.
    s.setYearDay(Winter + 1);
    expect(s.supplyLeft("flour") >= 4, "a new day, a new supply: " + std::to_string(s.supplyLeft("flour")));
    // Mid-autumn: half the price, twice the supply.
    s.setYearDay(Autumn);
    expect(s.storeCost("baker", "flour", 10) == (plain + 1) / 2 && s.supplyLeft("flour") == 20, "autumn: cheap and plentiful");
    expect(s.drawFromStore("baker", "flour", 20).ok && !s.drawFromStore("baker", "flour", 1).ok, "twenty a day");
    expect(s.conserved(), "conserved");
}

// A day of the loop on day `day` of the year: what residents bought and paid of the grain group.
Society::LoopDay::Group aDayOn(double day, bool& conserved)
{
    auto s = city();
    const auto money = s.moneySupply();
    run(s, day + 7.9 / 24, day + 1 + 6. / 24);
    conserved = s.conserved() && s.moneySupply() == money;
    const auto& groups = s.loopYesterday().groups;
    const auto found = groups.find("grain");
    return found == groups.end() ? Society::LoopDay::Group{} : found->second;
}

void residentsBuyWhatIsInSeason()
{
    bool winterKept = false, autumnKept = false;
    const auto winter = aDayOn(std::floor(Winter), winterKept);
    const auto autumn = aDayOn(std::floor(Autumn), autumnKept);
    expect(winterKept && autumnKept, "a day of the loop in each season: money conserved");
    expect(winter.sold > 0 && autumn.sold > 0, "bread sold in both: " + std::to_string(winter.sold) + ", " + std::to_string(autumn.sold));
    const double winterPaid = double(winter.spent) / winter.sold, autumnPaid = double(autumn.spent) / autumn.sold;
    expect(winterPaid > 1.6 && autumnPaid < .8, "residents paid the season's price (1p bread): winter " + std::to_string(winterPaid) +
                                                     "p, autumn " + std::to_string(autumnPaid) + "p");
    expect(autumn.sold > winter.sold, "more bread is eaten when grain is in season: autumn " + std::to_string(autumn.sold) + ", winter " +
                                          std::to_string(winter.sold));
    expect(winter.drawn <= 10 && autumn.drawn <= 2 * 20, "draws within the season's supply: winter " + std::to_string(winter.drawn));
}

void thePreserverStocksUpForWinter()
{
    // Its shelves run three times fuller at autumn's middle and fall back by winter's: it makes more while fruit is in
    // season, and the stock carries into the winter.
    const auto stocked = [](double day) {
        auto s = city();
        const auto till = s.tillOf("preserver");
        s.shift("treasury", till, "", 0, 3000, "test: working money");
        s.setYearDay(day);
        const int shelf = s.shelfOf(till, "dried_apples");
        for (int h = 8; h < 20; ++h)
            s.restockFromStore("preserver", std::floor(day) + h / 24.);
        expect(s.conserved(), "conserved");
        return std::pair<int, int>{shelf, Society::stock(*s.account(till), "dried_apples")};
    };
    const auto [springShelf, spring] = stocked(Spring);
    const auto [autumnShelf, autumn] = stocked(Autumn);
    const auto [winterShelf, winter] = stocked(Winter);
    expect(springShelf == Society::GoodsKept && autumnShelf == 3 * Society::GoodsKept && winterShelf == Society::GoodsKept,
           "the shelf: " + std::to_string(springShelf) + ", " + std::to_string(autumnShelf) + ", " + std::to_string(winterShelf));
    expect(autumn > spring && autumn >= 2 * Society::GoodsKept, "it makes more in autumn: " + std::to_string(autumn) + " against " +
                                                                    std::to_string(spring) + " in spring");
    expect(winter <= Society::GoodsKept + 2, "and in winter only what its usual shelf wants: " + std::to_string(winter));
    // Between autumn's middle and winter's the shelf falls in a straight line: early winter keeps more than late.
    auto s = city();
    s.setYearDay(275);
    const double early = s.shelfFactor("dried_apples");
    s.setYearDay(Winter - 1);
    expect(early > s.shelfFactor("dried_apples") && s.shelfFactor("dried_apples") > 1, "the winter stock is sold down slowly");
}

std::map<std::string, std::string> files(const std::string& extra)
{
    std::ostringstream cell;
    cell << "id: town\nname: Town\ndescription: A town.\nworld: 0 0 0\nsize: 24 24\noutdoors: true\nweather: clear\ngrid:\n";
    for (int y = 0; y < 24; ++y)
        cell << std::string(24, '.') << '\n';
    std::string manifest = "RATW_WORLD 2\ncell \"town\" \"cells/town.cell\"\nspawn \"town\" 12.5 12.5\neconomy 1000 100 50 10 12\n" + extra +
                           "resident \"baker\" \"baker\" \"merchant\" \"baker at The Loaf\" \"Someone.\" \"Hello.\" 38 \"timber\" "
                           "\"female\" \"average\" \"piebald\" 4 3 0 8 1 8 17 \"-\" 30 0 1 \"town\" 3.5 5.5 \"town\" 3.5 8.5 \"town\" 3.5 11.5\n";
    return {{"world.ratw", manifest}, {"cells/town.cell", cell.str()}};
}

void theWorldsOwnSeasons()
{
    {
        World w;
        expect(w.loadWorldFiles(files(""), "test").ok, "a world without season records loads");
        const auto& e = w.society().authored().economy;
        expect(e.seasons.empty() && e.comfortsAbove == -1 && e.comfortsShare == -1, "city_loop.json's seasons and comforts");
        expect(w.society().comfortsAbove() == items::cityLoop().comfortsAbove && near(w.society().comfortsShare(), items::cityLoop().comfortsShare),
               "the comforts as city_loop.json has them");
        expect(w.society().seasonTable().size() == 3 && w.society().seasonTable()[0].normal == 10, "and its seasons");
    }
    {
        World w;
        const auto r = w.loadWorldFiles(files("comforts 40 -\nseason \"grain\" supply 50 100 150 40 price 120 100 80 130 shelf 100 100 100 100 normal 90\n"
                                              "season \"gone\" supply 1 1 1 1 price 100 100 100 100 shelf 100 100 100 100 normal 0\n"), "test");
        expect(r.ok, "the season records load: " + r.message);
        const auto& s = w.society();
        expect(s.comfortsAbove() == 40 && near(s.comfortsShare(), items::cityLoop().comfortsShare), "comforts 40, the share city_loop.json's");
        const auto& grain = s.seasonTable()[0];
        expect(grain.supply == std::array<int, 4>{50, 100, 150, 40} && grain.price == std::array<int, 4>{120, 100, 80, 130} && grain.normal == 90,
               "the world's grain");
        expect(s.seasonTable()[1].shelf == std::array<int, 4>{100, 100, 300, 100}, "the other groups as city_loop.json has them");
        expect(s.authored().economy.seasons.count("gone"), "a group city_loop.json no longer has is kept, and does nothing");
    }
    for (const auto* bad : {"comforts 40\n", "comforts x 10\n", "comforts 10 101\n", "comforts 1 1\ncomforts 2 2\n",
                            "season \"grain\" supply 50 100 150 price 120 100 80 130 shelf 100 100 100 100 normal 90\n",
                            "season \"grain\" supply 50 100 150 40 price 5 100 80 130 shelf 100 100 100 100 normal 90\n",
                            "season \"grain\" supply 50 100 150 40 price 120 100 80 130 shelf 100 100 100 100 normal -1\n",
                            "season \"grain\" supply 50 100 150 40 price 120 100 80 130 shelf 100 100 100 100\n",
                            "season \"grain\" supply 1 1 1 1 price 100 100 100 100 shelf 100 100 100 100 normal 0\n"
                            "season \"grain\" supply 1 1 1 1 price 100 100 100 100 shelf 100 100 100 100 normal 0\n"})
    {
        World w;
        expect(!w.loadWorldFiles(files(bad), "test").ok, std::string("refused: ") + bad);
    }
    const auto build = files("comforts 40 10\nseason \"grain\" supply 1 1 1 1 price 100 100 100 100 shelf 100 100 100 100 normal 0\n").at("world.ratw");
    const auto stripped = game::Game::withoutPeople(build);
    expect(stripped.find("comforts ") == std::string::npos && stripped.find("season ") == std::string::npos,
           "a streamed build's season records are stripped");
}

void theDmChangesTheRulesLive()
{
    // economy.sync (the DM's change, read again by the running server): the rules change at once; the treasury's
    // starting amount stays what the world began with.
    auto s = city();
    s.setYearDay(Winter);
    expect(s.supplyLeft("flour") == 5, "five a day before");
    auto next = s.authored().economy;
    next.treasury = 1;
    next.gateToll = 10;
    next.comfortsAbove = 25;
    next.seasons["grain"] = {{100, 100, 100, 100}, {100, 100, 100, 100}, {100, 100, 100, 100}, 40};
    s.adoptEconomy(next);
    expect(s.authored().economy.gateToll == 10 && s.comfortsAbove() == 25 && s.authored().economy.treasury == 10000,
           "the new rules, the starting treasury kept");
    expect(s.supplyLeft("flour") == 40 && s.priceFactor("flour") == 1, "the world's grain now: 40 a day, no season");
    const auto money = s.moneySupply();
    s.shift("treasury", s.tillOf("baker"), "", 0, 500, "test: working money");
    expect(s.drawFromStore("baker", "flour", 40).ok && s.conserved() && s.moneySupply() == money, "and drawn by them, conserved");
}
} // namespace

int main()
{
    try
    {
        catalogue();
        theFactorsByDay();
        drawsFollowTheSeason();
        residentsBuyWhatIsInSeason();
        thePreserverStocksUpForWinter();
        theWorldsOwnSeasons();
        theDmChangesTheRulesLive();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Season tests passed: " << checks << " checks.\n";
    return 0;
}
