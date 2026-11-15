// The dialogue window (Docs/Design/83-dialogue-trees.md): a conversation a player has with a resident by choosing what
// to say. The server runs the tree (Data/Talk/dialogue/*.tree, read by the talk library: RatwTalkTree.cpp); the client
// shows the node and sends back the number chosen, and the words the player chose to say (the choice's default line,
// or their own). Both sides are said aloud in the world, as any speech is. Actions are the game's own handlers.
//
//   client -> server   {"type": "dialogue_open", "npc", "entry"}           trade, talk, work, job:<id>, missions, mine,
//                                                                           deliver:<id> (a giver's work: Phase 2)
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

std::string pennies(std::int64_t n)
{
    return std::to_string(n) + (n == 1 ? " penny" : " pennies");
}

// A faction official's answer comes quoted ("\"The faction thanks your Chapter.\""): in the window it is their line.
std::string unquoted(const std::string& s)
{
    const auto a = s.find('"'), b = s.rfind('"');
    return a == 0 && b != std::string::npos && b > a ? s.substr(1, b - 1) : s;
}

std::string dueWords(double days)
{
    if (days < 1)
        return "before this day is out";
    const int n = int(std::floor(days));
    return n == 1 ? "within a day" : "within " + std::to_string(n) + " days";
}

std::string missionGoods(const faction::Mission& m)
{
    const bool meal = m.item == "meal";
    if (m.quantity == 1)
        return meal ? "a meal" : "a bundle of herbs";
    return std::to_string(m.quantity) + (meal ? " meals" : " bundles of herbs");
}

std::string jobOf(const talk::ShownChoice& s)
{
    const auto f = s.bound.find("topic.job_id");
    return f == s.bound.end() ? std::string() : f->second;
}

bool isMission(const std::string& id)
{
    return id.rfind("mission-", 0) == 0;
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
    const auto directions = jobDirections(id, k);
    offer.add("explanation", directions);
    send(c, offer);
    return true;
}

// ------------------------------------------------------------------ A giver's work, in words

std::string Game::jobDirections(const std::string& viewer, const Contract& k) const
{
    // Where, what and how: the one place it is said (the dialogue's details and the older offer window).
    const auto* player = world_.entity(viewer);
    if (k.kind == "escort")
    {
        const auto* here = player ? world_.roadMarketOf(player->cellId) : nullptr;
        return "Meet the wagon at the " + world_.marketWords(k.from.empty() ? (here ? here->id : std::string()) : k.from) +
               " market. It waits for its guards before setting out. Stay within twelve tiles of the wagon along the road to " +
               world_.marketWords(k.town) + "; you must be with it for at least half the route and when it arrives.";
    }
    if (k.kind == "courier")
        return "Carry the letter to " + labelFor(viewer, k.target) + ". Find them, and hand it over in person.";
    if (k.kind == "bounty")
        return "Find the bandit camp on the map and clear it. The bounty is paid to the wolf who defeats the camp.";
    return k.detail.empty() ? std::string("It's work, and it pays.") : names::capitalised(k.detail) + ".";
}

std::string Game::jobDirections(const std::string& viewer, const faction::Mission& m) const
{
    const auto official = labelFor(viewer, m.official);
    if (m.kind == "guard")
        return "Keep watch at " + m.place + ", within twelve tiles of " + official + ", for one unbroken stretch of " +
               std::to_string(int(m.seconds / 60)) + " minutes. Finish before this day ends.";
    if (m.kind == "message")
        return "Carry this sealed letter to " + labelFor(viewer, m.to) + (m.place.empty() ? "" : " at " + m.place) +
               ". Hand it to them in person before this day ends.";
    return "Bring " + missionGoods(m) + " to " + official + " before this day ends, and hand them over in person.";
}

bool Game::contractOfMarket(const Contract& k, const std::string& npcId) const
{
    const auto* npc = world_.entity(npcId);
    const auto* here = npc ? world_.roadMarketOf(npc->cellId) : nullptr;
    if (!here)
        return false;
    if (k.town == here->id || k.from == here->id)
        return true;
    const auto* poster = world_.entity(k.poster);
    return k.kind == "courier" && poster && world_.roadMarketOf(poster->cellId) == here;
}

