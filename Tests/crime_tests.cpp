// Crime and law (Core/RatwCrime.h; Docs/Design/26-living-npcs.md, Phase 7), in Greyfen: Wren keeps shop, four guards
// keep the Watch.
#include "RatwCheckpoint.h"
#include "battle_play.h"
#include "RatwWorld.h"

#include <algorithm>
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
World town()
{
    World world;
    const auto loaded = world.loadWorldFile(RATW_SOURCE_DIR "/Data/Worlds/Greyfen/world.ratw");
    expect(loaded.ok, "Town loads: " + loaded.message);
    world.setTimeOfDay(11);
    for (int i = 0; i < 240 && world.society().resident("sloe")->task != "patrol"; ++i)
        world.tick(1);                              // Residents take up their day's work.
    return world;
}
std::int64_t cash(const World& w, const std::string& id)
{
    const auto* a = w.society().account(id);
    return a ? a->cash : 0;
}
// Puts someone beside someone else, facing them.
void beside(World& w, const std::string& id, const std::string& other, double dx = 1, double dy = 0)
{
    auto& e = *w.entity(id);
    const auto& o = *w.entity(other);
    e.cellId = o.cellId;
    e.position = {o.position.x + dx, o.position.y + dy};
    e.velocity = {};
    e.facing = std::atan2(o.position.y - e.position.y, o.position.x - e.position.x);
}
// Pickpockets until a theft is noticed by someone (a clumsy try always is), or the tries run out.
std::string stealUntilSeen(World& w, const std::string& thief, const std::string& victim)
{
    for (int i = 0; i < 40; ++i)
    {
        const auto done = w.steal(thief, victim);
        expect(done.ok, "The try is allowed: " + done.message);
        const auto& inc = w.crime().incidents.back();
        if (!inc.witnesses.empty())
            return inc.id;
        w.tick(4.1);
        beside(w, thief, victim);
    }
    throw std::runtime_error("No theft was ever noticed");
}
// Money from or to the treasury, so the money supply stays conserved.
void give(World& w, const std::string& id, std::int64_t coins)
{
    if (coins > 0)
        w.society().shift("treasury", id, "", 0, coins, "test");
    else if (coins < 0)
        w.society().shift(id, "treasury", "", 0, -coins, "test");
}
// Everyone but the Watch and the shop starving, broke and without food.
void starve(World& w)
{
    auto state = w.society().state();
    for (auto& [id, life] : state.residents)
        if (life.role != "guard" && !w.society().merchant(id))
        {
            life.hunger = 90;
            auto& a = state.accounts[id];
            state.accounts["treasury"].cash += a.cash;
            a.cash = 0;
            a.stock.erase("meal");
            if (auto larder = state.accounts.find(Society::homeStore(life.homeCell, "larder")); larder != state.accounts.end())
                larder->second.stock.erase("meal");    // (Nothing at home either: doc 36's larders.)
        }
    expect(w.society().restore(state), "The society takes the change");
}
bool hasEvent(World& w, const std::string& kind, std::vector<WorldEvent>& seen)
{
    for (auto& e : w.takeEvents())
        seen.push_back(e);
    return std::any_of(seen.begin(), seen.end(), [&](const WorldEvent& e) { return e.kind == kind; });
}

void refusals()
{
    auto w = town();
    w.addPlayer("player-ada", "Ada");
    w.addPlayer("player-bo", "Bo");
    beside(w, "player-ada", "player-bo");
    expect(!w.steal("player-ada", "player-bo").ok, "No stealing from other players");
    expect(w.attack("player-ada", "player-bo").ok && !w.inBattle("player-ada") && w.challengeTo("player-bo"),
           "Another player is challenged, not set on");
    expect(w.answerChallenge("player-bo", false).ok && !w.inBattle("player-bo"), "and a no is a no");
    beside(w, "player-ada", "wren", 5);
    expect(!w.steal("player-ada", "wren").ok, "A purse out of reach is safe");
    beside(w, "player-ada", "wren");
    expect(w.steal("player-ada", "wren").ok && !w.steal("player-ada", "wren").ok, "One try at a time");
    expect(w.society().conserved(), "Money is conserved");
}

