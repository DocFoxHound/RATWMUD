// econ_watch's players who push (Tests/econ_players.h; Docs/Design/64-economy-migration.md, Phase 13), on a little
// world of two towns: Ridgemere, the town with the most shops (a shopkeeper, a smith, a stall, six hands paid from the
// shopkeeper's till), and Larkfield (a shopkeeper, three hands); flood's has a fishmonger and a fisher in Ridgemere
// besides. And econ_watch itself, run on the same world for two days and a bit: its stamp, its files' rows, and which
// day's record a midnight's doings fall in; and the stamp's script and check on a git tree of their own.
#include "RatwCalendar.h"
#include "RatwCamps.h"
#include "RatwItems.h"
#include "RatwWorld.h"
#include "econ_players.h"
#include "econ_watch_stamp.h"                       // (Made at build time, beside the build: what econ_watch says it was built from.)

#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
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

// A folder of the test's own under the temp folder, its name made this run's by the process id (several suites run
// at once here: another's must not clear this one's), and removed when every check has passed (main).
std::vector<fs::path> scratches;
fs::path scratch(const std::string& name)
{
    const auto root = fs::temp_directory_path() / (name + "-" + std::to_string(::getpid()));
    fs::remove_all(root);
    if (std::find(scratches.begin(), scratches.end(), root) == scratches.end())
        scratches.push_back(root);
    return root;
}

// Five cells in a row: Ridgemere's homes and its market, wild country, Larkfield's homes and its market.
constexpr int Side = 16;
std::string cellId(int i) { return "c_" + std::to_string(i) + "_0"; }
std::string resident(const std::string& who, const std::string& name, const std::string& role, const std::string& work, int home,
                     double hx, int at, double wx)
{
    std::ostringstream r;
    r << "resident \"" << who << "\" \"" << name << "\" \"" << role << "\" \"" << work << "\" \"Someone.\" \"Hello.\" 30 \"timber\" "
      << "\"female\" \"average\" \"saddle\" 3 1 5 1 1 8 17 \"-\" 40 0 1 \"" << cellId(home) << "\" " << hx << " 4.5 \"" << cellId(at)
      << "\" " << wx << " 8.5 \"" << cellId(home) << "\" " << hx << " 5.5\n";
    return r.str();
}
// `extra`: more residents, as resident() writes them. `at`: where in its scratch folder (the folder itself, unless asked).
fs::path writeWorld(const std::string& name, const std::string& extra = {}, const std::string& at = {})
{
    const auto root = at.empty() ? scratch(name) : scratch(name) / at;
    const std::string layout = "UU.SS";
    fs::create_directories(root / "cells");
    fs::create_directories(root / "seams");
    std::map<std::string, std::string> seams;
    std::ostringstream m;
    m << "RATW_WORLD 3\n";
    const int cells = int(layout.size());
    int seam = 0;
    for (int i = 0; i < cells; ++i)
    {
        std::ofstream cell(root / "cells" / (cellId(i) + ".cell"));
        cell << "id: " << cellId(i) << "\nname: Stretch " << i << "\ndescription: Open ground.\nworld: " << i * Side
             << " 0 0\noutdoors: true\nweather: clear\nsize: " << Side << ' ' << Side << "\ngrid:\n";
        for (int y = 0; y < Side; ++y)
            cell << std::string(Side, '.') << '\n';
        m << "area \"" << cellId(i) << "\"\n";
        if (i + 1 < cells)
            for (int k = 0; k < Side; ++k)
            {
                const std::string a = "seam_" + std::to_string(seam) + "_a", b = "seam_" + std::to_string(seam) + "_b";
                std::ostringstream one, two;
                one << "door \"" << a << "\" \"Open boundary\" \"" << cellId(i) << "\" " << Side - .5 << ' ' << k + .5 << " \""
                    << cellId(i + 1) << "\" .5 " << k + .5 << " \"" << b << "\" 1 0 1 1 \"E\"\n";
                two << "door \"" << b << "\" \"Open boundary\" \"" << cellId(i + 1) << "\" .5 " << k + .5 << " \"" << cellId(i)
                    << "\" " << Side - .5 << ' ' << k + .5 << " \"" << a << "\" 1 0 1 1 \"W\"\n";
                seams[cellId(i)] += one.str();
                seams[cellId(i + 1)] += two.str();
                ++seam;
            }
    }
    for (const auto& [id, text] : seams)
        std::ofstream(root / "seams" / id) << text;
    const auto region = [](char c) { return c == 'U' ? "ridgemere" : c == 'S' ? "larkfield" : "wilds"; };
    for (int i = 0; i < cells; ++i)
    {
        m << "exits \"" << cellId(i) << "\" " << ((i > 0) + (i + 1 < cells));
        if (i > 0)
            m << " \"" << cellId(i - 1) << '"';
        if (i + 1 < cells)
            m << " \"" << cellId(i + 1) << '"';
        m << '\n';
        m << "territory \"" << cellId(i) << "\" \"" << region(layout[std::size_t(i)]) << "\" \"-\" 0\n";
    }
    m << "spawn \"" << cellId(0) << "\" 8.5 8.5\n";
    m << "economy 20000 100 50 10 12\n";
    m << resident("ug", "Grocer U", "merchant", "the shopkeeper", 0, 2.5, 1, 3.5);
    m << resident("us", "Smith U", "merchant", "the smith at the forge", 0, 4.5, 1, 7.5);
    m << resident("uk", "Stall U", "merchant", "keeping the stall", 0, 6.5, 1, 11.5);
    for (int n = 1; n <= 6; ++n)
        m << resident("u" + std::to_string(n), "Hand U" + std::to_string(n), "civilian", "working", 0, 7.5 + n, 1, 2.5 + 2 * n);
    m << resident("lg", "Grocer L", "merchant", "the shopkeeper", 3, 2.5, 4, 3.5);
    for (int n = 1; n <= 3; ++n)
        m << resident("l" + std::to_string(n), "Hand L" + std::to_string(n), "civilian", "working", 3, 4.5 + n, 4, 5.5 + 2 * n);
    m << extra;
    std::ofstream(root / "world.ratw") << m.str();
    return root;
}

std::string slurp(const fs::path& path)
{
    std::ifstream file(path, std::ios::binary);
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
}
// As econ_watch loads an export (its cells streamed, the rest at once), at `day` (calendar days: 3.25, day 3 at six).
std::unique_ptr<World> worldAt(const fs::path& root, double day)
{
    auto w = std::make_unique<World>();
    auto files = std::make_shared<std::map<std::string, std::string>>();
    for (const auto& entry : fs::recursive_directory_iterator(root))
        if (entry.is_regular_file())
            (*files)[fs::relative(entry.path(), root).generic_string()] = slurp(entry.path());
    w->setCellSource({[files](const std::string& id, std::string& header) {
                          const auto found = files->find("cells/" + id + ".cell");
                          if (found == files->end())
                              return std::string("no cell file for ") + id;
                          header = found->second.substr(0, found->second.find("grid:") + 5);
                          return std::string();
                      },
                      [files](const std::string& id, std::string& cell, std::string& seams) {
                          const auto found = files->find("cells/" + id + ".cell");
                          if (found == files->end())
                              return std::string("no cell file for ") + id;
                          cell = found->second;
                          const auto sides = files->find("seams/" + id);
                          seams = sides == files->end() ? std::string() : sides->second;
                          return std::string();
                      }});
    std::map<std::string, std::string> manifest;
    for (const auto& [path, text] : *files)
        if (path.rfind("cells/", 0) != 0 && path.rfind("seams/", 0) != 0)
            manifest[path] = text;
    const auto loaded = w->loadWorldFiles(manifest, root.string());
    expect(loaded.ok, "the little world loads: " + loaded.message);
    w->setTimeOfDay(std::fmod(day, 1) * 24);
    auto later = w->save();
    later.calendarDays = day;
    expect(w->restore(later).ok, "the world starts at day " + std::to_string(day));
    w->setDeterministic(true);
    return w;
}

// The calendar on to `day` (a test's jump: World::advanceCalendar), a tick, and the players' look.
void jumpTo(World& w, econ::Players& p, double day)
{
    if (day > w.calendarDays())
        w.advanceCalendar(day - w.calendarDays());
    w.tick(.05);
    p.step();
}
// Ticks as econ_watch's: the world's, then the players'.
void run(World& w, econ::Players& p, double seconds, const std::function<void()>& each = {})
{
    for (double t = 0; t < seconds; t += .05)
    {
        w.tick(.05);
        p.step();
        if (each)
            each();
    }
}
int count(const std::vector<econ::PlayerMove>& moves, const std::string& action, const std::string& item = "*")
{
    int n = 0;
    for (const auto& m : moves)
        if (m.action == action && (item == "*" || m.item == item))
            n += std::max(1, m.quantity);
    return n;
}
std::vector<econ::PlayerMove>& keep(std::vector<econ::PlayerMove>& all, econ::Players& p)
{
    for (auto& m : p.takeMoves())
        all.push_back(m);
    return all;
}

void poachIsPaidFromTheStart()
{
    // Players who start mid-morning: the Hold's treasury has the day's pay from the first, and each dawn it is topped up
    // to that day's at that day's rate, a rise too.
    const auto root = writeWorld("ratw_econ_players_poach");
    auto w = worldAt(root, 3 + 10. / 24);
    econ::Players p(*w, {"poach"}, 3, 0);
    std::vector<econ::PlayerMove> moves;
    jumpTo(*w, p, 3 + 10. / 24);
    keep(moves, p);
    expect(count(moves, "hired") == 6, "the Hold takes Ridgemere's six hands: " + std::to_string(count(moves, "hired")));
    const auto wage = 2 * std::max<std::int64_t>(camp::HoldWage, std::int64_t(std::ceil(w->society().labourDay("") - 1e-9)));
    const auto* chapter = w->society().account(p.accountsOf("poach").front());
    expect(chapter && chapter->cash == 6 * wage, "its treasury holds the first day's pay: " + std::to_string(chapter ? chapter->cash : -1));
    jumpTo(*w, p, 4 + 1. / 960);
    keep(moves, p);
    expect(count(moves, "paid") == 6 && count(moves, "left unpaid") == 0, "all six are paid at the first dawn: " +
                                                                              std::to_string(count(moves, "paid")));
    // The going rate up by half: the next dawn's top-up covers it.
    auto state = w->society().state();
    state.memory.going[""] = w->society().labourDay("") * 1.5;
    expect(w->society().restore(state), "the rate rises");
    const auto risen = 2 * std::max<std::int64_t>(camp::HoldWage, std::int64_t(std::ceil(w->society().labourDay("") - 1e-9)));
    expect(risen > wage, "the Hold's wage rises with it");
    jumpTo(*w, p, 5 + 1. / 960);
    keep(moves, p);
    expect(count(moves, "paid") == 12 && count(moves, "left unpaid") == 0, "and all six at the next, at the new rate");
    std::int64_t paid = 0;
    for (const auto& m : moves)
        if (m.action == "paid" && m.day == 5)
            paid += m.coins;
    expect(paid == 6 * risen, "each paid the risen wage");
    expect(w->society().conserved(), "money stays conserved");
}

