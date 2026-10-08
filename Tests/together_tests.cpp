// Working together (Docs/Design/53-hunting-and-working-together.md, 2; Phase 3): the cooperation scaling in every
// case doc 48 names; Lend a paw refused when the worker has work partners off or either blocks the other, and allowed
// when asked; members who go off or idle leave; two foragers each taking about 1.8 times a lone forager's goods, the
// patch giving 5 pickings for two and 6 for three; shares exact; the bond at the end.
#include "RatwCalendar.h"
#include "RatwItems.h"
#include "RatwTogether.h"
#include "RatwWild.h"
#include "RatwWorld.h"

#include <cmath>
#include <iostream>
#include <map>
#include <sstream>
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

// The hunt tests' wild cell, 80 by 60.
World wilds()
{
    std::ostringstream cell;
    cell << "id: wilds\nname: The Wilds\ndescription: Woods and meadow.\nworld: 0 0 0\nsize: 80 60\noutdoors: true\n"
            "weather: clear\nwind: 0 0.5 1\nlighting: 1 1 warm\ngrid:\n";
    for (int y = 0; y < 60; ++y)
    {
        std::string row;
        for (int x = 0; x < 80; ++x)
            row += (x * 7 + y * 13) % 11 == 0 ? 'Y' : (x * 3 + y * 5) % 17 == 0 ? 'B' : x == 70 ? '~' : ',';
        cell << row << '\n';
    }
    std::map<std::string, std::string> files;
    files["cells/wilds.cell"] = cell.str();
    files["world.ratw"] = "RATW_WORLD 2\ncell \"wilds\" \"cells/wilds.cell\"\nterritory \"wilds\" \"wilds\" \"-\" 0\nspawn \"wilds\" 40.5 30.5\n"
                          "economy 1000 100 50 10 12\n";
    World w;
    expect(w.loadWorldFiles(files, "wilds").ok, "the wilds load");
    return w;
}

Entity& wolf(World& w, const std::string& id, double x, double y)
{
    auto& e = w.addPlayer(id, id);
    e.cellId = "wilds";
    e.position = {x, y};
    return e;
}

int goods(const World& w, const std::string& id)
{
    int n = 0;
    if (const auto* a = w.society().account(id))
        for (const auto& [item, count] : a->stock)
            if (item != "sword" && count > 0)
                n += count;
    return n;
}

void theRate()
{
    using together::Hand;
    const auto rate = [](std::vector<Hand> h, int most = 6) { return together::rate(h, most); };
    expect(std::abs(rate({{"dig"}}) - 1) < 1e-9, "alone: 1");
    expect(std::abs(rate({{"dig"}, {"sort"}}) - 1.8) < 1e-9, "two players, two angles: 1.8");
    expect(std::abs(rate({{"dig"}, {"dig"}}) - 1.4) < 1e-9, "the same angle: 1.4");
    expect(std::abs(rate({{"dig"}, {"sort", true}}) - 1.4) < 1e-9, "with a resident hand: 1.4");
    expect(std::abs(rate({{"dig"}, {"sort"}, {"dig"}}) - 2.2) < 1e-9, "three: 2.2");
    expect(std::abs(rate({{"dig"}, {"sort"}, {"dig"}, {"sort"}}) - 2.45) < 1e-9, "four: 2.45");
    expect(std::abs(rate({{"dig"}, {"sort", true}, {"sort"}}) - 2.0) < 1e-9, "two players and a resident hand (hands counted last): 2.0");
    expect(std::abs(rate({{"dig"}, {"sort"}, {"dig"}, {"sort"}, {"dig"}, {"sort"}}) - 2.65) < 1e-9, "six: 0.1 for each of the 5th and 6th");
    expect(std::abs(rate({{"dig"}, {"sort"}, {"dig"}, {"sort"}}, 2) - 1.8) < 1e-9, "no more than the activity's most");
    const auto parts = together::split(7, 3, {2, 0, 1});
    expect(parts[0] + parts[1] + parts[2] == 7 && parts[2] == 3 && parts[0] == 2 && parts[1] == 2, "seven split three ways: 2 each, the one left over by the order");
}

