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
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "RatwMantle.h"

namespace ratw::talk
{
using Facts = std::map<std::string, std::string>;

// One of the two speakers.
struct Speaker
{
    Facts facts;                          // id, name, sex, stage, age, job, role, trade...: keys without "a."/"self.".
    std::vector<std::string> grammar;     // Their grammar layers, nearest first; "plain" always comes last.
    // A player's character (or facts "player=yes"): the engine never writes them a line (doc 84 §13: players speak only
    // in the window). What a resident's line owes them stays theirs to answer (Slice 2's review).
    bool player = false;
};

// What is about to be said, and everything true about it.
struct Query
{
    std::string concept;                  // "talk.prices", "bark"
    Facts facts;                          // "pair.band", "here.place", "topic.item", "conv.k"...: full keys.
    Speaker a, b;                         // a speaks first; b is no one when `alone`.
    bool alone = false;
    // A third party for a line that `binds x=<slot>` (doc 84 §7.1, Phase 2): the person the slot names, for the one
    // speaking (0: a, 1: b), as facts without "x." (a person's facts, and the speaker's bond to them: affinity,
    // disposition). False when there is no one: then the line doesn't fit. Unset: no line that binds ever fits.
    std::function<bool(const std::string& slot, int speaker, Facts& facts)> bind;
};

// What saying something changes (phase 4): a fact remembered about a resident, two of them, or a town, for a time;
// "said:<id>" records a rule said, for `once` and `cooldown`. The game keeps them (Game's talk memory) and gives them
// back as facts: a resident's as "mem.<key>" (self.mem.<key>), a pair's as "pair.mem.<key>", a town's as
// "world.mem.<key>" (and "said:<id>", "pair.said:<id>"). Scope "regard" (owner "holder|other", key an axis, value a
// number) is a change in how one feels about the other, made by the game through Game::regard ("talk", bounded).
struct Effect
{
    std::string scope;                    // "person", "pair", "town"
    std::string owner;                    // A resident's ID; a pair's two IDs, sorted, as "a|b"; a town's region.
    std::string key, value = "yes";
    double days = -1;                     // How long it is kept, in game days; -1: for good.
    std::string why;                      // The rule or scene that made it.
};

// Where one line of what was said came from (Docs/Design/84 §15.1, for the conversation simulator's transcripts and the
// server's --log-talk-words): its rule or scene, which of its `|` alternatives, and the grammar layers whose words
// filled its symbols (a dialect's name when a dialect's words were spoken). Recorded only; nothing reads it to choose.
//
// Doc 84 §8.1 (Phase 2): and what the line does in the talk, from its moves (Data/Talk/moves.json): the moves it makes, the
// answers it makes due (`owes`, written or handed on by `then`, else its moves' `expects`; "self:" or "any:" before one
// not owed by the one spoken to), the soft ones (`invites`), which of the line before it it answers, the claims it
// passes on or denies (claims.json's family, as topic.claim_kind says it), its tone, and the conversation's tags it sets
// ({k=v}: conv.k). The transcripts and --log-talk-words carry them; Phase 3's turn engine chooses by them.
struct SaidLine
{
    std::string entry;
    int alt = -1;
    std::vector<std::string> layers;      // In the order first used; "plain" included when plain's words were.
    std::vector<std::string> habits;      // The habits (`name!`, `name!!` symbols: an oath, a saying) it used.
    std::vector<std::string> moves;       // "tell.gossip", "react.news.known"
    std::vector<std::string> owes;        // Each "move|move" (any one answers it)
    std::vector<std::string> invites;     // The same, soft: answered only if a fitting line comes
    std::vector<std::string> answers;     // The moves of the line before it that this one answers
    std::vector<std::string> asserts, denies;   // Claims ("theft", or "topic" when the topic has no claim kind)
    std::string tone;                     // light, even, serious, grave, tender, cross ("": no move, no tone)
    bool gist = false;
    Facts tags;                           // {k=v}: conv.k for the rest of the conversation
    double keep = 0;                      // {k=v keep 7d}: kept in the ledger this long (game days; Phase 5)
    int who = 0, to = -1;                 // The speaker (0: a, 1: b) and the one spoken to (-1: no one)
    std::string text;
    // Phase 7 (§8.12.3): the opener's own words when something was said in its breath before them (a greeting, "Back
    // again?", "As I was saying."), so the collision filter keeps the words themselves, not only the whole line.
    std::string bare;
    // Phase 3 (§8.2, the scorecard's C2): how well a reply answered what was owed (3: the handed-on concept's own, or
    // the exact variant owed; 1: another line doing an owed move; 0: a fallback), and the best fit any line that held
    // had (before what was said already, the frames and the tone rules left out). -1: it answered nothing owed.
    int fit = -1, best = -1;
    // Phase 4 (§8.2, Valve's rule): the turn (from 0) and the line within it (a turn's variant) it was said from, so a
    // scene's queued turn is checked again when it comes due. -1 for a line written outside the library.
    int turn = -1, variant = -1;
    // Phase 5 (§8.5, §8.7): the topic it was said on (the talk's, after any pivot); whether its words stand on the talk
    // ledger (a callback: what the two said before, a thread, a kept tag, a promise or an invitation; the scorecard's
    // C9); an invitation it makes the world check (invite <place> <hour> <days>) and a talk-only promise (promise self:
    // <thing> <days>); the goal (frames.json's) it is a step of.
    std::string topic;
    bool callback = false;
    std::string invitePlace;
    int inviteHour = -1;
    double inviteDays = 0;
    std::string promiseWhat;
    double promiseDays = 0;
    std::string goal;
    // §37 (the hybrid): what part of the talk the line is: "scene" (a whole written scene's turn, the body), "exchange"
    // (a rule said as the body where no scene fit: the opener and what it owes), "written" (the game's authored
    // exchange, its body), "edge" (stitched at the edges: an answer owed after the body, a joiner, a break), "close"
    // (the goodbyes).
    std::string part;
    bool goalDone = false;                // Its goal's last step: done, kept between the two for the goal's days
    double goalDays = 0;
    // Phase 6 (§8.8, §10.6): the accent it was spoken in (the speaker's, a step broader or plainer toward the one spoken
    // to: accommodation; the scorecard's D1), and, on a line that airs a stance (an opinion given, agreed or disagreed
    // with, insisted on, conceded), the issue, the speaker's own side on it as it was then, and how many steps events had
    // moved it (drift: the scorecard's T4 tells a stance moved by events from one that wandered).
    std::string accent;
    std::string issue, stance;
    int drift = 0;
    std::string basis;                    // Which of the issue's leans held, and the drift: what the stance stood on
};
using LineInfo = SaidLine;
struct Said
{
    std::string id;                                   // The rule or scene ("" when nothing fits); a chain's first.
    std::vector<std::pair<int, std::string>> lines;   // (speaker 0 or 1, words)
    bool repeat = false;                              // May be said again to those who have heard it (weather, barks).
    std::vector<Effect> effects;                      // To remember (phase 4).
    std::vector<std::string> chain;                   // Every rule or scene said, in order (a chain's beats).
    std::vector<LineInfo> info;                       // One a line, beside `lines` (doc 84 §15.1).
    // How a whole conversation ended (doc 84 §8.4, Phase 3; converse): "farewell" (a closing pair, or a line that
    // `ends`), "cap" (the most lines a talk may have), "nothing" (nothing owed was answered, and no closing fitted),
    // "shrug" and "then" (a library before moves.json: a shrug, or a rule handing on nothing). "" for one line (pick).
    std::string ended;
};

// Doc 84 §8.1 (Phase 3): what keeps a conversation fresh for those listening. The listeners' heard set (by entry) and
// the place's recent list (an opener said here lately is never said again here). Phase 4 adds heard alternatives and
// the collision filter (§8.12).
struct Freshness
{
    std::function<bool(const std::string&)> heard;
    std::deque<std::string> recent;
    // Phase 6 (§8.6, §8.12.4): whether a listener (by id) has been told something already: "claim:<key>" (a claim or
    // one's own news, the talk ledger's told set) or "story:<entry>" (a story, a memory or a tease: its heard set). The
    // engine never tells it them again. Unset: nothing refused.
    std::function<bool(const std::string& listener, const std::string& what)> told;
    // Phase 6 (§8.12.3, the collision filter): whether another than `speaker` said these words in this place within the
    // hour (a distinctive line: 6 words or more). Such a line is passed over. Unset: nothing passed over.
    std::function<bool(const std::string& words, const std::string& speaker)> collides;
};

// One thing a speaker means to bring up (doc 84 §8.5, Phase 5: World::agendaFor, the director's topics for the two, each
// owned by the one who'd raise it). In a lull with budget left, the talk may turn to one: a step along the topic graph
// (topics.json's `next`, its criteria holding now: a pivot, "Speaking of wagons...") or, marked, anywhere (a change of
// subject, "Anyway..."); a thread left unfinished on it is returned to ("As I was saying...").
struct AgendaItem
{
    std::string kind, topic, claim;       // tell, news, smalltalk...; the topic; what it is about (the director's key)
    double urgency = 0;
    bool done = false;
    Facts facts;                          // Its topic's facts (topic.*, and any other it sets: here.festival)
    std::string concept;                  // talk.<topic>
    std::map<std::string, std::string> knows;   // Who held its claim before (by id: yes, no), for `knows`
    int index = -1;                       // The caller's own (which of its topics it is)
};

// A conversation as it goes (doc 84 §8.1, Phase 3): who is in it, its phase, its frames and budget, the answers owed,
// its own facts (conv.*) and what has been said. Library::begin starts it, Library::next adds one line at a time.
// Phase 4: a third may join (Library::join: people[2]); a line's `who` and `to` are indices into `people`.
struct Conversation
{
    std::vector<std::string> people;              // Ids, 2, or 3 once one has joined (§8.9); [0] opened.
    enum class Phase : std::uint8_t { Opening, Body, Closing, Ended } phase = Phase::Opening;
    std::vector<std::string> frames;              // frames.json's, stacked (strangers, at_work...): worked out at begin.
    std::string topic, tone = "even";
    int beat = 0, budget = 6, depth = 0;          // beat: lines said. budget: lines meant (§8.4). depth: Phase 5.
    // An answer owed (`owes`) or invited (`invites`, soft): `by` made it due, `to` owes it (-1: anyone). `concept` is a
    // `then` hand-on's concept (its own rules answer it best); a hand-on to a topic owes no move, only the concept.
    struct Open
    {
        int by = 0, to = -1;
        std::vector<std::string> moves;
        bool soft = false;
        std::string concept;
    };
    std::vector<Open> open;                       // Newest last.
    std::vector<std::vector<AgendaItem>> agenda;  // One list per person (Phase 5).
    Facts conv;                                   // conv.* facts, by their short keys: tags, and the engine's own
    std::map<std::string, std::string> bound;     // x -> resident id (Phase 4's per-beat queries)
    std::set<std::string> said, asked, covered;   // Entries, questions (their words) and topics used in this talk
    std::vector<SaidLine> lines;
    std::uint64_t seed = 0;
    // The engine's own bookkeeping.
    std::string concept;                          // The opener's (talk.gossip...)
    std::deque<SaidLine> queued;                  // A scene's turns still to come (§8.2)
    std::vector<Effect> effects;                  // What saying it changes, in the order said
    std::vector<std::string> chain;               // The rules and scenes said, in order
    std::string ended;                            // Said::ended
    bool legacy = false;                          // A library before moves.json: today's chain (the golden transcripts)
    bool repeat = false;                          // The opener's concept may be said again to those who heard it
    int closer = -1, extended = 0, gaps = 0;      // Who closes; extends taken (§7.2 max_extend); answers nobody gave
    int handOns = 0;                              // (legacy: the chain's beats so far)
    std::string stake;                            // Why the third joined (named, kin, friend, goods, witness, friends)
    int dropped = 0;                              // Queued scene turns dropped when they came due (Valve's rule)
    // Phase 5 (§8.5): the topic now, once the talk has turned from the director's (its facts over each query's topic.*,
    // and who knew its claim, by id); the agenda item it is on (-1: the director's first topic); the topics covered in
    // order; the goal under way and its next step.
    Facts topicFacts;
    std::map<std::string, std::string> knows;
    int agendaAt = -1;
    std::vector<std::string> topics;
    std::string goal;
    int goalStep = 0;
    // Phase 6 (§8.6): what the topic now is about (the director's key: a claim's subject and words, one's own news), for
    // Freshness::told: never told a listener twice.
    std::string claim;
    // Slice 2's review (§13): who in it is a player (0: a, 1: b), never given a line by the engine.
    std::set<int> players;
    // §37 (the hybrid): what carried the body ("scene": a whole written scene; "exchange": a rule, where none fit;
    // "written": the game's authored exchange), and
    // the lines stitched after it that weren't goodbyes.
    std::string body;
    int stitched = 0;
    bool hybrid = false;                          // (Played as the hybrid: settings.json's, a talk of residents on a topic.)
    // §43: the body is the one spoken to's own mantle scene, cast with them opening it (its turns' who swapped: the
    // scene's `a` is people[1]); its queued turns are checked again with the two the scene's way round. Its entry's id.
    std::string cast;
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

// Conv: the conversation's own facts (doc 84 §7.11: conv.move, conv.prev, conv.beat, conv.tone and every {k=v} tag);
// X: a third party a line binds (x.name, x.job..., x.affinity), both kept in Query::facts by their full keys.
enum class Ns : std::uint8_t { A, B, Self, Other, Pair, Here, Topic, World, Conv, X };
struct Criterion
{
    Ns ns = Ns::Self;
    std::string name, full;               // "place", "here.place" (full: for shared facts).
    enum class Op : std::uint8_t { In, NotIn, Lt, Le, Gt, Ge, Has, Lacks } op = Op::In;   // Has: remembered (self.mem.x)
    std::set<std::string> values;
    double number = 0;
    bool list = false;                    // A list fact (self.likes=x holds when the list has x): Phase 6
};
// {k=v} before an alternative's words (doc 84 §7.1): conv.k=v for the rest of the conversation; `keep 7d` keeps it in
// the ledger too (Phase 5).
struct Tag
{
    Facts set;
    double keep = 0;
};
struct Variant
{
    std::vector<Criterion> when;
    std::vector<std::string> texts;
    std::vector<Tag> tags;                // Beside texts: each alternative's (empty: none).
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
    struct Regard
    {
        std::string who;                  // "self": how the speaker comes to feel about the other; "other": the other's.
        std::vector<std::pair<std::string, double>> change;   // (axis, amount)
    };
    std::vector<Regard> regard;
    std::string thenWho, thenConcept;
    enum class Limit : std::uint8_t { None, Once, PerDay, PerOther, Cooldown } limit = Limit::None;
    double cooldownDays = 0;
    // Doc 84 §7.1 (Phase 2): what each turn does (a rule has one turn), from `does` (a scene names each turn's, `/`
    // between them), or a rule's answer concept's alias (moves.json); `tone` likewise. What the last turn owes and
    // invites (written; a `then` hands on its concept's move too, `derived`). The turn (from 0) that asserts the topic's
    // claim, denies it, or makes the gist (-1: none). A third party bound for x.* (`binds x=self.spouse`). The mantle
    // it belongs to (Phase 9).
    struct Owed
    {
        std::string who = "other";        // other, self, any
        std::vector<std::string> moves;   // Any one of them answers it.
        bool soft = false;                // `invites`
        bool derived = false;             // From `then` (its concept's alias), not written
    };
    std::vector<std::vector<std::string>> does;
    std::vector<std::string> tones;
    std::vector<Owed> owes;
    int asserts = -1, denies = -1, gist = -1;     // (asserts: the turn, -1 its move's default, NeverAsserts `asserts none`)
    static constexpr int NeverAsserts = -3;
    std::string assertsClaim, deniesClaim;
    std::map<std::string, std::string> binds;
    bool aliasMoves = false;              // Its moves are its concept's alias, not written
    std::string owner;
    bool hedged = false;                  // Phase 6: an alternative says #hedge# (a temperament weighs it: §7.5)
    // Phase 5 (§7.1, §8.7): an invitation the world checks (`invite here <hour> <days>`), a talk-only promise (`promise
    // self: <thing> <days>`), and whether its criteria read the talk ledger (a callback).
    std::string invitePlace;
    int inviteHour = -1;
    double inviteDays = 0;
    std::string promiseWhat;
    double promiseDays = 0;
    bool ledger = false;
};

// A dialogue move (doc 84 §7.2, Data/Talk/moves.json): what a line does, what it makes due, what it answers, and how a
// dispreferred answer is shaped. An answer concept is an alias of one (jest.answer: tease.return), so the 2,778 `then`
// hand-ons have moves too.
struct Move
{
    std::string name, family;
    bool first = false;                   // May open (needs nothing before it)
    bool asserts = false, denies = false; // Passes the topic's claim on / denies it, by default
    bool dispreferred = false, thirdParty = false, ends = false, weak = false, optional = false, stance = false;
    std::vector<std::string> expects;     // What it makes due by default (soft when `optional`)
    std::vector<std::string> answers;     // What it answers ("*": anything)
    std::vector<std::string> fallback;    // When nothing doing it fits, these instead, in order (down to ack)
    std::vector<std::string> insertion;   // May come between it and its answer (request: ask.detail)
    std::vector<std::string> variants;    // react.news.known...
    std::vector<std::string> shape;       // hedge, account, remedy: how a dispreferred one is said
    std::string tone;                     // Its lines' tone unless they say
    std::map<std::string, std::map<std::string, double>> effects;   // "self"|"other" -> axis -> change
    int maxExtend = 0;
};
class Moves
{
public:
    // moves.json's text; `concepts` are settings.json's (an alias's key must be one). False with the problems, and the
    // book left empty.
    bool read(const std::string& text, const std::set<std::string>& concepts, std::string& problem);
    void clear();
    bool empty() const { return moves_.empty(); }
    const std::map<std::string, Move>& all() const { return moves_; }
    const std::map<std::string, std::string>& aliases() const { return aliases_; }
    const std::vector<std::string>& tones() const { return tones_; }
    // A move by its name; with a variant ("react.news.known") its move's.
    const Move* find(const std::string& name) const;
    // Whether a name is a move or a move's variant.
    bool known(const std::string& name) const { return find(name) != nullptr; }
    // The move an answer concept stands for ("" if none).
    std::string alias(const std::string& concept) const;
    bool tone(const std::string& t) const;
    std::string toneOf(const std::string& move) const;    // Its default tone ("even" when it names none)
    // Whether a line doing `answer` answers one doing `said`: its move's `answers` names `said`, a move `said` is a
    // variant of, or "*".
    bool answers(const std::string& answer, const std::string& said) const;
    // Whether a move done meets an owed one: the same, or a variant or kind of it (react.news.known meets react.news).
    static bool meets(const std::string& done, const std::string& owed);
    // The same, by the book: a move of its own is never another's kind (farewell.return is not a farewell, tease.return
    // not a tease), only a variant is (react.news.known).
    bool kindOf(const std::string& done, const std::string& owed) const;
    // The fallbacks walked from a move, breadth first, without repeats (the move itself first).
    std::vector<std::string> chain(const std::string& move) const;
    struct ToneRule
    {
        std::string after;
        std::set<std::string> blocks, bridge;
        int beats = 0;
    };
    const std::vector<ToneRule>& toneRules() const { return toneRules_; }

private:
    std::map<std::string, Move> moves_;
    std::map<std::string, std::string> aliases_;          // concept -> move (or move.variant)
    std::vector<std::string> tones_;
    std::vector<ToneRule> toneRules_;
};

// The odds and lists of settings.json.
struct Settings
{
    double best = .7, next = .2, lower = .1;    // The chance of each score tier (see pick).
    int strict = 10;                            // A priority this high always wins when it holds.
    int namedPerson = 5;                        // What `self.id=...` counts for in a score.
    std::set<std::string> concepts, repeat, alone;
    std::set<std::string> bestOnly;             // Concepts that always take their best tier (answers to players).
    std::set<std::string> ends;                 // Lines that end a chain when said ("Mm.": a shrug is the end of it).
    // Salience (doc 79, 10.3): words a speaker never says while its facts hold (a failing shop never "can't complain"):
    // a line holding any of them, filled in, is passed over for another alternative, and a turn with none left can't be
    // spoken. `when` is the speaker's (self.), `never` lower case.
    struct Salience
    {
        std::vector<Criterion> when;
        std::vector<std::string> never;
    };
    std::vector<Salience> salience;
    // The towns with no dialect of their own, and whose speech is theirs (doc 83, Phase 4: Fenshore speaks as Mirebridge;
    // Saltwell, which spoke as Coldwater, has its own since 2026-11-15): settings.json's "dialect_of". A resident's `from` stays where they grew up.
    std::map<std::string, std::string> dialectOf;
    // Doc 84 §37, the hybrid (the user, 2026-11-17): the engine decides what is said; a written scene says it. A talk
    // between residents on a topic opens with a whole scene that fits wherever one does, plays it as written, and then
    // only its edges are stitched line by line (what its last line owes, a joiner's word and its answer, the goodbyes):
    // no lull's opinion, pivot, goal or change of subject after it, nobody speaking twice running outside a scene, at
    // most `hybridStitch` stitched lines after the body. settings.json's "hybrid" (false where it isn't said: the
    // tests' small libraries, and a library before it, keep the line-by-line engine).
    bool hybrid = false;
    int hybridStitch = 2;
    // Doc 84 §43 (U1): within the hybrid, a mantle-wearer's own content has first claim whenever they are either party
    // ("mantle_first": true). The body: of the scenes that fit, an unheard one of the opener's own mantle is played; else
    // one of the one spoken to's own, cast with them opening it (its `a` turns are theirs, written in their voice), on a
    // topic with no claim to tell; else the shared library. The edges (a greeting, an answer, a joiner's word, the
    // goodbyes): the speaker's own line where one fits as well as any, within the mantle's share (§8.2).
    bool mantleFirst = false;
    // Doc 84 §12 (Phase 10): a dead wolf's mantle rests this many game days before it passes on ("mantle_rest_days"), and
    // the neighbouring pool a dialect with nothing to give borrows from ("mantle_fallback": Frosthollow -> Coldwater...).
    double mantleRestDays = 365;
    std::map<std::string, std::string> mantleFallback;
};

// Doc 84 §7.4, §8.5 (Phase 5): frames.json's goals, a reason to steer a talk: learn a name, ask a favour, mend a quarrel,
// warn of the road, ask after a loss or a hurt, nag a promise let lapse. A goal holds when its `when` does (self: the one
// who'd pursue it, other: the one spoken to); its plan's moves are said in lulls, a step a lull, by lines doing them; a
// step no line fits gives it up. Done, it is kept between the two for `days` (pair.was.goal_<name>) and not pursued again.
struct Goal
{
    std::string name;
    std::vector<Criterion> when;
    std::vector<std::string> plan;
    double days = 7;
};
// Doc 84 §7.4 (Phase 3): Data/Talk/frames.json: the kinds of meeting, worked out when a conversation starts and
// stacked, and the length a talk is meant to have.
struct Frame
{
    std::string name;
    std::vector<Criterion> when;           // Of the opener (self) and the one spoken to (other)
    std::vector<std::string> open;                // Its opening moves (kin.open, greet...)
    double budget = 1, preclose = 1;              // Scales the budget; closes sooner (preclose > 1)
    std::set<std::string> forbid;                 // Moves never made in it
    std::map<std::string, double> tones;          // A tone's weight among candidates (light: 1.4)
    std::string hedge;                            // How a dispreferred answer is shaped (not yet read)
};
// Doc 84 §7.4, §8.11 (Phase 6): frames.json's town moods, read as world.mood (the first whose `when` holds of the town's
// facts): moves muted town-wide, moves boosted, tones weighed.
struct Mood
{
    std::string name;
    std::vector<Criterion> when;
    std::set<std::string> mutes;
    std::map<std::string, double> boosts, tones;
};
// Doc 84 §10.6 (Phase 6): frames.json's accommodation. Each line a speaker says is a step plainer toward one a `plainer`
// rule holds of (self: the speaker, other: the one spoken to), else a step broader toward one a `broader` rule holds of
// who speaks as they do (pair.same_speech: the same dialect); none rises no higher than `none_rises_to`.
struct Accommodation
{
    std::vector<std::vector<Criterion>> broader, plainer;
    std::string noneRisesTo = "light";
    bool on = false;
};
// Doc 84 §7.6, §8.8 (Phase 6): Data/Talk/opinions.json. An issue a resident has a stance on, worked out from their own
// facts: each lean that holds moves it a step for or against (with the reason it gives, in their own words), and events
// drift it a step (the game's, kept in the talk ledger). The topics it comes up on.
struct Issue
{
    std::string name, words;                      // "watch", "the Watch"
    std::set<std::string> topics;
    std::vector<Criterion> who;                   // Who has a view on it at all (none: everyone)
    struct Lean
    {
        std::vector<Criterion> when;
        int side = 0;                             // +1 for, -1 against
        std::string reason;                       // A key of `reasons`
    };
    std::vector<Lean> leans;
    std::map<std::string, std::string> reasons;   // key -> words, first person ("they never caught who robbed me")
    std::map<std::string, int> drift;             // event -> steps (robbed_uncaught: -1)
};
// A like or a dislike (§8.8): stable, held by the wearer (until mantles, drawn from the resident's id, Phase 6).
struct LikeItem
{
    std::string id, kind, words;                  // pastime, food, weather, thing
    std::vector<Criterion> when;                  // Who may hold it (a fisher's nets, a pup's games)
    bool dislike = false;
};
// Doc 84 §7.5 (Phase 6): Data/Talk/temperaments.json: a trait's lean on a move ("temper=short": {"take_offence": 1.8}),
// each 0.3-2.0, their product held to 0.3-2.0.
using Temperaments = std::map<std::string, std::map<std::string, double>>;
struct Budgets
{
    std::map<std::string, std::pair<int, int>> byTalk{{"chatty", {8, 12}}, {"plain", {5, 8}}, {"quiet", {3, 5}}};
    int friendsOrKin = 2, least = 2, most = 14;
};
// Doc 84 §8.4, §8.9, §8.10 (Phase 4): frames.json's "curt", "join" and "thread". A cold or short-tempered closer goes
// without a word this often, and it costs what `effects` says (moves.json's shape: "other", how the one left feels about
// the closer). A third may join (the game's Game::joiners) if idle within `tiles` of both, hearing at `hearing` or more,
// and not in a talk for `rest` seconds; `people` in all at most. A talk broken off before its point was made is kept as
// pair.thread for `days`.
struct Beats
{
    double curtChance = .5;
    std::map<std::string, std::map<std::string, double>> curtEffects;
    int joinPeople = 3;
    double joinTiles = 3, joinHearing = .5, joinRest = 60;
    double threadDays = 2;
};

// A dialogue tree (Docs/Design/83-dialogue-trees.md): a conversation a player has with a resident in the dialogue
// window, choosing what to say. Written in Data/Talk/dialogue/*.tree beside the library and read by the same loader:
//
//   tree shop
//   for role=merchant                          who may speak it (criteria, as a rule's `when`; self is the resident)
//   priority 1                                 over a tree whose `for` overlaps this one's
//   entry trade greet                          the wheel's Trade starts at the node greet
//
//   node greet
//   say #greet#, #address#. Looking or buying? | #greet#. #shop_offer#
//   say[here.shop=asleep priority=10] Mm... shop's shut. Morning.
//   ask player.trade                           the line from a concept the game answers (a fallback to the says)
//   free                                       the player may say something of their own here
//   mood warm                                  the face's mood with the line (Library::faceMoods: doc 83, Phase 5)
//   choice Let me see your wares. [here.shop=open|bare] -> do shop
//   choice Any work going? [here.work>0] locked "Nothing going." -> work
//   each job in here.work max 3
//     choice Ask about: #job.title# (#job.reward#p) -> job          (the job bound: do take job.id ; then taken)
//   choice Goodbye. -> bye
//
// A node's `say` lines are compiled into rules for the concept dlg.<tree>.<node> (the bare say at score 0, a say[...]
// scoring its criteria), so any more specific rule anywhere outranks the tree's words: a trade's, a dialect's, a
// resident's own (`self.id=`). These concepts always take their best tier. A choice's words are the player's default
// line, plain; its criteria hide it when they fail, or with `locked "why"` show it greyed with the reason. Its target
// is a node, `end`, or `do <action> [args] [; then <node>] [; refused <node>]`; the actions are a closed list the game
// performs (Library::treeActions). `#job.x#` in a tree is the bound item's fact (topic.job_x); so `#place.x#` and `#trouble.x#`.
//
// Doc 84 §13.4 (Phase 13): a choice may say the dialogue move the player makes with it, `[does ask.how ...]` among its
// criteria (moves.json's), and a node marked `answers` takes the resident's line from the library's answer to that move
// (Library::answer: the answer pool an NPC's question would get, the wearer's own lines first), its says the floor;
// `answers <move>` names the move itself. `ask game.<what>` asks the game for a line of its own (a matchmaker's pointer,
// a letter remembered: Library::treeAsks). A node named `unclear` is what the resident says to free talk it didn't
// follow (shown over the node the player was at, its choices again); it is reached by the game, not by a choice. No
// node may show more than nine choices: the loader counts each choice (an `each` as its max), choices that can never
// show together (their criteria disjoint) as one, and refuses a node over nine.
struct TreeChoice
{
    std::string words;                    // The player's default line (a template: plain words, the bound item's facts).
    std::vector<Criterion> when;
    std::string locked;                   // Shown greyed with this reason when `when` fails ("" hides it).
    std::string go;                       // A node, or "end"; "" when it does an action.
    std::string action;                   // shop, free, end, take, deliver, give_back, open...
    std::vector<std::string> args;
    std::string then, refused;            // Where an action goes when done, or refused ("" ends / stays).
    std::string each, item;               // An `each` choice: the list ("here.work") and the item's name ("job").
    int max = 0;
    int line = 0;
    std::string does;                     // The move the player makes with it (doc 84 §13.4.2): "ask.how"; "" none.
};
struct TreeNode
{
    std::string id, concept;              // "greet", "dlg.shop.greet"
    std::string ask;                      // A concept the game answers first (player.trade); "" for none.
    std::string mood;
    std::string answers;                  // `answers`: "*" (the move of the choice that led here) or a move; "" none.
    std::string invite;                   // A graft node's invitation (invite <place> <hour> <days>), the game's to check.
    bool free = false;
    std::vector<TreeChoice> choices;
    int line = 0;
};
struct Tree
{
    std::string id, file;
    std::vector<Criterion> who;           // `for`
    int priority = 0;
    std::map<std::string, std::string> entries;   // entry -> node
    std::map<std::string, TreeNode> nodes;
    std::vector<std::string> order;       // Nodes as written.
};
// A choice as the window shows it: its number, the player's default words, and whether it is locked (with why).
struct ShownChoice
{
    int n = 0;
    std::string text, why;
    bool locked = false;
    std::size_t index = 0;                // In the node's choices.
    int item = -1;                        // An `each` choice's item.
    Facts bound;                          // That item's facts (topic.job_title...).
};

// The facts there are, and the values each may take.
struct FactKind
{
    enum class Type : std::uint8_t { Words, Text, Number, List } type = Type::Words;   // List: words, any number of them
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
    // A grammar replaced whole (a dialect from the database, a person's own layer): its text must declare it
    // (`grammar <name>`). False with the problems, and the old one kept.
    bool setGrammar(const std::string& name, const std::string& text, std::string& problem);
    // A person's own rules (their character table, doc 62 phase 6; since doc 84 Phase 9 a per-NPC override, the DM's or
    // Atlas's, never passed on), replacing any they had: each rule's ID gets "<npc>." before it and `self.id=<npc>` among
    // its criteria, so it can only ever be theirs. Rules only, no scenes. Kept in that one wolf's own bucket, beside the
    // shared library (no copy of it: defect 16), and consulted only when they speak. False with the problems, and their
    // old rules kept. An empty text takes them away.
    bool setCharacter(const std::string& npc, const std::string& text, std::string& problem);
    // The name a person's own grammar layer has ("p_" and their ID, - as _).
    static std::string personalLayer(const std::string& npc);
    // A mantle's mark drawn again for a new wearer (doc 84 §12.4, Phase 10): a value from the bank's `mark.<symbol>`
    // (a dialect's mark bank, Phase 15's), else another of the mantle's own alternatives for the habit than the one it is
    // seeded to; "" when there is nothing else to draw (one alternative, no bank, or the mantle isn't loaded).
    std::string redrawHabit(const std::string& mantle, const std::string& symbol, const std::string& wearer,
                            const std::string& bank) const;