void poachShortOfMoney()
{
    // The capital's treasury nearly empty: the Hold's has the first day's pay, and the first dawn is paid; then the
    // treasury is emptied, and the Hold's own purse given three hands' day. At the second dawn the Hold notes what it
    // lacked ("short", the pennies as its quantity: none moved), pays the three it can, and the other three go home
    // unpaid.
    const auto root = writeWorld("ratw_econ_players_short");
    auto w = worldAt(root, 3 + 10. / 24);
    auto& s = w->society();
    const auto wage = 2 * std::max<std::int64_t>(camp::HoldWage, std::int64_t(std::ceil(s.labourDay("") - 1e-9)));
    expect(s.openAccount("chapter:econ-test-sink") && s.shift("treasury", "chapter:econ-test-sink", "", 0, s.account("treasury")->cash - 8 * wage,
                                                              "test: the treasury emptied"),
           "the treasury emptied but for eight hands' days");
    econ::Players p(*w, {"poach"}, 3, 0);
    std::vector<econ::PlayerMove> moves;
    jumpTo(*w, p, 3 + 10. / 24);
    jumpTo(*w, p, 4 + 1. / 960);
    keep(moves, p);
    expect(count(moves, "paid") == 6 && count(moves, "short") == 0, "all six paid at the first dawn, from the first day's pay");
    const auto chapter = p.accountsOf("poach").front();
    if (const auto left = s.account("treasury")->cash; left > 0)
        expect(s.shift("treasury", "chapter:econ-test-sink", "", 0, left, "test: the treasury emptied"), "the treasury emptied");
    if (const auto lacking = 3 * wage - s.account(chapter)->cash; lacking > 0)
        expect(s.shift("chapter:econ-test-sink", chapter, "", 0, lacking, "test: three hands' day"), "the Hold has three hands' day");
    const auto purse = s.account(chapter)->cash;
    jumpTo(*w, p, 5 + 1. / 960);
    keep(moves, p);
    // (The day's wage, as paid, and what the capital's treasury could still give: the rate may move from day to day.)
    std::int64_t wage5 = 0, funded = 0, moved = 0;
    int paid = 0, lacking = 0, unpaid = 0;
    for (const auto& m : moves)
        if (m.day == 5)
        {
            if (m.action == "paid")
                wage5 = m.coins, ++paid;
            if (m.action == "funded")
                funded += m.coins;
            if (m.action == "left unpaid")
                ++unpaid;
            if (m.action == "short")
            {
                moved += m.coins, lacking += m.quantity;
                expect(m.item == "pennies lacking", "the shortfall says what it is: " + m.item);
            }
        }
    expect(paid == 3 && unpaid == 3, "at the second dawn it pays the three it can, and three go home unpaid: " + std::to_string(paid) +
                                         " paid, " + std::to_string(unpaid) + " unpaid");
    expect(funded == 0, "the emptied treasury gives nothing: " + std::to_string(funded));
    if (wage5 == 0)
        wage5 = 2 * std::max<std::int64_t>(camp::HoldWage, std::int64_t(std::ceil(s.labourDay("") - 1e-9)));
    expect(lacking > 0 && moved == 0 && lacking == 6 * wage5 - purse - funded,
           "the shortfall noted as its quantity, no pennies moved: " + std::to_string(lacking) + " of " + std::to_string(6 * wage5));
    expect(int(p.staffOf("poach").size()) == 3, "the three paid stay");
    int home = 0;
    for (const auto& id : {"u1", "u2", "u3", "u4", "u5", "u6"})
        home += w->entity(id)->leaderId.empty();
    expect(home == unpaid, "the unpaid are their own again");
    expect(s.conserved(), "money stays conserved");
}

void bodiesLeaveAtTheEnd()
{
    // Their days over, the thieves' and visitors' bodies leave the world (World::removePlayer), and the Hold's hands go
    // back to their lives.
    const auto root = writeWorld("ratw_econ_players_end");
    auto w = worldAt(root, 2 + 8. / 24);
    econ::Players p(*w, {"theft", "visit", "poach"}, 2, 1);
    std::vector<econ::PlayerMove> moves;
    jumpTo(*w, p, 2 + 8. / 24);
    std::vector<std::string> bodies = p.playersOf("theft");
    for (const auto& id : p.playersOf("visit"))
        bodies.push_back(id);
    expect(bodies.size() == 10, "ten bodies");
    for (const auto& id : bodies)
        expect(w->entity(id) != nullptr, id + " is in the world");
    expect(w->entity("u1") && !w->entity("u1")->leaderId.empty(), "the Hold holds its hands");
    jumpTo(*w, p, 3 + 1. / 960);
    keep(moves, p);
    expect(p.ended(), "their day is over");
    for (const auto& id : bodies)
        expect(w->entity(id) == nullptr, id + " has left the world");
    expect(count(moves, "left") == 10 && count(moves, "released") == 6, "each noted: " + std::to_string(count(moves, "left")));
    for (const auto& id : {"u1", "u2", "u3", "u4", "u5", "u6"})
        expect(w->entity(id) && w->entity(id)->leaderId.empty(), std::string(id) + " is its own again");
}

void bodiesWalk()
{
    // theft's bodies walk up to their marks as a player walks (never a jump: a step at a time, into the next cell only
    // across its seam) and put their paw in a purse when there; standing still, they stay where they arrived. Jumping,
    // they don't (the first runs', kept to compare).
    for (const auto bodies : {econ::Players::Bodies::Walk, econ::Players::Bodies::Still, econ::Players::Bodies::Jump})
    {
        const auto root = writeWorld("ratw_econ_players_walk");
        auto w = worldAt(root, 2 + 8.9 / 24);
        econ::Players p(*w, {"theft"}, 2, 0, bodies);
        jumpTo(*w, p, 2 + 8.9 / 24);
        std::map<std::string, std::pair<std::string, Vec2>> was, arrived;
        for (const auto& id : p.playersOf("theft"))
            was[id] = arrived[id] = {w->entity(id)->cellId, w->entity(id)->position};
        double furthest = 0, step = 0;
        int crossed = 0;
        bool jumped = false;
        run(*w, p, 600 * 1.2, [&] {
            for (auto& [id, last] : was)
            {
                const auto* e = w->entity(id);
                if (e->cellId == last.first)
                    step = std::max(step, std::hypot(e->position.x - last.second.x, e->position.y - last.second.y));
                else
                {
                    // Across a seam: from one cell's edge to the next one's.
                    const bool edges = (last.second.x < 1.5 || last.second.x > Side - 1.5) && (e->position.x < 1.5 || e->position.x > Side - 1.5);
                    jumped = jumped || !w->neighbors(last.first).count(e->cellId) || !edges;
                    ++crossed;
                }
                const auto& first = arrived[id];
                if (e->cellId == first.first)
                    furthest = std::max(furthest, std::hypot(e->position.x - first.second.x, e->position.y - first.second.y));
                last = {e->cellId, e->position};
            }
        });
        int tries = 0;
        for (const auto& inc : w->crime().incidents)
            tries += inc.offender.rfind("player-theft-", 0) == 0;
        const std::string how = bodies == econ::Players::Bodies::Walk ? "walking" : bodies == econ::Players::Bodies::Still ? "standing" : "jumping";
        if (bodies == econ::Players::Bodies::Jump)
        {
            expect(step > 1 || jumped, "jumping bodies jump");
            continue;
        }
        expect(!jumped && step < .5, how + " bodies never jump: " + std::to_string(step) + " a tick at most");
        if (bodies == econ::Players::Bodies::Walk)
            expect(tries >= 3 && furthest > 2 && crossed > 0, "walking thieves go up to their marks and try, and on about the town: " +
                                                                  std::to_string(tries) + " tries, " + std::to_string(furthest) +
                                                                  " tiles, " + std::to_string(crossed) + " crossings");
        else
            expect(crossed == 0 && furthest < 2, "standing ones stay where they arrived: " + std::to_string(furthest));
        expect(w->society().conserved(), "money stays conserved");
    }
}

void bodiesAimEveryTwoSeconds()
{
    // A walking body after a mark that won't keep still (put across the place from it, on the far side of its middle,
    // each tick: the body turns, crosses the middle, and the mark is across again) turns after it every two seconds at
    // most (World::moveTo, a path found each time), not at every look.
    const auto root = writeWorld("ratw_econ_players_aim");
    auto w = worldAt(root, 2 + 9.3 / 24);
    econ::Players p(*w, {"visit"}, 2, 0);
    jumpTo(*w, p, 2 + 9.3 / 24);                    // (9:18: a mark in the place each is in, not on into the next.)
    std::string body, mark;
    for (const auto& id : p.playersOf("visit"))
        if (const auto m = p.markOf(id); body.empty() && !m.empty() && m.rfind("door:", 0) != 0)
            body = id, mark = m;
    expect(!body.empty(), "a visitor sets off after someone");
    const int before = p.aims();
    int ticks = 0, flips = 0;
    double side = 0;
    run(*w, p, 10, [&] {
        const auto* me = w->entity(body);
        const auto* m = w->entity(mark);
        if (!me || !m || m->cellId != me->cellId)
            return;
        ++ticks;
        const double x = me->position.x > Side / 2. ? 2.5 : Side - 2.5;
        flips += x != side;
        side = x;
        w->teleport(mark, me->cellId, x, Side - 3.5);
    });
    expect(p.markOf(body) == mark, "still after its mark: " + p.markOf(body));
    const int aims = p.aims() - before;
    expect(ticks > 150 && flips >= 3 && aims >= 3 && aims <= 6,
           "it turned after its mark " + std::to_string(aims) + " times in ten seconds (every two at most; the mark crossed " +
               std::to_string(flips) + " times)");
}

void stillThievesPassOver()
{
    // A thief standing where it arrived (at a shop, beside its keeper) tries a purse within reach once a quarter, each
    // purse once a day: it passes over the keeper it has tried (it had tried that one keeper every quarter).
    const auto root = writeWorld("ratw_econ_players_still");
    auto w = worldAt(root, 2 + 8.9 / 24);
    econ::Players p(*w, {"theft"}, 2, 0, econ::Players::Bodies::Still);
    std::vector<econ::PlayerMove> moves;
    jumpTo(*w, p, 2 + 8.9 / 24);
    run(*w, p, 600 * 1.2);                          // (9:00 to past 10:00: five quarters.)
    keep(moves, p);
    std::map<std::pair<std::string, std::string>, int> tries;
    int all = 0;
    for (const auto& m : moves)
        if (m.action == "tried")
            ++tries[{m.player, m.item}], ++all;
    expect(all >= 3, "standing thieves try the purses beside them: " + std::to_string(all));
    for (const auto& [who, n] : tries)
        expect(!who.second.empty() && n == 1, who.first + " tried " + who.second + "'s purse " + std::to_string(n) + " times in a day");
    expect(w->society().conserved(), "money stays conserved");
}

