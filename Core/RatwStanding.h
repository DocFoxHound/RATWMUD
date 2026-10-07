#pragma once
// Earned Gift tiers (Docs/Design/49-characters-and-earned-gifts.md, Phase 5): an account may make Gifted wolves once it
// has roleplayed as a Normal wolf a while, and Quickened ones once it has shown it roleplays well. The measures come
// from the social ledger; the thresholds from Data/Progression/standing.json; what was earned is kept, whatever the
// thresholds do later. This header is the pure part (no World, no Game): the game counts and keeps the records.
#include <string>
#include <vector>

namespace ratw::standing
{
// What an account has done, counted from the ledger (RatwGameSocial.cpp: Game::measuresOf).
struct Measures
{
    int socialLevel = 1;
    int normalScenes = 0;       // Scenes paid for on its Normal wolves.
    int stars = 0;              // Gold and Story Stars its wolves received from other accounts' wolves.
    int starGivers = 0;         // How many different accounts gave them.
    int closedStories = 0;      // Stories its wolves saw closed.
    int upheldReports = 0;      // Reports upheld against it lately (doc 50; none until reports exist).
};
// The thresholds (standing.json's "unlocks").
struct Thresholds
{
    int giftedLevel = 3, giftedScenes = 10;
    int quickenedLevel = 8, quickenedStars = 100, quickenedGivers = 30, quickenedStories = 2, reportDays = 30;
    int givers = 64;            // How many different givers are counted at most.
};
const Thresholds& thresholds();
// What is kept for an account: when each tier opened (-1: not yet) and how ("earned", "dm:<name>"), and the Dungeon
// Master's hold, which stops new unlocks (standing in for reports until doc 50).
struct Record
{
    std::string account;
    double giftedAt = -1, quickenedAt = -1;
    std::string giftedBy, quickenedBy;
    bool hold = false;
};
bool meetsGifted(const Measures& m, const Thresholds& t);
bool meetsQuickened(const Measures& m, const Thresholds& t);   // (Gifted must be open too: the caller checks.)
bool open(const Record& r, const std::string& tier);            // "normal" always; "gifted", "quickened" once opened.
// One line of progress toward a tier: what is measured, what the account has, what it needs, and the words for it
// ("Social level 2 of 3").
struct Progress
{
    std::string measure;
    int have = 0, need = 0;
    std::string label;
};
std::vector<Progress> progress(const std::string& tier, const Measures& m, const Thresholds& t);
// What a locked tier still needs, in a sentence: "Quickened isn't open to your account yet: 62 of 100 stars."
std::string lockedMessage(const std::string& tier, const Measures& m, const Thresholds& t);
} // namespace ratw::standing
