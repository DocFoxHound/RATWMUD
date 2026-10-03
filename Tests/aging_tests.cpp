#include "RatwWorld.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
int main()
{
    int checks = 0;
    auto check = [&](bool value, const char* reason) {
        ++checks;
        if (!value)
            throw std::runtime_error(reason);
    };
    try
    {
        ratw::Entity e;
        check(ratw::advanceAge(e, .5) == 0, "Anchor without retroactive reward");
        check(ratw::advanceAge(e, 365.49) == 0, "Before birthday");
        check(ratw::advanceAge(e, 365.5) == 1 && e.age == 19 && e.strength == 51 && e.dexterity == 51,
              "First birthday");
        check(e.ageNoticePending == 1, "Notice queued");
        check(ratw::advanceAge(e, 365.5) == 0 && e.strength == 51, "Idempotent reward");
        check(ratw::advanceAge(e, .5) == 0, "Clock rollback cannot replay rewards");
        check(ratw::advanceAge(e, 730.5) == 1 && e.age == 20, "Second birthday");
        e.age = 34;
        check(ratw::advanceAge(e, 1095.5) == 1 && e.wisdom == 31, "Maturity gains wisdom");
        e.age = 64;
        check(ratw::ageVisionFactor(e) == 1 && ratw::ageHearingFactor(e) == 1, "No premature elder penalty");
        check(ratw::advanceAge(e, 1460.5) == 1 && e.age == 65, "Elder birthday");
        check(ratw::ageVisionFactor(e) < 1 && ratw::ageHearingFactor(e) < 1 &&
                  ratw::effectiveDexterity(e) < e.dexterity,
              "Elder senses and dexterity");
        e.npc = true;
        e.ageNoticePending = 0;
        check(ratw::advanceAge(e, 1825.5) == 1 && e.ageNoticePending == 0, "NPC ages with no player notice");
        check(ratw::advanceAge(e, std::numeric_limits<double>::infinity()) == 0, "Nonfinite rejected");
        e.age = 1000;
        check(ratw::ageVisionFactor(e) == .45 && ratw::ageHearingFactor(e) == .5,
              "Bounded impairment, not negative senses");
        ratw::World world;
        auto& p = world.addPlayer("player-age", "Age test");
        world.advanceCalendar(365);
        check(p.age == 19 && p.ageNoticePending == 1, "World calendar birthday");
        auto saved = world.save();
        ratw::World loaded;
        check(loaded.restore(saved).ok && loaded.entity(p.id)->age == 19, "Age survives core restore");
        check(loaded.advanceCalendar(0).ok && loaded.entity(p.id)->age == 19, "Restored reward not duplicated");
        const auto before = loaded.calendarDays();
        saved.players[0].lastBirthdayDay = std::numeric_limits<double>::quiet_NaN();
        check(!loaded.restore(saved).ok && loaded.calendarDays() == before, "Invalid age restore atomic");
        check(!loaded.advanceCalendar(-1).ok, "No backwards calendar jump");
        const auto clean = loaded.save();
        auto damaged = clean;
        damaged.npcs.pop_back();
        check(!loaded.restore(damaged).ok && loaded.entity("player-age")->age == 19,
              "New-format resident life cannot restore without its physical NPC record");
        damaged = clean;
        damaged.society.accounts.at("player-age").cash += 1;
        check(!loaded.restore(damaged).ok && loaded.society().conserved() && loaded.calendarDays() == before &&
                  loaded.entity("player-age")->age == 19,
              "Broken money conservation rejects the combined age/calendar/society checkpoint atomically");
        damaged = clean;
        damaged.society.accounts.erase("npc_keeper");
        check(!loaded.restore(damaged).ok && loaded.society().account("npc_keeper") && loaded.society().conserved(),
              "Missing required NPC economy account cannot partially replace live society");
        damaged = clean;
        damaged.society.accounts.at("treasury").cash += damaged.society.accounts.at("player-age").cash;
        damaged.society.accounts.erase("player-age");
        check(!loaded.restore(damaged).ok && loaded.society().account("player-age") &&
                  loaded.society().account("treasury")->cash == clean.society.accounts.at("treasury").cash,
              "New-format saves cannot omit a player account and silently obtain another welcome grant");
        damaged = clean;
        damaged.players.front().lastBirthdayDay = damaged.calendarDays + 365;
        check(!loaded.restore(damaged).ok &&
                  loaded.entity("player-age")->lastBirthdayDay == clean.players.front().lastBirthdayDay,
              "A grossly future-dated birthday anchor rejects rather than suspending aging for a fabricated year");
        ratw::World nightSave;
        nightSave.addPlayer("player-night", "Night tester");
        check(nightSave.setTimeOfDay(0).ok && nightSave.restore(nightSave.save()).ok,
              "An ordinary developer night switch remains saveable despite a noon birthday anchor");
        const auto initialGrant = loaded.society().account("player-age")->cash;
        const auto initialTreasury = loaded.society().account("treasury")->cash;
        check(loaded.restore(clean).ok && loaded.society().account("player-age")->cash == initialGrant &&
                  loaded.society().account("treasury")->cash == initialTreasury && loaded.society().conserved(),
              "Reload does not repeat a welcome grant or replenish the settlement treasury");
        auto* customer = loaded.entity("player-age");
        customer->cellId = "exterior";
        customer->position = {16.5, 1.5};
        check(!loaded.trade(customer->id, "npc_keeper", "meal", 1, true).ok &&
                  loaded.society().account(customer->id)->cash == initialGrant,
              "World integration refuses remote purchases before touching money");
        customer->cellId = "tavern";
        customer->position = {9.5, 7.5};
        loaded.entity("npc_keeper")->position = {9.5, 6.5};
        check(loaded.trade(customer->id, "npc_keeper", "meal", 1, true).ok && loaded.society().conserved(),
              "A nearby visible merchant performs an actual conserved transaction");
        const auto traded = loaded.save();
        ratw::World tradedReload;
        check(tradedReload.restore(traded).ok &&
                  tradedReload.society().account(customer->id)->cash == loaded.society().account(customer->id)->cash &&
                  tradedReload.society().account(customer->id)->stock ==
                      loaded.society().account(customer->id)->stock &&
                  tradedReload.entity(customer->id)->age == 19 &&
                  tradedReload.entity(customer->id)->ageNoticePending == 1,
              "Real trade balances, annual reward, and pending birthday notice survive one combined restart");

        ratw::World blocked;
        auto* room = blocked.cell("tavern");
        room->tiles.assign(static_cast<std::size_t>(room->width * room->height), ratw::Tile{});
        for (const auto& actor : blocked.entities())
            if (actor.second.npc && actor.first != "npc_cook")
                blocked.entity(actor.first)->leaderId = "test-frozen";
        auto* cook = blocked.entity("npc_cook");
        cook->position = {4.5, 4.5};
        for (int y = 3; y <= 5; ++y)
            for (int x = 3; x <= 5; ++x)
                if (x != 4 || y != 4)
                {
                    room->tile(x, y)->solid = true;
                    room->tile(x, y)->opaque = true;
                }
        auto work = blocked.society().state();
        work.accounts["npc_cook"].stock = {{"herbs", 2}, {"meal", 0}};
        auto& cooking = work.residents["npc_cook"];
        cooking.hunger = cooking.fatigue = 0;
        cooking.task = "cook";
        cooking.goalCell = "tavern";
        cooking.goalX = 26.5;
        cooking.goalY = 6.5;
        cooking.progress = 44;
        check(blocked.society().restore(work), "Blocked worksite fixture loads a nearly complete action");
        blocked.tick(2);
        const auto* cookAccount = blocked.society().account("npc_cook");
        // (Herbs untouched: nothing was cooked. A meal may have come with the home's first provisions: doc 36.)
        check(ratw::Society::stock(*cookAccount, "herbs") == 2 && ratw::Society::stock(*cookAccount, "meal") <= 1 &&
                  blocked.society().resident("npc_cook")->progress == 0 && std::abs(cook->position.x - 4.5) < 1e-7 &&
                  std::abs(cook->position.y - 4.5) < 1e-7,
              "An unreachable physical worksite cannot complete a restored recipe or teleport its worker");

        ratw::World cadence30;
        ratw::World cadence60;
        for (auto* cadence : {&cadence30, &cadence60})
            for (const auto& actor : cadence->entities())
                cadence->entity(actor.first)->leaderId = "test-frozen";
        for (int frame = 0; frame < 60; ++frame)
            cadence30.tick(1. / 30);
        for (int frame = 0; frame < 120; ++frame)
            cadence60.tick(1. / 60);
        check(std::abs(cadence30.society().resident("npc_scout")->hunger - 20.007) < 1e-7 &&
                  std::abs(cadence60.society().resident("npc_scout")->hunger - 20.007) < 1e-7,
              "Resident need cadence retains fractional schedule time and is identical at thirty and sixty frames");

        ratw::World climate;
        check(climate.useSeasonalWeather("exterior").ok && climate.advanceCalendar(92).ok,
              "Seasonal fixture advances from spring to summer");
        const auto expectedForecast = ratw::calendar::forecastAt(0x52415457, "exterior", climate.calendarDays());
        check(expectedForecast.valid && int(climate.cell("exterior")->weather) == int(expectedForecast.weather),
              "Advancing calendar applies seasonal weather synchronously before any simulation tick or save");
        ratw::World climateReload;
        check(climateReload.restore(climate.save()).ok &&
                  climateReload.cell("exterior")->weather == climate.cell("exterior")->weather,
              "Immediate restart after a season jump preserves the correct forecast without a stale slot");
        auto maximum = climate.save();
        maximum.calendarDays = ratw::calendar::MaxGameDays;
        check(climate.restore(maximum).ok, "Largest supported calendar can be restored");
        climate.tick(.1);
        check(climate.environmentAt("exterior").date.valid && climate.calendarDays() <= ratw::calendar::MaxGameDays,
              "Ticking the maximum calendar cannot overflow into invalid dates or a false Year One sky");
        std::cout << checks << " aging checks passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Aging/world check " << checks << " failed: " << error.what() << '\n';
        return 1;
    }
}
