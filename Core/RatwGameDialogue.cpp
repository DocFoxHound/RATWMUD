// The dialogue window (Docs/Design/83-dialogue-trees.md): a conversation a player has with a resident by choosing what
// to say. The server runs the tree (Data/Talk/dialogue/*.tree, read by the talk library: RatwTalkTree.cpp); the client
// shows the node and sends back the number chosen, and the words the player chose to say (the choice's default line,
// or their own). Both sides are said aloud in the world, as any speech is. Actions are the game's own handlers.
//
//   client -> server   {"type": "dialogue_open", "npc", "entry"}           trade, talk
//                      {"type": "dialogue_choose", "session", "n", "text"?} text: the player's own words for the choice
//                      {"type": "dialogue_say", "session", "text"}         free talk, where the node allows it
//                      {"type": "dialogue_close", "session", "why"?}       bye (Esc), shop (the shop window closed)...
//   server -> client   {"type": "dialogue", ...}                           the node: the line, the choices, the face
//                      {"type": "dialogue_close", "session", "why"}
#include "RatwGame.h"
#include "RatwWire.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace ratw::game
{
namespace
{
constexpr double Reach = 3;             // Tiles: near enough to open a conversation (the client walks over first).
constexpr double WalkedAway = 4;        // Further than this, the conversation ends.
constexpr std::size_t MostWords = 300;  // Characters of a line the player writes.
constexpr std::size_t Kept = 4;         // Lines of history kept (two exchanges).

std::string oneLine(std::string s)
{
    for (auto& c : s)
        if (c == '\n' || c == '\r' || c == '\t')
            c = ' ';
    const auto a = s.find_first_not_of(' ');
    if (a == std::string::npos)
        return {};
    s = s.substr(a, s.find_last_not_of(' ') - a + 1);
    if (s.size() > MostWords)
    {
        s.resize(MostWords);
        while (!s.empty() && (static_cast<unsigned char>(s.back()) & 0xC0) == 0x80)
            s.pop_back();                           // (Never half a character.)
        if (!s.empty() && static_cast<unsigned char>(s.back()) >= 0xC0)
            s.pop_back();
    }
    return s;
}

// The words of a line, lower case, for telling a player's own line from the default they were offered.
std::vector<std::string> wordsOf(const std::string& s)
{
    std::vector<std::string> out;
    std::string w;
    for (const char ch : s)
    {
        const auto c = static_cast<unsigned char>(ch);
        if (std::isalnum(c) || c >= 128 || c == '\'')
            w += char(std::tolower(c));
        else if (!w.empty())
            out.push_back(w), w.clear();
    }
    if (!w.empty())
        out.push_back(w);
    return out;
}
} // namespace

bool Game::offerContract(Connection* c, const std::string& target, const std::string& contractId)
{
    const auto id = c->entityId;
    const auto* player = world_.entity(id);
    const auto* giver = world_.entity(target);
    const auto offered = world_.contractsNear(id);
    const auto found = std::find_if(offered.begin(), offered.end(), [&](const Contract* k) { return k->id == contractId; });
    if (!player || !giver || !giver->npc || !world_.society().merchant(target) || giver->cellId != player->cellId ||
        std::hypot(giver->position.x - player->position.x, giver->position.y - player->position.y) > 3 || found == offered.end())
    {
        system(c, "Ask the merchant about that work in person.");
        return false;
    }
    const auto& k = **found;
    auto offer = json::Value::object();
    offer.add("type", "missionOffer");
    offer.add("contract", k.id);
    offer.add("giver", names::capitalised(labelFor(id, target)));
    offer.add("appearance", wire::appearance(giver->appearance));
    offer.add("title", k.detail);
    offer.add("reward", double(k.reward));
    std::string directions;
    if (k.kind == "escort")
        directions = "Meet the wagon at the " + world_.marketWords(k.from.empty()
            ? (world_.roadMarketOf(player->cellId) ? world_.roadMarketOf(player->cellId)->id : std::string()) : k.from) +
                     " market. It waits for its guards before setting out. Stay within twelve tiles of the wagon along the road to " +
                     world_.marketWords(k.town) + "; you must be with it for at least half the route and when it arrives.";
    else if (k.kind == "courier")
        directions = "Carry the letter to " + labelFor(id, k.target) + ". Find them and stand within three tiles to hand it over.";
    else if (k.kind == "bounty")
        directions = "Find the bandit camp on the map and clear it. The bounty is paid to the wolf who defeats the camp.";
    offer.add("explanation", directions);
    send(c, offer);
    return true;
}

// ------------------------------------------------------------------ What the tree is asked

talk::Query Game::dialogueQuery(const std::string& playerId, const DialogueSession& d)
{
    talk::Facts facts;
    const auto& society = world_.society();
    const auto* bond = world_.bonds().find(d.npc, playerId);
    facts["topic.meeting"] = bond && bond->familiarity >= 25 ? "known" : "stranger";
    if (knowsName(d.npc, playerId))
        facts["topic.player"] = labelFor(d.npc, playerId);   // (Only a name they were given: doc 32.)
    else if (facts["topic.meeting"] == "known")
        facts["topic.meeting"] = "stranger";       // ("Back again, #topic.player#" needs the name.)
    const auto* npc = world_.entity(d.npc);
    const auto* life = society.resident(d.npc);
    const bool asleep = npc && (npc->posture == "lying" || (life && life->task == "sleep"));
    std::string shop = "none";
    if (society.merchant(d.npc))
    {
        shop = asleep ? "asleep" : "bare";
        if (!asleep)
            if (const auto* till = society.account(society.tillOf(d.npc)))
                for (const auto& w : society.wares(d.npc))
                    if (Society::stock(*till, w) > 0)
                    {
                        shop = "open";
                        break;
                    }
    }
    facts["here.shop"] = shop;
    facts["here.work"] = std::to_string(society.merchant(d.npc) && !asleep ? world_.contractsNear(playerId).size() : 0);
    for (const auto& [k, v] : d.bound)
        facts[k] = v;
    return talkQuery(d.npc, playerId, "", facts);
}

std::vector<talk::Facts> Game::dialogueItems(const std::string& playerId, const std::string& npcId, const std::string& list)
{
    std::vector<talk::Facts> out;
    if (list != "here.work" || !world_.society().merchant(npcId))
        return out;
    for (const auto* k : world_.contractsNear(playerId))
        out.push_back({{"topic.job_id", k->id},
                       {"topic.job_title", k->detail},
                       {"topic.job_reward", std::to_string(k->reward)},
                       {"topic.job_kind", k->kind}});
    return out;
}

// ------------------------------------------------------------------ Said aloud

void Game::dialogueSpeak(const std::string& speaker, const std::string& to, const std::string& words)
{
    // What's said in the window is said in the world (doc 83, decision 8; the user's answer 2): ordinary speech, heard
    // and seen as any is (a sneaker unseen hears it as a voice, or not at all), kept in the story log with the face.
    if (words.empty())
        return;
    ParsedPost post;
    post.ok = true;
    post.speech = true;
    post.segments.push_back({"speech", words});
    publish(speaker, post, Voice::Speak, {to});
    const auto* e = world_.entity(speaker);
    if (e && e->npc)
    {
        npcLastSpeech_[speaker] = world_.time();
        memories_.record(speaker, to, {sequence_++, now(), speaker, words});
        npcSpokeTo(speaker, to);
    }
    else if (const auto* npc = world_.entity(to))
    {
        // The resident heard it: it is in their memory of this wolf, and a storyline's "talk to" counts (doc 58).
        memories_.record(to, speaker, {sequence_++, now(), labelFor(to, speaker), words});
        storylineEvent({"talk", speaker, npc->cellId, to, {}, 0, 0, {}});
        logEvent("conversation", speaker, to);
    }
    saveSoon();
}

int Game::dialogueExperience(const std::string& playerId, const std::string& npcId, const std::string& said,
                             const std::string& offered)
{
    // Roleplay experience for the player's own words (the user, 2026-11-15: "These should count for roleplay
    // experience"): never the default line or a near copy of it, never a short or repeated line (doc 51's evidence: five
    // words or more, the same words not twice in a day), at most one line in fifteen seconds, and an allowance a day
    // (with each resident, and in all) inside the social daily cap.
    const auto mine = wordsOf(said), given = wordsOf(offered);
    if (mine.size() < 5)
        return 0;
    if (!given.empty())
    {
        const std::set<std::string> a(mine.begin(), mine.end()), b(given.begin(), given.end());
        std::size_t both = 0;
        for (const auto& w : a)
            both += b.count(w);
        const double alike = double(both) / double(a.size() + b.size() - both);
        if (mine == given || alike >= .8)
            return 0;
    }
    ParsedPost post;
    post.ok = post.speech = true;
    post.segments.push_back({"speech", said});
    const auto evidence = roleplayEvidence(post);
    return social_.dialogueLine(playerId, npcId, evidence.words, evidence.contentHash, now(), sequence_++);
}

// ------------------------------------------------------------------ The conversation

void Game::dialogueSend(Connection* c, const DialogueSession& d, const std::string& note)
{
    if (!c)
        return;
    const auto* npc = world_.entity(d.npc);
    const auto* t = talk_.tree(d.tree);
    const auto* node = t && t->nodes.count(d.node) ? &t->nodes.at(d.node) : nullptr;
    if (!npc || !node)
        return;
    auto m = json::Value::object();
    m.add("type", "dialogue");
    m.add("session", d.id);
    m.add("npc", d.npc);
    m.add("name", names::capitalised(labelFor(c->entityId, d.npc)));
    std::string role;
    if (const auto* job = world_.society().jobOf(d.npc))
        role = job->title;
    else if (const auto* spec = world_.society().spec(d.npc))
        role = spec->workLabel == "-" ? std::string() : spec->workLabel;
    m.add("role", role);
    m.add("place", world_.cell(npc->cellId) ? world_.cell(npc->cellId)->name : std::string());
    m.add("appearance", wire::appearance(npc->appearance));
    m.add("tree", d.tree);
    m.add("node", d.node);
    m.add("line", d.line);
    m.add("mood", node->mood);
    m.add("free", node->free);
    m.add("awaiting", d.awaiting);
    m.add("ends", node->choices.empty());
    auto choices = json::Value::array();
    for (const auto& s : d.shown)
    {
        auto o = json::Value::object();
        o.add("n", s.n);
        o.add("text", s.text);
        if (s.locked)
        {
            o.add("locked", true);
            o.add("why", s.why);
        }
        choices.push(o);
    }
    m.add("choices", choices);
    auto history = json::Value::array();
    for (std::size_t i = 0; i + 1 < d.history.size(); ++i)      // (The last is the line itself, or the player's.)
    {
        auto o = json::Value::object();
        o.add("who", d.history[i].first);
        o.add("text", d.history[i].second);
        history.push(o);
    }
    if (!d.history.empty() && d.history.back().first == "you")
    {
        auto o = json::Value::object();
        o.add("who", "you");
        o.add("text", d.history.back().second);
        history.push(o);
    }
    m.add("history", history);
    if (!note.empty())
        m.add("note", note);
    send(c, m);
}

bool Game::dialogueGo(Connection* c, DialogueSession& d, const std::string& nodeId, const std::string& note)
{
    const auto playerId = c->entityId;
    const auto* t = talk_.tree(d.tree);
    if (nodeId == "end" || !t || !t->nodes.count(nodeId))
    {
        dialogueClose(playerId, "end", false);
        return false;
    }
    const auto& node = t->nodes.at(nodeId);
    d.node = nodeId;
    auto q = dialogueQuery(playerId, d);
    q.concept = node.concept;
    const auto seed = std::hash<std::string>{}(d.npc + "|" + playerId + "|" + nodeId) + std::uint64_t(world_.time() * 10);
    std::string line;
    // A node that asks the game (`ask player.trade`): the answer the router's intent would give, in the resident's
    // voice (doc 28's facts, doc 62's rules); the node's own words where the game has nothing to say.
    if (node.ask.rfind("player.", 0) == 0)
        line = gameAnswer(d.npc, playerId, std::string(), true, node.ask.substr(7));
    if (line.empty())
    {
        const auto said = talk_.pick(q, seed, {}, {});
        if (!said.lines.empty())
        {
            line = said.lines.front().second;
            talkRemember(said.effects);
        }
    }
    d.line = line;
    d.awaiting = false;
    d.changed = world_.time();
    d.shown = talk_.choices(node, q, seed, [&](const std::string& list) { return dialogueItems(playerId, d.npc, list); });
    d.history.push_back({"npc", line});
    while (d.history.size() > Kept)
        d.history.pop_front();
    world_.faceTalker(d.npc, playerId);
    dialogueSpeak(d.npc, playerId, line);
    voiced("dialogue", "tree", d.npc);
    dialogueSend(c, d, note);
    if (node.choices.empty())
    {
        dialogueClose(playerId, "end", false);     // (Its last line said: the window shows it, and closes.)
        return false;
    }
    return true;
}

void Game::dialogueOpen(Connection* c, const std::string& npcId, const std::string& entry)
{
    const auto playerId = c->entityId;
    const auto* player = world_.entity(playerId);
    const auto* npc = world_.entity(npcId);
    const auto refuse = [&](const std::string& why, const std::string& words) {
        auto m = json::Value::object();
        m.add("type", "dialogue_close");
        m.add("npc", npcId);
        m.add("entry", entry);
        m.add("why", why);
        if (!words.empty())
        {
            m.add("text", words);
            system(c, words);
        }
        send(c, m);
    };
    if (entry != "trade" && entry != "talk")
    {
        refuse("none", "");
        return;
    }
    if (!player || !npc || !npc->npc || npc->dead || !world_.shown(playerId, npcId))
    {
        refuse("sight", "You cannot see that wolf.");
        return;
    }
    if (npc->transient)
    {
        refuse("busy", world_.hostile(npcId) ? "They want your purse, not your conversation." : npc->name + " has nothing to say to you just now.");
        return;
    }
    if (npc->cellId != player->cellId ||
        std::hypot(npc->position.x - player->position.x, npc->position.y - player->position.y) > Reach)
    {
        refuse("far", "");                          // (The client walks over, and asks again.)
        return;
    }
    if (world_.inBattle(playerId) || world_.inBattle(npcId) || player->downedLeft > 0)
    {
        refuse("fight", "Not now: there's fighting.");
        return;
    }
    if (entry == "trade" && !world_.society().merchant(npcId))
    {
        refuse("none", names::capitalised(labelFor(playerId, npcId)) + " keeps no shop.");
        return;
    }
    if (dialogues_.count(playerId))
        dialogueClose(playerId, "replaced", false);
    DialogueSession d;
    d.id = dialogueNext_++;
    d.npc = npcId;
    d.entry = entry;
    const auto q = dialogueQuery(playerId, d);
    const auto* t = talk_.treeFor(entry, q);
    if (!t)
    {
        refuse("none", "");                         // (No tree for it: the old way, the shop window or the talk target.)
        return;
    }
    d.tree = t->id;
    const auto* life = world_.society().resident(npcId);
    d.asleepAtOpen = npc->posture == "lying" || (life && life->task == "sleep");
    if (entry == "trade")
        shopWith_[playerId] = npcId;                 // (Its list in the snapshot, before a nearer keeper's.)
    auto& kept = dialogues_[playerId] = std::move(d);
    dialogueGo(c, kept, t->entries.at(entry));
}

void Game::dialogueClose(const std::string& playerId, const std::string& why, bool sayBye)
{
    const auto f = dialogues_.find(playerId);
    if (f == dialogues_.end())
        return;
    const auto d = f->second;
    dialogues_.erase(f);
    if (sayBye)
        if (const auto* t = talk_.tree(d.tree); t && t->nodes.count("bye"))
        {
            DialogueSession last = d;
            auto q = dialogueQuery(playerId, last);
            q.concept = t->nodes.at("bye").concept;
            const auto said = talk_.pick(q, std::hash<std::string>{}(d.npc + playerId) + std::uint64_t(world_.time() * 10), {}, {});
            if (!said.lines.empty())
                dialogueSpeak(d.npc, playerId, said.lines.front().second);
        }
    if (auto* c = clientOf(playerId))
    {
        auto m = json::Value::object();
        m.add("type", "dialogue_close");
        m.add("session", d.id);
        m.add("npc", d.npc);
        m.add("why", why);
        send(c, m);
    }
}

void Game::dialogueCommand(Connection* c, const std::string& type, const json::Value& j)
{
    const auto playerId = c->entityId;
    if (type == "dialogue_open")
    {
        dialogueOpen(c, j.string("npc"), j.string("entry", "talk"));
        return;
    }
    const auto f = dialogues_.find(playerId);
    const int session = int(wire::number(j, "session"));
    if (f == dialogues_.end() || f->second.id != session)
    {
        auto m = json::Value::object();
        m.add("type", "dialogue_close");
        m.add("session", session);
        m.add("why", "gone");
        send(c, m);
        return;
    }
    auto& d = f->second;
    const auto* t = talk_.tree(d.tree);
    const auto* node = t && t->nodes.count(d.node) ? &t->nodes.at(d.node) : nullptr;
    if (type == "dialogue_close")
    {
        const auto why = j.string("why", "bye");
        dialogueClose(playerId, why == "shop" || why == "bye" ? why : std::string("closed"), why == "bye" || why == "shop");
        return;
    }
    if (!node)
    {
        dialogueClose(playerId, "gone", false);
        return;
    }
    if (type == "dialogue_say")
    {
        // Free talk (doc 83, the user's answer 1): the player's own words, said aloud, answered by the resident (the
        // router and the library first, then the NPC Mind, which never takes or gives anything).
        const auto text = oneLine(j.string("text"));
        if (!node->free || text.empty())
        {
            dialogueSend(c, d, node->free ? "Say something first." : "Choose what to say.");
            return;
        }
        if (d.awaiting)
        {
            dialogueSend(c, d, "Let them answer first.");
            return;
        }
        if (lastChat_.count(playerId) && world_.time() - lastChat_[playerId] < 1)
        {
            dialogueSend(c, d, "Please leave a moment between lines.");
            return;
        }
        lastChat_[playerId] = world_.time();
        ParsedPost post;
        post.ok = post.speech = true;
        post.segments.push_back({"speech", text});
        publish(playerId, post, Voice::Speak, {d.npc});
        const int xp = dialogueExperience(playerId, d.npc, text, std::string());
        d.history.push_back({"you", text});
        while (d.history.size() > Kept)
            d.history.pop_front();
        d.awaiting = true;
        d.changed = world_.time();
        const int id = d.id;
        if (!talk(d.npc, playerId, text, Voice::Speak))
        {
            if (auto again = dialogues_.find(playerId); again != dialogues_.end() && again->second.id == id)
            {
                again->second.awaiting = false;
                dialogueSend(c, again->second, "They didn't catch that.");
            }
            return;
        }
        if (auto again = dialogues_.find(playerId); again != dialogues_.end() && again->second.id == id && again->second.awaiting)
            dialogueSend(c, again->second, xp > 0 ? "+" + std::to_string(xp) + " roleplay experience" : std::string());
        return;
    }
    if (type != "dialogue_choose")
        return;
    // A choice: checked again as it stands now, then its words said, then what it does.
    const int n = int(wire::number(j, "n"));
    const auto shown = std::find_if(d.shown.begin(), d.shown.end(), [&](const talk::ShownChoice& s) { return s.n == n; });
    if (shown == d.shown.end())
    {
        dialogueSend(c, d, "Choose one of the numbers shown.");
        return;
    }
    auto q = dialogueQuery(playerId, d);
    q.concept = node->concept;
    const auto now_ = talk_.choices(*node, q, 0, [&](const std::string& list) { return dialogueItems(playerId, d.npc, list); });
    const auto still = std::find_if(now_.begin(), now_.end(), [&](const talk::ShownChoice& s) {
        return s.index == shown->index && s.item == shown->item && s.bound == shown->bound;
    });
    if (still == now_.end() || still->locked)
    {
        d.shown = now_;
        dialogueSend(c, d, still != now_.end() ? still->why : "That's no longer on offer.");
        return;
    }
    const auto choice = node->choices[shown->index];
    const auto offered = shown->text;
    const auto bound = shown->bound;
    std::string words = oneLine(j.string("text"));
    if (words.empty())
        words = offered;
    // The choice picks the branch; the words are what the wolf says (the user's answer 1): said aloud, and the player's
    // own earning roleplay experience.
    dialogueSpeak(playerId, d.npc, words);
    const int xp = words == offered ? 0 : dialogueExperience(playerId, d.npc, words, offered);
    const std::string earned = xp > 0 ? "+" + std::to_string(xp) + " roleplay experience" : std::string();
    d.history.push_back({"you", words});
    while (d.history.size() > Kept)
        d.history.pop_front();
    for (const auto& [k, v] : bound)
        d.bound[k] = v;
    if (!choice.go.empty())
    {
        dialogueGo(c, d, choice.go, earned);
        return;
    }
    if (choice.action == "end")
    {
        dialogueClose(playerId, "end", false);
        return;
    }
    if (choice.action == "free")
    {
        dialogueSend(c, d, earned);
        return;
    }
    if (choice.action == "shop")
    {
        // The shop window, on this keeper's list (as Trade on the wheel did): the conversation stays open behind it, and
        // closing it ends the conversation with the keeper's farewell.
        const auto* npc = world_.entity(d.npc);
        const auto* player = world_.entity(playerId);
        const auto* life = world_.society().resident(d.npc);
        const bool asleep = npc && (npc->posture == "lying" || (life && life->task == "sleep"));
        if (!npc || !player || asleep || !world_.society().merchant(d.npc) ||
            std::hypot(npc->position.x - player->position.x, npc->position.y - player->position.y) > 2.5)
        {
            const std::string why = asleep ? "They're asleep." : "Step up to the counter first.";
            if (!choice.refused.empty())
            {
                d.bound["topic.why"] = why;
                dialogueGo(c, d, choice.refused, why);
            }
            else
                dialogueSend(c, d, why);
            return;
        }
        shopWith_[playerId] = d.npc;
        d.inShop = true;
        if (!choice.then.empty())
        {
            dialogueGo(c, d, choice.then, earned);
            return;
        }
        auto m = json::Value::object();
        m.add("type", "dialogue_shop");
        m.add("session", d.id);
        m.add("npc", d.npc);
        m.add("line", d.line);
        if (!earned.empty())
            m.add("note", earned);
        send(c, m);
        return;
    }
    if (choice.action == "offer")
    {
        // A contract's offer window (until Phase 2's work tree): the conversation ends as it opens.
        const auto contract = bound.count("topic.job_id") ? bound.at("topic.job_id") : std::string();
        dialogueClose(playerId, "offer", false);
        offerContract(c, d.npc, contract);
        return;
    }
    dialogueSend(c, d, earned);
}

void Game::dialogueHeard(const std::string& npcId, const std::string& playerId, const std::string& text)
{
    // The resident answered free talk: the answer shows in the window (it was said aloud already: speakReply).
    const auto f = dialogues_.find(playerId);
    if (f == dialogues_.end() || f->second.npc != npcId)
        return;
    auto& d = f->second;
    d.line = text;
    d.awaiting = false;
    d.changed = world_.time();
    d.history.push_back({"npc", text});
    while (d.history.size() > Kept)
        d.history.pop_front();
    dialogueSend(clientOf(playerId), d);
}

std::string Game::dialogueBrief(const std::string& npcId, const std::string& playerId) const
{
    // For the NPC Mind (doc 83, decision 7): which conversation it is in, what it last said and offered, and that only
    // the choices do anything.
    const auto f = dialogues_.find(playerId);
    if (f == dialogues_.end() || f->second.npc != npcId)
        return {};
    const auto& d = f->second;
    std::string offered;
    for (const auto& s : d.shown)
        if (!s.locked)
            offered += (offered.empty() ? "" : "; ") + s.text;
    return " They are speaking with you face to face (conversation: " + d.tree + ", " + d.node + "). You last said: \"" +
           mind::left(d.line, 200) + "\". They could also choose: " + mind::left(offered, 300) +
           ". Answer what they said in a line or two. Never take, give, sell or promise anything: only their choices do that.";
}

void Game::dialogueTick(double dt)
{
    // Once a second: a conversation ends when the wolf walks away, the resident falls asleep, goes out of sight or
    // into a fight, or the player is downed (doc 83, decision 9). The world doesn't pause for it.
    if (dialogues_.empty() || (dialogueLookIn_ += dt) < 1)
        return;
    dialogueLookIn_ = 0;
    std::vector<std::pair<std::string, std::string>> ending;
    for (auto& [playerId, d] : dialogues_)
    {
        if (d.awaiting && world_.time() - d.changed > 30)
        {
            d.awaiting = false;                     // (No answer came: the field is theirs again.)
            dialogueSend(clientOf(playerId), d, "No answer.");
        }
        const auto* player = world_.entity(playerId);
        const auto* npc = world_.entity(d.npc);
        const auto* life = npc ? world_.society().resident(d.npc) : nullptr;
        const bool asleep = npc && (npc->posture == "lying" || (life && life->task == "sleep"));
        std::string why;
        if (!clientOf(playerId) || !player || !npc || npc->dead || player->dead)
            why = "gone";
        else if (npc->cellId != player->cellId ||
                 std::hypot(npc->position.x - player->position.x, npc->position.y - player->position.y) > WalkedAway)
            why = "away";
        else if (!world_.shown(playerId, d.npc))
            why = "sight";
        else if (asleep && !d.asleepAtOpen)
            why = "asleep";
        else if (world_.inBattle(playerId) || world_.inBattle(d.npc))
            why = "fight";
        else if (player->downedLeft > 0)
            why = "downed";
        if (!why.empty())
            ending.emplace_back(playerId, why);
    }
    for (const auto& [playerId, why] : ending)
        dialogueClose(playerId, why, false);
}
} // namespace ratw::game
