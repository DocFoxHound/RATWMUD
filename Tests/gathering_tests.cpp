// Gathering places (Docs/Design/54-gathering-places.md). Phase 1, taverns: the common room heals 1.25 alone, +10% for
// each other active player up to +30%, residents not counted, +30% with a performer; a full rest only in a bed the wolf
// has a right to; performing starts in a common room, one a room, and lapses when quiet; a wolf away at an inn builds
// rested practice half again as fast.
#include "RatwCalendar.h"
#include "RatwGame.h"
#include "RatwItems.h"
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

void taverns()
{
    const auto root = fs::temp_directory_path() / ("ratw-taverns-" + std::to_string(::getpid()));
    const auto world = writeStrip(root);
    Town t(world, {}, 1);                           // (A performer may be quiet one whole second, here.)
    Client di;
    t.enter(t.ash, 1, "ash", "Ash", 1, 11.5);       // Upper Accord's common room (its inn's cell).
    t.enter(t.bo, 2, "bo", "Bo", 1, 12.5);
    t.enter(t.cy, 3, "cy", "Cy", 1, 13.5);
    t.enter(di, 4, "di", "Di", 0, 8.5);            // Out in the street.
    t.run(6);
    auto& w = t.g.world();
    auto* ash = w.entity(t.ash.entityId);
    ash->posture = "sitting";
    // Alone (Bo and Cy idle for now: they haven't done anything since entering... they have: entering counts).
    const auto rate = [&] {
        t.run(6);
        return w.restRate(*ash);
    };
    expect(std::abs(rate() - 1.25 * 1.2) < 1e-9, "two others active in the room: 1.25 x 1.2 = " + std::to_string(w.restRate(*ash)));
    // Di comes in and does something: three others, the most.
    t.place(di, 1, 10.5);
    t.g.command(&di, cmd({{"type", "chat"}, {"text", "\"Evening.\""}, {"commandId", "d1"}}));
    expect(std::abs(rate() - 1.25 * 1.3) < 1e-9, "three others: 1.625, better than a bed");
    expect(t.selfOf(t.ash).boolean("commonRoom") && std::abs(t.selfOf(t.ash).number("company") - 1.3) < 1e-9, "her status shows the company");
    // Out in the street: still anywhere, 0.75.
    t.place(t.ash, 0, 8.5);
    expect(std::abs(rate() - .75) < 1e-9, "out of the common room: 0.75");
    t.place(t.ash, 1, 11.5);
    // Residents don't count: the innkeeper and neighbours are in the room's cell, but alone among players it is 1.25.
    for (auto* c : {&t.bo, &t.cy, &di})
        t.place(*c, 0, 4.5);
    expect(std::abs(rate() - 1.25) < 1e-9, "only residents about: 1.25");
    // A performer: +30% to all resting there.
    t.place(t.bo, 1, 12.5);
    t.bo.events.clear();
    t.g.command(&t.bo, cmd({{"type", "perform"}, {"verb", "start"}, {"kind", "sing"}}));
    expect(contains(t.bo.said(), "You begin to sing"), "Bo sings: " + t.bo.said());
    expect(std::abs(rate() - 1.25 * 1.3) < 1e-9, "a performer: the full +30%");
    t.cy.events.clear();
    t.place(t.cy, 1, 13.5);
    t.g.command(&t.cy, cmd({{"type", "perform"}, {"verb", "start"}, {"kind", "tale"}}));
    expect(contains(t.cy.said(), "already performing"), "one performer a room");
    t.place(t.cy, 0, 8.5);
    t.g.command(&t.cy, cmd({{"type", "perform"}, {"verb", "start"}, {"kind", "tale"}}));
    expect(contains(t.cy.said(), "common room"), "not out in the street");
    // Quiet two (here, shortened) seconds: it trails off.
    std::this_thread::sleep_for(std::chrono::milliseconds(2600));
    t.bo.events.clear();
    t.run(11);
    expect(contains(t.bo.said(), "trails off"), "quiet too long: it trails off: [" + t.bo.said() + "] performing " + std::to_string(t.selfOf(t.bo).has("performing")));
    // A full rest needs a bed the wolf has a right to: on another's bed, a partial rest (0.75).
    auto* c = w.cell(cellId(1));
    const_cast<Tile*>(c->tile(11, 8))->glyph = 'b';
    ash->posture = "lying";
    ash->position = {11.5, 8.5};
    expect(w.inBed(*ash) && !w.bedIsTheirs(*ash) && std::abs(w.restRate(*ash) - 1.25 * w.companyFactor(*ash)) < 1e-9,
           "a bed in the common room not hers: the room's rest, no full rest");
    World bare;                                     // (A world with no game to say: any bed.)
    expect(bare.bedIsTheirs(*ash) == bare.inBed(*ash), "without the game, any bed counts");
    fs::remove_all(root);
}

void restedAtAnInn()
{
    // Ash leaves the world in the common room, Bo in the street; a game day away; Ash comes back with more rested.
    const auto root = fs::temp_directory_path() / ("ratw-rested-" + std::to_string(::getpid()));
    const auto world = writeStrip(root);
    Town t(world, {});
    t.enter(t.ash, 1, "ash", "Ash", 1, 11.5);
    t.enter(t.bo, 2, "bo", "Bo", 0, 8.5);
    t.run(6);
    t.g.disconnect(&t.ash);
    t.g.disconnect(&t.bo);
    t.g.world().advanceCalendar(1);
    t.ash = Client{};
    t.bo = Client{};
    t.enter(t.ash, 5, "ash", "Ash", 1, 11.5);
    t.enter(t.bo, 6, "bo", "Bo", 0, 8.5);
    const double ashRested = t.g.world().entity(t.ash.entityId)->practice->rested, boRested = t.g.world().entity(t.bo.entityId)->practice->rested;
    const double expected = .5 * practice::rules().restedPerDay * calendar::SecondsPerDay / 86400;
    expect(std::abs((ashRested - boRested) - expected) < 1e-6, "away a day at the inn: half again (" + std::to_string(ashRested) + " against " +
                                                                  std::to_string(boRested) + ")");
    fs::remove_all(root);
}