void lendingAPaw()
{
    auto w = wilds();
    wolf(w, "player-ash", 40.5, 30.5);
    wolf(w, "player-bo", 42.5, 30.5);
    wolf(w, "player-cy", 41.5, 31.5);
    expect(!w.atWork("player-ash") && !w.mayLend("player-bo", "player-ash"), "not at work: nothing to lend a paw to");
    const auto picked = w.forage("player-ash");
    expect(picked.ok && w.atWork("player-ash"), "Ash forages: at work: " + picked.message);
    expect(w.mayLend("player-bo", "player-ash"), "Bo, near, may lend a paw");
    // Off: only those Ash asks.
    w.setPartners("player-ash", "work", false);
    expect(!w.mayLend("player-bo", "player-ash") && !w.lendAPaw("player-bo", "player-ash").ok, "Ash's work partners off: Bo may not");
    expect(w.askToLend("player-ash", "player-bo").ok && w.mayLend("player-bo", "player-ash"), "asked, he may");
    // Blocked either way: never.
    w.setBlocked([](const std::string& a, const std::string& b) { return (a == "player-ash" && b == "player-cy") || (a == "player-cy" && b == "player-ash"); });
    w.setPartners("player-ash", "work", true);
    expect(!w.mayLend("player-cy", "player-ash") && !w.askToLend("player-ash", "player-cy").ok, "a blocked pair: neither lends nor asks");
    const auto lent = w.lendAPaw("player-bo", "player-ash");
    expect(lent.ok && w.jointOf("player-bo") && w.jointOf("player-ash") == w.jointOf("player-bo"), "Bo lends a paw: one joint: " + lent.message);
    const auto* j = w.jointOf("player-ash");
    expect(j->members.size() == 2 && j->members[0].role == "dig" && j->members[1].role == "sort", "Ash digs, Bo carries and sorts (the free role)");
    expect(std::abs(w.workRate("player-ash") - 1.8) < 1e-9 && std::abs(w.workRate("player-bo") - 1.8) < 1e-9, "each at 1.8");
    // Bo goes off 9 tiles: he has left, and with one left the joint ends.
    w.entity("player-bo")->position = {52.5, 30.5};
    for (int i = 0; i < 12; ++i)
        w.tick(.25);
    expect(!w.jointOf("player-bo") && !w.jointOf("player-ash"), "Bo went off: the joint ends");
    // Idle a minute: leaves too.
    w.entity("player-bo")->position = {41.5, 30.5};
    w.forage("player-ash");
    expect(w.lendAPaw("player-bo", "player-ash").ok, "Bo lends a paw again");
    for (int i = 0; i < 64; ++i)
    {
        w.tick(1);
        w.forage("player-ash");                    // (Ash keeps at it; Bo doesn't.)
    }
    expect(!w.jointOf("player-bo"), "idle a minute: Bo has left off");
}