void theftBeforeTheWatch()
{
    auto w = town();
    w.addPlayer("player-ada", "Ada");
    beside(w, "player-ada", "wren");
    beside(w, "sloe", "wren", -1, 2);          // A guard on patrol, in the shop (on open floor).                
    expect(w.guardOnDuty("sloe"), "Sloe is on duty: " + w.society().resident("sloe")->task + " hurt " + std::to_string(w.entity("sloe")->hurt));
    const auto id = stealUntilSeen(w, "player-ada", "wren");
    const auto* warrant = w.warrantFor("player-ada");
    std::string who;
    for (const auto& x : w.crime().incidents.back().witnesses)
        who += x.id + (x.identified ? "+" : "-") + std::to_string(x.clarity) + (x.reported ? "R " : " ");
    expect(warrant && warrant->town == "greyfen", "A theft the Watch itself saw makes a warrant: " + w.crime().incidents.back().kind + " " + w.crime().incidents.back().town + " " + who);
    expect(std::find(warrant->incidents.begin(), warrant->incidents.end(), id) != warrant->incidents.end(), "for that incident");
    const auto owed = w.owedBy(*warrant);
    expect(owed >= 3, "They ask restitution and a fine");
    // The guard stops the thief; paying settles it.
    std::vector<WorldEvent> seen;
    bool stopped = false;
    for (int i = 0; i < 20 && !stopped; ++i)
    {
        w.tick(.5);
        stopped = hasEvent(w, "stopped by the watch", seen);
    }
    const auto* sl = w.entity("sloe");
    const auto* ad = w.entity("player-ada");
    expect(stopped, "The guard stops them: sloe " + sl->cellId + " " + std::to_string(sl->position.x) + "," + std::to_string(sl->position.y) +
                        " " + sl->activity + " task " + w.society().resident("sloe")->task + " ada " + ad->cellId + " " +
                        std::to_string(ad->position.x) + "," + std::to_string(ad->position.y) + " off " + std::to_string(sl->offstage));
    give(w, "player-ada", owed + 5);
    const auto wrenBefore = cash(w, "wren");
    const auto paid = w.payFine("player-ada", "sloe");
    expect(paid.ok, "The fine is paid: " + paid.message);
    expect(!w.warrantFor("player-ada") && !w.custodyOf("player-ada"), "and the warrant is gone");
    expect(cash(w, "wren") >= wrenBefore || w.crime().incidents.back().kind == "attempted theft", "Wren gets back what was taken");
    expect(w.society().conserved(), "Money is conserved");
    const auto& settled = *std::find_if(w.crime().incidents.begin(), w.crime().incidents.end(),
                                        [&](const Incident& i) { return i.id == id; });
    expect(settled.status == "settled", "The incident is settled");
}

void unpaidMeansGaol()
{
    auto w = town();
    w.addPlayer("player-ada", "Ada");
    beside(w, "player-ada", "wren");
    beside(w, "sloe", "wren", -1, 2);          // A guard on patrol, in the shop (on open floor).
    stealUntilSeen(w, "player-ada", "wren");
    expect(w.warrantFor("player-ada"), "Wanted");
    // Broke, and with no intention of paying: the deadline passes.
    give(w, "player-ada", -cash(w, "player-ada"));
    for (int i = 0; i < 100 && !w.custodyOf("player-ada"); ++i)
        w.tick(.5);
    const auto* held = w.custodyOf("player-ada");
    expect(held, "An unpaid fine means the gaol");
    expect(!w.warrantFor("player-ada"), "Custody takes the place of the warrant");
    const auto* p = w.entity("player-ada");
    expect(p->cellId == held->cell && p->stamina <= 1, "They are dragged there, spent");
    expect(!w.steal("player-ada", "sloe").ok, "Nobody steals from the gaol");
    // Time served: two game hours.
    const double hours = (held->until - w.calendarDays()) * 24;
    expect(hours > 1.5 && hours <= 2.01, "Two game hours: " + std::to_string(hours));
    for (int i = 0; i < 2 * 600 + 40 && w.custodyOf("player-ada"); ++i)
        w.tick(1);
    expect(!w.custodyOf("player-ada"), "Then they go free");
    expect(w.society().conserved(), "Money is conserved");
}

// A wanted wolf lying Downed isn't asked to pay: the watch binds their wounds and carries them in (playtest,
// October 4: a downed player was left to bleed out in the cell).
void theFallenAreCarriedIn()
{
    auto w = town();
    w.addPlayer("player-ada", "Ada");
    beside(w, "player-ada", "wren");
    beside(w, "sloe", "wren", -1, 2);
    stealUntilSeen(w, "player-ada", "wren");
    expect(w.warrantFor("player-ada"), "Wanted");
    auto* p = w.entity("player-ada");
    p->hurt = 100;
    p->downedLeft = 600;
    p->posture = "lying";
    std::vector<WorldEvent> seen;
    bool asked = false;
    for (int i = 0; i < 60 && !w.custodyOf("player-ada"); ++i)
    {
        w.tick(.5);
        asked = asked || hasEvent(w, "stopped by the watch", seen);
    }
    expect(w.custodyOf("player-ada"), "Down, they are taken in");
    expect(!asked, "without being asked to pay");
    expect(p->downedLeft == 0 && std::abs(p->hurt - (100 - battle::TendedHealth)) < 1e-9 && p->posture == "standing",
           "their wounds bound: up again, hurt but not bleeding");
}