// Notice boards (Phase 2): placed the same way every time and off the stalls; the town's contracts listed and taken; a
// notice pinned (a penny to the board's own town), scented, masked, limited, expired; residents answer an item sought
// and an apprenticeship.
void boards()
{
    const auto root = fs::temp_directory_path() / ("ratw-boards-" + std::to_string(::getpid()));
    const auto world = writeStrip(root);
    Town t(world, {});
    t.enter(t.ash, 1, "ash", "Ash", 5, 8.5);        // Ser Ferro's market.
    t.enter(t.bo, 2, "bo", "Bo", 5, 9.5);
    t.run(1);
    auto& w = t.g.world();
    auto& society = w.society();
    const auto* spot = w.boardSpot("ser_ferro");
    expect(spot && spot->cell == cellId(5), "Ser Ferro has a board on its square");
    const auto where = *spot;
    expect(w.boardSpot("ser_ferro")->x == where.x && w.boardSpot("ser_ferro")->y == where.y, "the same place every time");
    const auto atBoard = [&](Client& c) { t.place(c, 5, where.x + .5, where.y); };
    const auto board = [&](Client& c, std::initializer_list<std::pair<const char*, json::Value>> more) {
        auto o = json::Value::object();
        o.add("type", "board");
        for (const auto& [k, v] : more)
            o.add(k, v);
        c.events.clear();
        t.g.command(&c, json::dump(o));
        return c.said();
    };
    t.place(t.ash, 4, 2.5);
    expect(contains(board(t.ash, {{"verb", "read"}}), "Go to the notice board"), "not from across town");
    atBoard(t.ash);
    atBoard(t.bo);
    t.run(.5);
    expect(t.selfOf(t.ash).boolean("nearBoard"), "beside it: Read the board");
    // The work side: the town's contracts, taken from the board.
    const auto& posted = w.postContract("supply", "treasury", "ser_ferro", "ser_ferro", 9, 7, "sacks of grain for the granary");
    const auto contract = posted.id;
    board(t.ash, {{"verb", "read"}});
    const auto* view = t.ash.last("board");
    bool listed = false;
    for (const auto& k : view ? view->array("work") : std::vector<json::Value>{})
        listed = listed || k.string("id") == contract;
    expect(listed, "the town's contract is on the board");
    board(t.ash, {{"verb", "take"}, {"id", contract}});
    bool taken = false;
    for (const auto& k : w.roads().contracts)
        taken = taken || (k.id == contract && k.taker == t.ash.entityId);
    expect(taken, "taken on from the board");
    // A notice: a penny to Ser Ferro's own stores, not the capital.
    const auto town = society.account(society.treasuryOf("ser_ferro"))->cash, capital = society.account("treasury")->cash;
    auto said = board(t.ash, {{"verb", "post"}, {"kind", "seeking"}, {"whatType", "item"}, {"whatValue", "honey"}, {"text", "Seeking good honey."}});
    expect(contains(said, "You pin your notice"), "Ash pins a notice: " + said);
    expect(society.account(society.treasuryOf("ser_ferro"))->cash == town + 1 && society.account("treasury")->cash == capital, "a penny to the town");
    board(t.bo, {{"verb", "read"}});
    auto notices = t.bo.last("board")->array("notices");
    expect(notices.size() == 1 && notices[0].string("scent") == "A wolf's scent you don't know." && contains(notices[0].string("what"), "honey"),
           "Bo reads it: seeking honey, a scent he doesn't know: " + (notices.empty() ? std::string("none") : json::dump(notices[0])));
    // A resident answers: the stall that has honey.
    society.create(society.tillOf("sm"), "honey", 4, "test");
    board(t.ash, {{"verb", "post"}, {"kind", "seeking"}, {"whatType", "item"}, {"whatValue", "honey"}, {"text", "Still seeking honey."}});
    board(t.bo, {{"verb", "read"}});
    std::string answer;
    for (const auto& n : t.bo.last("board")->array("notices"))
        if (!n.string("answer").empty())
            answer = n.string("answer");
    expect(contains(answer, "has honey"), "the stall answers: " + answer);
    // An apprenticeship: a master of the trade who trusts her.
    w.bonds().change("si", t.ash.entityId, {20, 40, 10, 0, 0}, w.calendarDays());
    said = board(t.ash, {{"verb", "post"}, {"kind", "seeking"}, {"whatType", "apprenticeship"}, {"whatValue", "inn"}, {"text", "Willing to learn."}});
    board(t.bo, {{"verb", "read"}});
    answer.clear();
    for (const auto& n : t.bo.last("board")->array("notices"))
        if (contains(n.string("answer"), "apprentice"))
            answer = n.string("answer");
    expect(!answer.empty(), "the innkeeper would take her on: " + answer);
    // Three a writer; masked, no scent.
    said = board(t.ash, {{"verb", "post"}, {"kind", "other"}, {"text", "A fourth."}});
    expect(contains(said, "three notices up here"), "three a writer: " + said);
    w.entity(t.bo.entityId)->scentMaskedUntil = w.time() + 999;
    board(t.bo, {{"verb", "post"}, {"kind", "event"}, {"text", "Something at the ford, midnight."}});
    board(t.ash, {{"verb", "read"}});
    bool masked = false;
    for (const auto& n : t.ash.last("board")->array("notices"))
        masked = masked || (contains(n.string("text"), "ford") && contains(n.string("scent"), "no scent at all"));
    expect(masked, "a masked writer leaves no scent");
    // Seven days on, they come down.
    w.advanceCalendar(7.1);
    t.run(2);
    board(t.ash, {{"verb", "read"}});
    expect(t.ash.last("board")->array("notices").empty(), "seven days on, the board is bare");
    expect(society.conserved(), "money conserved");
    fs::remove_all(root);
}

