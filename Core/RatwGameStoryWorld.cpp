// A world story's new world effects (Docs/Design/85-story-triggers-and-campaign-control.md, 3.5 and Phase 9). Each is a
// DM order of its own name, sent by the story runner (alone or in a story.effects batch), with the run and its visit in
// its payload; the game applies it under the rules it already keeps (a door's sides, a party's places, a resident's
// protection, a town's watch, the ground's account) and answers with a receipt. What the game must undo when the run
// ends is kept with the story (Storyline::held): a lock, a hold, a follower, a stranger, a mantle, a notice, the weather,
// a marker. A story may set the weather for a while, never the calendar or the hour.
#include "RatwGame.h"
#include "RatwClaims.h"
#include "RatwFame.h"
#include "RatwItems.h"
#include "RatwNames.h"

#include <algorithm>
#include <cctype>
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
        const auto text = fillStoryNames(p.string("text"), 200), to = p.string("player");   // (Doc 86, C1b: names for tokens.)
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
        if (locked != -1)
        {
            // (The lock it had before the story first touched it comes back when the story ends; kept again after a restart.)
            auto& doors = section(*story, "doors");
            if (!doors.has(target))
            {
                auto j = Value::object();
                j.add("was", wasLocked);
                doors.set(target, std::move(j));
            }
            doors.find(target)->set("locked", locked == 1);
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
        const auto text = fillStoryNames(p.string("text"), 300);   // (Doc 86, C1b: names for tokens.)
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
    // (Doc 86, C11 and C8: its holds on residents let go; what it had residents remember forgotten, unless it was kept.)
    dropStoryHolds("run:" + story.id);
    for (auto it = storyMemory_.begin(); it != storyMemory_.end();)
    {
        for (auto m = it->second.begin(); m != it->second.end();)
            m = m->second.run == story.id && !m->second.keep ? it->second.erase(m) : std::next(m);
        it = it->second.empty() ? storyMemory_.erase(it) : std::next(it);
    }
    if (!story.held.isObject() || !story.held.size())
        return;
    auto& h = story.held;
    for (const auto& [door, j] : h.object("doors").fields())
        if (world_.door(door))
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
            if (const auto* d = world_.door(door); d && d->locked != j.boolean("locked"))
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

// ---- Doc 86, Phase 1b: a story's holds on a resident (C11: spared from old age, protected for a while) and what the
// residents remember and come to believe from it (C8, T7). Each hold has owners, "run:<run>" (let go when the run ends)
// or "campaign:<id>" (let go when the runner says the campaign has ended: story.campaign_end); a resident is spared or
// protected while any owner holds them. A memory belongs to an owner as talk's own does (realm, town:<region>,
// person:<npc>, pair:<a>|<b>), kept apart from talk's so a story's is never evicted by talk's, and recalled under it.
namespace
{
constexpr std::size_t MemoryPerOwner = 64, MemoryMost = 8192, PairPerOwner = 16, PairMost = 20000;

bool talkWord(const std::string& v, bool key)
{
    if (v.empty() || v.size() > 40 || (key && !(v[0] >= 'a' && v[0] <= 'z')))
        return false;
    return std::all_of(v.begin(), v.end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'; });
}

// (A rumour's phrase never holds an id: a word with _ or -, or letters run into digits, rm111. As story_program.CLAIM_ID.)
bool idInWords(const std::string& text)
{
    std::size_t i = 0;
    while (i < text.size())
    {
        while (i < text.size() && !std::isalnum(static_cast<unsigned char>(text[i])) && text[i] != '_')
            ++i;
        const auto start = i;
        bool under = false, digit = false, letter = false;
        while (i < text.size() && (std::isalnum(static_cast<unsigned char>(text[i])) || text[i] == '_' || text[i] == '-'))
        {
            under |= text[i] == '_';
            digit |= std::isdigit(static_cast<unsigned char>(text[i])) != 0;
            letter |= std::isalpha(static_cast<unsigned char>(text[i])) != 0;
            ++i;
        }
        const auto word = text.substr(start, i - start);
        if (under || (digit && letter) || word.rfind("wolf-", 0) == 0)
            return true;
    }
    return false;
}

std::uint64_t storyHash(const std::string& text)
{
    std::uint64_t h = 1469598103934665603ULL;
    for (const unsigned char c : text)
        h = (h ^ c) * 1099511628211ULL;
    return h;
}
} // namespace

std::set<std::string> Game::storyHoldOwners(const std::string& kind, const std::string& npc) const
{
    const auto k = storyHolds_.find(kind);
    if (k == storyHolds_.end())
        return {};
    const auto n = k->second.find(npc);
    return n == k->second.end() ? std::set<std::string>{} : n->second;
}

void Game::sparedChanged()
{
    std::set<std::string> spared;
    if (const auto k = storyHolds_.find("spare"); k != storyHolds_.end())
        for (const auto& [npc, owners] : k->second)
            if (!owners.empty())
                spared.insert(npc);
    world_.setStorySpared(std::move(spared));
}

void Game::dropStoryHolds(const std::string& owner)
{
    bool changed = false;
    for (auto& [kind, npcs] : storyHolds_)
        for (auto it = npcs.begin(); it != npcs.end();)
        {
            changed |= it->second.erase(owner) > 0;
            it = it->second.empty() ? npcs.erase(it) : std::next(it);
        }
    if (changed)
    {
        sparedChanged();
        saveSoon();
    }
}

Result Game::applyStoryHold(const std::string& kind, const std::string& npc, const Value& p)
{
    // npc.spare / npc.unspare, and npc.protect / npc.unprotect with `lasts` run or campaign: the payload's run (and its
    // campaign) owns the hold. A Dungeon Master's protect (no run, or `lasts` ever) is the mark it always was.
    if (!world_.society().resident(npc))
        return {false, "No such resident.", npc};
    const auto run = p.string("run"), campaign = p.string("campaign");
    const bool sparing = kind == "npc.spare" || kind == "npc.unspare";
    const auto holdKind = sparing ? std::string("spare") : std::string("protect");
    if (kind == "npc.spare" || kind == "npc.protect")
    {
        const auto lasts = p.string("lasts", sparing ? "run" : "ever");
        if (lasts != "run" && lasts != "campaign")
            return {false, "A story's hold lasts for its run or its campaign.", npc};
        if (lasts == "campaign" && campaign.empty())
            return {false, "Only a chapter of a campaign holds a resident for the campaign.", npc};
        if (const auto* s = run.empty() ? nullptr : storylines_.find(run); !s || s->source != "storykeeper" || !s->live())
            return {false, "A story's hold names its run, and that story is running.", npc};
        if (!sparing && unprotectedMarks_.count(npc))
            return {false, "A Dungeon Master has taken that resident's protection off.", npc};
        storyHolds_[holdKind][npc].insert(lasts == "campaign" ? "campaign:" + campaign : "run:" + run);
        if (sparing)
            sparedChanged();
        saveSoon();
        return {true, std::string(sparing ? "Spared from old age " : "Protected ") +
                          (lasts == "campaign" ? "while the campaign runs." : "while the story runs."), npc};
    }
    // npc.unspare (npc.unprotect drops its holds beside the mark: RatwGame.cpp): this run's and its campaign's owners go.
    bool any = false;
    if (auto k = storyHolds_.find(holdKind); k != storyHolds_.end())
        if (auto n = k->second.find(npc); n != k->second.end())
        {
            any |= n->second.erase("run:" + run) > 0;
            if (!campaign.empty())
                any |= n->second.erase("campaign:" + campaign) > 0;
            if (n->second.empty())
                k->second.erase(n);
        }
    if (sparing)
        sparedChanged();
    saveSoon();
    const bool still = !storyHoldOwners(holdKind, npc).empty();
    return {true, std::string(any ? "No longer spared by this story" : "This story was not sparing them") +
                      (still ? " (another story still does)." : "."), npc};
}

void Game::storyRecall(const std::string& owner, const std::string& prefix, talk::Facts& into)
{
    const auto found = storyMemory_.find(owner);
    if (found == storyMemory_.end())
        return;
    const double today = world_.calendarDays();
    for (const auto& [key, m] : found->second)
        if (m.until < 0 || m.until >= today)
            into[prefix + "mem." + key] = m.value;
}

Result Game::applyStoryTalk(const std::string& kind, const std::string& target, const Value& p)
{
    const auto run = p.string("run");
    auto* story = storylines_.find(run);
    if (!story || story->kind != "world" || story->source != "storykeeper" || !story->live())
        return {false, "That story is not running.", target};
    const auto& society = world_.society();
    if (kind == "belief.plant")
    {
        // A rumour (T7: or its clearing) about a resident, believed by `count` residents of a town: placed, living, not
        // the one it is about, chosen by the run's seed so the same story picks the same.
        const auto subject = target, town = p.string("town"), claim = p.string("claim"), claimKind = p.string("claimKind", "story.rumour");
        const int count = whole(p, "count", 1, 50), confidence = whole(p, "confidence", 10, 90);
        if (!society.resident(subject))
            return {false, "No such resident to be talked of.", subject};
        if (claimKind != "story.rumour" && claimKind != "story.cleared")
            return {false, "A story's rumour is story.rumour or story.cleared.", subject};
        if (town.empty() || claim.empty() || !oneLine(claim, 120) || claim.find('#') != std::string::npos || claims::rawId(claim) || idInWords(claim) ||
            count < 1 || confidence < 10)
            return {false, "A rumour needs a town, a phrase of up to 120 characters (no ids), how many and how sure.", subject};
        std::vector<std::pair<std::uint64_t, std::string>> holders;
        for (const auto& [id, e] : world_.entities())
        {
            const auto* life = society.resident(id);
            if (!e.npc || e.dead || e.transient || id == subject || !life || life->homeCell.empty() || world_.townIdOf(life->homeCell) != town)
                continue;
            holders.push_back({storyHash(run + "|" + subject + "|" + claim + "|" + id), id});
        }
        if (holders.empty())
            return {false, "Nobody lives in " + town + " to hear it.", subject};
        std::sort(holders.begin(), holders.end());
        // (A clearing keeps words of its own: "never took the silver", so it never merges with the rumour it answers.)
        const auto words = claimKind == "story.cleared" ? "never " + claim : claim;
        std::size_t n = 0;
        for (const auto& [h, id] : holders)
        {
            if (n >= std::size_t(count))
                break;
            Belief b;
            b.holder = id;
            b.subject = subject;
            b.claim = words;
            b.source = "story:" + run;
            b.confidence = confidence / 100.0;
            b.kind = claimKind;
            world_.believe(b);
            ++n;
        }
        saveSoon();
        return {true, std::to_string(n) + " residents of " + town + " believe it now.", subject};
    }
    // talk.remember / talk.forget: the owner as talk names it.
    const auto scope = p.string("scope", "town"), key = p.string("key");
    std::string owner;
    if (scope == "realm")
        owner = "realm";
    else if (scope == "town")
        owner = p.string("town").empty() ? std::string() : "town:" + p.string("town");
    else if (scope == "npc")
        owner = society.resident(p.string("npc")) ? "person:" + p.string("npc") : std::string();
    else if (scope == "pair")
    {
        const auto npc = p.string("npc"), wolf = p.string("player");
        if (!society.resident(npc) || wolf.empty() || !story->takesPart(wolf))
            return {false, "Pair memory names a resident and a wolf of the story.", target};
        owner = "pair:" + std::min(npc, wolf) + "|" + std::max(npc, wolf);
    }
    if (owner.empty())
        return {false, "Name whose memory: the realm, a town, a resident, or a resident and a wolf.", target};
    if (!talkWord(key, true))
        return {false, "A talk memory's key is small letters, digits and _, up to 40.", target};
    if (kind == "talk.forget")
    {
        bool gone = false;
        if (auto found = storyMemory_.find(owner); found != storyMemory_.end())
        {
            gone = found->second.erase(key) > 0;
            if (found->second.empty())
                storyMemory_.erase(found);
        }
        saveSoon();
        return {true, gone ? "Forgotten." : "Nobody remembered that.", target};
    }
    const auto value = p.string("value");
    const int days = whole(p, "days", 1, 365);
    if (!talkWord(value, false) || days < 1)
        return {false, "A talk memory's value is small letters, digits and _ (up to 40), for 1 to 365 days.", target};
    const bool pair = owner.rfind("pair:", 0) == 0;
    auto& memos = storyMemory_[owner];
    memos[key] = {value, world_.calendarDays() + days, run, p.boolean("keep")};
    // The caps: an owner's (64, a pair's 16) and all of them (8,192; pairs 20,000): the soonest to be forgotten goes first.
    const auto soonestOf = [](std::map<std::string, StoryMemo>& m) {
        auto s = m.begin();
        for (auto it = m.begin(); it != m.end(); ++it)
            if (it->second.until < s->second.until)
                s = it;
        return s;
    };
    bool dropped = false;
    while (memos.size() > (pair ? PairPerOwner : MemoryPerOwner))
        memos.erase(soonestOf(memos)), dropped = true;
    std::size_t total = 0;
    for (const auto& [o, m] : storyMemory_)
        if ((o.rfind("pair:", 0) == 0) == pair)
            total += m.size();
    while (total > (pair ? PairMost : MemoryMost))
    {
        std::string ownerOf, keyOf;
        double until = 1e300;
        for (auto& [o, m] : storyMemory_)
            if ((o.rfind("pair:", 0) == 0) == pair)
                for (auto& [k, memo] : m)
                    if (memo.until < until)
                        until = memo.until, ownerOf = o, keyOf = k;
        if (ownerOf.empty())
            break;
        storyMemory_[ownerOf].erase(keyOf);
        if (storyMemory_[ownerOf].empty())
            storyMemory_.erase(ownerOf);
        --total, dropped = true;
    }
    if (dropped && std::floor(world_.calendarDays()) != storyMemoryWarnedDay_)
    {
        storyMemoryWarnedDay_ = std::floor(world_.calendarDays());
        note("warn", "RATW_STORY_MEMORY full: the residents forgot the soonest-to-go of the stories' memories to remember " + key + ".");
    }
    auto& held = section(*story, "memory", true);
    if (!listed(held, owner + "#" + key))
        held.push(owner + "#" + key);
    storiesHolding_.insert(story->id);
    storylines_.touch();
    record(StoryBook);
    saveSoon();
    return {true, "Remembered (" + owner + ": " + key + " = " + value + ", " + std::to_string(days) + " days).", target};
}

void Game::storyTalkSave(Value& root) const
{
    auto holds = Value::object();
    for (const auto& [kind, npcs] : storyHolds_)
    {
        auto byNpc = Value::object();
        for (const auto& [npc, owners] : npcs)
        {
            auto list = Value::array();
            for (const auto& o : owners)
                list.push(o);
            if (list.size())
                byNpc.add(npc, std::move(list));
        }
        if (byNpc.size())
            holds.add(kind, std::move(byNpc));
    }
    root.add("storyHolds", std::move(holds));
    auto memory = Value::object();
    for (const auto& [owner, memos] : storyMemory_)
    {
        auto kept = Value::object();
        for (const auto& [key, m] : memos)
            if (m.until < 0 || m.until >= world_.calendarDays())
            {
                auto j = Value::object();
                j.add("value", m.value);
                j.add("until", m.until);
                j.add("run", m.run);
                if (m.keep)
                    j.add("keep", true);
                kept.add(key, std::move(j));
            }
        if (kept.size())
            memory.add(owner, std::move(kept));
    }
    root.add("storyMemory", std::move(memory));
    storyGreetsSave(root);                          // (Doc 86, C5: the greetings waiting.)
}

void Game::storyTalkLoad(const Value& saved)
{
    storyHolds_.clear();
    for (const auto& [kind, npcs] : saved.object("storyHolds").fields())
        if (kind == "spare" || kind == "protect")
            for (const auto& [npc, owners] : npcs.fields())
                for (const auto& o : owners.items())
                    if (o.isString() && npc.size() <= 80 && (o.asString().rfind("run:", 0) == 0 || o.asString().rfind("campaign:", 0) == 0))
                        storyHolds_[kind][npc].insert(o.asString());
    storyMemory_.clear();
    for (const auto& [owner, kept] : saved.object("storyMemory").fields())
    {
        // (As remembering caps them: a pair's owner keeps PairPerOwner, any other MemoryPerOwner; no empty owner made.)
        const auto cap = owner.rfind("pair:", 0) == 0 ? PairPerOwner : MemoryPerOwner;
        for (const auto& [key, j] : kept.fields())
            if (talkWord(key, true) && talkWord(j.string("value"), false))
            {
                auto& mine = storyMemory_[owner];
                if (mine.size() < cap || mine.count(key))
                    mine[key] = {j.string("value"), j.number("until", -1), j.string("run"), j.boolean("keep")};
            }
    }
    storyGreetsLoad(saved);
    sparedChanged();
}

// ------------------------------------------------------------------ doc 86, Phase 3: thanks, titles and doubt
namespace
{
constexpr std::size_t TitlesKept = 8, GreetsMost = 20000;

std::string lowered(std::string s)
{
    for (auto& c : s)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// (The kinds of claim a story may put to rest: about a resident, from claims.json; story_program checks the same list.)
bool doubtable(const std::string& kind)
{
    static const std::set<std::string> kinds{"theft", "theft.attempt", "theft.unseen", "assault", "assault.unseen", "violent",
                                             "warrant", "promise.broken", "prowling", "story.rumour"};
    return kinds.count(kind) > 0;
}
} // namespace

std::string Game::storyTitleFor(const std::string& wolf, const std::string& npc) const
{
    // C4: the newest title known where the resident lives (the realm, or their town); a lapsed one is never used.
    const auto* e = world_.entity(wolf);
    if (!e || e->npc)
    {
        const auto saved = characters_.find(wolf);
        e = saved == characters_.end() ? nullptr : &saved->second;
    }
    if (!e)
        return {};
    std::string town;
    if (const auto* life = world_.society().resident(npc); life && !life->homeCell.empty())
        town = world_.townIdOf(life->homeCell);
    const double today = world_.calendarDays();
    const StoryTitle* best = nullptr;
    for (const auto& t : e->titles)
        if ((t.until < 0 || t.until >= today) && (t.known == "realm" || (!town.empty() && t.known == "town:" + town)) &&
            (!best || t.since >= best->since))
            best = &t;
    return best ? best->title : std::string();
}

Value Game::titlesView(const Entity& wolf) const
{
    // The card's titles (newest first), each with where it is known and when it lapses (-1: for good).
    auto list = Value::array();
    const double today = world_.calendarDays();
    std::vector<const StoryTitle*> live;
    for (const auto& t : wolf.titles)
        if (t.until < 0 || t.until >= today)
            live.push_back(&t);
    std::stable_sort(live.begin(), live.end(), [](const auto* a, const auto* b) { return a->since > b->since; });
    for (const auto* t : live)
    {
        auto j = Value::object();
        j.add("title", t->title);
        const auto town = t->known.rfind("town:", 0) == 0 ? t->known.substr(5) : std::string();
        j.add("known", town.empty() ? std::string("realm") : std::string("town"));
        if (!town.empty())
            j.add("town", townWords(town));
        j.add("days", t->until < 0 ? -1.0 : std::max(0.0, std::ceil(t->until - today)));
        list.push(std::move(j));
    }
    return list;
}

std::string Game::takeStoryGreet(const std::string& npc, const std::string& wolf)
{
    const auto byNpc = storyGreets_.find(npc);
    if (byNpc == storyGreets_.end())
        return {};
    const auto found = byNpc->second.find(wolf);
    if (found == byNpc->second.end())
        return {};
    const auto text = found->second.until >= world_.calendarDays() ? found->second.text : std::string();
    byNpc->second.erase(found);
    if (byNpc->second.empty())
        storyGreets_.erase(byNpc);
    saveSoon();
    return text;
}

Result Game::applyStoryThanks(const std::string& kind, const std::string& target, const Value& p)
{
    const auto run = p.string("run");
    auto* story = storylines_.find(run);
    if (!story || story->kind != "world" || story->source != "storykeeper" || !story->live())
        return {false, "That story is not running.", target};
    const double today = world_.calendarDays();
    if (kind == "belief.forget")
    {
        // J8: residents (of one town, or everywhere) less sure that a resident did one kind of thing.
        const auto claimKind = p.string("claimKind"), town = p.string("town");
        const int share = whole(p, "share", 10, 100);
        if (!world_.society().resident(target))
            return {false, "No such resident to be talked of.", target};
        if (!doubtable(claimKind) || share < 10)
            return {false, "Name the kind of claim (a theft, a story's rumour...) and by how much (10 to 100%).", target};
        const int touched = world_.doubt(target, claimKind, town, share);
        saveSoon();
        return {true, std::to_string(touched) + (touched == 1 ? " belief" : " beliefs") + " grew less sure" + (town.empty() ? "." : " in " + town + "."),
                target};
    }
    if (kind == "npc.greet")
    {
        // C5: the next time this wolf opens the resident's window (within the days), these words are its greeting, once.
        const auto wolf = p.string("player");
        const auto text = fillStoryNames(p.string("text"), 200);
        const int days = p.has("days") ? whole(p, "days", 1, 28) : 7;
        if (!world_.society().resident(target))
            return {false, "No such resident.", target};
        if (wolf.empty() || !story->takesPart(wolf))
            return {false, "A greeting is for a wolf of the story.", target};
        if (!oneLine(text, 200) || days < 1)
            return {false, "A greeting is one line of up to 200 characters, for 1 to 28 days.", target};
        storyGreets_[target][wolf] = {text, run, today + days};
        std::size_t total = 0;
        for (const auto& [npc, wolves] : storyGreets_)
            total += wolves.size();
        while (total > GreetsMost)
        {
            // (The soonest to lapse goes first.)
            auto npcAt = storyGreets_.end();
            std::map<std::string, StoryGreet>::iterator wolfAt;
            double soonest = 1e300;
            for (auto n = storyGreets_.begin(); n != storyGreets_.end(); ++n)
                for (auto w = n->second.begin(); w != n->second.end(); ++w)
                    if (w->second.until < soonest)
                        soonest = w->second.until, npcAt = n, wolfAt = w;
            if (npcAt == storyGreets_.end())
                break;
            npcAt->second.erase(wolfAt);
            if (npcAt->second.empty())
                storyGreets_.erase(npcAt);
            --total;
        }
        saveSoon();
        return {true, "A greeting waits for " + wolf + " (" + std::to_string(days) + " days).", target};
    }
    // title.give / title.take: the wolf named (online, or as saved).
    auto* online = world_.entity(target);
    Entity* e = online && !online->npc ? online : nullptr;
    if (!e)
        if (auto saved = characters_.find(target); saved != characters_.end())
            e = &saved->second;
    if (!e || e->npc)
        return {false, "Titles are for players' wolves.", target};
    const auto by = story->templateId.empty() ? run : story->templateId;
    std::string said;
    if (kind == "title.give")
    {
        const auto title = fillStoryNames(p.string("title"), 60);
        const auto known = p.string("known", "realm"), town = p.string("town");
        if (!oneLine(title, 60) || title.find('#') != std::string::npos || idInWords(title))
            return {false, "A title is one line of up to 60 characters, with no symbols and no ids.", target};
        if (known != "realm" && known != "town")
            return {false, "A title is known in the realm or in one town.", target};
        const auto& cells = world_.cells();
        if (known == "town" && (town.empty() || std::none_of(cells.begin(), cells.end(), [&](const auto& c) { return c.second.region == town; })))
            return {false, "Name the town the title is known in.", target};
        const int days = p.has("days") ? whole(p, "days", 0, 365) : 0;
        if (days < 0)
            return {false, "A title lasts 0 (for good) to 365 days.", target};
        StoryTitle t{title, known == "town" ? "town:" + town : std::string("realm"), by, run, today, days > 0 ? today + days : -1.0};
        auto& list = e->titles;
        list.erase(std::remove_if(list.begin(), list.end(), [&](const StoryTitle& o) { return o.until >= 0 && o.until < today; }), list.end());
        auto same = std::find_if(list.begin(), list.end(), [&](const StoryTitle& o) { return lowered(o.title) == lowered(title); });
        if (same != list.end())
            *same = t;                              // (Given again: renewed, known where it now says.)
        else
        {
            if (list.size() >= TitlesKept)
            {
                // (A ninth: the oldest that lapses goes, else the oldest.)
                auto drop = list.end();
                for (auto it = list.begin(); it != list.end(); ++it)
                    if (it->until >= 0 && (drop == list.end() || it->since < drop->since))
                        drop = it;
                if (drop == list.end())
                    drop = std::min_element(list.begin(), list.end(), [](const auto& a, const auto& b) { return a.since < b.since; });
                list.erase(drop);
            }
            list.push_back(t);
        }
        // The fame machinery carries it into gossip and the crier as any deed.
        recordDeed("award", {target}, {}, e->cellId, "story", "was named " + title, fame::weightOf("notable"));
        world_.recordEvent({"title given", target, run, {}, 0, 0, t.known, 0, 0, title});
        said = "Named " + title + ".";
    }
    else
    {
        const auto title = p.string("title");
        const auto before = e->titles.size();
        e->titles.erase(std::remove_if(e->titles.begin(), e->titles.end(),
                                       [&](const StoryTitle& o) { return o.by == by && (title.empty() || lowered(o.title) == lowered(title)); }),
                        e->titles.end());
        const auto gone = before - e->titles.size();
        said = gone ? "Took back " + std::to_string(gone) + (gone == 1 ? " title." : " titles.") : std::string("This story gave no such title.");
    }
    if (e == online)
        characters_[target] = *online;
    record(Character, target);
    saveSoon();
    return {true, said, target};
}

void Game::storyGreetsSave(Value& root) const
{
    auto greets = Value::object();
    for (const auto& [npc, wolves] : storyGreets_)
    {
        auto byWolf = Value::object();
        for (const auto& [wolf, g] : wolves)
            if (g.until >= world_.calendarDays())
            {
                auto j = Value::object();
                j.add("text", g.text);
                j.add("run", g.run);
                j.add("until", g.until);
                byWolf.add(wolf, std::move(j));
            }
        if (byWolf.size())
            greets.add(npc, std::move(byWolf));
    }
    root.add("storyGreets", std::move(greets));
}

void Game::storyGreetsLoad(const Value& saved)
{
    storyGreets_.clear();
    std::size_t total = 0;
    for (const auto& [npc, wolves] : saved.object("storyGreets").fields())
        for (const auto& [wolf, j] : wolves.fields())
            if (total < GreetsMost && npc.size() <= 80 && wolf.size() <= 80 && j.isObject() && oneLine(j.string("text"), 200))
            {
                storyGreets_[npc][wolf] = {j.string("text"), json::utf8Prefix(j.string("run"), 80), j.number("until")};
                ++total;
            }
}
} // namespace ratw::game
