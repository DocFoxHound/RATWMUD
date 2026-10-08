// Letters (Docs/Design/55-letters-gifts-favours.md, Phase 1): the document store, the courier's hours and fees, and
// letters between players in the game: written at an inn, to a wolf known by name, the fee to the town's treasury,
// handed over in town or waiting at the reader's post town's inns, collected or sent on; scent read by name, by sight,
// unknown, masked or faded; a signature introduces; an anonymous letter answered by the same courier; limits; saved.
#include "RatwCalendar.h"
#include "RatwDocuments.h"
#include "RatwGame.h"
#include "RatwWorld.h"

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

void theCourier()
{
    using namespace documents;
    expect(courierHours(0, true) == 1 && courierFee(0, true) == 1, "within a town: an hour, a penny");
    expect(std::abs(courierHours(9, false) - 4) < 1e-9 && courierFee(9, false) == 2, "nine road cells away: four hours, two pennies");
    expect(courierHours(90, false) == 12 && courierFee(25, false) == 4, "at most twelve hours; a penny more each ten cells");
}

void theStore()
{
    documents::Store s;
    documents::Document a;
    a.author = "ash";
    a.to = "bo";
    a.text = "First.";
    a.written = 1;
    a.deliverAt = 1.2;
    const auto first = s.add(a).id;
    a.text = "Second.";
    a.deliverAt = 1.1;
    const auto second = s.add(a).id;
    expect(first != second && s.due(1.15) == std::vector<std::string>{second}, "the courier brings them in order of arrival");
    expect(s.forReader("bo").empty(), "travelling letters aren't in the case yet");
    for (const auto& id : {first, second})
    {
        auto* d = s.find(id);
        d->state = "delivered";
        s.rescheduled(id, d->deliverAt);
    }
    expect(s.due(9).empty() && s.forReader("bo").size() == 2 && s.unread("bo") == 2, "delivered: in the case, unread");
    s.find(second)->readAt = 1.3;
    expect(s.forReader("bo").front()->id == first && s.unread("bo") == 1, "unread first");
    expect(s.writtenSince("ash", 1) == 2 && s.writtenSince("ash", 1.5) == 0, "what a wolf wrote today");
    const auto back = documents::load(documents::save(*s.find(first)));
    expect(back.id == first && back.text == "First." && back.state == "delivered" && back.to == "bo", "saved and loaded");
    s.erase(first);
    expect(!s.find(first) && s.forReader("bo").size() == 1, "burnt");
}


