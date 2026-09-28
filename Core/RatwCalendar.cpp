#include "RatwCalendar.h"

#include <algorithm>
#include <cmath>

namespace ratw::calendar
{
namespace
{
constexpr double Pi = 3.14159265358979323846;

double smooth(double value)
{
    value = std::clamp(value, 0.0, 1.0);
    return value * value * (3.0 - 2.0 * value);
}

std::uint64_t mix(std::uint64_t value)
{
    value += UINT64_C(0x9e3779b97f4a7c15);
    value = (value ^ (value >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    value = (value ^ (value >> 27)) * UINT64_C(0x94d049bb133111eb);
    return value ^ (value >> 31);
}

std::uint64_t cellHash(const std::string& cellId)
{
    std::uint64_t hash = UINT64_C(14695981039346656037);
    for (unsigned char byte : cellId)
    {
        hash ^= byte;
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

bool validWeather(Weather weather)
{
    return weather == Weather::Clear || weather == Weather::Rain || weather == Weather::Fog || weather == Weather::Snow;
}
} // namespace

Calendar calendarAt(double absoluteGameDays)
{
    Calendar date;
    if (!std::isfinite(absoluteGameDays) || absoluteGameDays < 0.0 || absoluteGameDays > MaxGameDays)
        return date;
    date.valid = true;
    date.absoluteDays = absoluteGameDays;
    const double completedDays = std::floor(absoluteGameDays);
    date.absoluteDay = static_cast<std::uint64_t>(completedDays);
    date.year = date.absoluteDay / DaysPerYear + 1;
    date.dayOfYear = static_cast<int>(date.absoluteDay % DaysPerYear) + 1;
    date.dayOfSeason = date.dayOfYear;
    if (date.dayOfYear > 275)
    {
        date.season = Season::Winter;
        date.dayOfSeason -= 275;
    }
    else if (date.dayOfYear > 184)
    {
        date.season = Season::Autumn;
        date.dayOfSeason -= 184;
    }
    else if (date.dayOfYear > 92)
    {
        date.season = Season::Summer;
        date.dayOfSeason -= 92;
    }
    date.hour = (absoluteGameDays - completedDays) * 24.0;
    if (date.hour >= 5.0 && date.hour < 7.0)
    {
        date.period = "dawn";
        date.daylight = smooth((date.hour - 5.0) / 2.0);
    }
    else if (date.hour >= 7.0 && date.hour < 17.0)
    {
        date.period = "day";
        date.daylight = 1.0;
    }
    else if (date.hour >= 17.0 && date.hour < 19.0)
    {
        date.period = "dusk";
        date.daylight = 1.0 - smooth((date.hour - 17.0) / 2.0);
    }
    date.moonPhase = std::fmod(absoluteGameDays, LunarCycleDays) / LunarCycleDays;
    date.moonIllumination = std::clamp((1.0 - std::cos(2.0 * Pi * date.moonPhase)) * .5, 0.0, 1.0);
    static const char* names[] = {"new moon",  "waxing crescent", "first quarter", "waxing gibbous",
                                  "full moon", "waning gibbous",  "last quarter",  "waning crescent"};
    const auto octant = static_cast<unsigned>(std::floor(date.moonPhase * 8.0 + .5)) % 8;
    date.moonName = names[octant];
    return date;
}

Sky skyAt(double absoluteGameDays, Weather weather)
{
    Sky sky;
    sky.date = calendarAt(absoluteGameDays);
    if (!sky.date.valid || !validWeather(weather))
        return sky;
    sky.valid = true;
    switch (weather)
    {
    case Weather::Clear:
        sky.moonTransmission = 1.0;
        break;
    case Weather::Rain:
        sky.moonTransmission = .35;
        break;
    case Weather::Fog:
        sky.moonTransmission = .2;
        break;
    case Weather::Snow:
        sky.moonTransmission = .45;
        break;
    }
    sky.nightLight = .08 + .32 * sky.date.moonIllumination * sky.moonTransmission;
    sky.outdoorIllumination = sky.nightLight + (1.0 - sky.nightLight) * sky.date.daylight;
    return sky;
}

std::array<unsigned, 4> weatherWeights(Season season, Climate climate)
{
    if (climate != Climate::Temperate)
        return {};
    switch (season)
    {
    case Season::Spring:
        return {450, 350, 180, 20};
    case Season::Summer:
        return {700, 200, 100, 0};
    case Season::Autumn:
        return {400, 350, 200, 50};
    case Season::Winter:
        return {300, 120, 180, 400};
    }
    return {};
}

Forecast forecast(std::uint64_t worldSeed, const std::string& cellId, std::uint64_t absoluteDay, int slot,
                  Climate climate)
{
    Forecast result;
    if (cellId.empty() || absoluteDay > static_cast<std::uint64_t>(MaxGameDays) || slot < 0 || slot > 3 ||
        climate != Climate::Temperate)
        return result;
    result.season = calendarAt(static_cast<double>(absoluteDay)).season;
    result.slot = slot;
    const auto weights = weatherWeights(result.season, climate);
    const auto hash = mix(mix(worldSeed) ^ mix(cellHash(cellId)) ^ mix(absoluteDay + UINT64_C(0x4f1bbcdc)) ^
                          mix(static_cast<std::uint64_t>(slot) + UINT64_C(0xa24baed4)));
    const auto roll = static_cast<unsigned>(hash % 1000);
    unsigned cumulative = 0;
    for (unsigned index = 0; index < weights.size(); ++index)
    {
        cumulative += weights[index];
        if (roll < cumulative)
        {
            result.valid = true;
            result.weather = static_cast<Weather>(index);
            return result;
        }
    }
    return result;
}

Forecast forecastAt(std::uint64_t worldSeed, const std::string& cellId, double absoluteGameDays, Climate climate)
{
    const auto date = calendarAt(absoluteGameDays);
    if (!date.valid)
        return {};
    return forecast(worldSeed, cellId, date.absoluteDay, static_cast<int>(date.hour / 6.0), climate);
}

std::string seasonName(Season season)
{
    switch (season)
    {
    case Season::Spring:
        return "spring";
    case Season::Summer:
        return "summer";
    case Season::Autumn:
        return "autumn";
    case Season::Winter:
        return "winter";
    }
    return "unknown";
}

std::string weatherName(Weather weather)
{
    switch (weather)
    {
    case Weather::Clear:
        return "clear";
    case Weather::Rain:
        return "rain";
    case Weather::Fog:
        return "fog";
    case Weather::Snow:
        return "snow";
    }
    return "unknown";
}
} // namespace ratw::calendar
