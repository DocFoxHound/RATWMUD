// Players speak with residents only in the dialogue window (Docs/Design/84-living-conversations.md §13, Phase 12; the
// user's goal 7: "I do not expect NPCs to be responding to players in general conversation"). Open chat is heard as
// any speech is, and never answered (Options::residentsAnswerChat, off unless a test turns it on): a resident within
// earshot keeps what it overheard, and one named or chosen glances over and the player is told how to talk with them.
// A shopkeeper looks up as a customer comes in, and the window's greeting knows it; a newcomer's welcome at an inn is
// the innkeeper's greeting in the window; a companion's view is a choice in theirs.
#include "RatwGame.h"

#include "RatwNames.h"
#include "RatwNewcomers.h"
#include "RatwTalk.h"
#include "RatwVoice.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace ratw::game
{
using json::Value;

namespace
{
// Lower case, letters and digits, single spaces: how a place or a name is matched against what was asked after.
std::string plain(const std::string& s)
{
    std::string out;
    for (const char c : s)
    {
        const char l = char(std::tolower(static_cast<unsigned char>(c)));
        if (std::isalnum(static_cast<unsigned char>(l)))
            out += l;
        else if (!out.empty() && out.back() != ' ')
            out += ' ';
    }
    while (!out.empty() && out.back() == ' ')
        out.pop_back();
    return out;
}
} // namespace

std::string Game::askedAfter(const std::string& heard, const std::string& npcName) const
{
    // What a line overheard asked after, in the world's own words: a place or a resident by their own names, never the
    // player's.
    const auto intent = voices_.intent(heard, npcName);
    std::string asked;
    if (intent.id == "where" && !intent.target.empty())
    {
        const auto target = plain(intent.target);
        for (const auto& [id, c] : world_.cells())
            if (const auto name = plain(c.name); !name.empty() && (name == target || name == "the " + target))
            {
                asked = "asking the way to " + c.name;
                break;
            }
        if (asked.empty())
            for (const auto& [id, e] : world_.entities())
                if (e.npc && !e.transient && !e.dead && world_.society().resident(id))
                    if (const auto name = plain(e.name); name == target || name.substr(0, name.find(' ')) == target)
                    {
                        asked = "asking after " + e.name;
                        break;
                    }
    }
    else if (intent.id == "trade")
        asked = "asking what was for sale";
    else if (intent.id == "hours")
        asked = "asking when the shops open";
    else if (intent.id == "day")
        asked = "asking what day it was";
    else if (intent.id == "trouble")
        asked = "asking about trouble in town";
    return asked;
}

void Game::overheard(const std::string& npcId, const std::string& playerId, const std::string& heard, const SensoryResult& sense,
                     bool addressed)
{
    // A resident within earshot of a player's open chat (§13.1.2): it hears it, and never answers. Only a wolf it can put
    // the words to is remembered (an unknown voice leaves nothing to keep of anyone).
    auto* npc = world_.entity(npcId);
    const auto* player = world_.entity(playerId);
    if (!npc || !player || !npc->npc || npc->dead || npc->transient || npc->offstage || heard.empty() || !sense.identifiable)
        return;
    const double t = world_.time();
    // The line in the resident's memory of this wolf, so the window (and the Mind there) can recall it. A memory with
    // nothing of the resident's own in it is never sent to the Mind to be summarised (Game::consolidate).
    memories_.record(npcId, playerId, {sequence_++, now(), labelFor(npcId, playerId), heard});
    // What it was about, as the router reads it, as a light person fact in the talk ledger (x.said.asked, a quarter of
    // a day): residents may talk of it to each other ("That stranger was asking the way to the smithy, loud as you
    // like."), never quoting. Only words the world gives: a place or a resident by their own names, never the player's.
    if (voices_.routes())
    {
        if (heard != overheardLine_)
        {
            overheardLine_ = heard;
            overheardAsked_.clear();
        }
        const auto cached = overheardAsked_.find(npc->name);
        const auto asked = cached != overheardAsked_.end() ? cached->second : overheardAsked_[npc->name] = askedAfter(heard, npc->name);
        if (!asked.empty())
            world_.talkLedger().said(playerId, "asked", asked, npcId, world_.calendarDays() + 0.25);
    }
    if (!addressed)
        return;
    // Named or chosen (§13.1.2): a glance, never a word, and how to talk with them, once in ten minutes for each.
    const auto* life = world_.society().resident(npcId);
    if (npc->posture == "lying" || (life && life->task == "sleep") || world_.inBattle(npcId))
        return;
    const auto key = npcId + "|" + playerId;
    if (emotedAt_.size() > 20000)                 // (Kept only as long as they matter: stage 4's review.)
        for (auto it = emotedAt_.begin(); it != emotedAt_.end();)
            it = t - it->second >= EmoteSeconds ? emotedAt_.erase(it) : std::next(it);
    if (const auto last = emotedAt_.find(key); last == emotedAt_.end() || t - last->second >= EmoteSeconds)
    {
        emotedAt_[key] = t;
        static const char* const Glances[] = {"glances over.", "looks up.", "turns to look.", "pricks up an ear."};
        const auto pick = std::hash<std::string>{}(key) + std::uint64_t(t);
        emote(npcId, playerId, npc->speakingUntil > t ? "glances over without breaking off." : Glances[pick % 4]);
    }
    talkHint(playerId, npcId);
}

void Game::emote(const std::string& npcId, const std::string& toward, const std::string& words)
{
    // A resident's gesture (§13.1.2, 13.1.4): seen as any action is, by whoever can see them; turned toward the one it is
    // for, who is named as meant.
    if (words.empty() || !world_.entity(npcId))
        return;
    ParsedPost post;
    post.ok = true;
    post.segments.push_back({"action", words});
    if (!toward.empty())
        world_.faceTalker(npcId, toward, 6);
    publish(npcId, post, Voice::Speak, toward.empty() ? std::vector<std::string>{} : std::vector<std::string>{toward});
}

void Game::talkHint(const std::string& playerId, const std::string& npcId)
{
    // "Talk to Myrtle (T)": T (or Talk on their wheel) opens their window. Once in ten minutes for each resident.
    auto* c = clientOf(playerId);
    if (!c)
        return;
    const auto key = npcId + "|" + playerId;
    const double t = world_.time();
    if (hintedAt_.size() > 20000)
        for (auto it = hintedAt_.begin(); it != hintedAt_.end();)
            it = t - it->second >= HintSeconds ? hintedAt_.erase(it) : std::next(it);
    if (const auto last = hintedAt_.find(key); last != hintedAt_.end() && t - last->second < HintSeconds)
        return;
    hintedAt_[key] = t;
    const auto name = names::capitalised(labelFor(playerId, npcId));
    auto e = Value::object();
    e.add("type", "talkHint");
    e.add("npc", npcId);
    e.add("name", name);
    e.add("text", "Talk to " + name + " (T)");
    send(c, e);
}

std::vector<std::string> Game::welcomeOthers(const std::vector<std::string>& newcomers, const std::string& innkeeper)
{
    // The wolves an innkeeper may point out to newcomers (doc 52, 6): players here, not new themselves, not Out of
    // character, matchmaking on, and blocked by none of the newcomers.
    std::vector<std::string> others;
    const auto* inn = world_.entity(innkeeper);
    if (!inn)
        return others;
    const auto settingOn = [&](const std::string& id) {
        const auto person = people_.find(accountKey(id));
        return person == people_.end() || person->second.settings.matchmaking;
    };
    const auto status = [&](const std::string& id) {
        const auto p = profiles_.find(id);
        return p == profiles_.end() ? std::string("ic") : p->second.status;
    };
    for (const auto* o : world_.entitiesIn(inn->cellId))
        if (!o->npc && !o->dead && std::find(newcomers.begin(), newcomers.end(), o->id) == newcomers.end() && status(o->id) != "ooc" &&
            settingOn(o->id) && std::none_of(newcomers.begin(), newcomers.end(), [&](const std::string& n) { return blocked(n, o->id); }))
            others.push_back(o->id);
    return others;
}

std::string Game::welcomeInWindow(const std::string& newcomer, const std::string& innkeeper)
{
    // The innkeeper's greeting was the welcome (inn.tree's [here.newcomer=yes]); now the wolves here it knows are
    // pointed out, aloud (each listener hears them as it knows them, so a name is never passed on as hearsay: doc 32,
    // 1.5), and each pointed out, and the newcomer, are asked to introduce themselves. Returns the words for the window.
    const auto due = welcomeDue_.find(newcomer);
    if (due == welcomeDue_.end() || due->second.first != innkeeper)
        return {};
    welcomeDue_.erase(due);
    evenings_[newcomer].done = true;
    welcomedAt_[innkeeper] = now();
    const auto& m = newcomers::matchRules();
    const auto& r = newcomers::rules();
    std::string shown;
    std::vector<std::string> pointed;
    for (const auto& id : welcomeOthers({newcomer}, innkeeper))
    {
        if (int(pointed.size()) >= r.eveningLines)
            break;
        const auto* bond = world_.bonds().find(innkeeper, id);
        if (!bond || bond->familiarity < m.knowsFamiliarity)
            continue;
        std::vector<std::string> facts;
        const auto fact = [&](const char* key) {
            if (const auto f = m.facts.find(key); f != m.facts.end())
                facts.push_back(f->second);
        };
        fact(bond->familiarity >= r.regular ? "regular" : "sometimes");
        if (const auto p = profiles_.find(id); p != profiles_.end() && p->second.status == "lfs")
            fact("looking");
        const auto account = accountKey(id);
        const auto person = people_.find(account);
        if (availableMentor(account) || (person != people_.end() && person->second.experience == "guide" && !person->second.mentor.on))
            fact("guide");
        std::string what;
        for (std::size_t i = 0; i < facts.size(); ++i)
            what += (i == 0 ? "" : i + 1 == facts.size() ? " and " : ", ") + facts[i];
        ParsedPost post;
        post.ok = true;
        post.speech = true;
        post.veilNames = true;
        post.segments.push_back({"speech", newcomers::fill(m.pointOut, {{"who", nameOf(id)}, {"what", what}})});
        publish(innkeeper, post, Voice::Speak, {newcomer});
        npcLastSpeech_[innkeeper] = world_.time();
        shown += (shown.empty() ? "" : " ") + newcomers::fill(m.pointOut, {{"who", labelFor(newcomer, id)}, {"what", what}});
        pointed.push_back(id);
    }
    const auto prompt = [&](const std::string& to, const std::string& target, const std::string& words) {
        if (auto* c = clientOf(to))
        {
            auto e = Value::object();
            e.add("type", "introducePrompt");
            e.add("target", target);
            e.add("text", newcomers::fill(words, {{"innkeeper", names::capitalised(labelFor(to, innkeeper))}}));
            e.add("seconds", 120);
            send(c, e);
        }
    };
    for (const auto& id : pointed)
        prompt(id, newcomer, m.promptPointed);
    if (!pointed.empty())
        prompt(newcomer, {}, m.promptNewcomer);
    logEvent("welcome", innkeeper, newcomer, std::to_string(pointed.size()) + " pointed out");
    saveSoon();
    return shown;
}

std::string Game::companionOpinion(const std::string& npcId, const std::string& playerId)
{
    // "What do you think?" in the companion's window (§13.1.6): its view on one of the realm's issues (opinions.json) it
    // holds a stance on, the strongest first (a day's draw among equals), as the library's opinion lines say it (the
    // wearer's mantle's own first, by the engine's own answer to an owed give.opinion).
    auto q = talkQuery(npcId, playerId, "", {});
    struct Held
    {
        int strength;
        std::uint64_t draw;
        const talk::Issue* issue;
    };
    std::vector<Held> held;
    const auto day = std::uint64_t(world_.calendarDays());
    for (const auto& i : talk_.issues())
    {
        const auto op = q.a.facts.find("op." + i.name);
        if (op == q.a.facts.end())
            continue;
        const auto s = q.a.facts.find("op_strength." + i.name);
        const int strength = s == q.a.facts.end() ? 0 : std::atoi(s->second.c_str());
        if (strength >= 1)
            held.push_back({strength, std::hash<std::string>{}(npcId + i.name) + day * 7919, &i});
    }
    std::sort(held.begin(), held.end(), [](const Held& x, const Held& y) { return x.strength != y.strength ? x.strength > y.strength : x.draw < y.draw; });
    for (std::size_t n = 0; n < held.size() && n < 4; ++n)
    {
        const auto& i = *held[n].issue;
        talk::Conversation c;
        c.people = {npcId, playerId};
        c.players = {1};
        c.phase = talk::Conversation::Phase::Body;
        c.seed = held[n].draw + std::uint64_t(world_.time());
        c.conv["issue"] = i.name;
        c.conv["issue_words"] = i.words;
        auto asked = q;
        asked.facts["conv.issue"] = i.name;
        asked.facts["conv.issue_words"] = i.words;
        const auto fact = [&](const std::string& k) {
            const auto f = asked.a.facts.find(k + "." + i.name);
            return f == asked.a.facts.end() ? std::string() : f->second;
        };
        asked.a.facts["stance"] = fact("op").empty() ? "mixed" : fact("op");
        asked.a.facts["stance_strength"] = fact("op_strength").empty() ? "0" : fact("op_strength");
        if (const auto why = fact("op_why"); !why.empty())
            asked.a.facts["stance_why"] = why;
        talk::Conversation::Open owed;
        owed.by = 1;
        owed.to = 0;
        owed.moves = {"give.opinion"};
        const auto said = talk_.answer(c, owed, asked, 0, {});
        if (!said.id.empty() && !said.lines.empty() && !said.lines.front().second.empty())
        {
            talkRemember(said.effects);
            return said.lines.front().second;
        }
    }
    // No view to give: the library's words for that, in their dialect (rules/companion.talk); the tree's own after.
    const auto none = talkPick(talkQuery(npcId, playerId, "companion.opinion"), npcId, "companion:" + npcId, 4,
                               std::hash<std::string>{}(npcId + playerId) + std::uint64_t(world_.time() * 10), false);
    return none.lines.empty() ? std::string() : none.lines.front().second;
}
} // namespace ratw::game
