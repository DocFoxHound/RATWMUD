// Mantles passed on, the host's side (Docs/Design/84-living-conversations.md §12, §17.7, Phase 10): the society decides
// (Core/RatwHandoff.cpp); the game gives it the ready mantles' cards and the rules, reconciles its ledger with a database
// world's bindings (live.npc_voice: the game writes them at birth, death and arrival, never a mantle's own rows), puts a
// binding to work (the mantle worn, a newborn's voice, re-drawn marks as the wearer's own words), logs the pools' health
// once a game day, and carries out the DM's mantle.assign and mantle.release.
#include "RatwGame.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace ratw::game
{
namespace
{
bool readFile(const std::filesystem::path& path, std::string& out)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;
    std::ostringstream all;
    all << in.rdbuf();
    out = all.str();
    return true;
}
} // namespace

bool Game::mantleRows() const
{
    return !options_.database.empty() && !options_.scratch && !liveWorldId_.empty() && mantleTables_ && options_.mantlesFrom.empty();
}

void Game::loadMantleRules()
{
    if (mantleRulesLoaded_)
        return;
    mantleRulesLoaded_ = true;
    MantleRules rules;
    rules.restDays = talk_.settings().mantleRestDays;
    rules.fallback = talk_.settings().mantleFallback;
    // The name pools (§7.10): Data/Talk/names/<dialect>.names. A pool that fails is left out and said so.
    if (!talkDir_.empty())
    {
        std::error_code error;
        for (std::filesystem::directory_iterator it(std::filesystem::path(talkDir_) / "names", error), end; !error && it != end;
             it.increment(error))
        {
            if (it->path().extension() != ".names")
                continue;
            std::string text, dialect;
            NamePool pool;
            std::vector<std::string> problems;
            if (!readFile(it->path(), text) || !readNamePool(text, it->path().string(), dialect, pool, problems))
            {
                std::string why;
                for (const auto& p : problems)
                    why += "\n" + p;
                note("warn", "RATW_MANTLE the name pool " + it->path().filename().string() + " was refused:" + why);
                continue;
            }
            rules.names[dialect] = std::move(pool);
        }
    }
    world_.society().setMantleRules(std::move(rules));
}

void Game::syncMantleLedger()
{
    loadMantleRules();
    auto& society = world_.society();
    society.setMantleCards(mantlePoolCards_);
    auto& ledger = society.mantleLedger();
    const double day = world_.calendarDays();
    if (!options_.database.empty() && mantleTables_ && options_.mantlesFrom.empty())
    {
        // A database world: its rows are the truth (§17.7), the checkpoint keeps what they don't (who knew the wearer, an
        // echo, re-drawn marks, a newborn's voice). A scratch server writes no row, so what it made itself stays.
        if (!options_.scratch)
            for (auto it = ledger.begin(); it != ledger.end();)
                it = rowBindings_.count(it->first) ? std::next(it) : ledger.erase(it);
        for (const auto& [npc, row] : rowBindings_)
        {
            auto& b = ledger[npc];
            if (b.mantle != row.mantle)
                b = MantleBinding{};
            b.mantle = row.mantle;
            b.how = row.how;
            b.boundDay = row.bound;
            b.releasedDay = row.released;
            if (b.dialect.empty())
                if (const auto* card = society.mantleCard(row.mantle))
                    b.dialect = card->dialect;
        }
    }
    else
    {
        // A world from files: the ledger is the checkpoint's; a mantle no binding names yet is worn by the resident it was
        // written for (loadMantles found them), as an authored binding.
        std::set<std::string> named;
        for (const auto& [npc, b] : ledger)
            named.insert(b.mantle);
        for (const auto& [npc, mantle] : mantleOf_)
            if (!ledger.count(npc) && !named.count(mantle))
            {
                std::string why;
                society.bindMantle(npc, mantle, "authored", day, why, false);
                named.insert(mantle);
            }
    }
    // Who wears what, by the ledger: living wearers of mantles not released.
    std::map<std::string, std::string> worn;
    for (const auto& [npc, b] : ledger)
        if (const auto* e = world_.entity(npc); e && e->npc && !e->dead && b.releasedDay < 0 && !b.mantle.empty())
            worn[npc] = b.mantle;
    for (auto it = mantleOf_.begin(); it != mantleOf_.end();)
        it = worn.count(it->first) && worn[it->first] == it->second ? std::next(it) : mantleOf_.erase(it);
    for (const auto& [npc, mantle] : worn)
        if (!mantleOf_.count(npc))
            wearBinding(npc);
        else if (const auto* b = society.mantleOf(npc); b && (b->born || !b->marks.empty() || !b->markDialect.empty()))
            wearBinding(npc);
    talkDaily_.clear();
}

