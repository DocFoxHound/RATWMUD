// The ambient director's voice (Docs/Design/26-living-npcs.md, Phase 10): the world picks an exchange worth hearing
// (World::ambientPicks) where players are; here it is written (the NPC Mind, or authored lines past the hour's budget
// or without one) and spoken a line at a time, as NPCs speak to players, to whoever can hear.
#include "RatwGame.h"

#include <algorithm>
#include <cmath>

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
    const auto begin = [this, id](const mind::Exchange& written) {
        for (auto& t : ambient_)
            if (t.id == id)
            {
                t.lines.assign(written.lines.begin(), written.lines.end());
                t.voiced = true;
                t.generated = written.generated;
                t.nextAt = world_.time();
                note("info", "RATW_AMBIENT " + t.pick.topic.kind + " " + t.pick.teller + " to " + t.pick.listener + " in " + t.pick.cell +
                                 (written.generated ? " (the Mind)" : " (authored)"));
            }
    };
    // The model is asked only so often; past that, and without it, the authored lines speak.
    while (!ambientCalls_.empty() && now - ambientCalls_.front() > 3600)
        ambientCalls_.pop_front();
    if (!mind_.live() || int(ambientCalls_.size()) >= AmbientCallsPerHour)
    {
        begin(mind::Client::authoredExchange(context));
        return;
    }
    ambientCalls_.push_back(now);
    std::weak_ptr<bool> alive = alive_;
    mind_.exchange(context, [alive, begin](const mind::Exchange& written) {
        if (!alive.expired())
            begin(written);
    });
}
} // namespace ratw::game
