// A world story's new world effects (Docs/Design/85-story-triggers-and-campaign-control.md, 3.5 and Phase 9). Each is a
// DM order of its own name, sent by the story runner (alone or in a story.effects batch), with the run and its visit in
// its payload; the game applies it under the rules it already keeps (a door's sides, a party's places, a resident's
// protection, a town's watch, the ground's account) and answers with a receipt. What the game must undo when the run
// ends is kept with the story (Storyline::held): a lock, a hold, a follower, a stranger, a mantle, a notice, the weather,
// a marker. A story may set the weather for a while, never the calendar or the hour.
#include "RatwGame.h"
#include "RatwItems.h"
#include "RatwNames.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace ratw::game
{
namespace
{
using json::Value;

int whole(const Value& p, const char* key, int low, int high, int otherwise = -1)
{
    const auto& v = p[key];
    if (!v.isNumber())
        return otherwise;
    const double n = v.asNumber();
    return n >= low && n <= high && std::floor(n) == n ? int(n) : otherwise;
}

bool oneLine(const std::string& text, std::size_t most)
{
    return !text.empty() && json::utf8Prefix(text, most) == text && text.find_first_of("\r\n") == std::string::npos;
}

Value& section(storylines::Storyline& s, const char* key, bool list = false)
{
    if (!s.held.isObject())
        s.held = Value::object();
    if (!s.held.has(key))
        s.held.set(key, list ? Value::array() : Value::object());
    return *s.held.find(key);
}

bool listed(const Value& list, const std::string& id)
{
    for (const auto& v : list.items())
        if (v.isString() && v.asString() == id)
            return true;
    return false;
}

void unlist(Value& list, const std::string& id)
{
    auto kept = Value::array();
    for (const auto& v : list.items())
        if (!(v.isString() && v.asString() == id))
            kept.push(v);
    list = std::move(kept);
}

constexpr std::size_t NotesKept = 12, PingsAtOnce = 16;
const char* StoryNoticeAuthor = "story";            // (Its notices' author: never counted against a board's pins.)
} // namespace

bool Game::storyWorldOrder(const std::string& kind)
{
    static const std::set<std::string> kinds{"journal.note", "story.ping",  "item.take",    "item.place",  "warrant.issue",
                                             "warrant.clear", "npc.hold",   "npc.release",  "npc.follow",  "npc.unfollow",
                                             "door.lock",    "door.unlock", "door.open",    "door.close",  "board.post",
                                             "weather.set",  "scene.stop"};
    return kinds.count(kind) > 0;
}

bool Game::heldByAnotherStory(const std::string& npc, const std::string& run) const
{
    for (const auto& id : storiesHolding_)
        if (id != run)
            if (const auto* s = storylines_.find(id); s && s->live() && s->held.isObject() && s->held["npcs"].has(npc))
                return true;
    return false;
}

Result Game::applyStoryWorld(const std::string& kind, const std::string& target, const Value& p)
{
    const auto run = p.string("run");
    auto* story = storylines_.find(run);
    if (!story || story->kind != "world" || story->source != "storykeeper" || !story->live())
        return {false, "That story is not running.", target};
    // (A line, a note or a marker belongs to the visit it was sent for: one sent for a visit that is over is refused.)
    const bool visitBound = kind == "journal.note" || kind == "story.ping" || kind == "scene.stop";
    if (visitBound && std::to_string(whole(p, "visit", 1, 1000000000)) != story->sourceRef)
        return {false, "This story visit is no longer active.", run};
    const auto changed = [&] {
        storylines_.touch();
        refreshStoryWatch();
        record(StoryBook);
    };

    // ------------------------------------------------------------------------------------- the journal and the maps
    if (kind == "journal.note")
    {
        const auto text = p.string("text"), to = p.string("player");
        if (!oneLine(text, 200))
            return {false, "A journal note is one line of up to 200 characters.", run};
        if (!to.empty() && !story->takesPart(to))
            return {false, "That wolf is not in this story.", run};
        auto& notes = section(*story, "notes", true);
        auto note = Value::object();
        note.add("text", text);
        note.add("day", world_.calendarDays());
        if (!to.empty())
            note.add("to", to);
        notes.push(std::move(note));
        while (notes.size() > NotesKept)
        {
            auto kept = Value::array();
            for (std::size_t i = 1; i < notes.size(); ++i)
                kept.push(notes.items()[i]);
            notes = std::move(kept);
        }
        for (const auto& [id, member] : story->participants)
            if (member.active() && (to.empty() || id == to))
                if (auto* c = clientOf(id))
                    storylineToast(c, story->title + ": a new line in your journal.");
        changed();
        return {true, to.empty() ? "Noted in every journal of the story." : "Noted in " + to + "'s journal.", run};
    }
    if (kind == "story.ping")
    {
        const auto cell = p.string("cell"), label = p.string("label"), to = p.string("player");
        const int x = whole(p, "x", 0, 4095), y = whole(p, "y", 0, 4095), minutes = whole(p, "minutes", 1, 240);
        if (!world_.ensureLoaded(cell).ok || !world_.cell(cell) || x < 0 || y < 0 || x >= world_.cell(cell)->width ||
            y >= world_.cell(cell)->height)
            return {false, "Mark a tile of a place in the world.", run};
        if (!oneLine(label, 40) || minutes < 1)
            return {false, "A marker has a label of up to 40 characters and lasts 1 to 240 minutes.", run};
        if (!to.empty() && !story->takesPart(to))
            return {false, "That wolf is not in this story.", run};
        auto& pings = section(*story, "pings", true);
        if (pings.size() >= PingsAtOnce)
            return {false, "This story already has " + std::to_string(PingsAtOnce) + " markers on the maps.", run};
        auto ping = Value::object();
        ping.add("cell", cell);
        ping.add("x", x);
        ping.add("y", y);
        ping.add("label", label);
        ping.add("until", now() + minutes * 60.);
        if (!to.empty())
            ping.add("to", to);
        pings.push(std::move(ping));
        changed();
        return {true, "Marked " + label + " on the maps for " + std::to_string(minutes) + " minutes.", run};
    }
    if (kind == "scene.stop")
    {
        const auto scene = p.string("scene");
        int stopped = 0;
        for (auto it = story->cinematics.begin(); it != story->cinematics.end();)
        {
            auto& s = it->second;
            if (!scene.empty() && s["doc"].string("id") != scene)
            {
                ++it;
                continue;
            }
            world_.recordEvent({"scene failed", s.string("player"), run, {}, 0, 0, {}, int(s.number("visit")), 0, s.string("cue")});
            sceneLines_.erase(it->first);
            it = story->cinematics.erase(it);
            ++stopped;
        }
        if (!stopped)
            return {true, "No scene of the story was playing.", run};
        storylines_.touch();
        ++revision_;
        saveSoon(StoryOutcomeSeconds);
        return {true, "Stopped " + std::to_string(stopped) + (stopped == 1 ? " scene." : " scenes."), run};
    }

    // ------------------------------------------------------------------------------------------- players' wolves
    if (kind == "item.take" || kind == "warrant.issue" || kind == "warrant.clear")
    {
        const auto* e = world_.entity(target);
        if (!e || e->npc || e->dead)
            return {false, "That wolf is not in the world just now.", target};
        if (kind == "item.take")
        {
            const auto good = p.string("item");
            const int quantity = whole(p, "quantity", 1, 20);
            if (quantity < 1 || !itemValid(good))
                return {false, "Take a known item, 1 to 20 of it.", target};
            const auto* purse = world_.society().account(target);
            if (!purse || Society::stockAll(*purse, good) < quantity)
                return {false, "They carry fewer than " + std::to_string(quantity) + " " + Society::itemName(good) + ".", target};
            world_.society().destroy(target, good, quantity, "story: taken");
            recordCharacters(Economy, {target});
            tellActedOn(target, std::to_string(quantity) + " " + Society::itemName(good) + (quantity == 1 ? " is" : " are") +
                                    " taken from you (" + story->title + ").");
            return {true, "Took " + std::to_string(quantity) + " " + Society::itemName(good) + ".", target};
        }
        const auto town = p.string("town");
        if (kind == "warrant.issue")
        {
            const auto reason = p.string("reason");
            const int fine = whole(p, "fine", 0, 200);
            if (!oneLine(reason, 80) || fine < 0)
                return {false, "A warrant needs what it is for (up to 80 characters) and a fine of 0 to 200p.", target};
            const auto outcome = world_.storyWarrant(target, town, fine, reason);
            if (outcome.ok)
                record(Crime, target);
            return outcome;
        }
        const auto outcome = world_.liftWarrant(target, town);
        if (outcome.ok)
            record(Crime, target);
        return outcome;
    }

    // ------------------------------------------------------------------------------------------------ residents
    if (kind == "npc.hold" || kind == "npc.release" || kind == "npc.follow" || kind == "npc.unfollow")
    {
        auto* e = world_.entity(target);
        if (!e || !e->npc || !world_.society().resident(target))
            return {false, "No such resident.", target};
        auto& holds = section(*story, "npcs");
        auto& follow = section(*story, "follow", true);
        if (kind == "npc.hold")
        {
            if (e->dead)
                return {false, "The dead keep their own place.", target};
            if (heldByAnotherStory(target, run))
                return {false, "Another story holds this resident.", target};
            for (const auto& other : storylines_.activeScenes())
                if (other != run)
                    if (const auto* s = storylines_.find(other); s && s->sceneOrders.count(target))
                        return {false, "Another story is directing this resident.", target};
            if (!holds.has(target))
                holds.set(target, e->pace);
            if (!e->sceneDirected && !world_.inBattle(target) && e->leaderId.empty())
                world_.sceneHold(target, true, e->pace);
            e->activity = "kept here by a story";
            changed();
            return {true, e->name + " is held where they are.", target};
        }
        if (kind == "npc.release")
        {
            if (!holds.has(target))
                return {false, "This story does not hold that resident.", target};
            const int pace = int(holds.number(target, e->pace));
            holds.erase(target);
            if (!story->sceneOrders.count(target))
                world_.sceneHold(target, false, pace);
            e->activity.clear();
            changed();
            return {true, e->name + " goes back to their day.", target};
        }
        if (kind == "npc.follow")
        {
            const auto wolf = p.string("player");
            const auto* w = world_.entity(wolf);
            if (!w || w->npc || w->dead)
                return {false, "That wolf is not in the world just now.", target};
            if (parties_.companion(target))
                return {false, "They already travel with a party.", target};
            // (A story sends them: their work and its hours give way, as a Dungeon Master's direction does; the rest of
            // the party's rules stand: a stranger of the road, the dead, a child, the Quickened, the wanted, the asleep.)
            if (const auto why = whyNotJoin(target);
                !why.empty() && why != "Their work keeps them here." && why.rfind("They have work to do", 0) != 0)
                return {false, why, target};
            party::Companion c;
            c.id = target;
            c.by = wolf;
            c.reason = "story";
            c.joined = world_.calendarDays();
            const auto* mine = parties_.of(wolf);
            const auto outcome = parties_.addCompanion(mine && mine->leader != wolf ? mine->leader : wolf, c);
            if (!outcome.ok)
                return {false, outcome.message, target};
            e->leaderId = "party:" + outcome.message;
            world_.stop(target);
            if (!listed(follow, target))
                follow.push(target);
            if (auto* client = clientOf(wolf))
                storylineToast(client, names::capitalised(labelFor(wolf, target)) + " travels with you now (" + story->title + ").");
            changed();
            saveSoon();
            return {true, e->name + " travels with " + w->name + ".", target};
        }
        const auto* c = parties_.companion(target);
        if (!c || c->reason != "story" || !listed(follow, target))
            return {false, "This story did not send that resident along.", target};
        parties_.releaseCompanion(target);
        releaseCompanions();
        unlist(follow, target);
        changed();
        saveSoon();
        return {true, e->name + " goes home.", target};
    }

    // ------------------------------------------------------------------------------------- doors, the ground, boards
    if (kind.rfind("door.", 0) == 0)
    {
        const auto* d = world_.door(target);
        if (!d)
            return {false, "No such door.", target};
        const bool wasLocked = d->locked;
        const int open = kind == "door.open" ? 1 : kind == "door.close" ? 0 : -1;
        const int locked = kind == "door.lock" ? 1 : kind == "door.unlock" ? 0 : -1;
        const auto outcome = world_.setDoorState(target, open, locked);
        if (!outcome.ok)
            return outcome;
        const auto* held = story->held.isObject() ? story->held.find("doors") : nullptr;
        if (locked != -1 || open == 1 || (held && held->has(target)))
        {
            // (The lock it had before the story first touched it comes back when the story ends; kept again after a restart.)
            auto& doors = section(*story, "doors");
            if (!doors.has(target))
            {
                auto j = Value::object();
                j.add("was", wasLocked);
                doors.set(target, std::move(j));
            }
            auto& j = *doors.find(target);
            if (locked != -1)
                j.set("locked", locked == 1);
            // (Doc 86: a door the story unlocked or opened is open to wolves while it holds it, a private home's never
            // (World::setStoryOpenDoors); locking it ends that, shutting it does not.)
            if (kind != "door.close")
                j.set("wolves", kind != "door.lock");
            changed();
        }
        saveSoon();
        return outcome;
    }
    if (kind == "item.place")
    {
        const auto good = p.string("item"), cell = p.string("cell");
        const int quantity = whole(p, "quantity", 1, 5), x = whole(p, "x", 0, 4095), y = whole(p, "y", 0, 4095);
        if (quantity < 1 || !itemValid(good) || x < 0 || y < 0)
            return {false, "Leave a known item (1 to 5) on a tile.", target};
        for (int n = 0; n < quantity; ++n)
            if (const auto outcome = world_.placeOnGround(cell, x, y, good); !outcome.ok)
                return outcome;
        record(Economy);
        return {true, "Left " + std::to_string(quantity) + " " + Society::itemName(good) + " on the ground.", target};
    }
    if (kind == "board.post")
    {
        const auto text = p.string("text");
        const int days = whole(p, "days", 1, 28);
        if (!oneLine(text, 300) || days < 1)
            return {false, "A notice is one line of up to 300 characters, up 1 to 28 days.", target};
        const auto& cells = world_.cells();
        if (target.empty() || std::none_of(cells.begin(), cells.end(), [&](const auto& c) { return c.second.region == target; }))
            return {false, "No such town.", target};
        auto& notices = section(*story, "notices", true);
        documents::Document d;
        d.id = "story-" + run.substr(6, 12) + "-" + std::to_string(notices.size() + 1) + "-" + std::to_string(std::int64_t(now()) % 100000);
        d.kind = "notice";
        d.author = StoryNoticeAuthor;
        d.board = "board:" + target;
        d.noticeKind = "other";
        d.text = text;
        d.sign = story->title;
        d.state = "delivered";
        d.written = d.deliverAt = world_.calendarDays();
        d.expires = d.written + days;
        d.fromTown = target;
        const auto& kept = documents_.add(d);
        notices.push(kept.id);
        world_.recordEvent({"notice posted", {}, {}, {}, 0, 0, {}, 0, 0, kept.id});
        changed();
        saveSoon();
        return {true, "Pinned to the board of " + target + " for " + std::to_string(days) + " days.", kept.id};
    }

    // ------------------------------------------------------------------------- the weather (never the calendar)
    if (kind == "weather.set")
    {
        Weather weather = Weather::Clear;
        const int hours = whole(p, "hours", 1, 72);
        if (!parseWeather(p.string("weather"), weather) || hours < 1)
            return {false, "Set a weather (clear, overcast, rain, storm, fog, snow, sandstorm) for 1 to 72 hours.", target};
        std::vector<std::string> places;
        if (p.boolean("streets"))
        {
            for (const auto& [id, c] : world_.cells())
                if (c.region == target && c.outdoors)
                    places.push_back(id);
            if (places.empty())
                return {false, "That town has no streets out of doors.", target};
        }
        else
        {
            const auto* c = world_.cell(target);
            if (!c)
                return {false, "No such place.", target};
            if (!c->outdoors)
                return {false, "The weather is set out of doors: choose a street or a field.", target};
            places.push_back(target);
        }
        auto& held = section(*story, "weather");
        const double until = world_.calendarDays() + hours / 24.;
        for (const auto& id : places)
        {
            const auto* c = world_.cell(id);
            if (!held.has(id))
            {
                auto j = Value::object();
                j.add("was", weatherName(c->weather));
                j.add("seasonal", c->seasonalWeather);
                held.set(id, std::move(j));
            }
            held.find(id)->set("until", until);
            world_.setWeather(id, weather);
        }
        changed();
        saveSoon();
        return {true, "The weather in " + (p.boolean("streets") ? "the streets of " + target : world_.cell(target)->name) + " is " +
                          weatherName(weather) + " for " + std::to_string(hours) + " hours.", target};
    }
    return {false, "Unknown story effect.", target};
}

void Game::noteStoryHeld(const Value& payload, const char* key, const std::string& id)
{
    auto* s = storylines_.find(payload.string("run"));
    if (!s || s->kind != "world" || s->source != "storykeeper" || !s->live())
        return;
    auto& list = section(*s, key, true);
    if (!listed(list, id))
        list.push(id);
    storylines_.touch();
    record(StoryBook);
}

std::string Game::storyProject(const std::string& town, const std::string& kind) const
{
    for (const auto& [id, p] : projects_.all())
        if (p.town == town && p.kind == kind && p.state == "open")
            return id;
    return {};
}

std::string Game::storyDeed(const std::string& wolf, const std::string& detail) const
{
    // (The newest still standing, awarded by a Dungeon Master's hand or a story: deed.award's source.)
    std::string found;
    double day = -1;
    for (const auto& [id, d] : fame_.all())
        if (!d.revoked && d.source == "dm" && d.detail == json::utf8Prefix(detail, 160) && d.day >= day &&
            std::find(d.doers.begin(), d.doers.end(), wolf) != d.doers.end())
            found = id, day = d.day;
    return found;
}

std::string Game::storyStoryline(const std::string& wolf, const std::string& templateId) const
{
    for (const auto* s : storylines_.of(wolf))
        if (s->kind == "personal" && s->live() && s->templateId == templateId && s->owner == wolf)
            return s->id;
    return {};
}

void Game::endStoryWorld(storylines::Storyline& story)
{
    if (!story.held.isObject() || !story.held.size())
        return;
    auto& h = story.held;
    for (const auto& [door, j] : h.object("doors").fields())
        if (world_.door(door) && j.has("locked"))
            world_.setDoorState(door, -1, j.boolean("was") ? 1 : 0);
    for (const auto& [npc, pace] : h.object("npcs").fields())
        if (!story.sceneOrders.count(npc))
            world_.sceneHold(npc, false, int(pace.asNumber()));
    bool released = false;
    for (const auto& npc : h.array("follow"))
        if (const auto* c = parties_.companion(npc.asString()); c && c->reason == "story")
        {
            parties_.releaseCompanion(npc.asString());
            released = true;
        }
    if (released)
        releaseCompanions();
    for (const auto& v : h.array("visitors"))
        world_.sendVisitorAway(v.asString());
    for (const auto& npc : h.array("mantles"))
        releaseMantle(npc.asString());
    for (const auto& id : h.array("notices"))
        if (documents_.find(id.asString()))
            documents_.erase(id.asString());
    for (const auto& [cell, j] : h.object("weather").fields())
    {
        Weather was = Weather::Clear;
        if (j.boolean("seasonal"))
            world_.useSeasonalWeather(cell);
        else if (parseWeather(j.string("was"), was))
            world_.setWeather(cell, was);
    }
    // (Its notes stay in the journal, with the story; everything else it held is gone.)
    auto kept = Value::object();
    if (h.array("notes").size())
        kept.set("notes", h["notes"]);
    story.held = std::move(kept);
    storiesHolding_.erase(story.id);
    refreshStoryDoors();    // (Its doors no longer open to wolves.)
    saveSoon();
}

void Game::tickStoryWorld(double dt)
{
    if (storiesHolding_.empty() || (storyWorldAccumulator_ += dt) < .5)
        return;
    storyWorldAccumulator_ = 0;
    bool touched = false;
    for (const auto& run : std::vector<std::string>(storiesHolding_.begin(), storiesHolding_.end()))
    {
        auto* s = storylines_.find(run);
        if (!s || !s->live() || !s->held.isObject())
            continue;
        // Residents held: kept off their day while they can be (not in a fight, not away with a party).
        for (const auto& [npc, pace] : s->held.object("npcs").fields())
            if (auto* e = world_.entity(npc); e && e->npc && !e->dead && !e->sceneDirected && !world_.inBattle(npc) && e->leaderId.empty())
                world_.sceneHold(npc, true, int(pace.asNumber()));
        // Locks kept (a restart, or a cell loaded again, gives a door the lock its map has).
        for (const auto& [door, j] : s->held.object("doors").fields())
            if (const auto* d = world_.door(door); d && j.has("locked") && d->locked != j.boolean("locked"))
                world_.setDoorState(door, -1, j.boolean("locked") ? 1 : 0);
        // The weather runs out on the world's clock; markers on the real one.
        if (auto* weather = s->held.find("weather"); weather && weather->size())
        {
            std::vector<std::string> done;
            for (const auto& [cell, j] : weather->fields())
                if (world_.calendarDays() >= j.number("until"))
                    done.push_back(cell);
            for (const auto& cell : done)
            {
                const auto& j = (*weather)[cell];
                Weather was = Weather::Clear;
                if (j.boolean("seasonal"))
                    world_.useSeasonalWeather(cell);
                else if (parseWeather(j.string("was"), was))
                    world_.setWeather(cell, was);
                weather->erase(cell);
                touched = true;
            }
        }
        if (auto* pings = s->held.find("pings"); pings && pings->size())
        {
            auto kept = Value::array();
            for (const auto& ping : pings->items())
                if (ping.number("until") > now())
                    kept.push(ping);
            if (kept.size() != pings->size())
            {
                *pings = std::move(kept);
                touched = true;
            }
        }
    }
    if (touched)
    {
        storylines_.touch();
        refreshStoryWatch();
        record(StoryBook);
    }
}

Value Game::storyJournalExtras(const storylines::Storyline& s, const std::string& viewer) const
{
    auto out = Value::object();
    if (!s.held.isObject())
        return out;
    auto notes = Value::array();
    for (const auto& n : s.held.array("notes"))
        if (!n.has("to") || n.string("to") == viewer)
            notes.push(n);
    if (notes.size())
        out.add("notes", std::move(notes));
    auto pings = Value::array();
    if (s.live())
        for (const auto& ping : s.held.array("pings"))
        {
            if ((ping.has("to") && ping.string("to") != viewer) || ping.number("until") <= now())
                continue;
            auto j = Value::object();
            const auto cell = ping.string("cell");
            j.add("cell", cell);
            j.add("x", ping.number("x"));
            j.add("y", ping.number("y"));
            j.add("label", ping.string("label"));
            j.add("until", ping.number("until"));
            // (Where it goes on the maps, in the realm's tiles, as a party mate's does: indoors, at the door from the street.)
            if (const auto* c = world_.cell(cell))
            {
                j.add("place", c->name);
                if (c->outdoors)
                {
                    j.add("wx", c->worldX + ping.number("x") + .5);
                    j.add("wy", c->worldY + ping.number("y") + .5);
                }
                else
                {
                    std::string street;
                    Vec2 door;
                    if (const auto* st = world_.streetDoor(cell, street, door) ? world_.cell(street) : nullptr)
                    {
                        j.add("inside", true);
                        j.add("wx", st->worldX + door.x);
                        j.add("wy", st->worldY + door.y);
                    }
                }
            }
            pings.push(std::move(j));
        }
    if (pings.size())
        out.add("pings", std::move(pings));
    return out;
}
} // namespace ratw::game
