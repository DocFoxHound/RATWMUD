// Dialogue trees and the dialogue window (Docs/Design/83-dialogue-trees.md, Phase 1; Core/RatwTalkTree.cpp,
// Core/RatwGameDialogue.cpp): a tree's loading and its refusals; choices hidden and locked; a node's line in a dialect,
// and a resident's own line over the tree's; then in a game, Trade on a keeper opens the shop tree, its first choice the
// shop window on the same quotes; both sides said aloud as speech (a bystander hears both, a hidden speaker stays a
// voice); the player's own words for a choice said in place of the default, and roleplay experience only for written,
// non-trivial lines; free talk answered in the window; the conversation closes on walking away and on sleep. Phase 2,
// a giver's work: the job list, a job's details, taking it on, declining, giving it back free, a letter handed over in
// the recipient's tree, a faction's board with its locks, a mission's goods handed in; taken, promised, expired and
// no-work cases refused in words, and the giver's tree offering only what the server would allow.
#include "test_purse.h"
#include "RatwGame.h"
#include "RatwPractice.h"
#include "RatwSections.h"
#include "RatwTalk.h"
#include "RatwWorld.h"

#include <algorithm>
#include <arpa/inet.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <mutex>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unistd.h>
#include <vector>

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

const std::string Data = RATW_SOURCE_DIR "/Data/Talk";

talk::Library library()
{
    talk::Library lib;
    std::string problem;
    expect(lib.load(Data, problem), "the talk library loads, with its trees: " + problem);
    return lib;
}

talk::Query keeperQuery(const std::string& concept, const talk::Facts& more = {})
{
    talk::Query q;
    q.concept = concept;
    q.a.facts = {{"id", "hesta"}, {"name", "Hesta"}, {"sex", "female"}, {"stage", "adult"}, {"age", "40"}, {"job", "baker"},
                 {"role", "merchant"}, {"trade", "baker"}};
    q.a.grammar = {"plain"};
    q.b.facts = {{"id", "player-ash"}, {"name", "Ash"}, {"sex", "male"}, {"stage", "adult"}, {"age", "20"}, {"job", "none"},
                 {"role", "civilian"}};
    q.facts = {{"here.region", "ridgemere"}, {"here.place", "work"}, {"here.time", "day"}, {"here.season", "spring"},
               {"here.weather", "clear"}, {"here.day", "work"}, {"here.shop", "open"}, {"here.work", "0"},
               {"topic.meeting", "stranger"}};
    for (const auto& [k, v] : more)
        q.facts[k] = v;
    return q;
}

std::string said(const talk::Library& lib, const talk::Query& q, std::uint64_t seed = 1)
{
    const auto s = lib.pick(q, seed, {}, {});
    return s.lines.empty() ? std::string() : s.lines.front().second;
}

// A tree in a library of its own beside the real one's foundation (facts, settings, grammars).
bool refused(const std::string& tree, const std::string& expected)
{
    talk::Library lib;
    std::string problem;
    expect(lib.loadFoundation(Data, problem), "the foundation loads: " + problem);
    const bool ok = lib.parseTree(tree, "test.tree", problem);
    expect(!ok, "refused: " + expected);
    expect(problem.find(expected) != std::string::npos, "refused for '" + expected + "': " + problem);
    expect(lib.trees().empty() && !lib.has("dlg.t.a"), "nothing of a refused tree is kept");
    return true;
}

void loading()
{
    const auto lib = library();
    const auto* shop = lib.tree("shop");
    expect(shop && shop->entries.count("trade") && shop->entries.at("trade") == "greet", "the shop tree, entered by Trade at greet");
    expect(shop->nodes.at("greet").choices.size() <= 4, "a shop's greeting has four choices at most");
    expect(shop->nodes.at("greet").choices.front().words == "Let me see your wares." &&
               shop->nodes.at("greet").choices.front().action == "shop",
           "its first choice is the wares, opening the shop window");
    expect(lib.tree("resident") && lib.tree("resident")->entries.count("talk") && lib.tree("resident")->nodes.at("hub").free,
           "Talk opens a resident's hub, with free talk");
    expect(lib.has("dlg.shop.greet") && lib.settings().bestOnly.count("dlg.shop.greet"), "a node's line is a talk concept, at its best tier");
    // The talk_check way: one file against the foundation.
    talk::Library one;
    std::string problem;
    expect(one.loadFoundation(Data, problem), "the foundation loads");
    std::ifstream in(Data + "/dialogue/shop.tree");
    std::stringstream text;
    text << in.rdbuf();
    expect(one.parseTree(text.str(), "shop.tree", problem), "the shop tree validates alone: " + problem);

    const std::string ok = "tree t\nentry talk a\nnode a\nsay Hello.\nchoice Bye. -> end\n";
    {
        talk::Library lib2;
        expect(lib2.loadFoundation(Data, problem) && lib2.parseTree(ok, "ok.tree", problem), "a small tree loads: " + problem);
    }
    refused("tree t\nentry talk a\nnode a\nsay #nosuchsymbol#.\nchoice Bye. -> end\n", "nosuchsymbol");
    refused("tree t\nentry talk a\nnode a\nsay Hello.\nchoice Bye. [here.nosuch=yes] -> end\n", "nosuch");
    refused("tree t\nentry talk a\nnode a\nsay Hello.\nchoice Bye. -> do dance\n", "'dance' is not an action");
    refused("tree t\nentry talk a\nnode a\nsay Hello.\nchoice Take. -> do take\n", "'take' is given the bound job");
    refused("tree t\nentry talk a\nnode a\nsay Hello.\nchoice Go. -> do open\n", "'open' names an entry");
    refused("tree t\nentry talk a\nnode a\nsay Hello.\nchoice Bye. -> nowhere\n", "no node 'nowhere'");
    refused("tree t\nentry talk a\nnode a\nsay Hello.\nchoice Bye. -> end\nnode b\nsay Lost.\nchoice Bye. -> end\n", "node b can't be reached");
    refused("tree t\nentry talk a\nnode a\nsay[here.shop=open] Hello.\nchoice Bye. -> end\n", "needs a say without criteria");
    refused("tree t\nentry talk a\nnode a\nsay Hello.\neach job in here.nothing max 3\nchoice x -> end\n", "not a list");
    refused("tree t\nentry talk a\nnode a\nsay Hello.\nmood jubilant\nchoice Bye. -> end\n", "not a mood the face knows");
    refused("tree t\nentry talk a\nnode a\nsay Hello there, this line is near.\nsay[here.shop=open] " + std::string(170, 'x') + "\nchoice Bye. -> end\n",
            "more than 160");
    {
        // Two trees for the same entry and residents, at one priority: refused; one priority higher, or disjoint `for`s: not.
        talk::Library lib3;
        expect(lib3.loadFoundation(Data, problem) && lib3.parseTree("tree x\nfor role=merchant\nentry talk a\nnode a\nsay Hi.\nchoice Bye. -> end\n", "x.tree", problem),
               "the first loads");
        expect(!lib3.parseTree("tree y\nfor role=merchant|guard\nentry talk a\nnode a\nsay Hi.\nchoice Bye. -> end\n", "y.tree", problem) &&
                   problem.find("give one a priority") != std::string::npos,
               "an overlapping for, refused: " + problem);
        expect(lib3.parseTree("tree y\nfor role=guard\nentry talk a\nnode a\nsay Hi.\nchoice Bye. -> end\n", "y.tree", problem),
               "a disjoint for loads: " + problem);
        expect(lib3.parseTree("tree z\npriority 2\nentry talk a\nnode a\nsay Hi.\nchoice Bye. -> end\n", "z.tree", problem),
               "an overlapping one with a priority loads: " + problem);
        auto q = keeperQuery("");
        expect(lib3.treeFor("talk", q) && lib3.treeFor("talk", q)->id == "z", "the higher priority answers");
    }
}

void choicesAndLines()
{
    auto lib = library();
    const auto& greet = lib.tree("shop")->nodes.at("greet");
    const auto none = [](const std::string&) { return std::vector<talk::Facts>{}; };
    auto q = keeperQuery("dlg.shop.greet");
    auto shown = lib.choices(greet, q, 1, none);
    std::vector<std::string> texts;
    for (const auto& s : shown)
        texts.push_back(s.text);
    expect(texts == std::vector<std::string>{"Let me see your wares.", "What's dear today?", "Goodbye for now."},
           "an open shop with no work: wares, prices, goodbye (work hidden)");
    expect(shown[0].n == 1 && shown[2].n == 3, "numbered from one");
    q.facts["here.work"] = "2";
    q.facts["here.giver"] = "yes";
    shown = lib.choices(greet, q, 1, none);
    expect(shown.size() == 4 && shown[1].text == "Any work going?", "with work going, four");
    q.facts["here.shop"] = "asleep";
    shown = lib.choices(greet, q, 1, none);
    expect(shown.size() == 1 && shown[0].text == "Goodbye for now.", "a sleeping keeper: only goodbye");
    expect(contains(said(lib, q), "shut") || contains(said(lib, q), "cold") || contains(said(lib, q), "Mm") || contains(said(lib, q), "Zzz"),
           "and they answer in words, asleep: " + said(lib, q));
    q.facts["here.shop"] = "bare";
    expect(contains(said(lib, q), "bare") || contains(said(lib, q), "left") || contains(said(lib, q), "Cleared") ||
               contains(said(lib, q), "selling"),
           "a bare shelf, said: " + said(lib, q));
    // A locked choice is shown greyed with its reason.
    {
        talk::Library one;
        std::string problem;
        expect(one.loadFoundation(Data, problem) &&
                   one.parseTree("tree t\nentry talk a\nnode a\nsay Hello.\nchoice Buy. [here.shop=open] locked \"We're shut.\" -> end\nchoice Bye. -> end\n",
                                 "t.tree", problem),
               "a locked choice loads: " + problem);
        auto lq = keeperQuery("dlg.t.a", {{"here.shop", "asleep"}});
        const auto s = one.choices(one.tree("t")->nodes.at("a"), lq, 1, none);
        expect(s.size() == 2 && s[0].locked && s[0].why == "We're shut." && !s[1].locked, "greyed, with why");
    }
    // An each: one choice per item, its facts bound (the work tree's list, Phase 2); one the server wouldn't allow, greyed.
    const auto& work = lib.tree("work")->nodes.at("list");
    const auto jobs = [](const std::string& list) {
        std::vector<talk::Facts> out;
        if (list == "here.work")
            for (int i = 0; i < 5; ++i)
                out.push_back({{"topic.job_id", "k" + std::to_string(i)}, {"topic.job_title", "Carry a letter " + std::to_string(i)},
                               {"topic.job_reward", "12"}, {"topic.job_kind", "courier"}, {"topic.job_allowed", i == 1 ? "no" : "yes"},
                               {"topic.job_why", "That's promised to another for now."}});
        return out;
    };
    shown = lib.choices(work, keeperQuery("dlg.work.list", {{"here.work", "5"}, {"here.mine", "0"}}), 1, jobs);
    expect(shown.size() == 5 && shown[0].text == "What about this one: Carry a letter 0 (12p)?" && shown[2].bound.at("topic.job_id") == "k2" &&
               shown[4].text == "Goodbye for now.",
           "four jobs at most, then goodbye: " + (shown.empty() ? std::string() : shown[0].text));
    expect(shown[1].locked && shown[1].why == "That's promised to another for now." && !shown[0].locked, "one promised elsewhere: greyed, why");
    expect(lib.tree("work") && lib.tree("official") && lib.tree("receiver") && lib.tree("receiver")->priority > lib.tree("resident")->priority,
           "the givers' trees: work, official, receiver (over the resident's Talk while something's to hand)");
    // The dialect: a broad Larkfield keeper greets in Larkfield's words.
    auto lark = keeperQuery("dlg.shop.greet");
    lark.a.facts["trade"] = "chandler";
    lark.a.facts["from"] = "larkfield";
    lark.a.facts["accent"] = "broad";
    lark.a.grammar = {"larkfield", "plain"};
    bool dialect = false;
    for (std::uint64_t seed = 1; seed < 40 && !dialect; ++seed)
    {
        const auto line = said(lib, lark, seed);
        dialect = contains(line, "Fair price") || contains(line, "Good trade") || contains(line, "priced to sell") ||
                  contains(line, "Name your weight") || contains(line, "weigh");
    }
    expect(dialect, "a Larkfield keeper's greeting is Larkfield's: " + said(lib, lark));
    std::string problem;
    lib.setCharacter("hesta", "rule hello\nwhen concept=dlg.shop.greet\nsay Hesta's own hello, love.\n", problem);
    expect(problem.empty() && said(lib, keeperQuery("dlg.shop.greet")) == "Hesta's own hello, love.",
           "a resident's own line wins in the window: " + problem);
}

void ledger()
{
    // The roleplay experience rule, in the ledger: five words or more, not the same words twice in a day, a line each
    // fifteen seconds, twelve with one resident and forty in all a day.
    SocialLedger s;
    expect(s.dialogueLine("ash", "hesta", 4, 11, 1000, 1) == 0, "four words: nothing");
    expect(s.dialogueLine("ash", "hesta", 6, 12, 1000, 2) == 2, "six words: 2");
    expect(s.dialogueLine("ash", "hesta", 20, 13, 1005, 3) == 0, "within fifteen seconds: nothing");
    expect(s.dialogueLine("ash", "hesta", 20, 12, 1100, 4) == 0, "the same words again: nothing");
    expect(s.dialogueLine("ash", "hesta", 20, 14, 1200, 5) == 3, "fifteen words or more: 3");
    int paid = 0;
    for (int i = 0; i < 20; ++i)
        paid += s.dialogueLine("ash", "hesta", 6, 100 + std::uint64_t(i), 1300 + 20 * i, 10 + std::uint64_t(i));
    expect(paid == 2 * (SocialLedger::DialogueLinesPerResident - 2), "twelve lines with one resident a day: " + std::to_string(paid));
    expect(s.dialogueLine("ash", "oda", 6, 500, 2000, 50) == 2, "another resident still listens");
    expect(s.dialogueLine("ash", "hesta", 6, 501, 1000 + 86400 + 10, 51) == 2, "and the next day, again");
    expect(practice::socialReason("npc_dialogue"), "it is social experience");
}

// ------------------------------------------------------------------ In a game

const json::Value* lastOf(const Client& c, const std::string& type)
{
    return c.last(type);
}
std::vector<std::string> heardFrom(const Client& c, const std::string& speakerId)
{
    std::vector<std::string> out;
    for (const auto& e : c.events)
        if (e.string("type") == "roleplay" && e.string("speakerId") == speakerId)
            out.push_back(e.string("text"));
    return out;
}
std::vector<const json::Value*> voices(const Client& c)
{
    std::vector<const json::Value*> out;
    for (const auto& e : c.events)
        if (e.string("type") == "roleplay" && e.boolean("anonymous"))
            out.push_back(&e);
    return out;
}

