// Careers (Docs/Design/26-living-npcs.md, Phase 4): positions outlive their holders, skill grows with work,
// apprentices are taken on and step up, estates go to the family, and nobody inherits a life overnight.
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
        revivedBeforeTheyAreReplaced();
        noFamilyGoesToTheTown();
        playersAsApprentices();
        savedAndRestored();
        griefInTheWorld();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Career tests passed: " << checks << " checks.\n";
    return 0;
}
