#pragma once
// Crime and law (Docs/Design/26-living-npcs.md, Phase 7). A theft or an assault is an incident, known to whoever
// perceived it at the time: nobody else, the Watch included. Witnesses tell the Watch when they meet a guard on duty
// (or, offstage, within the day), if they dare and are willing; the Watch weighs what it has been told, and what its
// guards have heard in gossip, and when that is enough it wants the offender. Guards who see someone wanted stop them:
// restitution and a fine, or, failing that, some hours held in the gaol.
#include <cstdint>
#include <string>
#include <vector>

namespace ratw
{
// Someone who perceived an incident: whether they could tell who did it, how clearly (0..1), and whether the Watch
// has their account yet.
struct Witness
{
    std::string id;
    bool identified = false;
    double clarity = 0;
    bool reported = false;
};

struct Incident
{
    std::string id;                     // "inc-<n>".
    std::string kind;                   // "theft", "attempted theft", "assault".
    std::string offender, victim, cell, town;
    double time = 0, day = 0;
    std::string item;                   // Goods taken ("" for coin).
    int quantity = 0;
    std::int64_t coins = 0;
    std::string status = "open";        // "open"; "charged" (in a warrant); "cold" (closed unsolved).
    std::vector<Witness> witnesses;
};

// What the Watch of a town holds against someone: what they must give back, to whom, and the fine.
struct Restitution
{
    std::string to, item;
    int quantity = 0;
    std::int64_t coins = 0;
};
struct Warrant
{
    std::string person, town;
    std::vector<std::string> incidents;
    std::vector<Restitution> restitution;
    std::int64_t fine = 0;
    double since = 0;                   // Game days.
};

// Held in a town's gaol until then (game days).
struct Custody
{
    std::string person, town, cell;
    double x = 0, y = 0, until = 0;
};

struct CrimeState
{
    std::int64_t nextIncident = 1;
    std::int64_t day = -1;              // The last calendar day the daily round ran.
    std::vector<Incident> incidents;
    std::vector<Warrant> warrants;
    std::vector<Custody> custody;
};
} // namespace ratw
