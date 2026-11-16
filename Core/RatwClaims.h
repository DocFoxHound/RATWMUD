#pragma once
// Claims and who says them (Docs/Design/84 §7.7, Phase 1): what kind of thing a belief is (Data/Talk/claims.json), and
// its words from each point of view. A belief (struct Belief, RatwRoads.h) keeps the free words it always had (the Watch
// and World::weigh read them) and, beside them, its kind, the second party it names (the object: a theft's victim) and
// the place it is about. A claim is spoken only in words this table gives it and that can be filled: never a raw id.
//
// The table is the process's: the talk library's loader reads claims.json (Library::load) and sets it. With none set
// (a world built without the library: unit tests), a claim about a named person is said in its free words after the
// name, as before, and one about anything else (a bandit camp) is never said.
#include <map>
#include <string>
#include <vector>

namespace ratw::claims
{
struct Kind
{
    std::string id;                               // "theft.attempt"
    std::string family;                           // The old topic.claim_kind: "theft", "death"... (facts.json).
    std::string topic = "gossip";                 // The ambient topic it is talked of under.
    std::string topicKind;                        // topic.kind for that topic's lines ("camp"), when it sets one.
    int grave = 0;
    std::map<std::string, std::vector<std::string>> said;   // Point of view -> wordings, the first that fills is said.
    std::vector<std::string> exact, starts;       // The free words that are this kind.
};
// What fills a wording: x is the subject, y the object.
struct Words
{
    std::string xName, xFirst, yName, yFirst, place, charge, text;
};

// Reads claims.json's text; false (and the table unchanged) with the reasons in `problem`.
bool load(const std::string& text, std::string& problem);
void clear();
bool loaded();
const Kind* find(const std::string& kind);
// The kind free words are, by the table's `text` matches ("stole from Wren" -> theft); "" if none matches.
std::string kindOfText(const std::string& claim);
// The old topic.claim_kind of a kind (the shared one the ambient talk and the window answer by: defect 7); "other" if
// it has none, and for free words with no kind, as the words say.
std::string family(const std::string& kind, const std::string& claimText = {});
// The claim in words, from a point of view ("they", "victim", "to_victim", "self", "kin"); "" when this kind has no
// wording for it that can be filled. Never words that look like an id.
std::string said(const std::string& kind, const std::string& pov, const Words& w);
// The words after the subject's name, for lines written "#topic.subject# #topic.claim#": the `they` wording that was
// said, less its leading "#x.name# " ("" if it doesn't start so).
std::string predicate(const std::string& kind, const Words& w);
// Whether words look like an identifier (camp_w_ridgemere_6_8, w_ridgemere_6_8): never spoken (doc 84, defect 1).
bool rawId(const std::string& words);
} // namespace ratw::claims
