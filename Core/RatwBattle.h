#pragma once
// Turn-based fights in arenas (Docs/Design/33-combat.md). A fight takes its fighters out of the running world into an
// arena cut from their cell: turns come from an initiative meter each fills by dexterity, and the world outside sees
// them frozen in a lineup inside a red square until it ends. World members, kept in RatwBattle.cpp.
//
// ("Encounter" is the bandits' hold-up on the road, RatwRoads.cpp; this is a Battle.)
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace ratw
{
struct Entity;

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
    // A move is a walk (doc 33): the tiles still to step onto, the next one at `stepAt`. A player faces the way they
    // walk unless they turned by hand since the move began (`turned`).
    std::vector<std::pair<int, int>> walk;
    double stepAt = 0;
    bool turned = false;
    int turnsTaken = 0;             // Turns begun: two or more and a player is paid for the fight (doc 33).
    bool resting = false;           // Rested this turn (no move): twice the stamina back at the next.
    // A turn's three parts (doc 33): the move, the action and the facing. With all three used it ends by itself, a
    // moment after the last (`partsAt`: when the last was used; another turn of the head puts it off).
    bool faced = false;
    int partsUsed = 0;
    double partsAt = 0;
    // Combat injuries (doc 38): turns of Bleeding left; Staggered (1: its bar is set back when this turn ends, 2: set
    // back already; both shown until its next turn).
    int bleeding = 0, staggered = 0;
    // Doc 37, phase 6: on guard until its next turn (harder to hit, and it turns to meet a blow); a sword taken up or
    // put away this turn (part of the move, not the action: once a turn, a tile off the move if before it).
    bool guarding = false, drew = false;
    // Stalking (doc 40): moving crouched, half as far and twice as slow, quiet, harder to see and helped by cover.
    bool stalking = false;
    // A hit zone aimed for (doc 40/35: "head", "throat", "body", "legs"; "" for wherever it lands): kept until changed.
    std::string aim;
    // A plan made while its bar fills (doc 37, phase 5): a tile to go to and an action (its target a fighter, or "x,y"
    // for fire), played out as its turn begins; the rest of the turn is still its own. `begun`: the move is under way.
    struct Plan
    {
        bool move = false;
        int x = 0, y = 0;
        std::string act, target;
        bool begun = false;
        bool empty() const { return !move && act.empty(); }
    } plan;
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
    bool hunt = false;                  // A hunt (doc 41, RatwHunt.cpp): its other side is animals.
    int nextOrder = 0;
    std::vector<BattleCast> casts;
    std::vector<BattleDrop> drops;
    std::string truceBy;                // Who offered a truce now on the table ("" for none).
    bool truced = false;                // It ended in a truce: no one stays hostile after it.
    // Its terms (doc 37): "blood" (the first wound ends it), "yield" (who would go down yields instead: no one bleeds
    // out), or "death" (doc 33's rules: the Downed bleed and may die). A challenge names them; other fights are to
    // the death.
    std::string terms = "death";
    std::string yieldBy;                // Who offered to yield, awaiting an answer ("" for none), and until when.
    double yieldUntil = 0;
    double allAwaySince = -1;           // Since when every player standing in it has been away (it lapses after a while).
    std::string opening;                // Who started it, until their first turn ends: NPCs wait for it.
    std::vector<std::pair<std::pair<int, int>, int>> smoke;   // Tiles of smoke and the round they clear.
    double lookedAround = -1;           // When it last looked for who can hear it.
    // Who has noticed whom (doc 40), observer to target: 0 unaware, from AwareSuspicious suspicious, from AwareAlert
    // alert. Only NPC and animal observers are kept; a pair not kept is alert, but game in a hunt starts unaware.
    std::map<std::pair<std::string, std::string>, double> aware;
    // Where a player last had sight of a stalking foe, and when (doc 40): a foe it has lost is shown only as a "?" there.
    struct Seen
    {
        int x = 0, y = 0;
        double at = 0;
    };
    std::map<std::pair<std::string, std::string>, Seen> seenAt;

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
    std::string terms = "yield";    // "blood", "yield" or "death" (doc 37).
};