// Renting by individuals (Phase 3): a bed at an inn by the night (to the inn's till), a full rest only in it, away at the
// inn half again the rested practice; the whole upstairs not while a bed is let; the night over at noon; a lodger's spare
// bed in a home (to the head's purse), refused by a head who dislikes the wolf; the chest's goods home at the end; held
// for a story, refused and given notice with the rest returned; money conserved.
void lodgings()
{
    const auto root = fs::temp_directory_path() / ("ratw-lodgings-" + std::to_string(::getpid()));
    const auto world = writeStrip(root, true);
    Town t(world, {});
    t.enter(t.ash, 1, "ash", "Ash", 1, 11.5);
    t.enter(t.bo, 2, "bo", "Bo", 1, 12.5);
    t.run(6);                                       // (The estates refresh: the upstairs is to let.)
    auto& w = t.g.world();
    auto& society = w.society();
    const auto lodge = [&](Client& c, std::initializer_list<std::pair<const char*, json::Value>> more) {
        auto o = json::Value::object();
        o.add("type", "lodge");
        for (const auto& [k, v] : more)
            o.add(k, v);
        c.events.clear();
        t.g.command(&c, json::dump(o));
        return c.said();
    };
    const auto upstairs = [&](Client& c, double x, double y) {
        auto* e = w.entity(c.entityId);
        e->cellId = "c_up";
        e->position = {x, y};
    };
    upstairs(t.ash, 2.5, 1.5);
    upstairs(t.bo, 6.5, 3.5);
    t.run(1);
    auto offers = t.selfOf(t.ash).object("lodging").array("offers");
    expect(offers.size() == 3, "upstairs at the inn: a bed for a night or a week, or the whole upstairs");
    const auto start = society.account(t.ash.entityId)->cash;
    auto said = lodge(t.ash, {{"verb", "bed"}, {"period", "night"}});
    expect(contains(said, "You take a bed for the night (2p)") && society.account(t.ash.entityId)->cash == start - 2, "Ash takes a bed for the night: " + said);
    said = lodge(t.bo, {{"verb", "night"}});
    expect(contains(said, "A bed up here is let"), "the whole upstairs can't be hired while a bed is let: " + said);
    // Her bed rests her fully; Bo on it doesn't.
    auto* ash = w.entity(t.ash.entityId);
    ash->posture = "lying";
    ash->position = {1.5, 1.5};
    expect(w.bedIsTheirs(*ash) && std::abs(w.restRate(*ash) - 1.5) < 1e-9, "her bed: a full rest, 1.5");
    auto* bo = w.entity(t.bo.entityId);
    bo->posture = "lying";
    bo->position = {1.5, 1.5};
    expect(!w.bedIsTheirs(*bo), "not Bo's");
    bo->position = {6.5, 3.5};
    // Her chest: a meal in, then home at the end.
    society.create(t.ash.entityId, "meal", 2, "test");
    const int meals = Society::stock(*society.account(t.ash.entityId), "meal");
    said = lodge(t.ash, {{"verb", "put"}, {"item", "meal"}, {"quantity", 1}});
    expect(contains(said, "You put it in your chest") && Society::stock(*society.account(t.ash.entityId), "meal") == meals - 1, "a meal in her chest");
    // She leaves the world in her bed; a day away: a full rest, and half again the rested practice.
    t.g.disconnect(&t.ash);
    w.advanceCalendar(.4);
    t.ash = Client{};
    t.enter(t.ash, 5, "ash", "Ash", 1, 11.5);
    ash = w.entity(t.ash.entityId);
    expect(std::abs(ash->fullRestDay - w.calendarDays()) < .01, "back from a night in her own bed: fully rested");
    // The night ends at noon; the meal comes home.
    w.advanceCalendar(1);
    t.run(6);
    expect(!t.selfOf(t.ash).object("lodging").has("mine") && Society::stock(*society.account(t.ash.entityId), "meal") == meals,
           "the night over, the lodging ends and the meal comes back to her");
    // A lodger's bed: a spare bed in a home, a week, to the head's own purse.
    auto* home = w.cell(cellId(0));
    const_cast<Tile*>(home->tile(13, 2))->glyph = 'b';
    t.place(t.ash, 0, w.entity("u1")->position.x + 1, w.entity("u1")->position.y);
    const auto head = society.account("u1")->cash;
    said = lodge(t.ash, {{"verb", "ask"}, {"target", "u1"}});
    expect(contains(said, "You take the spare bed, by the week (6p)") && society.account("u1")->cash == head + 6, "u1 lets its spare bed: " + said);
    w.bonds().change("u2", t.bo.entityId, {-10, -10, 5, 0, 0}, w.calendarDays());
    t.place(t.bo, 0, w.entity("u2")->position.x + 1, w.entity("u2")->position.y);
    said = lodge(t.bo, {{"verb", "ask"}, {"target", "u2"}});
    expect(contains(said, "won't have you under their roof"), "a head who dislikes Bo won't: " + said);
    // Held for a story: a week's notice, then the rest returned.
    const auto before = society.account(t.ash.entityId)->cash;
    t.g.holdForStory(cellId(0), 30, "the watch searches the house");
    w.advanceCalendar(7.1);
    t.run(6);
    expect(!t.selfOf(t.ash).object("lodging").has("mine"), "a week's notice, and it ended");
    expect(society.account(t.ash.entityId)->cash >= before, "with the rest of the rent returned (or none left to return)");
    said = lodge(t.ash, {{"verb", "ask"}, {"target", "u1"}});
    expect(contains(said, "promised it to someone"), "held: no new lodging there: " + said);
    expect(society.conserved(), "money conserved");
    fs::remove_all(root);
}

