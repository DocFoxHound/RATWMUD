// The world's facts and the watched events for live world stories (Docs/Design/85-story-triggers-and-campaign-control.md,
// Phase 8; Core/RatwGameStoryFacts.cpp). Through the game, on the town fixture's strip with an inn upstairs reached by a
// door and a locked gaol door: nothing is written while no story watches; a story's watch list (sent with story.phase)
// turns each watched event on, and each is recorded by the game where it happens (doors opened, closed and tried; going
// indoors and out; a region's first wolf in and last out, signing in inside counting; the weather in a watched place; the
// hour, dawn, nightfall, a new day and a new season; a trade; a dialogue window opened, moved on and closed; a party formed
// and broken up); the facts document has what the list names, as the world stands; and when the story ends it all stops.
// (A wolf downed and back up, an item dropped and picked up: battle_tests; a crime seen: crime_tests.)
#include "test_purse.h"
#include "RatwCalendar.h"
#include "RatwGame.h"
#include "RatwStorylines.h"
#include "RatwWorld.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unistd.h>

using namespace ratw;
namespace fs = std::filesystem;
namespace
{
int checks = 0;
void expect(bool condition, const std::string& message)
{
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}

#include "town_fixture.h"

// The strip, with Ridgemere's inn upstairs (c_up) behind a door from the first cell, and a locked gaol door beside it (in
// the manifest: a streamed cell's seams are its open boundaries only).
std::string writeDoors(const fs::path& root)
{
    const auto world = writeStrip(root, true);
    // (A door stands on a "+" tile at each end.)
    const auto doorTiles = [&](const std::string& cell, std::vector<std::pair<int, int>> tiles) {
        const auto path = root / "cells" / (cell + ".cell");
        std::stringstream text;
        text << std::ifstream(path).rdbuf();
        std::vector<std::string> lines;
        for (std::string line; std::getline(text, line);)
            lines.push_back(line);
        const auto grid = std::size_t(std::find(lines.begin(), lines.end(), "grid:") - lines.begin()) + 1;
        for (const auto& [x, y] : tiles)
            lines[grid + std::size_t(y)][std::size_t(x)] = '+';
        std::ofstream file(path);
        for (const auto& line : lines)
            file << line << '\n';
    };
    doorTiles(cellId(0), {{12, 6}, {3, 12}});
    doorTiles("c_up", {{1, 0}, {8, 5}});
    std::ofstream(root / "world.ratw", std::ios::app)
        << "door \"inn_door\" \"Inn door\" \"" << cellId(0) << "\" 12.5 6.5 \"c_up\" 1.5 1.5 \"inn_door_in\" 0 0 0 0 \"-\"\n"
        << "door \"gaol_door\" \"Gaol door\" \"" << cellId(0) << "\" 3.5 12.5 \"c_up\" 8.5 4.5 \"gaol_in\" 0 1 0 0 \"-\"\n"
        << "door \"inn_door_in\" \"Inn door\" \"c_up\" 1.5 0.5 \"" << cellId(0) << "\" 12.5 7.5 \"inn_door\" 0 0 0 0 \"-\"\n"
        << "door \"gaol_in\" \"Gaol door\" \"c_up\" 8.5 5.5 \"" << cellId(0) << "\" 3.5 13.5 \"gaol_door\" 0 1 0 0 \"-\"\n";
    return world;
}

const char* AllWatched[] = {"door opened", "door closed", "door locked", "building entered", "building left", "region occupied",
                            "region emptied", "weather changed", "hour", "day begins", "dawn", "night falls", "season begins",
                            "item picked up", "item dropped", "trade done", "dialogue opened", "dialogue node", "dialogue closed",
                            "crime seen", "wolf downed", "wolf revived", "party formed", "party ended"};

void factsAndWatchedEvents()
{
    const auto root = fs::temp_directory_path() / ("ratw-storyfacts-" + std::to_string(::getpid()));
    const auto save = root.string() + ".json";
    {
        Town t(writeDoors(root), save, 120, true);
        auto& w = t.g.world();
        t.enter(t.ash, 1, "ash", "Ash", 0, 12.5);
        t.enter(t.bo, 2, "bo", "Bo", 0, 14.5);
        t.enter(t.cy, 3, "cy", "Cy", 0, 8.5);
        std::vector<WorldEvent> seen;
        w.setEventWatcher([&](const WorldEvent& e) { seen.push_back(e); });
        const auto of = [&](const std::string& kind) {
            std::vector<WorldEvent> out;
            for (const auto& e : seen)
                if (e.kind == kind)
                    out.push_back(e);
            return out;
        };
        const auto act = [&](Client& c, const std::string& target, const std::string& action) {
            t.g.command(&c, cmd({{"type", "action"}, {"target", target}, {"action", action}}));
        };
        const auto at = [&](Client& c, const std::string& cell, double x, double y) {
            auto* e = w.entity(c.entityId);
            e->cellId = cell;
            e->position = {x, y};
            w.stop(c.entityId);
        };

        // Nobody watches: a locked door tried, a party made, a window opened: none of the watched events is written, and
        // there are no facts.
        at(t.ash, cellId(0), 3.5, 13.3);
        act(t.ash, "gaol_door", "open");
        t.g.command(&t.ash, cmd({{"type", "party"}, {"verb", "invite"}, {"target", t.bo.entityId}}));
        t.g.command(&t.bo, cmd({{"type", "party"}, {"verb", "accept"}}));
        t.run(1);
        t.g.command(&t.bo, cmd({{"type", "party"}, {"verb", "leave"}}));
        t.run(1);
        for (const auto* kind : AllWatched)
            expect(of(kind).empty(), std::string("unwatched: no ") + kind);
        expect(t.g.storyFactsJson().empty() && !t.g.storyWatches("door opened"), "no facts, nothing watched");

        // Cy steps out of the world inside where the gate region will be.
        at(t.cy, cellId(0), 8.5, 8.5);
        t.run(.5);
        const auto cyId = t.cy.entityId;
        t.g.disconnect(&t.cy);
        t.run(.5);

        // The story opens, watching every new event and every section of the facts.
        const auto run = "world-night-watch";
        auto phase = parsed(R"({"title":"The night watch","phase":"watch","journal":"Keep the night watch.","visit":1,
            "regions":[{"id":"gate","cell":"c_0_0","x1":6,"y1":6,"x2":10,"y2":10}],
            "watch":{"facts":["relations","residents","weather","wolves","world"],"items":["meal"],"towns":["ridgemere"],
                     "residents":["u1"],"cells":["c_0_0"],"weatherWhereWolves":true}})");
        auto players = json::Value::array();
        for (const auto& id : {t.ash.entityId, t.bo.entityId, cyId})
            players.push(id);
        phase.set("players", players);
        auto events = json::Value::array();
        for (const auto* kind : AllWatched)
            events.push(kind);
        auto watch = phase["watch"];
        watch.set("events", events);
        phase.set("watch", watch);
        expect(t.g.applyDmAct({"1", "story.phase", run, "dm", json::dump(phase)}).ok, "the story opens with its watch list");
        for (const auto* kind : AllWatched)
            expect(t.g.storyWatches(kind) && w.storyListens(kind), std::string("watched: ") + kind);
        auto bad = parsed(R"({"title":"X","phase":"x","journal":"X","visit":1,"watch":[1,2]})");
        bad.set("players", players);
        expect(!t.g.applyDmAct({"2", "story.phase", "world-bad", "dm", json::dump(bad)}).ok, "a watch list that isn't one is refused");

        // Doors: opened (and through it, indoors), out again, closed, a locked one tried.
        seen.clear();
        at(t.ash, cellId(0), 12.5, 7.9);
        act(t.ash, "inn_door", "open");
        auto opened = of("door opened");
        expect(opened.size() == 1 && opened[0].actor == t.ash.entityId && opened[0].target == "inn_door" && opened[0].cell == cellId(0) &&
                   opened[0].detail == "Inn door",
               "door opened: Ash, the inn door, where it is, its name");
        auto in = of("building entered");
        expect(w.entity(t.ash.entityId)->cellId == "c_up" && in.size() == 1 && in[0].target == "c_up" && in[0].detail == cellId(0),
               "through it, indoors: building entered, the room and where from");
        seen.clear();
        act(t.ash, "inn_door_in", "enter");
        auto out = of("building left");
        expect(w.entity(t.ash.entityId)->cellId == cellId(0) && out.size() == 1 && out[0].target == "c_up" && out[0].cell == cellId(0) &&
                   of("door opened").empty() && of("building entered").empty(),
               "out again: building left (the door was open already: no door opened)");
        seen.clear();
        at(t.ash, cellId(0), 12.5, 7.9);
        act(t.ash, "inn_door", "close");
        auto closed = of("door closed");
        expect(closed.size() == 1 && closed[0].target == "inn_door", "door closed: " + t.ash.said());
        act(t.ash, "inn_door", "close");
        expect(of("door closed").size() == 1, "a door already shut is not closed again");
        at(t.ash, cellId(0), 3.5, 13.3);
        act(t.ash, "gaol_door", "open");
        auto locked = of("door locked");
        expect(locked.size() == 1 && locked[0].actor == t.ash.entityId && locked[0].target == "gaol_door", "door locked: the gaol door tried");

        // A region's count of the story's wolves: the first in, the last out; signing in inside counts.
        seen.clear();
        at(t.ash, cellId(0), 8.5, 8.5);
        t.run(.5);
        auto occupied = of("region occupied");
        expect(of("region entered").size() == 1 && occupied.size() == 1 && occupied[0].actor == t.ash.entityId && occupied[0].target == "gate" &&
                   occupied[0].detail == run && occupied[0].quantity == 1 && occupied[0].x == 8 && occupied[0].y == 8,
               "Ash in: region entered, and occupied (the first)");
        at(t.bo, cellId(0), 9.5, 8.5);
        t.run(.5);
        expect(of("region entered").size() == 2 && of("region occupied").size() == 1, "Bo in: entered, not occupied again");
        at(t.ash, cellId(0), 13.5, 8.5);
        t.run(.5);
        expect(of("region left").size() == 1 && of("region emptied").empty(), "Ash out: Bo is still there");
        at(t.bo, cellId(0), 14.5, 9.5);
        t.run(.5);
        auto emptied = of("region emptied");
        expect(emptied.size() == 1 && emptied[0].actor == t.bo.entityId && emptied[0].quantity == 0, "Bo out last: region emptied");
        seen.clear();
        t.g.connect(&t.cy);
        t.g.command(&t.cy, cmd({{"type", "hello"}, {"id", "cy"}, {"name", "Cy"}}));
        auto entered = of("region entered");
        expect(w.entity(t.cy.entityId) && entered.size() == 1 && entered[0].actor == t.cy.entityId && entered[0].detail == run &&
                   of("region occupied").size() == 1,
               "Cy signs in inside the gate: region entered, and occupied");
        seen.clear();
        t.g.disconnect(&t.cy);
        expect(of("region emptied").size() == 1 && of("region left").empty(), "Cy signs out inside: emptied (no region left)");

        // The weather in a watched place: the first look makes nothing, a change does.
        t.run(2.5);
        seen.clear();
        const auto before = std::string(weatherName(w.weatherAt(cellId(0), {8, 8}).kind));
        expect(w.spawnFront(before == "storm" ? Weather::Snow : Weather::Storm, 8, 8, 60, 0, 6, true).ok, "a storm is called up over the first cell");
        t.run(2.5);
        auto weather = of("weather changed");
        expect(!weather.empty() && weather[0].target == cellId(0) && weather[0].item == before && weather[0].detail != before,
               "weather changed: the place, what it was, what it is (" + before + ")");

        // The calendar: an hour and dawn, nightfall, a new day with its weekday, a new season.
        const auto to = [&](double hour) {
            const double days = w.calendarDays();
            double next = std::floor(days) + hour / 24;
            if (next <= days)
                next += 1;
            w.advanceCalendar(next - days);
            t.run(.5);
        };
        to(4.95);
        seen.clear();
        to(5.05);
        auto hours = of("hour");
        expect(hours.size() == 1 && hours[0].quantity == 5 && hours[0].detail == "5" && of("dawn").size() == 1 && of("night falls").empty(),
               "five o'clock: the hour and dawn");
        to(18.95);
        seen.clear();
        to(19.05);
        expect(of("hour").size() == 1 && of("night falls").size() == 1, "seven: nightfall");
        to(23.95);
        seen.clear();
        to(0.05);
        auto day = of("day begins");
        const auto date = calendar::calendarAt(w.calendarDays());
        expect(day.size() == 1 && day[0].detail == calendar::weekdayName(calendar::weekdayOf(w.calendarDays())) &&
                   day[0].item == calendar::seasonName(date.season) && day[0].quantity == date.dayOfSeason && of("hour").size() == 1,
               "midnight: a new day, its weekday (" + (day.empty() ? std::string() : day[0].detail) + ")");
        {
            const int lengths[] = {92, 92, 91, 90};
            const int left = lengths[int(date.season)] - date.dayOfSeason + 1;
            seen.clear();
            w.advanceCalendar(left - 0.5);
            t.run(.5);
            w.advanceCalendar(0.6);
            t.run(.5);
            auto season = of("season begins");
            expect(season.size() == 1 && season[0].detail == calendar::seasonName(calendar::calendarAt(w.calendarDays()).season) &&
                       season[0].detail != calendar::seasonName(date.season),
                   "a new season: " + (season.empty() ? std::string("none") : season[0].detail));
        }

        // A trade with the stall's keeper at Ridgemere's market.
        {
            auto& s = w.society();
            const double now = w.calendarDays();
            w.advanceCalendar(std::floor(now) + 1 + 11. / 24 - now);   // (Late morning: the keeper at the stall.)
            t.run(2);
            const auto keep = [&] {
                at(t.ash, cellId(1), 10.3, 8.5);
                auto* um = w.entity("um");
                um->cellId = cellId(1);
                um->position = {9.5, 8.5};
                w.stop("um");
            };
            const auto till = s.tillOf("um");
            if (Society::stock(*s.account(till), "meal") < 10)
                s.create(till, "meal", 10, "test stock");
            for (int i = 0; i < 4; ++i)
            {
                keep();
                t.run(.25);
            }
            keep();
            seen.clear();
            const auto cash = s.cash(t.ash.entityId);
            t.g.command(&t.ash, cmd({{"type", "trade"}, {"target", "um"}, {"item", "meal"}, {"quantity", 2}, {"buy", true}}));
            auto trade = of("trade done");
            expect(trade.size() == 1 && trade[0].actor == t.ash.entityId && trade[0].target == "um" && trade[0].item == "meal" &&
                       trade[0].quantity == 2 && trade[0].detail == "bought" && trade[0].coins == cash - s.cash(t.ash.entityId) && trade[0].coins > 0,
                   "trade done: Ash bought two meals from the stall, and what they cost");
            keep();
            // The facts: Ash where she stands, the meals in her pack, her coins; the hour; the resident; the weather.
            json::Value facts;
            std::string error;
            expect(json::parse(t.g.storyFactsJson(), facts, error), "the facts are a document: " + error);
            const auto& ash = facts["wolves"][t.ash.entityId];
            expect(ash["cell"].asString() == cellId(1) && ash.number("x") == 10 && ash.number("y") == 8 && !ash.boolean("indoors") &&
                       ash.string("town") == "ridgemere" && ash["carries"].number("meal") >= 2 && ash.number("coins") == double(s.cash(t.ash.entityId)) &&
                       ash.number("partySize") == 1 && !ash.boolean("wanted") && !ash.boolean("downed"),
                   "Ash as the world stands: " + json::dump(ash));
            expect(!facts["wolves"].has(t.cy.entityId) && facts["wolves"].has(t.bo.entityId), "Bo is here; Cy, away, is not");
            const auto& world = facts["world"];
            const auto now2 = calendar::calendarAt(w.calendarDays());
            expect(world.number("hour") == std::floor(now2.hour) && world.string("weekday") == calendar::weekdayName(calendar::weekdayOf(w.calendarDays())) &&
                       world.string("season") == calendar::seasonName(now2.season) && world.string("period") == now2.period,
                   "the hour, the weekday, the season: " + json::dump(world));
            const auto& u1 = facts["residents"]["u1"];
            expect(u1.boolean("alive") && u1.string("cell") == w.entity("u1")->cellId && !u1.string("mood").empty() &&
                       u1.string("town") == "ridgemere",
                   "the resident named: " + json::dump(u1));
            expect(facts["weather"].has(cellId(0)) && facts["weather"].has(cellId(1)), "the weather where it is watched and where the wolves stand");
            expect(facts["relations"].isObject() && !facts.has("regions"), "the sections asked for, and no others");
        }

        // A dialogue window: opened, its node, closed.
        {
            w.setTimeOfDay(12);
            const auto keep = [&] {
                auto* n = w.entity("u1");
                n->cellId = cellId(1);
                n->position = {11.5, 9.5};
                w.stop("u1");
                at(t.ash, cellId(1), 10.5, 9.5);
            };
            for (int i = 0; i < 4; ++i)
            {
                keep();
                t.run(.25);
            }
            keep();
            seen.clear();
            t.g.command(&t.ash, cmd({{"type", "dialogue_open"}, {"npc", "u1"}, {"entry", "talk"}}));
            t.g.settle();
            auto opened2 = of("dialogue opened");
            auto nodes = of("dialogue node");
            expect(t.g.dialogueOf(t.ash.entityId) && opened2.size() == 1 && opened2[0].target == "u1" && opened2[0].detail == "talk" &&
                       !nodes.empty() && nodes[0].target == "u1" && !nodes[0].detail.empty() && !nodes[0].item.empty(),
                   "dialogue opened, and the node it opened on (" + (nodes.empty() ? std::string("none") : nodes[0].item + ":" + nodes[0].detail) + ")");
            t.g.command(&t.ash, cmd({{"type", "dialogue_close"}, {"session", t.g.dialogueOf(t.ash.entityId)->id}, {"why", "bye"}}));
            auto shut = of("dialogue closed");
            expect(shut.size() == 1 && shut[0].target == "u1", "dialogue closed");
        }

        // A party formed and broken up.
        at(t.bo, cellId(1), 11.5, 8.5);
        t.run(.5);
        seen.clear();
        t.g.command(&t.ash, cmd({{"type", "party"}, {"verb", "invite"}, {"target", t.bo.entityId}}));
        t.g.command(&t.bo, cmd({{"type", "party"}, {"verb", "accept"}}));
        t.run(1);
        auto formed = of("party formed");
        expect(formed.size() == 1 && formed[0].actor == t.ash.entityId && formed[0].target.rfind("party-", 0) == 0, "party formed: Ash leads " + t.ash.said() + t.bo.said());
        {
            json::Value facts;
            std::string error;
            json::parse(t.g.storyFactsJson(), facts, error);
            expect(facts["wolves"][t.ash.entityId].number("partySize") == 2 &&
                       facts["wolves"][t.ash.entityId].string("party") == facts["wolves"][t.bo.entityId].string("party"),
                   "the facts: one party of two");
        }
        t.g.command(&t.bo, cmd({{"type", "party"}, {"verb", "leave"}}));
        t.run(1);
        auto ended = of("party ended");
        expect(ended.size() == 1 && ended[0].target == formed[0].target && ended[0].actor == t.ash.entityId, "party ended");

        // Saved and loaded: the watch list is kept with the story.
        const auto kept = storylines::loadStoryline(storylines::saveStoryline(*t.g.storylineBook().find(run)));
        expect(kept.watch == t.g.storylineBook().find(run)->watch && kept.watch["events"].size() == 24, "the watch list is saved");

        // The story ends: nothing is watched, no facts.
        auto end = parsed(R"({"title":"The night watch","phase":"dawn","journal":"Dawn.","visit":2,"end":true})");
        end.set("players", players);
        expect(t.g.applyDmAct({"3", "story.phase", run, "dm", json::dump(end)}).ok, "the story ends");
        t.run(1.5);
        seen.clear();
        at(t.ash, cellId(0), 3.5, 13.3);
        act(t.ash, "gaol_door", "open");
        expect(of("door locked").empty() && !t.g.storyWatches("door locked") && t.g.storyFactsJson().empty(), "after the end: nothing watched");
    }
    fs::remove_all(root);
    fs::remove(save);
}
// Doc 85, Phase 10: the events Phase 8 left, each written only while a live story listens and only for what it names: a wolf
// come near a resident (and not again until they have been apart), a purse past an amount, a wolf's standing past a number,
// a faction's stance changed, a skill grown a step, an injury healed.
void phaseTenEvents()
{
    const auto root = fs::temp_directory_path() / ("ratw-storyfacts10-" + std::to_string(::getpid()));
    const auto save = root.string() + ".json";
    {
        Town t(writeDoors(root), save, 120, true);
        auto& w = t.g.world();
        t.enter(t.ash, 1, "ash", "Ash", 0, 12.5);
        t.enter(t.bo, 2, "bo", "Bo", 0, 14.5);
        t.enter(t.cy, 3, "cy", "Cy", 0, 8.5);
        std::vector<WorldEvent> seen;
        w.setEventWatcher([&](const WorldEvent& e) { seen.push_back(e); });
        const auto of = [&](const std::string& kind) {
            std::vector<WorldEvent> out;
            for (const auto& e : seen)
                if (e.kind == kind)
                    out.push_back(e);
            return out;
        };
        const auto at = [&](Client& c, const std::string& cell, double x, double y) {
            auto* e = w.entity(c.entityId);
            e->cellId = cell;
            e->position = {x, y};
            w.stop(c.entityId);
        };
        const auto keepU1 = [&] {
            auto* n = w.entity("u1");
            n->cellId = cellId(0);
            n->position = {20.5, 8.5};
            w.stop("u1");
        };
        // A faction, and a Chapter for Ash, Bo and Cy, so the faction can count their standing.
        auto& factions = t.g.factions();
        factions.define({"watch", "The Watch", "#336699", "order"});
        factions.define({"road", "The Road Company", "#996633", "guild"});
        auto& chapters = t.g.chapters();
        const double now = w.calendarDays();
        expect(chapters.propose(t.ash.entityId, {t.bo.entityId, t.cy.entityId}, "The Grey Lodge", "#556677", "", "the ford", now).ok, "a Chapter is proposed");
        chapters.agree(t.bo.entityId, now);
        chapters.agree(t.cy.entityId, now);
        const auto* lodge = chapters.of(t.ash.entityId);
        expect(lodge != nullptr, "the Chapter is founded");
        // The treasury the story watches, and what it holds.
        expect(!w.towns().empty(), "the strip has a town");
        const auto& town = w.towns().front();
        auto& society = w.society();
        if (!society.account(town.store) || society.account(town.store)->cash < 50)
            testPurse(society, town.store, 100);
        const auto held = society.account(town.store)->cash;

        // Nothing watched: none of them.
        at(t.ash, cellId(0), 19.5, 8.5);
        keepU1();
        factions.setRelation("watch", "road", {-40, "tense"});
        t.run(2.5);
        const char* Ten[] = {"near resident", "purse crossed", "standing crossed", "relation changed", "mantle bound", "mantle released",
                             "skill practised", "injury healed"};
        for (const auto* kind : Ten)
            expect(of(kind).empty(), std::string("unwatched: no ") + kind);

        // The story opens, watching them, with what its triggers name.
        const auto run = "world-the-ford";
        auto phase = parsed(R"({"title":"The ford","phase":"wait","journal":"Wait at the ford.","visit":1})");
        auto players = json::Value::array();
        for (const auto& id : {t.ash.entityId, t.bo.entityId, t.cy.entityId})
            players.push(id);
        phase.set("players", players);
        auto watch = json::Value::object();
        auto events = json::Value::array();
        for (const auto* kind : Ten)
            events.push(kind);
        watch.set("events", events);
        watch.set("near", parsed(R"(["u1"])"));
        auto purses = json::Value::array();
        auto purse = json::Value::array();
        purse.push("town:" + town.id);
        purse.push(double(held));
        purses.push(purse);
        watch.set("purses", purses);
        watch.set("standing", parsed(R"([["watch", 10]])"));
        phase.set("watch", watch);
        expect(t.g.applyDmAct({"1", "story.phase", run, "dm", json::dump(phase)}).ok, "the story opens with its watch list");
        expect(t.g.storyWatches("near resident") && w.storyListens("skill practised"), "watched");
        // First looks: nothing.
        seen.clear();
        at(t.ash, cellId(0), 30.5, 8.5);
        keepU1();
        t.run(2.5);
        for (const auto* kind : Ten)
            expect(of(kind).empty(), std::string("a first look makes nothing: ") + kind);

        // Near a resident: within three tiles, once; apart past five, and back: again.
        at(t.ash, cellId(0), 18.5, 8.5);
        keepU1();
        t.run(1.2);
        auto near = of("near resident");
        expect(near.size() == 1 && near[0].actor == t.ash.entityId && near[0].target == "u1" && near[0].cell == cellId(0) && near[0].quantity == 2,
               "near resident: Ash, two tiles from u1");
        at(t.ash, cellId(0), 16.5, 8.5);
        keepU1();
        t.run(1.2);
        expect(of("near resident").size() == 1, "four tiles off: not apart enough to meet again");
        at(t.ash, cellId(0), 19.5, 8.5);
        keepU1();
        t.run(1.2);
        expect(of("near resident").size() == 1, "and back: the same meeting");
        at(t.ash, cellId(0), 30.5, 8.5);
        keepU1();
        t.run(1.2);
        at(t.ash, cellId(0), 19.5, 8.5);
        keepU1();
        t.run(1.2);
        expect(of("near resident").size() == 2, "apart, then near again: a second");

        // A purse past its amount: down (a reward paid from it), then up again.
        seen.clear();
        auto grant = parsed(R"({"coins":5,"reason":"for the ford"})");
        grant.set("purse", "town:" + town.id);
        const auto paid = t.g.applyEconomyCommand("character.grant", t.bo.entityId, grant, "");
        expect(paid.ok, "a reward paid from the treasury: " + paid.message);
        t.run(2.5);
        auto crossed = of("purse crossed");
        expect(crossed.size() == 1 && crossed[0].target == "town:" + town.id && crossed[0].item == "down" &&
                   crossed[0].detail == std::to_string(held) && crossed[0].quantity == int(held - 5),
               "purse crossed: the treasury down past " + std::to_string(held));
        expect(society.shift(t.bo.entityId, town.store, "", 0, 5, "test: paid back"), "the treasury is paid back (" + town.store + ")");
        t.run(2.5);
        crossed = of("purse crossed");
        expect(crossed.size() == 2 && crossed[1].item == "up", "and up again");

        // Standing: the Chapter's with the Watch past 10, for each of its wolves in the world.
        seen.clear();
        factions.change("watch", lodge->id, 25, "kept the ford", w.calendarDays(), 0, false);
        t.run(2.5);
        auto standing = of("standing crossed");
        expect(standing.size() == 3 && standing[0].target == "watch" && standing[0].item == "up" && standing[0].detail == "10" &&
                   standing[0].quantity >= 10,
               "standing crossed: up past 10, for each of the three (" + std::to_string(standing.size()) + ")");

        // A stance changed.
        seen.clear();
        factions.setRelation("watch", "road", {-80, "hostile"});
        t.run(2.5);
        auto stance = of("relation changed");
        expect(stance.size() == 1 && stance[0].target == "watch" && stance[0].item == "road" && stance[0].detail == "hostile" &&
                   stance[0].quantity == -80,
               "relation changed: the Watch toward the Road Company, hostile");
        t.run(2.5);
        expect(of("relation changed").size() == 1, "once");

        // A skill grown a step, and an injury healed.
        seen.clear();
        auto* ash = w.entity(t.ash.entityId);
        const auto* sneak = practice::skill("sneak");
        double* slot = sneak ? World::practiceSlot(*ash, *sneak) : nullptr;
        expect(slot != nullptr, "a skill to grow");
        const auto& skill = *sneak;
        w.setPractice(t.ash.entityId, skill.id, std::floor(*slot) + 0.98, -1);
        for (int i = 0; i < 200 && of("skill practised").empty(); ++i)
        {
            World::PracticeContext context;
            context.amount = 1;
            context.occasion = "ambush-" + std::to_string(i);
            w.practise(t.ash.entityId, "sneak.ambush", context);
        }
        auto skilled = of("skill practised");
        expect(!skilled.empty() && skilled[0].actor == t.ash.entityId && skilled[0].item == skill.id && skilled[0].quantity == int(std::floor(*slot)),
               "skill practised: Ash's " + skill.id + " grows a step");
        expect(w.addInjury(t.bo.entityId, "torn_flank", 1, "", "test").ok, "Bo is hurt");
        w.healInjuries(*w.entity(t.bo.entityId), 24 * 120);
        auto healed = of("injury healed");
        expect(!healed.empty() && healed[0].actor == t.bo.entityId && !healed[0].detail.empty(), "injury healed: Bo's " +
               (healed.empty() ? std::string("none") : healed[0].detail));

        // The story ends: nothing more.
        auto end = parsed(R"({"title":"The ford","phase":"over","journal":"Over.","visit":2,"end":true})");
        end.set("players", players);
        expect(t.g.applyDmAct({"3", "story.phase", run, "dm", json::dump(end)}).ok, "the story ends");
        t.run(1.5);
        seen.clear();
        factions.setRelation("watch", "road", {0, "neutral"});
        at(t.ash, cellId(0), 30.5, 8.5);
        t.run(1.2);
        at(t.ash, cellId(0), 19.5, 8.5);
        keepU1();
        t.run(2.5);
        for (const auto* kind : Ten)
            expect(of(kind).empty(), std::string("after the end: no ") + kind);
    }
    fs::remove_all(root);
    fs::remove(save);
}

