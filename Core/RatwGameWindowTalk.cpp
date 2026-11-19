// The window's talk and typed words (Docs/Design/84-living-conversations.md §13.4-13.6, Phase 13). Everything a resident
// says to a player is said in the dialogue window (Phase 12); here the window gains its voice and its ears:
//
//   - the wearer's own lines first at every node their mantle covers (W1), the tree's the floor, and an `answers` node's
//     line the library's answer to the move the player made (the same pool an NPC's question would get);
//   - the hub rebuilt within nine choices, Ask about..., a name given or asked, greetings by meeting and disposition, the
//     player's claim store and the choices it unlocks, the ledger with players, the game's own asks (a matchmaker's
//     pointer, a vouch, a letter, a festival, the records, where someone is, something brought up, a name);
//   - a mantle's grafts shown (its choices and nodes, for that wearer only);
//   - typed words: the NPC Mind answers them when it is connected (the user's answer Q1); otherwise, and whenever a
//     call fails or times out, the window's own understanding (Data/Voice/understand.json: a choice on show, a node, a
//     move answered, or an in-character "didn't follow");
//   - lines turn over: the same resident doesn't say the same line twice running to the same player at a node.
#include "RatwGame.h"

#include "RatwClaims.h"
#include "RatwInjury.h"
#include "RatwNames.h"
#include "RatwNewcomers.h"
#include "RatwTalk.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <map>
#include <set>