// Phase 4, market stalls: rented on Marketday mornings at a free spot (3p to the town), the spot left out of the
// merchants' plan; wares off the purse, so not given or eaten; a sale face to face only; cleared at 2, goods home; foul
// weather returns the fee.
void stalls()
{
    const auto root = fs::temp_directory_path() / ("ratw-stalls-" + std::to_string(::getpid()));
    const auto world = writeStrip(root, false, true);
    Town t(world, {});
    auto& w = t.g.world();
    auto& society = w.society();
    const auto community = w.lawTown(cellId(1));
    const auto spots = w.stallSpots(community);
    expect(spots.size() >= 6, "Upper Accord's square has its built stalls' spots: " + std::to_string(spots.size()));
    const auto spot = spots.front();
    t.enter(t.ash, 1, "ash", "Ash", 1, spot.x);
    t.enter(t.bo, 2, "bo", "Bo", 1, 12.5);
    t.place(t.ash, 1, spot.x, spot.y);
    const auto stall = [&](Client& c, std::initializer_list<std::pair<const char*, json::Value>> more) {
        auto o = json::Value::object();
        o.add("type", "stall");
        for (const auto& [k, v] : more)
            o.add(k, v);
        c.events.clear();
        t.g.command(&c, json::dump(o));
        return c.said();
    };
    // Not Marketday (or not its morning): no stall.
    const auto toHour = [&](int weekday, double hour) {
        for (int i = 0; i < 24 * 8 && !(calendar::weekdayOf(w.calendarDays()) == weekday &&
                                         std::abs((w.calendarDays() - std::floor(w.calendarDays())) * 24 - hour) < .5); ++i)
            w.advanceCalendar(1. / 24);
        t.run(1.5);
    };
    toHour(calendar::Marketday == 0 ? 1 : 0, 9);
    auto said = stall(t.ash, {{"verb", "rent"}});
    expect(contains(said, "Marketday mornings"), "no stall on another day: " + said);
    toHour(calendar::Marketday, 8);
    const_cast<Cell*>(w.cell(spot.cell))->weather = Weather::Clear;
    expect(t.selfOf(t.ash).object("stall").has("offer"), "at a free spot on Marketday morning: RENT THIS STALL");
    const auto treasury = society.treasuryOf(community);
    const auto town = society.account(treasury)->cash, purse = society.account(t.ash.entityId)->cash;
    said = stall(t.ash, {{"verb", "rent"}});
    expect(contains(said, "You rent the stall") && society.account(t.ash.entityId)->cash == purse - 3 &&
               society.account(treasury)->cash == town + 3, "Ash rents it, 3p to the town: " + said);
    const auto key = World::stallKey(spot);
    bool left = true;
    for (const auto& s : w.dayPlan(community).stalls)
        left &= World::stallKey(s) != key;
    expect(left, "her spot is left out of the merchants' plan");
    t.run(30);
    const auto* um = w.entity("um");
    expect(!(um->cellId == spot.cell && std::hypot(um->position.x - spot.x, um->position.y - spot.y) < .5), "the merchant isn't sent there");
    t.place(t.bo, 1, spot.x, spot.y);
    said = stall(t.bo, {{"verb", "rent"}});
    expect(contains(said, "Someone has this stall"), "one wolf a spot: " + said);
    // Her wares: off the purse, so not given away or eaten.
    const int had = Society::stock(*society.account(t.ash.entityId), "meal");
    if (had > 0)
        society.shift(t.ash.entityId, "treasury", "meal", had, 0, "test");
    society.create(t.ash.entityId, "meal", 3, "test");
    said = stall(t.ash, {{"verb", "list"}, {"item", "meal"}, {"quantity", 3}, {"price", 5}});
    expect(contains(said, "You lay out 3 prepared meal at 5p") && Society::stock(*society.account(t.ash.entityId), "meal") == 0, "three meals at 5p: " + said);
    said = stall(t.ash, {{"verb", "list"}, {"item", "meal"}, {"quantity", 1}, {"price", 5}});
    expect(contains(said, "haven't that"), "nothing more to list: " + said);
    t.place(t.bo, 1, spot.x + 1.5, spot.y);
    t.run(1);
    auto here = t.selfOf(t.bo).object("stall").object("here");
    expect(here.array("wares").size() == 1 && here.boolean("present"), "Bo sees her wares, with her at the stall");
    // A sale face to face.
    const auto bo0 = society.account(t.bo.entityId)->cash, ash0 = society.account(t.ash.entityId)->cash;
    const int boMeals = Society::stock(*society.account(t.bo.entityId), "meal");
    said = stall(t.bo, {{"verb", "buy"}, {"id", here.string("id")}, {"item", "meal"}, {"quantity", 1}});
    expect(contains(said, "You buy 1 prepared meal for 5p") && society.account(t.bo.entityId)->cash == bo0 - 5 &&
               society.account(t.ash.entityId)->cash == ash0 + 5 && Society::stock(*society.account(t.bo.entityId), "meal") == boMeals + 1,
           "Bo buys a meal from her: " + said);
    // With her away, no sale.
    t.place(t.ash, 1, spot.x, spot.y + 6);
    said = stall(t.bo, {{"verb", "buy"}, {"id", here.string("id")}, {"item", "meal"}, {"quantity", 1}});
    expect(contains(said, "keeper isn't at the stall"), "no sale with the keeper away: " + said);
    t.place(t.ash, 1, spot.x, spot.y);
    stall(t.ash, {{"verb", "price"}, {"item", "meal"}, {"price", 7}});
    // A buyer who can't carry another kind is refused (a ware Bo has never held; Bo's purse filled to 64 kinds).
    std::string rare;
    for (const auto& g : items::allGoods())
        if (rare.empty() && g.id != "meal" && !society.account(t.bo.entityId)->stock.count(g.id) && !society.account(t.ash.entityId)->stock.count(g.id))
            rare = g.id;
    society.create(t.ash.entityId, rare, 1, "test");
    stall(t.ash, {{"verb", "list"}, {"item", rare}, {"quantity", 1}, {"price", 2}});
    int kinds = int(society.account(t.bo.entityId)->stock.size());
    for (const auto& g : items::allGoods())
        if (kinds < int(MaxGoodsKinds) && g.id != rare && !society.account(t.bo.entityId)->stock.count(g.id))
            society.create(t.bo.entityId, g.id, 1, "test"), ++kinds;
    said = stall(t.bo, {{"verb", "buy"}, {"id", here.string("id")}, {"item", rare}, {"quantity", 1}});
    expect(contains(said, "another kind"), "64 kinds: refused: " + said);
    expect(society.conserved(), "money conserved");
    // Residents at her stall: a household need at a penny, the keeper there; townsfolk near it buy (once a day each).
    std::string need;
    for (const auto& n : items::householdNeeds())
        for (const auto& id : n.any)
            if (need.empty() && items::good(id) && society.townPrice(community, id) >= 1)
                need = id;
    society.create(t.ash.entityId, need, 5, "test");
    said = stall(t.ash, {{"verb", "list"}, {"item", need}, {"quantity", 5}, {"price", 1}});
    expect(contains(said, "You lay out 5"), "five of a household need at 1p: " + said);
    const auto takings = society.account(t.ash.entityId)->cash;
    int round = 0;
    for (const char* who : {"u1", "u2", "u3", "um", "ui"})
    {
        auto* r = w.entity(who);
        r->cellId = spot.cell;
        r->position = {spot.x + 1 + .5 * round++, spot.y + 1};
        society.shift("treasury", who, "", 0, 200, "test purse");
    }
    t.g.command(&t.ash, cmd({{"type", "face"}, {"angle", 0}}));
    w.advanceCalendar(1. / 24);
    t.run(1.5);
    expect(society.account(t.ash.entityId)->cash > takings, "townsfolk buy at her stall: " + std::to_string(society.account(t.ash.entityId)->cash - takings) + "p");
    expect(society.conserved(), "money conserved");
    // At 2 the stall clears, and her meals come home.
    toHour(calendar::Marketday, 14.2);
    expect(!t.selfOf(t.ash).has("stall") || !t.selfOf(t.ash).object("stall").has("mine"), "cleared at 2");
    expect(Society::stock(*society.account(t.ash.entityId), "meal") == 2 && Society::stock(*society.account(t.ash.entityId), rare) == 1,
           "her two meals (and the rest) come home");
    bool back = false;
    for (const auto& s : w.dayPlan(community).stalls)
        back |= World::stallKey(s) == key;
    expect(back || w.dayPlan(community).kind != "market", "her spot back in the plan");
    // Foul weather: cleared, the fee returned.
    w.advanceCalendar(6);
    toHour(calendar::Marketday, 8);
    t.place(t.ash, 1, spot.x, spot.y);
    const_cast<Cell*>(w.cell(spot.cell))->weather = Weather::Clear;
    t.run(1);
    said = stall(t.ash, {{"verb", "rent"}});
    expect(contains(said, "You rent the stall"), "a stall next Marketday: " + said);
    const auto rented = society.account(t.ash.entityId)->cash;
    const_cast<Cell*>(w.cell(spot.cell))->weather = Weather::Storm;
    t.run(2);
    expect(society.account(t.ash.entityId)->cash == rented + 3, "a storm: the stall cleared and the fee returned");
    said = stall(t.ash, {{"verb", "rent"}});
    expect(contains(said, "too foul"), "no stalls in a storm: " + said);
    expect(society.conserved(), "money conserved");
    fs::remove_all(root);
}

