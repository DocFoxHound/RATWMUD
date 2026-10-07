#pragma once
// The star book (Docs/Design/51-scenes-and-stars.md, §1): every Gold Star and Story Star (and doc 58's milestone and
// tale stars, later) recorded against the receiving **account**, never a character, with the rules that decide whether
// one counts toward the total (so two friends can't farm each other), the account's tally, and what a viewer sees of
// it: the exact count for its own player and a friend who sees which wolf is theirs, bands for everyone else. Pure:
// the game records stars (RatwGameSocial.cpp) and saves the book; SocialLedger still decides who may star whom and the
// social XP a star pays. The numbers are in Data/Social/social.json.
#include "RatwJsonDoc.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace ratw::stars
{
struct Rules
{
    std::vector<int> bands{10, 25, 50, 100, 250, 500, 1000}, giverBands{5, 10, 30, 60, 100, 250};
    int giverADay = 10, pairADay = 3, pairIn30Days = 10, keptDays = 30, rateAfter = 20, knownForAt = 50;
    double tagWindow = 600;
    bool showRate = true;
    std::vector<std::pair<double, std::string>> rateWords;
    std::vector<std::string> tags;                 // "storyteller", "packmate", "goodfun", "welcoming".
    std::map<std::string, std::string> tagNames;   // "goodfun" -> "Good fun".
    std::vector<std::string> newcomersOnly;        // Tags only a newcomer may give ("welcoming": doc 52).
    json::Value catalog = json::Value::object();   // The file, as the client is sent it.
};
const Rules& rules();

// One star: who gave it to whom (account and character), what for, its tag, when, the social XP it paid, and whether
// it counts toward the recipient's total.
struct Star
{
    std::string id, kind, source, giverAccount, giverCharacter, recipientAccount, recipientCharacter, tag;
    double at = 0;
    int xp = 0;
    bool counted = false;
};

// An account's tally: counted stars, by kind and tag; the distinct accounts that gave them; and, for the rate, the
// chances it had to be starred and the Gold Stars it received.
struct Tally
{
    int total = 0, chances = 0, goldReceived = 0;
    std::map<std::string, int> kinds, tags;
    std::set<std::string> givers;
};

class Book
{
  public:
    // Records a star, deciding whether it counts (within the giver's day, the pair's day and the pair's 30 days), and
    // returns it as kept. A star between one account's own wolves is the caller's to refuse.
    Star record(Star star);
    const Tally* tally(const std::string& account) const;
    const std::vector<Star>& recent() const { return recent_; }   // The last 30 days', oldest first.
    // Tags a star (§2): only its giver, once, within the tag window, with one of the tags (a newcomer-only tag only
    // when `newcomer`). Counts toward the tally's tags when the star counted. False, with the reason.
    bool tag(const std::string& starId, const std::string& giverAccount, const std::string& tag, bool newcomer, double now,
             std::string& error);
    // The giver's stars still open to a tag (given in the last tag window, untagged), newest first.
    std::vector<const Star*> openToTag(const std::string& giverCharacter, double now) const;
    // A chance to have been starred (§3): at each settlement, one for each other who qualified with them.
    void chances(const std::string& account, int n);
    // Drops recent stars past 30 days (tallies keep their counts).
    void prune(double now);
    json::Value save() const;
    void load(const json::Value& saved);
    // What a viewer sees of an account's stars: exact (its own player, or a friend who sees which wolf is theirs) or in
    // bands, with the distinct givers the same way.
    json::Value view(const std::string& account, bool exact) const;

  private:
    std::map<std::string, Tally> tallies_;
    std::vector<Star> recent_;
};

// A count in bands: "a few", "10+", "25+"... (`givers`: the giver bands, under the first reads "a few" as well).
std::string band(int n, bool givers = false);
} // namespace ratw::stars
