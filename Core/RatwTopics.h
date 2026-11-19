#pragma once
// The topic director in data (Docs/Design/84 §7.3, §8.5; Phase 5): Data/Talk/topics.json. What two residents talk of was
// chosen by fixed C++ scores (World::ambientTopic, sceneTopics); those numbers live here now, by topic and by kind of
// source (a wagon late, a ship lost, a claim heard), with what cools a topic (the same pair on it lately, the same
// incident talked of all day in one place), the everyday floor (a place whose talk has been news lately turns to
// everyday life), a speaker's favourites, the topic graph's edges (the pivots: prices to the caravan when the road is the
// cause) and each topic's aspects.
//
// The book is the process's, as claims.json's table is (RatwClaims.h): the talk library's loader reads topics.json and
// sets it; a library without one (from before Phase 5: the golden transcripts' a46cbdac) sets today's, the C++ scores as
// they were, so it talks exactly as before. Tests/fixtures/talk_topics/topics-today.json is today's written out, and
// ctest talk_topics proves the two weigh every topic the same, number for number.
#include <map>
#include <string>
#include <vector>

namespace ratw::topics
{
// How one source of a topic is scored. The director's formula for each source is fixed (a claim: base + confidence +
// `add` for one about a traveller; a wagon late: base + the hour's draw...); these are its numbers.
struct Score
{
    double base = 0;
    double add = 0;              // The source's own extra: a traveller (a claim), a grave loss (news), a festival (the
                                 // day), a broad accent (accent).
    double scale = 1;            // A deed's confidence counts this much.
    double per = 1;              // A grudge's affinity is divided by this (quarrel).
    double step = 0;             // Each further price move scores this much less than the one before.
    int hourly = -1;             // The place's draw for the hour (a salt), or -1.
    int jitter = -1;             // The pair's draw (a salt), or -1...
    double jitterScale = 1;      // ...times this.
};
struct Topic
{
    std::string name;
    bool news = false;           // News (the everyday floor counts it; never cooled as everyday is).
    bool everyday = false;       // Everyday life (the floor raises it).
    double weight = 1;           // Its scores times this (tuning).
    std::map<std::string, Score> kinds;                      // By source ("" the topic's own).
    std::vector<std::pair<std::string, std::string>> next;   // The topic graph: (topic, criteria) a talk may pivot to.
    std::vector<std::string> aspects;                        // Its tiers (who, what, believe...): a pair one deeper.
};
struct Book
{
    std::map<std::string, Topic> topics;
    double jitter = .7, hourly = .4;                         // The pair's and the place's draws' widths.
    // The same pair on the same topic and thing within `noveltyDays` (the ledger): times `noveltyFactor`.
    double noveltyDays = 0, noveltyFactor = 1;
    // An incident (the same claim, wagon, ship, price, event...) talked of `saturationPerDay` times in this place today:
    // times `saturationFactor`.
    int saturationPerDay = 0;
    double saturationFactor = 1;
    // When more than `floorOver` of a place's last `floorWindow` talks were news, everyday topics get `floorBoost`.
    int floorWindow = 0;
    double floorOver = 1, floorBoost = 0;
    // A speaker's favourite topic gets `favourite` (from their traits: `favourites`, "piety=devout": ["faith"]; a
    // mantle's own from Phase 9).
    double favourite = 0;
    std::map<std::string, std::vector<std::string>> favourites;
    // Doc 84 §43 (U1): an everyday topic one of the two's mantle has whole scenes of its own for gets `mantleCovers`, and
    // its wearer raises it (never news, a claim or one's own news): a talk with a wearer in it leans toward what their
    // mantle says.
    double mantleCovers = 0;
    // Added to every topic of two by who they are to each other (pair.band: friends, family...): those close talk more.
    std::map<std::string, double> pairBonus;
    // A claim (or one's own news) is told to a listener once (the ledger's told set: the scorecard's K3).
    bool toldOnce = false;
    // How many topics a talk may cover (§8.5: 1-3; pivots along `next`, or a marked change of subject).
    int topicsPerTalk = 1;

    const Score& score(const std::string& topic, const std::string& kind = {}) const;
    bool news(const std::string& topic) const;
    bool everyday(const std::string& topic) const;
    double weight(const std::string& topic) const;
    // Whether the cooling and the floor are on (today's has neither).
    bool tuned() const
    {
        return noveltyFactor != 1 || saturationPerDay > 0 || floorBoost != 0 || favourite != 0 || mantleCovers != 0 || toldOnce || !pairBonus.empty();
    }
};

// topics.json's text, checked (every topic and source the director scores present, every field known), set as the
// process's book; false (and the book unchanged) with the reasons.
bool load(const std::string& text, std::string& problem);
// Today's: the C++ scores as they were before Phase 5 (a library without topics.json).
void reset();
const Book& book();
const Book& today();
// The sources the director scores, as (topic, kind): what a topics.json must give.
const std::vector<std::pair<std::string, std::string>>& sources();
} // namespace ratw::topics
