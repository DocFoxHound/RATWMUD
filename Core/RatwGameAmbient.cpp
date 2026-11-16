// The ambient director's voice (Docs/Design/26-living-npcs.md, Phase 10): the world picks an exchange worth hearing
// (World::ambientPicks) where players are; here it is written (the talk library, doc 62; the NPC Mind for what matters
// most, within the hour's budget; authored lines when nothing fits) and spoken a line at a time, as NPCs speak to
// players, to whoever can hear. Barks, a resident calling out to no one in particular, are the talk library's too.
#include "RatwClaims.h"
#include "RatwItems.h"
#include "RatwSermons.h"
#include "RatwGame.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <set>
#include <tuple>

namespace ratw::game
{
namespace
{
constexpr double LookEvery = 5;                     // World seconds between looks for an exchange.
constexpr std::size_t AtOnce = 3;                   // Exchanges under way across the world.
constexpr double Apart = 5;                         // Tiles: walk further apart and the exchange stops.
constexpr double ConversationQuiet = 60;            // Give a player's conversation room before more ambient speech.
constexpr double PersonFactsEvery = 30;             // World seconds a talk keeps a speaker's person facts (doc 84 §8.2).

// The authored lines (no library line fits) with their moves (doc 84, Phase 2), so a transcript says what they do.
talk::Said authoredMoves(const talk::Library& lib, const std::string& kind, const talk::Facts& facts, const mind::Exchange& x)
{
    static const std::map<std::string, std::vector<std::string>> Moves{
        {"gossip", {"tell.gossip", "react.news"}}, {"news", {"tell.news", "react.news"}},
        {"quarrel", {"challenge", "counter"}},     {"friends", {"greet", "greet.return"}}};
    const auto found = Moves.find(kind);
    const std::vector<std::string> pair = found == Moves.end() ? std::vector<std::string>{"give.opinion", "agree"} : found->second;
    talk::Said said;
    said.lines = x.lines;
    std::vector<std::string> before;
    for (std::size_t i = 0; i < x.lines.size(); ++i)
    {
        talk::SaidLine line;
        line.who = x.lines[i].first;
        line.to = 1 - line.who;
        line.text = x.lines[i].second;
        line.moves = {pair[std::min<std::size_t>(i, 1)]};
        lib.describeMoves(line, before, facts, i + 1 < x.lines.size());   // (Written whole, as a scene is.)
        before = line.moves;
        said.info.push_back(std::move(line));
    }
    return said;
}

double millisSince(std::chrono::steady_clock::time_point t)
{
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t).count();
}

std::string pairKey(const std::string& a, const std::string& b) { return std::min(a, b) + "|" + std::max(a, b); }

bool hasMove(const std::vector<std::string>& moves, const char* move)
{
    return std::find(moves.begin(), moves.end(), move) != moves.end();
}
} // namespace

void Game::ambient(double dt)
{
    // The director, as every tick runs it: what is due said, then, every five seconds and while fewer than three are
    // under way, a look for a new exchange where the players (and the simulator's listeners) can hear. Split in two
    // (doc 84 §15.1, Phase 0) so the conversation simulator runs the same director; the split changes nothing said
    // (Tests/fixtures/talk_golden, the golden transcripts).
    ambientSpeak(dt);
    if ((exchangeLookIn_ += dt) < LookEvery || ambient_.size() >= AtOnce)
        return;
    exchangeLookIn_ = 0;
    const double now = world_.time();
    std::vector<std::string> listeners;
    for (const auto* c : clients_)
        if (!c->entityId.empty() && (!lastChat_.count(c->entityId) || now - lastChat_.at(c->entityId) >= ConversationQuiet))
            listeners.push_back(c->entityId);
    for (const auto& id : talkListeners_)            // (The simulator's: none on a server.)
        if (world_.entity(id) && (!lastChat_.count(id) || now - lastChat_.at(id) >= ConversationQuiet))
            listeners.push_back(id);
    ambientStart(listeners);
}