void inAGame()
{
    const auto root = fs::temp_directory_path() / ("ratw-dialogue-" + std::to_string(::getpid()));
    const auto worldPath = writeStrip(root / "world");
    Town town(worldPath, (root / "save").string(), 120, true);
    auto& w = town.g.world();
    auto& s = w.society();
    town.enter(town.ash, 1, "player-ash", "Ash", 1, 9.5);
    town.enter(town.bo, 2, "player-bo", "Bo", 1, 11.5);
    town.enter(town.cy, 3, "player-cy", "Cy", 7, 8.5);
    w.setTimeOfDay(12);
    town.run(2);
    expect(s.merchant("um"), "the stall's keeper keeps shop");
    const auto till = s.tillOf("um");
    for (const auto& ware : s.wares("um"))
        if (Society::stock(*s.account(till), ware) < 10)
            s.create(till, ware, 10 - Society::stock(*s.account(till), ware), "test stock");
    const auto keep = [&] {
        auto* um = w.entity("um");
        um->cellId = cellId(1);
        um->position = {8.5, 8.5};
        w.stop("um");
        town.place(town.ash, 1, 9.5);
        town.place(town.bo, 1, 11.5);
    };
    const auto step = [&](double seconds = .5) {
        for (double t = 0; t < seconds; t += .25)
        {
            keep();
            town.run(.25);
        }
        keep();
    };
    step(1);
    expect(!town.ash.snapshots.empty() && town.ash.snapshots.back().boolean("dialogue"), "the snapshot says the window is there");
    const auto open = [&](const std::string& npc, const std::string& entry) {
        for (auto* c : {&town.ash, &town.bo, &town.cy})
            c->events.clear();
        town.g.command(&town.ash, cmd({{"type", "dialogue_open"}, {"npc", npc}, {"entry", entry}}));
        return lastOf(town.ash, "dialogue");
    };
    const auto choose = [&](int n, const std::string& text = {}) {
        const auto* d = town.g.dialogueOf(town.ash.entityId);
        expect(d, "a conversation to choose in");
        for (auto* c : {&town.ash, &town.bo, &town.cy})
            c->events.clear();
        auto o = json::Value::object();
        o.add("type", "dialogue_choose");
        o.add("session", d->id);
        o.add("n", n);
        if (!text.empty())
            o.add("text", text);
        town.g.command(&town.ash, json::dump(o));
    };
    const auto choiceN = [&](const json::Value& d, const std::string& words) {
        for (const auto& c : d.array("choices"))
            if (c.string("text") == words)
                return int(c.number("n"));
        return 0;
    };

    // Trade: the keeper's greeting, the face, at most four choices; said aloud.
    const auto* d = open("um", "trade");
    expect(d && d->string("tree") == "shop" && d->string("node") == "greet" && !d->string("line").empty(),
           "Trade opens the shop tree's greeting: " + (d ? json::dump(*d) : std::string("none")));
    expect(d->find("appearance") && d->find("appearance")->isObject() && d->string("npc") == "um" && !d->string("name").empty(),
           "with the keeper's face and name");
    const auto line = d->string("line");
    // The face (Phase 5): the node's mood, and how long the line is spoken (about fourteen characters a second).
    expect(d->string("mood") == "warm", "the greeting's mood, for the face: " + d->string("mood"));
    expect(d->number("speaking") >= .6 && std::abs(d->number("speaking") - std::clamp(double(line.size()) / 14, .6, 10.0)) < .2,
           "the time it speaks the line: " + std::to_string(d->number("speaking")) + " for " + std::to_string(line.size()));
    const auto choices = d->array("choices");
    expect(!choices.empty() && choices.size() <= 4 && choices.front().string("text") == "Let me see your wares." &&
               choices.back().string("text") == "Goodbye for now.",
           "wares first, goodbye last, four at most");
    const auto bo = heardFrom(town.bo, "um");
    expect(std::find(bo.begin(), bo.end(), line) != bo.end(), "a bystander hears the keeper's greeting: " + line);
    expect(heardFrom(town.cy, "um").empty(), "one far off hears nothing");
    // Wares: the shop window, on this keeper's list, at the trade's own quotes.
    choose(1);
    expect(lastOf(town.ash, "dialogue_shop") && lastOf(town.ash, "dialogue_shop")->string("npc") == "um", "the wares open the shop window");
    const auto ashSaid = heardFrom(town.bo, town.ash.entityId);
    expect(std::find(ashSaid.begin(), ashSaid.end(), "Let me see your wares.") != ashSaid.end(), "the player's choice is said aloud");
    expect(town.g.dialogueOf(town.ash.entityId) && town.g.dialogueOf(town.ash.entityId)->inShop, "the conversation waits behind it");
    step(1);
    expect(town.ash.snapshots.back()["merchant"].string("id") == "um", "the keeper's list in the snapshot");
    {
        const auto ware = s.wares("um").front();
        town.ash.events.clear();
        town.g.command(&town.ash, cmd({{"type", "trade_quote"}, {"target", "um"}, {"item", ware}, {"quantity", 1}, {"buy", true}}));
        const auto* quote = town.ash.last("tradeQuote");
        const auto expected = s.quote(town.ash.entityId, "um", ware, 1, true, town.g.accountOf(town.ash.entityId));
        expect(quote && quote->boolean("ok") == expected.ok && quote->number("total") == double(expected.total),
               "Trade -> dialogue -> shop quotes as the trade prices it: " + (quote ? json::dump(*quote) : std::string()));
        // A sale: the keeper's word on it.
        town.run(3.5);
        keep();
        town.g.command(&town.ash, cmd({{"type", "trade"}, {"target", "um"}, {"item", ware}, {"quantity", 1}, {"buy", true}}));
        step(1);
        const auto* receipt = town.ash.last("tradeResult");
        expect(receipt && (!receipt->boolean("ok") || !receipt->string("said").empty()),
               "a sale through the conversation, the keeper says so: " + (receipt ? json::dump(*receipt) : std::string()));
    }
    // Closing the shop window ends it with the keeper's farewell, said aloud.
    for (auto* c : {&town.ash, &town.bo})
        c->events.clear();
    town.g.command(&town.ash, cmd({{"type", "dialogue_close"}, {"session", town.g.dialogueOf(town.ash.entityId)->id}, {"why", "shop"}}));
    expect(!town.g.dialogueOf(town.ash.entityId) && lastOf(town.ash, "dialogue_close"), "closed");
    expect(!heardFrom(town.bo, "um").empty(), "the farewell is heard: " + (heardFrom(town.bo, "um").empty() ? "" : heardFrom(town.bo, "um").front()));

    // The player's own words for a choice: said in place of the default, and paid as roleplay.
    step(1);
    d = open("um", "trade");
    expect(d, "opened again");
    const int dear = choiceN(*d, "What's dear today?");
    expect(dear > 0, "what's dear, offered");
    const auto xpBefore = town.g.socialXp(town.ash.entityId);
    const auto entriesBefore = town.g.ledger().entries.size();
    const std::string own = "Tell me, good keeper, what costs the most this week?";
    choose(dear, own);
    const auto mine = heardFrom(town.bo, town.ash.entityId);
    expect(std::find(mine.begin(), mine.end(), own) != mine.end(), "the player's own words are what the wolf says");
    d = lastOf(town.ash, "dialogue");
    expect(d && d->string("node") == "prices" && !d->string("line").empty(), "the choice's branch: the prices");
    expect(town.g.socialXp(town.ash.entityId) == xpBefore + 2 && contains(d->string("note"), "roleplay"),
           "a written line earns roleplay experience: " + std::to_string(town.g.socialXp(town.ash.entityId) - xpBefore));
    // The default line, as it is: no experience, and no receipt.
    const auto entriesAfter = town.g.ledger().entries.size();
    expect(entriesAfter == entriesBefore + 1, "one receipt for the written line");
    choose(choiceN(*d, "Let me see your wares."));
    expect(town.g.ledger().entries.size() == entriesAfter, "the default line pays nothing");
    expect(town.g.dialogueExperience(town.ash.entityId, "ui", "Let me see your wares, please now.", "Let me see your wares, please now.") == 0,
           "the default, typed out, pays nothing");
    expect(town.g.dialogueExperience(town.ash.entityId, "ui", "Let me see all your wares, please now friend.",
                                     "Let me see your wares, please now friend.") == 0,
           "a near copy pays nothing");
    expect(town.g.dialogueExperience(town.ash.entityId, "ui", "Show me.", "") == 0, "a short line pays nothing");
    town.g.command(&town.ash, cmd({{"type", "dialogue_close"}, {"session", town.g.dialogueOf(town.ash.entityId)->id}, {"why", "bye"}}));

    // Talk: the resident's hub, with free talk; what's typed is said aloud and answered in the window.
    step(1);
    d = open("u1", "talk");
    if (!d)
    {
        // (The neighbour wanders: bring them over.)
        auto* n = w.entity("u1");
        n->cellId = cellId(1);
        n->position = {10.5, 9.5};
        w.stop("u1");
        d = open("u1", "talk");
    }
    expect(d && d->string("tree") == "resident" && d->boolean("free"), "Talk opens the resident's hub, free talk allowed: " +
                                                                          (d ? json::dump(*d) : json::dump(town.ash.events.empty() ? json::Value{} : town.ash.events.back())));
    town.ash.events.clear();
    {
        auto o = json::Value::object();
        o.add("type", "dialogue_say");
        o.add("session", town.g.dialogueOf(town.ash.entityId)->id);
        o.add("text", "Good day to you. What do you do around here?");
        town.g.command(&town.ash, json::dump(o));
    }
    for (int i = 0; i < 12 && (!town.g.dialogueOf(town.ash.entityId) || town.g.dialogueOf(town.ash.entityId)->awaiting); ++i)
    {
        auto* n = w.entity("u1");
        n->position = {10.5, 9.5};
        town.run(.5);
        keep();
    }
    const auto* answer = lastOf(town.ash, "dialogue");
    expect(answer && !answer->boolean("awaiting") && !answer->string("line").empty(),
           "free talk is answered in the window: " + (answer ? json::dump(*answer) : std::string()));
    town.g.command(&town.ash, cmd({{"type", "dialogue_close"}, {"session", town.g.dialogueOf(town.ash.entityId)->id}, {"why", "bye"}}));

    // A hidden speaker stays a voice: Ash crouched out of Bo's sight; Bo hears the keeper by name and Ash as a voice.
    step(1);
    town.place(town.bo, 1, 13.5);
    {
        auto* ash = w.entity(town.ash.entityId);
        ash->posture = "crouching";
        ash->sneakSkill = 100;                      // (Not made out at four tiles: doc 40's detection range.)
    }
    town.run(.5);
    d = open("um", "trade");
    expect(d, "a crouching wolf may still talk at the counter");
    const bool seen = w.shown(town.bo.entityId, town.ash.entityId);
    choose(choiceN(*d, "What's dear today?"));
    const auto heardVoices = voices(town.bo);
    expect(!seen, "the fixture: Bo can't make Ash out");
    {
        expect(heardFrom(town.bo, town.ash.entityId).empty(), "a hidden speaker's line carries no id");
        expect(std::any_of(heardVoices.begin(), heardVoices.end(), [](const json::Value* e) {
                   return e->string("speaker") == "A voice" && !e->find("speakerId") && !e->find("appearance");
               }),
               "heard only as a voice");
        expect(!heardFrom(town.bo, "um").empty(), "while the keeper, seen, is heard by name");
    }
    {
        auto* ash = w.entity(town.ash.entityId);
        ash->posture = "standing";
    }

    // Walking away ends it.
    expect(town.g.dialogueOf(town.ash.entityId), "still talking");
    town.ash.events.clear();
    town.place(town.ash, 1, 15.0);
    town.run(1.5);
    expect(!town.g.dialogueOf(town.ash.entityId) && lastOf(town.ash, "dialogue_close") &&
               lastOf(town.ash, "dialogue_close")->string("why") == "away",
           "walking away closes the conversation");
    // The keeper falling asleep ends it.
    step(1);
    d = open("um", "trade");
    expect(d, "open once more");
    town.ash.events.clear();
    w.entity("um")->posture = "lying";
    town.run(1.5);
    expect(!town.g.dialogueOf(town.ash.entityId) && lastOf(town.ash, "dialogue_close") &&
               lastOf(town.ash, "dialogue_close")->string("why") == "asleep",
           "the keeper falling asleep closes it");
    // Asleep from the start: the keeper mumbles, and there's only goodbye.
    d = open("um", "trade");
    expect(d && d->array("choices").size() == 1 && d->array("choices").front().string("text") == "Goodbye for now.",
           "a sleeping keeper: words, and only goodbye: " + (d ? json::dump(*d) : std::string()));
    expect(d && d->string("mood") == "sleepy", "a sleeping keeper's face is sleepy, whatever the node's mood");
    w.entity("um")->posture = "standing";
    // Too far to open: the client is told to walk over.
    town.g.command(&town.ash, cmd({{"type", "dialogue_close"}, {"session", town.g.dialogueOf(town.ash.entityId)->id}, {"why", "bye"}}));
    town.place(town.ash, 1, 14.5);
    town.ash.events.clear();
    town.g.command(&town.ash, cmd({{"type", "dialogue_open"}, {"npc", "um"}, {"entry", "trade"}}));
    expect(lastOf(town.ash, "dialogue_close") && lastOf(town.ash, "dialogue_close")->string("why") == "far", "too far: walk over first");
    // A stale session's choice is refused.
    town.g.command(&town.ash, cmd({{"type", "dialogue_choose"}, {"session", 9999}, {"n", 1}}));
    expect(lastOf(town.ash, "dialogue_close")->string("why") == "gone", "a stale session is closed");
    // (The slice's review.) A choice sent at once after walking off is neither said nor done: the conversation ends
    // there and then, not at the next second's look.
    step(3.5);
    d = open("um", "trade");
    expect(d, "open again at the counter");
    town.place(town.ash, 1, 15.0);
    choose(choiceN(*d, "What's dear today?"));
    expect(!town.g.dialogueOf(town.ash.entityId) && lastOf(town.ash, "dialogue_close") &&
               lastOf(town.ash, "dialogue_close")->string("why") == "away" && !lastOf(town.ash, "dialogue"),
           "walked off: the choice ends it at once");
    expect(heardFrom(town.bo, town.ash.entityId).empty(), "and its words aren't said");
    // Opened again within three seconds: the greeting shows in the window, but isn't said aloud again (nor goodbye).
    step(3.5);
    d = open("um", "trade");
    expect(d && !heardFrom(town.bo, "um").empty(), "the greeting heard");
    town.g.command(&town.ash, cmd({{"type", "dialogue_close"}, {"session", town.g.dialogueOf(town.ash.entityId)->id}, {"why", "bye"}}));
    d = open("um", "trade");
    expect(d && !d->string("line").empty() && heardFrom(town.bo, "um").empty(), "opened again at once: the greeting in the window, not aloud");
    town.g.command(&town.ash, cmd({{"type", "dialogue_close"}, {"session", town.g.dialogueOf(town.ash.entityId)->id}, {"why", "bye"}}));
    expect(heardFrom(town.bo, "um").empty(), "nor its goodbye");
    fs::remove_all(root);
}

// ------------------------------------------------------------------ A giver's work (Phase 2)

