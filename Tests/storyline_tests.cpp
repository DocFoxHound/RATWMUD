// Storylines and the journal (Docs/Design/58-player-storytellers.md). Phase 1: the pure rules (begun with step 1
// ticked; place, talk and scene objectives; parallel parts and two angles; take; abandon; the limit; save and load);
// then through the game: a Dungeon Master gives Ash "The debt"; her journal shows it with its marker; talking to the
// resident, unrecognised, ticks a step; walking to the place ticks the next; a restart keeps it.
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
    auto command = parsed(cmd({{"type", "chat"}, {"text", text}, {"channel", "ic"}, {"volume", "speak"}}));
    auto targets = json::Value::array();
    targets.push(to);
    command.add("targets", targets);
    t.g.command(&c, json::dump(command));
    t.g.settle();
    t.run(1);
}

void inTheGame()
{
    const auto root = fs::temp_directory_path() / ("ratw-storylines-" + std::to_string(::getpid()));
    const auto save = (fs::temp_directory_path() / ("ratw-storylines-" + std::to_string(::getpid()) + ".json")).string();
    fs::remove(save);
    const auto world = writeStrip(root);
    std::string id;
    {
        Town t(world, save);
        auto& w = t.g.world();
        t.enter(t.ash, 1, "ash", "Ash", 1, 9.5);
        const auto ash = t.ash.entityId;
        auto cast = json::Value::object();
        cast.add("resident", "u1");
        cast.add("place", cellId(5));
        const auto given = t.g.giveStoryline(ash, "the-debt", cast, "dm", "");
        expect(given.ok, "the DM gives Ash \"The debt\": " + given.message);
        id = given.targetId;
        const auto self = t.selfOf(t.ash);
        const auto& journal = self["journal"].items();
        expect(journal.size() == 1 && journal[0].string("title") == "The debt", "in her journal");
        expect(journal[0]["steps"].items()[0].string("state") == "done" && journal[0]["steps"].items()[1].string("state") == "current",
               "step 1 ticked, step 2 current");
        expect(contains(journal[0]["steps"].items()[1].string("title"), "Find") && !contains(journal[0]["steps"].items()[1].string("title"), "Neighbour"),
               "the resident shown as she knows them (not a name she wasn't given): " + journal[0]["steps"].items()[1].string("title"));
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
        s.shift("treasury", ash, "", 0, 200, "test purse");
        s.shift("treasury", bo, "", 0, 200, "test purse");
        // A trouble: the inn's loan.
        const auto till = s.tillOf("ui");
        auto st = s.state();
        st.memory.loans[till] = {40, std::int64_t(w.calendarDays())};
        expect(s.restore(st), "the inn owes the town");
        t.g.forgetTroubles();
        t.g.hearTroubleFor(ash, "ui");
        t.g.hearTroubleFor(bo, "ui");
        std::string mine, theirs;
        for (const auto& [id, line] : t.g.storylineBook().all())
            if (line.source == "trouble")
                (line.owner == ash ? mine : theirs) = id;
        expect(!mine.empty() && !theirs.empty(), "each who heard it has its storyline");
        expect(t.g.storylineBook().find(mine)->current() == 1, "begun, heard ticked");
        w.bonds().change("ui", ash, {10, 15, 30, 0, 0}, w.calendarDays());
        auto* ui = w.entity("ui");
        ui->cellId = cellId(1), ui->position = {11.5, 8.5}, ui->path.clear();
        t.place(t.ash, 1, 10.5);
        t.run(.5);
        t.ash.events.clear();
        t.g.command(&t.ash, cmd({{"type", "action"}, {"action", "trouble:loan:ui"}, {"target", "ui"}}));
        t.run(.5);
        expect(t.g.storylineBook().find(mine)->state == "done", "solved by her: her storyline is done");
        expect(t.g.storylineBook().find(theirs)->state == "ended", "Bo's ends: someone else saw to it");
        bool deed = false;
        for (const auto* d : t.g.deeds().byDoer(ash))
            deed |= d->kind == "finished_story";
        expect(deed, "recognised with a small deed (no pay)");
        // A chain: a courier job for u1, done; u1 likes her.
        w.bonds().change("u1", ash, {30, 10, 20, 0, 0}, w.calendarDays());
        auto& first = w.postContract("courier", "u1", "upper_accord", "u2", 3, 7, "a letter for Neighbour u2");
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
        expect(s.conserved(), "money conserved");
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
} // namespace

int main()
{
    try
    {
        pure();
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