void floodSellsThenStops()
{
    // flood's ten players (asked for fish: --players-staple) land a pack each morning and sell it on the hour to the
    // capital's shops that buy it (here a fishmonger), as many as they take (its shelf to its supplies' 40); the next
    // morning each pack is topped up by what it sold; after two weeks, they stop. The capital's fisher is its maker,
    // watched in the day's block.
    expect(std::string(econ::Players::DefaultStaple) == "raw_meat" && econ::Players::stapleKnown("raw_meat") &&
               econ::Players::stapleKnown("fish") && !econ::Players::stapleKnown("meal") && !econ::Players::stapleKnown("no_such_good"),
           "raw meat unless asked: a good its sellers price (not a meal, priced by the stores)");
    const auto root = writeWorld("ratw_econ_players_flood",
                                 resident("uf", "Fishmonger U", "merchant", "the fishmonger at the quay", 0, 14.5, 1, 13.5) +
                                     resident("ux", "Fisher U", "civilian", "fishes the lake", 0, 15.5, 1, 15.5));
    auto w = worldAt(root, 2 + 7.9 / 24);
    auto& s = w->society();
    econ::Players p(*w, {"flood"}, 2, 0);
    expect(p.staple() == "raw_meat", "flood sells raw meat unless asked");
    p.setStaple("fish");
    std::vector<econ::PlayerMove> moves;
    jumpTo(*w, p, 2 + 7.9 / 24);
    // (Its shelf as a day's buyers leave it: sold out.)
    const auto shelf = s.tillOf("uf");
    if (const int n = s.account(shelf) ? Society::stock(*s.account(shelf), "fish") : 0; n > 0)
        expect(s.openAccount("chapter:econ-test-buyers") && s.shift(shelf, "chapter:econ-test-buyers", "fish", n, 0, "test: sold out"),
               "the fishmonger sells out");
    jumpTo(*w, p, 2 + 8.01 / 24);
    keep(moves, p);
    expect(p.playersOf("flood").size() == 10 && count(moves, "landed", "fish") == 10 * 99, "ten players land a pack of fish each");
    const int sold = count(moves, "sold", "fish");
    const auto* till = s.account(s.tillOf("uf"));
    expect(sold > 0 && till && Society::stockAll(*till, "fish") == Society::SuppliesKept,
           "they fill the fishmonger's shelf: " + std::to_string(sold) + " sold, " + std::to_string(till ? Society::stockAll(*till, "fish") : -1) + " held");
    const auto day = p.takeDay();
    expect(day.find("\"staple\":\"fish\"") != std::string::npos && day.find("\"selling\":true") != std::string::npos &&
               day.find("\"makers\":1,") != std::string::npos && day.find("\"makers_held\":1,") != std::string::npos &&
               day.find("\"asked\":") != std::string::npos,
           "the day's block has the staple, its price and its maker: " + day);
    // The next morning at eight, each pack topped up to 99: by what it sold (a player's fish doesn't spoil).
    jumpTo(*w, p, 3 + 7.9 / 24);
    jumpTo(*w, p, 3 + 8.01 / 24);
    keep(moves, p);
    int landed3 = 0, sold3 = 0, held3 = 0;
    for (const auto& m : moves)
        if (m.day == 3)
            landed3 += m.action == "landed" ? m.quantity : 0, sold3 += m.action == "sold" ? m.quantity : 0;
    for (const auto& id : p.playersOf("flood"))
        held3 += s.account(id) ? Society::stock(*s.account(id), "fish") : 0;
    expect(landed3 == sold && held3 == 10 * 99 - sold3, "the second morning they land what they sold, each a full pack again: " +
                                                          std::to_string(landed3) + " of " + std::to_string(sold) + "; " +
                                                          std::to_string(held3) + " held after selling " + std::to_string(sold3));
    // Two weeks on, at eight, they stop; and sell no more.
    jumpTo(*w, p, 16 + 7.9 / 24);
    jumpTo(*w, p, 16 + 8.01 / 24);
    keep(moves, p);
    expect(count(moves, "stopped") == 10, "after two weeks they stop");
    const int then = count(moves, "sold", "fish");
    jumpTo(*w, p, 16 + 9.01 / 24);
    jumpTo(*w, p, 17 + 8.01 / 24);
    keep(moves, p);
    expect(count(moves, "sold", "fish") == then && count(moves, "landed") == 10 * 99 + sold, "and land and sell no more");
    expect(p.takeDay().find("\"selling\":false") != std::string::npos, "the block says so");
    expect(s.conserved(), "money stays conserved");
}

void floodsMeat()
{
    // Raw meat (flood's unless asked) is a butcher's make, not the land's: the capital's butcher is its maker, watched;
    // as a shop that makes it (priced at its own cost of making it, worked out once a day), it isn't asked its price at
    // midnight, and the provisioner, who buys it in, is.
    const auto root = writeWorld("ratw_econ_players_meat",
                                 resident("ub", "Butcher U", "merchant", "the butcher at the shambles", 0, 14.5, 1, 13.5) +
                                     resident("up", "Provisioner U", "merchant", "the provisioner", 0, 15.5, 1, 15.5));
    auto w = worldAt(root, 2 + 7.9 / 24);
    econ::Players p(*w, {"flood"}, 2, 0);
    std::vector<econ::PlayerMove> moves;
    jumpTo(*w, p, 2 + 7.9 / 24);
    jumpTo(*w, p, 2 + 8.01 / 24);
    keep(moves, p);
    const auto day = p.takeDay();
    expect(day.find("\"staple\":\"raw_meat\"") != std::string::npos && day.find("\"shops_making\":1,") != std::string::npos &&
               day.find("\"makers\":1,") != std::string::npos && day.find("\"makers_held\":1,") != std::string::npos,
           "the butcher is raw meat's maker, and isn't asked: " + day);
    expect(count(moves, "landed", "raw_meat") == 990 && count(moves, "sold", "raw_meat") > 0, "they sell raw meat: " + day);
    expect(w->society().conserved(), "money stays conserved");
}

void strikeTakesATenthForAMonth()
{
    // strike's Hold takes a tenth of the capital's hands (here one of its six), stationed as a hired companion is,
    // off its schedule, on a companion's 6p a day from the Hold's treasury; after StrikeDays, at that dawn, lets it go.
    const auto root = writeWorld("ratw_econ_players_strike");
    auto w = worldAt(root, 2 + 10. / 24);
    auto& s = w->society();
    econ::Players p(*w, {"strike"}, 2, 0);
    std::vector<econ::PlayerMove> moves;
    jumpTo(*w, p, 2 + 10. / 24);
    keep(moves, p);
    expect(count(moves, "hired") == 1 && p.staffOf("strike").size() == 1, "a tenth of six hands, rounded up: one");
    const auto hand = p.staffOf("strike").front();
    expect(hand.size() == 2 && hand[0] == 'u' && w->entity(hand)->leaderId == "camp:econ-strike", "one of Ridgemere's hands, held: " + hand);
    const auto* chapter = s.account(p.accountsOf("strike").front());
    expect(chapter && chapter->cash == 6, "the Hold's treasury has its first day's 6p: " + std::to_string(chapter ? chapter->cash : -1));
    jumpTo(*w, p, 3 + 1. / 960);
    keep(moves, p);
    std::int64_t paid = 0;
    for (const auto& m : moves)
        paid += m.action == "paid" ? m.coins : 0;
    expect(count(moves, "paid") == 1 && paid == 6, "paid a companion's wage at dawn: " + std::to_string(paid));
    jumpTo(*w, p, 2 + econ::Players::StrikeDays - .5);
    keep(moves, p);
    expect(p.staffOf("strike").size() == 1 && count(moves, "released") == 0, "held for the month");
    jumpTo(*w, p, 2 + econ::Players::StrikeDays + 1. / 960);
    keep(moves, p);
    expect(count(moves, "released") == 1 && p.staffOf("strike").empty() && w->entity(hand)->leaderId.empty(),
           "the month over, let go at its dawn");
    const auto day = p.takeDay();
    expect(day.find("\"let_go\":true") != std::string::npos && day.find("\"taken\":1,") != std::string::npos, "the block says so: " + day);
    expect(s.conserved(), "money stays conserved");
}

// A resident's hunger set (the society's state restored with it), and what it carries that feeds eaten up.
void hungry(World& w, const std::string& id, double hunger)
{
    auto& s = w.society();
    std::vector<std::pair<std::string, int>> food;
    for (const auto& [item, n] : s.account(id)->stock)
        if (n > 0 && Society::edible(item))
            food.push_back({item, n});
    for (const auto& [item, n] : food)
        s.consume(id, item, n, "test: eaten");
    auto state = s.state();
    state.residents[id].hunger = hunger;
    expect(s.restore(state), id + " is hungry");
}
int foodIn(const Society& s, const std::string& account)
{
    int n = 0;
    if (const auto* a = s.account(account))
        for (const auto& [item, k] : a->stock)
            n += Society::edible(item) ? k : 0;
    return n;
}

void keptHandsEat()
{
    // Kept hands' meals (doc 64, Phase 13; the user, 2026-10-26): off their schedules (World: a leader), a Hold's hand eats
    // from the Hold's stores and a companion from its hirer's pack, at the eat mark (60; round 6: taken then, not at 55),
    // the food moved by the ledger and eaten; one with no mess eats what it carries, and nothing else.
    const auto root = writeWorld("ratw_econ_players_kept");
    auto w = worldAt(root, 2 + 10. / 24);
    auto& s = w->society();
    const std::string hold = "chapter:test-hold", player = "player-test-1";
    expect(s.openAccount(hold) && s.create(hold, "jerky", 3, "test: laid in"), "the Hold's stores hold three jerky");
    s.addPlayer(player);
    const int carried = foodIn(s, player);
    expect(carried > 0, "the player carries its first meals");
    for (const auto& [id, leader] : {std::pair{"u1", "camp:test"}, std::pair{"u2", "party:test"}, std::pair{"u3", "party:test"},
                                     std::pair{"u4", "party:test"}})
    {
        w->entity(id)->leaderId = leader;
        w->stop(id);
        hungry(*w, id, 61);
    }
    expect(s.create("u3", "meal", 1, "test: its own"), "u3 carries a meal of its own");
    s.setMesses({{"u1", hold}, {"u2", player}});
    econ::Players idle(*w, {}, 99, 0);
    run(*w, idle, 30);
    expect(foodIn(s, hold) == 2 && s.resident("u1")->hunger < 61 - 30, "the Hold's hand ate a jerky from its stores: " +
                                                                       std::to_string(foodIn(s, hold)) + " left, hunger " +
                                                                       std::to_string(s.resident("u1")->hunger));
    expect(foodIn(s, "u1") == 0 && s.resident("u1")->task == "companion", "eaten, not kept; and still off its schedule");
    expect(foodIn(s, player) == carried - 1 && s.resident("u2")->hunger < 61 - 50, "the companion ate a meal from its hirer's pack: " +
                                                                                     std::to_string(s.resident("u2")->hunger));
    const auto meals = s.takeMessMeals();
    expect(meals.size() == 2 && meals[0].who == "u1" && meals[0].mess == hold && meals[0].item == "jerky" && meals[1].who == "u2" &&
               meals[1].mess == player,
           "the meals eaten are told: " + std::to_string(meals.size()));
    expect(foodIn(s, "u3") == 0 && s.resident("u3")->hunger < 61 - 50, "one without a mess eats what it carries");
    expect(s.resident("u4")->hunger > 61 && s.unfed("u4", 61) && !s.unfed("u1", 0) && !s.unfed("u2", 0),
           "and one with neither goes hungry: unfed");
    expect(s.conserved(), "money stays conserved");
}

