// Careers (Docs/Design/26-living-npcs.md, Phase 4): positions outlive their holders, skill grows with work,
// apprentices are taken on and step up, estates go to the family, and nobody inherits a life overnight.
#include "RatwItems.h"
#include "RatwSociety.h"
#include "RatwWorld.h"

#include <cmath>
#include <iostream>
#include <set>
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

ResidentSpec person(const std::string& id, const std::string& name, const std::string& role, const std::string& work,
                    int age, Spot home, Spot job)
{
    ResidentSpec r;
    r.id = id;
    r.name = name;
    r.role = role;
    r.workLabel = work;
    r.age = age;
    r.home = home;
    r.work = job;
    r.evening = home;
    r.serve = job;
    r.purse = 40;
    r.startHour = 8;
    r.endHour = 17;
    return r;
}

// The Brooks keep a stall; their neighbour Rook Fen does odd jobs; Wren Ash keeps another house.
AuthoredRoster town()
{
    AuthoredRoster roster;
    roster.residents = {
        person("hale", "Hale Brook", "merchant", "keeping the stall", 52, {"brook_house", 3.5, 3.5}, {"market", 4.5, 4.5}),
        person("tam", "Tam Brook", "civilian", "fetching for the stall", 16, {"brook_house", 4.5, 3.5}, {"market", 6.5, 4.5}),
        person("ivy", "Ivy Brook", "civilian", "weaving", 30, {"brook_house", 5.5, 3.5}, {"loom", 2.5, 2.5}),
        person("rook", "Rook Fen", "civilian", "odd jobs", 24, {"fen_house", 3.5, 3.5}, {"yard", 2.5, 2.5}),
        person("wren", "Wren Ash", "civilian", "baking", 40, {"ash_house", 3.5, 3.5}, {"bakery", 2.5, 2.5})};
    return roster;
}

struct Fates
{
    std::set<std::string> dead;
    std::map<std::string, std::map<std::string, double>> regard;
    CareerWorld world(const Society& s)
    {
        CareerWorld w;
        w.alive = [this](const std::string& id) { return !dead.count(id); };
        w.age = [&s](const std::string& id) { const auto* r = s.spec(id); return r ? r->age : 30; };
        w.regard = [this](const std::string& a, const std::string& b) {
            const auto x = regard.find(a);
            return x == regard.end() || !x->second.count(b) ? 0.0 : x->second.at(b);
        };
        return w;
    }
};

bool noted(const std::vector<CareerNote>& notes, const std::string& kind, const std::string& actor)
{
    for (const auto& n : notes)
        if (n.kind == kind && n.actor == actor)
            return true;
    return false;
}

void positionsAndFamilies()
{
    Society s(Roster::None);
    s.configure(town());
    expect(s.positions().size() == 5, "A position for every founding resident");
    expect(s.jobOf("hale") && s.jobOf("hale")->title == "keeping the stall" && s.jobOf("hale")->role == "merchant",
           "Everyone starts in the job they were written with");
    expect(s.skill("hale", "job:hale") > s.skill("tam", "job:tam"), "The older hand is the more skilled");
    expect(s.family("hale", "tam") && s.family("tam", "ivy"), "One house and one name: family");
    expect(!s.family("hale", "rook") && !s.family("hale", "wren"), "A neighbour is not family");
    expect(s.household("hale", "ivy") && !s.household("hale", "rook"), "Household is the home");
    expect(s.conserved(), "Money is conserved");
}

