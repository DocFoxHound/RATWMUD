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
#include "RatwClaims.h"
#include "RatwWire.h"

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
constexpr double Reach = 3;             // Tiles: near enough to open a conversation (the client walks over first).
constexpr double WalkedAway = 4;        // Further than this, the conversation ends.
constexpr std::size_t MostWords = 300;  // Characters of a line the player writes.
constexpr std::size_t Kept = 4;         // Lines of history kept (two exchanges).

// The face's mood for a word (doc 83, Phase 5): one of Library::faceMoods (the client's Client/src/game/face.ts knows
// the same), or none. A node's `mood` is one already (the loader checks); the NPC Mind's emotion, free words, is put
// into the nearest here.
std::string faceMood(const std::string& word)
{
    std::string w;
    for (const char ch : word)
    {
        if (!std::isalpha(static_cast<unsigned char>(ch)))
        {
            if (!w.empty())
                break;                              // (The first word: "annoyed, a little" is annoyed.)
            continue;
        }
        w += char(std::tolower(static_cast<unsigned char>(ch)));
    }
    if (talk::Library::faceMoods().count(w))
        return w;
    static const std::map<std::string, std::string> Like{
        {"happy", "pleased"}, {"glad", "pleased"}, {"cheerful", "pleased"}, {"amused", "pleased"}, {"delighted", "pleased"},
        {"content", "calm"}, {"relaxed", "calm"}, {"neutral", "calm"}, {"patient", "calm"},
        {"friendly", "warm"}, {"fond", "warm"}, {"affectionate", "warm"}, {"kind", "warm"}, {"grateful", "warm"},
        {"eager", "keen"}, {"curious", "keen"}, {"excited", "keen"}, {"interested", "keen"},
        {"polite", "formal"}, {"businesslike", "formal"}, {"stiff", "formal"},
        {"serious", "stern"}, {"strict", "stern"}, {"grim", "stern"},
        {"suspicious", "wary"}, {"guarded", "wary"}, {"cautious", "wary"}, {"nervous", "wary"}, {"distrustful", "wary"},
        {"angry", "cross"}, {"annoyed", "cross"}, {"irritated", "cross"}, {"furious", "cross"}, {"impatient", "cross"},
        {"hostile", "cross"},
        {"grieving", "sad"}, {"sorrowful", "sad"}, {"melancholy", "sad"}, {"unhappy", "sad"}, {"lonely", "sad"},
        {"anxious", "worried"}, {"afraid", "worried"}, {"scared", "worried"}, {"fearful", "worried"}, {"uneasy", "worried"},
        {"weary", "tired"}, {"exhausted", "tired"}, {"drowsy", "sleepy"}, {"pained", "hurt"}};
    const auto f = Like.find(w);
    return f == Like.end() ? std::string() : f->second;
}

// How long the resident is speaking a line (seconds, for the face: about fourteen characters a second, as speech goes).
double speakingTime(const std::string& line)
{
    std::size_t chars = 0;
    for (const char ch : line)
        chars += (static_cast<unsigned char>(ch) & 0xC0) != 0x80;
    return std::round(std::clamp(double(chars) / 14.0, .6, 10.0) * 10) / 10;
}

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

bool isMission(const std::string& id)
{
    return id.rfind("mission-", 0) == 0;
}

const char* yes(bool b)
{
    return b ? "yes" : "no";
}

// The id of the item a shown choice was made for (its `each`'s: topic.job_id, topic.place_id...), "" for none.
std::string itemOf(const talk::ShownChoice& s, const std::string& item)
{
    const auto f = s.bound.find("topic." + (item.empty() ? std::string("job") : item) + "_id");
    return f == s.bound.end() ? std::string() : f->second;
}

