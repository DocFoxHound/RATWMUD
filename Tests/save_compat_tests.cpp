// Saves from before doc 79 (Docs/Design/79-a-staged-economy.md, section 16 item 7; Tests/fixtures/pre79/README.md):
// frozen copies of real saves, each loaded by today's code. They load; players' purses and escrow come through to the
// penny (the totals are the README's); and the state round-trips through wire. This is the baseline every later
// phase of doc 79 keeps green: what it may change is how staged accounts are settled, never a player's coin.
//
//   save_compat_tests                     the checks (ctest: save_compat)
//   save_compat_tests --totals            prints each fixture's totals as the README records them
//   save_compat_tests --make-tail DIR     writes fixture (d) afresh into DIR (only for a new baseline: the fixture in
//                                         the repo was written once, by the code at cab65ee, and is frozen)
//   save_compat_tests --make-road DIR     the same for fixture (f), the road (and its save for fixture g's strip)
//   save_compat_tests --write-loaded DIR  each fixture's loaded state as a checkpoint document, for Postgres
//   save_compat_tests --check-loaded DIR  those documents as Postgres gave them back: the same state (save_compat_pg)
//
// The real worlds' fixtures (DEV, dev-old, the package) keep each world's manifest (world.ratw) and its cells'
// headers, not the cells' tiles (568 MB for DEV), so the society is restored against the world's authored roster
// exactly as World::restore does it (Society::restore), without the tile checks on homes and positions. With
// RATW_PRE79_BUILDS=DIR, where DIR/dev and DIR/dev-old hold the builds as `tools/world_build.py export` writes them,
// the whole save is also loaded by a game server (Game::start, the file store) on the full world.
#include "RatwCheckpoint.h"
#include "RatwGame.h"
#include "RatwJournal.h"
#include "RatwWire.h"
#include "RatwWorld.h"

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <unistd.h>
#include <vector>