std::map<std::string, std::string> Game::drawMarks(const std::string& mantle, const std::string& bank, const std::string& wearer)
{
    std::map<std::string, std::string> out;
    const auto* card = world_.society().mantleCard(mantle);
    if (!card)
        return out;
    for (const auto& mark : card->marks)
        if (const auto value = talk_.redrawHabit(mantle, mark, wearer, bank); !value.empty())
            out[mark] = value;
    return out;
}

bool Game::writeBinding(const std::string& npc, const MantleBinding& b, std::string& problem)
{
    if (!mantleRows())
        return true;
    const auto released = b.releasedDay >= 0 ? std::to_string(b.releasedDay) : std::string();
    const auto set = worldDb_.exec("UPDATE live.npc_voice SET mantle_id = $3, bound_how = $4, bound_day = $5::double precision, "
                                   "released_day = NULLIF($6, '')::double precision WHERE world_id = $1 AND npc_id = $2 RETURNING npc_id",
                                   {liveWorldId_, npc, b.mantle, b.how, std::to_string(b.boundDay), released});
    if (set.ok && !set.rows.empty())
        return true;
    const auto made = set.ok ? worldDb_.exec("INSERT INTO live.npc_voice (world_id, npc_id, mantle_id, bound_how, bound_day, released_day) "
                                             "VALUES ($1, $2, $3, $4, $5::double precision, NULLIF($6, '')::double precision)",
                                             {liveWorldId_, npc, b.mantle, b.how, std::to_string(b.boundDay), released})
                             : set;
    if (!made.ok)
        problem = made.error;
    return made.ok;
}

void Game::wearBinding(const std::string& npc)
{
    auto& society = world_.society();
    const auto* b = society.mantleOf(npc);
    const auto* e = world_.entity(npc);
    if (!b || b->mantle.empty() || b->releasedDay >= 0 || !e || e->dead)
    {
        mantleOf_.erase(npc);
        return;
    }
    const auto mantle = b->mantle;
    if (!talk_.mantle(mantle))
        loadMantle(mantle);                         // (A database world loads only the mantles worn: this one now.)
    if (!talk_.mantle(mantle))
    {
        note("warn", "RATW_MANTLE " + npc + " is bound to " + mantle + ", which isn't loaded: they speak from the library");
        mantleOf_.erase(npc);
        return;
    }
    // Their marks drawn again (§12.4): from their own dialect's mark bank, else another of the mantle's own words.
    if (!b->markDialect.empty())
    {
        auto marks = drawMarks(mantle, b->markDialect, npc);
        society.setMantleMarks(npc, marks);
        if (auto& ledger = society.mantleLedger(); ledger.count(npc))
            ledger[npc].markDialect.clear();
        b = society.mantleOf(npc);
        if (mantleRows() && !marks.empty())
        {
            // Into their own words, never the mantle's (an override is that wolf's alone: §11).
            const auto row = worldDb_.exec("SELECT grammar FROM live.npc_voice WHERE world_id = $1 AND npc_id = $2", {liveWorldId_, npc});
            std::string grammar = row.ok && !row.rows.empty() && row.rows.front()[0] ? *row.rows.front()[0] : std::string();
            for (const auto& [symbol, value] : marks)
                if (grammar.find("\n" + symbol + "!") == std::string::npos && grammar.rfind(symbol + "!", 0) != 0)
                    grammar += (grammar.empty() || grammar.back() == '\n' ? "" : "\n") + symbol + "! = " + value + "\n";
            worldDb_.exec("UPDATE live.npc_voice SET grammar = $3 WHERE world_id = $1 AND npc_id = $2", {liveWorldId_, npc, grammar});
        }
    }
    if (mantleRows())
    {
        loadVoice(npc);                             // (Their row: the voice, their own words, the mantle.)
        mantleOf_[npc] = mantle;
        return;
    }
    // A world from files (or a scratch server): the ledger is their voice.
    if (b->born)
    {
        auto& v = npcVoice_[npc];
        v.from = b->from, v.accent = b->accent, v.quirk = b->quirk, v.traits = b->traits;
        v.mantle = mantle;
        voicesChanged();
    }
    else if (auto v = npcVoice_.find(npc); v != npcVoice_.end())
        v->second.mantle = mantle;
    if (!b->marks.empty() && !voiceLayers_.count(npc))
    {
        const auto layer = talk::Library::personalLayer(npc);
        std::string grammar = "grammar " + layer + "\n", problem;
        for (const auto& [symbol, value] : b->marks)
            grammar += symbol + "! = " + value + "\n";
        if (talk_.setGrammar(layer, grammar, problem))
            voiceLayers_.insert(npc);
        else
            note("warn", "RATW_MANTLE " + npc + "'s re-drawn marks were refused: " + problem);
    }
    mantleOf_[npc] = mantle;
    talkDaily_.erase(npc);
    favourites_.erase(npc);
}

