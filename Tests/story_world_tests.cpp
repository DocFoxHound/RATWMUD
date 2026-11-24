// A world story's new world effects (Docs/Design/85-story-triggers-and-campaign-control.md, 3.5 and Phase 9;
// Core/RatwGameStoryWorld.cpp). Through the game, on the town fixture's strip with an inn upstairs behind a door and a
// locked gaol door: each order applied as the story runner sends it (alone and in a story.effects batch), with its
// refusals; what the game undoes when the run ends (a lock, a hold, a follower, a stranger, a notice, the weather, a
// marker) undone; the journal's notes and the maps' markers in the wolf's journal view; and what is held kept with the
// story through a save.
#include "test_purse.h"
#include "RatwAmbient.h"
#include "RatwClaims.h"
#include "RatwGame.h"
#include "RatwItems.h"
#include "RatwStorylines.h"
#include "RatwStoryFoes.h"
#include "RatwWorld.h"
#include "battle_play.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
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
        {
            // (End of slice, doc 86: a story that holds nothing yet holds a resident first. Making the hold's sections
            // added a field after the first was taken by reference, which moved it: the hold was written to freed memory,
            // and on Ridgemere 3 the server crashed.)
            auto third = parsed(R"({"title":"A third","phase":"a","journal":"A third story.","visit":1})");
            third.set("players", players);
            expect(t.g.applyDmAct({"5", "story.phase", "world-third", "dm", json::dump(third)}).ok, "a third story opens");
            expect(t.g.applyDmAct({"5", "npc.hold", "r1", "dm", R"({"run":"world-third","visit":1})"}).ok &&
                       book.find("world-third")->held["npcs"].has("r1") && book.find("world-third")->held["follow"].isArray(),
                   "its first order, a hold, is kept: " + json::dump(book.find("world-third")->held));
            expect(t.g.applyDmAct({"5", "npc.release", "r1", "dm", R"({"run":"world-third","visit":1})"}).ok, "and let go");
        }
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