void lettersInTheGame()
{
    const auto root = fs::temp_directory_path() / ("ratw-letters-" + std::to_string(::getpid()));
    const auto save = (fs::temp_directory_path() / ("ratw-letters-save-" + std::to_string(::getpid()) + ".json")).string();
    fs::remove(save);
    const auto world = writeStrip(root);
    {
        Town t(world, save);
        t.enter(t.ash, 1, "ash", "Ash", 1, 11.5);  // At Upper Accord's inn.
        t.enter(t.bo, 2, "bo", "Bo", 1, 12.5);
        t.run(1);
        // Ash learns Bo's name from his own mouth; Bo doesn't learn hers.
        t.g.command(&t.bo, cmd({{"type", "chat"}, {"text", "\"I'm Bo.\""}, {"commandId", "b1"}}));
        t.run(1);
        auto r = t.letter(t.ash, "write", {{"to", "Zed"}, {"text", "Hello."}});
        expect(!r.ok && contains(r.message, "know no wolf called Zed"), "a wolf she doesn't know by name: " + r.message);
        t.place(t.ash, 0, 8.5);
        r = t.letter(t.ash, "write", {{"to", "Bo"}, {"text", "Hello."}});
        expect(!r.ok && contains(r.message, "written at an inn"), "not out in the street: " + r.message);
        t.place(t.ash, 1, 11.5);
        const auto treasury = t.g.world().society().account("treasury")->cash;
        const auto purse = t.g.world().society().account(t.ash.entityId)->cash;
        r = t.letter(t.ash, "write", {{"to", "bo"}, {"text", "Meet me by the fountain at dusk."}, {"sign", "Ash"}});
        expect(r.ok && contains(r.message, "(1p)") && contains(r.message, "about 1 hour") && contains(r.message, "You signed as Ash"),
               "to Bo, by the name she knows (any case): a penny, an hour: " + r.message);
        expect(t.g.world().society().account(t.ash.entityId)->cash == purse - 1 && t.g.world().society().account("treasury")->cash == treasury + 1,
               "the penny to the town's treasury");
        expect(t.caseOf(t.bo).array("letters").empty(), "not there yet");
        t.place(t.bo, 0, 6.5);                      // (In town, not at the inn.)
        t.bo.events.clear();
        t.hours(1.1);
        expect(contains(t.bo.said(), "A messenger finds you with a letter"), "an hour on, in town: handed to him");
        expect(t.selfOf(t.bo).object("letters").number("unread") == 1, "one unread");
        auto letters = t.caseOf(t.bo).array("letters");
        expect(letters.size() == 1 && !letters[0].has("text") && letters[0].string("sign") == "Ash", "sealed until opened, signed Ash");
        expect(letters[0].string("scent") == "A wolf's scent you don't know.", "a scent he doesn't know: " + letters[0].string("scent"));
        const auto id = letters[0].string("id");
        r = t.letter(t.bo, "read", {{"id", id}});
        expect(contains(r.message, "You break the seal") && contains(r.message, "It is signed Ash"), "he opens it: " + r.message);
        letters = t.caseOf(t.bo).array("letters");
        expect(letters[0].string("text") == "Meet me by the fountain at dusk." && letters[0].string("scent") == "It smells of Ash.",
               "the signature introduced her: now it smells of Ash: " + letters[0].string("scent"));
        // Masked and unsigned: no scent, and anonymous; he answers once by the same courier.
        t.enter(t.cy, 3, "cy", "Cy", 1, 13.5);
        t.run(.5);
        t.place(t.bo, 1, 12.5);
        t.run(1);
        t.g.command(&t.bo, cmd({{"type", "chat"}, {"text", "\"I'm Bo.\""}, {"commandId", "b2"}}));
        t.run(1);
        t.g.world().entity(t.cy.entityId)->scentMaskedUntil = t.g.world().time() + 9999;
        r = t.letter(t.cy, "write", {{"to", "Bo"}, {"text", "Someone watches you."}});
        expect(r.ok && contains(r.message, "Unsigned"), "Cy writes, masked and unsigned: " + r.message);
        t.hours(1.1);
        letters = t.caseOf(t.bo).array("letters");
        std::string anon;
        for (const auto& l : letters)
            if (l.string("sign").empty())
                anon = l.string("id");
        t.letter(t.bo, "read", {{"id", anon}});
        letters = t.caseOf(t.bo).array("letters");
        for (const auto& l : letters)
            if (l.string("id") == anon)
                expect(l.string("scent") == "It carries no scent at all. Someone took care." && l.boolean("canReply"), "no scent at all, and he may answer");
        r = t.letter(t.bo, "reply", {{"id", anon}, {"text", "Who are you?"}});
        expect(r.ok && contains(r.message, "back the way the letter came"), "answered by the same courier: " + r.message);
        r = t.letter(t.bo, "reply", {{"id", anon}, {"text", "Answer me."}});
        expect(!r.ok && contains(r.message, "already"), "once: " + r.message);
        t.hours(1.1);
        const auto cyCase = t.caseOf(t.cy).array("letters");
        expect(cyCase.size() == 1 && cyCase[0].boolean("viaCourier"), "Cy has the answer");
        // Ten a day at most.
        int sent = 0;
        for (int i = 0; i < 12; ++i)
            sent += t.letter(t.ash, "write", {{"to", "Bo"}, {"text", "Again " + std::to_string(i) + "."}}).ok;
        expect(sent == 9, "ten letters a game day (one sent already): " + std::to_string(sent));
        // Between towns: Bo left the world in Ridgemere; it waits there; he has it sent on.
        t.place(t.bo, 7, 8.5);
        t.g.disconnect(&t.bo);
        t.g.world().advanceCalendar(1);
        t.run(1);
        r = t.letter(t.ash, "write", {{"to", "Bo"}, {"text", "Come home."}, {"sign", "Ash"}});
        expect(r.ok && contains(r.message, "(2p)") && contains(r.message, "reach Ridgemere") && contains(r.message, "about 4 hours"),
               "to Ridgemere: two pennies, about four hours: " + r.message);
        t.hours(4);
        t.bo = Client{};
        t.enter(t.bo, 4, "bo", "Bo", 0, 8.5);      // Back, in Upper Accord.
        t.run(1);
        const auto self = t.selfOf(t.bo).object("letters");
        expect(self.number("waiting") >= 1, "letters wait for him");
        int atRidgemere = 0;
        for (const auto& d : t.caseOf(t.bo).array("waiting"))
            if (d.string("town") == "Ridgemere")
                atRidgemere = int(d.number("count"));
        expect(atRidgemere == 1, "the case says one waits at Ridgemere's inns");
        r = t.letter(t.bo, "sendOn", {{"from", "ridgemere"}});
        expect(r.ok && contains(r.message, "from Ridgemere to Upper Accord (1p)"), "sent on to where he is: " + r.message);
        // Those waiting at Upper Accord's inns (the nine he was out for): he walks into its inn and has them.
        t.bo.events.clear();
        t.place(t.bo, 1, 12.5);
        t.run(2);
        expect(contains(t.bo.said(), "At the inn, a letter waits for you"), "collected at the inn");
        t.hours(4.1);
        expect(t.selfOf(t.bo).object("letters").number("unread") >= 1, "and it reaches him");
        // One travelling now, for the restart.
        // A fortnight on, the scent has faded from what he has.
        t.g.world().advanceCalendar(15);
        t.run(1);
        bool faded = true;
        for (const auto& l : t.caseOf(t.bo).array("letters"))
            faded = faded && (l.string("scent") == "Its scent has faded." || l.string("scent") == "It carries no scent at all. Someone took care.");
        expect(faded, "after fourteen days: its scent has faded");
        t.g.world().advanceCalendar(1);             // (A new day: the courier takes letters again.)
        const auto last = t.letter(t.ash, "write", {{"to", "Bo"}, {"text", "Still there?"}});
        expect(last.ok, "one more, to be in transit across the restart: " + last.message);
        t.g.save();
    }
    {
        // (His post town is still Ridgemere, where he last left the world: it goes there, about four hours.)
        Town t(world, save);
        t.enter(t.bo, 2, "bo", "Bo", 7, 8.5);
        t.run(1);
        t.bo.events.clear();
        t.hours(4.1);
        expect(contains(t.bo.said(), "A messenger finds you"), "a letter in transit across a restart still arrives: " + t.bo.said());
    }
    fs::remove_all(root);
    fs::remove(save);
}

