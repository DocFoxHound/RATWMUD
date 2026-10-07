// The ambient director's voice (Docs/Design/26-living-npcs.md, Phase 10): the world picks an exchange worth hearing
// (World::ambientPicks) where players are; here it is written (the NPC Mind, or authored lines past the hour's budget
// or without one) and spoken a line at a time, as NPCs speak to players, to whoever can hear.
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
    const auto* b = world_.entity(pick.listener);
    mind::ExchangeContext context;
    const auto persona = [&](const Entity& e) {
        mind::Persona p{e.name, e.description, {}};
        if (const auto* spec = world_.society().spec(e.id))
            p.personality = spec->personality;
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
    if (const auto* subject = world_.entity(pick.topic.subject))
        context.subjectName = subject->name;
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
    AmbientTalk talk;
    talk.id = ambientNext_++;
    talk.pick = pick;
    ambient_.push_back(talk);
    const auto id = talk.id;
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
    // From the library (doc 28): an exchange written once with blanks, filled in here, for nothing.
    const Bond ab = world_.bonds().find(a->id, b->id) ? *world_.bonds().find(a->id, b->id) : Bond{};
    const Bond ba = world_.bonds().find(b->id, a->id) ? *world_.bonds().find(b->id, a->id) : Bond{};
    const std::string band = std::min(ab.affinity, ba.affinity) <= -25                    ? "rivals"
                             : ab.familiarity >= 40 && ab.affinity >= 20 && ba.affinity >= 10 ? "friends"
                                                                                         : "acquaintances";
    const std::map<std::string, std::string> blanks{
        {"teller", a->name}, {"listener", b->name}, {"subject", context.subjectName}, {"claim", context.claim},
        {"news", mind::firstPerson(context.news, a->name)}, {"day", context.day}};
    const auto cell = pick.cell;
    const auto fromLibrary = [this, cell, kind = pick.topic.kind, band, blanks, seed = id * 2654435761u]() {
        const auto x = voices_.exchange(kind, band, blanks, seed, recentExchanges_[cell]);
        mind::Exchange out;
        out.lines = x.lines;
        return out;
    };
    // Written live only when it matters most: gossip about a player standing there, or the first talk of a crime
    // or a death. The model is asked only so often; past that, and without it, the library speaks (then the authored
    // lines, when it has nothing fitting).
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
    // The written scenes (doc 30): the moment as the scenes see it, and one no one listening has heard, if there is one.
    if (!live && scenes_.hasTopic(pick.topic.kind))
    {
        scenes::Situation s;
        s.topic = pick.topic.kind;
        world_.sceneMoment(a->id, s.tags, s.blanks);
        for (const auto& [k, v] : pick.topic.tags)
            s.tags[k] = v;
        for (const auto& [k, v] : pick.topic.blanks)
            s.blanks[k] = v;
        s.a = scenePerson(*a);
        s.b = scenePerson(*b);
        s.blanks["a_job"] = s.a.job == "none" ? std::string() : s.a.job;
        s.blanks["b_job"] = s.b.job == "none" ? std::string() : s.b.job;
        if (!context.subjectName.empty())
            s.blanks.emplace("subject", context.subjectName);
        if (pick.topic.kind == "gossip")
            s.blanks["claim"] = context.claim;
        if (!context.news.empty())
            s.blanks["news"] = mind::firstPerson(context.news, a->name);
        const auto hearers = hearersOf(a->id);
        const auto heard = [&](const std::string& scene) {
            for (const auto& h : hearers)
                if (const auto found = scenesHeard_.find(h); found != scenesHeard_.end() && found->second.ids.count(scene))
                    return true;
            return false;
        };
        auto& recent = recentScenes_[cell];
        auto chosen = scenes_.pick(s, id * 2654435761u + std::uint64_t(now * 7), heard, recent);
        if (!chosen.id.empty())
        {
            for (const auto& h : hearers)
                scenesHeard_[h].add(chosen.id);
            recent.push_back(chosen.id);
            while (recent.size() > 16)
                recent.pop_front();
            mind::Exchange x;
            x.lines = chosen.lines;
            begin(x, "scene");
            return;
        }
    }
    if (!live)
    {
        if (auto x = voices_.libraryEntries() ? fromLibrary() : mind::Exchange{}; !x.lines.empty())
            begin(x, "library");
        else
            begin(mind::Client::authoredExchange(context), "written");
        return;
    }
    ambientCalls_.push_back(now);
    if (!pick.topic.incident.empty())
        voicedIncidents_.insert(pick.topic.incident);
    std::weak_ptr<bool> alive = alive_;
    mind_.exchange(context, [alive, begin, fromLibrary, this](const mind::Exchange& written) {
        if (alive.expired())
            return;
        if (written.generated)
            begin(written, "model");
        else if (auto x = voices_.libraryEntries() ? fromLibrary() : mind::Exchange{}; !x.lines.empty())
            begin(x, "library");
        else
            begin(written, "written");
    });
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

scenes::Person Game::scenePerson(const Entity& e) const
{
    scenes::Person p;
    p.name = e.name;
    p.sex = e.appearance->sex == "female" ? "female" : "male";
    p.stage = lifeStageName(lifeStage(e.age));
    const auto* spec = world_.society().spec(e.id);
    const auto* post = world_.society().jobOf(e.id);
    p.role = spec ? spec->role : std::string("civilian");
    if (p.role != "merchant" && p.role != "guard")
        p.role = "civilian";
    p.job = scenes::jobCategory((post ? post->title : std::string()) + " " + e.description,
                                spec ? spec->workLabel : std::string(), p.role, e.age);
    return p;
}

void Game::barks(double dt)
{
    // Now and then, near a player, a resident calls out or remarks to no one in particular (doc 30): a merchant crying
    // their wares, a guard's word, a child at play. A line at most every 25 seconds in a place.
    if ((barkLookIn_ += dt) < 6 || !scenes_.hasTopic("bark"))
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
        scenes::Situation s;
        s.topic = "bark";
        world_.sceneMoment(who->id, s.tags, s.blanks);
        s.a = scenePerson(*who);
        s.blanks["a_job"] = s.a.job == "none" ? std::string() : s.a.job;
        const auto hearers = hearersOf(who->id);
        const auto heard = [&](const std::string& scene) {
            for (const auto& h : hearers)
                if (const auto found = scenesHeard_.find(h); found != scenesHeard_.end() && found->second.ids.count(scene))
                    return true;
            return false;
        };
        auto& recent = recentScenes_["bark:" + player->cellId];
        const auto chosen = scenes_.pick(s, std::uint64_t(now * 1000) ^ std::hash<std::string>{}(who->id), heard, recent);
        if (chosen.id.empty() || chosen.lines.empty())
            continue;
        recent.push_back(chosen.id);
        while (recent.size() > 12)
            recent.pop_front();
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