void giversWork()
{
    const auto root = fs::temp_directory_path() / ("ratw-dialogue-work-" + std::to_string(::getpid()));
    const auto worldPath = writeStrip(root / "world");
    Town town(worldPath, (root / "save").string(), 120, true);
    auto& w = town.g.world();
    auto& s = w.society();
    town.enter(town.ash, 1, "player-ash", "Ash", 1, 9.5);
    town.enter(town.bo, 2, "player-bo", "Bo", 1, 11.5);
    town.enter(town.cy, 3, "player-cy", "Cy", 1, 12.5);
    w.setTimeOfDay(12);
    town.run(2);
    const auto till = s.tillOf("um");
    for (const auto& ware : s.wares("um"))
        if (Society::stock(*s.account(till), ware) < 10)
            s.create(till, ware, 10 - Society::stock(*s.account(till), ware), "test stock");
    // Merchant um keeps the counter; Neighbour u3 (a letter's recipient) and u1 (the Watch's sergeant, later) stand by.
    const auto keep = [&] {
        for (const auto& [id, x, y] : std::vector<std::tuple<std::string, double, double>>{{"um", 8.5, 8.5}, {"u3", 9.5, 10.5}, {"u1", 10.5, 7.5}})
        {
            auto* e = w.entity(id);
            e->cellId = cellId(1);
            e->position = {x, y};
            e->posture = "standing";
            w.stop(id);
        }
        town.place(town.ash, 1, 9.5);
        town.place(town.bo, 1, 11.5);
    };
    const auto step = [&](double seconds = .5) {
        for (double t = 0; t < seconds; t += .25)
        {
            keep();
            town.run(.25);
        }
        keep();
    };
    step(1);
    const auto open = [&](const std::string& npc, const std::string& entry) -> const json::Value* {
        if (const auto* d = town.g.dialogueOf(town.ash.entityId))
            town.g.command(&town.ash, cmd({{"type", "dialogue_close"}, {"session", d->id}, {"why", "closed"}}));
        for (auto* c : {&town.ash, &town.bo, &town.cy})
            c->events.clear();
        town.g.command(&town.ash, cmd({{"type", "dialogue_open"}, {"npc", npc}, {"entry", entry}}));
        town.g.settle();
        return lastOf(town.ash, "dialogue");
    };
    const auto choose = [&](int n) -> const json::Value* {
        const auto* d = town.g.dialogueOf(town.ash.entityId);
        expect(d, "a conversation to choose in");
        for (auto* c : {&town.ash, &town.bo, &town.cy})
            c->events.clear();
        town.g.command(&town.ash, cmd({{"type", "dialogue_choose"}, {"session", d->id}, {"n", n}}));
        town.g.settle();                            // (A reply that changed valuables waits for its journal record.)
        return lastOf(town.ash, "dialogue");
    };
    const auto texts = [](const json::Value* d) {
        std::string all;
        if (d)
            for (const auto& c : d->array("choices"))
                all += "[" + c.string("text") + (c.boolean("locked") ? " (locked: " + c.string("why") + ")" : std::string()) + "]";
        return all;
    };
    const auto pick = [&](const json::Value* d, const std::string& starts) {
        if (d)
            for (const auto& c : d->array("choices"))
                if (c.string("text").rfind(starts, 0) == 0)
                    return int(c.number("n"));
        return 0;
    };
    const auto lockedWhy = [&](const json::Value* d, const std::string& starts) {
        if (d)
            for (const auto& c : d->array("choices"))
                if (c.string("text").rfind(starts, 0) == 0 && c.boolean("locked"))
                    return c.string("why");
        return std::string();
    };
    const auto contract = [&](const std::string& id) -> Contract& {
        for (auto& k : w.roads().contracts)
            if (k.id == id)
                return k;
        throw std::runtime_error("no contract " + id);
    };
    const auto* market = w.roadMarketOf(cellId(1));
    expect(market, "the fixture: a market at the counter");

    // No work: the keeper's greeting has no "Any work going?", and asked for work, they say there's none.
    auto* d = open("um", "trade");
    expect(d && pick(d, "Any work going?") == 0, "no work: the greeting doesn't offer it: " + texts(d));
    d = open("um", "work");
    expect(d && d->string("tree") == "work" && d->string("node") == "list" && d->array("choices").size() == 1 &&
               pick(d, "Goodbye for now.") == 1,
           "no work: the list is only goodbye: " + texts(d) + " / " + (d ? d->string("line") : std::string()));

    // Work posted: a letter from Neighbour u2 to u3, and a bounty on a camp.
    testPurse(s, "u2", 40);
    const std::string letter = w.postContract("courier", "u2", market->id, "u3", 5, 3, "a letter for Neighbour u3").id;
    const std::string bounty = w.postContract("bounty", "treasury", market->id, "camp", 0, 5, "Clear the bandits from the east road").id;
    expect(contract(letter).reward == 5, "the letter's reward is set aside");
    step(.5);
    d = open("um", "trade");
    const int any = pick(d, "Any work going?");
    expect(any > 0, "work going: the greeting offers it: " + texts(d));
    d = choose(any);
    expect(d && d->string("tree") == "work" && d->string("node") == "list" && pick(d, "What about this one: a letter for Neighbour u3 (5p)?") > 0 &&
               pick(d, "What about this one: Clear the bandits") > 0 && pick(d, "Goodbye for now.") > 0,
           "the keeper's work, each job in plain words with its pay: " + texts(d));
    // The details: where and what, then the pay and the time, then the risks.
    d = choose(pick(d, "What about this one: a letter"));
    expect(d && d->string("node") == "job" && contains(d->string("line"), "Carry the letter to") && pick(d, "I'll take it on.") > 0 &&
               pick(d, "Not for me") > 0,
           "a job's details: " + (d ? d->string("line") : std::string()) + " " + texts(d));
    d = choose(pick(d, "What does it pay"));
    expect(d && d->string("node") == "terms" && contains(d->string("line"), "5 pennies") &&
               (contains(d->string("line"), "within") || contains(d->string("line"), "before")),
           "the pay and the time it has: " + (d ? d->string("line") : std::string()));
    d = choose(pick(d, "Is there any danger"));
    expect(d && d->string("node") == "risk" && !d->string("line").empty(), "the risks: " + (d ? d->string("line") : std::string()));
    // Declined: nothing changes.
    d = choose(pick(d, "Not for me"));
    expect(d && d->string("node") == "declined" && contract(letter).status == "open" && contract(letter).taker.empty(),
           "declined: the work stays open for anyone");
    // From the wheel's "Ask about: ..." straight to the job, and taken on.
    d = open("um", "job:" + letter);
    expect(d && d->string("node") == "job", "the wheel's Ask about opens the job: " + texts(d));
    const auto heardBefore = heardFrom(town.bo, "um").size();
    d = choose(pick(d, "I'll take it on."));
    expect(d && d->string("node") == "taken" && contract(letter).status == "taken" && contract(letter).taker == town.ash.entityId &&
               contract(letter).byHand,
           "taken on, to be handed over by hand: " + (d ? json::dump(*d) : std::string()) + " status " + contract(letter).status +
               " events " + [&] { std::string all; for (const auto& e : town.ash.events) all += json::dump(e).substr(0, 300) + "\n"; return all; }());
    expect(contains(town.ash.said(), "You take on: a letter for Neighbour u3"), "and told so");
    expect(heardFrom(town.bo, "um").size() > heardBefore || !heardFrom(town.bo, "um").empty(), "the giver's answer is heard");
    step(2);
    {
        bool marked = false;
        for (const auto& m : town.ash.snapshots.back()["self"].array("missions"))
            marked |= m.string("id") == letter;
        expect(marked, "the job is marked on the map: " + json::dump(town.ash.snapshots.back()["self"]["missions"]) + " n=" +
                           std::to_string(town.ash.snapshots.size()) + " " + contract(letter).status + " rev " +
                           json::dump(town.ash.snapshots.back()["revision"]));
    }
    // Already taken: Ash asks about the bounty, Bo takes it meanwhile; Ash's "I'll take it on" is refused in words.
    d = open("um", "job:" + bounty);
    expect(d && pick(d, "I'll take it on.") > 0, "the bounty's details");
    const int takeN = pick(d, "I'll take it on.");
    expect(w.takeContract(town.bo.entityId, bounty).ok, "Bo takes the bounty meanwhile");
    d = choose(takeN);
    expect(d && contains(d->string("note"), "Someone's taken that on already.") && contract(bounty).taker == town.bo.entityId,
           "already taken: refused in words: " + (d ? json::dump(*d) : std::string()));
    expect(lockedWhy(d, "I'll take it on.") == "Someone's taken that on already.", "and the choice greyed with why");
    town.ash.events.clear();
    town.g.command(&town.ash, cmd({{"type", "dialogue_open"}, {"npc", "um"}, {"entry", "job:" + bounty}}));
    expect(lastOf(town.ash, "dialogue_close") && lastOf(town.ash, "dialogue_close")->string("why") == "gone",
           "asked about from the wheel once taken: gone");
    // Promised by letter to someone else: shown greyed, never taken.
    auto& promised = w.postContract("courier", "u2", market->id, "u1", 4, 3, "a letter for Neighbour u1");
    promised.offeredTo = town.cy.entityId;
    promised.offeredUntil = w.calendarDays() + 1;
    const std::string promisedId = promised.id;
    d = open("um", "work");
    expect(lockedWhy(d, "What about this one: a letter for Neighbour u1") == "That's promised to another for now.",
           "work promised to another: greyed, with why: " + texts(d));
    d = open("um", "job:" + promisedId);
    expect(d && lockedWhy(d, "I'll take it on.") == "That's promised to another for now.", "and its take greyed: " + texts(d));
    d = choose(pick(d, "I'll take it on."));
    expect(contract(promisedId).status == "open", "a locked choice does nothing");
    // Expired while asked about: refused in words.
    const std::string late = w.postContract("courier", "u2", market->id, "u1", 3, 3, "a note for Neighbour u1").id;
    d = open("um", "job:" + late);
    expect(d && pick(d, "I'll take it on.") > 0, "a note to carry");
    const int lateTake = pick(d, "I'll take it on.");
    expect(w.withdrawContract(late), "it lapses");
    d = choose(lateTake);
    expect(d && contract(late).status == "expired" && !d->string("note").empty() && contract(late).taker.empty(),
           "expired: refused in words: " + (d ? d->string("note") : std::string()));

    // Coming back to the giver: how it stands, and giving it back, free.
    const auto purse = s.cash(town.ash.entityId);
    d = open("um", "trade");
    d = choose(pick(d, "Any work going?"));
    expect(pick(d, "I wanted to ask about the work I took on.") > 0, "the giver hears about work taken: " + texts(d));
    d = choose(pick(d, "I wanted to ask about the work I took on."));
    expect(d && d->string("node") == "mine" && pick(d, "About this one: a letter for Neighbour u3") > 0, "the work taken, listed: " + texts(d));
    d = choose(pick(d, "About this one: a letter"));
    expect(d && d->string("node") == "progress" && contains(d->string("line"), "waiting on that letter"),
           "how it stands: " + (d ? d->string("line") : std::string()));
    d = choose(pick(d, "I can't do it after all."));
    expect(d && d->string("node") == "given" && contract(letter).status == "open" && contract(letter).taker.empty() && !contract(letter).byHand,
           "given back: open again: " + (d ? json::dump(*d) : std::string()));
    expect(s.cash(town.ash.entityId) == purse && contract(letter).reward == 5, "free: no penalty, the reward still set aside");
    step(2);
    {
        bool marked = false;
        for (const auto& m : town.ash.snapshots.back()["self"].array("missions"))
            marked |= m.string("id") == letter;
        expect(!marked, "and off the map");
    }

    // Taken again, and handed over in the recipient's tree: not by standing near.
    d = open("um", "job:" + letter);
    d = choose(pick(d, "I'll take it on."));
    expect(contract(letter).taker == town.ash.entityId, "taken again");
    town.ash.events.clear();
    step(2);
    expect(contract(letter).status == "taken" && contains(town.ash.said(), "Talk to"), "standing near u3 hands nothing over: told to talk");
    d = open("u1", "deliver:" + letter);
    expect(!d && lastOf(town.ash, "dialogue_close") && lastOf(town.ash, "dialogue_close")->string("why") == "none",
           "the letter isn't for u1");
    const auto before = s.cash(town.ash.entityId);
    d = open("u3", "talk");
    expect(d && d->string("tree") == "receiver" && pick(d, "I've a letter for you, from") > 0,
           "Talk with the recipient: the letter to hand over: " + texts(d) + " " + (d ? d->string("tree") : std::string()));
    d = choose(pick(d, "I've a letter for you"));
    expect(d && d->string("node") == "paid" && contains(d->string("line"), "5 pennies") && contract(letter).status == "done",
           "handed over, the pay said aloud: " + (d ? d->string("line") : std::string()));
    expect(s.cash(town.ash.entityId) == before + 5, "and paid");
    {
        const auto heard = heardFrom(town.bo, "u3");
        expect(std::any_of(heard.begin(), heard.end(), [](const std::string& l) { return l.find("5 pennies") != std::string::npos; }),
               "a bystander hears the thanks");
    }
    expect(s.realBalanced(), "real money is balanced");

    // A faction's official: the Watch, its sergeant u1; the Ashen Lodge of Ash, Bo and Cy.
    auto& f = town.g.factions();
    f.define({"watch", "The Greyfen Watch", "#9aa9c0", "city"});
    f.setMember("u1", "watch", "sergeant", true);
    d = open("u1", "missions");
    expect(d && d->string("tree") == "official" && contains(d->string("line"), "Chapters") && pick(d, "What about this one:") > 0 &&
               !lockedWhy(d, "What about this one:").empty(),
           "a wolf of no Chapter: the board shown, every mission greyed: " + texts(d) + " / " + (d ? d->string("line") : std::string()));
    auto& ch = town.g.chapters();
    ch.propose(town.ash.entityId, {town.bo.entityId, town.cy.entityId}, "Ashen Lodge", "#5b8bd9", "", "scene-x", 0);
    ch.agree(town.bo.entityId, 0);
    const auto lodge = ch.agree(town.cy.entityId, 0).message;
    expect(ch.of(town.ash.entityId), "the Lodge is founded");
    d = open("u1", "missions");
    expect(d && lockedWhy(d, "What about this one:") == "For a Lodge the faction knows well.", "a new Lodge: locked by standing, why: " + texts(d));
    ch.byId(lodge)->level = 2;
    f.change("watch", lodge, 20, "test", w.calendarDays(), 0, false);
    faction::Mission m;
    m.id = f.nextMissionId();
    m.faction = "watch";
    m.kind = "deliver";
    m.item = "meal";
    m.quantity = 1;
    m.official = "u1";
    m.coins = 8;
    m.standing = 4;
    m.renown = 5;
    m.expires = std::floor(w.calendarDays()) + 1;
    f.missions().clear();                           // (Ours first on the day's board.)
    f.missions().push_back(m);
    d = open("u1", "missions");
    const int ours = pick(d, "What about this one: Bring a meal to");
    expect(ours > 0 && lockedWhy(d, "What about this one: Bring a meal to").empty(), "known well: tier one open: " + texts(d));
    expect(contains(texts(d), "For a Company the faction trusts."), "tier two still greyed, with why");
    d = choose(ours);
    expect(d && d->string("tree") == "work" && d->string("node") == "job" && contains(d->string("line"), "Bring a meal to"),
           "a mission's details, in the work tree: " + (d ? d->string("line") : std::string()));
    d = choose(pick(d, "I'll take it on."));
    expect(d && d->string("node") == "taken" && f.mission(m.id)->state == "taken" && f.mission(m.id)->taker == town.ash.entityId,
           "the mission taken: " + (d ? json::dump(*d) : std::string()));
    // Handing in: without the goods, greyed with why; with them, paid and done.
    for (const auto& item : {"meal"})
        if (Society::stock(*s.account(town.ash.entityId), item) > 0)
            s.shift(town.ash.entityId, "treasury", item, Society::stock(*s.account(town.ash.entityId), item), 0, "test: none");
    d = open("u1", "missions");
    expect(lockedWhy(d, "I've brought what you asked for") == "You haven't a meal.", "no meal: greyed, with why: " + texts(d));
    d = choose(pick(d, "I've brought what you asked for"));
    expect(f.mission(m.id)->state == "taken", "a locked hand-in does nothing");
    s.shift("treasury", town.ash.entityId, "meal", 1, 0, "test: a meal");
    const double earned = f.earned("watch", lodge);
    const auto coins = s.cash(town.ash.entityId);
    d = open("u1", "missions");
    d = choose(pick(d, "I've brought what you asked for"));
    expect(d && d->string("node") == "paid" && f.mission(m.id)->state == "done" && f.earned("watch", lodge) == earned + 4,
           "goods handed in: done, standing earned: " + (d ? json::dump(*d) : std::string()));
    expect(s.cash(town.ash.entityId) > coins ? contains(d->string("line"), "8 pennies") || contains(d->string("line"), "pennies")
                                             : contains(d->string("line"), "no coin"),
           "and the reward said aloud: " + d->string("line"));
    // A mission given back, free.
    std::string another;
    for (const auto& one : f.missions())
        if (one.state == "open" && one.tier == 1 && one.faction == "watch")
            another = one.id;
    expect(!another.empty(), "another mission on the board");
    town.g.command(&town.ash, cmd({{"type", "faction"}, {"verb", "take"}, {"mission", another}}));
    expect(f.mission(another)->taker == town.ash.entityId, "taken from the board's old way");
    f.mission(another)->official = "u1";
    d = open("u1", "missions");
    d = choose(pick(d, "I wanted to ask about the work I took on."));
    expect(d && d->string("tree") == "work" && d->string("node") == "mine", "about what was taken: " + texts(d));
    d = choose(pick(d, "About this one:"));
    d = choose(pick(d, "I can't do it after all."));
    expect(d && d->string("node") == "given" && f.mission(another)->state == "open" && f.mission(another)->taker.empty(),
           "a mission given back: on the board again: " + (d ? json::dump(*d) : std::string()));
    // The Chapter's standing, asked in the window.
    d = open("u1", "missions");
    d = choose(pick(d, "I wanted to ask about our Chapter's standing"));
    d = choose(pick(d, "How does"));
    expect(d && d->string("node") == "answer" && !d->string("line").empty() && d->string("line").front() != '"',
           "the standing, said as the official's line: " + (d ? d->string("line") : std::string()));
    // An older client's wheel: "offer kNNN" still opens the old offer window.
    const std::string open2 = w.postContract("bounty", "treasury", market->id, "camp", 0, 5, "Clear the west road").id;
    town.ash.events.clear();
    town.g.command(&town.ash, cmd({{"type", "action"}, {"target", "um"}, {"action", "offer " + open2}}));
    expect(town.ash.last("missionOffer") && contains(town.ash.last("missionOffer")->string("explanation"), "bandit camp"),
           "an older client's offer window still works");
    fs::remove_all(root);
}
// ------------------------------------------------------------------ The other roles (Phase 3)

