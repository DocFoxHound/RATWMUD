#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace ratw
{

struct Segment
{
    std::string kind, text;
};
struct ParsedPost
{
    bool ok = false;
    std::string error, posture, state;
    bool hasState = false, speech = false;
    std::vector<Segment> segments;
};
ParsedPost parsePost(const std::string& text);
struct SocialEvidence
{
    int words = 0;
    std::uint64_t contentHash = 0;
};
SocialEvidence roleplayEvidence(const ParsedPost& post);
std::string maskWords(const std::string& text, double clarity, std::uint64_t seed);
std::vector<Segment> perceivePost(const ParsedPost& post, double hearing, double vision, std::uint64_t seed);

struct MemoryTurn
{
    std::uint64_t event = 0;
    double at = 0;
    std::string who, text;
};
struct ActiveMemory
{
    std::string id, npc, subject, olderContext;
    double started = 0, lastActivity = 0;
    std::vector<MemoryTurn> turns;
    std::vector<std::uint64_t> olderEvents;
};
struct MemorySummary
{
    std::string id, npc, subject, text;
    double started = 0, consolidated = 0;
    std::vector<std::uint64_t> sourceEvents;
};
class MemoryStore
{
  public:
    // Unix seconds: restart/offline time contributes to the inactivity interval.
    static constexpr double InactivitySeconds = 3600.0;
    std::map<std::string, ActiveMemory> active;
    std::vector<MemorySummary> summaries;
    std::uint64_t nextConversation = 1;
    void record(const std::string& npc, const std::string& subject, const MemoryTurn& turn);
    int consolidate(double now);
    std::string recall(const std::string& npc, const std::string& subject) const;
};

struct SocialPost
{
    std::uint64_t event = 0;
    double at = 0;
    std::string actor, cell;
    int words = 0;
    bool ooc = false;
    std::uint64_t contentHash = 0;
    std::vector<std::string> audience = {};
};
struct LedgerEntry
{
    std::uint64_t event = 0;
    double at = 0;
    std::string actor, partner, reason;
    int amount = 0;
    std::string session;
};
struct Contribution
{
    int turns = 0, words = 0, replies = 0;
    double last = 0, joined = 0;
    std::vector<std::string> lastAudience;
};
struct SocialSession
{
    std::string id, cell;
    double started = 0, last = 0, ended = 0;
    std::map<std::string, Contribution> members;
};
class SocialLedger
{
  public:
    std::vector<LedgerEntry> entries;
    std::map<std::string, SocialPost> recent;
    std::map<std::string, int> points;
    std::map<std::string, SocialSession> sessions;
    std::map<std::string, std::vector<SocialPost>> candidates;
    std::map<std::string, std::map<std::uint64_t, double>> duplicateHashes;
    std::set<std::uint64_t> acceptedEvents;
    // Eligible listeners must be connected humans that actually perceived this post.
    int record(SocialPost post, const std::vector<std::string>& eligibleListeners);
    int settle(const std::string& session, double now);
    int endFor(const std::string& actor, double now);
    void tick(double now);
    int level(const std::string& actor) const;
};

} // namespace ratw
