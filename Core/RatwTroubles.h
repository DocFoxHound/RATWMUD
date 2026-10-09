#pragma once
// Residents' troubles (Docs/Design/57-changing-the-world.md, 3): what weighs on a resident, read only from the
// simulation's real state (the society's purses, loans, careers and households, and the bonds), never invented. Pure:
// `troubleOf` reads, and changes nothing. The kinds, their order, thresholds and lines are Data/Town/troubles.json.
#include "RatwJsonDoc.h"

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace ratw
{
class Society;
class Bonds;
}

namespace ratw::troubles
{
struct Kind
{
    std::string id;
    bool on = true;
    std::string briefing, unfinished;
    std::vector<std::string> said;
    double underDays = 7, refillDays = 14;          // short
    int youngest = 0, oldest = 200;                 // child (the youth's age), work (the wolf's)
    std::int64_t fee = 20;                          // child: an apprenticeship's sponsorship
    double notableDays = 7;                         // work: idle this long, and its solving is notable
    double both = -25, one = -40;                   // feud: affinity both ways, or one way
};
struct Rules
{
    std::vector<Kind> kinds;                        // In order: the first that holds is the resident's trouble.
    double trust = 10, familiarity = 20;            // A trouble is spoken of to a wolf it trusts this much.
    double restDays = 14;                           // A solved trouble of a kind doesn't come back to that resident sooner.
    double perWolfDays = 92;                        // One of a resident's troubles solved by the same wolf a season.
    double peaceTrust = 30, peaceTiles = 4;         // Making peace: trusted by both, the three of them this close.
    double employerTrust = 30, masterTrust = 20;    // Speaking for a resident; sponsoring an apprenticeship.
    std::int64_t notableDebt = 100;                 // A debt paid off this large is a notable deed.
    std::vector<std::string> refusal;
    const Kind* kind(const std::string& id) const;
};
const Rules& rules();                               // Data/Town/troubles.json, read once.
Rules parse(const std::string& text);

struct Trouble
{
    std::string kind;                               // "" for none.
    std::string resident;                           // Whose it is.
    std::string other;                              // feud: the other; child: the youth; debt: the till.
    std::int64_t coins = 0;                         // debt: what is owed; short: what would bring it to refillDays.
    double days = 0;                                // short: days of food held; work: days without a post.
    int age = 0;                                    // child: the youth's age.
    std::vector<std::string> household;             // short: who lives there (the head first).
    explicit operator bool() const { return !kind.empty(); }
};

// What `troubleOf` reads besides the society and the bonds (the world knows ages, deaths and places).
struct Reads
{
    const Society* society = nullptr;
    const Bonds* bonds = nullptr;
    std::function<int(const std::string& id)> age;
    std::function<bool(const std::string& id)> alive;
    std::function<std::string(const std::string& cell)> communityOf;
    // Days a grown wolf has gone without a post (the game keeps when it was first seen without one); null: 0.
    std::function<double(const std::string& id)> idleDays;
    // Whether a kind of trouble is resting for a resident (solved lately); null: never.
    std::function<bool(const std::string& id, const std::string& kind)> resting;
    double day = 0;
};
Trouble troubleOf(const std::string& resident, const Reads& reads, const Rules& rules = troubles::rules());
// Each kind alone (troubleOf takes the first that holds), for tests and for checking a solution worked.
Trouble kindOf(const std::string& kind, const std::string& resident, const Reads& reads, const Rules& rules = troubles::rules());

// The household's money and food at the town's price, in days of a day's plain food for everyone in it.
double foodDays(const std::vector<std::string>& household, const std::string& homeCell, const Reads& reads, std::int64_t* held = nullptr,
                double* dayCost = nullptr);

// A line's blanks ({coins}, {other}...) filled.
std::string fill(std::string line, const std::map<std::string, std::string>& blanks);
} // namespace ratw::troubles
