// Mute, block and report (Docs/Design/50-player-card-friends-safety.md, Phase 2): a muted wolf's speech, emotes, local
// and party OOC never arrive, and the muted aren't told; a block follows the player to their other character while the
// list names only the one blocked; blocked wolves can't share a scene, invite or challenge; a report's evidence is the
// lines the reporter received from that wolf, found by a line's number without its author ever being sent; five a day;
// a DM's silence stops speech until it ends; and what the reports store keeps, and for how long.
#include "RatwGame.h"
#include "RatwReports.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace ratw;
namespace
{
int checks = 0;
void expect(bool condition, const std::string& message)
{
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}

json::Value parsed(const std::string& text)
{
    json::Value v;
    std::string error;
    expect(json::parse(text, v, error), "JSON: " + error);
    return v;
}

struct Client final : game::Connection
{
    std::vector<json::Value> events;
    sections::Cache cache;
    void event(const std::string& text) override { events.push_back(parsed(text)); }
    void snapshot(const std::string&) override {}
    void motion(const json::Value&) override {}
    bool allowsLocalCredentials() const override { return true; }
    const json::Value* last(const std::string& type) const
    {
        for (auto it = events.rbegin(); it != events.rend(); ++it)
            if (it->string("type") == type)
                return &*it;
        return nullptr;
    }
    // Every line received of a kind ("roleplay", "ooc") with this text in it.
    bool got(const std::string& type, const std::string& text) const
    {
        for (const auto& e : events)
            if (e.string("type") == type && e.string("text").find(text) != std::string::npos)
                return true;
        return false;
    }
    std::uint64_t sequenceOf(const std::string& text) const
    {
        for (const auto& e : events)
            if ((e.string("type") == "roleplay" || e.string("type") == "ooc") && e.string("text").find(text) != std::string::npos)
                return std::uint64_t(e.number("sequence"));
        return 0;
    }
    bool said(const std::string& text) const
    {
        for (const auto& e : events)
            if (e.string("text").find(text) != std::string::npos)
                return true;
        return false;
    }
};

std::string cmd(std::initializer_list<std::pair<const char*, json::Value>> fields)
{
    auto o = json::Value::object();
    for (const auto& [k, v] : fields)
        o.add(k, v);
    return json::dump(o);
}

json::Value appearance()
{
    return parsed(R"({"species": "timber", "sex": "female", "stature": "average", "pattern": "solid", "baseColor": 3, "gradientColor": 1,
                      "markingColor": 5, "gradientAmount": 0.5, "patternAmount": 0.5})");
}

struct World3
{
    game::Game g;
    Client ada, bo, cy;
    std::string adaId, bo1, bo2, cyId;
    explicit World3(game::Options o) : g(o)
    {
        std::string problem;
        expect(g.start(problem), "starts: " + problem);
        int n = 1;
        for (auto* c : {&ada, &bo, &cy})
        {
            c->id = n++;
            g.connect(c);
        }
        const auto make = [&](Client& c, const char* user, std::initializer_list<const char*> names) {
            g.command(&c, cmd({{"type", "auth_register"}, {"username", user}, {"password", "a long enough password"}}));
            g.settle();
            int i = 0;
            for (const char* name : names)
                g.command(&c, cmd({{"type", "character_create"}, {"name", name}, {"age", 24}, {"appearance", appearance()},
                                   {"commandId", std::string(user) + std::to_string(i++)}}));
            g.settle();
            std::vector<std::string> ids;
            expect(c.last("lobby") != nullptr, std::string("a lobby for ") + user);
            for (const auto& ch : c.last("lobby")->array("characters"))
                ids.push_back(ch.string("id"));
            expect(ids.size() == names.size(), std::string("characters made for ") + user + ": " + c.last("lobby")->string("message"));
            return ids;
        };
        adaId = make(ada, "ada", {"Ada"})[0];
        const auto bos = make(bo, "bob", {"Bo One", "Bo Two"});
        bo1 = bos[0];
        bo2 = bos[1];
        cyId = make(cy, "cyd", {"Cy"})[0];
        enter(ada, adaId);
        enter(bo, bo1);
        enter(cy, cyId);
    }
    void enter(Client& c, const std::string& id)
    {
        g.command(&c, cmd({{"type", "character_enter"}, {"id", id}}));
        g.settle();
        auto* e = g.world().entity(id);
        auto* a = g.world().entity(adaId);
        expect(e != nullptr, "entered: " + id);
        if (a && e != a)
        {
            e->cellId = a->cellId;
            e->position = {a->position.x + 1.2, a->position.y};
        }
        tick(.3);
    }
    void tick(double seconds)
    {
        for (double t = 0; t < seconds; t += .05)
            g.tick(.05);
    }
    void say(Client& c, const std::string& text, const char* channel = "")
    {
        g.command(&c, cmd({{"type", "chat"}, {"text", text}, {"channel", channel}}));
        g.settle();
        tick(1.0);                                  // (Past chat's rate limit before the next line.)
    }
    void safety(Client& c, std::initializer_list<std::pair<const char*, json::Value>> fields)
    {
        auto o = json::Value::object();
        o.add("type", "safety");
        for (const auto& [k, v] : fields)
            o.add(k, v);
        g.command(&c, json::dump(o));
        g.settle();
    }
};

