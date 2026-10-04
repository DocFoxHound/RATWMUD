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
    int actionWords = 0;        // Of /action and /pose: counted at half weight (doc 32, 1.1). /me sets a state: none.
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
    // The conversations the next consolidate(now) will close, as they stand (to be summarised elsewhere).
    std::vector<ActiveMemory> due(double now) const;
    // A better summary for a closed conversation (from the language model): replaces its text, keeps its sources.
    // False if there is no such summary or the text is empty.
    bool rewrite(const std::string& id, const std::string& text);
    // A short recollection: the subject's latest words in this conversation, or the start of the latest summary.
    std::string recall(const std::string& npc, const std::string& subject) const;
    // What an NPC brings to a reply, within `budget` bytes: this conversation's recent turns from both sides (its
    // own as "You"), then what came earlier in it, then the ends of up to three earlier conversations. The current
    // conversation comes first; the newest of everything is kept when something must be cut.
    std::string recallForDialogue(const std::string& npc, const std::string& subject, std::size_t budget = 3600) const;
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
    std::string party = {};     // Said with a party mate listening: the party's own scene (doc 32, 1.1).
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
    bool left = false;          // Stepped out of the scene (settled then if qualified); its words count no more here.
};
struct SocialSession
{
    std::string id, cell;
    double started = 0, last = 0, ended = 0;
    std::map<std::string, Contribution> members;
    std::string party = {};       // A party's own scene, apart from the cell's (doc 32, 1.1).
};
// Gold Stars and Story Stars (doc 32, 1.2): binary thanks from one qualified participant to another.
struct SocialStar
{
    std::string giver, recipient, source, kind;   // kind: "gold" (a scene) or "story".
    double at = 0;
    int amount = 0;
};
// A Story: a chain of qualified scenes sharing participants, approved by two thirds of them; closing it pays for the
// continuity. `chapter` is set when its members are mostly one Chapter's (a Chapter Story, doc 32, Part 3).
struct SocialStory
{
    std::string id, name, owner, state = "pending";   // pending, active, closed, expired.
    double created = 0, last = 0;
    std::vector<std::string> scenes;
    std::set<std::string> members, approvals, starred;
    std::string chapter;
};
struct SocialResult
{
    bool ok = false;
    std::string message;
    int amount = 0;
};

class SocialLedger
{
  public:
    std::vector<SocialStar> stars;
    std::map<std::string, SocialStory> stories;
    std::uint64_t nextStory = 1;
    // What `actor` was paid for a scene (0 if nothing).
    int paidFor(const std::string& actor, const std::string& session) const;
    // Everyone paid more than nothing for a scene.
    std::vector<std::string> paidIn(const std::string& session) const;
    int usedToday(const std::string& actor, double now) const;
    SocialResult star(const std::string& giver, const std::string& recipient, const std::string& session, double now);
    SocialResult propose(const std::string& owner, const std::string& session, const std::string& name, double now);
    SocialResult approve(const std::string& member, const std::string& story, double now);
    SocialResult extend(const std::string& owner, const std::string& story, const std::string& session, double now);
    SocialResult close(const std::string& owner, const std::string& story, double now);
    SocialResult storyStar(const std::string& giver, const std::string& recipient, const std::string& story, double now);
    const SocialStory* storyOf(const std::string& session) const;

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
    // A fight is a scene of its own (doc 33): its scene's id, and the tag its fighters' words carry (as a party's do).
    static std::string fightScene(const std::string& fight) { return "fight-" + fight; }
    static std::string fightTag(const std::string& fight) { return "fight:" + fight; }
    static bool isFight(const SocialSession& s) { return s.party.rfind("fight:", 0) == 0; }
    // A player in a fight: a member of its scene from the start, whether they talk or not.
    void joinFight(const std::string& fight, const std::string& cell, const std::string& member, double now);
    // The fight is over: those in `fought` (who took their turns) are paid for the fight, and those who talked it
    // through (the usual shape, with another who did) twice a scene's pay; the usual decay and daily caps after.
    int settleFight(const std::string& fight, const std::set<std::string>& fought, double now);
    static constexpr int FightXP = 10, FightTalkFactor = 2;
    int endFor(const std::string& actor, double now);
    // One member steps out of an open scene (not a fight's): paid at once if they have the shape and another member
    // has it too; the others carry on. False if they are in no such scene.
    bool leave(const std::string& actor, const std::string& session, double now, int* paid = nullptr);
    void tick(double now);
    // The contribution a member needs to be paid (doc 08), and how long a scene may lie quiet (pacing v2).
    static constexpr int ShapeTurns = 2, ShapeWords = 35, ShapeReplies = 1;
    static constexpr double QuietSeconds = 900, EndSeconds = 1800, FightEndSeconds = 10800;
    static bool shaped(const Contribution& c)
    {
        return c.turns >= ShapeTurns && c.words >= ShapeWords && c.replies >= ShapeReplies;
    }
    int level(const std::string& actor) const;

  private:
    // A qualified member's pay for a scene: by their place among those qualified (joined first, first), less for
    // partners repeated today, within the daily limits; written as the settlement receipt.
    int payMember(const SocialSession& scene, const std::vector<std::string>& qualified, std::size_t index, double now);
    int pay(const std::string& actor, const std::string& partner, const std::string& reason, const std::string& source,
            int requested, double now, std::uint64_t event);
    double pairDecay(const std::string& a, const std::string& b, double now) const;
};
// A title for a social level (doc 32, 1.3).
std::string socialTitle(int level);

} // namespace ratw
