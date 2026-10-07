// The star book (Docs/Design/51-scenes-and-stars.md, Phase 1): every star recorded against the receiving account;
// what counts toward its total (the giver's 10 a day, a pair's 3 a day and 10 in 30 days, never one's own wolves); the
// distinct giver accounts; bands for strangers and the exact count for the player and a friend who sees which wolf is
// theirs; stars refused between one account's wolves; Quickened reading the book; and the book kept across a restart.
#include "RatwGame.h"
#include "RatwStars.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unistd.h>

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
    std::vector<json::Value> events, snapshots;
    sections::Cache cache;
    void event(const std::string& text) override { events.push_back(parsed(text)); }
    void snapshot(const std::string& text) override
    {
        auto v = parsed(text);
        expect(sections::fill(v, cache), "a snapshot can be filled");
        snapshots.push_back(v);
    }
    void motion(const json::Value&) override {}
    bool allowsLocalCredentials() const override { return true; }
    const json::Value* last(const std::string& type) const
    {
        for (auto it = events.rbegin(); it != events.rend(); ++it)
            if (it->string("type") == type)
                return &*it;
        return nullptr;
    }
    const json::Value* seen(const std::string& id) const
    {
        if (snapshots.empty())
            return nullptr;
        for (const auto& e : snapshots.back().array("entities"))
            if (e.string("id") == id)
                return &e;
        return nullptr;
    }
    // Private messages received (not one's own copies) with this text in them.
    const json::Value* message(const std::string& text) const
    {
        for (const auto& e : events)
            if (e.string("type") == "ooc" && e.string("channel") == "private" && !e.boolean("outgoing") &&
                e.string("text").find(text) != std::string::npos)
                return &e;
        return nullptr;
    }
    bool said(const std::string& text) const
    {
        for (const auto& e : events)
            if (e.string("text").find(text) != std::string::npos)
                return true;
        return false;
    }
    // A friend on this player's list, by handle.
    const json::Value* friendRow(const std::string& handle) const
    {
        if (const auto* f = last("friends"))
            for (const auto& row : f->array("friends"))
                if (row.string("handle") == handle)
                    return &row;
        return nullptr;
    }
    bool asked(const std::string& handle, const char* list = "incoming") const
    {
        if (const auto* f = last("friends"))
            for (const auto& row : f->array(list))
                if (row.string("handle") == handle)
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

game::Options options(const std::string& save = {})
{
    game::Options o;
    o.hiddenNames = false;
    o.forkSnapshots = false;
    o.oneWolfPerAccount = true;
    if (!save.empty())
        o.savePath = save;
    return o;
}

// Three accounts: ada (Ada), bob (Bo One, Bo Two) and cyd (Cy), each with a handle unless told not to.
struct World3
{
    game::Game g;
    Client ada, bo, cy;
    std::string adaId, bo1, bo2, cyId;
    explicit World3(game::Options o, bool fresh = true) : g(o)
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
            g.command(&c, cmd({{"type", fresh ? "auth_register" : "auth_login"}, {"username", user}, {"password", "a long enough password"}}));
            g.settle();
            int i = 0;
            if (fresh)
                for (const char* name : names)
                    g.command(&c, cmd({{"type", "character_create"}, {"name", name}, {"age", 24}, {"appearance", appearance()},
                                       {"commandId", std::string(user) + std::to_string(i++)}}));
            g.settle();
            std::vector<std::string> ids;
            expect(c.last("lobby") != nullptr, std::string("a lobby for ") + user);
            for (const auto& ch : c.last("lobby")->array("characters"))
                ids.push_back(ch.string("id"));
            expect(ids.size() == names.size(), std::string("characters for ") + user + ": " + c.last("lobby")->string("message"));
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
            e->position = {a->position.x + 1.2 * double(&c == &cy ? 2 : 1), a->position.y};
        }
        tick(.3);
    }
    void leave(Client& c)
    {
        g.command(&c, cmd({{"type", "character_leave"}}));
        g.settle();
    }
    void tick(double seconds)
    {
        for (double t = 0; t < seconds; t += .05)
        {
            g.tick(.05);
            for (auto* c : {&ada, &bo, &cy})
                if (!c->snapshots.empty())
                    g.acknowledge(c, c->snapshots.back().number("revision"), false);
        }
    }
    void send(Client& c, const std::string& type, std::initializer_list<std::pair<const char*, json::Value>> fields)
    {
        auto o = json::Value::object();
        o.add("type", type);
        for (const auto& [k, v] : fields)
            o.add(k, v);
        g.command(&c, json::dump(o));
        g.settle();
    }
    void friends(Client& c, std::initializer_list<std::pair<const char*, json::Value>> fields) { send(c, "friends", fields); }
    void message(Client& c, const std::string& to, const std::string& text)
    {
        send(c, "chat", {{"channel", "private"}, {"to", to}, {"text", text}});
        tick(1.0);                                  // (Past chat's rate limit before the next line.)
    }
    void handles()
    {
        send(ada, "profile", {{"verb", "handle"}, {"handle", "Adder"}});
        send(bo, "profile", {{"verb", "handle"}, {"handle", "Bobbin"}});
        send(cy, "profile", {{"verb", "handle"}, {"handle", "Cypress"}});
    }
    void befriend(Client& a, Client& b, const char* bHandle, const char* aHandle)
    {
        friends(a, {{"verb", "request"}, {"handle", bHandle}});
        friends(b, {{"verb", "accept"}, {"handle", aHandle}});
    }
};