using namespace ratw;
namespace fs = std::filesystem;
namespace
{
int checks = 0;
void expect(bool condition, const std::string& message)
{
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}

#include "town_fixture.h"

#ifdef RATW_SOURCE_DIR
const fs::path Fixtures = fs::path(RATW_SOURCE_DIR) / "Tests" / "fixtures" / "pre79";
#else
const fs::path Fixtures = fs::path("Tests") / "fixtures" / "pre79";
#endif

std::string slurp(const fs::path& path)
{
    std::ifstream in(path, std::ios::binary);
    std::ostringstream text;
    text << in.rdbuf();
    return text.str();
}

// A gzip file's text (gzip -dc, as world_check writes its tables), so the fixtures stay plain .gz files.
std::string gunzip(const fs::path& path)
{
    if (!fs::exists(path))
        throw std::runtime_error("missing fixture " + path.string());
    std::FILE* pipe = ::popen(("gzip -dc '" + path.string() + "'").c_str(), "r");
    if (!pipe)
        throw std::runtime_error("can't run gzip for " + path.string());
    std::string out;
    char buffer[65536];
    for (std::size_t n; (n = std::fread(buffer, 1, sizeof buffer, pipe)) > 0;)
        out.append(buffer, n);
    if (::pclose(pipe) != 0)
        throw std::runtime_error("gzip failed on " + path.string());
    return out;
}

void gzipTo(const fs::path& path, const std::string& text)
{
    std::FILE* pipe = ::popen(("gzip -n -9 -c > '" + path.string() + "'").c_str(), "w");
    if (!pipe || std::fwrite(text.data(), 1, text.size(), pipe) != text.size() || ::pclose(pipe) != 0)
        throw std::runtime_error("can't write " + path.string());
}

json::Value parse(const std::string& text, const std::string& what)
{
    json::Value v;
    std::string error;
    if (!json::parse(text, v, error))
        throw std::runtime_error(what + " is not JSON: " + error);
    return v;
}

// --------------------------------------------------------------------------- What must survive to the penny

// Doc 79, 9.1: player purses are real; these prefixes are escrow (coins held for players). `chapter:` is a Chapter's
// treasury, real player money, counted with the escrow here.
bool escrowAccount(const std::string& id)
{
    for (const char* prefix : {"contract:", "letter:", "let:", "stall:", "table:", "fest:", "project:", "ground:", "bandits:", "chapter:"})
        if (id.rfind(prefix, 0) == 0)
            return true;
    return false;
}

struct Totals
{
    int playerAccounts = 0, escrowAccounts = 0;
    std::int64_t playerCash = 0, playerGoods = 0, escrowCash = 0, escrowGoods = 0;
    bool operator==(const Totals& o) const
    {
        return playerAccounts == o.playerAccounts && escrowAccounts == o.escrowAccounts && playerCash == o.playerCash &&
               playerGoods == o.playerGoods && escrowCash == o.escrowCash && escrowGoods == o.escrowGoods;
    }
    std::string text() const
    {
        std::ostringstream o;
        o << playerAccounts << " player accounts holding " << playerCash << "p and " << playerGoods << " goods; " << escrowAccounts
          << " escrow accounts holding " << escrowCash << "p and " << escrowGoods << " goods";
        return o.str();
    }
};

Totals totalsOf(const SocietyState& s)
{
    Totals t;
    for (const auto& [id, a] : s.accounts)
    {
        std::int64_t goods = 0;
        for (const auto& [item, n] : a.stock)
            goods += n;
        if (playerAccountId(id))
        {
            ++t.playerAccounts;
            t.playerCash += a.cash;
            t.playerGoods += goods;
        }
        else if (escrowAccount(id))
        {
            ++t.escrowAccounts;
            t.escrowCash += a.cash;
            t.escrowGoods += goods;
        }
    }
    return t;
}

// Every player and escrow account of `before` is in `after` exactly (cash and every good), and no other appeared.
void realAccountsExact(const SocietyState& before, const SocietyState& after, const std::string& name)
{
    for (const auto& [id, a] : before.accounts)
        if (playerAccountId(id) || escrowAccount(id))
        {
            const auto it = after.accounts.find(id);
            expect(it != after.accounts.end(), name + ": the account " + id + " survives");
            expect(it->second.cash == a.cash, name + ": " + id + " keeps " + std::to_string(a.cash) + "p, not " + std::to_string(it->second.cash));
            expect(it->second.stock == a.stock, name + ": " + id + " keeps its goods");
        }
    for (const auto& [id, a] : after.accounts)
        if ((playerAccountId(id) || escrowAccount(id)) && !before.accounts.count(id))
            expect(false, name + ": no player or escrow account appears from nowhere (" + id + ")");
}

// The state through wire and back is the same state (society alone, then the whole checkpoint document).
void roundTrips(const PersistedWorld& saved, const checkpoint::ServerState& server, const SocietyState& restored,
                const std::string& name)
{
    const auto once = json::dump(wire::society(restored));
    const auto back = wire::readSociety(parse(once, name + "'s society"));
    expect(json::dump(wire::society(back)) == once, name + ": the society round-trips through wire");
    realAccountsExact(restored, back, name + " (through wire)");
    expect(back.minted == restored.minted && back.sunk == restored.sunk && back.budgetDay == restored.budgetDay &&
               back.nextEntry == restored.nextEntry && back.ledger.size() == restored.ledger.size() &&
               back.residents.size() == restored.residents.size() && back.accounts.size() == restored.accounts.size(),
           name + ": its counters, ledger, residents and accounts come back");

    PersistedWorld world = saved;
    world.society = restored;
    const auto document = json::dump(checkpoint::encode(world, server, world.npcs, world.time));
    PersistedWorld again;
    checkpoint::ServerState serverAgain;
    std::string problem;
    expect(checkpoint::decode(parse(document, name + " re-encoded"), again, serverAgain, problem),
           name + ": the re-encoded checkpoint decodes: " + problem);
    expect(again.hasSociety && json::dump(wire::society(again.society)) == once, name + ": and its society is the same");
    expect(json::dump(checkpoint::encode(again, serverAgain, again.npcs, again.time)) == document,
           name + ": the whole checkpoint is a fixed point of encode and decode");
}

// --------------------------------------------------------------------------- A real world's fixture

// The world's cells as the fixture keeps them: each cell's header (heights left out), and in place of its tiles open
// ground of the header's size (the real tiles are 568 MB for DEV), with a door's tile ('+', or '^' for steps) under
// each of the manifest's doors. So the manifest loads with its authored roster, and where the save stands is checked
// against the right cells and sizes, not against walls.
using Stamps = std::map<std::string, std::vector<std::tuple<int, int, char>>>;
Stamps doorTiles(const std::string& manifest)
{
    // door "id" "name" "cell" x y "target" x y "linked" open locked boundary passage "edge" (RatwAuthoring.cpp, parseDoor)
    Stamps out;
    std::istringstream lines(manifest);
    for (std::string line; std::getline(lines, line);)
    {
        if (line.rfind("door \"", 0) != 0)
            continue;
        std::vector<std::string> tokens;
        for (std::size_t at = 0; at < line.size();)
        {
            if (line[at] == ' ')
                ++at;
            else if (line[at] == '"')
            {
                const auto close = line.find('"', at + 1);
                tokens.push_back(line.substr(at + 1, close - at - 1));
                at = close == std::string::npos ? line.size() : close + 1;
            }
            else
            {
                const auto space = line.find(' ', at);
                tokens.push_back(line.substr(at, space - at));
                at = space == std::string::npos ? line.size() : space;
            }
        }
        if (tokens.size() != 15)
            continue;
        const bool stairs = tokens[1].rfind("stairs_", 0) == 0, passage = tokens[13] == "1";
        if (passage && !stairs)
            continue;                               // (A passage's tile may be any open ground.)
        out[tokens[3]].emplace_back(int(std::atof(tokens[4].c_str())), int(std::atof(tokens[5].c_str())), stairs ? '^' : '+');
    }
    return out;
}

World::CellSource cellsOf(const fs::path& file, const std::string& manifest)
{
    auto stamps = std::make_shared<Stamps>(doorTiles(manifest));
    const auto headers = parse(gunzip(file), file.string());
    auto heads = std::make_shared<std::map<std::string, std::string>>();
    for (const auto& [id, header] : headers.fields())
        (*heads)[id] = header.asString();
    World::CellSource source;
    source.header = [heads](const std::string& id, std::string& header) {
        const auto it = heads->find(id);
        if (it == heads->end())
            return std::string("no header for ") + id;
        header = it->second;
        return std::string();
    };
    source.load = [heads, stamps](const std::string& id, std::string& cell, std::string& seams) {
        const auto it = heads->find(id);
        if (it == heads->end())
            return std::string("no header for ") + id;
        const auto& header = it->second;
        const auto size = header.find("\nsize: "), grid = header.rfind("grid:");
        int w = 0, h = 0;
        if (size == std::string::npos || grid == std::string::npos ||
            std::sscanf(header.c_str() + size + 7, "%d %d", &w, &h) != 2 || w < 1 || h < 1 || w > 4096 || h > 4096)
            return std::string("a header without its size: ") + id;
        cell = header.substr(0, grid) + "grid:\n";
        std::vector<std::string> rows(std::size_t(h), std::string(std::size_t(w), '.'));
        if (const auto it = stamps->find(id); it != stamps->end())
            for (const auto& [x, y, glyph] : it->second)
                if (x >= 0 && y >= 0 && x < w && y < h)
                    rows[std::size_t(y)][std::size_t(x)] = glyph;
        for (const auto& row : rows)
            cell += row + "\n";
        seams.clear();
        return std::string();
    };
    return source;
}

World worldOf(const fs::path& dir, const std::string& manifest = {})
{
    const auto text = manifest.empty() ? gunzip(dir / "world.ratw.gz") : manifest;
    World world;
    world.setCellSource(cellsOf(dir / "cells.json.gz", text));
    const auto loaded = world.loadWorldFiles({{"world.ratw", text}}, dir.string());
    expect(loaded.ok, dir.string() + ": the world's manifest loads: " + loaded.message);
    return world;
}

struct Loaded
{
    PersistedWorld saved;
    checkpoint::ServerState server;
    SocietyState restored;
};

// A checkpoint document (the journal after it already replayed, as world_package.py reads one), decoded and its
// society restored against `world`'s authored roster, as World::restore does.
Loaded loadSave(const World& world, const json::Value& document, const std::string& name)
{
    Loaded out;
    std::string problem;
    expect(checkpoint::decode(document, out.saved, out.server, problem), name + ": the checkpoint decodes: " + problem);
    expect(out.saved.hasSociety, name + ": it has a society");
    Society society = world.society();
    society.reset(world.society().roster());
    expect(society.restore(out.saved.society), name + ": its society restores against the world's authored roster");
    out.restored = society.state();
    realAccountsExact(out.saved.society, out.restored, name);
    expect(society.conserved(), name + ": the restored society's money adds up");
    return out;
}

struct Expected
{
    const char* name;
    const char* world;          // The world fixture whose roster it is restored against.
    const char* save;           // The checkpoint document, relative to the fixtures.
    Totals totals;
};

// The README's totals (Tests/fixtures/pre79/README.md, "Totals").
const std::vector<Expected>& expected()
{
    static const std::vector<Expected> e = {
        {"dev", "dev", "dev/checkpoint.json.gz", {0, 0, 0, 0, 0, 0}},
        {"dev-old", "dev-old", "dev-old/checkpoint.json.gz", {1, 86, 20, 3, 1771, 0}},
        {"package", "dev", "package/snapshot.json.gz", {0, 0, 0, 0, 0, 0}},
    };
    return e;
}

Totals checkWorldSave(const Expected& e, bool print)
{
    const auto world = worldOf(Fixtures / e.world);
    const auto document = parse(gunzip(Fixtures / e.save), e.save);
    const auto loaded = loadSave(world, document, e.name);
    const auto totals = totalsOf(loaded.restored);
    expect(totalsOf(loaded.saved.society) == totals, std::string(e.name) + ": the totals before and after the restore agree");
    if (!print)
        expect(totals == e.totals, std::string(e.name) + ": " + totals.text() + "; the README says " + e.totals.text());
    roundTrips(loaded.saved, loaded.server, loaded.restored, e.name);
    {
        // The staged day's turn's one pass over the classes (Society::householdClasses, doc 79 Phase 0) agrees with
        // householdClass one household at a time on each real world's people.
        Society society = world.society();
        society.reset(world.society().roster());
        expect(society.restore(loaded.saved.society), std::string(e.name) + ": restores for the classes");
        const auto homes = society.households(true);
        const auto classes = society.householdClasses(homes);
        for (const auto& [home, ids] : homes)
            expect(classes.at(home) == (home.rfind("|", 0) == 0 ? std::string("homeless") : society.householdClass(ids)),
                   std::string(e.name) + ": the one pass's class for " + home + " as householdClass's");
    }
    // And the whole save into the world (World::restore: weather, doors, memories, bonds, every NPC and player), on
    // the open-ground cells: where everyone stands checked against the right cells and sizes.
    auto whole = worldOf(Fixtures / e.world);
    const auto restored = whole.restore(loaded.saved);
    expect(restored.ok, std::string(e.name) + ": the whole save restores into the world: " + restored.message + " " + restored.targetId);
    realAccountsExact(loaded.saved.society, whole.society().state(), std::string(e.name) + " (World::restore)");
    return totals;
}

// The package (fixture c): the source's save with every player poured into the treasury, so none is left, and the
// money still adds up as the scan checks.
void thePackage()
{
    const auto document = parse(gunzip(Fixtures / "package" / "snapshot.json.gz"), "the package's snapshot");
    PersistedWorld saved;
    checkpoint::ServerState server;
    std::string problem;
    expect(checkpoint::decode(document, saved, server, problem), "the package's snapshot decodes: " + problem);
    expect(saved.players.empty(), "a package holds no player");
    for (const auto& [id, a] : saved.society.accounts)
        expect(!playerAccountId(id), "and no player's account: " + id);
    const auto manifest = parse(slurp(Fixtures / "package" / "package.json"), "package.json");
    expect(manifest.number("format") == 1 && manifest.boolean("snapshot"), "the package's manifest is format 1 with a snapshot");
}

// --------------------------------------------------------------------------- Fixture (d): a journal tail

// Fixture (d): a three-town strip (town_fixture.h's, frozen as files in the fixture), saved by today's server in file
// mode, then a journal written after that save by players' trades, a letter with coins and a gift. Loading replays
// the journal over the save.
const fs::path TailDir = Fixtures / "tail";

std::string unpackTail(const fs::path& to)
{
    fs::remove_all(to);
    fs::create_directories(to / "world");
    const auto files = parse(gunzip(TailDir / "world.json.gz"), "the strip's files");
    for (const auto& [path, text] : files.fields())
    {
        const auto at = to / "world" / path;
        fs::create_directories(at.parent_path());
        std::ofstream(at, std::ios::binary) << text.asString();
    }
    std::ofstream(to / "save.json", std::ios::binary) << gunzip(TailDir / "save.json.gz");
    std::ofstream(to / "save.json.journal", std::ios::binary) << gunzip(TailDir / "save.json.journal.gz");
    return (to / "save.json").string();
}

Totals tailExpected() { return {2, 1, 125, 11, 5, 0}; }

Totals checkTail(bool print)
{
    const auto dir = fs::temp_directory_path() / ("ratw-pre79-tail-" + std::to_string(::getpid()));
    const auto save = unpackTail(dir);
    // First as the package tool and the server read it: the document, the journal after it replayed, decoded.
    auto document = parse(slurp(save), "the tail's save");
    std::vector<journal::Record> records;
    std::string problem;
    expect(journal::readFile(save + ".journal", 0, records, problem), "the journal reads: " + problem);
    const auto after = std::uint64_t(document.number("journal"));
    std::size_t newer = 0;
    for (const auto& r : records)
        newer += r.seq > after;
    expect(newer >= 3, "the journal holds records written after the save (" + std::to_string(newer) + ")");
    std::uint64_t last = 0;
    expect(journal::replay(document, records, after, last, problem), "and replays over it: " + problem);
    bool setsSociety = false;
    for (const auto& r : records)
        setsSociety = setsSociety || (r.seq > after && r.text.find("\"minted\"") != std::string::npos &&
                                      r.text.find("\"budgetDay\"") != std::string::npos);
    expect(setsSociety, "a record sets the society's counters (minted, budgetDay and the rest)");
    PersistedWorld saved;
    checkpoint::ServerState server;
    expect(checkpoint::decode(document, saved, server, problem), "the replayed save decodes: " + problem);
    const auto totals = totalsOf(saved.society);
    if (!print)
        expect(totals == tailExpected(), "the tail: " + totals.text() + "; the README says " + tailExpected().text());

    // Then whole, by a game server starting from the file (Game::load: the journal replayed, decoded, restored).
    {
        game::Options o;
        o.hiddenNames = true;
        o.devIdentity = true;
        o.forkSnapshots = false;
        o.tiesOptional = true;
        o.worldExport = (dir / "world").string();
        o.savePath = save;
        game::Game g(o);
        std::string notes;
        g.log = [&](const char* level, const std::string& text) { notes += std::string(level) + ": " + text + "\n"; };
        expect(g.start(problem), "a server starts from the tail's save: " + problem);
        expect(g.storageReady(), "and takes it (nothing rejected): " + notes);
        const auto& society = g.world().society();
        // A world from files keeps no letters across a restart (they live in game.documents), so the letter's coins
        // go back to its writer (Game::orphanedEnclosures): the same pennies, in the writer's purse.
        auto expectedState = saved.society;
        for (auto& [id, a] : expectedState.accounts)
            if (id.rfind("letter:", 0) == 0 && id.find('@') != std::string::npos && a.cash > 0)
            {
                expectedState.accounts.at(id.substr(id.find('@') + 1)).cash += a.cash;
                a.cash = 0;
            }
        realAccountsExact(expectedState, society.state(), "the tail, loaded by a server");
        const auto loaded = totalsOf(society.state());
        expect(loaded.playerCash + loaded.escrowCash == totals.playerCash + totals.escrowCash &&
                   loaded.playerGoods + loaded.escrowGoods == totals.playerGoods + totals.escrowGoods,
               "the server holds the replayed save's player and escrow money to the penny: " + loaded.text());
        expect(society.conserved(), "its money adds up");
        expect(g.characters().size() == 2, "both characters are there");
        roundTrips(saved, server, society.state(), "the tail");
    }
    fs::remove_all(dir);
    return totals;
}

// Writes fixture (d) into `out` with today's code. Run once for a baseline; the fixture in the repo is frozen.
int makeTail(const fs::path& out)
{
    const auto work = fs::temp_directory_path() / ("ratw-pre79-make-" + std::to_string(::getpid()));
    fs::remove_all(work);
    fs::create_directories(work);
    const auto world = writeStrip(work / "world");
    const auto save = (work / "save.json").string();
    std::vector<journal::Record> written;
    std::string savedText;
    {
        Town t(world, save);
        t.enter(t.ash, 1, "ash", "Ash", 1, 12);
        t.enter(t.bo, 2, "bo", "Bo", 1, 13);
        t.g.world().setTimeOfDay(12);
        t.run(30);                                  // (The keepers at their posts.)
        auto& society = t.g.world().society();
        for (const auto* who : {"ui", "um"})
        {
            society.create(society.tillOf(who), "meal", 6, "stock for the fixture");
            society.create(society.tillOf(who), "bread", 6, "stock for the fixture");
        }
        t.g.save();                                 // The checkpoint the journal follows.
        savedText = slurp(save);
        auto* keeper = t.g.world().entity("ui");
        t.place(t.ash, 1, keeper->position.x + 1, keeper->position.y);
        t.g.command(&t.ash, cmd({{"type", "trade"}, {"target", "ui"}, {"item", "meal"}, {"quantity", 2}, {"buy", true}}));
        t.g.settle();
        t.g.command(&t.ash, cmd({{"type", "trade"}, {"target", "ui"}, {"item", "meal"}, {"quantity", 1}, {"buy", false}}));
        t.g.settle();
        // A letter with coins in it: escrow ("letter:..."), waiting for its courier. (Bo says his name first.)
        t.place(t.bo, 1, keeper->position.x + 1, keeper->position.y + 1);
        t.g.command(&t.bo, cmd({{"type", "chat"}, {"text", "\"I'm Bo.\""}, {"commandId", "b1"}}));
        t.run(1);
        auto o = json::Value::object();
        o.add("type", "letter");
        o.add("verb", "write");
        o.add("to", "Bo");
        o.add("text", "A few pennies for the road.");
        auto enclose = json::Value::object();
        enclose.add("coins", 5);
        o.add("enclose", enclose);
        t.g.command(&t.ash, json::dump(o));
        t.g.settle();
        // A gift of coins between players, accepted.
        t.g.command(&t.bo, cmd({{"type", "give"}, {"target", t.ash.entityId}, {"item", ""}, {"quantity", 0}, {"coins", 3}}));
        t.g.settle();
        t.g.command(&t.ash, cmd({{"type", "giveAnswer"}, {"accept", true}}));
        t.g.settle();
        std::string problem;
        expect(journal::readFile(save + ".journal", 0, written, problem), "the journal reads back: " + problem);
        expect(slurp(save) == savedText, "no save was taken after the commands (the journal is the tail)");
        std::cout << "said to Ash:\n" << t.ash.said() << "said to Bo:\n" << t.bo.said();
        const auto totals = totalsOf(society.state());
        std::cout << "live totals: " << totals.text() << "\n";
    }
    fs::create_directories(out);
    auto files = json::Value::object();
    for (const auto& entry : fs::recursive_directory_iterator(work / "world"))
        if (entry.is_regular_file())
            files.add(fs::relative(entry.path(), work / "world").generic_string(), slurp(entry.path()));
    gzipTo(out / "world.json.gz", json::dump(files));
    gzipTo(out / "save.json.gz", savedText);
    std::string journalText;
    for (const auto& r : written)
        journalText += std::to_string(r.seq) + "\t" + r.text + "\n";
    gzipTo(out / "save.json.journal.gz", journalText);
    std::cout << written.size() << " journal records after the save (journal " << parse(savedText, "save").number("journal")
              << ") written to " << out << "\n";
    fs::remove_all(work);
    return 0;
}

// --------------------------------------------------------------------------- Fixtures (f) and (g): the road and its package

// Fixture (f): roads_tests' strip (the capital "east", wild country, the town "west"), frozen as files, and a save of
// it written by the code at cab65ee (World::save, checkpoint::encode) after the city's loop ran past 18:00 and a night,
// with what the real fixtures lack (doc 79, 16.7): a `bandits:` hoard holding a player's coins, a caravan out with a
// player's letter on it and an escort a player took. Synthetic: no real world has them (README, "(f)").
const fs::path RoadDir = Fixtures / "road";
constexpr int RoadSide = 16;

std::string roadCell(int i) { return "c_" + std::to_string(i) + "_0"; }

std::string roadResident(const std::string& who, const std::string& name, const std::string& role, const std::string& work,
                         int home, double hx, int at, double wx)
{
    std::ostringstream r;
    r << "resident \"" << who << "\" \"" << name << "\" \"" << role << "\" \"" << work << "\" \"Someone.\" \"Hello.\" 30 \"timber\" "
      << "\"female\" \"average\" \"saddle\" 3 1 5 1 1 8 17 \"-\" 40 0 1 \"" << roadCell(home) << "\" " << hx << " 4.5 \""
      << roadCell(at) << "\" " << wx << " 8.5 \"" << roadCell(home) << "\" " << hx << " 5.5\n";
    return r.str();
}

// The strip's files as the fixture keeps them: {manifest, cells: {id: text}, seams: {id: doors}} (roads_tests' strip()).
json::Value roadFiles()
{
    const std::string layout = "EE.....WW";
    auto cells = json::Value::object(), seams = json::Value::object();
    std::map<std::string, std::string> sides;
    std::ostringstream m;
    m << "RATW_WORLD 3\n";
    int seam = 0;
    const int n = int(layout.size());
    for (int i = 0; i < n; ++i)
    {
        std::ostringstream cell;
        cell << "id: " << roadCell(i) << "\nname: Stretch " << i << "\ndescription: Open ground.\nworld: " << i * RoadSide
             << " 0 0\noutdoors: true\nweather: clear\nsize: " << RoadSide << ' ' << RoadSide << "\ngrid:\n";
        for (int y = 0; y < RoadSide; ++y)
            cell << std::string(RoadSide, '.') << '\n';
        cells.add(roadCell(i), cell.str());
        m << "area \"" << roadCell(i) << "\"\n";
        if (i + 1 < n)
            for (int k = 0; k < RoadSide; ++k)
            {
                const std::string a = "seam_" + std::to_string(seam) + "_a", b = "seam_" + std::to_string(seam) + "_b";
                std::ostringstream one, two;
                one << "door \"" << a << "\" \"Open boundary\" \"" << roadCell(i) << "\" " << RoadSide - .5 << ' ' << k + .5 << " \""
                    << roadCell(i + 1) << "\" .5 " << k + .5 << " \"" << b << "\" 1 0 1 1 \"E\"\n";
                two << "door \"" << b << "\" \"Open boundary\" \"" << roadCell(i + 1) << "\" .5 " << k + .5 << " \"" << roadCell(i)
                    << "\" " << RoadSide - .5 << ' ' << k + .5 << " \"" << a << "\" 1 0 1 1 \"W\"\n";
                sides[roadCell(i)] += one.str();
                sides[roadCell(i + 1)] += two.str();
                ++seam;
            }
    }
    for (int i = 0; i < n; ++i)
    {
        m << "exits \"" << roadCell(i) << "\" " << ((i > 0) + (i + 1 < n));
        if (i > 0)
            m << " \"" << roadCell(i - 1) << '"';
        if (i + 1 < n)
            m << " \"" << roadCell(i + 1) << '"';
        m << '\n';
        const char c = layout[std::size_t(i)];
        m << "territory \"" << roadCell(i) << "\" \"" << (c == 'E' ? "east" : c == 'W' ? "west" : "wilds") << "\" \"-\" 0\n";
    }
    m << "spawn \"" << roadCell(0) << "\" 8.5 8.5\n";
    m << "economy 20000 100 50 10 12\n";
    // The steering and its floors on (doc 74/75), so the save holds the steering's state and fund:relief.
    m << "rule \"steer\" 1\nrule \"neverHungry\" 1\nrule \"neverBroke\" 1\n";
    for (const auto& [c, who, name] : {std::tuple<char, const char*, const char*>{'E', "em", "Ember Oak"}, {'W', "wm", "Wren Reed"}})
    {
        const int home = int(layout.find(c));
        const std::string tag = c == 'E' ? "e" : "w";
        m << roadResident(who, name, "merchant", "keeping the stall", home, 2.5, home + 1, 8.5);
        for (int k = 1; k <= 5; ++k)
            m << roadResident(tag + std::to_string(k), std::string(c == 'E' ? "East " : "West ") + std::to_string(k), "civilian",
                              "working", home, 3.5 + k, home, 3.5 + k);
    }
    for (const auto& [id, text] : sides)
        seams.add(id, text);
    auto files = json::Value::object();
    files.add("manifest", m.str());
    files.add("cells", cells);
    files.add("seams", seams);
    return files;
}

World roadWorld(const json::Value& files)
{
    auto cells = std::make_shared<std::map<std::string, std::string>>(), seams = std::make_shared<std::map<std::string, std::string>>();
    for (const auto& [id, text] : files["cells"].fields())
        (*cells)[id] = text.asString();
    for (const auto& [id, text] : files["seams"].fields())
        (*seams)[id] = text.asString();
    World::CellSource source;
    source.header = [cells](const std::string& id, std::string& header) {
        const auto it = cells->find(id);
        if (it == cells->end())
            return std::string("no such cell");
        header = it->second.substr(0, it->second.find("grid:") + 5);
        return std::string();
    };
    source.load = [cells, seams](const std::string& id, std::string& text, std::string& sides) {
        const auto it = cells->find(id);
        if (it == cells->end())
            return std::string("no such cell");
        text = it->second;
        const auto s = seams->find(id);
        sides = s == seams->end() ? std::string() : s->second;
        return std::string();
    };
    World world;
    world.setCellSource(source);
    const auto loaded = world.loadWorldFiles({{"world.ratw", files["manifest"].asString()}}, "road");
    expect(loaded.ok, "the road's strip loads: " + loaded.message);
    return world;
}

void roadTicks(World& w, double seconds)
{
    for (double t = 0; t < seconds; t += 1)
        w.tick(1);
}

void roadMorning(World& w)
{
    auto saved = w.save();
    saved.calendarDays = std::floor(saved.calendarDays) + 1.3;     // (roads_tests' nextMorning.)
    expect(w.restore(saved).ok, "a day passes");
    w.tick(.6);
}

// Writes fixture (f) into `out` with today's code (once, for a baseline: the fixture in the repo is frozen), and the
// save alone as `save.json` beside it for world_package.py's strip (fixture g, README).
int makeRoad(const fs::path& out)
{
    const auto files = roadFiles();
    auto w = roadWorld(files);
    w.addPlayer("player-ada", "Ada");
    w.addPlayer("player-bo", "Bo");
    w.tick(.6);
    w.roads().camps.clear();
    w.roads().caravans.clear();
    // The city's loop past its evening and a night (its 18:00 money steps and the day's turn).
    expect(w.setTimeOfDay(17.9).ok, "to the evening");
    roadTicks(w, 600);
    expect(w.setTimeOfDay(23.9).ok, "to the night");
    roadTicks(w, 600);
    // Bo, in the capital, leaves a letter with 4p for Wren in the west (escrow from a player's purse, contract:...).
    auto* bo = w.entity("player-bo");
    bo->cellId = roadCell(1);
    bo->position = {8.5, 12.5};
    const std::string letter = w.postContract("courier", "player-bo", "west", "wm", 4, 30, "a letter from Bo to Wren").id;
    // Nobody takes it for a week: it goes with the next caravan's carters.
    for (int d = 0; d < 7; ++d)
        roadMorning(w);
    // Ada is stopped by the bandits in the wild and pays them: their hoard holds her coins (bandits:camp_mid).
    w.roads().camps = {{"camp_mid", roadCell(4), 9, 60, -100, true}};
    auto* ada = w.entity("player-ada");
    ada->cellId = roadCell(3);
    ada->position = {8.5, 8.5};
    roadTicks(w, 2);
    ada->cellId = roadCell(4);
    ada->position = {w.roads().camps[0].x - 3, w.roads().camps[0].y};
    roadTicks(w, 2);
    const auto demand = w.banditDemand("player-ada");
    expect(demand > 0, "the bandits stop Ada");
    std::string bandit;
    for (const auto& [eid, e] : w.entities())
        if (eid.rfind("road:camp_mid:", 0) == 0 && !e.dead)
            bandit = eid;
    expect(!bandit.empty() && w.payBandits("player-ada", bandit).ok, "and she pays them");
    expect(w.society().account("bandits:camp_mid") && w.society().account("bandits:camp_mid")->cash == demand,
           "their hoard holds her coins");
    // In the west she takes the escort for the next caravan, then goes to the capital's market to meet it.
    ada->cellId = roadCell(8);
    ada->position = {8.5, 8.5};
    roadTicks(w, 2);
    const std::string escort = w.postContract("escort", "treasury", "west", "west", 12, 7, "guarding the next caravan to west").id;
    expect(w.takeContract("player-ada", escort).ok, "Ada takes the escort");
    ada->cellId = roadCell(1);
    ada->position = {8.5, 10.5};
    const auto* sent = w.sendCaravan(w.towns().front(), w.towns().back(), {{"meal", 6}});
    expect(sent, "a caravan sets out");
    const auto trip = sent->id;
    roadTicks(w, 2);
    const Caravan* out_ = nullptr;
    for (const auto& c : w.roads().caravans)
        if (c.id == trip)
            out_ = &c;
    expect(out_ && out_->escorts == std::vector<std::string>{"player-ada"}, "it counts Ada among its escorts");
    expect(out_->letters == std::vector<std::string>{letter}, "and carries Bo's letter");
    expect(w.society().conserved(), "the money adds up");
    const auto saved = w.save();
    const auto document = checkpoint::encode(saved, {}, saved.npcs, saved.time);
    fs::create_directories(out);
    gzipTo(out / "world.json.gz", json::dump(files));
    gzipTo(out / "save.json.gz", json::dump(document));
    std::ofstream(out / "save.json", std::ios::binary) << json::dump(document);
    std::cout << "the road: " << totalsOf(saved.society).text() << "; accounts:";
    for (const auto& [id, a] : saved.society.accounts)
        if (id.find(':') != std::string::npos || playerAccountId(id))
            std::cout << ' ' << id << '=' << a.cash;
    std::cout << "\nwritten to " << out << "\n";
    return 0;
}

Totals roadExpected() { return {2, 3, 16, 10, 36, 0}; }   // (README, "Totals".)

// Every account prefix the road's save holds (before the colon).
std::set<std::string> prefixesOf(const SocietyState& s)
{
    std::set<std::string> out;
    for (const auto& [id, a] : s.accounts)
        if (const auto colon = id.find(':'); colon != std::string::npos)
            out.insert(id.substr(0, colon + 1));
    return out;
}

Totals checkRoad(bool print)
{
    const auto files = parse(gunzip(RoadDir / "world.json.gz"), "the road's files");
    const auto document = parse(gunzip(RoadDir / "save.json.gz"), "the road's save");
    const auto world = roadWorld(files);
    const auto loaded = loadSave(world, document, "the road");
    const auto totals = totalsOf(loaded.restored);
    if (!print)
    {
        expect(totals == roadExpected(), "the road: " + totals.text() + "; the README says " + roadExpected().text());
        const auto& accounts = loaded.restored.accounts;
        const auto hoard = accounts.find("bandits:camp_mid");
        expect(hoard != accounts.end() && hoard->second.cash > 0, "a bandits: hoard holding a player's coins");
        expect(loaded.saved.roads.caravans.size() == 1 && loaded.saved.roads.caravans[0].escorts == std::vector<std::string>{"player-ada"} &&
                   loaded.saved.roads.caravans[0].letters.size() == 1,
               "a caravan out with a player as its escort and a player's letter");
        int playerContracts = 0;
        for (const auto& k : loaded.saved.roads.contracts)
            playerContracts += (k.kind == "courier" && k.poster == "player-bo" && k.status == "taken") ||
                               (k.kind == "escort" && k.taker == "player-ada" && k.status == "taken");
        expect(playerContracts == 2, "Bo's letter and Ada's escort, both taken");
        // The loop's and the steering's state after an evening's pay and a night, and the dead prefixes' accounts
        // (doc 79, 9.1), for Phase 3's drops and read-past to be checked on (fund:relief and farm: aren't here: README).
        expect(document["society"]["loop"].isObject() && document["society"]["loop"]["steer"].isObject() &&
                   document["society"]["loop"]["wages"].isObject(),
               "the save holds the loop's state and the steering's");
        expect(!loaded.restored.loop.steer.empty() && loaded.restored.loop.steer.on, "and the steering's state restores, on");
        const auto prefixes = prefixesOf(loaded.restored);
        for (const char* p : {"outside:", "estate:", "stores:", "caravan:", "till:", "bandits:", "contract:"})
            expect(prefixes.count(p), std::string("an account with the prefix ") + p);
    }
    roundTrips(loaded.saved, loaded.server, loaded.restored, "the road");
    auto whole = roadWorld(files);
    const auto restored = whole.restore(loaded.saved);
    expect(restored.ok, "the road: the whole save restores into the world: " + restored.message + " " + restored.targetId);
    realAccountsExact(loaded.saved.society, whole.society().state(), "the road (World::restore)");
    return totals;
}

// Fixture (g): the road's save through world_package.py's strip at cab65ee (players poured into the treasury, doc 79
// 16.7): no player is left, their purses and their escrow are the treasury's now, and not a penny is lost.
void theRoadsPackage()
{
    const auto road = parse(gunzip(RoadDir / "save.json.gz"), "the road's save");
    const auto stripped = parse(gunzip(RoadDir / "package-snapshot.json.gz"), "the road's package snapshot");
    PersistedWorld before, after;
    checkpoint::ServerState s1, s2;
    std::string problem;
    expect(checkpoint::decode(road, before, s1, problem), "the road's save decodes: " + problem);
    expect(checkpoint::decode(stripped, after, s2, problem), "its package snapshot decodes: " + problem);
    int players = 0;
    for (const auto& [id, a] : before.society.accounts)
        players += playerAccountId(id);
    expect(players == 2 && after.players.empty(), "the road's save holds two players' purses, the package no player");
    std::int64_t poured = 0, cashBefore = 0, cashAfter = 0;
    for (const auto& [id, a] : before.society.accounts)
    {
        cashBefore += a.cash;
        if (playerAccountId(id) || (escrowAccount(id) && !after.society.accounts.count(id)))
            poured += a.cash;
    }
    for (const auto& [id, a] : after.society.accounts)
    {
        expect(!playerAccountId(id), "and no player's account: " + id);
        cashAfter += a.cash;
    }
    expect(poured > 0, "players' money to pour");
    expect(cashAfter == cashBefore, "not a penny lost: " + std::to_string(cashBefore) + "p to " + std::to_string(cashAfter) + "p");
    expect(after.society.accounts.at("treasury").cash >= before.society.accounts.at("treasury").cash + poured,
           "the players' purses and their escrow poured into the treasury");
    Society society = roadWorld(parse(gunzip(RoadDir / "world.json.gz"), "the road's files")).society();
    society.reset(society.roster());
    expect(society.restore(after.society) && society.conserved(), "the package's society restores, its money adding up");
}

// --------------------------------------------------------------------------- Through Postgres (doc 79, 16.7)

// Each fixture's loaded state (restored by today's code), as the checkpoint document a server would save of it.
std::vector<std::pair<std::string, std::string>> loadedDocuments()
{
    std::vector<std::pair<std::string, std::string>> out;
    const auto add = [&](const std::string& name, const Loaded& l) {
        PersistedWorld world = l.saved;
        world.society = l.restored;
        out.emplace_back(name, json::dump(checkpoint::encode(world, l.server, world.npcs, world.time)));
    };
    for (const auto& e : expected())
        add(e.name, loadSave(worldOf(Fixtures / e.world), parse(gunzip(Fixtures / e.save), e.save), e.name));
    add("road", loadSave(roadWorld(parse(gunzip(RoadDir / "world.json.gz"), "the road's files")),
                         parse(gunzip(RoadDir / "save.json.gz"), "the road's save"), "the road"));
    return out;
}

// --write-loaded DIR: writes DIR/<fixture>.json. Tests/save_compat_pg.py saves each through game.save_checkpoint into a
// throwaway database, loads it back with game.load_checkpoint into another directory, and calls --check-loaded.
int writeLoaded(const fs::path& dir)
{
    fs::create_directories(dir);
    for (const auto& [name, text] : loadedDocuments())
        std::ofstream(dir / (name + ".json"), std::ios::binary) << text;
    return 0;
}

// --check-loaded DIR: each document as Postgres gave it back decodes to the same state: the society the same through
// wire, every player and escrow account to the penny, and the whole checkpoint the same once re-encoded.
int checkLoaded(const fs::path& dir)
{
    int n = 0;
    for (const auto& [name, text] : loadedDocuments())
    {
        const auto file = dir / (name + ".json");
        expect(fs::exists(file), name + ": back from Postgres");
        PersistedWorld before, after;
        checkpoint::ServerState s1, s2;
        std::string problem;
        expect(checkpoint::decode(parse(text, name), before, s1, problem), name + ": decodes: " + problem);
        expect(checkpoint::decode(parse(slurp(file), name + " from Postgres"), after, s2, problem),
               name + ": decodes after Postgres: " + problem);
        realAccountsExact(before.society, after.society, name + " (through Postgres)");
        expect(json::dump(wire::society(after.society)) == json::dump(wire::society(before.society)),
               name + ": the society is the same through Postgres");
        expect(json::dump(checkpoint::encode(after, s2, after.npcs, after.time)) == text,
               name + ": the whole checkpoint is the same through Postgres");
        ++n;
    }
    std::cout << "save_compat_tests --check-loaded: " << n << " saves the same through Postgres (" << checks << " checks)\n";
    return 0;
}

// --------------------------------------------------------------------------- Fixture (e): Reset economy rules

// Fixture (e): doc 78's "Reset economy rules" copy of live.economy from before doc 79, holding one rule, steerLean.
// Today it becomes the world's rule lines as live.people_manifest() writes them (rule "<name>" N); the world loads with
// it and the DEV save restores over it.
void theResetCopy()
{
    const auto copy = parse(gunzip(Fixtures / "reset" / "economy.json.gz"), "the Reset economy rules copy");
    const auto& rules = copy["rules"];
    expect(rules.fields().size() == 1 && rules.has("steerLean"), "the copy holds one rule, steerLean");
    const int lean = int(rules.number("steerLean"));
    std::string manifest = gunzip(Fixtures / "dev" / "world.ratw.gz");
    expect(manifest.find("\nrule \"steerLean\"") == std::string::npos, "DEV's own manifest sets no steerLean");
    manifest += "rule \"steerLean\" " + std::to_string(lean) + "\n";
    const auto world = worldOf(Fixtures / "dev", manifest);
    expect(world.society().rule("steerLean") == lean, "and steerLean is the copy's " + std::to_string(lean));
    const auto document = parse(gunzip(Fixtures / "dev" / "checkpoint.json.gz"), "DEV's checkpoint");
    const auto save = loadSave(world, document, "dev under the Reset copy");
    expect(totalsOf(save.restored) == expected()[0].totals, "DEV's save restores over it, its totals unchanged");
}

// --------------------------------------------------------------------------- The whole save on the whole world

// With RATW_PRE79_BUILDS: a game server starts from each real save on its full world (every check World::restore
// makes, tiles and all), and the purses and escrow are the fixture's.
void onTheFullWorlds()
{
    const char* builds = std::getenv("RATW_PRE79_BUILDS");
    if (!builds || !*builds)
    {
        std::cout << "  (RATW_PRE79_BUILDS not set: the full-world loads skipped)\n";
        return;
    }
    for (const auto& e : expected())
    {
        const auto world = fs::path(builds) / e.world;
        if (!fs::exists(world / "world.ratw"))
        {
            std::cout << "  (no build at " << world << ": " << e.name << "'s full load skipped)\n";
            continue;
        }
        const auto dir = fs::temp_directory_path() / ("ratw-pre79-full-" + std::to_string(::getpid()));
        fs::remove_all(dir);
        fs::create_directories(dir);
        const auto save = (dir / "save.json").string();
        std::ofstream(save, std::ios::binary) << gunzip(Fixtures / e.save);
        {
            game::Options o;
            o.forkSnapshots = false;
            o.tiesOptional = true;
            o.worldExport = world.string();
            o.savePath = save;
            game::Game g(o);
            std::string problem, notes;
            g.log = [&](const char* level, const std::string& text) { notes += std::string(level) + ": " + text + "\n"; };
            expect(g.start(problem), std::string(e.name) + ": a server starts on the full world: " + problem);
            expect(g.storageReady(), std::string(e.name) + ": and takes the save (nothing rejected): " + notes);
            expect(totalsOf(g.world().society().state()) == e.totals, std::string(e.name) + ": the full world's totals are the README's: " +
                                                                          totalsOf(g.world().society().state()).text());
            const auto document = parse(gunzip(Fixtures / e.save), e.save);
            expect(std::abs(g.world().time() - document.number("time")) < 1 &&
                       g.world().society().state().budgetDay == std::int64_t(document["society"].number("budgetDay")),
                   std::string(e.name) + ": the world runs on from the save's clock and day, not from a fresh start");
            std::cout << "  " << e.name << ": loaded whole on " << world << "\n";
        }
        fs::remove_all(dir);
    }
}
// Doc 79, Phase 2's decision point (19): the bands of each fixture's households (the frozen copies of DEV and dev-old:
// read, never a world) at -100, 0 and +100, by class (save_compat_tests --census). A read, not a simulation.
int census()
{
    for (const auto& e : expected())
    {
        if (std::string(e.name) == "package")
            continue;
        const auto world = worldOf(Fixtures / e.world);
        const auto loaded = loadSave(world, parse(gunzip(Fixtures / e.save), e.save), e.name);
        auto roster = world.society().authored();
        roster.economy.rules["staged"] = 1;
        Society s(Roster::None);
        s.configure(roster);
        s.setDay(world.society().day());
        expect(s.restore(loaded.saved.society), std::string(e.name) + ": restores for the census");
        std::cout << e.name << " (" << s.state().residents.size() << " residents):\n";
        for (const int slider : {-100, 0, 100})
        {
            s.setProsperity(slider);
            const auto began = std::chrono::steady_clock::now();
            s.stagedBands(loaded.saved.society.budgetDay, true);
            for (int again = std::getenv("RATW_CENSUS_REPEAT") ? std::atoi(std::getenv("RATW_CENSUS_REPEAT")) : 0; again > 0; --again)
                s.stagedBands(loaded.saved.society.budgetDay, true);
            const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
            std::map<std::string, std::array<int, status::Bands>> byClass;
            std::array<int, status::Bands> all{};
            std::set<std::string> seen;
            for (const auto& [id, life] : s.state().residents)
                if (const auto* st = s.statusOf(id); st && seen.insert(st->household).second)
                {
                    ++all[std::size_t(st->band)];
                    ++byClass[st->cls][std::size_t(st->band)];
                }
            const auto row = [](const std::array<int, status::Bands>& n) {
                std::string out;
                for (int b = 0; b < status::Bands; ++b)
                    out += std::string(b ? ", " : "") + status::bandName(b) + " " + std::to_string(n[std::size_t(b)]);
                return out;
            };
            std::cout << "  at " << slider << ": " << seen.size() << " households: " << row(all) << " (the pass " << ms << " ms)\n";
            for (const auto& [cls, n] : byClass)
                std::cout << "    " << cls << ": " << row(n) << "\n";
        }
    }
    return 0;
}
} // namespace