// Catalog goods with weight (for filling a pack with kinds).
std::vector<std::string> someGoods(std::size_t n)
{
    std::ifstream in(std::string(RATW_SOURCE_DIR) + "/Data/Items/items.json");
    std::stringstream text;
    text << in.rdbuf();
    const auto doc = parsed(text.str());
    std::vector<std::string> out;
    for (const auto& g : doc.array("items"))
        if (g.number("weight") > 0 && out.size() < n)
            out.push_back(g.string("id"));
    return out;
}

int held(game::Game& g, const std::string& who, const std::string& item)
{
    const auto* a = g.world().society().account(who);
    return a ? Society::stock(*a, item) : 0;
}

// Giving, enclosures and the giver's scent (Phase 2).
void giving()
{
    const auto root = fs::temp_directory_path() / ("ratw-giving-" + std::to_string(::getpid()));
    const auto world = writeStrip(root);
    Town t(world, {});
    t.enter(t.ash, 1, "ash", "Ash", 1, 11.5);
    t.enter(t.bo, 2, "bo", "Bo", 1, 12.5);
    t.run(1);
    auto& society = t.g.world().society();
    society.create(t.ash.entityId, "bread", 3, "test");
    society.create(t.ash.entityId, "herbs", 4, "test");
    const auto give = [&](Client& c, const std::string& target, const std::string& item, int n, int coins) {
        c.events.clear();
        t.g.command(&c, cmd({{"type", "give"}, {"target", target}, {"item", item}, {"quantity", n}, {"coins", coins}}));
        return c.said();
    };
    const auto answer = [&](Client& c, bool yes) {
        c.events.clear();
        t.g.command(&c, cmd({{"type", "giveAnswer"}, {"accept", yes}}));
        return c.said();
    };
    // To a player: offered, declined, then accepted; it smells of the giver.
    auto said = give(t.ash, t.bo.entityId, "bread", 1, 2);
    expect(contains(said, "You offer") && t.bo.last("giveOffer"), "Ash offers Bo bread and 2p: " + said);
    said = answer(t.bo, false);
    expect(contains(said, "You decline") && held(t.g, t.bo.entityId, "bread") == 0, "he declines: nothing moves");
    give(t.ash, t.bo.entityId, "bread", 1, 2);
    const auto boCash = society.account(t.bo.entityId)->cash;
    said = answer(t.bo, true);
    expect(held(t.g, t.bo.entityId, "bread") == 1 && held(t.g, t.ash.entityId, "bread") == 2 && society.account(t.bo.entityId)->cash == boCash + 2,
           "accepted: the bread and the pennies are his: " + said);
    expect(contains(said, "It smells of"), "and it smells of her: " + said);
    expect(society.conserved(), "money conserved");
    const auto* warmth = t.g.world().bonds().find(t.bo.entityId, t.ash.entityId);
    const double liked = warmth ? warmth->affinity : 0;
    expect(liked >= 5, "the gift warms him to her: " + std::to_string(liked));
    give(t.ash, t.bo.entityId, "bread", 1, 0);
    answer(t.bo, true);
    expect(std::abs(t.g.world().bonds().find(t.bo.entityId, t.ash.entityId)->affinity - liked) < 1e-9, "a second gift the same day doesn't warm him again");
    // Not what is worn; not from afar.
    t.place(t.bo, 1, 4.5);
    expect(contains(give(t.ash, t.bo.entityId, "herbs", 1, 0), "Go nearer"), "too far off");
    t.place(t.bo, 1, 12.5);
    // To a resident: taken at once.
    t.g.world().entity("ui")->position = {12, 8.5};
    t.g.world().entity("ui")->cellId = cellId(1);
    said = give(t.ash, "ui", "herbs", 1, 0);
    expect(contains(said, "You give") && held(t.g, "ui", "herbs") >= 1, "the innkeeper takes it: " + said);
    // Never past 64 kinds: Bo's pack full of kinds, a new kind is refused.
    for (const auto& item : someGoods(400))
        if (society.account(t.bo.entityId)->stock.size() < MaxGoodsKinds && item != "beeswax")
            society.create(t.bo.entityId, item, 1, "test");
    society.create(t.ash.entityId, "beeswax", 1, "test");
    said = give(t.ash, t.bo.entityId, "beeswax", 1, 0);
    expect(contains(said, "can't carry another kind") && society.account(t.bo.entityId)->stock.size() == MaxGoodsKinds, "kinds " + std::to_string(society.account(t.bo.entityId)->stock.size()) + " goods " + std::to_string(someGoods(400).size()) + " " +
           "his pack full of kinds: refused: " + said);
    // An enclosure: escrowed, then taken; a full pack refuses it and it stays in the letter.
    t.g.command(&t.bo, cmd({{"type", "chat"}, {"text", "\"I'm Bo.\""}, {"commandId", "b1"}}));
    t.run(1);
    const auto ashCash = society.account(t.ash.entityId)->cash;
    society.create(t.ash.entityId, "beeswax", 2, "test");
    const int ashWax = held(t.g, t.ash.entityId, "beeswax"), ashHerbs = held(t.g, t.ash.entityId, "herbs");
    auto r = t.letter(t.ash, "write", {{"to", "Bo"}, {"text", "Something for you."}, {"sign", "Ash"}});
    expect(r.ok, "a plain letter first: " + r.message);
    auto o = json::Value::object();
    o.add("type", "letter");
    o.add("verb", "write");
    o.add("to", "Bo");
    o.add("text", "A little wax, and some coin.");
    auto enclose = json::Value::object();
    enclose.add("item", "beeswax");
    enclose.add("quantity", 2);
    enclose.add("coins", 5);
    o.add("enclose", enclose);
    t.ash.events.clear();
    t.g.command(&t.ash, json::dump(o));
    expect(contains(t.ash.said(), "The courier takes your letter"), "with an enclosure: " + t.ash.said());
    expect(society.account(t.ash.entityId)->cash == ashCash - 2 - 5 && held(t.g, t.ash.entityId, "beeswax") == ashWax - 2, "the 5p and 2 cakes of wax left her (and 2p postage): cash " + std::to_string(society.account(t.ash.entityId)->cash) + " from " + std::to_string(ashCash) + ", wax " + std::to_string(held(t.g, t.ash.entityId, "beeswax")));
    expect(society.conserved(), "money conserved, held in the letter");
    enclose.set("item", "bread");
    enclose.set("quantity", 2);
    o.set("enclose", enclose);
    t.ash.events.clear();
    t.g.command(&t.ash, json::dump(o));
    expect(contains(t.ash.said(), "only something small"), "two loaves are too heavy for a letter: " + t.ash.said());
    t.hours(1.1);
    std::string enclosed;
    for (const auto& l : t.caseOf(t.bo).array("letters"))
        if (!l.string("enclosed").empty())
            enclosed = l.string("id");
    expect(!enclosed.empty(), "it arrives with something in it");
    t.letter(t.bo, "read", {{"id", enclosed}});
    const auto before = society.account(t.bo.entityId)->cash;
    r = t.letter(t.bo, "take", {{"id", enclosed}});
    expect(contains(r.message, "You take 5p") && contains(r.message, "no room for the rest") && held(t.g, t.bo.entityId, "beeswax") == 0,
           "his pack full of kinds: the pennies, and the wax stays in the letter: " + r.message);
    society.consume(t.bo.entityId, someGoods(1).front(), 1, "test");   // (Room for one more kind.)
    r = t.letter(t.bo, "take", {{"id", enclosed}});
    expect(held(t.g, t.bo.entityId, "beeswax") == 2 && society.account(t.bo.entityId)->cash == before + 5 && contains(r.message, "You take 2 ") &&
               contains(r.message, "It smells of"),
           "taken: " + r.message);
    // Unread 56 days, an enclosure goes back to its writer.
    enclose.set("item", "herbs");
    enclose.set("quantity", 1);
    enclose.set("coins", 3);
    o.set("enclose", enclose);
    t.g.command(&t.ash, json::dump(o));
    const auto ashBefore = society.account(t.ash.entityId)->cash;
    t.g.world().advanceCalendar(57);
    t.run(2);
    expect(society.account(t.ash.entityId)->cash == ashBefore + 3 && held(t.g, t.ash.entityId, "herbs") == ashHerbs, "unread for 56 days: back to her");
    expect(society.conserved(), "money conserved throughout");
    fs::remove_all(root);
}