stars::Star star(const std::string& from, const std::string& to, double at, const char* kind = "gold")
{
    stars::Star s;
    s.id = from + "-" + to + "-" + std::to_string(at);
    s.kind = kind;
    s.giverAccount = from;
    s.giverCharacter = "w-" + from;
    s.recipientAccount = to;
    s.recipientCharacter = "w-" + to;
    s.at = at;
    return s;
}

void theBook()
{
    stars::Book b;
    const double t = 1e9;
    // A pair: 3 a day count; the 4th is given but doesn't.
    for (int i = 0; i < 4; ++i)
        expect(b.record(star("bob", "ada", t + i)).counted == (i < 3), "a pair's 3 a day: star " + std::to_string(i + 1));
    expect(b.tally("ada")->total == 3 && b.tally("ada")->goldReceived == 4 && b.tally("ada")->givers.size() == 1,
           "3 counted, 4 received, from 1 account");
    // The next day the pair counts again, up to 10 in 30 days.
    for (int day = 1; day < 6; ++day)
        for (int i = 0; i < 3; ++i)
            b.record(star("bob", "ada", t + day * 86400 + i));
    expect(b.tally("ada")->total == 10, "a pair's 10 in 30 days: " + std::to_string(b.tally("ada")->total));
    // A giver's 10 a day, across recipients.
    for (int i = 0; i < 11; ++i)
        b.record(star("cyd", "r" + std::to_string(i), t + 10 * 86400 + i));
    int counted = 0;
    for (int i = 0; i < 11; ++i)
        counted += b.tally("r" + std::to_string(i)) ? b.tally("r" + std::to_string(i))->total : 0;
    expect(counted == 10, "a giver's 10 a day");
    // Never one's own.
    expect(!b.record(star("ada", "ada", t + 20 * 86400)).counted, "a star from one's own account never counts");
    // Kinds and the spread, by account.
    b.record(star("dee", "eve", t + 21 * 86400, "story"));
    b.record(star("fay", "eve", t + 21 * 86400 + 1));
    expect(b.tally("eve")->total == 2 && b.tally("eve")->kinds.at("story") == 1 && b.tally("eve")->givers.size() == 2,
           "a Story Star counts as one; two givers");
    // Bands: strangers see a few, 10+, 25+...
    expect(stars::band(0) == "a few" && stars::band(9) == "a few" && stars::band(10) == "10+" && stars::band(437) == "250+" &&
               stars::band(1200) == "1000+",
           "the bands");
    expect(stars::band(4, true) == "a few" && stars::band(31, true) == "30+", "the giver bands");
    const auto exact = b.view("ada", true), banded = b.view("ada", false);
    expect(exact.number("total") == 10 && exact.number("from") == 1 && exact.boolean("exact"), "exact for the player");
    expect(!banded.has("total") && !banded.has("from") && banded.string("band") == "10+" && banded.string("fromBand") == "a few",
           "bands for a stranger, never the count");
    // Kept: a round trip; and stars past 30 days leave the recent list but not the tally.
    stars::Book back;
    back.load(b.save());
    expect(back.tally("ada")->total == 10 && back.recent().size() == b.recent().size(), "the book round-trips");
    back.prune(t + 60 * 86400);
    expect(back.recent().empty() && back.tally("ada")->total == 10, "old stars pruned, the total kept");
}