// What a kept hand takes from its mess, and when (round 6, doc 64, Phase 13, the companions' review): only at the eat
// mark, and eaten then (taken at the rations' mark, a dismissed companion walked off with a player's meal); prepared
// food before a raw good a craft works with, however soon that spoils; never a player's goods kept back for a contract;
// a purse full to MaxGoodsKinds kinds takes nothing, and reads as unfed (it starved unsaid); and one let go with a piece
// taken and uneaten gives it back.
void keptHandsChooseTheirMeal()
{
    const auto root = writeWorld("ratw_econ_players_choose");
    auto w = worldAt(root, 2 + 10. / 24);
    auto& s = w->society();
    const std::string hold = "chapter:test-hold", player = "player-test-1";
    expect(s.openAccount(hold) && s.create(hold, "jerky", 2, "test: laid in") && s.create(hold, "raw_meat", 4, "test: the workshop's"),
           "the Hold's stores hold jerky and the keep workshop's raw meat");
    expect(items::ingredient("raw_meat") && !items::ingredient("jerky") && !items::ingredient("meal"),
           "raw meat is a craft's, jerky and a meal nobody's");
    s.addPlayer(player);
    expect(s.create(player, "rabbit", 1, "test: a catch"), "the player carries a rabbit too");
    const int meals = Society::stock(*s.account(player), "meal");
    expect(meals > 0, "and its first meals");
    for (const auto& [id, leader] : {std::pair{"u1", "camp:test"}, std::pair{"u2", "party:test"}, std::pair{"u3", "party:test"},
                                     std::pair{"u4", "camp:test"}})
    {
        w->entity(id)->leaderId = leader;
        w->stop(id);
        hungry(*w, id, 57);
    }
    // u4's purse holds MaxGoodsKinds kinds, none of them food.
    for (const auto& g : items::allGoods())
        if (s.account("u4")->stock.size() < MaxGoodsKinds && !Society::edible(g.id) && g.price > 0 && !s.account("u4")->stock.count(g.id))
            s.create("u4", g.id, 1, "test: a full purse");
    expect(s.account("u4")->stock.size() == MaxGoodsKinds, "u4's purse is full of kinds: " + std::to_string(s.account("u4")->stock.size()));
    s.setMesses({{"u1", hold}, {"u2", player}, {"u3", player}, {"u4", hold}}, {{player, {"meal"}}});
    econ::Players idle(*w, {}, 99, 0);
    run(*w, idle, 10);
    expect(foodIn(s, hold) == 6 && Society::stock(*s.account(player), "rabbit") == 1 && foodIn(s, "u1") == 0 && foodIn(s, "u2") == 0,
           "under the eat mark, nothing is taken");
    for (const auto* id : {"u1", "u2", "u3", "u4"})
        hungry(*w, id, 61);
    run(*w, idle, 30);
    expect(Society::stock(*s.account(hold), "jerky") == 1 && Society::stock(*s.account(hold), "raw_meat") == 4 &&
               s.resident("u1")->hunger < 61 - 30,
           "the Hold's hand eats the jerky, not the raw meat that spoils sooner: jerky " +
               std::to_string(Society::stock(*s.account(hold), "jerky")) + ", raw meat " + std::to_string(Society::stock(*s.account(hold), "raw_meat")));
    expect(Society::stock(*s.account(player), "meal") == meals && Society::stock(*s.account(player), "rabbit") == 0 &&
               s.resident("u2")->hunger < 61 - 15,
           "the companion leaves the meals kept back for a contract, and eats the rabbit");
    expect(s.unfed("u3", 61), "a second, with nothing left it may eat, is unfed");
    expect(Society::stock(*s.account(hold), "jerky") == 1 && s.unfed("u4", 61) && s.resident("u4")->hunger > 61,
           "a full purse takes nothing, and reads as unfed (told of, and let go), not starving unsaid");
    const auto told = s.takeMessMeals();
    expect(told.size() == 2 && told[0].who == "u1" && told[0].item == "jerky" && told[1].who == "u2" && told[1].mess == player &&
               told[1].item == "rabbit",
           "the meals eaten are told: " + std::to_string(told.size()));
    // Let go with a piece taken, before eating it: given back.
    s.setMesses({{"u3", hold}});
    hungry(*w, "u3", 61);
    w->tick(.05);
    for (int i = 0; i < 20 && foodIn(s, "u3") == 0; ++i)
        w->tick(.05);
    expect(foodIn(s, "u3") == 1 && Society::stock(*s.account(hold), "jerky") == 0, "u3 takes the last jerky to eat");
    w->entity("u3")->leaderId.clear();
    s.setMesses({});
    run(*w, idle, 30);
    expect(Society::stock(*s.account("u3"), "jerky") == 0 && Society::stock(*s.account(hold), "jerky") == 1 && s.takeMessMeals().empty(),
           "let go before eating it, it gives it back and no meal from the stores is told");
    expect(s.conserved(), "money stays conserved");
}

// A Hold's hand that came to live there (Game::moveToHold, the user's Holds: its home the Hold's own cell, here in the
// wilds between the towns, no shop in it nor a town of its own) and is let go hungry at KeptLeaveHunger, its stores bare
// (round 6, the companions' review: every test's hand was town-homed): back in its own life it walks to a shop and buys
// a meal with its wage before it starves, and is fed within the quarter day.
void aMigrantHandLetGoHungryEats()
{
    const auto root = writeWorld("ratw_econ_players_migrant");
    auto w = worldAt(root, 2 + 10. / 24);
    auto& s = w->society();
    const std::string hand = "u6", hold = "chapter:test-migrant", wilds = cellId(2);
    // Moved to the Hold, as moveToHold does it (relocateResident), and arrived.
    expect(w->relocateResident(hand, wilds, 8.5, 8.5).ok, "u6 moves to the Hold in the wilds");
    econ::Players idle(*w, {}, 99, 0);
    for (int i = 0; i < 24000 && s.resident(hand)->homeCell != wilds; ++i)
    {
        w->tick(.05);
        idle.step();
    }
    expect(s.resident(hand)->homeCell == wilds && w->entity(hand)->cellId == wilds, "and makes its home there: " + s.resident(hand)->homeCell);
    expect(s.openAccount(hold), "the Hold's stores, bare");
    w->entity(hand)->leaderId = "camp:test";
    w->stop(hand);
    s.setMesses({{hand, hold}});
    if (const auto cash = s.account(hand)->cash; cash < 12)
        s.shift("treasury", hand, "", 0, 12 - cash, "test: its wages");
    hungry(*w, hand, 88);
    run(*w, idle, 600);
    expect(s.unfed(hand, Society::KeptLeaveHunger), "unfed at the Hold, at " + std::to_string(s.resident(hand)->hunger));
    // Let go, as Game::feedKeptHands does it.
    w->entity(hand)->leaderId.clear();
    s.setMesses({});
    double most = 0, after = 100;
    std::string where;
    run(*w, idle, calendar::SecondsPerDay / 4, [&] {
        most = std::max(most, s.resident(hand)->hunger);
        after = std::min(after, s.resident(hand)->hunger);
        if (where.empty() && after < Society::KeptLeaveHunger)
            where = w->entity(hand)->cellId;
    });
    expect(most < 100 && after < Society::KeptToldHunger, "let go hungry from its home at the Hold, it eats, never starving: hunger at most " +
                                                              std::to_string(most) + ", then as low as " + std::to_string(after) + " (fed in " + where + ")");
    expect(s.conserved(), "money stays conserved");
}

void strikeKeepsItsHandsFed()
{
    // strike's Hold keeps food in its stores (its quartermaster buys it at Ridgemere's shops each dawn: Players::provision),
    // and its hand eats from them: over two days it never goes hungry enough to be told of, nor leaves.
    const auto root = writeWorld("ratw_econ_players_fed");
    auto w = worldAt(root, 2 + 10. / 24);
    auto& s = w->society();
    econ::Players p(*w, {"strike"}, 2, 0);
    std::vector<econ::PlayerMove> moves;
    jumpTo(*w, p, 2 + 10. / 24);
    keep(moves, p);
    const auto hand = p.staffOf("strike").front();
    const auto hold = p.accountsOf("strike").front();
    const int laidIn = foodIn(s, hold);
    expect(laidIn > 0 && count(moves, "stored") == laidIn, "the stores laid in at the start: " + std::to_string(laidIn));
    hungry(*w, hand, 56);
    double most = 0;
    run(*w, p, 2 * calendar::SecondsPerDay, [&] { most = std::max(most, s.resident(hand)->hunger); });
    keep(moves, p);
    expect(p.staffOf("strike").size() == 1 && count(moves, "left hungry") == 0 && count(moves, "told hungry") == 0,
           "held, fed, never hungry enough to tell of");
    expect(most < Society::KeptToldHunger, "its hunger at most " + std::to_string(most));
    expect(count(moves, "stored") > laidIn, "the stores topped up at dawn");
    expect(s.conserved(), "money stays conserved");
}

// The quartermaster stocks food that keeps (round 6, doc 64, Phase 13, the companions' review: two days of porridge,
// which keeps a day, and 44% of what went in rotted there): toward the stores' ProvisionDays only food lasting till the
// next top-up counts, bought first, prepared before a craft's, the cheapest for what it feeds first; what spoils sooner,
// a day's food at most.
void strikeStocksFoodThatKeeps()
{
    // Ridgemere with an inn (porridge: keeps a day, the cheapest food there is), a butcher (salt pork: ninety days, dear)
    // and a bakery (bread: four days, cheap), the butcher first in the shops' order; and the stall's raw meat (three
    // days, a craft's).
    const auto root = writeWorld("ratw_econ_players_stocks", resident("ui", "Inn U", "merchant", "keeping the inn", 1, 3.5, 1, 4.5) +
                                                             resident("ua", "Butcher U", "merchant", "the butcher", 1, 7.5, 1, 9.5) +
                                                             resident("ub", "Baker U", "merchant", "the baker", 1, 5.5, 1, 6.5));
    auto w = worldAt(root, 2 + 10. / 24);
    auto& s = w->society();
    // Every shop's food in Ridgemere: only what this sets.
    const auto clear = [&](const std::string& keeper, std::initializer_list<const char*> keep) {
        const auto till = s.tillOf(keeper);
        std::vector<std::pair<std::string, int>> food;
        for (const auto& [item, n] : s.account(till)->stock)
            if (n > 0 && Society::edible(item) && std::none_of(keep.begin(), keep.end(), [&](const char* k) { return item == k; }))
                food.push_back({item, n});
        for (const auto& [item, n] : food)
            s.consume(till, item, n, "test: none of it");
    };
    for (const auto* keeper : {"ug", "uk", "ui", "ua", "ub"})
        clear(keeper, {});
    for (const auto& [keeper, item, n] : {std::tuple{"ui", "porridge", 60}, std::tuple{"ua", "salt_pork", 2}, std::tuple{"ub", "bread", 60},
                                          std::tuple{"uk", "raw_meat", 20}})
        expect(s.create(s.tillOf(keeper), item, n, "test: the shelves"), std::string(keeper) + " has " + item);
    econ::Players p(*w, {"strike"}, 2, 0);
    std::vector<econ::PlayerMove> moves;
    const int nourished = Society::DayNourishment;
    // What was stored at a dawn, by good and in order; and the stores emptied after, as if eaten (so the next dawn buys
    // its two days afresh).
    const auto dawn = [&](double day, std::vector<std::string>& order) {
        moves.clear();
        jumpTo(*w, p, day);
        keep(moves, p);
        std::map<std::string, int> stored;
        for (const auto& m : moves)
            if (m.action == "stored")
            {
                if (!stored.count(m.item))
                    order.push_back(m.item);
                stored[m.item] += m.quantity * Society::nourishment(m.item);
            }
        expect(!p.accountsOf("strike").empty(), "the Hold has its stores");
        const auto hold = p.accountsOf("strike").front();
        std::vector<std::pair<std::string, int>> food;
        for (const auto& [item, n] : s.account(hold)->stock)
            if (n > 0 && Society::edible(item))
                food.push_back({item, n});
        for (const auto& [item, n] : food)
            s.consume(hold, item, n, "test: eaten");
        return stored;
    };
    std::vector<std::string> order;
    auto stored = dawn(2 + 10. / 24, order);
    const int hands = int(p.staffOf("strike").size()), day = nourished * hands, want = econ::Players::ProvisionDays * day;
    expect(hands > 0 && order == std::vector<std::string>{"bread"} && stored["bread"] >= want,
           "bread, the cheapest food that lasts, and only it, till the days are held: " + std::to_string(stored["bread"]) + " of " +
               std::to_string(want) + " (first " + (order.empty() ? std::string("none") : order.front()) + ")");
    // No bread: the salt pork (prepared), then the stall's raw meat, cheaper but a craft's; no porridge.
    clear("ub", {});
    order.clear();
    stored = dawn(3 + 1. / 960, order);
    expect(order.size() == 2 && order[0] == "salt_pork" && order[1] == "raw_meat" && !stored.count("porridge") &&
               stored["salt_pork"] + stored["raw_meat"] >= want,
           "then the salt pork, the raw meat after it, and no porridge: " + std::to_string(order.size()) + " goods, first " +
               (order.empty() ? std::string("none") : order.front()));
    // Nothing that lasts on the shelves: porridge, but a day's at most.
    for (const auto* keeper : {"ug", "uk", "ui", "ua", "ub"})
        clear(keeper, {"porridge"});
    order.clear();
    stored = dawn(4 + 1. / 960, order);
    expect(stored["porridge"] >= day && stored["porridge"] < day + Society::nourishment("porridge"),
           "only porridge to be had: a day's of it, no more: " + std::to_string(stored["porridge"]) + " of " + std::to_string(day));
    expect(s.conserved(), "money stays conserved");
}

