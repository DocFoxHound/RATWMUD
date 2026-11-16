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
    int asserts = -1, denies = -1, gist = -1;
    std::string assertsClaim, deniesClaim;
    std::map<std::string, std::string> binds;
    bool aliasMoves = false;              // Its moves are its concept's alias, not written
    std::string owner;
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
};
struct TreeNode
{
    std::string id, concept;              // "greet", "dlg.shop.greet"
    std::string ask;                      // A concept the game answers first (player.trade); "" for none.
    std::string mood;
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
    // A grammar replaced whole (a dialect from the database, a person's own layer): its text must declare it
    // (`grammar <name>`). False with the problems, and the old one kept.
    bool setGrammar(const std::string& name, const std::string& text, std::string& problem);
    // A person's own rules (their character table, doc 62 phase 6), replacing any they had: each rule's ID gets
    // "<npc>." before it and `self.id=<npc>` among its criteria, so it can only ever be theirs. Rules only, no scenes.
    // False with the problems, and their old rules kept.
    bool setCharacter(const std::string& npc, const std::string& text, std::string& problem);
    // The name a person's own grammar layer has ("p_" and their ID, - as _).
    static std::string personalLayer(const std::string& npc);

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
                 const std::deque<std::string>& recent, int speaker, bool chained, Trace* trace) const;
    bool checkGrammars(std::vector<std::string>& errors) const;
    void reindex();                                         // byConcept_, byId_ and byMove_ from entries_.
    void indexMoves(std::size_t i);
    bool readMoves(const std::string& text, std::string& problem);
    bool unconditioned(const Entry& e) const;               // (deadEnds: holds and speaks with every fact empty.)
    bool bindThird(const Entry& e, const Query& q, int speaker, Query& out) const;   // (binds: q with x.* filled.)
    bool fitsWith(const Entry& e, const Query& q, int speaker) const;            // (fits, x bound already.)
    Said renderWith(const Entry& e, const Query& q, std::uint64_t seed, Trace* trace, int speaker) const;
    std::vector<std::string> answersOf(const std::vector<std::string>& before, const std::vector<std::string>& moves) const;
    bool readSchema(const std::string& text, std::string& problem);
    bool readSettings(const std::string& text, std::string& problem);

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