int main(int argc, char** argv)
{
    try
    {
        if (argc == 2 && std::string(argv[1]) == "--census")
            return census();
        if (argc == 3 && std::string(argv[1]) == "--make-tail")
            return makeTail(argv[2]);
        if (argc == 3 && std::string(argv[1]) == "--make-road")
            return makeRoad(argv[2]);
        if (argc == 3 && std::string(argv[1]) == "--write-loaded")
            return writeLoaded(argv[2]);
        if (argc == 3 && std::string(argv[1]) == "--check-loaded")
            return checkLoaded(argv[2]);
        const bool print = argc == 2 && std::string(argv[1]) == "--totals";
        for (const auto& e : expected())
        {
            const auto t = checkWorldSave(e, print);
            if (print)
                std::cout << e.name << ": " << t.text() << "\n";
        }
        const auto tail = checkTail(print);
        const auto road = checkRoad(print);
        if (print)
        {
            std::cout << "tail: " << tail.text() << "\n";
            std::cout << "road: " << road.text() << "\n";
            return 0;
        }
        thePackage();
        theRoadsPackage();
        theResetCopy();
        onTheFullWorlds();
    }
    catch (const std::exception& error)
    {
        std::cerr << "save_compat_tests failed after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "save_compat_tests passed (" << checks << " checks)\n";
    return 0;
}