void foragingTogether()
{
    // Over many pickings, each of a pair takes about 1.8 times what a lone forager does (each moving on to a fresh
    // patch every round, as foragers do).
    const auto forageRounds = [](bool pair, int rounds) {
        auto w = wilds();
        wolf(w, "player-ash", 4.5, 4.5);
        wolf(w, "player-bo", 5.5, 4.5);
        const int ashBefore = goods(w, "player-ash"), boBefore = goods(w, "player-bo");
        for (int round = 0; round < rounds; ++round)
        {
            // A fresh patch each round (the last round's joint ends once they have moved on); two pickings each.
            const double x = 4.5 + (round % 8) * 9, y = 4.5 + (round / 8 % 6) * 9;
            w.entity("player-ash")->position = {x, y};
            w.entity("player-bo")->position = {x + 1, y};
            for (int t = 0; t < 5; ++t)
                w.tick(1);
            w.forage("player-ash");
            if (pair)
            {
                w.lendAPaw("player-bo", "player-ash");
                w.forage("player-bo");
            }
            for (int t = 0; t < 5; ++t)
                w.tick(1);
            w.forage("player-ash");
            if (pair)
                w.forage("player-bo");
        }
        return std::pair<int, int>{goods(w, "player-ash") - ashBefore, goods(w, "player-bo") - boBefore};
    };
    const auto lone = forageRounds(false, 40);
    const auto pair = forageRounds(true, 40);
    const double each = (pair.first + pair.second) / 2.0;
    expect(lone.first > 20, "a lone forager gathers: " + std::to_string(lone.first));
    expect(each / lone.first > 1.55 && each / lone.first < 2.05, "each of a pair about 1.8 times: " + std::to_string(each) + " against " +
                                                                    std::to_string(lone.first));
    expect(std::abs(pair.first - pair.second) <= std::max(4, pair.first / 5), "shared evenly: " + std::to_string(pair.first) + " and " + std::to_string(pair.second));
    std::cout << "  foraging, 40 rounds of two pickings each: alone " << lone.first << ", a pair " << pair.first << " and " << pair.second << " (each "
              << each / lone.first << "x)\n";
    // One patch: 4 pickings alone, 5 for two, 6 for three.
    const auto pickingsOf = [](int wolves) {
        auto w = wilds();
        const std::vector<std::string> ids{"player-ash", "player-bo", "player-cy"};
        for (int i = 0; i < wolves; ++i)
            wolf(w, ids[std::size_t(i)], 20.5 + i * .5, 20.5);
        int taken = w.forage("player-ash").ok ? 1 : 0;
        for (int i = 1; i < wolves; ++i)
            w.lendAPaw(ids[std::size_t(i)], "player-ash");
        for (int k = 0; k < 12; ++k)
        {
            for (int t = 0; t < 5; ++t)
                w.tick(1);
            for (int i = 0; i < wolves; ++i)
                taken += w.forage(ids[std::size_t(i)]).ok;
        }
        return taken;
    };
    const int one = pickingsOf(1), two = pickingsOf(2), three = pickingsOf(3);
    expect(one >= 4 && two == one + 1 && three == one + 2, "a patch gives one more picking a wolf, up to two more: " + std::to_string(one) + ", " +
                                                                std::to_string(two) + ", " + std::to_string(three));
}

void theBond()
{
    auto w = wilds();
    wolf(w, "player-ash", 30.5, 30.5);
    wolf(w, "player-bo", 31.5, 30.5);
    w.forage("player-ash");
    expect(w.lendAPaw("player-bo", "player-ash").ok, "Bo lends a paw");
    for (int k = 0; k < 3; ++k)
    {
        for (int t = 0; t < 5; ++t)
            w.tick(1);
        w.forage("player-ash");
        w.forage("player-bo");
    }
    const auto before = w.bonds().find("player-ash", "player-bo");
    const double was = before ? before->familiarity : 0;
    w.takeEvents();
    expect(w.leaveWork("player-bo").ok && !w.jointOf("player-ash"), "Bo leaves off; the joint ends");
    const auto* after = w.bonds().find("player-ash", "player-bo");
    expect(after && after->familiarity > was && w.bonds().find("player-bo", "player-ash"), "two beats or more together: they know each other better");
    bool recorded = false;
    for (const auto& e : w.takeEvents())
        recorded = recorded || (e.kind == "together" && (e.actor == "player-ash" || e.actor == "player-bo"));
    expect(recorded, "and it is recorded");
}

// A field with Hale, a farmer at work in it (8 to 17), on the given season's first day at 9.
World farm(int season)
{
    std::ostringstream cell;
    cell << "id: fields\nname: The Fields\ndescription: Fields.\nworld: 0 0 0\nsize: 40 40\noutdoors: true\nweather: clear\n"
            "wind: 0 0.5 1\nlighting: 1 1 warm\ngrid:\n";
    for (int y = 0; y < 40; ++y)
        cell << std::string(40, ',') << '\n';
    std::map<std::string, std::string> files;
    files["cells/fields.cell"] = cell.str();
    files["world.ratw"] = "RATW_WORLD 2\ncell \"fields\" \"cells/fields.cell\"\nterritory \"fields\" \"fields\" \"-\" 0\n"
                          "spawn \"fields\" 20.5 20.5\neconomy 1000 100 50 10 12\n"
                          "resident \"hale\" \"Hale\" \"civilian\" \"farms the valley fields\" \"A farmer.\" \"Hello.\" 40 \"timber\" \"male\" "
                          "\"average\" \"saddle\" 3 1 5 1 1 8 17 \"-\" 40 0 1 \"fields\" 10.5 10.5 \"fields\" 20.5 20.5 \"fields\" 10.5 11.5\n";
    World w;
    expect(w.loadWorldFiles(files, "fields").ok, "the fields load");
    while (int(calendar::calendarAt(w.calendarDays()).season) != season)
        w.advanceCalendar(1);
    w.setTimeOfDay(9);
    for (int i = 0; i < 90; ++i)
        w.tick(1);                                  // (Hale walks out to his post.)
    return w;
}

