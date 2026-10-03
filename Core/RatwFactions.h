#pragma once
// Factions in play (Docs/Design/32-parties-chapters-factions.md, Part 4): who belongs to which faction, how factions
// regard each other, and how each regards a Chapter. Only Chapters have standing with a faction; a player has none of
// their own. A Chapter's standing is what it earned, less the burden of its members' crimes against the faction as the
// faction knows them (4.2a). A member who is sent away takes their burden with them, once the faction hears of it.
// Also the faction's mission boards (4.5). Pure rules: the game (RatwGameFactions.cpp) says what happened.
#include "RatwJsonDoc.h"

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace ratw::faction
{
struct Faction
{
    std::string id, name, colour, kind;
};

struct Relation
{
    int disposition = 0;
    std::string stance = "neutral";               // allied, friendly, neutral, tense, hostile, war.
};

struct StandingChange
{
    std::string reason;
    double delta = 0, at = 0;                    // Calendar days.
};

struct Standing
{
    double earned = 0;                           // -100..100.
    std::string stance;                          // Set by a DM ("" follows the disposition).
    std::vector<StandingChange> log;             // The newest kept.
    std::map<std::string, double> weekly;        // Capped sources this game week: reason → amount.
    double week = -1;
};

struct Burden                                    // A faction's grievance against one wolf (4.2a).
{
    double amount = 0;
    std::vector<std::string> incidents;
    double last = 0;                             // Calendar day.
    bool restitution = false;                    // Paid what the Watch asked: fades faster.
};

struct Expulsion                                 // Sent from a Chapter; the faction may not have heard yet.
{
    std::string chapter, who;
    double at = 0;                               // Calendar day.
    std::set<std::string> heardBy;               // Factions that know.
};

struct Mission
{
    std::string id, faction, kind;               // kind: "deliver", "message", "guard".
    int tier = 1;
    std::string item;                            // deliver: what, and how many.
    int quantity = 0;
    std::string official, to, cell, place;       // Who gives it; message: to whom; guard: where.
    double seconds = 0, held = 0;                // guard: how long, and how long so far.
    std::int64_t coins = 0;
    double standing = 0;
    int renown = 0;
    std::string chapter, taker, state = "open";  // open, taken, done, expired.
    double expires = 0;                          // Calendar day.
};

// A treaty (4.6): typed terms between a Chapter and a faction. Proposed by the Chapter's Head to an official; decided by
// a Dungeon Master, or after a game day without one, by the faction's own rule (it agrees with a Chapter it trusts).
struct Treaty
{
    std::string id, faction, chapter;
    bool build = false;                          // May build (and fortify) on the faction's land.
    std::int64_t tithe = 0;                      // Pennies a game week, from the Chapter's treasury.
    bool levy = false;                           // Answers the faction's levies.
    int weeks = 8;
    std::string state = "pending";               // pending, active, ended, rejected.
    double proposed = 0, started = 0, paidTo = 0;// Calendar days.
};

// A levy (4.4): the faction calls on a Chapter sworn to it, or bound by treaty, to keep watch somewhere this week.
struct Levy
{
    std::string id, faction, chapter, cell, place;
    double needed = 1200, done = 0;              // Members' seconds there.
    double due = 0;                              // Calendar day.
    std::string state = "called";                // called, answered, ignored.
};

// A Chapter asking to be recognised as a minor House by a faction (Hold level, Sworn): decided like a treaty.
struct HouseRequest
{
    std::string chapter, faction, state = "pending";
    double day = 0;
};

// A band's name for a disposition (4.2): Sworn, Trusted, Known well, Neutral, Distrusted, Hostile, Enemy.
const char* band(double disposition);
std::string defaultStance(double disposition);

class Factions
{
  public:
    void define(const Faction& f);
    void clear();
    const std::map<std::string, Faction>& all() const { return factions_; }
    const Faction* find(const std::string& id) const;
    // Members: an NPC's faction and rank. Explicit ones win over those derived from where they work.
    void setMember(const std::string& npc, const std::string& faction, const std::string& rank, bool explicitly);
    void clearDerived();
    const std::pair<std::string, std::string>* memberOf(const std::string& npc) const;   // (faction, rank)
    void setRelation(const std::string& a, const std::string& b, const Relation& r);
    Relation relation(const std::string& a, const std::string& b) const;

    // A Chapter's standing with a faction: earned, less its members' burdens as the faction knows them.
    double earned(const std::string& faction, const std::string& chapter) const;
    double effective(const std::string& faction, const std::string& chapter, const std::vector<std::string>& members) const;
    std::string stanceOf(const std::string& faction, const std::string& chapter, const std::vector<std::string>& members) const;
    // Earned standing moves by `delta` (capped per source per game week where `weeklyCap` > 0), and ripples to the
    // faction's allies (+¼) and enemies (−¼). Returns what moved.
    double change(const std::string& faction, const std::string& chapter, double delta, const std::string& reason, double day,
                  double weeklyCap = 0, bool ripple = true);
    void setStance(const std::string& faction, const std::string& chapter, const std::string& stance);
    const Standing* standing(const std::string& faction, const std::string& chapter) const;
    // Each game week, earned standing drifts one toward Neutral; burdens fade (twice as fast once restitution is paid).
    void drift(double day);

    void addBurden(const std::string& faction, const std::string& who, double amount, const std::string& incident, double day);
    void restitution(const std::string& who);
    double burden(const std::string& faction, const std::string& who) const;
    const std::map<std::string, Burden>* burdensOf(const std::string& faction) const;
    // A member sent away: the faction counts them against the Chapter until it hears (told, or by word in a day and a
    // half: ExpulsionNewsDays).
    void expelled(const std::string& chapter, const std::string& who, double day);
    void hear(const std::string& faction, const std::string& chapter, double day);   // Told of the Chapter's expulsions.
    void spread(double day);                                                          // News travels.
    std::vector<std::string> stillCounted(const std::string& faction, const std::string& chapter) const;

    // Treaties, levies and Houses (4.4, 4.6; Phase 9).
    std::vector<Treaty>& treaties() { return treaties_; }
    const std::vector<Treaty>& treaties() const { return treaties_; }
    Treaty* treaty(const std::string& id);
    const Treaty* activeTreaty(const std::string& faction, const std::string& chapter) const;
    std::string nextTreatyId() { return "treaty-" + std::to_string(nextMission_++); }
    std::vector<Levy>& levies() { return levies_; }
    const std::vector<Levy>& levies() const { return levies_; }
    std::vector<HouseRequest>& houseRequests() { return houses_; }

    // Missions (4.5).
    std::vector<Mission>& missions() { return missions_; }
    const std::vector<Mission>& missions() const { return missions_; }
    Mission* mission(const std::string& id);
    std::string nextMissionId() { return "mission-" + std::to_string(nextMission_++); }

    json::Value save() const;
    void load(const json::Value& saved);

    static constexpr double ExpulsionNewsDays = 1.5, WeekDays = 7;

  private:
    std::map<std::string, Faction> factions_;
    std::map<std::string, std::pair<std::string, std::string>> members_;   // npc → (faction, rank)
    std::set<std::string> explicit_;
    std::map<std::string, Relation> relations_;                             // "a|b"
    std::map<std::string, Standing> standings_;                             // "faction|chapter"
    std::map<std::string, std::map<std::string, Burden>> burdens_;          // faction → wolf → burden
    std::vector<Expulsion> expulsions_;
    std::vector<Mission> missions_;
    std::vector<Treaty> treaties_;
    std::vector<Levy> levies_;
    std::vector<HouseRequest> houses_;
    std::uint64_t nextMission_ = 1;
    double lastDrift_ = -1;
};
} // namespace ratw::faction