// A guard who chases a wanted resident and reaches them takes them in, and the chase ends cleanly. (Taking them in
// ended the chase under the loop's feet: the server hung on a simulated week's third day.)
void residentChasedDown()
{
    auto w = town();
    beside(w, "rook", "wren");
    beside(w, "sloe", "wren", -1, 2);          // A guard on patrol, in the shop (on open floor).
    const auto id = stealUntilSeen(w, "rook", "wren");
    give(w, "rook", -cash(w, "rook"));          // Whatever was taken is spent: nothing to pay a fine with.
    // Wren tells the guard; the guard goes after Rook, reaches them and, as they can't pay, takes them in.
    for (int i = 0; i < 200 && !w.custodyOf("rook"); ++i)
    {
        beside(w, "sloe", "wren", -1, 0);
        beside(w, "rook", "sloe", 1.2, 0);
        w.tick(.25);
    }
    const Incident* inc = nullptr;
    for (const auto& x : w.crime().incidents)
        if (x.id == id)
            inc = &x;
    expect(w.custodyOf("rook") && !w.warrantFor("rook") && inc && inc->status == "settled",
           "The guard catches the broke resident, who goes to the gaol: " + (inc ? inc->status : std::string("?")));
    for (int i = 0; i < 20; ++i)
        w.tick(.5);
    expect(w.custodyOf("rook"), "and stays there, the chase over");
    expect(w.society().conserved(), "Money is conserved");
}

void assaultBeatsDown()
{
    // Setting on a resident is an assault: a fight in an arena (Docs/Design/33-combat.md), and a crime.
    auto w = town();
    w.addPlayer("player-ada", "Ada");
    auto* ada = w.entity("player-ada");
    ada->strength = 100;
    ada->dexterity = 100;
    beside(w, "player-ada", "wren");
    const auto started = w.attack("player-ada", "wren");
    expect(started.ok && w.inBattle("player-ada") && w.inBattle("wren"), "An attack starts a fight: " + started.message);
    const auto assaults = [&] {
        return std::count_if(w.crime().incidents.begin(), w.crime().incidents.end(), [](const Incident& i) { return i.kind == "assault"; });
    };
    expect(assaults() == 1, "It is an assault");
    expect(w.bonds().find("wren", "player-ada") && w.bonds().find("wren", "player-ada")->fear > 0, "Wren fears Ada now");
    std::vector<WorldEvent> seen;
    bool stoppedMidFight = false;
    for (int i = 0; i < 4000 && w.inBattle("player-ada"); ++i)
    {
        w.entity("player-ada")->hurt = 0;           // (Not a test of losing.)
        test::playTurn(w, "player-ada");
        w.tick(.25);
        stoppedMidFight = stoppedMidFight || (w.inBattle("player-ada") && hasEvent(w, "stopped by the watch", seen));
    }
    expect(!w.inBattle("player-ada"), "The fight ends");
    expect(!stoppedMidFight, "The watch doesn't stop her in the middle of it (doc 37): it waits for the end");
    const auto* wren = w.entity("wren");
    expect(wren->downedLeft > 0 || wren->hurt > 0, "Wren is hurt, or down");
    expect(!wren->dead, "and lives");
    expect(assaults() == 1, "One fight is one incident");
    // Wren tells the Watch when a guard comes by.
    beside(w, "sloe", "wren", -1, 2);
    for (int i = 0; i < 10; ++i)
        w.tick(.5);
    expect(w.warrantFor("player-ada"), "The victim's account, told to a guard, makes a warrant");
}

