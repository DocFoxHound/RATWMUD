// Player storytellers (Docs/Design/58-player-storytellers.md, Phases 3-6). Applying (social level 5, one application at a
// time), approved, revoked (its tale paused, its commands refused); a tale written (a real wolf's name refused for a story
// character), begun, a call on the board, a stranger asking and admitted; an invitation out of scope refused; narration and
// a story character's line reaching participants here and never another place; open dice (their limits, and fair over many
// rolls); a tick by hand; a visitor from the DM's list (a fight with it refused); a prize from a real purse to a participant
// only; the end card and the storyteller's star; a milestone's credits screen with up to 3 stars and deeds for the main
// contributors; across a restart.
#include "RatwGame.h"
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

std::string order(Town& t, Client& c, std::initializer_list<std::pair<const char*, json::Value>> fields)
{
    c.events.clear();
    t.g.command(&c, cmd(fields));
    t.run(1);
    return c.said();
}

bool heardStory(const Client& c, const std::string& part)
{
    for (const auto& e : c.events)
        if (e.string("type") == "story" && e.string("text").find(part) != std::string::npos)
            return true;
    return false;
}

void dice()
{
    std::string error;
    const auto r = game::Game::rollDice("2d6+1", "seed", error);
    expect(contains(r, "2d6 + 1: ") && contains(r, " = "), "rolled in words: " + r);
    for (const char* bad : {"11d6", "2d7", "2d6+21", "two dice", "0d6"})
        expect(game::Game::rollDice(bad, "s", error).empty(), std::string("refused: ") + bad);
    // Fair over many rolls: each face of a d6 about a sixth.
    std::map<int, int> faces;
    for (int i = 0; i < 6000; ++i)
    {
        const auto words = game::Game::rollDice("1d6", "fair" + std::to_string(i), error);
        ++faces[std::stoi(words.substr(words.rfind('=') + 2))];
    }
    for (int f = 1; f <= 6; ++f)
        expect(faces[f] > 850 && faces[f] < 1150, "a fair die: face " + std::to_string(f) + " came " + std::to_string(faces[f]) + " times");
}

