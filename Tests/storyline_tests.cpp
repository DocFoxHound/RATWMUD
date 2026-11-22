// Storylines and the journal (Docs/Design/58-player-storytellers.md). Phase 1: the pure rules (begun with step 1
// ticked; place, talk and scene objectives; parallel parts and two angles; take; abandon; the limit; save and load);
// then through the game: a Dungeon Master gives Ash "The debt"; her journal shows it with its marker; talking to the
// resident, unrecognised, ticks a step; walking to the place ticks the next; a restart keeps it.
#include "test_purse.h"
#include "RatwGame.h"
#include "RatwStorylines.h"
#include "RatwWorld.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
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

const char* Sample = R"({"templates": [
  {"id": "walk", "title": "A walk with {friend}", "steps": [
    {"title": "Begun", "objectives": [{"kind": "told"}]},
    {"title": "To the mill", "marker": "place", "objectives": [{"kind": "place", "target": "{place}", "x": 4, "y": 4, "radius": 2, "line": "Go"}]},
    {"title": "Two ways", "distinct": true, "objectives": [{"kind": "talk", "target": "{miller}", "line": "Ask"}, {"kind": "place", "target": "{place}", "line": "Look"}]},
    {"title": "Together", "objectives": [{"kind": "scene", "count": 2, "line": "A scene"}]}]}]})";

void pure()
{
    using namespace storylines;
    const auto t = parseTemplates(Sample);
    expect(t.size() == 1 && t[0].steps.size() == 4, "the sample template reads");
    Book book;
    std::string why;
    const auto marker = [](const std::string& role, const std::string& id) { return Marker{id, "the mill", 4, 4, 0}; };
    auto* s = book.begin(t[0], "ash", {"bo"}, {{"friend", "bo"}, {"place", "mill"}, {"miller", "wren"}}, "dm", "", 100, marker, why);
    expect(s && s->steps[0].done() && s->current() == 1, "begun, with step 1 ticked: " + why);
    expect(s->title == "A walk with bo" && s->participants.size() == 2, "its cast filled, both in it");
    expect(s->steps[1].marker.cell == "mill" && s->steps[1].marker.label == "the mill", "the step's marker");
    const auto id = s->id;
    expect(book.waiting("place") && !book.waiting("talk"), "only the current step waits");
    // Out of range, then in.
    expect(book.happen({"place", "ash", "mill", {}, {}, 9, 9, {}}, 101).empty(), "too far from the tile");
    expect(book.happen({"place", "cy", "mill", {}, {}, 4, 4, {}}, 101).empty(), "not one of its wolves");
    auto p = book.happen({"place", "ash", "mill", {}, {}, 4.5, 4, {}}, 102);
    expect(p.size() == 2 && p[1].kind == "step" && book.find(id)->current() == 2, "there: step 2 done");
    // Two angles: Ash can't do both parts.
    expect(book.take(id, 2, 0, "bo") && book.find(id)->steps[2].objectives[0].takenBy == "bo", "Bo takes the talking");
    book.happen({"place", "ash", "mill", {}, {}, 1, 1, {}}, 103);
    book.happen({"talk", "ash", "mill", "wren", {}, 0, 0, {}}, 104);
    expect(!book.find(id)->steps[2].done(), "Ash, who looked, can't also ask");
    p = book.happen({"talk", "bo", "mill", "wren", {}, 0, 0, {}}, 105);
    expect(book.find(id)->current() == 3, "Bo asks: the step is done");
    // A scene with both.
    expect(book.happen({"scene", "ash", "inn", {}, {}, 0, 0, {"ash", "cy"}}, 106).empty(), "a scene without Bo doesn't count");
    p = book.happen({"scene", "ash", "inn", {}, {}, 0, 0, {"ash", "bo"}}, 107);
    expect(!p.empty() && p.back().kind == "done" && book.find(id)->state == "done", "together: done");
    // The limit, abandon, save and load.
    for (int i = 0; i < 3; ++i)
        expect(book.begin(t[0], "cy", {}, {{"friend", "x"}, {"place", "mill"}, {"miller", "wren"}}, "dm", "", 200, marker, why) != nullptr, "cy begins one");
    expect(!book.begin(t[0], "cy", {}, {{"friend", "x"}, {"place", "mill"}, {"miller", "wren"}}, "dm", "", 200, marker, why), "a fourth is refused");
    expect(!book.begin(t[0], "dee", {}, {{"place", "mill"}}, "dm", "", 200, marker, why), "an incomplete cast is refused");
    const auto saved = book.save();
    Book again;
    again.load(saved);
    expect(again.find(id) && again.find(id)->state == "done" && again.find(id)->steps[2].objectives[0].doneBy == "bo", "saved and loaded");
    expect(again.counting("cy", "personal") == 3, "with its counts");
    auto& added = again.add(Storyline{});
    expect(added.id.rfind("story-", 0) == 0 && added.id != id, "ids go on from the loaded");
}

void say(Town& t, Client& c, const std::string& text, const std::string& to)
{
    if (!to.empty())
    {
        sayInWindow(t, c, to, text);               // (Said to a resident: in their window, doc 84 Phase 12.)
        return;
    }
    auto command = parsed(cmd({{"type", "chat"}, {"text", text}, {"channel", "ic"}, {"volume", "speak"}}));
    auto targets = json::Value::array();
    targets.push(to);
    command.add("targets", targets);
    t.g.command(&c, json::dump(command));
    t.g.settle();
    t.run(1);
}

