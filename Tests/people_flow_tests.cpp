// Births follow deaths; arrivals from the Country (Docs/Design/68-the-country-and-the-city-loop.md, Progress, "Births follow
// deaths; arrivals from the Country; the treasury's target"). The user, 2026-11-01: "Birthrates should match death rates
// for this game ... Player count should be ignored for that count. ... I never said to simulate new NPCs or players, so
// don't do that. If you need newcomers for the economy simulate someone from our preexisting NPC list coming in from the
// country." So: each resident's death owes one child, born a few days after to a couple at home, one for one, a player's
// never; no NPC is ever made but by birth; a post that pays, empty eight days, goes to a wolf of the city with no post
// that pays (grown pups first) of its class, else to an unplaced NPC of the world's own list, of its class, who comes in
// from the Country: to the dead's home if empty, else to a household of its class with room, else the lodgings; never
// alone at the work; its purse from the Country's account, the money conserved. The catalogue is the real Data/Items
// (RATW_SOURCE_DIR): the posts' pay and so their class are the world's rules.
#include "RatwItems.h"
#include "RatwJsonDoc.h"
#include "RatwSociety.h"
#include "RatwWire.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>

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

ResidentSpec person(const std::string& id, const std::string& name, const std::string& role, const std::string& work, int age,
                    const std::string& sex, Spot home, Spot job, std::int64_t purse = 40)
{
    ResidentSpec r;
    r.id = id;
    r.name = name;
    r.role = role;
    r.workLabel = work;
    r.age = age;
    r.appearance.sex = sex;
    r.home = home;
    r.work = job;
    r.evening = home;
    r.serve = job;
    r.purse = purse;
    r.startHour = 8;
    r.endHour = 17;
    return r;
}
// Unplaced (doc 66, 2): no home, no body; in the roster with its label, purse and families.
ResidentSpec away(const std::string& id, const std::string& name, const std::string& role, const std::string& work, int age,
                  const std::string& sex, std::int64_t purse, const std::string& description = "A wolf of the Country.")
{
    auto r = person(id, name, role, work, age, sex, {}, {}, purse);
    r.description = description;
    return r;
}

struct Fates
{
    std::set<std::string> dead;
    std::vector<Spot> lodgings;
    CareerWorld world(const Society& s)
    {
        CareerWorld w;
        w.alive = [this](const std::string& id) { return !dead.count(id); };
        w.age = [&s](const std::string& id) { const auto* r = s.spec(id); return r ? r->age : 30; };
        w.regard = [](const std::string&, const std::string&) { return 0.0; };
        w.lodgings = [this](const std::string&) { return lodgings; };
        return w;
    }
};

// The Docks: two dockers (the Country pays them: lower work) with their pup at lessons (a parent each: grown, 17) in the
// Sump Rents; the Ferns keep the stall (a middle post) and live over it; the Ashes, a married couple of an age for
// children; Lady Vance at home in the Heights (upper). In the Country, unplaced: a fisher (lower, a labourer's trade
// as the docks are), a servant (lower), a baker of a town (middle), a Heights cousin (upper), an old field hand and a pup
// (neither of working age).
AuthoredRoster docks(bool withPup = true)
{
    AuthoredRoster roster;
    roster.residents = {
        person("ivar", "Ivar Nailham", "civilian", "unloading ships at the piers", 40, "male", {"sump_rents", 2.5, 2.5}, {"piers", 3.5, 3.5}),
        person("una", "Una Nailham", "civilian", "unloading ships at the piers", 38, "female", {"sump_rents", 3.5, 2.5}, {"piers", 4.5, 3.5}),
        person("moss", "Moss Fern", "merchant", "keeping the stall", 45, "male", {"stall_up", 2.5, 2.5}, {"stall", 2.5, 4.5}),
        person("birch", "Birch Ash", "civilian", "unloading ships at the piers", 32, "male", {"ash_house", 2.5, 2.5}, {"piers", 5.5, 3.5}),
        person("wren", "Wren Ash", "civilian", "unloading ships at the piers", 30, "female", {"ash_house", 3.5, 2.5}, {"piers", 6.5, 3.5}),
        person("lady", "Lady Vance", "civilian", "at home in the Heights", 50, "female", {"vance_house", 2.5, 2.5}, {"vance_house", 2.5, 2.5})};
    if (withPup)
        roster.residents.push_back(person("tam", "Tam Nailham", "civilian", "at lessons", 17, "male", {"sump_rents", 4.5, 2.5}, {"school", 2.5, 2.5}));
    roster.unplaced = {away("cty_fisher", "Kjell Saltdale", "civilian", "fishes the waters off Greywater", 30, "male", 49,
                            "A tall grey wolf, who fishes the waters off Greywater."),
                       away("rm_servant", "Sigrun Salthand", "civilian", "in service to the Ashbys", 31, "female", 36),
                       away("tn_baker", "Elm Wheatridge", "merchant", "baker at Downs Bakery", 33, "female", 86),
                       away("rm_cousin", "Solveig Ashby", "civilian", "at home in the Heights", 46, "female", 97),
                       away("cty_old", "Hob Furrow", "civilian", "works the fields at Hayward", 70, "male", 20),
                       away("cty_pup", "Pell Barleymead", "civilian", "plays about the town", 8, "male", 0)};
    roster.kin = {{"ivar", "una", "spouse"}, {"ivar", "tam", "parent"}, {"una", "tam", "parent"}, {"birch", "wren", "spouse"}};
    return roster;
}

