#pragma once
// Turn-based fights in arenas (Docs/Design/33-combat.md). A fight takes its fighters out of the running world into an
// arena cut from their cell: turns come from an initiative meter each fills by dexterity, and the world outside sees
// them frozen in a lineup inside a red square until it ends. World members, kept in RatwBattle.cpp.
//
// ("Encounter" is the bandits' hold-up on the road, RatwRoads.cpp; this is a Battle.)
#include <cstdint>
#include <set>
#include <string>
#include <vector>

namespace ratw
{
struct BattleFighter
{
    std::string id;
    int side = 0;                   // 0: those who started it; 1: those they set on.
    int x = 0, y = 0;               // Tile in the arena (the cell's own tile coordinates).
    int facing = 0;                 // Eighths of a turn from east (y down: 2 is south).
    double meter = 0;               // Initiative, 0..100: at 100 it is their turn.
    std::string status = "fighting";    // "fighting", "downed", "dead", "fled".
    bool struggling = false;        // Downed and getting up: stands at the start of their next turn.
    bool away = false;              // Three turns let run out: skipped at once until they act again.
    int timeouts = 0;
    int order = 0;                  // When they came in (ties in the turn order go to the earlier).
    double lineupX = 0, lineupY = 0;    // Where the wolf stands, frozen, in the world.
};

struct BattleLine
{
    std::uint32_t seq = 0;
    double time = 0;
    std::string actor, target, kind, text;
};

struct Battle
{
    std::string id, cellId;
    int x0 = 0, y0 = 0, w = 0, h = 0;   // The arena, in the cell's tiles.
    std::vector<BattleFighter> fighters;
    std::set<std::string> observers;    // Watching now (bodiless: Observe mode).
    std::set<std::string> observed;     // Everyone who ever watched: they may only watch again.
    std::set<std::string> fled;         // Gone for good: they may only watch.
    std::string turn;                   // Whose turn it is ("" between turns).
    double turnStarted = 0, deadline = 0;
    bool moved = false, acted = false, extended = false;
    double weight = 0;                  // The heaviest action this turn: it delays the next.
    int turns = 0;                      // Turns taken, for the round count the square shows.
    std::uint32_t seq = 0;
    std::vector<BattleLine> log;        // The most recent lines (BattleLogKept).
    bool over = false;
    double overAt = 0;
    std::string banner;                 // "The fight is over · Bracken's side stands".
    bool pvp = false;
    std::string incident;               // An assault on a resident: the crime it is (RatwCrime.h).
    std::string camp;                   // Bandits from this camp (RatwRoads.h).
    int nextOrder = 0;

    const BattleFighter* fighter(const std::string& who) const
    {
        for (const auto& f : fighters)
            if (f.id == who)
                return &f;
        return nullptr;
    }
    BattleFighter* fighter(const std::string& who)
    {
        return const_cast<BattleFighter*>(static_cast<const Battle&>(*this).fighter(who));
    }
    bool inArena(int x, int y) const { return x >= x0 && y >= y0 && x < x0 + w && y < y0 + h; }
    // Where one can flee from: the arena's outer two rows (its very edge is often a cell's wall).
    bool onEdge(int x, int y) const { return inArena(x, y) && (x <= x0 + 1 || y <= y0 + 1 || x >= x0 + w - 2 || y >= y0 + h - 2); }
    int standing(int side) const
    {
        int n = 0;
        for (const auto& f : fighters)
            n += f.side == side && f.status == "fighting";
        return n;
    }
};

// A challenge to fight between players (doc 33: a fight between players needs the other's yes).
struct Challenge
{
    std::string from, to;
    double until = 0;               // World seconds; silence is a no.
};

namespace battle
{
// Placeholder numbers, to be tuned with play (doc 33).
constexpr int ArenaWidth = 64, ArenaHeight = 48;     // Twice a 32×24 map view at the default zoom.
constexpr int GrowthFrom = 2;                        // Each fighter past these two adds a tile each way.
constexpr double TurnSeconds = 30, TypingExtra = 15, NpcPause = 1.5;
constexpr int AwayAfter = 3;
constexpr double BannerSeconds = 2.0, FadeSeconds = .5, SettleSeconds = 5;
constexpr double ChallengeSeconds = 30, StartReach = 3.0;
constexpr double BiteDamage = 12, BiteStamina = 8;
constexpr double TendStamina = 10, StruggleUpHealth = 15, TendedHealth = 20;
constexpr double DownedBite = 15 * 60, DownedBlunt = 20 * 60, DownedFire = 12 * 60;
constexpr double DownedMinimum = .6, OverkillSeconds = 10, DownedTurnSeconds = 60;
constexpr double StruggleSeconds = 20, TendSeconds = 10;   // Out of a fight.
constexpr std::size_t BattleLogKept = 40;
constexpr int YoungestFighter = 13;

// The meter a fighter gains each tick, and how far they may move in a turn (injury shortens it).
inline double meterGain(double dexterity) { return 6 + dexterity / 10; }
inline double injuryFactor(double hurt) { return 1 - .6 * (hurt / 100); }
int moveRange(double dexterity, double hurt);
// Stamina back at the start of each of one's own turns.
double staminaPerTurn(double hurt);
// Eighths of a turn from east for a step (dx, dy); and how far apart two facings are (0..4).
int octant(double dx, double dy);
inline int octantGap(int a, int b)
{
    const int d = ((a - b) % 8 + 8) % 8;
    return d > 4 ? 8 - d : d;
}
// How a fighter fights (NPCs by their trade and age; doc 33's temperament table).
struct Temperament
{
    double skill = 50;
    std::string kind = "cautious";  // "aggressive", "cautious", "timid".
    double fleeBelow = 0;           // Health (100 − hurt) under which they run.
};
Temperament temperament(const std::string& role, bool bandit, int age, bool npc);
} // namespace battle
} // namespace ratw