void privateCameraAndMovement()
{
    const auto root = fs::temp_directory_path() / ("ratw-private-scene-" + std::to_string(::getpid()));
    const auto save = root.string() + ".json";
    {
        const auto manifest = writeStrip(root);
        const auto paintTile = [&](const std::string& cell, int x, int y, char tile) {
            const auto path = root / "cells" / (cell + ".cell");
            std::ifstream file(path);
            std::string terrain{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
            terrain.at(terrain.find("grid:\n") + 6 + y * (Side + 1) + x) = tile;
            std::ofstream(path) << terrain;
        };
        paintTile("c_7_0", 5, 6, '#'); // Camera (4,6), wall, tile (6,6).
        paintTile("c_1_0", 9, 11, '+');
        paintTile("c_2_0", 4, 4, '+');
        std::ofstream(root / "world.ratw", std::ios::app)
            << R"(door "scene-door" "Hall door" "c_1_0" 9.5 11.5 "c_2_0" 3.5 4.5 "scene-back" 1 0 0 0 "-")" << '\n'
            << R"(door "scene-back" "Hall door" "c_2_0" 4.5 4.5 "c_1_0" 8.5 11.5 "scene-door" 1 0 0 0 "-")" << '\n';
        Town t(manifest, save);
        t.enter(t.ash, 1, "ash", "Ash", 1, 8.5);
        t.enter(t.bo, 2, "bo", "Bo", 1, 10.5);
        t.place(t.ash, 1, 8.5, 11.5);
        const auto run = "world-camera";
        auto phase = parsed(R"({"title":"Audience","phase":"waiting","journal":"Hear the runner.","visit":1})");
        auto players = json::Value::array();
        players.push(t.ash.entityId);
        players.push(t.bo.entityId);
        phase.add("players", players);
        expect(t.g.applyDmAct({"1","story.phase",run,"dm",json::dump(phase)}).ok, "The camera story opens");
        auto payload = parsed(R"({"cue":"report","visit":1,"scene":{"format":"ratw-scene","version":1,"id":"audience",
            "title":"Audience","visibility":"individual","lockMovement":true,"camera":{"cell":"c_7_0","x":4,"y":6},
            "cast":[{"id":"runner","npc":"u1"}],"start":"ask","beats":[{"id":"ask","type":"choice","speaker":"runner",
            "text":"Which gate needs runners?","seconds":300,"mode":"first","options":[{"id":"east","label":"East"},
            {"id":"west","label":"West"}],"default":"west"}]}})");
        payload.add("player",t.ash.entityId);
        auto& world = t.g.world();
        auto* ash = world.entity(t.ash.entityId);
        const auto cell = ash->cellId;
        const auto position = ash->position;
        world.observe(ash->id);
        const auto knewCameraCell = world.memories(ash->id).count("c_7_0");
        const auto prior = world.moveTo(ash->id, 5.5, 11.5);
        expect(prior.ok && !ash->path.empty(), "A queued route is present as the scene begins");
        t.place(t.bo, 1, position.x, position.y);
        const auto started = t.g.applyDmAct({"2","story.scene",run,"dm",json::dump(payload)});
        expect(started.ok, "A private viewing starts: " + started.message);
        auto view = t.selfOf(t.ash)["scene"];
        expect(view.string("beat") == "ask" && view.boolean("lockMovement"), "The addressed player sees the choice");
        expect(view.object("frame").string("cell") == "c_7_0" && !view.object("frame").array("rows").empty() &&
               world.cell("c_7_0")->loaded, "A remote camera returns authored terrain from its own cell");
        expect(view.object("frame").array("rows").at(6).asString().at(6) == ' ' &&
               !view.object("speaker").has("cell") && view.array("actors").empty(),
               "A camera cannot reveal the tile behind the wall or the off-camera speaker's position");
        expect(knewCameraCell == world.memories(ash->id).count("c_7_0"), "Watching does not mark the camera cell as explored");
        expect(!t.selfOf(t.bo).has("scene"), "Another player at the same spot cannot see a private camera or dialogue");
        const auto* bo = world.entity(t.bo.entityId);
        expect(ash->path.empty() && ash->position.x == position.x && ash->position.y == position.y && ash->cellId == cell &&
               (bo->position.x != position.x || bo->position.y != position.y),
               "Opening the shot cancels the old route and moves the overlapping bystander off the anchored viewer");
        expect(world.door("scene-door") != nullptr, "The fixture has an accessible door");
        t.g.command(&t.ash, R"({"type":"action","target":"scene-door","action":"enter"})");
        expect(ash->cellId == cell, "The door cannot move a locked character");
        t.g.command(&t.ash, R"({"type":"path","x":11,"y":8})");
        t.g.command(&t.ash, R"({"type":"move","x":1,"y":0})");
        t.g.pose(&t.ash, ash->poseSeq + 1, position.x + .4, position.y, 0, 1, 0);
        t.run(.1);
        expect(ash->position.x == position.x && ash->position.y == position.y && ash->path.empty(),
               "Neither input channel nor a new route moves a locked character");
        auto ballot = parsed(R"({"type":"scene_vote","option":"east"})");
        ballot.add("beat",view.string("beat"));
        ballot.add("instance",view.string("instance"));
        ballot.add("actor",t.ash.entityId); // The signed-in connection owns the vote.
        t.g.command(&t.bo,json::dump(ballot));
        expect(t.selfOf(t.ash).object("scene").string("beat") == "ask", "A bystander cannot cast the player's ballot");
        t.g.command(&t.ash,json::dump(ballot));
        expect(!t.selfOf(t.ash).has("scene") && !t.g.cinematicLocks(ash->id), "The player's vote ends the scene and releases input");
        t.place(t.bo, 1, position.x, position.y);
        t.run(.1);
        expect(ash->position.x != position.x || ash->position.y != position.y, "Both bodies participate in collisions again after release");
        t.place(t.bo, 1, 11.5, 11.5);
        t.place(t.ash, 1, position.x, position.y);
        t.g.command(&t.ash, R"({"type":"path","x":11,"y":8})");
        expect(!ash->path.empty(), "The ordinary movement command works again");
        t.g.command(&t.ash, R"({"type":"action","target":"scene-door","action":"enter"})");
        expect(ash->cellId == "c_2_0", "The same door admits the player after the scene");

        auto publicScene = payload["scene"];
        publicScene.set("visibility","public");
        publicScene.set("area",parsed(R"({"cell":"c_1_0","x1":8,"y1":11,"x2":8,"y2":11})"));
        // The vote leads to a long closing line, so the scene goes on after the viewer has taken part.
        auto beats = publicScene["beats"];
        auto ask = beats.items().at(0);
        auto options = ask["options"];
        for (auto& o : options.items()) o.set("next","close");
        ask.set("options",options);
        beats.items().at(0) = ask;
        beats.push(parsed(R"({"id":"close","type":"line","speaker":"runner","text":"Then go with the Bright One.","seconds":600})"));
        publicScene.set("beats",beats);
        payload.set("scene",publicScene);
        payload.set("player","world");
        payload.set("cue","crowd");
        const auto audienceView = [&] {
            t.run(1); // Four fixture ticks cover every connection's staggered snapshot slot.
            return t.ash.snapshots.back()["self"]["scene"];
        };
        t.place(t.ash, 1, 8.5, 11.5);
        const auto crowd = t.g.applyDmAct({"3","story.scene",run,"dm",json::dump(payload)});
        expect(crowd.ok, "A public scene opens with Ash in the square");
        expect(audienceView().isObject() && !audienceView().boolean("lockMovement") && !t.g.cinematicLocks(ash->id),
               "Standing in a public scene shows it but holds no one who has not taken part");
        auto crowdBallot = parsed(R"({"type":"scene_vote","option":"east"})");
        crowdBallot.add("beat","ask");
        crowdBallot.add("instance",crowd.targetId);
        t.g.command(&t.ash,json::dump(crowdBallot));
        expect(audienceView().string("beat") == "close" && t.g.cinematicLocks(ash->id), "Voting is taking part: the voter is held");
        // At the southeast-facing corner of the collision pair, an ordinary body push would carry Ash over x=9.
        t.place(t.ash, 1, 8.99, 11.01);
        t.place(t.bo, 1, 8.89, 10.99); // Touching from outside the viewing area.
        expect(audienceView().boolean("lockMovement") && ash->position.x == 8.99 && ash->position.y == 11.01,
               "The crowd beyond the public boundary cannot push an audience member out of the scene");
        t.place(t.bo, 1, 11.5, 11.5);
        auto& joined = *t.g.storylineBook().find(run)->cinematics.at(crowd.targetId).find("joined");
        const auto since = joined.number(ash->id);
        joined.set(ash->id, since - ratw::game::Game::kSceneLockSeconds);
        expect(audienceView().isObject() && !audienceView().boolean("lockMovement") && !t.g.cinematicLocks(ash->id),
               "No one is held past the lock's cap, however long the scene runs");
        joined.set(ash->id, since);
        expect(t.g.cinematicLocks(ash->id), "A participant within the cap is held");
        t.place(t.ash, 1, 7.5, 11.5);
        expect(audienceView().isNull() && !t.g.cinematicLocks(ash->id), "Leaving the area releases the public lock");
        t.place(t.ash, 1, 8.99, 11.5);
        expect(audienceView().boolean("lockMovement"), "A participant's reentry reacquires the current scene and its lock");
        t.place(t.ash, 1, 7.5, 11.5);
        t.place(t.bo, 1, 8.5, 11.5);
        t.run(1);
        expect(t.bo.snapshots.back()["self"]["scene"].isObject() && !t.g.cinematicLocks(t.bo.entityId),
               "A passer-by walking in later sees the scene and is not held");
    }
    fs::remove(save);
    fs::remove_all(root);
}

void privateSceneQueue()
{
    const auto root = fs::temp_directory_path() / ("ratw-private-queue-" + std::to_string(::getpid()));
    const auto file = root.string() + ".json";
    const auto world = writeStrip(root);
    std::string ash, bo, first;
    {
        Town t(world, file);
        t.enter(t.ash, 1, "ash", "Ash", 1, 8.5);
        t.enter(t.bo, 2, "bo", "Bo", 1, 10.5);
        ash=t.ash.entityId; bo=t.bo.entityId;
        const auto run="world-private-queue";
        auto phase=parsed(R"({"title":"Interviews","phase":"hear","journal":"State your case.","visit":1})");
        auto names=json::Value::array(); names.push(ash); names.push(bo); phase.add("players",names);
        expect(t.g.applyDmAct({"1","story.phase",run,"dm",json::dump(phase)}).ok,"private hearing opens");
        auto payload=parsed(R"({"visit":1,"cue":"first","scene":{"format":"ratw-scene","version":1,"id":"hearing","title":"Give an account","visibility":"individual","lockMovement":true,"cast":[{"id":"scribe","npc":"u1"}],"start":"ask","beats":[{"id":"ask","type":"response","speaker":"scribe","text":"Tell me about the crossing.","seconds":120,"mode":"first"}]}})");
        auto launch=[&](const std::string& who,const char* cue) {
            payload.set("player",who); payload.set("cue",cue);
            return t.g.applyDmAct({"2","story.scene",run,"dm",json::dump(payload)});
        };
        expect(launch(ash,"first").ok,"Ash can enter the first hearing");
        first=t.g.cinematicView(ash).string("instance");
        expect(!first.empty() && launch(ash,"second").ok && launch(bo,"other").ok,"a second private hearing and a disjoint hearing accepted");
        const auto other=t.g.cinematicView(bo);
        expect(other.string("instance")!=first && other.string("beat")=="ask" && t.g.cinematicView(ash).string("instance")==first,
               "disjoint actors play concurrently and an overlapping instance waits");
        expect(!t.g.cinematicReply(bo,first,"ask","Someone else's answer.").ok,"a private scene rejects another actor's input");
        t.g.save();
    }
    {
        Town t(world,file);
        t.enter(t.ash, 1, "ash", "Ash", 1, 8.5);
        expect(t.g.cinematicView(ash).string("instance")==first,"recover both the current and waiting private instances");
        std::vector<WorldEvent> promptEvents;
        t.g.world().setEventWatcher([&](const WorldEvent& e) { if (e.kind=="scene response") promptEvents.push_back(e); });
        const std::string accented=u8"é";
        std::string full;
        for (int i=0;i<280;++i) full+=accented;
        expect(!t.g.cinematicReply(ash,first,"ask",full+accented).ok && !t.g.cinematicReply(ash,first,"ask",accented+'\x01').ok,
               "reply bound counts Unicode characters and rejects embedded control bytes");
        expect(t.g.cinematicReply(ash,first,"ask",full).ok,"a full 280-character Unicode reply is accepted");
        t.g.tick(.05);
        auto second=t.g.cinematicView(ash);
        expect(second.string("instance")!=first && second.string("beat")=="ask" && second.number("remainingMs")>118000,
               "the waiting prompt starts with its own reading time");
        expect(!contains(json::dump(t.g.storylineBook().save()),full),"answers without an AI consumer are not retained");
        expect(promptEvents.size()==1 && promptEvents.front().detail=="first.ask.submitted" &&
               promptEvents.front().actor==ash && promptEvents.front().target=="world-private-queue" && promptEvents.front().quantity==1,
               "resolving a response emits only the owning cue, beat and outcome");
        // Simulate a restored deadline already in the past; the waiting instance gets the timeout outcome once.
        auto* play=t.g.storylineBook().find("world-private-queue");
        play->cinematics.at(second.string("instance")).set("end",1.);
        t.g.tick(.05);
        expect(promptEvents.size()==2 && promptEvents.back().detail=="second.ask.timeout", "a missed response branches on timeout");
        t.g.world().setEventWatcher({});
        expect(t.g.applyDmAct({"3","story.stop","world-private-queue","dm","{}"}).ok && !t.g.cinematicLocks(ash) && !t.g.cinematicLocks(bo),
               "stopping the owning story cancels every instance");
    }
    fs::remove_all(root);
    fs::remove(file);
}

