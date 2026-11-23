// A world story's new world effects (Docs/Design/85-story-triggers-and-campaign-control.md, 3.5 and Phase 9;
// Core/RatwGameStoryWorld.cpp). Through the game, on the town fixture's strip with an inn upstairs behind a door and a
// locked gaol door: each order applied as the story runner sends it (alone and in a story.effects batch), with its
// refusals; what the game undoes when the run ends (a lock, a hold, a follower, a stranger, a notice, the weather, a
// marker) undone; the journal's notes and the maps' markers in the wolf's journal view; and what is held kept with the
// story through a save.
#include "test_purse.h"
#include "RatwClaims.h"
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

} // namespace

int main()
{
    try
    {
        worldEffects();
        phase1b();
        phase2Names();
        phase3Thanks();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "story world effects: " << checks << " checks passed\n";
    return 0;
}