// Phase 5, tavern games: a table in the common room; Knucklebones between two wolves to a winner, with practice;
// Liar's Bones for 3p against a resident (residents with little money refused), the pot paid less the house's penny,
// money conserved, the resident kept in its seat; a wolf leaving Wolves and Deer hands the game to the other.
void tables()
{
    const auto root = fs::temp_directory_path() / ("ratw-tables-" + std::to_string(::getpid()));
    const auto world = writeStrip(root, false, false, true);
    Town t(world, {});
    auto& w = t.g.world();
    auto& society = w.society();
    t.enter(t.ash, 1, "ash", "Ash", 1, 12.5);
    t.enter(t.bo, 2, "bo", "Bo", 1, 13.5);
    t.place(t.ash, 1, 12.5, 12.5);
    t.place(t.bo, 1, 13.5, 11.5);
    const auto table = [&](Client& c, std::initializer_list<std::pair<const char*, json::Value>> more) {
        auto o = json::Value::object();
        o.add("type", "table");
        for (const auto& [k, v] : more)
            o.add(k, v);
        c.events.clear();
        t.g.command(&c, json::dump(o));
        return c.said();
    };
    t.run(6);                                       // (The inns are known.)
    expect(t.selfOf(t.ash).object("table").boolean("offer"), "at a table in the common room: a game can be set out");
    auto said = table(t.ash, {{"verb", "start"}, {"game", "knucklebones"}});
    expect(contains(said, "You set out Knucklebones"), "Ash sets out Knucklebones: " + said);
    table(t.bo, {{"verb", "join"}});
    said = table(t.ash, {{"verb", "begin"}});
    expect(contains(said, "Knucklebones begins"), "it begins: " + said);
    // Play to the end: each tries twice, then banks.
    int moves = 0;
    std::string ended;
    t.run(1);
    while (moves++ < 400)
    {
        t.run(.5);
        const auto view = t.selfOf(t.ash).object("table");
        if (!view.boolean("begun"))
            break;
        Client& c = view.number("turn") == view.number("seat") ? t.ash : t.bo;
        const auto st = view.object("state");
        const int seat = int(view.number("turn"));
        const int gained = int(st.array("at")[std::size_t(seat)].asNumber() - st.array("banked")[std::size_t(seat)].asNumber());
        said = table(c, {{"verb", "move"}, {"action", gained >= 2 ? "bank" : "try"}});
        if (contains(said, "is over"))
            ended = said;
    }
    expect(contains(ended, "Knucklebones is over") && contains(ended, "win"), "played to a winner: " + ended);
    expect(w.entity(t.ash.entityId)->gameSkills.count("knucklebones") && w.entity(t.ash.entityId)->gameSkills.at("knucklebones") > 0,
           "playing sharpens her knucklebones");
    // Liar's Bones for 3p against a resident.
    t.run(61);                                      // (The old table cleared.)
    w.advanceCalendar(std::ceil(w.calendarDays()) + 20. / 24 - w.calendarDays());   // (The evening: folk are free.)
    t.run(2);
    said = table(t.ash, {{"verb", "start"}, {"game", "liars"}, {"stake", 3}});
    expect(contains(said, "You set out Liar's Bones"), "Liar's Bones for 3p: " + said);
    auto* u2 = w.entity("u2");
    u2->cellId = cellId(1);
    u2->position = {13.5, 12.5};
    society.shift("u2", "treasury", "", 0, society.account("u2")->cash, "test");
    t.run(.5);
    said = table(t.ash, {{"verb", "resident"}});
    expect(contains(said, "Nobody in the room is free to play for stakes"), "a resident with little money won't stake: " + said);
    auto* u1 = w.entity("u1");
    u1->cellId = cellId(1);
    u1->position = {11.5, 12.5};
    society.shift("treasury", "u1", "", 0, 100, "test purse");
    t.run(.5);
    said = table(t.ash, {{"verb", "resident"}});
    expect(contains(said, "took a seat at Liar's Bones"), "a resident takes a seat: " + said);
    const auto ash0 = society.account(t.ash.entityId)->cash, u10 = society.account("u1")->cash;
    const auto* till = society.account(society.tillOf("ui"));
    const auto house0 = till ? till->cash : 0;
    said = table(t.ash, {{"verb", "begin"}});
    expect(contains(said, "6p in the pot"), "the stakes in the pot: " + said);
    t.run(1);
    double farthest = 0;
    for (int i = 0; i < 600; ++i)
    {
        const auto view = t.selfOf(t.ash).object("table");
        if (!view.boolean("begun"))
            break;
        farthest = std::max(farthest, std::hypot(w.entity("u1")->position.x - 12.5, w.entity("u1")->position.y - 11.5));
        if (view.number("turn") == view.number("seat"))
        {
            const auto st = view.object("state");
            if (st.number("count") > 0)
                table(t.ash, {{"verb", "move"}, {"action", "call"}});
            else
                table(t.ash, {{"verb", "move"}, {"action", "bid"}, {"count", 1}, {"face", st.array("mine")[0].asNumber()}});
        }
        t.run(1);
    }
    const auto ashWon = society.account(t.ash.entityId)->cash - ash0, u1Won = society.account("u1")->cash - u10;
    const auto house = (till ? society.account(society.tillOf("ui"))->cash : 0) - house0;
    expect((ashWon == 2 && u1Won == -3) || (ashWon == -3 && u1Won == 2), "the pot to the winner less a penny: " + std::to_string(ashWon) + " / " + std::to_string(u1Won));
    expect(farthest < 2.5, "the resident kept to its seat through the game");
    expect(house == 1 || !till, "the house's penny to the inn's till");
    expect(!w.seatedResident("u1"), "the resident let go");
    expect(society.conserved(), "money conserved");
    // Wolves and Deer: Bo leaves; Ash wins.
    t.run(61);
    table(t.ash, {{"verb", "start"}, {"game", "wolves"}});
    table(t.bo, {{"verb", "join"}});
    table(t.ash, {{"verb", "begin"}});
    auto view = t.selfOf(t.bo).object("table");
    const auto& move = view.object("state").array("moves");
    expect(!move.empty() && view.number("turn") == 1, "the deer (Bo) first, with moves to make");
    said = table(t.bo, {{"verb", "move"}, {"from", move[0].items()[0].asNumber()}, {"to", move[0].items()[1].asNumber()}});
    t.ash.events.clear();
    table(t.bo, {{"verb", "leave"}});
    expect(contains(t.ash.said(), "You win"), "Bo leaves: Ash wins: " + t.ash.said());
    fs::remove_all(root);
}

