// econ_watch's players who push (Tests/econ_players.h; Docs/Design/64-economy-migration.md, Phase 13), on a little
// world of two towns: Ridgemere, the town with the most shops (a shopkeeper, a smith, a stall, six hands paid from the
// shopkeeper's till), and Larkfield (a shopkeeper, three hands). And econ_watch itself, run on the same world for a day
// and a bit: its stamp, its files' rows, and which day's record a midnight's doings fall in.
#include "RatwCalendar.h"
#include "RatwCamps.h"
#include "RatwItems.h"
#include "RatwWorld.h"
#include "econ_players.h"

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
fs::path writeWorld(const std::string& name)
{
    const auto root = fs::temp_directory_path() / name;
    const std::string layout = "UU.SS";
    fs::remove_all(root);
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
    const auto root = writeWorld("ratw_econ_watch_world");
    const auto out = fs::temp_directory_path() / "ratw_econ_watch_out";
    fs::remove_all(out);
    const auto command = std::string("'") + RATW_ECON_WATCH + "' '" + root.string() + "' 1.2 '" + out.string() +
                         "' --deterministic --threads 1 --players poach,visit --players-from 1 > '" + (out.string() + ".log") + "' 2>&1";
    expect(std::system(command.c_str()) == 0, "econ_watch runs: " + slurp(out.string() + ".log"));
    // The stamp: what it was built from, said at build time.
    const auto stamp = slurp(out / "run.txt");
    expect(stamp.find(std::string("built from: ") + RATW_SOURCE_DIR + " at ") != std::string::npos, "the stamp says what it was built from: " + stamp);
    // Each day's rows once (the run's end repeats none), and a till's hands in tills.csv, once a day. (No employer keeps
    // a book in its first days here: books.csv is its header alone.)
    const auto once = [&](const std::string& file, std::size_t key, const std::string& header, bool any = true) {
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
        expect(!any || days == 1, file + " reaches day 1");
    };
    once("work.csv", 1, "day,account,title,credited,lost,till,banked");
    once("tills.csv", 1, "day,till,holder,title,cash,working,owed,rate_units,materials_day,orders_week,sold_units,takings,hands,wanted,"
                         "hire_slots,hire_taken");
    once("books.csv", 1, "day,key,town,wage,rate,open_days,calm,short_pay,vacancies,hands,skill", false);
    once("residents.csv", 1, "day,resident,home,holder,title,age,cash");
    // Day 1's record (made at its midnight: day 0's) has none of what the players did at that midnight; the end's has.
    std::ifstream days(out / "days.jsonl");
    int records = 0;
    for (std::string line; std::getline(days, line);)
    {
        ++records;
        expect(line.find("\"conserved\":true") != std::string::npos, "money conserved in every record");
        if (line.find("\"day\":1,\"when\":\"day end\"") != std::string::npos)
            expect(line.find("\"hired\"") == std::string::npos && line.find("\"joined\"") == std::string::npos,
                   "day 1's record has nothing the players did at its midnight");
        if (line.find("\"when\":\"end\"") != std::string::npos)
            expect(line.find("\"hired\"") != std::string::npos && line.find("\"joined\"") != std::string::npos, "the end's has it all");
    }
    expect(records == 3, "a record at the start, at day 1's midnight and at the end: " + std::to_string(records));
    bool hiredOnDay1 = false;
    for (const auto& r : rows(out / "players.csv"))
        hiredOnDay1 = hiredOnDay1 || (r.size() > 3 && r[0] == "1" && r[3] == "hired");
    expect(hiredOnDay1, "players.csv has the hiring on day 1");
}
} // namespace

int main()
{
    try
    {
        poachIsPaidFromTheStart();
        bodiesLeaveAtTheEnd();
        bodiesWalk();
        winterBuysInTheAutumn();
        daysCountFromTheStart();
        theDeadKeepNoShop();
        chestsInLots();
        econWatchsFiles();
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAILED: " << e.what() << "\n";
        return 1;
    }
    std::cout << "econ_players_tests: " << checks << " checks passed\n";
    return 0;
}