std::int64_t cashOf(const World& w, const std::string& id)
{
    const auto* a = w.society().account(id);
    return a ? a->cash : 0;
}

std::int64_t allCash(const World& w)
{
    std::int64_t n = 0;
    for (const auto& [id, a] : w.society().state().accounts)
        n += a.cash;
    return n;
}

int barn(const World& w)
{
    int n = 0;
    if (const auto* a = w.society().account(w.society().tillOf("hale")))
        for (const auto& item : {"wheat", "oats", "firewood", "vegetables", "straw", "flax", "hemp", "apples", "hay"})
            n += Society::stock(*a, item);
    return n;
}

// Working for `seconds`, each player doing something every half minute (as a player at work does).
void workFor(World& w, double seconds, std::initializer_list<const char*> active)
{
    for (double t = 0; t < seconds; t += 1)
    {
        if (int(t) % 30 == 0)
            for (const auto* id : active)
                w.noteActive(id);
        w.tick(1);
    }
}

void farmWork()
{
    // Only in the harvest (autumn) and at threshing (winter).
    {
        auto spring = farm(0);
        wolf(spring, "player-ash", 21.5, 20.5).cellId = "fields";
        expect(!spring.residentWorkAt("hale") && !spring.helpAtWork("player-ash", "hale").ok, "spring: nothing to help with");
        auto autumn = farm(2);
        expect(autumn.residentWorkAt("hale") && autumn.residentWorkAt("hale")->id == "harvest", "autumn: the harvest");
    }
    auto w = farm(3);
    auto control = farm(3);
    for (auto* world : {&w, &control})
    {
        wolf(*world, "player-ash", 21.5, 20.5).cellId = "fields";
        wolf(*world, "player-bo", 19.5, 20.5).cellId = "fields";
    }
    expect(w.residentWorkAt("hale") && w.residentWorkAt("hale")->id == "threshing", "winter: threshing");
    const auto till = w.society().tillOf("hale");
    const auto tillBefore = cashOf(w, till), ashBefore = cashOf(w, "player-ash"), moneyBefore = allCash(w), controlMoney = allCash(control);
    const int barnBefore = barn(w), controlBarn = barn(control);
    const auto started = w.helpAtWork("player-ash", "hale");
    expect(started.ok && w.jointOf("player-ash") && w.jointOf("player-ash") == w.jointOf("hale"), "Ash sets to work beside Hale: " + started.message);
    expect(std::abs(w.workRate("player-ash") - 1.4) < 1e-9, "a player and the farmer: 1.4");
    // Four spells: each pays the hand wage a spell times 1.4 (12 / 8 x 1.4 = 2.1p), fractions carried.
    workFor(w, 4 * 300 + 2, {"player-ash"});
    workFor(control, 4 * 300 + 2, {});
    const auto paid = cashOf(w, "player-ash") - ashBefore;
    expect(w.jointOf("player-ash") && w.jointOf("player-ash")->members.back().beats == 4, "four spells worked");
    expect(paid == 8, "paid four spells at 2.1p each, fractions carried: " + std::to_string(paid));
    expect(tillBefore - cashOf(w, till) == paid, "from Hale's till");
    expect(allCash(w) - moneyBefore == allCash(control) - controlMoney, "money only moved (doc 15)");
    const int brought = (barn(w) - barnBefore) - (barn(control) - controlBarn);
    std::cout << "  threshing, four spells beside Hale: paid " << paid << "p from his till; " << brought << " more goods in his barn than he alone brought in\n";
    expect(brought >= 3 && brought <= 12, "and the barn has more than Hale alone would bring in: " + std::to_string(brought));
    // Bo lends a paw: two players and the farmer, 2.0 each; pay follows the rate.
    expect(w.lendAPaw("player-bo", "player-ash").ok && std::abs(w.workRate("player-bo") - 2.0) < 1e-9, "Bo lends a paw: 2.0 each");
    const auto boBefore = cashOf(w, "player-bo");
    const auto ashMid = cashOf(w, "player-ash");
    workFor(w, 2 * 300, {"player-ash", "player-bo"});
    const auto boPaid = cashOf(w, "player-bo") - boBefore, ashPaid = cashOf(w, "player-ash") - ashMid;
    std::cout << "  two spells with Bo too: Ash paid " << ashPaid << "p, Bo " << boPaid << "p\n";
    expect(boPaid >= 5 && boPaid <= 7 && ashPaid >= 5 && ashPaid <= 7, "at 2.0 each, 3p a spell (Bo's first nearly whole): Ash " +
                                                                             std::to_string(ashPaid) + ", Bo " + std::to_string(boPaid));
    // Idle a whole spell: Ash has left off, and isn't paid for it.
    const auto ashIdle = cashOf(w, "player-ash");
    workFor(w, 330, {"player-bo"});
    expect(!w.jointOf("player-ash") && cashOf(w, "player-ash") - ashIdle <= 3, "Ash, idle a spell, has left off");
    // A full barn: no more work.
    for (const auto& item : {"wheat", "oats", "firewood"})
        if (const int held = Society::stock(*w.society().account(till), item); held < Society::ProducerKept)
            w.society().create(till, item, Society::ProducerKept - held, "test");
    workFor(w, 301, {"player-bo"});
    expect(!w.jointOf("player-bo") && !w.jointOf("hale"), "the barn full: the work ends");
    const auto full = w.helpAtWork("player-bo", "hale");
    expect(!full.ok && full.message.find("barn is full") != std::string::npos, "and can't start: " + full.message);
    // An empty till: no work.
    auto poor = farm(3);
    wolf(poor, "player-ash", 21.5, 20.5).cellId = "fields";
    const auto poorTill = poor.society().tillOf("hale");
    poor.society().shift(poorTill, "player-ash", "", 0, poor.society().spendable(poorTill), "test");
    const auto broke = poor.helpAtWork("player-ash", "hale");
    expect(!broke.ok && broke.message.find("can't pay") != std::string::npos, "an empty till: " + broke.message);
    // Not from afar.
    auto far = farm(3);
    wolf(far, "player-ash", 34.5, 20.5).cellId = "fields";
    expect(!far.helpAtWork("player-ash", "hale").ok, "not from 14 tiles off");
}

