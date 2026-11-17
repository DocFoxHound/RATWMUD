#pragma once
// Mantles (Docs/Design/84-living-conversations.md §7.9, §9, §11, §17; Phase 9): one wolf's way of talking, written
// without their name, worn by one living wolf at a time and passed on at death. Kept as rows in the world database
// (live.mantles, live.mantle_lines: the user's answer Q2), written, exported and checked in the `.mantle` syntax:
//
//   mantle cw017                       the header: the card (live.mantles' columns)
//   revision 4
//   state ready
//   dialect coldwater
//   accent broad light
//   tier full
//   traits temper=calm talk=plain warmth=warm
//   likes still_water cold_mornings
//   ...
//   words                              the mantle's own grammar layer, m_<id> (one row a symbol line)
//   oath! = Deep and dark | Glass and stone
//   opinion road against 2 : The High Road? I'd not send my own soap up it.
//   grown                              a stage section: its items hold for that stage only
//   line @carry request [other.job=carter] : {request=carry} Will you take a box of my candles up the road?
//   rule @name ... / scene name ...    a whole rule or scene in the .talk syntax (a block, to the next blank line)
//   story first_swim [self.from=here.region]
//     : #mum!# threw me off the jetty when I was a minnow.
//     ask : Do you swim, #other.called#?
//     b : Not since I was a pup.
//   dlg shop.greet : #greet#. Salt, candles, string, soap.
//   graft resident.about               choices and nodes added to a window tree for this wearer only
//   choice You swim in the lake? -> swim
//   node swim
//   say Every morning till the ice says no.
//   choice Goodbye. -> bye
//
// Every item has a key, stable inside the mantle (§7.9): a line's is <section>.<move>.<name>, a symbol's
// words.<symbol>[.<name>], an opinion's opinion.<issue>.<name>, a story's story.<name>, a scene's <section>.scene.<name>,
// a window line's dlg.<tree>.<node>.<name>, a graft's graft.<tree>.<node>[.<name>]. `@name` right after an item's keyword
// (or first on a symbol's line) names it; unnamed items take the next free number. The game reads only `ready`
// mantles; a draft is invisible to it.
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace ratw::talk
{
// One item: a row of live.mantle_lines.
struct MantleItem
{
    std::string key;
    std::string section = "any";          // any, grown, young, adolescent, old
    std::string kind;                     // symbol, opinion, line, rule, scene, story, dlg, graft
    std::string move;                     // A line's first move; a dlg's or graft's tree.node; "" otherwise
    std::string body;                     // Its text as written (one line, or a block's lines)
    std::string name;                     // Its @name, or "" (numbered)
    int position = 0;                     // Its order within its section
    int line = 0;                         // Its first line in the text read
};

// A graft's shape (§7.9): choices added to a tree's node for this wearer, and nodes of the mantle's own. The window shows
// them (Phase 13); the loader checks that every node is reached and every way out leads to a node of the tree, bye or end.
struct MantleGraft
{
    std::string key, tree, node;          // graft.resident.about; resident; about
    struct Choice
    {
        std::string words, when, to;      // The player's words, its criteria as written, a node (own or the tree's), bye, end
        std::string from;                 // "" for the base node, else the graft node it is shown at
    };
    struct Node
    {
        std::string name, concept, mood, invite;   // concept: graft.<tree>.<mantle>.<name> (its say lines' rules)
        bool free = false;
    };
    std::vector<Choice> choices;
    std::vector<Node> nodes;
};

struct Mantle
{
    std::string id, dialect = "plain", tier = "standard", state = "draft";
    int revision = 0, format = 1;
    std::vector<std::string> accents{"light"};
    std::string title, writtenFor, writtenBy, quirk, fits, prefers, persona;
    std::map<std::string, std::string> traits, styles;
    std::vector<std::string> likes, dislikes, fears, hopes, marks;
    std::vector<std::pair<std::string, double>> topics;   // A favourite topic (+) or one avoided (-)
    std::vector<MantleItem> items;
    // Worked out when it is loaded: the stances its unconditioned opinions hold (issue -> side, strength), its grafts.
    struct Stance
    {
        std::string side;
        int strength = 0;
    };
    std::map<std::string, Stance> stances;
    std::vector<MantleGraft> grafts;
    std::size_t entries = 0;              // Rules and scenes it compiled to
};

// A mantle's text, from a file (a draft, the export, the test fixture) or assembled from its rows.
struct MantleText
{
    std::string id;                       // As the caller knows it (the file's name, the row's id): "" to take the text's
    std::string text;
    std::string where;                    // For messages: a file's path, or "db:<id>"
};

// The .mantle syntax read into a Mantle: the header, and the items with their keys, sections and positions. Syntax only
// (Library::setMantle checks the rest against the library). False with the problems, one a line ("<where>:<line>: ...").
bool readMantle(const std::string& text, const std::string& where, Mantle& out, std::vector<std::string>& errors);
// The grammar layer of a mantle's own words: "m_" and its id.
std::string mantleLayer(const std::string& id);
// The sections in their order (any, grown, young, adolescent, old), for assembling a mantle's text from its rows.
const std::vector<std::string>& mantleSections();
// A mantle's text from its card and its items as rows hold them (the header as the export writes it, then the items by
// section and position): what Game reads from live.mantles and live.mantle_lines.
std::string mantleText(const Mantle& card, std::vector<MantleItem> items);
} // namespace ratw::talk
