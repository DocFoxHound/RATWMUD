// Residents' troubles (Docs/Design/57-changing-the-world.md, 3). Phase 1, seen: each of the five kinds found from the
// simulation's own state, none for an untroubled resident; the order when two hold; kept a game day; spoken of only past
// the trust rule, by the Mind's briefing and the game's own answer to "is something troubling you?"; a wolf who heard
// one has it in its unfinished business until it is gone; across a restart. Phase 2, solved: a loan paid off, a short
// household helped (by coin, and by a gift), a feud ended, a resident spoken for, a youth's apprenticeship sponsored;
// each changes the simulation and the bonds, makes a deed and its rumour, and keeps its limits across a restart.
#include "RatwGame.h"
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

// The first roleplay line `c` hears within a few seconds that `fits`.
std::string replyFits(Town& t, Client& c, const std::function<bool(const std::string&)>& fits)
{
    for (int i = 0; i < 80; ++i)
    {
        t.run(.5);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));   // (Replies come from the dialogue thread.)
        for (const auto& e : c.events)
            if (e.string("type") == "roleplay" && fits(e.string("text")))
                return e.string("text");
    }
    return {};
}

void alter(Society& s, const std::function<void(SocietyState&)>& change)
{
    auto st = s.state();
    change(st);
    expect(s.restore(st), "the altered society restores");
}