talk::Facts Game::jobFacts(const std::string& playerId, const std::string& npcId, const std::string& jobId)
{
    // A job as the player stands with this resident now (doc 83, Phase 2): the tree shows only what the server would
    // allow, and says why not when it wouldn't. (npcId: the one asked; the facts are the job's, whoever is asked.)
    static_cast<void>(npcId);
    talk::Facts f{{"topic.job_id", jobId}, {"topic.job_state", "gone"}, {"topic.job_allowed", "no"}, {"topic.job_ready", "no"},
                  {"topic.job_why", "That work is gone."}, {"topic.job_title", "that work"}, {"topic.job_reward", "0"},
                  {"topic.job_directions", "That work is gone."}, {"topic.job_due", "no time at all"},
                  {"topic.job_risk", "None now."}, {"topic.job_progress", "That's done with."}, {"topic.job_handover", "About that work."},
                  {"topic.job_poster", "someone"}};
    const double today = world_.calendarDays();
    if (isMission(jobId))
    {
        const auto* m = factions_.mission(jobId);
        if (!m)
            return f;
        f["topic.job_source"] = "mission";
        f["topic.job_kind"] = m->kind;
        f["topic.job_state"] = m->state;
        f["topic.job_reward"] = std::to_string(m->coins);
        f["topic.job_directions"] = jobDirections(playerId, *m);
        f["topic.job_poster"] = labelFor(playerId, m->official);
        f["topic.job_due"] = dueWords(m->expires - today);
        {
            std::string what;
            if (m->kind == "deliver")
                what = "Bring " + missionGoods(*m) + " to " + labelFor(playerId, m->official);
            else if (m->kind == "message")
                what = "Carry a sealed letter to " + labelFor(playerId, m->to) + (m->place.empty() ? "" : " at " + m->place);
            else
                what = "Keep watch at " + m->place + " for " + std::to_string(int(m->seconds / 60)) + " minutes";
            f["topic.job_title"] = what;
        }
        f["topic.job_risk"] = m->kind == "guard"     ? "Only the hours, and whatever comes by while you watch."
                              : m->kind == "message" ? "Little, if you're quick about it."
                                                     : "None, if you can find the goods.";
        const auto* chapter = chapters_.of(playerId);
        const double eff = chapter ? standingOf(m->faction, chapter->id) : 0;
        const bool trusted = chapter && (m->tier == 1 ? chapter->level >= 2 && eff >= 15 : chapter->level >= 3 && eff >= 40);
        if (m->state == "open")
        {
            f["topic.job_allowed"] = trusted ? "yes" : "no";
            f["topic.job_why"] = !chapter      ? "Factions deal with Chapters, not with one wolf alone."
                                 : trusted     ? std::string()
                                 : m->tier == 1 ? "For a Lodge the faction knows well."
                                                : "For a Company the faction trusts.";
        }
        else if (m->state == "taken")
            f["topic.job_why"] = m->taker == playerId ? "You've taken that on already." : "Another Chapter has taken that on.";
        else
            f["topic.job_why"] = m->state == "done" ? "That's been done." : "That's lapsed.";
        if (m->state == "taken" && m->taker == playerId)
        {
            if (m->kind == "guard")
                f["topic.job_progress"] = m->held > 0 ? "Keep watch, unbroken: " + std::to_string(std::max(1, int(std::ceil((m->seconds - m->held) / 60)))) +
                                                            " minutes more."
                                                      : "Keep watch near " + labelFor(playerId, m->official) + " for " +
                                                            std::to_string(int(m->seconds / 60)) + " minutes, unbroken.";
            else if (m->kind == "message")
                f["topic.job_progress"] = "That letter's still to go to " + labelFor(playerId, m->to) + ".";
            else
                f["topic.job_progress"] = "Still waiting on " + missionGoods(*m) + ".";
            if (m->kind == "deliver")
            {
                const auto* purse = world_.society().account(playerId);
                const bool has = purse && Society::stock(*purse, m->item) >= m->quantity;
                f["topic.job_ready"] = has ? "yes" : "no";
                f["topic.job_why"] = has ? std::string() : "You haven't " + missionGoods(*m) + ".";
                f["topic.job_handover"] = "I've brought what you asked for: " + missionGoods(*m) + ".";
            }
            else if (m->kind == "message")
            {
                f["topic.job_ready"] = "yes";
                f["topic.job_handover"] = "A sealed letter for you, from " + labelFor(playerId, m->official) + ".";
            }
        }
        return f;
    }
    const Contract* k = nullptr;
    for (const auto& c : world_.roads().contracts)
        if (c.id == jobId)
            k = &c;
    if (!k)
        return f;
    f["topic.job_source"] = "contract";
    f["topic.job_kind"] = k->kind;
    f["topic.job_state"] = k->status;
    f["topic.job_title"] = k->detail;
    f["topic.job_reward"] = std::to_string(k->reward);
    f["topic.job_directions"] = jobDirections(playerId, *k);
    f["topic.job_due"] = dueWords(k->due - today);
    f["topic.job_poster"] = world_.entity(k->poster) ? labelFor(playerId, k->poster) : std::string("a friend");
    f["topic.job_risk"] = k->kind == "escort"    ? "Bandits watch the roads. If the wagon's robbed, there's no pay."
                          : k->kind == "bounty"  ? "They'll fight. Go armed, and better not alone."
                          : k->kind == "courier" ? "Little, unless the road's unkind."
                                                 : "Nothing to speak of.";
    if (k->status == "open")
    {
        const auto near = world_.contractsNear(playerId);
        const bool here = std::find(near.begin(), near.end(), k) != near.end();
        const bool promised = !k->offeredTo.empty() && k->offeredTo != playerId && k->offeredUntil > today;
        const bool own = k->poster == playerId;
        f["topic.job_allowed"] = here && !promised && !own ? "yes" : "no";
        f["topic.job_why"] = own        ? "That's your own work."
                             : promised ? "That's promised to another for now."
                             : !here    ? "That's asked for at another market."
                                        : std::string();
    }
    else if (k->status == "taken")
        f["topic.job_why"] = k->taker == playerId ? "You've taken that on already." : "Someone's taken that on already.";
    else
        f["topic.job_why"] = k->status == "done" ? "That's been done." : "That's gone, I'm afraid.";
    if (k->status == "taken" && k->taker == playerId)
    {
        if (k->kind == "courier")
        {
            f["topic.job_progress"] = labelFor(playerId, k->target) + "'s still waiting on that letter.";
            f["topic.job_ready"] = "yes";
            f["topic.job_handover"] = "I've a letter for you, from " + f["topic.job_poster"] + ".";
        }
        else if (k->kind == "escort")
        {
            bool out = false;
            for (const auto& wagon : world_.roads().caravans)
                out |= wagon.status == "travelling" && std::find(wagon.escorts.begin(), wagon.escorts.end(), playerId) != wagon.escorts.end();
            f["topic.job_progress"] = out ? "The wagon's on the road to " + world_.marketWords(k->town) + ". Stay with it."
                                          : "The wagon waits at the " +
                                                world_.marketWords(k->from.empty() ? k->town : k->from) + " market for its guards.";
        }
        else if (k->kind == "bounty")
            f["topic.job_progress"] = "That camp's still out there.";
        else
            f["topic.job_progress"] = "It's still to be done.";
    }
    return f;
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
    // A giver's work (Phase 2): the counts of each list a tree may go through, whether this is a faction's official,
    // and the bound job's facts as they stand now (taken by someone else meanwhile, expired, the goods to hand).
    const auto faction = officialOf(d.npc);
    const auto* f = faction.empty() ? nullptr : factions_.find(faction);
    const auto work = dialogueItems(playerId, d.npc, "here.work").size(), mine = dialogueItems(playerId, d.npc, "here.mine").size();
    facts["here.work"] = std::to_string(work);
    facts["here.mine"] = std::to_string(mine);
    facts["here.missions"] = std::to_string(dialogueItems(playerId, d.npc, "here.missions").size());
    facts["here.deliveries"] = std::to_string(dialogueItems(playerId, d.npc, "here.deliveries").size());
    facts["here.giver"] = society.merchant(d.npc) && !asleep && (work > 0 || mine > 0) ? "yes" : "no";
    facts["here.official"] = faction.empty() ? "no" : "yes";
    facts["here.faction"] = f ? f->name : faction;
    facts["here.chapter"] = chapters_.of(playerId) ? "yes" : "no";
    facts["here.report_fee"] = std::to_string(reportFee(playerId));
    facts["here.tithe_fee"] = std::to_string(titheFee(playerId));
    for (const auto& [k, v] : d.bound)
        facts[k] = v;
    if (const auto job = d.bound.find("topic.job_id"); job != d.bound.end())
        for (const auto& [k, v] : jobFacts(playerId, d.npc, job->second))
            facts[k] = v;
    return talkQuery(d.npc, playerId, "", facts);
}