void tagsKnownForAndTheRate()
{
    // Tags: only the giver, once, within ten minutes, a real tag; Welcoming only from a newcomer.
    stars::Book b;
    const double t = 2e9;
    const auto s1 = b.record(star("bob", "ada", t));
    std::string why;
    expect(!b.tag(s1.id, "cyd", "packmate", false, t + 1, why) && why == "That isn't a star you gave.", "only its giver tags it");
    expect(!b.tag(s1.id, "bob", "nemesis", false, t + 1, why) && why == "Not a tag.", "a real tag");
    expect(!b.tag(s1.id, "bob", "welcoming", false, t + 1, why) && why == "Only a newcomer gives that tag.", "Welcoming from newcomers only");
    expect(b.openToTag("w-bob", t + 5).size() == 1, "open to a tag for ten minutes");
    expect(b.tag(s1.id, "bob", "packmate", false, t + 5, why) && b.tally("ada")->tags.at("packmate") == 1, "tagged Packmate");
    expect(!b.tag(s1.id, "bob", "goodfun", false, t + 6, why) && why == "That star is tagged already.", "once, for good");
    expect(b.openToTag("w-bob", t + 7).empty(), "and no longer open");
    const auto s2 = b.record(star("bob", "ada", t + 10));
    expect(!b.tag(s2.id, "bob", "goodfun", false, t + 700, why) && why == "It's too late to tag that star." &&
               b.openToTag("w-bob", t + 700).empty(),
           "past ten minutes, too late");
    expect(b.tag(s2.id, "bob", "welcoming", true, t + 11, why), "a newcomer may give Welcoming");
    // Known for, from 50 counted stars: the most-given tag, both when tied; counts for the player, words for others.
    stars::Book k;
    for (int i = 0; i < 49; ++i)
    {
        const auto s = k.record(star("g" + std::to_string(i), "ada", t + i));
        k.tag(s.id, "g" + std::to_string(i), i < 30 ? "storyteller" : i < 45 ? "packmate" : "goodfun", false, t + i, why);
    }
    expect(!k.view("ada", true).has("knownFor"), "49 stars: not yet Known for anything");
    k.record(star("g49", "ada", t + 49));
    const auto mine = k.view("ada", true), theirs = k.view("ada", false);
    expect(mine.array("knownFor").size() == 1 && mine.array("knownFor")[0].asString() == "Storyteller", "50: Known for Storyteller");
    expect(mine.array("tags")[0].number("count") == 30 && !mine.array("tags")[0].has("words"), "the player sees counts");
    expect(theirs.array("tags")[0].string("words") == "mostly" && theirs.array("tags")[1].string("words") == "often" &&
               !theirs.array("tags")[0].has("count") && theirs.array("knownFor")[0].asString() == "Storyteller",
           "a stranger sees words: mostly Storyteller, often Packmate");
    stars::Book tie;
    for (int i = 0; i < 50; ++i)
    {
        const auto s = tie.record(star("g" + std::to_string(i), "ada", t + i));
        tie.tag(s.id, "g" + std::to_string(i), i % 2 ? "packmate" : "goodfun", false, t + i, why);
    }
    expect(tie.view("ada", false).array("knownFor").size() == 2, "a tie shows both");
    // The rate: Gold Stars received for chances, in words, once there have been 20 chances.
    stars::Book r;
    r.chances("ada", 19);
    for (int i = 0; i < 12; ++i)
        r.record(star("h" + std::to_string(i), "ada", t + i));
    expect(!r.view("ada", false).has("rate"), "19 chances: no rate yet");
    r.chances("ada", 1);
    expect(r.view("ada", false).string("rate") == "most wolves who play with them leave a star" &&
               r.view("ada", true).number("rateShare") == 0.6 && !r.view("ada", false).has("rateShare"),
           "12 of 20: \"most wolves...\"; the share for the player only");
    r.chances("ada", 30);
    expect(r.view("ada", false).string("rate") == "some wolves who play with them leave a star", "12 of 50: \"some\"");
}

// A party scene between Ada and Bo, ended: its id.
std::string scene(World3& w)
{
    w.send(w.ada, "action", {{"action", "invite"}, {"target", w.bo1}});
    w.send(w.bo, "party", {{"verb", "accept"}});
    w.tick(.3);
    const char* lines[] = {"\"The river rose in the night and took the lower bridge with it, so we came round by the mill.\"",
                           "\"Then you will have seen the miller's dog, the grey one that guards the ford and barks at carts.\"",
                           "\"We did, and it followed us halfway to the crossroads before it lost interest in us.\"",
                           "\"That dog has walked that road longer than I have been alive, and still thinks it owns it.\""};
    for (int i = 0; i < 4; ++i)
    {
        if (i == 2)
            ::usleep(2100000);
        w.send(i % 2 ? w.bo : w.ada, "chat", {{"text", lines[i]}, {"channel", "party"}});
        w.tick(1.0);
    }
    w.send(w.ada, "action", {{"action", "session_end"}, {"target", ""}});
    w.tick(.3);
    for (const auto& [id, s] : w.g.ledger().sessions)
        if (s.ended > 0 && s.members.count(w.adaId))
            return id;
    return {};
}

