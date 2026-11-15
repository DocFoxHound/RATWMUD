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
#include <cmath>
#include <filesystem>
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
    expect(texts == std::vector<std::string>{"Let me see your wares.", "What's dear today?", "Goodbye."},
           "an open shop with no work: wares, prices, goodbye (work hidden)");
    expect(shown[0].n == 1 && shown[2].n == 3, "numbered from one");
    q.facts["here.work"] = "2";
    q.facts["here.giver"] = "yes";
    shown = lib.choices(greet, q, 1, none);
    expect(shown.size() == 4 && shown[1].text == "Any work going?", "with work going, four");
    q.facts["here.shop"] = "asleep";
    shown = lib.choices(greet, q, 1, none);
    expect(shown.size() == 1 && shown[0].text == "Goodbye.", "a sleeping keeper: only goodbye");
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
    expect(shown.size() == 5 && shown[0].text == "Ask about: Carry a letter 0 (12p)" && shown[2].bound.at("topic.job_id") == "k2" &&
               shown[4].text == "Goodbye.",
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
    const auto choices = d->array("choices");
    expect(!choices.empty() && choices.size() <= 4 && choices.front().string("text") == "Let me see your wares." &&
               choices.back().string("text") == "Goodbye.",
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
    expect(d && d->array("choices").size() == 1 && d->array("choices").front().string("text") == "Goodbye.",
           "a sleeping keeper: words, and only goodbye: " + (d ? json::dump(*d) : std::string()));
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
               pick(d, "Goodbye.") == 1,
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
    expect(d && d->string("tree") == "work" && d->string("node") == "list" && pick(d, "Ask about: a letter for Neighbour u3 (5p)") > 0 &&
               pick(d, "Ask about: Clear the bandits") > 0 && pick(d, "Goodbye.") > 0,
           "the keeper's work, each job in plain words with its pay: " + texts(d));
    // The details: where and what, then the pay and the time, then the risks.
    d = choose(pick(d, "Ask about: a letter"));
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
    expect(lockedWhy(d, "Ask about: a letter for Neighbour u1") == "That's promised to another for now.",
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
    expect(pick(d, "About the work I took on.") > 0, "the giver hears about work taken: " + texts(d));
    d = choose(pick(d, "About the work I took on."));
    expect(d && d->string("node") == "mine" && pick(d, "About: a letter for Neighbour u3") > 0, "the work taken, listed: " + texts(d));
    d = choose(pick(d, "About: a letter"));
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
    expect(d && d->string("tree") == "official" && contains(d->string("line"), "Chapters") && pick(d, "Ask about:") > 0 &&
               !lockedWhy(d, "Ask about:").empty(),
           "a wolf of no Chapter: the board shown, every mission greyed: " + texts(d) + " / " + (d ? d->string("line") : std::string()));
    auto& ch = town.g.chapters();
    ch.propose(town.ash.entityId, {town.bo.entityId, town.cy.entityId}, "Ashen Lodge", "#5b8bd9", "", "scene-x", 0);
    ch.agree(town.bo.entityId, 0);
    const auto lodge = ch.agree(town.cy.entityId, 0).message;
    expect(ch.of(town.ash.entityId), "the Lodge is founded");
    d = open("u1", "missions");
    expect(d && lockedWhy(d, "Ask about:") == "For a Lodge the faction knows well.", "a new Lodge: locked by standing, why: " + texts(d));
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
    const int ours = pick(d, "Ask about: Bring a meal to");
    expect(ours > 0 && lockedWhy(d, "Ask about: Bring a meal to").empty(), "known well: tier one open: " + texts(d));
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
    d = choose(pick(d, "About what I took on."));
    expect(d && d->string("tree") == "work" && d->string("node") == "mine", "about what was taken: " + texts(d));
    d = choose(pick(d, "About:"));
    d = choose(pick(d, "I can't do it after all."));
    expect(d && d->string("node") == "given" && f.mission(another)->state == "open" && f.mission(another)->taker.empty(),
           "a mission given back: on the board again: " + (d ? json::dump(*d) : std::string()));
    // The Chapter's standing, asked in the window.
    d = open("u1", "missions");
    d = choose(pick(d, "About our Chapter's standing"));
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
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAILED after " << checks << " checks: " << e.what() << "\n";
        return 1;
    }
    std::cout << "dialogue tests passed (" << checks << " checks)\n";
    return 0;
}