// Grooming (Phase 3): consent, distance, the fifteen seconds, once a day, a day long; self-grooming at half for two
// hours; the scent scale; first impressions; the bond; the posted line.
void grooming()
{
    const auto root = fs::temp_directory_path() / ("ratw-grooming-" + std::to_string(::getpid()));
    const auto world = writeStrip(root);
    Town t(world, {});
    t.enter(t.ash, 1, "ash", "Ash", 1, 11.5);
    t.enter(t.bo, 2, "bo", "Bo", 1, 12.5);
    t.enter(t.cy, 3, "cy", "Cy", 1, 13.5);
    t.run(1);
    auto& w = t.g.world();
    const auto groom = [&](Client& c, const std::string& target) {
        c.events.clear();
        t.g.command(&c, cmd({{"type", "groom"}, {"target", target}}));
        return c.said();
    };
    const auto answer = [&](Client& c, bool yes) {
        c.events.clear();
        t.g.command(&c, cmd({{"type", "groomAnswer"}, {"accept", yes}}));
        return c.said();
    };
    auto said = groom(t.ash, t.bo.entityId);
    expect(contains(said, "You ask to groom") && t.bo.last("groomOffer"), "Ash asks to groom Bo: " + said);
    answer(t.bo, false);
    expect(w.groomedFactor(*w.entity(t.bo.entityId)) == 0, "he declines: nothing");
    groom(t.ash, t.bo.entityId);
    answer(t.bo, true);
    t.run(5);
    t.place(t.bo, 1, 14.5);                         // He walks off mid-way.
    t.ash.events.clear();
    t.run(2);
    expect(contains(t.ash.said(), "broken off") && w.groomedFactor(*w.entity(t.bo.entityId)) == 0, "moving apart breaks it off");
    t.place(t.bo, 1, 12.5);
    t.run(.5);
    groom(t.ash, t.bo.entityId);
    answer(t.bo, true);
    t.ash.events.clear();
    t.run(16);
    auto* bo = w.entity(t.bo.entityId);
    expect(w.groomedFactor(*bo) == 1 && std::abs((bo->groomedUntil - w.calendarDays()) * 24 - 24) < .2 && w.scentScale(*bo) == .6,
           "fifteen seconds, and Bo is Well-groomed for a day, his scent smelt from 0.6 as far");
    const auto* bond = w.bonds().find(t.bo.entityId, t.ash.entityId);
    expect(bond && bond->affinity >= 3 && bond->trust >= 2 && w.bonds().find(t.ash.entityId, t.bo.entityId), "a bond, both ways");
    bool posted = false;
    for (const auto& e : t.cy.events)
        posted = posted || contains(json::dump(e), "ruff, slow and careful");
    expect(posted, "the grooming posted as Ash's action, seen by Cy");
    expect(t.selfOf(t.bo).object("groomed").string("by") != "" && !t.selfOf(t.bo).object("groomed").boolean("half"), "Bo's status says so");
    said = groom(t.ash, t.cy.entityId);
    expect(contains(said, "groomed another today"), "once a game day for the groomer: " + said);
    // Self-grooming: half, two hours; never in place of Bo's.
    t.place(t.cy, 1, 13.5);
    groom(t.cy, "self");
    t.run(16);
    auto* cy = w.entity(t.cy.entityId);
    expect(w.groomedFactor(*cy) == .5 && w.scentScale(*cy) == .8 && std::abs((cy->groomedUntil - w.calendarDays()) * 24 - 2) < .2,
           "Cy grooms herself: half, two hours, 0.8 of her scent");
    expect(contains(groom(t.cy, "self"), "groomed yourself today"), "once a day");
    groom(t.bo, "self");
    t.run(16);
    expect(w.groomedFactor(*bo) == 1, "Bo grooming himself doesn't undo Ash's");
    cy->scentMaskedUntil = w.time() + 999;
    expect(w.scentScale(*cy) == 0, "masking oil still wins");
    // First impressions: a resident who hardly knows Bo warms a quarter faster to him in talk.
    const auto warmth = [&](const std::string& wolf) {
        const auto* b = w.bonds().find("um", wolf);
        return b ? b->affinity : 0.;
    };
    const double boBefore = warmth(t.bo.entityId), ashBefore = warmth(t.ash.entityId);
    w.recordEvent({"conversation", t.bo.entityId, "um", cellId(1), 0, 0, {}, 0, 0, {}});
    w.recordEvent({"conversation", t.ash.entityId, "um", cellId(1), 0, 0, {}, 0, 0, {}});
    expect(std::abs((warmth(t.bo.entityId) - boBefore) - 1.25 * (warmth(t.ash.entityId) - ashBefore)) < 1e-3 && warmth(t.ash.entityId) > ashBefore,
           "first impressions: the merchant warms to groomed Bo a quarter faster than to Ash: " + std::to_string(warmth(t.bo.entityId) - boBefore) + " against " + std::to_string(warmth(t.ash.entityId) - ashBefore));
    // A day on, it has worn off.
    w.advanceCalendar(1.05);
    t.run(1);
    expect(w.groomedFactor(*bo) == 0 && w.scentScale(*bo) == 1, "a day on, worn off");
    fs::remove_all(root);
}

