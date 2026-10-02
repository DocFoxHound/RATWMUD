#pragma once
// Cheaper NPC voices (Docs/Design/28-ai-cost.md): what the game says itself instead of asking a model. The speech
// router (Data/Voice/router.json) recognises a clear, single request in what a player said (a greeting, a price, the
// hours, a way, the watch) so the game can answer it from the world; the exchange library (Data/Voice/library.json)
// has overheard exchanges with blanks, for the ambient director to fill in.
#include <cstdint>
#include <map>
#include <regex>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace ratw::voice
{
struct Intent
{
    std::string id, target;                        // target: what a "where" asks after ("" otherwise).
};
struct Exchange
{
    std::string kind, band, tone;
    std::vector<std::pair<int, std::string>> lines;   // 0: who opens, 1: who answers.
};

class Rules
{
  public:
    // router.json and library.json from a directory (either may be missing: that part is then off).
    bool load(const std::string& directory, std::string& problem);
    bool routes() const { return !intents_.empty(); }
    // Lower case, contractions spelt out, punctuation gone, the NPC's name and "please" left out.
    static std::string normalise(const std::string& heard, const std::string& npcName);
    // The one clear request in what was said, or an empty id: a model answers that.
    Intent intent(const std::string& heard, const std::string& npcName) const;
    // A tone from a personality's words ("plain" when none fits).
    std::string toneOf(const std::string& personality) const;
    // A line for the intent, how well they know the speaker ("stranger", "known") and tone, with every blank filled
    // from the facts; "" when none can be. `avoid` is the line said last, not to be said twice running.
    std::string line(const std::string& intent, const std::string& band, const std::string& tone,
                     std::map<std::string, std::string> facts, std::uint64_t seed, const std::string& avoid = {}) const;
    // An exchange of this kind for two who are friends, acquaintances or rivals, with its blanks filled; none (empty
    // lines) when the library has nothing fitting. Entries in `recent` (by index) are passed over while others fit.
    Exchange exchange(const std::string& kind, const std::string& band, const std::map<std::string, std::string>& facts,
                      std::uint64_t seed, std::set<std::size_t>& recent) const;
    std::size_t libraryEntries() const { return library_.size(); }
    // `{name}` blanks filled from the facts; `complete` false if any is missing (or empty).
    static std::string fill(const std::string& text, const std::map<std::string, std::string>& facts, bool& complete);

  private:
    struct Pattern
    {
        std::string intent;
        std::regex regex;
    };
    std::vector<Pattern> intents_;
    std::size_t maxWords_ = 9;
    std::map<std::string, std::vector<std::string>> openers_, tones_;
    std::map<std::string, std::map<std::string, std::map<std::string, std::vector<std::string>>>> lines_;
    std::vector<Exchange> library_;
};
} // namespace ratw::voice
