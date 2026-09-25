#include "RatwWorld.h"

#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace ratw;
namespace
{
int checks = 0;
void expect(bool value, const char* message)
{
    ++checks;
    if (!value)
        throw std::runtime_error(message);
}
bool near(double a, double b, double tolerance = 1e-7)
{
    return std::abs(a - b) < tolerance;
}
Entity& runner(World& world)
{
    // A long, level test lane permits a complete sprint without a wall or
    // portal accidentally converting part of it into a rest period.
    auto& cell = *world.cell("tavern");
    cell.width = 256;
    cell.height = 64;
    cell.tiles.assign(static_cast<std::size_t>(cell.width * cell.height), Tile{});
    for (const auto& item : world.entities())
        if (item.second.npc)
            world.entity(item.first)->leaderId = "test-frozen";
    auto& actor = world.addPlayer("p", "Pace tester");
    actor.position = {10.5, 30.5};
    return actor;
}

void defaultsAndValidation()
{
    World w;
    auto& p = runner(w);
    expect(p.dexterity == 50 && p.stamina == 100 && p.pace == 0 && !p.exhausted && p.staminaRate == 0,
           "New and legacy actors have safe walking defaults");
    expect(!w.setPace("missing", 1).ok, "An unknown actor cannot change pace");
    expect(!w.setPace("p", -1).ok && !w.setPace("p", 11).ok && !w.setPace("p", 1000000).ok,
           "Pace commands reject out-of-range notches");
    expect(p.pace == 0, "Rejected pace changes are atomic");
    for (int notch = 0; notch <= 10; ++notch)
        expect(w.setPace("p", notch).ok && p.pace == notch, "All eleven pace notches are accepted");
    expect(std::string(paceName(0)) == "walk" && std::string(paceName(5)) == "trot" &&
               std::string(paceName(6)) == "run" && std::string(paceName(10)) == "sprint",
           "Pace labels provide legible gait bands");
    p.path = {{15, 30.5}};
    expect(w.setPace("p", 3).ok && p.path.size() == 1, "Changing pace preserves the current destination");
}

void dexterityAndGaits()
{
    Entity e;
    for (double dexterity : {0.0, 50.0, 100.0})
    {
        e.dexterity = dexterity;
        e.pace = 0;
        expect(near(paceSpeed(e), 2.6), "Walking remains the familiar baseline at every dexterity");
        double previous = paceSpeed(e);
        for (int notch = 1; notch <= 10; ++notch)
        {
            e.pace = notch;
            expect(paceSpeed(e) > previous, "Each pace notch increases speed monotonically");
            previous = paceSpeed(e);
        }
        expect(near(paceSpeed(e), 5.2 + .052 * dexterity), "Dexterity determines the bounded top speed");
    }
    World w;
    auto& p = runner(w);
    w.move("p", 1, 0);
    w.tick(1);
    expect(near(p.position.x, 13.1), "Default walking still travels 2.6 tiles per second");
    w.setPace("p", 10);
    const double origin = p.position.x;
    w.tick(1);
    expect(near(p.position.x - origin, 7.8), "Authoritative movement uses dexterity-scaled sprint speed");
    expect(near(p.stamina, 90) && near(p.staminaRate, -10), "Sprint loses ten net stamina per second");
    p.dexterity = 100;
    const double fastOrigin = p.position.x;
    w.tick(.5);
    expect(near(p.position.x - fastOrigin, 5.2), "Higher dexterity changes real movement, not merely a UI label");
    expect(near(p.stamina, 85), "Dexterity does not secretly change the stamina recovery budget");
}

void energyBalance()
{
    for (int notch = 0; notch <= 10; ++notch)
    {
        World w;
        auto& p = runner(w);
        p.stamina = 40;
        w.setPace("p", notch);
        w.move("p", 1, 0);
        w.tick(1);
        const double expectedRate = 5.0 - 15.0 * std::pow(notch / 10.0, 2);
        expect(near(p.stamina, 40 + expectedRate), "Gross effort and constant recovery combine at every notch");
        expect(near(p.staminaRate, expectedRate), "The UI receives the actual net stamina rate");
    }
    World w;
    auto& p = runner(w);
    p.stamina = 40;
    w.setPace("p", 10);
    w.tick(2);
    expect(near(p.stamina, 50) && near(p.staminaRate, 5),
           "Selecting sprint while standing still does not spend stamina");
    w.setPace("p", 5);
    w.move("p", 1, 0);
    w.tick(10);
    expect(near(p.stamina, 62.5), "A middle trot can support prolonged travel while recovering");
    w.stop("p");
    w.tick(20);
    expect(p.stamina == 100 && near(p.staminaRate, 0), "Recovery is capped at full stamina");
    const double before = p.stamina;
    w.tick(-1);
    w.tick(std::numeric_limits<double>::quiet_NaN());
    expect(p.stamina == before, "Invalid simulation time cannot alter stamina");
}

void timestepAndPathAccounting()
{
    World combined, divided, waypoint;
    auto& a = runner(combined);
    auto& b = runner(divided);
    auto& c = runner(waypoint);
    for (auto* actor : {&a, &b, &c})
    {
        actor->stamina = 70;
        actor->pace = 10;
    }
    combined.move("p", 1, 0);
    divided.move("p", 1, 0);
    combined.tick(.7);
    for (int index = 0; index < 70; ++index)
        divided.tick(.01);
    expect(near(a.position.x, b.position.x) && near(a.stamina, b.stamina),
           "Equivalent elapsed time gives equivalent translation and energy");
    for (int index = 1; index <= 1000; ++index)
        c.path.push_back({10.5 + index * .0078, 30.5});
    waypoint.tick(.7);
    expect(near(a.position.x, c.position.x) && near(a.stamina, c.stamina),
           "Hundreds of tiny waypoints do not multiply recovery or drain");
    waypoint.tick(.5);
    expect(c.path.empty() && near(c.position.x, 18.3), "An over-budget frame stops exactly at the final waypoint");
    expect(near(c.stamina, 70 + 5 * 1.2 - 15), "Unused time after a completed route is recovery-only");

    World shortRoute;
    auto& p = runner(shortRoute);
    p.pace = 10;
    p.stamina = 50;
    p.path = {{p.position.x + .039, p.position.y}}; // Five milliseconds of motion.
    shortRoute.tick(1.0 / 30.0);
    expect(near(p.stamina, 50 + 5.0 / 30.0 - 15 * .005), "Partial-frame movement pays only its actual time");
    shortRoute.tick(1);
    expect(near(p.stamina, 50 + 5.0 / 30.0 - 15 * .005 + 5), "Completed paths cannot drain stamina while idle");
}

void collisionAndEnvironment()
{
    World w;
    auto& p = runner(w);
    p.stamina = 40;
    p.pace = 10;
    p.position = {19.934, 30.5};
    auto* wall = w.cell("tavern")->tile(20, 30);
    wall->solid = wall->opaque = true;
    w.move("p", 1, 0);
    w.tick(1);
    expect(near(p.position.x, 19.934) && near(p.stamina, 45),
           "Holding sprint into a wall recovers without phantom effort");
    expect(near(p.velocity.x, 0), "Blocked movement has no fabricated velocity");
    p.path = {{22, 30.5}};
    p.input = {};
    w.tick(.1);
    expect(p.path.empty() && near(p.stamina, 45.5), "A newly blocked route clears without stamina loss");

    World terrain;
    auto& t = runner(terrain);
    t.pace = 10;
    t.stamina = 50;
    auto& cell = *terrain.cell("tavern");
    cell.outdoors = true;
    cell.weather = Weather::Rain;
    for (auto& tile : cell.tiles)
        tile.movementCost = 2.4;
    terrain.move("p", 1, 0);
    terrain.tick(1);
    expect(near(t.position.x - 10.5, 7.8 / 2.4 * .85), "Terrain and rain still modify speed after pace selection");
    expect(near(t.stamina, 40), "Slow terrain does not create free full-effort sprinting");
    cell.weather = Weather::Snow;
    const double origin = t.position.x;
    terrain.tick(1);
    expect(near(t.position.x - origin, 7.8 / 2.4 * .70), "Snow continues to slow movement");
}

void postureAndExhaustion()
{
    World w;
    auto& p = runner(w);
    p.pace = 10;
    p.stamina = 40;
    p.dexterity = 100;
    w.setPosture("p", "crouching");
    w.move("p", 1, 0);
    w.tick(1);
    expect(near(p.position.x - 10.5, 2.6 * .30) && effectivePace(p) == 0,
           "Crouching never turns a selected sprint into a fast sneak");
    expect(near(p.stamina, 45), "Slow stealth movement is sustainable");
    w.stop("p");
    w.setPosture("p", "sitting");
    const double sitting = p.position.x;
    w.move("p", 1, 0);
    w.tick(.2);
    expect(near(p.position.x, sitting) && near(p.stamina, 46), "Standing up takes time and permits recovery");
    w.stop("p");
    w.tick(1);
    w.setPosture("p", "lying");
    w.move("p", 1, 0);
    w.tick(1);
    expect(p.posture == "crouching" && p.position.x - sitting < .8,
           "Lying-to-motion still rises into a slow sneak despite sprint selection");

    World fatigue;
    auto& runnerActor = runner(fatigue);
    fatigue.setPace("p", 10);
    fatigue.move("p", 1, 0);
    fatigue.tick(10);
    expect(runnerActor.stamina == 0 && runnerActor.exhausted && runnerActor.pace == 10,
           "A full sprint exhausts stamina in ten seconds and retains requested pace");
    const double exhaustedOrigin = runnerActor.position.x;
    fatigue.tick(3.9);
    expect(runnerActor.exhausted && near(runnerActor.stamina, 19.5),
           "Exhaustion has a four-second recovery band, not frame stutter");
    expect(near(runnerActor.position.x - exhaustedOrigin, 2.6 * 3.9),
           "Exhaustion permits safe walking during recovery");
    fatigue.tick(.1);
    expect(!runnerActor.exhausted && near(runnerActor.stamina, 20),
           "Sprinting becomes available again at twenty stamina");
    fatigue.tick(.1);
    expect(near(runnerActor.stamina, 19) && effectivePace(runnerActor) == 10,
           "The retained requested gait resumes after recovery");
}

void transitionsAndPassiveBumps()
{
    World w;
    // This energy fixture deliberately measures the authored rain slowdown;
    // a seasonal forecast must not change the terrain factor during crossing.
    w.setWeather("exterior", Weather::Rain);
    for (const auto& item : w.entities())
        if (item.second.npc)
            w.entity(item.first)->leaderId = "test-frozen";
    auto& p = w.addPlayer("p", "Traveler");
    p.position = {16.5, 22.5};
    p.stamina = 40;
    p.pace = 10;
    expect(w.interact("p", "door_main", "open").ok && p.cellId == "exterior" && p.stamina == 40,
           "An explicit portal does not spend stamina for its teleport distance");
    p.position = {16.5, .1};
    w.move("p", 0, -1);
    w.tick(1.0 / 30.0);
    expect(p.cellId == "tavern" && near(p.position.y, 22.5),
           "Sprinting across a boundary still preserves the connection point");
    expect(near(p.stamina, 40 + 5.0 / 30.0 - 15 * (.1 - .065) / (7.8 * .85)),
           "A crossing charges only movement before the threshold, not the unused stopped frame");
    const double crossingStamina = p.stamina;
    w.tick(1);
    expect(near(p.position.y, 22.5) && near(p.stamina, crossingStamina + 5),
           "Ordinary local-cell transitions stop and recover after crossing");
    w.setPosture("p", "sitting");
    const double before = p.stamina;
    expect(w.interact("p", "door_main", "enter").ok && p.cellId == "tavern",
           "A sitting portal user waits to stand even with sprint selected");
    w.tick(1);
    expect(p.cellId == "exterior" && near(p.stamina, before + 5),
           "Standing and explicit portal movement permit constant recovery");

    World crowd;
    auto& a = runner(crowd);
    auto& b = crowd.addPlayer("b", "Bystander");
    b.position = a.position;
    a.stamina = 40;
    b.stamina = 40;
    a.pace = b.pace = 10;
    crowd.tick(.1);
    expect(a.position.x != b.position.x && near(a.stamina, 40.5) && near(b.stamina, 40.5),
           "Gentle passive bumps never masquerade as sprint effort");
}

void persistence()
{
    World w;
    auto& p = runner(w);
    p.dexterity = 72;
    p.pace = 8;
    p.stamina = 12;
    p.exhausted = true;
    w.move("p", 1, 0);
    w.tick(.1);
    const auto saved = w.save();
    expect(saved.players.size() == 1 && saved.players[0].staminaRate == 0,
           "Saves clear transient stamina rate and movement");
    World restored;
    runner(restored);
    expect(restored.restore(saved).ok, "Pace and stamina round-trip with the existing save model");
    const auto* r = restored.entity("p");
    expect(r && r->dexterity == 72 && r->pace == 8 && near(r->stamina, 12.5) && r->exhausted && r->staminaRate == 0,
           "Restoring preserves dexterity, selected pace, stamina and exhaustion without offline refill");
    expect(r->path.empty() && r->input.x == 0 && r->velocity.x == 0, "Reload does not revive travel intentions");
    auto later = saved;
    later.time += 3600;
    expect(restored.restore(later).ok && near(restored.entity("p")->stamina, 12.5),
           "A later saved clock does not award offline stamina recovery");
    auto altered = saved;
    altered.players[0].staminaRate = 3;
    expect(restored.restore(altered).ok && restored.entity("p")->staminaRate == 0,
           "Finite transient rate is ignored on restore");
    const auto stable = restored.save();
    const auto rejects = [&](const std::function<void(Entity&)>& change) {
        auto corrupt = saved;
        change(corrupt.players[0]);
        expect(!restored.restore(corrupt).ok, "Malformed saved movement statistics are rejected");
        expect(near(restored.entity("p")->stamina, stable.players[0].stamina) &&
                   restored.entity("p")->dexterity == stable.players[0].dexterity && near(restored.time(), stable.time),
               "Malformed movement statistics do not partially replace a live world");
    };
    for (double invalid :
         {-1.0, 101.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()})
    {
        rejects([&](Entity& e) { e.dexterity = invalid; });
        rejects([&](Entity& e) { e.stamina = invalid; });
    }
    for (int invalid : {-1, 11, std::numeric_limits<int>::max()})
        rejects([&](Entity& e) { e.pace = invalid; });
    for (double invalid :
         {-100.0, 100.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()})
        rejects([&](Entity& e) { e.staminaRate = invalid; });
    rejects([](Entity& e) {
        e.stamina = 80;
        e.exhausted = true;
    });
    auto legacy = saved;
    Entity legacyActor;
    legacyActor.id = "legacy";
    legacyActor.name = "Legacy";
    legacyActor.cellId = "tavern";
    legacyActor.position = {12, 30};
    legacy.players = {legacyActor};
    legacy.memories.clear();
    expect(restored.restore(legacy).ok && restored.entity("legacy")->stamina == 100 &&
               restored.entity("legacy")->pace == 0 && restored.entity("legacy")->dexterity == 50,
           "An older actor initialized without the added fields restores with safe defaults");
}
} // namespace

int main()
{
    try
    {
        defaultsAndValidation();
        dexterityAndGaits();
        energyBalance();
        timestepAndPathAccounting();
        collisionAndEnvironment();
        postureAndExhaustion();
        transitionsAndPassiveBumps();
        persistence();
        std::cout << "Passed " << checks << " pace and stamina assertions.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Failed after " << checks << " assertions: " << error.what() << '\n';
        return 1;
    }
}
