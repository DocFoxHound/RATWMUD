#pragma once
// Cheaper NPC voices (Docs/Design/28-ai-cost.md): what the game says itself instead of asking a model. The speech
// router (Data/Voice/router.json) recognises a clear, single request in what a player said (a greeting, a price, the
// hours, a way, the watch) so the game can answer it from the world, and a resident's tone from their personality. (What
// is said back is the talk library's: Data/Talk/rules/player.talk, doc 62, phase 5.) (Its overheard exchanges are the talk library's
// now: Data/Talk/library.talk, doc 62.)
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

class Rules
{
  public:
    // router.json from a directory (it may be missing: the router is then off).
    bool load(const std::string& directory, std::string& problem);
    bool routes() const { return !intents_.empty(); }
    // Lower case, contractions spelt out, punctuation gone, the NPC's name and "please" left out.
    static std::string normalise(const std::string& heard, const std::string& npcName);
    // The one clear request in what was said, or an empty id: a model answers that.
    Intent intent(const std::string& heard, const std::string& npcName) const;
    // A tone from a personality's words ("plain" when none fits).
    std::string toneOf(const std::string& personality) const;

  private:
    struct Pattern
    {
        std::string intent;
        std::regex regex;
    };
    std::vector<Pattern> intents_;
    std::size_t maxWords_ = 9;
    std::map<std::string, std::vector<std::string>> tones_;
};
} // namespace ratw::voice