void strikeHandsGoHomeHungry()
{
    // The stores bare (gone bad mid-morning, say; the next dawn's top-up far off) and the hand hungry: it is told of at
    // KeptToldHunger, leaves at KeptLeaveHunger, and back in its own life eats as anyone does (buys a meal with its wage):
    // over the half day, it never starves.
    const auto root = writeWorld("ratw_econ_players_unfed");
    auto w = worldAt(root, 2 + 10. / 24);
    auto& s = w->society();
    econ::Players p(*w, {"strike"}, 2, 0);
    std::vector<econ::PlayerMove> moves;
    jumpTo(*w, p, 2 + 10. / 24);
    keep(moves, p);
    const auto hand = p.staffOf("strike").front();
    const auto hold = p.accountsOf("strike").front();
    std::vector<std::pair<std::string, int>> food;
    for (const auto& [item, n] : s.account(hold)->stock)
        if (n > 0)
            food.push_back({item, n});
    for (const auto& [item, n] : food)
        s.consume(hold, item, n, "test: spoiled");
    hungry(*w, hand, 88);
    double most = 0;
    std::string leftAt;
    run(*w, p, calendar::SecondsPerDay / 4, [&] {
        most = std::max(most, s.resident(hand)->hunger);
        if (leftAt.empty() && w->entity(hand)->leaderId.empty())
            leftAt = std::to_string(s.resident(hand)->hunger);
    });
    keep(moves, p);
    expect(count(moves, "told hungry") == 1 && count(moves, "left hungry") == 1 && p.staffOf("strike").empty(),
           "told of, then gone home hungry: " + std::to_string(count(moves, "told hungry")) + " told, " +
               std::to_string(count(moves, "left hungry")) + " left");
    expect(!leftAt.empty() && std::stod(leftAt) >= Society::KeptLeaveHunger && std::stod(leftAt) < Society::KeptLeaveHunger + 1,
           "it left at " + leftAt);
    double after = 100;
    run(*w, p, calendar::SecondsPerDay / 4, [&] {
        most = std::max(most, s.resident(hand)->hunger);
        after = std::min(after, s.resident(hand)->hunger);
    });
    expect(most < Society::KeptLeaveHunger + 1 && after < Society::KeptToldHunger,
           "its own again, it eats, never starving: hunger at most " + std::to_string(most) + ", then as low as " + std::to_string(after));
    expect(s.conserved(), "money stays conserved");
}

void strikeEndsWithThePlayers()
{
    // Players there five days, fewer than StrikeDays: at their end the Hold lets its hands go, and the block says so (it
    // had said "let_go":false with none held). And the Hold takes none Game::whyNotJoin refuses at any hour: the hand
    // it would take, Quickened, beaten down (or still down), wanted by the watch (a warrant) or in its hands (custody),
    // or passing through, is passed over for another (the review: only the Quickened had been tried).
    const auto root = writeWorld("ratw_econ_players_strike_end");
    std::string first;
    {
        auto w = worldAt(root, 2 + 10. / 24);
        econ::Players p(*w, {"strike"}, 2, 5);
        jumpTo(*w, p, 2 + 10. / 24);
        expect(p.staffOf("strike").size() == 1, "it takes one");
        first = p.staffOf("strike").front();
        jumpTo(*w, p, 7 + 1. / 960);
        expect(p.ended() && p.staffOf("strike").empty() && w->entity(first)->leaderId.empty(), "their five days over, the hand goes home");
        const auto day = p.takeDay();
        expect(day.find("\"let_go\":true") != std::string::npos && day.find("\"staff\":0,") != std::string::npos,
               "the block says it is let go: " + day);
        expect(w->society().conserved(), "money stays conserved");
    }
    const std::vector<std::pair<std::string, std::function<void(World&)>>> refused{
        {"the Quickened", [&](World& w) { w.entity(first)->quickened = true; }},
        {"one beaten down", [&](World& w) { w.entity(first)->state = "beaten down"; }},
        {"one still down", [&](World& w) { w.entity(first)->downedLeft = 30; }},
        {"one passing through", [&](World& w) { w.entity(first)->transient = true; }},
        {"one with a warrant", [&](World& w) { w.crime().warrants.push_back({first, "ridgemere", {}, {}, 5, w.calendarDays()}); }},
        {"one in custody", [&](World& w) {
             const auto* e = w.entity(first);
             w.crime().custody.push_back({first, "ridgemere", e->cellId, e->position.x, e->position.y, w.calendarDays() + 30});
         }},
    };
    for (const auto& [who, make] : refused)
    {
        auto w = worldAt(root, 2 + 10. / 24);
        make(*w);
        econ::Players p(*w, {"strike"}, 2, 0);
        jumpTo(*w, p, 2 + 10. / 24);
        expect(p.staffOf("strike").size() == 1 && p.staffOf("strike").front() != first,
               who + " " + first + " passed over: " + (p.staffOf("strike").empty() ? std::string("none") : p.staffOf("strike").front()));
    }
}

void buyoutHoldsTheFood()
{
    // buyout's five buy every piece of food on the capital's shelves at six (here Ridgemere's grocer's and stall's),
    // none of the other town's, each morning of their week; and hold it: after the week, nothing is sold back, and
    // what they hold (a player's food doesn't spoil) is all still there.
    const auto root = writeWorld("ratw_econ_players_buyout");
    auto w = worldAt(root, 2 + 5. / 24);
    auto& s = w->society();
    const auto food = [&](const std::string& account) {
        std::int64_t n = 0;
        if (const auto* a = s.account(account))
            for (const auto& [item, k] : a->stock)
                if (const auto* good = items::good(item); (good && good->nourish > 0) || items::baseOf(item) == "meal")
                    n += k;
        return n;
    };
    const auto larkfield = food(s.tillOf("lg"));
    // Every shop's till holds food it doesn't sell besides (a maker's materials): not to be had, and passed over (the
    // five had given up at the first five such, and bought almost nothing on DEV's map).
    std::map<std::string, int> unsold;
    for (const auto& keeper : {"ug", "us", "uk"})
    {
        const auto deals = s.wares(keeper);
        int kinds = 0;
        for (const auto& g : items::allGoods())
            if (kinds < 6 && g.nourish > 0 && g.id == items::baseOf(g.id) && std::find(deals.begin(), deals.end(), g.id) == deals.end() &&
                s.create(s.tillOf(keeper), g.id, 3, "test: materials"))
                ++kinds, unsold[keeper] += 3;
    }
    expect(unsold["us"] == 18, "the smith holds six kinds of food it doesn't sell: " + std::to_string(unsold["us"]));
    econ::Players p(*w, {"buyout"}, 2, 0);
    std::vector<econ::PlayerMove> moves;
    jumpTo(*w, p, 2 + 5. / 24);
    expect(food(s.tillOf("ug")) + food(s.tillOf("uk")) > unsold["ug"] + unsold["uk"] && larkfield > 0, "both towns' shops have food to sell");
    jumpTo(*w, p, 2 + 6.01 / 24);
    keep(moves, p);
    const int bought = count(moves, "bought");
    expect(bought > 0 && food(s.tillOf("ug")) == unsold["ug"] && food(s.tillOf("uk")) == unsold["uk"] && food(s.tillOf("us")) == unsold["us"],
           "every piece of Ridgemere's food for sale bought, and none of what isn't: " + std::to_string(bought) + "; left " +
               std::to_string(food(s.tillOf("ug"))) + ", " + std::to_string(food(s.tillOf("uk"))) + ", " + std::to_string(food(s.tillOf("us"))));
    expect(food(s.tillOf("lg")) >= larkfield, "none of Larkfield's");
    std::int64_t held = 0;
    for (const auto& id : p.playersOf("buyout"))
        held += food(id);
    expect(p.playersOf("buyout").size() == 5 && held >= bought, "the five hold it: " + std::to_string(held));
    jumpTo(*w, p, 9 + 6.01 / 24);
    jumpTo(*w, p, 10 + 6.01 / 24);
    keep(moves, p);
    std::int64_t after = 0;
    for (const auto& id : p.playersOf("buyout"))
        after += food(id);
    expect(count(moves, "bought") == bought && count(moves, "sold") == 0 && after == held,
           "after the week they buy none, sell none, and hold it all: " + std::to_string(after) + " of " + std::to_string(held));
    expect(p.takeDay().find("\"food_held\":" + std::to_string(after)) != std::string::npos, "the block has what they hold");
    expect(s.conserved(), "money stays conserved");
}