void tales()
{
    const auto root = fs::temp_directory_path() / ("ratw-storyteller-" + std::to_string(::getpid()));
    const auto save = (fs::temp_directory_path() / ("ratw-storyteller-" + std::to_string(::getpid()) + ".json")).string();
    fs::remove(save);
    const auto world = writeStrip(root);
    std::string tale;
    {
        Town t(world, save);
        auto& w = t.g.world();
        auto& s = w.society();
        t.enter(t.ash, 1, "ash", "Ash", 1, 9.5);
        t.enter(t.bo, 2, "bo", "Bo", 1, 10.5);
        t.enter(t.cy, 3, "cy", "Cy", 5, 8.5);
        const auto ash = t.ash.entityId, bo = t.bo.entityId, cy = t.cy.entityId;
        s.shift("treasury", ash, "", 0, 100, "test purse");
        // Applying.
        auto said = order(t, t.ash, {{"type", "storyteller"}, {"verb", "apply"}, {"note", "Tales of the drowned bell."}});
        expect(contains(said, "social level 5"), "not yet at social level 5: " + said);
        t.g.ledger().points[ash] = 1000000;
        said = order(t, t.ash, {{"type", "storyteller"}, {"verb", "apply"}, {"note", "Tales of the drowned bell."}});
        expect(contains(said, "with the Dungeon Masters"), "applied: " + said);
        said = order(t, t.ash, {{"type", "storyteller"}, {"verb", "apply"}, {"note", "Again."}});
        expect(contains(said, "with the Dungeon Masters") && !contains(said, "Again"), "one application at a time");
        said = order(t, t.ash, {{"type", "storyteller"}, {"verb", "save"}, {"title", "x"}});
        expect(contains(said, "approved"), "no tales before approval");
        const auto account = t.g.accountOf(ash);
        t.g.decideStorytellerFor(account, true);
        expect(t.g.approvedStoryteller(account), "approved");
        expect(t.selfOf(t.ash).object("storyteller").string("state") == "approved", "the sheet knows");
        // A tale: a real wolf's name refused for a story character.
        auto steps = json::Value::array();
        for (const char* title : {"The pier", "The crossing"})
        {
            auto step = json::Value::object();
            step.add("title", title);
            step.add("text", "What happens there.");
            auto objectives = json::Value::array();
            auto o = json::Value::object();
            o.add("kind", "told");
            o.add("line", std::string(title) + ", done");
            objectives.push(o);
            step.add("objectives", objectives);
            steps.push(step);
        }
        auto cast = json::Value::array();
        auto ferryman = json::Value::object();
        ferryman.add("name", "Neighbour u1");
        cast.push(ferryman);
        said = order(t, t.ash, {{"type", "storyteller"}, {"verb", "save"}, {"title", "The Drowned Bell"}, {"steps", steps}, {"cast", cast}});
        expect(contains(said, "real wolf's name"), "a resident's name refused: " + said);
        cast = json::Value::array();
        ferryman = json::Value::object();
        ferryman.add("name", "the ferryman");
        ferryman.add("looks", "a lean old wolf");
        cast.push(ferryman);
        said = order(t, t.ash, {{"type", "storyteller"}, {"verb", "save"}, {"title", "The Drowned Bell"}, {"steps", steps}, {"cast", cast}});
        expect(contains(said, "Saved"), "saved: " + said);
        for (const auto& [id, line] : t.g.storylineBook().all())
            if (line.kind == "tale")
                tale = id;
        said = order(t, t.ash, {{"type", "storyteller"}, {"verb", "start"}, {"tale", tale}});
        expect(contains(said, "begins") && t.g.storylineBook().find(tale)->state == "running", "begun: " + said);
        // An invitation out of scope; a call, an ask, an admit.
        said = order(t, t.ash, {{"type", "storyteller"}, {"verb", "invite"}, {"tale", tale}, {"who", bo}});
        expect(contains(said, "party, Chapter, circles and friends"), "a stranger isn't invited: " + said);
        said = order(t, t.ash, {{"type", "storyteller"}, {"verb", "call"}, {"tale", tale}});
        expect(contains(said, "board"), "a call posted: " + said);
        const auto calls = t.selfOf(t.bo).object("storyteller")["calls"].items();
        expect(calls.size() == 1 && calls[0].string("id") == tale, "Bo sees the call");
        said = order(t, t.bo, {{"type", "storyteller"}, {"verb", "ask"}, {"tale", tale}});
        expect(contains(said, "You ask to join"), "Bo asks: " + said);
        said = order(t, t.ash, {{"type", "storyteller"}, {"verb", "admit"}, {"tale", tale}, {"who", bo}});
        expect(t.g.storylineBook().find(tale)->takesPart(bo), "admitted: " + said);
        // Narration and a story character's line: Bo, here, hears; Cy, elsewhere, doesn't.
        t.bo.events.clear();
        t.cy.events.clear();
        order(t, t.ash, {{"type", "storyteller"}, {"verb", "narrate"}, {"tale", tale}, {"text", "Fog lies on the water."}});
        expect(heardStory(t.bo, "Fog lies on the water."), "Bo hears the narration");
        expect(!heardStory(t.cy, "Fog"), "Cy, in another place, doesn't");
        said = order(t, t.ash, {{"type", "storyteller"}, {"verb", "narrate"}, {"tale", tale}, {"text", "Too soon."}});
        expect(contains(said, "moment"), "a moment between lines");
        t.run(3);
        t.bo.events.clear();
        order(t, t.ash, {{"type", "storyteller"}, {"verb", "npc"}, {"tale", tale}, {"name", "the ferryman"}, {"text", "Two pennies, or swim."}});
        expect(heardStory(t.bo, "Two pennies"), "the ferryman's line, marked as the story's");
        t.run(3);
        said = order(t, t.ash, {{"type", "storyteller"}, {"verb", "npc"}, {"tale", tale}, {"name", "the mayor"}, {"text", "Hello."}});
        expect(contains(said, "cast"), "only the tale's cast speaks: " + said);
        // Dice, by a participant too.
        t.bo.events.clear();
        order(t, t.bo, {{"type", "storyteller"}, {"verb", "roll"}, {"tale", tale}, {"dice", "2d6"}, {"for", "the crossing"}});
        expect(heardStory(t.bo, "2d6: ") && heardStory(t.bo, "the crossing"), "Bo rolls in the open");
        // A tick by hand.
        said = order(t, t.ash, {{"type", "storyteller"}, {"verb", "tick"}, {"tale", tale}, {"step", 0}, {"objective", 0}});
        expect(t.g.storylineBook().find(tale)->current() == 1, "step 1 ticked: " + said);
        // A visitor from the DM's list; no fight with it.
        auto list = json::Value::array();
        auto v = json::Value::object();
        v.add("id", "messenger");
        v.add("name", "a cloaked messenger");
        v.add("description", "A wolf in a grey cloak.");
        list.push(v);
        t.g.setStoryVisitors(list);
        said = order(t, t.ash, {{"type", "storyteller"}, {"verb", "visitor"}, {"tale", tale}, {"visitor", "nobody"}});
        expect(contains(said, "list"), "only from the DM's list: " + said);
        said = order(t, t.ash, {{"type", "storyteller"}, {"verb", "visitor"}, {"tale", tale}, {"visitor", "messenger"}, {"minutes", 20}});
        std::string visitor;
        for (const auto& [id, e] : w.entities())
            if (w.isStoryVisitor(id))
                visitor = id;
        expect(!visitor.empty() && contains(said, "arrives"), "the messenger arrives: " + said);
        expect(contains(w.attack(bo, visitor, "").message, "part of a story"), "no fight with a story's visitor");
        said = order(t, t.ash, {{"type", "storyteller"}, {"verb", "dismiss"}, {"tale", tale}, {"visitor", visitor}});
        t.run(1);
        expect(!w.entity(visitor), "sent away");
        // A prize: to a participant only, from her own purse.
        const auto before = s.account(bo)->cash;
        said = order(t, t.ash, {{"type", "storyteller"}, {"verb", "prize"}, {"tale", tale}, {"to", cy}, {"coins", 5}});
        expect(contains(said, "participants"), "not to one outside the tale: " + said);
        said = order(t, t.ash, {{"type", "storyteller"}, {"verb", "prize"}, {"tale", tale}, {"to", bo}, {"coins", 5}});
        expect(s.account(bo)->cash == before + 5 && s.conserved(), "5p to Bo, conserved: " + said);
        // The end card, and Bo's star for the storyteller.
        said = order(t, t.ash, {{"type", "storyteller"}, {"verb", "end"}, {"tale", tale}, {"how", "done"}});
        t.run(6);
        const auto* card = t.bo.last("credits");
        expect(card && card->string("kind") == "tale" && card->string("teller") == ash, "Bo's end card");
        const auto cardId = card->string("id");     // (The event goes with the next command's clearing.)
        said = order(t, t.bo, {{"type", "storyteller"}, {"verb", "star"}, {"credits", cardId}, {"to", ash}});
        expect(contains(said, "Your star is given"), "Bo stars the storyteller: " + said);
        said = order(t, t.bo, {{"type", "storyteller"}, {"verb", "star"}, {"credits", cardId}, {"to", ash}});
        expect(contains(said, "starred"), "once: " + said);
        bool starred = false;
        for (const auto& st : t.g.starBook().recent())
            starred |= st.kind == "tale" && st.recipientCharacter == ash && st.tag == "storyteller";
        expect(starred, "a tale star, tagged Storyteller");
        // A milestone: credits, stars (up to 3), deeds for the main contributors.
        auto payload = json::Value::object();
        payload.add("story", "The siege of Ser Ferro");
        payload.add("milestone", "The gate holds");
        payload.add("weight", "great");
        auto people = json::Value::array();
        for (const auto& [who, line] : std::vector<std::pair<std::string, std::string>>{{ash, "fought at the gate"}, {bo, "carried the warning"}})
        {
            auto p = json::Value::object();
            p.add("id", who);
            auto lines = json::Value::array();
            lines.push(line);
            p.add("lines", lines);
            people.push(p);
        }
        payload.add("people", people);
        const auto credited = t.g.creditMilestone(payload);
        expect(credited.ok, "credited: " + credited.message);
        t.run(6);
        const auto* screen = t.ash.last("credits");
        expect(screen && screen->string("kind") == "milestone" && screen->number("stars") == 3, "Ash's credits screen, 3 stars to give");
        bool lines = false;
        for (const auto& p : (*screen)["people"].items())
            lines |= contains(json::dump(p), "carried the warning");
        expect(lines, "who did what");
        said = order(t, t.ash, {{"type", "storyteller"}, {"verb", "star"}, {"credits", credited.targetId}, {"to", bo}});
        expect(contains(said, "Your star is given"), "Ash stars Bo: " + said);
        bool great = false;
        for (const auto* d : t.g.deeds().byDoer(bo))
            great |= d->weight >= fame::Great;
        expect(great, "a great deed for a main contributor");
        // Revoked: the tale-telling stops.
        t.g.decideStorytellerFor(account, true);
        said = order(t, t.ash, {{"type", "storyteller"}, {"verb", "save"}, {"title", "Another"}, {"steps", steps}});
        std::string second;
        for (const auto& [id, line] : t.g.storylineBook().all())
            if (line.kind == "tale" && id != tale)
                second = id;
        order(t, t.ash, {{"type", "storyteller"}, {"verb", "start"}, {"tale", second}});
        t.g.revokeStorytellerFor(account);
        expect(t.g.storylineBook().find(second)->state == "paused", "revoked: the running tale pauses");
        said = order(t, t.ash, {{"type", "storyteller"}, {"verb", "narrate"}, {"tale", second}, {"text", "Still here?"}});
        expect(contains(said, "approved"), "and its commands are refused: " + said);
        t.g.settle();
        t.run(2);
    }
    {
        Town t(world, save);
        expect(t.g.storylineBook().find(tale) && t.g.storylineBook().find(tale)->state == "done", "the tale kept across a restart");
        t.enter(t.ash, 1, "ash", "Ash", 1, 9.5);
        expect(!t.g.approvedStoryteller(t.g.accountOf(t.ash.entityId)), "and the standing (revoked)");
    }
    fs::remove_all(root);
    fs::remove(save);
}
} // namespace

int main()
{
    try
    {
        dice();
        tales();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "storytellers: " << checks << " checks passed\n";
    return 0;
}