// Phase 6, festivals: a called festival's programme; sign-ups (a penny each); the feast once and rested time at the
// square; the race run round its marks; the tug on the beat; the howling with cheers; the tourney and the hunting
// contest among residents; the storytelling judged by stars from different wolves; the steward's calls; the town's
// purse in the pots; money conserved.
void festivals()
{
    const auto root = fs::temp_directory_path() / ("ratw-festivals-" + std::to_string(::getpid()));
    const auto world = writeStrip(root);
    Town t(world, {});
    auto& w = t.g.world();
    auto& society = w.society();
    const auto community = w.lawTown(cellId(1));
    t.enter(t.ash, 1, "ash", "Ash", 1, 8.5);
    t.enter(t.bo, 2, "bo", "Bo", 1, 9.5);
    t.enter(t.cy, 3, "cy", "Cy", 1, 10.5);
    for (auto* c : {&t.ash, &t.bo, &t.cy})
        t.place(*c, 1, c == &t.ash ? 8.5 : c == &t.bo ? 9.5 : 10.5, 9.5);
    const auto fest = [&](Client& c, std::initializer_list<std::pair<const char*, json::Value>> more) {
        auto o = json::Value::object();
        o.add("type", "festival");
        for (const auto& [k, v] : more)
            o.add(k, v);
        c.events.clear();
        t.g.command(&c, json::dump(o));
        return c.said();
    };
    const auto toHour = [&](double hour) {
        const double now = (w.calendarDays() - std::floor(w.calendarDays())) * 24;
        w.advanceCalendar((hour > now ? hour - now : hour + 24 - now) / 24);
        t.run(1.5);
    };
    toHour(10);
    expect(w.callFestival(community, "Test Fair", 0).ok, "a festival called for today");
    t.run(1.5);
    const auto treasury = society.treasuryOf(community);
    auto view = t.selfOf(t.ash).object("festival");
    expect(view.string("name") == "Test Fair" && view.array("programme").size() == 9, "the programme: " + json::dump(view));
    for (const char* k : {"race", "tug", "howl", "tourney", "story"})
        expect(contains(fest(t.ash, {{"verb", "enter"}, {"contest", k}}), "You enter"), std::string("Ash enters ") + k);
    fest(t.bo, {{"verb", "enter"}, {"contest", "howl"}});
    fest(t.bo, {{"verb", "enter"}, {"contest", "story"}});
    expect(contains(fest(t.ash, {{"verb", "enter"}, {"contest", "race"}}), "already"), "once a contest");
    // Noon: the feast, once; rested time at the square.
    const int meals = Society::stock(*society.account(t.ash.entityId), "meal");
    const double rested = w.entity(t.ash.entityId)->practice->rested;
    toHour(12.1);
    t.run(2);
    expect(Society::stock(*society.account(t.ash.entityId), "meal") == meals + 1, "the feast: a meal from the town's store, once");
    expect(w.entity(t.ash.entityId)->practice->rested > rested || rested >= 30, "rested time at the square");
    // 13:00 the race: round the marks and home.
    toHour(13.01);
    std::string heard;
    for (int i = 0; i < 12; ++i)
    {
        const auto now = t.selfOf(t.ash).object("festival").object("now");
        if (now.string("kind") != "race")
            break;
        t.place(t.ash, 1, now.number("x"), now.number("y"));
        t.ash.events.clear();
        t.run(1);
        heard += t.ash.said();
    }
    t.run(4);
    heard += t.ash.said();
    expect(contains(heard, "Home, in") && contains(heard, "The steward calls"), "Ash runs the race home and the winner is called: " + heard);
    auto programme = t.selfOf(t.ash).object("festival").array("programme");
    expect(programme[1].string("state") == "done" && programme[1].has("winner"), "the race done, with a winner");
    expect(society.conserved(), "money conserved");
    // 14:00 the tug: pulls on the beat.
    t.place(t.ash, 1, 8.5, 9.5);
    toHour(14.01);
    for (int i = 0; i < 280; ++i)
    {
        t.g.command(&t.ash, cmd({{"type", "festival"}, {"verb", "pull"}}));
        t.run(.25);
    }
    programme = t.selfOf(t.ash).object("festival").array("programme");
    expect(programme[2].string("state") == "done", "the tug decided within the minute");
    // 15:00 howling, with cheers.
    toHour(15.01);
    for (int i = 0; i < 120; ++i)
    {
        const auto now = t.selfOf(t.ash).object("festival").object("now");
        if (now.string("kind") != "howl")
            break;
        if (now.boolean("mine"))
            fest(t.ash, {{"verb", "howl"}});
        if (t.selfOf(t.bo).object("festival").object("now").boolean("mine"))
            fest(t.bo, {{"verb", "howl"}});
        fest(t.cy, {{"verb", "cheer"}, {"target", t.ash.entityId}});
        t.run(1);
    }
    programme = t.selfOf(t.ash).object("festival").array("programme");
    expect(programme[3].string("state") == "done" && programme[3].has("winner"), "the howling done, with a winner");
    // 16:00 the tourney and 17:00 the hunt: residents only.
    toHour(16.01);
    t.ash.events.clear();
    std::string ring;
    for (int i = 0; i < 60 && t.selfOf(t.ash).object("festival").array("programme")[4].string("state") != "done"; ++i)
    {
        t.run(10);
        ring += t.ash.said();
        t.ash.events.clear();
    }
    expect(contains(ring, "You yield to") || contains(ring, "yields to you"), "Ash fights a bout in the ring, to a yield: " + ring);
    expect(contains(ring, "yield"), "bouts end at a yield");
    toHour(17.01);
    t.run(2);
    programme = t.selfOf(t.ash).object("festival").array("programme");
    expect(programme[4].string("state") == "done" && programme[5].string("state") == "done", "the tourney and the hunt decided");
    // 19:00 storytelling: Ash then Bo; Cy and Bo star Ash, Ash stars Bo.
    toHour(19.01);
    expect(contains(fest(t.cy, {{"verb", "star"}, {"target", t.ash.entityId}}), "You star"), "Cy stars Ash's tale");
    expect(contains(fest(t.cy, {{"verb", "star"}, {"target", t.bo.entityId}}), "given your star"), "one star a wolf");
    expect(contains(fest(t.ash, {{"verb", "star"}, {"target", t.ash.entityId}}), "Not your own"), "not one's own tale");
    fest(t.bo, {{"verb", "star"}, {"target", t.ash.entityId}});
    fest(t.ash, {{"verb", "star"}, {"target", t.bo.entityId}});
    const auto ashCash = society.account(t.ash.entityId)->cash;
    t.ash.events.clear();
    t.run(730);
    programme = t.selfOf(t.ash).object("festival").array("programme");
    expect(programme[6].string("state") == "done" && programme[6].string("winner") == "You", "Ash wins the storytelling: " + json::dump(programme[6]));
    expect(society.account(t.ash.entityId)->cash > ashCash, "the pot (with the town's share) paid");
    expect(society.conserved(), "money conserved");
    (void)treasury;
    fs::remove_all(root);
}