void apprenticeshipAndSuccession()
{
    Society s(Roster::None);
    s.configure(town());
    Fates fates;
    std::vector<CareerNote> all;
    // Hale (52) takes on young Tam, his family, who already works alongside him at the market.
    int day = 0;
    for (; day < 30 && !s.apprenticedTo("tam"); ++day)
        for (auto& n : s.tendCareers(day + .1, fates.world(s)))
            all.push_back(n);
    expect(s.apprenticedTo("tam") && s.apprenticedTo("tam")->id == "job:hale", "Hale takes Tam on as his apprentice");
    expect(noted(all, "apprenticeship", "tam"), "and the apprenticeship is noted");
    expect(!s.apprenticedTo("rook") && !s.apprenticedTo("ivy"), "Not a neighbour, not the grown weaver");
    // Tam learns beside Hale at the stall (practise at the rate decideAuthored uses beside a master).
    const auto before = s.skill("tam", "job:hale");
    for (int second = 0; second < 600 * 20; ++second)
        s.practise("tam", "job:hale", .006);
    expect(s.skill("tam", "job:hale") > before + 30, "An apprentice grows skilled with time beside the master");

    // Hale dies. The stall stands empty; nobody takes it overnight.
    const double died = day + .5;
    fates.dead.insert("hale");
    const auto grief = s.died("hale", died);
    expect(noted(grief, "vacancy", "hale") && !s.jobOf("hale"), "His position stands empty");
    expect(!s.jobOf("tam") || s.jobOf("tam")->id != "job:hale", "Not on the day he died");
    const auto haleCash = s.account("hale")->cash;
    const auto tamCash = s.account("tam")->cash, ivyCash = s.account("ivy")->cash;
    std::vector<CareerNote> after;
    for (int later = 1; later <= 3; ++later)
        for (auto& n : s.tendCareers(died + later, fates.world(s)))
            after.push_back(n);
    expect(noted(after, "estate settled", "hale"), "A day on, the estate is settled");
    expect(s.account("hale")->cash == 0 && s.account("tam")->cash + s.account("ivy")->cash == tamCash + ivyCash + haleCash,
           "shared among the family, nothing made or lost");
    expect(s.conserved(), "Money is still conserved");
    expect(s.jobOf("tam") && s.jobOf("tam")->id == "job:hale", "The apprentice steps up to keep the stall");
    expect(noted(after, "succession", "tam") && noted(after, "vacancy", "tam"),
           "and his own errand-running job falls empty in turn");
    expect(!s.apprenticedTo("tam"), "He is no longer an apprentice");

    // Five days on, someone out of work would take Tam's old job; nobody is, so it waits.
    for (int later = 4; later <= 12; ++later)
        s.tendCareers(died + later, fates.world(s));
    expect(!s.position("job:tam") || s.jobOf("tam")->id == "job:hale", "Tam keeps the stall");
}

// The user's succession rule (2026-10-14; doc 64, Phase 8): a masters' trade whose master dies with no apprentice goes to
// the best-trained wolf about who isn't in a master's place or an important post, to head it and work toward mastership;
// and a master already about (idle, or working below its level) is sought out first.
void theBestTrainedInherits()
{
    AuthoredRoster roster;
    roster.residents = {
        person("smith", "Garth Anvil", "merchant", "the smith at the forge", 55, {"anvil_house", 3.5, 3.5}, {"forge", 3.5, 3.5}),
        person("rook", "Rook Fen", "civilian", "odd jobs", 24, {"fen_house", 3.5, 3.5}, {"yard", 2.5, 2.5}),
        person("wren", "Wren Ash", "civilian", "baking", 40, {"ash_house", 3.5, 3.5}, {"bakery", 2.5, 2.5})};
    for (const bool master : {false, true})
    {
        Society s(Roster::None);
        s.configure(roster);
        Fates fates;
        expect(s.position("job:smith") && s.position("job:smith")->level >= Society::TradeMasters, "The forge is a masters' trade");
        // Rook has worked at a forge a while (better trained than Wren, whose baking is craft work too); or, the second
        // time, Wren is a master smith baking bread.
        s.practise("rook", "job:smith", 45);
        s.practise("wren", "job:smith", master ? 60 : 10);
        fates.dead.insert("smith");
        s.died("smith", 1.5);
        std::vector<CareerNote> notes;
        int took = -1;
        for (int day = 2; day <= 9 && took < 0; ++day)
        {
            for (auto& n : s.tendCareers(day + .1, fates.world(s)))
                notes.push_back(n);
            if (s.position("job:smith") && (s.jobOf("rook") == s.position("job:smith") || s.jobOf("wren") == s.position("job:smith")))
                took = day;
        }
        if (master)
            expect(s.jobOf("wren") && s.jobOf("wren")->id == "job:smith" && took <= 3,
                   "A master smith working below her trade is sought out for the forge within days (" + std::to_string(took) + ")");
        else
            expect(s.jobOf("rook") && s.jobOf("rook")->id == "job:smith" && took >= 6,
                   "With no apprentice and no master about, the best-trained takes the forge after five days (" + std::to_string(took) + ")");
        expect(noted(notes, "succession", master ? "wren" : "rook"), "and the succession is noted");
        expect(s.conserved(), "Money is conserved");
    }
}