void seen()
{
    const auto root = fs::temp_directory_path() / ("ratw-troubles-" + std::to_string(::getpid()));
    const auto save = (fs::temp_directory_path() / ("ratw-troubles-" + std::to_string(::getpid()) + ".json")).string();
    fs::remove(save);
    const auto world = writeStrip(root);
    {
        Town t(world, save, 120, true);
        auto& w = t.g.world();
        auto& s = w.society();
        t.enter(t.ash, 1, "ash", "Ash", 1, 10.5);
        const auto ash = t.ash.entityId;
        for (const auto* who : {"um", "ui", "u1", "u2", "u3"})
            expect(!t.g.troubleNow(who), std::string("untroubled at first: ") + who + " has " + t.g.troubleNow(who).kind);
        const auto put = [&](const char* who, int cell, double x, double y = 8.5) {
            auto* e = w.entity(who);
            e->cellId = cellId(cell);
            e->position = {x, y};
            e->path.clear();
            e->velocity = {};
        };

        // A feud: both ways at -25 or worse.
        w.bonds().change("um", "u2", {-30, 0, 10, 0, 0}, w.calendarDays());
        expect(!t.g.troubleNow("um"), "kept a game day: nothing yet");
        t.g.forgetTroubles();
        expect(!t.g.troubleNow("um"), "one way at -30 isn't a feud");
        w.bonds().change("u2", "um", {-30, 0, 10, 0, 0}, w.calendarDays());
        t.g.forgetTroubles();
        auto trouble = t.g.troubleNow("um");
        expect(trouble.kind == "feud" && trouble.other == "u2", "both ways: a feud with u2, not " + trouble.kind);

        // No steady work, which comes before a feud.
        alter(s, [](SocietyState& st) { st.careers.positions["job:um"].holder.clear(); });
        expect(!s.jobOf("um"), "um's post stands empty");
        t.g.forgetTroubles();
        trouble = t.g.troubleNow("um");
        expect(trouble.kind == "work", "no post: no steady work comes first, not " + trouble.kind);

        // A child with no trade: u2, 15, has neither post nor master; u3 is its parent.
        alter(s, [](SocietyState& st) {
            st.careers.positions["job:u2"].holder.clear();
            st.careers.parents["u2"] = {"u3"};
        });
        w.entity("u2")->age = 15;
        t.g.forgetTroubles();
        trouble = t.g.troubleNow("u3");
        expect(trouble.kind == "child" && trouble.other == "u2" && trouble.age == 15, "u3's child has no trade: " + trouble.kind);
        expect(t.g.troubleNow("u2").kind != "work", "a youth under 16 isn't 'no steady work'");

        // A shop in debt: the inn's till holds a rescue loan.
        const auto till = s.tillOf("ui");
        expect(till.rfind("till:", 0) == 0, "the inn keeps a till of its own: " + till);
        alter(s, [&](SocietyState& st) { st.memory.loans[till] = {40, std::int64_t(w.calendarDays())}; });
        t.g.forgetTroubles();
        trouble = t.g.troubleNow("ui");
        expect(trouble.kind == "debt" && trouble.coins == 40, "the inn owes the town 40p: " + trouble.kind);

        // Short of coin: the head of the household (its eldest) with under a week's food in purses, savings and larder.
        w.entity("u1")->age = 50;
        for (const auto& [id, life] : s.state().residents)
            if (life.homeCell == cellId(0))
                if (const auto* purse = s.account(id); purse && purse->cash > 2)
                    s.shift(id, "treasury", "", 0, purse->cash - 2, "test: spent");
        if (const auto* larder = s.account(Society::homeStore(cellId(0), "larder")))
            for (const auto& [item, n] : std::map<std::string, int>(larder->stock.begin(), larder->stock.end()))
                s.consume(Society::homeStore(cellId(0), "larder"), item, n, "test: eaten");
        t.g.forgetTroubles();
        trouble = t.g.troubleNow("u1");
        expect(trouble.kind == "short" && trouble.days < 7 && trouble.coins > 0 && trouble.household.front() == "u1",
               "the household is short of coin: " + trouble.kind + " " + std::to_string(trouble.days));
        expect(t.g.troubleNow("um").kind == "work", "only the head carries the household's trouble");
        expect(s.conserved(), "nothing minted or lost by looking");

        // Spoken of only to a wolf it trusts.
        expect(t.g.troubleBriefingFor("ui", ash).empty(), "a stranger isn't told");
        w.bonds().change("ui", ash, {10, 15, 25, 0, 0}, w.calendarDays());
        const auto briefing = t.g.troubleBriefingFor("ui", ash);
        expect(contains(briefing, "borrowed 40 pennies") && contains(briefing, "Never ask outright"), "the Mind is told: " + briefing);
        // The game's own answer.
        put("ui", 1, 11.5);
        t.run(.5);
        t.ash.events.clear();
        say(t, t.ash, "Is something troubling you?", "ui");
        auto answer = replyFits(t, t.ash, [](const std::string& text) { return contains(text, "40 pennies"); });
        expect(!answer.empty(), "the innkeeper says what's wrong");
        expect(t.g.troublesHeardBy(ash).count("ui") && t.g.troublesHeardBy(ash).at("ui") == "debt", "she has heard it");
        bool listed = false;
        for (const auto& o : t.selfOf(t.ash)["unfinished"].items())
            listed |= contains(o.string("text"), "shop owes the town 40 pennies");
        expect(listed, "in her unfinished business");
        // Not to a wolf it doesn't trust.
        put("u3", 1, 9.5);
        t.run(.5);
        t.ash.events.clear();
        say(t, t.ash, "What's wrong?", "u3");
        answer = replyFits(t, t.ash, [](const std::string& text) {
            return contains(text, "nothing worth") || contains(text, "Don't trouble") || contains(text, "well enough") || contains(text, "trade");
        });
        expect(!answer.empty() && !contains(answer, "trade"), "a polite nothing to a stranger: " + answer);
        expect(!t.g.troublesHeardBy(ash).count("u3"), "nothing heard");
        t.g.settle();
        t.run(2);
    }
    {
        Town t(world, save, 120, true);
        t.enter(t.ash, 1, "ash", "Ash", 1, 10.5);
        expect(t.g.troublesHeardBy(t.ash.entityId).count("ui"), "what she heard survives a restart");
    }
    fs::remove_all(root);
    fs::remove(save);
}