// The maker's scent (Phase 4): a meal bought from the inn that cooks it smells of its keeper; food from a stall that only
// sells it smells of nobody; sold back, the record goes.
void makersScent()
{
    const auto root = fs::temp_directory_path() / ("ratw-makers-" + std::to_string(::getpid()));
    const auto world = writeStrip(root);
    Town t(world, {});
    t.enter(t.ash, 1, "ash", "Ash", 1, 12);
    t.g.world().setTimeOfDay(12);
    t.run(30);                                      // (The keepers at their posts.)
    auto& society = t.g.world().society();
    for (const auto* who : {"ui", "um"})
        if (auto* keeper = t.g.world().entity(who))
        {
            t.place(t.ash, 1, keeper->position.x + 1, keeper->position.y);
            society.create(society.tillOf(who), "meal", 6, "test");
            society.create(society.tillOf(who), "bread", 6, "test");
            if (std::string(who) == "um")
            {
                // (The stall sells food it doesn't make: its bread, as a reseller's, to Ash by hand here.)
                society.shift(society.tillOf(who), t.ash.entityId, "bread", 1, 0, "test");
                continue;
            }
            t.ash.events.clear();
            t.g.command(&t.ash, cmd({{"type", "trade"}, {"target", who}, {"item", std::string(who) == "ui" ? "meal" : "bread"}, {"quantity", 1}, {"buy", true}}));
            t.run(.5);
            if (std::string(who) == "ui")
                expect(contains(t.ash.said(), "Bought"), "bought a meal at the inn: " + t.ash.said());
        }
    const auto inventory = [&] {
        t.run(.5);
        std::map<std::string, std::string> out;
        for (const auto& i : t.ash.snapshots.back().array("inventory"))
            out[i.string("id")] = i.string("scent");
        return out;
    };
    auto held = inventory();
    expect(contains(held["meal"], "it smells of") && held["bread"].empty(), "scents " + std::to_string(t.g.world().entity(t.ash.entityId)->scents.size()) + " the inn's meal smells of its keeper (" + held["meal"] +
                                                                            "); the stall's bread of nobody (" + held["bread"] + ")");
    // Sold back, the record goes: a meal got later from nowhere in particular smells of nobody.
    auto* keeper = t.g.world().entity("ui");
    t.place(t.ash, 1, keeper->position.x + 1, keeper->position.y);
    t.g.command(&t.ash, cmd({{"type", "trade"}, {"target", "ui"}, {"item", "meal"}, {"quantity", 1}, {"buy", false}}));
    t.run(.5);
    expect(t.g.world().entity(t.ash.entityId)->scents.empty() || inventory()["meal"].empty(), "sold back: its record gone");
    society.create(t.ash.entityId, "meal", 1, "test");
    expect(inventory()["meal"].empty(), "a meal from elsewhere smells of nobody");
    fs::remove_all(root);
}