namespace battle
{
// Placeholder numbers, to be tuned with play (doc 33).
constexpr int ArenaWidth = 64, ArenaHeight = 48;     // Twice a 32×24 map view at the default zoom.
constexpr int GrowthFrom = 2;                        // Each fighter past these two adds a tile each way.
constexpr double TurnSeconds = 20, TypingExtra = 15, NpcPause = 1.5;
// The initiative bar fills in real time: at DEX 50 (a gain of 11) in twenty-five seconds.
constexpr double MeterPerSecond = 100.0 / (11 * 25);
// Resting a turn (no move): the stamina back at the next turn, this many times over.
constexpr double RestFactor = 2;
// Combat injuries (doc 38): a bite or sword blow this hard bleeds (damage a turn, for turns); any blow this hard
// staggers (the bar set back).
constexpr double BleedFrom = 18, BleedDamage = 2, StaggerFrom = 25, StaggerSetback = 20;
constexpr int BleedTurns = 3;
constexpr int AwayAfter = 3;
constexpr double PartsGrace = 1.5;
// Pace (doc 37, phase 5): a plan plays this long into its turn (time to see it is one's turn); with no player taking a
// turn and no fire gathering, every bar fills this many times faster (no dead air).
constexpr double PlanBeat = .5, Haste = 2.5;              // A turn with move, action and facing used ends this long after the last.
// A move is walked a tile at a time (slower hurt: the injury factor), a crawl slower still.
constexpr double StepSeconds = .45, SprintStepSeconds = .2, CrawlStepSeconds = 1.0;   // At a walk; at a sprint.
constexpr int NpcPace = 6;                                     // NPCs fight at a run.
constexpr double BannerSeconds = 2.0, FadeSeconds = .5, SettleSeconds = 5;
constexpr double ChallengeSeconds = 30, StartReach = 3.0;
constexpr double YieldSeconds = 20, LapseSeconds = 60;   // An offer to yield unanswered; a fight everyone left.
constexpr double BiteDamage = 12, BiteStamina = 8;
// Guard (doc 37): a blow's chance against one on guard, this much lower. Shove: its breath, and the odds of a push
// (STR against STR), less against one on guard.
constexpr double GuardDodge = .2, ShoveStamina = 8, ShoveOdds = .6;
// Armour (doc 35, Part 8): a flat reduction by the armour on where the blow lands (its hit zone), its protection plus
// its extra against the kind of blow, less the blow's pierce; at least this share of a blow still gets through. A bite
// is a thrust of the teeth, a sword a cut, neither with any pierce.
constexpr double ArmourFloor = .25;
// Sneaking (doc 40), placeholders. Each notice adds (notice × NoticeGain) to an observer's awareness of a wolf; a check
// with almost nothing to notice calms it by Calm. Alert, it is set to AwareKept, so it takes a few calm checks to lose.
constexpr double AwareSuspicious = .3, AwareAlert = 1, AwareKept = 1.5, NoticeGain = 1.2, Calm = .25, NoticeFloor = .05;
// Stalking: a move half as long and twice as slow; cover (tall grass, ferns, reeds, a shrub, heather, or a tree or
// boulder between) hides a stalker to this share, one standing to that.
constexpr double StalkRange = .5, StalkSlow = 2, CoverStalking = .5, CoverStanding = .8;
// Scent counts for this share of a sense: noticed faintly at first, it takes a few checks to be sure.
constexpr double ScentFaint = .6;
// What one notices of another, by sense (0..1 each), and all of them together.
struct Senses
{
    double sight = 0, noise = 0, scent = 0;
    double total() const { return 1 - (1 - sight) * (1 - noise) * (1 - scent); }
};
// An ambush (§3): a first blow on a wolf unaware of its attacker is struck as from behind, this much likelier still,
// and this much harder, aimed (hit zone table 3).
constexpr double AmbushHit = .15, AmbushDamage = 1.5;
// Sneaking skill grows with an ambush, and a little with each check one stays unnoticed close by.
constexpr double SneakPerAmbush = .5, SneakUnnoticed = .05;
// Aiming for a hit zone: the blow is this much less likely to land (none on one taken unawares), and lands there if the
// side it comes at allows (no head from behind). Bandits creeping up give up after this long unseen-but-unready.
constexpr double AimPenalty = .15, CreepGiveUp = 90;
// In the open world (doc 40, §2): residents check what they notice of players near them this often (seconds), within
// this many tiles; a sneak unnoticed close by learns this much a check; a guard who hears something goes to look for
// this long. Noticing a stalker teaches a player's ears or nose this much.
constexpr double AwareEvery = .4, AwareReach = 30, SneakUnnoticedWorld = .01, LookSeconds = 20, NoticeTeaches = .3;
// Hit zones: where a blow lands, rolled by the side of the body it comes at (0 head on, 1 the side, 2 behind; 3 an
// ambush's aimed blow, doc 40: the throat, head and body over the legs). Each is
// a zone armour covers ("head", "throat", "body", "legs"), the part a fight's log names, and its weight among them.
struct HitZone
{
    const char* zone;
    const char* part;
    double weight;
};
const std::vector<HitZone>& hitZones(int quarter);
inline int quarterOf(int octantGap) { return octantGap >= 3 ? 2 : octantGap == 2 ? 1 : 0; }
// The zone a piece of armour covers, from its catalog slot ("throat", "head", "body"; "paws" are the legs), or "".
std::string armourZone(const std::string& catalogSlot);
// What a wolf has on a zone against a kind of blow ("cut", "thrust", "blunt"), and the piece's name ("" for none); and
// what all its armour takes off its dexterity for the initiative bar (0 or less: doc 35's weight).
int armourAt(const Entity& e, const std::string& zone, const std::string& type);
std::string armourPieceAt(const Entity& e, const std::string& zone);
int armourDex(const Entity& e);
// A blow of `damage` of `type` and `pierce` landing on `zone` of `target`, after the armour there.
double throughArmour(const Entity& target, const std::string& zone, double damage, const std::string& type, int pierce = 0);
// The blow to expect coming at `target` from a side (`quarter`), its zones weighed: for the page's preview.
double expectedThrough(const Entity& target, int quarter, double damage, const std::string& type, int pierce = 0);
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
// Pace (the wheel, 0 walk .. 10 sprint, as in the world) scales how far a turn's move goes: half at a walk, as far as
// DEX allows at a trot (5), half again at a sprint. Faster than a trot (4 and up) costs stamina for every tile; an
// exhausted wolf walks.
inline double paceFactor(int pace) { return .5 + .1 * pace; }
inline double tileStamina(int pace) { return pace > 3 ? (pace - 3) * .6 : 0; }
int moveRange(double dexterity, double hurt, int pace = 5);
// Stamina back at the start of each of one's own turns: by strength, less hurt.
double staminaPerTurn(double hurt, double strength = 40);
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
