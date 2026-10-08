// Not a test: what a hunter takes home (Docs/Design/53-hunting-and-working-together.md, Phases 1 and 2). Hunters
// hunt the same wild ground over many seeded runs of fixed game time, setting out again whenever a hunt ends, and the
// sim prints, per hunter-hour and per wolf, kills taken part in, goods gained (at the catalog's price, masterwork 3×)
// and the masterwork share. Suites: a lone stalker; a pair, three and four (a driver and ambushers); a lone wolf with
// a companion; each staying in one place, and roaming between three (kills press on a place: doc 41).
//
//   hunt_sim [RUNS] [MINUTES] [SUITE]      (defaults: 20 runs of 30 game minutes, every suite)
#include "RatwItems.h"
#include "RatwSociety.h"
#include "RatwWild.h"
#include "RatwWorld.h"
#include "hunt_play.h"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace ratw;
namespace
{
constexpr int Places = 3;
std::string place(int i) { return "wilds" + std::to_string(i); }

// Three of the hunt tests' wild cells, 80 by 60: woodland and grass with scrub and a stream.
World wilds()
{
    std::map<std::string, std::string> files;
    std::string manifest = "RATW_WORLD 2\n";
    for (int i = 0; i < Places; ++i)
    {
        std::ostringstream cell;
        cell << "id: " << place(i) << "\nname: The Wilds " << i << "\ndescription: Woods and meadow.\nworld: " << i * 100
             << " 0 0\nsize: 80 60\noutdoors: true\nweather: clear\nwind: 0 0.5 1\nlighting: 1 1 warm\ngrid:\n";
        for (int y = 0; y < 60; ++y)
        {
            std::string row;
            for (int x = 0; x < 80; ++x)
                row += (x * 7 + y * 13 + i) % 11 == 0 ? 'Y' : (x * 3 + y * 5 + i) % 17 == 0 ? 'B' : x == 70 ? '~' : ',';
            cell << row << '\n';
        }
        files["cells/" + place(i) + ".cell"] = cell.str();
        manifest += "cell \"" + place(i) + "\" \"cells/" + place(i) + ".cell\"\nterritory \"" + place(i) + "\" \"wilds\" \"-\" 0\n";
    }
    manifest += "spawn \"wilds0\" 40.5 30.5\neconomy 1000 100 50 10 12\n"
                "resident \"sorrel\" \"Sorrel\" \"civilian\" \"tending the herb garden\" \"A gardener.\" \"Hello.\" 38 \"timber\" \"female\" "
                "\"average\" \"piebald\" 4 3 0 8 1 8 17 \"-\" 30 0 1 \"wilds0\" 5.5 5.5 \"wilds0\" 6.5 5.5 \"wilds0\" 7.5 5.5\n";
    files["world.ratw"] = manifest;
    World w;
    w.loadWorldFiles(files, "wilds0");
    return w;
}

struct Haul
{
    std::map<std::string, int> species;
    int kills = 0, items = 0, masterwork = 0;
    double value = 0, seconds = 0, wolves = 0;
};

double worth(const World& w, const std::string& id, int* items = nullptr, int* masterwork = nullptr)
{
    double value = 0;
    if (const auto* a = w.society().account(id))
        for (const auto& [item, n] : a->stock)
            if (const auto* g = items::good(item); g && n > 0)
            {
                value += double(g->price) * n;
                if (items)
                    *items += n;
                if (masterwork)
                    *masterwork += items::qualityOf(item) == 3 ? n : 0;
            }
    return value;
}

Haul run(const std::string& suite, bool roam, int seed, double seconds)
{
    auto w = wilds();
    const int pack = suite == "pair" ? 2 : suite == "three" ? 3 : suite == "four" ? 4 : 1;
    std::vector<std::string> wolves;
    for (int i = 0; i < pack; ++i)
    {
        const std::string id = i == 0 ? "player-s" + std::to_string(seed) : "player-s" + std::to_string(seed) + "-" + std::to_string(i);
        auto& e = w.addPlayer(id, id);
        e.cellId = place(0);
        e.position = {40.5 + i, 30.5};
        e.pace = 7;                                 // (At a trot, as a player closing on game would be.)
        wolves.push_back(id);
    }
    std::string companion;
    if (suite == "companion")
    {
        companion = "npc-fern-" + std::to_string(seed);
        auto& c = w.addPlayer(companion, "Fern");
        c.npc = true;
        c.leaderId = wolves.front();
        c.cellId = place(0);
        c.position = {41.5, 30.5};
    }
    std::map<std::string, double> start;
    std::map<std::string, int> startItems, startMaster;
    for (const auto& id : wolves)
        start[id] = worth(w, id, &startItems[id], &startMaster[id]);
    Haul h;
    const double step = 0.1;
    int at = 0;
    for (double t = 0; t < seconds; t += step)
    {
        const auto& lead = wolves.front();
        if (!w.inBattle(lead))
        {
            if (roam)
            {
                at = (at + 1) % Places;             // (Off to the next ground, where no one has hunted lately.)
                for (std::size_t i = 0; i < wolves.size(); ++i)
                    if (auto* e = w.entity(wolves[i]); e && !w.inBattle(wolves[i]))
                        e->cellId = place(at), e->position = {40.5 + double(i), 30.5};
                if (auto* c = w.entity(companion))
                    c->cellId = place(at), c->position = {41.5, 31.5};
            }
            const auto started = w.startHunt(lead);
            if (std::getenv("HUNT_TRACE") && seed == 1)
                std::printf("  [%.0f] set out in %s: %s\n", t, w.entity(lead)->cellId.c_str(), started.message.c_str());
            if (started.ok)
            {
                w.readyToFight(lead, true);
                if (const auto* b = w.battleOf(lead))
                {
                    const auto id = b->id;
                    for (std::size_t i = 1; i < wolves.size(); ++i)
                        if (w.joinBattle(wolves[i], id, 0).ok)
                            w.readyToFight(wolves[i], true);
                    if (!companion.empty())
                        w.joinBattle(companion, id, 0);
                }
            }
        }
        else if (pack == 1)
            hunt::stalkerTurn(w, lead);
        else
            for (const auto& id : wolves)
                hunt::packTurn(w, id, wolves);
        w.tick(step);
        if (std::getenv("HUNT_TRACE") && seed == 1 && !roam)
            if (const auto* bt = w.battleOf(wolves.front()))
            {
                static std::size_t shown = 0;
                if (bt->log.size() < shown)
                    shown = 0;
                for (std::size_t i = shown; i < bt->log.size(); ++i)
                    std::printf("  [%.0f] %s\n", t, bt->log[i].text.c_str());
                shown = bt->log.size();
                static double lastState = -100;
                if (t - lastState >= 30)
                {
                    lastState = t;
                    const auto* me = bt->fighter(wolves.front());
                    for (const auto& o : bt->fighters)
                        if (!w.animalOf(o.id).empty() && o.status == "fighting" && me)
                            std::printf("    t=%.0f %s %s gap %d aware %.2f stalking %d\n", t, w.animalOf(o.id).c_str(), w.animalState(o.id).c_str(),
                                        hunt::apart(me->x, me->y, o.x, o.y), w.awareness(*bt, o.id, me->id), me->stalking);
                }
            }
        for (const auto& ev : w.takeEvents())
            if (ev.kind == "hunted" && std::find(wolves.begin(), wolves.end(), ev.actor) != wolves.end())
                ++h.kills, ++h.species[ev.target];
            else if (ev.kind == "huntRole")
                ++h.species["(" + ev.target + ")"];
    }
    for (const auto& id : wolves)
    {
        int items = 0, master = 0;
        h.value += worth(w, id, &items, &master) - start[id];
        h.items += items - startItems[id];
        h.masterwork += master - startMaster[id];
    }
    h.seconds = seconds;
    h.wolves = double(pack);
    return h;
}
} // namespace

