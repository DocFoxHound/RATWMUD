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
    double meter = 0;               // Initiative, 0..100, filling in real time: full, it is their turn when it comes.
    double readyAt = -1;            // When it last filled.
    // Its own turn (doc 33): taken the moment its bar is full, alongside anyone else whose bar is.
    bool acting = false, moved = false, acted = false, extended = false;
    double turnStarted = 0, deadline = 0;
    double weight = 0;              // The heaviest action this turn: it sets the bar back.
    std::string status = "fighting";    // "fighting", "downed", "dead", "fled".
    bool struggling = false;        // Downed and getting up: stands at the start of their next turn.
    bool away = false;              // Three turns let run out: skipped at once until they act again.
    int timeouts = 0;
    int order = 0;                  // When they came in (ties in the turn order go to the earlier).
    double lineupX = 0, lineupY = 0;    // Where the wolf stands, frozen, in the world.
    int burning = 0;                // Turns of Burning left (3 damage at the start of each).
    bool casting = false;           // Charging a spell: can't move until it goes off.
    bool scared = false;            // An NPC the fire put to flight.
    bool truce = false;             // Agreed to the truce on offer.
};

// A spell charging (the tell): it goes off when its meter fills, on the tiles locked when it began.
struct BattleCast
{
    std::string caster, spell;
    int dir = 0;
    std::vector<std::pair<int, int>> tiles;
    double meter = 0, gain = 10, mana = 0;
    double castAt = 0, firesAt = 0;         // When it began, and when it goes off: a hard countdown everyone sees.
    bool quickened = false;
};

// Something on the arena's ground: a sword knocked loose.
struct BattleDrop
{
    int x = 0, y = 0;
    std::string item, owner;
};

struct BattleLine
{
    std::uint32_t seq = 0;
    double time = 0;
    std::string actor, target, kind, text;
    std::vector<std::pair<int, int>> tiles;     // Where a spell went off (for the page's flame).
};

struct Battle
{
    std::string id, cellId;
    int x0 = 0, y0 = 0, w = 0, h = 0;   // The arena, in the cell's tiles.
    std::vector<BattleFighter> fighters;
    std::set<std::string> observers;    // Watching now (bodiless: Observe mode).
    std::set<std::string> observed;     // Everyone who ever watched: they may only watch again.
    std::set<std::string> fled;         // Gone for good: they may only watch.
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
    std::vector<BattleCast> casts;
    std::vector<BattleDrop> drops;
    std::string truceBy;                // Who offered a truce now on the table ("" for none).
    std::vector<std::pair<std::pair<int, int>, int>> smoke;   // Tiles of smoke and the round they clear.
    double lookedAround = -1;           // When it last looked for who can hear it.

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

// Something lying in the world: a sword knocked loose in a fight, dropped where it fell.
struct GroundItem
{
    std::string id, cellId, item;
    double x = 0, y = 0;
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
constexpr double TurnSeconds = 10, TypingExtra = 10, NpcPause = 1.5;
// The initiative bar fills in real time: at DEX 50 (a gain of 11) in fifteen seconds.
constexpr double MeterPerSecond = 100.0 / (11 * 15);
constexpr int AwayAfter = 3;
constexpr double BannerSeconds = 2.0, FadeSeconds = .5, SettleSeconds = 5;
constexpr double ChallengeSeconds = 30, StartReach = 3.0;
constexpr double BiteDamage = 12, BiteStamina = 8;
constexpr double TendStamina = 10, StruggleUpHealth = 15, TendedHealth = 20;
constexpr double DownedBite = 15 * 60, DownedBlunt = 20 * 60, DownedFire = 12 * 60;
constexpr double DownedMinimum = .6, OverkillSeconds = 10;
// A player is never killed (doc 38): they lie down for a while, longer for each downing since their last full rest, and
// get up at GetUpHealth. The bases are for the cause, as above; the stretch is by downings, counting this one.
constexpr double GetUpBite = 150, GetUpBlunt = 180, GetUpFire = 120, GetUpHealth = 10;
constexpr double GetUpOverkillSeconds = 2, GetUpLongest = 30 * 60;
inline double getUpStretch(int downs) { return downs <= 1 ? 1 : downs == 2 ? 2 : downs == 3 ? 4 : downs == 4 ? 6 : 8; }
// Rest (doc 38), in game hours: lying or sitting still, unbroken, is a partial rest; six hours of it lying in a bed is a
// full rest, which resets the downings, gives back the struggle-up and is when the last one was. Logged out counts
// half again, in a bed if they left lying in one.
constexpr double RestHourSeconds = 600, FullRestHours = 6, AwayRestRate = 1.5;
constexpr double StruggleSeconds = 20, TendSeconds = 10;   // Out of a fight.
constexpr std::size_t BattleLogKept = 60;
constexpr int YoungestFighter = 13;
// The sword, held in the mouth.
constexpr double SwordDamage = 20, SwordStamina = 14, SwordWeight = 10, KnockLooseFrom = 18;
constexpr int SwordReach = 2;
// Flamethrower, a Fire Gift (Gifted, Quickened).
struct Spell
{
    double charge, length, halfAngle, damage, mana, stamina, self, weight;
};
// `charge`: seconds the fire gathers, before wisdom (÷ (1 + WIS/200)): 3.5 s Gifted, 2.6 s Quickened at WIS 30.
constexpr Spell GiftedFlame{4, 3, 23, 21, 25, 12, 3, 20};
constexpr Spell QuickenedFlame{3, 5, 35, 45, 40, 20, 5, 10};
constexpr int BurnTurns = 3, SmokeRounds = 3;
constexpr double BurnDamage = 3, RainFactor = .6, ManaPerTurn = 2, ManaPerSecond = 1.0 / 6;
inline double manaMax(double wisdom, bool gifted) { return gifted ? 20 + wisdom * .8 : 0; }
// Fighting skill grows with fighting, slower as it climbs.
constexpr double SkillPerHit = .2, SkillPerFight = .5;
constexpr double LingerSeconds = 60, NoiseReach = 30;

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
