#pragma once
// Injuries that outlast a fight (Docs/Design/38-injuries.md, phases 3 and 4): acute ones, which heal with rest, and
// lasting ones, marks for life. The tables, how one is given and grows worse, what they do to a wolf (worked out from
// the list wherever it matters, never stored apart), how rest heals them, and how they are told. Nothing here knows the
// world: World (RatwBattle.cpp, RatwWorld.cpp) decides when one is given and how much rest was had.
#include <string>
#include <vector>

namespace ratw
{
struct Injury
{
    std::string id;
    std::string kind = "acute";      // "acute" or "lasting".
    std::string type;                // "cracked_rib", "torn_ear"...
    std::string side;                // "left", "right" or "".
    std::string cause;               // "bite", "sword", "blunt" or "fire".
    std::string from;                // Who or what gave it, in words ("a bandit").
    int severity = 1;                // Acute: 1 minor, 2 moderate, 3 severe, as it was got (it eases as it heals).
    double restLeft = 0, restFull = 0;   // Acute: rest hours still needed, of how many.
    double gotDay = 0;               // The calendar day it was got.
};

namespace injury
{
// What a wolf's injuries do, together (doc 38's "Effects", the lasting ones' too). Acute ones stack with floors: speed
// above a walk at least 60%, stamina recovery at least 50%, each sense at least 50%; lasting ones take at most 25% off
// any one ability.
struct Effects
{
    double sprint = 1;               // Of the speed above a walk, this much is left.
    bool slowWalk = false;           // A severe leg: the walk itself slows (to 2.0 tiles a second).
    int arenaMove = 0;               // Tiles off a fight move.
    double recovery = 1;             // Stamina coming back, times this.
    double attackStamina = 0;        // Breath an attack costs, more.
    double biteLess = 0, swordLess = 0;   // Damage off a bite, a sword stroke.
    double hearing = 1, vision = 1, smell = 1;
    double initiative = 1;           // The fight's bar fills times this.
    double fireExtra = 0;            // Fire hurts the burned this much more.
    bool any() const;
};
Effects effects(const std::vector<Injury>& injuries);

// The kinds: what part of the body each touches ("leg", "ribs", "neck", "muzzle", "ear", "head", "burns", and for
// lasting marks "tail", "flank", "eye", "nose"), and its name.
bool known(const std::string& type);
bool lasting(const std::string& type);
std::string part(const std::string& type);
std::string name(const Injury& i);               // "Cracked rib", "Torn left ear".
int severityNow(const Injury& i);                // As it heals: severe, then moderate, then minor.
std::string severityWord(int severity);          // "minor", "moderate", "severe".
std::string does(const Injury& i);               // What it does, in a few words.
// The sheet's line: "Cracked rib: moderate, healing (about 2 more days of rest)"; a lasting one with when and how
// ("Torn left ear: in a fight with a bandit, early spring, year 2").
std::string describe(const Injury& i);
// What another sees on a closer look ("limping on a bitten foreleg, and a torn left ear"), "" for nothing.
std::string visible(const std::vector<Injury>& injuries);
std::string dateWords(double day);               // "early spring, year 2".

// An acute injury from a blow: its cause and damage pick the kind and how bad (doc 38's tables). `downing`: it was the
// blow that downed the wolf; `downs`: downings since its last full rest, this one counted; `roll`, `roll2` in [0, 1).
Injury acute(const std::string& cause, double damage, double overkill, bool downing, int downs, double roll, double roll2);
// A lasting injury from a cause (`roll` in [0, 1)), none if the wolf already has every mark it could get from it.
// `from`: an acute injury setting into its lasting form (a leg's into a limp...), else empty.
Injury lastingFrom(const std::string& cause, const std::vector<Injury>& has, double roll, const Injury* from = nullptr);
// An injury of a kind given outright (a Dungeon Master's, doc 38 phase 5): acute at a severity with the middle of its
// healing time, or lasting. Its type is empty if the kind is unknown.
Injury given(const std::string& type, int severity, const std::string& side, const std::string& from);
// Adds one given outright to a list (acute: as addAcute; lasting: unless the list is full). False if it couldn't.
bool give(std::vector<Injury>& injuries, const Injury& i);
// Takes one away by its id: its name, or "" if there was none.
std::string takeAway(std::vector<Injury>& injuries, const std::string& id);
// Adds an acute injury: the same kind already there grows a severity worse and starts healing again instead. The
// injury as it now stands.
const Injury& addAcute(std::vector<Injury>& injuries, Injury i);
// Rest heals acute injuries (`hours` of rest, already weighed by how it was had). The names of those healed.
std::vector<std::string> heal(std::vector<Injury>& injuries, double hours);
// Strain sets healing back: this share of the healing done so far on each acute injury is lost.
void strain(std::vector<Injury>& injuries, double share);
// The lasting-injury chance's rise for days without a full rest: +5 points a day after the first, at most +25.
double weariness(double daysSinceFullRest);
constexpr std::size_t MostKept = 24;            // On one character, for safety.
} // namespace injury
} // namespace ratw