void Game::ambientSpeak(double)
{
    const double now = world_.time();
    // Speak what is due; stop an exchange when its speakers part, fall, or a player talks to one of them.
    for (auto it = ambient_.begin(); it != ambient_.end();)
    {
        const auto* a = world_.entity(it->pick.teller);
        const auto* b = world_.entity(it->pick.listener);
        // A third who joined (doc 84 §8.9) parts as the two may: gone, walked off, or busy with a player.
        const auto* third = it->people.size() > 2 ? world_.entity(it->people[2]) : nullptr;
        const bool thirdGone = it->people.size() > 2 && a &&
                               (!third || third->dead || third->offstage || third->cellId != a->cellId ||
                                std::hypot(third->position.x - a->position.x, third->position.y - a->position.y) > Apart ||
                                pendingNpc_.count(third->id));
        const bool broken = !a || !b || a->dead || b->dead || a->cellId != b->cellId || a->offstage || b->offstage ||
                            std::hypot(a->position.x - b->position.x, a->position.y - b->position.y) > Apart ||
                            pendingNpc_.count(a->id) || pendingNpc_.count(b->id) || thirdGone ||
                            (lastChat_.count(it->pick.hearer) && now - lastChat_.at(it->pick.hearer) < ConversationQuiet);
        const bool done = it->engine ? it->voiced && it->finished : it->voiced && it->lines.empty();
        if (broken || done)
        {
            std::string reason = "done";
            if (broken)
                reason = !a || !b || a->dead || b->dead || a->offstage || b->offstage ? "gone"
                         : a->cellId != b->cellId                                       ? "cell"
                         : std::hypot(a->position.x - b->position.x, a->position.y - b->position.y) > Apart ? "apart"
                         : pendingNpc_.count(a->id) || pendingNpc_.count(b->id)        ? "busy"
                         : thirdGone                                                    ? "apart"
                                                                                        : "chat";
            if (it->engine)
            {
                it->chain = it->conv.chain;
                it->ended = it->conv.ended;
                it->written = int(it->conv.lines.size());
                ambientFinish(*it, reason);
            }
            if (onAmbientEnd)                         // (The simulator's transcripts: doc 84 §15.1.)
            {
                AmbientEnd end{it->id, it->pick.teller, it->pick.listener, it->pick.cell, it->pick.hearer, it->pick.topic.kind,
                               it->route, reason, it->spoken, it->written, it->chain, it->ended};
                end.people = it->people.empty() ? std::vector<std::string>{it->pick.teller, it->pick.listener} : it->people;
                end.stake = it->conv.stake;
                end.marked = it->marked;
                end.dropped = it->conv.dropped;
                onAmbientEnd(end);
            }
            it = ambient_.erase(it);
            continue;
        }
        if (it->voiced && now >= it->nextAt)
        {
            int who = 0;
            std::string text;
            talk::LineInfo info;
            if (it->engine)
            {
                // The next line, chosen now it is due (doc 84 §8.2), or the talk is over.
                auto line = ambientBeat(*it);
                if (!line)
                {
                    it->finished = true;
                    ++it;
                    continue;
                }
                info = std::move(*line);
                who = info.who;
                text = info.text;
            }
            else
            {
                std::tie(who, text) = it->lines.front();
                it->lines.pop_front();
                if (!it->info.empty())
                {
                    info = std::move(it->info.front());
                    it->info.pop_front();
                }
            }
            const auto& speaker = it->engine ? it->people.at(std::size_t(who)) : who == 0 ? it->pick.teller : it->pick.listener;
            const auto spokenTo = it->engine ? (info.to >= 0 && std::size_t(info.to) < it->people.size() ? it->people[std::size_t(info.to)]
                                                                                                         : std::string())
                                             : who == 0 ? it->pick.listener : it->pick.teller;
            ParsedPost post;
            post.ok = true;
            post.speech = true;
            post.segments.push_back({"speech", text});
            publish(speaker, post, Voice::Speak);
            npcLastSpeech_[speaker] = now;
            it->nextAt = now + 2.5 + double(text.size()) / 25;   // Time to read it, before the answer.
            if (options_.logTalkWords)
                voicedWords("exchange", speaker, spokenTo, text, &info);
            if (onAmbientLine)
            {
                const auto knew = it->knew.find(spokenTo);
                onAmbientLine({it->id, it->spoken, speaker, spokenTo, text, a->cellId, it->pick.hearer, it->pick.topic.kind,
                               it->pick.topic.claim, it->route, info, it->pick.topic.subject, it->pick.topic.seen,
                               knew == it->knew.end() ? std::string() : knew->second});
            }
            if (it->engine)
            {
                ambientSaid(*it, info);
                if (it->conv.phase == talk::Conversation::Phase::Ended && it->conv.queued.empty())
                    it->finished = true;
            }
            ++it->spoken;
        }
        ++it;
    }
}