// In a scene each speaker has their own panel with their face (the user, 2026-11-20): the resident's lines and each
// player's typed reply reach the scene's audience as lines with the speaker's id, name and look, newest last; the
// players' words are never written into the saved story book.
void scenePanels()
{
    const auto root = fs::temp_directory_path() / ("ratw-scene-panels-" + std::to_string(::getpid()));
    const auto file = root.string() + ".json";
    {
        Town t(writeStrip(root), file);
        t.enter(t.ash, 1, "ash", "Ash", 1, 8.5);
        t.enter(t.bo, 2, "bo", "Bo", 1, 10.5);
        const auto ash = t.ash.entityId, bo = t.bo.entityId;
        t.place(t.ash, 1, 8.5, 8.5);
        t.place(t.bo, 1, 10.5, 8.5);
        const auto run = "world-scene-panels";
        expect(t.g.applyDmAct({"1", "story.phase", run, "dm",
                               R"({"title":"Oath","phase":"square","journal":"Gather in the square.","visit":1})"}).ok, "ceremony visit opens");
        auto payload = parsed(R"({"visit":1,"player":"world","cue":"oath","scene":{"format":"ratw-scene","version":1,"id":"oath",
            "title":"The oath","visibility":"public","lockMovement":true,"cast":[{"id":"captain","npc":"u1"}],"start":"ask",
            "beats":[{"id":"ask","type":"response","speaker":"captain","text":"Who keeps the ford tonight?","seconds":120,"mode":"all","next":"close"},
                     {"id":"close","type":"line","speaker":"captain","text":"Then the ford is kept.","seconds":600}]}})");
        auto scene = payload["scene"];
        auto area = json::Value::object();
        area.add("cell", cellId(1)); area.add("x1", 7); area.add("y1", 8); area.add("x2", 11); area.add("y2", 8);
        scene.set("area", area);
        payload.set("scene", scene);
        const auto started = t.g.applyDmAct({"2", "story.scene", run, "dm", json::dump(payload)});
        expect(started.ok, "ceremony starts: " + started.message);
        const auto captain = t.g.world().entity("u1")->name;
        auto lines = t.g.cinematicView(bo).array("lines");
        expect(lines.size() == 1 && lines[0].string("kind") == "resident" && lines[0].string("speakerId") == "u1" &&
               lines[0].string("name") == captain && lines[0].has("appearance") && lines[0].string("text") == "Who keeps the ford tonight?",
               "the resident's line is in the resident's own panel");
        expect(t.g.cinematicReply(ash, started.targetId, "ask", "I will, with Bo.").ok, "Ash replies");
        lines = t.g.cinematicView(bo).array("lines");
        expect(lines.size() == 2 && lines[1].string("kind") == "player" && lines[1].string("speakerId") == ash &&
               lines[1].string("name") == "Ash" && lines[1].has("appearance") && lines[1].string("text") == "I will, with Bo." &&
               !lines[1].boolean("self"), "Ash's reply is in Ash's own panel, with Ash's face, for Bo");
        expect(t.g.cinematicView(ash).array("lines").at(1).boolean("self"), "and in Ash's own panel for Ash");
        expect(!contains(json::dump(t.g.storylineBook().save()), "I will, with Bo."), "a typed reply is never saved with the story");
        expect(t.g.cinematicReply(bo, started.targetId, "ask", "Aye.").ok, "Bo replies, and the beat resolves");
        lines = t.g.cinematicView(ash).array("lines");
        expect(t.g.cinematicView(ash).string("beat") == "close" && lines.size() == 4 && lines[2].string("speakerId") == bo &&
               lines[3].string("text") == "Then the ford is kept." && lines[3].string("kind") == "resident",
               "several speakers: several panels, oldest first and the newest last");
        t.place(t.ash, 1, 8.5, 12.5);
        expect(t.g.cinematicView(ash).isNull(), "outside the square the scene's panels are not shown");
    }
    fs::remove_all(root);
    fs::remove(file);
}

// A locked viewer set upon by a resident (a story's bandits, say) fights as anyone does; the lock returns once it is over.
void sceneLockYieldsToFight()
{
    const auto root = fs::temp_directory_path() / ("ratw-scene-fight-" + std::to_string(::getpid()));
    const auto file = root.string() + ".json";
    {
        Town t(writeStrip(root), file);
        t.enter(t.ash, 1, "ash", "Ash", 1, 8.5);
        auto& world = t.g.world();
        const auto run = "world-scene-fight";
        auto phase = parsed(R"({"title":"Hearing","phase":"hear","journal":"State your case.","visit":1})");
        auto names = json::Value::array();
        names.push(t.ash.entityId);
        phase.add("players", names);
        expect(t.g.applyDmAct({"1", "story.phase", run, "dm", json::dump(phase)}).ok, "hearing opens");
        auto payload = parsed(R"({"visit":1,"cue":"hear","scene":{"format":"ratw-scene","version":1,"id":"hearing","title":"Give an account","visibility":"individual","lockMovement":true,"cast":[{"id":"scribe","npc":"u1"}],"start":"ask","beats":[{"id":"ask","type":"response","speaker":"scribe","text":"Tell me about the crossing.","seconds":300,"mode":"first"}]}})");
        payload.set("player", t.ash.entityId);
        expect(t.g.applyDmAct({"2", "story.scene", run, "dm", json::dump(payload)}).ok && t.g.cinematicLocks(t.ash.entityId),
               "the private scene locks its viewer");
        const auto* ash = world.entity(t.ash.entityId);
        expect(world.teleport("u2", ash->cellId, ash->position.x + 1, ash->position.y).ok, "a resident beside the viewer");
        const auto fight = world.attack("u2", t.ash.entityId);
        expect(fight.ok && world.inBattle(t.ash.entityId), "the resident sets upon the viewer: " + fight.message);
        expect(!t.g.cinematicLocks(t.ash.entityId), "a fight releases the scene's lock");
        t.run(.1);
        expect(t.g.cinematicView(t.ash.entityId).isObject(), "the scene still plays for the fighter");
        t.ash.events.clear();
        t.g.command(&t.ash, R"({"type":"battle","verb":"wait"})");
        const bool held = std::any_of(t.ash.events.begin(), t.ash.events.end(),
                                      [](const json::Value& m) { return json::dump(m).find("holding position for the scene") != std::string::npos; });
        expect(!held, "the fighter's battle turn is not refused by the scene");
    }
    fs::remove_all(root);
    fs::remove(file);
}

// A linked-AI line is asked of the Mind only when some player can hear it: an empty square plays the authored line.
void sceneAiNeedsAnAudience()
{
    const auto root = fs::temp_directory_path() / ("ratw-scene-ai-" + std::to_string(::getpid()));
    const auto file = root.string() + ".json";
    {
        Town t(writeStrip(root), file, 120, false, 3 * 86400, "http://127.0.0.1:9/dialogue"); // Plugged in, never answers.
        t.enter(t.ash, 1, "ash", "Ash", 1, 8.5);
        const auto run = "world-scene-ai";
        expect(t.g.applyDmAct({"1", "story.phase", run, "dm",
                               R"({"title":"Oath","phase":"square","journal":"Gather in the square.","visit":1})"}).ok, "ceremony visit opens");
        auto payload = parsed(R"({"visit":1,"player":"world","scene":{"format":"ratw-scene","version":1,"id":"oath","title":"The oath",
            "visibility":"public","lockMovement":false,"cast":[{"id":"captain","npc":"u1"}],"start":"speak",
            "beats":[{"id":"speak","type":"line","speaker":"captain","text":"Stand with us.","seconds":5,
                      "ai":{"personality":"Solemn.","intent":"Welcome the watch."}}]}})");
        const auto open = [&](const char* cue, int x1, int x2) {
            payload.set("cue", cue);
            auto scene = payload["scene"];
            auto area = json::Value::object();
            area.add("cell", cellId(1)); area.add("x1", x1); area.add("y1", 8); area.add("x2", x2); area.add("y2", 8);
            scene.set("area", area);
            payload.set("scene", scene);
            const auto started = t.g.applyDmAct({"2", "story.scene", run, "dm", json::dump(payload)});
            expect(started.ok, "ceremony starts: " + started.message);
            return t.g.storylineBook().find(run)->cinematics.at(started.targetId);
        };
        const auto empty = open("empty", 1, 2); // Ash stands at (8, 8), outside.
        expect(!empty.boolean("generating") && !empty.has("aiPending") && empty.string("text") == "Stand with us.",
               "nobody in the square: the authored line plays and the Mind is not asked");
        const auto heard = open("heard", 7, 9);
        expect(heard.boolean("generating") && heard.has("aiPending"), "a player in the square: the live Mind is asked for the line");
    }
    fs::remove_all(root);
    fs::remove(file);
}