void roleTrees()
{
    // Each role's tree loads, and validates alone; Talk (and Trade, and the wheel's role entries) finds the right one.
    auto lib = library();
    for (const auto* id : {"inn", "watch", "church", "master", "companion", "resident"})
    {
        expect(lib.tree(id), std::string("the ") + id + " tree loads");
        talk::Library one;
        std::string problem;
        expect(one.loadFoundation(Data, problem), "the foundation loads");
        std::ifstream in(Data + "/dialogue/" + id + ".tree");
        std::stringstream text;
        text << in.rdbuf();
        expect(one.parseTree(text.str(), std::string(id) + ".tree", problem), std::string("the ") + id + " tree validates alone: " + problem);
    }
    const auto who = [&](const talk::Facts& self, const talk::Facts& here = {}) {
        auto q = keeperQuery("");
        q.a.facts = {{"id", "x"}, {"name", "X"}, {"sex", "female"}, {"stage", "adult"}, {"age", "40"}, {"job", "none"}, {"role", "civilian"}};
        for (const auto& [k, v] : self)
            q.a.facts[k] = v;
        q.facts["here.deliveries"] = "0";
        q.facts["here.companion"] = "no";
        for (const auto& [k, v] : here)
            q.facts[k] = v;
        return q;
    };
    const auto talksTo = [&](const talk::Query& q, const std::string& entry = "talk") {
        const auto* t = lib.treeFor(entry, q);
        return t ? t->id : std::string("none");
    };
    expect(talksTo(who({{"role", "guard"}, {"job", "guard"}})) == "watch", "Talk with a guard: the watch");
    expect(talksTo(who({{"role", "merchant"}, {"job", "innkeeper"}, {"trade", "innkeeper"}})) == "inn", "with an innkeeper: the inn");
    expect(talksTo(who({{"role", "merchant"}, {"job", "innkeeper"}}), "trade") == "inn", "Trade with an innkeeper: the inn too");
    expect(talksTo(who({{"role", "merchant"}, {"job", "baker"}}), "trade") == "shop", "Trade with a baker: the shop");
    expect(talksTo(who({{"job", "priest"}})) == "church", "with a priest: the church");
    expect(talksTo(who({{"job", "farmer"}})) == "resident", "with anyone else: the resident's hub");
    expect(talksTo(who({{"role", "guard"}, {"job", "guard"}}, {{"here.companion", "yes"}})) == "companion", "one in the party: the companion's");
    expect(talksTo(who({{"job", "priest"}}, {{"here.deliveries", "1"}})) == "receiver", "one with something to hand over: the receiver's");
    expect(talksTo(who({{"job", "smith"}}), "learn") == "master" && talksTo(who({{"role", "guard"}}), "report") == "watch" &&
               talksTo(who({{"role", "guard"}}), "fine") == "watch",
           "the wheel's Apprentice, Report and Pay fine: the master's and the watch's");
    expect(lib.tree("resident")->entries.count("chat") && lib.tree("resident")->entries.count("news") && lib.tree("resident")->entries.count("way"),
           "every role's 'I was wondering about something else...', news and ways go on in the resident's tree");
    // Only what the server allows: a fine only while wanted; a bed locked, with why, for one who has a lodging; a master
    // who wouldn't take this wolf on, locked with the reason.
    const auto none = [](const std::string&) { return std::vector<talk::Facts>{}; };
    const auto shownOf = [&](const char* tree, const char* node, const talk::Query& q) {
        std::vector<std::string> out;
        for (const auto& c : lib.choices(lib.tree(tree)->nodes.at(node), q, 1, none))
            out.push_back(c.text + (c.locked ? " [" + c.why + "]" : std::string()));
        return out;
    };
    const auto has = [](const std::vector<std::string>& v, const std::string& start) {
        return std::any_of(v.begin(), v.end(), [&](const std::string& t) { return t.rfind(start, 0) == 0; });
    };
    auto guard = who({{"role", "guard"}, {"job", "guard"}}, {{"here.wanted", "no"}, {"here.fine", "0"}});
    expect(!has(shownOf("watch", "hub", guard), "What do I owe"), "not wanted: no fine offered");
    guard.facts["here.wanted"] = "yes";
    guard.facts["here.fine"] = "11";
    expect(has(shownOf("watch", "hub", guard), "What do I owe"), "wanted: the fine offered");
    expect(has(shownOf("watch", "fine", guard), "I'll pay it (11p)."), "and what settles it, said");
    auto keeper = who({{"role", "merchant"}, {"job", "innkeeper"}}, {{"here.inn", "yes"}, {"here.bed_ok", "no"}, {"here.bed_why", "You have a lodging already."},
                                                                   {"here.bed_night", "2"}, {"here.bed_week", "10"}, {"here.upstairs_night", "8"}});
    expect(has(shownOf("inn", "greet", keeper), "I'd like a bed for the night (2p). [You have a lodging already.]"), "a bed, locked for one lodged already");
    auto master = who({{"job", "smith"}}, {{"here.teaches", "yes"}, {"here.take_on", "no"}, {"here.take_on_why", "They don't know you well enough to take you on yet."},
                                          {"here.trade_title", "smith"}, {"here.giver", "no"}});
    expect(has(shownOf("master", "learn", master), "Will you take me on? [They don't know you well enough"), "the master: locked, why");
    // The tree format's new pieces, and their refusals.
    refused("tree t\nentry talk a\nnode a\nsay Hello.\nchoice Bed. -> do lodge bed year\n", "'lodge' is");
    refused("tree t\nentry talk a\nnode a\nsay Hello.\nchoice Sit. -> do order sit\n", "'order' is");
    refused("tree t\nentry talk a\nnode a\nsay Hello.\nchoice Coin. -> do offering 500\n", "'offering' gives");
    refused("tree t\nentry talk a\nnode a\nsay Hello.\nchoice Help. -> do trouble job.id\n", "'trouble' is given");
    refused("tree t\nentry talk a\nnode a\nsay Hello.\neach cat in here.places max 3\nchoice x -> end\n", "an each's item");
}

