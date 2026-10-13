// The ambient director's voice (Docs/Design/26-living-npcs.md, Phase 10): the world picks an exchange worth hearing
// (World::ambientPicks) where players are; here it is written (the talk library, doc 62; the NPC Mind for what matters
// most, within the hour's budget; authored lines when nothing fits) and spoken a line at a time, as NPCs speak to
// players, to whoever can hear. Barks, a resident calling out to no one in particular, are the talk library's too.
#include "RatwSermons.h"
#include "RatwGame.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>

namespace ratw::game
{
namespace
{
constexpr double LookEvery = 5;                     // World seconds between looks for an exchange.
constexpr std::size_t AtOnce = 3;                   // Exchanges under way across the world.
constexpr double Apart = 5;                         // Tiles: walk further apart and the exchange stops.

} // namespace

void Game::ambient(double dt)
{
    const double now = world_.time();
    // Speak what is due; stop an exchange when its speakers part, fall, or a player talks to one of them.
    for (auto it = ambient_.begin(); it != ambient_.end();)
    {
        const auto* a = world_.entity(it->pick.teller);
        const auto* b = world_.entity(it->pick.listener);
        const bool broken = !a || !b || a->dead || b->dead || a->cellId != b->cellId || a->offstage || b->offstage ||
                            std::hypot(a->position.x - b->position.x, a->position.y - b->position.y) > Apart ||
                            pendingNpc_.count(a->id) || pendingNpc_.count(b->id);
        if (broken || (it->voiced && it->lines.empty()))
        {
            it = ambient_.erase(it);
            continue;
        }
        if (it->voiced && now >= it->nextAt)
        {
            const auto [who, text] = it->lines.front();
            it->lines.pop_front();
            const auto& speaker = who == 0 ? it->pick.teller : it->pick.listener;
            ParsedPost post;
            post.ok = true;
            post.speech = true;
            post.segments.push_back({"speech", text});
            publish(speaker, post, Voice::Speak);
            npcLastSpeech_[speaker] = now;
            it->nextAt = now + 2.5 + double(text.size()) / 25;   // Time to read it, before the answer.
        }
        ++it;
    }
    if ((exchangeLookIn_ += dt) < LookEvery || ambient_.size() >= AtOnce)
        return;
    exchangeLookIn_ = 0;
    std::vector<std::string> listeners;
    for (const auto* c : clients_)
        if (!c->entityId.empty())
            listeners.push_back(c->entityId);
    if (listeners.empty())
        return;
    std::set<std::string> busy(pendingNpc_.begin(), pendingNpc_.end());
    for (const auto& [npc, queue] : queuedTalk_)
        busy.insert(npc);
    for (const auto& t : ambient_)
        busy.insert(t.pick.teller), busy.insert(t.pick.listener);
    auto picks = world_.ambientPicks(listeners, busy);
    if (picks.empty())
        return;
    auto pick = std::move(picks.front());
    world_.ambientSpoken(pick);                     // It counts from now: the talk is under way.
    const auto* a = world_.entity(pick.teller);
    const auto context = exchangeContext(pick);
    AmbientTalk started;
    started.id = ambientNext_++;
    started.pick = pick;
    ambient_.push_back(started);
    const auto id = started.id;
    const auto begin = [this, id](const mind::Exchange& written, const char* route) {
        for (auto& t : ambient_)
            if (t.id == id)
            {
                t.lines.assign(written.lines.begin(), written.lines.end());
                t.voiced = true;
                t.generated = written.generated;
                t.nextAt = world_.time();
                note("info", "RATW_AMBIENT " + t.pick.topic.kind + " " + t.pick.teller + " to " + t.pick.listener + " in " + t.pick.cell +
                                 " (" + route + ")");
                voiced("exchange", route, t.pick.teller);
            }
    };
    const auto q = exchangeQuery(pick, context);
    const auto cell = pick.cell;
    const auto seed = id * 2654435761u + std::uint64_t(now * 7);
    const auto fromTalk = [this, q, cell, seed, teller = a->id](const char*& route) {
        const auto said = talkPick(q, teller, cell, 16, seed, true);
        mind::Exchange out;
        out.lines = said.lines;
        route = said.id.rfind("lib_", 0) == 0 ? "library" : "scene";   // (Doc 28's library, or a written scene.)
        return out;
    };
    // Written live only when it matters most: gossip about a player standing there, or the first talk of a crime
    // or a death. The model is asked only so often; past that, and without it, the talk library speaks (then the
    // authored lines, when it has nothing fitting).
    bool playerHere = false;
    for (const auto* c : clients_)
        if (const auto* e = world_.entity(c->entityId); e && e->id == pick.topic.subject && e->cellId == pick.cell)
            playerHere = true;
    const bool fresh = !pick.topic.incident.empty() && !voicedIncidents_.count(pick.topic.incident);
    const bool salient = (pick.topic.kind == "gossip" && (playerHere || fresh)) ||
                         (pick.topic.kind == "news" && (pick.topic.claim == "loss" || pick.topic.claim == "crime"));
    while (!ambientCalls_.empty() && now - ambientCalls_.front() > 3600)
        ambientCalls_.pop_front();
    const bool live = salient && mind_.live() && int(ambientCalls_.size()) < options_.ambientModelCallsPerHour;
    if (!live)
    {
        const char* route = "written";
        if (auto x = fromTalk(route); !x.lines.empty())
            begin(x, route);
        else
            begin(mind::Client::authoredExchange(context), "written");
        return;
    }
    ambientCalls_.push_back(now);
    if (!pick.topic.incident.empty())
        voicedIncidents_.insert(pick.topic.incident);
    std::weak_ptr<bool> alive = alive_;
    mind_.exchange(context, [alive, begin, fromTalk](const mind::Exchange& written) {
        if (alive.expired())
            return;
        const char* route = "written";
        if (written.generated)
            begin(written, "model");
        else if (auto x = fromTalk(route); !x.lines.empty())
            begin(x, route);
        else
            begin(written, "written");
    });
}
mind::ExchangeContext Game::exchangeContext(const AmbientPick& pick)
{
    // What the Mind is told of an exchange, and what the talk library's words draw on.
    const auto* a = world_.entity(pick.teller);
    const auto* b = world_.entity(pick.listener);
    mind::ExchangeContext context;
    if (!a || !b)
        return context;
    const auto persona = [&](const Entity& e) {
        mind::Persona p{e.name, e.description, {}};
        if (const auto* spec = world_.society().spec(e.id))
            p.personality = spec->personality;
        if (const auto v = npcVoice_.find(e.id); v != npcVoice_.end() && !v->second.persona.empty())
            p.personality = v->second.persona;      // (Written for them: doc 62, decision 10.)
        if (const auto card = voiceCardOf(e.id); !card.empty())    // (Doc 62, phase 5.)
            p.personality += (p.personality.empty() ? "" : "\n") + std::string("How they speak: ") + card;
        return p;
    };
    context.a = persona(*a);
    context.b = persona(*b);
    context.aSeesB = world_.describeRegard(a->id, b->id);
    context.bSeesA = world_.describeRegard(b->id, a->id);
    context.kind = pick.topic.kind;
    context.facts = pick.topic.facts;
    if (const auto* place = world_.cell(a->cellId))
        context.scene = mind::left(place->description, 1200);
    // The subject as the teller knows them: a player by a name they were given, else by look (doc 56, 6).
    if (const auto found = pick.topic.talk.find("topic.subject"); found != pick.topic.talk.end())
        context.subjectName = found->second;
    else if (const auto* subject = world_.entity(pick.topic.subject))
        context.subjectName = subject->npc || knowsName(a->id, subject->id) ? labelFor(a->id, subject->id) : strangerLabel(subject->id);
    context.claim = pick.topic.claim;
    if (!pick.topic.facts.empty())
        context.news = pick.topic.facts.front().substr(0, pick.topic.facts.front().find_last_not_of(". ") + 1);
    const auto plan = world_.dayPlan(world_.communityOf(a->cellId));
    const auto* place = world_.cell(a->cellId);
    context.day = plan.kind == "festival" ? "Quite a feast for " + plan.name + "."
                  : plan.kind == "market" ? std::string("The stalls are full today.")
                  : plan.kind == "rest"   ? std::string("A quiet day, for once.")
                  : place && place->outdoors && place->weather == Weather::Rain ? std::string("Wet enough for you?")
                                                                                 : std::string();
    return context;
}

talk::Query Game::exchangeQuery(const AmbientPick& pick, const mind::ExchangeContext& context)
{
    // The talk library (doc 62): the topic's facts, and what the Mind would be told.
    auto facts = pick.topic.talk;
    if (!context.subjectName.empty())
        facts.emplace("topic.subject", context.subjectName);
    if (pick.topic.kind == "gossip" && !context.claim.empty())
        facts["topic.claim"] = context.claim;
    if (!context.news.empty())
        facts["topic.news"] = mind::firstPerson(context.news, context.a.name);
    if (!context.day.empty())
        facts["topic.remark"] = context.day;
    return talkQuery(pick.teller, pick.listener, "talk." + pick.topic.kind, facts);
}

talk::Query Game::talkTopicQuery(const std::string& a, const std::string& b)
{
    AmbientPick pick;
    pick.teller = a;
    pick.listener = b;
    if (const auto* e = world_.entity(a))
        pick.cell = e->cellId;
    pick.topic = world_.ambientTopic(a, b);
    if (pick.topic.kind.empty())
        return talkQuery(a, b, "talk.day");
    return exchangeQuery(pick, exchangeContext(pick));
}

std::vector<std::string> Game::hearersOf(const std::string& speaker) const
{
    std::vector<std::string> out;
    const auto* s = world_.entity(speaker);
    for (const auto* c : clients_)
        if (const auto* e = world_.entity(c->entityId); s && e && e->cellId == s->cellId &&
                                                     world_.hearingClarity(e->id, speaker, Voice::Speak) >= .35)
            out.push_back(e->id);
    return out;
}

talk::Said Game::talkPick(const talk::Query& q, const std::string& speaker, const std::string& where, std::size_t keep,
                          std::uint64_t seed, bool remember)
{
    const auto hearers = hearersOf(speaker);
    const auto heard = [&](const std::string& id) {
        for (const auto& h : hearers)
            if (const auto found = scenesHeard_.find(h); found != scenesHeard_.end() && found->second.ids.count(id))
                return true;
        return false;
    };
    auto& recent = recentScenes_[where];
    auto said = talk_.converse(q, seed, heard, recent);
    if (said.id.empty())
        return said;
    talkRemember(said.effects);                     // (What it remembers, and that it was said: phase 4.)
    if (remember)
        for (const auto& h : hearers)
            scenesHeard_[h].add(said.id);
    recent.push_back(said.id);
    while (recent.size() > keep)
        recent.pop_front();
    return said;
}

void Game::barks(double dt)
{
    // Now and then, near a player, a resident calls out or remarks to no one in particular (doc 30): a merchant crying
    // their wares, a guard's word, a child at play. A line at most every 25 seconds in a place.
    if ((barkLookIn_ += dt) < 6 || !talk_.has("bark"))
        return;
    barkLookIn_ = 0;
    const double now = world_.time();
    std::set<std::string> busy(pendingNpc_.begin(), pendingNpc_.end());
    for (const auto& t : ambient_)
        busy.insert(t.pick.teller), busy.insert(t.pick.listener);
    for (const auto* c : clients_)
    {
        const auto* player = world_.entity(c->entityId);
        if (!player || player->dead)
            continue;
        if (const auto last = barkLast_.find(player->cellId); last != barkLast_.end() && now - last->second < 25)
            continue;
        std::vector<const Entity*> near;
        for (const auto& [id, e] : world_.entities())
            if (e.npc && !e.dead && !e.transient && !e.offstage && e.cellId == player->cellId && !busy.count(id) &&
                e.speakingUntil <= now && e.posture != "lying" &&
                std::hypot(e.position.x - player->position.x, e.position.y - player->position.y) <= 14 &&
                world_.hearingClarity(player->id, id, Voice::Speak) >= .5)
                near.push_back(&e);
        barkLast_[player->cellId] = now;          // Looked: the next look here waits, whether anyone spoke or not.
        if (near.empty())
            continue;
        const auto* who = near[std::size_t(std::uint64_t(now * 13) % near.size())];
        const auto q = talkQuery(who->id, {}, "bark");
        const auto chosen = talkPick(q, who->id, "bark:" + player->cellId, 12,
                                     std::uint64_t(now * 1000) ^ std::hash<std::string>{}(who->id), false);
        if (chosen.id.empty() || chosen.lines.empty())
            continue;
        ParsedPost post;
        post.ok = true;
        post.speech = true;
        post.segments.push_back({"speech", chosen.lines.front().second});
        publish(who->id, post, Voice::Speak);
        npcLastSpeech_[who->id] = now;
        voiced("bark", "scene", who->id);
    }
}

void Game::sermons(double dt)
{
    if ((sermonLookIn_ += dt) < 2 || sermons::all().empty())
        return;
    sermonLookIn_ = 0;
    const double now = world_.time();
    const auto day = std::int64_t(std::floor(world_.calendarDays()));
    std::set<std::string> watched;                    // Only where a player is: nobody preaches to an empty page.
    for (const auto* c : clients_)
        if (const auto* p = world_.entity(c->entityId); p && !p->dead)
            watched.insert(p->cellId);
    if (watched.empty())
        return;
    for (const auto& [id, e] : world_.entities())
    {
        if (!e.npc || e.dead || e.offstage || !watched.count(e.cellId))
            continue;
        const auto* life = world_.society().resident(id);
        if (!life || life->task != "preaching" || e.cellId != life->goalCell ||
            std::hypot(e.position.x - life->goalX, e.position.y - life->goalY) > 1.5)
            continue;                                 // (Still on the way to the pulpit.)
        const auto community = world_.communityOf(e.cellId);
        auto& st = sermons_[id];
        if (st.day != day)
        {
            const auto* sermon = sermons::forWeek(community, day / 7);
            if (!sermon)
                continue;
            st = {sermon->id, 0, now, day};
            logEvent("sermon", id, community, sermon->title);
        }
        if (now < st.nextAt)
            continue;
        const auto& all = sermons::all();
        const auto found = std::find_if(all.begin(), all.end(), [&](const sermons::Sermon& s) { return s.id == st.sermon; });
        if (found == all.end() || st.line >= found->lines.size())
            continue;
        // The town's name where the sermon says {town}.
        std::string town = community;
        for (std::size_t i = 0; i < town.size(); ++i)
            if (town[i] == '_')
                town[i] = ' ';
        for (std::size_t i = 0; i < town.size(); ++i)
            if (i == 0 || town[i - 1] == ' ')
                town[i] = char(std::toupper(static_cast<unsigned char>(town[i])));
        auto line = found->lines[st.line];
        for (std::size_t at; (at = line.find("{town}")) != std::string::npos;)
            line.replace(at, 6, town);
        ParsedPost post;
        post.ok = true;
        post.speech = true;
        post.segments.push_back({"speech", line});
        publish(id, post, Voice::Yell);               // (From the pulpit, to the back of the church.)
        npcLastSpeech_[id] = now;
        ++st.line;
        st.nextAt = now + 60;
    }
}
} // namespace ratw::game