    // Mantles (doc 84 §9.4, Phase 9). Each is read (the .mantle syntax), checked against the library as a file of its own
    // would be (facts, symbols, moves, 160 characters, the window's trees, a graft's nodes and its ways out), and compiled
    // to rules and scenes kept in the mantle's own bucket (never the shared one: a pick looks at the shared lines and the
    // speaker's own), with its words as the grammar layer m_<id>, between the wearer's own layer and their dialect. Its
    // rules carry `self.mantle=<id>` and count as a named person's (+5). A speaker wears one when their facts say
    // `mantle=<id>`.
    struct MantleRefusal
    {
        std::string id, why;
    };
    // Every mantle at once (start, and voice.sync): those that load replace all that were loaded; one that fails is left
    // out and named in `refused` (its wearer speaks from the library: a bad row never stops a server). Only `ready` ones
    // are taken; a draft or a retired one is refused as such. The number loaded.
    std::size_t setMantles(const std::vector<MantleText>& texts, std::vector<MantleRefusal>& refused);
    // One mantle, loaded or loaded again (mantle.sync's hot reload): its bucket and its layer swapped whole. False with the
    // problems, and the revision loaded before kept. `id` gets the mantle's id. A mantle not `ready` is refused (and the
    // one loaded, if any, kept), unless `anyState` (talk_check, which checks drafts).
    bool setMantle(const MantleText& text, std::string& problem, std::string* id = nullptr, bool anyState = false);
    // A mantle read and checked as setMantle would, kept nowhere; `out` gets it as read and worked out. For talk_check
    // --mantle and tools/mantle_store.py's checks on every save (G1).
    bool checkMantle(const MantleText& text, Mantle& out, std::string& problem) const;
    void dropMantle(const std::string& id);
    void dropMantles();
    const Mantle* mantle(const std::string& id) const;   // A loaded one (null if none)
    // §43 (U1): the director's topics a loaded mantle's own whole scenes and stories are written for (talk.<topic>), so
    // a talk with its wearer in it leans toward what the mantle can say.
    std::vector<std::string> mantleTopics(const std::string& id) const;
    std::vector<std::string> mantleIds() const;
    std::size_t mantleEntries() const;                   // The rules and scenes of every loaded mantle
    // The rules and scenes a mantle (or an NPC's own lines: "@<npc>") compiled to, for talk_check.
    std::vector<const Entry*> ownEntries(const std::string& owner) const;
    // A wearer's card over their worked-out facts (doc 84 §8.8, §9.5): its opinions over the stances worked out (op.*,
    // op_strength.*, op_basis "mantle"), its likes over the drawn ones (one of each kind it names), its dislikes, fears
    // and hopes. Traits and the quirk are the caller's (Game::talkSpeaker: never honesty, which a placed resident keeps).
    void wear(Facts& person, const std::string& mantle) const;