void storyAreaEffects()
{
    const auto root = fs::temp_directory_path() / ("ratw-storyareas-" + std::to_string(::getpid()));
    const auto save = root.string() + ".json";
    {
        Town t(writeStrip(root), save);
        t.enter(t.ash, 1, "ash", "Ash", 1, 9.99); // Last tile inside the region.
        t.enter(t.bo, 2, "bo", "Bo", 1, 10.0);    // Member just outside.
        t.enter(t.cy, 3, "cy", "Cy", 1, 8.5);     // Visitor inside.
        auto& world = t.g.world();
        const auto run = "world-area-test";
        auto phase = parsed(R"({"title":"Breach","phase":"defend","journal":"Hold the wall.","visit":1})");
        auto members = json::Value::array();
        members.push(t.ash.entityId);
        members.push(t.bo.entityId);
        phase.set("players", members);
        phase.set("regions", parsed(R"([{"id":"wall","cell":"c_1_0","x1":7,"x2":9,"y1":8,"y2":8}])"));
        expect(t.g.applyDmAct({"1", "story.phase", run, "dm", json::dump(phase)}).ok, "area story started");
        // story.tell (doc 85, Phase 2): the story's wolves, one of them, or those in a region; never Cy, who is not in it.
        auto tell = [&](const std::string& extra) {
            return t.g.applyDmAct({"9", "story.tell", run, "dm", R"({"visit":1,"title":"Breach","text":"Hold fast.")" + extra + "}"});
        };
        const auto everyone = tell(R"(,"to":"participants")");
        expect(everyone.ok && everyone.message == "Told 2 in the world.", "told the story's two wolves: " + everyone.message);
        expect(tell(R"(,"to":"wolf","player":")" + t.bo.entityId + "\"").message == "Told 1 in the world.", "told one wolf");
        expect(!tell(R"(,"to":"wolf","player":")" + t.cy.entityId + "\"").ok, "a wolf outside the story is refused");
        const auto inside = tell(R"(,"to":"region","region":{"id":"wall","cell":"c_1_0","x1":7,"x2":9,"y1":8,"y2":8})");
        expect(inside.ok && inside.message == "Told 1 in the world.", "only Ash stands on the wall: " + inside.message);
        expect(!tell(R"(,"to":"region","region":{"id":"wall","cell":"c_1_0","x1":6,"x2":9,"y1":8,"y2":8})").ok, "an altered region is refused");
        expect(!t.g.applyDmAct({"9", "story.tell", run, "dm", R"({"visit":2,"to":"participants","text":"Late."})"}).ok, "another visit is refused");
        expect(!t.g.applyDmAct({"9", "story.tell", run, "dm", R"({"visit":1,"to":"participants","text":"two\nlines"})"}).ok,
               "one line only");
        // A revision (doc 85, Phase 5b): the run moved onto a new version keeps its visit; the game takes the new words and
        // regions for it, checked whole first.
        auto revised = phase;
        revised.set("revise", true);
        revised.set("journal", "Hold the gate.");
        revised.set("title", "Breach, revised");
        revised.set("regions", parsed(R"([{"id":"wall","cell":"c_1_0","x1":6,"x2":9,"y1":8,"y2":8}])"));
        auto badFit = revised;
        badFit.set("regions", parsed(R"([{"id":"wall","cell":"c_1_0","x1":6,"x2":9000,"y1":8,"y2":8}])"));
        expect(!t.g.applyDmAct({"7", "story.phase", run, "dm", json::dump(badFit)}).ok &&
               t.g.storylineBook().find(run)->steps.back().text == "Hold the wall.", "a revision that does not fit changes nothing");
        auto otherVisit = revised;
        otherVisit.set("visit", 2);
        expect(!t.g.applyDmAct({"7", "story.phase", run, "dm", json::dump(otherVisit)}).ok, "only the visit the story is in is revised");
        auto ending = revised;
        ending.set("end", true);
        expect(!t.g.applyDmAct({"7", "story.phase", run, "dm", json::dump(ending)}).ok, "a revision never ends the story");
        expect(!t.g.applyDmAct({"7", "story.phase", "world-no-such", "dm", json::dump(revised)}).ok, "nor makes one");
        const auto revision = t.g.applyDmAct({"7", "story.phase", run, "dm", json::dump(revised)});
        const auto* book = t.g.storylineBook().find(run);
        expect(revision.ok && book->steps.size() == 1 && book->steps.back().text == "Hold the gate." && book->title == "Breach, revised" &&
               book->sourceRef == "1" && book->regions.at("wall").x1 == 6, "revised in place: one step, the new words and region: " + revision.message);
        expect(!tell(R"(,"to":"region","region":{"id":"wall","cell":"c_1_0","x1":7,"x2":9,"y1":8,"y2":8})").ok &&
               tell(R"(,"to":"region","region":{"id":"wall","cell":"c_1_0","x1":6,"x2":9,"y1":8,"y2":8})").ok,
               "the region is the new version's now");
        expect(t.g.applyDmAct({"7", "story.phase", run, "dm", json::dump(phase)}).message == "Phase already received.",
               "the visit's own phase order stays harmless");
        revised.set("regions", parsed(R"([{"id":"wall","cell":"c_1_0","x1":7,"x2":9,"y1":8,"y2":8}])"));
        expect(t.g.applyDmAct({"7", "story.phase", run, "dm", json::dump(revised)}).ok, "and back (the area effects below use the first bounds)");
        auto area = parsed(R"({"region":"wall","visit":1,"cell":"c_1_0","x1":7,"x2":9,"y1":8,"y2":8})");
        auto apply = [&](const char* effect) {
            return t.g.applyDmAct({"2", std::string("story.area_") + effect, run, "dm", json::dump(area)});
        };
        auto* ash = world.entity(t.ash.entityId);
        auto* bo = world.entity(t.bo.entityId);
        auto* cy = world.entity(t.cy.entityId);
        area.set("points", 12);
        expect(apply("damage").ok && ash->hurt == 12 && bo->hurt == 0 && cy->hurt == 0, "damage selects just eligible occupants");
        t.place(t.bo, 1, 8.5);
        area.set("hours", 8);
        area.set("buff", "fed");
        expect(apply("buff").ok && std::abs(ash->fedUntil - world.calendarDays() - 8. / 24) < 1e-7 &&
               bo->fedUntil == ash->fedUntil && cy->fedUntil < world.calendarDays(), "native timed Fed effect on both members");
        area.set("x1", 8);
        expect(!apply("damage").ok && ash->hurt == 12 && bo->hurt == 0, "altered region differs from the approved visit");
        area.set("x1", 7);
        area.set("visit", 2);
        expect(!apply("damage").ok && ash->hurt == 12, "wrong visit refused before applying damage");
        area.set("visit", 1);
        area.set("item", "meal");
        area.set("quantity", 2);
        auto stock = [&](const Client& c) { return Society::stock(*world.society().account(c.entityId), "meal"); };
        const int ashStart = stock(t.ash), boStart = stock(t.bo);
        expect(apply("item").ok && stock(t.ash) == ashStart + 2 && stock(t.bo) == boStart + 2, "group reward reaches real inventory");
        expect(world.society().create(t.bo.entityId, "meal", 10000 - stock(t.bo), "dm grant: full pack fixture"),
               "filled a participant's pack");
        const auto missed = apply("item");
        expect(missed.ok && stock(t.ash) == ashStart + 4 && stock(t.bo) == 10000 && missed.message.find("1 had no room") != std::string::npos,
               "a full pack misses the reward alone; the others receive it and the story keeps running: " + missed.message);
        area.set("item", "no-such-world-item");
        expect(!apply("item").ok && stock(t.ash) == ashStart + 4, "invalid good refused");
        area.set("count", 2);
        const auto n = world.roads().camps.size();
        expect(apply("spawn").ok && world.roads().camps.size() == n + 1, "a real camp is created");
        const auto& camp = world.roads().camps.back();
        expect(camp.cell == cellId(1) && camp.x >= 7 && camp.x < 10 && camp.y >= 8 && camp.y < 9 &&
               camp.strength == 6, "camp anchored inside the specified tiles");
        const auto& born = world.recentEvents().back();
        expect(born.kind == "bandits called" && born.cell == camp.cell && born.x == int(camp.x) && born.y == int(camp.y),
               "the camp event carries its exact tile for further region conditions");
        area.set("x2", 300);
        expect(!apply("spawn").ok && world.roads().camps.size() == n + 1, "out-of-map rectangle refused");
    }
    fs::remove_all(root);
    fs::remove(save);
}

