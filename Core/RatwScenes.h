#pragma once
// NPC-to-NPC scenes (Docs/Design/30-towns-and-talk.md, phase 6): short written exchanges, chosen by where two
// residents are, who they are to each other and themselves, the hour and the weather, and what has been happening
// (prices, caravans, crime, a death or a wedding in town). No language model: the library is written ahead
// (Data/Voice/scenes/**/*.scene) and only filled in here.
//
// A scene file holds scenes like this one:
//
//   group rain_coast = ridgemere saltreach
//
//   scene rm_market_herbs_dear
//   when topic=prices item=herbs dir=up region=rain_coast place=market|street
//   weight 2
//   a : Have you seen what herbs cost this week? | {price} a bundle, {b}. {price}!
//   b[merchant] : The caravan's late. I pay more, you pay more.
//   b[old] : In my day a bundle cost less than a smile.
//   b : I'll gather my own, then. | Robbery, plain and simple.
//   a?[friends] : Come by tonight; I'll share what I have.
//
// Lines are grouped into turns: consecutive lines of the same speaker whose tags ([...]) choose between them, ended by
// a line without tags (the fallback). The first line whose tags all hold for its speaker and the moment is spoken, one
// of its `|` alternatives at random. A turn marked `?` is spoken only if one of its lines holds; any other turn that
// can't be spoken (no line holds, or a {blank} can't be filled) means the scene isn't used.
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace ratw::scenes
{
// One of the two speakers, as the scenes see them.
struct Person
{
    std::string name;
    std::string sex;      // "female", "male"
    std::string stage;    // "young", "adolescent", "adult", "old"
    std::string job;      // A category: see Jobs.
    std::string role;     // "merchant", "guard", "civilian"
};

// The moment: the topic, the conditions it meets (region, place, band, time, season, day, weather, and the topic's own
// tags such as item=herbs or kind=raided), the two speakers, and what the blanks are filled with.
struct Situation
{
    std::string topic;
    std::map<std::string, std::string> tags;
    Person a, b;                      // b has no name for a bark (one speaker).
    std::map<std::string, std::string> blanks;
};

struct Condition
{
    std::string key;                  // "region", "a.job", "b.stage"... (a line's bare words are its speaker's).
    std::set<std::string> values;
    bool negate = false;
};
struct Variant
{
    std::vector<Condition> conditions;
    std::vector<std::string> texts;
    int line = 0;                     // In its file, for messages.
};
struct Turn
{
    int who = 0;                      // 0: a, 1: b
    bool optional = false;
    std::vector<Variant> variants;
};
struct Scene
{
    std::string id, file;
    std::set<std::string> topics;
    std::vector<Condition> when;
    double weight = 1;
    std::vector<Turn> turns;
    bool always = false;              // Weather and greetings: repeats are allowed (the "unheard" preference skips them).
};

struct Rendered
{
    std::string id;
    std::vector<std::pair<int, std::string>> lines;   // (speaker 0 or 1, words)
    bool always = false;
};

// The values the scenes may test, so a misspelt one is caught when the library loads.
extern const std::set<std::string> Topics, Places, Bands, Times, Seasons, Days, Weathers, Sexes, Stages, Jobs, Roles,
    Blanks, TopicTags;

class Library
{
public:
    // Every *.scene file under `dir`. False with the problems (up to twenty) if any file is wrong; nothing is loaded then.
    bool load(const std::string& dir, std::string& problem);
    // One file's text, added to what is loaded (tests, and load itself). False with the problems.
    bool parse(const std::string& text, const std::string& file, std::string& problem);
    void clear();
    std::size_t size() const { return scenes_.size(); }
    const std::vector<Scene>& scenes() const { return scenes_; }
    bool hasTopic(const std::string& topic) const { return byTopic_.count(topic) > 0; }

    // A scene for the moment, filled in: never one in `recent` (used here lately), and one that `heard` says the
    // listening players have not heard if there is any. Empty if nothing fits.
    Rendered pick(const Situation& s, std::uint64_t seed, const std::function<bool(const std::string&)>& heard,
                  const std::deque<std::string>& recent) const;
    // A scene filled in for the moment, or empty if it doesn't fit (its conditions, or a turn that can't be spoken).
    Rendered render(const Scene& scene, const Situation& s, std::uint64_t seed) const;
    bool fits(const Scene& scene, const Situation& s) const;

private:
    bool holds(const Condition& c, const Situation& s, int speaker) const;
    std::vector<Scene> scenes_;
    std::map<std::string, std::vector<std::size_t>> byTopic_;
    std::map<std::string, std::set<std::string>> groups_;   // region groups: name -> regions
    std::set<std::string> ids_;
};

// A job title or work label in the scenes' categories ("Master Smith of Westmarch" -> "smith").
std::string jobCategory(const std::string& title, const std::string& label, const std::string& role, int age);
} // namespace ratw::scenes