void Game::ambientStart(const std::vector<std::string>& listeners)
{
    // A new exchange for these listeners: the director's pick among those standing near them, written (the talk
    // library; the Mind only where ambientModelCallsPerHour allows) and begun at once.
    if (listeners.empty() || ambient_.size() >= AtOnce)
        return;
    const auto clock = std::chrono::steady_clock::now();
    struct Timed
    {
        Game& g;
        std::chrono::steady_clock::time_point at;
        ~Timed()
        {
            if (g.talkTiming_)
                g.talkTiming_->looks.push_back(millisSince(at));
        }
    } timed{*this, clock};
    const double now = world_.time();
    std::set<std::string> busy(pendingNpc_.begin(), pendingNpc_.end());
    for (const auto& [npc, queue] : queuedTalk_)
        busy.insert(npc);
    for (const auto& t : ambient_)
    {
        busy.insert(t.pick.teller), busy.insert(t.pick.listener);
        busy.insert(t.people.begin(), t.people.end());
    }
    for (const auto& [player, d] : dialogues_)      // (At the window with a player: doc 84 §8.10, "called".)
        busy.insert(d.npc);
    auto picks = world_.ambientPicks(listeners, busy);
    if (picks.empty())
        return;
    auto pick = std::move(picks.front());
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
    // Spoken a beat at a time by the turn engine (doc 84 §8.2, Phase 4) with a library that has moves.json and no model
    // writing it: it counts from now, and each line does what it does as it is said. Otherwise (a library from before
    // the moves, the golden transcripts' a46cbdac; the model) as before: written whole, and what talk does done now.
    const bool beats = !talk_.moves().empty() && !live;
    if (beats)
        world_.ambientBegun(pick);
    else
        world_.ambientSpoken(pick);                 // It counts from now: the talk is under way.
    const auto* a = world_.entity(pick.teller);
    const auto context = exchangeContext(pick);
    AmbientTalk started;
    started.id = ambientNext_++;
    started.pick = pick;
    ambient_.push_back(started);
    const auto id = started.id;
    const auto begin = [this, id](const mind::Exchange& written, const char* route, const talk::Said* said = nullptr) {
        for (auto& t : ambient_)
            if (t.id == id)
            {
                t.lines.assign(written.lines.begin(), written.lines.end());
                t.voiced = true;
                t.generated = written.generated;
                t.nextAt = world_.time();
                t.route = route;
                t.written = int(t.lines.size());
                if (said && said->info.size() == said->lines.size())
                {
                    t.info.assign(said->info.begin(), said->info.end());
                    t.chain = said->chain;
                    t.ended = said->ended;
                }
                note("info", "RATW_AMBIENT " + t.pick.topic.kind + " " + t.pick.teller + " to " + t.pick.listener + " in " + t.pick.cell +
                                 " (" + route + ")");
                voiced("exchange", route, t.pick.teller);
            }
    };
    world_.talkFor(pick.hearer);                    // (Its prices at the list of the player it was picked for: doc 79, 11.)
    const auto q = exchangeQuery(pick, context);
    world_.talkFor({});
    const auto cell = pick.cell;
    const auto seed = id * 2654435761u + std::uint64_t(now * 7);
    if (beats)
    {
        if (!ambientBegin(ambient_.back(), q, context, seed))
            ambient_.pop_back();
        return;
    }
    const auto fromTalk = [this, q, cell, seed, teller = a->id](const char*& route, talk::Said* kept = nullptr) {
        auto said = talkPick(q, teller, cell, 16, seed, true);
        mind::Exchange out;
        out.lines = said.lines;
        route = said.id.rfind("lib_", 0) == 0 ? "library" : "scene";   // (Doc 28's library, or a written scene.)
        if (kept)
            *kept = std::move(said);
        return out;
    };
    const auto authored = [this, kind = pick.topic.kind, facts = q.facts](const mind::Exchange& x) {
        return authoredMoves(talk_, kind, facts, x);
    };
    if (!live)
    {
        const char* route = "written";
        talk::Said said;
        if (auto x = fromTalk(route, &said); !x.lines.empty())
            begin(x, route, &said);
        else
        {
            const auto x2 = mind::Client::authoredExchange(context);
            const auto moves = authored(x2);
            begin(x2, "written", &moves);
        }
        return;
    }
    ambientCalls_.push_back(now);
    if (!pick.topic.incident.empty())
        voicedIncidents_.insert(pick.topic.incident);
    std::weak_ptr<bool> alive = alive_;
    mind_.exchange(context, [alive, begin, fromTalk, authored](const mind::Exchange& written) {
        if (alive.expired())
            return;
        const char* route = "written";
        talk::Said said;
        if (written.generated)
            begin(written, "model");
        else if (auto x = fromTalk(route, &said); !x.lines.empty())
            begin(x, route, &said);
        else
        {
            const auto moves = authored(written);
            begin(written, "written", &moves);
        }
    });
}

bool Game::ambientBegin(AmbientTalk& t, const talk::Query& start, const mind::ExchangeContext& context, std::uint64_t seed)
{
    // Doc 84 §8.2 (Phase 4): the talk's first line now; the rest a line at a time as each comes due (ambientBeat).
    const double now = world_.time();
    t.engine = true;
    t.people = {t.pick.teller, t.pick.listener};
    auto q = start;
    // Who held the topic's claim before it began (§8.6: `knows`: "I heard." rather than "Never!"), the teller always.
    if (claimTopic(t.pick))
        for (auto* s : {&q.a, &q.b})
        {
            const auto id = s->facts.count("id") ? s->facts.at("id") : std::string();
            t.knew[id] = id == t.pick.teller || claimHeld(t, id) ? "yes" : "no";
            s->facts["knows"] = t.knew[id];
        }
    // A talk this pair left unfinished (§8.10): its topic, kept two game days.
    if (const auto m = q.facts.find("pair.mem.thread"); m != q.facts.end())
        q.facts["pair.thread"] = m->second;
    t.queries[t.pick.teller + "|" + t.pick.listener] = q;
    t.fresh[t.pick.teller] = t.fresh[t.pick.listener] = now;
    // The place as they began: rain falling on them already, fights already under way (neither breaks it off).
    if (const auto* c = world_.cell(t.pick.cell))
        t.rainAtStart = c->outdoors && (c->weather == Weather::Rain || c->weather == Weather::Storm);
    for (const auto& b : world_.battles())
        if (b.cellId == t.pick.cell)
            t.fights.insert(b.id);
    t.conv.seed = seed;
    auto first = talk_.begin(t.conv, q, talkFreshness(t, 0));
    if (first)
    {
        t.route = !t.conv.chain.empty() && t.conv.chain.front().rfind("lib_", 0) == 0 ? "library" : "scene";
        auto& recent = recentScenes_[t.pick.cell];
        recent.push_back(t.conv.chain.front());
        while (recent.size() > 16)
            recent.pop_front();
    }
    else
    {
        // Nothing in the library fits: the authored lines, and the engine carries on from them (an answer owed, the
        // closing), so they end as the library's talks do (§8.4).
        const auto x = mind::Client::authoredExchange(context);
        talk::Conversation fresh;
        fresh.seed = seed;
        t.conv = std::move(fresh);
        first = talk_.beginWith(t.conv, q, authoredMoves(talk_, t.pick.topic.kind, q.facts, x));
        t.route = "written";
    }
    if (!first)
        return false;
    t.due = std::move(first);
    t.voiced = true;
    t.nextAt = now;
    note("info", "RATW_AMBIENT " + t.pick.topic.kind + " " + t.pick.teller + " to " + t.pick.listener + " in " + t.pick.cell + " (" +
                     t.route + ")");
    voiced("exchange", t.route.c_str(), t.pick.teller);
    return true;
}

