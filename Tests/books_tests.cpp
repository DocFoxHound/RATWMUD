// Story books and the bookshelf (Docs/Design/51-scenes-and-stars.md, Phase 7): a book begun from a scene under way; a
// scene linked after the fact, placed where it falls; "my next scene goes into it"; chapters kept past their scenes,
// titled and summarised (from a recap); a private scene sealed from readers until the book is finished; sharing with
// friends and the shelf's filters, and Unaffiliated; finishing by a majority, with the private-scene warning, or after
// quiet days; volumes; an official book agreed to; and all of it kept across a restart.
#include "RatwGame.h"
#include "RatwBooks.h"

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
    o.tiesOptional = true;                      // (Ties: doc 52, tested in newcomer_tests.)
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


// A party scene between Ada and Bo, its lines long enough to be paid: its id (still open).
std::string partyScene(World3& w, bool invite = true)
{
    if (invite)
    {
        w.send(w.ada, "action", {{"action", "invite"}, {"target", w.bo1}});
        w.send(w.bo, "party", {{"verb", "accept"}});
        w.tick(.3);
    }
    const char* lines[] = {"\"The river rose in the night and took the lower bridge with it, so we came the long way round by the mill.\"",
                           "\"Then you will have seen the miller's dog, the grey one that guards the ford and barks at every passing cart.\"",
                           "\"We did, and it followed us halfway to the crossroads before it lost interest and wandered back home again.\"",
                           "\"That dog has walked that road longer than I have been alive, and it still thinks the whole valley is its own.\""};
    static int scene = 0;                           // (Fresh words each scene: the ledger refuses the same line twice.)
    ++scene;
    for (int i = 0; i < 4; ++i)
    {
        ::usleep(2100000);
        std::string line = lines[i];
        line.insert(line.size() - 1, scene == 1 ? "" : scene == 2 ? " Truly." : scene == 3 ? " Even now." : " Again.");
        w.send(i % 2 ? w.bo : w.ada, "chat", {{"text", line}, {"channel", "party"}});
        w.tick(1.0);
    }
    for (const auto& sid : w.g.ledger().scenesOf(w.adaId))
        return sid;
    return {};
}
void book(World3& w, Client& c, std::initializer_list<std::pair<const char*, json::Value>> fields) { w.send(c, "book", fields); }
const json::Value* view(Client& c) { return c.last("book") ? &c.last("book")->object("book") : nullptr; }
void endScene(World3& w)
{
    w.send(w.ada, "action", {{"action", "session_end"}, {"target", ""}});
    w.tick(.5);
}

void thePureRules()
{
    books::Book b;
    b.keeper = "ada";
    b.wolves = {"ada", "bo", "cy"};
    b.chapters.push_back({"c1", "s1", "At the ford", "", "", false, false, "the ford", "ada", 1000, 1600, {"ada", "bo", "cy"}});
    b.state = "finishing";
    b.finishProposed = 2000;
    b.agreed = {"ada"};
    expect(books::recentlyActive(b, 3000).size() == 3, "three recently active");
    expect(!books::finishes(b, 3000), "one of three: not yet");
    b.agreed.insert("bo");
    expect(books::finishes(b, 3000), "two of three: a majority");
    b.agreed = {"ada"};
    expect(books::finishes(b, 2000 + 3 * 86400 + 1), "three quiet days, no objection: finished");
    b.objected = {"cy"};
    expect(!books::finishes(b, 2000 + 3 * 86400 + 1), "an objection stops the quiet way");
    expect(books::inverse("sequel") == "prequel" && books::inverse("related") == "related", "volumes link both ways");
    const auto back = books::load(books::save(b));
    expect(back.chapters.size() == 1 && back.chapters[0].title == "At the ford" && back.agreed == b.agreed && back.state == "finishing",
           "a book round-trips");
}