void otherRoles()
{
    // A strip with an inn's upstairs, a guard at Ridgemere's market and a priest in a chapel off it (an interior, by a
    // door: a place one asks the way to).
    const auto root = fs::temp_directory_path() / ("ratw-dialogue-roles-" + std::to_string(::getpid()));
    const auto worldPath = writeStrip(root / "world", true);
    {
        std::ofstream world(fs::path(worldPath) / "world.ratw", std::ios::app);
        world << resident("ug", "Guard u", "guard", "keeping the watch", 0, 10.5, 1, 6.5);
        world << resident("uh", "Pot hand u", "civilian", "helping at the inn", 0, 13.5, 1, 11.5);   // (Doc 83, Phase 4.)
        auto priest = resident("up", "Mother Ada", "civilian", "tending the chapel", 0, 11.5, 1, 4.5);
        priest.replace(priest.find("\"" + cellId(1) + "\""), cellId(1).size() + 2, "\"c_chapel\"");
        world << priest;
        world << "area \"c_chapel\"\nterritory \"c_chapel\" \"ridgemere\" \"-\" 0\n";
        std::ofstream cell(fs::path(worldPath) / "cells" / "c_chapel.cell");
        cell << "id: c_chapel\nname: The Chapel\ndescription: Pews and a pulpit.\nworld: 0 200 0\noutdoors: false\nweather: clear\nsize: 10 10\ngrid:\n";
        for (int y = 0; y < 10; ++y)
            cell << (y == 9 ? "....+.....\n" : "..........\n");
        {
            // The chapel's door in the market's ground.
            const auto market = fs::path(worldPath) / "cells" / (cellId(1) + ".cell");
            std::ifstream in(market);
            std::stringstream text;
            text << in.rdbuf();
            auto body = text.str();
            const auto grid = body.find("grid:\n") + 6;
            body[grid + 14 * (Side + 1) + 3] = '+';
            in.close();
            std::ofstream(market) << body;
        }
        world << "door \"chapel_a\" \"The chapel door\" \"" << cellId(1) << "\" 3.5 14.5 \"c_chapel\" 4.5 8.5 \"chapel_b\" 1 0 0 0 \"-\"\n"
              << "door \"chapel_b\" \"The chapel door\" \"c_chapel\" 4.5 9.5 \"" << cellId(1) << "\" 3.5 13.5 \"chapel_a\" 1 0 0 0 \"-\"\n";
        std::ofstream(fs::path(worldPath) / "seams" / "c_chapel") << "";
    }
    Town town(worldPath, (root / "save").string(), 120, true);
    auto& w = town.g.world();
    auto& s = w.society();
    town.enter(town.ash, 1, "player-ash", "Ash", 1, 9.5);
    town.enter(town.bo, 2, "player-bo", "Bo", 1, 11.5);
    town.enter(town.cy, 3, "player-cy", "Cy", 7, 8.5);
    w.setTimeOfDay(12);
    town.run(6);                                    // (The estates refresh: the upstairs is to let.)
    std::map<std::string, std::tuple<std::string, double, double>> spots{
        {"um", {cellId(1), 8.5, 8.5}}, {"ui", {cellId(1), 10.5, 9.5}}, {"ug", {cellId(1), 9.5, 7.5}}, {"u1", {cellId(1), 10.5, 7.5}},
        {"u2", {cellId(1), 8.5, 10.5}}, {"up", {"c_chapel", 4.5, 2.5}}, {"uh", {cellId(1), 11.5, 9.5}}};
    std::string ashCell = cellId(1);
    double ashX = 9.5, ashY = 8.5;
    const auto keep = [&] {
        for (const auto& [id, at] : spots)
        {
            auto* e = w.entity(id);
            e->cellId = std::get<0>(at);
            e->position = {std::get<1>(at), std::get<2>(at)};
            e->posture = "standing";
            w.stop(id);
        }
        auto* a = w.entity(town.ash.entityId);
        a->cellId = ashCell;
        a->position = {ashX, ashY};
        town.place(town.bo, 1, 11.5);
    };
    const auto step = [&](double seconds = .5) {
        for (double t = 0; t < seconds; t += .25)
        {
            keep();
            town.run(.25);
        }
        keep();
    };
    step(1);
    expect(town.ash.snapshots.back().boolean("dialogueRoles"), "the snapshot says the roles' entries open the window");
    expect(s.jobOf("ug") && s.jobOf("ug")->role == "guard" && town.g.jobCategoryOf("up") == "priest" && town.g.jobCategoryOf("ui") == "innkeeper",
           "the fixture: a guard, a priest, an innkeeper");
    // Doc 84, Phase 1: residents know each other's names by acquaintance (defect 11), and a keeper behind walls knows
    // the weather out in the street (here.outdoors).
    {
        auto q = town.g.talkQuery("u1", "u2", "talk.smalltalk");
        const bool kinOrWork = s.family("u1", "u2") || s.household("u1", "u2") ||
                               (s.jobOf("u1") && s.jobOf("u2") && s.jobOf("u1")->work.cell == s.jobOf("u2")->work.cell);
        expect(kinOrWork || q.facts["pair.named"] == "no", "two residents who never met don't know each other's names");
        w.bonds().change("u1", "u2", {5, 5, 20, 0, 0}, w.calendarDays());
        q = town.g.talkQuery("u1", "u2", "talk.smalltalk");
        expect(q.facts["pair.named"] == "yes", "acquainted, they do (" + q.facts["pair.named"] + ")");
        const auto inside = town.g.talkQuery("up", "ui", "talk.smalltalk");
        expect(inside.facts.count("here.weather") && inside.facts.at("here.weather") == "indoors" && inside.facts.count("here.outdoors") &&
                   inside.facts.at("here.outdoors") != "indoors",
               "indoors, the weather outside is known: here.outdoors");
        expect(inside.facts.count("here.hour") && inside.facts.count("here.fishers"), "and the hour and the town's fishers");
        // Doc 84, Phase 2: a line that binds x (binds x=self.priest) names one of the speaker's people, with their facts
        // and how the speaker feels about them; a slot nobody fills, or one the plain query can't (topic.subject), binds
        // nothing.
        int bound = 0;
        for (const char* who : {"um", "ui", "ug", "u1", "u2", "uh"})
            for (const char* slot : {"self.priest", "self.shopkeeper", "self.leader", "self.neighbour", "self.spouse"})
            {
                auto qx = town.g.talkQuery(who, "up", "talk.smalltalk");
                talk::Facts f;
                if (!qx.bind || !qx.bind(slot, 0, f))
                    continue;
                ++bound;
                expect(f.count("name") && f.count("id") && f.at("id") != who && w.entity(f.at("id")) && f.count("stage") &&
                           f.count("affinity") && f.count("disposition"),
                       std::string("bound ") + slot + " of " + who + ": a resident's facts and the speaker's bond");
            }
        // Slice 2's review (§13): a player's character is never given a line by the engine: the query says who is one.
        const auto toPlayer = town.g.talkQuery("ui", town.ash.entityId, "react.customer");
        expect(toPlayer.b.player && !toPlayer.a.player, "a player spoken to is marked a player, the resident isn't");
        auto qx = town.g.talkQuery("u1", "u2", "talk.smalltalk");
        talk::Facts none;
        expect(bound > 0 && qx.bind && !qx.bind("self.cousin", 0, none) && !qx.bind("topic.subject", 0, none),
               "some resident's people bind (" + std::to_string(bound) + "); nothing else does");
    }
    const auto open = [&](const std::string& npc, const std::string& entry) -> const json::Value* {
        if (const auto* d = town.g.dialogueOf(town.ash.entityId))
            town.g.command(&town.ash, cmd({{"type", "dialogue_close"}, {"session", d->id}, {"why", "closed"}}));
        for (auto* c : {&town.ash, &town.bo, &town.cy})
            c->events.clear();
        town.g.command(&town.ash, cmd({{"type", "dialogue_open"}, {"npc", npc}, {"entry", entry}}));
        town.g.settle();
        return lastOf(town.ash, "dialogue");
    };
    const auto choose = [&](int n, const std::string& text = {}) -> const json::Value* {
        const auto* d = town.g.dialogueOf(town.ash.entityId);
        expect(d, "a conversation to choose in");
        for (auto* c : {&town.ash, &town.bo, &town.cy})
            c->events.clear();
        auto o = json::Value::object();
        o.add("type", "dialogue_choose");
        o.add("session", d->id);
        o.add("n", n);
        if (!text.empty())
            o.add("text", text);
        town.g.command(&town.ash, json::dump(o));
        town.g.settle();
        return lastOf(town.ash, "dialogue");
    };
    const auto texts = [](const json::Value* d) {
        std::string all;
        if (d)
            for (const auto& c : d->array("choices"))
                all += "[" + c.string("text") + (c.boolean("locked") ? " (locked: " + c.string("why") + ")" : std::string()) + "]";
        return all;
    };
    const auto pick = [&](const json::Value* d, const std::string& starts) {
        if (d)
            for (const auto& c : d->array("choices"))
                if (c.string("text").rfind(starts, 0) == 0)
                    return int(c.number("n"));
        return 0;
    };
    const auto show = [&](const json::Value* d) { return d ? d->string("tree") + "/" + d->string("node") + ": " + d->string("line") + " " + texts(d) : std::string("none"); };

    // The resident's hub: the town from its real numbers; the way to a place, from the world's own posts and doors.
    auto* d = open("u1", "talk");
    expect(d && d->string("tree") == "resident" && d->string("node") == "hub", "Talk with a neighbour: the hub: " + show(d));
    // (Doc 84 §13.4.3, Phase 13: the hub rebuilt: the news, the about node, a name given, goodbye; the town, the work and the
    // way under the about node)
    for (const auto* topic : {"Heard any news?", "I want to know more about something...", "I'm Ash.", "Goodbye for now."})
        expect(pick(d, topic) > 0, std::string("the hub offers: ") + topic + " " + texts(d));
    expect(d && d->array("choices").size() <= 9, "nine choices at most: " + texts(d));
    talk::Facts facts;
    town.g.dialogueRoleFacts(town.ash.entityId, "u1", facts);
    d = choose(pick(d, "I want to know more about something..."));
    for (const auto* topic : {"Tell me about yourself.", "What's it like around here?", "Could you tell me the way?", "I was wondering about something else..."})
        expect(pick(d, topic) > 0, std::string("the about node offers: ") + topic + " " + texts(d));
    d = choose(pick(d, "What's it like around here?"));
    expect(d && d->string("node") == "town" && (contains(d->string("line"), facts["here.folk"]) || contains(d->string("line"), facts["here.shops"]) ||
                                                contains(d->string("line"), "times") || contains(d->string("line"), "doing well")),
           "the town, by its numbers (" + facts["here.folk"] + " folk, " + facts["here.shops"] + " shops): " + show(d));
    d = choose(pick(d, "I was wondering about something else..."));
    d = choose(pick(d, "I want to know more about something..."));
    d = choose(pick(d, "Could you tell me the way?"));
    expect(d && d->string("node") == "way" && pick(d, "How do I get to The Chapel (the church)?") > 0, "the ways: the chapel among them: " + show(d));
    d = choose(pick(d, "How do I get to The Chapel (the church)?"));
    expect(d && d->string("node") == "place" && contains(d->string("line"), "The Chapel is close by to the") &&
               contains(d->string("line"), "south") && contains(d->string("line"), "Mother Ada"),
           "the way to it, from where they stand, and who's there: " + show(d));
    // Gossip: what they've heard lately, of someone else.
    w.believe("u1", "u2", "stole from Merchant u", "saw it", .9);
    d = open("u1", "talk");
    d = choose(pick(d, "Heard any news?"));
    d = choose(pick(d, "Any gossip going round?"));
    expect(d && d->string("node") == "gossip" && contains(d->string("line"), "Neighbour u2 stole from Merchant u"),
           "the gossip is what they heard: " + show(d));

    // The watch: the law, a report (nothing new, then a theft seen), a fine only while wanted.
    d = open("ug", "talk");
    expect(d && d->string("tree") == "watch" && pick(d, "What do I owe") == 0 && pick(d, "I want to report a crime.") > 0 &&
               pick(d, "What's the law here?") > 0 && pick(d, "Any bounties posted?") > 0,
           "Talk with a guard: the watch, no fine while not wanted: " + show(d));
    d = choose(pick(d, "Any bounties posted?"));
    town.g.dialogueRoleFacts(town.ash.entityId, "ug", facts);
    expect(d && d->string("line") == facts["here.bounty"], "the bounties, as the roads stand: " + show(d));
    d = open("ug", "report");
    expect(d && d->string("node") == "crime", "the wheel's Report opens the watch at the crime: " + show(d));
    d = choose(pick(d, "I saw it with my own eyes."));
    expect(d && d->string("node") == "reported" && contains(d->string("line"), "nothing new") | contains(d->string("line"), "Nothing we don't know"),
           "nothing seen: nothing new to them: " + show(d));
    {
        Incident inc;
        inc.id = "inc-test";
        inc.kind = "theft";
        inc.offender = "u2";
        inc.victim = "um";
        inc.cell = cellId(1);
        inc.town = w.lawTown(cellId(1));
        inc.day = w.calendarDays();
        inc.witnesses.push_back({town.ash.entityId, true, 1, false});
        w.crime().incidents.push_back(inc);
    }
    d = open("ug", "report");
    d = choose(pick(d, "I saw it with my own eyes."));
    expect(d && d->string("node") == "reported" && (contains(d->string("line"), "Noted") || contains(d->string("line"), "find them")),
           "a theft seen: noted: " + show(d));
    expect(w.crime().incidents.back().witnesses.front().reported, "and the watch has it");
    w.crime().warrants.push_back({town.ash.entityId, w.lawTown(cellId(1)), {}, {}, 3, w.calendarDays()});
    const auto cash = s.cash(town.ash.entityId);
    d = open("ug", "talk");
    expect(d && pick(d, "What do I owe the watch?") > 0 && contains(d->string("line"), "3"), "wanted: the fine, offered and said: " + show(d));
    d = choose(pick(d, "What do I owe the watch?"));
    d = choose(pick(d, "I'll pay it (3p)."));
    expect(d && d->string("node") == "paid" && !w.warrantFor(town.ash.entityId) && s.cash(town.ash.entityId) == cash - 3,
           "paid in the window, as Pay fine pays: " + show(d));
    d = open("ug", "fine");
    expect(d && d->string("node") == "fine" && pick(d, "I'll pay it") == 0, "and not offered again: " + show(d));
    town.ash.events.clear();
    town.g.command(&town.ash, cmd({{"type", "dialogue_open"}, {"npc", "u1"}, {"entry", "report"}}));
    expect(town.ash.last("dialogue_close") && town.ash.last("dialogue_close")->string("why") == "none", "a neighbour isn't the watch");

    // The inn: a bed upstairs from the keeper downstairs, as the lodging's own rule takes it; then locked, with why.
    d = open("ui", "talk");
    expect(d && d->string("tree") == "inn" && pick(d, "I'd like a bed for the night (2p).") > 0, "Talk with the innkeeper: the inn: " + show(d));
    d = open("ui", "trade");
    expect(d && d->string("tree") == "inn", "Trade with them too: " + show(d));
    const auto before = s.cash(town.ash.entityId);
    d = choose(pick(d, "I'd like a bed for the night (2p)."));
    expect(d && d->string("node") == "roomed", "the bed taken: " + show(d));
    expect(contains(town.ash.said(), "You take a bed for the night (2p)") && s.cash(town.ash.entityId) == before - 2,
           "as the lodging rule takes it (2p, upstairs): " + town.ash.said());
    d = open("ui", "talk");
    const auto* bed = d ? &d->array("choices")[std::size_t(pick(d, "I'd like a bed for the night") - 1)] : nullptr;
    expect(bed && bed->boolean("locked") && bed->string("why") == "You have a lodging already.", "one lodging a wolf: greyed, why: " + show(d));
    d = choose(pick(d, "What's the news of the house?"));
    expect(d && d->string("tree") == "resident" && d->string("node") == "news", "the news of the house: " + show(d));
    d = choose(pick(d, "I was wondering about something else..."));
    expect(d && d->string("tree") == "inn" && d->string("node") == "greet", "and back to the innkeeper's own: " + show(d));

    // A pot hand at the inn (doc 83, Phase 4): the inn's tree, but only the keeper lets the beds.
    d = open("uh", "talk");
    expect(d && d->string("tree") == "inn" && contains(d->string("line"), "Keeper u"), "a helper at the inn sends one to the keeper: " + show(d));
    bed = d && pick(d, "I'd like a bed for the night") > 0 ? &d->array("choices")[std::size_t(pick(d, "I'd like a bed for the night") - 1)] : nullptr;
    expect(bed && bed->boolean("locked") && bed->string("why") == "Ask Keeper u: the rooms are theirs to let.",
           "and the bed is the keeper's to let: " + show(d));
    expect(pick(d, "What other rooms") == 0 && pick(d, "I'd like something to eat or drink") == 0, "no rooms, no counter: " + show(d));

    // A master: their trade from the hub; taken on only when they know and trust the wolf.
    d = open("um", "talk");
    expect(d && d->string("tree") == "resident" && pick(d, "Could you teach me your trade?") > 0, "the hub offers to learn the trade: " + show(d));
    d = choose(pick(d, "Could you teach me your trade?"));
    expect(d && d->string("tree") == "master" && d->string("node") == "learn" && pick(d, "What does it pay?") > 0, "the master's: " + show(d));
    expect(!contains(d->string("line"), "the stall"), "the trade in a word, not the post's title: " + show(d));
    const auto* ask = &d->array("choices")[std::size_t(pick(d, "Will you take me on?") - 1)];
    expect(ask->boolean("locked") && contains(ask->string("why"), "know you well enough"), "a stranger: locked, why: " + show(d));
    w.bonds().change("um", town.ash.entityId, {50, 50, 50, 0, 0}, w.calendarDays());
    d = open("um", "learn");
    expect(d && d->string("tree") == "master", "the wheel's Apprentice opens the master's: " + show(d));
    d = choose(pick(d, "Will you take me on?"));
    expect(d && d->string("node") == "taken" && s.apprenticedTo(town.ash.entityId) && s.apprenticedTo(town.ash.entityId)->id == s.jobOf("um")->id,
           "taken on as an apprentice: " + show(d));
    d = open("ui", "learn");
    ask = d ? &d->array("choices")[std::size_t(pick(d, "Will you take me on?") - 1)] : nullptr;
    expect(ask && ask->boolean("locked") && ask->string("why") == "You're learning a trade already.", "one trade at a time: " + show(d));

    // The church: the service's hours, an offering (a gift of coins, to the priest).
    ashCell = "c_chapel";
    ashX = 4.5, ashY = 3.5;
    step(.5);
    d = open("up", "talk");
    expect(d && d->string("tree") == "church", "Talk with the priest: the church: " + show(d));
    d = choose(pick(d, "When is the service?"));
    expect(d && contains(d->string("line"), "from nine until eleven in the morning"), "the service's hours: " + show(d));
    d = choose(pick(d, "I'd like to make an offering to the church."));
    const auto mine = s.cash(town.ash.entityId), theirs = s.cash("up");
    d = choose(pick(d, "Here are five pennies."));
    expect(d && d->string("node") == "thanked" && s.cash(town.ash.entityId) == mine - 5 && s.cash("up") == theirs + 5,
           "five pennies given: " + show(d));
    const auto guardAt = spots["ug"], helperAt = spots["uh"];
    spots["ug"] = {"c_chapel", 7.5, 2.5};           // (The guard and the pot hand away: a keeper of their own looks up.)
    spots["uh"] = {"c_chapel", 8.5, 2.5};
    step(2.5);                                      // (The game looks where players are every two seconds.)
    town.ash.events.clear();
    ashCell = cellId(1);
    ashX = 9.5, ashY = 8.5;
    step(2.5);
    // Doc 84 Phase 12 (§13.1.4): back in the market, a keeper looks up as the customer comes in (a look, never a word:
    // react.customer is the window's now), and the keeper's window greets one who has just come in.
    {
        std::string keeper, look;
        for (const auto& e : town.ash.events)
            if (e.string("type") == "roleplay" && !e.string("speakerId").empty() && e.string("speakerId") != town.ash.entityId)
            {
                bool speech = false;
                for (const auto& seg : e.array("segments"))
                    speech = speech || seg.string("kind") == "speech";
                expect(!speech || e.array("to").empty() || e.array("to")[0].asString() != "you",
                       "no keeper greets a customer aloud: " + e.string("text"));
                if (!speech && (contains(e.string("text"), "looks up") || contains(e.string("text"), "glances up")))
                    keeper = e.string("speakerId"), look = e.string("text");
            }
        expect(!keeper.empty(), "a keeper looks up as Ash comes in: " + town.ash.said());
        expect(keeper == "ui" || keeper == "um", "the innkeeper or the master: " + keeper);
        {
            d = open(keeper, keeper == "ui" ? "talk" : "trade");
            expect(d && (contains(d->string("line"), "Come in") || contains(d->string("line"), "Sit anywhere") ||
                         contains(d->string("line"), "Have a look around")),
                   "and the window's greeting knows the customer has just come in (" + look + "): " + show(d));
            if (town.g.dialogueOf(town.ash.entityId))
                choose(pick(d, "Goodbye for now."));
        }
        spots["ug"] = guardAt;
        spots["uh"] = helperAt;
        step(.5);
    }

    // A companion: the party's orders as words (they answer in their own, and the conversation ends).
    w.setTimeOfDay(19);
    spots.erase("u2");
    step(.5);
    w.entity("u2")->position = {8.5, 9.5};
    w.entity("u2")->cellId = cellId(1);
    w.stop("u2");
    step(.5);
    std::string hire;
    for (const auto& e : town.ash.snapshots.back().array("entities"))
        if (e.string("id") == "u2")
            for (const auto& a : e.array("actions"))
                if (a.isString() && a.asString().rfind("hire for ", 0) == 0)
                    hire = a.asString();
    expect(!hire.empty(), "u2 may be hired along");
    town.ash.events.clear();
    town.g.command(&town.ash, cmd({{"type", "action"}, {"target", "u2"}, {"action", hire}}));
    const auto along = [&](const char* fact) {
        talk::Facts f;
        town.g.dialogueRoleFacts(town.ash.entityId, "u2", f);
        return f[fact] == "yes";
    };
    expect(along("here.companion"), "the fixture: u2 comes along: " + town.ash.said());
    w.entity("u2")->position = {8.5, 9.5};
    w.entity("u2")->cellId = cellId(1);
    w.stop("u2");
    d = open("u2", "talk");
    expect(d && d->string("tree") == "companion" && pick(d, "Wait here for me.") > 0 && pick(d, "Follow me.") == 0,
           "Talk with a companion: their orders: " + show(d));
    choose(pick(d, "Wait here for me."));
    expect(along("here.waiting") && !town.g.dialogueOf(town.ash.entityId), "waiting, and the conversation over");
    w.entity("u2")->position = {8.5, 9.5};
    d = open("u2", "orders");
    expect(d && pick(d, "Follow me.") > 0 && pick(d, "Wait here for me.") == 0, "waiting: follow is offered: " + show(d));
    // "What do you think?" (doc 84 §13.1.6, Phase 12): once said to a companion in open chat and answered there; now a
    // choice in their window, answered from their own stances (their view on one of the realm's issues), else words
    // for having none.
    {
        const auto q = town.g.talkQuery("u2", town.ash.entityId, "");
        bool holds = false;
        for (const auto& [k, v] : q.a.facts)
            holds = holds || (k.rfind("op_strength.", 0) == 0 && std::atoi(v.c_str()) >= 1);
        d = choose(pick(d, "What do you think?"));
        const auto line = d ? d->string("line") : std::string();
        const bool none = contains(line, "Hard to say") || contains(line, "no strong view") || contains(line, "keep my thoughts");
        expect(d && d->string("node") == "think" && !line.empty() && (holds ? !none : none),
               std::string("What do you think?: ") + (holds ? "their view" : "no view to give") + ": " + show(d));
        d = choose(pick(d, "Goodbye for now."));
    }
    // The window's gossip keeps the street's rules: nobody gossips about their own kin (doc 84 §24, the slice-end review).
    // Married to u2, u1 has no gossip of him: a widow never says "Sad news. Orm died." of her husband.
    w.believe("u1", "u2", "stole from Merchant u", "saw it", .9);
    talk::Facts gossip;
    town.g.dialogueRoleFacts(town.ash.entityId, "u1", gossip);
    expect(gossip["here.gossip"] == "yes" && gossip["topic.subject"] == "Neighbour u2", "the fixture: u1 has gossip of u2");
    expect(s.marry("u1", "u2"), "u1 marries u2");
    gossip.clear();
    town.g.dialogueRoleFacts(town.ash.entityId, "u1", gossip);
    expect(gossip["here.gossip"] == "no", "and has none of her husband for the window: " + gossip["topic.subject"]);
    expect(s.realBalanced(), "real money balanced");
    fs::remove_all(root);
}
// A stand-in NPC Mind: answers POST /dialogue after a little while (the live Mind's 1-5 s) with a reply that carries a
// promise, as a model might despite the window's brief; anything else 503.
struct SlowMind
{
    int port = 0, fd = -1;
    std::atomic<int> asked{0};
    std::atomic<bool> stop{false};
    std::thread worker;
    SlowMind()
    {
        fd = ::socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        ::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof addr);
        socklen_t len = sizeof addr;
        ::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len);
        ::listen(fd, 8);
        port = ntohs(addr.sin_port);
        worker = std::thread([this] {
            while (!stop)
            {
                const int c = ::accept(fd, nullptr, nullptr);
                if (c < 0)
                    break;
                char buf[65536];
                std::string in;
                for (;;)
                {
                    const auto n = ::recv(c, buf, sizeof buf, 0);
                    if (n <= 0)
                        break;
                    in.append(buf, std::size_t(n));
                    const auto head = in.find("\r\n\r\n");
                    const auto at = in.find("Content-Length: ");
                    if (head != std::string::npos && at != std::string::npos && in.size() >= head + 4 + std::stoul(in.substr(at + 16)))
                        break;
                }
                std::string reply = R"({"error": "busy"})", status = "503 Service Unavailable";
                if (in.rfind("POST /dialogue ", 0) == 0)
                {
                    ++asked;
                    std::this_thread::sleep_for(std::chrono::milliseconds(600));
                    reply = R"({"text": "I'll see to it, never fear.", "emotion": "warm", "promise": {"by": "npc", "what": "to mind the cart"}})";
                    status = "200 OK";
                }
                const std::string out = "HTTP/1.1 " + status + "\r\nContent-Type: application/json\r\nContent-Length: " +
                                        std::to_string(reply.size()) + "\r\n\r\n" + reply;
                ::send(c, out.data(), out.size(), 0);
                ::close(c);
            }
        });
    }
    ~SlowMind()
    {
        stop = true;
        ::shutdown(fd, SHUT_RDWR);
        ::close(fd);
        worker.join();
    }
    std::string endpoint() const { return "http://127.0.0.1:" + std::to_string(port) + "/dialogue"; }
};