talk::Freshness Game::talkFreshness(const AmbientTalk& t, int speaker) const
{
    // What those who'd hear the speaker have heard (doc 62: what no listener has heard first), and the place's recent
    // openers (never one said here lately).
    const auto who = std::size_t(speaker) < t.people.size() ? t.people[std::size_t(speaker)] : t.pick.teller;
    talk::Freshness fresh;
    fresh.heard = [this, hearers = hearersOf(who)](const std::string& entry) {
        for (const auto& h : hearers)
            if (const auto found = scenesHeard_.find(h); found != scenesHeard_.end() && found->second.ids.count(entry))
                return true;
        return false;
    };
    if (const auto recent = recentScenes_.find(t.pick.cell); recent != recentScenes_.end())
        fresh.recent = recent->second;
    return fresh;
}

talk::Query Game::talkBeat(AmbientTalk& t, int speaker, int to)
{
    // Library::next's contract: the teller and the listener as a and b between the two of them; with a third, the
    // speaker as a and the one spoken to as b.
    const double now = world_.time();
    const bool firstTwo = speaker < 2 && to < 2;
    const auto at = [&](int i) { return i >= 0 && std::size_t(i) < t.people.size() ? t.people[std::size_t(i)] : std::string(); };
    const auto a = firstTwo ? t.pick.teller : at(speaker);
    const auto b = firstTwo ? t.pick.listener : at(to);
    auto found = t.queries.find(a + "|" + b);
    if (found == t.queries.end())
    {
        // A third and another (§8.9): as talkQuery has it, with the talk's topic, and its prices as it began.
        const auto& start = t.queries.at(t.pick.teller + "|" + t.pick.listener);
        talk::Facts topic;
        for (const auto& [k, v] : start.facts)
            if (k.rfind("topic.", 0) == 0 || (k.rfind("world.", 0) == 0 && k.rfind("world.mem.", 0) != 0 && k.rfind("world.said:", 0) != 0))
                topic[k] = v;
        auto q = talkQuery(a, b, start.concept, topic);
        q.bind = [this, subject = t.pick.topic.subject, people = std::move(q.bind), a, b](const std::string& slot, int side,
                                                                                         talk::Facts& out) {
            if (slot != "topic.subject")
                return people && people(slot, side, out);
            const auto* s = world_.entity(subject);
            return s && s->npc && talkThird(side == 0 ? a : b, subject, out);
        };
        for (auto* s : {&q.a, &q.b})
            if (const auto k = t.knew.find(s->facts.count("id") ? s->facts.at("id") : std::string()); k != t.knew.end())
                s->facts["knows"] = k->second;
        for (const auto& id : {a, b})
            t.fresh.emplace(id, now);
        found = t.queries.emplace(a + "|" + b, std::move(q)).first;
    }
    auto& base = found->second;
    // A speaker's person facts are worked out again every 30 s (peopleOf and neighboursOf walk the town: §16, B1);
    // what they knew as it began, and whether they deal in its goods, stay.
    for (auto* s : {&base.a, &base.b})
    {
        const auto id = s->facts.count("id") ? s->facts.at("id") : std::string();
        const auto* e = id.empty() ? nullptr : world_.entity(id);
        if (!e || now - t.fresh[id] < PersonFactsEvery)
            continue;
        auto renewed = talkSpeaker(*e);
        for (const char* k : {"knows", "deals", "sold_out"})
            if (const auto kept = s->facts.find(k); kept != s->facts.end())
                renewed.facts[k] = kept->second;
        *s = std::move(renewed);
        t.fresh[id] = now;
    }
    auto q = base;
    // The moment (here.*: the hour, the sky), how each is now (hurt, fighting), the bonds, and what is remembered: fresh.
    for (auto it = q.facts.begin(); it != q.facts.end();)
        it = it->first.rfind("here.", 0) == 0 || it->first.rfind("pair.mem.", 0) == 0 || it->first.rfind("pair.said:", 0) == 0 ||
                     it->first.rfind("world.mem.", 0) == 0 || it->first.rfind("world.said:", 0) == 0 || it->first == "pair.thread"
                 ? q.facts.erase(it)
                 : std::next(it);
    world_.talkMoment(a, q.facts);
    if (const auto region = q.facts.find("here.region"); region != q.facts.end())
    {
        q.facts["here.fishers"] = std::to_string(townFishers(region->second));
        talkRecall("town:" + region->second, "world.", q.facts);
    }
    for (auto* s : {&q.a, &q.b})
    {
        if (s == &q.b && q.alone)
            break;
        const auto id = s->facts.count("id") ? s->facts.at("id") : std::string();
        for (auto it = s->facts.begin(); it != s->facts.end();)
            it = it->first.rfind("mem.", 0) == 0 || it->first.rfind("said:", 0) == 0 ? s->facts.erase(it) : std::next(it);
        talkRecall("person:" + id, "", s->facts);
        if (const auto* e = world_.entity(id); e && e->npc)
        {
            s->facts["injured"] = e->hurt >= 50 || e->downedLeft > 0 ? "bad" : e->hurt >= 1 ? "light" : "none";
            s->facts["fighting"] = world_.inBattle(e->id) ? "yes" : "no";
        }
    }
    if (!q.alone)
    {
        talkBond(a, b, q.facts);
        talkRecall("pair:" + pairKey(a, b), "pair.", q.facts);
        if (const auto m = q.facts.find("pair.mem.thread"); m != q.facts.end())
            q.facts["pair.thread"] = m->second;
    }
    return q;
}

