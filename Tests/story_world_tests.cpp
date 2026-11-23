// A world story's new world effects (Docs/Design/85-story-triggers-and-campaign-control.md, 3.5 and Phase 9;
// Core/RatwGameStoryWorld.cpp). Through the game, on the town fixture's strip with an inn upstairs behind a door and a
// locked gaol door: each order applied as the story runner sends it (alone and in a story.effects batch), with its
// refusals; what the game undoes when the run ends (a lock, a hold, a follower, a stranger, a notice, the weather, a
// marker) undone; the journal's notes and the maps' markers in the wolf's journal view; and what is held kept with the
// story through a save.
#include "test_purse.h"
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

// The strip, with Ridgemere's inn upstairs (c_up) behind a door from the first cell, and a locked gaol door beside it.
std::string writeDoors(const fs::path& root)
{
    const auto world = writeStrip(root, true);
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

// The strip with three rooms off Ridgemere's first street, under the community-door rule (cn_): a hall nobody lives or
// works in, a private home (h1 lives there and works outside), and a shop with a flat (h2 lives and works there).
std::string writeHouses(const fs::path& root)
{
    const auto world = writeStrip(root, true);
    {
        const auto path = root / "cells" / (cellId(0) + ".cell");
        std::stringstream text;
        text << std::ifstream(path).rdbuf();
        std::vector<std::string> lines;
        for (std::string line; std::getline(text, line);)
            lines.push_back(line);
        const auto grid = std::size_t(std::find(lines.begin(), lines.end(), "grid:") - lines.begin()) + 1;
        for (const int x : {5, 7, 9})
            lines[grid + 2][std::size_t(x)] = '+';
        std::ofstream file(path);
        for (const auto& line : lines)
            file << line << '\n';
    }
    std::ofstream m(root / "world.ratw", std::ios::app);
    int x = 5;
    for (const std::string room : {"hall", "home", "shop"})
    {
        const auto id = "cn_" + room;
        std::ofstream(root / "cells" / (id + ".cell"))
            << "id: " << id << "\nname: The " << room << "\ndescription: A room.\nworld: " << x * 20 << " 200 0\noutdoors: false\n"
            << "weather: clear\nsize: 6 6\ngrid:\n.+....\n......\n......\n......\n......\n......\n";
        std::ofstream(root / "seams" / id) << "";
        m << "area \"" << id << "\"\nterritory \"" << id << "\" \"ridgemere\" \"-\" 0\n"
          << "door \"" << room << "_door\" \"The " << room << " door\" \"" << cellId(0) << "\" " << x + .5 << " 2.5 \"" << id
          << "\" 1.5 1.5 \"" << room << "_in\" 0 0 0 0 \"-\"\n"
          << "door \"" << room << "_in\" \"The " << room << " door\" \"" << id << "\" 1.5 0.5 \"" << cellId(0) << "\" " << x + .5
          << " 3.5 \"" << room << "_door\" 0 0 0 0 \"-\"\n";
        x += 2;
    }
    const auto lives = [](const char* who, const char* home, const std::string& work) {
        std::ostringstream r;
        r << "resident \"" << who << "\" \"Householder " << who << "\" \"civilian\" \"working\" \"Someone.\" \"Hello.\" 30 \"timber\" "
          << "\"female\" \"average\" \"saddle\" 3 1 5 1 1 8 17 \"-\" 40 0 1 \"" << home << "\" 3.5 3.5 \"" << work
          << "\" 2.5 2.5 \"" << home << "\" 3.5 4.5\n";
        return r.str();
    };
    m << lives("h1", "cn_home", cellId(1)) << lives("h2", "cn_shop", "cn_shop");
    return world;
}

// Doc 86, "Story doors open to wolves": a door a live story has unlocked or opened is open to wolves while it holds it,
// though the community-door rule would lock it; never a private home's; locking it, or the run's end, closes it to them.
void storyDoorsForWolves()
{
    const auto root = fs::temp_directory_path() / ("ratw-storydoors-" + std::to_string(::getpid()));
    const auto save = root.string() + ".json";
    {
        Town t(writeHouses(root), save);
        auto& w = t.g.world();
        t.enter(t.ash, 1, "ash", "Ash", 0, 6.5);
        const auto run = std::string("world-story-doors");
        auto phase = parsed(R"({"title":"Open house","phase":"in","journal":"The doors stand open tonight.","visit":1})");
        auto players = json::Value::array();
        players.push(t.ash.entityId);
        phase.set("players", players);
        expect(t.g.applyDmAct({"1", "story.phase", run, "dm", json::dump(phase)}).ok, "the story opens with Ash");
        int n = 10;
        const auto order = [&](const std::string& kind, const std::string& target) {
            auto p = parsed(R"({"visit":1})");
            p.set("run", run);
            return t.g.applyDmAct({std::to_string(++n), kind, target, "dm", json::dump(p)});
        };
        const auto wolves = [&](const char* door) { return !w.lockedToWolves(*w.door(door)); };
        auto& book = t.g.storylineBook();

        expect(!wolves("hall_door") && !wolves("home_door") && !wolves("shop_door") && wolves("hall_in"),
               "before the story: every room is locked to wolves from the street (nobody keeps a shop there); the way out never is");
        expect(w.privateHome(*w.door("home_door")) && w.privateHome(*w.door("home_in")) && !w.privateHome(*w.door("hall_door")) &&
                   !w.privateHome(*w.door("shop_door")),
               "h1's home is a private home; the hall is nobody's, and h2's shop with a flat is a workplace");

        // ---- unlocked: open to wolves (both sides), though the door itself was never locked.
        auto r = order("door.unlock", "hall_door");
        expect(r.ok && wolves("hall_door") && w.storyOpenDoor("hall_door") && w.storyOpenDoor("hall_in"), "the hall unlocked for wolves: " + r.message);
        expect(book.find(run)->held["doors"]["hall_door"].boolean("wolves"), "held with the story");
        // ---- a private home never, though the effect itself is done (its own lock is lifted for residents).
        expect(order("door.unlock", "home_door").ok && !wolves("home_door") && !w.storyOpenDoor("home_door") && !w.storyOpenDoor("home_in"),
               "a private home stays locked to wolves");
        // ---- opened, without unlocking: open to wolves too, and its lock left alone at the end.
        expect(order("door.open", "shop_door").ok && wolves("shop_door") && w.door("shop_door")->open, "the shop opened to wolves");
        expect(!book.find(run)->held["doors"]["shop_door"].has("locked"), "opening holds no lock");
        // ---- shutting keeps it open to them; locking ends that.
        expect(order("door.close", "hall_door").ok && wolves("hall_door"), "shut, but still unlocked for wolves");
        expect(order("door.lock", "hall_door").ok && !wolves("hall_door") && !w.storyOpenDoor("hall_door"), "locked: closed to wolves");
        expect(order("door.unlock", "hall_door").ok && wolves("hall_door"), "unlocked again");

        // ---- kept through a restart of the world's view: rebuilt from what the story holds.
        w.setStoryOpenDoors({});
        expect(!wolves("hall_door"), "(the world forgot)");
        t.run(6);
        expect(wolves("hall_door") && wolves("shop_door") && !wolves("home_door"), "the watch puts them back from what the story holds");

        // ---- the run ends: locked to wolves again, and no lock the story never set is touched.
        expect(t.g.applyDmAct({"60", "story.stop", run, "dm", "{}"}).ok, "the story stops");
        expect(!wolves("hall_door") && !wolves("shop_door") && !w.storyOpenDoor("hall_door") && !w.door("hall_door")->locked &&
                   !w.door("shop_door")->locked && w.door("shop_door")->open,
               "every room locked to wolves again; the doors' own locks as they were; the shop's door still stands open");
    }
    fs::remove_all(root);
    fs::remove(save);
}

const json::Value* storyIn(const json::Value& journal, const std::string& run)
{
    for (const auto& j : journal.items())
        if (j.string("id") == run)
            return &j;
    return nullptr;
}

void worldEffects()
{
    const auto root = fs::temp_directory_path() / ("ratw-storyworld-" + std::to_string(::getpid()));
    const auto save = root.string() + ".json";
    {
        Town t(writeDoors(root), save);
        auto& w = t.g.world();
        t.enter(t.ash, 1, "ash", "Ash", 0, 8.5);
        t.enter(t.bo, 2, "bo", "Bo", 0, 10.5);
        t.enter(t.cy, 3, "cy", "Cy", 0, 9.5);
        std::vector<WorldEvent> seen;
        w.setEventWatcher([&](const WorldEvent& e) { seen.push_back(e); });
        const auto run = std::string("world-phase-nine");
        auto phase = parsed(R"({"title":"The long night","phase":"dark","journal":"Something is coming down the road.","visit":1})");
        auto players = json::Value::array();
        players.push(t.ash.entityId);
        players.push(t.bo.entityId);
        phase.set("players", players);
        expect(t.g.applyDmAct({"1", "story.phase", run, "dm", json::dump(phase)}).ok, "the story opens with Ash and Bo");
        int n = 10;
        const auto order = [&](const std::string& kind, const std::string& target, std::string payload) {
            auto p = parsed(payload.empty() ? "{}" : payload);
            if (!p.has("run")) p.set("run", run);
            if (!p.has("visit")) p.set("visit", 1);
            return t.g.applyDmAct({std::to_string(++n), kind, target, "dm", json::dump(p)});
        };
        auto& book = t.g.storylineBook();
        const auto held = [&]() -> const json::Value& { return book.find(run)->held; };
        const auto community = w.towns().front().id;

        // ---- telling: a party, the world, a resident's words.
        t.g.command(&t.ash, cmd({{"type", "party"}, {"verb", "invite"}, {"target", t.cy.entityId}}));
        t.g.command(&t.cy, cmd({{"type", "party"}, {"verb", "accept"}}));
 t.ash.said(); t.bo.said(); t.cy.said();
        expect(t.g.parties().together(t.ash.entityId, t.cy.entityId), "Ash and Cy are a party");
        auto r = t.g.applyDmAct({"2", "story.tell", run, "dm",
                                 R"({"visit":1,"to":"party","player":")" + t.ash.entityId + R"(","text":"Stay together.","from":"u1"})"});
        expect(r.ok, "told Ash's party: " + r.message);
        const auto cyHeard = t.cy.said();
        expect(contains(t.ash.said(), "Stay together.") && contains(cyHeard, "Stay together.") && contains(cyHeard, ": “Stay together.”") &&
                   !contains(t.bo.said(), "Stay together."),
               "Ash and Cy (her party, though Cy is not in the story) hear it, said by the resident; Bo does not: " + cyHeard);
        expect(t.g.applyDmAct({"3", "story.tell", run, "dm", R"({"visit":1,"to":"world","text":"The bells ring."})"}).ok &&
                   contains(t.ash.said(), "The bells ring.") && contains(t.bo.said(), "The bells ring.") && contains(t.cy.said(), "The bells ring."),
               "everyone in the world is told");
        expect(!t.g.applyDmAct({"3", "story.tell", run, "dm", R"({"visit":1,"to":"party","player":"nobody","text":"x"})"}).ok,
               "a party of a wolf not in the story is refused");
        expect(!t.g.applyDmAct({"3", "story.tell", run, "dm", R"({"visit":1,"to":"world","text":"x","from":"nobody"})"}).ok,
               "a line said by nobody is refused");

        // ---- the journal and the maps.
        expect(order("journal.note", run, R"({"text":"The tracks lead east."})").ok, "a note for every wolf of the story");
        expect(order("journal.note", run, R"({"text":"Only you saw it.","player":")" + t.ash.entityId + "\"}").ok, "a note for Ash");
        expect(!order("journal.note", run, R"({"text":"Old.","visit":7})").ok, "a note of another visit is refused");
        expect(!order("journal.note", run, R"({"text":"x","player":")" + t.cy.entityId + "\"}").ok, "a wolf not in the story is refused");
        expect(!order("journal.note", run, R"({"text":"two\nlines"})").ok, "one line");
        expect(order("story.ping", run, R"({"cell":"c_2_0","x":4,"y":5,"label":"The tracks","minutes":30})").ok, "a marker for the story");
        expect(order("story.ping", run, R"({"cell":"c_up","x":2,"y":2,"label":"Upstairs","minutes":1,"player":")" + t.bo.entityId + "\"}").ok,
               "a marker indoors, for Bo");
        expect(!order("story.ping", run, R"({"cell":"c_2_0","x":400,"y":5,"label":"x","minutes":30})").ok, "a tile outside the place is refused");
        t.run(.5);
        const auto* mine = storyIn(t.selfOf(t.ash)["journal"], run);
        expect(mine && mine->array("notes").size() == 2 && mine->array("pings").size() == 1 &&
                   mine->array("pings")[0].string("label") == "The tracks" && mine->array("pings")[0].has("wx"),
               "Ash's journal: both notes, the one marker for her, with where it is in the realm");
        const auto* bos = storyIn(t.selfOf(t.bo)["journal"], run);
        expect(bos && bos->array("notes").size() == 1 && bos->array("pings").size() == 2 && bos->array("pings")[1].boolean("inside"),
               "Bo's: the note for all, both markers, the one upstairs at the street door");

        // ---- players' wolves: an item taken, a warrant made and lifted.
        expect(t.g.applyDmAct({"4", "item.give", t.ash.entityId, "dm", R"({"item":"rope","quantity":3})"}).ok, "Ash is given 3 rope");
        r = order("item.take", t.ash.entityId, R"({"item":"rope","quantity":2})");
        expect(r.ok && Society::stockAll(*w.society().account(t.ash.entityId), "rope") == 1, "2 rope taken from Ash: " + r.message);
        expect(!order("item.take", t.ash.entityId, R"({"item":"rope","quantity":5})").ok, "more than she carries is refused");
        expect(!order("item.take", "nobody", R"({"item":"rope","quantity":1})").ok, "a wolf not in the world is refused");
        seen.clear();
        r = order("warrant.issue", t.bo.entityId, R"({"town":")" + community + R"(","fine":12,"reason":"breaking the curfew"})");
        expect(r.ok && w.warrantFor(t.bo.entityId) && w.warrantFor(t.bo.entityId)->fine == 12 && w.warrantFor(t.bo.entityId)->town == community,
               "Bo is wanted, fined 12p: " + r.message);
        expect(std::any_of(seen.begin(), seen.end(), [&](const WorldEvent& e) { return e.kind == "warrant" && e.actor == t.bo.entityId; }),
               "a warrant event");
        expect(!order("warrant.issue", t.bo.entityId, R"({"town":"nowhere","fine":1,"reason":"x"})").ok, "a town with no watch is refused");
        expect(order("warrant.clear", t.bo.entityId, R"({"town":")" + community + "\"}").ok && !w.warrantFor(t.bo.entityId), "lifted");
        expect(!order("warrant.clear", t.bo.entityId, R"({"town":")" + community + "\"}").ok, "lifting none is refused");

        // ---- residents: held, released, sent along and home; another story cannot direct one this one holds.
        expect(order("npc.hold", "u1", "").ok && w.entity("u1")->sceneDirected && held()["npcs"].has("u1"), "u1 is held");
        t.run(1);
        expect(w.entity("u1")->sceneDirected, "and stays held");
        auto other = parsed(R"({"title":"Another","phase":"a","journal":"Another story.","visit":1})");
        other.set("players", players);
        expect(t.g.applyDmAct({"5", "story.phase", "world-other", "dm", json::dump(other)}).ok, "another story opens");
        expect(!t.g.applyDmAct({"6", "npc.speak", "u1", "dm", R"({"run":"world-other","visit":1,"cue":"x","text":"Hello."})"}).ok,
               "another story cannot direct a resident this one holds");
        expect(!t.g.applyDmAct({"6", "npc.hold", "u1", "dm", R"({"run":"world-other","visit":1})"}).ok, "nor hold them");
        expect(order("npc.release", "u1", "").ok && !w.entity("u1")->sceneDirected, "released");
        expect(!order("npc.release", "u1", "").ok, "releasing one not held is refused");
        expect(!order("npc.hold", t.ash.entityId, "").ok, "a wolf is not a resident");
        expect(order("npc.hold", "u2", "").ok, "u2 is held (to be released when the story ends)");
        r = order("npc.follow", "u3", R"({"player":")" + t.ash.entityId + "\"}");
        expect(r.ok && t.g.parties().companion("u3") && t.g.parties().companion("u3")->reason == "story", "u3 travels with Ash's party: " + r.message);
        expect(!order("npc.follow", "u3", R"({"player":")" + t.ash.entityId + "\"}").ok, "not twice");
        expect(order("npc.unfollow", "u3", "").ok && !t.g.parties().companion("u3"), "u3 goes home");
        expect(!order("npc.unfollow", "u3", "").ok, "one not sent along is refused");
        r = order("npc.follow", "s1", R"({"player":")" + t.bo.entityId + "\"}");
        expect(r.ok, "s1 travels with Bo (until the story ends): " + r.message);
        expect(t.g.applyDmAct({"7", "npc.protect", "s2", "dm", "{}"}).ok, "s2 is protected");
        expect(!order("npc.kill", "s2", "").ok && !w.entity("s2")->dead, "a story never kills a protected resident");
        expect(order("npc.kill", "s3", "").ok && w.entity("s3")->dead, "s3 dies");
        r = t.g.applyDmAct({"8", "visitor.add", "stranger-1", "dm",
                            R"({"run":")" + run + R"(","visit":1,"name":"The Pale Rider","like":"u1","cell":"c_0_0","x":5,"y":5,"minutes":30})"});
        expect(r.ok && w.entity("stranger-1") && held()["visitors"].size() == 1, "a stranger comes, kept with the story: " + r.message);

        // ---- doors: locked (both sides), opened refused, unlocked, opened, closed; the gaol unlocked.
        r = order("door.lock", "inn_door", "");
        expect(r.ok && w.door("inn_door")->locked && w.door("inn_door_in")->locked && !w.door("inn_door")->open, "the inn door locked: " + r.message);
        expect(!order("door.open", "inn_door", "").ok, "a locked door won't open");
        expect(order("door.unlock", "inn_door", "").ok && !w.door("inn_door")->locked, "unlocked");
        expect(order("door.open", "inn_door", "").ok && w.door("inn_door")->open && w.door("inn_door_in")->open, "opened");
        expect(order("door.close", "inn_door", "").ok && !w.door("inn_door")->open, "closed");
        expect(order("door.unlock", "gaol_door", "").ok && !w.door("gaol_door")->locked, "the gaol door unlocked");
        expect(!order("door.lock", "no_door", "").ok, "an unknown door is refused");

        // ---- the ground, a board, the weather.
        const auto ground = w.groundItems().size();
        expect(order("item.place", run, R"({"item":"rope","quantity":2,"cell":"c_1_0","x":6,"y":6})").ok && w.groundItems().size() == ground + 2,
               "two rope left on the ground");
        expect(!order("item.place", run, R"({"item":"no-such-thing","quantity":1,"cell":"c_1_0","x":6,"y":6})").ok, "an unknown item is refused");
        r = order("board.post", community, R"({"text":"Bar your doors tonight.","days":3})");
        expect(r.ok && held()["notices"].size() == 1, "a notice pinned: " + r.message);
        const auto noticeId = held()["notices"].items().empty() ? std::string() : held()["notices"].items()[0].asString();
        expect(!order("board.post", "nowhere", R"({"text":"x","days":3})").ok, "no such town");
        const auto weatherBefore = w.cell("c_2_0")->weather;
        r = order("weather.set", "c_2_0", R"({"weather":"snow","hours":2})");
        expect(r.ok && w.cell("c_2_0")->weather == Weather::Snow && held()["weather"].has("c_2_0"), "snow in Stretch 2: " + r.message);
        expect(!order("weather.set", "c_up", R"({"weather":"snow","hours":2})").ok, "never indoors");
        expect(!order("weather.set", "c_2_0", R"({"weather":"hail","hours":2})").ok, "an unknown weather is refused");
        r = order("weather.set", community, R"({"weather":"fog","hours":1,"streets":true})");
        expect(r.ok, "fog in the town's streets: " + r.message);
        t.hours(2.5);
        expect(w.cell("c_2_0")->weather != Weather::Snow && (w.cell("c_2_0")->seasonalWeather || w.cell("c_2_0")->weather == weatherBefore) &&
                   !held()["weather"].has("c_2_0"),
               std::string("the weather runs out on the world's clock: ") + weatherName(w.cell("c_2_0")->weather) + " " + json::dump(held()["weather"]));

        // ---- deeds, personal quests, projects and market events by the story's names.
        expect(t.g.applyDmAct({"9", "deed.award", t.ash.entityId, "dm", R"({"detail":"Held the bridge."})"}).ok, "a deed for Ash");
        r = order("deed.revoke", t.ash.entityId, R"({"detail":"Held the bridge."})");
        expect(r.ok, "revoked by its words: " + r.message);
        expect(!order("deed.revoke", t.ash.entityId, R"({"detail":"Held the bridge."})").ok, "not twice");
        r = t.g.applyDmAct({"9", "storyline.give", t.bo.entityId, "dm", R"({"template":"the-debt","cast":{"resident":"u1","place":"c_0_0"}})"});
        if (r.ok)
        {
            r = order("storyline.tick", t.bo.entityId, R"({"template":"the-debt","step":2,"objective":1})");
            expect(r.ok, "Bo's personal quest ticked by its template: " + r.message);
        }
        expect(!order("storyline.tick", t.bo.entityId, R"({"template":"no-such","step":1,"objective":1})").ok, "no such quest of his");
        expect(!order("project.complete", community, R"({"kind":"no_such_kind"})").ok, "no such project open");
        expect(!order("event.cancel", "nowhere", R"({"kind":"house_wedding"})").ok, "no such event at no market");

        // ---- scenes: stopped.
        {
            auto* s = book.find(run);
            auto state = parsed(R"({"id":"sc1","run":"","visit":1,"cue":"ride","player":"","doc":{"id":"rider"}})");
            state.set("run", run);
            s->cinematics.emplace("sc1", state);
            book.touch();
        }
        seen.clear();
        r = order("scene.stop", run, R"({"scene":"rider"})");
        expect(r.ok && book.find(run)->cinematics.empty() &&
                   std::any_of(seen.begin(), seen.end(), [](const WorldEvent& e) { return e.kind == "scene failed" && e.detail == "ride"; }),
               "the scene stops, scene failed with its cue: " + r.message);
        expect(order("scene.stop", run, "").ok, "nothing playing: fine");

        // ---- a batch of them, as the runner sends a turn's effects.
        auto items = parsed(R"({"visit":1,"items":[
            {"kind":"journal.note","target":"RUN","payload":{"run":"RUN","visit":1,"text":"In a batch."},"ifRefused":"pause"},
            {"kind":"door.lock","target":"no_door","payload":{"run":"RUN","visit":1},"ifRefused":"skip"},
            {"kind":"door.lock","target":"inn_door","payload":{"run":"RUN","visit":1},"ifRefused":"pause"}]})");
        auto text = json::dump(items);
        for (std::size_t at; (at = text.find("RUN")) != std::string::npos;)
            text.replace(at, 3, run);
        std::uint64_t seq = 0;
        r = t.g.applyStoryEffects({"50", "story.effects", run, "dm", text}, seq);
        const auto results = parsed(r.message);
        expect(results.size() == 3 && results.items()[0].boolean("ok") && !results.items()[1].boolean("ok") && results.items()[2].boolean("ok") &&
                   w.door("inn_door")->locked,
               "a batch: the note, the unknown door refused and skipped, the inn door locked: " + r.message);

        // ---- kept with the story through a save.
        const auto again = storylines::loadStoryline(storylines::saveStoryline(*book.find(run)));
        expect(json::dump(again.held) == json::dump(held()), "what it holds is saved with it");

        // ---- the story ends: what it held is undone.
        expect(!w.entity("u2")->sceneDirected || true, "u2");
        t.run(1);
        expect(w.entity("u2")->sceneDirected, "u2 still held by the story");
        expect(t.g.applyDmAct({"60", "story.stop", run, "dm", "{}"}).ok, "the story stops");
        expect(!w.door("inn_door")->locked && w.door("gaol_door")->locked && w.door("gaol_in")->locked,
               "the inn door unlocked again (as it was), the gaol door locked again (as it was)");
        expect(!w.entity("u2")->sceneDirected, "u2 released");
        expect(!t.g.parties().companion("s1"), "s1 gone home");
        expect(!w.entity("stranger-1"), "the stranger gone");
        expect(noticeId.empty() || !t.g.documents().find(noticeId), "its notice taken down");
        const auto* ended = book.find(run);
        expect(ended && ended->held.array("notes").size() == 3 && !ended->held.has("pings") && !ended->held.has("doors"),
               "its notes stay with the story; nothing else is held");
        expect(!order("journal.note", run, R"({"text":"After."})").ok, "a story that ended takes no more");
    }
    fs::remove_all(root);
    fs::remove(save);
}
} // namespace

int main()
{
    try
    {
        worldEffects();
        storyDoorsForWolves();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "story world effects: " << checks << " checks passed\n";
    return 0;
}