// Phase 7, the archive: work only where a keeper of records is; six records sorted by their clues (solved here from the
// words alone), a wrong order counted and tried again, the right one paid 2p from the town with the town's first lore
// fragment; four a day; copying at a desk, sitting, for five minutes; leaving drops the work; the journal.
std::vector<std::string> solveRecords(const json::Value& task)
{
    static const char* const Times[] = {"early spring", "midspring", "late spring", "early summer", "midsummer", "late summer",
                                        "early autumn", "midautumn", "late autumn", "early winter", "midwinter", "late winter"};
    const auto romanValue = [](const std::string& r) {
        const std::map<char, int> v{{'I', 1}, {'V', 5}, {'X', 10}, {'L', 50}};
        int total = 0;
        for (std::size_t i = 0; i < r.size(); ++i)
            total += i + 1 < r.size() && v.at(r[i]) < v.at(r[i + 1]) ? -v.at(r[i]) : v.at(r[i]);
        return total;
    };
    std::vector<std::pair<int, std::string>> keyed;
    for (const auto& r : task.array("records"))
    {
        const auto text = r.string("text");
        int key = 0;
        if (const auto at = text.find(" winter of"); at != std::string::npos)
            key = std::stoi(text.substr(text.rfind(' ', at - 1) + 1));   // ("the 123rd winter": the number, its ending ignored.)
        else if (text.rfind("Roll ", 0) == 0)
            key = romanValue(text.substr(5, text.find(':') - 5));
        else
            for (int k = 11; k >= 0; --k)
                if (contains(text, std::string("in ") + Times[k] + "."))
                {
                    key = k;
                    break;
                }
        keyed.push_back({key, r.string("id")});
    }
    std::sort(keyed.begin(), keyed.end());
    std::vector<std::string> order;
    for (const auto& [k, id] : keyed)
        order.push_back(id);
    return order;
}