std::optional<talk::SaidLine> Game::ambientBeat(AmbientTalk& t)
{
    if (t.due)                                      // (The first line, chosen as it began.)
    {
        auto first = std::move(t.due);
        t.due.reset();
        return first;
    }
    if (t.conv.phase == talk::Conversation::Phase::Ended)
        return std::nullopt;
    const auto clock = std::chrono::steady_clock::now();
    const auto facts = [&](int speaker, int to) { return talkBeat(t, speaker, to); };
    const int next = t.conv.lines.empty() ? 1 : talk::Library::partner(t.conv, t.conv.lines.back().who);
    const auto fresh = talkFreshness(t, next);
    std::optional<talk::SaidLine> line;
    // Something breaks it off (§8.10): the next line says so, and it ends.
    int who = next;
    if (const auto why = interruption(t, who); !why.empty())
    {
        line = talk_.interrupt(t.conv, why, who, facts, talkFreshness(t, who));
        t.marked = line.has_value();
    }
    // A third voice (§8.9), cutting in between lines, before what was owed is answered.
    else if (!(line = joiners(t, fresh)))
        line = talk_.next(t.conv, facts, fresh);
    if (talkTiming_)
        talkTiming_->beats.push_back(millisSince(clock));
    return line;
}

std::string Game::interruption(const AmbientTalk& t, int& who) const
{
    if (t.conv.lines.empty())
        return {};
    // One of them called to the window by a player ("One moment, Wick.").
    for (const auto& [player, d] : dialogues_)
        for (std::size_t i = 0; i < t.people.size(); ++i)
            if (d.npc == t.people[i])
            {
                who = int(i);
                return "called";
            }
    // A fight breaking out where they stand (not one already under way as they began).
    for (const auto& b : world_.battles())
        if (!b.over && b.cellId == t.pick.cell && !t.fights.count(b.id))
            return "fight";
    // Rain starting on them in the open (the place's sky, as here.weather says it).
    if (const auto* c = world_.cell(t.pick.cell); c && c->outdoors && !t.rainAtStart &&
                                                  (c->weather == Weather::Rain || c->weather == Weather::Storm))
        return "rain";
    return {};
}

