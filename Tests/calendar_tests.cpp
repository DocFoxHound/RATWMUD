#include "RatwCalendar.h"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace cal = ratw::calendar;
namespace
{
int checks = 0;
void expect(bool condition, const char* message)
{
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}
bool near(double a, double b, double epsilon = 1e-8)
{
    return std::abs(a - b) < epsilon;
}

void datesAndBounds()
{
    const auto epoch = cal::calendarAt(0);
    expect(epoch.valid && epoch.absoluteDay == 0 && epoch.year == 1 && epoch.dayOfYear == 1 && epoch.dayOfSeason == 1 &&
               epoch.season == cal::Season::Spring && near(epoch.hour, 0),
           "Epoch is midnight on Spring 1 in Year 1");
    expect(near(cal::SecondsPerDay, 4 * 60 * 60) && cal::DaysPerYear == 365,
           "Four real hours make one leapless 365-day-calendar day");
    expect(near(cal::SecondsPerDay * cal::DaysPerYear, 5256000),
           "One continuously running game year takes 1460 real hours");
    const auto last = cal::calendarAt(364.999999);
    expect(last.year == 1 && last.dayOfYear == 365 && last.dayOfSeason == 90 && last.season == cal::Season::Winter &&
               last.hour > 23.99,
           "Year ends at Winter 90 without an extra or missing day");
    for (unsigned year = 1; year <= 9; ++year)
    {
        const auto next = cal::calendarAt(year * 365.0);
        expect(next.year == year + 1 && next.dayOfYear == 1 && next.dayOfSeason == 1 &&
                   next.season == cal::Season::Spring && near(next.hour, 0),
               "Every year wraps after exactly 365 days including Earth leap-year numbers");
    }
    for (const auto& boundary :
         std::array<std::pair<double, cal::Season>, 4>{std::pair<double, cal::Season>{0, cal::Season::Spring},
                                                       {92, cal::Season::Summer},
                                                       {184, cal::Season::Autumn},
                                                       {275, cal::Season::Winter}})
    {
        const auto date = cal::calendarAt(boundary.first);
        expect(date.season == boundary.second && date.dayOfSeason == 1,
               "Every season begins on its documented whole-day boundary");
        const auto nextYear = cal::calendarAt(boundary.first + 365);
        expect(nextYear.season == boundary.second && nextYear.dayOfSeason == 1 && nextYear.year == 2,
               "Season boundaries repeat unchanged in subsequent years");
    }
    const auto springEnd = cal::calendarAt(91.9999);
    const auto summerEnd = cal::calendarAt(183.9999);
    const auto autumnEnd = cal::calendarAt(274.9999);
    expect(springEnd.season == cal::Season::Spring && springEnd.dayOfSeason == 92, "Spring retains its full 92nd day");
    expect(summerEnd.season == cal::Season::Summer && summerEnd.dayOfSeason == 92, "Summer retains its full 92nd day");
    expect(autumnEnd.season == cal::Season::Autumn && autumnEnd.dayOfSeason == 91, "Autumn retains its full 91st day");
    const auto far = cal::calendarAt(cal::MaxGameDays);
    expect(far.valid && far.year == 1000001 && far.dayOfYear == 1,
           "Documented maximum epoch remains finite and safely convertible");
    for (double value :
         {-1.0, -std::numeric_limits<double>::denorm_min(), std::numeric_limits<double>::infinity(),
          -std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN(), cal::MaxGameDays + 1})
    {
        const auto invalid = cal::calendarAt(value);
        expect(!invalid.valid && invalid.absoluteDay == 0 && near(invalid.hour, 0),
               "Corrupt epochs are rejected instead of clamped or integer-overflowed");
        expect(!cal::skyAt(value, cal::Weather::Clear).valid, "A corrupt epoch cannot produce a valid sky");
        expect(!cal::forecastAt(1, "glade", value).valid, "A corrupt epoch cannot produce a valid forecast");
    }
    expect(cal::seasonName(cal::Season::Spring) == "spring" && cal::seasonName(cal::Season::Summer) == "summer" &&
               cal::seasonName(cal::Season::Autumn) == "autumn" && cal::seasonName(cal::Season::Winter) == "winter",
           "Season names are stable readable identifiers");
    expect(cal::seasonName(static_cast<cal::Season>(-1)) == "unknown",
           "Invalid season labels do not invent a valid season");
}

void equalDayAndNight()
{
    for (double seasonStart : {0.0, 92.0, 184.0, 275.0})
    {
        const auto dawn = cal::calendarAt(seasonStart + 6.0 / 24);
        const auto dusk = cal::calendarAt(seasonStart + 18.0 / 24);
        const auto nextDawn = cal::calendarAt(seasonStart + 1 + 6.0 / 24);
        expect(near(dawn.daylight, .5) && dawn.period == "dawn" && near(dusk.daylight, .5) && dusk.period == "dusk",
               "Dawn and dusk straddle 06:00 and 18:00 symmetrically in every season");
        expect(near((dusk.absoluteDays - dawn.absoluteDays) * cal::SecondsPerDay, 7200) &&
                   near((nextDawn.absoluteDays - dusk.absoluteDays) * cal::SecondsPerDay, 7200),
               "Morning-to-evening and evening-to-morning each take two real hours");
        for (double hour : {0.0, 4.0, 5.0, 19.0, 23.0})
            expect(near(cal::calendarAt(seasonStart + hour / 24).daylight, 0),
                   "Night and twilight outer boundaries have no daylight");
        for (double hour : {7.0, 12.0, 17.0})
            expect(near(cal::calendarAt(seasonStart + hour / 24).daylight, 1),
                   "Day and twilight inner boundaries retain full daylight");
    }
    for (double hour : {5.0, 7.0, 17.0, 19.0})
    {
        const double before = cal::calendarAt((hour - .00001) / 24).daylight;
        const double at = cal::calendarAt(hour / 24).daylight;
        const double after = cal::calendarAt((hour + .00001) / 24).daylight;
        expect(near(before, at) && near(at, after), "Twilight ramps remain continuous at phase boundaries");
    }
    bool inBounds = true, symmetric = true;
    for (int minute = 0; minute < 1440; ++minute)
    {
        const auto date = cal::calendarAt(minute / 1440.0);
        const auto opposite = cal::calendarAt(std::fmod(minute / 1440.0 + .5, 1.0));
        inBounds &= date.hour >= 0 && date.hour < 24 && date.daylight >= 0 && date.daylight <= 1;
        symmetric &= near(date.daylight + opposite.daylight, 1);
    }
    expect(inBounds, "Every minute produces bounded hours and daylight");
    expect(symmetric, "The complete light curve has exact opposite-half symmetry");
    expect(cal::calendarAt(0).period == "night" && cal::calendarAt(.25).period == "dawn" &&
               cal::calendarAt(.5).period == "day" && cal::calendarAt(.75).period == "dusk",
           "Four phase labels agree with their corresponding clock intervals");
}

void moonAndWeatherLight()
{
    const char* names[] = {"new moon",  "waxing crescent", "first quarter", "waxing gibbous",
                           "full moon", "waning gibbous",  "last quarter",  "waning crescent"};
    for (unsigned phase = 0; phase < 8; ++phase)
    {
        const auto date = cal::calendarAt(cal::LunarCycleDays * phase / 8);
        expect(near(date.moonPhase, phase / 8.0) && date.moonName == names[phase],
               "Moon progresses through the eight standard phase names in order");
        const auto repeated = cal::calendarAt(cal::LunarCycleDays * (2 + phase / 8.0));
        expect(near(date.moonIllumination, repeated.moonIllumination) && date.moonName == repeated.moonName,
               "Fractional lunar periods repeat without rounding to 29 or 30 days");
    }
    expect(near(cal::calendarAt(0).moonIllumination, 0) &&
               near(cal::calendarAt(cal::LunarCycleDays * .5).moonIllumination, 1),
           "New moon is unilluminated and full moon fully illuminated");
    expect(near(cal::calendarAt(cal::LunarCycleDays * .25).moonIllumination, .5) &&
               near(cal::calendarAt(cal::LunarCycleDays * .75).moonIllumination, .5),
           "First and last quarter each illuminate half the lunar disk");
    const auto beforeNew = cal::calendarAt(cal::LunarCycleDays - .00001);
    const auto afterNew = cal::calendarAt(cal::LunarCycleDays + .00001);
    expect(beforeNew.moonPhase > .99 && afterNew.moonPhase < .01 &&
               near(beforeNew.moonIllumination, afterNew.moonIllumination),
           "Moon phase wraps while illumination remains continuous");
    expect(!near(cal::calendarAt(365).moonPhase, 0),
           "Lunar cycle does not reset or drift to a year-aligned month at New Year");
    const auto newSky = cal::skyAt(0, cal::Weather::Clear);
    const auto fullSky = cal::skyAt(cal::LunarCycleDays * .5, cal::Weather::Clear);
    expect(newSky.valid && near(newSky.nightLight, .08) && near(newSky.outdoorIllumination, .08),
           "New moon midnight retains only provisional close-awareness darkness floor");
    expect(near(fullSky.nightLight, .4), "Clear full moon produces the provisional 40-percent night factor");
    for (auto weather : {cal::Weather::Clear, cal::Weather::Rain, cal::Weather::Fog, cal::Weather::Snow})
    {
        const auto dark = cal::skyAt(0, weather);
        const auto midday = cal::skyAt(.5, weather);
        const auto moon = cal::skyAt(cal::LunarCycleDays * .5, weather);
        expect(near(dark.nightLight, .08) && near(dark.outdoorIllumination, .08),
               "Weather cannot subtract the local-awareness floor at new moon");
        expect(near(midday.outdoorIllumination, 1),
               "Daytime sky provides full light before separate weather sight penalties");
        expect(moon.nightLight >= .08 && moon.nightLight <= .4 && moon.outdoorIllumination <= 1,
               "Every weather sky remains bounded");
    }
    const auto rain = cal::skyAt(cal::LunarCycleDays * .5, cal::Weather::Rain);
    const auto fog = cal::skyAt(cal::LunarCycleDays * .5, cal::Weather::Fog);
    const auto snow = cal::skyAt(cal::LunarCycleDays * .5, cal::Weather::Snow);
    expect(near(rain.nightLight, .192) && near(fog.nightLight, .144) && near(snow.nightLight, .224),
           "Cloud and fog transmission measurably attenuate full-moon night brightness");
    expect(fog.nightLight < rain.nightLight && rain.nightLight < snow.nightLight &&
               snow.nightLight < fullSky.nightLight,
           "Weather-driven night brightness follows the documented transmission tuning");
    expect(cal::skyAt(14, cal::Weather::Clear).outdoorIllumination >
               cal::skyAt(14, cal::Weather::Fog).outdoorIllumination,
           "Cloud attenuation affects actual outdoor illumination during near-full midnight");
    expect(!cal::skyAt(0, static_cast<cal::Weather>(99)).valid,
           "Unknown weather enum cannot silently generate a valid sky");
    expect(cal::weatherName(cal::Weather::Clear) == "clear" && cal::weatherName(cal::Weather::Rain) == "rain" &&
               cal::weatherName(cal::Weather::Fog) == "fog" && cal::weatherName(cal::Weather::Snow) == "snow" &&
               cal::weatherName(static_cast<cal::Weather>(-1)) == "unknown",
           "Weather names map explicitly and unknown inputs remain unknown");
}

void deterministicClimate()
{
    const std::array<cal::Season, 4> seasons = {cal::Season::Spring, cal::Season::Summer, cal::Season::Autumn,
                                                cal::Season::Winter};
    const std::array<std::uint64_t, 4> days = {0, 92, 184, 275};
    for (unsigned season = 0; season < seasons.size(); ++season)
    {
        const auto weights = cal::weatherWeights(seasons[season]);
        expect(std::accumulate(weights.begin(), weights.end(), 0u) == 1000,
               "Every temperate season probability table totals exactly 1000");
        std::array<unsigned, 4> counts{};
        bool valid = true;
        for (unsigned sample = 0; sample < 10000; ++sample)
        {
            const auto result = cal::forecast(sample, "weather-test-cell", days[season], sample % 4);
            valid &= result.valid && result.season == seasons[season] && result.slot == static_cast<int>(sample % 4);
            ++counts[static_cast<unsigned>(result.weather)];
        }
        expect(valid, "Every sampled forecast reports its actual season and deterministic slot");
        bool distributed = true;
        for (unsigned weather = 0; weather < weights.size(); ++weather)
            distributed &= std::abs(counts[weather] / 10000.0 - weights[weather] / 1000.0) < .025;
        expect(distributed, "Deterministic samples reflect the authored seasonal probabilities");
        if (seasons[season] == cal::Season::Summer)
            expect(counts[3] == 0, "Temperate summer cannot select zero-weight snow");
    }
    const auto spring = cal::weatherWeights(cal::Season::Spring);
    const auto summer = cal::weatherWeights(cal::Season::Summer);
    const auto autumn = cal::weatherWeights(cal::Season::Autumn);
    const auto winter = cal::weatherWeights(cal::Season::Winter);
    expect(summer[0] > spring[0] && summer[0] > autumn[0] && summer[0] > winter[0],
           "Summer is more prone to clear skies than other seasons");
    expect(winter[3] > spring[3] && winter[3] > summer[3] && winter[3] > autumn[3],
           "Winter is more prone to snow than other seasons");
    expect(spring[1] > summer[1] && autumn[1] > summer[1],
           "Spring and autumn have wetter temperate forecasts than summer");
    expect(cal::weatherWeights(static_cast<cal::Season>(-1)) == std::array<unsigned, 4>{} &&
               cal::weatherWeights(cal::Season::Spring, static_cast<cal::Climate>(-1)) == std::array<unsigned, 4>{},
           "Unsupported season and climate tables reject rather than silently substituting");
    unsigned differingSeed = 0, differingCell = 0, differingSlot = 0;
    bool repeatable = true;
    for (unsigned day = 0; day < 1000; ++day)
    {
        const auto original = cal::forecast(42, "glade", day, 0);
        // An intervening unrelated query must not advance shared PRNG state.
        (void)cal::forecast(1001, "another-place", 12222, 3);
        const auto replay = cal::forecast(42, "glade", day, 0);
        repeatable &= original.valid && replay.valid && original.weather == replay.weather;
        differingSeed += original.weather != cal::forecast(43, "glade", day, 0).weather;
        differingCell += original.weather != cal::forecast(42, "meadow", day, 0).weather;
        differingSlot += original.weather != cal::forecast(42, "glade", day, 1).weather;
    }
    expect(repeatable, "Forecast replay is independent of process history and unrelated query order");
    expect(differingSeed > 100 && differingCell > 100 && differingSlot > 100,
           "Seed, cell, and time slot each independently affect the forecast");
    for (int slot = 0; slot < 4; ++slot)
    {
        const auto start = cal::forecastAt(42, "glade", 275 + slot / 4.0);
        const auto middle = cal::forecastAt(42, "glade", 275 + slot / 4.0 + .124);
        const auto end = cal::forecastAt(42, "glade", 275 + (slot + 1) / 4.0 - .000001);
        const auto explicitCall = cal::forecast(42, "glade", 275, slot);
        expect(start.valid && start.slot == slot && start.weather == middle.weather && start.weather == end.weather &&
                   start.weather == explicitCall.weather,
               "A weather slot remains stable for six game hours and matches explicit-key lookup");
    }
    expect(cal::forecastAt(42, "glade", 276).slot == 0,
           "Midnight rolls to the next absolute day and first weather slot");
    expect(cal::forecast(std::numeric_limits<std::uint64_t>::max(), "\xE2\x98\x83-cell", 365000000, 3).valid,
           "Largest seed, UTF-8 cell bytes, and largest accepted day are portable inputs");
    expect(!cal::forecast(1, "", 0, 0).valid && !cal::forecast(1, "cell", 0, -1).valid &&
               !cal::forecast(1, "cell", 0, 4).valid &&
               !cal::forecast(1, "cell", std::numeric_limits<std::uint64_t>::max(), 0).valid &&
               !cal::forecast(1, "cell", 0, 0, static_cast<cal::Climate>(-1)).valid,
           "Malformed forecast keys are rejected without unsafe conversion or fallback claims");
}
} // namespace

int main()
{
    try
    {
        datesAndBounds();
        equalDayAndNight();
        moonAndWeatherLight();
        deterministicClimate();
        std::cout << "Passed " << checks << " calendar, lunar, and seasonal climate checks.\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Calendar check " << checks << " failed: " << error.what() << '\n';
        return 1;
    }
}