// Letters from residents (Phase 5): thanks the day after a deed from a fond resident and none from an indifferent one;
// a gift only from its own purse, at most 6p and a tenth; at most 3 a week to a player and one from any resident; a
// courier contract offered first to the player its poster trusts, reserved, taken on from the letter.
void residentLetters()
{
    const auto root = fs::temp_directory_path() / ("ratw-resident-letters-" + std::to_string(::getpid()));
    const auto world = writeStrip(root);
    Town t(world, {});
    t.enter(t.ash, 1, "ash", "Ash", 1, 11.5);
    t.enter(t.bo, 2, "bo", "Bo", 1, 12.5);
    t.run(1);
    auto& w = t.g.world();
    auto& society = w.society();
    const auto fond = [&](const char* resident, const std::string& player, double liking, double trust, double familiar = 10) {
        w.bonds().change(resident, player, {liking, trust, familiar, 0, 0}, w.calendarDays());
    };
    fond("ui", t.ash.entityId, 60, 30);
    const auto money = [&] {
        std::int64_t n = 0;
        for (const auto& [id, a] : society.state().accounts)
            n += a.cash;
        return n;
    };
    const auto start = money();
    w.recordEvent({"tended", t.ash.entityId, "ui", cellId(1), 0, 0, {}, 0, 0, {}});
    w.recordEvent({"tended", t.ash.entityId, "um", cellId(1), 0, 0, {}, 0, 0, {}});   // (The merchant cares nothing for her.)
    t.hours(2);
    expect(t.caseOf(t.ash).array("letters").empty(), "not the same day");
    t.hours(24);
    t.hours(2);
    auto letters = t.caseOf(t.ash).array("letters");
    expect(letters.size() == 1 && contains(letters[0].string("by"), "inn"), "the next day, thanks from the fond innkeeper only, by its role: " +
                                                                              (letters.empty() ? std::string("none") : letters[0].string("by")));
    t.letter(t.ash, "read", {{"id", letters[0].string("id")}});
    letters = t.caseOf(t.ash).array("letters");
    expect(contains(letters[0].string("text"), "tending me when I was down") && !letters[0].boolean("canReply"), "it thanks her for tending it; no reply by post");
    (void)start;
    expect(society.conserved(), "money only moved");
    if (!letters[0].string("enclosed").empty())
        expect(contains(letters[0].string("enclosed"), "p") && society.account("ui")->cash >= 0, "a few pennies from its own purse");
    // Three a week at most, one from any resident.
    for (const char* r : {"um", "sm", "si", "rm"})
        fond(r, t.bo.entityId, 60, 30);
    fond("ui", t.bo.entityId, 60, 30);
    for (const char* r : {"ui", "um", "sm", "si", "rm"})
        w.recordEvent({"tended", t.bo.entityId, r, cellId(1), 0, 0, {}, 0, 0, {}});
    w.recordEvent({"tended", t.bo.entityId, "ui", cellId(1), 0, 0, {}, 0, 0, {}});
    t.hours(26);
    t.hours(24);
    expect(t.caseOf(t.bo).array("letters").size() == 3, "three residents' letters a week at most: " + std::to_string(t.caseOf(t.bo).array("letters").size()));
    // A courier contract offered to the one its poster trusts.
    fond("ui", t.ash.entityId, 0, 20);
    t.g.world().advanceCalendar(8);                 // (A new week.)
    t.run(1);
    const auto& posted = w.postContract("courier", "ui", "upper_accord", "rm", 6, 7, "a letter for Ridgemere");
    const auto contract = posted.id;
    t.hours(1.5);                                   // (Offered at the next hourly pass; the courier's hour after that.)
    t.hours(1.5);
    std::string offer;
    for (const auto& l : t.caseOf(t.ash).array("letters"))
        if (!l.string("work").empty())
            offer = l.string("id");
    std::string why;
    for (const auto& c : w.roads().contracts)
        if (c.id == contract)
            why = c.status + " offered " + c.offeredTo + " created " + std::to_string(c.created) + " now " + std::to_string(w.calendarDays());
    expect(!offer.empty(), "the innkeeper offers Ash the letter to carry, by letter: " + why);
    auto r = t.letter(t.ash, "read", {{"id", offer}});
    std::string text;
    for (const auto& l : t.caseOf(t.ash).array("letters"))
        if (l.string("id") == offer)
            text = l.string("text");
    expect(contains(text, "Ridgemere") && contains(text, "6 pennies"), "the letter names the place and the pennies: " + text);
    t.place(t.bo, 1, 12.5);
    expect(!w.takeContract(t.bo.entityId, contract).ok, "reserved: Bo can't take it from the board");
    r = t.letter(t.ash, "takeWork", {{"id", offer}});
    bool taken = false;
    for (const auto& c : w.roads().contracts)
        taken = taken || (c.id == contract && c.taker == t.ash.entityId);
    expect(taken, "Ash takes it on from the letter: " + r.message);
    fs::remove_all(root);
}

