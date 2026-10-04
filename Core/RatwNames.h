#pragma once
// Names and introductions (Docs/Design/32-parties-chapters-factions.md, 1.5): a wolf's name is hidden from everyone
// until that wolf introduces itself. Until then it is its look ("a tall grey wolf with white socks") or its trade ("the
// innkeeper"). A wolf has a true name and up to three aliases, and introduces itself by saying one of them after an
// introduction phrase ("I'm Kestrel", "call me Kestrel"); whoever heard it knows it by that name. Pure rules: the game
// (RatwGameNames.cpp) decides who heard what.
#include "RatwAppearance.h"
#include "RatwJsonDoc.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace ratw::names
{
constexpr std::size_t MaxAliases = 3;
constexpr std::size_t MaxKnown = 600;           // Names one wolf keeps (the oldest-met go first).

// Among `names` (a wolf's own: true name and aliases), the one `text` introduces it by: an introduction phrase ("I'm",
// "I am", "my name is", "name's", "call me", "they call me", "you can call me"), or the name opening the line followed
// by ", of" or "at your service". Whole words, any case. "" when it introduces none of them.
std::string introducedName(const std::string& text, const std::vector<std::string>& names);

// Why an alias can't be taken ("" when it can): 2-24 letters (spaces, hyphens and apostrophes inside), not a name the
// wolf already has, at most MaxAliases.
std::string aliasProblem(const std::string& alias, const std::string& trueName, const std::vector<std::string>& aliases);

// How a stranger looks, in a few words: "a tall young grey wolf with white socks". Lower case; "a"/"an" fitted.
std::string describe(const Appearance& a, int age);
// A resident called by their post, lower case: "the innkeeper", or "a guard" where there are `several`. A post
// written as what the wolf does reads as such: "a wolf who carries loads for hire", "the wolf on patrol".
std::string byTrade(const std::string& title, bool several);
// A written description whose kind of wolf carries a colour ("a grey timber wolf") said in the coat's own colour
// ("a sandy timber wolf"): the generator wrote the kind and drew the coat apart, and the eye should win.
std::string fitCoat(const std::string& text, const Appearance& a);
// "a" or "an" before a word.
std::string article(const std::string& word);
// The first letter in upper case (for a label opening a sentence).
std::string capitalised(const std::string& text);
// `text` with each whole-word occurrence of a name in `labels` replaced by its label (capitalised at the start of a
// sentence). A name followed by another capitalised word is left alone: it is part of a longer proper name ("Ash
// Hollow").
std::string veil(const std::string& text, const std::map<std::string, std::string>& labels);

struct Known
{
    std::vector<std::string> names;   // Every name they were given, the newest last.
    std::string how;                  // "introduced", "founding", "familiar"...
    double day = 0;                   // Calendar day they first learned one.
};

// Who knows whom, and by what name: one-sided, as hearing is.
class Acquaintances
{
  public:
    // `knower` learns `name` for `known`. True when it is new to them (a first name, or another name).
    bool learn(const std::string& knower, const std::string& known, const std::string& name, const std::string& how, double day);
    const Known* find(const std::string& knower, const std::string& known) const;
    bool knows(const std::string& knower, const std::string& known) const { return find(knower, known) != nullptr; }
    // The name `knower` calls `known` by (the newest they were given), or "".
    std::string nameFor(const std::string& knower, const std::string& known) const;
    void forget(const std::string& id);          // A wolf gone from the world: nobody knows them, they know nobody.
    std::size_t size() const;
    json::Value save() const;
    void load(const json::Value& saved);

  private:
    std::map<std::string, std::map<std::string, Known>> known_;
};
} // namespace ratw::names