void windowPromises()
{
    // The window's free talk makes no promises (doc 83's brief; doc 84, defect 15), decided where the words were said: the
    // window closed while the Mind was still writing (bye, the shop, walking away) doesn't let a promise in its answer
    // through (the slice-end review).
    SlowMind mind;
    const auto root = fs::temp_directory_path() / ("ratw-dialogue-promise-" + std::to_string(::getpid()));
    const auto worldPath = writeStrip(root / "world");
    Town town(worldPath, (root / "save").string(), 120, true, 3 * 86400, mind.endpoint());
    auto& w = town.g.world();
    town.enter(town.ash, 1, "player-ash", "Ash", 1, 9.5);
    w.setTimeOfDay(12);
    town.run(2);
    const auto keep = [&] {
        auto* n = w.entity("u1");
        n->cellId = cellId(1);
        n->position = {10.5, 9.5};
        w.stop("u1");
        town.place(town.ash, 1, 9.5);
    };
    keep();
    town.run(.5);
    keep();
    town.ash.events.clear();
    town.g.command(&town.ash, cmd({{"type", "dialogue_open"}, {"npc", "u1"}, {"entry", "talk"}}));
    const auto* d = town.g.dialogueOf(town.ash.entityId);
    expect(d && d->npc == "u1", "Talk opens a window with u1");
    {
        auto o = json::Value::object();
        o.add("type", "dialogue_say");
        o.add("session", d->id);
        o.add("text", "Would you keep an eye on my cart while I am away up the road?");
        town.g.command(&town.ash, json::dump(o));
    }
    // Closed before the answer comes.
    town.g.command(&town.ash, cmd({{"type", "dialogue_close"}, {"session", d->id}, {"why", "bye"}}));
    expect(!town.g.dialogueOf(town.ash.entityId), "the window closed before the answer");
    bool answered = false;
    for (int i = 0; i < 40 && !answered; ++i)
    {
        keep();
        town.run(.25);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        for (const auto& e : town.ash.events)
            answered |= json::dump(e).find("never fear") != std::string::npos;
    }
    expect(mind.asked > 0 && answered, "the Mind's answer came, after the window closed (asked " + std::to_string(mind.asked.load()) + ")");
    bool promised = false;
    for (const auto& p : w.promises())
        promised |= p.by == "u1" || p.to == "u1";
    expect(!promised && w.promisesBetween("u1", town.ash.entityId, "Ash").empty(),
           "and the promise in it, said of words from the window, is let go");
    fs::remove_all(root);
}

