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
            logEvent("storyline objective", p.actor, s->id, json::utf8Prefix(o.line, 120) + (p.byHand ? " (by hand)" : ""));
        }
        else if (p.kind == "step")
        {
            const auto& step = s->steps[p.step];
            logEvent("storyline step", p.actor, s->id, json::utf8Prefix(step.title, 120));
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
    logEvent("storyline done", s.owner.empty() ? s.authorCharacter : s.owner, s.id, json::utf8Prefix(s.title, 120));
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
        if (s->source == "storykeeper" && !s->takesPart(viewer))
            continue;
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
            if (s->source == "storykeeper")
            {
                j.add("text", storyWords(viewer, st.text));
                auto missions = Value::array();
                for (const auto& mission : st.missions.items())
                {
                    const auto mode = mission.string("mode");
                    const auto& track = mission.object("tracks")[mode == "individual" ? viewer : "group"].items();
                    const auto& objectives = mission.array("objectives");
                    if (objectives.empty())
                        continue;
                    const auto index = track.size() == 2 ? std::size_t(std::clamp(track[0].asNumber(), 0., double(objectives.size()))) : 0;
                    const auto& team = mission.array("contributors");
                    auto entry = Value::object();
                    entry.add("title", mission.string("title"));
                    entry.add("mode", mode);
                    entry.add("done", index == objectives.size());
                    entry.add("optional", mission.boolean("optional"));
                    entry.add("completed", mission.number("completed"));
                    entry.add("required", mission.number("required", 1));
                    entry.add("credited", mode == "individual" ? index == objectives.size() :
                        mission.boolean("ready") && std::any_of(team.begin(), team.end(), [&](const auto& id) { return id.asString() == viewer; }));
                    entry.add("contributors", int(team.size()));
                    entry.add("requiredContributors", mission.number("requiredContributors", 1));
                    if (index < objectives.size())
                    {
                        entry.add("instruction", objectives[index].string("label"));
                        entry.add("amount", track.size() == 2 ? track[1].asNumber() : 0.);
                        entry.add("goal", objectives[index].number("goal", 1));
                    }
                    missions.push(std::move(entry));
                }
                if (missions.size()) j.add("missions", std::move(missions));
            }
            if (i == at && open)
            {
                j.set("text", storyWords(viewer, st.text));
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
        if (s->source == "storykeeper")
            if (const auto f = s->questViews.find(viewer); f != s->questViews.end())
            {
                // Doc 85, Phase 6: the quests the runner made for this wolf, as they may see them; the giver by name as the
                // wolf knows them.
                auto quests = Value::array();
                for (auto q : f->second.array("quests"))
                {
                    if (const auto giver = q.string("giver"); !giver.empty())
                        q.set("giverName", names::capitalised(labelFor(viewer, giver)));
                    q.set("title", storyWords(viewer, q.string("title")));
                    quests.push(std::move(q));
                }
                if (quests.size()) o.add("quests", std::move(quests));
                if (f->second.array("trackers").size()) o.add("trackers", f->second["trackers"]);
            }
        if (s->source == "storykeeper")
        {
            // Doc 85, Phase 9: the story's notes in this wolf's journal, and its markers on their maps.
            const auto extras = storyJournalExtras(*s, viewer);
            for (const auto& [key, value] : extras.fields())
                o.add(key, value);
        }
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
    else if (verb == "quest_accept" || verb == "quest_decline" || verb == "quest_abandon")
    {
        // A world story's quest from the journal (doc 85, Phase 6): accepted or declined when it was offered with no giver,
        // abandoned while under way. The runner reads the event and keeps the ledger.
        const auto quest = j.string("quest");
        const auto* view = s->source == "storykeeper" && s->live() ? s->questView(id, quest) : nullptr;
        const auto status = view ? view->string("status") : std::string();
        if (!view)
            result = {false, "That quest isn't in your journal.", {}};
        else if (verb != "quest_abandon" && !view->boolean("accept"))
            result = {false, "That quest is taken up with whoever gives it.", {}};
        else if (verb == "quest_abandon" && (!view->boolean("abandon") || (status != "active" && status != "ready")))
            result = {false, "That quest can't be put aside.", {}};
        else
        {
            storyQuestEvent(verb == "quest_accept" ? "quest accepted" : verb == "quest_decline" ? "quest declined" : "quest abandoned", id,
                            s->id, "", quest);
            result = {true, verb == "quest_accept" ? "You take on: " + view->string("title") + "."
                            : verb == "quest_decline" ? std::string("You let it pass, for now.") : "You put aside: " + view->string("title") + ".",
                      {}};
        }
    }
    else if (verb == "abandon" || verb == "leave")
    {
        auto& me = s->participants[id];
        me.left = now();
        s->tracking.erase(id);
        const bool anyone = std::any_of(s->participants.begin(), s->participants.end(), [](const auto& p) { return p.second.active(); });
        if (!anyone && s->kind != "tale" && s->source != "storykeeper")
            storylines_.end(s->id, "abandoned", now());
        storylines_.touch();
        logEvent(s->kind == "tale" ? "tale left" : "storyline abandoned", id, s->id, json::utf8Prefix(s->title, 120));
        result = {true, s->kind == "tale" ? "You leave the story." : "You put it aside.", {}};
    }
    else
        result = {false, "There's nothing like that to do with it.", {}};
    saveSoon();
    return true;
}

// Scene orders belong to one approved run and journal visit. Accepting an order and reaching
// its cue are distinct: arrival is observed from the resident in the native simulation.
Result Game::directSceneResident(const std::string& kind, const std::string& npc, const Value& p)
{
    const auto run = p.string("run"), cue = p.string("cue");
    auto* story = storylines_.find(run);
    auto* actor = world_.entity(npc);
    if (!story || !story->live() || story->source != "storykeeper" || p.number("visit", -1) != std::strtod(story->sourceRef.c_str(), nullptr))
        return {false, "The requested scene visit is no longer open.", npc};
    if (npc.empty() || npc.size() > 80 || (actor && !actor->npc))
        return {false, "A scene order must name a resident.", npc};
    if (cue.empty() || cue.size() > 80 || cue.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.:-") != std::string::npos)
        return {false, "Give the scene cue an identifier.", npc};
    for (const auto& other : storylines_.activeScenes())
        if (other != run && storylines_.find(other)->sceneOrders.count(npc))
            return {false, "Another story is directing this resident.", npc};
    if (heldByAnotherStory(npc, run))               // (Doc 85, Phase 9: npc.hold keeps a resident for its story.)
        return {false, "Another story holds this resident.", npc};
    const auto prior = story->sceneOrders.find(npc);
    if (prior != story->sceneOrders.end() && !prior->second.done)
        return {false, "Wait for this resident's pending scene cue.", npc};
    storylines::SceneOrder order;
    order.kind = kind;
    order.cue = cue;
    const auto held = sceneActors_.find(npc);
    order.previousPace = prior != story->sceneOrders.end() ? prior->second.previousPace
                         : held != sceneActors_.end() ? held->second.previousPace : actor ? actor->pace : 0;
    if (kind == "npc.walk")
    {
        order.cell = p.string("cell");
        const double x = p.number("x", -1), y = p.number("y", -1);
        const auto pace = p.string("pace");
        if (x < 0 || y < 0 || x > 4095 || y > 4095 || std::floor(x) != x || std::floor(y) != y ||
            (pace != "walk" && pace != "run") || !world_.ensureLoaded(order.cell).ok)
            return {false, "Choose a valid destination tile and pace.", npc};
        order.x = int(x); order.y = int(y); order.pace = pace == "run" ? 10 : 0;
        const auto* tile = world_.cell(order.cell)->tile(order.x, order.y);
        if (!tile || tile->solid)
            return {false, "Choose a walkable destination tile.", npc};
    }
    else if (kind == "npc.pose")
    {
        order.posture = p.string("posture");
        if (order.posture != "standing" && order.posture != "sitting" && order.posture != "lying")
            return {false, "Choose a scene posture.", npc};
    }
    else
    {
        order.words = p.string("text");
        if (order.words.empty() || json::utf8Prefix(order.words, 200) != order.words ||
            order.words.find_first_of("\r\n") != std::string::npos)
            return {false, "Supply a single spoken line of up to 200 characters.", npc};
    }
    if (actor && world_.society().resident(npc) && !actor->dead && !world_.inBattle(npc) && actor->downedLeft <= 0 && actor->leaderId.empty())
    {
        world_.sceneHold(npc, true, kind == "npc.walk" ? order.pace : order.previousPace);
        sceneActors_[npc] = {order.previousPace, world_.time(), actor->cellId, actor->position};
        if (kind == "npc.walk") actor->activity = order.pace ? "running on a story errand" : "walking on a story errand";
    }
    story->sceneOrders[npc] = std::move(order);
    storylines_.touch();
    record(StoryBook);
    return {true, "Scene direction accepted.", npc};
}

void Game::releaseSceneResidents(const storylines::Storyline& story)
{
    for (const auto& [npc, order] : story.sceneOrders)
    {
        world_.sceneHold(npc, false, order.previousPace);
        sceneActors_.erase(npc);
    }
}

void Game::tickStoryScenes(double dt)
{
    const auto& runs = storylines_.activeScenes();
    if (runs.empty() && sceneActors_.empty()) return;
    nextSceneCheck_ -= dt;
    if (nextSceneCheck_ > 0) return;
    nextSceneCheck_ = .25;
    std::set<std::string> held;
    bool changed = false;
    for (const auto& run : runs)
    {
        auto* story = storylines_.find(run);
        std::vector<std::string> blocked;
        for (auto& [npc, order] : story->sceneOrders)
        {
            held.insert(npc);
            auto* e = world_.entity(npc);
            auto& actor = sceneActors_[npc];
            const bool available = e && e->npc && !e->dead && world_.society().resident(npc) &&
                                   !world_.inBattle(npc) && e->downedLeft <= 0 && e->leaderId.empty();
            if (available && !e->sceneDirected)
            {
                world_.sceneHold(npc, true, order.kind == "npc.walk" && !order.done ? order.pace : order.previousPace);
                actor = {order.previousPace, world_.time(), e->cellId, e->position};
            }
            if (order.done) continue;
            const char* event = nullptr;
            if (!available)
                event = "scene blocked";
            else if (order.kind == "npc.walk")
            {
                if (e->cellId != actor.cell || std::hypot(e->position.x - actor.position.x, e->position.y - actor.position.y) >= .35)
                {
                    actor.position = e->position; actor.cell = e->cellId; actor.progressAt = world_.time();
                }
                if (e->cellId == order.cell && std::floor(e->position.x) == order.x && std::floor(e->position.y) == order.y)
                    event = "scene arrived";
                else if (!world_.canWalkBetween(e->cellId, order.cell) || world_.time() - actor.progressAt >= 180)
                    event = "scene blocked";
                else
                    world_.sceneWalk(*e, order.cell, {order.x + .5, order.y + .5});
            }
            else if (order.kind == "npc.pose")
            {
                if (e->posture == order.posture) event = "scene posed";
                else world_.setPosture(npc, order.posture);
            }
            else if (order.kind == "npc.speak")
            {
                ParsedPost post;
                post.ok = post.speech = true;
                post.segments.push_back({"speech", order.words});
                publish(npc, post, Voice::Speak);
                npcLastSpeech_[npc] = world_.time();
                event = "scene spoken";
            }
            if (!event) continue;
            if (std::string_view(event) == "scene blocked")
            {
                blocked.push_back(npc);
                held.erase(npc);
                if (!e || !e->sceneDirected) sceneActors_.erase(npc);
            }
            else world_.sceneHold(npc, true, order.previousPace);
            WorldEvent fact{event, npc, run, e ? e->cellId : std::string{}, 0, 0, {}, std::stoi(story->sourceRef), 0, order.cue};
            if (e && std::string_view(event) != "scene blocked")
            {
                // A directed speaker or seated resident holds this tile even without observers.
                // Offstage travel also finishes at the specific tile before it can emit arrival.
                fact.x = int(std::floor(e->position.x));
                fact.y = int(std::floor(e->position.y));
                if (order.kind == "npc.walk") e->activity = "waiting at the destination";
            }
            world_.recordEvent(std::move(fact));
            order.done = true;
            changed = true;
        }
        for (const auto& npc : blocked) story->sceneOrders.erase(npc);
    }
    for (auto it = sceneActors_.begin(); it != sceneActors_.end();)
    {
        if (held.count(it->first)) { ++it; continue; }
        world_.sceneHold(it->first, false, it->second.previousPace);
        it = sceneActors_.erase(it);
    }
    if (changed)
    {
        // The cue and post-arrival position leave with the checkpoint that marks the direction
        // complete. A storybook-only journal record could retire the order before its cue survived.
        // (An early save on the store's worker, never one waited for here: doc 85, Phase 7.)
        storylines_.touch();
        ++revision_;
        saveSoon(StoryOutcomeSeconds);
    }
}

// Doc 85, Phase 7: a story's effects in one order, story.effects {run, visit, items: [{kind, target, payload, ifRefused}]},
// each item applied as its own order would be (the community world's guards, the economy's journal) and answered with a
// result of its own: the order's result is a JSON array of {ok, text}, one for each item, in order. An item refused whose
// ifRefused is "pause" (or absent) stops the batch there: the items after it are not applied ({ok: false, held: true}),
// and the runner sends them again when the run is retried or the refused one skipped. The batch's receipt (dmActs) holds
// what each item that journalled something came to, as each is applied, so a crash part-way through answers those from
// it when the row is taken again and applies only the rest: none twice, none lost. `seq` is the last journal record the
// batch wrote (0 for none): the row is told "applied" once it is committed.
Result Game::applyStoryEffects(const std::vector<PgValue>& row, std::uint64_t& seq)
{
    seq = 0;
    const std::string id = row[0] ? *row[0] : std::string(), target = row[2] ? *row[2] : std::string();
    json::Value payload;
    std::string problem;
    if (row.size() < 5 || !row[4] || !json::parse(*row[4], payload, problem) || !payload.isObject() || !payload["items"].isArray())
        return {false, "Invalid story effects.", target};
    const auto& items = payload.array("items");
    if (items.empty() || items.size() > StoryEffectsMax)
        return {false, "A story's batch holds 1 to " + std::to_string(StoryEffectsMax) + " orders.", target};
    json::Value entry = json::Value::object();
    if (const auto* kept = dmActs_.find(id); kept && kept->isObject() && kept->boolean("batch"))
        entry = *kept;                              // (Taken again after a crash: what each item came to, so far.)
    entry.set("batch", true);
    entry.set("kind", "story.effects");
    entry.set("target", target);
    if (!entry["items"].isObject())
        entry.set("items", json::Value::object());
    const auto seqBefore = journalSeq_;
    auto results = json::Value::array();
    bool stopped = false, any = false;
    std::size_t applied = 0, refused = 0;
    for (std::size_t n = 0; n < items.size(); ++n)
    {
        const auto& item = items[n];
        const std::string key = std::to_string(n), flat = id + "." + key;
        auto r = json::Value::object();
        if (stopped)
        {
            r.add("ok", false);
            r.add("held", true);
            r.add("text", "Not applied: an order before it was refused.");
            results.push(std::move(r));
            continue;
        }
        Result outcome{false, "", target};
        const json::Value* done = entry["items"].find(key);
        if (!done)
            done = dmActs_.find(flat);              // (An economy act's own receipt: applyEconomyCommand's.)
        const std::string kind = item.string("kind"), to = item.string("target");
        if (done && done->isObject())
            outcome = {done->boolean("ok"), done->string("text"), to};
        else if (!item.isObject() || kind.empty() || kind == "story.effects" || kind == "economy.sync" || kind == "story.phase" ||
                 kind == "story.stop" || kind == "story.finish" || (item.has("payload") && !item["payload"].isObject()))
            outcome = {false, "Not an order a story's batch may hold.", to};
        else
        {
            auto act = json::Value::object();       // (What record() writes while the item is applied: applyDmActions.)
            act.add("ok", true);
            act.add("text", "Applied (the server stopped before it could say what it did).");
            act.add("kind", kind);
            act.add("target", to);
            entry.find("items")->set(key, std::move(act));
            dmActs_.set(id, entry);
            actingFor_ = id;
            const auto before = journalSeq_;
            const json::Value orderPayload = item.has("payload") ? item["payload"] : json::Value::object();
            if (economyCommand(kind))
                outcome = applyEconomyCommand(kind, to, orderPayload, flat);
            else
                outcome = applyDmAct({row[0], kind, to, row.size() > 3 ? row[3] : PgValue(), json::dump(orderPayload)});
            actingFor_.clear();
            dmActs_.erase(flat);
            if (journalSeq_ != before)
            {
                auto kept = json::Value::object();
                kept.add("ok", outcome.ok);
                kept.add("text", outcome.message);
                kept.add("kind", kind);
                kept.add("target", to);
                entry.find("items")->set(key, std::move(kept));
            }
            else
                entry.find("items")->erase(key);   // (Nothing durable: after a crash it is applied again, as an order would be.)
            dmActs_.set(id, entry);
        }
        r.add("ok", outcome.ok);
        r.add("text", outcome.message);
        results.push(std::move(r));
        any |= outcome.ok;
        outcome.ok ? ++applied : ++refused;
        if (!outcome.ok && item.string("ifRefused", "pause") == "pause")
            stopped = true;
    }
    dmActs_.erase(id);
    if (journalSeq_ != seqBefore || entry["items"].size())
    {
        keepDmAct(id, std::move(entry));            // (For the checkpoint: the row is never applied again after a crash.)
        seq = journalSeq_ != seqBefore ? journalSeq_ : 0;
    }
    note("info", "RATW_STORY_EFFECTS " + target + " #" + id + ": " + std::to_string(applied) + " applied, " + std::to_string(refused) +
                     " refused" + (stopped ? ", the rest held" : ""));
    return {any || items.empty(), json::dump(results), target};
}

// The DM runner owns transitions. Only reached phases and their audience enter the game checkpoint.
Result Game::applyWorldStoryPhase(const std::string& run, const Value& p)
{
    const double visit = p.number("visit", 0);
    if (run.rfind("world-", 0) != 0 || run.size() > 80 || visit < 1 || visit > 1000000000 || visit != int(visit) ||
        p.string("title").empty() || p.string("phase").empty() || p.string("journal").empty() ||
        p.array("players").size() > 5000 || p.array("lines").size() > 12 || p.array("guestVoices").size() > 8)
        return {false, "Invalid world story phase or audience.", run};
    auto* prior = storylines_.find(run);
    if (prior && (prior->kind != "world" || prior->source != "storykeeper"))
        return {false, "Story ID belongs to another story.", run};
    const auto revision = std::to_string(int(visit));
    // A revision (doc 85, Phase 5b): a running story moved onto a newly approved version of itself keeps the visit it is
    // in. The game takes the new version's title, journal, regions, speakers and missions for that visit; nothing is
    // replayed, nobody is toasted, scenes under way go on. Refused unless it is the visit the live story is in, and,
    // like a phase, checked whole before anything changes (the runner then puts the run back on its old version).
    const bool revise = p.boolean("revise");
    if (revise)
    {
        if (!prior || !prior->live() || prior->sourceRef != revision || prior->steps.empty() || p.boolean("end"))
            return {false, "Only the visit a live world story is in can be revised.", run};
    }
    else if (prior && prior->sourceRef == revision)
        return {true, "Phase already received.", run};
    if (!revise && ((!prior && visit != 1) || (prior && prior->sourceRef != std::to_string(int(visit) - 1))))
        return {false, "World story phases arrived out of order.", run};
    storylines::Storyline story = prior ? *prior : storylines::Storyline{};
    // (Doc 86, N2: a storykeeper story's program, kept as its template: the window's player.quest.<program>/<quest>.)
    if (const auto program = p.string("program"); !program.empty() && program.size() <= 80)
        story.templateId = program;
    if (!storylines::readRegions(p["regions"], story.regions))
        return {false, "Invalid world story region.", run};
    // What the story asks the game to tell it (doc 85, Phase 8): the world's facts and the watched events it names.
    if (const auto* watch = p.find("watch"); watch && !watch->isNull())
    {
        if (!watch->isObject() || json::dump(*watch).size() > 64 * 1024)
            return {false, "Invalid world story watch list.", run};
        story.watch = *watch;
    }
    else
        story.watch = Value();
    for (const auto& [id, r] : story.regions)
    {
        const auto* cell = world_.cell(r.cell);
        if (!cell || r.x2 >= cell->width || r.y2 >= cell->height)
            return {false, "Story region does not fit its live cell: " + id, run};
    }
    if (revise)
    {
        story.title = p.string("title");
        story.admitted.clear();
        for (const auto& voice : p.array("guestVoices"))
        {
            if (!voice.isString() || !world_.society().resident(voice.asString()))
                return {false, "A guest speaker is missing.", run};
            story.admitted.insert(voice.asString());
        }
        story.cast.clear();
        for (const auto& line : p.array("lines"))
        {
            if (!world_.society().resident(line.string("npc")) || line.string("text").empty())
                return {false, "A speaking resident is missing.", run};
            story.cast.push_back({line.string("npc"), line.string("text")});
        }
        auto& step = story.steps.back();
        step.title = p.string("phase");
        step.text = p.string("journal");
        step.missions = p["missions"];
        if (!step.objectives.empty()) step.objectives.front().line = step.text;
        story.lastActivity = now();
        for (const auto* client : clients_) observeStoryTile(client->entityId);
        const auto& kept = storylines_.add(std::move(story));
        if (!kept.regions.empty()) storiesWatchingRegions_.insert(run);
        else storiesWatchingRegions_.erase(run);
        for (const auto* client : clients_) observeStoryTile(client->entityId);
        refreshStoryWatch();
        recountStoryRegions(run);
        record(StoryBook);
        return {true, "World story revised.", run};
    }
    story.id = run;
    story.kind = "world";
    story.source = "storykeeper";
    story.sourceRef = revision;
    story.title = p.string("title");
    if (!prior)
    {
        story.began = now();
        for (const auto& who : p.array("players"))
            if (who.isString()) story.participants[who.asString()] = {now(), -1};
    }
    story.sceneOrders.clear();
    story.cinematics.clear();
    story.admitted.clear();
    for (const auto& voice : p.array("guestVoices"))
    {
        if (!voice.isString() || !world_.society().resident(voice.asString()))
            return {false, "A guest speaker is missing.", run};
        story.admitted.insert(voice.asString());
    }
    story.cast.clear();
    for (const auto& line : p.array("lines"))
    {
        if (!world_.society().resident(line.string("npc")) || line.string("text").empty())
            return {false, "A speaking resident is missing.", run};
        story.cast.push_back({line.string("npc"), line.string("text")});
    }
    if (!story.steps.empty() && !story.steps.back().objectives.empty())
        story.steps.back().objectives.front().doneAt = now();
    if (!story.steps.empty() && !p.array("previousMissions").empty())
        story.steps.back().missions = p["previousMissions"];
    storylines::Step step;
    step.missions = p["missions"];
    step.title = p.string("phase");
    step.text = p.string("journal");
    storylines::Objective wait;
    wait.kind = "world phase";
    wait.line = step.text;
    if (p.boolean("end")) wait.doneAt = now();
    step.objectives.push_back(std::move(wait));
    if (story.steps.size() >= 32) story.steps.erase(story.steps.begin());
    story.steps.push_back(std::move(step));
    story.state = p.boolean("end") ? "done" : "running";
    story.lastActivity = now();
    if (p.boolean("end")) story.ended = now();
    if (prior && prior->live()) releaseSceneResidents(*prior);
    if (p.boolean("end")) endStoryWorld(story);    // (Doc 85, Phase 9: an ending undoes what its world effects left.)
    // Sample pending movement against the previous watches before changing this run.
    for (const auto* client : clients_) observeStoryTile(client->entityId);
    const auto& kept = storylines_.add(std::move(story));
    if (kept.live() && !kept.regions.empty()) storiesWatchingRegions_.insert(run);
    else storiesWatchingRegions_.erase(run);
    for (const auto* client : clients_) observeStoryTile(client->entityId);
    refreshStoryWatch();
    recountStoryRegions(run);
    for (const auto& [who, member] : kept.participants)
        if (member.active()) if (auto* client = clientOf(who)) storylineToast(client, kept.title + ": " + p.string("journal"));
    logEvent("story phase", run, p.string("phase"), kept.title);
    record(StoryBook);
    return {true, "World story phase recorded.", run};
}

std::string Game::worldStoryLine(const std::string& npc, const std::string& wolf) const
{
    std::string lines;
    for (const auto* story : storylines_.of(wolf))
        if (story->kind == "world" && story->live() && story->takesPart(wolf))
            for (const auto& part : story->cast)
                if (part.name == npc)
                {
                    if (!lines.empty()) lines += " ";
                    lines += part.looks;
                }
    return lines;
}

std::string Game::storylineBrief(const std::string& npc, const std::string& wolf) const
{
    const auto directed = worldStoryLine(npc, wolf);
    if (!directed.empty()) return " " + directed;
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
    storiesWatchingRegions_.clear();
    lastStoryTile_.clear();
    for (const auto& [id, s] : storylines_.all())
        if (s.source == "storykeeper" && s.live() && !s.regions.empty()) storiesWatchingRegions_.insert(id);
    journalViews_.clear();
    storyOccupants_.clear();
    refreshStoryWatch();
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
    if (!world_.npcCourierRoom())
        return;                                     // (Staged: the day's NPC courier contracts are all posted, doc 79, 9.3.)
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

// ------------------------------------------------------------------ A world story's quests (doc 85, Phase 6)
// The runner keeps each quest's ledger (who holds it, how far each objective is, who may take it); the game is told what
// each wolf may see (`story.quests`), shows it in the journal and in the residents' dialogue windows, and writes what the
// wolves do with it as events the runner reads (quest accepted, declined, handed in, abandoned; dialogue choice).
namespace
{
constexpr std::size_t StoryAudienceMost = 5000, StoryOffersMost = 64, StoryChoicesMost = 64, QuestViewsMost = 64,
                      TrackersMost = 16, TalkMost = 8;

bool plainId(const Value& v)
{
    return v.isString() && !v.asString().empty() && v.asString().size() <= 80;
}

bool endedQuest(const std::string& status)
{
    return status == "done" || status == "failed" || status == "abandoned" || status == "expired";
}
} // namespace

Result Game::applyStoryCohort(storylines::Storyline& story, const Value& p)
{
    const auto& add = p.array("add");
    const auto& remove = p.array("remove");
    if (add.size() > StoryAudienceMost || remove.size() > StoryAudienceMost ||
        !std::all_of(add.begin(), add.end(), plainId) || !std::all_of(remove.begin(), remove.end(), plainId))
        return {false, "Invalid story audience change.", story.id};
    std::size_t active = 0;
    for (const auto& [id, member] : story.participants)
        active += member.active();
    for (const auto& who : add)
        if (const auto f = story.participants.find(who.asString()); f == story.participants.end() || !f->second.active())
            ++active;
    if (active > StoryAudienceMost)
        return {false, "A world story takes 5000 wolves at most.", story.id};
    for (const auto& who : add)
    {
        auto& member = story.participants[who.asString()];
        if (!member.active() || member.joined == 0)
            member = {now(), -1};
    }
    for (const auto& who : remove)
        if (const auto f = story.participants.find(who.asString()); f != story.participants.end() && f->second.active())
        {
            f->second.left = now();
            story.tracking.erase(who.asString());
            story.questViews.erase(who.asString());
        }
    storylines_.touch();
    record(StoryBook);
    return {true, "The story's wolves: " + std::to_string(add.size()) + " in, " + std::to_string(remove.size()) + " out.", story.id};
}

Result Game::applyStoryQuests(storylines::Storyline& story, const Value& p)
{
    if ((p.has("offers") && (!p["offers"].isArray() || p.array("offers").size() > StoryOffersMost)) ||
        (p.has("choices") && (!p["choices"].isArray() || p.array("choices").size() > StoryChoicesMost)) ||
        (p.has("wolves") && (!p["wolves"].isObject() || p["wolves"].size() > StoryAudienceMost)))
        return {false, "Invalid story quests.", story.id};
    for (const auto& o : p.array("offers"))
        if (!plainId(o["quest"]) || o.string("title").empty() ||
            (o.string("npc").empty() ? !plainId(o["board"]) : !world_.society().resident(o.string("npc"))))
            return {false, "A quest offer needs its quest, title and a resident who offers it, or a town's board.", story.id};
    for (const auto& c : p.array("choices"))
        if (!plainId(c["id"]) || !plainId(c["npc"]) || c.string("words").empty() || !world_.society().resident(c.string("npc")))
            return {false, "A story choice needs its name, words and a resident.", story.id};
    for (const auto& [wolf, entry] : p["wolves"].fields())
        if (wolf.empty() || wolf.size() > 80 || !entry.isObject() || entry.array("quests").size() > QuestViewsMost ||
            entry.array("trackers").size() > TrackersMost || (entry.has("talk") && (!entry["talk"].isObject() || entry["talk"].size() > TalkMost)))
            return {false, "Invalid quest journal for " + wolf + ".", story.id};
    if (p.has("offers")) story.offers = p["offers"];
    if (p.has("choices")) story.choices = p["choices"];
    if (const auto program = p.string("program"); !program.empty() && program.size() <= 80)
        story.templateId = program;                 // (Doc 86, N2: as story.phase names it, for a run older than that.)
    std::size_t wolves = 0;
    for (const auto& [wolf, entry] : p["wolves"].fields())
    {
        auto kept = Value::object();
        kept.add("quests", entry["quests"].isArray() ? entry["quests"] : Value::array());
        kept.add("trackers", entry["trackers"].isArray() ? entry["trackers"] : Value::array());
        // (Doc 86, N2: what the residents this wolf talks to know of the wolf's own values: player.story.<name>.)
        auto talk = Value::object();
        for (const auto& [name, v] : entry.object("talk").fields())
            if (!name.empty() && name.size() <= 80 && v.isString() && v.asString().size() <= 40)
                talk.add(name, v.asString());
        if (talk.size())
            kept.add("talk", std::move(talk));
        if (!kept.array("quests").size() && !kept.array("trackers").size() && !kept.object("talk").size())
            story.questViews.erase(wolf);
        else
            story.questViews[wolf] = std::move(kept);
        ++wolves;
        if (const auto toast = entry.string("toast"); !toast.empty())
            if (auto* c = clientOf(wolf))
                storylineToast(c, story.title + ": " + json::utf8Prefix(toast, 300));
    }
    storylines_.touch();
    record(StoryBook);
    return {true, "Story quests recorded (" + std::to_string(wolves) + " journals).", story.id};
}

bool Game::storyOfferOpen(const storylines::Storyline& s, const Value& offer, const std::string& wolf) const
{
    if (!s.live() || offer.boolean("full"))
        return false;
    if (offer.string("who") == "participants" && !s.takesPart(wolf))
        return false;
    if (const auto region = offer.string("region"); !region.empty())
    {
        const auto r = s.regions.find(region);
        const auto* e = world_.entity(wolf);
        if (r == s.regions.end() || !e || !r->second.contains(e->cellId, int(std::floor(e->position.x)), int(std::floor(e->position.y))))
            return false;
    }
    const auto* view = s.questView(wolf, offer.string("quest"));
    if (!view)
        return true;
    const auto status = view->string("status");
    if (status == "offered")
        return true;
    if (!endedQuest(status))
        return false;
    const auto* again = view->find("again");
    return again && again->isNumber() && again->asNumber() <= world_.calendarDays();
}

std::vector<Game::StoryChoice> Game::storyChoicesFor(const std::string& wolf, const std::string& npc) const
{
    std::vector<StoryChoice> out;
    for (const auto& [id, s] : storylines_.all())
    {
        if (s.source != "storykeeper" || !s.live())
            continue;
        for (const auto& o : s.offers.items())
            if (!npc.empty() && o.string("npc") == npc && storyOfferOpen(s, o, wolf))
                out.push_back({"offer", id, o.string("quest"), o.string("ask", "Ask about: " + o.string("title"))});
        if (const auto f = s.questViews.find(wolf); f != s.questViews.end())
            for (const auto& v : f->second.array("quests"))
                if (v.boolean("handIn") && v.string("status") == "ready" && v.string("giver") == npc)
                {
                    std::string words = "About " + v.string("title") + ": it's done.";
                    for (const auto& o : s.offers.items())
                        if (o.string("quest") == v.string("id") && !o.string("handIn").empty())
                            words = o.string("handIn");
                    out.push_back({"handin", id, v.string("id"), words});
                }
        for (const auto& c : s.choices.items())
        {
            if (c.string("npc") != npc || !s.takesPart(wolf))
                continue;
            const auto& players = c["players"];
            const auto has = [&](const Value& list) {
                return std::any_of(list.items().begin(), list.items().end(), [&](const Value& w) { return w.asString("") == wolf; });
            };
            if (players.isArray() && !has(players))
                continue;
            if (c.boolean("once") && has(c["picked"]))
                continue;
            out.push_back({"choice", id, c.string("id"), c.string("words")});
        }
    }
    return out;
}

const Value* Game::storyOffer(const std::string& run, const std::string& quest) const
{
    const auto* s = storylines_.find(run);
    if (!s)
        return nullptr;
    for (const auto& o : s->offers.items())
        if (o.string("quest") == quest)
            return &o;
    return nullptr;
}

void Game::storyQuestEvent(const char* kind, const std::string& wolf, const std::string& run, const std::string& npc,
                           const std::string& id)
{
    WorldEvent e;
    e.kind = kind;
    e.actor = wolf;
    e.target = run;
    e.item = npc;
    e.detail = id;
    world_.recordEvent(std::move(e));
    saveSoon();
}
} // namespace ratw::game