void downedAndUp()
{
    // A guard fights back, and goes down; a resident left Downed gets up again by itself (once a day).
    auto w = town();
    w.addPlayer("player-ada", "Ada");
    auto* ada = w.entity("player-ada");
    ada->strength = 100;
    ada->dexterity = 100;
    beside(w, "player-ada", "sloe");
    expect(w.attack("player-ada", "sloe").ok, "Ada goes for Sloe");
    std::vector<WorldEvent> seen;
    bool downed = false;
    std::string guard;                              // Whichever guard goes down (Sloe may run, and Harrow come in).
    for (int i = 0; i < 4000 && w.inBattle("player-ada") && !downed; ++i)
    {
        w.entity("player-ada")->hurt = 0;
        test::playTurn(w, "player-ada");
        w.tick(.25);
        for (const char* id : {"sloe", "harrow", "birch", "tamsin"})
            if (w.entity(id) && w.entity(id)->downedLeft > 0)
                guard = id;
        downed = !guard.empty();
    }
    expect(downed, "Enough bites put a guard down");
    expect(hasEvent(w, "downed", seen), "It is an event");
    expect(!w.entity(guard)->dead, "Down is not dead");
    for (int i = 0; i < 2000 && w.inBattle("player-ada"); ++i)
    {
        w.entity("player-ada")->hurt = 0;
        test::playTurn(w, "player-ada");
        w.tick(.25);
    }
    // Out of the fight: a guard who hasn't used today's getting-up struggles up alone; one who has stays down until tended.
    const bool canRise = w.recoveryAvailable(*w.entity(guard));
    for (int i = 0; i < 60; ++i)
        w.tick(1);
    if (canRise)
        expect(w.entity(guard)->downedLeft <= 0 && w.entity(guard)->hurt > 70 && w.entity(guard)->hurt <= 85, "The guard struggles up, hurt");
    else
    {
        expect(w.entity(guard)->downedLeft > 0, "Down twice in a day: the guard stays down");
        for (int i = 0; i < 10; ++i)
            w.tick(1);                              // (Past the five seconds' settling after a fight.)
        beside(w, "player-ada", guard);
        const auto tended = w.tendWounds("player-ada", guard);
        expect(tended.ok, "Ada can tend them: " + tended.message);
        for (int i = 0; i < 12; ++i)
            w.tick(1);
        expect(w.entity(guard)->downedLeft <= 0 && !w.entity(guard)->dead && w.entity(guard)->hurt > 75 && w.entity(guard)->hurt <= 80,
               "and the guard is back on their feet, hurt");
    }
}

void needMakesThieves()
{
    auto w = town();
    starve(w);
    std::vector<WorldEvent> seen;
    bool tried = false;
    for (int i = 0; i < 3 * 600 && !tried; ++i)
    {
        w.tick(1);
        tried = hasEvent(w, "theft", seen) || hasEvent(w, "attempted theft", seen);
        if (i % 300 == 0)
            starve(w);
    }
    expect(tried, "A hungry resident with nothing tries to steal food or coin");
    expect(w.society().conserved(), "Money is conserved");
}

void savedAndRestored()
{
    auto w = town();
    w.addPlayer("player-ada", "Ada");
    beside(w, "player-ada", "wren");
    beside(w, "sloe", "wren", -1, 2);          // A guard on patrol, in the shop (on open floor).
    stealUntilSeen(w, "player-ada", "wren");
    w.entity("wren")->hurt = 30;
    const auto before = w.crime();
    // Through the world's own save...
    auto saved = w.save();
    World again = town();
    expect(again.restore(saved).ok, "Restores");
    expect(again.warrantFor("player-ada") && again.crime().incidents.size() == before.incidents.size(), "The warrant is kept");
    // ...and the checkpoint document.
    checkpoint::ServerState server;
    std::vector<Entity> npcs(saved.npcs.begin(), saved.npcs.end());
    const auto doc = checkpoint::encode(saved, server, npcs, w.time());
    json::Value parsed;
    std::string error;
    expect(json::parse(json::dump(doc), parsed, error), "Parses: " + error);
    PersistedWorld back;
    checkpoint::ServerState serverBack;
    expect(checkpoint::decode(parsed, back, serverBack, error), "Decodes: " + error);
    expect(back.crime.warrants.size() == before.warrants.size() && back.crime.incidents.size() == before.incidents.size() &&
               back.crime.nextIncident == before.nextIncident,
           "Crime survives the checkpoint");
    const auto& a = before.incidents.back();
    const auto& b = back.crime.incidents.back();
    expect(a.id == b.id && a.kind == b.kind && a.offender == b.offender && a.witnesses.size() == b.witnesses.size() &&
               a.status == b.status,
           "Incidents with their witnesses");
    expect(back.crime.warrants[0].fine == before.warrants[0].fine &&
               back.crime.warrants[0].restitution.size() == before.warrants[0].restitution.size(),
           "Warrants with their restitution");
    bool hurtKept = false, incidentKept = false;
    for (const auto& e : back.npcs)
        hurtKept |= e.id == "wren" && std::abs(e.hurt - 30) < 1e-9;
    for (const auto& h : back.roads.beliefs)
        incidentKept |= !h.incident.empty();
    expect(hurtKept, "Injury is kept");
    expect(incidentKept, "Beliefs remember which incident they are about");
}
} // namespace

int main()
{
    try
    {
        refusals();
        theftBeforeTheWatch();
        unpaidMeansGaol();
        theFallenAreCarriedIn();
        residentChasedDown();
        assaultBeatsDown();
        downedAndUp();
        needMakesThieves();
        savedAndRestored();
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAILED after " << checks << " checks: " << e.what() << "\n";
        return 1;
    }
    std::cout << "crime tests passed (" << checks << " checks)\n";
    return 0;
}