void windowTalk()
{
    // Doc 84 §13.4-13.6 (Phase 13): the window's talk and typed words. The loader holds every node to nine choices; the
    // hub rebuilt (a name given, Ask about...); an `answers` node; typed words understood without the Mind (a choice by its
    // words, a node, a move, "didn't follow", goodbye) and when its call fails; what the player heard brought up; the
    // ledger's callback; lines turned over.
    std::string problem;
    {
        talk::Library lib;
        expect(lib.load(RATW_SOURCE_DIR "/Data/Talk", problem), "the library loads: " + problem);
        std::string ten = "tree capped\nentry capped hub\nnode hub\nsay Yes?\n";
        for (int i = 1; i <= 10; ++i)
            ten += "choice Choice " + std::to_string(i) + ". -> hub\n";
        const bool tenOk = lib.parseTree(ten, "capped.tree", problem);
        expect(!tenOk && contains(problem, "could show 10 choices"), "a node of ten choices is refused: " + problem);
        std::string nine = "tree capped2\nentry capped2 hub\nnode hub\nsay Yes?\n";
        nine += "choice Wares. [role=merchant] -> hub\nchoice Missions. [role!=merchant] -> hub\n";
        for (int i = 1; i <= 7; ++i)
            nine += "choice Choice " + std::to_string(i) + ". -> hub\n";
        nine += "each heard in here.heard max 1\nchoice I heard #heard.words#. [does tell.news] -> hub\n";
        const bool nineOk = lib.parseTree(nine, "capped2.tree", problem);
        expect(nineOk, "choices that can't show together count once: " + problem);
        std::string bad = "tree capped3\nentry capped3 hub\nnode hub\nanswers ask.nothing\nsay Yes?\nchoice Hm. [does no.such] -> hub\n";
        const bool badOk = lib.parseTree(bad, "capped3.tree", problem);
        expect(!badOk && contains(problem, "not a move"), "an unknown move is refused: " + problem);
        for (const auto& [id, t] : lib.trees())
            for (const auto& [n, node] : t.nodes)
                expect(talk::Library::mostShown(node) <= 9, "every node of every tree shows nine at most: " + id + "." + n);
    }
    for (const bool failing : {false, true})
    {
        // Without the Mind; then with one that is plugged in but fails (a port nobody answers): the same understanding.
        const auto root = fs::temp_directory_path() / ("ratw-dialogue-window-" + std::to_string(::getpid()) + (failing ? "-f" : ""));
        const auto worldPath = writeStrip(root / "world");
        Town town(worldPath, (root / "save").string(), 120, true, 3 * 86400, failing ? "http://127.0.0.1:9/dialogue" : "");
        auto& w = town.g.world();
        town.enter(town.ash, 1, "player-ash", "Ash", 1, 9.5);
        w.setTimeOfDay(12);
        town.run(2);
        const auto keep = [&](const char* who) {
            auto* n = w.entity(who);
            n->cellId = cellId(1);
            n->position = {10.5, 9.5};
            w.stop(who);
            town.place(town.ash, 1, 9.5);
        };
        const auto open = [&](const char* npc) -> const json::Value* {
            if (const auto* d = town.g.dialogueOf(town.ash.entityId))
                town.g.command(&town.ash, cmd({{"type", "dialogue_close"}, {"session", d->id}, {"why", "closed"}}));
            keep(npc);
            town.ash.events.clear();
            town.g.command(&town.ash, cmd({{"type", "dialogue_open"}, {"npc", npc}, {"entry", "talk"}}));
            town.g.settle();
            return lastOf(town.ash, "dialogue");
        };
        const auto pick = [&](const json::Value* d, const std::string& starts) {
            if (d)
                for (const auto& c : d->array("choices"))
                    if (c.string("text").rfind(starts, 0) == 0)
                        return int(c.number("n"));
            return 0;
        };
        const auto texts = [](const json::Value* d) {
            std::string all;
            if (d)
                for (const auto& c : d->array("choices"))
                    all += "[" + c.string("text") + "]";
            return all;
        };
        const auto choose = [&](int n) -> const json::Value* {
            const auto* d = town.g.dialogueOf(town.ash.entityId);
            town.ash.events.clear();
            if (d)
                town.g.command(&town.ash, cmd({{"type", "dialogue_choose"}, {"session", d->id}, {"n", n}}));
            town.g.settle();
            return lastOf(town.ash, "dialogue");
        };
        const auto type = [&](const std::string& text) -> const json::Value* {
            town.run(1.1);                          // (A line a second at most, as chat.)
            keep(town.g.dialogueOf(town.ash.entityId) ? town.g.dialogueOf(town.ash.entityId)->npc.c_str() : "u1");
            const auto* d = town.g.dialogueOf(town.ash.entityId);
            town.ash.events.clear();
            if (d)
                town.g.command(&town.ash, cmd({{"type", "dialogue_say"}, {"session", d->id}, {"text", text}}));
            town.g.settle();
            for (int i = 0; i < 100 && !lastOf(town.ash, "dialogue_close"); ++i)
            {
                const auto* last = lastOf(town.ash, "dialogue");
                if (last && !last->boolean("awaiting") && i > 0)
                    break;
                town.run(.1);                       // (A failing Mind's answer comes back on the game thread.)
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            return lastOf(town.ash, "dialogue");
        };
        const auto show = [&](const json::Value* d) { return d ? d->string("node") + ": " + d->string("line") + " " + texts(d) : std::string("none"); };
        const std::string mode = failing ? " (the Mind failing)" : " (no Mind)";

        // The hub: a name to give, nine choices at most, for every resident here.
        for (const char* who : {"u1", "um", "ui"})
        {
            const auto* d = open(who);
            expect(d && d->array("choices").size() <= 9, std::string("nine at most at ") + who + "'s: " + show(d));
        }
        auto* d = open("u1");
        expect(d && d->string("node") == "hub" && pick(d, "I'm Ash.") > 0 && pick(d, "What's your name?") == 0, "the hub: a name to give: " + show(d));
        d = choose(pick(d, "I'm Ash."));
        expect(d && d->string("node") == "introduced" && contains(d->string("line"), "Ash"),
               "introduced: the name learned and said back: " + show(d));
        d = open("u1");
        expect(pick(d, "I'm Ash.") == 0 && pick(d, "What's your name?") == 0, "and not offered again, theirs given back: " + show(d));
        // An `answers` node: the move made, answered.
        d = choose(pick(d, "I want to know more about something..."));
        d = choose(pick(d, "Tell me about yourself."));
        expect(d && d->string("node") == "job" && pick(d, "How are you keeping") > 0, "about themselves: " + show(d));
        d = choose(pick(d, "How are you keeping"));
        expect(d && d->string("node") == "how" && !d->string("line").empty(), "how they are keeping, answered: " + show(d));

        // Typed words (§13.5): a choice by its own words; a node by what they mean; a move answered; not followed.
        d = open("u1");
        d = type("heard any news lately?");
        expect(d && d->string("node") == "news", "\"heard any news lately?\": the news" + mode + ": " + show(d));
        d = type("What do you make of the Watch these days?");
        expect(d && d->string("node") == "opinion" && !d->string("line").empty(), "an opinion asked: the opinion node" + mode + ": " + show(d));
        const auto before = texts(d);
        d = type("can you read the stars for me?");
        expect(d && d->string("node") == "opinion" && texts(d) == before && !d->string("line").empty(),
               "not followed: said in character, the choices again" + mode + ": " + show(d));
        d = type("You're a fool.");
        expect(d && !d->string("line").empty(), "an insult: answered" + mode + ": " + show(d));
        d = open("um");
        d = type("show me your wares");
        expect(lastOf(town.ash, "dialogue_shop") != nullptr || (d && d->string("node") == "hub"),
               "\"show me your wares\" at the counter: the shop" + mode);
        d = open("u1");
        type("Well, I'll be off. Goodbye.");
        expect(lastOf(town.ash, "dialogue_close") && !town.g.dialogueOf(town.ash.entityId), "goodbye: the window closes" + mode);

        // What the player heard, brought up (§13.4.5): a claim about another, then told.
        Belief b;
        b.holder = "u3";
        b.subject = "u2";
        b.claim = "stole from Merchant u";
        b.confidence = .9;
        b.day = w.calendarDays();
        auto heard = town.g.claimHeard(town.ash.entityId, b, "u3");
        expect(!heard.key.empty() && contains(heard.words, "Neighbour u2"), "a claim in the player's words: " + heard.words);
        town.g.playerHeard(town.ash.entityId, heard);
        d = open("u1");
        expect(pick(d, "I heard") > 0, "the hub offers to bring it up: " + show(d));
        d = choose(pick(d, "I heard"));
        bool holds = false;
        if (const auto* held = w.beliefsOf("u1"))
            for (const auto& x : *held)
                holds |= x.subject == "u2";
        expect(d && d->string("node") == "told" && !d->string("line").empty() && holds, "told: answered, and it holds the claim now: " + show(d));
        d = open("u1");
        expect(pick(d, "I heard") == 0, "and it isn't offered again to the one told: " + show(d));

        // (With the Mind plugged in, the router's own answers come first, as before: "Merchant u works at...". The rest is
        // the window's alike.)
        if (failing)
        {
            d = type("Where's Merchant u?");
            expect(d && contains(d->string("line"), "Merchant u"), "the router's answer, the Mind plugged in: " + show(d));
            fs::remove_all(root);
            continue;
        }
        // The ledger with players (§13.4.7): someone asked after, remembered, and brought up a while later (once).
        d = type("Where's Merchant u?");
        expect(d && d->string("node") == "whereis", "asked after Merchant u" + mode + ": " + show(d));
        town.g.command(&town.ash, cmd({{"type", "dialogue_close"}, {"session", town.g.dialogueOf(town.ash.entityId)->id}, {"why", "bye"}}));
        auto later = w.save();
        later.calendarDays += .3;
        expect(w.restore(later).ok, "a while passes");
        d = open("u1");
        expect(d && contains(d->string("line"), "Merchant u"), "the callback: " + show(d));
        d = open("u1");
        expect(d && !contains(d->string("line"), "Merchant u"), "said once: " + show(d));
        // Stage 4's review: a stranger's whole name isn't told from a surname alone; a typed refusal takes no choice; a
        // sleeping resident told a name by the wheel's Introduce doesn't give its own back.
        d = open("u1");
        d = type("Where can I find u3?");
        expect(d && !contains(d->string("line"), "Neighbour u3"), "a surname alone tells no stranger's whole name: " + show(d));
        {
            d = open("u2");
            expect(d && pick(d, "I'm Ash.") > 0, "u2 not yet told Ash's name: " + show(d));
            auto* sleeper = w.entity("u2");
            keep("u2");
            if (const auto* open = town.g.dialogueOf(town.ash.entityId))
                town.g.command(&town.ash, cmd({{"type", "dialogue_close"}, {"session", open->id}, {"why", "closed"}}));
            town.run(1.1);
            sleeper->posture = "lying";
            town.ash.events.clear();
            town.g.command(&town.ash, cmd({{"type", "dialogue_open"}, {"npc", "u2"}, {"entry", "introduce"}}));
            town.g.settle();
            const auto* asleep = lastOf(town.ash, "dialogue");
            expect(!asleep || !contains(asleep->string("line"), "Neighbour u2"), "asleep: its name not given: " + show(asleep));
            sleeper->posture = "standing";
            town.run(4);
            d = open("u2");
            expect(d && pick(d, "I'm Ash.") > 0, "asleep when told Ash's name: not learned (offered again awake): " + show(d));
        }
        // Lines turn over: the hub's greeting not the same twice running.
        std::string last, again;
        int same = 0;
        for (int i = 0; i < 6; ++i)
        {
            town.run(4);
            d = open("u1");
            again = d ? d->string("line") : std::string();
            same += again == last;
            last = again;
        }
        expect(same == 0, "the hub's line never the same twice running (" + std::to_string(same) + ")");
        fs::remove_all(root);
    }
}

void windowMantle()
{
    // W1 (doc 84 §13.4.1, Phase 13): a wearer's window talk is their mantle's where it has a line: the hub, the job and the
    // goodbye (the last two once the game's own answers, now the mantle's first); a graft's choice shown on the hub for
    // them alone, its node and its way back; typed words about their quirk taken to it.
    const auto root = fs::temp_directory_path() / ("ratw-dialogue-mantle-" + std::to_string(::getpid()));
    const auto worldPath = writeStrip(root / "world");
    fs::create_directories(root / "mantles" / "plain");
    std::ofstream(root / "mantles" / "plain" / "rm950.mantle") << R"(# rm950 (test): worn by Neighbour u1.
mantle rm950
state ready
dialect plain
accent none
tier standard
written_for u1
title the cobble counter
fits stage=adult|old
traits talk=chatty humour=merry warmth=warm
quirk counts_the_cobbles
marks oath

words
oath! = Cobbles and crumbs | By my tail

dlg resident.hub : Four hundred and twelve cobbles in this lane, and you're standing on the loose one. What is it?
dlg resident.job : I count the cobbles, and I mind the lane. Somebody has to.
dlg resident.bye : Mind the loose one on your way out.

graft resident.hub
choice Up counting by moonlight? [time=night] -> loose
choice What are you counting? -> counting
node counting
say Cobbles. Four hundred and twelve, last I looked. One's loose.
mood pleased
invite the_lane dawn 2
free
choice Which one's loose? -> loose
choice Goodbye. -> bye
node loose
say The one you're on. Step careful.
choice Back to the rest. -> hub
)";
    Town town(worldPath, (root / "save").string(), 120, true, 3 * 86400, "", (root / "mantles").string());
    auto& w = town.g.world();
    expect(town.g.mantleWearers().count("u1") && town.g.mantleWearers().at("u1") == "rm950", "u1 wears rm950");
    town.enter(town.ash, 1, "player-ash", "Ash", 1, 9.5);
    w.setTimeOfDay(12);
    town.run(2);
    const auto keep = [&](const char* who) {
        auto* n = w.entity(who);
        n->cellId = cellId(1);
        n->position = {10.5, 9.5};
        w.stop(who);
        town.place(town.ash, 1, 9.5);
    };
    const auto open = [&](const char* npc) -> const json::Value* {
        if (const auto* d = town.g.dialogueOf(town.ash.entityId))
            town.g.command(&town.ash, cmd({{"type", "dialogue_close"}, {"session", d->id}, {"why", "closed"}}));
        keep(npc);
        town.run(1.1);
        town.ash.events.clear();
        town.g.command(&town.ash, cmd({{"type", "dialogue_open"}, {"npc", npc}, {"entry", "talk"}}));
        town.g.settle();
        return lastOf(town.ash, "dialogue");
    };
    const auto pick = [&](const json::Value* d, const std::string& starts) {
        if (d)
            for (const auto& c : d->array("choices"))
                if (c.string("text").rfind(starts, 0) == 0)
                    return int(c.number("n"));
        return 0;
    };
    const auto choose = [&](int n) -> const json::Value* {
        const auto* d = town.g.dialogueOf(town.ash.entityId);
        town.ash.events.clear();
        if (d)
            town.g.command(&town.ash, cmd({{"type", "dialogue_choose"}, {"session", d->id}, {"n", n}}));
        town.g.settle();
        return lastOf(town.ash, "dialogue");
    };
    const auto show = [&](const json::Value* d) { return d ? d->string("node") + ": " + d->string("line") : std::string("none"); };
    auto* d = open("u1");
    expect(d && contains(d->string("line"), "Four hundred and twelve cobbles"), "the hub's line is the wearer's: " + show(d));
    expect(pick(d, "What are you counting?") > 0 && d->array("choices").size() <= 9, "the graft's choice on their hub: " + show(d));
    d = choose(pick(d, "What are you counting?"));
    expect(d && contains(d->string("line"), "One's loose") && d->string("mood") == "pleased", "the graft's node, its mood: " + show(d));
    d = choose(pick(d, "Which one's loose?"));
    expect(d && contains(d->string("line"), "The one you're on"), "and on: " + show(d));
    d = choose(pick(d, "Back to the rest."));
    expect(d && d->string("node") == "hub", "back to the tree's hub: " + show(d));
    // (The graft node's invitation stands once between the two, however often the node is visited: stage 4's review.)
    for (int i = 0; i < 3; ++i)
    {
        d = choose(pick(d, "What are you counting?"));
        d = choose(pick(d, "Which one's loose?"));
        d = choose(pick(d, "Back to the rest."));
    }
    const auto invites = std::count_if(w.talkLedger().invites().begin(), w.talkLedger().invites().end(),
                                       [&](const auto& i) { return i.by == "u1" && i.to == town.ash.entityId && i.state == "open"; });
    expect(invites == 1, "one invitation standing, not one a visit: " + std::to_string(invites));
    d = choose(pick(d, "I want to know more about something..."));
    d = choose(pick(d, "Tell me about yourself."));
    expect(d && contains(d->string("line"), "I count the cobbles"), "the job's line the wearer's, before the game's answer: " + show(d));
    town.ash.events.clear();
    town.g.command(&town.ash, cmd({{"type", "dialogue_close"}, {"session", town.g.dialogueOf(town.ash.entityId)->id}, {"why", "bye"}}));
    town.g.settle();
    expect(std::any_of(town.ash.events.begin(), town.ash.events.end(), [](const json::Value& e) { return contains(e.string("text"), "Mind the loose one"); }),
           "the goodbye the wearer's");
    // Stage 4's review: the hub is rebuilt at every choice, a graft shown or not by the hour, so the choices move; the one
    // chosen is the one taken (never its neighbour, never past the end of the list).
    d = open("u1");
    const int byeAtNoon = pick(d, "Goodbye for now.");
    expect(byeAtNoon > 0 && pick(d, "Up counting by moonlight?") == 0, "no moonlight choice at noon: " + show(d));
    w.setTimeOfDay(23);
    d = choose(byeAtNoon);
    expect(!d || d->string("node") == "bye" || town.g.dialogueOf(town.ash.entityId) == nullptr,
           "Goodbye chosen at noon is Goodbye at night, the moonlight graft now before it: " + show(d));
    d = open("u1");
    const int byeAtNight = pick(d, "Goodbye for now.");
    expect(pick(d, "Up counting by moonlight?") > 0 && byeAtNight > byeAtNoon, "the moonlight choice at night: " + show(d));
    w.setTimeOfDay(12);
    d = choose(byeAtNight);
    expect(!d || d->string("node") == "bye" || town.g.dialogueOf(town.ash.entityId) == nullptr,
           "Goodbye chosen at night is Goodbye at noon (it stood past the end of noon's list): " + show(d));
    d = open("u1");
    d = choose(pick(d, "I want to know more about something..."));
    w.setTimeOfDay(23);
    d = open("u2");
    expect(d && pick(d, "What are you counting?") == 0 && !contains(d->string("line"), "cobbles"), "nobody else's: " + show(d));
    w.setTimeOfDay(12);
    // Typed words about the quirk: the graft.
    d = open("u1");
    town.run(1.1);
    town.ash.events.clear();
    town.g.command(&town.ash, cmd({{"type", "dialogue_say"}, {"session", town.g.dialogueOf(town.ash.entityId)->id}, {"text", "Do you really count the cobbles?"}}));
    town.g.settle();
    d = lastOf(town.ash, "dialogue");
    expect(d && contains(d->string("line"), "One's loose"), "asked of their quirk in their own words: the graft: " + show(d));
    fs::remove_all(root);
}
// A world story's quests in the window (doc 85, Phase 6): a resident offers a quest to the wolves its join rules let take
// it (not to one outside the story when only its wolves may; not when full; not to one holding it; only in its region
// when it has one), in the story's words said in the resident's voice; accepting, declining, handing in and a story
// choice each write the event the story's runner reads.
void storyQuestsInTheWindow()
{
    const auto root = fs::temp_directory_path() / ("ratw-dialogue-quests-" + std::to_string(::getpid()));
    const auto worldPath = writeStrip(root / "world");
    Town town(worldPath, (root / "save").string(), 120, true);
    auto& w = town.g.world();
    town.enter(town.ash, 1, "player-ash", "Ash", 1, 9.5);
    town.enter(town.cy, 3, "player-cy", "Cy", 1, 12.5);
    w.setTimeOfDay(12);
    town.run(2);
    const auto keep = [&] {
        auto* n = w.entity("u1");
        n->cellId = cellId(1);
        n->position = {10.5, 9.5};
        w.stop("u1");
        town.place(town.ash, 1, 9.5);
    };
    keep();
    std::vector<WorldEvent> seen;
    w.setEventWatcher([&](const WorldEvent& e) { if (e.kind.rfind("quest ", 0) == 0 || e.kind == "dialogue choice") seen.push_back(e); });
    const auto run = "world-ford";
    auto phase = parsed(R"({"title":"The ford","phase":"open","journal":"The carter is stuck.","visit":1,
        "regions":[{"id":"bank","cell":"c_1_0","x1":12,"y1":8,"x2":13,"y2":9}]})");
    phase.set("players", parsed("[\"" + town.ash.entityId + "\"]"));
    expect(town.g.applyDmAct({"1", "story.phase", run, "dm", json::dump(phase)}).ok, "the story opens with Ash");
    const auto offer = [](const std::string& quest, const std::string& ask, const std::string& extra) {
        return R"({"quest":")" + quest + R"(","npc":"u1","title":"Title of )" + quest + R"(","say":"My axle's split, #address#.","ask":")" + ask +
               R"(","accept":"I'll find you one.","decline":"Not now.","handIn":"Here's your axle.","thanks":"Moving again, then.")" + extra + "}";
    };
    const auto tell = [&](const std::string& views) {
        const auto text = R"({"visit":1,"offers":[)" + offer("axle", "Is there work going?", R"(,"who":"anyone","full":false)") + "," +
                          offer("secret", "Any secrets?", R"(,"who":"participants","full":false)") + "," +
                          offer("crowded", "Room for one more?", R"(,"who":"anyone","full":true)") + "," +
                          offer("bank", "Down by the bank?", R"(,"who":"anyone","full":false,"region":"bank")") +
                          R"(],"choices":[{"id":"rumour","npc":"u1","words":"Heard about the axle?","reply":"Someone cut it.","players":null,"once":true,"picked":[]}],
                             "wolves":{")" + town.ash.entityId + R"(":{"quests":[)" + views + R"(],"trackers":[]}}})";
        expect(town.g.applyDmAct({"2", "story.quests", run, "dm", text}).ok, "the quests are told");
    };
    tell("");
    const auto ids = [&](const std::string& wolf) {
        std::string out;
        for (const auto& c : town.g.storyChoicesFor(wolf, "u1"))
            out += c.kind + ":" + c.id + " ";
        return out;
    };
    expect(ids(town.ash.entityId) == "offer:axle offer:secret choice:rumour ", "Ash, of the story: " + ids(town.ash.entityId));
    expect(ids(town.cy.entityId) == "offer:axle offer:bank ", "Cy, outside it, standing by the bank: " + ids(town.cy.entityId));
    town.place(town.cy, 1, 5.5);
    expect(ids(town.cy.entityId) == "offer:axle ", "away from the bank: " + ids(town.cy.entityId));
    tell(R"({"id":"axle","title":"Axle","status":"active","giver":"u1","objectives":[],"handIn":false,"abandon":true,"accept":false})");
    expect(ids(town.ash.entityId) == "offer:secret choice:rumour ", "holding it: not offered again: " + ids(town.ash.entityId));
    tell(R"({"id":"axle","title":"Axle","status":"done","again":null,"objectives":[],"handIn":false,"abandon":false,"accept":false})");
    expect(ids(town.ash.entityId) == "offer:secret choice:rumour ", "done for good: " + ids(town.ash.entityId));
    tell(R"({"id":"axle","title":"Axle","status":"done","again":0,"objectives":[],"handIn":false,"abandon":false,"accept":false})");
    expect(ids(town.ash.entityId) == "offer:axle offer:secret choice:rumour ", "may be taken again now: " + ids(town.ash.entityId));
    tell("");

    const auto open = [&]() -> const json::Value* {
        if (const auto* d = town.g.dialogueOf(town.ash.entityId))
            town.g.command(&town.ash, cmd({{"type", "dialogue_close"}, {"session", d->id}, {"why", "closed"}}));
        keep();
        town.ash.events.clear();
        town.g.command(&town.ash, cmd({{"type", "dialogue_open"}, {"npc", "u1"}, {"entry", "talk"}}));
        return lastOf(town.ash, "dialogue");
    };
    const auto pick = [&](const json::Value* d, const std::string& words) {
        if (d)
            for (const auto& c : d->array("choices"))
                if (c.string("text") == words)
                    return int(c.number("n"));
        return 0;
    };
    const auto texts = [](const json::Value* d) {
        std::string all;
        if (d)
            for (const auto& c : d->array("choices")) all += "[" + c.string("text") + "]";
        return all;
    };
    const auto choose = [&](int n) -> const json::Value* {
        const auto* d = town.g.dialogueOf(town.ash.entityId);
        expect(d && n > 0, "a choice to make");
        town.ash.events.clear();
        seen.clear();
        auto o = json::Value::object();
        o.add("type", "dialogue_choose");
        o.add("session", d->id);
        o.add("n", n);
        town.g.command(&town.ash, json::dump(o));
        return lastOf(town.ash, "dialogue");
    };
    // The offer: asked, said in the resident's voice, accepted.
    auto* d = open();
    expect(d && d->string("node") == "hub" && pick(d, "Is there work going?") && pick(d, "Any secrets?") && pick(d, "Heard about the axle?") &&
           !pick(d, "Room for one more?") && !pick(d, "Down by the bank?"), "the hub offers what Ash may take: " + texts(d));
    const auto hubChoices = d->array("choices").size();
    expect(hubChoices <= 9 && d->array("choices").back().string("text") == "Goodbye for now.", "before the way out, nine at most");
    d = choose(pick(d, "Is there work going?"));
    expect(d && d->string("line").find("My axle's split") == 0 && d->string("line").find('#') == std::string::npos &&
           pick(d, "I'll find you one.") && pick(d, "Not now."), "the story's words, the symbols said: " + (d ? d->string("line") : std::string()) + texts(d));
    d = choose(pick(d, "I'll find you one."));
    expect(seen.size() == 1 && seen[0].kind == "quest accepted" && seen[0].actor == town.ash.entityId && seen[0].target == run &&
           seen[0].item == "u1" && seen[0].detail == "axle", "accepted: the event the runner reads");
    expect(d && d->string("node") == "hub" && d->string("note") == "You take on: Title of axle.", "back at the hub, told");
    // Declined.
    d = choose(pick(d, "Any secrets?"));
    d = choose(pick(d, "Not now."));
    expect(seen.size() == 1 && seen[0].kind == "quest declined" && seen[0].detail == "secret", "declined");
    // The offer gone between asking and answering: no event.
    d = choose(pick(d, "Is there work going?"));
    tell(R"({"id":"axle","title":"Axle","status":"active","giver":"u1","objectives":[],"handIn":false,"abandon":true,"accept":false})");
    d = choose(pick(d, "I'll find you one."));
    expect(seen.empty(), "held by now: accepting again writes nothing");
    // Handing it in, when it is ready.
    tell(R"({"id":"axle","title":"Axle","status":"ready","giver":"u1","objectives":[],"handIn":true,"abandon":true,"accept":false})");
    d = open();
    expect(pick(d, "Here's your axle.") && !pick(d, "Is there work going?"), "ready: the hand-in, and no offer: " + texts(d));
    d = choose(pick(d, "Here's your axle."));
    expect(seen.size() == 1 && seen[0].kind == "quest handed in" && seen[0].item == "u1" && seen[0].detail == "axle", "handed in");
    expect(d && d->string("line") == "Moving again, then." && pick(d, "Something else..."), "the resident's thanks: " + (d ? d->string("line") : std::string()));
    // A story choice, once.
    d = choose(pick(d, "Something else..."));
    d = choose(pick(d, "Heard about the axle?"));
    expect(seen.size() == 1 && seen[0].kind == "dialogue choice" && seen[0].detail == "rumour" && seen[0].item == "u1", "the story choice");
    expect(d && d->string("line") == "Someone cut it.", "its reply");
    d = open();
    expect(d && !pick(d, "Heard about the axle?"), "once: not shown again: " + texts(d));
    fs::remove_all(root);
}