    // Dialogue trees (doc 83). One .tree file's text, added to what is loaded (its nodes' lines become rules): false
    // with the problems, and nothing of it kept. Checked: every fact, symbol, node, action and list known; every node
    // reachable from an entry (or `bye`); no `for` overlapping another tree's on the same entry without a priority.
    bool parseTree(const std::string& text, const std::string& file, std::string& problem);
    const std::map<std::string, Tree>& trees() const { return trees_; }
    const Tree* tree(const std::string& id) const;
    // The tree for an entry ("trade", "talk") whose `for` holds of the resident (q.a): the highest priority, then the
    // most criteria. Null if none.
    const Tree* treeFor(const std::string& entry, const Query& q) const;
    // What a node offers now, numbered from 1: hidden choices left out, locked ones greyed, `each` choices one per item
    // (`items` gives a list's items as facts, at most `max`), at most nine in all.
    std::vector<ShownChoice> choices(const TreeNode& node, const Query& q, std::uint64_t seed,
                                     const std::function<std::vector<Facts>(const std::string&)>& items) const;
    bool holdsAll(const std::vector<Criterion>& when, const Query& q, int speaker = 0) const;
    // The actions a tree may do, and the lists an `each` may go through.
    static const std::set<std::string>& treeActions();
    static const std::set<std::string>& treeLists();
    // The game's own lines a node may ask for (`ask game.match`...: doc 84 §13.4.6, Phase 13).
    static const std::set<std::string>& treeAsks();
    // The most choices a node could show at once (doc 84 §13.4.3): each choice one (an `each` its max), choices whose
    // criteria are disjoint counted once. The loader refuses a node over nine.
    static std::size_t mostShown(const TreeNode& node);
    // A mantle's grafts as window nodes (doc 84 §13.4, Phase 13): the choices a wearer adds to a tree's node (`node` the
    // tree's, its graft choices with `from` empty), or the graft node itself ("@<mantle>.<name>": its own choices and
    // concept). Targets are the tree's nodes, bye, end, or "@<mantle>.<name>". False if the mantle holds no such graft.
    bool graftChoices(const std::string& mantle, const std::string& tree, const std::string& node,
                      std::vector<TreeChoice>& out) const;
    bool graftNode(const std::string& mantle, const std::string& tree, const std::string& name, TreeNode& out) const;
    // Criteria as written ("here.season=spring|autumn self.stage=adult"), parsed as a rule's `when`; those that don't
    // parse are left out.
    std::vector<Criterion> criteriaOf(const std::string& text) const;
    // Whether a rule or scene is one of the speaker's own (their mantle's, or their own lines'), by its id: what the
    // window tells the wearer's words from the tree's by (doc 84 §13.4.1, W1).
    bool ownLine(const std::string& entry, const Query& q, int speaker = 0) const;
    // The face's moods a node's `mood` may name (doc 83, Phase 5; the client's Client/src/game/face.ts knows the same).
    static const std::set<std::string>& faceMoods();

