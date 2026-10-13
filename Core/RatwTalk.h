#pragma once
// Talk (Docs/Design/62-talk.md): one dialogue system for every written line, chosen by what's true and spoken in the
// speaker's own words. Three layers:
//
//   facts (the query)  ->  rules and scenes (the most specific usually wins)  ->  grammar (the person -> dialect -> plain)
//
// The library is written ahead in Data/Talk: what facts there are (facts.json), the odds (settings.json), the rules
// and scenes (**/*.talk) and the grammars (grammar/*.grammar). Nothing here asks a model.
//
// A .talk file holds rules (one line, for one speaker) and scenes (a whole exchange, written in one place):
//
//   rule bark.bread
//   when concept=bark here.place=market merchant
//   say Fresh bread! Still warm! | Loaves, #here.town#'s best!
//
//   scene rm_market_herbs_dear
//   when topic=prices topic.item=herbs topic.dir=up here.region=rain_coast here.place=market|street
//   weight 2
//   a : Have you seen what herbs cost this week? | #topic.price# a bundle, #b.name#. #topic.price#!
//   b[merchant] : The caravan's late. I pay more, you pay more.
//   b[old] : In my day a bundle cost less than a smile.
//   b : I'll gather my own, then. | Robbery, plain and simple.
//   a?[friends] : Come by tonight; I'll share what I have.
//
// A scene's lines are grouped into turns: consecutive lines of the same speaker whose tags ([...]) choose between them,
// ended by a line without tags. The first line whose tags hold is spoken, one of its `|` alternatives; alternatives
// written in parallel answer each other (the second reply to the second remark). A turn marked `?` is spoken only if
// one of its lines holds; any other turn that can't be spoken means the scene isn't used. A scene is checked whole
// before it's chosen.
//
// Criteria are facts: `key=value|value`, `key!=value`, and numbers (`self.age>=60`). Keys are namespaced: `self.` (who
// is speaking this line), `other.` (who they speak to), `a.` and `b.` (in a scene, the opener and the answerer),
// `pair.`, `here.`, `topic.`, `world.`. A key may be written without its namespace where only one has it (`place` is
// `here.place`), and a person's key alone is the speaker's (`b[stage=young]`, `when sex=female`). Bare words in tags
// stand for the speaker's sex, stage, role or job, or the two's band, the hour or the weather (`b[old]`, `a[friends]`).
//
// Words are Tracery templates: `#symbol#` from the speaker's grammars (their own, their dialect, then plain), `#a.name#`
// or `#topic.price#` from the facts, with modifiers (`.capitalize`, `.a`, `.s`) and pronouns (`#other.they#`).
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace ratw::talk
{
using Facts = std::map<std::string, std::string>;

// One of the two speakers.
struct Speaker
{
    Facts facts;                          // id, name, sex, stage, age, job, role, trade...: keys without "a."/"self.".
    std::vector<std::string> grammar;     // Their grammar layers, nearest first; "plain" always comes last.
};

// What is about to be said, and everything true about it.
struct Query
{
    std::string concept;                  // "talk.prices", "bark"
    Facts facts;                          // "pair.band", "here.place", "topic.item"...: full keys.
    Speaker a, b;                         // a speaks first; b is no one when `alone`.
    bool alone = false;
};

// What saying something changes (phase 4): a fact remembered about a resident, two of them, or a town, for a time;
// "said:<id>" records a rule said, for `once` and `cooldown`. The game keeps them (Game's talk memory) and gives them
// back as facts: a resident's as "mem.<key>" (self.mem.<key>), a pair's as "pair.mem.<key>", a town's as
// "world.mem.<key>" (and "said:<id>", "pair.said:<id>").
struct Effect
{
    std::string scope;                    // "person", "pair", "town"
    std::string owner;                    // A resident's ID; a pair's two IDs, sorted, as "a|b"; a town's region.
    std::string key, value = "yes";
    double days = -1;                     // How long it is kept, in game days; -1: for good.
};

struct Said
{
    std::string id;                                   // The rule or scene ("" when nothing fits); a chain's first.
    std::vector<std::pair<int, std::string>> lines;   // (speaker 0 or 1, words)
    bool repeat = false;                              // May be said again to those who have heard it (weather, barks).
    std::vector<Effect> effects;                      // To remember (phase 4).
    std::vector<std::string> chain;                   // Every rule or scene said, in order (a chain's beats).
};

// Why something was said (talk_check): every rule or scene that held, its score, and the choice made.
struct Trace
{
    struct Held
    {
        std::string id;
        int score = 0, priority = 0, tier = 0;
        double weight = 1;
        bool heard = false;
    };
    std::vector<Held> held;
    std::vector<std::string> passed;      // Ids that didn't fit, and why.
    std::string why;                      // How the choice was made.
    std::vector<std::string> steps;       // The grammar's expansions, in order.
};

enum class Ns : std::uint8_t { A, B, Self, Other, Pair, Here, Topic, World };
struct Criterion
{
    Ns ns = Ns::Self;
    std::string name, full;               // "place", "here.place" (full: for shared facts).
    enum class Op : std::uint8_t { In, NotIn, Lt, Le, Gt, Ge, Has, Lacks } op = Op::In;   // Has: remembered (self.mem.x)
    std::set<std::string> values;
    double number = 0;
};
struct Variant
{
    std::vector<Criterion> when;
    std::vector<std::string> texts;
    std::set<std::string> needs, marks;   // Marks needed (a@worried), and set when spoken (#worried): a scene's branches.
    int line = 0;                         // In its file, for messages.
};
struct Turn
{
    int who = 0;                          // 0: a, 1: b
    bool optional = false;
    std::vector<Variant> variants;
};
// A rule or a scene. A rule is a scene of one turn for its speaker.
struct Entry
{
    std::string id, file;
    bool scene = false;
    std::set<std::string> concepts;
    std::vector<Criterion> when;
    int priority = 0, score = 0;          // score: the criteria tested, plus priority (a named person counts 5).
    double weight = 1;
    bool repeat = false;
    std::vector<Turn> turns;
    // Phase 4: what is remembered when it's said (scope self, other, pair or town), the concept handed on (to the
    // other speaker, or the same one), and how often it may be said.
    struct Remember
    {
        std::string scope, key, value;
        double days = -1;
    };
    std::vector<Remember> remember;
    std::string thenWho, thenConcept;
    enum class Limit : std::uint8_t { None, Once, PerDay, PerOther, Cooldown } limit = Limit::None;
    double cooldownDays = 0;
};

// The odds and lists of settings.json.
struct Settings
{
    double best = .7, next = .2, lower = .1;    // The chance of each score tier (see pick).
    int strict = 10;                            // A priority this high always wins when it holds.
    int namedPerson = 5;                        // What `self.id=...` counts for in a score.
    std::set<std::string> concepts, repeat, alone;
    std::set<std::string> bestOnly;             // Concepts that always take their best tier (answers to players).
};

// The facts there are, and the values each may take.
struct FactKind
{
    enum class Type : std::uint8_t { Words, Text, Number } type = Type::Words;
    std::set<std::string> values;               // Words only.
};

class Library
{
public:
    // facts.json, settings.json, grammar/*.grammar (plain first), groups.talk, then every other *.talk under `dir`.
    // False with the problems (up to twenty) if anything is wrong; nothing is loaded then.
    bool load(const std::string& dir, std::string& problem);
    // Only what the rules stand on (facts, settings, grammars, groups), for tests and checking one file.
    bool loadFoundation(const std::string& dir, std::string& problem);
    // One file's text, added to what is loaded. False with the problems, and nothing of it kept.
    bool parse(const std::string& text, const std::string& file, std::string& problem);
    bool parseGrammar(const std::string& text, const std::string& file, std::string& problem);
    void clear();

    std::size_t size() const { return entries_.size(); }
    const std::vector<Entry>& entries() const { return entries_; }
    const Entry* find(const std::string& id) const;
    bool has(const std::string& concept) const { return byConcept_.count(concept) > 0; }
    const Settings& settings() const { return settings_; }
    const std::map<std::string, FactKind>& personFacts() const { return person_; }
    const std::map<std::string, FactKind>& sharedFacts() const { return shared_; }
    std::vector<std::string> grammars() const;
    std::set<std::string> symbols(const std::string& grammar) const;

    // Something to say for the query, filled in: never one in `recent` (said here lately). Of those that hold, scores
    // are grouped into tiers, and a tier is chosen first (the best 70%, the next 20%, any lower 10%), then within it one
    // `heard` says the listening players haven't heard, by weight. A priority of `strict` or more always wins; one below
    // 0 speaks only when nothing else holds. Empty if nothing fits.
    Said pick(const Query& q, std::uint64_t seed, const std::function<bool(const std::string&)>& heard,
              const std::deque<std::string>& recent, Trace* trace = nullptr) const;
    // A whole conversation: pick, then each rule's `then` handed on (to the other speaker, who answers with their own
    // best rule for that concept, its best tier), up to 8 beats; what each beat remembers is known to the next.
    Said converse(const Query& q, std::uint64_t seed, const std::function<bool(const std::string&)>& heard,
                  const std::deque<std::string>& recent, Trace* trace = nullptr) const;
    // How `speaker` speaks, for a model voicing them (doc 62, phase 5): where they grew up and how strongly it shows,
    // their dialect's words, and their habits (their own oath...). Empty for plain speech with no habits.
    std::string voiceCard(const Speaker& speaker) const;
    // An effect as the facts a query would have of it (so a chain's later beats see what its earlier ones remembered).
    static void remembered(const Effect& e, Query& q);
    // One rule or scene, filled in, or empty if it doesn't fit (its criteria, or a turn that can't be spoken).
    Said render(const Entry& e, const Query& q, std::uint64_t seed, Trace* trace = nullptr, int speaker = 0) const;
    bool fits(const Entry& e, const Query& q, int speaker = 0) const;
    // A template, expanded for `speaker` (0: a, 1: b); false if a fact it needs is missing.
    bool expand(const std::string& text, const Query& q, int speaker, std::uint64_t seed, std::string& out,
                Trace* trace = nullptr) const;

private:
    struct Alt
    {
        std::string text;
        char sex = 0;                       // 'm' or 'f': only for one sex of the one spoken to (^m, ^f).
    };
    struct GrammarEntry
    {
        std::vector<Criterion> when;
        std::vector<Alt> alts;
        bool add = false;                   // `+=`: added to what the layers below say.
        int score = 0;
    };
    struct Symbol
    {
        bool habit = false;                 // `name!`: chosen once per person, for good.
        bool pairHabit = false;             // `name!!`: once per person for each one they speak to.
        std::vector<GrammarEntry> entries;
    };
    using Grammar = std::map<std::string, Symbol>;
    struct Expansion;

    enum class Where : std::uint8_t { When, Line, GrammarWhen };
    bool criterion(const std::string& token, Where where, int speaker, Criterion& c, std::string& error) const;
    bool bareWord(const std::string& word, int speaker, Where where, Criterion& c) const;
    bool holds(const Criterion& c, const Query& q, int speaker) const;
    const std::string* fact(Ns ns, const std::string& name, const std::string& full, const Query& q, int speaker) const;
    // Checks a template at load: every fact known, every symbol in plain; `longest` is its longest expansion (facts
    // counted as nothing).
    bool checkText(const std::string& text, std::vector<std::string>& errors, const std::string& where,
                   std::size_t* longest) const;
    std::size_t longestSymbol(const std::string& name, std::map<std::string, std::size_t>& memo, std::set<std::string>& open,
                              int depth, std::string& error) const;
    bool expandInto(const std::string& text, Expansion& x, int depth, std::string& out) const;
    bool expandSymbol(const std::string& name, Expansion& x, int depth, std::string& out) const;
    Said pickOne(const Query& q, std::uint64_t seed, const std::function<bool(const std::string&)>& heard,
                 const std::deque<std::string>& recent, int speaker, bool chained, Trace* trace) const;
    bool checkGrammars(std::vector<std::string>& errors) const;
    bool readSchema(const std::string& text, std::string& problem);
    bool readSettings(const std::string& text, std::string& problem);

    std::vector<Entry> entries_;
    std::map<std::string, std::vector<std::size_t>> byConcept_;
    std::map<std::string, std::size_t> byId_;
    std::map<std::string, std::set<std::string>> groups_;    // Region groups: name -> regions.
    std::map<std::string, Grammar> grammars_;
    std::map<std::string, FactKind> person_, shared_;       // shared_: by full key ("here.place").
    std::map<std::string, std::string> aliases_;             // "place" -> "here.place"
    Settings settings_;
};

// A job title or work label in the talk's job categories ("Master Smith of Westmarch" -> "smith").
std::string jobCategory(const std::string& title, const std::string& label, const std::string& role, int age);
} // namespace ratw::talk