void revivedBeforeTheyAreReplaced()
{
    Society s(Roster::None);
    s.configure(town());
    Fates fates;
    fates.dead.insert("wren");
    s.died("wren", 3.2);
    s.tendCareers(3.5, fates.world(s));
    fates.dead.erase("wren");
    const auto back = s.revived("wren");
    expect(noted(back, "returned to work", "wren") && s.jobOf("wren") && s.jobOf("wren")->id == "job:wren",
           "Brought back before anyone took it, she has her bakery again");
    const auto cash = s.account("wren")->cash;
    for (int later = 1; later <= 3; ++later)
        s.tendCareers(3.2 + later, fates.world(s));
    expect(s.account("wren")->cash == cash, "and her estate was never given away");
}

void noFamilyGoesToTheTown()
{
    Society s(Roster::None);
    s.configure(town());
    Fates fates;
    fates.dead.insert("rook");
    const auto treasury = s.account("treasury")->cash, rook = s.account("rook")->cash;
    s.died("rook", 1.1);
    s.tendCareers(2.2, fates.world(s));
    expect(s.account("treasury")->cash == treasury + rook, "With no family, the estate goes to the town");
    // Nobody is out of work, so his odd jobs wait; then the weaver's loom falls empty and Ivy... stays at her loom.
    for (int later = 3; later <= 10; ++later)
        s.tendCareers(1.1 + later, fates.world(s));
    expect(!s.position("job:rook") || s.state().careers.positions.at("job:rook").holder.empty(),
           "His work waits for someone free to take it");
}

void playersAsApprentices()
{
    Society s(Roster::None);
    s.configure(town());
    s.addPlayer("player-ada");
    Fates fates;
    expect(s.apprentice("player-ada", "job:wren", fates.world(s), 1).kind == "refused", "A stranger is not taken on");
    fates.regard["wren"]["player-ada"] = 40;
    const auto taken = s.apprentice("player-ada", "job:wren", fates.world(s), 1);
    expect(taken.kind == "apprenticeship" && s.apprenticedTo("player-ada")->id == "job:wren",
           "Someone the baker knows and trusts is taken on");
    expect(s.apprentice("player-ada", "job:hale", fates.world(s), 1).kind == "refused", "One apprenticeship at a time");
}

void savedAndRestored()
{
    Society s(Roster::None);
    s.configure(town());
    Fates fates;
    fates.dead.insert("hale");
    s.died("hale", 2);
    auto saved = s.state();
    Society again(Roster::None);
    again.configure(town());
    expect(again.restore(saved), "Careers restore");
    expect(!again.jobOf("hale") && again.state().careers.positions.at("job:hale").lastHolder == "hale" &&
               again.state().careers.estates.count("hale"),
           "with the empty position and the estate still to settle");
    auto old = s.state();
    old.careers = {};
    Society legacy(Roster::None);
    legacy.configure(town());
    expect(legacy.restore(old) && legacy.jobOf("hale"), "A save from before careers: everyone in their own job");
}

void skillFamiliesAndPace()
{
    expect(std::string(skillFamily("keeping the inn")) == "trade" && std::string(skillFamily("keeping the stall")) == "trade",
           "Shopkeeping is trade");
    expect(std::string(skillFamily("working the saws")) == "labour" && std::string(skillFamily("blowing glass")) == "craft" &&
               std::string(skillFamily("on the wall walk")) == "watch" && std::string(skillFamily("dreaming")) == "general",
           "and so on, with anything unknown general");
    expect(workPace(50) == 1 && workPace(90) < 1 && workPace(10) > 1 && workPace(100) >= .75 && workPace(-5) <= 1.25,
           "The skilled work faster, within bounds");
    Society s(Roster::None);
    s.configure(town());
    expect(s.familySkill("hale", "trade") == s.skill("hale", "job:hale") && s.familySkill("hale", "craft") == 0,
           "A resident's skill in a family is their best job of it");
}

