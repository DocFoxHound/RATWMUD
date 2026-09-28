// Optional physical-world soak; intentionally not part of the fast CTest suite.
// From the repository root, after building the portable library:
//   c++ -std=c++17 -ICore Tests/society_soak.cpp build-core/libratw_core.a -o /tmp/ratw-society-soak
//   /tmp/ratw-society-soak 3 > artifacts/logs/society-physical-world-probe.log
// Optional first argument: whole game days from 1 to 365 (default 3).
// Optional flags: --collapsed restores the representative old day-15 failure;
// --require-fed fails if any resident stays at hunger >=90 for two game days.
// Actual World ticks and navigation only: no teleported bodies, fake deliveries,
// clock jumps, player inputs, dialogue-provider calls, or modified fixture stock.
// The explicit --collapsed mode is a one-time initial-state fixture, not a rescue
// intervention: its money/goods match the recorded failed settlement. No assets
// are injected or positions altered after simulation begins in either mode.
#include "RatwWorld.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>

int main(int argc, char** argv)
{
    long days = 3;
    if (argc > 4)
    {
        std::cerr << "Usage: society-soak [whole-game-days: 1..365] [--collapsed] [--require-fed]\n";
        return 2;
    }
    if (argc >= 2)
    {
        char* end = nullptr;
        days = std::strtol(argv[1], &end, 10);
        if (!end || *end || end == argv[1] || days < 1 || days > 365)
        {
            std::cerr << "Game-day count must be a whole number from 1 to 365.\n";
            return 2;
        }
    }
    bool collapsed = false, requireFed = false;
    for (int index = 2; index < argc; ++index)
    {
        const std::string flag = argv[index];
        if (flag == "--collapsed" && !collapsed)
            collapsed = true;
        else if (flag == "--require-fed" && !requireFed)
            requireFed = true;
        else
        {
            std::cerr << "Unknown or repeated soak flag: " << flag << '\n';
            return 2;
        }
    }
    ratw::World world;
    if (collapsed)
    {
        auto stalled = world.save();
        stalled.calendarDays = 15.5;
        stalled.time = 216000;
        auto& society = stalled.society;
        society.budgetDay = 15;
        society.minted = 1928;
        society.sunk = 144;
        society.exportsRemaining = 8;
        society.importsRemaining = 4;
        society.herbPatch = 60;
        const std::map<std::string, std::int64_t> cash = {{"npc_cook", 301}, {"npc_keeper", 5},  {"npc_porter", 188},
                                                          {"npc_scout", 98}, {"npc_scribe", 98}, {"npc_smith", 94}};
        for (auto& resident : society.residents)
        {
            resident.second.hunger = resident.first == "npc_cook" ? 6 : 100;
            resident.second.fatigue = resident.first == "npc_scribe" ? 70 : 35;
            resident.second.task = "idle";
            resident.second.progress = 0;
            resident.second.goalCell.clear();
            resident.second.wagesToday = 0;
            society.accounts.at(resident.first).cash = cash.at(resident.first);
            society.accounts.at(resident.first).stock = {{"herbs", 0}, {"meal", 0}};
        }
        society.accounts.at("npc_cook").stock["meal"] = 3;
        society.accounts.at("npc_porter").stock["herbs"] = 6;
        const std::map<std::string, ratw::Vec2> positions = {
            {"npc_cook", {10.4753, 6.48767}}, {"npc_keeper", {10.6441, 6.46797}}, {"npc_porter", {10.5519, 6.59274}},
            {"npc_scout", {10.5028, 6.3606}}, {"npc_scribe", {10.3539, 6.44112}}, {"npc_smith", {10.3941, 6.58916}}};
        for (auto& actor : stalled.npcs)
        {
            actor.cellId = "tavern";
            actor.position = positions.at(actor.id);
        }
        if (!world.restore(stalled).ok)
        {
            std::cerr << "Representative collapse fixture failed checkpoint validation.\n";
            return 1;
        }
        std::cout << "Restored day-15 collapse: 1784 existing pennies, cook3meals, porter6herbs, patch60.\n";
    }
    std::map<std::string, long long> kinds;
    std::map<std::string, int> criticalSeconds, longestCritical;
    long long last = 0;
    const int totalSeconds = int(days * ratw::calendar::SecondsPerDay);
    for (int elapsed = 0; elapsed <= totalSeconds; elapsed += 60)
    {
        if (elapsed > 0)
            world.tick(60);
        for (const auto& event : world.society().state().ledger)
            if (event.sequence > last)
            {
                ++kinds[event.kind];
                last = event.sequence;
            }
        if (!world.society().conserved())
        {
            std::cerr << "Money conservation failed at simulation second " << elapsed << '\n';
            return 1;
        }
        for (const auto& pair : world.society().state().accounts)
        {
            if (pair.second.cash < 0)
            {
                std::cerr << "Negative cash on " << pair.first << '\n';
                return 1;
            }
            for (const auto& item : pair.second.stock)
                if (item.second < 0 || item.second > 10000)
                {
                    std::cerr << "Out-of-bounds stock on " << pair.first << ':' << item.first << '\n';
                    return 1;
                }
        }
        for (const auto& pair : world.society().state().residents)
        {
            if (!std::isfinite(pair.second.hunger) || pair.second.hunger < 0 || pair.second.hunger > 100 ||
                !std::isfinite(pair.second.fatigue) || pair.second.fatigue < 0 || pair.second.fatigue > 100)
            {
                std::cerr << "Out-of-bounds need on " << pair.first << '\n';
                return 1;
            }
            if (elapsed > 0)
            {
                criticalSeconds[pair.first] = pair.second.hunger >= 90 ? criticalSeconds[pair.first] + 60 : 0;
                longestCritical[pair.first] = std::max(longestCritical[pair.first], criticalSeconds[pair.first]);
            }
            if (requireFed && criticalSeconds[pair.first] >= 2 * ratw::calendar::SecondsPerDay)
            {
                std::cerr << "Food-access regression: " << pair.first << " has hunger >=90 for two game days at "
                          << world.calendarDays() << "; cash=" << world.society().account(pair.first)->cash << '\n';
                return 1;
            }
        }
        if (elapsed % 3600 == 0 || elapsed == totalSeconds)
        {
            std::cout << "elapsed=" << elapsed << " gameDay=" << world.calendarDays()
                      << " cash=" << world.society().moneySupply() << " minted=" << world.society().state().minted
                      << " sunk=" << world.society().state().sunk << " patch=" << world.society().state().herbPatch
                      << '\n';
            for (const auto& pair : world.society().state().residents)
            {
                const auto* actor = world.entity(pair.first);
                const auto* account = world.society().account(pair.first);
                std::cout << "  " << pair.first << ' ' << actor->cellId << ' ' << actor->position.x << ','
                          << actor->position.y << " task=" << pair.second.task << " progress=" << pair.second.progress
                          << " hunger=" << pair.second.hunger << " fatigue=" << pair.second.fatigue
                          << " cash=" << account->cash << " herbs=" << ratw::Society::stock(*account, "herbs")
                          << " meals=" << ratw::Society::stock(*account, "meal") << " activity=" << actor->activity
                          << '\n';
            }
            for (const auto& entry : kinds)
                std::cout << "  ledger " << entry.first << '=' << entry.second << '\n';
            std::cout << std::flush;
        }
    }
    for (const auto& entry : longestCritical)
        std::cout << "Longest critical hunger: " << entry.first << '=' << entry.second << " simulation seconds.\n";
    std::cout << "Completed " << days << " game days with conserved money and bounded needs/stock.\n";
    return 0;
}