void scriptedResidentScene()
{
    const auto root = fs::temp_directory_path() / ("ratw-scene-" + std::to_string(::getpid()));
    const auto save = root.string() + ".json";
    const auto world = writeStrip(root);
    const auto visit = parsed(R"({"title":"Breach","phase":"report","journal":"Await the courier.","visit":1,"players":["player-ash"]})");
    auto order = [&](Town& town, const std::string& kind, const std::string& npc, json::Value payload) {
        payload.add("visit", 1); payload.add("run", "world-scene");
        return town.g.applyDmAct({"4", kind, npc, "dm", json::dump(payload)});
    };
    {
        Town town(world, save, 120);
        town.enter(town.ash, 1, "ash", "Ash", 1, 7.5);
        auto& w = town.g.world();
        expect(town.g.applyDmAct({"1", "story.phase", "world-scene", "dm", json::dump(visit)}).ok, "scene visit opened");
        expect(w.teleport("u1", cellId(0), 4, 7).ok, "courier at breach");
        auto goal = parsed(R"({"pace":"run","cue":"runner-at-palace","x":8,"y":7})");
        goal.add("cell", cellId(1));
        expect(order(town, "npc.walk", "u1", goal).ok, "runner dispatched");
        expect(w.entity("u1")->cellId == cellId(0) && !town.g.storylineBook().find("world-scene")->sceneOrders.at("u1").done,
               "accepting the route did not report arrival");
        expect(!order(town, "npc.speak", "u1", parsed(R"({"cue":"too-soon","text":"Arrived."})")).ok,
               "busy resident refuses an overlapping direction");
        town.g.save();                              // Snapshot while the route is outstanding.
    }
    {
        Town town(world, save, 120);
        auto& w = town.g.world();
        town.enter(town.ash, 1, "ash", "Ash", 1, 7.5);
        town.enter(town.bo, 2, "bo", "Bo", 7, 7.5);
        std::vector<WorldEvent> cues;
        w.setEventWatcher([&](const WorldEvent& e) { if (e.kind.rfind("scene ", 0) == 0) cues.push_back(e); });
        for (int n = 0; n < 480 && cues.empty(); ++n) town.run(.25);
        expect(cues.size() == 1 && cues[0].kind == "scene arrived" && cues[0].actor == "u1" && cues[0].target == "world-scene" &&
               cues[0].cell == cellId(1) && cues[0].quantity == 1 && cues[0].detail == "runner-at-palace",
               "resumed runner crosses a cell and reports exactly one actual arrival");
        expect(int(w.entity("u1")->position.x) == 8 && int(w.entity("u1")->position.y) == 7, "courier on the ordered tile");
        expect(order(town, "npc.pose", "u1", parsed(R"({"cue":"seated","posture":"sitting"})")).ok, "seat the speaker");
        town.run(1);
        expect(order(town, "npc.pose", "u1", parsed(R"({"cue":"standing","posture":"standing"})")).ok, "rise for the address");
        town.run(2);
        expect(cues.size() == 3 && cues.back().kind == "scene posed" && w.entity("u1")->posture == "standing", "posture confirmed in the simulation");
        expect(order(town, "npc.speak", "u1", parsed(R"({"cue":"address","text":"Reinforce the breach."})")).ok, "address ordered");
        town.run(2);
        expect(cues.size() == 4 && cues.back().kind == "scene spoken" && cues.back().detail == "address", "speech ends the scene beat");
        const auto heard = [](const Client& client) {
            return std::any_of(client.events.begin(), client.events.end(), [](const json::Value& e) {
                return contains(json::dump(e), "Reinforce the breach");
            });
        };
        expect(heard(town.ash) && !heard(town.bo), "the speaker reaches a nearby listener through ordinary local speech");
        auto home = parsed(R"({"pace":"walk","cue":"return","x":4,"y":7})");
        home.add("cell", cellId(0));
        expect(order(town, "npc.walk", "u1", home).ok && w.setDead("u1", true).ok, "a resident's return journey is interrupted");
        town.run(1);
        expect(cues.size() == 5 && cues.back().kind == "scene blocked" && cues.back().detail == "return",
               "an interrupted journey emits its blocked cue once");
        expect(order(town, "npc.speak", "u1", parsed(R"({"cue":"missing-courier","text":"Report."})")).ok,
               "a valid order for an unavailable resident can take its fallback");
        town.run(1);
        expect(cues.size() == 6 && cues.back().kind == "scene blocked" && cues.back().detail == "missing-courier" &&
               !w.entity("u1")->sceneDirected, "unavailable at dispatch emits one scoped blocked cue without holding the resident");
        expect(town.g.applyDmAct({"5", "story.stop", "world-scene", "dm", "{}"}).ok, "scene stopped");
        town.run(1);
        expect(!w.entity("u1")->sceneDirected && !order(town, "npc.pose", "u1", parsed(R"({"cue":"stale","posture":"lying"})")).ok,
               "ordinary resident schedule resumes and stale directions are refused");
        w.setEventWatcher({});
    }
    fs::remove_all(root);
    fs::remove(save);
}