void strangersAndChildren()
{
    Society s(Roster::None);
    auto roster = town();
    s.configure(roster);
    Fates fates;
    s.tendCareers(1.0, fates.world(s));             // (The town's number, as it was made.)
    // Rook dies; nobody here is free, so after eight days a stranger is sent for (the town is under its number).
    fates.dead.insert("rook");
    s.died("rook", 1.2);
    std::vector<ResidentRequest> wanted;
    for (int later = 1; later <= 12 && wanted.empty(); ++later)
    {
        s.tendCareers(1.2 + later, fates.world(s));
        wanted = s.takeRequests();
        expect(wanted.empty() || later >= 8, "Nobody is sent for before eight days");
    }
    expect(wanted.size() == 1 && wanted[0].kind == "newcomer" && wanted[0].positionId == "job:rook" &&
               wanted[0].templateId == "rook" && wanted[0].name.find(' ') != std::string::npos && wanted[0].age >= 22,
           "A stranger is sent for, for his odd jobs");
    s.tendCareers(40, fates.world(s));
    expect(s.takeRequests().empty(), "and only once");
    // The host makes them (here: a second roster with them in it), and they take up the post.
    auto newcomer = roster.residents[3];
    newcomer.id = "stranger";
    newcomer.name = wanted[0].name;
    newcomer.workLabel = "-";
    roster.residents.push_back(newcomer);
    Society made(Roster::None);
    made.configure(roster);
    expect(s.adoptResident(made, "stranger"), "The stranger arrives");
    expect(!s.position("job:stranger"), "with no job of their own");
    const auto note = s.welcome(wanted[0], "stranger");
    expect(note.kind == "newcomer" && s.jobOf("stranger") && s.jobOf("stranger")->id == "job:rook",
           "and takes up the post they came for");
    expect(s.skill("stranger", "job:rook") >= 35, "knowing something of the work");

    // A married couple at home together: a child to take the place of one who died (the town stays about its size:
    // doc 62, the user 2026-10-13), never more than three.
    auto couple = town();
    couple.residents[4].appearance.sex = "female";                     // Wren Ash, 40.
    couple.residents.push_back(person("birch", "Birch Ash", "civilian", "hauling", 42, {"ash_house", 4.5, 3.5}, {"yard", 3.5, 2.5}));
    Society home(Roster::None);
    home.configure(couple);
    expect(home.marry("wren", "birch") && home.spouse("wren") && *home.spouse("wren") == "birch", "Wren marries Birch");
    expect(!home.marry("wren", "rook"), "and nobody else");
    expect(home.family("birch", "wren"), "Married, they are family");
    Fates alive;
    std::vector<ResidentRequest> births;
    for (int day = 0; day < 400; ++day)
    {
        home.tendCareers(day + .3, alive.world(home));
        for (auto& r : home.takeRequests())
            if (r.kind == "birth")
                births.push_back(r);
    }
    expect(births.empty(), "A town at the size it was made has no children");
    alive.dead.insert("rook");                                         // One of the town dies.
    for (int day = 400; day < 1600; ++day)
    {
        home.tendCareers(day + .3, alive.world(home));
        for (auto& r : home.takeRequests())
            if (r.kind == "birth")
                births.push_back(r);
    }
    // (Here no child is ever welcomed in, so the town stays short: a child a year or so, up to the couple's three.)
    expect(!births.empty() && births.size() <= 3, "Children come when it's short, but no more than three: " + std::to_string(births.size()));
    expect(births[0].age == 0 && births[0].templateId == "wren" && births[0].name.find("Ash") != std::string::npos &&
               births[0].parents.size() == 2 && births[0].home.cell == "ash_house",
           "A child of Wren and Birch, at home: " + births[0].name);
}

