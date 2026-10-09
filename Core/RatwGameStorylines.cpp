// Storylines in the game (Docs/Design/58-player-storytellers.md, 1-3; the rules are RatwStorylines.cpp). What happens
// in the world reaches the storylines at once: a player in a place (checked every 2 s, only for those with a place to be),
// a talk with a resident, a scene settled, a contract done, a kill, a fight won, a Gift used, an item given. Each step done
// is told (a toast, a moment on the scene's end card, a chronicle line), and the journal (the JOURNAL page's STORIES) and
// the tracked marker follow the book's version. A finished personal storyline is recognised, never paid (the user,
// 2026-10-08): a chronicle line and a small deed.
#include "RatwGame.h"

#include "RatwItems.h"

#include <algorithm>
#include <cmath>

namespace ratw::game
{
using json::Value;

storylines::Marker Game::markerFor(const std::string& role, const std::string& id)
{
    // A resident: where it works (public knowledge), else its home; a place: its middle (a town's: its square).
    storylines::Marker m;
    const auto place = [&](const std::string& cell, double x, double y, const std::string& label) {
        const auto* c = world_.cell(cell);
        m.cell = cell;
        m.x = x, m.y = y;
        m.label = label.empty() ? (c ? c->name : cell) : label;
    };
    if (const auto* spec = world_.society().spec(id))
    {
        const auto* job = world_.society().jobOf(id);
        const auto& at = job ? job->work : spec->home;
        const auto* c = world_.cell(at.cell);
        place(at.cell, at.x, at.y, (job ? job->title + " at " : std::string("at home in ")) + (c ? c->name : at.cell));
        return m;
    }
    if (const auto* c = world_.cell(id))
    {
        if (const auto town = world_.communityOf(id); role == "square" && !town.empty())
            if (const auto* sq = world_.marketSpot(town))
            {
                place(sq->cell, sq->x, sq->y, "the market at " + townWords(town));
                return m;
            }
        place(id, c->width / 2.0, c->height / 2.0, c->name);
    }
    return m;
}

std::string Game::storyWords(const std::string& viewer, std::string text) const
{
    // "[[id]]" as the reader knows that wolf ("you" for itself).
    for (auto at = text.find("[["); at != std::string::npos; at = text.find("[[", at))
    {
        const auto close = text.find("]]", at);
        if (close == std::string::npos)
            break;
        const auto id = text.substr(at + 2, close - at - 2);
        const auto words = id == viewer ? std::string("you") : world_.entity(id) || world_.society().spec(id) ? labelFor(viewer, id) : id;
        text.replace(at, close + 2 - at, words);
        at += words.size();
    }
    return text;
}

Result Game::giveStoryline(const std::string& character, const std::string& templateId, const Value& cast, const std::string& source,
                           const std::string& sourceRef, const std::vector<std::string>& also)
{
    const auto* t = storylines::findTemplate(templateId);
    const auto* who = world_.entity(character);
    if (!t)
        return {false, "No such story.", {}};
    if (!who && !characters_.count(character))
        return {false, "No such character.", {}};
    std::map<std::string, std::string> roles;
    for (const auto& [role, id] : cast.fields())
        if (id.isString())
            roles[role] = id.asString();
    // Roles left out: the place where the character is; the square of its town.
    const auto cell = who ? who->cellId : characters_.at(character).cellId;
    roles.try_emplace("place", cell);
    if (const auto town = world_.communityOf(cell); !town.empty())
        if (const auto* sq = world_.marketSpot(town))
            roles.try_emplace("square", sq->cell);
    std::string why;
    auto* s = storylines_.begin(*t, character, also, roles, source, sourceRef, now(),
                                [this](const std::string& role, const std::string& id) { return markerFor(role, id); }, why);
    if (!s)
        return {false, why, {}};
    // A place objective with a tile: the marker's.
    for (auto& step : s->steps)
        for (auto& o : step.objectives)
            if (o.kind == "place" && o.x < 0 && step.marker.set() && step.marker.cell == o.target)
                o.x = step.marker.x, o.y = step.marker.y;
    storylines_.touch();
    logEvent("storyline begun", character, s->id, s->title);
    for (const auto& [id, p] : s->participants)
        if (auto* c = clientOf(id))
            storylineToast(c, "In your journal: " + storyWords(id, s->title));
    saveSoon();
    return {true, s->id, s->id};
}

void Game::storylineToast(Connection* c, const std::string& text)
{
    auto e = Value::object();
    e.add("type", "storyline");
    e.add("toast", text);
    send(c, e);
    system(c, text);
}

void Game::storylineEvent(const storylines::Event& e)
{
    if (!storylines_.waiting(e.kind))
        return;
    storylineProgress(storylines_.happen(e, now()));
}

void Game::storylineProgress(const std::vector<storylines::Progress>& progress)
{
    for (const auto& p : progress)
    {
        auto* s = storylines_.find(p.storyline);
        if (!s)
            continue;
        if (p.kind == "objective")
        {
            const auto& o = s->steps[p.step].objectives[p.objective];
            logEvent("storyline objective", p.actor, s->id, o.line.substr(0, 120) + (p.byHand ? " (by hand)" : ""));
        }
        else if (p.kind == "step")
        {
            const auto& step = s->steps[p.step];
            logEvent("storyline step", p.actor, s->id, step.title.substr(0, 120));
            for (const auto& [id, part] : s->participants)
            {
                if (!part.active())
                    continue;
                const auto words = "Step done: " + storyWords(id, step.title) + " (" + storyWords(id, s->title) + ", " +
                                   std::to_string(p.step + 1) + " of " + std::to_string(s->steps.size()) + ")";
                if (auto* c = clientOf(id))
                    storylineToast(c, words);
                // A moment on the end card of the scene it is in (doc 51, 8).
                for (auto& [sid, scene] : social_.sessions)
                    if (scene.ended == 0 && scene.members.count(id) && scene.moments.size() < 24)
                        scene.moments.push_back({"step", id, s->id, storyWords(id, step.title), now()});
            }
        }
        else if (p.kind == "done")
            finishStoryline(*s);
    }
    if (!progress.empty())
        saveSoon();
}

void Game::finishStoryline(const storylines::Storyline& s)
{
    // Recognition only (the user, 2026-10-08): a chronicle line, and for a personal storyline the server made, a small
    // deed for whoever took part. A tale's end is its storyteller's (RatwGameStoryteller.cpp).
    logEvent("storyline done", s.owner.empty() ? s.authorCharacter : s.owner, s.id, s.title.substr(0, 120));
    for (const auto& [id, part] : s.participants)
    {
        if (!part.active())
            continue;
        if (auto* c = clientOf(id))
            storylineToast(c, "Story done: " + storyWords(id, s.title) + ".");
        if (s.kind == "personal")
            if (const auto* e = world_.entity(id); e && !e->npc)
                recordDeed("finished_story", {id}, {}, e->cellId, "storyline", storyWords(id, s.title));
    }
}

void Game::storylineTick(double dt)
{
    // Every couple of seconds: those with a place to be, where they are; a chain's contract done.
    storylineProbe_ += dt;
    if (storylineProbe_ < storylines::rules().placeEvery)
        return;
    storylineProbe_ = 0;
    if (storylines_.waiting("place"))
        for (const auto& id : storylines_.waitingFor("place"))
            if (const auto* e = world_.entity(id); e && !e->npc && clientOf(id))
                storylineEvent({"place", id, e->cellId, {}, {}, e->position.x, e->position.y, {}});
    // Contracts done since: a chain's job ticked; work well done for a resident may bring more (doc 58, 3). (Those done
    // before a start are only noted.)
    std::vector<Contract> done;
    for (const auto& k : world_.roads().contracts)
        if (k.status == "done" && !k.taker.empty() && chainsSeen_.insert(k.id).second && chainsSeeded_)
            done.push_back(k);
    chainsSeeded_ = true;
    for (const auto& k : done)
    {
        storylineEvent({"contract", k.taker, {}, k.id, k.poster, 0, 0, {}});
        offerChain(k);
    }
    if (chainsSeen_.size() > 4096)
        chainsSeen_.clear(), chainsSeeded_ = false;
    // A tale idle for its days pauses (doc 58, 1), and its storyteller is told.
    const double idle = storylines::rules().idleDays * 86400;
    for (const auto& [id, s] : storylines_.all())
        if (s.kind == "tale" && s.state == "running" && now() - s.lastActivity > idle)
        {
            const auto author = s.authorCharacter;
            const auto title = s.title;
            if (auto* t = storylines_.find(id))
                t->state = "paused";
            storylines_.touch();
            if (auto* c = clientOf(author))
                storylineToast(c, "Your tale \"" + title + "\" has been idle a month, and is paused.");
            break;
        }
}

Value Game::journalView(const std::string& viewer)
{
    // The journal's stories: under way (steps done, the current with its objectives and marker, later ones), and done.
    // Rebuilt only when the book changes.
    auto& cached = journalViews_[viewer];
    if (cached.first == storylines_.version() && !cached.second.isNull())
        return cached.second;
    auto list = Value::array();
    std::vector<const storylines::Storyline*> mine = storylines_.of(viewer);
    std::stable_sort(mine.begin(), mine.end(), [](const auto* a, const auto* b) { return a->lastActivity > b->lastActivity; });
    int ended = 0;
    for (const auto* s : mine)
    {
        const bool open = s->state == "running" || s->state == "paused" || s->state == "draft";
        if (!open && ++ended > 10)
            continue;
        auto o = Value::object();
        o.add("id", s->id);
        o.add("kind", s->kind);
        o.add("title", storyWords(viewer, s->title));
        o.add("state", s->state);
        o.add("source", s->kind == "tale" ? "a story by " + (s->authorCharacter == viewer ? std::string("you") : labelFor(viewer, s->authorCharacter))
                        : s->source == "tie" ? std::string("a tie") : s->source == "trouble" ? std::string("a resident's trouble")
                        : s->source == "contract" ? std::string("work well done") : s->source == "storykeeper" ? std::string("the world's story")
                                                                                                    : std::string("the Dungeon Master"));
        if (!s->premise.empty())
            o.add("premise", s->premise);
        o.add("tracked", s->tracking.count(viewer) > 0);
        o.add("invited", s->invited.count(viewer) > 0);
        o.add("asked", s->asked.count(viewer) > 0);
        o.add("mine", s->authorCharacter == viewer);
        const auto at = s->current();
        auto steps = Value::array();
        for (std::size_t i = 0; i < s->steps.size(); ++i)
        {
            const auto& st = s->steps[i];
            auto j = Value::object();
            j.add("title", storyWords(viewer, st.title));
            j.add("state", st.done() ? "done" : i == at ? "current" : "later");
            if (i == at && open)
            {
                j.add("text", storyWords(viewer, st.text));
                j.add("distinct", st.distinct);
                if (st.marker.set())
                    j.add("marker", storyWords(viewer, st.marker.label));
                auto objectives = Value::array();
                for (const auto& ob : st.objectives)
                {
                    auto k = Value::object();
                    k.add("kind", ob.kind);
                    k.add("line", storyWords(viewer, ob.line));
                    k.add("done", ob.done());
                    if (!ob.suits.empty())
                        k.add("suits", ob.suits);
                    if (!ob.doneBy.empty())
                        k.add("by", ob.doneBy == viewer ? std::string("you") : labelFor(viewer, ob.doneBy));
                    if (!ob.takenBy.empty())
                        k.add("taken", ob.takenBy == viewer ? std::string("you") : labelFor(viewer, ob.takenBy));
                    objectives.push(k);
                }
                j.add("objectives", objectives);
            }
            steps.push(j);
        }
        o.add("steps", steps);
        if (s->kind == "tale")
        {
            auto who = Value::array();
            for (const auto& [id, p] : s->participants)
                if (p.active())
                {
                    const auto* e = world_.entity(id);
                    auto w = Value::object();
                    w.add("name", id == viewer ? std::string("you") : labelFor(viewer, id));
                    w.add("where", !clientOf(id) ? "offline" : e && world_.entity(viewer) && e->cellId == world_.entity(viewer)->cellId ? "here" : "away");
                    who.push(w);
                }
            o.add("participants", who);
        }
        list.push(o);
    }
    cached = {storylines_.version(), list};
    return list;
}

Value Game::trackedView(const std::string& viewer) const
{
    // The tracked storyline's next marker, for the minimap and the World Map.
    for (const auto* s : storylines_.of(viewer))
        if (s->tracking.count(viewer) && s->live())
        {
            const auto at = s->current();
            if (at >= s->steps.size() || !s->steps[at].marker.set())
                return {};
            const auto& m = s->steps[at].marker;
            auto o = Value::object();
            o.add("storyline", s->id);
            o.add("title", storyWords(viewer, s->steps[at].title));
            o.add("label", storyWords(viewer, m.label));
            o.add("cell", m.cell);
            o.add("x", m.x);
            o.add("y", m.y);
            if (const auto* c = world_.cell(m.cell))
                o.add("wx", c->worldX + m.x), o.add("wy", c->worldY + m.y);
            return o;
        }
    return {};
}

bool Game::storylineCommand(Connection* c, const Value& j, Result& result)
{
    const auto id = c ? c->entityId : std::string();
    const auto verb = j.string("verb");
    auto* s = storylines_.find(j.string("storyline"));
    if (!s || !s->participants.count(id))
    {
        result = {false, "That story isn't in your journal.", {}};
        return true;
    }
    if (verb == "track")
    {
        const bool on = !s->tracking.count(id);
        for (const auto& [sid, other] : storylines_.all())
            if (auto* o = storylines_.find(sid); o && o->tracking.count(id))
                o->tracking.erase(id);
        if (on)
            s->tracking.insert(id);
        storylines_.touch();
        result = {true, on ? "Tracking it: its next place shows on your map." : "No longer tracked.", {}};
    }
    else if (verb == "take")
    {
        const bool ok = storylines_.take(s->id, std::size_t(j.number("step")), std::size_t(j.number("objective")), id);
        result = {ok, ok ? "You're on it: the others see so." : "That part can't be taken now.", {}};
    }
    else if (verb == "abandon" || verb == "leave")
    {
        auto& me = s->participants[id];
        me.left = now();
        s->tracking.erase(id);
        const bool anyone = std::any_of(s->participants.begin(), s->participants.end(), [](const auto& p) { return p.second.active(); });
        if (!anyone && s->kind != "tale")
            storylines_.end(s->id, "abandoned", now());
        storylines_.touch();
        logEvent(s->kind == "tale" ? "tale left" : "storyline abandoned", id, s->id, s->title.substr(0, 120));
        result = {true, s->kind == "tale" ? "You leave the story." : "You put it aside.", {}};
    }
    else
        result = {false, "There's nothing like that to do with it.", {}};
    saveSoon();
    return true;
}

std::string Game::storylineBrief(const std::string& npc, const std::string& wolf) const
{
    // A step's brief, while the step is current and names this resident in a talk objective (doc 58, 9).
    for (const auto* s : storylines_.of(wolf))
    {
        if (!s->live() || !s->takesPart(wolf))
            continue;
        const auto at = s->current();
        if (at >= s->steps.size() || s->steps[at].brief.empty())
            continue;
        for (const auto& o : s->steps[at].objectives)
            if (o.kind == "talk" && o.target == npc && !o.done())
                return " " + s->steps[at].brief;
    }
    return {};
}

void Game::storylinesSave(Value& root) const
{
    root.add("storylines", storylines_.save());
}

void Game::storylinesLoad(const Value& saved)
{
    storylines_.load(saved.find("storylines") ? saved["storylines"] : Value::array());
    journalViews_.clear();
}

void Game::storylinesFromEvent(const WorldEvent& e)
{
    // What the world records that a storyline may wait on: a contract done (by its kind), a kill, a fight won, a Gift
    // used, an item given.
    const auto* actor = world_.entity(e.actor);
    const bool player = actor && !actor->npc;
    if (e.kind == "contract done" && player)
        storylineEvent({"contract", e.actor, e.cell, e.detail.substr(0, e.detail.find(':')), {}, 0, 0, {}});
    else if (e.kind == "hunted" && player)
        storylineEvent({"hunt", e.actor, e.cell, e.target, {}, 0, 0, {}});
    else if (e.kind == "gift used" && player)
        storylineEvent({"gift", e.actor, e.cell, e.item, {}, 0, 0, {}});
    else if ((e.kind == "gift" || e.kind == "gift again") && player && !e.item.empty())
        storylineEvent({"deliver", e.actor, e.cell, items::baseOf(items::unmarked(e.item)), e.target, 0, 0, {}});
    else if (e.kind == "fight ends" && storylines_.waiting("fight"))
        for (const auto& b : world_.battles())
            if (b.id == e.detail && !b.hunt)
            {
                // The side still standing won.
                std::set<int> standing;
                for (const auto& f : b.fighters)
                    if (f.status == "fighting")
                        standing.insert(f.side);
                if (standing.size() != 1)
                    break;
                for (const auto& f : b.fighters)
                    if (f.side == *standing.begin())
                        if (const auto* w = world_.entity(f.id); w && !w->npc)
                            storylineEvent({"fight", f.id, b.cellId, {}, {}, 0, 0, {}});
                break;
            }
}

// ------------------------------------------------------------------ Phase 2: where personal storylines come from

void Game::tieStoryline(const newcomers::Tie& t)
{
    // A tie's storyline (doc 52's starters): with a resident, find them, see the town, talk it over; with a mentor, in
    // both journals, a scene together, then two ways round the town (one asks the innkeeper, the other looks over the
    // market), then another scene.
    auto cast = Value::object();
    cast.add("tie", t.other);
    if (const auto* sq = world_.marketSpot(t.town))
        cast.add("square", sq->cell);
    std::string keeper;
    for (const auto& p : world_.society().positions())
        if (p.role == "merchant" && world_.communityOf(p.work.cell) == t.town)
            if (const auto held = world_.society().state().careers.positions.find(p.id);
                held != world_.society().state().careers.positions.end() && !held->second.holder.empty())
            {
                const bool inn = p.title.find("inn") != std::string::npos;
                if (keeper.empty() || inn)
                    keeper = held->second.holder;
                if (inn)
                    break;
            }
    if (!keeper.empty())
        cast.add("keeper", keeper);
    const auto id = "tie-" + t.starter + (t.resident ? "" : "-mentor");
    giveStoryline(t.newcomer, storylines::findTemplate(id) ? id : "tie-cart" + std::string(t.resident ? "" : "-mentor"), cast, "tie", t.id,
                  t.resident ? std::vector<std::string>{} : std::vector<std::string>{t.other});
}

void Game::endStorylinesOf(const std::string& source, const std::string& ref, const std::string& state, const std::string& who)
{
    for (const auto& [id, s] : storylines_.all())
        if (s.source == source && s.sourceRef == ref && s.live() && (who.empty() || s.takesPart(who)))
        {
            const auto title = s.title;
            const std::vector<std::string> told = [&] {
                std::vector<std::string> out;
                for (const auto& [w, p] : s.participants)
                    if (p.active())
                        out.push_back(w);
                return out;
            }();
            storylines_.end(id, state, now());
            for (const auto& w : told)
                if (auto* c = clientOf(w))
                    storylineToast(c, "Story ended: " + storyWords(w, title) + (source == "trouble" ? " (someone else saw to it, or it passed)." : "."));
        }
}

void Game::offerChain(const Contract& done)
{
    // Work well done for a resident who likes the wolf (20+): more work, posted from the resident's own purse through
    // the ordinary contract posting, offered to that wolf first; only if it can pay, one at a time.
    const auto* poster = world_.entity(done.poster);
    const auto* taker = world_.entity(done.taker);
    const auto& society = world_.society();
    if (!poster || !poster->npc || poster->dead || !taker || taker->npc || !society.resident(done.poster))
        return;
    const auto* bond = world_.bonds().find(done.poster, done.taker);
    if (!bond || bond->affinity < 20)
        return;
    for (const auto& [id, s] : storylines_.all())
        if (s.source == "contract" && s.live() && (s.takesPart(done.taker) || s.sourceRef == done.poster))
            return;
    // Someone else of its town to carry for.
    const auto town = world_.communityOf(poster->cellId);
    std::string to;
    for (const auto& [id, life] : society.state().residents)
        if (id != done.poster && world_.communityOf(life.homeCell) == town)
            if (const auto* e = world_.entity(id); e && !e->dead && (to.empty() || std::hash<std::string>{}(id + done.id) < std::hash<std::string>{}(to + done.id)))
                to = id;
    const std::int64_t reward = 4;
    const auto* purse = society.account(done.poster);
    if (to.empty() || !purse || purse->cash < reward + 20)
        return;                                     // (It keeps its own food money.)
    auto& k = world_.postContract("courier", done.poster, town, to, reward, 7, "a parcel for " + nameOf(to));
    k.offeredTo = done.taker;
    k.offeredUntil = world_.calendarDays() + 1;
    auto cast = Value::object();
    cast.add("poster", done.poster);
    const auto given = giveStoryline(done.taker, "chain", cast, "contract", done.poster);
    if (auto* s = given.ok ? storylines_.find(given.targetId) : nullptr)
    {
        for (auto& step : s->steps)
            for (auto& o : step.objectives)
                if (o.kind == "contract")
                    o.target = k.id;                // (That job, not any.)
        storylines_.touch();
    }
    else
        world_.withdrawContract(k.id);              // (No room in its journal: the work goes back.)
}
} // namespace ratw::game
