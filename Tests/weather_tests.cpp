#include "RatwWorld.h"

#include <algorithm>
#include <cmath>
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
bool near(double a, double b, double epsilon = 1e-7)
{
    return std::abs(a - b) < epsilon;
}
void arena(World& world)
{
    auto& c = *world.cell("tavern");
    c.width = 80;
    c.height = 36;
    c.tiles.assign(static_cast<std::size_t>(c.width * c.height), Tile{});
    c.outdoors = true;
    world.setWeather("tavern", Weather::Clear); // Freeze a clear fixture; calendar commands can refresh forecasts.
    c.wind = {};
    for (const auto& entry : world.entities())
        if (entry.second.npc)
            world.entity(entry.first)->leaderId = "test-frozen";
}
bool contains(const Snapshot& snapshot, const std::string& id)
{
    return std::any_of(snapshot.entities.begin(), snapshot.entities.end(),
                       [&](const Entity& entity) { return entity.id == id; });
}

void clockAndProfiles()
{
    World world;
    arena(world);
    const auto noon = world.environmentAt("tavern");
    expect(near(noon.hour, 12) && noon.phase == "day" && near(noon.daylight, 1), "New worlds retain the noon baseline");
    expect(near(noon.illumination, 1) && near(noon.sight, 1) && near(noon.hearing, 1) && near(noon.scent, 1) &&
               near(noon.movement, 1),
           "Clear calm noon preserves all original healthy multipliers");
    struct Phase
    {
        double hour, daylight;
        const char* phase;
    };
    for (const auto& profile : {Phase{0, 0, "night"}, Phase{4, 0, "night"}, Phase{5, 0, "dawn"}, Phase{6, .5, "dawn"},
                                Phase{7, 1, "day"}, Phase{12, 1, "day"}, Phase{17, 1, "dusk"}, Phase{18, .5, "dusk"},
                                Phase{19, 0, "night"}, Phase{23.999, 0, "night"}})
    {
        expect(world.setTimeOfDay(profile.hour).ok, "Supported authoring times are accepted");
        const auto environment = world.environmentAt("tavern");
        expect(near(environment.hour, profile.hour) && near(environment.daylight, profile.daylight) &&
                   environment.phase == profile.phase,
               "Dawn, day, dusk, and night share deterministic clock bands");
        const auto sky = calendar::skyAt(world.save().calendarDays, calendar::Weather::Clear);
        expect(sky.valid && near(environment.illumination, sky.outdoorIllumination) &&
                   near(environment.sight, environment.illumination),
               "Outdoor sight follows calendar-derived moonlight and daylight with a low-light floor");
        expect(near(environment.hearing, 1) && near(environment.scent, 1) && near(environment.movement, 1),
               "Darkness alone does not impair hearing, smell, or gait");
    }
    for (double boundary : {5.0, 7.0, 17.0, 19.0})
    {
        world.setTimeOfDay(boundary - 1e-5);
        const double before = world.environmentAt("tavern").illumination;
        world.setTimeOfDay(boundary + 1e-5);
        expect(near(before, world.environmentAt("tavern").illumination, 1e-8),
               "Illumination is continuous at dawn and dusk boundaries");
    }
    world.setTimeOfDay(23.999);
    world.tick(1);
    expect(near(world.environmentAt("tavern").hour, 23.999 + 1.0 / 600.0 - 24.0),
           "The simulation clock rolls through midnight");
    // One world hour is 600 simulation seconds; do not perform four hours of
    // movement ticks merely to test the mathematical clock's wraparound.
    world.setTimeOfDay(6.5);
    const double simulationTime = world.time();
    world.tick(1.5);
    expect(near(world.time(), simulationTime + 1.5) && near(world.environmentAt("tavern").hour, 6.5025),
           "Time advances at the requested 14400-second day rate");
    auto state = world.save();
    const auto oldDate = calendar::calendarAt(state.calendarDays);
    state.time += calendar::SecondsPerDay;
    state.calendarDays += 1;
    expect(world.restore(state).ok && near(world.environmentAt("tavern").hour, 6.5025) &&
               world.environmentAt("tavern").date.absoluteDay == oldDate.absoluteDay + 1 &&
               !near(world.environmentAt("tavern").date.moonPhase, oldDate.moonPhase),
           "A complete day wraps the hour while advancing the date and lunar phase");
    for (double invalid :
         {-1.0, 24.0, 1e300, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
    {
        const auto before = world.save();
        expect(!world.setTimeOfDay(invalid).ok && world.time() == before.time &&
                   world.save().clockOffsetHours == before.clockOffsetHours &&
                   world.save().calendarDays == before.calendarDays,
               "Invalid time commands are rejected atomically");
    }
}

void weatherFactorsAndShelter()
{
    World world;
    arena(world);
    struct Profile
    {
        Weather weather;
        double sight, hearing, scent, movement;
    };
    for (const auto& profile : {Profile{Weather::Clear, 1, 1, 1, 1}, Profile{Weather::Rain, .78, .72, .65, .85},
                                Profile{Weather::Snow, .65, .85, .8, .7}, Profile{Weather::Fog, .4, 1, 1.05, 1}})
    {
        world.setWeather("tavern", profile.weather);
        world.setTimeOfDay(12);
        const auto day = world.environmentAt("tavern");
        expect(near(day.sight, profile.sight) && near(day.hearing, profile.hearing) && near(day.scent, profile.scent) &&
                   near(day.movement, profile.movement),
               "All senses and movement consume the same weather tuning table");
        world.setTimeOfDay(0);
        const auto night = world.environmentAt("tavern");
        const auto sky =
            calendar::skyAt(world.save().calendarDays, profile.weather == Weather::Rain   ? calendar::Weather::Rain
                                                       : profile.weather == Weather::Fog  ? calendar::Weather::Fog
                                                       : profile.weather == Weather::Snow ? calendar::Weather::Snow
                                                                                          : calendar::Weather::Clear);
        expect(sky.valid && near(night.sight, day.sight * sky.outdoorIllumination) &&
                   near(night.hearing, day.hearing) && near(night.scent, day.scent) &&
                   near(night.movement, day.movement),
               "Lunar weather-attenuated night stacks with sight obstruction but not unrelated sense penalties");
        world.cell("tavern")->outdoors = false;
        const auto shelter = world.environmentAt("tavern");
        expect(shelter.phase == "night" && near(shelter.daylight, 0) && near(shelter.illumination, 1) &&
                   near(shelter.sight, 1) && near(shelter.hearing, 1) && near(shelter.scent, 1) &&
                   near(shelter.movement, 1),
               "Whole-cell shelter preserves shared time but protects all local environmental factors");
        world.cell("tavern")->outdoors = true;
    }
    world.setWeather("tavern", Weather::Clear);
    world.setTimeOfDay(0);
    const double newMoonNight = world.environmentAt("tavern").illumination;
    expect(world.advanceCalendar(14).ok, "A forward calendar jump reaches a near-full lunar phase");
    const auto nearFull = world.environmentAt("tavern");
    expect(near(newMoonNight, .08) && nearFull.illumination > .39 && nearFull.phase == "night" &&
               nearFull.date.moonName == "full moon",
           "The actual world sky becomes brighter at near-full moon without changing midnight to daylight");
    world.setWeather("tavern", Weather::Fog);
    const auto cloudyMoon = world.environmentAt("tavern");
    expect(cloudyMoon.illumination < nearFull.illumination &&
               near(cloudyMoon.illumination,
                    calendar::skyAt(world.save().calendarDays, calendar::Weather::Fog).outdoorIllumination) &&
               near(cloudyMoon.sight, cloudyMoon.illumination * .4),
           "Fog attenuates moonlight and separately blocks visual reach in the authoritative environment");
    world.setWeather("tavern", Weather::Clear);
    world.setTimeOfDay(12);
    for (double strength : {0.0, .5, 1.0})
    {
        expect(world.setWind("tavern", 0, strength).ok, "Wind strength can be authored independently");
        const auto wind = world.environmentAt("tavern");
        expect(near(wind.hearing, 1 - .25 * strength) && near(wind.sight, 1) && near(wind.movement, 1),
               "Wind masks hearing without arbitrarily reducing vision or movement");
    }
    world.setWeather("tavern", Weather::Rain);
    expect(near(world.environmentAt("tavern").hearing, .72 * .75),
           "Strong wind and rain combine their audible masking");
    world.setWeather("tavern", static_cast<Weather>(-1));
    expect(world.cell("tavern")->weather == Weather::Rain, "Invalid enum input cannot corrupt live weather");
}

void actualPerceptionAndPrivacy()
{
    World world;
    arena(world);
    world.setTimeOfDay(0);
    auto& observer = world.addPlayer("observer", "Observer");
    auto& source = world.addPlayer("source", "Source");
    observer.position = {10.5, 20.5};
    source.position = {30.5, 20.5};
    const std::size_t distantTile = static_cast<std::size_t>(20 * 80 + 30);
    const auto darkness = world.snapshot("observer");
    expect(!contains(darkness, "source") && world.visionClarity("observer", "source") == 0 &&
               !world.perceive("observer", "source").identifiable,
           "Night omits distant wolves and prevents visual identity and actions");
    expect(!darkness.visibleTiles[distantTile] && !darkness.rememberedTiles[distantTile] &&
               darkness.cell.tiles[distantTile].glyph == ' ',
           "Night does not transmit or memorize never-seen distant terrain");
    expect(near(darkness.environment.sight, world.environmentAt("tavern").sight),
           "The snapshot describes the same environment used to filter its entities");
    const double nightHearing = world.hearingClarity("observer", "source");
    world.setTimeOfDay(12);
    const auto daylight = world.snapshot("observer");
    expect(contains(daylight, "source") && daylight.visibleTiles[distantTile] &&
               world.visionClarity("observer", "source") > 0,
           "Daylight really expands visible wolves and terrain, not merely brightness");
    expect(near(world.hearingClarity("observer", "source"), nightHearing),
           "Actual speech clarity is independent of darkness");
    world.setTimeOfDay(0);
    const auto remembered = world.snapshot("observer");
    expect(!remembered.visibleTiles[distantTile] && remembered.rememberedTiles[distantTile] &&
               remembered.cell.tiles[distantTile].glyph == '.' && !contains(remembered, "source"),
           "Known terrain stays dimly remembered at night without leaking live actors");

    world.setTimeOfDay(12);
    world.setWeather("tavern", Weather::Fog);
    expect(!contains(world.snapshot("observer"), "source"), "Fog culls distant entities server-side");
    world.setWeather("tavern", Weather::Clear);
    const double clearSpeech = world.hearingClarity("observer", "source");
    world.setWeather("tavern", Weather::Rain);
    expect(world.hearingClarity("observer", "source") < clearSpeech, "Rain reduces actual word clarity over distance");
    world.setWeather("tavern", Weather::Snow);
    expect(world.hearingClarity("observer", "source") < clearSpeech, "Snow reduces actual word clarity over distance");
    world.setWeather("tavern", Weather::Clear);
    world.setWind("tavern", 0, 1);
    expect(world.hearingClarity("observer", "source") < clearSpeech, "Clear-weather wind masks actual speech too");
    source.position = {19.5, 20.5};
    source.velocity = {1, 0};
    world.setWind("tavern", 0, 0);
    const double clearFootsteps = world.movementAudibility("observer", "source");
    world.setWeather("tavern", Weather::Rain);
    expect(world.movementAudibility("observer", "source") < clearFootsteps,
           "Rain masks actual movement sounds as well as speech");
    world.setWeather("tavern", Weather::Clear);
    world.setWind("tavern", 0, 1);
    expect(world.movementAudibility("observer", "source") < clearFootsteps,
           "Wind masks footsteps through the shared hearing factor");
    observer.position = {30.5, 20.5};
    source.position = {10.5, 20.5};
    const double clearScent = world.scentClarity("observer", "source");
    expect(clearScent > 0, "The downwind scent fixture has a detectable source");
    world.setTimeOfDay(0);
    expect(near(world.scentClarity("observer", "source"), clearScent),
           "Darkness does not erase actual wind-carried scent");
    world.setWeather("tavern", Weather::Rain);
    const double rainScent = world.scentClarity("observer", "source");
    expect(rainScent < clearScent, "Rain attenuates actual airborne scent");
    world.setWeather("tavern", Weather::Snow);
    expect(world.scentClarity("observer", "source") < clearScent, "Snow attenuates actual airborne scent");
    world.setWeather("tavern", Weather::Fog);
    expect(world.scentClarity("observer", "source") > clearScent,
           "Fog's modest game-tuned humidity bonus reaches actual scent detection");
    auto& npc = *world.entity("npc_keeper");
    npc.position = observer.position;
    npc.vision = observer.vision;
    npc.hearing = observer.hearing;
    npc.smell = observer.smell;
    const auto npcPerception = world.perceive(npc.id, "source");
    const auto playerPerception = world.perceive("observer", "source");
    expect(near(npcPerception.hearing, playerPerception.hearing) &&
               near(npcPerception.vision, playerPerception.vision) && near(npcPerception.scent, playerPerception.scent),
           "NPC observers use exactly the same environmental perception as players");
}

void actualMovementAndStamina()
{
    for (Weather weather : {Weather::Clear, Weather::Rain, Weather::Snow, Weather::Fog})
        for (bool outdoors : {false, true})
        {
            World world;
            arena(world);
            world.cell("tavern")->outdoors = outdoors;
            world.setWeather("tavern", weather);
            world.setTimeOfDay(0);
            auto& actor = world.addPlayer("runner", "Runner");
            actor.position = {10.5, 30.5};
            actor.pace = 10;
            actor.stamina = 60;
            const double factor = world.environmentAt("tavern").movement;
            world.move(actor.id, 1, 0);
            world.tick(.5);
            expect(near(actor.position.x - 10.5, 7.8 * factor * .5),
                   "Actual travel uses the shared weather factor and ignores darkness alone");
            expect(near(actor.stamina, 55) && near(actor.staminaRate, -10),
                   "Weather leaves constant stamina recovery and selected-gait effort accounting intact");
            world.stop(actor.id);
            world.tick(1);
            expect(near(actor.stamina, 60) && near(actor.staminaRate, 5),
                   "Rest regenerates stamina at the same constant rate in every environment");
        }
}

void persistenceAndAtomicValidation()
{
    World original;
    original.tick(1.5);
    original.setTimeOfDay(18.25);
    original.setWeather("exterior", Weather::Snow);
    original.setWind("exterior", 1.2, .6, true);
    original.addPlayer("p", "Traveler");
    const auto state = original.save();
    World restored;
    expect(restored.restore(state).ok, "Weather, wind, and clock offset restore together");
    const auto before = original.environmentAt("exterior");
    const auto after = restored.environmentAt("exterior");
    expect(near(before.hour, after.hour) && near(before.daylight, after.daylight) &&
               near(before.illumination, after.illumination) && near(before.sight, after.sight) &&
               near(before.hearing, after.hearing) && near(before.scent, after.scent) &&
               near(before.movement, after.movement) && before.phase == after.phase,
           "Restart reproduces the complete authoritative environment without offline clock advance");
    original.tick(.5);
    restored.tick(.5);
    expect(near(original.environmentAt("exterior").hour, restored.environmentAt("exterior").hour) &&
               near(original.environmentAt("exterior").hearing, restored.environmentAt("exterior").hearing),
           "Clock and deterministic gusts resume together after restart");
    for (double invalid :
         {-1.0, 24.0, 1e300, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
    {
        const auto clean = restored.save();
        auto bad = clean;
        bad.clockOffsetHours = invalid;
        bad.weather["exterior"] = Weather::Clear;
        bad.players[0].name = "Must not apply";
        expect(!restored.restore(bad).ok && restored.save().clockOffsetHours == clean.clockOffsetHours &&
                   restored.time() == clean.time && restored.entity("p")->name == "Traveler" &&
                   restored.cell("exterior")->weather == Weather::Snow,
               "Malformed persisted time offset rejects atomically before any world mutation");
    }
    PersistedWorld legacy;
    expect(near(legacy.clockOffsetHours, 12), "Legacy persistence defaults to the noon epoch");
    legacy.time = 300;
    expect(restored.restore(legacy).ok && near(restored.environmentAt("exterior").hour, 13),
           "Legacy elapsed simulation time advances from the default noon epoch");
    legacy.time = 1e12;
    expect(restored.restore(legacy).ok && std::isfinite(restored.environmentAt("exterior").hour) &&
               restored.environmentAt("exterior").hour >= 0 && restored.environmentAt("exterior").hour < 24,
           "The largest supported persisted clock retains a finite bounded hour");
}
} // namespace

int main()
{
    try
    {
        clockAndProfiles();
        weatherFactorsAndShelter();
        actualPerceptionAndPrivacy();
        actualMovementAndStamina();
        persistenceAndAtomicValidation();
        std::cout << "PASS " << checks << " weather/day-night checks\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
}