// Doc 86, Phase 1b: a story's choice for a list of wolves (J1); what the window knows of the wolf's own part in a story,
// player.quest.<program>/<quest> and player.story.<name> (N2); a world.mem and a pair.mem line a story's memory plays (C8).
// The library is Data/Talk with five lines of the test's own in the resident's hub.
void storyTalkInTheWindow()
{
    const auto root = fs::temp_directory_path() / ("ratw-dialogue-story-talk-" + std::to_string(::getpid()));
    fs::remove_all(root);
    const auto worldPath = writeStrip(root / "world");
    const auto talk = root / "talk";
    fs::copy(RATW_SOURCE_DIR "/Data/Talk", talk, fs::copy_options::recursive);
    {
        std::ifstream in(talk / "dialogue" / "resident.tree");
        std::stringstream text;
        text << in.rdbuf();
        auto tree = text.str();
        const auto at = tree.find("say[player.asked=person");
        expect(at != std::string::npos, "the resident's hub has its ledger lines");
        const auto end = tree.find('\n', at) + 1;
        tree.insert(end, "say[world.mem.nh_stage=reckoning priority=11] The reckoning's close, they say.\n"
                         "say[pair.mem.nh_stone=brought priority=12] You're the one who brought my mother's stone.\n"
                         "say[player.quest.ninth-howl.1/where_king=active priority=13] Still looking for the King, then?\n"
                         "say[player.story.p_howl=coldwater priority=14] You pledged to Coldwater, didn't you?\n"
                         "say[self.mem.nh_willa=met priority=15] I met Willa at the well.\n");
        std::ofstream(talk / "dialogue" / "resident.tree") << tree;
    }
    {
        talk::Library lib;
        std::string problem;
        expect(lib.load(talk.string(), problem), "the library with the story's lines loads: " + problem);
    }
    Town town(worldPath, (root / "save").string(), 120, true, 3 * 86400, "", "", talk.string());
    auto& w = town.g.world();
    town.enter(town.ash, 1, "player-ash", "Ash", 1, 9.5);
    town.enter(town.bo, 2, "player-bo", "Bo", 1, 9.5);
    town.enter(town.cy, 3, "player-cy", "Cy", 1, 12.5);
    w.setTimeOfDay(12);
    town.run(2);
    const auto run = std::string("world-ninth-howl");
    auto phase = parsed(R"({"title":"The Ninth Howl","phase":"one","journal":"A howl in the hills.","visit":1,"program":"ninth-howl.1"})");
    phase.set("players", parsed("[\"" + town.ash.entityId + "\",\"" + town.bo.entityId + "\"]"));
    expect(town.g.applyDmAct({"1", "story.phase", run, "dm", json::dump(phase)}).ok, "the story opens with Ash and Bo");
    int n = 2;
    const auto order = [&](const std::string& kind, const std::string& payload) {
        auto p = parsed(payload);
        p.set("run", run);
        p.set("visit", 1);
        return town.g.applyDmAct({std::to_string(++n), kind, run, "dm", json::dump(p)});
    };
    const auto quests = [&](const std::string& body) { return town.g.applyDmAct({std::to_string(++n), "story.quests", run, "dm", body}); };
    // ---- J1: one choice for two wolves, one of whom has picked it; one for Ash alone.
    expect(quests(R"({"visit":1,"program":"ninth-howl.1","choices":[
        {"id":"pledge","npc":"u1","words":"I'll stand with Coldwater.","reply":"Then stand.","players":[")" + town.ash.entityId + R"(",")" + town.bo.entityId +
                  R"("],"once":true,"picked":[")" + town.ash.entityId + R"("]},
        {"id":"ask_king","npc":"u1","words":"Where is the King?","reply":"Nobody knows.","players":[")" + town.ash.entityId + R"("],"once":false,"picked":[]}]})").ok,
           "the choices are told");
    const auto ids = [&](const std::string& wolf) {
        std::string out;
        for (const auto& c : town.g.storyChoicesFor(wolf, "u1"))
            out += c.kind + ":" + c.id + " ";
        return out;
    };
    expect(ids(town.ash.entityId) == "choice:ask_king ", "Ash picked the pledge; the King's question is his: " + ids(town.ash.entityId));
    expect(ids(town.bo.entityId) == "choice:pledge ", "Bo sees the pledge, not Ash's question: " + ids(town.bo.entityId));
    expect(ids(town.cy.entityId).empty(), "Cy, outside the story, neither");
    // ---- the window's lines.
    const auto line = [&](Client& c) {
        if (const auto* d = town.g.dialogueOf(c.entityId))
            town.g.command(&c, cmd({{"type", "dialogue_close"}, {"session", d->id}, {"why", "closed"}}));
        auto* u = w.entity("u1");
        u->cellId = cellId(1);
        u->position = {10.5, 9.5};
        w.stop("u1");
        town.place(c, 1, 9.5);
        c.events.clear();
        town.g.command(&c, cmd({{"type", "dialogue_open"}, {"npc", "u1"}, {"entry", "talk"}}));
        const auto* d = lastOf(c, "dialogue");
        const auto said = d ? d->string("line") : std::string();
        if (const auto* open = town.g.dialogueOf(c.entityId))
            town.g.command(&c, cmd({{"type", "dialogue_close"}, {"session", open->id}, {"why", "closed"}}));
        return said;
    };
    expect(line(town.ash).find("reckoning") == std::string::npos, "nothing remembered yet: " + line(town.ash));
    expect(order("talk.remember", R"({"scope":"realm","key":"nh_stage","value":"reckoning","days":30})").ok, "the realm remembers the stage");
    expect(line(town.ash) == "The reckoning's close, they say.", "a world.mem line plays: " + line(town.ash));
    expect(order("talk.remember", R"({"scope":"pair","npc":"u1","player":")" + town.ash.entityId + R"(","key":"nh_stone","value":"brought","days":60})").ok,
           "u1 remembers Ash brought the stone");
    expect(line(town.ash) == "You're the one who brought my mother's stone.", "a pair.mem line plays for Ash: " + line(town.ash));
    expect(line(town.bo) == "The reckoning's close, they say.", "and not for Bo at the same resident: " + line(town.bo));
    // ---- N2: the wolf's quest, and a value told to residents.
    expect(quests(R"({"visit":1,"program":"ninth-howl.1","wolves":{")" + town.ash.entityId +
                  R"(":{"quests":[{"id":"where_king","title":"Where is the King?","status":"active","objectives":[]}],"trackers":[]}}})").ok,
           "Ash's journal is told");
    expect(line(town.ash) == "Still looking for the King, then?", "player.quest plays for Ash: " + line(town.ash));
    expect(line(town.bo) == "The reckoning's close, they say.", "not for Bo");
    expect(quests(R"({"visit":1,"wolves":{")" + town.ash.entityId +
                  R"(":{"quests":[{"id":"where_king","title":"Where is the King?","status":"done","objectives":[]}],"trackers":[],"talk":{"p_howl":"coldwater"}}}})").ok,
           "Ash pledged to Coldwater, and found the King");
    expect(line(town.ash) == "You pledged to Coldwater, didn't you?", "player.story plays: " + line(town.ash));
    expect(quests(R"({"visit":1,"wolves":{")" + town.ash.entityId + R"(":{"quests":[],"trackers":[],"talk":{}}}})").ok, "Ash's view cleared");
    expect(line(town.ash) == "You're the one who brought my mother's stone.", "and the line goes with it: " + line(town.ash));
    // ---- C8: a resident's own memory, as self.mem.
    expect(order("talk.remember", R"({"scope":"npc","npc":"u1","key":"nh_willa","value":"met","days":10})").ok, "u1 remembers Willa");
    expect(line(town.bo) == "I met Willa at the well.", "a self.mem line plays: " + line(town.bo));
    fs::remove_all(root);
}

// Doc 86, Phase 3: a story's greeting for one wolf (C5), said once in the resident's own dialect instead of the usual
// one, gone once lapsed, replaced by a later one; and a phase's npc.line said in the speaker's voice too (T5).
void storyGreetingsInTheWindow()
{
    const auto root = fs::temp_directory_path() / ("ratw-dialogue-story-greet-" + std::to_string(::getpid()));
    fs::remove_all(root);
    const auto worldPath = writeStrip(root / "world");
    const auto talk = root / "talk";
    fs::copy(RATW_SOURCE_DIR "/Data/Talk", talk, fs::copy_options::recursive);
    std::ofstream(talk / "grammar" / "plain.grammar", std::ios::app) << "\nnh_hello = Good day to you.\n";
    std::ofstream(talk / "grammar" / "larkfield.grammar", std::ios::app) << "\nnh_hello = Fair day and fair trade to you.\n";
    {
        std::ifstream in(talk / "dialogue" / "resident.tree");
        std::stringstream text;
        text << in.rdbuf();
        auto tree = text.str();
        const auto at = tree.find("say[player.asked=person");
        expect(at != std::string::npos, "the resident's hub has its ledger lines");
        tree.insert(tree.find('\n', at) + 1, "say[player.titled=yes priority=16] They call you #player.title# now, don't they?\n");
        std::ofstream(talk / "dialogue" / "resident.tree") << tree;
    }
    Town town(worldPath, (root / "save").string(), 120, true, 3 * 86400, "", "", talk.string());
    auto& w = town.g.world();
    town.enter(town.ash, 1, "player-ash", "Ash", 1, 9.5);
    town.enter(town.bo, 2, "player-bo", "Bo", 1, 9.5);
    w.setTimeOfDay(12);
    w.setTalkOrigins({{"u1", {"larkfield", "broad"}}, {"u2", {"", "none"}}});
    town.run(2);
    const auto run = std::string("world-greetings");
    auto phase = parsed(R"({"title":"Greetings","phase":"one","journal":"Thanks.","visit":1,"program":"lamp-pack",
                            "lines":[{"npc":"u2","text":"#nh_hello# The lamps are lit again."}]})");
    phase.set("players", parsed("[\"" + town.ash.entityId + "\",\"" + town.bo.entityId + "\"]"));
    expect(town.g.applyDmAct({"1", "story.phase", run, "dm", json::dump(phase)}).ok, "the story opens with Ash and Bo");
    int n = 2;
    const auto order = [&](const std::string& kind, const std::string& target, const std::string& payload) {
        auto p = parsed(payload);
        p.set("run", run);
        p.set("visit", 1);
        return town.g.applyDmAct({std::to_string(++n), kind, target, "dm", json::dump(p)});
    };
    const auto line = [&](Client& c, const std::string& npc, const std::string& entry = "talk") {
        if (const auto* d = town.g.dialogueOf(c.entityId))
            town.g.command(&c, cmd({{"type", "dialogue_close"}, {"session", d->id}, {"why", "closed"}}));
        town.run(3.5);                              // (Past the window's quiet reopening: each opening a fresh greeting.)
        auto* u = w.entity(npc);
        u->cellId = cellId(1);
        u->position = {10.5, 9.5};
        w.stop(npc);
        town.place(c, 1, 9.5);
        c.events.clear();
        town.g.command(&c, cmd({{"type", "dialogue_open"}, {"npc", npc}, {"entry", entry}}));
        const auto* d = lastOf(c, "dialogue");
        const auto said = d ? d->string("line") : std::string();
        if (const auto* open = town.g.dialogueOf(c.entityId))
            town.g.command(&c, cmd({{"type", "dialogue_close"}, {"session", open->id}, {"why", "closed"}}));
        return said;
    };
    const auto usual = line(town.ash, "u1");
    expect(!usual.empty() && !contains(usual, "oil"), "u1's usual greeting: " + usual);
    expect(order("npc.greet", "u1", R"({"player":")" + town.ash.entityId + R"(","text":"#nh_hello# Thank you for the oil.","days":2})").ok,
           "u1 will thank Ash");
    expect(line(town.bo, "u1").find("oil") == std::string::npos, "not Bo");
    // (Only the talk window's opening line: another entry's own line is kept, and the greeting waits for Talk.)
    auto said = line(town.ash, "u1", "introduce");
    expect(!said.empty() && !contains(said, "oil"), "Introduce answers as ever: " + said);
    said = line(town.ash, "u1");
    expect(said == "Fair day and fair trade to you. Thank you for the oil.", "once, in u1's Larkfield words, instead of the usual: " + said);
    said = line(town.ash, "u1");
    expect(!contains(said, "oil"), "and only once: " + said);
    // A later greeting replaces one waiting; one that has lapsed is never said.
    expect(order("npc.greet", "u1", R"({"player":")" + town.ash.entityId + R"(","text":"The first words.","days":5})").ok &&
               order("npc.greet", "u1", R"({"player":")" + town.ash.entityId + R"(","text":"The second words.","days":5})").ok, "two greetings");
    said = line(town.ash, "u1");
    expect(said == "The second words.", "the later one: " + said);
    expect(order("npc.greet", "u1", R"({"player":")" + town.ash.entityId + R"(","text":"Too late now.","days":1})").ok, "a greeting for a day");
    w.advanceCalendar(1.5);
    said = line(town.ash, "u1");
    expect(!contains(said, "Too late"), "lapsed after its days: " + said);
    // C4: the title residents use, in a line gated on it, where it is known.
    expect(order("title.give", town.ash.entityId, R"({"title":"the Lamp of the Ford"})").ok, "Ash is named");
    said = line(town.ash, "u1");
    expect(said == "They call you the Lamp of the Ford now, don't they?", "u1 uses the title: " + said);
    expect(!contains(line(town.bo, "u1"), "Lamp"), "not for Bo");
    // T5: the phase's line for its wolves, said in u2's voice (plain: no accent), its #symbols# filled.
    w.setTimeOfDay(12);
    town.run(2);
    said = line(town.ash, "u2");
    expect(contains(said, "Good day to you. The lamps are lit again.") && !contains(said, "#"), "npc.line in the speaker's voice: " + said);
    fs::remove_all(root);
}

} // namespace

int main()
{
    try
    {
        loading();
        choicesAndLines();
        ledger();
        inAGame();
        giversWork();
        roleTrees();
        otherRoles();
        windowPromises();
        windowTalk();
        windowMantle();
        storyQuestsInTheWindow();
        storyTalkInTheWindow();
        storyGreetingsInTheWindow();
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAILED after " << checks << " checks: " << e.what() << "\n";
        return 1;
    }
    std::cout << "dialogue tests passed (" << checks << " checks)\n";
    return 0;
}
