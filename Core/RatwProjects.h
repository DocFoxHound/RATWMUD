#pragma once
// Town projects (Docs/Design/57-changing-the-world.md, 4): public works a town needs, built from materials, labour and
// coin. Pure rules, as RatwCamps is: the kinds (Data/Town/projects.json), a project's needs and progress, its gifts and
// their worth, the plaque and the naming, and refunds pro rata. The game (RatwGameProjects.cpp) holds the coin and goods
// in the project's own account ("project:<id>"), counts the work, and stands the structure on the Chapters' layer.
#include "RatwJsonDoc.h"

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace ratw::projects
{
struct Kind
{
    std::string id, name, structure, title;         // structure: a camp kind ("" for a mending, which stands as nothing).
    std::vector<std::pair<std::string, int>> materials;
    double hours = 20, wear = 1;                    // Work-hours to build; condition lost a game day once built.
    std::vector<std::string> roles;                 // The two angles workers take (doc 53's cooperation rate).
    bool city = false;                              // Only a city has one.
};
struct Rules
{
    std::vector<Kind> kinds;
    int perTown = 1, perCity = 2;
    double lapseDays = 60, hourShare = .125, nameShare = .4, upkeepUnder = 60, otherShare = .1;
    std::int64_t nameWorth = 300, greatWorth = 1000;
    double handHours = 2;
    int handsMost = 3;
    const Kind* kind(const std::string& id) const;
};
const Rules& rules();                               // Data/Town/projects.json, read once.
Rules parse(const std::string& text);

// A gift, merged by giver, kind and item: coin (amount in pennies), goods (a count of `item`), labour (work-hours).
// `value` is in pennies: coin at face, goods at the town's price, an hour at hourShare of a labourer's day.
struct Gift
{
    std::string who, kind, item;
    double amount = 0, value = 0, day = 0;
};
struct Project
{
    std::string id, kind, town, cell, title;
    std::string state = "open";                     // open, built, worn, ruin, cancelled.
    std::string by;                                 // "town" (its own needs) or the Dungeon Master's action.
    std::string site, structure;                    // On the Chapters' layer (a town's site), once placed.
    std::string namedFor;                           // The giver it is named for ("" none).
    int x = 0, y = 0;
    double hours = 0, worked = 0;                   // Work-hours needed, and done.
    double posted = 0, finished = -1;               // Calendar days.
    double condition = 100;                         // Once built.
    std::map<std::string, int> needs;               // Materials needed (what it holds is its account's stock).
    std::map<std::string, std::string> shown;       // Giver -> the name it chose at its first gift ("" a friend of the town).
    std::vector<Gift> gifts;
    std::string purse() const { return "project:" + id; }
};
struct Giver
{
    std::string who;
    double value = 0;
};

class Ledger
{
  public:
    const std::map<std::string, Project>& all() const { return all_; }
    std::map<std::string, Project>& every() { return all_; }
    Project* find(const std::string& id);
    const Project* find(const std::string& id) const;
    Project& post(const Kind& k, const std::string& town, const std::string& cell, int x, int y, const std::string& title,
                  double day, const std::string& by);
    void erase(const std::string& id) { all_.erase(id); }
    // A gift recorded (merged with the giver's earlier of that kind and item); a refund taken off.
    static void give(Project& p, const std::string& who, const std::string& kind, const std::string& item, double amount, double value,
                     double day);
    static double worth(const Project& p);
    static std::vector<Giver> givers(const Project& p);   // By value, most first.
    // The plaque: up to `most` givers by value, each by the name it chose ("a friend of the town" if none).
    static std::vector<std::pair<std::string, std::string>> plaque(const Project& p, int most = 3);
    // Who it is named for: a giver of nameShare of the whole, the whole worth nameWorth or more; else "".
    static std::string chief(const Project& p, const Rules& r = rules());
    // What each giver of `kind` (and `item`) gets back of `pool`, by their share of what was given: exact, the remainder
    // a penny or a piece at a time to the largest givers first. Nothing made or lost.
    static std::vector<std::pair<std::string, std::int64_t>> shares(const Project& p, const std::string& kind, const std::string& item,
                                                                    std::int64_t pool);
    int openIn(const std::string& town) const;

    json::Value save() const;
    void load(const json::Value& saved);

  private:
    std::map<std::string, Project> all_;
    std::uint64_t next_ = 1;
};

std::string titleFor(const Kind& k, const std::string& townName);
} // namespace ratw::projects