// Occasions and shared meals (Phase 6): a marriage's wedding on the next Restday, an invitation by letter to a liked
// player, answered; the invited present at the hour stand witness, the hosts warm to them; a funeral the next morning;
// Fed alone for 2 hours, shared for 4, the two closer.
void occasionsAndMeals()
{
    const auto root = fs::temp_directory_path() / ("ratw-occasions-" + std::to_string(::getpid()));
    const auto world = writeStrip(root);
    Town t(world, {});
    t.enter(t.ash, 1, "ash", "Ash", 1, 11.5);
    t.enter(t.bo, 2, "bo", "Bo", 1, 12.5);
    t.run(1);
    auto& w = t.g.world();
    w.bonds().change("u1", t.ash.entityId, {60, 30, 10, 0, 0}, w.calendarDays());
    w.recordEvent({"marriage", "u1", "u2", cellId(0), 0, 0, {}, 0, 0, "u2 moves in"});
    expect(w.occasions().size() == 1 && w.occasions()[0].kind == "wedding" && calendar::weekdayOf(w.occasions()[0].start) == calendar::Restday,
           "a marriage: a wedding, on a Restday");
    const auto wedding = w.occasions()[0];
    expect(std::abs((wedding.start - std::floor(wedding.start)) * 24 - 11) < .01, "at 11:00");
    // A day before, the hosts invite the players they like: Ash, not Bo.
    if (wedding.start - w.calendarDays() > 1.2)
        w.advanceCalendar(wedding.start - w.calendarDays() - 1.1);
    t.hours(1.5);
    t.hours(1.5);
    std::string invite;
    for (const auto& l : t.caseOf(t.ash).array("letters"))
        if (l.boolean("invitation"))
            invite = l.string("id");
    expect(!invite.empty() && t.caseOf(t.bo).array("letters").empty(), "Ash is invited by letter; Bo isn't");
    t.letter(t.ash, "read", {{"id", invite}});
    auto r = t.letter(t.ash, "answer", {{"id", invite}, {"yes", true}});
    expect(contains(r.message, "You'll be there") && w.occasions()[0].coming.count(t.ash.entityId), "she answers that she'll come");
    // At the hour, at the place: she stands witness; Bo, there but uninvited, doesn't.
    w.advanceCalendar(wedding.start - w.calendarDays() + .2 / 24);
    t.place(t.ash, 0, 1, 1);
    auto* ash = w.entity(t.ash.entityId);
    ash->cellId = wedding.cell;
    ash->position = {wedding.x + .5, wedding.y + 1};
    auto* bo = w.entity(t.bo.entityId);
    bo->cellId = wedding.cell;
    bo->position = {wedding.x - .5, wedding.y + 1};
    const double liked = w.bonds().find("u1", t.ash.entityId)->affinity;
    t.run(8);
    expect(!w.occasions().empty() && w.occasions()[0].witnesses.count(t.ash.entityId) && !w.occasions()[0].witnesses.count(t.bo.entityId),
           "Ash stands witness; Bo, uninvited, doesn't");
    expect(w.bonds().find("u1", t.ash.entityId)->affinity > liked, "the hosts warm to her: " + std::to_string(liked) + " to " + std::to_string(w.bonds().find("u1", t.ash.entityId)->affinity));
    // A death in a family: the funeral the next morning at 10:00.
    w.recordEvent({"mourning", "u3", "u2", cellId(0), 0, 0, {}, 0, 0, "family"});
    bool funeral = false;
    for (const auto& o : w.occasions())
        funeral = funeral || (o.kind == "funeral" && std::abs((o.start - std::floor(o.start)) * 24 - 10) < .01 && std::floor(o.start) == std::floor(w.calendarDays()) + 1);
    expect(funeral, "a death: a funeral the next morning at 10:00");
    // Meals: Fed alone for 2 hours; shared, 4 for both, and closer.
    w.society().create(t.ash.entityId, "meal", 2, "test");
    w.society().create(t.bo.entityId, "meal", 2, "test");
    t.place(t.ash, 1, 11.5);
    t.place(t.bo, 1, 15.5);
    t.run(.5);                                      // (The world's cell index catches up with them.)
    expect(w.eat(t.ash.entityId).ok && std::abs((ash->fedUntil - w.calendarDays()) * 24 - 2) < .05, "Ash eats alone: fed 2 hours");
    t.place(t.bo, 1, 12.5);
    w.tick(.05);
    const auto* before = w.bonds().find(t.bo.entityId, t.ash.entityId);
    const double familiar = before ? before->familiarity : 0;
    expect(w.eat(t.bo.entityId).ok && std::abs((bo->fedUntil - w.calendarDays()) * 24 - 4) < .05 && std::abs((ash->fedUntil - w.calendarDays()) * 24 - 4) < .05,
           "Bo eats beside her: a shared meal, 4 hours for both");
    expect(w.bonds().find(t.bo.entityId, t.ash.entityId)->familiarity > familiar, "and they know each other a little better");
    fs::remove_all(root);
}

