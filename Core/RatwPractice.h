#pragma once
// Practice (Docs/Design/49-characters-and-earned-gifts.md, Design 3 and 4): the one way a wolf's attributes and skills
// grow. Every number is in Data/Progression/skills.json, a placeholder for the user's balance pass. This header has the
// catalog and the pure arithmetic (no World), so tests and Tests/practice_sim.cpp can drive it; World::practise
// (RatwProgress.cpp) applies it to a wolf.
//
//   gain = base × room × partner × teacher × variety × soft     (then rested practice doubles it, from the pool)
//   room = (cap − value) / cap, at least `room.floor` below the cap, 0 at it
//   soft = 1 until today's gains in the skill reach its soft limit, then `soft.after`
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace ratw
{
// What practice keeps on a character. `days`, `rested` and `lastGainAt` are saved; the rest is for this run only.
struct PracticeState
{
    struct Day
    {
        double start = 0, gained = 0;               // When the skill's rolling day began (real seconds), and its gains since.
    };
    std::map<std::string, Day> days;                // By skill.
    double rested = 0;                              // Rested practice left (points; doubles gains until spent).
    double lastGainAt = -1;                         // When anything last grew (real seconds); -1 never.
    // Not saved:
    struct Seen
    {
        std::string key, occasion;
        double at = 0;
    };
    std::vector<Seen> ring;                         // The latest occasions of practice, for variety.
    double partnersDay = -1;                        // When today's partners began.
    std::map<std::string, std::vector<std::string>> partners;   // A partner wolf -> the occasions practised with them today.
    std::map<std::string, std::pair<double, int>> told;          // By skill: when a growth line last went, and the step it told.
    double carried = 0, ran = 0;                    // Tiles walked under a heavy load, and run, toward the next practice of each.
};

namespace practice
{
// Who or what a wolf practised with or against (World::practise says what each part means).
struct Context
{
    std::string partner, partnerKind, occasion, teacher;
    double partnerValue = -1;
    double amount = 1;                              // How many of the source's units this was (10 mana spent: 1).
    Context() = default;
    Context(std::string partnerId, std::string kind = {}, std::string occasionName = {})
        : partner(std::move(partnerId)), partnerKind(std::move(kind)), occasion(std::move(occasionName))
    {
    }
};
// An attribute or a skill, as Data/Progression/skills.json has it.
struct Skill
{
    std::string id, name, shortName;
    std::string field;                              // The Entity field it lives in; empty for the trade skills (Entity::skills).
    bool attribute = false;
    double start = 0, cap = 100, quickenedCap = -1, specialtyStart = -1, softPerDay = 1, lineStep = 1;
    bool percent = false;                           // Told as a percentage (the senses), not a whole number.
    std::string line;                               // "Your tracking sharpened ({value})."
};
// Something that makes skills grow, and how much of each (its base, before the factors).
struct Source
{
    std::string id;
    std::vector<std::pair<std::string, double>> grows;
    bool byPartner = false;                         // Its partner's kind and skill change it (a fight's foe).
    bool partnerDecay = false;                      // The same partner again today teaches less.
};
struct Rules
{
    double roomFloor = .1;
    double softDay = 86400, softAfter = .2;
    std::map<std::string, double> partnerKinds;     // "player", "resident", "animal", "fierce", "post".
    double weakerBy = 15, weaker = .5, betterBy = 10, better = 1.5;
    double teachReach = 6, teachBetterBy = 10, teachNearby = 1.5, teachCache = 30;
    std::map<std::string, double> teacherKinds;     // "mentor", "trainer", "master".
    double restedAwayFor = 86400, restedPerDay = 5, restedMost = 30, restedRate = 1;
    double occasion = 60, varietyWindow = 600, varietyFactor = .25, block = 4;
    int varietyRepeats = 3, ring = 16;
    std::vector<double> partnerDecay{1, .5, .25, 0};
    double lineThrottle = 120;
    std::string capLine;
    double seasoned = .5, veteran = .9;             // The simulators' bands: shares of the way from start to cap.
};

// Loads the catalog once (RATW_DATA_DIR, the working directory or above it, or the source tree); false, with the
// reason, if it can't. Everything below works on an empty catalog (nothing grows) when it can't.
bool load(std::string* error = nullptr);
const std::vector<Skill>& skills();                 // Attributes first, then skills, in catalog order.
const Skill* skill(const std::string& id);
const Source* source(const std::string& id);
const Rules& rules();

// A skill's cap for a wolf: an attribute's by its grade ("weak", "strong"; "" or "plain": plain) from creation.json;
// Quickened wolves fight to the skill's `quickenedCap`.
double capFor(const Skill& s, bool quickened, const std::string& grade = {});
// How much room is left to grow (1 empty, 0 at or past the cap).
double room(double value, double cap, const Rules& r);
// The soft limit: 1 until today's gains reach the skill's limit, then `softAfter`. `day` is moved on when a day has gone.
double soft(PracticeState::Day& day, const Skill& s, double now, const Rules& r);
// The partner's kind and skill: a weaker foe teaches less, a better one more.
double partnerFactor(const std::string& kind, double partnerValue, double value, const Rules& r);
// Variety: the same key (skill, source, partner or place) on more than `varietyRepeats` occasions in the window counts
// for less. One occasion is everything with the same key and occasion name, or within `occasion` seconds of the last.
double variety(PracticeState& p, const std::string& key, const std::string& occasion, double now, const Rules& r);
// The same partner on another occasion today: ×1, ×½, ×¼, then nothing.
double partnerDecay(PracticeState& p, const std::string& partner, const std::string& occasion, double now, const Rules& r);
// Rested practice: a gap of a day or more since the last gain fills the pool; then each gain is doubled from it.
// Returns the extra the pool gives (outside the soft limit), and spends it.
double rested(PracticeState& p, double gain, double now, const Rules& r);
// Whether a growth line is due: the value passed a whole step since the last told and the throttle has gone by.
// Returns the step to tell (or -1), and marks it told.
int lineDue(PracticeState& p, const Skill& s, double before, double after, double now, const Rules& r);
// The line's words with the value filled in ("Your tracking sharpened (31).", "Your nose grows keener (112%).").
std::string lineText(const Skill& s, double value);
std::string capText(const Skill& s);

// Social standing (doc 49, 7; Data/Progression/standing.json): the social level curve and its titles, and which ledger
// receipts are social XP. Defaults (doc 44's curve, its titles) if the file can't be read.
struct Standing
{
    double stepBase = 100, stepGrowth = 50;
    std::vector<std::pair<int, std::string>> titles{{1, "Stranger"}};
    std::vector<std::string> socialReasons{"qualified_session_settlement", "gold_star", "story_star", "story_closure"};
    int dailyCap = 150;
};
const Standing& standing();

// Building a wolf at creation (doc 49, 2 and 3; Data/Progression/creation.json): each attribute's start and cap by
// grade, the budget, the specialties and presets, and what the stamina attribute does.
struct Grade
{
    double start = 0, cap = 0;
};
struct Specialty
{
    std::string id, name, skill;
};
struct Creation
{
    std::map<std::string, std::map<std::string, Grade>> grades;   // Attribute -> "weak"/"plain"/"strong".
    int budget = 2, strongCost = 1, weakRefund = 1, mostStrong = 3, mostWeak = 3;
    double capBonus = 0;
    std::vector<Specialty> specialties;
    std::map<std::string, int> tierCost;
    double recoveryBase = .8, recoveryPer = .004, drainBase = 1.2, drainPer = .004, turnPer = .04;
};
const Creation& creation();
const Specialty* specialty(const std::string& id);
// A wolf's build: a grade for each attribute that isn't plain, and its specialty ("" for none).
struct Build
{
    std::map<std::string, std::string> grades;
    std::string specialty;
};
// (Checking a creator's build, and the creator's copy of all this: RatwCreation.h, which needs the JSON types.)
// What the stamina attribute does (1 at 50): how fast the bar comes back, how much running drains it, and what a fight
// turn gives back beyond the usual.
double staminaRecovery(double endurance);
double staminaDrain(double endurance);
double staminaPerTurnExtra(double endurance);
long long xpFor(int level);                         // Social XP to reach a level (1: 0).
int levelFor(long long xp);
std::string titleFor(int level);
bool socialReason(const std::string& reason);       // A receipt that counts as social XP.
} // namespace practice
} // namespace ratw