// A training yard with Tam, who trains the recruits there (8 to 17), at 9 in the morning; and Ash beside her.
World yard(const std::string& cellId = "training_yard", const std::string& work = "trains the recruits")
{
    std::ostringstream cell;
    cell << "id: " << cellId << "\nname: The Yard\ndescription: Packed earth and a post.\nworld: 0 0 0\nsize: 30 30\noutdoors: true\n"
            "weather: clear\nwind: 0 0.5 1\nlighting: 1 1 warm\ngrid:\n";
    for (int y = 0; y < 30; ++y)
        cell << std::string(30, '.') << '\n';
    std::map<std::string, std::string> files;
    files["cells/yard.cell"] = cell.str();
    files["world.ratw"] = "RATW_WORLD 2\ncell \"" + cellId + "\" \"cells/yard.cell\"\nterritory \"" + cellId + "\" \"yard\" \"-\" 0\n"
                          "spawn \"" + cellId + "\" 15.5 15.5\neconomy 1000 100 50 10 12\n"
                          "resident \"tam\" \"Tam\" \"civilian\" \"" + work + "\" \"A trainer.\" \"Hello.\" 40 \"timber\" \"female\" "
                          "\"average\" \"saddle\" 3 1 5 1 1 8 17 \"-\" 40 0 1 \"" + cellId + "\" 5.5 5.5 \"" + cellId + "\" 15.5 15.5 \"" + cellId +
                          "\" 5.5 6.5\n";
    World w;
    expect(w.loadWorldFiles(files, "yard").ok, "the yard loads");
    w.setTimeOfDay(9);
    for (int i = 0; i < 90; ++i)
        w.tick(1);
    auto& ash = w.addPlayer("player-ash", "Ash");
    ash.cellId = cellId;
    ash.position = {16.5, 15.5};
    return w;
}

