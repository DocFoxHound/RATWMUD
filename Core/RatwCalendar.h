#pragma once

#include <array>
#include <cstdint>
#include <string>

// Engine-independent calendar and regional climate primitives. No operating-
// system clock, random-device state, world entities, or dialogue providers are
// read here: a saved epoch/seed reproduces the same sky and forecast.
namespace ratw::calendar
{
constexpr double SecondsPerDay = 14400.0;
constexpr int DaysPerYear = 365;
// Mean Earth synodic month, not its 27.3-day orbital period. This first version
// applies the natural phase sequence to accelerated GAME days; it is not an
// astronomical ephemeris or a claim to match today's real-world lunar phase.
// https://eclipse.gsfc.nasa.gov/phase/phasecat.html
constexpr double LunarCycleDays = 29.530588;
// Bound conversion/precision and reject corrupt saves. One million game years
// is far beyond a normal world lifetime, while fractional hours remain useful.
constexpr double MaxGameDays = 365000000.0;

enum class Season
{
    Spring,
    Summer,
    Autumn,
    Winter
};
// Deliberately separate from ratw::Weather so this module does not depend on
// RatwWorld.h. Integrations must map enum values explicitly.
enum class Weather
{
    Clear,
    Rain,
    Fog,
    Snow
};
enum class Climate
{
    Temperate
};

struct Calendar
{
    bool valid = false;
    double absoluteDays = 0.0;
    std::uint64_t absoluteDay = 0; // Zero-based completed days since the epoch.
    std::uint64_t year = 1;
    int dayOfYear = 1, dayOfSeason = 1; // Human-facing, one-based.
    double hour = 0.0, daylight = 0.0;
    Season season = Season::Spring;
    std::string period = "night";
    double moonPhase = 0.0, moonIllumination = 0.0;
    std::string moonName = "new moon";
};

struct Sky
{
    bool valid = false;
    Calendar date;
    double moonTransmission = 1.0;
    double nightLight = .08;
    double outdoorIllumination = .08;
};

struct Forecast
{
    bool valid = false;
    Season season = Season::Spring;
    Weather weather = Weather::Clear;
    int slot = 0; // Four stable six-game-hour slots per day.
};

// Invalid/nonfinite/negative/out-of-bounds input returns valid=false and safe
// defaults; never clamps a corrupt epoch into a plausible valid date. Year 1
// starts on Spring 1 at midnight, with a new moon. Season lengths are currently
// Spring 92, Summer 92, Autumn 91, Winter 90; no leap days or month abstraction.
// Dawn ramps smoothly 05:00–07:00; dusk 17:00–19:00. Midpoints 06:00/18:00 are
// exactly two real hours apart in either direction, in EVERY season.
Calendar calendarAt(double absoluteGameDays);

// Provisional gameplay light factors, not photometric simulation:
// nightLight=.08+.32*moonIllumination*moonTransmission. Cloud transmission is
// clear 1, rain .35, fog .20, snow .45. The .08 local-awareness floor remains
// even at new moon. Illumination blends nightLight toward 1 with daylight;
// integrations apply separate weather visibility/movement/sound factors.
Sky skyAt(double absoluteGameDays, Weather weather);

// Weights in Weather order Clear/Rain/Fog/Snow, summing to 1000. A temperate
// tuning profile, not a forecast of Earth's weather. Invalid enums give zeros.
std::array<unsigned, 4> weatherWeights(Season season, Climate climate = Climate::Temperate);

// Stable cell-specific forecast: FNV-1a cell bytes plus SplitMix64 mixing of
// world seed, absolute day, and slot. No std::hash or iteration-order coupling.
// Empty cell ID, unsupported climate, slot outside [0,3], and out-of-bounds
// day return valid=false. Changing these weights/hash later changes forecasts
// and should be treated as a world-generation version change.
Forecast forecast(std::uint64_t worldSeed, const std::string& cellId, std::uint64_t absoluteDay, int slot,
                  Climate climate = Climate::Temperate);
Forecast forecastAt(std::uint64_t worldSeed, const std::string& cellId, double absoluteGameDays,
                    Climate climate = Climate::Temperate);

std::string seasonName(Season season);
std::string weatherName(Weather weather);

// The week (Docs/Design/26-living-npcs.md, Phase 9): seven named days, the same everywhere, counted from Year 1's
// first day (a Dawnday). Marketday brings stalls to every town's market; Restday rests all but the watch.
constexpr int DaysPerWeek = 7;
constexpr int Marketday = 5, Restday = 6;
int weekdayOf(double absoluteGameDays);             // 0..6; 0 for invalid input.
std::string weekdayName(int weekday);               // "Dawnday"... "Restday"; "" out of range.
// Each season has one festival, on this day of it (a town's own name for it: World::festivalName).
constexpr int FestivalDayOfSeason = 46;
bool festivalDay(double absoluteGameDays);
} // namespace ratw::calendar
