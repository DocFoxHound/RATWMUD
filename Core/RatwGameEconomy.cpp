// The staged economy's live acts (Docs/Design/79-a-staged-economy.md, 12; Phase 9): the dm.actions kinds the DM's
// Prosperity tab and the studio queue, applied by the game and journalled before anyone is told.
//
//   economy.sync      the world's live.economy read again: the rules, seasons, services, needs and the host's overrides,
//                     the slider's target (the rule prosperity) and its cause; {now}: no smoothing, the bands at once
//   times.nudge       a market's Times nudged {points -100..100, days 0..366, words}; points or days 0 lifts it
//   harvest.override  an area's harvest called {word poor|fair|good|clear, days (the season's left), words}
//   event.add         a staged event {kind, day (this calendar week or the next), hour 0..23}: "e<N>: ..."
//   event.cancel      a staged event cancelled (the target its id)
//   ship.storm        a ship still due kept out by a storm (the target its id)
//   prices.reset      a market's prices eased back to normal over a day (the target a market, or "all") {words}
//   band.override     an NPC's household held to a band {band word|clear, days 1..112 (28)}
//   character.grant   a player granted or fined {coins ±1..2500, reason}
//
// Every number is clamped, never refused; a refusal is only an unknown target or the hook's own reason. Each applied act
// is journalled (record: the Board whole when it differs, the staged state, the tills and the boards' notices), so a crash
// before the next checkpoint keeps it once; a player hears of a grant only after its record. Then the Prosperity report is
// worked out again (SocietyState::report).
#include "RatwCalendar.h"
#include "RatwGame.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace ratw::game
{
using json::Value;

namespace
{
constexpr int NudgeDaysMost = 366, BandDaysMost = 112, BandDaysUsual = 28;
constexpr std::int64_t GrantMost = 2500;
constexpr std::size_t WarningsLogged = 20;

std::string words80(const Value& payload, const char* key)
{
    auto w = json::utf8Prefix(payload.string(key), 80);
    while (!w.empty() && (w.back() == ' ' || w.back() == '.'))
        w.pop_back();
    while (!w.empty() && w.front() == ' ')
        w.erase(w.begin());
    return w;
}
double clampedNumber(const Value& payload, const char* key, double low, double high, double fallback)
{
    const double v = payload.number(key, fallback);
    return std::isfinite(v) ? std::clamp(v, low, high) : fallback;
}
// The days left in the calendar's season from `day` (a harvest called with no days: until the season turns).
double seasonLeft(double day)
{
    const auto season = calendar::calendarAt(day).season;
    double n = 1;
    while (n < 100 && calendar::calendarAt(std::floor(day) + n).season == season)
        ++n;
    return std::floor(day) + n - day;
}
} // namespace

bool Game::economyCommand(const std::string& kind)
{
    for (const char* k : {"economy.sync", "times.nudge", "harvest.override", "event.add", "event.cancel", "ship.storm", "prices.reset",
                          "band.override", "character.grant"})
        if (kind == k)
            return true;
    return false;
}

bool Game::readEconomy(EconomySpec& spec, std::size_t& skipped, std::string& problem)
{
    skipped = 0;
    if (economyReader)
        return economyReader(spec, problem);
    World candidate;
    bool loaded = false;
    if (!options_.database.empty())
        loaded = loadWithLivePeople(candidate, problem);
    else if (worldFiles_.count("world.ratw"))
    {
        // (A world export: its manifest read again from disk, the host's edit of its records, over the cells as loaded.)
        auto files = worldFiles_;
        if (!options_.worldExport.empty())
            if (std::ifstream in(std::filesystem::path(options_.worldExport) / "world.ratw", std::ios::binary); in)
            {
                std::ostringstream text;
                text << in.rdbuf();
                files["world.ratw"] = text.str();
            }
        if (streamedBuild_)
            candidate.setCellSource(cellSource());
        const auto read = candidate.loadWorldFiles(files, "world export");
        loaded = read.ok;
        problem = read.message;
    }
    else
    {
        // (A world from one file or the built-in town: its settings as loaded, since nothing can change them.)
        spec = world_.society().authored().economy;
        return true;
    }
    if (!loaded)
        return false;
    spec = candidate.society().authored().economy;
    skipped = candidate.society().authored().warningCount;
    logEconomyWarnings(candidate.society().authored());
    return true;
}

void Game::logEconomyWarnings(const AuthoredRoster& roster)
{
    for (std::size_t i = 0; i < roster.warnings.size() && i < WarningsLogged; ++i)
        note("warn", "RATW_ECONOMY skipped " + roster.warnings[i]);
    if (roster.warningCount > WarningsLogged)
        note("warn", "RATW_ECONOMY and " + std::to_string(roster.warningCount - WarningsLogged) + " more economy records skipped");
}

void Game::applyEconomySpec(const EconomySpec& spec, bool now)
{
    auto& society = world_.society();
    society.adoptEconomy(spec);
    std::vector<std::string> warnings;
    society.adoptOverrides(spec.overrides, &warnings);
    for (std::size_t i = 0; i < warnings.size() && i < WarningsLogged; ++i)
        note("warn", "RATW_ECONOMY override " + warnings[i]);
    // The slider re-meant (12): the rule prosperity is where it heads (absent: 0, so an old world opens at 0), with the
    // host's cause; `now` skips the smoothing and works the bands out at once.
    society.setProsperity(society.rule("prosperity"), now, spec.overrides.cause);
    if (now && society.state().staged.day >= 0)
        society.stagedBands(society.state().staged.day, true);
    world_.refreshRoadMarkets();                    // (The goods no ship brings may have changed: the ports' imports again.)
}

void Game::startEconomy()
{
    // At start, after the restore: the world's settings and overrides in force, the slider's target from the rules (its
    // smoothed value as saved), and the report worked out. A world never staged before says so, once.
    if (!world_.restoredStaged())
        note("info", "RATW_ECONOMY economy staged (doc 79)");
    logEconomyWarnings(world_.society().authored());
    applyEconomySpec(world_.society().authored().economy, false);
    world_.society().setReport(world_.prosperityReport());
}

Result Game::applyEconomyCommand(const std::string& kind, const std::string& target, const Value& payload)
{
    auto& society = world_.society();
    const auto boardBefore = json::dump(market::encode(society.board()));
    Result outcome{false, "Unknown action.", target};
    unsigned journal = Economy;
    std::string character;
    std::function<void()> tell;                     // (A player's word of it, after the record.)
    if (kind == "economy.sync")
    {
        EconomySpec spec;
        std::size_t skipped = 0;
        std::string problem;
        if (!readEconomy(spec, skipped, problem))
            return {false, "The economy's rules could not be read: " + problem, target};
        const bool now = payload.boolean("now", false);
        applyEconomySpec(spec, now);
        outcome = {true, "The economy's rules are read again and in force." +
                             (skipped ? " (" + std::to_string(skipped) + " records skipped)" : std::string()),
                   target};
        // (Journalled only when the Board or the staged state moved: a sync that changes nothing writes nothing.)
        if (json::dump(market::encode(society.board())) == boardBefore && !now)
            journal = 0;
    }
    else if (kind == "times.nudge")
    {
        const int points = int(std::lround(clampedNumber(payload, "points", -100, 100, 0)));
        const double days = std::round(clampedNumber(payload, "days", 0, NudgeDaysMost, 7));
        auto words = words80(payload, "words");
        if (words.empty() && points != 0)
            words = points > 0 ? "good news at the market" : "bad news at the market";
        if (!world_.roadMarket(target))
            outcome = {false, "No such market.", target};
        else if (!world_.nudgeTimes(target, points, days, words))
            outcome = {false, "Those words won't do: plain words, 80 at most.", target};
        else
            outcome = {true, points == 0 || days <= 0 ? "The nudge at " + world_.marketWords(target) + " is lifted."
                                                      : "The times at " + world_.marketWords(target) + " nudged " +
                                                            (points > 0 ? "+" : "") + std::to_string(points) + " for " +
                                                            std::to_string(int(days)) + " days: " + words + ".",
                       target};
    }
    else if (kind == "harvest.override")
    {
        const auto word = payload.string("word");
        const int w = word == "poor" ? 0 : word == "fair" ? 1 : word == "good" ? 2 : word == "clear" ? -1 : -2;
        if (w == -2)
            outcome = {false, "Call the harvest poor, fair or good, or clear it.", target};
        else if (w == -1)
            outcome = world_.clearHarvest(target) ? Result{true, "The harvest at " + target + " is as the season gives it again.", target}
                                                  : Result{false, "No harvest is called there.", target};
        else
        {
            const double days = std::round(clampedNumber(payload, "days", 1, NudgeDaysMost, seasonLeft(world_.calendarDays())));
            auto words = words80(payload, "words");
            if (words.empty())
                words = w == 0 ? "a poor harvest" : w == 2 ? "a good harvest" : "a fair harvest";
            outcome = world_.overrideHarvest(target, w, std::max(1., days), words)
                          ? Result{true, "The harvest at " + target + " called " + word + " for " + std::to_string(int(std::max(1., days))) +
                                             " days: " + words + ".", target}
                          : Result{false, "No such farm area, or those words won't do.", target};
        }
    }
    else if (kind == "event.add")
    {
        const auto today = std::int64_t(std::floor(world_.calendarDays()));
        const auto week = today / calendar::DaysPerWeek;
        const auto day = std::int64_t(std::lround(clampedNumber(payload, "day", double(today), double((week + 2) * calendar::DaysPerWeek - 1), double(today))));
        const double hour = clampedNumber(payload, "hour", 0, 23, 9);
        std::string problem;
        if (!world_.roadMarket(target))
            outcome = {false, "No such market.", target};
        else if (const auto id = world_.addStagedEvent(target, payload.string("kind"), day, hour, problem); id.empty())
            outcome = {false, problem.empty() ? "That event can't be added there." : problem, target};
        else
        {
            const auto* e = world_.stagedEvent(id);
            outcome = {true, id + ": " + (e ? e->words : payload.string("kind")) + " at " + world_.marketWords(target) + ", " +
                                 calendar::weekdayName(calendar::weekdayOf(double(day) + .5)) + ".", id};
        }
    }
    else if (kind == "event.cancel")
        outcome = world_.cancelStagedEvent(target) ? Result{true, "The event " + target + " is cancelled.", target}
                                                   : Result{false, "No such event still to come or on.", target};
    else if (kind == "ship.storm")
    {
        journal = Economy | Roads;
        outcome = world_.stormShip(target) ? Result{true, "A storm keeps the ship " + target + " out.", target}
                                           : Result{false, "No such ship still due.", target};
    }
    else if (kind == "prices.reset")
    {
        auto words = words80(payload, "words");
        if (words.empty() || !market::plainWords(words, 80))
            words = "fresh stock everywhere";
        const auto which = target == "all" ? std::string() : target;
        const double at = society.marketClock();
        if (!society.resetPrices(which, at, words))
            outcome = {false, "No such market.", target};
        else
        {
            for (const auto& [id, reset] : society.board().resets)
                if ((which.empty() || id == which) && reset.from == at)
                    if (const auto* m = world_.roadMarket(id))
                        world_.recordEvent({"prices reset", id, id, m->market, 0, 0, {}, 0, 0, words});
            outcome = {true, (which.empty() ? std::string("Prices everywhere") : "Prices at " + world_.marketWords(which)) +
                                 " ease back to their usual over a day: " + words + ".", target};
        }
    }
    else if (kind == "band.override")
    {
        const auto word = payload.string("band");
        const int band = word == "clear" ? -1 : status::bandOf(word);
        const auto household = society.householdOf(target);
        const auto days = std::int64_t(std::lround(clampedNumber(payload, "days", 1, BandDaysMost, BandDaysUsual)));
        if (!society.resident(target))
            outcome = {false, "No such resident.", target};
        else if (household.empty())
            outcome = {false, "That resident has no household yet.", target};
        else if (band < 0 && word != "clear")
            outcome = {false, "No band " + word + ": destitute, poor, comfortable or rich, or clear.", target};
        else
        {
            society.overrideBand(household, band, society.state().budgetDay + days);
            outcome = {true, band < 0 ? "The band of " + target + "'s household is theirs again."
                                      : target + "'s household is held " + word + " for " + std::to_string(days) + " days.",
                       target};
        }
    }
    else if (kind == "character.grant")
    {
        auto coins = std::int64_t(std::llround(clampedNumber(payload, "coins", -double(GrantMost), double(GrantMost), 0)));
        const auto reason = payload.string("reason");
        std::string problem;
        journal = Economy | Character;
        character = target;
        if (society.grantPlayer(target, coins, reason, problem))
        {
            outcome = {true, (coins > 0 ? "Granted " : "Fined ") + std::to_string(std::abs(coins)) + "p: " + reason + ".", target};
            if (auto* c = clientOf(target))
                tell = [this, c, coins, reason] {
                    system(c, coins > 0 ? "A Dungeon Master grants you " + std::to_string(coins) + " pennies: " + reason + "."
                                        : "A Dungeon Master fines you " + std::to_string(-coins) + " pennies: " + reason + ".");
                };
        }
        else
            outcome = {false, problem, target};
    }
    if (!outcome.ok)
        return outcome;
    if (journal)
        record(journal, character);                 // (Before anyone is told: RatwEvents.cpp's hooks journal nothing.)
    if (tell)
        tell();
    society.setReport(world_.prosperityReport());
    return outcome;
}
} // namespace ratw::game
