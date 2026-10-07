#pragma once
// Reports (Docs/Design/50-player-card-friends-safety.md, 7): a player's report of another wolf to the Dungeon Master,
// with the lines the reporter received from them as evidence (the one exception to doc 08's no-prose rule, agreed
// 2026-10-06). Kept apart from the save, like portraits: in memory (tests, scratch servers), in a folder beside a file
// world, or in game.reports. Kept 30 days unless a DM upholds it; an upheld report keeps its record for good and its
// evidence 180 days.
#include "RatwJsonDoc.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ratw::reports
{
struct Line
{
    std::uint64_t seq = 0;
    double at = 0;
    std::string channel, text;
};

struct Report
{
    std::string id;
    double created = 0;
    std::string reporterAccount, reporterCharacter, reportedAccount, reportedCharacter;
    std::string kind = "speech";        // "speech", "profile", "tell", "circle".
    std::string category = "other";     // "harassment", "hateful", "spam", "cheating", "other".
    std::string note;
    std::vector<Line> evidence;
    std::string status = "open";        // "open", "upheld", "dismissed".
    std::string decidedBy, outcome;     // outcome: "note", "warning", "silence".
    double decidedAt = -1;
    int silenceHours = 0;
};

json::Value toJson(const Report& r);
Report fromJson(const json::Value& o);
bool validCategory(const std::string& category);

// What is kept, and for how long: open and dismissed reports 30 days from their making; upheld ones for good, their
// evidence 180 days from the decision (the placeholders of doc 50, 7).
constexpr double KeepDays = 30, EvidenceDays = 180;

class Store
{
  public:
    virtual ~Store() = default;
    virtual bool add(const Report& r, std::string& error) = 0;
    virtual bool update(const Report& r) = 0;
    virtual std::vector<Report> all() = 0;
    virtual void purge(double now) = 0;
};
std::unique_ptr<Store> memoryStore();
// One JSON file in a folder beside a file world's save.
std::unique_ptr<Store> folderStore(const std::string& directory, std::string& error);
// game.reports (migration 0036).
std::unique_ptr<Store> databaseStore(const std::string& conninfo, const std::string& worldId, std::string& error);
} // namespace ratw::reports