// "nine" for 9, "eleven in the morning" for 11 (a service's hours).
std::string clockWords(double hour, bool part)
{
    static const char* const numbers[] = {"twelve", "one", "two", "three", "four", "five", "six", "seven", "eight",
                                          "nine", "ten", "eleven"};
    const int h = int(std::floor(std::fmod(hour + 24, 24)));
    if (h == 12)
        return "noon";
    return std::string(numbers[h % 12]) + (!part ? "" : h < 12 ? " in the morning" : h < 18 ? " in the afternoon" : " in the evening");
}

// The places a stranger asks the way to, by the trade that keeps them (doc 83, Phase 3).
const std::map<std::string, std::string>& placeKinds()
{
    static const std::map<std::string, std::string> Kinds{
        {"innkeeper", "the inn"}, {"priest", "the church"},   {"guard", "the watch"},  {"smith", "the smithy"},
        {"baker", "the bakery"},  {"brewer", "the brewery"},  {"healer", "the healer"}, {"official", "the town office"}};
    return Kinds;
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
    dialogueRoleFacts(playerId, d.npc, facts);
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
    else if (list == "here.places")
        return dialoguePlaces(npcId);
    else if (list == "here.troubles")
    {
        // What this wolf may do here about a trouble it has heard of (doc 57): the wheel's own "trouble:" actions.
        const auto* self = world_.entity(playerId);
        const auto* npc = world_.entity(npcId);
        if (!self || !npc)
            return out;
        auto actions = json::Value::array();
        troubleActions(*self, *npc, std::hypot(npc->position.x - self->position.x, npc->position.y - self->position.y), actions);
        for (const auto& a : actions.items())
            if (a.isString())
            {
                const auto& action = a.asString();
                const auto bar = action.find('|');
                out.push_back({{"topic.trouble_id", action},
                               {"topic.trouble_words", bar == std::string::npos ? std::string("About that trouble.") : action.substr(bar + 1) + "."}});
            }
        return out;
    }
    for (const auto& id : ids)
        out.push_back(jobFacts(playerId, npcId, id));
    return out;
}

// ------------------------------------------------------------------ The other roles (doc 83, Phase 3)

std::string Game::upstairsOf(const std::string& keeper) const
{
    // Only the keeper lets the beds (doc 83, Phase 4): one who holds the inn's till, not a pot hand or a helper there.
    const auto* post = world_.society().jobOf(keeper);
    const auto* inn = post && post->role == "merchant" ? world_.cell(post->work.cell) : nullptr;
    if (!inn)
        return {};
    for (const auto& [id, c] : world_.cells())
        if (c.name == inn->name + ", upstairs" && innUpstairs(id))
            return id;
    return {};
}