void training()
{
    auto w = yard();
    expect(w.trainingGround("training_yard") && !w.trainingGround("fields"), "a cell named for training is a training ground");
    expect(w.trainer("tam"), "Tam, who trains the recruits there, is a trainer");
    const auto bond = [&] {
        const auto* b = w.bonds().find("tam", "player-ash");
        return b ? b->affinity : 0.;
    };
    const double liking = bond();
    const auto asked = w.sparWithTrainer("player-ash", "tam");
    const auto* b = w.battleOf("player-ash");
    expect(asked.ok && b && b->terms == "spar" && b->incident.empty(), "Ash asks Tam to spar: a spar, no assault: " + asked.message);
    expect(std::abs(bond() - liking) < 1e-9, "and Tam thinks no worse of her");
    expect(b->fighters.size() == 2, "nobody comes in against her");
    // Not a trainer off the training ground.
    auto elsewhere = yard("meadow");
    expect(!elsewhere.trainer("tam") && !elsewhere.sparWithTrainer("player-ash", "tam").ok, "off a training ground Tam doesn't spar");
    // The post: practice with no partner, now and then.
    auto p = yard();
    const double skill = p.entity("player-ash")->fightingSkill;
    expect(p.practiseAtPost("player-ash").ok, "Ash works at the post");
    expect(!p.practiseAtPost("player-ash").ok, "not again at once");
    for (int i = 0; i < 25; ++i)
        p.tick(1);
    expect(p.practiseAtPost("player-ash").ok, "a while later, again");
    expect(p.entity("player-ash")->fightingSkill > skill, "her fighting grows: " + std::to_string(p.entity("player-ash")->fightingSkill));
    expect(!elsewhere.practiseAtPost("player-ash").ok, "no post off a training ground");
}

// A hand at a resident's bench (doc 53, 3, part A): Tam bakes at The Loaf; Ash lends a paw at the workshop, paid by the
// spell from the shop's till, at x1.4 beside the maker (who leads).
void benchWork()
{
    auto w = yard("bakery", "baker at The Loaf");
    const auto* act = w.residentWorkAt("tam");
    expect(act && act->id == "workshop" && act->verb == "lend a paw at the workshop", "the baker at work: Lend a paw at the workshop");
    const auto ashBefore = cashOf(w, "player-ash");
    w.takeEvents();
    const auto started = w.helpAtWork("player-ash", "tam");
    const auto* j = w.jointOf("player-ash");
    expect(started.ok && j && j->members.front().id == "tam" && j->members.front().role == "lead" && j->members.back().role == "hand",
           "Ash hands, Tam leads: " + started.message);
    expect(std::abs(w.workRate("player-ash") - 1.4) < 1e-9, "beside the maker: 1.4");
    workFor(w, 2 * 300 + 2, {"player-ash"});
    const auto paid = cashOf(w, "player-ash") - ashBefore;
    std::int64_t recorded = 0;
    for (const auto& e : w.takeEvents())
        if (e.kind == "bench work" && e.actor == "player-ash" && e.target == "tam")
            recorded += e.coins;
    expect(paid >= 3 && recorded == paid, "two spells paid from the shop's till: " + std::to_string(paid) + "p");
    expect(w.jointOf("player-ash") && w.jointOf("player-ash")->members.back().beats == 2, "two spells at the bench");
    // Not a farm's harvest: a farmer isn't a workshop, nor a baker a farm.
    expect(!w.residentWorkAt("player-ash"), "a player is nobody's bench");
}