    std::size_t size() const { return entries_.size(); }
    const std::vector<Entry>& entries() const { return entries_; }
    const Entry* find(const std::string& id) const;
    bool has(const std::string& concept) const { return byConcept_.count(concept) > 0; }
    // Moves (doc 84 §7.2, Phase 2): the book (empty for a library from before them), and the entries that make a move
    // (a rule's own, written or by its concept's alias; a scene's first turn's), by move and by each of its kinds
    // (react.news.known is under react.news too). Null if none.
    const Moves& moves() const { return moves_; }
    const std::vector<std::size_t>* byMove(const std::string& move) const;
    // What `binds x=<slot>` may name: self.<role>, other.<role> (spouse, child, mother, father, neighbour, shopkeeper,
    // priest, leader) and topic.subject.
    static const std::set<std::string>& bindSlots();
    // The dead-end proof (doc 84 §7.2), run when the whole library has loaded: every concept a line hands on to has a
    // plain rule of its own that holds for a speaker with every fact empty, and every move a line can owe has one,
    // walking its fallbacks down to ack. The problems, one a line ("" if none).
    std::string deadEnds() const;
    // The moves a line of `e` makes as its turn `turn` is spoken, and the rest of SaidLine's moves (owes, tone...), for
    // a query (`q`: the topic's claim kind, for what it asserts). `before`: the moves of the line said before it.
    void describe(const Entry& e, std::size_t turn, const Query& q, const std::vector<std::string>& before, SaidLine& line) const;
    // The same for a line whose moves are already set (an authored line's): its tone, what its moves expect (unless
    // `expect` is false: the last line of a whole written exchange), what it answers of `before`, what it asserts on the
    // topic's claim. Nothing without moves.json.
    void describeMoves(SaidLine& line, const std::vector<std::string>& before, const Facts& facts, bool expect = true) const;
    const Settings& settings() const { return settings_; }
    void setHybrid(bool on) { settings_.hybrid = on; }   // (§37: the tests of the line-by-line engine turn it off.)
    void setMantleFirst(bool on) { settings_.mantleFirst = on; }   // (§43: the tests compare with it and without.)
    // The dialect (a grammar layer) of one who grew up in `from`: its own, or settings.json's dialect_of for a town with
    // none of its own (Fenshore: mirebridge).
    std::string dialectOf(const std::string& from) const;
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
    // A whole conversation over one query (doc 84 §8.1: begin, then next until it ends; what each beat remembers is
    // known to the next). With moves.json, the turn engine (§8.2-8.4): what each line owes is answered, best fit first
    // and tiers within it, then the talk closes when its budget is spent or nothing is left to say. A library without
    // moves.json talks as it did before Phase 3: each rule's `then` handed on to the other speaker's best rule for that
    // concept (its best tier), up to 8 beats, a shrug ending it.
    Said converse(const Query& q, std::uint64_t seed, const std::function<bool(const std::string&)>& heard,
                  const std::deque<std::string>& recent, Trace* trace = nullptr) const;
    // The turn engine (doc 84 §8.1-8.4, Phase 3). begin: the conversation's first line, the opener for q.concept as
    // pick chooses it (a scene's later turns are queued), with the conversation's frames and budget worked out; c.seed
    // is the caller's. next: the line after, or nothing once it has ended. `facts` gives the query for a speaker (0: a,
    // 1: b) speaking to another (-1: no one), a and b as begin had them; the engine adds its own conv.* facts.
    //
    // Phase 4 (§8.2: beats chosen when they are due, from fresh facts): `facts(speaker, to)` is asked again for every line,
    // so it may change between them. For the opener and the one first spoken to (0 and 1), a and b are as begin had
    // them; once a third has joined, a query between the third and another has the speaker as a and the one spoken to as
    // b. A scene's queued turn is checked again when it comes due (Valve's rule: its criteria and its line's, not its
    // limits) and the rest of the scene dropped if it no longer holds. What is owed in react.news is answered by the
    // known variant (react.news.known: "I heard.") when the one answering already knew (their `knows`), and never by it
    // when they didn't; never "Never!" (react.news.new) from one who knew.
    //
    // §43 (U1): `turned`, where given, is the same talk asked the other way round (the one spoken to as a, with their view
    // of the other: the game's query for them): where the one spoken to wears a mantle and the opener's own has no scene
    // that fits, one of theirs may carry the talk, cast with them opening it (settings.json's mantle_first).
    std::optional<SaidLine> begin(Conversation& c, const Query& q, const Freshness& fresh, Trace* trace = nullptr,
                                  const Query* turned = nullptr) const;
    std::optional<SaidLine> next(Conversation& c, const std::function<Query(int speaker, int to)>& facts,
                                 const Freshness& fresh, Trace* trace = nullptr) const;
    // A conversation begun with lines already written (the game's authored exchange, when no library line fits): set up
    // as begin does (frames, budget), its lines said as they are, then `next` carries on (answers owed, the closing).
    std::optional<SaidLine> beginWith(Conversation& c, const Query& q, Said written, Trace* trace = nullptr) const;
    // A third voice (§8.9): `id` joins with a line doing one of `moves` (join.defend, join.add...), said to the one who
    // spoke last; `q` has the joiner as a and that one as b. Nothing (and nobody joins) unless such a line fits, the
    // talk is in its body with room left, and only two are in it. What the joining line owes, the one it is said to owes;
    // it comes before whatever was owed before it (the joiner cuts in).
    // `stake` (why they join: named, kin, friend, goods, witness, friends) is conv.stake for the joining line's criteria,
    // and the conversation's once they're in.
    std::optional<SaidLine> join(Conversation& c, const std::string& id, const std::string& stake,
                                 const std::vector<std::string>& moves, const Query& q, const Freshness& fresh,
                                 Trace* trace = nullptr) const;
    // An interruption (§8.10: rain starting outdoors, a fight in sight, one of them called away): conv.interrupt is
    // `why`, and `who` says a line that breaks off (a `break` move line) to whoever they spoke with; the talk ends there.
    // Nothing said if no such line fits (the talk ends all the same: "break").
    std::optional<SaidLine> interrupt(Conversation& c, const std::string& why, int who,
                                      const std::function<Query(int speaker, int to)>& facts, const Freshness& fresh,
                                      Trace* trace = nullptr) const;
    // Who `who` is talking with: the one they last spoke to, else the last other speaker, else the other of the two.
    static int partner(const Conversation& c, int who);
    // What `speaker` says to what is owed (§8.2), rendered, or empty if nothing fits: of the rules that hold, the best
    // fit only (its handed-on concept's own rules, or the exact variant owed: 3; another line doing an owed move: 1;
    // only a rule with no concept is found by its move alone), then tiers by score within it (70/20/10: no longer the
    // best tier only), what no listener has heard first, then weight (times the frames' tone weights). Never an entry
    // or words said in this talk, a move the frames forbid, a tone the tone rules block, a tease to one mourning, or a
    // scene. Its line's `fit` and `best` say how well it answered and the best that held.
    // `bestHeld`: the best fit of any line that held, even when nothing could be said.
    Said answer(const Conversation& c, const Conversation::Open& owed, const Query& q, int speaker, const Freshness& fresh,
                Trace* trace = nullptr, int* bestHeld = nullptr) const;
    // The conversation said so far, as converse returns it.
    static Said whole(const Conversation& c);
    // frames.json (doc 84 §7.4): the frames, the budgets, and the chance an invitation (`invites`) is taken.
    const std::vector<Frame>& frames() const { return frames_; }
    const Budgets& budgets() const { return budgets_; }
    const Beats& beats() const { return curt_; }
    double inviteTaken() const { return inviteTaken_; }
    // Phase 6 (§7.4-7.6, §8.8, §10.6): the town moods, accommodation, the issues and the likes, the temperaments.
    const std::vector<Mood>& moods() const { return moods_; }
    const Accommodation& accommodation() const { return accent_; }
    const std::vector<Issue>& issues() const { return issues_; }
    const Issue* issue(const std::string& name) const;
    const std::vector<LikeItem>& likeItems() const { return likes_; }
    const Temperaments& temperaments() const { return temper_; }
    // A resident's stances (§8.8) as their facts: op.<issue> (for, against, mixed), op_why.<issue> (the reason, in their
    // own words), op_strength.<issue> (0-3), from their facts (`person`: theirs as talkSpeaker gives them, without
    // namespace) and the steps events drifted each (`drift`: issue -> steps, and the event that last moved it).
    // op_basis.<issue> says what it stood on (the leans that held, the drift): a stance that changes on the same basis
    // has wandered (the scorecard's T4); one whose basis changed was moved by the wolf's life.
    void stances(Facts& person, const std::map<std::string, std::pair<int, std::string>>& drift) const;
    // A resident's likes and dislikes (§8.8), stable for good: drawn from their id among the items whose `when` holds of
    // them, one of each kind (likes) and two dislikes: likes, dislikes (list facts), and each kind's words.
    void likes(Facts& person) const;
    // A speaker's lean on a move (§7.5): the product of their traits' multipliers for it (or a kind of it), 0.3-2.0.
    double lean(const Speaker& s, const std::string& move) const;
    // The town's mood (§7.4): the first of frames.json's moods whose `when` holds ("" none), as world.mood.
    std::string moodOf(const Query& q) const;
    // Accommodation (§10.6): each speaker's accent toward the other for this query, as their `speaks` (read before their
    // accent when a line is said); the town's mood as world.mood. Both speakers', since one query may serve both.
    void accommodate(Query& q) const;
    // frames.json's goals (Phase 5), in their order; the topic graph's steps from a topic (topics.json's `next`, its
    // criteria parsed against facts.json when the library loads).
    const std::vector<Goal>& goals() const { return goals_; }
    struct Step
    {
        std::string to;
        std::vector<Criterion> when;
    };
    const std::vector<Step>& steps(const std::string& topic) const;
    // Whether a fact's key is the talk ledger's (pair.last_talk, pair.talked.*, x.said.*...): a line testing one is a
    // callback (§8.7).
    static bool ledgerFact(const std::string& key);
    // The frames that hold for a query (a opens, to b), in frames.json's order; the budget drawn for them (§8.4: by the
    // two speakers' talk, the smaller; +2 for friends and kin; scaled by the frames; closing sooner by their preclose).
    std::vector<std::string> framesFor(const Query& q) const;
    int budgetFor(const Query& q, const std::vector<std::string>& frames, std::uint64_t seed) const;
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