std::vector<talk::Facts> Game::dialoguePlaces(const std::string& npcId)
{
    // The places in this resident's town worth asking the way to: where its innkeepers, priests, watch, smiths, bakers,
    // brewers, healers and officials work (a building, each once), the nearest six, each with its way in words.
    std::vector<talk::Facts> out;
    const auto* npc = world_.entity(npcId);
    if (!npc)
        return out;
    const auto& society = world_.society();
    const auto town = world_.communityOf(npc->cellId);
    if (town.empty())
        return out;
    std::vector<std::pair<double, talk::Facts>> found;
    std::set<std::string> seen{npc->cellId};
    for (const auto& p : society.positions())
    {
        if (seen.count(p.work.cell))
            continue;
        const auto held = society.state().careers.positions.find(p.id);
        if (held == society.state().careers.positions.end() || held->second.holder.empty())
            continue;
        const auto* c = world_.cell(p.work.cell);
        if (!c || c->outdoors || world_.communityOf(p.work.cell) != town)
            continue;
        const auto kind = talk::jobCategory(p.title, {}, p.role == "merchant" || p.role == "guard" ? p.role : "civilian", 40);
        const auto words = placeKinds().find(kind);
        if (words == placeKinds().end())
            continue;
        double far = -1;
        const auto way = wayWords(npcId, p.work.cell, far);
        if (way.empty() || far < 0)
            continue;
        seen.insert(p.work.cell);
        const auto* keeper = world_.entity(held->second.holder);
        found.push_back({far, {{"topic.place_id", p.work.cell}, {"topic.place_title", c->name}, {"topic.place_kind", words->second},
                               {"topic.place_way", way + (keeper && !keeper->dead ? " " + keeper->name + " is there, most days." : std::string())}}});
    }
    // The nearest of each kind first (an inn, a church, the watch...), then the nearest others.
    std::sort(found.begin(), found.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    std::set<std::string> kinds;
    std::vector<bool> taken(found.size(), false);
    for (int pass = 0; pass < 2; ++pass)
        for (std::size_t i = 0; i < found.size() && out.size() < 6; ++i)
            if (!taken[i] && (pass == 1 || kinds.insert(found[i].second.at("topic.place_kind")).second))
            {
                taken[i] = true;
                out.push_back(found[i].second);
            }
    return out;
}

namespace
{
// A trade in a word or two, for a master's "Smithing? It's a trade worth knowing." (doc 83, Phase 4): the post's craft
// where its title names one ("potter at Fenshore Kilns": pottery), else its kind of work (talk::jobCategory), never the
// whole title ("Smith at Timberland's Anchor Forge").
std::string tradeWords(const std::string& title, const std::string& role, int age)
{
    std::string lower;
    for (const char ch : title)
        lower += char(std::tolower(static_cast<unsigned char>(ch)));
    static const std::vector<std::pair<const char*, const char*>> Crafts{
        {"carpenter", "carpentry"}, {"joiner", "joinery"}, {"cooper", "coopering"}, {"tailor", "tailoring"},
        {"weav", "weaving"}, {"loom", "weaving"}, {"potter", "potting"}, {"kiln", "potting"}, {"mason", "masonry"},
        {"tanner", "tanning"}, {"chandler", "candle-making"}, {"candle", "candle-making"}, {"rope", "rope-making"},
        {"dye", "dyeing"}, {"glass", "glass-making"}, {"jewel", "jewel-work"}, {"fletcher", "fletching"},
        {"netmak", "net-making"}, {"boatwright", "boat-building"}, {"shipwright", "ship-building"}, {"plaster", "plastering"},
        {"soap", "soap-making"}, {"brick", "brick-making"}, {"print", "printing"}, {"paper", "paper-making"},
        {"distill", "distilling"}, {"still", "distilling"}, {"butcher", "butchery"}, {"cleaver", "butchery"},
        {"smokehouse", "smoking fish"}, {"salt", "salt-making"}, {"vine", "vine-keeping"}, {"press", "pressing"},
        {"butter", "dairying"}, {"dairy", "dairying"}, {"eel", "eel-catching"}, {"needle", "needlework"}};
    // The work's own words first ("potter" of "potter at Fenshore Kilns"), then the whole title where its kind is a craft
    // ("helping at Wickham's Chandlery"): a place's name ("The Vine and Sickle") never names an innkeeper's trade.
    const auto at = lower.find(" at ");
    const auto head = lower.substr(0, at);
    const auto craft = [&](const std::string& words) -> std::string {
        for (const auto& [stem, said] : Crafts)
            if (words.find(stem) != std::string::npos)
                return said;
        return {};
    };
    if (auto own = craft(head); !own.empty())
        return own;
    static const std::map<std::string, std::string> Kinds{
        {"smith", "smithing"},       {"baker", "baking"},        {"brewer", "brewing"},         {"innkeeper", "innkeeping"},
        {"cook", "cooking"},         {"farmer", "farming"},      {"herder", "herding"},         {"fisher", "fishing"},
        {"sailor", "sailing"},       {"miner", "mining"},        {"healer", "healing"},         {"priest", "the church's work"},
        {"scholar", "clerking"},     {"woodcutter", "woodcutting"}, {"carter", "carting"},       {"servant", "service"},
        {"official", "clerking"},    {"crafter", "the craft"},   {"laborer", "labouring"},      {"merchant", "keeping a shop"}};
    const auto kind = talk::jobCategory(title, {}, role.empty() ? std::string("civilian") : role, age);
    if (kind == "crafter" || kind == "laborer" || kind == "merchant")
        if (auto own = craft(lower); !own.empty())
            return own;
    const auto found = Kinds.find(kind);
    return found != Kinds.end() ? found->second : std::string("my work");
}
} // namespace

void Game::dialogueRoleFacts(const std::string& playerId, const std::string& npcId, talk::Facts& f)
{
    const auto& society = world_.society();
    const auto* npc = world_.entity(npcId);
    if (!npc)
        return;
    const auto* post = society.jobOf(npcId);
    // The watch: wanted here (and what settles it), crimes this wolf saw and hasn't told, the bounties and the camps.
    const auto lawTown = world_.lawTown(npc->cellId);
    bool wanted = false;
    std::int64_t owed = 0;
    for (const auto& w : world_.crime().warrants)
        if (w.person == playerId && !lawTown.empty() && w.town == lawTown)
            wanted = true, owed = world_.owedBy(w);
    f["here.wanted"] = yes(wanted);
    f["here.fine"] = std::to_string(owed);
    int witnessed = 0;
    for (const auto& inc : world_.crime().incidents)
        for (const auto& w : inc.witnesses)
            witnessed += w.id == playerId && !w.reported && inc.offender != playerId;
    f["here.witnessed"] = std::to_string(witnessed);
    int camps = 0;
    for (const auto& camp : world_.roads().camps)
        camps += camp.active;
    f["here.camps"] = std::to_string(camps);
    {
        const auto* market = world_.roadMarketOf(npc->cellId);
        const Contract* bounty = nullptr;
        for (const auto& k : world_.roads().contracts)
            if (k.kind == "bounty" && k.status == "open" && (!bounty || (market && (k.town == market->id || k.from == market->id))))
                bounty = &k;
        std::string words;
        if (bounty)
        {
            std::string where;
            for (const auto& camp : world_.roads().camps)
                if (camp.id == bounty->target)
                    if (const auto* c = world_.cell(camp.cell))
                        where = " out by " + c->name;
            words = "There's a price on the bandit camp" + where + ": " + pennies(bounty->reward) +
                    " to whoever clears it. The merchants at the market post it.";
        }
        else if (camps > 0)
            words = "There are bandits out on the roads, but nobody's put a price on them just now.";
        else
            words = "The roads are quiet. There's no bounty posted.";
        f["here.bounty"] = words;
    }
    // An inn: a bed upstairs, by the night or the week, or the whole upstairs; one lodging a wolf.
    const auto upstairs = upstairsOf(npcId);
    const bool lodging = lodgingOf(playerId) != nullptr;
    // A helper at an inn (a pot hand, "helping at The Grey Gull Inn") lets no beds: they send the wolf to the keeper.
    std::string keeper;
    if (post && post->role != "merchant" && upstairs.empty())
        for (const auto& p : society.positions())
            if (p.role == "merchant" && p.work.cell == post->work.cell && p.id != post->id)
                if (const auto held = society.state().careers.positions.find(p.id); held != society.state().careers.positions.end())
                    if (const auto* k = world_.entity(held->second.holder); k && !k->dead &&
                        talk::jobCategory(p.title, {}, "merchant", 40) == "innkeeper")
                    {
                        keeper = k->name;
                        break;
                    }
    f["here.inn"] = yes(!upstairs.empty());
    f["here.inn_helper"] = yes(!keeper.empty());
    if (!keeper.empty())
        f["here.innkeeper"] = keeper;
    f["here.lodging"] = yes(lodging);
    f["here.bed_ok"] = yes(!upstairs.empty() && !lodging);
    f["here.bed_why"] = !keeper.empty()    ? "Ask " + keeper + ": the rooms are theirs to let."
                        : upstairs.empty() ? "We've no rooms to let."
                        : lodging          ? "You have a lodging already."
                                           : std::string();
    f["here.bed_night"] = std::to_string(innPrice("bed night"));
    f["here.bed_week"] = std::to_string(innPrice("bed week"));
    f["here.upstairs_night"] = std::to_string(innPrice("whole"));
    // A master: their trade, open to an apprentice (the wheel's rule: not the watch, no apprentice yet); and whether they
    // would take this wolf on (Society::apprentice: not learning another, known and trusted, a regard of 30).
    const bool master = post && post->role != "guard" && !npc->dead &&
                        society.state().careers.positions.count(post->id) && society.state().careers.positions.at(post->id).apprentice.empty();
    f["here.teaches"] = yes(master);
    f["here.trade_title"] = tradeWords(post ? post->title : std::string(), post ? post->role : std::string(), npc->age);
    double regard = 0;
    if (const auto* b = world_.bonds().find(npcId, playerId))
        regard = std::clamp(b->familiarity * .4 + b->affinity * .4 + b->trust * .2, 0.0, 100.0);
    const bool learning = society.apprenticedTo(playerId) != nullptr;
    f["here.take_on"] = yes(master && !learning && regard >= 30);
    f["here.take_on_why"] = !master     ? (post && post->role != "guard" ? "They have an apprentice already." : "They've no trade to teach.")
                            : learning  ? "You're learning a trade already."
                            : regard < 30 ? "They don't know you well enough to take you on yet."
                                          : std::string();
    // A companion in the party (doc 32): waiting or following.
    const auto* companion = parties_.companion(npcId);
    const bool along = companion && parties_.together(playerId, npcId);
    f["here.companion"] = yes(along);
    f["here.waiting"] = yes(along && companion->waiting);
    // A priest's Restday (doc 42): the service's hours.
    f["here.service"] = "from " + clockWords(Society::ServiceStart, false) + " until " + clockWords(Society::ServiceEnd, true);
    // The town: how many live here, its shops and inns, and whose district this is (a House's: doc 61).
    f["here.folk"] = f["here.shops"] = f["here.inns"] = "0";
    if (const auto town = world_.communityOf(npc->cellId); !town.empty())
    {
        int folk = 0, shops = 0, inns = 0;
        for (const auto& [id, life] : society.state().residents)
            if (!life.homeCell.empty() && world_.communityOf(life.homeCell) == town)
                if (const auto* e = world_.entity(id); e && !e->dead)
                    ++folk;
        for (const auto& p : society.positions())
            if (p.role == "merchant" && world_.communityOf(p.work.cell) == town)
                if (const auto held = society.state().careers.positions.find(p.id);
                    held != society.state().careers.positions.end() && !held->second.holder.empty())
                {
                    ++shops;
                    inns += talk::jobCategory(p.title, {}, "merchant", 40) == "innkeeper";
                }
        f["here.folk"] = std::to_string(folk);
        f["here.shops"] = std::to_string(shops);
        f["here.inns"] = std::to_string(inns);
    }
    if (const auto house = world_.districtHouse(npc->cellId); !house.empty())
        f["here.district"] = house;
    // Gossip: the freshest thing this resident has heard of someone else (a week old at most, half sure at least; never
    // of the two of them, nor a deed, which has its own words: doc 56).
    f["here.gossip"] = "no";
    if (const auto* heard = world_.beliefsOf(npcId))
    {
        // (Doc 84, Phase 1: in claims.json's words, never to or by its victim as gossip, never a camp's id; the claim's
        // kind shared with the residents' own talk: claims::family.)
        const Belief* best = nullptr;
        std::string subject, predicate;
        for (const auto& held : *heard)
        {
            if (held.subject == npcId || held.subject == playerId || held.object == npcId || held.object == playerId ||
                held.confidence < .45 || world_.calendarDays() - held.day > 7 || held.claim.rfind("deed:", 0) == 0 ||
                !world_.entity(held.subject) || (best && held.day <= best->day))
                continue;
            auto b = held;
            world_.claimOf(b);
            const auto* about = world_.entity(b.subject);
            claims::Words w;
            w.xName = about->npc ? about->name : labelFor(npcId, b.subject);
            if (const auto* object = b.object.empty() ? nullptr : world_.entity(b.object))
                w.yName = object->npc ? object->name : labelFor(npcId, b.object);
            w.place = world_.placeWords(b.place);
            w.text = b.claim;
            auto said = claims::loaded() ? claims::predicate(b.kind, w) : (claims::rawId(b.claim) ? std::string() : b.claim);
            if (said.empty() || (claims::find(b.kind) && claims::find(b.kind)->topic != "gossip"))
                continue;
            best = &held;
            subject = w.xName;
            predicate = said;
        }
        if (best)
        {
            f["here.gossip"] = "yes";
            f["topic.subject"] = subject;
            f["topic.claim"] = predicate;
            f["topic.claim_kind"] = claims::family(best->kind, best->claim);
        }
    }
    f["here.places"] = std::to_string(dialoguePlaces(npcId).size());
    f["here.troubles"] = std::to_string(dialogueItems(playerId, npcId, "here.troubles").size());
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

std::string Game::dialogueMood(const std::string& npcId, const talk::Query& q, const std::string& tagged) const
{
    // The face's mood for a line (doc 83, Phase 5): asleep before anything (a node's tag is for the awake), then the
    // node's own `mood`, then how the resident is (grieving, hurt, worn out, the Mind's last emotion), then how they feel
    // about this wolf. None (calm, unmarked) otherwise.
    const auto fact = [&](const std::string& key) {
        const auto f = q.a.facts.find(key);
        return f == q.a.facts.end() ? std::string() : f->second;
    };
    const auto* npc = world_.entity(npcId);
    const auto* life = world_.society().resident(npcId);
    if (npc && (npc->posture == "lying" || (life && life->task == "sleep")))
        return "sleepy";
    if (!faceMood(tagged).empty())
        return faceMood(tagged);
    if (world_.society().mourning(npcId))
        return "sad";
    if (fact("injured") == "bad")
        return "hurt";
    if (fact("tired") == "yes")
        return "tired";
    if (const auto mood = npcMood_.find(npcId); mood != npcMood_.end() && !faceMood(mood->second).empty())
        return faceMood(mood->second);
    const auto pair = q.facts.find("pair.disposition");
    const std::string disposition = pair == q.facts.end() ? std::string() : pair->second;
    if (disposition == "hostile")
        return "cross";
    if (disposition == "wary")
        return "wary";
    if (disposition == "warm" || disposition == "fond")
        return "warm";
    return {};
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
    m.add("mood", d.mood);                          // The face (Phase 5): its mood, and how long it speaks the line.
    m.add("speaking", d.awaiting ? 0.0 : speakingTime(d.line));
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
    talk::LineInfo lineFrom{line.empty() ? std::string() : node.ask, -1, {}};
    if (line.empty())
    {
        const auto said = talk_.pick(q, seed, {}, {});
        if (!said.lines.empty())
        {
            line = said.lines.front().second;
            talkRemember(said.effects);
            if (!said.info.empty())
                lineFrom = said.info.front();
        }
    }
    d.line = line;
    d.mood = dialogueMood(d.npc, q, node.mood);
    d.awaiting = false;
    d.changed = world_.time();
    d.shown = talk_.choices(node, q, seed, [&](const std::string& list) { return dialogueItems(playerId, d.npc, list); });
    d.history.push_back({"npc", line});
    while (d.history.size() > Kept)
        d.history.pop_front();
    world_.faceTalker(d.npc, playerId);
    if (!d.quiet || d.history.size() > 1)          // (A quiet window's greeting isn't said aloud: dialogueOpen.)
        dialogueSpeak(d.npc, playerId, line);
    voiced("dialogue", "tree", d.npc);
    if (options_.logTalkWords && !line.empty())
        voicedWords("dialogue", d.npc, playerId, line, &lineFrom);
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
    // The other roles (Phase 3): learn (a master's trade: the wheel's Apprentice), report and fine (the watch's: the
    // wheel's Report and Pay fine), orders (a companion's).
    static const std::set<std::string> Entries{"trade", "talk", "work", "job", "missions", "mine", "deliver", "learn", "report", "fine",
                                               "orders"};
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
    const auto* post = world_.society().jobOf(npcId);
    if (base == "learn" && (!post || post->role == "guard"))
    {
        refuse("none", who + " has no trade to teach.");
        return;
    }
    if ((base == "report" || base == "fine") && !(post && post->role == "guard"))
    {
        refuse("none", who + " isn't of the watch.");
        return;
    }
    if (base == "orders" && !(parties_.companion(npcId) && parties_.together(playerId, npcId)))
    {
        refuse("none", who + " isn't travelling with you.");
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
    // (A resident greets aloud each time one opens, and says goodbye as it closes: a window opened again within three
    // seconds of the last is quiet, its greeting and goodbye shown only in the window, so opening and closing it over
    // and over can't fill the room with them.)
    const auto last = dialogueOpened_.find(playerId);
    const bool quiet = last != dialogueOpened_.end() && world_.time() - last->second < 3;
    dialogueOpened_[playerId] = world_.time();
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
    d.quiet = quiet;
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
    if (sayBye && !(d.quiet && d.history.size() <= 1))   // (Nor its goodbye, if nothing more was said.)
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
    // (Checked now, not only once a second: nothing is said or done with a resident one has walked away from, can no
    // longer see, or is fighting.)
    if (const auto why = dialogueEnding(playerId, d); !why.empty())
    {
        dialogueClose(playerId, why, false);
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
        if (std::string until; silenced(playerId, &until))
        {
            dialogueSend(c, d, (communityWorld() ? "The host has" : "A Dungeon Master has") + std::string(" silenced your account for another ") +
                                   until + ".");   // (As chat is: doc 50, 7.)
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
    const auto& itemName = node->choices[shown->index].item;
    const auto still = std::find_if(now_.begin(), now_.end(), [&](const talk::ShownChoice& s) {
        return s.index == shown->index && (shown->item < 0 ? s.item < 0 : s.item >= 0 && itemOf(s, itemName) == itemOf(*shown, itemName));
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
    // The player's own words are chat: never from a silenced account (it says the default line), and no faster than
    // chat (the window can't be a way round either).
    if (!words.empty() && words != offered)
    {
        if (silenced(playerId, nullptr))
            words.clear();
        else if (lastChat_.count(playerId) && world_.time() - lastChat_[playerId] < .5)
        {
            dialogueSend(c, d, "Please leave a moment between lines.");
            return;
        }
        else
            lastChat_[playerId] = world_.time();
    }
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
    // The other roles (doc 83, Phase 3), each the wheel's own handler.
    if (choice.action == "report")
    {
        // The watch: what this wolf saw and hasn't told (World::report).
        talk::Facts before;
        dialogueRoleFacts(playerId, d.npc, before);
        const auto r = world_.report(playerId, d.npc);
        if (r.ok)
        {
            record(Economy | Crime | Character, playerId);
            d.bound["topic.reported"] = before["here.witnessed"] != "0" ? "yes" : "no";
        }
        finish(r, r.ok && before["here.witnessed"] != "0" ? r.message : std::string());
        return;
    }
    if (choice.action == "pay_fine")
    {
        const auto r = world_.payFine(playerId, d.npc);
        if (r.ok)
            record(Economy | Crime | Character, playerId);
        finish(r, r.ok ? r.message : std::string());
        return;
    }
    if (choice.action == "lodge")
    {
        // An inn's bed (doc 54): taken from its keeper downstairs, in the upstairs over the inn they keep.
        const auto upstairs = upstairsOf(d.npc);
        Result r{false, "There are no rooms to let here.", {}};
        if (!upstairs.empty())
        {
            auto k = json::Value::object();
            const bool whole = choice.args.size() == 1 && choice.args[0] == "whole";
            k.add("verb", whole ? "night" : "bed");
            k.add("period", whole || choice.args.size() < 2 ? std::string("night") : choice.args[1]);
            lodgeCommand(c, k, r, upstairs);
        }
        if (r.ok)
            system(c, r.message);
        finish(r, r.ok ? std::string("Your bed is upstairs.") : std::string());
        return;
    }
    if (choice.action == "apprentice")
    {
        const auto r = world_.apprentice(playerId, d.npc);
        if (r.ok)
        {
            system(c, r.message);
            record(Careers | Economy | Character, playerId);
        }
        finish(r, r.ok ? r.message : std::string());
        return;
    }
    if (choice.action == "trouble")
    {
        // A resident's trouble (doc 57, 3): the remedy the wheel offers, bound from here.troubles.
        const auto action = d.bound.count("topic.trouble_id") ? d.bound.at("topic.trouble_id") : std::string();
        const auto r = action.empty() ? Result{false, "About what?", {}} : troubleAction(playerId, d.npc, action);
        finish(r, r.ok ? r.message : std::string());
        return;
    }
    if (choice.action == "order")
    {
        // A companion's orders (doc 32): they answer in their own words, and the conversation ends.
        const auto& o = choice.args.empty() ? std::string() : choice.args.front();
        const auto r = orderCompanion(playerId, d.npc, o == "follow" ? "follow me" : o == "wait" ? "wait here" : o == "home" ? "go home" : "dismiss");
        finish(r, {});
        return;
    }
    if (choice.action == "offering")
    {
        // To the church, by its priest: doc 55's gift of coins, by the give handler's rules.
        Result r;
        if (jobCategoryOf(d.npc) != "priest")
            r = {false, "Give that at a church.", {}};
        else
        {
            auto k = json::Value::object();
            k.add("target", d.npc);
            k.add("coins", choice.args.empty() ? 1.0 : std::atof(choice.args.front().c_str()));
            giveCommand(c, k, r);
        }
        finish(r, r.ok ? r.message : std::string());
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
    // The face's mood with the answer: the emotion the Mind gave it (heed), if it is one the face knows, else as it was.
    if (const auto mood = npcMood_.find(npcId); mood != npcMood_.end() && !faceMood(mood->second).empty())
        d.mood = faceMood(mood->second);
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

std::string Game::dialogueEnding(const std::string& playerId, const DialogueSession& d) const
{
    const auto* player = world_.entity(playerId);
    const auto* npc = world_.entity(d.npc);
    const auto* life = npc ? world_.society().resident(d.npc) : nullptr;
    const bool asleep = npc && (npc->posture == "lying" || (life && life->task == "sleep"));
    if (!clientOf(playerId) || !player || !npc || npc->dead || player->dead)
        return "gone";
    if (npc->cellId != player->cellId ||
        std::hypot(npc->position.x - player->position.x, npc->position.y - player->position.y) > WalkedAway)
        return "away";
    if (!world_.shown(playerId, d.npc))
        return "sight";
    if (asleep && !d.asleepAtOpen)
        return "asleep";
    if (world_.inBattle(playerId) || world_.inBattle(d.npc))
        return "fight";
    if (player->downedLeft > 0)
        return "downed";
    return {};
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
        const auto why = dialogueEnding(playerId, d);
        if (!why.empty())
            ending.emplace_back(playerId, why);
    }
    for (const auto& [playerId, why] : ending)
        dialogueClose(playerId, why, false);
}
} // namespace ratw::game
