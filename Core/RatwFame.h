#pragma once
// Fame (Docs/Design/56-fame-and-memory.md): good deeds as ledger events, never prose. A deed has a kind (from
// Data/Fame/deeds.json: its weight, family and phrase), its doers (player characters), a beneficiary (a resident, a town
// as "town:<id>", or none), where and when, its witnesses (with the name each knew each doer by, or none), each doer's
// look at the time, and the names it travels under. Game::recordDeed makes them (RatwGameFame.cpp); this is the record,
// its rules and its saving.
#include "RatwJsonDoc.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace ratw::fame
{
enum Weight
{
    Small = 0,
    Notable = 1,
    Great = 2,
    Legendary = 3
};
int weightOf(const std::string& name);              // -1 for an unknown name.
const char* weightName(int weight);

struct Kind
{
    std::string id, family, phrase;
    int weight = Small, perSeason = 0;
};
struct Rules
{
    std::vector<Kind> kinds;
    double liveDays[4] = {30, 184, 730, -1};        // -1: always.
    int witnessesMost = 8;
    double warmth = 5;                              // Liking a resident gains the first time it ties a notable deed to the doer.
    // The town's word (doc 56, 2): its reach and fading, and residents' ears.
    double wordStart = .25, wordDays = 3, caravanStart = .15, legendStart = .5;
    double fresh[4] = {0, 92, 365, -1}, fade[4] = {0, 92, 365, -1};
    int townMost = 40;
    double earMerchant = 1.5, earGuard = 1.5, earClergy = 1.5, earChild = .6;
    double greetEveryDays = 1;                      // The game's own "famous" greeting, at most this often a pair.
};
const Rules& rules();
Rules parseRules(const std::string& text);
// A file of Data/Fame (criers.json, chronicle.json...), parsed; a null value if missing.
json::Value dataFile(const std::string& name);
const Kind* kind(const std::string& id);

struct Witness
{
    std::string id;
    std::map<std::string, std::string> as;          // Doer -> the name this witness knew them by ("" none).
};
// A town whose word holds the deed (Phase 2): since when, how far it has reached (0..1), and how it came.
struct TownWord
{
    std::string town, carrier;
    double since = 0, reach = 0;
};
// How far word of a deed of `weight` has got round a town at `now` (0..1): grown from where it started, then faded.
double reach(const TownWord& w, int weight, double now, const Rules& r = rules());

struct Deed
{
    std::string id, kind;
    int weight = Small;
    std::vector<std::string> doers;                 // At most 6.
    std::string beneficiary, town, cell, place, detail, source;
    double x = 0, y = 0, day = 0;
    std::vector<Witness> witnesses;                 // At most rules().witnessesMost.
    std::map<std::string, std::string> looks;       // Doer -> its look then ("a grey wolf with a torn ear").
    std::map<std::string, std::vector<std::string>> names;   // Doer -> the names it travels under (at most 4).
    std::vector<TownWord> towns;
    std::vector<std::string> warmed;                // Residents already warmed by it (once each: the user, 2026-10-08).
    std::string nickname;                           // The nickname coined from it (Phase 3), "" none.
    bool noNickname = false;                        // The wolf asked folk not to use it: never coined again from this deed.
    bool cried = false;                             // A legendary deed called in every town (doc 56, 7).
    bool revoked = false;
};
// A nickname (doc 56, 4): what residents call a wolf for a deed, who coined it first, and whether the wolf asked folk
// not to use it.
struct Nickname
{
    std::string id, wolf, text, family, deed, coinedBy, town;
    double day = 0;
    bool dropped = false;
};
struct NicknameForms
{
    std::vector<std::string> epithets, possessive, deed;
};
struct NicknameRules
{
    std::map<std::string, NicknameForms> families;
    int smallOfAFamily = 3, most = 3;
    double withinDays = 92, affinity = 20, familiarity = 15, innkeeperAtReach = .5;
};
const NicknameRules& nicknameRules();
NicknameRules parseNicknames(const std::string& text);
// The forms a coiner may use for `deed` of `family`: `name` the name it knows the wolf by ("" by look: no possessive).
std::vector<std::string> nicknameForms(const std::string& family, const std::string& name, const std::string& deedPhrase);
json::Value toJson(const Deed& d);
Deed fromJson(const json::Value& j);

// The deed's phrase, its blanks filled: {beneficiary} by `beneficiary` (as the teller knows them), {place}, {detail}.
std::string phrase(const Deed& d, const std::string& beneficiary);

class Ledger
{
  public:
    Deed& record(Deed d);                           // Gives it its id.
    Deed* find(const std::string& id);
    const Deed* find(const std::string& id) const;
    bool revoke(const std::string& id);
    std::vector<const Deed*> byDoer(const std::string& doer) const;   // Live, newest first.
    const std::map<std::string, Deed>& all() const { return deeds_; }
    // Deeds past their life (rules().liveDays) and revoked ones a season old go; true if any went.
    bool prune(double today);
    json::Value save() const;
    void load(const json::Value& list);
    // Nicknames (Phase 3), saved apart.
    Nickname& coin(Nickname n);                     // Gives it its id.
    Nickname* nickname(const std::string& id);
    std::vector<const Nickname*> nicknamesOf(const std::string& wolf) const;   // Newest first, dropped too.
    const std::map<std::string, Nickname>& nicknames() const { return nicknames_; }
    json::Value saveNicknames() const;
    void loadNicknames(const json::Value& list);

  private:
    std::map<std::string, Nickname> nicknames_;
    std::uint64_t nextNickname_ = 1;
    std::map<std::string, Deed> deeds_;
    std::map<std::string, std::vector<std::string>> byDoer_;
    std::uint64_t next_ = 1;
    void index(const Deed& d);
};
} // namespace ratw::fame