void buyoutLastsTheWeek()
{
    // buyout buys on the hour from six to eight in the evening, each player's purse over the week's hours still to
    // come (the review: at six only, all of it, they ran dry on the fifth morning): with a shelf fuller than the hour's
    // money, each spends at most its purse over the week's 105 hours at six, and buys again at seven; on the week's
    // last hour it may spend all it has.
    const auto root = writeWorld("ratw_econ_players_buyout_week");
    auto w = worldAt(root, 2 + 5. / 24);
    auto& s = w->society();
    std::string staple;
    for (const auto& item : s.wares("ug"))
        if (const auto* good = items::good(item); good && good->nourish > 0 && staple.empty())
            staple = item;
    expect(!staple.empty() && s.create(s.tillOf("ug"), staple, 3000, "test: a full shelf"), "Ridgemere's grocer with a full shelf of " + staple);
    econ::Players p(*w, {"buyout"}, 2, 0);
    std::vector<econ::PlayerMove> moves;
    jumpTo(*w, p, 2 + 5. / 24);
    std::map<std::string, std::int64_t> purse;
    for (const auto& id : p.playersOf("buyout"))
        purse[id] = s.account(id)->cash;
    const int hours = econ::Players::BuyoutDays * (econ::Players::BuyoutTo - econ::Players::BuyoutFrom + 1);
    jumpTo(*w, p, 2 + 6.01 / 24);
    keep(moves, p);
    std::map<std::string, std::int64_t> spent;
    for (const auto& m : moves)
        if (m.action == "bought")
            spent[m.player] += m.coins;
    bool paced = purse.size() == 5 && purse.begin()->second > hours;   // (The little world's treasury funds the first.)
    for (const auto& [id, coins] : purse)
        paced &= coins <= 0 ? !spent[id] : spent[id] > 0 && spent[id] <= coins / hours;
    expect(paced && Society::stockAll(*s.account(s.tillOf("ug")), staple) > 0,
           "at six each spends its purse over the week's hours at most, and food is left: " + [&] {
               std::string said;
               for (const auto& [id, coins] : purse)
                   said += id + " " + std::to_string(spent[id]) + "p of " + std::to_string(coins) + "p; ";
               return said + std::to_string(Society::stockAll(*s.account(s.tillOf("ug")), staple)) + " left";
           }());
    moves.clear();
    jumpTo(*w, p, 2 + 7.01 / 24);
    keep(moves, p);
    expect(count(moves, "bought") > 0, "and buys again at seven");
    // The week's last hour: all each has left, as far as the shelf goes (here 200p, the shelf full again).
    jumpTo(*w, p, 8 + 19.5 / 24);
    s.create(s.tillOf("ug"), staple, 3000, "test: a full shelf again");
    const auto& one = p.playersOf("buyout").front();
    expect(s.shift(one, "treasury", "", 0, s.account(one)->cash - 200, "test: 200p left"), "200p left");
    const auto piece = s.quote(one, "ug", staple, 1, true).total;
    jumpTo(*w, p, 8 + 20.01 / 24);
    expect(piece > 0 && s.account(one)->cash < piece && Society::stockAll(*s.account(s.tillOf("ug")), staple) > 0,
           "on the week's last hour it spends all it has: 200p to " + std::to_string(s.account(one)->cash) + "p, at " + std::to_string(piece) + "p a piece");
    keep(moves, p);
    moves.clear();
    jumpTo(*w, p, 9 + 8.01 / 24);
    keep(moves, p);
    expect(count(moves, "bought") == 0, "and none after the week");
    expect(s.conserved(), "money stays conserved");
}

void giftToThePoorest()
{
    // gift's player gives 100p at ten to each of the poorest households (here all ten: Ridgemere's nine grown under one
    // roof are each a household, as a barracks' are; Larkfield's four are one), to one grown member of each, by the
    // ledger; nothing is made.
    const auto root = writeWorld("ratw_econ_players_gift");
    auto w = worldAt(root, 2 + 9. / 24);
    auto& s = w->society();
    econ::Players p(*w, {"gift"}, 2, 0);
    std::vector<econ::PlayerMove> moves;
    jumpTo(*w, p, 2 + 9. / 24);
    const auto supply = s.moneySupply();
    const auto& giver = p.playersOf("gift").front();
    const auto purse = s.account(giver)->cash;
    expect(purse >= std::int64_t(econ::Players::GiftHouseholds) * econ::Players::GiftEach, "the giver has its month's gifts: " + std::to_string(purse));
    w->takeEvents();
    jumpTo(*w, p, 2 + 10.01 / 24);
    keep(moves, p);
    std::map<std::string, std::int64_t> ledger;      // (The journal's "a gift" entries: from the giver, to whom, how much.)
    for (const auto& e : w->takeEvents())
        if (e.kind == "economy" && e.detail == "a gift" && e.actor == giver)
            ledger[e.target] += e.coins;
    std::map<std::string, int> to;
    for (const auto& m : moves)
        if (m.action == "gave")
        {
            expect(m.coins == econ::Players::GiftEach, "each given 100p");
            ++to[m.item];
        }
    expect(to.size() == 10 && count(moves, "gave") == 10, "ten households given to: " + std::to_string(to.size()));
    expect(to.count("l1") && !to.count("l2") && !to.count("l3") && !to.count("lg"), "Larkfield's household once, to its first grown");
    for (const auto& [id, n] : to)
        expect(ledger[id] == econ::Players::GiftEach, id + "'s gift is in the journal: " + std::to_string(ledger[id]));
    expect(s.account(giver)->cash == purse - 10 * econ::Players::GiftEach, "the giver is 1,000p the poorer");
    expect(s.conserved() && s.moneySupply() == supply, "money stays conserved, none made");
    // Larkfield's first grown in the watch's hands: the household's gift goes to the next (the review: the skip was
    // never tried).
    auto again = worldAt(root, 2 + 9. / 24);
    const auto* l1 = again->entity("l1");
    again->crime().custody.push_back({"l1", "larkfield", l1->cellId, l1->position.x, l1->position.y, again->calendarDays() + 30});
    econ::Players q(*again, {"gift"}, 2, 0);
    jumpTo(*again, q, 2 + 10.01 / 24);
    std::set<std::string> given;
    for (const auto& m : q.takeMoves())
        if (m.action == "gave")
            given.insert(m.item);
    expect(again->custodyOf("l1") && given.size() == 10 && !given.count("l1") && given.count("l2"),
           "one in custody passed over for the household's next grown: " + std::to_string(given.size()) + " given");
}

void drainWorksEveryBench()
{
    // drain's players at every bench of the town (not the capital) with the most: here Larkfield's smith's (its keeper
    // trades, at no bench), two of them, arriving beside it; at work beside it while it is at its forge
    // (World::helpAtWork), paid by the beat from what Society::tillOf gives for it: for a hand, whose post has no till,
    // its own purse (so it is on DEV's map: doc 64, "players who push, the third review").
    const auto root = writeWorld("ratw_econ_players_drain", resident("ls", "Smith L", "civilian", "the smith at the forge", 3, 9.5, 4, 9.5));
    auto w = worldAt(root, 2 + 8.5 / 24);
    auto& s = w->society();
    econ::Players p(*w, {"drain"}, 2, 0);
    std::vector<econ::PlayerMove> moves;
    jumpTo(*w, p, 2 + 8.5 / 24);
    keep(moves, p);
    std::map<std::string, std::vector<std::string>> at;
    for (const auto& id : p.playersOf("drain"))
        at[p.benchOf(id)].push_back(id);
    expect(p.drainTown() == "larkfield" && p.playersOf("drain").size() == 2 && at["ls"].size() == 2,
           "two at Larkfield's one bench: " + p.drainTown() + ", " + std::to_string(p.playersOf("drain").size()));
    for (const auto& id : at["ls"])
        expect(w->entity(id) && w->entity(id)->cellId == cellId(4), id + " is at the smith's");
    std::map<std::string, std::int64_t> was;
    for (const auto& id : at["ls"])
        was[id] = s.account(id) ? s.account(id)->cash : 0;
    w->takeEvents();
    run(*w, p, 1500);                               // (Five beats: a beat is 300 s.)
    keep(moves, p);
    int working = 0;
    std::int64_t earned = 0, fromTill = 0;
    for (const auto& id : at["ls"])
    {
        const auto* j = w->jointOf(id);
        working += j && j->target == "ls";
        earned += (s.account(id) ? s.account(id)->cash : 0) - was[id];
    }
    for (const auto& e : w->takeEvents())
        if (e.kind == "economy" && e.detail == "a hand at the bench" && e.actor == s.tillOf("ls"))
            fromTill += e.coins;
    expect(count(moves, "set to work", "ls") == 2 && working == 2,
           "both at work beside the smith, keeping at it (each set to work once in five beats): " + std::to_string(working) + ", " +
               std::to_string(count(moves, "set to work", "ls")));
    expect(earned > 0 && fromTill == earned, "paid from its till: " + std::to_string(earned) + "p, " + std::to_string(fromTill) + "p");
    const auto day = p.takeDay();
    expect(day.find("\"town\":\"larkfield\",\"benches\":1,\"working\":2") != std::string::npos, "the block has them at work: " + day);
    jumpTo(*w, p, 2 + 23.9 / 24);
    expect(s.conserved(), "money stays conserved");
}

void winterBuysInTheAutumn()
{
    // The winter players lay in firewood in the calendar's autumn, never in the spring.
    const auto root = writeWorld("ratw_econ_players_winter");
    for (const double day : {100., 200.})
    {
        const bool autumn = calendar::calendarAt(day + .5).season == calendar::Season::Autumn;
        expect(autumn == (day == 200.), "day 200 is autumn's, day 100 isn't");
        auto w = worldAt(root, day + 6. / 24);
        econ::Players p(*w, {"winter"}, std::int64_t(day), 0);
        std::vector<econ::PlayerMove> moves;
        jumpTo(*w, p, day + 6. / 24);
        jumpTo(*w, p, day + 7.01 / 24);
        keep(moves, p);
        const int bought = count(moves, "bought", "firewood");
        expect(autumn ? bought >= 80 : bought == 0, "on day " + std::to_string(int(day)) + " they buy " + std::to_string(bought) + " firewood");
    }
}

void daysCountFromTheStart()
{
    // A run that starts after --players-from starts its players on its first day, says so, and counts their days (and
    // the corner's buying week) from then.
    const auto root = writeWorld("ratw_econ_players_late");
    auto w = worldAt(root, 10 + 5. / 24);
    econ::Players p(*w, {"corner", "visit"}, 0, 2);
    std::ostringstream said;
    auto* was = std::cerr.rdbuf(said.rdbuf());
    jumpTo(*w, p, 10 + 5. / 24);
    std::cerr.rdbuf(was);
    expect(said.str().find("they start on day 10") != std::string::npos, "the late start is said: " + said.str());
    expect(p.startDay() == 10, "they start on day 10");
    std::vector<econ::PlayerMove> moves;
    jumpTo(*w, p, 10 + 6.01 / 24);
    keep(moves, p);
    expect(count(moves, "bought") > 0, "the corner buys in its first week");
    jumpTo(*w, p, 11 + 23. / 24);
    expect(!p.ended(), "still there on their second day");
    jumpTo(*w, p, 12 + 1. / 960);
    keep(moves, p);
    expect(p.ended() && count(moves, "left") == 5, "gone on the third");
}

void theDeadKeepNoShop()
{
    // The smith killed in the morning keeps no shop the rest of the day: the swords dumped at ten go to nobody's purse.
    const auto root = writeWorld("ratw_econ_players_kill");
    auto w = worldAt(root, 2 + 6. / 24);
    const auto* till = w->society().account(w->society().tillOf("us"));
    expect(till && Society::stock(*till, "sword") > 0, "the smith keeps swords");
    econ::Players p(*w, {"kill", "dump"}, 2, 0);
    std::vector<econ::PlayerMove> moves;
    jumpTo(*w, p, 2 + 6. / 24);
    keep(moves, p);
    expect(count(moves, "killed") == 1 && w->entity("us")->dead, "the smith dies");
    std::int64_t before = 0;
    for (const auto& id : {std::string("us"), w->society().tillOf("us"), std::string("till:job:us")})
        if (const auto* a = w->society().account(id))
            before += Society::stockAll(*a, "sword") + Society::stockAll(*a, "iron_sword");
    jumpTo(*w, p, 2 + 10.01 / 24);
    keep(moves, p);
    std::int64_t after = 0;
    for (const auto& id : {std::string("us"), w->society().tillOf("us"), std::string("till:job:us")})
        if (const auto* a = w->society().account(id))
            after += Society::stockAll(*a, "sword") + Society::stockAll(*a, "iron_sword");
    expect(after == before, "no sword sold to the dead smith: " + std::to_string(before) + " then " + std::to_string(after));
    expect(w->society().conserved(), "money stays conserved");
}