int count(const std::vector<ResidentRequest>& rs, const std::string& kind)
{
    int n = 0;
    for (const auto& r : rs)
        n += r.kind == kind;
    return n;
}

void theClasses()
{
    Society s(Roster::None);
    s.configure(docks());
    Fates f;
    s.tendCareers(1.3, f.world(s));                   // (The families as written taken in.)
    const auto cls = [&](const std::string& post) { return s.postClass(*s.position(post)); };
    expect(cls("job:ivar") == "lower" && cls("job:moss") == "middle" && cls("job:lady") == "upper" && cls("job:tam").empty(),
           "a docker's post lower, the stall's middle, at home in the Heights upper, lessons no post that pays: " + cls("job:ivar") +
               " " + cls("job:moss") + " " + cls("job:lady") + " '" + cls("job:tam") + "'");
    expect(s.classOf("ivar") == "lower" && s.classOf("moss") == "middle" && s.classOf("lady") == "upper", "a founder: its post's");
    expect(s.classOf("tam") == "lower", "a pup at lessons: its parents' (" + s.classOf("tam") + ")");
    expect(s.classOf("cty_fisher") == "lower" && s.classOf("rm_servant") == "lower" && s.classOf("tn_baker") == "middle" &&
               s.classOf("rm_cousin") == "upper",
           "the unplaced by their own work as written: the Country's and service lower, a town's baker middle, the Heights upper");
}

void birthsFollowDeaths()
{
    Society s(Roster::None);
    s.configure(docks());
    s.addPlayer("player-one");                       // (A player: its death never counts.)
    Fates f;
    std::vector<ResidentRequest> born;
    const auto days = [&](int from, int to) {
        for (int day = from; day < to; ++day)
        {
            s.tendCareers(day + .3, f.world(s));
            for (const auto& r : s.takeRequests())
                if (r.kind == "birth")
                    born.push_back(r);
        }
    };
    days(1, 400);
    expect(born.empty(), "nobody dies, nobody is born: a world at its size has no children (two couples could)");
    // A player dies: nothing owed (player count is ignored).
    s.died("player-one", 400.2);
    expect(s.state().careers.birthsOwed.empty(), "a player's death owes no child");
    // Moss dies (a resident): one child owed, due 1 to 7 days after, to a couple at home (the Ashes or the Nailhams).
    f.dead.insert("moss");
    s.died("moss", 400.5);
    expect(s.state().careers.birthsOwed.size() == 1, "a resident's death owes one child");
    const double due = s.state().careers.birthsOwed.at("moss");
    expect(due >= 401 && due <= 407, "due a few days after: " + std::to_string(due));
    days(400, 420);
    expect(born.size() == 1 && born[0].inPlaceOf == "moss" && born[0].age == 0 && born[0].parents.size() == 2 &&
               (born[0].templateId == "wren" || born[0].templateId == "una") && (born[0].sex == "male" || born[0].sex == "female"),
           "one child, in his place, to a couple at home: " + std::to_string(born.size()));
    expect(s.state().careers.birthsOwed.empty(), "and nothing owed now");
    // Two more die: two children, never more, as soon as couples can have them (a year or so apart, three at most).
    for (const auto* who : {"lady", "tam"})
    {
        f.dead.insert(who);
        s.died(who, 420.5);
    }
    s.died("tam", 421);                               // (Dying twice owes nothing more.)
    days(420, 2000);
    expect(born.size() == 3, "a child for each death, one for one: " + std::to_string(born.size()) + " for 3");
    expect(s.state().careers.birthsOwed.empty(), "none left owed");
    // Revived before the child came: none owed; a child the host couldn't make: owed again.
    f.dead.insert("una");
    s.died("una", 2000.5);
    s.revived("una");
    f.dead.erase("una");
    expect(s.state().careers.birthsOwed.empty(), "brought back before their child was born: none owed");
    ResidentRequest lost;
    lost.kind = "birth";
    lost.inPlaceOf = "moss";
    s.birthFailed(lost);
    expect(s.state().careers.birthsOwed.count("moss") == 1, "a child the host could not make is owed again");
    // Saved and read back with the society (the game's checkpoint).
    const auto saved = wire::society(s.state());
    std::string error;
    json::Value parsed;
    expect(json::parse(json::dump(saved), parsed, error), "the save parses");
    const auto back = wire::readSociety(parsed);
    expect(back.careers.birthsOwed == s.state().careers.birthsOwed, "the children owed survive a save");
}

