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
    // treasury is emptied. At the second dawn the Hold notes what it lacked ("short", the pennies as its quantity: none
    // moved), pays what it can, and the rest go home unpaid.
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
    const auto purse = s.account(chapter)->cash;
    if (const auto left = s.account("treasury")->cash; left > 0)
        expect(s.shift("treasury", "chapter:econ-test-sink", "", 0, left, "test: the treasury emptied"), "the treasury emptied");
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
    expect(paid + unpaid == 6 && unpaid >= 1, "at the second dawn some go home unpaid: " + std::to_string(paid) + " paid, " +
                                                   std::to_string(unpaid) + " unpaid");
    if (wage5 == 0)
        wage5 = 2 * std::max<std::int64_t>(camp::HoldWage, std::int64_t(std::ceil(s.labourDay("") - 1e-9)));
    expect(lacking > 0 && moved == 0 && lacking == 6 * wage5 - purse - funded,
           "the shortfall noted as its quantity, no pennies moved: " + std::to_string(lacking) + " of " + std::to_string(6 * wage5));
    expect(int(p.staffOf("poach").size()) == paid, "the paid stay");
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
    // flood's ten players land a pack of fish each morning and sell it on the hour to the capital's shops that buy it
    // (here a fishmonger), as many as they take (its shelf to its supplies' 40); after two weeks, they stop. The
    // capital's fisher is its maker, watched in the day's block.
    const auto root = writeWorld("ratw_econ_players_flood",
                                 resident("uf", "Fishmonger U", "merchant", "the fishmonger at the quay", 0, 14.5, 1, 13.5) +
                                     resident("ux", "Fisher U", "civilian", "fishes the lake", 0, 15.5, 1, 15.5));
    auto w = worldAt(root, 2 + 7.9 / 24);
    auto& s = w->society();
    econ::Players p(*w, {"flood"}, 2, 0);
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
    expect(day.find("\"selling\":true") != std::string::npos && day.find("\"makers\":1,") != std::string::npos &&
               day.find("\"makers_held\":1,") != std::string::npos && day.find("\"asked\":") != std::string::npos,
           "the day's block has the price and the maker: " + day);
    // Two weeks on, at eight, they stop; and sell no more.
    jumpTo(*w, p, 16 + 7.9 / 24);
    jumpTo(*w, p, 16 + 8.01 / 24);
    keep(moves, p);
    expect(count(moves, "stopped") == 10, "after two weeks they stop");
    const int then = count(moves, "sold", "fish");
    jumpTo(*w, p, 16 + 9.01 / 24);
    jumpTo(*w, p, 17 + 8.01 / 24);
    keep(moves, p);
    expect(count(moves, "sold", "fish") == then && count(moves, "landed") == 10 * 99, "and land and sell no more");
    expect(p.takeDay().find("\"selling\":false") != std::string::npos, "the block says so");
    expect(s.conserved(), "money stays conserved");
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
void econWatchsFiles()
{
    // Two days and a fifth: an employer's book is opened at its weekly review, each on its own day of the seven
    // (RatwLabour.cpp), here the first by day 2's record, so books.csv has rows to check.
    const auto world = writeWorld("ratw_econ_watch", {}, "world");
    const auto out = world.parent_path() / "out", log = world.parent_path() / "log.txt";
    const auto command = std::string("'") + RATW_ECON_WATCH + "' '" + world.string() + "' 2.2 '" + out.string() +
                         "' --deterministic --threads 1 --players poach,visit --players-from 1 > '" + log.string() + "' 2>&1";
    expect(std::system(command.c_str()) == 0, "econ_watch runs: " + slurp(log));
    // The stamp: what it was built from, as the build had it (econ_watch_stamp.h, which this test was built with too).
    const auto stamp = slurp(out / "run.txt");
    expect(stamp.find(std::string("built from: ") + RATW_BUILT_SOURCE + " at " + RATW_BUILT_DESCRIBE + " (" + RATW_BUILT_COMMIT + ")") !=
               std::string::npos,
           "the stamp says what it was built from, as built: " + stamp);
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
    fs::create_directories(plain / "Core");
    std::ofstream(tree / "Core" / "a.cpp") << "int a() { return 1; }\n";
    std::ofstream(tree / "Docs" / "a.md") << "# A\n";
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
        strikeTakesATenthForAMonth();
        winterBuysInTheAutumn();
        daysCountFromTheStart();
        theDeadKeepNoShop();
        chestsInLots();
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