void inTheGame()
{
    const auto root = fs::temp_directory_path() / ("ratw-storylines-" + std::to_string(::getpid()));
    const auto save = (fs::temp_directory_path() / ("ratw-storylines-" + std::to_string(::getpid()) + ".json")).string();
    fs::remove(save);
    const auto world = writeStrip(root);
    std::string id;
    {
        Town t(world, save, 120, true);             // (The talk library: talking to a resident is in their window, doc 84 Phase 12.)
        auto& w = t.g.world();
        t.enter(t.ash, 1, "ash", "Ash", 1, 9.5);
        const auto ash = t.ash.entityId;
        auto phase = json::Value::object();
        phase.add("title", "The lost wheel");
        phase.add("phase", "road");
        phase.add("journal", "Ask the carter.");
        phase.add("visit", 1);
        auto players = json::Value::array();
        players.push(ash);
        phase.add("players", players);
        auto line = json::Value::object();
        line.add("npc", "u1");
        line.add("text", "I can pay for a wheel.");
        auto lines = json::Value::array();
        lines.push(line);
        phase.add("lines", lines);
        auto speakers = json::Value::array();
        speakers.push("u1");
        phase.add("guestVoices", speakers);
        auto patch = parsed(R"([{"id":"road-watch","x1":7,"y1":1,"x2":7,"y2":8}])");
        patch.items().front().add("cell", w.entity(ash)->cellId);
        phase.add("regions", patch);
        auto deliver = [&]() { return t.g.applyDmAct({"1", "story.phase", "world-run-test", "dm", json::dump(phase)}); };
        expect(deliver().ok && deliver().ok && t.g.storylineBook().find("world-run-test")->steps.size() == 1, "duplicate phase is harmless");
        expect(t.g.worldStoryLine("u1", ash) == line.string("text") && t.g.worldStoryLine("u1", "outsider").empty(), "only participants hear the clue");
        const auto original = w.entity(ash)->position;
        w.takeEvents();
        w.entity(ash)->position = {7.5, 4.5};
        t.g.tick(.05);
        w.entity(ash)->position = {7.5, 5.5};
        t.g.tick(.05);
        w.entity(ash)->position = original;
        t.g.tick(.05);
        std::vector<WorldEvent> crossings;
        for (const auto& e : w.takeEvents())
            if (e.target == "road-watch") crossings.push_back(e);
        expect(crossings.size() == 2 && crossings.front().kind == "region entered" && crossings.back().kind == "region left"
               && crossings.front().actor == ash && crossings.front().detail == "world-run-test" && crossings.front().y == 4
               && crossings.back().x == 7 && crossings.back().y == 5, "Only actual boundary crossings fire, with the occupied tile");
        w.entity(ash)->position = {6.98, 5.5};
        t.g.tick(.05); // A warp from x=9 to x=6 crosses no intermediate cells of the strip.
        const auto warpEvents = w.takeEvents();
        expect(std::none_of(warpEvents.begin(), warpEvents.end(), [](const auto& e){return e.target == "road-watch";}),
               "A warp across a region does not create intermediate visits");
        const auto wasClientWalking = w.entity(ash)->clientWalks;
        w.setClientWalks(ash, true);
        t.g.pose(&t.ash, w.entity(ash)->poseSeq + 1, 8.02, 5.5, 0., 0., 0.);
        const auto afterStride = w.takeEvents();
        crossings.clear();
        for (const auto& e : afterStride)
            if (e.target == "road-watch") crossings.push_back(e);
        expect(crossings.size() == 2 && crossings[0].kind == "region entered" && crossings[1].kind == "region left" &&
               crossings[0].x == 7 && crossings[1].x == 7, "One accepted stride across a thin area records both boundaries in order");
        w.setClientWalks(ash, wasClientWalking);
        w.entity(ash)->position = original;
        t.g.tick(.05);
        auto guest = R"({"npc":"u1","visit":1,"text":"I lost it on the bridge."})";
        expect(t.g.applyDmAct({"2","story.voice","world-run-test","dm",guest}).ok &&
               t.g.worldStoryLine("u1", ash) == "I lost it on the bridge.", "a delegated line belongs to this phase");
        expect(t.g.storylineBook().tick("world-run-test", 0, 0, ash, 42, true).empty(), "a manual tick cannot finish a world phase");
        auto mission = parsed(R"({"visit":1,"missions":[{"id":"rig","title":"Rig the line","mode":"individual","objectives":[{"label":"Deliver four ropes.","goal":4}],"tracks":{},"contributors":[]}]})");
        auto tracks = json::Value::object();
        tracks.add(ash, parsed("[0,2]"));
        tracks.add("another-volunteer", parsed("[1,0]"));
        auto snapshot = mission.array("missions").front();
        snapshot.set("tracks", tracks);
        auto ledger = json::Value::array();
        ledger.push(snapshot);
        mission.set("missions", ledger);
        auto sendMissions = [&]() { return t.g.applyDmAct({"4", "story.missions", "world-run-test", "dm", json::dump(mission)}); };
        expect(sendMissions().ok && sendMissions().ok, "repeated mission snapshots are harmless");
        // The connected client may still hold the phase-opening snapshot while waiting for the next journal update.
        json::Value personal;
        for (int poll = 0; poll < 8 && personal.isNull(); ++poll)
        {
            const auto self = t.selfOf(t.ash);
            for (const auto& story : self.array("journal"))
                if (story.string("id") == "world-run-test")
                    for (const auto& step : story.array("steps"))
                        for (const auto& objective : step.array("missions"))
                            if (objective.string("title") == "Rig the line") personal = objective;
        }
        expect(personal.string("instruction") == "Deliver four ropes." && personal.number("amount") == 2 &&
               !personal.boolean("credited") && !contains(json::dump(personal), "another-volunteer"),
               "the journal shows only this participant’s track");
        phase.set("previousMissions", ledger);
        phase.set("visit", 3);
        expect(!deliver().ok, "out-of-order visit refused");
        phase.set("visit", 2);
        phase.set("end", true);
        phase.set("phase", "mended");
        expect(deliver().ok && t.g.worldStoryLine("u1", ash).empty(), "ending retires the line");
        expect(!t.g.applyDmAct({"3","story.voice","world-run-test","dm",guest}).ok, "expired speaker cannot change the ending");
        storylines::Book restored;
        restored.load(t.g.storylineBook().save());
        expect(restored.find("world-run-test")->regions.count("road-watch") == 1, "Named areas survive checkpoint restore");
        expect(restored.find("world-run-test")->state == "done", "world story survives a save and load");
        expect(!sendMissions().ok && restored.find("world-run-test")->steps.front().missions.items().size() == 1 &&
               restored.find("world-run-test")->steps.front().missions.items().front().object("tracks")[ash].items()[1].asNumber() == 2,
               "closed visits keep their ledgers through save and load and refuse stale updates");
        t.g.storylineBook().erase("world-run-test");
        auto cast = json::Value::object();
        cast.add("resident", "u1");
        cast.add("place", cellId(5));
        const auto given = t.g.giveStoryline(ash, "the-debt", cast, "dm", "");
        expect(given.ok, "the DM gives Ash \"The debt\": " + given.message);
        id = given.targetId;
        t.run(2); // Allow a fresh full journal snapshot after the preceding campaign update.
        const auto self = t.selfOf(t.ash);
        const auto& journal = self["journal"].items();
        expect(journal.size() == 1 && journal[0].string("title") == "The debt", "in her journal");
        expect(journal[0]["steps"].items()[0].string("state") == "done" && journal[0]["steps"].items()[1].string("state") == "current",
               "step 1 ticked, step 2 current");
        // (A resident's name shows to a player at once: af4c3e1a, doc 82. A player's would still need an introduction.)
        expect(contains(journal[0]["steps"].items()[1].string("title"), "Find") && contains(journal[0]["steps"].items()[1].string("title"), "Neighbour"),
               "the resident shown by name (residents' names show at once): " + journal[0]["steps"].items()[1].string("title"));
        expect(self.object("tracked").string("cell") == cellId(0), "tracked: the marker at u1's work, " + self.object("tracked").string("cell"));
        // Talking to u1 ticks step 2, recognised or not.
        auto* u1 = w.entity("u1");
        u1->cellId = cellId(1), u1->position = {10.5, 8.5}, u1->path.clear();
        t.run(.5);
        t.ash.events.clear();
        say(t, t.ash, "Good day to you.", "u1");
        expect(t.g.storylineBook().find(id)->current() == 2, "talking to them ticked it");
        expect(contains(t.ash.said(), "Step done"), "she's told: " + t.ash.said().substr(0, 200));
        // Walking to the place ticks the next.
        t.place(t.ash, 5, 8.5);
        t.run(3);
        expect(t.g.storylineBook().find(id)->current() == 3, "there: the next step");
        t.g.settle();
        t.run(2);
    }
    {
        Town t(world, save);
        expect(t.g.storylineBook().find(id) && t.g.storylineBook().find(id)->current() == 3, "a restart keeps it");
    }
    fs::remove_all(root);
    fs::remove(save);
}