void noNpcIsMadeButByBirth()
{
    // A death, and a post that stands empty: only a child and an arrival from the world's own list are asked for; nobody
    // new. Every resident afterwards is one written, or one born.
    Society s(Roster::None);
    s.configure(docks(false));
    Fates f;
    std::set<std::string> written;
    for (const auto& r : s.authored().residents)
        written.insert(r.id);
    for (const auto& r : s.authored().unplaced)
        written.insert(r.id);
    f.dead.insert("ivar");
    s.died("ivar", 1.5);
    std::vector<ResidentRequest> asked;
    for (int day = 1; day < 40; ++day)
    {
        s.tendCareers(day + .3, f.world(s));
        for (auto& r : s.takeRequests())
        {
            expect(r.kind == "birth" || r.kind == "arrival", "only births and arrivals are asked for: " + r.kind);
            expect(r.kind != "arrival" || written.count(r.templateId), "an arrival is one of the world's own: " + r.templateId);
            if (r.kind == "arrival")
            {
                expect(s.arrive(r), "the arrival comes");
                s.welcome(r, r.templateId);
            }
            asked.push_back(r);
        }
    }
    expect(count(asked, "birth") == 1 && count(asked, "arrival") == 1, "one child for the dead, one arrival for his post");
    for (const auto& [id, life] : s.state().residents)
        expect(written.count(id), id + " is one of the world's own");
}

void postsFromTheCityFirst()
{
    // Ivar dies; nobody of the city is free (the Ashes and Una work, Moss keeps the stall) but Tam, 17, at lessons (no post
    // that pays): a grown pup of the Sump, the docks' class. At eight days, not before, he takes it.
    Society s(Roster::None);
    s.configure(docks());
    Fates f;
    s.tendCareers(1.3, f.world(s));
    f.dead.insert("ivar");
    s.died("ivar", 1.5);
    for (int day = 2; day <= 12; ++day)
    {
        const auto notes = s.tendCareers(day + .3, f.world(s));
        const bool now = s.jobOf("tam") && s.jobOf("tam")->id == "job:ivar";
        expect(!now || day >= 1.5 + Society::PostWaitDays, "not before eight days: day " + std::to_string(day));
        expect(count(s.takeRequests(), "arrival") == 0, "nobody is sent for while the city has one");
    }
    expect(s.jobOf("tam") && s.jobOf("tam")->id == "job:ivar", "the grown pup takes the docker's post");
    expect(s.fills().at("from the city") == 1, "from the city");
    expect(s.state().careers.positions.at("job:tam").holder.empty(), "leaving his lessons (no post that pays: never sent for)");
}