namespace ratw::game
{
namespace
{
const char* yes(bool b) { return b ? "yes" : "no"; }

std::string fact(const talk::Facts& f, const std::string& k)
{
    const auto found = f.find(k);
    return found == f.end() ? std::string() : found->second;
}

std::string trimmed(std::string s)
{
    while (!s.empty() && s.front() == ' ')
        s.erase(s.begin());
    while (!s.empty() && s.back() == ' ')
        s.pop_back();
    return s;
}

// "swims_in_the_lake_even_in_autumn" -> "swims in the lake even in autumn".
std::string quirkWords(std::string q)
{
    std::replace(q.begin(), q.end(), '_', ' ');
    return q;
}
} // namespace

// ------------------------------------------------------------------ The player's claim store (§13.4.5)

void Game::playerHeard(const std::string& playerId, PlayerClaim claim)
{
    const auto* p = world_.entity(playerId);
    if (!p || p->npc || claim.key.empty() || claim.words.empty())
        return;
    auto& heard = playerClaims_[playerId];
    for (auto it = heard.begin(); it != heard.end(); ++it)
        if (it->key == claim.key)
        {
            heard.erase(it);                        // (Heard again: the newest.)
            break;
        }
    heard.push_back(std::move(claim));
    while (heard.size() > PlayerClaimsKept)
        heard.pop_front();
}

Game::PlayerClaim Game::claimHeard(const std::string& playerId, const Belief& held, const std::string& from, const std::string& toWhom) const
{
    PlayerClaim c;
    auto b = held;
    world_.claimOf(b);
    const auto* about = world_.entity(b.subject);
    if (!about || b.claim.rfind("deed:", 0) == 0)
        return c;
    claims::Words w;
    w.xName = labelFor(playerId, b.subject);
    w.xFirst = w.xName.substr(0, w.xName.find(' '));
    if (const auto* object = b.object.empty() ? nullptr : world_.entity(b.object))
    {
        w.yName = labelFor(playerId, b.object);
        w.yFirst = w.yName.substr(0, w.yName.find(' '));
    }
    w.place = world_.placeWords(b.place);
    w.text = b.claim;
    std::string words;
    if (claims::loaded())
    {
        if (!toWhom.empty() && toWhom == b.object)
            words = claims::said(b.kind, "to_victim", w);
        if (words.empty())
            words = claims::said(b.kind, "they", w);
        if (words.empty())
            if (const auto predicate = claims::predicate(b.kind, w); !predicate.empty())
                words = w.xName + " " + predicate;
    }
    else if (!claims::rawId(b.claim))
        words = w.xName + " " + b.claim;
    if (words.empty())
        return c;
    c.key = b.subject + "|" + b.claim;
    c.kind = claims::family(b.kind, b.claim);
    c.subject = b.subject;
    c.object = b.object;
    c.place = b.place;
    c.words = words;
    c.from = from;
    c.day = world_.calendarDays();
    return c;
}

void Game::playerClaimsSave(json::Value& root) const
{
    auto list = json::Value::array();
    const double today = world_.calendarDays();
    for (const auto& [id, heard] : playerClaims_)
        for (const auto& c : heard)
        {
            if (today - c.day > PlayerClaimDays)
                continue;
            auto o = json::Value::object();
            o.add("character", id);
            o.add("key", c.key);
            o.add("kind", c.kind);
            o.add("subject", c.subject);
            o.add("object", c.object);
            o.add("place", c.place);
            o.add("words", c.words);
            o.add("from", c.from);
            o.add("day", c.day);
            list.push(o);
        }
    root.add("playerClaims", list);
}

void Game::playerClaimsLoad(const json::Value& saved)
{
    playerClaims_.clear();
    for (const auto& o : saved.array("playerClaims"))
        if (const auto id = o.string("character"); !id.empty() && characters_.count(id))
            playerClaims_[id].push_back({o.string("key"), o.string("kind"), o.string("subject"), o.string("object"), o.string("place"),
                                         json::utf8Prefix(o.string("words"), 200), o.string("from"), o.number("day")});
}

// ------------------------------------------------------------------ What the window knows (§13.4)

void Game::windowFacts(const std::string& playerId, const DialogueSession& d, talk::Facts& f)
{
    const auto& npcId = d.npc;
    const auto* npc = world_.entity(npcId);
    if (!npc)
        return;
    const auto& society = world_.society();
    const double today = world_.calendarDays();
    f["player.named"] = yes(knowsName(npcId, playerId));
    f["player.met"] = yes(known_.knows(playerId, npcId));
    f["player.newcomer"] = yes(isNewcomer(accountKey(playerId)));
    // What the player has heard: each claim's kind, and :self or :kin where it is about the one spoken to or theirs.
    std::string knows;
    if (const auto heard = playerClaims_.find(playerId); heard != playerClaims_.end())
        for (const auto& c : heard->second)
        {
            if (today - c.day > PlayerClaimDays || c.kind.empty())
                continue;
            knows += " " + c.kind;
            if (c.subject == npcId || c.object == npcId)
                knows += " " + c.kind + ":self";
            else if (society.family(npcId, c.subject) || society.family(npcId, c.object) ||
                     (society.spouse(npcId) && (*society.spouse(npcId) == c.subject || *society.spouse(npcId) == c.object)))
                knows += " " + c.kind + ":kin";
        }
    f["player.knows"] = trimmed(knows);
    // The ledger's callback (§13.4.7): a way or a person asked after in an earlier window, a while ago (a quarter of a day).
    f["player.asked"] = "none";
    for (const auto& [k, v] : world_.talkLedger().pairKept(npcId, playerId, today))
    {
        const auto bar = v.rfind('|');
        if (bar == std::string::npos || today - std::atof(v.c_str() + bar + 1) < .25)
            continue;
        if (k == "asked_way")
            f["player.asked"] = "way", f["here.way_asked"] = v.substr(0, bar);
        else if (k == "asked_person" && f["player.asked"] == "none")
            f["player.asked"] = "person", f["here.person_asked"] = v.substr(0, bar);
    }
    // The new lists' counts.
    for (const char* list : {"here.opinions", "here.kin", "here.people", "here.heard"})
        f[list] = std::to_string(windowItems(playerId, npcId, list).size());
    // The hours, a matchmaker, the game's own asks.
    if (const auto* job = society.jobOf(npcId))
    {
        const auto words = [](double hour) {
            static const char* const N[] = {"twelve", "one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten", "eleven"};
            const int h = int(std::floor(std::fmod(hour + 24, 24)));
            return h == 12 ? std::string("noon") : std::string(N[h % 12]) + (h < 12 ? " in the morning" : h < 18 ? " in the afternoon" : " in the evening");
        };
        f["here.hours"] = "from " + words(job->startHour) + " until " + words(job->endHour);
    }
    f["here.has_hours"] = yes(!fact(f, "here.hours").empty());
    f["here.matchmaker"] = yes(isMatchmaker(npcId));
    bool vouched = false;
    for (const auto& [id, v] : vouches_)
        vouched |= v.resident == npcId && ((v.state == "active" && v.vouched == playerId) || (v.state == "broken" && v.voucher == playerId && !v.raised));
    f["here.vouched"] = yes(vouched);
    f["here.letter"] = yes(!residentLetterBriefing(npcId, playerId).empty());
    f["here.festival_won"] = yes(!festivalBriefing(npcId, playerId).empty());
    f["here.records"] = yes(!scholarBriefing(npcId, playerId).empty());
    // The meeting, at the window's opening: back after a long while (doc 56, 10), or its deeds heard of (doc 56, 5).
    if (d.history.empty())
    {
        if (const auto away = absences_.find(playerId); away != absences_.end() && away->second.unseen.count(npcId))
        {
            f["topic.meeting"] = "returning";
            f["topic.since"] = calendar::seasonName(calendar::calendarAt(away->second.unseen.at(npcId)).season);
        }
        else
        {
            const auto pair = npcId + "|" + playerId;
            const auto greeted = fameGreeted_.find(pair);
            if (greeted == fameGreeted_.end() || today - greeted->second >= fame::rules().greetEveryDays)
                for (const auto& r : recognise(npcId, playerId, false))
                    if (r.deed && r.deed->weight >= fame::Notable && (!r.byName || knowsName(npcId, playerId)))
                    {
                        f["topic.meeting"] = r.byName ? "famous" : "famous_look";
                        f["topic.deed"] = deedPhrase(npcId, *r.deed);
                        break;
                    }
        }
    }
}

std::vector<talk::Facts> Game::windowItems(const std::string& playerId, const std::string& npcId, const std::string& list)
{
    std::vector<talk::Facts> out;
    const auto& society = world_.society();
    const double today = world_.calendarDays();
    if (list == "here.opinions")
    {
        // The issues it holds a view on, the strongest three (a stance of 1 or more).
        const auto q = talkQuery(npcId, playerId, "", {});
        std::vector<std::pair<int, const talk::Issue*>> held;
        for (const auto& i : talk_.issues())
        {
            const auto op = q.a.facts.find("op." + i.name);
            const auto s = q.a.facts.find("op_strength." + i.name);
            const int strength = s == q.a.facts.end() ? 0 : std::atoi(s->second.c_str());
            if (op != q.a.facts.end() && strength >= 1)
                held.push_back({strength, &i});
        }
        std::stable_sort(held.begin(), held.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
        for (std::size_t n = 0; n < held.size() && n < 3; ++n)
            out.push_back({{"topic.opinion_issue", held[n].second->name}, {"topic.opinion_words", held[n].second->words}});
        return out;
    }
    if (list == "here.kin")
    {
        // Their living family: a spouse, their children, their parents (the first three).
        const auto people = peopleOf(npcId);
        for (const char* role : {"spouse", "child", "mother", "father"})
        {
            const auto found = people.find(role);
            if (found == people.end() || out.size() >= 3)
                continue;
            const auto* k = world_.entity(found->second);
            if (!k || k->dead)
                continue;
            const bool male = k->appearance->sex != "female";
            const std::string word = std::string(role) == "spouse" ? (male ? "husband" : "wife")
                                     : std::string(role) == "child" ? (male ? "son" : "daughter")
                                                                    : role;
            std::string state = "well";
            if (society.mourning(k->id))
                state = "mourning";
            else if (k->hurt >= 50)
                state = "hurt";
            else if (const auto* life = society.resident(k->id); life && life->hunger >= 70)
                state = "hungry";
            out.push_back({{"topic.kin_id", k->id}, {"topic.kin_name", k->name}, {"topic.kin_word", word}, {"topic.kin_state", state}});
        }
        return out;
    }
    if (list == "here.people")
    {
        // Residents of this town the player has met (their Known wolves), the nearest six.
        const auto* npc = world_.entity(npcId);
        if (!npc)
            return out;
        const auto town = world_.communityOf(npc->cellId);
        std::vector<std::pair<double, talk::Facts>> found;
        for (const auto& [id, life] : society.state().residents)
        {
            if (id == npcId || !known_.knows(playerId, id))
                continue;
            const auto* e = world_.entity(id);
            if (!e || e->dead || town.empty() || world_.communityOf(life.homeCell) != town)
                continue;
            const double far = e->cellId == npc->cellId ? std::hypot(e->position.x - npc->position.x, e->position.y - npc->position.y) : 1e6;
            found.push_back({far, {{"topic.person_id", id}, {"topic.person_name", e->name}}});
        }
        std::sort(found.begin(), found.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        for (std::size_t i = 0; i < found.size() && i < 6; ++i)
            out.push_back(found[i].second);
        return out;
    }
    if (list == "here.heard")
    {
        // What the player has heard and might bring up here, the newest two: never about the one spoken to (the subject),
        // never what it was told already by this player or by someone else of whom it holds the claim.
        const auto heard = playerClaims_.find(playerId);
        if (heard == playerClaims_.end())
            return out;
        for (auto it = heard->second.rbegin(); it != heard->second.rend() && out.size() < 2; ++it)
        {
            const auto& c = *it;
            if (today - c.day > PlayerClaimDays || c.subject == npcId || world_.talkLedger().toldBy(npcId, playerId, c.key))
                continue;
            out.push_back({{"topic.heard_key", c.key}, {"topic.heard_words", c.words}, {"topic.heard_kind", c.kind}});
        }
        return out;
    }
    return out;
}

// ------------------------------------------------------------------ A node as this wearer sees it (grafts)

bool Game::windowNode(const std::string& playerId, const DialogueSession& d, const std::string& nodeId, talk::TreeNode& out)
{
    const auto* t = talk_.tree(d.tree);
    if (!t)
        return false;
    const auto* npc = world_.entity(d.npc);
    const auto worn = npc ? mantleOf_.find(d.npc) : mantleOf_.end();
    const auto mantle = worn != mantleOf_.end() && talk_.mantle(worn->second) ? worn->second : std::string();
    if (!nodeId.empty() && nodeId[0] == '@')
    {
        // A graft's own node: "@<mantle>.<name>", only the wearer's.
        const auto dot = nodeId.find('.');
        if (dot == std::string::npos || nodeId.substr(1, dot - 1) != mantle)
            return false;
        return talk_.graftNode(mantle, d.tree, nodeId.substr(dot + 1), out);
    }
    const auto f = t->nodes.find(nodeId);
    if (f == t->nodes.end())
        return false;
    out = f->second;
    if (mantle.empty())
        return true;
    // The wearer's graft choices on this node, in what room the node leaves (nine at most); a hub's that don't fit are
    // shown under Ask about... instead (the resident tree's `about`).
    const auto room = [&](const talk::TreeNode& n) {
        auto q = dialogueQuery(playerId, d);
        q.concept = n.concept;
        const auto shown = talk_.choices(n, q, 0, [&](const std::string& list) { return dialogueItems(playerId, d.npc, list); });
        return shown.size() >= 9 ? std::size_t(0) : 9 - shown.size();
    };
    const auto holding = [&](std::vector<talk::TreeChoice> list) {
        auto q = dialogueQuery(playerId, d);
        list.erase(std::remove_if(list.begin(), list.end(), [&](const talk::TreeChoice& c) { return !talk_.holdsAll(c.when, q, 0); }), list.end());
        return list;
    };
    const auto place = [&](talk::TreeNode& n, std::vector<talk::TreeChoice> add) {
        // Before the node's last choice where that is the way out (Goodbye., Back to the rest.), else at the end.
        auto at = n.choices.end();
        if (!n.choices.empty() && (n.choices.back().go == "bye" || n.choices.back().go == "hub" || n.choices.back().go == "about"))
            at = n.choices.end() - 1;
        n.choices.insert(at, add.begin(), add.end());
    };
    std::vector<talk::TreeChoice> own;
    talk_.graftChoices(mantle, d.tree, nodeId, own);
    own = holding(std::move(own));
    std::vector<talk::TreeChoice> overflow;
    if (!own.empty())
    {
        const auto n = std::min(own.size(), room(out));
        overflow.assign(own.begin() + std::ptrdiff_t(n), own.end());
        own.resize(n);
        place(out, own);
    }
    if (d.tree == "resident" && nodeId == "about")
    {
        // The hub's grafts that had no room there.
        talk::TreeNode hub;
        if (const auto h = t->nodes.find("hub"); h != t->nodes.end())
        {
            std::vector<talk::TreeChoice> hubs;
            talk_.graftChoices(mantle, d.tree, "hub", hubs);
            hubs = holding(std::move(hubs));
            const auto fit = std::min(hubs.size(), room(h->second));
            std::vector<talk::TreeChoice> rest(hubs.begin() + std::ptrdiff_t(fit), hubs.end());
            rest.resize(std::min(rest.size(), room(out)));
            place(out, rest);
        }
    }
    return true;
}

std::string Game::windowPointer(const std::string& npcId, const std::string& playerId, const std::string& heard)
{
    // A matchmaker's pointer (doc 52, 5) in the window: one already worked out for this player (the Mind was asked, and
    // failed), else worked out now from what they said; the written line, the other named as the player knows them
    // (aloud, each listener hears the window's words as the player's: no name passed on as hearsay).
    const auto pending = matchPending_.find(npcId);
    if (pending == matchPending_.end() || pending->second.player != playerId)
        if (!isMatchmaker(npcId) || matchmake(npcId, playerId, heard).empty())
            return {};
    const auto found = matchPending_.find(npcId);
    const std::string other = found == matchPending_.end() ? std::string() : found->second.other;
    const std::string who = found == matchPending_.end() ? std::string() : found->second.who;   // (As In Sight shows them.)
    auto said = trimmed(sayMatch(npcId, "", false));
    if (const auto* o = world_.entity(other); o && !o->name.empty() && !who.empty() && !knowsName(playerId, other))
        for (auto at = said.find(o->name); at != std::string::npos; at = said.find(o->name, at + who.size()))
            said.replace(at, o->name.size(), who);
    return said;
}

std::string Game::windowFame(const std::string& npcId, const std::string& playerId)
{
    // A deed it knew by look, joined to the name just given (doc 56, 3): said as it realises, and noted as the Mind's
    // briefing notes it (realised, spoken of).
    const auto realised = fameRealised_.find(npcId + "|" + playerId);
    if (realised == fameRealised_.end())
        return {};
    const auto* deed = fame_.find(realised->second);
    if (!deed)
        return {};
    const auto what = deedPhrase(npcId, *deed);
    fameBriefing(npcId, playerId, true);
    return what.empty() ? std::string() : "Wait. You're the one who " + what + "!";
}

// ------------------------------------------------------------------ The resident's line at a node

std::string Game::windowAnswer(const std::string& playerId, DialogueSession& d, const std::string& move, const talk::Query& q)
{
    // The library's answer to the move the player made (§13.4.2): what it expects, answered as an NPC's would be (the
    // wearer's own lines first, within their share), in the resident's dialect and by their temperament.
    const auto* m = talk_.moves().find(move);
    std::vector<std::string> owedMoves;
    if (m && !m->expects.empty())
        owedMoves = m->expects;
    else if (!move.empty())
        owedMoves = {move};                         // (A move given as what is owed: take_offence, ack.)
    if (owedMoves.empty())
        return {};
    talk::Conversation c;
    c.people = {d.npc, playerId};
    c.players = {1};
    c.phase = talk::Conversation::Phase::Body;
    c.seed = std::hash<std::string>{}(d.npc + playerId + move) + std::uint64_t(world_.time() * 10);
    auto asked = q;
    // An opinion asked: the issue, and the resident's stance on it (as the companion's "What do you think?").
    if (const auto issue = fact(q.facts, "topic.opinion_issue"); !issue.empty())
        if (const auto* i = talk_.issue(issue))
        {
            c.conv["issue"] = i->name;
            c.conv["issue_words"] = i->words;
            asked.facts["conv.issue"] = i->name;
            asked.facts["conv.issue_words"] = i->words;
            const auto of = [&](const std::string& k) { return fact(asked.a.facts, k + "." + i->name); };
            asked.a.facts["stance"] = of("op").empty() ? "mixed" : of("op");
            asked.a.facts["stance_strength"] = of("op_strength").empty() ? "0" : of("op_strength");
            if (const auto why = of("op_why"); !why.empty())
                asked.a.facts["stance_why"] = why;
        }
    talk::Conversation::Open owed;
    owed.by = 1;
    owed.to = 0;
    owed.moves = owedMoves;
    const auto said = talk_.answer(c, owed, asked, 0, {});
    if (said.id.empty() || said.lines.empty() || said.lines.front().second.empty())
        return {};
    talkRemember(said.effects);
    return said.lines.front().second;
}

std::string Game::windowAsk(const std::string& playerId, DialogueSession& d, const std::string& ask, const talk::Query& q)
{
    // The game's own lines (§13.4.6): what only chat reached before.
    const auto& npcId = d.npc;
    const auto* npc = world_.entity(npcId);
    if (!npc)
        return {};
    if (ask == "game.match")
    {
        // A matchmaker's pointer (doc 52, 5): the written line, the other named as the player knows them in the window
        // (aloud, each listener hears them as it knows them: the line is veiled as it is said).
        return windowPointer(npcId, playerId, "");
    }
    if (ask == "game.vouch")
    {
        // Who vouched for this wolf (a little better thought of for it), or that one they vouched for wronged it (once).
        for (auto& [id, v] : vouches_)
        {
            if (v.resident != npcId)
                continue;
            if (v.state == "active" && v.vouched == playerId)
                return names::capitalised(labelFor(npcId, v.voucher)) + " spoke up for you. I think the better of you for it.";
            if (v.state == "broken" && v.voucher == playerId && !v.raised)
            {
                v.raised = true;
                saveSoon();
                return "You spoke up for " + labelFor(npcId, v.vouched) + ", and they wronged me. I've not forgotten it.";
            }
        }
        return {};
    }
    if (ask == "game.letters")
    {
        // What it wrote them in the last 28 days, and an occasion it stood witness at (doc 55, 5-6).
        std::vector<const documents::Document*> mine;
        for (const auto* doc : documents_.fromAuthor(npcId))
            if (doc->to == playerId && doc->kind == "resident_letter" && world_.calendarDays() - doc->written <= 28 && !doc->facts.empty())
                mine.push_back(doc);
        std::sort(mine.begin(), mine.end(), [](const auto* a, const auto* b) { return a->written > b->written; });
        std::string out;
        if (!mine.empty())
            out = "I wrote to you " + injury::dateWords(mine.front()->written) + " " + mine.front()->facts + ". Did it reach you?";
        if (const auto found = witnessedBy_.find(npcId); found != witnessedBy_.end())
            for (auto it = found->second.rbegin(); it != found->second.rend(); ++it)
                if (it->player == playerId)
                {
                    out += std::string(out.empty() ? "" : " ") +
                           (it->kind == "wedding" ? "I stood witness at your wedding. I'll not forget it." : "I stood with you at the funeral.");
                    break;
                }
        return out;
    }
    if (ask == "game.festival")
    {
        auto words = festivalBriefing(npcId, playerId);
        if (words.empty())
            return {};
        words = trimmed(words);                     // "At today's Lamp Fair: Wren won the race."
        if (const auto colon = words.find(": "); colon != std::string::npos)
            words = words.substr(0, colon) + ", " + words.substr(colon + 2);
        return words;
    }
    if (ask == "game.records")
        return scholarBriefing(npcId, playerId).empty() ? std::string()
                                                        : "You've read our old records, I hear. More than most of us have. Ask what you like of the town's past.";
    if (ask == "game.where")
    {
        const auto name = fact(q.facts, "topic.person_name");
        return name.empty() ? std::string() : gameAnswer(npcId, playerId, "where is " + name, true);
    }
    if (ask == "game.name")
    {
        // The resident gives its name (or keeps it): the player knows it now (their Known wolves).
        if (!willName(npcId, playerId))
            return refusalInVoice(npcId, playerId, "My name's my own business.");
        learnName(playerId, npcId, npc->name, "introduced");
        auto line = windowAnswer(playerId, d, "ask.name", q);
        if (line.find(npc->name.substr(0, npc->name.find(' '))) == std::string::npos)
            line.clear();                           // (An answer that never says the name isn't one.)
        return line;
    }
    if (ask == "game.tell")
    {
        // Something the player heard, brought up (§13.4.5): known already ("So I'd heard"), or news to it (it holds the
        // claim now, as heard from this wolf, at a stranger's word: 0.4).
        const auto key = fact(q.facts, "topic.heard_key");
        const auto heard = playerClaims_.find(playerId);
        const PlayerClaim* claim = nullptr;
        if (heard != playerClaims_.end())
            for (const auto& c : heard->second)
                if (c.key == key)
                    claim = &c;
        if (!claim)
            return {};
        bool knew = false;
        if (const auto* held = world_.beliefsOf(npcId))
            for (const auto& b : *held)
                knew |= b.subject == claim->subject && (claim->key == b.subject + "|" + b.claim || (!claim->kind.empty() && b.kind == claim->kind));
        auto told = q;
        told.b.facts["knows"] = yes(knew);
        told.a.facts["knows"] = yes(knew);
        told.facts["topic.claim_kind"] = claim->kind;
        told.facts["topic.said"] = claim->words;
        auto line = windowAnswer(playerId, d, knew ? "react.news.known" : "react.news.new", told);
        if (!knew)
        {
            Belief b;
            b.holder = npcId;
            b.subject = claim->subject;
            b.claim = claim->key.substr(claim->key.find('|') + 1);
            b.source = playerId;
            b.confidence = .4;
            b.kind = claim->kind;
            b.object = claim->object;
            b.place = claim->place;
            world_.told(b, playerId);
        }
        world_.talkLedger().told(npcId, playerId, claim->key, claim->kind, world_.calendarDays());
        d.topics.push_back("news");
        return line;
    }
    return {};
}

std::string Game::windowLine(const std::string& playerId, DialogueSession& d, const talk::TreeNode& node, talk::Query& q,
                             std::uint64_t seed, talk::LineInfo& from)
{
    const auto key = d.npc + "|" + playerId + "|" + node.concept;
    const auto last = windowLast_.find(key);
    // The node's own lines (the wearer's mantle's first, +5; a line of strict priority over it: asleep, hostile, a
    // welcome, a callback), turned over so the same line isn't said twice running to the same player (§13.4.8).
    talk::Said said;
    for (std::uint64_t attempt = 0; attempt < 4; ++attempt)
    {
        said = talk_.pick(q, seed + attempt * 7919, {}, {});
        if (said.lines.empty() || last == windowLast_.end() || said.lines.front().second != last->second)
            break;
    }
    const bool own = !said.id.empty() && talk_.ownLine(said.id, q);
    const auto* picked = said.id.empty() ? nullptr : talk_.find(said.id);
    const bool strict = picked && picked->priority >= talk_.settings().strict;
    std::string line;
    if (!own && !strict)
    {
        // An `answers` node: the library's answer to the move made (the choice's, or the node's own).
        const auto move = node.answers == "*" ? d.asked : node.answers;
        if (!node.answers.empty() && !move.empty())
            line = windowAnswer(playerId, d, move, q);
        // An `ask`: the game's answer in the resident's voice (doc 28), a companion's view, the game's own lines.
        if (line.empty() && node.ask.rfind("player.", 0) == 0)
        {
            const auto intent = node.ask.substr(7);
            if (intent == "where" && !fact(q.facts, "topic.person_name").empty())
                line = gameAnswer(d.npc, playerId, "where is " + fact(q.facts, "topic.person_name"), true);
            else
                line = gameAnswer(d.npc, playerId, std::string(), true, intent);
        }
        else if (line.empty() && node.ask == "companion.opinion")
            line = companionOpinion(d.npc, playerId);
        else if (line.empty() && node.ask.rfind("game.", 0) == 0)
            line = windowAsk(playerId, d, node.ask, q);
        if (!line.empty())
            from = talk::LineInfo{node.answers.empty() ? node.ask : "answers:" + move, -1, {}};
    }
    else if (node.ask.rfind("game.", 0) == 0 && node.ask != "game.name")
    {
        // (The game's own news still told under the wearer's words: the matchmaker's pointer, the claim answered.)
        if (const auto more = windowAsk(playerId, d, node.ask, q); !more.empty())
            said.lines.front().second += " " + more;
    }
    if (line.empty() && !said.lines.empty())
    {
        line = said.lines.front().second;
        talkRemember(said.effects);
        if (!said.info.empty())
            from = said.info.front();
    }
    // Introduced: a deed it knew by look, now by name (doc 56, 3).
    if (node.concept == "dlg.resident.introduced" && !line.empty())
        if (const auto fame = windowFame(d.npc, playerId); !fame.empty())
            line += " " + fame;
    // Gossip told in the window is heard (§13.4.5): the player may bring it up elsewhere.
    if (node.concept == "dlg.resident.gossip" && fact(q.facts, "here.gossip") == "yes" && !line.empty())
    {
        Belief b;
        b.subject = fact(q.facts, "~gossip.subject");
        b.claim = fact(q.facts, "~gossip.claim");
        b.kind = fact(q.facts, "~gossip.kind");
        b.object = fact(q.facts, "~gossip.object");
        b.place = fact(q.facts, "~gossip.place");
        if (auto c = claimHeard(playerId, b, d.npc); !c.key.empty())
            playerHeard(playerId, std::move(c));
    }
    // The callback said is said once (§13.4.7).
    if (picked && strict && node.concept == "dlg.resident.hub")
        for (const auto& c : picked->when)
            if (c.full == "player.asked")
                world_.talkLedger().forget(d.npc, playerId, c.values.count("way") ? "asked_way" : "asked_person");
    if (!line.empty())
    {
        if (windowLast_.size() > 20000)
            windowLast_.clear();                    // (A long-running server's: forgetting is harmless here.)
        windowLast_[key] = line;
    }
    return line;
}

// ------------------------------------------------------------------ The resident's speech, in dialect

std::string Game::inVoice(const std::string& npcId, const std::string& playerId, const std::string& text, const talk::Facts& facts)
{
    const auto q = talkQuery(npcId, playerId, "", facts);
    std::string out;
    if (!talk_.expand(text, q, 0, std::hash<std::string>{}(npcId + text) + std::uint64_t(world_.calendarDays()), out) || out.empty())
        return {};
    return out;
}

std::string Game::refusalInVoice(const std::string& npcId, const std::string& playerId, const std::string& why)
{
    // The C++'s reasons, each a `#refuse_*#` symbol whose plain words are the reason as written (doc 84 §10, Phase 13):
    // a dialect's keeper says it in their own.
    static const std::vector<std::pair<const char*, const char*>> Kinds{
        {"That work is gone.", "#refuse_gone#"},           {"That's gone, I'm afraid.", "#refuse_gone#"},
        {"That's promised to another for now.", "#refuse_promised#"}, {"That's asked for at another market.", "#refuse_elsewhere#"},
        {"That's your own work.", "#refuse_own#"},         {"Someone's taken that on already.", "#refuse_taken#"},
        {"You've taken that on already.", "#refuse_yours#"}, {"That's been done.", "#refuse_done#"},
        {"We've no rooms to let.", "#refuse_no_rooms#"},   {"You have a lodging already.", "#refuse_lodging#"},
        {"They don't know you well enough to take you on yet.", "#refuse_trust#"},
        {"You're learning a trade already.", "#refuse_learning#"}, {"My name's my own business.", "#refuse_name#"}};
    for (const auto& [words, symbol] : Kinds)
        if (why == words)
            if (auto said = inVoice(npcId, playerId, symbol); !said.empty())
                return said;
    return why;
}

// ------------------------------------------------------------------ Typed words without the Mind (§13.5)

understand::Lexicon Game::dialogueLexicon(const std::string& npcId)
{
    // The world's words for one reading: the town's living residents by name, its places by name and kind, the goods its
    // keepers sell, the Houses, the wearer's quirk.
    understand::Lexicon l;
    const auto* npc = world_.entity(npcId);
    if (!npc)
        return l;
    l.self = npc->name;
    const auto& society = world_.society();
    const auto town = world_.communityOf(npc->cellId);
    l.town = town.empty() ? std::string() : townWords(town);
    std::set<std::string> goods;
    for (const auto& [id, life] : society.state().residents)
        if (const auto* e = world_.entity(id); e && !e->dead && !e->name.empty() && id != npcId &&
                                               (town.empty() || world_.communityOf(life.homeCell) == town || world_.communityOf(e->cellId) == town))
            l.people.push_back(e->name);
    for (const auto& p : dialoguePlaces(npcId))
        l.places.push_back(fact(p, "topic.place_title"));
    for (const auto& [id, c] : world_.cells())
        if (!c.outdoors && !town.empty() && world_.communityOf(id) == town && l.places.size() < 400)
            l.places.push_back(c.name);
    l.placeKinds = {"inn", "tavern", "alehouse", "church", "chapel", "smithy", "forge", "bakery", "brewery", "healer", "watch house",
                    "watch", "market", "town office", "docks", "jetty", "notice board", "stables", "well", "shop", "store"};
    for (const auto& p : society.positions())
        if (p.role == "merchant" && world_.communityOf(p.work.cell) == town)
            if (const auto held = society.state().careers.positions.find(p.id); held != society.state().careers.positions.end())
                for (const auto& w : society.wares(held->second.holder))
                    goods.insert(Society::itemName(w));
    l.goods.assign(goods.begin(), goods.end());
    if (const auto q = talkQuery(npcId, "", "", {}); q.a.facts.count("quirk"))
        l.quirk = q.a.facts.at("quirk");
    return l;
}

void Game::dialogueUnderstand(Connection* c, DialogueSession& d, const std::string& text, const std::string& note)
{
    const auto playerId = c->entityId;
    if (!understanding_.loaded())
    {
        d.awaiting = false;
        dialogueSend(c, d, "They didn't catch that.");
        return;
    }
    const auto lexicon = dialogueLexicon(d.npc);
    // A line said over the node the player is at: the choices stay as they were.
    const auto sayOver = [&](std::string line, const std::string& mood) {
        if (line.empty())
            return false;
        auto q = dialogueQuery(playerId, d);
        d.line = line;
        d.mood = dialogueMood(d.npc, q, mood);
        d.awaiting = false;
        d.changed = world_.time();
        d.history.push_back({"npc", line});
        while (d.history.size() > 4)
            d.history.pop_front();
        world_.faceTalker(d.npc, playerId);
        dialogueSpeak(d.npc, playerId, line);
        voiced("dialogue", "understood", d.npc);
        if (options_.logTalkWords)
            voicedWords("dialogue", d.npc, playerId, line, nullptr);
        dialogueSend(c, d, note);
        return true;
    };
    // A node's line said over the node the player is at (unclear, thanks...): this tree's, else the resident tree's.
    const auto nodeOver = [&](const std::string& nodeId) {
        for (const auto& treeId : {d.tree, std::string("resident")})
            if (const auto* t = talk_.tree(treeId); t && t->nodes.count(nodeId))
            {
                auto shadow = d;
                shadow.tree = treeId;
                auto q = dialogueQuery(playerId, shadow);
                const auto& n = t->nodes.at(nodeId);
                q.concept = n.concept;
                talk::LineInfo from;
                const auto line = windowLine(playerId, shadow, n, q, std::hash<std::string>{}(d.npc + playerId + nodeId) + std::uint64_t(world_.time() * 10), from);
                return sayOver(line, n.mood);
            }
        return false;
    };
    // Going to a node: this tree's, else the resident tree's (Talk's), the conversation going on there.
    const auto goTo = [&](const std::string& nodeId, const talk::Facts& bind, const std::string& move) {
        for (const auto& treeId : {d.tree, std::string("resident")})
            if (const auto* t = talk_.tree(treeId); t && t->nodes.count(nodeId))
            {
                d.tree = treeId;
                for (const auto& [k, v] : bind)
                    d.bound[k] = v;
                d.asked = move;
                d.awaiting = false;
                dialogueGo(c, d, nodeId, note);
                return true;
            }
        return false;
    };
    const auto openEntry = [&](const std::string& entry) {
        auto q = dialogueQuery(playerId, d);
        const auto* next = talk_.treeFor(entry, q);
        if (!next)
            return false;
        d.tree = next->id;
        d.entry = entry;
        d.awaiting = false;
        dialogueGo(c, d, next->entries.at(entry), note);
        return true;
    };
    // 1. A choice on show, in the player's own words ("show me your stock" is the wares choice).
    {
        // (Not a name given or asked: said in the words already, the intent takes it: "Good day. I'm Ash.")
        talk::TreeNode here;
        const bool known = windowNode(playerId, d, d.node, here);
        std::vector<std::string> shown;
        for (const auto& s : d.shown)
        {
            const bool naming = known && s.index < here.choices.size() &&
                                (here.choices[s.index].action == "introduce" || here.choices[s.index].action == "ask_name");
            shown.push_back(s.locked || naming ? std::string() : s.text);
        }
        if (const int n = understanding_.matchChoice(text, shown, lexicon); n >= 0)
        {
            d.awaiting = false;
            dialogueChoose(c, d, d.shown[std::size_t(n)].n, text, true);
            return;
        }
    }
    // 2. What the words mean.
    const auto r = understanding_.read(text, lexicon);
    const auto& in = r.intent;
    const auto slot = [&](const char* k) {
        const auto f = r.slots.find(k);
        return f == r.slots.end() ? std::string() : f->second;
    };
    d.topics.push_back("said");
    // A matchmaker hearing a newcomer (doc 52, 5): whatever was said, the pointer, as the innkeeper's answer.
    if (const auto pointer = windowPointer(d.npc, playerId, text); !pointer.empty())
    {
        const auto greeting = in == "greet" ? gameAnswer(d.npc, playerId, text, true, "greet") : std::string();
        sayOver(greeting.empty() ? pointer : greeting + " " + pointer, "warm");
        return;
    }
    if (!r.sure)
    {
        if (!nodeOver("unclear"))
            sayOver("I'm not sure I follow you.", "");
        return;
    }
    // A choice on show that goes where the intent does (the wares, the news...).
    const auto take = [&](const std::function<bool(const talk::TreeChoice&)>& wanted) {
        talk::TreeNode node;
        if (!windowNode(playerId, d, d.node, node))
            return false;
        for (const auto& s : d.shown)
            if (!s.locked && s.index < node.choices.size() && wanted(node.choices[s.index]))
            {
                d.awaiting = false;
                dialogueChoose(c, d, s.n, text, true);
                return true;
            }
        return false;
    };
    const bool merchant = world_.society().merchant(d.npc);
    const auto moveLine = [&](const std::string& move) {
        auto q = dialogueQuery(playerId, d);
        return sayOver(windowAnswer(playerId, d, move, q), "");
    };
    if (in == "bye")
    {
        dialogueClose(playerId, "bye", true);
        return;
    }
    if (in == "greet")
    {
        if (sayOver(gameAnswer(d.npc, playerId, text, true, "greet"), ""))
            return;
    }
    else if (in == "thank")
    {
        if (nodeOver("thanks"))
            return;
    }
    else if (in == "introduce")
    {
        // (The name was learned as it was said: noticeIntroduction. The resident gives theirs back if it would.)
        if (willName(d.npc, playerId) && !known_.knows(playerId, d.npc))
            if (const auto* npc = world_.entity(d.npc))
                learnName(playerId, d.npc, npc->name, "introduced");
        if (goTo("introduced", {}, "introduce"))
            return;
    }
    else if (in == "ask_name")
    {
        if (goTo("name", {}, "ask.name"))
            return;
    }
    else if (in == "ask_self" || in == "ask_job")
    {
        if (goTo("job", {}, "ask.detail"))
            return;
    }
    else if (in == "ask_feeling")
    {
        if (goTo("how", {}, "ask.how"))
            return;
    }
    else if (in == "ask_family")
    {
        const auto kin = windowItems(playerId, d.npc, "here.kin");
        // The kin word asked after, as the window names them (mum is mother, the pups a son or daughter, a mate the
        // husband or wife).
        const auto word = slot("kin");
        const auto asked = [&](const std::string& theirs) {
            static const std::map<std::string, std::set<std::string>> Means{
                {"mother", {"mother", "mum", "mam", "ma", "mama"}},
                {"father", {"father", "dad", "da", "pa", "papa"}},
                {"husband", {"husband", "mate", "spouse"}},
                {"wife", {"wife", "mate", "spouse"}},
                {"son", {"son", "sons", "child", "children", "pup", "pups", "little ones", "young ones"}},
                {"daughter", {"daughter", "daughters", "child", "children", "pup", "pups", "little ones", "young ones"}}};
            const auto m = Means.find(theirs);
            return m != Means.end() && m->second.count(word) > 0;
        };
        for (const auto& k : kin)
            if (!word.empty() && asked(fact(k, "topic.kin_word")) && goTo("kin_said", k, "ask.after"))
                return;
        if (goTo(kin.empty() ? "about" : "kin_list", {}, "ask.after"))
            return;
    }
    else if (in == "ask_opinion")
    {
        if (const auto* i = talk_.issue(slot("issue")))
            if (goTo("opinion", {{"topic.opinion_issue", i->name}, {"topic.opinion_words", i->words}}, "ask.opinion"))
            {
                d.topics.push_back("opinion:" + i->name);
                return;
            }
    }
    else if (in == "ask_person")
    {
        const auto name = slot("person");
        for (const auto& [id, life] : world_.society().state().residents)
            if (const auto* e = world_.entity(id); e && e->name == name)
                if (goTo("whereis", {{"topic.person_id", id}, {"topic.person_name", name}}, "ask.whereabouts"))
                {
                    d.topics.push_back("person:" + name);
                    return;
                }
    }
    else if (in == "ask_way")
    {
        const auto place = slot("place");
        if (!place.empty())
        {
            std::string lower;
            for (const char ch : place)
                lower += char(std::tolower(static_cast<unsigned char>(ch)));
            for (const auto& p : dialoguePlaces(d.npc))
            {
                auto title = fact(p, "topic.place_title"), kind = fact(p, "topic.place_kind");
                std::string t;
                for (const char ch : title)
                    t += char(std::tolower(static_cast<unsigned char>(ch)));
                if (t == lower || t.find(lower) != std::string::npos || kind == "the " + lower || kind.find(lower) != std::string::npos)
                    if (goTo("place", p, "ask.whereabouts"))
                    {
                        d.topics.push_back("way:" + title);
                        return;
                    }
            }
            // A place it can't place (no keeper there, or another town's): the game's way, else "couldn't tell you".
            if (sayOver(gameAnswer(d.npc, playerId, "where is " + place, true), ""))
                return;
        }
        if (goTo("way", {}, "ask.whereabouts"))
            return;
    }
    else if (in == "ask_town")
    {
        if (goTo("town", {}, "ask.detail"))
            return;
    }
    else if (in == "ask_news")
    {
        if (goTo("news", {}, "ask.detail"))
            return;
    }
    else if (in == "ask_gossip")
    {
        if (goTo("gossip", {}, "ask.detail"))
            return;
    }
    else if (in == "ask_trouble")
    {
        if (goTo("trouble", {}, "ask.after"))
            return;
    }
    else if (in == "ask_work")
    {
        if (take([](const talk::TreeChoice& ch) { return ch.action == "open" && !ch.args.empty() && ch.args[0] == "work"; }))
            return;
        if (merchant && openEntry("work"))
            return;
        if (sayOver(inVoice(d.npc, playerId, "#work_none#"), ""))
            return;
    }
    else if (in == "ask_wares" || in == "ask_prices")
    {
        // Asked what's sold ("What do you sell?"): the keeper's answer, two wares at their listed prices (doc 79, 7.8);
        // asked to be shown ("Show me your stock."): the shop window, the wares choice taken.
        const bool question = text.find('?') != std::string::npos;
        if (in == "ask_wares" && !question && take([](const talk::TreeChoice& ch) { return ch.action == "shop"; }))
            return;
        if (merchant && in == "ask_prices")
        {
            // The keeper's "What's dear today?" (the shop tree's prices).
            auto q = dialogueQuery(playerId, d);
            if (const auto* shop = talk_.treeFor("trade", q); shop && shop->nodes.count("prices"))
            {
                d.tree = shop->id;
                d.entry = "trade";
                d.asked = "ask.detail";
                d.awaiting = false;
                dialogueGo(c, d, "prices", note);
                return;
            }
        }
        if (sayOver(gameAnswer(d.npc, playerId, std::string(), true, "trade"), ""))
            return;
    }
    else if (in == "ask_hours")
    {
        if (goTo("hours", {}, "ask.detail"))
            return;
    }
    else if (in == "ask_day")
    {
        if (goTo("day", {}, "ask.detail"))
            return;
    }
    else if (in == "ask_wanted")
    {
        const auto* post = world_.society().jobOf(d.npc);
        if (post && post->role == "guard" && goTo("fine", {}, "ask.detail"))
            return;
        if (goTo("wanted", {}, "ask.detail"))
            return;
    }
    else if (in == "ask_teach")
    {
        if (take([](const talk::TreeChoice& ch) { return ch.action == "open" && !ch.args.empty() && ch.args[0] == "learn"; }))
            return;
        if (const auto* post = world_.society().jobOf(d.npc); post && post->role != "guard" && openEntry("learn"))
            return;
    }
    else if (in == "ask_bed")
    {
        if (take([](const talk::TreeChoice& ch) { return ch.action == "lodge" || ch.go == "rooms"; }))
            return;
        if (jobCategoryOf(d.npc) == "innkeeper" && goTo("rooms", {}, "ask.detail"))
            return;
        for (const auto& p : dialoguePlaces(d.npc))
            if (fact(p, "topic.place_kind") == "the inn" && goTo("place", p, "ask.whereabouts"))
                return;
    }
    else if (in == "ask_quirk")
    {
        // The wearer's own graft first (its choices on the hub: Myrtle's swim), else the quirk owned up to.
        const auto worn = mantleOf_.find(d.npc);
        const auto mantle = worn == mantleOf_.end() ? std::string() : worn->second;
        std::vector<talk::TreeChoice> grafts;
        if (!mantle.empty())
            talk_.graftChoices(mantle, "resident", "hub", grafts);
        if (!grafts.empty() && !grafts.front().go.empty() && grafts.front().go[0] == '@')
        {
            d.tree = "resident";
            d.asked = "ask.detail";
            d.awaiting = false;
            dialogueGo(c, d, grafts.front().go, note);
            return;
        }
        if (goTo("quirk", {}, "ask.detail"))
            return;
    }
    else if (in == "ask_match")
    {
        if (goTo("match", {}, "request"))
            return;
    }
    else if (in == "ask_missions")
    {
        if (take([](const talk::TreeChoice& ch) { return ch.action == "open" && !ch.args.empty() && ch.args[0] == "missions"; }))
            return;
        if (!officialOf(d.npc).empty() && openEntry("missions"))
            return;
    }
    else if (in == "ask_weather")
    {
        auto q = talkQuery(d.npc, playerId, "talk.weather", {});
        if (const auto said = talk_.pick(q, std::hash<std::string>{}(d.npc + text) + std::uint64_t(world_.time()), {}, {});
            !said.lines.empty() && said.lines.front().first == 0 && sayOver(said.lines.front().second, ""))
            return;
    }
    else if (in == "ask_help" || in == "ask_advice")
    {
        if (moveLine("ask.advice"))
            return;
    }
    else if (in == "offer_help")
    {
        if (moveLine("offer"))
            return;
    }
    else if (in == "compliment")
    {
        regard(d.npc, playerId, {1, 0, 0, 0, 0}, "talk", "praised in the window");
        if (moveLine("praise"))
            return;
    }
    else if (in == "insult")
    {
        regard(d.npc, playerId, {-3, -1, 0, 0, -1}, "talk", "insulted in the window");
        if (moveLine("take_offence"))
            return;
    }
    else if (in == "condole")
    {
        if (moveLine("sympathise") || moveLine("thank"))
            return;
    }
    else if (in == "apologise")
    {
        if (moveLine("apologise"))
            return;
    }
    else if (in == "joke")
    {
        if (moveLine("tease"))
            return;
    }
    else if (in == "agree" || in == "yes" || in == "no")
    {
        if (moveLine("ack"))
            return;
    }
    else if (in == "disagree")
    {
        if (moveLine("disagree") || moveLine("ack"))
            return;
    }
    // Understood, but nothing true to say to it here: in character, the choices again.
    if (!nodeOver("unclear"))
        sayOver("I'm not sure I follow you.", "");
}

// ------------------------------------------------------------------ The ledger with players (§13.4.7)

void Game::windowNote(const std::string& playerId, const DialogueSession& d)
{
    auto& ledger = world_.talkLedger();
    if (!ledger.enabled || d.topics.empty())
        return;
    const double day = world_.calendarDays();
    auto& note = ledger.note(d.npc, playerId, day);
    note.opener = playerId;
    note.outcome = "even";
    for (const auto& t : d.topics)
    {
        const auto colon = t.find(':');
        const auto topic = colon == std::string::npos ? t : t.substr(0, colon);
        const auto what = colon == std::string::npos ? std::string() : t.substr(colon + 1);
        if (std::find(note.topics.begin(), note.topics.end(), topic) == note.topics.end())
            note.topics.push_back(topic);
        if (!what.empty())
            note.keys.push_back(what);
        // A way or a person asked after: remembered between the two (the callback, ten days), and as a light person fact
        // the residents may talk of to each other ("That stranger was asking the way to the smithy.").
        if (topic == "way")
        {
            note.kept["asked_way"] = {what + "|" + std::to_string(day), day + 10};
            ledger.said(playerId, "asked", "asking the way to " + what, d.npc, day + .25);
        }
        else if (topic == "person")
        {
            note.kept["asked_person"] = {what + "|" + std::to_string(day), day + 10};
            ledger.said(playerId, "asked", "asking after " + what, d.npc, day + .25);
        }
    }
}
} // namespace ratw::game
