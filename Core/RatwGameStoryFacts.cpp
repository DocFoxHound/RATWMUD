// The world's facts for its live stories, and the events written only while one listens (Docs/Design/85-story-triggers-and-
// campaign-control.md, 3.3, 3.4 and Phase 8). The story runner works out from each program what it reads of the world and
// which events it listens for (story_program.watch_of: its watch list), and sends it with each story.phase. The game keeps the
// union of the live stories' lists (refreshStoryWatch): the World writes the watched events it makes (doors, items picked up
// and dropped, wolves downed and back up, crimes seen), the game the rest (here: the calendar's and the weather's, a region's
// count of story wolves; trades, buildings, dialogue windows and parties where they happen). With every write of the events
// (a checkpoint's, or Phase 7's early one), the game hands the store one document of the facts the lists name
// (storyFactsJson), written in the same transaction (game.story_facts, migration 0076), so a fact is as old as the events
// beside it. A world no live story asks anything of pays nothing: no facts, no watched events.
#include "RatwGame.h"

#include "RatwCalendar.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace ratw::game
{
using json::Value;

namespace
{
constexpr double WatchSeconds = 1;      // The union of the watch lists, looked at again (a story ended, say).
constexpr double WeatherSeconds = 2;    // The weather in the watched places.
constexpr double ReputationSeconds = 10;
constexpr std::size_t WatchMost = 2000; // Of each kind, all stories together.
constexpr double NearSeconds = 1;       // Doc 85, Phase 10: wolves near the watched residents, looked at this often,
constexpr double NearTiles = 3;         // within this many tiles,
constexpr double ApartTiles = 5;        // and not again until they have been further apart than this.
constexpr double WorldSeconds = 2;      // Purses, standing, stances and mantles, looked at this often.

std::string baseItem(const std::string& item) { return item.substr(0, item.find_first_of("~@")); }
} // namespace

void Game::refreshStoryWatch()
{
    StoryWatch w;
    storiesHolding_.clear();
    for (const auto& [id, s] : storylines_.all())
    {
        // (Doc 85, Phase 9: the live stories holding something in the world, for tickStoryWorld.)
        if (s.source == "storykeeper" && s.live() && s.held.isObject() &&
            (s.held.object("npcs").size() || s.held.object("doors").size() || s.held.object("weather").size() || s.held.array("pings").size()))
            storiesHolding_.insert(id);
        if (s.source != "storykeeper" || !s.live() || !s.watch.isObject())
            continue;
        const auto add = [&](const char* key, std::set<std::string>& into) {
            for (const auto& v : s.watch.array(key))
                if (v.isString() && !v.asString().empty() && v.asString().size() <= 80 && into.size() < WatchMost)
                    into.insert(v.asString());
        };
        add("facts", w.sections);
        add("items", w.items);
        add("factions", w.factions);
        add("towns", w.towns);
        add("residents", w.residents);
        add("cells", w.cells);
        add("events", w.events);
        for (const auto& r : s.watch.array("relations"))
            if (r.isArray() && r.size() == 2 && r.items()[0].isString() && r.items()[1].isString() && w.relations.size() < WatchMost)
                w.relations.insert({r.items()[0].asString(), r.items()[1].asString()});
        add("near", w.near);
        const auto pairs = [&](const char* key, std::set<std::pair<std::string, std::int64_t>>& into) {
            for (const auto& r : s.watch.array(key))
                if (r.isArray() && r.size() == 2 && r.items()[0].isString() && r.items()[1].isNumber() && into.size() < WatchMost &&
                    r.items()[0].asString().size() <= 80)
                    into.insert({r.items()[0].asString(), std::int64_t(std::llround(r.items()[1].asNumber()))});
        };
        pairs("purses", w.purses);
        pairs("standing", w.standing);
        w.weatherWhereWolves = w.weatherWhereWolves || s.watch.boolean("weatherWhereWolves");
    }
    if (w.events != storyWatch_.events)
        world_.setWatchedEvents(w.events);
    storyWatch_ = std::move(w);
}

void Game::handEventsToStore()
{
    // (Every write of the events goes through here: the facts as of this moment go with them, when a story asks.)
    auto events = world_.takeEvents();
    if (!events.empty() && storyWatch_.facts())
        store_->queueFacts(storyFactsJson());
    store_->queueEvents(std::move(events));
}

std::string Game::storyFactsJson()
{
    const auto& w = storyWatch_;
    if (!w.facts())
        return {};
    auto doc = Value::object();
    const auto tile = [](double v) { return double(int(std::floor(v))); };
    std::set<std::string> wolfCells;
    if (w.sections.count("wolves") || w.weatherWhereWolves)
    {
        // A wolf's town reputation: those of the town's residents who know them, liking less disliking (read from the
        // bonds, as Game::reputationLines does), worked out at most every ReputationSeconds for the watched towns.
        if (!w.towns.empty() && world_.time() - storyReputationAt_ >= ReputationSeconds)
        {
            storyReputationAt_ = world_.time();
            storyReputation_.clear();
            std::vector<std::string> online;
            for (const auto* c : clients_)
                if (!c->entityId.empty())
                    online.push_back(c->entityId);
            for (const auto& [id, e] : world_.entities())
            {
                if (!e.npc || e.transient || e.dead)
                    continue;
                const auto town = townFor(e.cellId);
                if (!w.towns.count(town))
                    continue;
                for (const auto& wolf : online)
                    if (const auto* b = world_.bonds().find(id, wolf); b && b->familiarity >= 10)
                        storyReputation_[town][wolf] += (b->affinity >= 20) - (b->affinity <= -20);
            }
        }
        auto wolves = Value::object();
        std::map<std::string, std::vector<std::string>> members;
        for (const auto* c : clients_)
        {
            const auto* e = c->entityId.empty() ? nullptr : world_.entity(c->entityId);
            if (!e || e->npc || e->dead || e->offstage)
                continue;
            const auto& id = e->id;
            wolfCells.insert(e->cellId);
            if (!w.sections.count("wolves"))
                continue;
            auto f = Value::object();
            const auto [x, y] = world_.eventTile(*e);
            const auto* cell = world_.cell(e->cellId);
            f.add("cell", e->cellId);
            f.add("x", double(x));
            f.add("y", double(y));
            f.add("indoors", cell && !cell->outdoors);
            f.add("town", townFor(e->cellId));
            const auto* party = parties_.of(id);
            f.add("party", party ? party->id : std::string());
            f.add("partySize", party ? double(std::max<std::size_t>(1, party->members.size())) : 1.0);
            const auto* purse = world_.society().account(id);
            if (!w.items.empty())
            {
                auto carries = Value::object();
                std::map<std::string, double> held;
                if (purse)
                    for (const auto& [item, n] : purse->stock)
                        if (n > 0 && w.items.count(baseItem(item)))
                            held[baseItem(item)] += n;
                for (const auto& [item, n] : held)
                    carries.add(item, n);
                f.add("carries", std::move(carries));
            }
            f.add("coins", purse ? double(purse->cash) : 0.0);
            if (!w.factions.empty())
                if (const auto* chapter = chapters_.of(id))
                {
                    auto& who = members[chapter->id];
                    if (who.empty())
                        who = chapterMembers(chapter->id);
                    auto standing = Value::object();
                    for (const auto& faction : w.factions)
                        if (factions_.find(faction))
                            standing.add(faction, std::round(factions_.effective(faction, chapter->id, who)));
                    f.add("standing", std::move(standing));
                }
            if (!w.towns.empty())
            {
                auto rep = Value::object();
                for (const auto& town : w.towns)
                {
                    const auto found = storyReputation_.find(town);
                    const auto n = found == storyReputation_.end() ? 0 : found->second.count(id) ? found->second.at(id) : 0;
                    if (n)
                        rep.add(town, double(n));
                }
                f.add("reputation", std::move(rep));
            }
            f.add("downed", e->downedLeft > 0);
            f.add("hurt", e->hurt > 0 || !e->injuries.empty());
            f.add("wanted", world_.warrantFor(id) != nullptr);
            f.add("held", world_.custodyOf(id) != nullptr);
            wolves.add(id, std::move(f));
        }
        if (w.sections.count("wolves"))
            doc.add("wolves", std::move(wolves));
    }
    if (w.sections.count("residents"))
    {
        auto residents = Value::object();
        for (const auto& npc : w.residents)
        {
            const auto* e = world_.entity(npc);
            if (!e || !e->npc)
                continue;
            auto r = Value::object();
            r.add("alive", !e->dead);
            r.add("cell", e->cellId);
            r.add("x", tile(e->position.x));
            r.add("y", tile(e->position.y));
            const auto* cell = world_.cell(e->cellId);
            r.add("indoors", cell && !cell->outdoors);
            const auto speaker = talkSpeaker(*e);
            const auto fact = [&](const char* key) { const auto f = speaker.facts.find(key); return f == speaker.facts.end() ? std::string() : f->second; };
            r.add("job", fact("job"));
            r.add("mood", fact("mood"));
            const auto mantle = mantleOf_.find(npc);
            r.add("mantle", mantle == mantleOf_.end() ? std::string() : mantle->second);
            const auto* spec = world_.society().spec(npc);
            r.add("town", townFor(spec && !spec->home.cell.empty() ? spec->home.cell : e->cellId));
            residents.add(npc, std::move(r));
        }
        doc.add("residents", std::move(residents));
    }
    if (w.sections.count("world"))
    {
        const double days = world_.calendarDays();
        const auto date = calendar::calendarAt(days);
        auto o = Value::object();
        o.add("day", days);
        o.add("hour", double(int(std::floor(date.hour))));
        o.add("minute", double(int(std::floor((date.hour - std::floor(date.hour)) * 60))));
        o.add("weekday", calendar::weekdayName(calendar::weekdayOf(days)));
        o.add("season", calendar::seasonName(date.season));
        o.add("period", date.period);
        o.add("dayOfSeason", double(date.dayOfSeason));
        o.add("year", double(date.year));
        doc.add("world", std::move(o));
    }
    if (w.sections.count("weather"))
    {
        auto weather = Value::object();
        std::set<std::string> cells = w.cells;
        if (w.weatherWhereWolves)
            cells.insert(wolfCells.begin(), wolfCells.end());
        for (const auto& id : cells)
        {
            // (Indoors, the weather outside: the street the place opens on.)
            std::string at = id;
            const auto* cell = world_.cell(at);
            Vec2 spot{};
            if (cell && !cell->outdoors && world_.streetDoor(id, at, spot))
                cell = world_.cell(at);
            if (!cell)
                continue;
            if (cell->outdoors && at == id)
                spot = {cell->width / 2.0, cell->height / 2.0};
            weather.add(id, weatherName(world_.weatherAt(at, spot).kind));
        }
        doc.add("weather", std::move(weather));
    }
    if (w.sections.count("relations"))
    {
        auto relations = Value::object();
        for (const auto& [a, b] : w.relations)
            if (factions_.find(a) && factions_.find(b))
                relations.add(a + ">" + b, factions_.relation(a, b).stance);
        doc.add("relations", std::move(relations));
    }
    return json::dump(doc);
}

void Game::storyWatchTick(double real)
{
    if ((storyWatchAccumulator_ += real) >= WatchSeconds)
    {
        storyWatchAccumulator_ = 0;
        refreshStoryWatch();
    }
    const auto& w = storyWatch_.events;
    if (w.empty())
    {
        storyHour_ = -1;
        storyDay_ = -1;
        storyWeather_.clear();
        storyNear_.clear();
        storyPurses_.clear();
        storyStanding_.clear();
        storyRelations_.clear();
        storyMantles_.clear();
        storyRelationsSeen_ = storyMantlesSeen_ = false;
        return;
    }
    storyWorldWatch(real);
    // The calendar: an hour, a new day (its weekday), dawn and nightfall, a new season. Remembered from the first look, so a
    // story opening (or the server starting) makes no event for the hour it is already in.
    const double days = world_.calendarDays();
    const auto date = calendar::calendarAt(days);
    const int hour = int(std::floor(date.hour));
    const auto day = std::int64_t(std::floor(days));
    const auto season = calendar::seasonName(date.season);
    if (storyHour_ >= 0)
    {
        if (day != storyDay_ && w.count("day begins"))
            world_.recordEvent({"day begins", {}, {}, {}, 0, 0, season, date.dayOfSeason, 0, calendar::weekdayName(calendar::weekdayOf(days))});
        if (season != storySeason_ && w.count("season begins"))
            world_.recordEvent({"season begins", {}, {}, {}, 0, 0, {}, 0, 0, season});
        if (hour != storyHour_ && w.count("hour"))
            world_.recordEvent({"hour", {}, {}, {}, 0, 0, {}, hour, 0, std::to_string(hour)});
        if (date.period != storyPeriod_ && date.period == "dawn" && w.count("dawn"))
            world_.recordEvent({"dawn", {}, {}, {}, 0, 0, {}, 0, 0, {}});
        if (date.period != storyPeriod_ && date.period == "night" && w.count("night falls"))
            world_.recordEvent({"night falls", {}, {}, {}, 0, 0, {}, 0, 0, {}});
    }
    storyHour_ = hour;
    storyDay_ = day;
    storySeason_ = season;
    storyPeriod_ = date.period;
    // The weather in the places watched: those a story names, and (for a story listening for any change) where the wolves
    // in the world stand. Looked at every WeatherSeconds; a place first looked at makes no event.
    if (!w.count("weather changed") || (storyWeatherAccumulator_ += real) < WeatherSeconds)
        return;
    storyWeatherAccumulator_ = 0;
    std::set<std::string> cells = storyWatch_.cells;
    if (storyWatch_.weatherWhereWolves)
        for (const auto* c : clients_)
            if (const auto* e = c->entityId.empty() ? nullptr : world_.entity(c->entityId); e && !e->npc)
                cells.insert(e->cellId);
    std::map<std::string, std::string> now;
    for (const auto& id : cells)
    {
        const auto* cell = world_.cell(id);
        if (!cell || !cell->outdoors)
            continue;
        const auto kind = std::string(weatherName(world_.weatherAt(id, {cell->width / 2.0, cell->height / 2.0}).kind));
        now[id] = kind;
        const auto was = storyWeather_.find(id);
        if (was != storyWeather_.end() && was->second != kind)
            world_.recordEvent({"weather changed", {}, id, id, 0, 0, was->second, 0, 0, kind});
    }
    storyWeather_ = std::move(now);
}

// ---- Doc 85, Phase 10: the events Phase 8 left (3.3), each looked for only while a live story listens, and only at what
// the stories name: a wolf come near a resident, a purse or a wolf's standing past a number, a faction's stance changed, a
// mantle taken or laid down. (A skill grown and an injury healed are written where they happen: World::practise and
// World::healInjuries.)
std::string Game::storyPurseAccount(const std::string& purse) const
{
    // (As character.grant reads a reward's purse: a town's treasury by its town, or the one treasury of a world without
    // towns on its roads; town:<id>:works, till:job:<id> and chapter:<id> are accounts by those names.)
    const auto& society = world_.society();
    if (purse.rfind("town:", 0) == 0 && purse.find(':', 5) == std::string::npos)
    {
        for (const auto& t : world_.towns())
            if (t.id == purse.substr(5))
                return t.store;
        if (society.account("stores:" + purse.substr(5)))
            return "stores:" + purse.substr(5);
        if (world_.towns().empty() && society.account("treasury"))
            return "treasury";
    }
    // (Doc 86, SQ2: a business's till is the account till:job:<founder>; till:<id> names it when no till:<id> account is.)
    if (purse.rfind("till:", 0) == 0 && purse.rfind("till:job:", 0) != 0 && !society.account(purse) &&
        society.account("till:job:" + purse.substr(5)))
        return "till:job:" + purse.substr(5);
    return purse;
}

void Game::storyWorldWatch(double real)
{
    const auto& w = storyWatch_.events;
    // A wolf within three tiles of a watched resident, in the same place (again only once they were more than five apart).
    if (w.count("near resident") && !storyWatch_.near.empty() && (storyNearAccumulator_ += real) >= NearSeconds)
    {
        storyNearAccumulator_ = 0;
        std::map<std::string, std::set<std::string>> now;
        for (const auto* c : clients_)
        {
            const auto* e = c->entityId.empty() ? nullptr : world_.entity(c->entityId);
            if (!e || e->npc || e->offstage || e->dead)
                continue;
            auto& mine = now[e->id];
            const auto& was = storyNear_[e->id];
            for (const auto& npc : storyWatch_.near)
            {
                const auto* r = world_.entity(npc);
                if (!r || r->dead || r->offstage || r->cellId != e->cellId)
                    continue;
                const double d = std::hypot(r->position.x - e->position.x, r->position.y - e->position.y);
                if (d <= NearTiles)
                {
                    mine.insert(npc);
                    if (!was.count(npc))
                    {
                        const auto [x, y] = world_.eventTile(*e);
                        world_.recordEvent({"near resident", e->id, npc, e->cellId, 0, 0, {}, int(std::lround(d)), 0, {}, x, y});
                    }
                }
                else if (was.count(npc) && d <= ApartTiles)
                    mine.insert(npc);   // (Still near enough: not a new meeting until they have been apart.)
            }
        }
        storyNear_ = std::move(now);
    }
    if ((storyWatchedAccumulator_ += real) < WorldSeconds)
        return;
    storyWatchedAccumulator_ = 0;
    // A purse past an amount a story names: up or down.
    if (w.count("purse crossed"))
    {
        std::map<std::string, std::int64_t> now;
        auto& society = world_.society();
        for (const auto& [purse, amount] : storyWatch_.purses)
        {
            const auto* a = society.account(storyPurseAccount(purse));
            if (!a)
                continue;
            now[purse] = a->cash;
            const auto was = storyPurses_.find(purse);
            if (was == storyPurses_.end())
                continue;
            if (was->second < amount && a->cash >= amount)
                world_.recordEvent({"purse crossed", {}, purse, {}, 0, 0, "up", int(std::min<std::int64_t>(a->cash, std::numeric_limits<int>::max())), 0, std::to_string(amount)});
            else if (was->second >= amount && a->cash < amount)
                world_.recordEvent({"purse crossed", {}, purse, {}, 0, 0, "down", int(std::max<std::int64_t>(a->cash, std::numeric_limits<int>::min())), 0, std::to_string(amount)});
        }
        storyPurses_ = std::move(now);
    }
    // A wolf's standing with a faction (their Chapter's, as the faction counts it) past a number a story names.
    if (w.count("standing crossed"))
    {
        std::map<std::string, double> now;
        std::map<std::string, std::vector<std::string>> members;
        for (const auto* c : clients_)
        {
            const auto* e = c->entityId.empty() ? nullptr : world_.entity(c->entityId);
            const auto* chapter = e && !e->npc ? chapters_.of(e->id) : nullptr;
            if (!chapter)
                continue;
            auto& who = members[chapter->id];
            if (who.empty())
                who = chapterMembers(chapter->id);
            for (const auto& [faction, number] : storyWatch_.standing)
            {
                if (!factions_.find(faction))
                    continue;
                const auto key = e->id + "|" + faction;
                const double v = std::round(factions_.effective(faction, chapter->id, who));
                now[key] = v;
                const auto was = storyStanding_.find(key);
                if (was == storyStanding_.end())
                    continue;
                const auto n = double(number);
                if ((was->second < n && v >= n) || (was->second >= n && v < n))
                    world_.recordEvent({"standing crossed", e->id, faction, {}, 0, 0, v >= n ? "up" : "down", int(v), 0, std::to_string(number)});
            }
        }
        storyStanding_ = std::move(now);
    }
    // A faction's stance toward another changed (by anyone's hand).
    if (w.count("relation changed"))
    {
        std::map<std::string, std::pair<std::string, double>> now;
        for (const auto& [a, fa] : factions_.all())
            for (const auto& [b, fb] : factions_.all())
                if (a != b)
                {
                    const auto r = factions_.relation(a, b);
                    now[a + ">" + b] = {r.stance, r.disposition};
                    const auto was = storyRelations_.find(a + ">" + b);
                    if (storyRelationsSeen_ && (was == storyRelations_.end() || was->second.first != r.stance))
                        world_.recordEvent({"relation changed", {}, a, {}, 0, 0, b, int(std::lround(r.disposition)), 0, r.stance});
                }
        storyRelations_ = std::move(now);
        storyRelationsSeen_ = true;
    }
    // A mantle taken up or laid down.
    if (w.count("mantle bound") || w.count("mantle released"))
    {
        const auto& now = mantleWearers();
        if (storyMantlesSeen_)
        {
            for (const auto& [npc, mantle] : storyMantles_)
                if (const auto found = now.find(npc); (found == now.end() || found->second != mantle) && w.count("mantle released"))
                    world_.recordEvent({"mantle released", npc, {}, {}, 0, 0, mantle, 0, 0, {}});
            for (const auto& [npc, mantle] : now)
                if (const auto found = storyMantles_.find(npc); (found == storyMantles_.end() || found->second != mantle) && w.count("mantle bound"))
                    world_.recordEvent({"mantle bound", npc, {}, {}, 0, 0, mantle, 0, 0, {}});
        }
        storyMantles_ = now;
        storyMantlesSeen_ = true;
    }
}

// ---- A region's count of the story's wolves (doc 85, 3.3: region occupied, region emptied; signing in inside counts).
void Game::storyRegionCrossed(const std::string& run, const std::string& region, const std::string& wolf, bool inside,
                              const std::string& cell, int x, int y)
{
    const auto* s = storylines_.find(run);
    if (!s || !s->live() || !s->takesPart(wolf))
        return;
    auto& in = storyOccupants_[run][region];
    const bool changed = inside ? in.insert(wolf).second : in.erase(wolf) > 0;
    if (!changed)
        return;
    if (inside && in.size() == 1 && world_.storyListens("region occupied"))
        world_.recordEvent({"region occupied", wolf, region, cell, 0, 0, {}, 1, 0, run, x, y});
    if (!inside && in.empty() && world_.storyListens("region emptied"))
        world_.recordEvent({"region emptied", wolf, region, cell, 0, 0, {}, 0, 0, run, x, y});
}

void Game::recountStoryRegions(const std::string& run)
{
    // (As the story opens a phase, or the server starts: who is inside now, with no event.)
    storyOccupants_.erase(run);
    const auto* s = storylines_.find(run);
    if (!s || !s->live() || s->regions.empty())
        return;
    for (const auto* c : clients_)
    {
        const auto* e = c->entityId.empty() ? nullptr : world_.entity(c->entityId);
        if (!e || e->npc || e->offstage || !s->takesPart(e->id))
            continue;
        const auto [x, y] = world_.eventTile(*e);
        for (const auto& [id, area] : s->regions)
            if (area.contains(e->cellId, x, y))
                storyOccupants_[run][id].insert(e->id);
    }
}

void Game::storyArrivedInside(const std::string& wolf)
{
    const auto* e = world_.entity(wolf);
    if (!e || e->npc || e->offstage)
        return;
    const auto [x, y] = world_.eventTile(*e);
    for (const auto& run : storiesWatchingRegions_)
    {
        const auto* s = storylines_.find(run);
        if (!s || !s->live())
            continue;
        for (const auto& [id, area] : s->regions)
            if (area.contains(e->cellId, x, y))
            {
                world_.recordEvent({"region entered", wolf, id, e->cellId, 0, 0, {}, 0, 0, run, x, y});
                storyRegionCrossed(run, id, wolf, true, e->cellId, x, y);
            }
    }
}

void Game::storyLeftWorld(const std::string& wolf)
{
    const auto* e = world_.entity(wolf);
    for (auto& [run, regions] : storyOccupants_)
        for (auto& [region, in] : regions)
            if (in.count(wolf))
                storyRegionCrossed(run, region, wolf, false, e ? e->cellId : std::string(), -1, -1);
}
} // namespace ratw::game