game::Options options()
{
    game::Options o;
    o.hiddenNames = false;
    o.forkSnapshots = false;
    o.oneWolfPerAccount = true;
    o.tiesOptional = true;                      // (Ties: doc 52, tested in newcomer_tests.)
    return o;
}

void muting()
{
    World3 w(options());
    w.say(w.bo, "Hello there, Ada.");
    expect(w.ada.got("roleplay", "Hello there, Ada."), "before: Bo's words reach Ada");
    w.safety(w.ada, {{"verb", "mute"}, {"target", w.bo1}});
    expect(w.ada.last("safety") && w.ada.last("safety")->array("marks").size() == 1, "Ada's list: one mute");
    w.say(w.bo, "Are you ignoring me?");
    w.say(w.bo, "/me waves a paw.");
    w.say(w.bo, "ooc: lag?", "ooc");
    expect(!w.ada.got("roleplay", "ignoring me") && !w.ada.got("roleplay", "waves a paw") && !w.ada.got("ooc", "lag?"),
           "muted: no speech, no emote, no local OOC");
    expect(w.cy.got("roleplay", "ignoring me"), "Cy still hears him");
    expect(w.cy.got("ooc", "lag?"), "and reads his OOC");
    expect(!w.bo.said("muted") && !w.bo.said("Muted"), "and Bo isn't told");
    w.safety(w.ada, {{"verb", "unmute"}, {"target", w.bo1}});
    w.say(w.bo, "Back again.");
    expect(w.ada.got("roleplay", "Back again."), "unmuted, he's heard again");
}

void blocking()
{
    World3 w(options());
    w.safety(w.ada, {{"verb", "block"}, {"target", w.bo1}});
    w.say(w.bo, "Can you hear me?");
    expect(!w.ada.got("roleplay", "Can you hear me?"), "blocked: Bo One is silent to Ada");
    expect(w.g.blocked(w.adaId, w.bo1) && w.g.blocked(w.bo1, w.adaId), "the test other plans call, either way round");
    // Party invites and challenges refused, in the ordinary words.
    w.g.command(&w.bo, cmd({{"type", "party"}, {"verb", "invite"}, {"target", w.adaId}}));
    w.g.settle();
    expect(w.bo.said("They are not here to answer."), "Bo can't invite her: an ordinary refusal");
    w.g.command(&w.bo, cmd({{"type", "action"}, {"action", "challenge"}, {"target", w.adaId}, {"terms", "yield"}}));
    w.g.settle();
    expect(w.bo.said("isn't taking challenges."), "nor challenge her");
    // His other character: blocked too, but her list names only the one she blocked.
    w.g.command(&w.bo, cmd({{"type", "character_leave"}}));
    w.g.settle();
    w.enter(w.bo, w.bo2);
    w.say(w.bo, "It's me, someone new.");
    expect(!w.ada.got("roleplay", "someone new"), "Bo Two is silent to her too");
    expect(w.cy.got("roleplay", "someone new"), "though Cy hears him");
    w.safety(w.ada, {{"verb", "list"}});
    const auto marks = w.ada.last("safety")->array("marks");
    expect(marks.size() == 1 && marks[0].string("character") == w.bo1, "her list names only Bo One");
    // No scene between them: his line never makes her a listener in the ledger.
    w.say(w.ada, "Hello, Cy, how are you today?");
    w.say(w.bo, "Hello Ada, I said hello to you too just now.");
    bool together = false;
    for (const auto& [sid, s] : w.g.ledger().sessions)
        together = together || (s.members.count(w.adaId) && s.members.count(w.bo2));
    expect(!together, "no scene holds both");
}