// Families as authored (doc 62, decision 8): kept by the society as those made in play are.
void foundingFamilies()
{
    auto t = town();
    t.residents[4].appearance.sex = "female";                          // Wren Ash, 40.
    t.residents.push_back(person("birch", "Birch Ash", "civilian", "hauling", 42, {"ash_house", 4.5, 3.5}, {"yard", 3.5, 2.5}));
    t.residents.push_back(person("sprig", "Sprig Ash", "civilian", "playing", 6, {"ash_house", 4.5, 3.5}, {"yard", 3.5, 2.5}));
    t.kin = {{"birch", "wren", "spouse"}, {"birch", "sprig", "parent"}, {"wren", "sprig", "parent"}};
    Society s(Roster::None);
    s.configure(t);
    Fates alive;
    s.tendCareers(10.3, alive.world(s));
    expect(s.spouse("wren") && *s.spouse("wren") == "birch", "a founding couple is married");
    expect(s.family("sprig", "birch") && s.state().careers.parents.at("sprig").size() == 2, "and their child is theirs");
    expect(s.state().careers.births.at("birch|wren") == 1, "who counts toward their three");
    expect(s.state().careers.lastBirth.at("birch|wren") <= 10.3 - 6 * 365 + 1, "born when the child was");
    s.tendCareers(11.3, alive.world(s));
    expect(s.state().careers.parents.at("sprig").size() == 2 && s.state().careers.births.at("birch|wren") == 1,
           "and taking them again each day changes nothing");
}

// LandPostsFilled (round 16, lane 2; the user, 2026-10-30), with work by the hours: a farm's fields lie in the country's
// region, its town's wolves in the town's, so the passes' region test (CareerWorld.near, as RatwWorld's: equal regions
// pass, unknown ones pass) leaves an empty farm to its apprentice and family alone. A land post empty LandPostWait days
// whose goods are wanted goes to a wolf of its own town: one out of work first, else one poor in an unpaid post, never a
// House's family (its purse in the thousands). The towns by cell as the world's day plans give them: the fields none
// (a farmer belongs to the town it lives in), "far_" cells the far town's, the rest the town's.
CareerWorld byRegion(Fates& fates, const Society& s)
{
    auto w = fates.world(s);
    w.near = [](const std::string& workCell, const std::string& homeCell) {
        const auto region = [](const std::string& cell) {
            return cell.rfind("fields", 0) == 0 ? std::string("warmwood") : cell.rfind("far_", 0) == 0 ? std::string("far")
                                                                              : std::string("town");
        };
        return region(workCell) == region(homeCell);
    };
    return w;
}

Society landTown(const std::vector<ResidentSpec>& residents)
{
    AuthoredRoster roster;
    roster.residents = residents;
    Society s(Roster::None);
    s.configure(roster);
    LifeDay day;
    day.communityOf = [](const std::string& cell) {
        return cell.rfind("fields", 0) == 0 ? std::string() : cell.rfind("far_", 0) == 0 ? std::string("far") : std::string("town");
    };
    s.setDay(day);
    return s;
}

ResidentSpec unpaid(ResidentSpec r, std::int64_t purse)
{
    r.paid = false;
    r.purse = purse;
    return r;
}