const json::Value* myStars(Client& c)
{
    if (c.snapshots.empty())
        return nullptr;
    const auto& s = c.snapshots.back().object("self").object("social");
    return s.has("stars") ? &s.object("stars") : nullptr;
}

void throughTheGame()
{
    World3 w(options());
    w.handles();
    const auto sid = scene(w);
    expect(!sid.empty(), "a scene between Ada and Bo, ended");
    w.send(w.bo, "social", {{"verb", "star"}, {"session", sid}, {"target", w.adaId}});
    expect(w.bo.said("You give") && w.bo.said("a Gold Star"), "Bo stars Ada: " + std::string(w.bo.last("system") ? w.bo.last("system")->string("text") : ""));
    // Bo may tag it for ten minutes: it's listed for him, and his tag reaches Ada's count.
    w.tick(2.5);
    const auto& given = w.bo.snapshots.back().object("self").object("social").array("starsGiven");
    expect(given.size() == 1 && given[0].number("left") > 500 && !given[0].string("to").empty(), "Bo's star, open to a tag");
    w.send(w.bo, "social", {{"verb", "startag"}, {"star", given[0].string("id")}, {"tag", "storyteller"}});
    expect(w.bo.said("Tagged: Storyteller."), "tagged");
    expect(w.g.starBook().tally("ada")->tags.at("storyteller") == 1, "Ada's Storyteller count");
    expect(w.g.starBook().tally("ada")->chances == 1 && w.g.starBook().tally("bob")->chances == 1,
           "each had one chance to be starred: one other qualified with them");
    w.tick(2.5);
    const auto* mine = myStars(w.ada);
    expect(mine && mine->boolean("exact") && mine->number("total") == 1 && mine->number("from") == 1,
           "Ada's sheet: exactly one star from one wolf");
    // A stranger sees a band; a friend who sees which wolf is hers sees the count.
    w.send(w.cy, "action", {{"action", "inspect"}, {"target", w.adaId}});
    auto stars = w.cy.last("inspect")->object("stars");
    expect(!stars.boolean("exact") && stars.string("band") == "a few" && !stars.has("total"), "Cy, a stranger, sees \"a few\"");
    w.friends(w.cy, {{"verb", "request"}, {"handle", "Adder"}});
    w.friends(w.ada, {{"verb", "accept"}, {"handle", "Cypress"}});
    w.send(w.cy, "action", {{"action", "inspect"}, {"target", w.adaId}});
    stars = w.cy.last("inspect")->object("stars");
    expect(stars.boolean("exact") && stars.number("total") == 1, "a friend she shares her wolf with sees the count");
    w.friends(w.ada, {{"verb", "share"}, {"handle", "Cypress"}, {"on", false}});
    w.send(w.cy, "action", {{"action", "inspect"}, {"target", w.adaId}});
    expect(!w.cy.last("inspect")->object("stars").boolean("exact"), "a friend she doesn't share with sees the band");
    // Quickened reads the book.
    bool counted = false;
    if (const auto* tiers = w.g.tiersOf("ada"))
        for (const auto& line : tiers->object("quickened").array("progress"))
            counted |= line.string("measure") == "stars" && line.number("have") == 1;
    expect(counted, "Quickened's stars come from the book");
    // Not between one account's own wolves.
    w.leave(w.bo);
    w.enter(w.bo, w.bo2);
    w.send(w.bo, "social", {{"verb", "star"}, {"session", sid}, {"target", w.bo1}});
    expect(w.bo.said("Not one of your own wolves."), "Bo Two can't star Bo One");
}

void keptAcrossARestart()
{
    const std::string save = "/tmp/ratw-stars-" + std::to_string(::getpid()) + ".json";
    std::remove(save.c_str());
    {
        World3 w(options(save));
        const auto sid = scene(w);
        w.send(w.bo, "social", {{"verb", "star"}, {"session", sid}, {"target", w.adaId}});
        w.g.save();
    }
    {
        World3 w(options(save), false);
        const auto* tally = w.g.starBook().tally("ada");
        expect(tally && tally->total == 1 && tally->givers.count("bob") && w.g.starBook().recent().size() == 1, "the book after a restart");
    }
    std::remove(save.c_str());
}
} // namespace

int main()
{
    try
    {
        theBook();
        tagsKnownForAndTheRate();
        throughTheGame();
        keptAcrossARestart();
    }
    catch (const std::exception& e)
    {
        std::cerr << "stars_tests failed after " << checks << " checks: " << e.what() << '\n';
        return 1;
    }
    std::cout << "stars_tests passed (" << checks << " checks)\n";
    return 0;
}