int main(int argc, char** argv)
{
    const int runs = argc > 1 ? std::atoi(argv[1]) : 20;
    const double minutes = argc > 2 ? std::atof(argv[2]) : 30;
    const std::string only = argc > 3 ? argv[3] : "";
    std::string error;
    if (!wild::load(&error))
    {
        std::cerr << "the wild's data: " << error << '\n';
        return 1;
    }
    std::printf("%d runs of %.0f game minutes; per wolf per hunter-hour\n", runs, minutes);
    std::printf("  %-10s %-6s %8s %8s %10s %11s\n", "suite", "ground", "kills", "goods", "pennies", "masterwork");
    for (const std::string suite : {"lone", "pair", "three", "four", "companion"})
    {
        if (!only.empty() && only != suite)
            continue;
        for (const bool roam : {false, true})
        {
            Haul all;
            for (int i = 1; i <= runs; ++i)
            {
                const auto h = run(suite, roam, i, minutes * 60);
                all.kills += h.kills;
                for (const auto& [sp, n] : h.species)
                    all.species[sp] += n;
                all.items += h.items;
                all.masterwork += h.masterwork;
                all.value += h.value;
                all.seconds += h.seconds;
                all.wolves = h.wolves;
            }
            const double wolfHours = all.seconds / 3600 * all.wolves;
            std::printf("  %-10s %-6s %8.2f %8.1f %10.0f %10.0f%%   ", suite.c_str(), roam ? "roam" : "stay", all.kills / (all.seconds / 3600),
                        all.items / wolfHours, all.value / wolfHours, all.items ? 100.0 * all.masterwork / all.items : 0.0);
            for (const auto& [sp, n] : all.species)
                std::printf(" %s %d", sp.c_str(), n);
            std::printf("\n");
        }
    }
    return 0;
}