// The menu entries a client is offered on an entity.
std::vector<std::string> actionsOn(Town& t, Client& c, const std::string& id)
{
    std::vector<std::string> out;
    for (const auto& e : t.selfOf(c).isNull() ? json::Value::array().items() : c.snapshots.back()["entities"].items())
        if (e.string("id") == id)
            for (const auto& a : e["actions"].items())
                out.push_back(a.asString());
    return out;
}
std::string offered(Town& t, Client& c, const std::string& id, const std::string& prefix)
{
    for (const auto& a : actionsOn(t, c, id))
        if (a.rfind(prefix, 0) == 0)
            return a;
    return {};
}
Result act(Town& t, Client& c, const std::string& action, const std::string& target)
{
    c.events.clear();
    t.g.command(&c, cmd({{"type", "action"}, {"action", action}, {"target", target}}));
    t.run(.5);
    const auto said = c.said();
    return {!said.empty() && !contains(said, "Come closer") && !contains(said, "haven't") && !contains(said, "doesn't") &&
                !contains(said, "enough for them") && !contains(said, "know of no") && !contains(said, "first"),
            said, {}};
}
bool deedFor(Town& t, const std::string& wolf, const std::string& kind)
{
    for (const auto* d : t.g.deeds().byDoer(wolf))
        if (d->kind == kind)
            return true;
    return false;
}