// Doc 86, Phase 2: the moon (J4) in the world's facts and as a watched event, and what a purse a story reads holds (M2).
void phaseTwoMoonAndPurses()
{
    const auto root = fs::temp_directory_path() / ("ratw-storyfacts-p2-" + std::to_string(::getpid()));
    const auto save = root.string() + ".json";
    {
        Town t(writeDoors(root), save, 120, true);
        auto& w = t.g.world();
        t.enter(t.ash, 1, "ash", "Ash", 0, 12.5);
        std::vector<WorldEvent> seen;
        w.setEventWatcher([&](const WorldEvent& e) { seen.push_back(e); });
        const auto of = [&](const std::string& kind) {
            std::vector<WorldEvent> out;
            for (const auto& e : seen)
                if (e.kind == kind)
                    out.push_back(e);
            return out;
        };
        expect(std::string(game::Game::moonQuarter(0.0)) == "new" && std::string(game::Game::moonQuarter(0.2)) == "waxing" &&
                   std::string(game::Game::moonQuarter(0.5)) == "full" && std::string(game::Game::moonQuarter(0.8)) == "waning" &&
                   std::string(game::Game::moonQuarter(0.97)) == "new",
               "the moon in four, by the octant of its name");
        // Day 88.6 is a new moon, 103.4 a full one (a round of 29.53 days from Spring 1's new moon).
        const auto toDay = [&](double day) {
            const double now = w.calendarDays();
            const double cycles = now > day ? std::ceil((now - day) / calendar::LunarCycleDays) : 0;
            w.advanceCalendar(day + cycles * calendar::LunarCycleDays - now);
            t.run(.5);
        };
        toDay(88.6);
        expect(calendar::calendarAt(w.calendarDays()).moonName == "new moon", "a new moon");
        const auto& town = w.towns().front();
        auto& society = w.society();
        if (!society.account(town.store) || society.account(town.store)->cash < 50)
            testPurse(society, town.store, 212);
        const auto run = "world-the-moon";
        auto phase = parsed(R"({"title":"The moon","phase":"wait","journal":"Wait for the moon.","visit":1})");
        auto players = json::Value::array();
        players.push(t.ash.entityId);
        phase.set("players", players);
        auto watch = parsed(R"({"events":["moon"],"facts":["world","purses"]})");
        auto held = json::Value::array();
        held.push("town:" + town.id);
        held.push("till:job:nobody_here");
        watch.set("purseFacts", held);
        phase.set("watch", watch);
        expect(t.g.applyDmAct({"1", "story.phase", run, "dm", json::dump(phase)}).ok, "the story opens watching the moon and a purse");
        seen.clear();
        t.run(2.5);
        expect(of("moon").empty(), "the first look makes no event");
        json::Value facts;
        std::string error;
        expect(json::parse(t.g.storyFactsJson(), facts, error), "the facts are a document: " + error);
        expect(facts["world"].string("moon") == "new", "the world's facts have the moon: " + json::dump(facts["world"]));
        expect(facts["purses"].number("town:" + town.id) == double(society.account(town.store)->cash) &&
                   !facts["purses"].has("till:job:nobody_here"),
               "the purse read holds what the treasury holds; one with no account is left out: " + json::dump(facts["purses"]));
        toDay(103.4);
        t.run(2.5);
        auto moon = of("moon");
        expect(moon.size() == 1 && moon[0].detail == "full" && moon[0].quantity == 14, "the moon turns full: one event, day 14 of its round (" +
               (moon.empty() ? std::string("none") : moon[0].detail + " " + std::to_string(moon[0].quantity)) + ")");
        t.run(2.5);
        expect(of("moon").size() == 1, "once per change");
        expect(json::parse(t.g.storyFactsJson(), facts, error) && facts["world"].string("moon") == "full", "and the facts say full");
        auto end = parsed(R"({"title":"The moon","phase":"over","journal":"Over.","visit":2,"end":true})");
        end.set("players", players);
        expect(t.g.applyDmAct({"2", "story.phase", run, "dm", json::dump(end)}).ok, "the story ends");
        t.run(1.5);
        seen.clear();
        toDay(118.2);
        t.run(2.5);
        expect(of("moon").empty(), "after the end: no moon");
    }
    fs::remove_all(root);
    fs::remove(save);
}
} // namespace

int main()
{
    try
    {
        factsAndWatchedEvents();
        phaseTenEvents();
        phaseTwoMoonAndPurses();
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAILED after " << checks << " checks: " << e.what() << "\n";
        return 1;
    }
    std::cout << "story facts tests passed (" << checks << " checks)\n";
    return 0;
}