std::optional<talk::SaidLine> Game::joiners(AmbientTalk& t, const talk::Freshness& fresh)
{
    // §8.9: one more may join a talk of two in its body (frames.json's join): idle, within reach of both, hearing it,
    // out of talk a while, knowing one of them, and with a stake in what was just said; only with a line that fits.
    const auto& rules = talk_.beats();
    if (int(t.people.size()) >= rules.joinPeople || t.conv.people.size() != 2 || t.conv.lines.empty() || !t.conv.queued.empty() ||
        t.conv.phase != talk::Conversation::Phase::Body || int(t.conv.lines.size()) + 4 > talk_.budgets().most)
        return std::nullopt;
    const auto* a = world_.entity(t.pick.teller);
    const auto* b = world_.entity(t.pick.listener);
    if (!a || !b)
        return std::nullopt;
    const double now = world_.time();
    const int lastWho = t.conv.lines.back().who;
    const auto lastSpeaker = t.people.at(std::size_t(lastWho));   // (A copy: t.people grows below.)
    std::set<std::string> busy(pendingNpc_.begin(), pendingNpc_.end());
    for (const auto& [npc, queue] : queuedTalk_)
        busy.insert(npc);
    for (const auto& [player, d] : dialogues_)
        busy.insert(d.npc);
    for (const auto& other : ambient_)
    {
        busy.insert(other.pick.teller), busy.insert(other.pick.listener);
        busy.insert(other.people.begin(), other.people.end());
    }
    static const std::set<std::string> Occupied{"sleep", "patrol", "companion", "relocate", "held in the gaol",
                                                "looking for a chance", "guarding a caravan", "carrying a letter"};
    const auto& society = world_.society();
    const auto& topic = t.pick.topic;
    const auto subject = world_.entity(topic.subject);
    const bool claimSaid = std::any_of(t.conv.lines.begin(), t.conv.lines.end(), [](const talk::SaidLine& l) { return !l.asserts.empty(); }) ||
                           (claimTopic(t.pick) && t.spoken > 0);
    // A claim that speaks ill of its subject (claims.json's grave ones, a theft, a warrant): kin and friends defend.
    static const std::set<std::string> Ill{"theft", "theft.attempt", "assault", "violent", "warrant", "prowling", "promise.broken"};
    const auto bond = [&](const std::string& x, const std::string& y) { return world_.bonds().find(x, y); };
    const auto& pacing = world_.ambientPacing();
    const auto& startFacts = t.queries.at(t.pick.teller + "|" + t.pick.listener).facts;
    struct Candidate
    {
        double distance = 0;
        std::string id, stake;
        std::vector<std::string> moves;
    };
    std::vector<Candidate> found;
    for (const auto* e : world_.entitiesIn(t.pick.cell))
    {
        if (!e || !e->npc || e->dead || e->transient || e->offstage || busy.count(e->id) || e->speakingUntil > now || !e->path.empty() ||
            std::hypot(e->velocity.x, e->velocity.y) > .05 || e->posture == "lying" || !e->leaderId.empty() ||
            std::find(t.people.begin(), t.people.end(), e->id) != t.people.end())
            continue;
        const double da = std::hypot(e->position.x - a->position.x, e->position.y - a->position.y);
        const double db = std::hypot(e->position.x - b->position.x, e->position.y - b->position.y);
        if (da > rules.joinTiles || db > rules.joinTiles)
            continue;
        const auto* life = society.resident(e->id);
        if (!life || Occupied.count(life->task) || life->task.rfind("stopping ", 0) == 0)
            continue;
        if (const auto p = pacing.find(e->id); p != pacing.end() && now - p->second < rules.joinRest)
            continue;
        if (world_.hearingClarity(e->id, lastSpeaker, Voice::Speak) < rules.joinHearing)
            continue;
        const auto knows = [&](const std::string& o) {
            const auto* k = bond(e->id, o);
            return (k && k->familiarity >= 1) || society.family(e->id, o) || society.household(e->id, o);
        };
        if (!knows(a->id) && !knows(b->id))
            continue;
        // The stake (§8.9), the strongest first.
        Candidate c{std::min(da, db), e->id, {}, {}};
        const bool kin = subject && (society.family(e->id, subject->id) ||
                                     (society.spouse(e->id) && *society.spouse(e->id) == subject->id));
        const auto* toSubject = subject ? bond(e->id, subject->id) : nullptr;
        const auto& lastText = t.conv.lines.back().text;
        const auto good = startFacts.find("topic.good");
        const auto deals = [&] {
            if (good == startFacts.end() || !society.merchant(e->id))
                return false;
            const auto wares = society.wares(e->id);   // (A copy: wares() makes its list.)
            return std::any_of(wares.begin(), wares.end(), [&](const std::string& w) { return items::baseOf(w) == good->second; });
        };
        if ((subject && e->id == subject->id && claimSaid) || (!e->name.empty() && lastText.find(e->name) != std::string::npos))
            c.stake = "named", c.moves = {"join.object"};
        else if (subject && subject->npc && claimSaid && (kin || (toSubject && toSubject->affinity >= 40)))
            c.stake = kin ? "kin" : "friend", c.moves = {Ill.count(topic.claimKind) ? "join.defend" : "join.add"};
        else if (deals())
            c.stake = "goods", c.moves = {"join.add"};
        else if (!topic.incident.empty() &&
                 std::any_of(world_.crime().incidents.begin(), world_.crime().incidents.end(), [&](const Incident& i) {
                     return i.id == topic.incident && std::any_of(i.witnesses.begin(), i.witnesses.end(),
                                                                   [&](const Witness& w) { return w.id == e->id; });
                 }))
            c.stake = "witness", c.moves = {"join.add"};
        else
        {
            // Close to both (friends, kin, a shared roof: World::pairBand) and one who likes to talk.
            static const std::set<std::string> Close{"friends", "family", "spouses", "housemates"};
            const auto voice = npcVoice_.find(e->id);
            const bool chatty = voice != npcVoice_.end() && voice->second.traits.count("talk") && voice->second.traits.at("talk") == "chatty";
            if (chatty && Close.count(world_.pairBand(e->id, a->id)) && Close.count(world_.pairBand(e->id, b->id)))
                c.stake = "friends", c.moves = {"join.add"};
        }
        if (!c.stake.empty())
            found.push_back(std::move(c));
    }
    std::sort(found.begin(), found.end(), [](const Candidate& x, const Candidate& y) {
        return x.distance < y.distance || (x.distance == y.distance && x.id < y.id);
    });
    // The nearest two at most (a candidate's query is a whole one: §16, B1).
    if (found.size() > 2)
        found.resize(2);
    for (const auto& c : found)
    {
        // Their query: the joiner as a, the one who spoke last as b.
        t.people.push_back(c.id);
        auto q = talkBeat(t, int(t.people.size()) - 1, lastWho);
        t.people.pop_back();
        if (claimTopic(t.pick) && !t.knew.count(c.id))
        {
            t.knew[c.id] = claimHeld(t, c.id) || c.id == topic.subject ? "yes" : "no";
            q.a.facts["knows"] = t.knew[c.id];
        }
        if (auto line = talk_.join(t.conv, c.id, c.stake, c.moves, q, fresh))
        {
            t.people.push_back(c.id);
            world_.ambientPacing()[c.id] = now;
            if (const auto key = t.queries.find(c.id + "|" + lastSpeaker); key != t.queries.end())
                key->second.a.facts["knows"] = t.knew.count(c.id) ? t.knew[c.id] : std::string();
            return line;
        }
        t.queries.erase(c.id + "|" + lastSpeaker);
    }
    return std::nullopt;
}

const Belief* Game::claimHeld(const AmbientTalk& t, const std::string& who) const
{
    const auto& topic = t.pick.topic;
    return world_.holding(who, topic.subject, topic.claim, topic.incident, topic.claimKind, topic.object);
}