void solved()
{
    const auto root = fs::temp_directory_path() / ("ratw-troubles2-" + std::to_string(::getpid()));
    const auto save = (fs::temp_directory_path() / ("ratw-troubles2-" + std::to_string(::getpid()) + ".json")).string();
    fs::remove(save);
    const auto world = writeStrip(root);
    {
        Town t(world, save);
        auto& w = t.g.world();
        auto& s = w.society();
        t.enter(t.ash, 1, "ash", "Ash", 1, 10.5);
        t.enter(t.bo, 2, "bo", "Bo", 0, 10.5);
        const auto ash = t.ash.entityId, bo = t.bo.entityId;
        s.shift("treasury", ash, "", 0, 400, "test purse");
        s.shift("treasury", bo, "", 0, 400, "test purse");
        const auto put = [&](const char* who, int cell, double x, double y = 8.5) {
            auto* e = w.entity(who);
            e->cellId = cellId(cell);
            e->position = {x, y};
            e->path.clear();
            e->velocity = {};
        };
        const auto trusts = [&](const char* who, const std::string& wolf, double trust) {
            w.bonds().change(who, wolf, {10, trust, 30, 0, 0}, w.calendarDays());
        };

        // A shop's loan paid off.
        const auto till = s.tillOf("ui");
        alter(s, [&](SocietyState& st) { st.memory.loans[till] = {40, std::int64_t(w.calendarDays())}; });
        trusts("ui", ash, 15);
        put("ui", 1, 11.5);
        t.g.forgetTroubles();
        t.g.hearTroubleFor(ash, "ui");
        expect(t.g.troublesHeardBy(ash).count("ui"), "she heard of the inn's loan");
        const auto loan = offered(t, t.ash, "ui", "trouble:loan:ui");
        expect(contains(loan, "Pay off their loan (40p)"), "offered: pay off their loan");
        const auto before = s.account(ash)->cash;
        const double liking = w.bonds().find("ui", ash)->affinity;
        auto done = act(t, t.ash, loan, "ui");
        expect(contains(done.message, "owes nothing now"), "the loan paid: " + done.message);
        expect(s.account(ash)->cash == before - 40 && !s.state().memory.loans.count(till), "40p from her purse, and no loan");
        expect(s.conserved(), "money conserved");
        expect(w.bonds().find("ui", ash)->affinity > liking, "the innkeeper warms to her");
        expect(deedFor(t, ash, "paid_debt"), "a deed: paid off the debt");
        expect(!t.g.troublesHeardBy(ash).count("ui"), "out of her unfinished business");
        alter(s, [&](SocietyState& st) { st.memory.loans[till] = {30, std::int64_t(w.calendarDays())}; });
        t.g.forgetTroubles();
        expect(!t.g.troubleNow("ui"), "a solved kind rests a while");

        // A feud ended.
        w.bonds().change("um", "u2", {-30, 0, 10, 0, 0}, w.calendarDays());
        w.bonds().change("u2", "um", {-30, 0, 10, 0, 0}, w.calendarDays());
        trusts("um", ash, 35);
        trusts("u2", ash, 35);
        put("um", 1, 9.5);
        put("u2", 1, 8.5);
        t.place(t.ash, 1, 10.5);
        t.g.forgetTroubles();
        expect(t.g.troubleNow("um").kind == "feud", "um and u2 feud");
        t.g.hearTroubleFor(ash, "um");
        const auto peace = offered(t, t.ash, "um", "trouble:peace:um");
        expect(!peace.empty(), "offered: make peace");
        done = act(t, t.ash, peace, "um");
        expect(contains(done.message, "make their peace"), "peace made: " + done.message);
        expect(w.bonds().find("um", "u2")->affinity > -25 && w.bonds().find("u2", "um")->affinity > -25, "no longer at daggers drawn");
        expect(deedFor(t, ash, "made_peace"), "a deed: made peace");
        t.g.forgetTroubles();
        expect(t.g.troubleNow("um").kind != "feud" && t.g.troubleNow("u2").kind != "feud", "the feud is over for both");

        // A resident spoken for: u3 has no post; u1 works where it stood empty.
        alter(s, [](SocietyState& st) { st.careers.positions["job:u3"].holder.clear(); });
        trusts("u3", ash, 15);
        trusts("u1", ash, 35);
        put("u3", 0, 9.5);
        put("u1", 0, 11.5);
        t.place(t.ash, 0, 10.5);
        t.g.forgetTroubles();
        expect(t.g.troubleNow("u3").kind == "work", "u3 has no steady work");
        t.g.hearTroubleFor(ash, "u3");
        const auto speak = offered(t, t.ash, "u1", "trouble:speak:u3");
        expect(!speak.empty(), "offered on u1: speak for u3");
        done = act(t, t.ash, speak, "u1");
        expect(contains(done.message, "takes your word"), "spoken for: " + done.message);
        expect(s.jobOf("u3") != nullptr, "u3 has a post again");
        expect(deedFor(t, ash, "found_work"), "a deed: found them work");

        // A youth's apprenticeship sponsored: u2, 15, no post; its parent u3; um (40) takes it on.
        alter(s, [](SocietyState& st) {
            st.careers.positions["job:u2"].holder.clear();
            st.careers.parents["u2"] = {"u3"};
        });
        w.entity("u2")->age = 15;
        w.entity("um")->age = 40;
        trusts("u3", bo, 15);
        trusts("um", bo, 25);
        put("u3", 0, 9.5);
        put("um", 0, 11.5);
        t.place(t.bo, 0, 10.5);
        t.g.forgetTroubles();
        expect(t.g.troubleNow("u3").kind == "child", "u3's child has no trade: " + t.g.troubleNow("u3").kind);
        t.g.hearTroubleFor(bo, "u3");
        const auto sponsor = offered(t, t.bo, "um", "trouble:sponsor:u3");
        expect(contains(sponsor, "(20p)"), "offered on um: sponsor the apprenticeship");
        const auto umCash = s.account("um")->cash;
        done = act(t, t.bo, sponsor, "um");
        expect(contains(done.message, "on as an apprentice"), "sponsored: " + done.message);
        expect(s.apprenticedTo("u2") && s.apprenticedTo("u2")->id == "job:um", "u2 learns um's trade");
        expect(s.account("um")->cash == umCash + 20, "the fee is in the master's purse");
        expect(deedFor(t, bo, "sponsored_apprentice"), "a deed: sponsored an apprenticeship");

        // A short household, helped by coin.
        w.entity("u1")->age = 50;
        const auto drain = [&] {
            for (const auto& [id, life] : s.state().residents)
                if (life.homeCell == cellId(0))
                    if (const auto* purse = s.account(id); purse)
                    {
                        if (purse->cash > 2)
                            s.shift(id, "treasury", "", 0, purse->cash - 2, "test: spent");
                        for (const auto& [item, n] : std::map<std::string, int>(purse->stock.begin(), purse->stock.end()))
                            if (Society::edible(item))
                                s.consume(id, item, n, "test: eaten");
                    }
            if (const auto* larder = s.account(Society::homeStore(cellId(0), "larder")))
                for (const auto& [item, n] : std::map<std::string, int>(larder->stock.begin(), larder->stock.end()))
                    s.consume(Society::homeStore(cellId(0), "larder"), item, n, "test: eaten");
        };
        drain();
        trusts("u1", ash, 15);
        put("u1", 0, 11.5);
        t.place(t.ash, 0, 10.5);
        t.g.forgetTroubles();
        const auto short_ = t.g.troubleNow("u1");
        expect(short_.kind == "short", "u1's household is short: " + short_.kind);
        t.g.hearTroubleFor(ash, "u1");
        const auto help = offered(t, t.ash, "u1", "trouble:help:u1");
        expect(contains(help, "Help the household (" + std::to_string(short_.coins) + "p)"), "offered: help the household: " + help);
        done = act(t, t.ash, help, "u1");
        expect(contains(done.message, "food enough for a fortnight"), "helped: " + done.message);
        expect(deedFor(t, ash, "fed_household"), "a deed: kept the household fed");
        // The same wolf can't solve another of u1's troubles this season.
        alter(s, [](SocietyState& st) { st.careers.positions["job:u1"].holder.clear(); });
        t.g.forgetTroubles();
        t.g.hearTroubleFor(ash, "u1");
        expect(offered(t, t.ash, "u1", "trouble:").empty(), "nothing more offered by her this season");

        // A short household, helped by a gift that brings it to a fortnight: Bo gives the head coin.
        alter(s, [](SocietyState& st) { st.careers.positions["job:u1"].holder = "u1"; });
        t.run(1);
        drain();
        t.g.forgetTroubles();
        w.advanceCalendar(15);                     // (The kind rests 14 days after Ash's help.)
        t.run(1);
        drain();
        t.g.forgetTroubles();
        const auto again = t.g.troubleNow("u1");
        expect(again.kind == "short", "short again: " + again.kind);
        t.place(t.bo, 0, 12.5);
        put("u1", 0, 11.5);
        t.run(.5);
        t.bo.events.clear();
        t.g.command(&t.bo, cmd({{"type", "give"}, {"target", "u1"}, {"item", ""}, {"quantity", 0}, {"coins", double(again.coins + 5)}}));
        t.run(1);
        expect(deedFor(t, bo, "fed_household"), "Bo's gift kept the household fed: " + t.bo.said());
        expect(s.conserved(), "money conserved throughout");
        t.g.settle();
        t.run(2);
    }
    {
        Town t(world, save);
        t.enter(t.ash, 1, "ash", "Ash", 1, 10.5);
        auto& w = t.g.world();
        auto& s = w.society();
        const auto till = s.tillOf("ui");
        alter(s, [&](SocietyState& st) { st.memory.loans[till] = {30, std::int64_t(w.calendarDays())}; });
        t.g.forgetTroubles();
        t.g.hearTroubleFor(t.ash.entityId, "ui");
        auto* ui = w.entity("ui");
        ui->cellId = cellId(1), ui->position = {11.5, 8.5}, ui->path.clear();
        expect(offered(t, t.ash, "ui", "trouble:").empty(), "the season's limit survives a restart");
        expect(deedFor(t, t.ash.entityId, "paid_debt"), "and so does her deed");
    }
    fs::remove_all(root);
    fs::remove(save);
}
} // namespace

int main()
{
    try
    {
        seen();
        solved();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "troubles: " << checks << " checks passed\n";
    return 0;
}
