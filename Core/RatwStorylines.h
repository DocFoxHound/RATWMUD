#pragma once
// Storylines (Docs/Design/58-player-storytellers.md, 1): short chains of steps, each of 1-3 objectives, shown in a
// player's journal with a marker for where to go. One model for personal storylines the server makes (from a tie, a
// resident's trouble, a contract chain or the Dungeon Master), storytellers' tales, and world stories. Pure: the game
// (RatwGameStorylines.cpp) feeds it what happens (`happen`), and it says what that ticked. A trigger index maps each
// kind of objective to the storylines waiting on one, so nothing scans them in the tick. ("Journal" on screen; doc 31's
// RatwJournal is the save's journal, doc 32's Stories are scenes chained, so the code says storylines and tales.)
#include "RatwJsonDoc.h"

#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace ratw::storylines
{
// Where to go: a place, a tile and a radius, and a label ("the mill at Ser Ferro"). Never a live position.
struct Marker
{
    std::string cell, label;
    double x = -1, y = -1, radius = 0;
    bool set() const { return !cell.empty(); }
};

// One thing to do. Kinds: told (ticked by its storyteller or the DM), place (be there), talk (speak with a resident),
// scene (a qualified scene with `count` of the storyline's wolves), contract (one done, of a kind or for a poster), hunt
// (a kill of a species), fight (a fight won), gift (a Gift of a family used outside a fight), deliver (an item given).
struct Objective
{
    std::string kind, target;                       // place: a cell; talk: a resident; contract: a kind; hunt: a species;
                                                    // gift: a family; deliver: an item.
    std::string cell;                               // Where it must happen (scene, hunt, fight, gift); "" anywhere.
    std::string to;                                 // deliver: to whom; contract: the poster ("" any).
    double x = -1, y = -1, radius = 0;              // place: within `radius` of the tile (0: anywhere in the cell).
    int count = 2;                                  // scene: how many of the storyline's wolves in it.
    std::string line, suits;                        // The journal's words; who it suits (talker, nose, fighter...).
    std::string doneBy, takenBy;
    double doneAt = -1;
    bool byHand = false;                            // Judged done by its storyteller or the DM.
    bool done() const { return doneAt >= 0; }
};

struct Step
{
    std::string title, text, brief;                 // brief: one line for the resident's Mind while the step is current.
    Marker marker;
    bool distinct = false;                          // Each objective by a different wolf (doc 48, 6.8: two angles).
    std::vector<Objective> objectives;
    bool done() const;
};

struct Participant
{
    double joined = 0, left = -1;
    bool active() const { return left < 0; }
};

struct CastMember
{
    std::string name, looks;
};

struct Storyline
{
    std::string id;
    std::string kind = "personal";                  // personal, tale, world.
    std::string source, sourceRef, templateId;      // source: tie, trouble, contract, dm, storyteller, storykeeper.
    std::string owner;                              // A personal storyline's character.
    std::string authorAccount, authorCharacter;     // A tale's storyteller.
    std::string title, premise, chapter;
    std::vector<Step> steps;
    std::map<std::string, Participant> participants;
    std::vector<CastMember> cast;
    std::string state = "running";                  // draft, running, paused, done, failed, abandoned.
    double began = 0, ended = -1, lastActivity = 0; // Unix seconds.
    std::set<std::string> tracking;                 // Characters with its marker on their map.
    std::set<std::string> invited, asked;           // A tale's: invited and not yet answered; strangers asking to join.
    std::set<std::string> admitted;                 // Strangers its storyteller admitted (doc 58, 6: calls).
    bool calledOn = false;                          // A call up on a town's board (strangers may ask to join).
    std::string calledTown, callText;
    std::size_t current() const;                    // The first step not done (steps.size() when all are).
    bool takesPart(const std::string& who) const;
    bool live() const { return state == "running"; }
};

// Something that happened, as the game tells it.
struct Event
{
    std::string kind;                               // place, talk, scene, contract, hunt, fight, gift, deliver.
    std::string actor, cell, target, to;            // target: the resident, species, contract kind, family, item.
    double x = 0, y = 0;
    std::vector<std::string> together;              // scene: everyone paid in it.
};

// What it ticked: an objective, a step, or the storyline's end.
struct Progress
{
    std::string storyline, actor, kind;             // kind: objective, step, done.
    std::size_t step = 0, objective = 0;
    bool byHand = false;
};

struct Rules
{
    int personalAtOnce = 3, talesAtOnce = 3, runningTales = 2, participants = 12, cast = 6;
    int personalSteps[2] = {3, 7}, taleSteps[2] = {1, 10};
    double idleDays = 30, placeEvery = 2, narrateEvery = 3;
    int narrateLetters = 1000, castName = 40, title = 60, premise = 500, stepTitle = 80, stepText = 300;
    int visitorsAtOnce = 2, visitorMinutes[2] = {10, 60}, visitorReach = 6, diceMost = 10, diceBonus = 20;
    int applyLevel = 5, reportDays = 30, keepTextDays = 30, creditsDays = 3, creditsStars = 3;
};
const Rules& rules();
Rules parseRules(const std::string& text);

// A personal storyline's template (Data/Storylines/templates.json): its steps, with roles in braces ("{resident}",
// "{tie}", "{place}") the cast fills; a marker by role ("resident": its work place) or by place.
struct Template
{
    std::string id, title, source;
    json::Value steps;                              // As the file has them (filled at `begin`).
};
const std::vector<Template>& templates();
std::vector<Template> parseTemplates(const std::string& text);
const Template* findTemplate(const std::string& id);

// Fills a role ("resident") with a marker (the game: a resident's work place, a place's middle).
using MarkerFor = std::function<Marker(const std::string& role, const std::string& id)>;

class Book
{
  public:
    const std::map<std::string, Storyline>& all() const { return all_; }
    Storyline* find(const std::string& id);
    const Storyline* find(const std::string& id) const;
    // A personal storyline from a template, for `owner` (and `also`: a mentor's tie puts it in both journals), with its
    // cast (role -> id); its first step ticked (endowed progress). Null, with why, past the limits or a bad template.
    Storyline* begin(const Template& t, const std::string& owner, const std::vector<std::string>& also,
                     const std::map<std::string, std::string>& cast, const std::string& source, const std::string& sourceRef,
                     double now, const MarkerFor& markerFor, std::string& why);
    // A storyline made whole (a tale, a world story): kept as given, with an id if it has none.
    Storyline& add(Storyline s);
    void erase(const std::string& id);
    // What happened ticks what it fulfils: only live storylines' current steps, only for wolves taking part.
    std::vector<Progress> happen(const Event& e, double now);
    // An objective ticked by hand (its storyteller, the DM), or a `told` one.
    std::vector<Progress> tick(const std::string& id, std::size_t step, std::size_t objective, const std::string& by, double now,
                               bool byHand);
    bool take(const std::string& id, std::size_t step, std::size_t objective, const std::string& who);
    void end(const std::string& id, const std::string& state, double now);
    // Storylines `who` takes part in (live or ended lately), and how many personal or tales it is in now.
    std::vector<const Storyline*> of(const std::string& who) const;
    int counting(const std::string& who, const std::string& kind) const;
    // Kinds of objective someone is waiting on now (for the game's cheap checks: who needs a place probe).
    bool waiting(const std::string& kind) const;
    std::set<std::string> waitingFor(const std::string& kind) const;   // Characters taking part in one waiting on it.
    std::uint64_t version() const { return version_; }   // Bumped at every change (the journal views follow it).
    void touch() { ++version_; reindex(); }

    json::Value save() const;
    void load(const json::Value& saved);

  private:
    void reindex();
    std::vector<Progress> settle(Storyline& s, double now);
    std::map<std::string, Storyline> all_;
    std::map<std::string, std::set<std::string>> waiting_;   // Objective kind -> storylines whose current step waits on one.
    std::uint64_t next_ = 1, version_ = 1;
};

json::Value saveStoryline(const Storyline& s);
Storyline loadStoryline(const json::Value& o);
} // namespace ratw::storylines