void Game::ambientSaid(AmbientTalk& t, const talk::SaidLine& line)
{
    // What a line said does, as it is said (doc 84 §8.2, §8.6).
    const auto at = [&](int i) { return i >= 0 && std::size_t(i) < t.people.size() ? t.people[std::size_t(i)] : std::string(); };
    const auto speaker = at(line.who);
    const auto to = at(line.to);
    // Heard: every rule and scene said so far (a greeting with its opener), by all who could hear it (defect 5).
    const auto hearers = hearersOf(speaker);
    for (; t.chainDone < t.conv.chain.size(); ++t.chainDone)
        for (const auto& h : hearers)
            scenesHeard_[h].add(t.conv.chain[t.chainDone]);
    // What it remembers, and that it was said (once, cooldowns).
    if (t.effectsDone < t.conv.effects.size())
    {
        const std::vector<talk::Effect> now(t.conv.effects.begin() + std::ptrdiff_t(t.effectsDone), t.conv.effects.end());
        t.effectsDone = t.conv.effects.size();
        talkRemember(now);
    }
    // Its moves' effects on how the two feel (moves.json: "self", the speaker toward the one spoken to; "other", theirs
    // toward the speaker), bounded as talk is (Game::regard, "talk"). Replaces the blanket warmth of a whole talk.
    if (!speaker.empty() && !to.empty())
        for (const auto& m : line.moves)
            if (const auto* mv = talk_.moves().find(m))
                for (const auto& [whose, axes] : mv->effects)
                {
                    BondChange change;
                    for (const auto& [axis, amount] : axes)
                        (axis == "affinity" ? change.affinity : axis == "trust" ? change.trust : axis == "respect" ? change.respect
                         : axis == "fear" ? change.fear : change.familiarity) += amount;
                    if (whose == "self")
                        regard(speaker, to, change, "talk", "said " + m);
                    else
                        regard(to, speaker, change, "talk", "heard " + m);
                }
    // The claim (§8.6). A line that asserts it passes it on (an opener that says nothing of what it does, the teller's
    // first line, as every talk on it did before the moves); one who denies it, the subject or their kin, weakens it.
    const auto& topic = t.pick.topic;
    if (claimTopic(t.pick))
    {
        const bool untagged = line.moves.empty() && line.who == 0 && t.spoken == 0;
        if (!line.asserts.empty() || untagged)
        {
            const auto* held = claimHeld(t, speaker);
            const double sure = held ? held->confidence : speaker == t.pick.teller ? topic.confidence : 0;
            const auto via = held ? held->source : topic.source;
            const auto pass = [&](const std::string& holder, double factor) {
                Belief told;
                told.holder = holder;
                told.subject = topic.subject;
                told.claim = held ? held->claim : topic.claim;
                told.source = speaker;
                told.confidence = sure * factor;
                told.incident = topic.incident;
                told.as = topic.as;                 // (Doc 56.)
                told.kind = held && !held->kind.empty() ? held->kind : topic.claimKind;
                told.object = held && !held->object.empty() ? held->object : topic.object;
                told.place = held && !held->place.empty() ? held->place : topic.place;
                world_.told(told, via);
            };
            if (sure > 0)
            {
                // Those in it, by their trust in the speaker; those standing by who hear it, at 0.4.
                for (const auto& p : t.people)
                    if (p != speaker)
                    {
                        const auto* k = world_.bonds().find(p, speaker);
                        pass(p, std::clamp(.5 + .4 * (k ? k->trust : 0) / 100, .1, .9));
                    }
                for (const auto* e : world_.entitiesIn(t.pick.cell))
                    if (e && e->npc && !e->dead && !e->offstage && !e->transient &&
                        std::find(t.people.begin(), t.people.end(), e->id) == t.people.end() &&
                        world_.society().resident(e->id) && world_.hearingClarity(e->id, speaker, Voice::Speak) >= .5)
                        pass(e->id, .4);
                t.gist = true;
            }
        }
        if (!line.denies.empty() && (speaker == topic.subject || world_.society().family(speaker, topic.subject) ||
                                     (world_.society().spouse(speaker) && *world_.society().spouse(speaker) == topic.subject)))
            for (const auto& p : t.people)
                if (p != speaker)
                    if (const auto* held = claimHeld(t, p))
                    {
                        const auto* k = world_.bonds().find(p, speaker);
                        world_.doubted(p, held->subject, held->claim, 1 - .3 * (k ? k->trust : 0) / 100);
                    }
    }
    // The point of it made (§8.10): a thread is kept only for a talk broken off before.
    if (line.gist || !line.asserts.empty() || hasMove(line.moves, "preclose") || hasMove(line.moves, "farewell"))
        t.gist = true;
    // A thread taken up again is done with.
    if (hasMove(line.moves, "return"))
        if (const auto memo = talkMemory_.find("pair:" + pairKey(t.pick.teller, t.pick.listener)); memo != talkMemory_.end())
            memo->second.erase("thread");
}