std::vector<talk::Facts> Game::dialogueItems(const std::string& playerId, const std::string& npcId, const std::string& list)
{
    // The lists a tree's `each` goes through (Library::treeLists), as the server would allow them now.
    std::vector<talk::Facts> out;
    const auto& society = world_.society();
    const bool merchant = society.merchant(npcId);
    const auto faction = officialOf(npcId);
    std::vector<std::string> ids;
    if (list == "here.work" && merchant)
    {
        const auto* npc = world_.entity(npcId);
        const auto* life = society.resident(npcId);
        if (npc && npc->posture != "lying" && !(life && life->task == "sleep"))
            for (const auto* k : world_.contractsNear(playerId))
                if (k->poster != playerId)          // (One's own work is never offered back.)
                    ids.push_back(k->id);
    }
    else if (list == "here.missions" && !faction.empty())
    {
        for (const auto& m : factions_.missions())
            if (m.faction == faction && m.state == "open")
                ids.push_back(m.id);
    }
    else if (list == "here.mine")
    {
        if (merchant)
            for (const auto& k : world_.roads().contracts)
                if (k.status == "taken" && k.taker == playerId && contractOfMarket(k, npcId))
                    ids.push_back(k.id);
        for (const auto& m : factions_.missions())
            if (m.state == "taken" && m.taker == playerId && m.official == npcId)
                ids.push_back(m.id);
    }
    else if (list == "here.deliveries")
    {
        for (const auto& k : world_.roads().contracts)
            if (k.kind == "courier" && k.status == "taken" && k.taker == playerId && k.target == npcId)
                ids.push_back(k.id);
        for (const auto& m : factions_.missions())
            if (m.state == "taken" && m.taker == playerId && m.kind != "guard" && (m.kind == "message" ? m.to : m.official) == npcId)
                ids.push_back(m.id);
    }
    for (const auto& id : ids)
        out.push_back(jobFacts(playerId, npcId, id));
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
    // The entry: trade, talk (Phase 1); a giver's work (Phase 2): work, job:<contract>, missions, mine, deliver:<id>.
    const auto colon = entry.find(':');
    const std::string base = entry.substr(0, colon), arg = colon == std::string::npos ? std::string() : entry.substr(colon + 1);
    static const std::set<std::string> Entries{"trade", "talk", "work", "job", "missions", "mine", "deliver"};
    if (!Entries.count(base) || (base == "job" || base == "deliver") == arg.empty())
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
    const bool merchant = world_.society().merchant(npcId);
    const auto who = names::capitalised(labelFor(playerId, npcId));
    if (base == "trade" && !merchant)
    {
        refuse("none", who + " keeps no shop.");
        return;
    }
    if ((base == "work" || base == "job") && !merchant)
    {
        refuse("none", "Ask a merchant about work.");
        return;
    }
    const auto faction = officialOf(npcId);
    if (base == "missions" && faction.empty())
    {
        refuse("none", who + " has no missions to give.");
        return;
    }
    if (base == "mine" && !merchant && faction.empty())
    {
        refuse("none", who + " gave you no work.");
        return;
    }
    const auto listed = [&](const std::string& list) {
        const auto items = dialogueItems(playerId, npcId, list);
        return std::any_of(items.begin(), items.end(), [&](const talk::Facts& f) { return f.count("topic.job_id") && f.at("topic.job_id") == arg; });
    };
    if (base == "job" && !listed("here.work"))
    {
        refuse("gone", "That work has been taken, or isn't to be had here.");
        return;
    }
    if (base == "deliver" && !listed("here.deliveries"))
    {
        refuse("none", "You have nothing for " + who + ".");
        return;
    }
    if (base == "missions")
        missionBoard(faction, npcId, world_.calendarDays());   // (The day's board, as "Ask for missions" made it.)
    if (dialogues_.count(playerId))
        dialogueClose(playerId, "replaced", false);
    DialogueSession d;
    d.id = dialogueNext_++;
    d.npc = npcId;
    d.entry = base;
    if (!arg.empty())
        d.bound["topic.job_id"] = arg;            // (The job asked about, or handed in: its facts as they stand.)
    const auto q = dialogueQuery(playerId, d);
    const auto* t = talk_.treeFor(base, q);
    if (!t)
    {
        refuse("none", "");                         // (No tree for it: the old way, the shop window or the talk target.)
        return;
    }
    d.tree = t->id;
    const auto* life = world_.society().resident(npcId);
    d.asleepAtOpen = npc->posture == "lying" || (life && life->task == "sleep");
    if (base == "trade")
        shopWith_[playerId] = npcId;                 // (Its list in the snapshot, before a nearer keeper's.)
    auto& kept = dialogues_[playerId] = std::move(d);
    dialogueGo(c, kept, t->entries.at(base));
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
    // (The same choice: an `each` one by its job, wherever it now stands in the list, its facts as they are now.)
    const auto still = std::find_if(now_.begin(), now_.end(), [&](const talk::ShownChoice& s) {
        return s.index == shown->index && (shown->item < 0 ? s.item < 0 : s.item >= 0 && jobOf(s) == jobOf(*shown));
    });
    if (still == now_.end() || still->locked)
    {
        d.shown = now_;
        dialogueSend(c, d, still != now_.end() ? still->why : "That's no longer on offer.");
        return;
    }
    const auto choice = node->choices[shown->index];
    const auto offered = shown->text;
    const auto bound = still->bound;
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
    // A giver's work (doc 83, Phase 2): each action is the game's own rule (World::takeContract, the faction's take, the
    // courier's hand-over and the mission's, giving back), so the window can do nothing the wheel couldn't. Done, the
    // conversation goes on at `then` (or ends); refused, at `refused` with the reason as #topic.why# (or the reason is
    // shown and the node stays).
    const auto job = d.bound.count("topic.job_id") ? d.bound.at("topic.job_id") : std::string();
    const auto finish = [&](const Result& r, const std::string& note) {
        if (r.ok)
        {
            const auto both = earned.empty() ? note : note.empty() ? earned : note + " · " + earned;
            if (!choice.then.empty())
                dialogueGo(c, d, choice.then, both);
            else
                dialogueClose(playerId, "end", false);
            return;
        }
        d.bound["topic.why"] = r.message;
        if (!choice.refused.empty())
            dialogueGo(c, d, choice.refused, r.message);
        else
            dialogueSend(c, d, r.message);
    };
    if (choice.action == "decline")
    {
        finish({true, {}, {}}, {});                 // (Nothing changes: the work stays open for anyone.)
        return;
    }
    if (choice.action == "take")
    {
        Result r;
        if (job.empty())
            r = {false, "Which work?", {}};
        else if (isMission(job))
            r = officialOf(d.npc).empty() ? Result{false, "Ask the faction's official about that.", job} : takeMission(playerId, job);
        else
        {
            r = world_.society().merchant(d.npc) ? world_.takeContract(playerId, job) : Result{false, "Ask a merchant about that work.", job};
            if (r.ok)
                for (auto& k : world_.roads().contracts)
                    if (k.id == job && k.kind == "courier")
                        k.byHand = true;            // (Handed over in words, in the recipient's conversation.)
        }
        if (r.ok)
        {
            system(c, r.message);
            record(Roads | Economy | Character, playerId);
            chapterViewsDirty_ = true;
        }
        finish(r, r.ok ? "Taken on: it's marked on your map." : std::string());
        return;
    }
    if (choice.action == "deliver")
    {
        Result r;
        const auto before = world_.society().cash(playerId);
        std::int64_t paid = 0;
        if (job.empty())
            r = {false, "Hand over what?", {}};
        else if (isMission(job))
        {
            const auto* m = factions_.mission(job);
            if (!m || (m->kind == "message" ? m->to : m->official) != d.npc)
                r = {false, "That isn't for them.", job};
            else
                r = deliverMission(playerId, job, paid);
        }
        else
        {
            const Contract* k = nullptr;
            for (const auto& one : world_.roads().contracts)
                if (one.id == job)
                    k = &one;
            r = k && k->target == d.npc ? world_.handOverLetter(playerId, job) : Result{false, "That letter isn't for them.", job};
            paid = std::max<std::int64_t>(0, world_.society().cash(playerId) - before);
        }
        if (r.ok)
        {
            record(Roads | Economy | Character, playerId);
            d.bound["topic.job_paid"] = paid > 0 ? pennies(paid) : "nothing";
        }
        finish(r, r.ok ? (paid > 0 ? "+" + pennies(paid) : std::string("Handed over.")) : std::string());
        return;
    }
    if (choice.action == "give_back")
    {
        // Free, with no penalty (the user's answer, doc 83): to whoever gave it out, and open again for anyone.
        Result r;
        if (job.empty())
            r = {false, "Give back what?", {}};
        else if (isMission(job))
        {
            const auto* m = factions_.mission(job);
            r = m && m->official == d.npc ? giveBackMission(playerId, job) : Result{false, "Give it back to whoever gave it you.", job};
        }
        else
        {
            const Contract* k = nullptr;
            for (const auto& one : world_.roads().contracts)
                if (one.id == job)
                    k = &one;
            r = k && world_.society().merchant(d.npc) && contractOfMarket(*k, d.npc) ? world_.giveBackContract(playerId, job)
                                                                     : Result{false, "Give it back where you took it on.", job};
        }
        if (r.ok)
        {
            system(c, r.message);
            record(Roads | Economy | Character, playerId);
            chapterViewsDirty_ = true;
        }
        finish(r, r.ok ? "Given back: no harm done." : std::string());
        return;
    }
    if (choice.action == "standing" || choice.action == "buy_report" || choice.action == "tithe")
    {
        // The faction official's own (the wheel's "ask about our standing", "pay for a report", "give a tithe").
        auto k = json::Value::object();
        k.add("verb", choice.action == "standing" ? "report" : choice.action == "buy_report" ? "payreport" : "tithe");
        k.add("target", d.npc);
        Result r;
        factionCommand(c, k, r);
        if (r.ok)
            d.bound["topic.answer"] = unquoted(r.message);
        finish(r, {});
        return;
    }
    if (choice.action == "open")
    {
        // The conversation goes on in the tree that answers another entry (the keeper's "Any work going?" into the
        // work tree), with what is bound so far.
        const auto entry = choice.args.empty() ? std::string() : choice.args.front();
        if (entry == "missions")
        {
            const auto faction = officialOf(d.npc);
            if (faction.empty())
            {
                dialogueSend(c, d, "They have no missions to give.");
                return;
            }
            missionBoard(faction, d.npc, world_.calendarDays());
        }
        auto there = dialogueQuery(playerId, d);
        const auto* next = talk_.treeFor(entry, there);
        if (!next)
        {
            dialogueSend(c, d, "They've nothing to say about that.");
            return;
        }
        d.tree = next->id;
        d.entry = entry;
        dialogueGo(c, d, next->entries.at(entry), earned);
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