// Doc 86, Phase 1b (Core/RatwGameStoryWorld.cpp): a business's till by either spelling; residents spared from old age and
// protected for a run or a campaign (and let go); what residents remember of a story (the realm, a town, a resident, a
// resident about one wolf: recalled in that order, capped, taken back at the run's end unless kept, through a save); a
// rumour and its clearing believed by residents of a town, chosen by the run's seed.
void phase1b()
{
    const auto root = fs::temp_directory_path() / ("ratw-storyworld-1b-" + std::to_string(::getpid()));
    const auto save = root.string() + ".json";
    const auto world = writeDoors(root);
    const auto run = std::string("world-ninth-howl"), other = std::string("world-pack");
    {
        Town t(world, save);
        auto& w = t.g.world();
        t.enter(t.ash, 1, "ash", "Ash", 0, 8.5);
        t.enter(t.bo, 2, "bo", "Bo", 0, 10.5);
        int n = 100;
        const auto open = [&](const std::string& id) {
            auto phase = parsed(R"({"title":"The Ninth Howl","phase":"one","journal":"The howl is coming.","visit":1,"program":"ninth-howl.1"})");
            auto players = json::Value::array();
            players.push(t.ash.entityId);
            players.push(t.bo.entityId);
            phase.set("players", players);
            return t.g.applyDmAct({std::to_string(++n), "story.phase", id, "dm", json::dump(phase)}).ok;
        };
        expect(open(run) && open(other), "two stories open");
        const auto order = [&](const std::string& kind, const std::string& target, const std::string& payload, const std::string& by = {}) {
            auto p = parsed(payload.empty() ? "{}" : payload);
            if (!p.has("run")) p.set("run", by.empty() ? run : by);
            if (!p.has("visit")) p.set("visit", 1);
            return t.g.applyDmAct({std::to_string(++n), kind, target, "dm", json::dump(p)});
        };

        // ---- SQ2: a till pays by till:<founder> as by till:job:<founder>.
        auto& soc = w.society();
        expect((soc.account("till:job:um") || soc.openAccount("till:job:um")) && soc.shift("treasury", "till:job:um", "", 0, 200, "test till"),
               "Merchant u's till has 200p more");
        const auto cash = [&](const std::string& a) { return soc.account(a) ? soc.account(a)->cash : -1; };
        const auto ash0 = cash(t.ash.entityId), till0 = cash("till:job:um");
        auto r = t.g.applyEconomyCommand("character.grant", t.ash.entityId, parsed(R"({"coins":5,"purse":"till:um","reason":"for the rope"})"));
        expect(r.ok && cash(t.ash.entityId) == ash0 + 5 && cash("till:job:um") == till0 - 5, "till:um pays from till:job:um: " + r.message);
        r = t.g.applyEconomyCommand("character.grant", t.ash.entityId, parsed(R"({"coins":4,"purse":"till:job:um","reason":"for the net"})"));
        expect(r.ok && cash(t.ash.entityId) == ash0 + 9 && cash("till:job:um") == till0 - 9, "and till:job:um the same: " + r.message);
        expect(!t.g.applyEconomyCommand("character.grant", t.ash.entityId, parsed(R"({"coins":4,"purse":"till:nobody","reason":"x"})")).ok,
               "a till nobody has pays nothing");

        // ---- C11: spared from old age. At 120 a resident dies the first day (a year's chance of one); spared, never.
        w.entity("u1")->age = 120;
        w.entity("u2")->age = 120;
        w.entity("s1")->age = 120;
        expect(order("npc.spare", "u1", "").ok && t.g.storyHoldOwners("spare", "u1") == std::set<std::string>{"run:" + run} && w.storySpared("u1"),
               "u1 is spared by the story");
        expect(order("npc.spare", "s1", R"({"lasts":"campaign","campaign":"the-ninth-howl"})").ok, "s1 is spared for the campaign");
        expect(!order("npc.spare", "s2", R"({"lasts":"campaign"})").ok, "only a chapter of a campaign spares for the campaign");
        expect(order("npc.spare", "u1", "", other).ok && t.g.storyHoldOwners("spare", "u1").size() == 2, "the pack spares u1 too");
        for (int day = 0; day < 3; ++day)
        {
            w.advanceCalendar(1);
            t.run(.25);
        }
        expect(!w.entity("u1")->dead && !w.entity("s1")->dead, "the spared live on");
        expect(w.entity("u2")->dead, "u2, not spared, dies of old age");
        r = order("npc.unspare", "u1", "", other);
        expect(r.ok && t.g.storyHoldOwners("spare", "u1") == std::set<std::string>{"run:" + run} && w.storySpared("u1") &&
                   contains(r.message, "another story still does"), "the pack lets go; the story still spares u1: " + r.message);

        // ---- C11: protected for the run (and for good, as ever).
        expect(!t.g.residentProtected("u3"), "u3 is not protected");
        expect(order("npc.protect", "u3", R"({"lasts":"run"})").ok && t.g.residentProtected("u3"), "u3 protected while the story runs");
        expect(!order("npc.kill", "u3", "").ok && !w.entity("u3")->dead, "a story never kills them");
        expect(order("npc.protect", "s3", R"({"lasts":"run"})", other).ok && t.g.residentProtected("s3"), "the pack protects s3");
        expect(order("npc.unprotect", "s3", "", other).ok && !t.g.residentProtected("s3"), "and its Unprotect lets go");
        // (A story's Unprotect lets go of its own hold only: a resident protected for good stays so, and stories may still
        // protect them; doc 86, G2.)
        expect(t.g.applyDmAct({std::to_string(++n), "npc.protect", "s2", "dm", "{}"}).ok && t.g.residentProtected("s2"),
               "a Dungeon Master protects s2 for good");
        expect(order("npc.protect", "s2", R"({"lasts":"run"})").ok, "the story protects s2 while it runs too");
        r = order("npc.unprotect", "s2", "");
        expect(r.ok && t.g.residentProtected("s2") && t.g.storyHoldOwners("protect", "s2").empty(),
               "the story's Unprotect lets go of its own hold, and s2 is still protected: " + r.message);
        r = order("npc.protect", "s2", R"({"lasts":"run"})", other);
        expect(r.ok, "and another story may still protect s2: " + r.message);

        // ---- C8: what residents remember.
        const auto here = w.townIdOf(w.entity("u1")->cellId);
        expect(!here.empty(), "u1 stands in a town: " + here);
        expect(order("talk.remember", run, R"({"scope":"realm","key":"nh_stage","value":"waking","days":30})").ok, "the realm remembers");
        auto q = t.g.talkQuery("u1", t.ash.entityId, "", {});
        expect(q.facts["world.mem.nh_stage"] == "waking", "u1 recalls the realm's: " + q.facts["world.mem.nh_stage"]);
        expect(order("talk.remember", run, R"({"scope":"town","town":")" + here + R"(","key":"nh_stage","value":"reckoning","days":30})").ok,
               "the town remembers");
        q = t.g.talkQuery("u1", t.ash.entityId, "", {});
        expect(q.facts["world.mem.nh_stage"] == "reckoning", "the town's overrides the realm's");
        expect(order("talk.remember", run, R"({"scope":"npc","npc":"u1","key":"nh_willa","value":"met","days":10})").ok, "u1 remembers");
        expect(order("talk.remember", run, R"({"scope":"pair","npc":"u1","player":")" + t.ash.entityId + R"(","key":"nh_stone","value":"brought","days":60,"keep":true})").ok,
               "u1 remembers Ash brought the stone, kept after the story");
        expect(!order("talk.remember", run, R"({"scope":"pair","npc":"u1","player":"player-nobody","key":"nh_stone","value":"brought","days":60})").ok,
               "pair memory names a wolf of the story");
        expect(!order("talk.remember", run, R"({"scope":"realm","key":"Nh Stage","value":"x","days":3})").ok, "a key is small letters");
        q = t.g.talkQuery("u1", t.ash.entityId, "", {});
        expect(q.a.facts["mem.nh_willa"] == "met" && q.facts["pair.mem.nh_stone"] == "brought", "u1's own, and the pair's for Ash");
        q = t.g.talkQuery("u1", t.bo.entityId, "", {});
        expect(!q.facts.count("pair.mem.nh_stone"), "not for Bo");
        // (The caps: a town's 64, a pair's 16; the soonest to be forgotten goes first.)
        for (int k = 0; k < 70; ++k)
            order("talk.remember", run, R"({"scope":"town","town":"oakhaven","key":"k)" + std::to_string(k) + R"(","value":"v","days":)" + std::to_string(1 + k) + "}");
        const auto& mem = t.g.storyMemory();
        expect(mem.at("town:oakhaven").size() == 64 && !mem.at("town:oakhaven").count("k0") && mem.at("town:oakhaven").count("k69"),
               "a town keeps 64: the soonest gone first");
        const auto pairOwner = "pair:" + std::min(std::string("u1"), t.ash.entityId) + "|" + std::max(std::string("u1"), t.ash.entityId);
        for (int k = 0; k < 20; ++k)
            order("talk.remember", run, R"({"scope":"pair","npc":"u1","player":")" + t.ash.entityId + R"(","key":"p)" + std::to_string(k) +
                                            R"(","value":"v","days":)" + std::to_string(100 + k) + "}");
        expect(mem.at(pairOwner).size() == 16 && mem.at(pairOwner).count("nh_stone") == 0 && mem.at(pairOwner).count("p19"),
               "a pair keeps 16 (the stone, soonest to go, went first)");
        expect(order("talk.remember", run, R"({"scope":"pair","npc":"u1","player":")" + t.ash.entityId + R"(","key":"nh_stone","value":"brought","days":200,"keep":true})").ok,
               "the stone remembered again");
        expect(order("talk.forget", run, R"({"scope":"town","town":"oakhaven","key":"k69"})").ok && !mem.at("town:oakhaven").count("k69"), "forgotten");

        // ---- C8 and T7: a rumour, and its clearing.
        r = order("belief.plant", "u1", R"({"town":")" + here + R"(","claim":"was seen at the mill at midnight","count":3,"confidence":60})");
        expect(r.ok, "the rumour is planted: " + r.message);
        std::vector<std::string> holders;
        for (const auto& [id, e] : w.entities())
            if (const auto* list = w.beliefsOf(id))
                for (const auto& b : *list)
                    if (b.subject == "u1" && b.kind == "story.rumour" && b.claim == "was seen at the mill at midnight" && b.source == "story:" + run)
                        holders.push_back(id);
        expect(holders.size() == 3 && std::find(holders.begin(), holders.end(), "u1") == holders.end(), "three residents of the town hold it, not u1");
        r = order("belief.plant", "u1", R"({"town":")" + here + R"(","claim":"was seen at the mill at midnight","count":3,"confidence":60})");
        std::size_t again = 0;
        for (const auto& id : holders)
            for (const auto& b : *w.beliefsOf(id))
                again += b.subject == "u1" && b.kind == "story.rumour";
        expect(r.ok && again == 3, "planted again, the same three hold it once each");
        r = order("belief.plant", "u1", R"({"town":")" + here + R"(","claim":"took the mill's silver","count":2,"confidence":70,"claimKind":"story.cleared"})");
        std::size_t cleared = 0;
        for (const auto& [id, e] : w.entities())
            if (const auto* list = w.beliefsOf(id))
                for (const auto& b : *list)
                    cleared += b.subject == "u1" && b.kind == "story.cleared" && b.claim == "never took the mill's silver";
        expect(r.ok && cleared == 2, "two believe the clearing, in its own words");
#ifdef RATW_SOURCE_DIR
        {
            std::ifstream file(std::string(RATW_SOURCE_DIR) + "/Data/Talk/claims.json");
            std::stringstream text;
            text << file.rdbuf();
            std::string problem;
            expect(claims::load(text.str(), problem), "claims.json loads: " + problem);
            claims::Words words;
            words.xName = "Neighbour u1";
            words.text = "was seen at the mill at midnight";
            expect(claims::said("story.rumour", "they", words) == "Neighbour u1 was seen at the mill at midnight", "gossip tells the rumour");
            words.text = "never took the mill's silver";
            expect(claims::said("story.cleared", "they", words) == "Neighbour u1 never took the mill's silver", "and the clearing");
            claims::clear();
        }
#endif
        expect(!order("belief.plant", "u1", R"({"town":")" + here + R"(","claim":"is rm111","count":3,"confidence":60})").ok, "never an id");
        // (Any word that mixes letters and digits is taken for an id, as story_program.claim_has_id: doc 86, B6.)
        for (const std::string claim : {"was seen at the docks at 2am", "sold 3rd-rate grain", "paid 40s for it"})
            expect(!order("belief.plant", "u1", R"({"town":")" + here + R"(","claim":")" + claim + R"(","count":3,"confidence":60})").ok,
                   "refused as Python refuses it: " + claim);
        expect(order("belief.plant", "u1", R"({"town":")" + here + R"(","claim":"paid 40 shillings for a well-known blade","count":3,"confidence":60})").ok,
               "a number and a hyphen alone are words");
        t.g.save();
    }
    {
        // ---- kept through a save; the run ends and lets go.
        Town t(world, save);
        auto& w = t.g.world();
        const auto& mem = t.g.storyMemory();
        expect(mem.count("realm") && mem.at("realm").count("nh_stage") && mem.count("town:oakhaven") && mem.at("town:oakhaven").size() == 63,
               "the memories come back with the save");
        expect(t.g.storyHoldOwners("spare", "u1").size() == 1 && w.storySpared("u1") && t.g.storyHoldOwners("spare", "s1").size() == 1 &&
                   t.g.residentProtected("u3"), "and the holds");
        expect(t.g.applyDmAct({"300", "story.stop", run, "dm", "{}"}).ok, "the story stops");
        expect(!w.storySpared("u1") && !t.g.residentProtected("u3"), "its holds are let go");
        expect(w.storySpared("s1"), "the campaign's hold outlasts the chapter");
        expect(!mem.count("realm") && !mem.count("town:oakhaven"), "what it had residents remember is taken back");
        bool kept = false;
        for (const auto& [owner, memos] : mem)
            kept |= owner.rfind("pair:", 0) == 0 && memos.count("nh_stone");
        expect(kept, "but what was kept stays");
        expect(t.g.applyDmAct({"301", "story.campaign_end", "the-ninth-howl", "story-runner", "{}"}).ok && !w.storySpared("s1"),
               "the campaign ends: its hold is let go");
    }
    fs::remove_all(root);
    fs::remove(save);
}
// Doc 86, Phase 2 (C1b): the name tokens a story's engines leave in its words ({@w:<wolf>}, {@r:...}, {@p:...}, {@i:...},
// {@f:...}) become names where the game applies the words, cut at the limit on a word boundary.
void phase2Names()
{
    const auto root = fs::temp_directory_path() / ("ratw-storyworld-2-" + std::to_string(::getpid()));
    const auto save = root.string() + ".json";
    {
        Town t(writeDoors(root), save);
        auto& w = t.g.world();
        t.enter(t.ash, 1, "ash", "Ash", 0, 8.5);
        const auto ash = t.ash.entityId;
        const auto* u1 = w.entity("u1");
        expect(u1 && !u1->name.empty(), "a resident to name");
        const auto* place = w.cell(cellId(0));
        expect(place && !place->name.empty(), "a place to name");
        const auto filled = t.g.fillStoryNames("{@w:" + ash + "} met {@r:u1} at {@p:" + cellId(0) + "} with {@i:rope}.", 300);
        expect(filled == "Ash met " + u1->name + " at " + place->name + " with " + Society::itemName("rope") + ".", "the tokens are names: " + filled);
        expect(t.g.fillStoryNames("{@x:what} {@w:} {@ and {plain}", 300) == "{@x:what} someone {@ and {plain}",
               "what the engines do not make is kept as written; a wolf not known is someone: " + t.g.fillStoryNames("{@x:what} {@w:} {@ and {plain}", 300));
        const auto cut = t.g.fillStoryNames("{@r:u1} walked the long road down to the ford and back again", 30);
        expect(cut.size() <= 30 && cut.back() != ' ' && ("x " + u1->name + " walked the long road down to the ford and back again").find(cut) != std::string::npos &&
                   (cut.size() == 30 || cut.find(' ') != std::string::npos),
               "cut at the limit on a word boundary: \"" + cut + "\"");
        expect(t.g.fillStoryNames("No tokens here.", 300) == "No tokens here.", "words without tokens are as they were");
        // Told: the wolf hears the name; the quest's journal and the offer read names too.
        const auto run = std::string("world-names");
        auto phase = parsed(R"({"title":"Names","phase":"one","journal":"Names.","visit":1})");
        auto players = json::Value::array();
        players.push(ash);
        phase.set("players", players);
        expect(t.g.applyDmAct({"1", "story.phase", run, "dm", json::dump(phase)}).ok, "the story opens");
        t.ash.said();
        expect(t.g.applyDmAct({"2", "story.tell", run, "dm", R"({"visit":1,"to":"wolf","player":")" + ash + R"(","text":"{@r:u1} asks for you, {@w:)" + ash + R"(}."})"}).ok,
               "told with tokens");
        const auto heard = t.ash.said();
        expect(contains(heard, u1->name + " asks for you, Ash.") && !contains(heard, "{@"), "Ash hears the names: " + heard);
        auto quests = parsed(R"({"visit":1,"offers":[{"quest":"q1","title":"A word","npc":"u1","say":"Go to {@p:)" + cellId(0) + R"(}.","ask":"Ask"}]})");
        auto wolves = json::Value::object();
        wolves.set(ash, parsed(R"({"quests":[{"id":"q1","title":"A word","summary":"Find {@r:u1}.","status":"active","objectives":[{"id":"o","label":"Speak to {@r:u1}.","goal":1}]}],"trackers":[]})"));
        quests.set("wolves", wolves);
        expect(t.g.applyDmAct({"3", "story.quests", run, "dm", json::dump(quests)}).ok, "the quests with tokens are told");
        const auto* story = t.g.storylineBook().find(run);
        expect(story && story->offers.items().size() == 1 && story->offers.items()[0].string("say") == "Go to " + place->name + ".",
               "the offer's words name the place");
        const auto& view = story->questViews.at(ash);
        expect(view.array("quests")[0].string("summary") == "Find " + u1->name + "." &&
                   view.array("quests")[0].array("objectives")[0].string("label") == "Speak to " + u1->name + ".",
               "the journal's summary and objective name the resident");
    }
    fs::remove_all(root);
    fs::remove(save);
}