void anEmptyFieldTakenInTown()
{
    Society::setWorkByHours(true);
    const auto farmer = person("farmer", "Fern Furrow", "civilian", "farms the valley fields", 45, {"farmhouse", 3.5, 3.5}, {"fields", 4.5, 4.5});
    const auto idle = person("idle", "Ash Idle", "civilian", "-", 30, {"row_a", 3.5, 3.5}, {"row_a", 3.5, 3.5});
    const auto beggar = unpaid(person("beggar", "Bryn Alms", "civilian", "begging in the market", 40, {"row_b", 3.5, 3.5}, {"market", 4.5, 4.5}), 20);
    const auto heir = unpaid(person("heir", "Hollis Fell", "civilian", "at home in the Heights", 35, {"heights", 3.5, 3.5}, {"heights", 4.5, 4.5}), 5000);
    // (And the House's head, and its daughter out of work: of its head's household, the House's family, whatever her post.)
    const auto head = unpaid(person("head", "Isolde Fell", "civilian", "ruling House Fell", 55, {"heights", 4.5, 3.5}, {"heights", 5.5, 4.5}), 5000);
    const auto kin = unpaid(person("kin", "Wenna Fell", "civilian", "-", 34, {"heights", 5.5, 3.5}, {"heights", 5.5, 4.5}), 200);
    // (Of the far town, out of work, the dead farmer's friend: by the score it would come first, but it isn't the post's.)
    const auto far = person("far", "Fen Faraway", "civilian", "-", 30, {"far_row", 3.5, 3.5}, {"far_row", 3.5, 3.5});
    // Who takes the farm by day 30, and on which day; and the notes.
    const auto run = [&](bool rule, const std::vector<ResidentSpec>& residents, std::vector<CareerNote>& notes) {
        Society::wageRulesOff(rule ? 0 : Society::LandPostsFilled);
        auto s = landTown(residents);
        Fates fates;
        fates.regard["farmer"]["far"] = 50;
        expect(s.position("job:farmer") && items::producerFor(s.position("job:farmer")->title) && s.position("job:farmer")->role != "merchant",
               "the farm is a land post");
        s.tendCareers(1.1, byRegion(fates, s));     // (The town's number, as it was made.)
        fates.dead.insert("farmer");
        s.died("farmer", 1.5);
        std::pair<std::string, int> took;
        for (int day = 2; day <= 30 && took.first.empty(); ++day)
        {
            for (auto& n : s.tendCareers(day + .1, byRegion(fates, s)))
                notes.push_back(n);
            if (const auto& held = s.state().careers.positions.at("job:farmer").holder; !held.empty())
                took = {held, day};
            expect(s.conserved(), "money is conserved");
        }
        Society::wageRulesOff(0);
        return took;
    };
    const auto how = [](const std::vector<CareerNote>& notes, const std::string& who) {
        for (const auto& n : notes)
            if (n.kind == "succession" && n.actor == who)
                return n.detail;
        return std::string();
    };
    std::vector<CareerNote> notes;
    // (Dead at 1.5: waited 7 at 8.5, so the day's pass at 9.1 is the first after.)
    auto took = run(true, {farmer, idle, beggar, heir, far}, notes);
    expect(took.first == "idle" && took.second == 9,
           "a week on, the town's wolf out of work takes up the farm (" + took.first + " on day " + std::to_string(took.second) + ")");
    expect(how(notes, "idle") == "farms the valley fields: someone in town takes up the land", "noted as someone in town taking up the land");
    notes.clear();
    took = run(true, {farmer, beggar, heir, far}, notes);
    expect(took.first == "beggar" && took.second == 9, "with nobody out of work in town, the beggar (" + took.first + ")");
    expect(noted(notes, "vacancy", "beggar"), "whose begging falls empty in turn");
    notes.clear();
    took = run(true, {farmer, heir, far}, notes);
    expect(took.first.empty(), "never the House's family, its purse in the thousands, nor a wolf of the far town (" + took.first + ")");
    notes.clear();
    took = run(true, {farmer, head, heir, kin, far}, notes);
    expect(took.first.empty(), "nor the House's daughter out of work, of its head's household (" + took.first + ")");
    notes.clear();
    took = run(false, {farmer, idle, beggar, heir, far}, notes);
    expect(took.first.empty(), "without the rule the farm stands empty through day 30: its fields aren't the town's region (" + took.first + ")");
    Society::setWorkByHours(false);
}