void chestsInLots()
{
    // The whale's goods go into its chests in lots of 99 at most, so the journal (99 a move at most) has every one.
    const auto root = writeWorld("ratw_econ_players_whale");
    auto w = worldAt(root, 2 + 8. / 24);
    econ::Players p(*w, {"whale"}, 2, 0);
    jumpTo(*w, p, 2 + 8. / 24);
    auto& s = w->society();
    const auto& whale = p.playersOf("whale").front();
    int kinds = 0;
    for (const auto& g : items::allGoods())
        if (kinds < 58 && g.id != "charm" && g.id != "meal" && g.id != "herbs" && s.create(whale, g.id, 1, "test"))
            ++kinds;
    expect(s.create(whale, "charm", 190, "test"), "a pack of charms");
    w->takeEvents();
    jumpTo(*w, p, 2 + 9.01 / 24);
    std::int64_t journalled = 0;
    for (const auto& e : w->takeEvents())
        if (e.kind == "economy" && e.detail == "put in a chest" && e.item == "charm")
            journalled += e.quantity;
    std::int64_t chested = 0;
    for (const auto& id : p.accountsOf("whale"))
        if (const auto* a = s.account(id))
            chested += Society::stock(*a, "charm");
    expect(chested >= 190 && journalled == chested, "every charm put away is in the journal: " + std::to_string(journalled) + " of " +
                                                       std::to_string(chested));
    for (const auto& m : p.takeMoves())
        expect(m.action != "stowed" || (m.quantity > 0 && m.quantity <= 99), "a lot of 99 at most");
    expect(s.conserved(), "money stays conserved");
}

// econ_watch on the little world, a day and a fifth from day 0's six (its players from midnight): the files.
std::vector<std::vector<std::string>> rows(const fs::path& csv)
{
    std::vector<std::vector<std::string>> out;
    std::ifstream file(csv);
    for (std::string line; std::getline(file, line);)
    {
        std::vector<std::string> cells;
        std::string cell;
        bool quoted = false;
        for (const char c : line)
            if (c == '"')
                quoted = !quoted;
            else if (c == ',' && !quoted)
                cells.push_back(cell), cell.clear();
            else
                cell += c;
        cells.push_back(cell);
        out.push_back(cells);
    }
    return out;
}
// econ_watch --hold-pace (doc 64, Phase 13: strike's starving hands held the realm's pace at 0 for its month, so its
// hours were the pace's as much as the strike's): pace.csv has the pace each day to the last digit; a run held to its
// own pace.csv is that run, record for record; held to another, its pace is the other's whatever its own day would
// make it; a file that isn't a pace.csv is refused; and readPace reads none of a file with a bad line.
void heldPace()
{
    const auto world = writeWorld("ratw_econ_held_pace", {}, "world");
    const auto base = world.parent_path();
    const auto run = [&](const std::string& name, const std::string& more) {
        const auto command = std::string("'") + RATW_ECON_WATCH + "' '" + world.string() + "' 1.2 '" + (base / name).string() +
                             "' --deterministic --threads 1 --hours" + more + " > '" + (base / (name + ".txt")).string() + "' 2>&1";
        return std::system(command.c_str());
    };
    expect(run("free", "") == 0, "econ_watch runs by the hours: " + slurp(base / "free.txt"));
    const auto free = slurp(base / "free" / "pace.csv");
    const auto read = econ::readPace(free);
    expect(free.rfind("day,when,society_day,pace\n", 0) == 0 && read.size() >= 2 && read.count(0) && read.count(1),
           "pace.csv has the pace of each of the society's days: " + free);
    // A row at each turn of the society's day, once (a turn can fall after the watch's midnight: at the day's records
    // one went missing on DEV's map, day 36 of 44).
    {
        const auto all = rows(base / "free" / "pace.csv");
        bool once = all.size() == read.size() + 1;
        for (std::size_t i = 1; i < all.size(); ++i)
            once &= all[i].size() == 4 && all[i][1] == "turn" && std::atoll(all[i][2].c_str()) == std::int64_t(i - 1);
        expect(once, "a row at each turn of the society's day, once: " + free);
    }
    expect(run("same", " --hold-pace '" + (base / "free" / "pace.csv").string() + "'") == 0, "held to its own pace: " + slurp(base / "same.txt"));
    expect(slurp(base / "same" / "days.jsonl") == slurp(base / "free" / "days.jsonl") && slurp(base / "same" / "pace.csv") == free,
           "a run held to its own pace is that run, record for record");
    std::ofstream(base / "quarter.csv") << "day,when,society_day,pace\n0,start,0,0.25\n1,day end,1,0.25\n";
    expect(run("quarter", " --hold-pace '" + (base / "quarter.csv").string() + "'") == 0, "held to another's: " + slurp(base / "quarter.txt"));
    const auto held = econ::readPace(slurp(base / "quarter" / "pace.csv"));
    expect(held.count(1) && held.at(1) == .25 && slurp(base / "quarter" / "days.jsonl").find("\"pace\":0.25}") != std::string::npos,
           "its pace is the other's on each day: " + slurp(base / "quarter" / "pace.csv"));
    std::ofstream(base / "bad.csv") << "day,when,society_day,pace\n0,start,0,1.5\n";
    expect(run("bad", " --hold-pace '" + (base / "bad.csv").string() + "'") != 0 && slurp(base / "bad.txt").find("--hold-pace wants") != std::string::npos,
           "a pace out of 0 to 1 is refused");
    // Refused where it can't act (round 6, the tidy's review): without --hours (the pace is stepped only there), and with
    // a file that ends before the run does (the run went back to its own pace from the first day missing, unsaid).
    {
        const auto command = std::string("'") + RATW_ECON_WATCH + "' '" + world.string() + "' 1.2 '" + (base / "seconds").string() +
                             "' --deterministic --threads 1 --hold-pace '" + (base / "quarter.csv").string() + "' > '" +
                             (base / "seconds.txt").string() + "' 2>&1";
        expect(std::system(command.c_str()) != 0 && slurp(base / "seconds.txt").find("it wants --hours") != std::string::npos &&
                   !fs::exists(base / "seconds"),
               "--hold-pace without --hours is refused: " + slurp(base / "seconds.txt"));
    }
    std::ofstream(base / "short.csv") << "day,when,society_day,pace\n0,start,0,0.25\n";
    expect(run("short", " --hold-pace '" + (base / "short.csv").string() + "'") != 0 &&
               slurp(base / "short.txt").find("no pace for the society's day 1") != std::string::npos,
           "a file that ends before the run is refused: " + slurp(base / "short.txt"));
    expect(econ::readPace("day,when,society_day,pace\n0,start,0,0.1\n1,day end,x,0.2\n").empty() && econ::readPace("0,start,0,0.1\n").empty() &&
               econ::readPace("day,when,society_day,pace\n0,start,0,0.1\n3,day end,3,0.2\n").size() == 2,
           "readPace reads all of a pace.csv or none of it");
}

void econWatchsFiles()
{
    // Two days and a fifth: an employer's book is opened at its weekly review, each on its own day of the seven
    // (RatwLabour.cpp), here the first by day 2's record, so books.csv has rows to check.
    const auto world = writeWorld("ratw_econ_watch", {}, "world");
    const auto out = world.parent_path() / "out", log = world.parent_path() / "log.txt";
    const auto data = std::string(RATW_SOURCE_DIR) + "/Data";
    const auto command = "RATW_DATA_DIR='" + data + "' '" + RATW_ECON_WATCH + "' '" + world.string() + "' 2.2 '" + out.string() +
                         "' --deterministic --threads 1 --players poach,visit --players-from 1 > '" + log.string() + "' 2>&1";
    expect(std::system(command.c_str()) == 0, "econ_watch runs: " + slurp(log));
    // The stamp: what it was built from, as the build had it (econ_watch_stamp.h, which this test was built with too);
    // and the Data it read (RATW_DATA_DIR here), with its state now: a line of its own, never a warning.
    const auto stamp = slurp(out / "run.txt");
    expect(stamp.find(std::string("built from: ") + RATW_BUILT_SOURCE + " at " + RATW_BUILT_DESCRIBE + " (" + RATW_BUILT_COMMIT + ")") !=
               std::string::npos,
           "the stamp says what it was built from, as built: " + stamp);
    expect(stamp.find("\n" + econ::dataState(data) + "\n") != std::string::npos && stamp.find("\ndata: " + data) != std::string::npos,
           "the stamp says which Data it read, and its state: " + stamp);
    // Each day's rows once (the run's end repeats none), and a till's hands in tills.csv, once a day.
    const auto once = [&](const std::string& file, std::size_t key, const std::string& header) {
        const auto all = rows(out / file);
        expect(!all.empty(), file + " is there");
        std::string head;
        for (const auto& c : all.front())
            head += (head.empty() ? "" : ",") + c;
        expect(head == header, file + "'s columns: " + head);
        std::set<std::pair<std::string, std::string>> seen;
        int days = 0;
        for (std::size_t i = 1; i < all.size(); ++i)
        {
            expect(seen.insert({all[i][0], all[i][key]}).second, file + " has day " + all[i][0] + "'s " + all[i][key] + " twice");
            days = std::max(days, std::atoi(all[i][0].c_str()));
        }
        expect(all.size() > 1 && days == 2, file + " has rows, up to day 2: " + std::to_string(all.size() - 1) + " rows, to day " +
                                                std::to_string(days));
    };
    once("work.csv", 1, "day,account,title,credited,lost,till,banked");
    once("tills.csv", 1, "day,till,holder,title,cash,working,owed,rate_units,materials_day,orders_week,sold_units,takings,hands,wanted,"
                         "hire_slots,hire_taken");
    once("books.csv", 1, "day,key,town,wage,rate,open_days,calm,short_pay,vacancies,hands,skill");
    once("residents.csv", 1, "day,resident,home,holder,title,age,cash");
    // Day 1's record (made at its midnight: day 0's) has none of what the players did at that midnight; day 2's (day
    // 1's) has.
    std::ifstream days(out / "days.jsonl");
    int records = 0;
    for (std::string line; std::getline(days, line);)
    {
        ++records;
        expect(line.find("\"conserved\":true") != std::string::npos, "money conserved in every record");
        if (line.find("\"day\":1,\"when\":\"day end\"") != std::string::npos)
            expect(line.find("\"hired\"") == std::string::npos && line.find("\"joined\"") == std::string::npos,
                   "day 1's record has nothing the players did at its midnight");
        if (line.find("\"day\":2,\"when\":\"day end\"") != std::string::npos)
            expect(line.find("\"hired\"") != std::string::npos && line.find("\"joined\"") != std::string::npos, "day 2's (day 1's) has it all");
        // The end's (day 2's first hours) has what they did since day 2's midnight: the Hold's dawn pay, and none of
        // the hiring and joining of day 1.
        if (line.find("\"when\":\"end\"") != std::string::npos)
        {
            const auto block = line.find("\"players\":{\"poach\":{");
            expect(block != std::string::npos && line.find("\"paid\":[6,", block) != std::string::npos &&
                       line.find("\"hired\"", block) == std::string::npos && line.find("\"joined\"", block) == std::string::npos,
                   "the end's record has the players' doings since the last midnight: " + line.substr(std::min(block, line.size())));
        }
    }
    expect(records == 4, "a record at the start, at day 1's and day 2's midnights and at the end: " + std::to_string(records));
    bool hiredOnDay1 = false;
    for (const auto& r : rows(out / "players.csv"))
        hiredOnDay1 = hiredOnDay1 || (r.size() > 3 && r[0] == "1" && r[3] == "hired");
    expect(hiredOnDay1, "players.csv has the hiring on day 1");
    // And it knows flood and strike by name (an hour and a bit: they join, and the strike's Hold takes its hand).
    const auto later = world.parent_path() / "out-names", logged = world.parent_path() / "log-names.txt";
    const auto names = std::string("'") + RATW_ECON_WATCH + "' '" + world.string() + "' 0.05 '" + later.string() +
                       "' --deterministic --threads 1 --players flood,strike --players-from 0 > '" + logged.string() + "' 2>&1";
    expect(std::system(names.c_str()) == 0, "econ_watch runs flood and strike: " + slurp(logged));
    int joined = 0, hired = 0;
    for (const auto& r : rows(later / "players.csv"))
        if (r.size() > 3)
            joined += r[2] == "flood" && r[3] == "joined", hired += r[2] == "strike" && r[3] == "hired";
    expect(joined == 10 && hired == 1, "flood's ten join and strike's Hold takes one: " + std::to_string(joined) + ", " + std::to_string(hired));
    expect(slurp(later / "days.jsonl").find("\"staple\":\"raw_meat\"") != std::string::npos, "flood sells raw meat unless asked");
    // And the new three by name, flood's staple asked for; a staple the stores price refused.
    const auto three = world.parent_path() / "out-three", told = world.parent_path() / "log-three.txt";
    const auto again = std::string("'") + RATW_ECON_WATCH + "' '" + world.string() + "' 0.05 '" + three.string() +
                       "' --deterministic --threads 1 --players buyout,drain,gift,flood --players-staple fish --players-from 0 > '" +
                       told.string() + "' 2>&1";
    expect(std::system(again.c_str()) == 0, "econ_watch runs buyout, drain and gift: " + slurp(told));
    std::set<std::string> came;
    for (const auto& r : rows(three / "players.csv"))
        if (r.size() > 3 && r[3] == "joined")
            came.insert(r[2]);
    expect(came.count("buyout") && came.count("gift") && came.count("flood") &&
               slurp(three / "days.jsonl").find("\"staple\":\"fish\"") != std::string::npos,
           "they join, and flood sells fish as asked");
    const auto refused = std::string("'") + RATW_ECON_WATCH + "' '" + world.string() + "' 0.05 '" + (world.parent_path() / "out-meal").string() +
                         "' --players flood --players-staple meal > /dev/null 2>&1";
    expect(std::system(refused.c_str()) != 0, "a staple the stores price is refused");
}