void postsFromTheCountry()
{
    // Without Tam, nobody in the city is free: at eight days an unplaced NPC of the docks' class is sent for, the one whose
    // trade suits it (a fisher for the piers, before a servant); never the old, a pup, or a wolf two classes away.
    Society s(Roster::None);
    s.configure(docks(false));
    Fates f;
    s.tendCareers(1.3, f.world(s));
    s.shift("treasury", Society::WharfFund, "", 0, 500, "test: the Country's purse");
    const auto money = s.moneySupply();
    const auto country = s.cash(Society::WharfFund);
    f.dead.insert("ivar");
    s.died("ivar", 1.5);
    std::vector<ResidentRequest> arrivals;
    for (int day = 2; day <= 14 && arrivals.empty(); ++day)
    {
        s.tendCareers(day + .3, f.world(s));
        for (auto& r : s.takeRequests())
            if (r.kind == "arrival")
            {
                expect(day >= 1.5 + Society::PostWaitDays, "not before eight days");
                arrivals.push_back(r);
            }
    }
    expect(arrivals.size() == 1 && arrivals[0].templateId == "cty_fisher" && arrivals[0].positionId == "job:ivar",
           "the fisher comes for the docker's post: " + (arrivals.empty() ? std::string("nobody") : arrivals[0].templateId));
    // Ivar's home has Una in it still: the fisher lodges with a household of his class with room (the Nailhams').
    const auto& r = arrivals[0];
    expect(r.home.cell == "sump_rents" && r.home.cell != "piers", "lodging with the Nailhams, not alone at the piers: " + r.home.cell);
    expect(r.purse == 30 && r.skill >= 35, "his own purse up to 30p; a labourer's trade, so he knows the work");
    const auto unplaced = s.unplacedCount();
    expect(s.arrive(r), "he comes in from the Country");
    expect(!s.arrive(r), "(once)");
    const auto note = s.welcome(r, r.templateId);
    expect(note.kind == "newcomer" && s.jobOf("cty_fisher") && s.jobOf("cty_fisher")->id == "job:ivar", "and takes the post");
    expect(s.unplacedCount() == unplaced - 1 && !s.unplaced("cty_fisher") && s.spec("cty_fisher"), "a resident now, no longer unplaced");
    const auto* spec = s.spec("cty_fisher");
    expect(spec->home.cell == "sump_rents" && spec->workLabel == "-" && !s.position("job:cty_fisher"),
           "placed without work: no post of his own made");
    expect(s.cash("cty_fisher") == 30 && s.cash(Society::WharfFund) == country - 30, "his 30p from the Country's account");
    expect(s.moneySupply() == money && s.conserved(), "the money conserved: nothing made");
    expect(s.fills().at("from the Country") == 1, "from the Country");
}

void whereArrivalsStay()
{
    const auto arrivalFor = [](AuthoredRoster roster, Fates& f, const std::string& dies) {
        Society s(Roster::None);
        s.configure(roster);
        s.tendCareers(1.3, f.world(s));
        f.dead.insert(dies);
        s.died(dies, 1.5);
        for (int day = 2; day <= 14; ++day)
        {
            s.tendCareers(day + .3, f.world(s));
            for (auto& r : s.takeRequests())
                if (r.kind == "arrival")
                    return r;
        }
        return ResidentRequest{};
    };
    // Birch lived alone: his home is empty now, so the arrival has it.
    {
        auto roster = docks(false);
        roster.residents[3].home = {"birch_house", 2.5, 2.5};
        roster.kin.pop_back();                       // (Not married.)
        Fates f;
        const auto r = arrivalFor(roster, f, "birch");
        expect(r.kind == "arrival" && r.home.cell == "birch_house", "the dead's home, empty: " + r.home.cell);
    }
    // No household of its class with room (Ivar and five pups, the Ashes and five: six and seven): the communal lodgings.
    {
        auto roster = docks(false);
        for (int n = 0; n < 5; ++n)
            roster.residents.push_back(person("kid" + std::to_string(n), "Kid Nailham", "civilian", "-", 6, "male", {"sump_rents", 5.5, 2.5}, {"sump_rents", 5.5, 2.5}));
        for (int n = 0; n < 5; ++n)
            roster.residents.push_back(person("ash" + std::to_string(n), "Kid Ash", "civilian", "-", 6, "male", {"ash_house", 5.5, 2.5}, {"ash_house", 5.5, 2.5}));
        for (int n = 0; n < 5; ++n)
            roster.kin.push_back({"ivar", "kid" + std::to_string(n), "parent"});
        for (int n = 0; n < 5; ++n)
            roster.kin.push_back({"birch", "ash" + std::to_string(n), "parent"});
        roster.unplaced.resize(2);                   // (Only the fisher and the servant: none of another class to come.)
        Fates f;
        f.lodgings = {{"anchor_lodgings", 4.5, 4.5}};
        const auto r = arrivalFor(roster, f, "una");
        expect(r.kind == "arrival" && r.home.cell == "anchor_lodgings", "the lodgings: " + r.home.cell);
        Fates none;
        const auto nowhere = arrivalFor(roster, none, "una");
        expect(nowhere.kind.empty(), "and with no lodgings either, nobody is sent for (no home is built)");
    }
    // Of the next class when none of its own is left: the docks with no lower wolf in the Country take the town's baker
    // (middle), never the Heights cousin (upper: two apart).
    {
        auto roster = docks(false);
        roster.unplaced.erase(roster.unplaced.begin(), roster.unplaced.begin() + 2);   // (The fisher and the servant.)
        Fates f;
        const auto r = arrivalFor(roster, f, "ivar");
        expect(r.kind == "arrival" && r.templateId == "tn_baker", "the next class: the baker (" + r.templateId + ")");
        expect(r.skill == 5, "new to the docks' work");
    }
}
// Doc 79, Phase 0: a post's kind and class without the city's loop (postKind, postRules in RatwCareers.cpp), the rules
// as city_loop.json's `wages` had them while that file is still read.
void postKindsWithoutTheLoop()
{
    const auto& kept = Society::postRules();
    const auto& file = items::cityLoop().wages;
    expect(kept.size() == file.size(), "the post rules, one for each of city_loop.json's wage rules");
    for (std::size_t i = 0; i < kept.size() && i < file.size(); ++i)
        expect(kept[i].name == file[i].name && kept[i].payer == file[i].payer && kept[i].wage == file[i].wage &&
                   kept[i].words == file[i].words,
               "post rule " + std::to_string(i) + " as city_loop.json's: " + kept[i].name);
    Society s(Roster::None);
    s.configure(docks());
    Fates f;
    s.tendCareers(1.3, f.world(s));
    const auto kind = [&](const std::string& post) { return s.postKind(*s.position(post)).kind; };
    expect(kind("job:moss") == "keeper" && kind("job:ivar") == "estate" && kind("job:lady") == "estate" && kind("job:tam").empty(),
           "a stall's keeper, the sea trade's and the Houses' people, lessons none: " + kind("job:moss") + " " + kind("job:ivar") +
               " " + kind("job:lady") + " '" + kind("job:tam") + "'");
    for (const auto& p : s.positions())
        expect(s.postKind(p).kind == s.payOf(p).kind, "payOf's kind is postKind's: " + p.id);
}