void archive()
{
    const auto root = fs::temp_directory_path() / ("ratw-archive-" + std::to_string(::getpid()));
    const auto world = writeStrip(root, false, false, false, true);
    Town t(world, {});
    auto& w = t.g.world();
    auto& society = w.society();
    t.enter(t.ash, 1, "ash", "Ash", 0, 8.5);
    const auto arc = [&](std::initializer_list<std::pair<const char*, json::Value>> more) {
        auto o = json::Value::object();
        o.add("type", "archive");
        for (const auto& [k, v] : more)
            o.add(k, v);
        t.ash.events.clear();
        t.g.command(&t.ash, json::dump(o));
        return t.ash.said();
    };
    const double hour = (w.calendarDays() - std::floor(w.calendarDays())) * 24;
    w.advanceCalendar(((hour < 10 ? 10 : 34) - hour) / 24);
    t.run(1.5);
    auto said = arc({{"verb", "ask"}});
    expect(contains(said, "Ask a keeper of records"), "no archive work away from the records: " + said);
    auto* clerk = w.entity("ua");
    clerk->cellId = cellId(1);
    clerk->position = {4.5, 8.5};
    t.place(t.ash, 1, 5.5, 8.5);
    t.run(1);
    said = arc({{"verb", "ask"}});
    expect(contains(said, "six records to sort"), "the keeper gives work: " + said);
    const auto* task = t.ash.last("archive");
    expect(task && task->array("records").size() == 6 && !task->has("answer"), "six records, and not the answer");
    auto order = solveRecords(*task);
    auto wrong = order;
    std::reverse(wrong.begin(), wrong.end());
    auto toValue = [](const std::vector<std::string>& ids) {
        auto a = json::Value::array();
        for (const auto& id : ids)
            a.push(id);
        return a;
    };
    said = arc({{"verb", "sort"}, {"order", toValue(wrong)}});
    expect(contains(said, "of 6 in the right place. Try again"), "a wrong order counted: " + said);
    const auto treasury = society.treasuryOf(w.lawTown(cellId(1)));
    const auto town = society.account(treasury)->cash, purse = society.account(t.ash.entityId)->cash;
    said = arc({{"verb", "sort"}, {"order", toValue(order)}});
    expect(contains(said, "The records are in order") && contains(said, "pays you 2p") && society.account(t.ash.entityId)->cash == purse + 2 &&
               society.account(treasury)->cash == town - 2, "sorted, and paid 2p by the town: " + said);
    expect(contains(said, "Among the records") && w.entity(t.ash.entityId)->lore.size() == 1 && w.entity(t.ash.entityId)->lore[0] == "ua-oath-1",
           "the town's first fragment: " + said);
    for (int i = 0; i < 3; ++i)
    {
        arc({{"verb", "ask"}});
        arc({{"verb", "sort"}, {"order", toValue(solveRecords(*t.ash.last("archive")))}});
    }
    expect(w.entity(t.ash.entityId)->lore.size() == 4, "four tasks, four fragments");
    said = arc({{"verb", "ask"}});
    expect(contains(said, "enough for one day"), "four a day: " + said);
    // The next day: copying, sitting at a desk.
    w.advanceCalendar(1);
    t.run(1.5);
    clerk->cellId = cellId(1);
    clerk->position = {4.5, 8.5};
    t.run(.5);
    said = arc({{"verb", "ask"}, {"kind", "copy"}});
    expect(contains(said, "a page to copy"), "copying: " + said);
    w.entity(t.ash.entityId)->posture = "sitting";
    t.ash.events.clear();
    t.run(302);
    expect(contains(t.ash.said(), "copy") && contains(t.ash.said(), "pays you"), "five minutes at the desk, then paid: " + t.ash.said());
    // Leaving drops the work.
    arc({{"verb", "ask"}});
    t.place(t.ash, 0, 8.5);
    t.ash.events.clear();
    t.run(1.5);
    expect(contains(t.ash.said(), "the keeper takes them back"), "leaving the archive drops the work: " + t.ash.said());
    // The journal.
    w.noteFound(t.ash.entityId, "herbs", "broadleaf woods");
    t.g.command(&t.ash, cmd({{"type", "journal"}}));
    const auto* journal = t.ash.last("journal");
    expect(journal && journal->array("lore").size() == 5 && journal->array("herbarium").size() == 1 && !journal->array("places").empty(),
           "the journal: lore, the herbarium and places");
    expect(society.conserved(), "money conserved");
    fs::remove_all(root);
}
} // namespace

int main()
{
    try
    {
        taverns();
        restedAtAnInn();
        boards();
        lodgings();
        stalls();
        tables();
        festivals();
        archive();
    }
    catch (const std::exception& error)
    {
        std::cerr << "gathering_tests failed after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "gathering_tests passed (" << checks << " checks)\n";
    return 0;
}
