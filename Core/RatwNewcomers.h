#pragma once
// Newcomers (Docs/Design/52-newcomers.md): where a new character arrives, and whether an account is still new. Pure:
// the start towns and thresholds from Data/Social/newcomers.json, the rolling count of active wolves in each start
// town, and the choice between them. The game's side is in RatwGameNewcomers.cpp.
#include "RatwJsonDoc.h"

#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <vector>

namespace ratw::newcomers
{
struct StartTown
{
    std::string id, name, line;
    std::string cell;                      // An arrival cell and tile, when neither the spawn nor the market will do.
    double x = 0, y = 0;
};

struct Rules
{
    std::vector<StartTown> starts;         // In order: the first wins a tie.
    double hours = 15;                     // An account is new until this many hours played,
    int socialLevel = 3;                   // or this social level, whichever comes first.
    double sampleEvery = 60;               // Active wolves counted in each start town this often (seconds),
    int window = 30;                       // and their mean taken over this many counts.
    double activeWithin = 300;             // At the keys within this long (seconds) to count.
    int mentorLevel = 5;                   // Who may mentor: this social level (doc 48, decision 18),
    int mentorReportDays = 30;             // and no upheld report within this many days.
    // A newcomer's first evenings at an inn (doc 52, 6): the hours, evenings tried, wolves pointed out, a regular.
    double eveningFrom = 17, eveningTo = 23, regular = 30;
    int eveningTries = 3, eveningLines = 4;
    // Vouching (doc 52, 7).
    struct Vouching
    {
        double trust = 30, liking = 20, distrust = -20, trustShare = 0.3, trustMost = 15, likingShare = 0.2, likingMost = 10;
        double familiarity = 10, days = 30, householdShare = 0.5, hitTrust = 2, hitLiking = 5;
        int perVoucher = 5, householdMost = 8;
    } vouching;
};

// What a vouch carries to the resident: a share of its trust and liking for the voucher (each to a most), and
// familiarity.
struct VouchShare
{
    double trust = 0, liking = 0, familiarity = 0;
};
VouchShare vouchShare(double trustInVoucher, double likingVoucher, const Rules& r);
// Whether a wolf may vouch for another to a resident; if not, why, in words.
bool mayVouch(double trustInVoucher, double likingVoucher, double trustInVouched, int activeVouches, bool already,
              std::string& why, const Rules& r);

// A vouch: the resident, who vouched, for whom; when (real seconds, and the calendar day) and until (calendar day);
// what it gave the resident and each of its household (taken back if it goes bad); active, ended or broken; whether
// the resident has brought up a broken one with the voucher yet.
struct VouchGift
{
    std::string id;
    double trust = 0, liking = 0, familiarity = 0;
};
struct Vouch
{
    std::string id, resident, voucher, vouched, state = "active";
    double at = 0, day = 0, until = 0;
    VouchShare given;
    std::vector<VouchGift> household;
    bool raised = false;
};
json::Value saveVouch(const Vouch& v);
Vouch loadVouch(const json::Value& o);

// What a mentor's account is now (doc 52, 3), as the eligibility check needs it.
struct MentorCheck
{
    int socialLevel = 0;
    int upheldReports = 0;                 // Upheld within the rules' report days.
    bool newcomer = false, silenced = false, revoked = false;
};

const Rules& rules();
// Reads rules from a document like newcomers.json (for tests); fields left out keep their defaults.
Rules parse(const std::string& text);

// Each start town's recent counts of active wolves, and their mean. One busy minute moves the mean a little; a crowd
// that stays moves it a lot.
class Counts
{
public:
    void add(const std::map<std::string, int>& counts, const std::vector<std::string>& towns, int window);
    double mean(const std::string& town) const;

private:
    std::map<std::string, std::deque<int>> samples_;
};

// The busiest of the towns given (in the data file's order) by their means; a tie goes to the earlier one. "" if none.
std::string busiest(const std::vector<std::string>& towns, const Counts& counts);

// Whether an account may mentor; if not, why, in words for the player.
bool mayMentor(const MentorCheck& m, std::string& why, const Rules& r = rules());

// Whether an account with this much played time and this social level has stopped being new.
bool graduates(double playedSeconds, int socialLevel, const Rules& r = rules());

// ------------------------------------------------------------------ Ties (doc 52, 4)

// A story starter (Data/Social/ties.json): the newcomer's line and the other side's; whether a mentor may take it; what
// a resident must be to fit it; what it does with a resident.
struct Starter
{
    std::string id, newcomer, other;
    bool mentor = true, names = false, fallback = false;
    std::vector<std::string> jobs;                  // Job categories (scenes::jobCategory); empty: any.
    int minAge = 18, maxAge = 99;
    std::string need;                               // "apprentice": holds a post with no apprentice.
    double affinity = 0, trust = 0, familiarity = 0, respect = 0;
    std::int64_t owed = 0;                          // Pennies the newcomer owes the resident (a record only).
};

struct TieRules
{
    std::vector<Starter> starters;
    double offerSeconds = 180, lapseDays = 7, restSeconds = 86400, markerSeconds = 1200;
    int lapseScenes = 3, nearCells = 2;
};

const TieRules& tieRules();
TieRules parseTies(const std::string& text);
const Starter* starter(const std::string& id, const TieRules& r = tieRules());
const Starter* fallbackStarter(const TieRules& r = tieRules());

// A tie: seeking a mentor (or offered to one), active, lapsed or ended. The newcomer's character and account; the
// other (a mentor's character, or a resident); the start town; whom it was offered to and until when; the mentors'
// accounts asked; where the other was when it was made (the marker, for a while); the newcomer told yet.
struct Tie
{
    std::string id, newcomer, account, other, otherAccount, starter, state = "seeking", town, arrivalCell;
    std::string offeredTo;
    double offerUntil = 0;
    std::vector<std::string> asked;
    bool mentorsDone = false, resident = false, told = false, otherTold = false;
    double created = 0, made = 0, lapsesAt = 0, ended = 0;
    int scenesAtStart = 0;
    std::string spotCell, spotPlace;
    double spotX = 0, spotY = 0, markerUntil = 0;
};
json::Value saveTie(const Tie& t);
Tie loadTie(const json::Value& o);

// A mentor who could be asked: the one longest without a tie first (never tied before: first of all), ties broken by
// the seed, so the order is fair but not always the same.
struct MentorCandidate
{
    std::string account, character;
    double lastTieAt = -1;
};
std::vector<MentorCandidate> mentorOrder(std::vector<MentorCandidate> candidates, std::uint32_t seed);

// Whether a resident of this job category and age fits a starter (and has an apprentice place free, if it needs one).
bool residentFits(const Starter& s, const std::string& job, int age, bool apprenticePlaceFree);

// Whether an active tie has run its course: so many days since it was made, or so many scenes shared since.
bool lapsed(const Tie& t, int scenesShared, double now, const TieRules& r = tieRules());

// ------------------------------------------------------------------ Residents as matchmakers (doc 52, 5)

// Something a player may ask for (matched on the speech router's normalised words), and what meets it: a resident
// master with an apprentice place, a resident of certain trades, or a player looking for a scene.
struct Ask
{
    std::string id, resident, player, meets;
    std::vector<std::string> patterns, jobs;
};

struct MatchRules
{
    std::vector<std::string> matchmakers{"innkeeper", "priest"}, order{"newcomer", "tie", "looking", "need", "roots"};
    bool marketMerchant = true;
    double knowsFamiliarity = 10, sharedRootsDays = 7;
    int wellKnownScenes = 2, perPlayerHours = 1, perMatchmakerHour = 6, pointedPerHour = 3;
    std::map<std::string, std::string> reasons;
    std::vector<Ask> asks;
    std::string briefing, line, quiet;
    // The innkeeper's welcome on a newcomer's first evening (doc 52, 6), and the Introduce prompts.
    std::string welcome, pointOut, promptPointed, promptNewcomer;
    std::map<std::string, std::string> facts;
};

const MatchRules& matchRules();
MatchRules parseMatch(const std::string& text);
// The first ask whose words appear in what was said (already normalised), or none.
const Ask* askIn(const std::string& normalised, const MatchRules& r = matchRules());

// The one talking to the matchmaker (A), and each wolf it might be pointed at, as the game found them.
struct MatchSide
{
    bool newcomer = false, helper = false, looking = false;
};
struct MatchCandidate
{
    std::string id;
    bool player = true;
    MatchSide side;
    bool tiedUnmet = false, meetsAsk = false, sameRoots = false;
    bool excluded = false;   // Out of character, opted out, blocked, a party mate, known well, pointed at enough, unknown.
};
// Who A is pointed at and why ("newcomerToHelper", "helperToNewcomer", "tie", "looking", "need", "roots"): the first
// reason in the rules' order that someone fits, one of those at random by the seed; an empty id for none.
struct Pairing
{
    std::string id, reason;
};
Pairing pickPairing(const MatchSide& a, const std::vector<MatchCandidate>& candidates, std::uint32_t seed,
                    const MatchRules& r = matchRules());
// "{who}" and the like filled in.
std::string fill(std::string text, const std::map<std::string, std::string>& blanks);
} // namespace ratw::newcomers