// Phase 2: a resident's trouble heard begins its storyline, solved by her it is done (with a small deed), by someone
// else it ends; work well done for a resident who likes her brings a chain: a second job from the resident's own purse,
// offered to her first; done, it ticks the step, and the resident's Mind hears the next step's line; a Gift used.
void sources()
{
    const auto root = fs::temp_directory_path() / ("ratw-storylines2-" + std::to_string(::getpid()));
    const auto save = (fs::temp_directory_path() / ("ratw-storylines2-" + std::to_string(::getpid()) + ".json")).string();
    fs::remove(save);
    const auto world = writeStrip(root);
    {
        Town t(world, save);
        auto& w = t.g.world();
        auto& s = w.society();
        t.enter(t.ash, 1, "ash", "Ash", 1, 9.5);
        t.enter(t.bo, 2, "bo", "Bo", 1, 10.5);
        const auto ash = t.ash.entityId, bo = t.bo.entityId;
        testPurse(s, ash, 200);
        testPurse(s, bo, 200);
        // A trouble: the inn's household short of coin (its purses emptied into the treasury, its food eaten).
        const auto home = s.resident("ui")->homeCell;
        std::string head;
        for (const auto& [id, life] : s.state().residents)
            if (life.homeCell == home)
            {
                if (const auto cash = s.account(id)->cash; cash > 0)
                    s.shift(id, "treasury", "", 0, cash, "test: spent");
                for (const auto& [item, n] : std::map<std::string, int>(s.account(id)->stock))
                    if (n > 0 && Society::edible(item))
                        s.consume(id, item, n, "test: eaten");
                if (const auto* e = w.entity(id); e && e->age >= 16 && (head.empty() || e->age > w.entity(head)->age))
                    head = id;
            }
        if (const auto* larder = s.account(Society::homeStore(home, "larder")))
            for (const auto& [item, n] : std::map<std::string, int>(larder->stock))
                if (n > 0 && Society::edible(item))
                    s.consume(Society::homeStore(home, "larder"), item, n, "test: eaten");
        expect(!head.empty(), "the inn's household has a head");
        // (Only a poor or destitute household is short of coin: doc 79, 6.7. The inn's is poor here.)
        expect(s.overrideBand(s.householdOf(head), status::Poor, 100000), "the inn's household poor");
        t.g.forgetTroubles();
        expect(t.g.troubleNow(head).kind == "short", "and it is short of coin: " + t.g.troubleNow(head).kind);
        t.g.hearTroubleFor(ash, head);
        t.g.hearTroubleFor(bo, head);
        std::string mine, theirs;
        for (const auto& [id, line] : t.g.storylineBook().all())
            if (line.source == "trouble")
                (line.owner == ash ? mine : theirs) = id;
        expect(!mine.empty() && !theirs.empty(), "each who heard it has its storyline");
        expect(t.g.storylineBook().find(mine)->current() == 1, "begun, heard ticked");
        w.bonds().change(head, ash, {10, 15, 30, 0, 0}, w.calendarDays());
        auto* ui = w.entity(head);
        ui->cellId = cellId(1), ui->position = {11.5, 8.5}, ui->path.clear();
        t.place(t.ash, 1, 10.5);
        t.run(.5);
        t.ash.events.clear();
        if (const auto need = t.g.troubleNow(head).coins; need > s.account(ash)->cash)
            s.shift("treasury", ash, "", 0, need - s.account(ash)->cash, "test: enough to help");
        t.g.command(&t.ash, cmd({{"type", "action"}, {"action", "trouble:help:" + head}, {"target", head}}));
        t.run(.5);
        expect(t.g.storylineBook().find(mine)->state == "done", "solved by her: her storyline is done: " + t.ash.said());   // (Her coin keeps them fed.)
        expect(t.g.storylineBook().find(theirs)->state == "ended", "Bo's ends: someone else saw to it");
        bool deed = false;
        for (const auto* d : t.g.deeds().byDoer(ash))
            deed |= d->kind == "finished_story";
        expect(deed, "recognised with a small deed (no pay)");
        // A chain: a courier job for u1, done; u1 likes her.
        w.bonds().change("u1", ash, {30, 10, 20, 0, 0}, w.calendarDays());
        auto& first = w.postContract("courier", "u1", "ridgemere", "u2", 3, 7, "a letter for Neighbour u2");
        const auto firstId = first.id;
        for (auto& k : w.roads().contracts)
            if (k.id == firstId)
                k.status = "done", k.taker = ash;
        const auto purse = s.account("u1")->cash;
        t.run(3);
        std::string chain;
        for (const auto& [id, line] : t.g.storylineBook().all())
            if (line.source == "contract" && line.owner == ash)
                chain = id;
        expect(!chain.empty(), "more work for u1: a chain in her journal");
        std::string next;
        for (const auto& k : w.roads().contracts)
            if (k.poster == "u1" && k.id != firstId && k.status == "open")
                next = k.id;
        expect(!next.empty() && s.account("u1")->cash < purse, "the next job posted from u1's own purse");
        expect(s.realBalanced(), "real money balanced");
        for (auto& k : w.roads().contracts)
            if (k.id == next)
                k.status = "done", k.taker = ash;
        t.run(3);
        expect(t.g.storylineBook().find(chain)->current() == 2, "that job done: the step ticks");
        expect(contains(t.g.storylineBook().find(chain)->steps[2].brief, "two jobs"), "the next step's line for u1");
        expect(contains(t.g.storylineBrief("u1", ash), "two jobs") && t.g.storylineBrief("u2", ash).empty(),
               "u1's Mind hears it, and nobody else's");
    }
    fs::remove_all(root);
    fs::remove(save);
}
// The story runner's "needs you" alerts in the Dev Console (doc 85, 5.6; Phase 3): each told once to the Dungeon Masters in
// the world, never to anyone else; the runner down while stories should run is one of them; a Dungeon Master who comes in
// later is told those still standing.
void storyAlertsInTheDevConsole()
{
    const auto root = fs::temp_directory_path() / ("ratw-storyalerts-" + std::to_string(::getpid()));
    const auto save = root.string() + ".json";
    {
        Town t(writeStrip(root), save);
        t.enter(t.ash, 1, "ash", "Ash", 1, 8.5);
        t.enter(t.bo, 2, "bo", "Bo", 1, 9.5);
        t.g.world().entity(t.ash.entityId)->dungeonMaster = true;
        const auto notices = [](const Client& c) {
            std::vector<std::string> out;
            for (const auto& m : c.events)
                if (m.string("type") == "devNotice")
                    out.push_back(m.string("text"));
            return out;
        };
        t.ash.events.clear();
        t.bo.events.clear();
        const auto paused = parsed(R"({"running":1,"beatAge":2.5,"runnerSeconds":30,
            "alerts":[{"key":"world-a:paused:3","run":"world-a","kind":"paused","text":"Story \"Breach\" is paused."}]})");
        expect(t.g.readStoryAlerts(paused) == 1, "one alert, new");
        expect(notices(t.ash) == std::vector<std::string>{"Story \"Breach\" is paused."}, "the Dungeon Master is told");
        expect(notices(t.bo).empty(), "a wolf who is not a Dungeon Master is told nothing");
        expect(t.g.readStoryAlerts(paused) == 0 && notices(t.ash).size() == 1, "told once, not on every reading");
        const auto down = parsed(R"({"running":2,"beatAge":95,"runnerSeconds":30,"alerts":[]})");
        expect(t.g.readStoryAlerts(down) == 1, "the runner down is an alert of its own");
        expect(notices(t.ash).size() == 2 && notices(t.ash)[1].find("has not beaten for 95 s: 2 stories are not moving") != std::string::npos,
               "the runner down, in words: " + notices(t.ash).back());
        expect(t.g.storyAlerts().count("world-a:paused:3"), "while the runner is down the story's alert still stands");
        expect(t.g.readStoryAlerts(paused) == 0 && notices(t.ash).size() == 2, "the runner back: the paused story is not told again");
        const auto quiet = parsed(R"({"running":0,"beatAge":2,"alerts":[]})");
        expect(t.g.readStoryAlerts(quiet) == 0 && t.g.storyAlerts().empty(), "nothing running: nothing to tell, and the alerts are gone");
        expect(t.g.readStoryAlerts(paused) == 0, "back within a minute: not told again");
        for (int i = 0; i < 6; ++i)
            t.g.readStoryAlerts(quiet);
        expect(t.g.readStoryAlerts(paused) == 1 && notices(t.ash).size() == 3, "gone for long, then back: told again");
        expect(t.g.readStoryAlerts(parsed(R"({"running":1,"beatAge":null,"alerts":[]})")) == 1 &&
                   notices(t.ash).back().find("never run on this world") != std::string::npos, "a runner that never ran");
        // Bo made a Dungeon Master now: told what still stands.
        t.bo.events.clear();
        expect(t.g.applyDmAct({"7", "character.dm", t.bo.entityId, "dm", R"({"dungeonMaster":true})"}).ok, "Bo is a Dungeon Master");
        const auto bo = notices(t.bo);
        expect(bo.size() == 2 && std::count(bo.begin(), bo.end(), std::string("Story \"Breach\" is paused.")) == 1,
               "told the alerts still standing: the paused story and the runner");
    }
    fs::remove_all(root);
    fs::remove(save);
}