    // Doc 84 §12.3.5 (Phase 11): a dialect's distinctive words (its grammars' phrases of two words or more, or coined
    // with a hyphen, that no other grammar says: tools/dialect_check.py's lexicon), and whether a text says one. A mantle
    // lent from a neighbouring pool speaks its own words to its wearer only where they aren't its dialect's.
    const std::vector<std::string>& dialectWords(const std::string& dialect) const;
    bool saysDialect(const std::string& dialect, const std::string& text) const;
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
    bool muted(const std::string& said, const Query& q, int speaker) const;   // (Settings::salience.)
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
                 const std::deque<std::string>& recent, int speaker, bool chained, Trace* trace, bool scenesFirst = false,
                 bool ownOnly = false) const;
    // §43: whether an entry is one of the speaker's own (their mantle's, or their own lines').
    bool ownEntry(const Entry& e, const Query& q, int speaker) const;
    // §37: the topic's facts a rendered entry says (its words, or a flag it tests: topic.bare=yes), the director's.
    std::set<std::string> factsSaid(const Entry& e, const Said& said, const Query& q) const;
    bool checkGrammars(std::vector<std::string>& errors) const;
    void reindex();                                         // byConcept_, byId_ and byMove_ from entries_.
    void indexMoves(std::size_t i);
    // The moves an entry is found by (its first turn's, each under every kind it is of).
    std::set<std::string> movesUnder(Entry& e) const;
    // A grammar file's lines read into `merged` (parseGrammar's, and a mantle's words): the texts each symbol line said,
    // to be checked once the layers are in place.
    struct GrammarLine
    {
        std::string grammar, symbol, where;
        std::vector<std::string> texts;
    };
    bool readGrammarText(const std::string& text, const std::string& file, std::map<std::string, Grammar>& merged,
                         std::vector<GrammarLine>& lines, std::vector<std::string>& errors) const;
    // Doc 84 Phase 9: lines that belong to one owner, a mantle ("m:<id>") or one wolf's own ("p:<npc>"), kept apart from
    // the shared library and swapped whole.
    struct Owned
    {
        std::string owner;                                  // The mantle's id, or "@<npc>"
        std::vector<Entry> entries;
        std::map<std::string, std::vector<std::size_t>> byConcept, byMove;
        std::map<std::string, std::size_t> byId;
        Grammar grammar;                                    // m_<id>: the mantle's words
        std::shared_ptr<const Mantle> card;
    };
    std::map<std::string, std::shared_ptr<const Owned>> owned_;
    // The buckets a speaker's lines may come from: their mantle's and their own (by their facts' mantle and id).
    std::vector<const Owned*> ownersOf(const Query& q, int speaker) const;
    // Every entry for a concept: the shared library's, then the speaker's own buckets'.
    std::vector<const Entry*> forConcept(const std::string& concept, const Query& q, int speaker) const;
    const Grammar* layerOf(const std::string& name) const;   // grammars_, or a mantle's m_<id>
    bool compileMantle(Mantle& m, Owned& out, std::vector<std::string>& errors) const;
    bool loadMantle(const MantleText& text, Mantle& card, Owned& out, std::string& problem, bool anyState) const;
    static void indexOwned(Owned& o, const Library& lib);
    // Parsing into a vector of one's own (a mantle's items, an NPC's rules) instead of the shared library; and the concepts
    // only a mantle's grafts have (graft.<tree>.<mantle>.<node>). Set only for the length of one parse.
    mutable std::vector<Entry>* parseInto_ = nullptr;
    mutable bool mantleParse_ = false;
    // A mantle's own words while its lines are checked (checkText, longestSymbol): its symbols beside plain's.
    mutable const Grammar* checking_ = nullptr;
    // Whether each shared entry holds and speaks with nothing known (Library::unconditioned), worked out as asked: a
    // mantle's dead-end check (what its lines owe has an answer) asks it again for every mantle.
    mutable std::vector<char> plain_;
    bool parseOwn(const std::string& text, const std::string& file, std::vector<Entry>& out, std::string& problem) const;
    bool readMoves(const std::string& text, std::string& problem);
    bool unconditioned(const Entry& e) const;               // (deadEnds: holds and speaks with every fact empty.)
    bool bindThird(const Entry& e, const Query& q, int speaker, Query& out) const;   // (binds: q with x.* filled.)
    bool fitsWith(const Entry& e, const Query& q, int speaker) const;            // (fits, x bound already.)
    Said renderWith(const Entry& e, const Query& q, std::uint64_t seed, Trace* trace, int speaker) const;
    std::vector<std::string> answersOf(const std::vector<std::string>& before, const std::vector<std::string>& moves) const;
    bool readSchema(const std::string& text, std::string& problem);
    bool readSettings(const std::string& text, std::string& problem);
    bool readFrames(const std::string& text, std::string& problem);
    // The turn engine's own (Phase 3).
    std::optional<SaidLine> legacyNext(Conversation& c, const std::function<Query(int, int)>& facts, const Freshness& fresh,
                                       Trace* trace) const;
    void take(Conversation& c, Said said) const;                    // An entry's lines queued, its effects kept
    SaidLine sayNext(Conversation& c) const;                          // The next queued line said
    // The query for `speaker` speaking to `to` with the conversation's own facts, and which of its a and b speaks.
    Query beatQuery(const Conversation& c, const std::function<Query(int, int)>& facts, int speaker, int to, int& side) const;
    Query beatQuery(const Conversation& c, const std::function<Query(int, int)>& facts, int speaker) const;
    // A line answered for a query whose speaker is `side`, placed in the conversation: who `speaker`, to `to`.
    static void place(Said& s, int speaker, int to);
    // Whether a line still holds when its turn comes due (Valve's rule): its entry's criteria and its line's.
    bool stillHolds(const SaidLine& line, const Query& q) const;
    // A scene with a turn untrue to what its speaker knew (§8.6: "I'd heard" from one who didn't, "Never!" from one who did).
    bool untrueScene(const Entry& e, const Query& q) const;
    // Opening moves said in the opener's breath (the frames' greeting, a return to a thread), before an opener.
    void preface(Conversation& c, Said& said, const Query& q, const Freshness& fresh, Trace* trace) const;
    void setUp(Conversation& c, const Query& q) const;               // (begin's: people, topic, frames, budget.)
    std::string toneOf(const Entry& e) const;                         // A rule's tone ("": no move, no tone)
    bool toneBlocked(const Conversation& c, const std::string& tone) const;
    bool forbidden(const Conversation& c, const Entry& e, const Query& q, int speaker) const;
    void end(Conversation& c, const std::string& why, Trace* trace) const;
    std::vector<Frame> frames_;
    Budgets budgets_;
    Beats curt_;
    double inviteTaken_ = .5;
    std::vector<Goal> goals_;
    std::vector<Mood> moods_;
    Accommodation accent_;
    std::vector<Issue> issues_;
    std::vector<LikeItem> likes_;
    Temperaments temper_;
    // temper_ by axis, then value (found without building a key: lean is asked for every candidate, §16 B1).
    std::map<std::string, std::map<std::string, const std::map<std::string, double>*, std::less<>>, std::less<>> temperBy_;
    std::map<std::string, std::vector<Step>> steps_;
    bool readOpinions(const std::string& text, std::string& problem);
    bool readTemperaments(const std::string& text, std::string& problem);
    // Phase 6: a stance's facts for the issue a talk is on (conv.issue): each speaker's own (stance, stance_why,
    // stance_strength) and whether the two agree (conv.agree: yes, no, half), and the pair's earlier agreement on it.
    void stanceFacts(const Conversation& c, Query& q) const;
    // Whether an entry's move fits the stances (§8.8): agree only where the two agree, disagree where they differ.
    bool stanceFits(const Entry& e, const Query& q) const;
    // Whether an entry would tell a listener what they were told before (§8.6, §8.12.4: Freshness::told).
    bool toldAlready(const Conversation& c, const Entry& e, const Query& q, int speaker, const Freshness& fresh) const;
    // Whether a rendered line repeats another speaker's distinctive words here within the hour (§8.12.3).
    static bool collides(const Said& s, const Query& q, const Freshness& fresh);
    // A mood's mutes and boosts for an entry (world.mood).
    bool moodMuted(const Entry& e, const Query& q) const;
    double moodWeight(const Entry& e, const Query& q, const std::string& tone) const;
    // Phase 6's lull step: an opinion on the issue the talk's topic raises (§8.8).
    std::optional<SaidLine> opinion(Conversation& c, const Query& now, int who, const Freshness& fresh, Trace* trace) const;
    bool readSteps(std::string& problem);                             // (topics.json's `next`, parsed: Phase 5.)
    static void deepen(Conversation& c, const Query& q);              // (A topic's aspect, by the pair's past talk of it.)
    // The topic a talk turned to over a query's own (its topic.* facts, who knew its claim, conv.topic).
    static void overlay(const Conversation& c, Query& q);
    // pickOne's choice made before anything is rendered (§16: lazily): only the chosen entry is rendered, and one whose
    // turn can't be spoken is dropped and the choice made again. (A talk's turn to a new topic, mid-talk: Phase 5.)
    Said pickLazy(const Query& q, std::uint64_t seed, const std::function<bool(const std::string&)>& heard,
                  const std::deque<std::string>& recent, int speaker, Trace* trace,
                  const std::function<bool(const Said&)>& refuse = {}) const;
    // Phase 5's lull: a goal's next step, or a turn to an agenda item, or nothing (then the talk closes).
    std::optional<SaidLine> lull(Conversation& c, const std::function<Query(int, int)>& facts, const Freshness& fresh,
                                 Trace* trace) const;

    std::vector<Entry> entries_;
    std::map<std::string, std::vector<std::size_t>> byConcept_;
    std::map<std::string, std::vector<std::size_t>> byMove_;
    Moves moves_;
    std::map<std::string, FactKind> conv_, x_;               // facts.json's "conv" and "x" (the engine's own keys)
    std::map<std::string, std::size_t> byId_;
    std::map<std::string, std::set<std::string>> groups_;    // Region groups: name -> regions.
    std::map<std::string, Grammar> grammars_;
    std::map<std::string, FactKind> person_, shared_;       // shared_: by full key ("here.place").
    std::map<std::string, std::string> aliases_;             // "place" -> "here.place"
    Settings settings_;
    std::map<std::string, Tree> trees_;
};

// A job title or work label in the talk's job categories ("Master Smith of Westmarch" -> "smith").
std::string jobCategory(const std::string& title, const std::string& label, const std::string& role, int age);
} // namespace ratw::talk