int sh(const std::string& command)
{
    return std::system((command + " > /dev/null 2>&1").c_str());
}
std::map<std::string, std::string> stampOf(const fs::path& source, const fs::path& out, const fs::path& ceiling)
{
    // Tests/econ_watch_stamp.cmake as the build runs it, on `source` (git looks no higher than `ceiling`): its defines.
    fs::remove(out);
    expect(sh("GIT_CEILING_DIRECTORIES='" + ceiling.string() + "' '" + RATW_CMAKE + "' -DSOURCE='" + source.string() + "' -DOUT='" +
              out.string() + "' -P '" + RATW_SOURCE_DIR + "/Tests/econ_watch_stamp.cmake'") == 0,
           "the stamp's script runs on " + source.string());
    std::map<std::string, std::string> defines;
    std::istringstream text(slurp(out));
    for (std::string line; std::getline(text, line);)
        if (line.rfind("#define ", 0) == 0)
        {
            const auto name = line.substr(8, line.find(' ', 8) - 8);
            const auto open = line.find('"'), close = line.rfind('"');
            defines[name] = open < close ? line.substr(open + 1, close - open - 1) : std::string();
        }
    return defines;
}
void theStampSaysWhatChanged()
{
    // The stamp's script on a git tree of its own (Core/a.cpp and Docs/a.md, committed), and the run's check
    // (econ::sourceDrift) against what it wrote: the tree as built, a docs edit and a docs-only commit since are no
    // change; a code edit is (a warning to rebuild); built again from it, none; built from no git checkout, a note that
    // it can't be told, never a warning (it had read the run's "" against the build's "(unknown)").
    if (sh("git --version") != 0)
    {
        std::cout << "(no git here: the stamp's check not tried)\n";
        return;
    }
    const auto root = scratch("ratw_econ_stamp");
    const auto tree = root / "tree", plain = root / "plain", header = root / "stamp.h";
    fs::create_directories(tree / "Core");
    fs::create_directories(tree / "Docs");
    fs::create_directories(tree / "Tests");
    fs::create_directories(tree / "Data" / "Items");
    fs::create_directories(plain / "Core");
    fs::create_directories(plain / "Data");
    std::ofstream(tree / "Core" / "a.cpp") << "int a() { return 1; }\n";
    std::ofstream(tree / "Docs" / "a.md") << "# A\n";
    std::ofstream(tree / "Tests" / "econ_players.cpp") << "int p() { return 1; }\n";
    std::ofstream(tree / "Tests" / "other_tests.cpp") << "int o() { return 1; }\n";
    std::ofstream(tree / "Data" / "Items" / "items.json") << "{}\n";
    std::ofstream(tree / "CMakeLists.txt") << "project(a)\n";
    std::ofstream(plain / "Core" / "a.cpp") << "int a() { return 1; }\n";
    const auto git = "GIT_CEILING_DIRECTORIES='" + root.string() + "' git -C '" + tree.string() +
                     "' -c user.name=econ -c user.email=econ@example.invalid -c commit.gpgsign=false -c core.hooksPath=/dev/null "
                     "-c init.defaultBranch=main ";
    expect(sh(git + "init -q") == 0 && sh(git + "add -A") == 0 && sh(git + "commit -qm one") == 0, "a git tree of its own");
    auto built = stampOf(tree, header, root);
    const auto drift = [&] {
        return econ::sourceDrift(tree.string(), built["RATW_BUILT_COMMIT"], built["RATW_BUILT_CODE"], built["RATW_BUILT_PATHS"]);
    };
    expect(!built["RATW_BUILT_PATHS"].empty() && built["RATW_BUILT_CODE"].size() == 12 && built["RATW_BUILT_COMMIT"].size() == 40,
           "the stamp has the commit, the code's changes and its paths: " + slurp(header));
    expect(drift().empty(), "run from the tree it was built from: nothing to say: " + drift());
    std::ofstream(tree / "Docs" / "a.md", std::ios::app) << "More.\n";
    expect(drift().empty(), "a docs edit since: nothing to say: " + drift());
    expect(sh(git + "commit -qam docs") == 0, "the docs committed");
    expect(drift().empty(), "a docs-only commit since: nothing to say: " + drift());
    // Another suite's test is no change of econ_watch's; its own players' code is.
    std::ofstream(tree / "Tests" / "other_tests.cpp", std::ios::app) << "int q() { return 2; }\n";
    expect(drift().empty(), "another suite's edit: nothing to say: " + drift());
    std::ofstream(tree / "Tests" / "econ_players.cpp", std::ios::app) << "int q() { return 2; }\n";
    expect(drift().rfind("WARNING: ", 0) == 0, "an edit of econ_watch's players: a warning: " + drift());
    std::ofstream(tree / "Tests" / "econ_players.cpp") << "int p() { return 1; }\n";
    expect(drift().empty(), "taken back: nothing to say: " + drift());
    // The Data it reads: its state a line to read (never a warning), apart for an edit or a file added since.
    const auto data = (tree / "Data").string();
    const auto clean = econ::dataState(data);
    expect(clean.rfind("data: " + data + " at ", 0) == 0 && clean.find("its own changes since: none") != std::string::npos &&
               clean.find("WARNING") == std::string::npos,
           "Data as committed: " + clean);
    std::ofstream(tree / "Data" / "Items" / "items.json") << "{\"items\": []}\n";
    const auto edited = econ::dataState(data);
    expect(edited.find("its own changes since: none") == std::string::npos && edited.size() == clean.size() + 8,
           "Data edited: its changes' SHA-1 (12 places for 'none'): " + edited);
    std::ofstream(tree / "Data" / "Items" / "more.json") << "{}\n";
    const auto added = econ::dataState(data);
    expect(added != edited && added.find("WARNING") == std::string::npos, "a file added to Data: another: " + added);
    std::ofstream(tree / "Data" / "Items" / "more.json") << "{\"a\": 1}\n";
    expect(econ::dataState(data) != added, "and that file changed: another again");
    fs::remove(tree / "Data" / "Items" / "more.json");
    std::ofstream(tree / "Data" / "Items" / "items.json") << "{}\n";
    expect(econ::dataState(data) == clean, "taken back: as committed");
    const auto loose = econ::dataState((plain / "Data").string());
    expect(loose.find("in no git checkout") != std::string::npos && loose.find("WARNING") == std::string::npos, "Data in no checkout: said so: " + loose);
    std::ofstream(tree / "Core" / "a.cpp", std::ios::app) << "int b() { return 2; }\n";
    expect(drift().rfind("WARNING: ", 0) == 0, "a code edit since: a warning to rebuild: " + drift());
    built = stampOf(tree, header, root);
    expect(drift().empty() && built["RATW_BUILT_DESCRIBE"].find("-dirty ") != std::string::npos,
           "built again from the edit (a dirty tree, said so): nothing to say: " + drift() + "; " + built["RATW_BUILT_DESCRIBE"]);
    std::ofstream(tree / "Core" / "a.cpp") << "int a() { return 1; }\n";
    expect(drift().rfind("WARNING: ", 0) == 0, "the edit taken back since that build: a warning: " + drift());
    built = stampOf(plain, header, root);
    expect(built["RATW_BUILT_COMMIT"] == "(unknown)" && built["RATW_BUILT_CODE"] == "(unknown)", "no git checkout: unknown");
    const auto none = econ::sourceDrift(plain.string(), built["RATW_BUILT_COMMIT"], built["RATW_BUILT_CODE"], built["RATW_BUILT_PATHS"]);
    expect(none.find("WARNING") == std::string::npos && none.find("not a git checkout when it was built") != std::string::npos,
           "built from no git checkout: a note that says so, no warning: " + none);
}
} // namespace

int main()
{
    try
    {
        poachIsPaidFromTheStart();
        poachShortOfMoney();
        bodiesLeaveAtTheEnd();
        bodiesWalk();
        bodiesAimEveryTwoSeconds();
        stillThievesPassOver();
        floodSellsThenStops();
        floodsMeat();
        strikeTakesATenthForAMonth();
        strikeEndsWithThePlayers();
        keptHandsEat();
        keptHandsChooseTheirMeal();
        aMigrantHandLetGoHungryEats();
        strikeKeepsItsHandsFed();
        strikeStocksFoodThatKeeps();
        strikeHandsGoHomeHungry();
        buyoutHoldsTheFood();
        buyoutLastsTheWeek();
        giftToThePoorest();
        drainWorksEveryBench();
        winterBuysInTheAutumn();
        daysCountFromTheStart();
        theDeadKeepNoShop();
        chestsInLots();
        heldPace();
        econWatchsFiles();
        theStampSaysWhatChanged();
        // Every check passed: the scratch folders go (a failed run's stay, to look at).
        const auto pid = "-" + std::to_string(::getpid());
        for (const auto& root : scratches)
        {
            expect(root.filename().string().size() > pid.size() &&
                       root.filename().string().compare(root.filename().string().size() - pid.size(), pid.size(), pid) == 0,
                   root.string() + " is this run's own");
            fs::remove_all(root);
            expect(!fs::exists(root), root.string() + " is removed");
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAILED: " << e.what() << "\n";
        return 1;
    }
    std::cout << "econ_players_tests: " << checks << " checks passed\n";
    return 0;
}
