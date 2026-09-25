#include "RatwWorld.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

using namespace ratw;
namespace fs = std::filesystem;
namespace
{
int checks = 0;
void expect(bool condition, const char* message)
{
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b, double epsilon = 1e-7)
{
    return std::abs(a - b) < epsilon;
}
bool equal(const Lighting& a, const Lighting& b)
{
    return near(a.artificial, b.artificial) && near(a.daylightAccess, b.daylightAccess) && a.tone == b.tone;
}
void arena(World& world)
{
    auto& cell = *world.cell("tavern");
    cell.width = 40;
    cell.height = 26;
    cell.tiles.assign(static_cast<std::size_t>(cell.width * cell.height), Tile{});
    cell.outdoors = false;
    cell.weather = Weather::Clear;
    cell.wind = {};
    for (const auto& entry : world.entities())
        if (entry.second.npc)
            world.entity(entry.first)->leaderId = "test-frozen";
}
bool contains(const Snapshot& snapshot, const std::string& id)
{
    return std::any_of(snapshot.entities.begin(), snapshot.entities.end(),
                       [&](const Entity& actor) { return actor.id == id; });
}
void profilesAndShelter()
{
    World world;
    arena(world);
    expect(equal(world.cell("tavern")->lighting, Lighting{}), "Legacy rooms default to warm light and daylight access");
    for (double hour : {0.0, 6.0, 12.0, 18.0})
    {
        world.setTimeOfDay(hour);
        const auto environment = world.environmentAt("tavern");
        expect(near(environment.illumination, 1) && near(environment.sight, 1),
               "A lit tavern retains full local visibility at every time of day");
        expect(near(environment.glowStrength, 1 - environment.daylight) && environment.lightingTone == "warm",
               "Warm atmosphere grows as natural daylight declines");
        expect(near(environment.hearing, 1) && near(environment.scent, 1) && near(environment.movement, 1),
               "Artificial light does not alter hearing, smell, or movement");
    }
    world.setTimeOfDay(12);
    expect(near(world.environmentAt("tavern").glowStrength, 0),
           "A naturally bright daytime tavern has no glow overlay");
    world.setTimeOfDay(0);
    expect(world.environmentAt("tavern").lightSource == "artificial", "Night tavern identifies its artificial light");
    for (Weather weather : {Weather::Clear, Weather::Rain, Weather::Snow, Weather::Fog})
    {
        world.setWeather("tavern", weather);
        // Authored indoor wind is rejected elsewhere, but even stale mutable
        // fixture state must not make a sheltered room windy or weather-wet.
        world.cell("tavern")->wind = {1, 1, true};
        const auto sheltered = world.environmentAt("tavern");
        expect(near(sheltered.sight, 1) && near(sheltered.hearing, 1) && near(sheltered.scent, 1) &&
                   near(sheltered.movement, 1) && near(world.windAt("tavern").strength, 0),
               "Shelter protects a lit room from every outdoor weather profile");
    }
    world.cell("tavern")->wind = {};
    expect(world.setLighting("tavern", 0, 0, "neutral").ok, "An unlit windowless cellar can be authored");
    for (double hour : {0.0, 6.0, 12.0, 18.0})
    {
        world.setTimeOfDay(hour);
        const auto cellar = world.environmentAt("tavern");
        expect(near(cellar.illumination, .08) && near(cellar.sight, .08) && cellar.lightSource == "dark" &&
                   near(cellar.glowStrength, 0),
               "Unlit windowless interiors stay dark during both day and night");
        expect(near(cellar.hearing, 1) && near(cellar.scent, 1) && near(cellar.movement, 1),
               "Dark sheltered interiors still protect their nonvisual senses and movement");
    }
    world.setLighting("tavern", 0, 1, "warm");
    for (double hour : {0.0, 6.0, 12.0, 18.0})
    {
        world.setTimeOfDay(hour);
        const auto windowed = world.environmentAt("tavern");
        expect(near(windowed.illumination, std::max(.08, windowed.daylight)) && near(windowed.glowStrength, 0),
               "Windowed unlit interiors follow daylight without acquiring artificial glow");
    }
    world.setTimeOfDay(12);
    expect(world.environmentAt("tavern").lightSource == "daylight", "An unlit windowed day room uses natural light");
    for (const std::string tone : {"warm", "neutral", "cool"})
    {
        expect(world.setLighting("tavern", .4, .25, tone).ok, "All supported lighting tones can be authored");
        const auto partial = world.environmentAt("tavern");
        expect(near(partial.illumination, .4) && near(partial.glowStrength, .3) && partial.lightingTone == tone &&
                   partial.lightSource == "mixed",
               "Partial sources use strongest illumination and continuous contextual glow");
    }
    for (double boundary : {5.0, 7.0, 17.0, 19.0})
    {
        world.setLighting("tavern", .4, .8, "warm");
        world.setTimeOfDay(boundary - 1e-5);
        const auto before = world.environmentAt("tavern");
        world.setTimeOfDay(boundary + 1e-5);
        const auto after = world.environmentAt("tavern");
        expect(near(before.illumination, after.illumination, 1e-8) &&
                   near(before.glowStrength, after.glowStrength, 1e-8),
               "Interior illumination and glow remain continuous at day phase boundaries");
    }
    world.cell("tavern")->outdoors = true;
    world.setWeather("tavern", Weather::Clear);
    for (double hour : {0.0, 6.0, 12.0})
    {
        world.setTimeOfDay(hour);
        world.setLighting("tavern", 0, 0, "cool");
        const auto unlit = world.environmentAt("tavern");
        world.setLighting("tavern", 1, 1, "warm");
        const auto lit = world.environmentAt("tavern");
        const auto sky = calendar::skyAt(world.save().calendarDays, calendar::Weather::Clear);
        expect(sky.valid && near(unlit.sight, lit.sight) && near(lit.illumination, sky.outdoorIllumination) &&
                   near(lit.glowStrength, 0) && near(lit.artificialLight, 0) && lit.lightingTone == "neutral",
               "Outdoor cells use calendar sky light and ignore indoor artificial-light profiles");
    }
}
void authoritativePerceptionAndMemory()
{
    World world;
    arena(world);
    world.setTimeOfDay(0);
    world.setLighting("tavern", 0, 0, "warm");
    auto& observer = world.addPlayer("observer", "Observer");
    auto& source = world.addPlayer("source", "Source");
    observer.position = {10.5, 12.5};
    source.position = {24.5, 12.5};
    const auto distantTile = static_cast<std::size_t>(12 * 40 + 24);
    const auto dark = world.snapshot("observer");
    expect(!contains(dark, "source") && near(world.visionClarity("observer", "source"), 0) &&
               !world.perceive("observer", "source").identifiable,
           "Dark interiors hide distant wolves, visual poses, and identity server-side");
    expect(!dark.visibleTiles[distantTile] && !dark.rememberedTiles[distantTile] &&
               dark.cell.tiles[distantTile].glyph == ' ',
           "Unknown terrain in a dark room is neither revealed nor recorded");
    expect(near(dark.environment.sight, .08) && near(dark.environment.sight, world.environmentAt("tavern").sight),
           "The transmitted atmosphere matches the authoritative visibility filter");
    const double hearing = world.hearingClarity("observer", "source");
    world.setLighting("tavern", 1, 1, "warm");
    const auto lit = world.snapshot("observer");
    expect(contains(lit, "source") && lit.visibleTiles[distantTile] && world.visionClarity("observer", "source") > 0,
           "Artificial lighting restores actual night-time visibility in the room");
    expect(near(world.hearingClarity("observer", "source"), hearing),
           "Lighting does not silently change spoken clarity");
    world.setLighting("tavern", 0, 0, "warm");
    world.cell("tavern")->tile(24, 12)->glyph = ',';
    const auto remembered = world.snapshot("observer");
    expect(!remembered.visibleTiles[distantTile] && remembered.rememberedTiles[distantTile] &&
               remembered.cell.tiles[distantTile].glyph == '.' && !contains(remembered, "source"),
           "Dimming preserves previously seen geometry without disclosing live edits or actors");
    source.position = {13, 12.5};
    const double scent = world.scentClarity("observer", "source");
    expect(world.visionClarity("observer", "source") == 0 && scent > 0 && !world.scentCues("observer").empty(),
           "A wolf beyond dark sight range can still be scented without becoming visible");
    world.setLighting("tavern", 1, 1, "warm");
    expect(near(scent, world.scentClarity("observer", "source")) && world.scentCues("observer").empty(),
           "Lighting changes the cue's visual context, not scent sensitivity");
    observer.eyeHealth = 0;
    expect(world.visionClarity("observer", "source") == 0, "Bright rooms do not bypass damaged vision");
    observer.eyeHealth = 1;
    auto* wall = world.cell("tavern")->tile(12, 12);
    wall->opaque = true;
    wall->solid = true;
    expect(world.visionClarity("observer", "source") == 0, "Artificial lighting does not let sight pass through walls");
}
void validationAndPersistence()
{
    World world;
    arena(world);
    world.setLighting("tavern", .65, .2, "cool");
    const Lighting original = world.cell("tavern")->lighting;
    for (double bad :
         {-1.0, 1.01, 1e300, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()})
    {
        expect(!world.setLighting("tavern", bad, .5, "warm").ok && equal(world.cell("tavern")->lighting, original),
               "Invalid artificial-light levels reject atomically");
        expect(!world.setLighting("tavern", .5, bad, "warm").ok && equal(world.cell("tavern")->lighting, original),
               "Invalid daylight access rejects atomically");
    }
    for (const std::string bad : {"", "WARM", "red", "warm cool"})
        expect(!world.setLighting("tavern", 1, 1, bad).ok && equal(world.cell("tavern")->lighting, original),
               "Unknown lighting tones reject atomically");
    expect(!world.setLighting("missing", 1, 1, "warm").ok, "Lighting cannot create nonexistent cells");
    world.setTimeOfDay(23);
    const auto saved = world.save();
    expect(saved.lighting.size() == world.cells().size() && equal(saved.lighting.at("tavern"), original),
           "Every authored lighting profile is captured in world saves");
    World restarted;
    arena(restarted);
    expect(restarted.restore(saved).ok && equal(restarted.cell("tavern")->lighting, original) &&
               near(restarted.environmentAt("tavern").hour, 23) && near(restarted.environmentAt("tavern").sight, .65),
           "Restart preserves light levels, tone, and the day-night clock together");
    auto legacy = saved;
    legacy.lighting.clear();
    World fresh;
    arena(fresh);
    expect(fresh.restore(legacy).ok && equal(fresh.cell("tavern")->lighting, Lighting{}),
           "Legacy saves retain the loaded map's default lighting");
    fresh.setLighting("tavern", 0, 0, "neutral");
    expect(fresh.restore(legacy).ok && equal(fresh.cell("tavern")->lighting, Lighting{0, 0, "neutral"}),
           "Missing legacy overrides do not relight a newly authored unlit room");
    for (const Lighting invalid : {Lighting{-1, 1, "warm"}, Lighting{1, 2, "warm"}, Lighting{1, 1, "red"},
                                   Lighting{std::numeric_limits<double>::quiet_NaN(), 1, "warm"},
                                   Lighting{1, std::numeric_limits<double>::infinity(), "warm"}})
    {
        auto corrupt = saved;
        corrupt.time = 123;
        corrupt.lighting["tavern"] = invalid;
        const auto before = restarted.save();
        expect(!restarted.restore(corrupt).ok && near(restarted.time(), before.time) &&
                   equal(restarted.cell("tavern")->lighting, original),
               "Malformed lighting in saves rejects the entire restore before mutation");
    }
    auto missingCell = saved;
    missingCell.lighting["nonexistent"] = Lighting{};
    expect(!restarted.restore(missingCell).ok && equal(restarted.cell("tavern")->lighting, original),
           "Unknown saved lighting targets cannot introduce ghost cells");
}
struct Fixture
{
    fs::path directory;
    Fixture()
    {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        for (int suffix = 0; suffix < 100; ++suffix)
        {
            const auto candidate =
                fs::temp_directory_path() / ("ratw-lighting-" + std::to_string(stamp) + "-" + std::to_string(suffix));
            if (fs::create_directory(candidate))
            {
                directory = candidate;
                return;
            }
        }
        throw std::runtime_error("Could not create isolated lighting fixture");
    }
    ~Fixture()
    {
        std::error_code ignored;
        // Remove only the exact temporary directory created above.
        fs::remove_all(directory, ignored);
    }
    std::string cell(const std::string& lighting)
    {
        const auto path = directory / "interior.cell";
        std::ofstream file(path);
        file << "id: interior\nname: Lighting fixture\ndescription: Test room\nworld: 0 0 0\n"
                "outdoors: false\nweather: rain\n"
             << lighting << "grid:\n........\n........\n........\n........\n........\n........\n........\n........\n";
        file.close();
        if (!file)
            throw std::runtime_error("Could not write lighting fixture");
        return path.string();
    }
};
void authoredLighting()
{
    Fixture fixture;
    World world;
    expect(world.loadCellFile(fixture.cell("")).ok && equal(world.cell("interior")->lighting, Lighting{}),
           "Legacy cell files remain warmly lit without a lighting header");
    expect(world.loadCellFile(fixture.cell("lighting: 0 0 neutral\n")).ok &&
               near(world.environmentAt("interior").illumination, .08),
           "Authored unlit interiors are actually dark despite shelter and daylight");
    expect(world.loadCellFile(fixture.cell("lighting: 0.4 0.25 cool\n")).ok &&
               equal(world.cell("interior")->lighting, Lighting{.4, .25, "cool"}),
           "Fractional lighting and tone load from the cell format");
    const Lighting original = world.cell("interior")->lighting;
    for (const std::string bad :
         {"-1 0 warm", "1.01 0 warm", "0 -1 warm", "0 1.01 warm", "nan 1 warm", "1 inf warm", "true 1 warm", "1 1 red",
          "1 1 WARM", "1 1", "1 warm", "", "1 1 warm extra", "1 1 warm\nlighting: 0 0 cool"})
        expect(!world.loadCellFile(fixture.cell("lighting: " + bad + "\n")).ok &&
                   equal(world.cell("interior")->lighting, original),
               "Malformed, duplicate, or incomplete lighting headers reject the whole cell replacement");
    expect(world.loadCellFile(fixture.cell("lighting: 1 1 warm\n")).ok,
           "Valid lighting still loads after failed replacements");
}
} // namespace
int main()
{
    try
    {
        profilesAndShelter();
        authoritativePerceptionAndMemory();
        validationAndPersistence();
        authoredLighting();
        std::cout << "PASS: " << checks << " lighting checks\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL after " << checks << " lighting checks: " << error.what() << '\n';
        return 1;
    }
}