// Gifted and Quickened at work (doc 53, 4): Winnow and Dry at threshing is an angle of its own and lifts the joint for the
// beat; Keep watch is an angle any wolf may take in the wild; a resident in talk faces the talker.
void anglesOfTheGifts()
{
    auto w = farm(3);
    wolf(w, "player-ash", 21.5, 20.5).cellId = "fields";
    expect(w.giveGift("player-ash", "wind", false).ok, "Ash is Gifted: Wind");
    w.entity("player-ash")->mana = 100;
    expect(w.helpAtWork("player-ash", "hale").ok && std::abs(w.workRate("player-ash") - 1.4) < 1e-9, "threshing beside Hale: 1.4");
    const auto lifted = w.useWorkGift("player-ash", "winnow_and_dry", {});
    expect(lifted.ok && std::abs(w.workRate("player-ash") - 1.6) < 1e-9, "Winnow and Dry on the threshing: 1.6 this spell: " + lifted.message);
    workFor(w, 302, {"player-ash"});
    expect(std::abs(w.workRate("player-ash") - 1.4) < 1e-9, "the spell over, 1.4 again");
    // Carry (a Sound wolf's) at the threshing too; and at a maker's bench, a Gift of its trade lifts the joint and the batch.
    auto t = farm(3);
    wolf(t, "player-ash", 21.5, 20.5).cellId = "fields";
    t.giveGift("player-ash", "sound", false);
    t.entity("player-ash")->mana = 100;
    t.helpAtWork("player-ash", "hale");
    expect(t.useWorkGift("player-ash", "carry", {}).ok && std::abs(t.workRate("player-ash") - 1.6) < 1e-9, "Carry on the threshing: 1.6");
    auto bench = yard("bakery", "baker at The Loaf");
    bench.giveGift("player-ash", "wind", false);
    bench.entity("player-ash")->mana = 100;
    bench.helpAtWork("player-ash", "tam");
    expect(bench.useWorkGift("player-ash", "winnow_and_dry", {}).ok && std::abs(bench.workRate("player-ash") - 1.6) < 1e-9 &&
               bench.society().giftLiftOf("tam") > 0,
           "Winnow and Dry at the bakery's bench: 1.6, and the batch lifted");
    // Keep watch: two digging (1.4), then one keeps watch: two angles (1.8).
    auto f = wilds();
    wolf(f, "player-ash", 30.5, 30.5);
    wolf(f, "player-bo", 31.5, 30.5);
    f.forage("player-ash");
    expect(f.lendAPaw("player-bo", "player-ash", "dig").ok && std::abs(f.workRate("player-ash") - 1.4) < 1e-9, "two digging: 1.4");
    expect(f.keepWatch("player-bo", true).ok && f.keepingWatch("player-bo") && std::abs(f.workRate("player-ash") - 1.8) < 1e-9,
           "Bo keeps watch: an angle of its own, 1.8");
    expect(f.watchersOver("player-ash") == std::vector<std::string>{"player-bo"}, "watching over Ash");
    for (int i = 0; i < 90; ++i)
    {
        if (i % 16 == 0)
        {
            // On to fresh ground together.
            f.entity("player-ash")->position = {10.5 + i / 16 * 9, 10.5};
            f.entity("player-bo")->position = {11.5 + i / 16 * 9, 10.5};
        }
        f.tick(1);
        if (i % 4 == 0)
            f.forage("player-ash");
    }
    expect(f.keepingWatch("player-bo"), "a watcher isn't idle: still at it after a minute and a half");
    // A resident in talk faces the talker, for 20 seconds after.
    auto y = yard("bakery", "baker at The Loaf");
    auto* tam = y.entity("tam");
    auto* ash = y.entity("player-ash");
    ash->position = {tam->position.x, tam->position.y - 2};
    tam->facing = 0;
    y.faceTalker("tam", "player-ash");
    expect(std::abs(tam->facing - std::atan2(-2., 0.)) < 1e-6 && y.facingTalker("tam") == "player-ash", "Tam turns to Ash, who talks to her");
    for (int i = 0; i < 22; ++i)
        y.tick(1);
    expect(y.facingTalker("tam").empty(), "and is free of her twenty seconds after");
}
} // namespace

int main()
{
    try
    {
        std::string error;
        expect(wild::load(&error), "the wild's data: " + error);
        expect(together::activity("forage") && together::activity("forage")->roles.size() == 2, "foraging: two roles");
        theRate();
        lendingAPaw();
        foragingTogether();
        theBond();
        farmWork();
        training();
        benchWork();
        anglesOfTheGifts();
    }
    catch (const std::exception& error)
    {
        std::cerr << "together_tests failed after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "together_tests passed (" << checks << " checks)\n";
    return 0;
}