void Game::tendMantles()
{
    auto& society = world_.society();
    for (const auto& c : society.takeMantleChanges())
    {
        if (c.kind == "none")
        {
            note("info", "RATW_MANTLE none for " + c.dialect + " (" + c.how + (c.npc.empty() ? "" : ": " + c.npc) + "): born with the voice it had");
            continue;
        }
        const auto* b = society.mantleOf(c.npc);
        if (c.kind == "release")
        {
            mantleOf_.erase(c.npc);
            talkDaily_.erase(c.npc);
            if (mantleRows())
            {
                const auto done = worldDb_.exec("UPDATE live.npc_voice SET released_day = $3::double precision WHERE world_id = $1 AND "
                                                "npc_id = $2 AND mantle_id = $4 AND released_day IS NULL",
                                                {liveWorldId_, c.npc, std::to_string(c.day), c.mantle});
                if (!done.ok)
                    note("warning", "RATW_MANTLE " + c.mantle + "'s release could not be written: " + done.error);
            }
            note("info", "RATW_MANTLE " + c.mantle + " rests: " + c.npc + " died on day " + std::to_string(int(c.day)) +
                             (b ? " (known well by " + std::to_string(b->knownBy.size()) + ")" : ""));
            continue;
        }
        if (c.kind == "bind" && b && b->mantle == c.mantle && b->releasedDay < 0)
        {
            std::string problem;
            if (!writeBinding(c.npc, *b, problem))
                note("warning", "RATW_MANTLE " + c.npc + "'s binding to " + c.mantle + " could not be written: " + problem);
            wearBinding(c.npc);
            const auto* e = world_.entity(c.npc);
            note("info", "RATW_MANTLE " + (e ? e->name : c.npc) + " (" + c.npc + ") wears " + c.mantle + " (" + c.how +
                             (b->echoOf.empty() ? "" : ", an echo of " + b->echoOf) + (b->marks.empty() ? "" : ", marks drawn again") + ")");
        }
    }
    // The pools' health, once a game day (§12.1).
    const auto day = std::int64_t(std::floor(world_.calendarDays()));
    if (day == mantlePoolDay_ || mantlePoolCards_.empty())
        return;
    mantlePoolDay_ = day;
    for (const auto& p : world_.mantlePools())
    {
        note(p.low ? "warn" : "info", "RATW_MANTLE pool " + p.dialect + " spare=" + std::to_string(p.spare) + " resting=" +
                                          std::to_string(p.resting) + " ready=" + std::to_string(p.ready) +
                                          (p.low ? " LOW: under 1.5 x the " + std::to_string(p.births180).substr(0, 4) +
                                                       " births expected in 180 days, plus 2"
                                                 : ""));
    }
}

Result Game::assignMantle(const std::string& npc, const std::string& mantle, const std::string& how)
{
    auto& society = world_.society();
    const auto* e = world_.entity(npc);
    if (!e || !e->npc || e->dead)
        return {false, "No living resident " + npc + ".", npc};
    std::string why;
    MantleBinding was;
    if (const auto* b = society.mantleOf(npc))
        was = *b;
    if (!society.bindMantle(npc, mantle, how.empty() ? std::string("dm") : how, world_.calendarDays(), why))
        return {false, why, npc};
    const auto* b = society.mantleOf(npc);
    std::string problem;
    if (!writeBinding(npc, *b, problem))
    {
        society.mantleLedger()[npc] = was;          // (Nothing written: as it was.)
        return {false, "The binding could not be written: " + problem, npc};
    }
    wearBinding(npc);
    return {true, e->name + " wears " + mantle + " now.", npc};
}

Result Game::releaseMantle(const std::string& npc)
{
    auto& society = world_.society();
    const auto* b = society.mantleOf(npc);
    if (!b || b->mantle.empty() || b->releasedDay >= 0)
        return {false, npc + " wears no mantle.", npc};
    const auto mantle = b->mantle;
    if (mantleRows())
    {
        const auto done = worldDb_.exec("UPDATE live.npc_voice SET released_day = $3::double precision WHERE world_id = $1 AND npc_id = $2",
                                        {liveWorldId_, npc, std::to_string(world_.calendarDays())});
        if (!done.ok)
            return {false, "The release could not be written: " + done.error, npc};
    }
    society.releaseMantle(npc, world_.calendarDays());
    mantleOf_.erase(npc);
    talkDaily_.erase(npc);
    favourites_.erase(npc);
    return {true, mantle + " is released: " + npc + " speaks from the library; it rests from today.", npc};
}
} // namespace ratw::game
