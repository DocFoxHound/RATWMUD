#pragma once
// Chapters (Docs/Design/32-parties-chapters-factions.md, Part 3): this game's clans. A lasting, named fellowship of
// wolves, founded together in a scene by three; ranked (Head, Officer, Member, Initiate); with a renown ledger, five
// levels that are never lost, a treasury, a hostile list and a meeting place. Pure rules: the game
// (RatwGameChapters.cpp) says who is where and what was earned, and moves money; the checkpoint keeps it all.
#include "RatwJsonDoc.h"

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace ratw::chapter
{
constexpr int RankHead = 0, RankOfficer = 1, RankMember = 2, RankInitiate = 3;
constexpr std::size_t Founders = 3;
constexpr double WeekSeconds = 7 * 86400.0, ActiveSeconds = 14 * 86400.0, InviteSeconds = 300, HeadAwaySeconds = 30 * 86400.0,
                 RejoinSeconds = 14 * 86400.0;
constexpr int WeeklyRenownCap = 300;
constexpr std::int64_t FoundingFee = 20;          // Two marks, in pennies.

struct Member
{
    std::string id;
    int rank = RankInitiate;
    double joined = 0, active = 0;                // Unix seconds: when they joined, and last took part in a scene.
};

struct RenownEntry
{
    std::string kind, source, actor;              // kind: "member scene", "chapter scene", "outreach", "story", "mission", "award"...
    int amount = 0;
    double at = 0;
};

struct LogEntry
{
    std::string kind, by, other, detail;
    std::int64_t amount = 0;
    double at = 0;
};

struct Hostile
{
    std::string target, kind, reason, by;        // kind: "wolf", "chapter" or "faction".
    double at = 0;
};

struct Chapter
{
    std::string id, name, colour, charter;
    int level = 1;
    int renown = 0, storiesTold = 0;
    double founded = 0;
    std::map<std::string, Member> members;
    std::vector<std::string> rankNames{"Head", "Officer", "Member", "Initiate"};
    std::vector<RenownEntry> renownLog;           // The newest kept (RenownKept).
    std::vector<LogEntry> log;                    // Ranks, joining, leaving, treasury: the newest kept (LogKept).
    std::vector<Hostile> hostiles;
    std::string meetingCell, meetingName;
    double meetingX = 0, meetingY = 0;
    std::set<std::string> scenesCounted;          // Scenes already paid renown (each once).
    std::map<std::string, double> expelled;       // Who was sent away, and when (they can't rejoin for a while).
    // The ground held (Parts 5): kept by the game's later phases; the level gates read them.
    bool hallHeldTwoWeeks = false, campStanding = false, fortified = false, friendlyFaction = false;
};

struct Proposal                                   // A founding waiting for its founders' word.
{
    std::string by, name, colour, charter, scene;
    std::vector<std::string> founders;            // Including `by`.
    std::set<std::string> agreed;
    double expires = 0;
};

struct Outcome
{
    bool ok = false;
    std::string message;
};

// The level a Chapter has earned (never lower than it has: levels are sticky).
struct Gate
{
    int renown = 0, active = 0, stories = 0;
    const char* ground = "";
};
const Gate& gateFor(int level);                    // Level 2..5.
const char* levelName(int level);                  // Gathering, Lodge, Company, Hall, Hold.
// Why a colour can't be a Chapter's ("" when it can): "#rrggbb", and not red (red is for the hostile, doc 32 2.4).
std::string colourProblem(const std::string& hex);

class Chapters
{
  public:
    const Chapter* of(const std::string& who) const;
    const Chapter* byId(const std::string& id) const;
    Chapter* byId(const std::string& id);
    const std::map<std::string, Chapter>& all() const { return chapters_; }
    const Member* member(const std::string& who) const;
    bool together(const std::string& a, const std::string& b) const;

    // Founding: `by` proposes with two others (all in one scene: the game checks); each agrees; the last word founds it.
    Outcome propose(const std::string& by, const std::vector<std::string>& founders, const std::string& name, const std::string& colour,
                    const std::string& charter, const std::string& scene, double now);
    const Proposal* proposalFor(const std::string& who, double now) const;
    // The founding `who` is a founder of agrees; when everyone has, returns the new Chapter's ID in `message`.
    Outcome agree(const std::string& who, double now);
    Outcome withdraw(const std::string& who);
    Outcome found(const Proposal& p, double now);

    Outcome invite(const std::string& by, const std::string& to, double now);
    const std::pair<std::string, double>* inviteFor(const std::string& to, double now) const;   // (chapter, expires)
    Outcome accept(const std::string& to, double now);
    Outcome decline(const std::string& to);
    // Leaving: a Head hands on to the longest-serving Officer (else the longest-serving Member); the last one out ends it.
    Outcome leave(const std::string& who, double now);
    Outcome remove(const std::string& by, const std::string& who, double now);
    Outcome setRank(const std::string& by, const std::string& who, int rank, double now);
    Outcome renameRank(const std::string& by, int rank, const std::string& name);
    Outcome setMeeting(const std::string& by, const std::string& cell, double x, double y, const std::string& place);
    Outcome markHostile(const std::string& by, const std::string& target, const std::string& kind, const std::string& reason, double now);
    Outcome unmarkHostile(const std::string& by, const std::string& target);
    void touch(const std::string& who, double now);   // Took part in a scene: active.

    // Renown, within the rolling weekly cap; returns what was added.
    int addRenown(const std::string& chapterId, const std::string& kind, int amount, const std::string& source,
                  const std::string& actor, double now);
    int activeMembers(const Chapter& c, double now) const;
    // Raises a Chapter to every level whose gates it meets (one at a time); returns the levels newly reached.
    std::vector<int> advance(const std::string& chapterId, double now);
    // Succession for a Head long away, and lapsed proposals and invitations.
    std::vector<std::string> tick(double now);
    void log(const std::string& chapterId, LogEntry entry);

    json::Value save() const;
    void load(const json::Value& saved);

  private:
    std::map<std::string, Chapter> chapters_;
    std::map<std::string, std::string> chapterOf_;
    std::map<std::string, Proposal> proposals_;                           // By proposer.
    std::map<std::string, std::pair<std::string, double>> invites_;       // To → (chapter, expires).
    std::uint64_t next_ = 1;
    static constexpr std::size_t RenownKept = 300, LogKept = 300;
    Outcome needRank(const std::string& by, int atLeast, const Chapter** c) const;
};
} // namespace ratw::chapter
