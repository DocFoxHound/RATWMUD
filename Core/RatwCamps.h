#pragma once
// A Chapter's own ground (Docs/Design/32-parties-chapters-factions.md, 5.3–5.6): sites (a camp, then a fortified camp
// or Hall, then a Hold) made of structures a Chapter plans and its members build by work. Structures wear with time,
// faster when the site is left alone, and a site long abandoned is a ruin. They are a layer over the world, never the
// terrain (5.7). Pure rules: the game (RatwGameCamps.cpp) places them, takes the materials' cost, and counts the work.
#include "RatwJsonDoc.h"

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace ratw::camp
{
constexpr double SiteRadius = 12;                 // Tiles from a site's centre that are its ground.
constexpr double Apart = 20;                      // Tiles between two Chapters' sites in one place.
constexpr double SecondsPerWorkHour = 60;         // Placeholder: one worker's hour of building is a real minute.
constexpr double AbandonedSeconds = 21 * 86400.0; // Real time with no member there before it wears fast.

struct Kind
{
    const char* id;
    const char* name;
    char glyph;
    std::int64_t cost;                            // Materials, in pennies, from the Chapter's treasury.
    double hours;                                 // Work-hours to build.
    int level;                                    // The Chapter level it needs (3 camp, 4 Hall, 5 Hold).
    bool fortification;
};
const std::vector<Kind>& catalogue();
const Kind* kind(const std::string& id);

struct Structure
{
    std::string id, site, kind;
    int x = 0, y = 0;                             // Tile in the site's place.
    double work = 0;                              // Work-hours done (built when it reaches the kind's hours).
    double condition = 100;
    bool built = false;
};

struct Site
{
    std::string id, chapter, cell, name;
    int x = 0, y = 0;                             // Its centre.
    double founded = 0;                           // Calendar day.
    double visited = 0;                           // Unix seconds a member was last there.
    std::string state = "standing";               // standing, ruin.
};

// A resident working at a site for the Chapter (5.4): a cook, a watch. Paid each game day from its treasury.
struct Staff
{
    std::string npc, site, role;
    std::int64_t wage = 0;
    double paidTo = 0;                            // Calendar day.
    bool arriving = false;                        // Coming to live at the Hold (5.5): not working, nor paid, till home.
    double since = 0;                             // Calendar day they were taken on.
};

// Room at a Hold (5.5): beds in what is built to live in, and work in what is built to work at. Placeholders.
int bedsIn(const std::string& kind);              // tent 1, lean-to 1, hall 4, keep 6, tower 1.
// The work a building gives, one role per post: workshop 2 (smith's hand), stable (groom), well (water-carrier),
// cookfire (cook), storage pile (storekeeper).
std::vector<std::string> postsIn(const std::string& kind);
constexpr std::int64_t HoldWage = 5;              // Pennies a game day for work at a Hold (placeholder).
constexpr double OfferDays = 7;                   // How long those willing to come wait for an answer.

// A resident willing to come and live and work at a Hold (doc 16's migration preview, offered to the Chapter).
struct Offer
{
    std::string npc, site, role;
    double expires = 0;                           // Calendar day.
};

struct Outcome
{
    bool ok = false;
    std::string message;
};

class Camps
{
  public:
    const std::map<std::string, Site>& sites() const { return sites_; }
    const std::map<std::string, Structure>& structures() const { return structures_; }
    const Site* site(const std::string& id) const;
    Site* site(const std::string& id);
    Structure* structure(const std::string& id);
    std::vector<const Site*> sitesOf(const std::string& chapter) const;
    // The Chapter's standing site whose ground (SiteRadius) holds this point, if any.
    const Site* siteAt(const std::string& cell, double x, double y, const std::string& chapter = {}) const;
    std::vector<const Structure*> structuresOf(const std::string& site) const;
    std::vector<const Structure*> structuresIn(const std::string& cell) const;
    const Structure* structureAt(const std::string& cell, int x, int y) const;

    Outcome found(const std::string& chapter, const std::string& cell, int x, int y, const std::string& name, double day, double now);
    Outcome plan(const std::string& site, const std::string& kind, int x, int y);
    Outcome unplan(const std::string& structure);
    // Work on a structure: building it, or mending it. Returns true when this finished it.
    bool work(const std::string& structure, double hours);
    // Built structures of a site (not counting ruins), and whether the Chapter's ground meets the level gates.
    int built(const std::string& site, bool fortificationsOnly = false) const;
    bool hasBuilt(const std::string& site, const std::string& kind) const;
    bool campStanding(const std::string& chapter) const;   // A site with three things built.
    bool fortified(const std::string& chapter) const;      // A palisade ring (8+ sections), a gate and a hall.
    bool hold(const std::string& chapter) const;            // A keep and stone walls (Phase 9's own gate).
    // Wear: a little each game day, four times as fast when no member has been there for AbandonedSeconds; a site
    // whose structures have all crumbled is a ruin.
    std::vector<std::string> wear(double days, double now);
    void abandon(const std::string& site);
    std::map<std::string, Staff>& staff() { return staff_; }
    const std::map<std::string, Staff>& staff() const { return staff_; }
    // A site's beds and posts in what is built and standing, and the posts not yet taken by its staff.
    int beds(const std::string& site) const;
    std::vector<std::string> freePosts(const std::string& site) const;
    std::vector<Offer>& offers() { return offers_; }
    const std::vector<Offer>& offers() const { return offers_; }
    std::map<std::string, double>& offered() { return offered_; }   // Site → the calendar day it was last offered folk.

    json::Value save() const;
    void load(const json::Value& saved);

  private:
    std::map<std::string, Site> sites_;
    std::map<std::string, Structure> structures_;
    std::map<std::string, Staff> staff_;          // By resident.
    std::vector<Offer> offers_;
    std::map<std::string, double> offered_;
    std::uint64_t next_ = 1;
};
} // namespace ratw::camp