// Lending, a letter carried by a friend, and pacts (Phase 7).
void favours()
{
    const auto root = fs::temp_directory_path() / ("ratw-favours-" + std::to_string(::getpid()));
    const auto world = writeStrip(root);
    Town t(world, {});
    t.enter(t.ash, 1, "ash", "Ash", 1, 11.5);
    t.enter(t.bo, 2, "bo", "Bo", 1, 12.5);
    t.enter(t.cy, 3, "cy", "Cy", 1, 13.5);
    t.run(1);
    auto& w = t.g.world();
    auto& society = w.society();
    for (auto* c : {&t.ash, &t.bo, &t.cy})
    {
        const auto name = std::string(c == &t.ash ? "Ash" : c == &t.bo ? "Bo" : "Cy");
        t.g.command(c, cmd({{"type", "chat"}, {"text", "\"I'm " + name + ".\""}, {"commandId", name}}));
        t.run(.5);
    }
    const auto said = [&](Client& c, const std::string& json) {
        c.events.clear();
        t.g.command(&c, json);
        return c.said();
    };
    society.create(t.ash.entityId, "beeswax", 3, "test");
    // A loan: offered, accepted; his to use, not to give or sell; handed back.
    auto r = said(t.ash, cmd({{"type", "lend"}, {"target", t.bo.entityId}, {"item", "beeswax"}, {"quantity", 1}, {"days", 2}}));
    expect(contains(r, "You offer to lend"), "Ash offers Bo a loan: " + r);
    r = said(t.bo, cmd({{"type", "lendAnswer"}, {"accept", true}}));
    expect(contains(r, "You borrow") && held(t.g, t.bo.entityId, "beeswax") == 1, "he borrows it: " + r);
    r = said(t.bo, cmd({{"type", "give"}, {"target", t.cy.entityId}, {"item", "beeswax"}, {"quantity", 1}}));
    expect(contains(r, "haven't that to give"), "he can't give it away: " + r);
    t.run(1);
    auto loans = t.selfOf(t.bo).array("loans");
    expect(loans.size() == 1 && loans[0].boolean("borrowed"), "his belongings show the loan");
    r = said(t.bo, cmd({{"type", "return"}, {"loan", loans[0].string("id")}}));
    expect(contains(r, "You hand back") && held(t.g, t.bo.entityId, "beeswax") == 0, "handed back: " + r);
    // Due: a courier carries it back, a penny from him.
    said(t.ash, cmd({{"type", "lend"}, {"target", t.bo.entityId}, {"item", "beeswax"}, {"quantity", 1}, {"days", 1}}));
    said(t.bo, cmd({{"type", "lendAnswer"}, {"accept", true}}));
    const auto boCash = society.account(t.bo.entityId)->cash;
    const int ashWax = held(t.g, t.ash.entityId, "beeswax");
    t.hours(25);
    expect(held(t.g, t.bo.entityId, "beeswax") == 0 && held(t.g, t.ash.entityId, "beeswax") == ashWax + 1 && society.account(t.bo.entityId)->cash == boCash - 1,
           "at the day, a courier takes it back (1p from him)");
    // Gone: a debt, and trust lost.
    said(t.ash, cmd({{"type", "lend"}, {"target", t.bo.entityId}, {"item", "beeswax"}, {"quantity", 1}, {"days", 1}}));
    said(t.bo, cmd({{"type", "lendAnswer"}, {"accept", true}}));
    society.consume(t.bo.entityId, "beeswax", 1, "test");   // (Used up.)
    const auto* before = w.bonds().find(t.ash.entityId, t.bo.entityId);
    const double trusted = before ? before->trust : 0;
    t.hours(25);
    const auto* after = w.bonds().find(t.ash.entityId, t.bo.entityId);
    expect(after && after->owed > 0 && after->trust < trusted - 8, "gone: he owes her for it, and she trusts him less: owed " + std::to_string(after ? after->owed : -1) + " trust " + std::to_string(after ? after->trust : -1) + " from " + std::to_string(trusted));
    expect(society.conserved(), "money conserved");
    // A letter carried by a friend: Ash's to Cy, by Bo, who is paid when he hands it over.
    t.place(t.bo, 1, 2.5);
    t.place(t.cy, 0, 8.5);
    const auto boBefore = society.account(t.bo.entityId)->cash;
    r = said(t.ash, cmd({{"type", "letter"}, {"verb", "write"}, {"to", "Cy"}, {"text", "Bo will bring you this."}, {"sign", "Ash"}, {"by", "Bo"}}));
    expect(contains(r, "will carry your letter to Cy"), "Bo will carry it: " + r);
    t.run(3);
    expect(t.caseOf(t.cy).array("letters").empty(), "not until he hands it over");
    t.place(t.bo, 0, 9.5);
    t.cy.events.clear();
    t.run(3);
    expect(contains(t.cy.said(), "hands you a letter") && t.caseOf(t.cy).array("letters").size() == 1, "Bo hands it to Cy");
    expect(society.account(t.bo.entityId)->cash > boBefore, "and is paid the fee");
    // A pact: set down, sealed by both, witnessed.
    t.place(t.bo, 1, 12.5);
    t.place(t.cy, 1, 13.5);
    r = said(t.ash, cmd({{"type", "pact"}, {"verb", "write"}, {"with", "Bo"}, {"text", "We share the hunting at the ford, and neither sells the other out."}}));
    expect(contains(r, "You set down the pact"), "Ash sets down a pact with Bo: " + r);
    std::string mine;
    for (const auto& l : t.caseOf(t.bo).array("letters"))
        if (l.boolean("pact") || contains(json::dump(l), "pact"))
            mine = l.string("id");
    expect(!mine.empty(), "Bo holds a copy");
    t.letter(t.bo, "read", {{"id", mine}});
    w.takeEvents();
    r = said(t.bo, cmd({{"type", "pact"}, {"verb", "seal"}, {"id", mine}}));
    bool sealed = false;
    for (const auto& e : w.takeEvents())
        sealed = sealed || e.kind == "pact sealed";
    expect(contains(r, "You seal the pact") && sealed, "Bo seals it: sealed by both");
    r = said(t.bo, cmd({{"type", "pact"}, {"verb", "witness"}, {"id", mine}, {"by", "Cy"}}));
    expect(contains(r, "witness"), "Bo asks Cy to witness: " + r);
    std::string theirs;
    for (const auto& l : t.caseOf(t.cy).array("letters"))
        if (l.boolean("pact"))
            theirs = l.string("id");
    t.letter(t.cy, "read", {{"id", theirs}});
    r = said(t.cy, cmd({{"type", "pact"}, {"verb", "seal"}, {"id", theirs}}));
    int seals = 0;
    for (const auto& l : t.caseOf(t.ash).array("letters"))
        if (l.boolean("pact"))
            seals = int(l.array("seals").size());
    expect(contains(r, "as a witness") && seals == 3, "Cy seals as witness; Ash's copy shows three seals");
    fs::remove_all(root);
}
} // namespace

int main()
{
    try
    {
        theCourier();
        theStore();
        lettersInTheGame();
        giving();
        grooming();
        makersScent();
        residentLetters();
        occasionsAndMeals();
        favours();
    }
    catch (const std::exception& error)
    {
        std::cerr << "letters_tests failed after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "letters_tests passed (" << checks << " checks)\n";
    return 0;
}