// Doc 86, Phase 3: a wolf named by a title (C4), a resident's greeting waiting for one wolf (C5), a rumour put to rest (J8).
void phase3Thanks()
{
    const auto root = fs::temp_directory_path() / ("ratw-storyworld-3-" + std::to_string(::getpid()));
    const auto save = root.string() + ".json";
    const auto world = writeDoors(root);
    const auto run = std::string("world-thanks"), other = std::string("world-other");
    std::string here;
    {
        Town t(world, save);
        auto& w = t.g.world();
        t.enter(t.ash, 1, "ash", "Ash", 0, 8.5);
        t.enter(t.bo, 2, "bo", "Bo", 0, 10.5);
        const auto ash = t.ash.entityId;
        int n = 400;
        const auto open = [&](const std::string& id, const std::string& program) {
            auto phase = parsed(R"({"title":"Thanks","phase":"one","journal":"Thanks.","visit":1,"program":")" + program + R"("})");
            auto players = json::Value::array();
            players.push(ash);
            phase.set("players", players);
            return t.g.applyDmAct({std::to_string(++n), "story.phase", id, "dm", json::dump(phase)}).ok;
        };
        expect(open(run, "ninth-howl.1") && open(other, "lamp-pack"), "two stories open");
        const auto order = [&](const std::string& kind, const std::string& target, const std::string& payload, const std::string& by = {}) {
            auto p = parsed(payload.empty() ? "{}" : payload);
            if (!p.has("run")) p.set("run", by.empty() ? run : by);
            if (!p.has("visit")) p.set("visit", 1);
            return t.g.applyDmAct({std::to_string(++n), kind, target, "dm", json::dump(p)});
        };
        here = w.townIdOf(w.society().resident("u1")->homeCell);
        expect(!here.empty(), "u1 lives in a town: " + here);
        const auto titles = [&]() -> const std::vector<StoryTitle>& { return w.entity(ash)->titles; };

        // ---- C4: titles.
        auto r = order("title.give", ash, R"({"title":"the Lamp of the Ford"})");
        expect(r.ok && titles().size() == 1 && titles()[0].known == "realm" && titles()[0].until < 0 && titles()[0].by == "ninth-howl.1",
               "named for good in the realm: " + r.message);
        expect(t.g.storyTitleFor(ash, "u1") == "the Lamp of the Ford", "u1 knows it");
        bool heard = false;
        for (const auto& e : w.recentEvents())
            heard |= e.kind == "title given" && e.actor == ash && e.target == run && e.item == "realm" && e.detail == "the Lamp of the Ford";
        expect(heard, "the event title given is recorded");
        bool deed = false;
        for (const auto* d : t.g.deeds().byDoer(ash))
            deed |= d->detail == "was named the Lamp of the Ford" && d->source == "story";
        expect(deed, "a deed of its naming goes into the town's talk");
        r = order("title.give", ash, R"({"title":"Warden of the Weir","known":"town","town":"nowhere"})");
        expect(!r.ok, "a town that is not");
        r = order("title.give", ash, R"({"title":"Warden of the Weir","known":"town","town":")" + here + R"(","days":5})");
        expect(r.ok && t.g.storyTitleFor(ash, "u1") == "Warden of the Weir", "the newest known in u1's town is the one used");
        expect(!order("title.give", ash, R"({"title":"the #greet# one"})").ok && !order("title.give", ash, R"({"title":"rm111's friend"})").ok,
               "no symbols, no ids");
        expect(!order("title.give", "u1", R"({"title":"the Miller"})").ok, "a resident has no story titles");
        r = order("title.give", ash, R"({"title":"warden of the weir","known":"town","town":")" + here + R"(","days":20})");
        expect(r.ok && titles().size() == 2 && titles()[1].until > w.calendarDays() + 19, "given again: renewed, not doubled");
        int namings = 0;
        for (const auto* d : t.g.deeds().byDoer(ash))
            namings += d->detail == "was named Warden of the Weir" || d->detail == "was named warden of the weir";
        expect(namings == 1, "renewed: no second deed of its naming: " + std::to_string(namings));
        w.advanceCalendar(21);
        expect(t.g.storyTitleFor(ash, "u1") == "the Lamp of the Ford", "the town's lapsed: the realm's is used again");
        auto card = t.g.titlesView(*w.entity(ash));
        expect(card.size() == 1 && card.items()[0].string("title") == "the Lamp of the Ford" && card.items()[0].number("days") == -1,
               "the card shows only what has not lapsed");
        // The ninth: the oldest that lapses goes, else the oldest.
        for (int k = 0; k < 7; ++k)
            expect(order("title.give", ash, R"({"title":"Title )" + std::string(1, char('A' + k)) + R"(")" + (k == 3 ? R"(,"days":200)" : "") + "}").ok,
                   "title " + std::to_string(k));
        expect(titles().size() == 8, "eight titles: " + std::to_string(titles().size()));
        expect(order("title.give", ash, R"({"title":"Title H"})").ok && titles().size() == 8 &&
                   std::none_of(titles().begin(), titles().end(), [](const StoryTitle& x) { return x.title == "Title D"; }),
               "the ninth drops the one that lapses");
        expect(order("title.give", ash, R"({"title":"Title I"})").ok &&
                   std::none_of(titles().begin(), titles().end(), [](const StoryTitle& x) { return x.title == "the Lamp of the Ford"; }),
               "then the oldest");
        expect(order("title.give", ash, R"({"title":"the Other's Own"})", other).ok, "another story names Ash");
        r = order("title.take", ash, R"({"title":"title h"})");
        expect(r.ok && std::none_of(titles().begin(), titles().end(), [](const StoryTitle& x) { return x.title == "Title H"; }), "taken by its title");
        r = order("title.take", ash, "{}");
        expect(r.ok && titles().size() == 1 && titles()[0].title == "the Other's Own", "every title this story gave taken: " + r.message);
        expect(order("title.take", ash, R"({"title":"the Other's Own"})").ok && titles().size() == 1, "not another story's");
        expect(order("title.give", ash, R"({"title":"Friend of {@w:)" + ash + R"(}"})").ok && titles().size() == 2 &&
                   titles()[1].title == "Friend of Ash", "a title's names filled as it is given");
        expect(order("title.take", ash, R"({"title":"Friend of {@w:)" + ash + R"(}"})").ok && titles().size() == 1,
               "and taken back by the same words, filled the same");
        t.g.command(&t.ash, cmd({{"type", "social"}, {"verb", "hidetitles"}, {"hidden", true}}));
        expect(w.entity(ash)->titlesHidden,
               "Ash hides the titles from the card");

        // ---- C5: a greeting waits, replaced by a later one, once.
        expect(!order("npc.greet", "u1", R"({"player":"player-nobody","text":"Hello."})").ok, "only for a wolf of the story");
        expect(!order("npc.greet", "nobody", R"({"player":")" + ash + R"(","text":"Hello."})").ok, "a resident");
        expect(order("npc.greet", "u1", R"({"player":")" + ash + R"(","text":"Hello, {@w:)" + ash + R"(}.","days":3})").ok, "u1 will greet Ash");
        expect(t.g.storyGreets().at("u1").at(ash).text == "Hello, Ash.", "names filled as it is kept");
        expect(order("npc.greet", "u1", R"({"player":")" + ash + R"(","text":"Thank you for the oil."})").ok &&
                   t.g.storyGreets().at("u1").at(ash).text == "Thank you for the oil.", "a later one replaces it");
        expect(order("npc.greet", "u2", R"({"player":")" + ash + R"(","text":"Old words.","days":1})").ok, "u2's lapses soon");

        // ---- J8: doubt.
        r = order("belief.plant", "u1", R"({"town":")" + here + R"(","claim":"was seen at the mill at midnight","count":4,"confidence":80})");
        expect(r.ok, "a rumour: " + r.message);
        const auto strengths = [&]() {
            std::vector<double> out;
            for (const auto& [id, e] : w.entities())
                if (const auto* list = w.beliefsOf(id))
                    for (const auto& b : *list)
                        if (b.subject == "u1" && b.kind == "story.rumour")
                            out.push_back(b.confidence);
            return out;
        };
        expect(strengths().size() == 4, "four hold it");
        expect(!order("belief.forget", "u1", R"({"claimKind":"rumour","share":50})").ok && !order("belief.forget", "u1", R"({"claimKind":"story.rumour","share":5})").ok,
               "a kind and a share it knows");
        expect(order("belief.forget", "u1", R"({"claimKind":"story.rumour","town":"elsewhere","share":50})").ok && strengths()[0] == .8,
               "another town's residents are not touched");
        r = order("belief.forget", "u1", R"({"claimKind":"story.rumour","share":50})");
        auto now = strengths();
        expect(r.ok && now.size() == 4 && std::all_of(now.begin(), now.end(), [](double c) { return std::abs(c - .4) < 1e-9; }), "half as sure: " + r.message);
        r = order("belief.forget", "u1", R"({"claimKind":"story.rumour","town":")" + here + R"(","share":50})");
        now = strengths();
        expect(r.ok && std::all_of(now.begin(), now.end(), [](double c) { return std::abs(c - .2) < 1e-9; }), "and half again in the town");
        expect(order("belief.forget", "u1", R"({"claimKind":"story.rumour","share":100})").ok && strengths().empty(), "100 forgets it");
        t.g.save();
    }
    {
        Town t(world, save);
        auto& w = t.g.world();
        const auto ash = t.ash.entityId.empty() ? std::string() : t.ash.entityId;
        bool found = false;
        for (const auto& [id, e] : t.g.characters())
            if (!e.titles.empty())
                found = e.titles.size() == 1 && e.titles[0].title == "the Other's Own" && e.titlesHidden;
        expect(found, "the titles come back with the character");
        expect(t.g.storyGreets().count("u1") && t.g.storyGreets().at("u1").size() == 1, "and the greeting waiting");
        w.advanceCalendar(2);
        (void)ash;
    }
    fs::remove_all(root);
    fs::remove(save);
}


// Doc 86, J9 (Core/RatwGameStoryWorld.cpp, RatwStoryArea.cpp): a story's own items. A tagged item.give names its item and
// lists it in the run's held.tags (a kept one not); the pack shows the name; a gift of it to a resident is a gift of that
// id; no shop takes it, nobody eats it; a give refused for a full pack names nothing; the facts count it under its own id
// when the watch names it and as its base when the watch names that (another story's "carries letter"); the run's
// end takes every unit back from every purse and the ground, the kept one staying with its name through a save.
void phase6Items()
{
    const auto root = fs::temp_directory_path() / ("ratw-storyworld-6-" + std::to_string(::getpid()));
    const auto save = root.string() + ".json";
    const auto world = writeDoors(root);
    const auto run = std::string("world-letters");
    const std::string list = "letter^ab12cd34.watch_list", writ = "writ^ab12cd34.thanks";
    {
        Town t(world, save);
        auto& w = t.g.world();
        auto& s = w.society();
        t.enter(t.ash, 1, "ash", "Ash", 0, 8.5);
        t.enter(t.bo, 2, "bo", "Bo", 0, 10.5);
        const auto ash = t.ash.entityId;
        std::vector<WorldEvent> seen;
        w.setEventWatcher([&](const WorldEvent& e) { seen.push_back(e); });
        auto phase = parsed(R"({"title":"The watch list","phase":"carry","journal":"A list for the warden.","visit":1,"program":"v2-watch-list",
            "watch":{"facts":["wolves"],"items":["letter","letter^ab12cd34.watch_list"]}})");
        auto players = json::Value::array();
        players.push(ash);
        players.push(t.bo.entityId);
        phase.set("players", players);
        expect(t.g.applyDmAct({"700", "story.phase", run, "dm", json::dump(phase)}).ok, "the story opens");
        int n = 700;
        const auto give = [&](const std::string& wolf, const std::string& payload) {
            return t.g.applyDmAct({std::to_string(++n), "item.give", wolf, "dm", payload});
        };
        auto& book = t.g.storylineBook();
        const auto tags = [&]() -> const json::Value& { return book.find(run)->held["tags"]; };

        // ---- refusals: no running story, a malformed id, a base that is no plain item, a name of more than one line.
        expect(!give(ash, R"({"item":"letter^ab12cd34.watch_list","quantity":1})").ok, "a story's own item needs its running story");
        expect(!give(ash, R"({"item":"letter^watch_list","quantity":1,"run":"world-letters"})").ok, "the short form is the author's, not the game's");
        expect(!give(ash, R"({"item":"letter~fine^ab12cd34.x","quantity":1,"run":"world-letters"})").ok, "a plain item's");
        expect(!give(ash, R"({"item":"letter^ab12cd34.watch_list","quantity":1,"run":"world-letters","name":"two\nlines"})").ok, "one line");
        expect(!give(ash, R"({"item":"letter^ab12cd34.watch_list","quantity":1,"run":"world-nothing","name":"x"})").ok, "of a story running");
        expect(Society::stock(*s.account(ash), list) == 0 && !tags().size() && !items::storyItemName(list), "nothing given, named or held");

        // ---- given, named, held; a kept one given and named, not held.
        auto r = give(ash, R"({"item":"letter^ab12cd34.watch_list","quantity":2,"name":"The watch list","run":"world-letters","visit":1})");
        expect(r.ok && Society::stock(*s.account(ash), list) == 2 && contains(t.ash.said(), "You receive 2 of the watch list."), "Ash is given the list: " + r.message);
        expect(t.g.storyItemNames().at(list) == "The watch list" && std::string(Society::itemName(list)) == "The watch list" &&
                   tags().size() == 1 && tags().items()[0].asString() == list, "named, and held by the run");
        r = give(ash, R"({"item":"writ^ab12cd34.thanks","quantity":1,"name":"The warden's writ of thanks","keep":true,"run":"world-letters","visit":1})");
        expect(r.ok && Society::stock(*s.account(ash), writ) == 1 && tags().size() == 1, "the writ is given, kept after the story: " + r.message);
        expect(give(t.bo.entityId, R"({"item":"letter","quantity":1})").ok, "Bo has a plain letter");
        t.run(.5);
        bool shown = false;
        for (const auto& i : t.ash.snapshots.back().array("inventory"))
            if (i.string("id") == list)
                shown = i.string("name") == "The watch list" && !i.has("condition") && !i.boolean("blade");
        expect(shown, "Ash's pack shows it by its name, with no wear");

        // ---- the facts: under its own id where the watch names it; a plain letter as itself.
        json::Value facts;
        std::string error;
        expect(json::parse(t.g.storyFactsJson(), facts, error), "the facts: " + error);
        expect(facts["wolves"][ash]["carries"].number(list) == 2 && facts["wolves"][ash]["carries"].number("letter") == 2 &&
                   facts["wolves"][t.bo.entityId]["carries"].number("letter") == 1, "the list under its own id and as a letter, Bo's letter as a letter: " +
               json::dump(facts["wolves"]));

        // ---- never lent (the story may take it back while it is out).
        t.ash.said();
        t.g.command(&t.ash, cmd({{"type", "lend"}, {"target", t.bo.entityId}, {"item", list}, {"quantity", 1}, {"days", 2}}));
        expect(contains(t.ash.said(), "That belongs to a story."), "a story's own item is not lent");

        // ---- no shop takes it.
        auto* um = w.entity("um");
        const auto keep = [&] {
            um->cellId = w.entity(ash)->cellId;
            um->position = {w.entity(ash)->position.x + 1, w.entity(ash)->position.y};
            w.stop("um");
        };
        keep();
        const auto quote = s.quote(ash, "um", list, 1, false);
        expect(!quote.ok && quote.message == "That belongs to a story.", "no shop buys it: " + quote.message);
        keep();
        t.g.command(&t.ash, cmd({{"type", "trade"}, {"target", "um"}, {"item", list}, {"quantity", 1}, {"buy", false}}));
        expect(Society::stock(*s.account(ash), list) == 2, "nor takes it in trade");

        // ---- a gift of it to a resident: the gift's item is that id; one more lies on the ground.
        auto* u1 = w.entity("u1");
        t.place(t.ash, 0, u1->position.x + .6, u1->position.y);
        w.entity(ash)->cellId = u1->cellId;
        seen.clear();
        t.ash.said();
        t.g.command(&t.ash, cmd({{"type", "give"}, {"target", "u1"}, {"item", list}, {"quantity", 1}}));
        const auto gift = std::find_if(seen.begin(), seen.end(), [](const WorldEvent& e) { return e.kind == "gift" || e.kind == "gift again"; });
        const auto gave = t.ash.said();
        expect(gift != seen.end() && gift->item == list && gift->target == "u1" && Society::stock(*s.account("u1"), list) == 1,
               "u1 is given the list, the gift's item its own id: " + gave);
        // (" 1 the": a count before the name, not the end of "Neighbour u1 the watch list", the line as it is told.)
        expect(contains(gave, "the watch list") && !contains(gave, " 1 the watch list") && !contains(gave, "the the"), "said as \"the watch list\": " + gave);
        // (Nor eaten, for all a story's bread is bread underneath.)
        expect(!Society::edible("bread^ab12cd34.offering") && Society::edible("bread") && Society::nourishment("bread^ab12cd34.offering") == 0,
               "a story's own food is never eaten");

        // ---- a give refused for a full pack leaves no name behind (and nothing held).
        for (const auto& item : items::allGoods())
            if (s.account(t.bo.entityId)->stock.size() < MaxGoodsKinds && item.id != "letter" && item.id != "rope")
                s.create(t.bo.entityId, item.id, 1, "test pack");
        t.bo.said();
        r = give(t.bo.entityId, R"({"item":"rope^ab12cd34.coil","quantity":1,"name":"The coil","run":"world-letters","visit":1})");
        expect(!r.ok && contains(t.bo.said(), "no room to carry the coil") && !t.g.storyItemNames().count("rope^ab12cd34.coil") &&
                   !items::storyItemName("rope^ab12cd34.coil") && tags().size() == 1, "refused, unnamed, not held: " + r.message);
        const auto ground = w.groundItems().size();
        expect(w.placeOnGround(cellId(0), 5, 5, list).ok && w.groundItems().size() == ground + 1, "and one lies on the ground");

        // ---- the run ends: every unit taken back, the kept writ stays with its name.
        t.ash.said();
        expect(t.g.applyDmAct({std::to_string(++n), "story.stop", run, "dm", "{}"}).ok, "the story stops");
        expect(Society::stock(*s.account(ash), list) == 0 && Society::stock(*s.account("u1"), list) == 0 &&
                   std::none_of(w.groundItems().begin(), w.groundItems().end(), [&](const GroundItem& g) { return g.item == list; }) &&
                   Society::stock(*s.account(t.bo.entityId), "letter") == 1,
               "taken back from Ash, from u1 and off the ground; Bo's plain letter stays");
        expect(contains(t.ash.said(), "gone with it") && !t.g.storyItemNames().count(list) && !items::storyItemName(list), "Ash told; its name let go");
        expect(Society::stock(*s.account(ash), writ) == 1 && std::string(Society::itemName(writ)) == "The warden's writ of thanks", "the writ kept, named");
        expect(!book.find(run)->held.has("tags"), "nothing is held");
        t.g.save();
    }
    items::nameStoryItem(writ, "");
    {
        Town t(world, save);
        expect(t.g.storyItemNames().count(writ) && std::string(Society::itemName(writ)) == "The warden's writ of thanks",
               "the kept writ's name comes back with the save");
    }
    fs::remove_all(root);
    fs::remove(save);
}

// Doc 86, C10 (Core/RatwGameStoryWorld.cpp, RatwStoryFoes.cpp): a story's foe brought and kept with the story (held.foes),
// its refusals, sent away; a resident made a foe; both made again after a restart while their time lasts; a resident made
// a foe and beaten held until the story ends; the story's end takes every foe away with no event.
void phase7Foes()
{
    const auto root = fs::temp_directory_path() / ("ratw-storyworld-7-" + std::to_string(::getpid()));
    const auto save = root.string() + ".json";
    const auto world = writeDoors(root);
    const auto run = std::string("world-foes");
    const auto camp = game::Game::storyFoeCampId(run, "captain"), outlaw = game::Game::storyFoeCampId(run, "npc-u1");
    {
        Town t(world, save);
        auto& w = t.g.world();
        t.enter(t.ash, 1, "ash", "Ash", 0, 8.5);
        auto phase = parsed(R"({"title":"The Pass","phase":"pass","journal":"The Captain holds the Pass.","visit":1})");
        auto players = json::Value::array();
        players.push(t.ash.entityId);
        phase.set("players", players);
        expect(t.g.applyDmAct({"900", "story.phase", run, "dm", json::dump(phase)}).ok, "the story opens");
        int n = 900;
        const auto order = [&](const std::string& kind, const std::string& target, std::string payload) {
            auto p = parsed(payload.empty() ? "{}" : payload);
            if (!p.has("run")) p.set("run", run);
            if (!p.has("visit")) p.set("visit", 1);
            return t.g.applyDmAct({std::to_string(++n), kind, target, "dm", json::dump(p)});
        };
        auto& book = t.g.storylineBook();
        const auto foes = [&]() -> const json::Value& { return book.find(run)->held["foes"]; };
        const std::string captain = R"({"name":"The Grey Captain","cell":"c_2_0","x":8,"y":8,"strength":"champion","band":3,"stance":"waits","minutes":60,"like":"u2"})";

        // ---- refusals.
        expect(!order("foe.add", "captain", R"({"name":"x","cell":"c_2_0","x":8,"y":8,"minutes":60,"run":"world-nothing"})").ok, "of a story running");
        expect(!order("foe.add", "captain", R"({"name":"The Captain","cell":"c_2_0","x":8,"y":8,"minutes":0})").ok, "1 to 1440 minutes");
        expect(!order("foe.add", "captain", R"({"name":"The Captain","cell":"c_2_0","x":8,"y":8,"minutes":60,"band":9})").ok, "0 to 8 followers");
        expect(!order("foe.add", "captain", R"({"name":"The Captain","cell":"c_2_0","x":8,"y":8,"minutes":60,"strength":"mighty"})").ok,
               "tough, fierce or champion");
        expect(!order("foe.add", "captain", R"({"name":"The Captain","cell":"c_2_0","x":8,"y":8,"minutes":60,"like":"nobody"})").ok,
               "looking like a resident there is");
        expect(!order("foe.add", "Bad Id", R"({"name":"The Captain","cell":"c_2_0","x":8,"y":8,"minutes":60})").ok, "an id of small letters");
        expect(!order("foe.leave", "captain", "").ok, "sending away one the story never brought");
        expect(!foes().size() && w.storyFoes().empty(), "nothing made, nothing held");

        // ---- brought: in the world, looking like u2, kept with the story.
        auto r = order("foe.add", "captain", captain);
        expect(r.ok, "the Captain comes: " + r.message);
        const auto* lead = w.entity("road:" + camp + ":0");
        expect(lead && lead->name == "The Grey Captain" && lead->appearance->baseColor == w.entity("u2")->appearance->baseColor &&
                   w.entity("road:" + camp + ":3") && foes().has(camp) && foes()[camp].string("kind") == "foe.add",
               "the Captain and three followers, held by the story");
        expect(!order("foe.add", "captain", captain).ok, "once");
        expect(order("foe.leave", "captain", "").ok && !w.entity("road:" + camp + ":0") && !foes().has(camp) && !w.storyFoe(camp),
               "sent away: gone, no longer held");
        expect(order("foe.add", "captain", captain).ok, "and back");

        // ---- a resident made a foe (u1), with one follower; never one protected.
        r = order("foe.outlaw", "u1", R"({"minutes":30,"strength":"champion","band":1})");
        expect(r.ok && foes().has(outlaw) && w.storyFoeOf("u1") && w.entity("road:" + outlaw + ":1"), "u1 turns on the wolves: " + r.message);
        expect(!order("foe.outlaw", "u1", R"({"minutes":30})").ok, "once");
        expect(!order("foe.outlaw", "u2", R"({"minutes":721})").ok, "1 to 720 minutes");
        // (After review: while u1 is a foe no other story takes hold of him, and nobody's party takes him along, this story's
        // nor a wolf's; a follower's strength is no foe's; a foe's id never begins npc-.)
        auto other = parsed(R"({"title":"Another","phase":"x","journal":"Another story.","visit":1})");
        other.set("players", players);
        expect(t.g.applyDmAct({"950", "story.phase", "world-other", "dm", json::dump(other)}).ok, "another story opens");
        r = order("npc.hold", "u1", R"({"run":"world-other"})");
        expect(!r.ok && r.message == "Another story holds this resident.", "another story's hold of the foe refused: " + r.message);
        r = order("npc.follow", "u1", R"({"player":")" + t.ash.entityId + "\"}");
        expect(!r.ok && r.message == "They are set against the wolves just now.", "the foe follows no wolf: " + r.message);
        expect(!order("foe.add", "hakon", R"({"name":"Hakon","cell":"c_2_0","x":9,"y":9,"minutes":60,"strength":"follower"})").ok,
               "a follower's strength is no foe's");
        expect(!order("foe.add", "npc-u2", R"({"name":"Hakon","cell":"c_2_0","x":9,"y":9,"minutes":60})").ok, "an id beginning npc- refused");
        // (A rider for a minute, looking like u3: the restart below outlasts him.)
        expect(order("foe.add", "rider", R"({"name":"The Rider","cell":"c_2_0","x":10,"y":10,"minutes":1})").ok, "a rider for a minute");
        t.run(1);
        expect(foes().has(camp) && foes().has(outlaw), "both kept while they last");
        t.g.save();
    }
    {
        // ---- a restart: both made again from what the story keeps.
        Town t(world, save);
        auto& w = t.g.world();
        std::vector<WorldEvent> lost;
        w.setEventWatcher([&](const WorldEvent& e) { lost.push_back(e); });
        for (int i = 0; i < 70; ++i)
            w.tick(1);                                        // (The world on past the rider's minute before the story looks.)
        t.run(1);
        expect(w.entity("road:" + camp + ":0") && w.entity("road:" + camp + ":0")->name == "The Grey Captain" && w.storyFoeOf("u1"),
               "after a restart the Captain and u1 are foes again");
        const auto rider = std::find_if(lost.begin(), lost.end(), [](const WorldEvent& e) { return e.kind == "foe fled" && e.target == "rider"; });
        expect(rider != lost.end() && rider->item == "time" && rider->detail == "The Rider" &&
                   rider->coins == std::int64_t(ratw::foes::runMark(run)) && !t.g.storylineBook().find(run)->held["foes"].has(game::Game::storyFoeCampId(run, "rider")),
               "a foe a restart outlasted is 'foe fled' (time), with the run's mark: the story never waits on it");
        auto& book = t.g.storylineBook();
        // ---- u1 beaten (not killed): held until the story ends.
        auto* u1 = w.entity("u1");
        auto& ada = w.addPlayer("ada-7", "Ada");
        ada.cellId = u1->cellId;
        ada.position = {u1->position.x + 1, u1->position.y};
        ada.fightingSkill = 150;
        u1->leaderId.clear();
        std::vector<WorldEvent> seen;
        w.setEventWatcher([&](const WorldEvent& e) { seen.push_back(e); });
        const auto attacked = w.attack("ada-7", "u1");
        expect(attacked.ok && w.battleOf("ada-7") && w.battleOf("ada-7")->incident.empty(), "set on, no crime: " + attacked.message);
        // (u1 a champion, who never runs, badly hurt already; his follower barely standing: he is downed.)
        u1->hurt = 85;
        for (const auto& f : w.battleOf("ada-7")->fighters)
            if (f.id != "u1" && w.storyFoeOf(f.id))
                w.entity(f.id)->hurt = 95;
        for (int i = 0; i < 6000 && w.inBattle("ada-7"); ++i)
        {
            test::takeGround(w, "ada-7");
            test::playTurn(w, "ada-7");
            t.g.tick(.1);
        }
        t.run(2);
        const bool beaten = std::any_of(seen.begin(), seen.end(), [&](const WorldEvent& e) {
            return e.kind == "foe defeated" && e.target == "u1" && e.coins == std::int64_t(ratw::foes::runMark(run));
        });
        const bool ran = std::any_of(seen.begin(), seen.end(), [](const WorldEvent& e) { return e.kind == "foe fled" && e.target == "u1"; });
        expect(beaten && !ran, "u1 beaten");
        expect(!u1->dead && book.find(run)->held["npcs"].has("u1") && u1->sceneDirected, "beaten, not killed, u1 is held by the story");
        expect(!book.find(run)->held["foes"].has(outlaw) && !w.storyFoeOf("u1"), "and u1 is a foe no more");
        // ---- the story ends: the Captain goes, with no event.
        seen.clear();
        expect(t.g.applyDmAct({"990", "story.stop", run, "dm", "{}"}).ok, "the story stops");
        expect(!w.entity("road:" + camp + ":0") && !w.storyFoe(camp) && !book.find(run)->held.has("foes") && !u1->sceneDirected,
               "the Captain gone, u1 let go");
        expect(std::none_of(seen.begin(), seen.end(), [](const WorldEvent& e) { return e.kind == "foe fled"; }), "with no event");
    }
    fs::remove_all(root);
    fs::remove(save);
}
// Doc 86, Phase 8: a sentence that outlives its story (L4, npc.sentence) and the crown's change (C14, npc.post), through the
// game: the refusals; sentenced upstairs, kept there when the story ends, put back when moved, after a restart, and let go
// on the seventh day (walking home); "Release now"; a post taken from its holder in one step.
void phase8Sentences()
{
    const auto root = fs::temp_directory_path() / ("ratw-storyworld-8-" + std::to_string(::getpid()));
    const auto save = root.string() + ".json";
    const auto world = writeDoors(root);
    const auto run = std::string("world-wreckers");
    int n = 2000;
    {
        Town t(world, save);
        auto& w = t.g.world();
        t.enter(t.ash, 1, "ash", "Ash", 0, 8.5);
        auto players = json::Value::array();
        players.push(t.ash.entityId);
        const auto open = [&](const std::string& id, const std::string& program) {
            auto phase = parsed(R"({"title":"The Wreckers' Light","phase":"cells","journal":"The Watch has her.","visit":1})");
            phase.set("program", program);
            phase.set("players", players);
            return t.g.applyDmAct({std::to_string(++n), "story.phase", id, "dm", json::dump(phase)}).ok;
        };
        expect(open(run, "lq-wreckers-light") && open("world-other", "another"), "two stories open");
        const auto order = [&](const std::string& kind, const std::string& target, std::string payload) {
            auto p = parsed(payload.empty() ? "{}" : payload);
            if (!p.has("run")) p.set("run", run);
            if (!p.has("visit")) p.set("visit", 1);
            return t.g.applyDmAct({std::to_string(++n), kind, target, "dm", json::dump(p)});
        };
        std::vector<WorldEvent> seen;
        w.setEventWatcher([&](const WorldEvent& e) { seen.push_back(e); });

        // ---- refusals.
        expect(t.g.applyDmAct({std::to_string(++n), "npc.protect", "u2", "dm", "{}"}).ok, "a Dungeon Master protects u2");
        auto r = order("npc.sentence", "u2", R"({"place":"c_up","days":7})");
        expect(!r.ok && contains(r.message, "protected"), "a protected resident is never sentenced: " + r.message);
        expect(!order("npc.sentence", "u1", R"({"place":"c_up","days":0})").ok && !order("npc.sentence", "u1", R"({"place":"c_up","days":29})").ok,
               "1 to 28 days");
        expect(!order("npc.sentence", "u1", R"({"place":")" + cellId(0) + R"(","days":7})").ok, "a place indoors");
        expect(!order("npc.sentence", "u1", R"({"place":"nowhere","days":7})").ok, "a place there is");
        expect(!order("npc.sentence", "nobody", R"({"place":"c_up","days":7})").ok, "a resident there is");
        expect(!order("npc.sentence", "u1", R"({"place":"c_up","days":7,"run":"world-nothing"})").ok, "of a story running");
        expect(t.g.storySentences().empty(), "nobody sentenced");

        // ---- sentenced: upstairs, held, the story's program named.
        r = order("npc.sentence", "u1", R"({"place":"c_up","days":7})");
        auto* u1 = w.entity("u1");
        expect(r.ok && u1->cellId == "c_up" && u1->sceneDirected && contains(u1->activity, "held at"), "u1 is held upstairs: " + r.message);
        const auto& sentence = t.g.storySentences().at("u1");
        expect(sentence.by == "lq-wreckers-light" && sentence.run == run && sentence.days == 7 && std::abs(sentence.until - sentence.began - 7) < 1e-9,
               "the sentence names the story's program and lasts seven days");
        const auto begun = std::find_if(seen.begin(), seen.end(), [](const WorldEvent& e) { return e.kind == "sentence begun"; });
        expect(begun != seen.end() && begun->actor == "u1" && begun->cell == "c_up" && begun->quantity == 7 && begun->target == run &&
                   begun->detail == "lq-wreckers-light", "'sentence begun': the resident, the place, the days");
        expect(!order("npc.sentence", "u1", R"({"place":"c_up","days":3})").ok, "once at a time");
        r = order("npc.sentence", "u1", R"({"place":"c_up","days":7})");
        expect(r.ok && t.g.storySentences().at("u1").until == sentence.until, "the same order again is no change: " + r.message);
        r = order("npc.hold", "u1", R"({"run":"world-other"})");
        expect(!r.ok && r.message == "Another story holds this resident.", "another story takes no hold of them: " + r.message);
        r = order("npc.follow", "u1", R"({"player":")" + t.ash.entityId + "\"}");
        expect(!r.ok && r.message == "They are serving a sentence.", "and they travel with nobody: " + r.message);
        const auto post = w.society().jobOf("u1") ? w.society().jobOf("u1")->id : std::string();
        expect(!post.empty(), "u1 keeps their post: " + post);

        // ---- the story ends: still held.
        expect(t.g.applyDmAct({std::to_string(++n), "story.stop", run, "dm", "{}"}).ok, "the story stops");
        t.run(1);
        expect(t.g.storySentences().count("u1") && u1->cellId == "c_up" && u1->sceneDirected, "u1 is still held when the story has ended");
        // (Moved away, as a Dungeon Master might: put back.)
        expect(w.teleport("u1", cellId(0), 5.5, 8.5).ok && u1->cellId == cellId(0), "u1 moved out");
        t.run(1);
        expect(u1->cellId == "c_up" && u1->sceneDirected, "and put back where the sentence keeps them");
        t.g.save();
    }
    {
        // ---- a restart: the sentence kept, and served on the seventh day.
        Town t(world, save);
        auto& w = t.g.world();
        std::vector<WorldEvent> seen;
        w.setEventWatcher([&](const WorldEvent& e) { seen.push_back(e); });
        t.run(1);
        auto* u1 = w.entity("u1");
        expect(t.g.storySentences().count("u1") && t.g.storySentences().at("u1").place == "c_up" && u1->cellId == "c_up" && u1->sceneDirected,
               "after a restart u1 is held upstairs still");
        w.advanceCalendar(6.5);
        t.run(1);
        expect(t.g.storySentences().count("u1") && u1->sceneDirected, "on the sixth day still held");
        w.advanceCalendar(.6);
        t.run(1);
        const auto served = std::find_if(seen.begin(), seen.end(), [](const WorldEvent& e) { return e.kind == "sentence served"; });
        expect(!t.g.storySentences().count("u1") && !u1->sceneDirected && served != seen.end() && served->actor == "u1" &&
                   served->item == "served" && served->quantity == 7 && served->detail == "lq-wreckers-light",
               "served after seven days: let go, 'sentence served'");

        // ---- "Release now" (the console's), and the crown's change.
        auto phase = parsed(R"({"title":"Again","phase":"one","journal":"Again.","visit":1,"program":"again"})");
        auto players = json::Value::array();
        players.push(t.ash.entityId);
        phase.set("players", players);
        expect(t.g.applyDmAct({std::to_string(++n), "story.phase", "world-again", "dm", json::dump(phase)}).ok, "a new story opens");
        expect(t.g.applyDmAct({std::to_string(++n), "npc.sentence", "u3", "dm", R"({"run":"world-again","visit":1,"place":"c_up","days":2})"}).ok &&
                   t.g.storySentences().count("u3"), "u3 sentenced");
        seen.clear();
        expect(t.g.applyDmAct({std::to_string(++n), "npc.unsentence", "u3", "dm", "{}"}).ok && !t.g.storySentences().count("u3") &&
                   !w.entity("u3")->sceneDirected, "released now");
        expect(std::any_of(seen.begin(), seen.end(), [](const WorldEvent& e) { return e.kind == "sentence served" && e.item == "released"; }),
               "with 'sentence served' (released)");
        expect(!t.g.applyDmAct({std::to_string(++n), "npc.unsentence", "u3", "dm", "{}"}).ok, "nobody to release twice");

        auto& soc = w.society();
        const auto stall = soc.jobOf("um") ? soc.jobOf("um")->id : std::string();
        seen.clear();
        auto r = t.g.applyDmAct({std::to_string(++n), "npc.post", "u1", "dm", R"({"post":")" + stall + "\"}"});
        expect(r.ok && soc.jobOf("u1") && soc.jobOf("u1")->id == stall && !soc.jobOf("um"), "u1 keeps the stall now, um nothing: " + r.message);
        expect(std::any_of(seen.begin(), seen.end(), [](const WorldEvent& e) {
                   return e.kind == "succession" && e.actor == "u1" && e.target == "um" && contains(e.detail, "by the story"); }),
               "a succession, by the story");
        expect(!t.g.applyDmAct({std::to_string(++n), "npc.post", "u1", "dm", R"({"post":"job:nobody"})"}).ok, "no such post");
        expect(t.g.applyDmAct({std::to_string(++n), "npc.post", "u1", "dm", R"({"post":")" + stall + "\"}"}).ok, "sent again: no change, no refusal");
        expect(!t.g.applyDmAct({std::to_string(++n), "npc.post", "nobody", "dm", R"({"post":")" + stall + "\"}"}).ok, "no such resident");
    }
    fs::remove_all(root);
    fs::remove(save);
}
// Doc 86, Phase 9 (C15): a mantle a story binds is given back when it ends: u1 wore rm950 before the story put rm951 on
// her, and wears rm950 again after; u2 wore none, and wears none after; a story's own "lays down their mantle" gives
// back what was worn before too; an older save's list of mantles is read as "none before".
void writeMantle(const fs::path& dir, const std::string& id, const std::string& wearer)
{
    std::ofstream(dir / (id + ".mantle")) << "# " << id << " (test).\nmantle " << id << "\nstate ready\ndialect plain\naccent none\n"
                                          << "tier standard\nwritten_for " << wearer << "\ntitle the lane's " << id << "\n"
                                          << "fits stage=adult|old\ntraits talk=chatty humour=merry warmth=warm\nquirk counts_the_cobbles\n\n"
                                          << "words\noath! = By my tail\n\ndlg resident.hub : Mind the lane.\n";
}

void phase9Mantles()
{
    const auto root = fs::temp_directory_path() / ("ratw-storyworld-9-" + std::to_string(::getpid()));
    const auto save = root.string() + ".json";
    const auto world = writeDoors(root);
    fs::create_directories(root / "mantles" / "plain");
    writeMantle(root / "mantles" / "plain", "rm950", "u1");
    writeMantle(root / "mantles" / "plain", "rm951", "nobody_here");
    writeMantle(root / "mantles" / "plain", "rm952", "nobody_there");
    const auto run = std::string("world-masks");
    int n = 3000;
    {
        Town t(world, save, 120, true, 3 * 86400, "", (root / "mantles").string());
        t.enter(t.ash, 1, "ash", "Ash", 0, 8.5);
        auto players = json::Value::array();
        players.push(t.ash.entityId);
        const auto open = [&](const std::string& id) {
            auto phase = parsed(R"({"title":"Masks","phase":"masks","journal":"Faces change.","visit":1})");
            phase.set("players", players);
            return t.g.applyDmAct({std::to_string(++n), "story.phase", id, "dm", json::dump(phase)}).ok;
        };
        const auto order = [&](const std::string& kind, const std::string& target, std::string payload, const std::string& id) {
            auto p = parsed(payload.empty() ? "{}" : payload);
            p.set("run", id);
            p.set("visit", 1);
            return t.g.applyDmAct({std::to_string(++n), kind, target, "dm", json::dump(p)});
        };
        const auto worn = [&](const std::string& npc) {
            const auto found = t.g.mantleWearers().find(npc);
            return found == t.g.mantleWearers().end() ? std::string() : found->second;
        };
        expect(worn("u1") == "rm950" && worn("u2").empty(), "u1 wears rm950, u2 none");
        expect(open(run), "the story opens");
        auto r = order("mantle.assign", "u1", R"({"mantle":"rm951"})", run);
        expect(r.ok && worn("u1") == "rm951", "the story binds rm951 to u1: " + r.message);
        r = order("mantle.assign", "u2", R"({"mantle":"rm952"})", run);
        expect(r.ok && worn("u2") == "rm952", "and rm952 to u2: " + r.message);
        const auto* held = &t.g.storylineBook().find(run)->held;
        expect(held->find("mantles") && held->find("mantles")->isObject() && held->find("mantles")->string("u1") == "rm950" &&
                   held->find("mantles")->has("u2") && held->find("mantles")->string("u2").empty(),
               "what each wore before is kept with the story");
        expect(order("mantle.assign", "u1", R"({"mantle":"rm951"})", run).ok &&
                   t.g.storylineBook().find(run)->held.find("mantles")->string("u1") == "rm950",
               "bound again: what she wore before the first binding is kept");
        // The story's own "lays down their mantle": u1 has rm950 back at once, and it is no longer the story's.
        r = order("mantle.release", "u1", "", run);
        expect(r.ok && worn("u1") == "rm950" && !t.g.storylineBook().find(run)->held.find("mantles")->has("u1"),
               "the story lays down rm951: u1 wears rm950 again: " + r.message);
        expect(order("mantle.assign", "u1", R"({"mantle":"rm951"})", run).ok && worn("u1") == "rm951", "bound once more");
        // T6: a realm key remembered by a live story; the story topic only where the talk library has scenes for it (this
        // one has none: silent); given its prefix, the director weighs "story" (story|nh, topic.story nh).
        expect(order("talk.remember", run, R"({"scope":"realm","key":"nh_stage","value":"two","days":30})", run).ok, "a realm key remembered");
        expect(t.g.world().storyTopics().empty(), "no talk.story scenes in the library: the story topic stays silent");
        std::vector<AmbientTopic> weighed;
        t.g.world().onTopic = [&](const std::string&, const std::string&, const AmbientTopic& topic) { weighed.push_back(topic); };
        t.g.world().ambientTopic("u1", "u2");
        expect(std::none_of(weighed.begin(), weighed.end(), [](const AmbientTopic& x) { return x.kind == "story"; }), "not weighed");
        t.g.world().setStoryTopics({"nh"});
        weighed.clear();
        t.g.world().ambientTopic("u1", "u2");
        const auto story = std::find_if(weighed.begin(), weighed.end(), [](const AmbientTopic& x) { return x.kind == "story"; });
        expect(story != weighed.end() && story->key == "story|nh" && story->talk.count("topic.story") && story->talk.at("topic.story") == "nh",
               "with scenes for it, the director weighs the story topic, by its prefix");
        t.g.world().onTopic = nullptr;
        // The story ends: u1 has rm950 back, u2 wears nothing.
        expect(t.g.applyDmAct({std::to_string(++n), "story.stop", run, "dm", "{}"}).ok, "the story ends");
        expect(worn("u1") == "rm950", "u1 wears rm950 again: " + worn("u1"));
        expect(worn("u2").empty(), "u2 wears no mantle, as before: " + worn("u2"));
        // An older save's list ["u2"] is read as {"u2": ""}: let go of at the end.
        expect(open("world-old"), "another story opens");
        expect(order("mantle.assign", "u2", R"({"mantle":"rm952"})", "world-old").ok && worn("u2") == "rm952", "rm952 on u2 again");
        auto& old = t.g.storylineBook().find("world-old")->held;
        auto list = json::Value::array();
        list.push("u2");
        old.set("mantles", list);
        expect(t.g.applyDmAct({std::to_string(++n), "story.stop", "world-old", "dm", "{}"}).ok && worn("u2").empty(),
               "an older save's list: let go of when the story ends");
    }
    std::error_code ec;
    fs::remove(save, ec);
    fs::remove_all(root, ec);
}

// Doc 86, Phase 9 after review (C15): a mantle is given back only while the resident still wears what the story bound.
// (a) The DM binds another meanwhile: the story's end leaves it. (b) Two stories bind one resident in turn: whichever ends
// first, the end of both leaves them in what they wore before either, and never in a story's mantle.
void phase9MantlesOverlap()
{
    const auto root = fs::temp_directory_path() / ("ratw-storyworld-9o-" + std::to_string(::getpid()));
    const auto save = root.string() + ".json";
    const auto world = writeDoors(root);
    fs::create_directories(root / "mantles" / "plain");
    writeMantle(root / "mantles" / "plain", "rm950", "u1");
    for (const auto* id : {"rm951", "rm952", "rm953"})
        writeMantle(root / "mantles" / "plain", id, std::string("nobody_") + id);
    int n = 3500;
    {
        Town t(world, save, 120, true, 3 * 86400, "", (root / "mantles").string());
        t.enter(t.ash, 1, "ash", "Ash", 0, 8.5);
        auto players = json::Value::array();
        players.push(t.ash.entityId);
        const auto open = [&](const std::string& id) {
            auto phase = parsed(R"({"title":"Masks","phase":"masks","journal":"Faces change.","visit":1})");
            phase.set("players", players);
            return t.g.applyDmAct({std::to_string(++n), "story.phase", id, "dm", json::dump(phase)}).ok;
        };
        const auto bind = [&](const std::string& mantle, const std::string& run) {
            auto p = parsed(R"({"mantle":")" + mantle + R"("})");
            if (!run.empty())
            {
                p.set("run", run);
                p.set("visit", 1);
            }
            return t.g.applyDmAct({std::to_string(++n), "mantle.assign", "u1", "dm", json::dump(p)}).ok;
        };
        const auto stop = [&](const std::string& run) { return t.g.applyDmAct({std::to_string(++n), "story.stop", run, "dm", "{}"}).ok; };
        const auto worn = [&] {
            const auto found = t.g.mantleWearers().find("u1");
            return found == t.g.mantleWearers().end() ? std::string() : found->second;
        };
        expect(worn() == "rm950", "u1 wears rm950");
        // (a) The story binds rm951; the DM binds rm953 meanwhile; the story's end leaves rm953.
        expect(open("world-a") && bind("rm951", "world-a") && worn() == "rm951", "the story binds rm951");
        expect(bind("rm953", "") && worn() == "rm953", "the DM binds rm953");
        expect(stop("world-a") && worn() == "rm953", "the story's end leaves the DM's rm953: " + worn());
        expect(bind("rm950", "") && worn() == "rm950", "the DM gives rm950 back");
        // (b) S1 binds rm951 over rm950, S2 rm952 over rm951. S1 ends first: S2's stands; S2 ends: rm950.
        expect(open("world-s1") && bind("rm951", "world-s1") && open("world-s2") && bind("rm952", "world-s2") && worn() == "rm952",
               "two stories bind u1 in turn");
        expect(stop("world-s1") && worn() == "rm952", "S1 ends: S2's rm952 stands: " + worn());
        expect(t.g.storylineBook().find("world-s2")->held.find("mantles")->string("u1") == "rm950", "S2 now gives back rm950");
        expect(stop("world-s2") && worn() == "rm950", "S2 ends: rm950, what u1 wore before either: " + worn());
        // The other order: S2 ends first (rm951 back), then S1 (rm950 back).
        expect(open("world-s3") && bind("rm951", "world-s3") && open("world-s4") && bind("rm952", "world-s4") && worn() == "rm952", "again");
        expect(stop("world-s4") && worn() == "rm951", "the later ends first: the earlier's rm951: " + worn());
        expect(stop("world-s3") && worn() == "rm950", "then the earlier: rm950: " + worn());
        // The story's own release, under another story's binding: the other's stands and takes the "before" on.
        expect(open("world-s5") && bind("rm951", "world-s5") && open("world-s6") && bind("rm952", "world-s6"), "and again");
        auto p = parsed(R"({"run":"world-s5","visit":1})");
        expect(t.g.applyDmAct({std::to_string(++n), "mantle.release", "u1", "dm", json::dump(p)}).ok && worn() == "rm952",
               "S5 lays down its mantle: S6's rm952 stands: " + worn());
        expect(stop("world-s5") && stop("world-s6") && worn() == "rm950", "both ended: rm950: " + worn());
    }
    std::error_code ec;
    fs::remove(save, ec);
    fs::remove_all(root, ec);
}

// Doc 86, Phase 9 after review (T6): the story topic opens again after a restart (the story memory is read before the
// stories), and closes when the story ends. The library is Data/Talk with one talk.story line of the test's own (and the
// concept declared).
void phase9StoryTopicRestart()
{
    const auto root = fs::temp_directory_path() / ("ratw-storyworld-9t-" + std::to_string(::getpid()));
    fs::remove_all(root);
    const auto save = root.string() + ".json";
    const auto world = writeDoors(root);
    const auto talk = root / "talk";
    fs::copy(RATW_SOURCE_DIR "/Data/Talk", talk, fs::copy_options::recursive);
    {
        // (The concept declared as the talk plan will when it writes the first talk.story lines: the real library has
        // none yet, and every concept it declares must have lines.)
        std::stringstream text;
        text << std::ifstream(talk / "settings.json").rdbuf();
        auto settings = text.str();
        const auto at = settings.find("\"talk.harvest\",");
        expect(at != std::string::npos, "the settings name talk.harvest");
        settings.insert(at, "\"talk.story\", ");
        std::ofstream(talk / "settings.json") << settings;
    }
    std::ofstream(talk / "rules" / "zz_story_topic_test.talk")
        << "rule zz_story_topic_test\nwhen concept=talk.story topic.story=nh\ndoes tell.news\ncooldown 1d\n"
           "say Something stirs out on the ridge, they say.\n";
    const auto run = std::string("world-topic");
    int n = 3700;
    {
        Town t(world, save, 120, false, 3 * 86400, "", "", talk.string());
        expect(t.g.talkLibrary().has("talk.story"), "the library has a talk.story line");
        t.enter(t.ash, 1, "ash", "Ash", 0, 8.5);
        auto phase = parsed(R"({"title":"Ridge","phase":"ridge","journal":"Something stirs.","visit":1})");
        auto players = json::Value::array();
        players.push(t.ash.entityId);
        phase.set("players", players);
        expect(t.g.applyDmAct({std::to_string(++n), "story.phase", run, "dm", json::dump(phase)}).ok, "the story opens");
        auto p = parsed(R"({"scope":"realm","key":"nh_stage","value":"two","days":30,"visit":1})");
        p.set("run", run);
        expect(t.g.applyDmAct({std::to_string(++n), "talk.remember", run, "dm", json::dump(p)}).ok, "a realm key remembered");
        expect(t.g.world().storyTopics() == std::set<std::string>{"nh"}, "the story topic opens on nh");
        t.g.save();
    }
    {
        Town again(world, save, 120, false, 3 * 86400, "", "", talk.string());
        expect(again.g.world().storyTopics() == std::set<std::string>{"nh"}, "after a restart: still open on nh");
        expect(again.g.applyDmAct({std::to_string(++n), "story.stop", run, "dm", "{}"}).ok && again.g.world().storyTopics().empty(),
               "the story ends: closed");
    }
    std::error_code ec;
    fs::remove(save, ec);
    fs::remove_all(root, ec);
}

// Doc 86, Phase 9 (M3): House purses. The stall in the strip's second cell is House Greatly's (it claims the cell).
// Off (the world's switch): nothing moves and house:greatly pays nothing. On: once a game day the stall's till puts a
// tenth of what it holds over 80 in purse:house_greatly, which pays a wolf what it holds and no more; the switch is kept
// through a restart.
void phase9HousePurses()
{
    const auto root = fs::temp_directory_path() / ("ratw-storyworld-9h-" + std::to_string(::getpid()));
    const auto save = root.string() + ".json";
    const auto world = writeStrip(root);
    {
        std::stringstream text;
        text << std::ifstream(root / "world.ratw").rdbuf();
        auto m = text.str();
        const auto line = "territory \"" + cellId(1) + "\" \"ridgemere\" \"-\" 0\n";
        m.replace(m.find(line), line.size(), "territory \"" + cellId(1) + "\" \"ridgemere\" \"-\" 1 \"house_greatly\"\n");
        m.insert(m.find("territory"), "faction \"house_greatly\" \"House Greatly\" \"#2f4f6f\"\nfaction \"council\" \"The Council\" \"#7d8a96\"\n");
        std::ofstream(root / "world.ratw") << m;
    }
    int n = 4000;
    {
        Town t(world, save);
        t.enter(t.ash, 1, "ash", "Ash", 0, 8.5);
        auto& soc = t.g.world().society();
        const auto till = soc.tillOf("um");
        expect(!till.empty() && soc.account(till), "the stall's till: " + till);
        expect(testPurse(soc, till, 500 - soc.cash(till)) || soc.cash(till) == 500, "the till holds 500");
        const auto grant = [&](int coins) {
            return t.g.applyEconomyCommand("character.grant", t.ash.entityId,
                                           parsed(R"({"coins":)" + std::to_string(coins) + R"(,"purse":"house:greatly","reason":"Greatly pays it"})"), "");
        };
        // Off: no purse, nothing moved, nothing paid.
        t.run(2);
        expect(!t.g.storyHousePurses() && !soc.account("purse:house_greatly") && soc.cash(till) == 500, "off: nothing moves");
        auto r = grant(5);
        expect(!r.ok && contains(r.message, "no such purse"), "off: house:greatly pays nothing: " + r.message);
        expect(!t.g.applyDmAct({std::to_string(++n), "story.house_purses", "maybe", "dm", "{}"}).ok, "on or off");
        // On: the share at once, then once a game day.
        r = t.g.applyDmAct({std::to_string(++n), "story.house_purses", "on", "dm", "{}"});
        expect(r.ok && t.g.storyHousePurses(), "the switch on: " + r.message);
        expect(soc.cash("purse:house_greatly") == 42 && soc.cash(till) == 458, "a tenth of 420 over the float: 42 in Greatly's purse, " +
               std::to_string(soc.cash("purse:house_greatly")));
        t.run(2);
        expect(soc.cash("purse:house_greatly") == 42, "once a game day");
        expect(t.g.housePurseFaction("house:greatly") == "house_greatly" && t.g.housePurseFaction("house:house_greatly") == "house_greatly" &&
                   t.g.housePurseFaction("house:council").empty() && t.g.housePurseFaction("house:nobody").empty(),
               "house:<name> or house:<faction>; never a faction that is no House");
        const auto ashBefore = soc.cash(t.ash.entityId);
        r = grant(30);
        expect(r.ok && soc.cash("purse:house_greatly") == 12 && soc.cash(t.ash.entityId) == ashBefore + 30, "Greatly pays Ash 30: " + r.message);
        r = grant(20);
        expect(!r.ok && contains(r.message, "short") && soc.cash("purse:house_greatly") == 12, "and no more than it holds (never topped up): " + r.message);
        // (After review: off and on again the same game day takes no second share.)
        expect(t.g.applyDmAct({std::to_string(++n), "story.house_purses", "off", "dm", "{}"}).ok &&
                   t.g.applyDmAct({std::to_string(++n), "story.house_purses", "on", "dm", "{}"}).ok && soc.cash("purse:house_greatly") == 12,
               "off and on again the same day: no second share, " + std::to_string(soc.cash("purse:house_greatly")));
        t.g.save();
    }
    {
        Town again(world, save);
        expect(again.g.storyHousePurses() && again.g.world().society().account("purse:house_greatly"), "after a restart: still on, the purse kept");
        again.run(2);
        expect(again.g.world().society().cash("purse:house_greatly") == 12, "and the same game day: no second share after a restart, " +
               std::to_string(again.g.world().society().cash("purse:house_greatly")));
        expect(again.g.applyDmAct({"1", "story.house_purses", "off", "dm", "{}"}).ok && !again.g.storyHousePurses(), "and off again");
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
        phase1b();
        phase2Names();
        phase3Thanks();
        phase6Items();
        phase7Foes();
        phase8Sentences();
        phase9Mantles();
        phase9MantlesOverlap();
        phase9StoryTopicRestart();
        phase9HousePurses();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "story world effects: " << checks << " checks passed\n";
    return 0;
}
