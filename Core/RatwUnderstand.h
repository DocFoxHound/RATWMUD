#pragma once
// Authored understanding of a player's own words in the dialogue window (Docs/Design/84-living-conversations.md §13.5,
// Phase 13): what typed words mean when the NPC Mind isn't connected, or a call to it fails (the user's answer Q1). Read
// from Data/Voice/understand.json: about forty intents, each scored by its cues (runs of words, some only with a slot
// found), and the world's slots (a resident, a place, an issue, a kin word, a good, the wearer's quirk, the player's own
// name). Knows nothing of the world: the game gives it the words to look for (Lexicon). Tests/understand_tests.cpp holds
// it to the 500-line corpus (the scorecard's W2).
#include <map>
#include <string>
#include <vector>

namespace ratw::understand
{
// The world's words for one reading: given by the game (Game::dialogueLexicon) or by the corpus's fixture.
struct Lexicon
{
    std::string self;                                   // The resident spoken to ("Myrtle Stillford"): left out of the words
    std::vector<std::string> people;                    // Residents by full name ("Wick Marshwade")
    std::vector<std::string> places;                    // Places by name ("the Grey Gull Inn")
    std::vector<std::string> placeKinds;                // "smithy", "inn", "watch house"
    std::vector<std::string> goods;                     // "bread", "lamp oil"
    std::vector<std::string> houses;                    // "House Greatly"
    std::string quirk;                                  // The wearer's quirk ("swims_in_the_lake_even_in_autumn")
    std::string town;                                   // The town spoken in ("Coldwater")
};

struct Reading
{
    std::string intent;                                 // "" when nothing was understood
    double confidence = 0;                              // 0-1; under the threshold, the resident didn't follow
    std::map<std::string, std::string> slots;           // person, place, issue, kin, good, name, quirk (as found)
    std::string second;                                 // The next best intent (for the corpus's report)
    bool sure = false;                                  // confidence >= threshold
};

class Understanding
{
  public:
    // understand.json's text. False with the problem; nothing is kept then.
    bool load(const std::string& json, std::string& problem);
    bool loaded() const { return !intents_.empty(); }
    double threshold() const { return threshold_; }
    // What `text` means, said to the lexicon's resident.
    Reading read(const std::string& text, const Lexicon& lexicon) const;
    // The words as read: lower case, contractions spelt out, punctuation gone, the resident's own names and the
    // filler at either end left out.
    std::vector<std::string> words(const std::string& text, const Lexicon& lexicon) const;
    // Which of the choices on show the words are (their words, nearly enough: the content words shared over those of
    // either, 0.6 or more, or the choice's words all said), or -1.
    int matchChoice(const std::string& text, const std::vector<std::string>& choices, const Lexicon& lexicon) const;
    std::vector<std::string> intents() const;
    // The issue ids understand.json knows (opinions.json's).
    std::vector<std::string> issues() const;

  private:
    struct Cue
    {
        std::vector<std::string> words;
        double weight = 0;
        bool whole = false, start = false;
        std::string slot;
    };
    struct Intent
    {
        std::string id, needs;
        std::vector<Cue> cues;
        bool minor = false;                             // A greeting, thanks, yes...: a question said with it wins
        std::vector<std::vector<std::string>> unless;   // `not`: words that cost it 0.4
    };
    std::vector<Intent> intents_;
    double threshold_ = .5, runnerUp_ = .35;
    std::vector<std::vector<std::string>> filler_, address_;
    std::map<std::string, std::vector<std::string>> quirkWords_;
    std::map<std::string, std::vector<std::string>> contractions_;
    std::vector<std::vector<std::string>> kin_;
    std::map<std::string, std::vector<std::vector<std::string>>> issues_;
    std::vector<std::string> notNames_;
};

// Lower case letters, digits and apostrophes, split into words ("What's that?" -> what's, that).
std::vector<std::string> tokens(const std::string& text);
} // namespace ratw::understand
