#pragma once
// A Chapter's ground (Docs/Design/32-parties-chapters-factions.md, Part 5). Phase 7: rented places. A rentable place is
// a whole interior (a room above an inn, a warehouse, a hall) with a landlord (a resident, or the town's treasury), the
// faction that claims it, and a weekly rent. A lease is paid a week ahead from the Chapter's treasury; unpaid, a week's
// grace, then eviction. Inside: locked to all but members and guests, a notice board. Camps, halls and holds (Phases
// 8–9) are the structures layer. Pure rules: the game (RatwGameEstates.cpp) moves the money and keeps the doors.
#include "RatwJsonDoc.h"

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace ratw::estate
{
constexpr double WeekDays = 7, GraceDays = 7, HeldForGate = 14;

struct Property
{
    std::string id;                               // The cell.
    std::string name, kind;                       // kind: "hall" (rooms, small halls) or "warehouse".
    std::string landlord;                         // A resident's ID, or "treasury" (the town's).
    std::string faction;                          // Who claims it ("" for none).
    std::int64_t rent = 0;                        // Pennies a game week.
    int level = 2;                                // The Chapter level it needs (doc 32, 3.4).
};

struct Notice
{
    std::string by, text;
    double at = 0;                                // Calendar day.
};

struct Lease
{
    std::string property, chapter, landlord;
    std::int64_t rent = 0;
    double started = 0, paidTo = 0;               // Calendar days.
    std::string state = "active";                 // active, grace.
    std::set<std::string> guests;
    std::vector<Notice> notices;
};

struct Outcome
{
    bool ok = false;
    std::string message;
};

class Estates
{
  public:
    void define(const Property& p);
    void clearDerived();
    const Property* property(const std::string& cell) const;
    const std::map<std::string, Property>& properties() const { return properties_; }
    const Lease* lease(const std::string& cell) const;
    Lease* lease(const std::string& cell);
    std::vector<const Lease*> leasesOf(const std::string& chapter) const;
    // A new lease, the first week paid by the game before calling this.
    Outcome open(const std::string& cell, const std::string& chapter, double day);
    Outcome close(const std::string& cell);
    // Rent due (paidTo passed) for each lease: the game pays it (true) or not; unpaid past the grace, the lease ends.
    struct Due
    {
        std::string property, chapter, landlord;
        std::int64_t rent = 0;
    };
    std::vector<Due> due(double day) const;
    void paid(const std::string& cell, double day);
    // Unpaid: into grace, or (past it) ended. True when it ended.
    bool unpaid(const std::string& cell, double day);
    // Whether the Chapter has held any lease, paid up, for HeldForGate days.
    bool heldLongEnough(const std::string& chapter, double day) const;

    json::Value save() const;
    void load(const json::Value& saved);

  private:
    std::map<std::string, Property> properties_;
    std::set<std::string> authored_;              // Defined by a DM or a tool, not derived from the world.
    std::map<std::string, Lease> leases_;

  public:
    void markAuthored(const std::string& cell) { authored_.insert(cell); }
    // No longer to let (a DM's word): gone at once if unrented, else once its lease ends.
    void unmark(const std::string& cell)
    {
        authored_.erase(cell);
        if (!leases_.count(cell))
            properties_.erase(cell);
    }
};
} // namespace ratw::estate