// LandPostsFilled's question: whether the empty farm's goods are wanted in its town. Its town's other farm turning away
// none of its buyers, it waits (a dead market stays dead); a tenth of them (missedAt, the days' passes making the running
// average missedRate), it's taken a week on; with no other farm of its goods in its town, it's taken.
void aFieldWithNoBuyersWaits()
{
    Society::setWorkByHours(true);
    const auto run = [&](bool missed, bool otherInTown) {
        const auto a = person("a", "Alder Acre", "civilian", "farms the valley fields", 45, {"farmhouse_a", 3.5, 3.5}, {"fields_a", 4.5, 4.5});
        const auto b = person("b", "Bram Barley", "civilian", "farms the valley fields", 45,
                              {otherInTown ? "farmhouse_b" : "far_farmhouse_b", 3.5, 3.5}, {"fields_b", 4.5, 4.5});
        auto miller = person("miller", "Moll Grist", "merchant", "runs The Ford Mill", 50, {"mill_house", 3.5, 3.5}, {"mill", 4.5, 4.5});
        miller.purse = 5000;
        const auto idle = person("idle", "Ash Idle", "civilian", "-", 30, {"row_a", 3.5, 3.5}, {"row_a", 3.5, 3.5});
        auto s = landTown({a, b, miller, idle});
        auto st = s.state();
        st.accounts.at(s.tillOf("b")).stock["wheat"] = 900;
        expect(s.restore(st), "the other farm's barn restores");
        Fates fates;
        s.tendCareers(1.1, byRegion(fates, s));
        fates.dead.insert("a");
        s.died("a", 1.5);
        s.sellersListed("wheat");                    // (Who sells what afresh, after the death: the day's pass would.)
        int took = -1;
        for (int day = 2; day <= 30 && took < 0; ++day)
        {
            // The other farm sells the mill 20 wheat a day, and turns away 2 more (or none).
            expect(s.sale(s.tillOf("b"), s.tillOf("miller"), "wheat", 20, 1, "test: the mill's order"), "the mill buys the other farm's wheat");
            if (missed)
                s.missedAt("town", "wheat", 2);
            s.spoil(day);
            s.tendCareers(day + .1, byRegion(fates, s));
            if (s.state().careers.positions.at("job:a").holder == "idle")
                took = day;
            expect(s.conserved(), "money is conserved");
        }
        if (otherInTown)
            expect(s.missedRate(s.tillOf("b"), "wheat") > (missed ? 1.8 : -1) && s.missedRate(s.tillOf("b"), "wheat") < (missed ? 2 : .01),
                   "the other farm's buyers turned away, a day: " + std::to_string(s.missedRate(s.tillOf("b"), "wheat")));
        return took;
    };
    expect(run(false, true) < 0, "its town's other farm turning none away, the empty one waits through day 30");
    expect(run(true, true) == 9, "a tenth of its buyers turned away, it's taken a week on");
    expect(run(false, false) == 9, "with no other farm of its goods in its town, it's taken a week on");
    Society::wageRulesOff(0);
    Society::setWorkByHours(false);
}

// In a world: a death empties a job and those close to the dead grieve.
void griefInTheWorld()
{
    World w;                                       // The authored demo world has no positions (Roster::Demo)...
    expect(w.society().positions().empty(), "The demo tavern has no careers");
    w.bonds().change("npc_cook", "npc_keeper", {40, 20, 60, 0, 0}, 0);
    expect(w.setDead("npc_keeper", true).ok, "A death");
    const auto events = w.takeEvents();
    bool mourned = false;
    for (const auto& e : events)
        mourned |= e.kind == "mourning" && e.actor == "npc_cook" && e.target == "npc_keeper";
    expect(mourned, "A friend grieves");
    expect(w.society().mourning("npc_cook") && w.society().mourning("npc_cook")->whom == "npc_keeper",
           "and the society knows whom for");
}
} // namespace

int main()
{
    try
    {
        positionsAndFamilies();
        apprenticeshipAndSuccession();
        theBestTrainedInherits();
        revivedBeforeTheyAreReplaced();
        noFamilyGoesToTheTown();
        playersAsApprentices();
        savedAndRestored();
        foundingFamilies();
        griefInTheWorld();
        skillFamiliesAndPace();
        strangersAndChildren();
        anEmptyFieldTakenInTown();
        aFieldWithNoBuyersWaits();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Career tests passed: " << checks << " checks.\n";
    return 0;
}