// Doc 79, Phase 0: with the rule "staged" on, the city's loop is off; households are fed by class, purses and the
// treasury are set each night, the welcome is made, and posts still fill (from the city at eight days, and from the
// Country with a purse of their own).
AuthoredRoster stagedDocks(bool withPup = true)
{
    auto roster = docks(withPup);
    roster.economy.rules["staged"] = 1;
    return roster;
}
double fed(const Society& s, const std::string& id)
{
    double n = 0;
    for (const auto& [item, count] : s.account(id)->stock)
        n += double(Society::nourishment(item)) * count;
    return n;
}
void theStagedEconomy()
{
    Society s(Roster::None);
    s.configure(stagedDocks());
    expect(s.staged() && !s.needsPaused() && !s.steering(), "the staged economy on, needs not paused, no steering");
    Fates f;
    s.tendCareers(1.3, f.world(s));
    // The welcome is made, not taken: with the treasury empty, a newcomer still starts with 20p, herbs and meals.
    if (const auto t = s.cash("treasury"); t > 0)
        s.shift("treasury", "moss", "", 0, t, "test: the treasury emptied");
    s.addPlayer("player-one");
    expect(s.cash("player-one") == Society::PlayerWelcome && Society::stock(*s.account("player-one"), "herbs") == 2 &&
               Society::stock(*s.account("player-one"), "meal") == Society::StartingMeals,
           "the welcome made with the treasury empty: " + std::to_string(s.cash("player-one")) + "p");
    expect(s.conserved(), "conserved (the welcome counted as made)");
    // The households: the homeless are one each; every resident is in one.
    const auto homes = s.households(true);
    std::size_t members = 0;
    for (const auto& [home, ids] : homes)
        members += ids.size();
    expect(members == s.state().residents.size(), "every resident in a household: " + std::to_string(members));
    // The day's turn works out every household's class in one pass (ClassPass): the same as one by one.
    const auto classes = s.householdClasses(homes);
    for (const auto& [home, ids] : homes)
        expect(classes.at(home) == (home.rfind("|", 0) == 0 ? std::string("homeless") : s.householdClass(ids)),
               "the one pass's class for " + home + " as householdClass's: " + classes.at(home));
    // Two days, hour by hour (no bodies: nobody eats): fed by class, two days at most; purses drawn to their class.
    const auto wagesBefore = std::count_if(s.state().ledger.begin(), s.state().ledger.end(),
                                           [](const EconomyEntry& e) { return e.kind == "wages"; });
    for (int h = 0; h <= 48; ++h)
        s.tick(1., 3 + h / 24. + 1e-4, 0, {});
    for (const auto& [id, life] : s.state().residents)
        expect(fed(s, id) >= Society::NourishADay && fed(s, id) < 2 * Society::NourishADay + 50,
               id + " fed a day or two: " + std::to_string(fed(s, id)));
    // (Doc 79, Phase 2: fed by band, each household the first of its band's foods there is.)
    for (const auto& [id, life] : s.state().residents)
    {
        const auto* st = s.statusOf(id);
        expect(st != nullptr, id + " has a status");
        bool eats = false;
        for (const auto& food : s.statusTable().rows[std::size_t(st->band)].food)
            eats |= Society::stock(*s.account(id), food) > 0;
        expect(eats, id + " holds its band's food (" + status::bandName(st->band) + ")");
    }
    expect(s.statusOf("lady")->band >= status::Comfortable, "the Heights' lady at least comfortable");
    // (Doc 79, Phase 2: toward their band's range, from the 40p each was written with.)
    for (const auto& [id, life] : s.state().residents)
    {
        if (id == "moss")
            continue;                               // (Given the treasury above: drawn down, not yet in range.)
        const auto& row = s.statusTable().rows[std::size_t(s.statusOf(id)->band)];
        expect(s.cash(id) >= std::min<std::int64_t>(40, row.purseLow) && s.cash(id) <= std::max<std::int64_t>(40, row.purseHigh) &&
                   s.cash(id) != 40,
               "purses drawn toward their band's: " + id + " " + std::to_string(s.cash(id)) + "p");
    }
    expect(s.cash("treasury") >= Society::TreasuryFloat && s.cash("treasury") <= Society::StagedTreasuryMost,
           "the treasury set within its range: " + std::to_string(s.cash("treasury")));
    const auto wagesAfter = std::count_if(s.state().ledger.begin(), s.state().ledger.end(),
                                          [](const EconomyEntry& e) { return e.kind == "wages"; });
    expect(wagesAfter == wagesBefore, "no wages paid: the city's loop is off");
    expect(s.conserved(), "conserved (the night's set counted as made and sunk until Phase 3)");
    expect(!s.moveWharfCredit(10, "dm", "test").ok, "the Wharf's credit is off");
}
void stagedPostsStillFill()
{
    {
        Society s(Roster::None);
        s.configure(stagedDocks());
        Fates f;
        s.tendCareers(1.3, f.world(s));
        f.dead.insert("ivar");
        s.died("ivar", 1.5);
        for (int day = 2; day <= 12; ++day)
        {
            s.tendCareers(day + .3, f.world(s));
            const bool now = s.jobOf("tam") && s.jobOf("tam")->id == "job:ivar";
            expect(!now || day >= 1.5 + Society::PostWaitDays, "staged: not before eight days");
        }
        expect(s.jobOf("tam") && s.jobOf("tam")->id == "job:ivar", "staged: the grown pup takes the docker's post at eight days");
    }
    Society s(Roster::None);
    s.configure(stagedDocks(false));
    Fates f;
    s.tendCareers(1.3, f.world(s));
    f.dead.insert("ivar");
    s.died("ivar", 1.5);
    std::vector<ResidentRequest> arrivals;
    for (int day = 2; day <= 14 && arrivals.empty(); ++day)
    {
        s.tendCareers(day + .3, f.world(s));
        for (auto& r : s.takeRequests())
            if (r.kind == "arrival")
                arrivals.push_back(r);
    }
    expect(arrivals.size() == 1 && arrivals[0].templateId == "cty_fisher", "staged: the fisher is sent for");
    expect(s.arrive(arrivals[0]), "staged: he comes in from the Country");
    s.welcome(arrivals[0], arrivals[0].templateId);
    expect(s.jobOf("cty_fisher") && s.jobOf("cty_fisher")->id == "job:ivar" && s.cash("cty_fisher") == 30,
           "staged: he takes the post with a purse of his own (30p), none from the Country's");
    expect(s.conserved(), "staged: conserved");
}
} // namespace


int main()
{
    setenv("RATW_DATA_DIR", (fs::path(RATW_SOURCE_DIR) / "Data").string().c_str(), 1);
    try
    {
        theClasses();
        birthsFollowDeaths();
        noNpcIsMadeButByBirth();
        postsFromTheCityFirst();
        postsFromTheCountry();
        whereArrivalsStay();
        postKindsWithoutTheLoop();
        theStagedEconomy();
        stagedPostsStillFill();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "People flow tests passed: " << checks << " checks.\n";
    return 0;
}