void reporting()
{
    World3 w(options());
    w.say(w.bo, "You are a terrible wolf and everyone hates you.");
    w.say(w.cy, "Good evening, all.");
    w.say(w.bo, "Get lost, nobody wants you here.");
    // A report by a line's number: the server finds its author in Ada's own record.
    const auto seq = w.ada.sequenceOf("nobody wants you");
    expect(seq > 0, "Ada received the line");
    w.safety(w.ada, {{"verb", "report"}, {"line", double(seq)}, {"category", "harassment"}, {"note", "He keeps at it."}, {"block", true}});
    expect(w.ada.said("Reported. A Dungeon Master will look at it. And blocked."), "reported, and blocked");
    expect(w.g.reportsKept().size() == 1, "one report kept");
    const auto& r = w.g.reportsKept().begin()->second;
    expect(r.reportedCharacter == w.bo1 && r.reportedAccount == "bob" && r.reporterAccount == "ada" && r.category == "harassment" &&
               r.note == "He keeps at it.",
           "against Bo One, from Ada, as harassment, with her note");
    bool cy = false, terrible = false, lost = false;
    for (const auto& l : r.evidence)
    {
        cy = cy || l.text.find("Good evening") != std::string::npos;
        terrible = terrible || l.text.find("terrible wolf") != std::string::npos;
        lost = lost || l.text.find("nobody wants you") != std::string::npos;
    }
    expect(terrible && lost && !cy, "the evidence: his lines as she received them, and nobody else's");
    expect(w.g.blocked(w.adaId, w.bo1), "and the block holds");
    // Five a day.
    for (int i = 0; i < 4; ++i)
        w.safety(w.ada, {{"verb", "report"}, {"target", w.cyId}, {"category", "spam"}});
    w.safety(w.ada, {{"verb", "report"}, {"target", w.cyId}, {"category", "spam"}});
    expect(w.ada.said("You have made 5 reports today"), "five reports a day");
}

void silencing()
{
    World3 w(options());
    w.say(w.bo, "Something awful.");
    w.safety(w.ada, {{"verb", "report"}, {"target", w.bo1}, {"category", "hateful"}});
    expect(!w.g.decideReport("rep-nope", "uphold", "silence", 1, "dm:test").ok, "no such report");
    const auto id = w.g.reportsKept().begin()->first;
    expect(!w.g.decideReport(id, "uphold", "silence", 5, "dm:test").ok, "a silence is 1, 6, 24 or 72 hours");
    expect(w.g.upheldReportsWithin("bob", 30) == 0, "nothing upheld yet");
    // The DM upholds it with a silence (here as report.decide would).
    expect(w.g.decideReport(id, "uphold", "silence", 1, "dm:test").ok && w.g.upheldReportsWithin("bob", 30) == 1, "upheld");
    expect(w.bo.said("silenced your account for 1 hour"), "Bo is told");
    w.say(w.bo, "Can I talk now?");
    expect(!w.cy.got("roleplay", "Can I talk now?") && w.bo.said("A Dungeon Master has silenced your account"), "silenced: he can't speak");
    w.say(w.bo, "ooc: hello?", "ooc");
    expect(!w.cy.got("ooc", "hello?"), "not out of character either");
}

void theStore()
{
    auto store = reports::memoryStore();
    reports::Report old;
    old.id = "rep-old";
    old.created = 1000;
    reports::Report upheld;
    upheld.id = "rep-upheld";
    upheld.created = 1000;
    upheld.status = "upheld";
    upheld.decidedAt = 1000;
    upheld.evidence.push_back({1, 1000, "ic", "awful words"});
    reports::Report fresh;
    fresh.id = "rep-fresh";
    fresh.created = 1000 + 25 * 86400;
    std::string error;
    for (const auto* r : {&old, &upheld, &fresh})
        store->add(*r, error);
    store->purge(1000 + 31 * 86400);
    auto kept = store->all();
    expect(kept.size() == 2, "an open report past 30 days goes; the upheld and the recent stay");
    store->purge(1000 + 200 * 86400);
    kept = store->all();
    expect(kept.size() == 1 && kept[0].id == "rep-upheld" && kept[0].evidence.empty(), "an upheld report keeps its record for good, its evidence 180 days");
    const auto back = reports::fromJson(reports::toJson(upheld));
    expect(back.id == "rep-upheld" && back.status == "upheld" && back.evidence.size() == 1 && back.evidence[0].text == "awful words", "a report round-trips");
}
} // namespace

int main()
{
    try
    {
        theStore();
        muting();
        blocking();
        reporting();
        silencing();
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAILED after " << checks << " checks: " << e.what() << '\n';
        return 1;
    }
    std::cout << "Safety tests passed: " << checks << " checks.\n";
    return 0;
}