void Game::ambientFinish(AmbientTalk& t, const std::string& reason)
{
    if (t.spoken == 0)
        return;                                     // (Nothing was said.)
    world_.ambientEnded(t.pick, t.people);
    // A curt ending costs regard (§8.4; frames.json's curt effects).
    if (t.conv.ended == "curt" && t.conv.closer >= 0 && std::size_t(t.conv.closer) < t.people.size())
    {
        const auto& closer = t.people[std::size_t(t.conv.closer)];
        const auto left = talk::Library::partner(t.conv, t.conv.closer);
        if (std::size_t(left) < t.people.size())
            for (const auto& [whose, axes] : talk_.beats().curtEffects)
            {
                BondChange change;
                for (const auto& [axis, amount] : axes)
                    (axis == "affinity" ? change.affinity : axis == "trust" ? change.trust : axis == "respect" ? change.respect
                     : axis == "fear" ? change.fear : change.familiarity) += amount;
                if (whose == "self")
                    regard(closer, t.people[std::size_t(left)], change, "talk", "left without a word");
                else
                    regard(t.people[std::size_t(left)], closer, change, "talk", "left without a word");
            }
    }
    // A talk broken off before its point was made is kept as the pair's thread (§8.10), unless one of them is dead.
    const auto* a = world_.entity(t.pick.teller);
    const auto* b = world_.entity(t.pick.listener);
    const bool brokenOff = t.conv.ended == "break" || reason != "done";
    if (brokenOff && !t.gist && !t.conv.topic.empty() && a && b && !a->dead && !b->dead && talk_.beats().threadDays > 0)
        talkRemember({{"pair", pairKey(t.pick.teller, t.pick.listener), "thread", t.conv.topic, talk_.beats().threadDays,
                       "broken off"}});
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
    context.claim = claims::rawId(pick.topic.claim) ? pick.topic.said : pick.topic.claim;   // (Never an id, to the Mind either.)
    // What the teller tells, in their words (doc 84, defect 9): the topic's own, never the day's sentence a topic with
    // nothing to tell would have put first among its facts.
    context.news = pick.topic.said;
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
        facts.emplace("topic.claim", context.claim);   // (The claim's words from claims.json, not its free words: doc 84.)
    if (!context.news.empty())                      // (A topic's own news, a wagon's, is kept: Phase 8 repair.)
    {
        facts.emplace("topic.news", mind::firstPerson(context.news, context.a.name));
        facts.emplace("topic.said", facts.at("topic.news"));
    }
    if (!context.day.empty())
        facts["topic.remark"] = context.day;
    auto q = talkQuery(pick.teller, pick.listener, "talk." + pick.topic.kind, facts);
    // binds x=topic.subject: the one the talk is about, when it is a resident (doc 84 §7.1).
    q.bind = [this, subject = pick.topic.subject, people = std::move(q.bind), a = pick.teller,
              b = pick.listener](const std::string& slot, int speaker, talk::Facts& out) {
        if (slot != "topic.subject")
            return people && people(slot, speaker, out);
        const auto* s = world_.entity(subject);
        return s && s->npc && talkThird(speaker == 0 ? a : b, subject, out);
    };
    return q;
}

talk::Query Game::talkTopicQuery(const std::string& a, const std::string& b, const std::string& hearer)
{
    AmbientPick pick;
    pick.teller = a;
    pick.listener = b;
    pick.hearer = hearer;
    if (const auto* e = world_.entity(a))
        pick.cell = e->cellId;
    world_.talkFor(hearer);
    pick.topic = world_.ambientTopic(a, b);
    auto q = pick.topic.kind.empty() ? talkQuery(a, b, "talk.day") : exchangeQuery(pick, exchangeContext(pick));
    world_.talkFor({});
    return q;
}

std::vector<std::string> Game::hearersOf(const std::string& speaker) const
{
    std::vector<std::string> out;
    const auto* s = world_.entity(speaker);
    for (const auto* c : clients_)
        if (const auto* e = world_.entity(c->entityId); s && e && e->cellId == s->cellId &&
                                                     world_.hearingClarity(e->id, speaker, Voice::Speak) >= .35)
            out.push_back(e->id);
    for (const auto& id : talkListeners_)             // (The simulator's listeners hear as players do: doc 84 §15.1.)
        if (const auto* e = world_.entity(id); s && e && e->cellId == s->cellId &&
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
            for (const auto& id : said.chain)       // (Every beat heard, the answers too, not only the opener: doc 84, defect 5.)
                scenesHeard_[h].add(id);
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
        if (lastChat_.count(player->id) && now - lastChat_.at(player->id) < ConversationQuiet)
            continue;
        if (const auto last = barkLast_.find(player->cellId); last != barkLast_.end() && now - last->second < 75)
            continue;
        std::vector<const Entity*> near;
        for (const auto& [id, e] : world_.entities())
            if (e.npc && !e.dead && !e.transient && !e.offstage && e.cellId == player->cellId && !busy.count(id) &&
                e.speakingUntil <= now && npcLastSpeech_[id] + 90 <= now && e.posture != "lying" &&
                std::hypot(e.position.x - player->position.x, e.position.y - player->position.y) <= 14 &&
                world_.hearingClarity(player->id, id, Voice::Speak) >= .5)
                near.push_back(&e);
        barkLast_[player->cellId] = now;          // Looked: the next look here waits, whether anyone spoke or not.
        if (near.empty())
            continue;
        const auto* who = near[std::size_t(std::uint64_t(now * 13) % near.size())];
        // A beggar near a player calls out first, every other look (doc 79, 10.2: a bark when a player is near).
        if (std::uint64_t(now * 7) % 2 == 0)
            for (const auto* e : near)
                if (e->activity.rfind("begging", 0) == 0)
                {
                    who = e;
                    break;
                }
        world_.talkFor(player->id);                 // (A price fact at the list of the player it's called to.)
        const auto q = talkQuery(who->id, {}, "bark");
        world_.talkFor({});
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
        if (options_.logTalkWords)
            voicedWords("bark", who->id, {}, chosen.lines.front().second, chosen.info.empty() ? nullptr : &chosen.info.front());
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