void aBookFromScenes()
{
    const std::string save = "/tmp/ratw-books-" + std::to_string(::getpid()) + ".json";
    std::remove(save.c_str());
    std::string id, first;
    {
        World3 w(options(save));
        w.handles();
        // During: begun from the party scene under way (a Private scene: party scenes start so).
        first = partyScene(w);
        book(w, w.ada, {{"verb", "start"}, {"title", "The Drowned Bell"}, {"session", first}});
        expect(w.ada.said("You begin \"The Drowned Bell\".") && view(w.ada), "Ada begins a book from their scene");
        id = view(w.ada)->string("id");
        const auto* b = view(w.ada);
        expect(b->array("chapterList").size() == 1 && b->array("chapterList")[0].string("title").rfind("At ", 0) == 0 &&
                   b->array("chapterList")[0].boolean("private") && b->array("wolves").size() == 2,
               "its first chapter: their scene, private, both of them in it: " + json::dump(*b));
        endScene(w);
        // Before: Bo's next scene goes into it.
        book(w, w.bo, {{"verb", "next"}, {"book", id}, {"on", true}});
        expect(w.bo.said("Your next scene goes into"), "Bo flags his next scene");
        const auto second = partyScene(w, false);
        endScene(w);
        expect(w.g.storyBooks().at(id).chapters.size() == 2 && w.g.storyBooks().at(id).chapters[1].session == second,
               "his next scene went into it, after the first");
        // Chapters titled and summarised: from Ada's recap of the first, by hand for the second.
        const auto chapterOne = w.g.storyBooks().at(id).chapters[0].id, chapterTwo = w.g.storyBooks().at(id).chapters[1].id;
        book(w, w.ada, {{"verb", "summarise"}, {"book", id}, {"chapter", chapterOne}});
        expect(w.ada.said("drawn from your recap") && !w.g.storyBooks().at(id).chapters[0].summary.empty(), "a chapter summary from her recap");
        book(w, w.bo, {{"verb", "chapter"}, {"book", id}, {"chapter", chapterTwo}, {"title", "The miller's dog"},
                       {"text", "They talked of the dog that thinks the valley is its own."}});
        expect(w.g.storyBooks().at(id).chapters[1].title == "The miller's dog", "a chapter titled by hand");
        // Moved: the second first.
        book(w, w.ada, {{"verb", "move"}, {"book", id}, {"chapter", chapterTwo}, {"to", 0}});
        expect(w.g.storyBooks().at(id).chapters[0].id == chapterTwo, "chapters reordered");
        book(w, w.ada, {{"verb", "move"}, {"book", id}, {"chapter", chapterTwo}, {"to", 1}});
        // Shared with friends: Cy, Ada's friend, finds it under Unaffiliated, and reads it with the private scenes sealed.
        w.friends(w.cy, {{"verb", "request"}, {"handle", "Adder"}});
        w.friends(w.ada, {{"verb", "accept"}, {"handle", "Cypress"}});
        book(w, w.cy, {{"verb", "open"}, {"book", id}});
        expect(!w.cy.last("book"), "members only at first: Cy can't open it");
        book(w, w.ada, {{"verb", "share"}, {"book", id}, {"sharing", "friends"}});
        w.send(w.cy, "book", {{"verb", "shelf"}, {"tab", "unaffiliated"}, {"filter", "all"}});
        const auto* shelf = w.cy.last("shelf");
        expect(shelf && shelf->array("books").size() == 1 && shelf->array("books")[0].string("kind") == "friend",
               "Cy's Unaffiliated shelf: a friend's book");
        w.send(w.cy, "book", {{"verb", "shelf"}, {"tab", "shelf"}, {"filter", "all"}});
        expect(w.cy.last("shelf")->array("books").empty(), "and not on her own shelf: she isn't in it");
        book(w, w.cy, {{"verb", "open"}, {"book", id}});
        const auto* read = view(w.cy);
        expect(read && read->array("chapterList")[0].boolean("sealed") && !read->array("chapterList")[0].has("summary"),
               "a private scene's summary sealed from her: " + (read ? json::dump(*read) : std::string()));
        w.send(w.ada, "book", {{"verb", "shelf"}, {"tab", "shelf"}, {"filter", "other"}});
        expect(w.ada.last("shelf")->array("books").size() == 1, "on Ada's own shelf, under Other (Bo is no friend of hers)");
        // Official: Ada proposes it; Bo agrees.
        book(w, w.ada, {{"verb", "official"}, {"book", id}});
        expect(w.ada.said("You propose making it official"), "official proposed: " + std::string(w.ada.last("system") ? w.ada.last("system")->string("text") : ""));
        book(w, w.bo, {{"verb", "approve"}, {"book", id}});
        book(w, w.ada, {{"verb", "open"}, {"book", id}});
        expect(view(w.ada)->string("storyState") == "active", "agreed: an official Story underneath");
        w.g.save();
    }
    {
        World3 w(options(save), false);
        expect(w.g.storyBooks().count(id) && w.g.storyBooks().at(id).chapters.size() == 2 && w.g.storyBooks().at(id).sharing == "friends",
               "the book after a restart");
        // Finishing: Ada proposes (warned of private scenes), Bo agrees, it's finished, with flavour, and Cy may now read all.
        book(w, w.ada, {{"verb", "finish"}, {"book", id}});
        expect(w.ada.said("This book has private scenes") && w.bo.said("proposes to finish"), "proposed, with the warning; Bo asked");
        const auto social = w.g.socialXp(w.adaId);
        book(w, w.bo, {{"verb", "agree"}, {"book", id}});
        expect(w.g.storyBooks().at(id).state == "finished" && !w.g.storyBooks().at(id).flavour.empty(), "a majority: finished, with flavour");
        expect(w.g.socialXp(w.adaId) > social, "an official book pays when it is finished");
        book(w, w.cy, {{"verb", "open"}, {"book", id}});
        expect(view(w.cy) && !view(w.cy)->array("chapterList")[0].boolean("sealed"), "finished: Cy reads its private scene's summary");
        book(w, w.ada, {{"verb", "link"}, {"book", id}, {"session", "x"}});
        expect(w.ada.said("You weren't in that scene.") || w.ada.said("That book is finished."), "no more chapters");
        // A volume: a second finished book as its sequel.
        book(w, w.bo, {{"verb", "start"}, {"title", "The Ford Again"}});
        const auto sequel = view(w.bo)->string("id");
        const auto third = partyScene(w, false);
        book(w, w.bo, {{"verb", "link"}, {"book", sequel}, {"session", third}});
        endScene(w);
        book(w, w.bo, {{"verb", "finish"}, {"book", sequel}});
        expect(w.g.storyBooks().at(sequel).state == "finishing", "Bo alone is one of its two recent wolves: not yet");
        book(w, w.ada, {{"verb", "agree"}, {"book", sequel}});
        expect(w.g.storyBooks().at(sequel).state == "finished", "with Ada, finished");
        book(w, w.ada, {{"verb", "volume"}, {"book", id}, {"other", sequel}, {"kind", "sequel"}});
        book(w, w.cy, {{"verb", "open"}, {"book", sequel}});
        book(w, w.bo, {{"verb", "open"}, {"book", sequel}});
        const auto& volume = view(w.bo)->array("volume");
        expect(volume.size() == 1 && volume[0].string("id") == id && volume[0].string("kind") == "prequel", "its volume: the first, its prequel");
        // The DM ties it to a world storyline: the World shelf.
        expect(w.g.tieBookToStoryline(id, "The Bandit Winter", "dm:test").ok, "tied to a world storyline");
        w.send(w.ada, "book", {{"verb", "shelf"}, {"tab", "shelf"}, {"filter", "world"}});
        expect(w.ada.last("shelf")->array("books").size() == 1, "on the World shelf");
    }
    std::remove(save.c_str());
}
} // namespace

int main()
{
    try
    {
        thePureRules();
        aBookFromScenes();
    }
    catch (const std::exception& e)
    {
        std::cerr << "books_tests failed after " << checks << " checks: " << e.what() << '\n';
        return 1;
    }
    std::cout << "books_tests passed (" << checks << " checks)\n";
    return 0;
}