// A world story's living audience and quests (doc 85, Phase 6): story.cohort, story.quests (offers, choices, each wolf's
// journal entries and trackers, a toast), the journal's quest verbs writing the events the runner reads, parties told as
// events, and the rewards: coins from a named purse, an item, standing.
void storyQuestsInTheGame()
{
    const auto root = fs::temp_directory_path() / ("ratw-storyquests-" + std::to_string(::getpid()));
    const auto save = root.string() + ".json";
    {
        Town t(writeStrip(root), save);
        t.enter(t.ash, 1, "ash", "Ash", 1, 8.5);
        t.enter(t.bo, 2, "bo", "Bo", 1, 9.5);
        t.enter(t.cy, 3, "cy", "Cy", 1, 10.5);
        auto& w = t.g.world();
        std::vector<WorldEvent> seen;
        w.setEventWatcher([&](const WorldEvent& e) { seen.push_back(e); });
        const auto run = "world-quests";
        auto phase = parsed(R"({"title":"The ford","phase":"open","journal":"The carter is stuck.","visit":1})");
        auto players = json::Value::array();
        players.push(t.ash.entityId);
        phase.set("players", players);
        expect(t.g.applyDmAct({"1", "story.phase", run, "dm", json::dump(phase)}).ok, "the story opens with Ash");
        auto* book = &t.g.storylineBook();
        // story.cohort: Bo in, then out; past 5000 refused.
        auto cohort = parsed(R"({"visit":1,"add":[],"remove":[]})");
        cohort.set("add", parsed("[\"" + t.bo.entityId + "\"]"));
        expect(t.g.applyDmAct({"2", "story.cohort", run, "dm", json::dump(cohort)}).ok && book->find(run)->takesPart(t.bo.entityId),
               "Bo is taken into the story");
        auto many = json::Value::array();
        for (int i = 0; i < 5001; ++i) many.push("wolf-" + std::to_string(i));
        auto crowd = parsed(R"({"visit":1})");
        crowd.set("add", many);
        expect(!t.g.applyDmAct({"2", "story.cohort", run, "dm", json::dump(crowd)}).ok, "5000 wolves at most");
        expect(!t.g.applyDmAct({"2", "story.cohort", "world-no-such", "dm", json::dump(cohort)}).ok, "an unknown story is refused");
        // story.quests: Ash's journal entries, an offer, a story choice; a toast.
        auto text = std::string(R"({"visit":1,
            "offers":[{"quest":"axle","npc":"u1","title":"An axle for the carter","say":"My axle's split.","ask":"Is there work going?",
                       "accept":"I'll find you one.","decline":"Not now.","handIn":"Here's your axle.","thanks":"Moving again.","who":"anyone","full":false}],
            "choices":[{"id":"rumour","npc":"u1","words":"Heard about the axle?","reply":"Someone cut it.","players":null,"once":true,"picked":[]}],
            "wolves":{"ash":{"quests":[
                {"id":"axle","title":"An axle for the carter","summary":"The carter is stuck.","scope":"player","status":"active","giver":"u1",
                 "objectives":[{"id":"ask","label":"Ask the carter.","done":true,"open":true},{"id":"bring","label":"Bring an axle.","amount":1,"goal":2,"open":true}],
                 "hiddenLeft":1,"handIn":false,"abandon":true,"accept":false},
                {"id":"letter","title":"A letter","scope":"player","status":"offered","objectives":[],"hiddenLeft":0,"handIn":false,"abandon":true,"accept":true},
                {"id":"bound","title":"Bound","scope":"player","status":"active","objectives":[],"hiddenLeft":0,"handIn":false,"abandon":false,"accept":false}],
              "trackers":[{"label":"Ropes delivered","value":3}],"toast":"You take on: An axle for the carter."}}})");
        text.replace(text.find("\"ash\":"), 6, "\"" + t.ash.entityId + "\":");
        const auto quests = parsed(text);
        t.ash.events.clear();
        expect(t.g.applyDmAct({"3", "story.quests", run, "dm", json::dump(quests)}).ok, "the quests are recorded");
        expect(contains(t.ash.said(), "You take on: An axle for the carter."), "Ash is told");
        const auto* story = book->find(run);
        expect(story->offers.size() == 1 && story->choices.size() == 1 && story->questViews.count(t.ash.entityId), "kept with the story");
        const auto journal = t.selfOf(t.ash)["journal"];
        const json::Value* mine = nullptr;
        for (const auto& j : journal.items())
            if (j.string("id") == run) mine = &j;
        expect(mine && mine->array("quests").size() == 3 && !mine->array("quests")[0].string("giverName").empty() &&
               mine->array("trackers").size() == 1, "the journal shows the quests, the giver by name, the trackers: " + json::dump(journal));
        // The journal's verbs: abandon one under way, accept one offered with nobody to ask; refusals.
        const auto verb = [&](const std::string& v, const std::string& quest) {
            auto o = parsed(R"({"type":"storyline"})");
            o.set("verb", v);
            o.set("storyline", run);
            o.set("quest", quest);
            seen.clear();
            t.ash.events.clear();
            t.g.command(&t.ash, json::dump(o));
            return seen;
        };
        auto got = verb("quest_abandon", "axle");
        expect(got.size() == 1 && got[0].kind == "quest abandoned" && got[0].actor == t.ash.entityId && got[0].target == run &&
               got[0].detail == "axle" && got[0].item.empty(), "abandon writes quest abandoned");
        expect(verb("quest_abandon", "bound").empty(), "one that can't be abandoned is refused");
        expect(verb("quest_accept", "axle").empty(), "one with a giver is accepted from the giver");
        got = verb("quest_accept", "letter");
        expect(got.size() == 1 && got[0].kind == "quest accepted" && got[0].detail == "letter", "accept from the journal");
        got = verb("quest_decline", "letter");
        expect(got.size() == 1 && got[0].kind == "quest declined", "decline from the journal");
        expect(verb("quest_abandon", "nothing").empty(), "an unknown quest is refused");
        // A wolf listed with nothing is cleared.
        expect(t.g.applyDmAct({"4", "story.quests", run, "dm", "{\"wolves\":{\"" + t.ash.entityId + "\":{\"quests\":[],\"trackers\":[]}}}"}).ok &&
               !book->find(run)->questViews.count(t.ash.entityId) && book->find(run)->offers.size() == 1, "cleared; the offers kept");
        expect(!t.g.applyDmAct({"4", "story.quests", run, "dm", R"({"offers":[{"quest":"x","npc":"nobody","title":"X"}]})"}).ok,
               "an offer by nobody is refused");
        // Out of the story: its journal entries go.
        cohort.set("add", json::Value::array());
        cohort.set("remove", parsed("[\"" + t.bo.entityId + "\"]"));
        expect(t.g.applyDmAct({"5", "story.cohort", run, "dm", json::dump(cohort)}).ok && !book->find(run)->takesPart(t.bo.entityId), "Bo leaves");
        // A quest pinned on the town's board (an offer with `board`), taken from it.
        expect(!w.towns().empty(), "the strip has a town");
        const auto community = w.towns().front().id;
        const auto* spot = w.boardSpot(community);
        expect(spot, "and a board");
        const auto pinned = parsed(R"({"offers":[{"quest":"posted","npc":"","board":")" + community +
                                   R"(","title":"Wanted: a rope","say":"Bring a rope to the ford.","who":"anyone","full":false}]})");
        expect(t.g.applyDmAct({"4", "story.quests", run, "dm", json::dump(pinned)}).ok, "a board offer is recorded");
        t.cy.events.clear();
        {
            auto* cy = w.entity(t.cy.entityId);
            cy->cellId = spot->cell;
            cy->position = {spot->x, spot->y};
        }
        t.g.command(&t.cy, cmd({{"type", "board"}, {"verb", "read"}}));
        const auto* board = t.cy.last("board");
        expect(board && board->array("quests").size() == 1 && board->array("quests")[0].string("quest") == "posted", "the board shows it");
        seen.clear();
        t.g.command(&t.cy, cmd({{"type", "board"}, {"verb", "quest"}, {"run", run}, {"quest", "posted"}}));
        expect(seen.size() == 1 && seen[0].kind == "quest accepted" && seen[0].actor == t.cy.entityId && seen[0].item == "board:" + community &&
               seen[0].detail == "posted", "taken from the board: quest accepted, item board:<town>");
        seen.clear();
        t.g.command(&t.cy, cmd({{"type", "board"}, {"verb", "quest"}, {"run", run}, {"quest", "nothing"}}));
        expect(seen.empty(), "an unknown one is refused");
        // Saved and loaded.
        const auto again = storylines::loadStoryline(storylines::saveStoryline(*book->find(run)));
        expect(again.offers == book->find(run)->offers && again.choices == book->find(run)->choices, "offers and choices saved");

        // Parties as events: a party made, joined, left.
        seen.clear();
        t.g.command(&t.ash, cmd({{"type", "party"}, {"verb", "invite"}, {"target", t.bo.entityId}}));
        t.g.command(&t.bo, cmd({{"type", "party"}, {"verb", "accept"}}));
        const auto kinds = [&](const std::string& kind) {
            std::vector<std::string> who;
            for (const auto& e : seen) if (e.kind == kind) who.push_back(e.actor + "@" + e.target);
            std::sort(who.begin(), who.end());
            return who;
        };
        const auto joined = kinds("party joined");
        expect(joined.size() == 2 && joined[0].rfind(t.ash.entityId + "@party-", 0) == 0 && joined[1].rfind(t.bo.entityId + "@party-", 0) == 0,
               "both joined the new party: " + std::to_string(joined.size()));
        seen.clear();
        t.g.command(&t.bo, cmd({{"type", "party"}, {"verb", "leave"}}));
        expect(kinds("party left").size() == 2 && kinds("party joined").empty(), "a party of two losing one: both left");
        seen.clear();
        t.run(1);
        expect(kinds("party left").empty() && kinds("party joined").empty(), "nothing more on the tick");

        // Rewards: coins from a named purse; an item; standing.
        auto& soc = w.society();
        const auto grant = [&](const std::string& purse, int coins) {
            auto p = parsed(R"({"reason":"the carter's thanks"})");
            p.set("coins", coins);
            if (!purse.empty()) p.set("purse", purse);
            return t.g.applyEconomyCommand("character.grant", t.ash.entityId, p, "");
        };
        const auto cash = [&](const std::string& a) { return soc.account(a) ? soc.account(a)->cash : -1; };
        const auto ashBefore = cash(t.ash.entityId), treasuryBefore = cash("treasury");
        auto r = grant("treasury", 12);
        expect(r.ok && cash(t.ash.entityId) == ashBefore + 12 && cash("treasury") == treasuryBefore - 12, "paid from the treasury: " + r.message);
        expect(soc.openAccount("town:strip:works"), "a town's works purse");
        r = grant("town:strip:works", 5);
        expect(!r.ok && r.message == "The purse is short (0p)." && cash(t.ash.entityId) == ashBefore + 12, "a short purse refuses: " + r.message);
        expect(!grant("player-nobody", 5).ok && !grant(t.bo.entityId, 5).ok, "a player's purse is no purse to pay from");
        r = grant("", 7);
        expect(r.ok && r.message == "Granted 7p: the carter's thanks." && cash(t.ash.entityId) == ashBefore + 19, "without a purse, as ever: " + r.message);
        const auto meals = Society::stock(*soc.account(t.ash.entityId), "meal");
        r = t.g.applyDmAct({"6", "item.give", t.ash.entityId, "dm", R"({"item":"meal","quantity":2})"});
        expect(r.ok && Society::stock(*soc.account(t.ash.entityId), "meal") == meals + 2, "an item into the pack: " + r.message);
        expect(!t.g.applyDmAct({"6", "item.give", t.ash.entityId, "dm", R"({"item":"no-such","quantity":2})"}).ok, "an unknown item refused");
        expect(!t.g.applyDmAct({"6", "item.give", t.ash.entityId, "dm", R"({"item":"meal","quantity":21})"}).ok, "20 at most");
        expect(!t.g.applyDmAct({"6", "item.give", "player-away", "dm", R"({"item":"meal","quantity":1})"}).ok, "a wolf not in the world");
        expect(!t.g.applyDmAct({"6", "standing.adjust", t.ash.entityId, "dm", R"({"faction":"no-such","points":2,"reason":"helped"})"}).ok,
               "an unknown faction refused");
    }
    fs::remove_all(root);
    fs::remove(save);
}

} // namespace

int main()
{
    try
    {
        pure();
        privateSceneQueue();
        scriptedResidentScene();
        privateCameraAndMovement();
        sceneLockYieldsToFight();
        scenePanels();
        sceneAiNeedsAnAudience();
        storyAreaEffects();
        storyQuestsInTheGame();
        storyAlertsInTheDevConsole();
        inTheGame();
        sources();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "storylines: " << checks << " checks passed\n";
    return 0;
}
