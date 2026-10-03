#include "RatwGame.h"

#include "RatwCellPrefetch.h"
#include "RatwMotionCore.h"
#include "RatwWire.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace ratw::game
{
using json::Value;

// --------------------------------------------------------------------------- Stores

namespace
{
class DatabaseStore final : public Store
{
  public:
    DbStore db;
    std::unique_ptr<journal::Writer> writer;
    bool database() const override { return true; }
    std::string load() override { return db.load(); }
    bool save(DbStore::Build build, std::uint64_t revision) override { return db.save(std::move(build), revision); }
    bool saveInBackground(DbStore::Build build, std::uint64_t revision) override { return db.saveInBackground(std::move(build), revision); }
    bool flush() override { return db.flush(); }
    void queueEvents(std::vector<WorldEvent> events) override { db.queueEvents(std::move(events)); }
    std::vector<std::pair<std::string, std::string>> externalNpcStates() override { return db.externalNpcStates(); }
    std::string error() const override { return db.error(); }
    journal::Writer* journal() override { return writer.get(); }
    bool journalAfter(std::uint64_t after, std::vector<journal::Record>& out, std::string& problem) override
    {
        return db.journalAfter(after, out, problem);
    }
    std::uint64_t storedRevision() const override { return db.storedRevision(); }
};

// The journal in game.journal (migration 0028), written on its own connection so a long checkpoint write never
// holds a trade's record back.
std::unique_ptr<journal::Writer> databaseJournal(const std::string& conninfo, const std::string& worldId)
{
    auto pg = std::make_shared<PgClient>();
    auto commit = [pg, conninfo, worldId](const std::vector<journal::Record>& batch, std::string& error) {
        if (!pg->connected() && !pg->connect(conninfo, error))
            return false;
        auto result = pg->exec("BEGIN");
        for (std::size_t i = 0; result.ok && i < batch.size(); ++i)
            result = pg->exec("INSERT INTO game.journal (world_id, seq, record) VALUES ($1, $2, $3) ON CONFLICT DO NOTHING",
                              {worldId, std::to_string(batch[i].seq), withoutNul(batch[i].text)});
        if (result.ok)
            result = pg->exec("COMMIT");
        if (!result.ok)
        {
            error = result.error;
            pg->exec("ROLLBACK");
        }
        return result.ok;
    };
    auto trim = [pg, conninfo, worldId](std::uint64_t upTo, std::string& error) {
        if (!pg->connected() && !pg->connect(conninfo, error))
            return false;
        const auto result = pg->exec("DELETE FROM game.journal WHERE world_id = $1 AND seq <= $2", {worldId, std::to_string(upTo)});
        error = result.error;
        return result.ok;
    };
    return std::make_unique<journal::Writer>(commit, trim);
}

class FileStore final : public Store
{
  public:
    std::string path, problem;
    std::unique_ptr<journal::Writer> writer;
    journal::Writer* journal() override { return writer.get(); }
    bool journalAfter(std::uint64_t after, std::vector<journal::Record>& out, std::string& trouble) override
    {
        return journal::readFile(path + ".journal", after, out, trouble);
    }
    std::string snapshotFile() const override { return path; }
    bool database() const override { return false; }
    std::string load() override
    {
        std::ifstream in(path, std::ios::binary);
        if (!in)
            return {};
        std::ostringstream text;
        text << in.rdbuf();
        return text.str().empty() ? "{\"schema\":-1}" : text.str();
    }
    bool save(DbStore::Build build, std::uint64_t) override
    {
        Value document;
        std::string states;
        build(document, states);
        const std::string temp = path + ".tmp";
        {
            std::ofstream out(temp, std::ios::binary | std::ios::trunc);
            if (!out)
            {
                problem = "Cannot write " + temp + ".";
                return false;
            }
#if defined(__unix__) || defined(__APPLE__)
            ::chmod(temp.c_str(), 0600);           // Account verifiers are in it: the owner's alone.
#endif
            out << json::dump(document);
            if (!out.flush())
            {
                problem = "Cannot write " + temp + ".";
                return false;
            }
        }
        if (std::rename(temp.c_str(), path.c_str()) != 0)
        {
            problem = "Cannot replace " + path + ".";
            return false;
        }
        return true;
    }
    bool saveInBackground(DbStore::Build build, std::uint64_t revision) override { return save(std::move(build), revision); }
    bool flush() override { return true; }
    void queueEvents(std::vector<WorldEvent>) override {}
    std::vector<std::pair<std::string, std::string>> externalNpcStates() override { return {}; }
    std::string error() const override { return problem; }
};

class MemoryStoreImpl final : public Store
{
  public:
    std::string saved;
    std::shared_ptr<std::vector<journal::Record>> records = std::make_shared<std::vector<journal::Record>>();
    std::unique_ptr<journal::Writer> writer = journal::memoryWriter(records);
    journal::Writer* journal() override { return writer.get(); }
    bool journalAfter(std::uint64_t after, std::vector<journal::Record>& out, std::string&) override
    {
        writer->flush();
        for (const auto& r : *records)
            if (r.seq > after)
                out.push_back(r);
        return true;
    }
    bool database() const override { return false; }
    std::string load() override { return saved; }
    bool save(DbStore::Build build, std::uint64_t) override
    {
        Value document;
        std::string states;
        build(document, states);
        saved = json::dump(document);
        return true;
    }
    bool saveInBackground(DbStore::Build build, std::uint64_t revision) override { return save(std::move(build), revision); }
    bool flush() override { return true; }
    void queueEvents(std::vector<WorldEvent>) override {}
    std::vector<std::pair<std::string, std::string>> externalNpcStates() override { return {}; }
    std::string error() const override { return {}; }
};

std::string lowerAscii(std::string s)
{
    for (auto& c : s)
        if (c >= 'A' && c <= 'Z')
            c = char(c - 'A' + 'a');
    return s;
}
std::string base36(std::uint64_t n)
{
    std::string out;
    for (; n; n /= 36)
        out.insert(out.begin(), "0123456789abcdefghijklmnopqrstuvwxyz"[n % 36]);
    return out;
}
std::string format(const char* fmt, double a, double b)
{
    char buf[256];
    std::snprintf(buf, sizeof buf, fmt, a, b);
    return buf;
}
} // namespace

std::unique_ptr<Store> databaseStore(const std::string& conninfo, const std::string& worldId, std::string& error)
{
    auto store = std::make_unique<DatabaseStore>();
    if (!store->db.open(conninfo, worldId, error))
        return nullptr;
    if (store->db.journalSupported())
        store->writer = databaseJournal(conninfo, worldId);
    return store;
}

std::unique_ptr<Store> fileStore(const std::string& path, std::string& error)
{
    if (path.empty())
    {
        error = "A save path is needed.";
        return nullptr;
    }
    auto store = std::make_unique<FileStore>();
    store->path = path;
    store->writer = journal::fileWriter(path + ".journal");
    return store;
}

std::unique_ptr<Store> memoryStore() { return std::make_unique<MemoryStoreImpl>(); }

// --------------------------------------------------------------------------- The game

namespace
{
FRatwCellPrefetch& prefetcher()
{
    static FRatwCellPrefetch p;                    // (One world per process.)
    return p;
}
bool prefetching = false;
} // namespace

Game::Game(Options options) : options_(std::move(options)), random_(std::random_device{}()) {}

Game::~Game()
{
    *alive_ = false;
    reapSnapshot(true);
    if (store_)
    {
        store_->flush();
        if (auto* writer = store_->journal())
            writer->flush();
    }
}

void Game::note(const char* level, const std::string& text)
{
    if (log)
        log(level, text);
    else
        std::cerr << "[" << level << "] " << text << '\n';
}

double Game::now()
{
    return double(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
}

double Game::clock()
{
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

std::string Game::guid()
{
    char out[33];
    std::snprintf(out, sizeof out, "%016llx%016llx", static_cast<unsigned long long>(random_()), static_cast<unsigned long long>(random_()));
    return out;
}

bool Game::start(std::string& problem)
{
    // Cheaper voices (doc 28): the router and the exchange library, and the ledger. Neither is needed to play.
    if (!options_.voiceData.empty())
    {
        std::string trouble;
        if (voices_.load(options_.voiceData, trouble))
            note("info", "RATW_VOICE router " + std::string(voices_.routes() ? "on" : "off") + ", " +
                             std::to_string(voices_.libraryEntries()) + " exchanges in the library");
        else
            note("warn", "RATW_VOICE the voice data could not be read (" + trouble + "); models answer everything");
        if (scenes_.load(options_.voiceData + "/scenes", trouble))
            note("info", "RATW_SCENES " + std::to_string(scenes_.size()) + " written scenes for NPCs talking to each other");
        else
            note("warn", "RATW_SCENES the scenes could not be read: " + trouble);
    }
    if (!options_.voiceLog.empty())
    {
        voiceLog_.open(options_.voiceLog, std::ios::app);
        if (!voiceLog_)
            note("warn", "RATW_VOICE the ledger " + options_.voiceLog + " cannot be written");
    }
    const bool live = !options_.database.empty();
    if (live)
    {
        if (!loadFromDatabase(problem))
            return false;
        store_ = databaseStore(options_.conninfo, liveWorldId_, problem);
        if (!store_)
        {
            problem = "the world database save is unavailable: " + problem;
            return false;
        }
    }
    else
    {
        if (!options_.worldExport.empty())
        {
            if (!loadExport(problem))
                return false;
        }
        else if (!options_.worldFile.empty())
        {
            const auto loaded = world_.loadWorldFile(options_.worldFile);
            if (!loaded.ok)
            {
                problem = loaded.message;
                return false;
            }
            note("info", "RATW_WORLD_IMPORTED cells=" + std::to_string(world_.cells().size()) + " residents=" +
                             std::to_string(world_.society().state().residents.size()));
        }
        else
        {
            for (const auto& file : options_.cellFiles)
                if (const auto loaded = world_.loadCellFile(file); !loaded.ok)
                    note("warning", "RATW authored cell rejected; using built-in fallback: " + loaded.message);
            for (const auto& [id, cell] : world_.cells())
                world_.cell(id)->region = "demo_reach";
        }
        store_ = customStore_ ? std::move(customStore_) : options_.savePath.empty() ? memoryStore() : fileStore(options_.savePath, problem);
        if (!store_)
            return false;
    }
    storageReady_ = true;
    if (options_.workerThreads > 0)
    {
        pool_ = std::make_unique<Pool>(options_.workerThreads);
        world_.setParallel([this](std::size_t count, const std::function<void(std::size_t)>& job) { pool_->run(count, job); });
        world_.setRoutesOffThread(true);
    }
    {
        // Uploaded portraits (doc 29, phase 9): beside the save, or in the database. Not needed to play.
        std::string trouble;
        artwork_ = live ? art::databaseStore(options_.conninfo, liveWorldId_, trouble)
                   : options_.savePath.empty() ? art::memoryStore() : art::folderStore(options_.savePath + ".art", trouble);
        if (artwork_)
            for (const auto& m : artwork_->all())
                artworkMeta_[m.id] = m;
        else
            note("warn", "RATW_ARTWORK portraits can't be uploaded: " + trouble);
    }
    const std::string payload = store_->load();
    load(payload);
    if (auto* writer = store_->journal(); writer && storageReady_ && !journalSeq_)
    {
        // No checkpoint to replay over (a world never saved): records left by a crash before its first snapshot
        // can't be applied to anything. They are set aside, and new ones numbered after them.
        std::vector<journal::Record> orphans;
        std::string trouble;
        if (store_->journalAfter(0, orphans, trouble) && !orphans.empty())
        {
            journalSeq_ = orphans.back().seq;
            writer->trim(journalSeq_);
            note("warn", "RATW_JOURNAL " + std::to_string(orphans.size()) + " record(s) without a checkpoint were set aside");
        }
    }
    // A world never saved is saved now, so the journal always has a checkpoint to be replayed over.
    if (payload.empty() && storageReady_ && store_->journal())
        save();
    if (!storageReady_)
    {
        problem = "the save could not be read; it is kept as it is and nothing is saved over it (" + store_->error() + ")";
        if (live || options_.requireStorage)
            return false;
        note("error", "RATW " + problem);
    }
    if (live)
        applyExternalNpcStates();
    if (!options_.directorDirectory.empty())
    {
        director_.log = [this](const std::string& text) { note("error", text); };
        if (!director_.configure(options_.directorDirectory))
        {
            problem = "the operator bridge needs a healthy checkpoint and an absolute owner-private directory.";
            return false;
        }
        save();
        if (!storageReady_)
        {
            problem = "the save failed.";
            return false;
        }
        note("info", "RATW private operator bridge enabled; no character credentials are accepted.");
    }
    mind_.configure(options_.dialogueEndpoint);
    consolidate();
    prime(shadow_);
    note("info", "RATW authoritative world ready; 20Hz; save=" + std::string(live ? options_.database + " database" : options_.savePath.empty() ? "memory" : options_.savePath) +
                     "; dialogue=" + mind_.label());
    return true;
}

bool Game::loadFromDatabase(std::string& problem)
{
    if (options_.database != "prod" && options_.database != "dev")
    {
        problem = "the database must be prod or dev.";
        return false;
    }
    if (options_.conninfo.empty())
    {
        problem = "set RATW_DATABASE_URL (tools/live.sh does this from Database/.env).";
        return false;
    }
    if (!worldDb_.connect(options_.conninfo, problem))
    {
        problem = "cannot reach the " + options_.database + " database: " + problem;
        return false;
    }
    const auto build = worldDb_.exec("SELECT w.id, b.id, coalesce(b.release, 0), b.files::text FROM world.worlds w "
                                     "JOIN world.builds b ON b.world_id = w.id ORDER BY b.id DESC LIMIT 1");
    if (!build.ok || build.rows.empty() || !build.rows[0][3])
    {
        problem = "the " + options_.database + " database has no world build yet" +
                  (build.ok ? std::string(" (Push to live makes one for PROD; python3 tools/world_build.py dev for DEV)") : ": " + build.error);
        return false;
    }
    const auto& row = build.rows[0];
    Value files;
    std::string error;
    if (!json::parse(*row[3], files, error) || !files.isObject())
    {
        problem = "build " + *row[1] + " is not readable.";
        return false;
    }
    worldFiles_.clear();
    for (const auto& [name, text] : files.fields())
    {
        if (!text.isString())
        {
            problem = "build " + *row[1] + " has a malformed file entry.";
            return false;
        }
        worldFiles_[name] = text.asString();
    }
    worldFiles_["world.ratw"] = withoutPeople(worldFiles_["world.ratw"]);
    liveWorldId_ = *row[0];
    loadedBuild_ = std::stoll(*row[1]);
    streamedBuild_ = worldFiles_["world.ratw"].rfind("RATW_WORLD 3", 0) == 0;
    cellHeaders_.clear();
    if (streamedBuild_)
    {
        const auto headers = worldDb_.exec("SELECT cell_id, header FROM world.build_cells WHERE build_id = $1", {std::to_string(loadedBuild_)});
        if (!headers.ok || headers.rows.empty())
        {
            problem = "build " + std::to_string(loadedBuild_) + " has no cells: " + headers.error;
            return false;
        }
        for (const auto& cell : headers.rows)
            if (cell[0] && cell[1])
                cellHeaders_[*cell[0]] = *cell[1];
        std::string prefetchProblem;
        prefetching = prefetcher().Start(options_.conninfo, std::to_string(loadedBuild_), prefetchProblem);
        if (!prefetching)
            note("warning", "RATW cells will not be fetched ahead (each loads when needed): " + prefetchProblem);
    }
    if (!loadWithLivePeople(world_, problem))
        return false;
    std::string listen;
    if (!worldDb_.listen("ratw_release", listen))
        note("warning", "RATW will not hear about new releases: " + listen);
    if (!worldDb_.listen("ratw_dm", listen))
        note("warning", "RATW will check for Dungeon Master actions once a minute: " + listen);
    note("info", "RATW_DATABASE_WORLD_LOADED database=" + options_.database + " world=" + liveWorldId_ + " build=" +
                     std::to_string(loadedBuild_) + " release=" + (row[2] ? *row[2] : "0") + " cells=" + std::to_string(world_.cells().size()) +
                     " residents=" + std::to_string(world_.society().state().residents.size()) + " streamed=" +
                     (streamedBuild_ ? "1" : "0") + " loaded=" + std::to_string(world_.loadedCells()));
    return true;
}

bool Game::loadExport(std::string& problem)
{
    namespace fs = std::filesystem;
    const fs::path root = options_.worldExport;
    std::error_code error;
    worldFiles_.clear();
    cellHeaders_.clear();
    exportCells_.clear();
    std::map<std::string, std::string> seams;
    for (fs::recursive_directory_iterator it(root, error), end; !error && it != end; it.increment(error))
    {
        if (!it->is_regular_file())
            continue;
        std::ifstream in(it->path(), std::ios::binary);
        std::ostringstream text;
        text << in.rdbuf();
        const std::string path = fs::relative(it->path(), root).generic_string();
        if (path.rfind("cells/", 0) == 0 && path.size() > 11 && path.compare(path.size() - 5, 5, ".cell") == 0)
        {
            const std::string id = path.substr(6, path.size() - 11), body = text.str();
            const auto grid = body.find("grid:");
            cellHeaders_[id] = grid == std::string::npos ? body : body.substr(0, grid + 5);
            exportCells_[id].first = body;
        }
        else if (path.rfind("seams/", 0) == 0)
            seams[path.substr(6)] = text.str();
        else
            worldFiles_[path] = text.str();
    }
    if (error || !worldFiles_.count("world.ratw"))
    {
        problem = "no world export at " + root.string() + (error ? ": " + error.message() : " (no world.ratw)");
        return false;
    }
    for (auto& [id, cell] : exportCells_)
        cell.second = seams[id];
    streamedBuild_ = worldFiles_["world.ratw"].rfind("RATW_WORLD 3", 0) == 0;
    if (streamedBuild_)
        world_.setCellSource(cellSource());
    const auto loaded = world_.loadWorldFiles(worldFiles_, root.string());
    if (!loaded.ok)
    {
        problem = loaded.message;
        return false;
    }
    note("info", "RATW_WORLD_EXPORT cells=" + std::to_string(world_.cells().size()) + " residents=" +
                     std::to_string(world_.society().state().residents.size()));
    return true;
}

std::string Game::withoutPeople(const std::string& manifest)
{
    std::istringstream in(manifest);
    std::string line, out;
    bool first = true;
    while (std::getline(in, line))
    {
        if (first)
        {
            first = false;
            if (line == "RATW_WORLD 1")
                line = "RATW_WORLD 2";             // The version that allows NPC records, which are added next.
        }
        else if (line.rfind("economy ", 0) == 0 || line.rfind("route ", 0) == 0 || line.rfind("resident ", 0) == 0 ||
                 line.rfind("story ", 0) == 0 || line.rfind("faction ", 0) == 0)
            continue;
        else if (line.rfind("territory ", 0) == 0)
        {
            std::istringstream fields(line.substr(10));
            std::string id, region, chapter;
            if (fields >> std::quoted(id) >> std::quoted(region) >> std::quoted(chapter))
            {
                std::ostringstream kept;
                kept << "territory " << std::quoted(id) << ' ' << std::quoted(region) << ' ' << std::quoted(chapter) << " 0";
                line = kept.str();
            }
        }
        out += line + "\n";
    }
    return out;
}

World::CellSource Game::cellSource()
{
    World::CellSource source;
    source.header = [this](const std::string& id, std::string& header) {
        const auto found = cellHeaders_.find(id);
        if (found == cellHeaders_.end())
            return "The build has no cell " + id + ".";
        header = found->second;
        return std::string();
    };
    const auto build = std::to_string(loadedBuild_);
    source.load = [this, build](const std::string& id, std::string& body, std::string& seams) {
        if (!exportCells_.empty())
        {
            const auto found = exportCells_.find(id);
            if (found == exportCells_.end())
                return "The export has no cell " + id + ".";
            body = found->second.first;
            seams = found->second.second;
            return std::string();
        }
        if (prefetching && prefetcher().Take(id, body, seams))
            return std::string();
        const auto cell = worldDb_.exec("SELECT body, seams FROM world.build_cells WHERE build_id = $1 AND cell_id = $2", {build, id});
        if (!cell.ok || cell.rows.empty() || !cell.rows[0][0])
            return "Cannot read cell " + id + " of build " + build + ": " + (cell.ok ? std::string("missing") : cell.error);
        body = *cell.rows[0][0];
        seams = cell.rows[0][1] ? *cell.rows[0][1] : "";
        return std::string();
    };
    return source;
}

bool Game::loadWithLivePeople(World& into, std::string& problem)
{
    const auto people = worldDb_.exec("SELECT live.people_manifest($1)", {liveWorldId_});
    if (!people.ok || people.rows.empty() || !people.rows[0][0])
    {
        problem = "Cannot read the NPCs from the live tables: " + people.error;
        return false;
    }
    auto files = worldFiles_;
    files["world.ratw"] += *people.rows[0][0] + "\n";
    if (streamedBuild_)
        into.setCellSource(cellSource());
    const auto loaded = into.loadWorldFiles(files, "live world");
    problem = loaded.message;
    return loaded.ok;
}

void Game::readNotifications()
{
    for (const auto& [channel, payload] : worldDb_.notifications())
    {
        if (channel == "ratw_dm")
        {
            dmNotified_ = true;
            continue;
        }
        Value v;
        std::string error;
        if (channel != "ratw_release" || !json::parse(payload, v, error) || !v.isObject())
            continue;
        const auto build = std::int64_t(v.number("build"));
        if (build > loadedBuild_)
        {
            pendingRelease_ = std::int64_t(v.number("release"));
            releaseAnnounced_ = false;
            note("info", "RATW_RELEASE_PUBLISHED release=" + std::to_string(pendingRelease_) + " build=" + std::to_string(build));
        }
    }
}

void Game::watchReleases(double dt)
{
    if (options_.database.empty() || (releaseAccumulator_ += dt) < 2)
        return;
    releaseAccumulator_ = 0;
    readNotifications();
    if (!pendingRelease_)
        return;
    if (clients_.empty())
    {
        save();
        note("info", "RATW_RELEASE_RESTART release=" + std::to_string(pendingRelease_) + "; saved, exiting so the new world loads.");
        pendingRelease_ = 0;
        exit_ = 75;
    }
    else if (!releaseAnnounced_)
    {
        releaseAnnounced_ = true;
        for (auto* c : clients_)
            system(c, "A new version of the world (release " + std::to_string(pendingRelease_) +
                          ") has been published. It takes effect after a short restart once everyone has left.");
    }
}

void Game::applyDmActions(double dt)
{
    if (options_.database.empty() || (dmAccumulator_ += dt) < 1)
        return;
    dmAccumulator_ = 0;
    readNotifications();
    const bool minute = (dmExpiryAccumulator_ += 1) >= 60;
    if (!dmNotified_ && !minute)
        return;
    dmNotified_ = false;
    if (minute)
    {
        dmExpiryAccumulator_ = 0;
        worldDb_.exec("UPDATE dm.actions SET status = 'expired', result = 'Not applied within ten minutes.', done_at = now() "
                      "WHERE status = 'queued' AND requested_at < now() - interval '10 minutes'");
    }
    const auto queued = worldDb_.exec("SELECT id, kind, target_id, requested_by, payload::text FROM dm.actions WHERE status = 'queued' ORDER BY id LIMIT 50");
    bool changed = false;
    for (const auto& row : queued.rows)
    {
        if (!row[0] || !row[1] || !row[2])
            continue;
        const std::string kind = *row[1], target = *row[2];
        Result outcome{false, "Unknown action.", {}};
        if (kind == "layers.sync" || kind == "factions.sync" || kind == "npc.sync")
        {
            World candidate;
            std::string problem;
            const bool loaded = loadWithLivePeople(candidate, problem);
            if (kind == "layers.sync")
            {
                if (loaded) world_.adoptLayers(candidate);
                outcome = loaded ? Result{true, "Routes and areas are updated.", {}}
                                 : Result{false, "The routes and areas could not be applied: " + problem, {}};
            }
            else if (kind == "factions.sync")
            {
                if (loaded) world_.adoptFactions(candidate);
                outcome = loaded ? Result{true, "Factions and territory are updated.", {}}
                                 : Result{false, "The factions could not be applied: " + problem, {}};
            }
            else
                outcome = loaded ? world_.adoptResident(candidate, target) : Result{false, "The NPC could not be placed: " + problem, {}};
        }
        else if (kind == "artwork.review")
        {
            // A Dungeon Master's decision on an uploaded portrait: payload {"decision": "approve"|"reject", "reason"}.
            Value payload;
            std::string problem;
            json::parse(row[4] ? *row[4] : "{}", payload, problem);
            outcome = reviewArtwork(target, payload.string("decision"), payload.string("reason"));
        }
        else if (kind == "festival.call")
        {
            // Payload: {"name": "...", "inDays": 0..30}; the target is the community (a town, or a region).
            json::Value payload;
            std::string problem;
            if (row.size() > 4 && row[4])
                json::parse(*row[4], payload, problem);
            const std::string name = payload.isObject() ? payload.string("name") : std::string();
            const int inDays = payload.isObject() ? int(wire::number(payload, "inDays", 0)) : 0;
            outcome = world_.callFestival(target, name, inDays);
            if (outcome.ok)
                for (auto* c : clients_)
                    if (const auto* e = world_.entity(c->entityId); e && world_.communityOf(e->cellId) == target)
                        system(c, "Word goes round: " + outcome.message);
        }
        else if (kind == "npc.kill" || kind == "npc.revive")
        {
            const auto* npc = world_.entity(target);
            outcome = npc && npc->npc ? world_.setDead(target, kind == "npc.kill") : Result{false, "No such NPC.", {}};
        }
        else if (kind == "character.gift")
        {
            // A Gift given or taken away (doc 33: who has one is the setting's to decide, through the Dungeon Master).
            // Payload: {"gift": "fire" | "", "quickened": bool}.
            json::Value payload;
            std::string problem;
            if (row.size() > 4 && row[4])
                json::parse(*row[4], payload, problem);
            const std::string gift = payload.isObject() ? payload.string("gift") : std::string();
            const bool quickened = payload.isObject() && payload.boolean("quickened");
            const auto told = [&](const Entity& e) {
                return gift.empty() ? e.name + " no longer has a Gift."
                                    : e.name + (quickened ? " is Quickened: the Gift of fire, enormous." : " has the Gift of fire.");
            };
            if (!gift.empty() && gift != "fire")
                outcome = {false, "The only Gift known to the game is fire.", {}};
            else if (auto* online = world_.entity(target); online && !online->npc)
            {
                outcome = world_.giveGift(target, gift, quickened);
                if (outcome.ok)
                {
                    if (auto* c = clientOf(target))
                        system(c, gift.empty() ? "The fire in you has gone quiet." : quickened
                                                                                      ? "Fire wakes in you, vast and frightening: you are Quickened."
                                                                                      : "Fire wakes in you: you have the Gift.");
                    characters_[target] = *online;
                    outcome.message = told(*online);
                }
            }
            else if (auto saved = characters_.find(target); saved != characters_.end())
            {
                auto& e = saved->second;
                e.gift = gift;
                e.quickened = !gift.empty() && quickened;
                e.mana = battle::manaMax(e.wisdom, !gift.empty());
                outcome = {true, told(e) + " (offline)", {}};
            }
            else
                outcome = {false, "No such character.", {}};
        }
        else if (kind == "character.kill" || kind == "character.resurrect")
        {
            const bool kill = kind == "character.kill";
            if (auto* online = world_.entity(target); online && !online->npc)
            {
                outcome = world_.setDead(target, kill);
                if (outcome.ok)
                {
                    if (auto* c = clientOf(target))
                        system(c, kill ? "You have died." : "You have been brought back to life.");
                    characters_[target] = *online;
                }
            }
            else if (auto saved = characters_.find(target); saved != characters_.end())
            {
                auto& e = saved->second;
                if (e.dead == kill)
                    outcome = {false, e.name + (kill ? " is already dead." : " is not dead."), {}};
                else
                {
                    e.dead = kill;
                    e.posture = kill ? "lying" : "standing";
                    e.state = e.activity = kill ? "dead" : "";
                    outcome = {true, e.name + (kill ? " is dead (offline)." : " lives again (offline)."), {}};
                }
            }
            else
                outcome = {false, "No such character.", {}};
        }
        changed |= outcome.ok;
        worldDb_.exec("UPDATE dm.actions SET status = $2, result = $3, done_at = now() WHERE id = $1 AND status = 'queued'",
                      {*row[0], std::string(outcome.ok ? "applied" : "refused"), outcome.message});
        note("info", "RATW_DM_ACTION " + kind + " " + target + " by " + (row[3] ? *row[3] : "") + ": " + outcome.message);
        logEvent("operator", row[3] ? *row[3] : "", target, kind + (outcome.ok ? ": applied" : ": refused"));
    }
    if (changed)
    {
        ++revision_;
        saveSoon();
    }
}

void Game::makeResidents()
{
    for (const auto& request : world_.takeResidentRequests())
    {
        if (options_.database.empty())
        {
            note("info", "RATW_RESIDENT " + request.kind + " wanted (" + request.name + "), but a world from files can't add residents.");
            continue;
        }
        const auto* model = world_.society().spec(request.templateId);
        const auto* modelLife = world_.society().resident(request.templateId);
        if (!model || !modelLife)
            continue;
        std::string id;
        for (int attempt = 0; id.empty() && attempt < 50; ++attempt)
        {
            const auto candidate = std::string(request.kind == "birth" ? "born_" : "new_") + base36(std::uint64_t(now()) * 64 + std::uint64_t(attempt));
            const auto taken = worldDb_.exec("SELECT 1 FROM live.npcs WHERE world_id = $1 AND id = $2 UNION ALL "
                                             "SELECT 1 FROM live.npc_state WHERE world_id = $1 AND npc_id = $2", {liveWorldId_, candidate});
            if (taken.ok && taken.rows.empty() && !world_.entity(candidate))
                id = candidate;
        }
        if (id.empty())
            continue;
        const auto home = request.home.cell.empty() ? Spot{modelLife->homeCell, modelLife->homeX, modelLife->homeY} : request.home;
        const bool child = request.kind == "birth";
        const auto* jobFor = world_.society().position(request.positionId);
        const std::string description = child ? "A young wolf, born here to " + model->name + "'s family."
                                              : "A stranger lately come to town" + (jobFor ? " to take up " + jobFor->title : std::string()) + ".";
        const auto x = std::to_string(int(std::floor(home.x))), y = std::to_string(int(std::floor(home.y)));
        const auto made = worldDb_.exec(
            "INSERT INTO live.npcs (world_id, id, position, name, role, description, greeting, personality, backstory, work_label, age, voice, "
            "appearance, route_id, paid, purse, herbs, meals, hours_start, hours_end, home_area, home_x, home_y, work_area, work_x, work_y, "
            "evening_area, evening_x, evening_y, origin, wander_area, spawn_id) "
            "SELECT world_id, $2, (SELECT coalesce(max(position) + 1, 0) FROM live.npcs WHERE world_id = $1), $3, 'civilian', $4, "
            "'Hello.', personality, $5, '-', $6::integer, voice, appearance, NULL, false, $7::integer, 0, 1, hours_start, hours_end, "
            "$8, $9::integer, $10::integer, $8, $9::integer, $10::integer, $8, $9::integer, $10::integer, 'runtime', NULL, NULL "
            "FROM live.npcs WHERE world_id = $1 AND id = $11",
            {liveWorldId_, id, request.name, description, child ? std::string("Born in the world, not written.") : std::string(),
             std::to_string(request.age), child ? std::string("0") : std::string("20"), home.cell, x, y, request.templateId});
        World candidate;
        std::string problem;
        const bool loaded = made.ok && loadWithLivePeople(candidate, problem);
        const auto adopted = loaded ? world_.adoptResident(candidate, id) : Result{false, made.ok ? problem : made.error, {}};
        if (!adopted.ok)
        {
            worldDb_.exec("DELETE FROM live.npcs WHERE world_id = $1 AND id = $2", {liveWorldId_, id});
            note("warning", "RATW_RESIDENT " + request.name + " could not be made: " + adopted.message);
            continue;
        }
        const auto welcomed = world_.welcomeResident(request, id);
        note("info", "RATW_RESIDENT " + request.kind + ": " + request.name + " (" + id + ") " + welcomed.message);
        ++revision_;
        saveSoon();
    }
}

void Game::runSpawns(double dt)
{
    if (options_.database.empty() || (spawnAccumulator_ += dt) < 30)
        return;
    spawnAccumulator_ = 0;
    const double at = clock();
    const auto rules = worldDb_.exec(
        "SELECT s.id, s.name, s.template_id, s.count, s.respawn_minutes, a.id, a.area, a.tiles::text, "
        "coalesce((SELECT string_agg(n.id, ',' ORDER BY n.id) FROM live.npcs n WHERE n.world_id = s.world_id AND n.spawn_id = s.id), '') "
        "FROM live.spawns s JOIN live.npc_areas a ON a.world_id = s.world_id AND a.id = s.area_id "
        "WHERE s.world_id = $1 AND s.enabled AND EXISTS (SELECT 1 FROM live.npcs t WHERE t.world_id = s.world_id AND t.id = s.template_id) "
        "ORDER BY s.position",
        {liveWorldId_});
    bool changed = false;
    for (const auto& row : rules.rows)
    {
        const std::string rule = *row[0], name = *row[1], templateId = *row[2], wanderArea = *row[5], area = *row[6];
        const int count = std::stoi(*row[3]);
        const double respawn = std::stod(*row[4]) * 60;
        if (spawnBackoff_.count(rule) && spawnBackoff_[rule] > at)
            continue;
        std::vector<std::string> aliveIds, dead;
        std::stringstream ids(*row[8]);
        for (std::string id; std::getline(ids, id, ',');)
            if (const auto* e = world_.entity(id))
            {
                if (!e->dead) { aliveIds.push_back(id); deadSince_.erase(id); }
                else { dead.push_back(id); deadSince_.emplace(id, at); }
            }
        if (int(aliveIds.size()) >= count)
            continue;
        std::string cleared;
        for (const auto& id : dead)
            if (at - deadSince_[id] >= respawn)
            {
                cleared = id;
                break;
            }
        if (!dead.empty() && cleared.empty() && int(aliveIds.size() + dead.size()) >= count)
            continue;                              // Waiting out the respawn time.
        if (!cleared.empty())
        {
            worldDb_.exec("DELETE FROM live.npc_state WHERE world_id = $1 AND npc_id = $2", {liveWorldId_, cleared});
            worldDb_.exec("DELETE FROM live.npcs WHERE world_id = $1 AND id = $2", {liveWorldId_, cleared});
            World candidate;
            std::string problem;
            if (loadWithLivePeople(candidate, problem))
                world_.adoptResident(candidate, cleared);
            deadSince_.erase(cleared);
            note("info", "RATW_SPAWN " + rule + " cleared the fallen " + cleared);
            logEvent("cleared", cleared, {}, "spawn rule " + rule);
            changed = true;
        }
        int tx = 0, ty = 0;
        if (!spawnTile(area, *row[7], tx, ty))
        {
            spawnBackoff_[rule] = at + 300;
            note("warning", "RATW_SPAWN " + rule + ": no open tile in its area; trying again in five minutes.");
            continue;
        }
        // A newcomer's ID is never reused; the name takes the lowest free number among those the rule keeps now.
        std::string id;
        for (int attempt = 0; id.empty(); ++attempt)
        {
            const auto candidate = rule + "_" + base36(std::uint64_t(now()) + std::uint64_t(attempt));
            const auto taken = worldDb_.exec("SELECT 1 FROM live.npcs WHERE world_id = $1 AND id = $2 UNION ALL "
                                             "SELECT 1 FROM live.npc_state WHERE world_id = $1 AND npc_id = $2", {liveWorldId_, candidate});
            if (taken.ok && taken.rows.empty() && !world_.entity(candidate))
                id = candidate;
            else if (!taken.ok || attempt > 50)
                break;
        }
        if (id.empty())
        {
            spawnBackoff_[rule] = at + 300;
            continue;
        }
        int number = 1;
        while (std::any_of(aliveIds.begin(), aliveIds.end(), [&](const std::string& other) {
            const auto* e = world_.entity(other);
            return e && e->name == name + " " + std::to_string(number);
        }))
            ++number;
        const auto x = std::to_string(tx), y = std::to_string(ty);
        const auto made = worldDb_.exec(
            "INSERT INTO live.npcs (world_id, id, position, name, role, description, greeting, personality, backstory, work_label, age, voice, "
            "appearance, route_id, paid, purse, herbs, meals, hours_start, hours_end, home_area, home_x, home_y, work_area, work_x, work_y, "
            "evening_area, evening_x, evening_y, origin, wander_area, spawn_id) "
            "SELECT world_id, $2, (SELECT coalesce(max(position) + 1, 0) FROM live.npcs WHERE world_id = $1), $3, role, description, greeting, "
            "personality, backstory, work_label, age, voice, appearance, route_id, paid, purse, herbs, meals, hours_start, hours_end, "
            "$4, $5::integer, $6::integer, $4, $5::integer, $6::integer, $4, $5::integer, $6::integer, 'runtime', $7, $8 "
            "FROM live.npcs WHERE world_id = $1 AND id = $9",
            {liveWorldId_, id, name + " " + std::to_string(number), area, x, y, wanderArea, rule, templateId});
        World candidate;
        std::string problem;
        const bool loaded = made.ok && loadWithLivePeople(candidate, problem);
        const auto outcome = loaded ? world_.adoptResident(candidate, id) : Result{false, made.ok ? problem : made.error, {}};
        if (!outcome.ok)
        {
            worldDb_.exec("DELETE FROM live.npcs WHERE world_id = $1 AND id = $2", {liveWorldId_, id});
            spawnBackoff_[rule] = at + 300;
            note("warning", "RATW_SPAWN " + rule + " failed; trying again in five minutes: " + outcome.message);
            continue;
        }
        changed = true;
        note("info", "RATW_SPAWN " + rule + ": " + id + " arrives in " + area + " at " + x + ", " + y);
        logEvent("spawn", id, {}, "spawn rule " + rule);
    }
    if (changed)
    {
        ++revision_;
        saveSoon();
    }
}

bool Game::spawnTile(const std::string& area, const std::string& tilesJson, int& x, int& y)
{
    Value tiles;
    std::string error;
    if (!json::parse(tilesJson, tiles, error) || !tiles.isArray() || !world_.ensureLoaded(area).ok)
        return false;
    const auto* cell = world_.cell(area);
    if (!cell)
        return false;
    std::vector<std::pair<int, int>> open;
    for (const auto& pair : tiles.items())
    {
        if (!pair.isArray() || pair.size() != 2)
            continue;
        const int tx = int(pair.items()[0].asNumber()), ty = int(pair.items()[1].asNumber());
        const auto* tile = cell->tile(tx, ty);
        bool taken = false;
        for (const auto& [id, e] : world_.entities())
            taken |= e.cellId == area && std::abs(e.position.x - (tx + .5)) < .8 && std::abs(e.position.y - (ty + .5)) < .8;
        if (tile && !tile->solid && !taken)
            open.emplace_back(tx, ty);
    }
    if (open.empty())
        return false;
    const auto& chosen = open[std::uniform_int_distribution<std::size_t>(0, open.size() - 1)(random_)];
    x = chosen.first;
    y = chosen.second;
    return true;
}

void Game::applyExternalNpcStates()
{
    const auto states = store_->externalNpcStates();
    if (states.empty())
        return;
    SocietyState society = world_.society().state();
    int applied = 0;
    for (const auto& [id, text] : states)
    {
        auto* e = world_.entity(id);
        Value j;
        std::string error;
        if (!e || !e->npc || !json::parse(text, j, error) || !j.isObject())
            continue;
        if (j["cell"].isString() && j["x"].isNumber() && j["y"].isNumber())
            if (const auto* c = world_.cell(j.string("cell")))
                if (const auto* t = c->tile(int(std::floor(j.number("x"))), int(std::floor(j.number("y")))); t && !t->solid)
                {
                    e->cellId = j.string("cell");
                    e->position = {j.number("x"), j.number("y")};
                    world_.stop(e->id);
                }
        if (j["age"].isNumber() && j.number("age") >= 0 && j.number("age") <= 200)
            e->age = int(j.number("age"));
        if (j["dead"].isBool() && j.boolean("dead") != e->dead)
            world_.setDead(e->id, j.boolean("dead"));
        if (auto life = society.residents.find(e->id); life != society.residents.end())
        {
            if (j["hunger"].isNumber()) life->second.hunger = std::clamp(j.number("hunger"), 0.0, 100.0);
            if (j["fatigue"].isNumber()) life->second.fatigue = std::clamp(j.number("fatigue"), 0.0, 100.0);
        }
        auto account = society.accounts.find(e->id);
        auto treasury = society.accounts.find("treasury");
        if (account != society.accounts.end() && treasury != society.accounts.end())
        {
            if (j["cash"].isNumber() && j.number("cash") >= 0)
            {
                std::int64_t delta = std::int64_t(j.number("cash")) - account->second.cash;
                delta = std::min<std::int64_t>(delta, treasury->second.cash);   // The treasury pays in...
                delta = std::max<std::int64_t>(delta, -account->second.cash);   // ...or takes back.
                account->second.cash += delta;
                treasury->second.cash -= delta;
            }
            if (const auto& stock = j["stock"]; stock.isObject())
                for (const char* item : {"herbs", "meal", "sword"})
                    if (stock[item].isNumber())
                        account->second.stock[item] = int(std::clamp(stock.number(item), 0.0, 10000.0));
        }
        ++applied;
    }
    if (!world_.society().restore(society))
        note("warning", "RATW copied NPC purses/needs were not valid for this world; positions and ages applied only.");
    note("info", "RATW_NPC_STATE_APPLIED count=" + std::to_string(applied));
}

// --------------------------------------------------------------------------- Clients

Connection* Game::clientOf(const std::string& entityId) const
{
    for (auto* c : clients_)
        if (c->entityId == entityId)
            return c;
    return nullptr;
}

void Game::connect(Connection* c)
{
    if (!c || std::find(clients_.begin(), clients_.end(), c) != clients_.end())
        return;
    clients_.push_back(c);
    lobby(c);
}

void Game::pose(Connection* c, std::uint32_t seq, double x, double y, double facing, double ix, double iy)
{
    if (!c || c->entityId.empty() || c->movementMode == Fighting)
        return;
    const auto& id = c->entityId;
    auto* player = world_.entity(id);
    if (!player)
        return;
    if (c->keysWalking && c->clientWalking && c->movementMode == FreeMovement)
    {
        c->keysWalking = false;                    // Poses again: the client walks it.
        world_.setClientWalks(id, true);
    }
    const auto check = world_.placeByClient(id, seq, x, y, facing, ix, iy);
    if (check.accepted || !player->clientWalks)
        return;
    // Where the wolf truly is: the client goes back there, eased.
    auto correction = Value::object();
    correction.add("type", "correction");
    correction.add("seq", double(player->poseSeq));
    correction.add("cellId", player->cellId);
    correction.add("x", player->position.x);
    correction.add("y", player->position.y);
    correction.add("facing", player->facing);
    correction.add("reason", check.reason);
    send(c, correction);
    if (player->poseStrikes == 20 || player->poseStrikes == 200)
    {
        note("warn", "RATW_POSE " + id + " " + std::to_string(player->poseStrikes) + " poses refused in a row (" + check.reason + ")");
        logEvent("movement refused", id, {}, std::to_string(player->poseStrikes) + " poses refused: " + check.reason);
    }
}

void Game::setFighting(const std::string& id, bool fighting)
{
    if (fighting)
    {
        fighting_.insert(id);
        world_.setClientWalks(id, false);
        world_.stop(id);
    }
    else
        fighting_.erase(id);
    updateMovementModes();
}

void Game::updateMovementModes()
{
    for (auto* c : clients_)
    {
        const auto* e = world_.entity(c->entityId);
        if (!e)
            continue;
        const auto& id = e->id;
        std::uint8_t mode = FreeMovement;
        if (fighting_.count(id) || world_.inBattle(id) || e->lingering)
            mode = Fighting;                       // In an arena: no walking at all (doc 33).
        else if (e->downedLeft > 0)
            mode = HeldMovement;                   // Downed: the server crawls the wolf (half a tile a second).
        else if (world_.pursued(id) || world_.foughtWithin(id, HeldAfter) || world_.offendedWithin(id, HeldAfter) ||
                 !e->path.empty() || world_.travelState(id).active)
            mode = HeldMovement;                   // (A route or a journey the server walks counts as held too.)
        else
            for (const Entity* other : world_.entitiesIn(e->cellId))
                if (other->cellId == e->cellId && other->npc && !other->dead &&
                    std::hypot(other->position.x - e->position.x, other->position.y - e->position.y) <= HostileNear &&
                    world_.hostile(other->id))
                {
                    mode = HeldMovement;
                    break;
                }
        if (mode != c->movementMode && mode != FreeMovement)
            world_.stop(id);                       // The server takes over from where the wolf stands, still.
        c->movementMode = mode;
        world_.setClientWalks(id, mode == FreeMovement && c->clientWalking && !c->keysWalking);
    }
}

void Game::finishSignIns()
{
    for (const auto& done : hasher_.finished())
    {
        const auto found = signIns_.find(done.ticket);
        if (found == signIns_.end())
            continue;                              // Its client has gone.
        const auto pending = found->second;
        signIns_.erase(found);
        Connection* c = pending.c;
        beginHolding(c);                           // A registration's answer waits for its journal record.
        if (pending.registering)
        {
            std::string error;
            if (!c->accountUsername.empty())
                lobby(c, false, "Log out before signing into another account.");
            else if (!accounts_.addRegistered(pending.user, done, error))
                lobby(c, false, error);
            else
            {
                record(Accounts);
                if (!storageReady_)
                {
                    accounts_.removeAccount(pending.user);
                    lobby(c, false, "Account creation could not be saved. No account was acknowledged.");
                }
                else
                {
                    c->accountUsername = pending.user;
                    lobby(c, true, "Account saved. Create your first character.");
                }
            }
        }
        else if (!done.ok)
            lobby(c, false, "Username or password was not accepted.");
        else if (!c->accountUsername.empty())
            lobby(c, false, "Log out before signing into another account.");
        else
        {
            c->accountUsername = pending.user;
            lobby(c, true, "Signed in. Choose a character or create one.");
        }
        endHolding();
    }
}

void Game::disconnect(Connection* c)
{
    if (!c)
        return;
    for (auto it = signIns_.begin(); it != signIns_.end();)
        it = it->second.c == c ? signIns_.erase(it) : std::next(it);
    leaveCharacter(c);
    c->accountUsername.clear();
    authRate_.forget(std::to_string(c->id));
    clients_.erase(std::remove(clients_.begin(), clients_.end(), c), clients_.end());
    waiting_.erase(std::remove_if(waiting_.begin(), waiting_.end(), [c](const Waiting& w) { return w.c == c; }), waiting_.end());
    c->clientWalking = false;
    cellRows_.erase(c);
    if (holding_ == c)
        holding_ = nullptr;
}

void Game::acknowledge(Connection* c, double revision, bool missing)
{
    if (c)
        c->held.acknowledged(revision, missing);
}

void Game::send(Connection* c, const Value& e)
{
    if (!c)
        return;
    if (c == holding_)
        held_.push_back(json::dump(e));            // Sent when the command is done (endHolding).
    else
        c->event(json::dump(e));
}

void Game::system(Connection* c, const std::string& message)
{
    auto e = Value::object();
    e.add("type", "system");
    e.add("speaker", "World");
    e.add("text", c ? veilFor(c->entityId, message) : message);   // Names they don't know, as the wolf looks (doc 32).
    e.add("sequence", sequence_++);
    e.add("color", 7);
    if (c)
    {
        const auto command = currentCommands_.find(c->entityId);
        if (command != currentCommands_.end() && !command->second.empty())
            responseReceipts_[c->entityId][command->second] = json::dump(e);
    }
    send(c, e);
}

void Game::lobby(Connection* c, bool ok, const std::string& message)
{
    if (!c)
        return;
    if (!c->entityId.empty())
    {
        system(c, message.empty() ? "Leave your character before returning to account selection." : message);
        return;
    }
    auto e = Value::object();
    e.add("type", "lobby");
    e.add("stage", c->accountUsername.empty() ? "login" : "characters");
    e.add("ok", ok);
    e.add("localOnly", true);
    e.add("credentialsAllowed", c->allowsLocalCredentials());
    e.add("message", message.empty() ? "Local development accounts only. Native networking is not encrypted; use a unique test password." : message);
    e.add("slotsLimit", int(accounts::CharacterSlots));
    auto roster = Value::array();
    if (!c->accountUsername.empty())
        for (const auto& id : accounts_.characters(c->accountUsername))
        {
            const auto it = characters_.find(id);
            if (it == characters_.end())
                continue;
            auto character = it->second;
            advanceAge(character, world_.calendarDays());
            auto item = Value::object();
            item.add("id", id);
            item.add("name", character.name);
            item.add("age", character.age);
            item.add("appearance", wire::appearance(character.appearance));
            if (const auto* portrait = portraitOf(id))
            {
                item.add("artwork", portrait->id);
                item.add("artworkStatus", portrait->status);
            }
            roster.push(item);
        }
    e.add("characters", roster);
    send(c, e);
}

bool Game::enterCharacter(Connection* c, const std::string& actor, const std::string& name, const std::string& developmentId)
{
    for (auto* other : clients_)
        if (other != c && other->entityId == actor)
        {
            lobby(c, false, "That character is already connected. Leave it on the other client first.");
            return false;
        }
    // Undone exactly, if it fails, rather than by copying the whole world first (a 50 ms stall at every entry with
    // DEV's world: doc 31). Only this character changes: its entity and its saved record.
    const auto savedBefore = characters_.find(actor);
    const std::optional<Entity> characterBefore =
        savedBefore == characters_.end() ? std::nullopt : std::optional<Entity>(savedBefore->second);
    const auto undo = [&] {
        world_.removePlayer(actor);
        if (characterBefore)
            characters_[actor] = *characterBefore;
        else
            characters_.erase(actor);
    };
    // Back before a lingering body (doc 33) left the fight: the wolf as it is now, not as it was saved.
    const auto* stayed = world_.entity(actor);
    const bool lingered = stayed && stayed->lingering;
    auto& player = world_.addPlayer(actor, name);
    if (!world_.society().account(actor))
    {
        undo();
        lobby(c, false, "Character entry could not allocate a valid economy account; no world change was saved.");
        return false;
    }
    const auto saved = characters_.find(actor);
    if (lingered)
    {
        player.lingering = false;
        lingering_.erase(actor);
    }
    else if (saved != characters_.end())
        player = saved->second;
    else
    {
        player.description = "A road-worn quadrupedal wolf with a small shoulder satchel. Their coat and history are yours to imagine.";
        player.speakingColor = int(characters_.size() * 9) % 32;
    }
    player.input = {};
    player.velocity = {};
    player.path.clear();
    player.turning = false;
    player.turnTarget = player.facing;
    if (player.posture == "rising")
        player.posture = player.postureTarget;
    player.postureRemaining = 0;
    player.postureTarget.clear();
    player.typing = false;
    player.speakingUntil = 0;
    advanceAge(player, world_.calendarDays());
    characters_[actor] = player;
    world_.observe(actor);
    logEvent("arrival", actor);
    record(Economy | Character, actor);
    if (!storageReady_)
    {
        undo();
        lobby(c, false, "Character entry could not be saved; the world remains closed.");
        return false;
    }
    c->entityId = actor;
    c->developmentIdentity = developmentId;
    c->motionSession = guid();
    c->motionCell.clear();
    c->motionGeneration = 0;
    c->movementMode = FreeMovement;
    c->held.reset();                               // A new session: the client starts with nothing kept.
    auto entered = Value::object();
    entered.add("type", "entered");
    entered.add("id", c->entityId);
    entered.add("motionSession", c->motionSession);
    send(c, entered);
    system(c, "Connected to " + world_.cell(world_.entity(actor)->cellId)->name +
                  ". Enter to write; Shift+Enter for a new line; Escape preserves your draft. "
                  "Dialogue is authored offline unless a local provider is configured.");
    sendSnapshot(c);
    note("info", "RATW_LOGIN " + c->entityId + " connected=" + std::to_string(clients_.size()));
    return true;
}

void Game::leaveCharacter(Connection* c)
{
    const auto id = c->entityId;
    const bool had = !id.empty();
    if (auto* e = world_.entity(id))
    {
        e->typing = false;
        world_.stop(e->id);
        characters_[e->id] = *e;
        logEvent("departure", id);
        if (const auto* fight = world_.battleOf(id); fight && !fight->over)
        {
            // Gone mid-fight: the body stays in it, away, for a minute or until it ends (doc 33), so leaving the
            // world is no way out of a fight.
            world_.linger(id);
            lingering_[id] = world_.time() + battle::LingerSeconds;
        }
        else
            world_.removePlayer(id);
        ++revision_;
    }
    lastMovementSound_.erase(id);
    typingExpiry_.erase(id);
    currentCommands_.erase(id);
    c->entityId.clear();
    c->developmentIdentity.clear();
    if (had)
        record(Economy | Character, id);           // Where they left, kept; the rest with the next snapshot.
}

void Game::releaseLingering()
{
    // A body left in a fight goes once the fight is over, or after a minute; what happened to it is kept.
    for (auto it = lingering_.begin(); it != lingering_.end();)
    {
        auto* e = world_.entity(it->first);
        const auto* fight = world_.battleOf(it->first);
        if (e && e->lingering && fight && !fight->over && world_.time() < it->second)
        {
            ++it;
            continue;
        }
        if (e && e->lingering)
        {
            e->lingering = false;
            characters_[e->id] = *e;
            world_.removePlayer(e->id);
            record(Character, it->first);
            ++revision_;
        }
        it = lingering_.erase(it);
    }
}

bool Game::accountCommand(Connection* c, const Value& j, const std::string& type)
{
    if (type != "auth_register" && type != "auth_login" && type != "character_create" && type != "character_enter" &&
        type != "character_leave" && type != "auth_logout")
        return false;
    if (type == "auth_logout" || type == "character_leave")
    {
        leaveCharacter(c);
        if (type == "auth_logout")
            c->accountUsername.clear();
        lobby(c, storageReady_, storageReady_ ? "Your character has left the world."
                                              : "Storage failed; the character was removed from play, but the latest state could not be saved.");
        return true;
    }
    if (!c->allowsLocalCredentials())
    {
        lobby(c, false, "Account access is disabled for remote peers. Native transport is unencrypted; connect to loopback on this computer.");
        return true;
    }
    if (!storageReady_)
    {
        lobby(c, false, "Account access is unavailable because world storage is not healthy.");
        return true;
    }
    std::set<std::string> allowed = {"type", "commandId"};
    if (type == "auth_register" || type == "auth_login")
        allowed.insert({"username", "password"});
    else if (type == "character_create")
        allowed.insert({"name", "age", "appearance"});
    else if (type == "character_enter")
        allowed.insert("id");
    for (const auto& [key, v] : j.fields())
        if (!allowed.count(key))
        {
            lobby(c, false, "The account request contains unsupported fields.");
            return true;
        }
    if (!c->entityId.empty())
    {
        if (type == "character_enter" && j.string("id") == c->entityId && accounts_.owns(c->accountUsername, c->entityId))
        {
            auto entered = Value::object();
            entered.add("type", "entered");
            entered.add("id", c->entityId);
            entered.add("motionSession", c->motionSession);
            c->held.reset();
            send(c, entered);
            sendSnapshot(c);
            return true;
        }
        system(c, "Leave the current character before changing account or character selection.");
        return true;
    }
    if (type == "auth_register" || type == "auth_login")
    {
        if (!c->accountUsername.empty())
        {
            lobby(c, false, "Log out before signing into another account.");
            return true;
        }
        if (!authRate_.allow(std::to_string(c->id), clock()))
        {
            lobby(c, false, "Too many account attempts. Wait up to a minute before trying again.");
            return true;
        }
        std::string user;
        const auto* userValue = j.find("username");
        const auto* passwordValue = j.find("password");
        if (!userValue || !passwordValue || !userValue->isString() || !passwordValue->isString() ||
            !accounts::normalizeUsername(userValue->asString(), user) || !accounts::validPassword(passwordValue->asString()))
        {
            lobby(c, false, "Use a 3\xe2\x80\x93" "32 character username starting with a letter (letters, digits, _ or -) and a 12\xe2\x80\x93" "128 byte password without control characters.");
            return true;
        }
        for (const auto& [ticket, pending] : signIns_)
            if (pending.c == c)
            {
                lobby(c, false, "Your last request is still being checked.");
                return true;
            }
        // The password is worked on by the hasher's threads (about 0.2 s, on purpose); the answer comes in a later
        // tick (finishSignIns), and nobody else waits for it.
        const bool registering = type == "auth_register";
        accounts::PasswordJob job;
        if (registering)
        {
            std::string error;
            if (!accounts_.mayRegister(user, error))
            {
                lobby(c, false, error);
                return true;
            }
            job.registering = true;
            job.username = user;
            job.password = passwordValue->asString();
        }
        else
            job = accounts_.signInJob(user, passwordValue->asString());
        job.ticket = ++nextSignIn_;
        signIns_[job.ticket] = {c, registering, user};
        hasher_.submit(std::move(job));
        return true;
    }
    if (c->accountUsername.empty() || !accounts_.exists(c->accountUsername))
    {
        lobby(c, false, "Sign in before managing characters.");
        return true;
    }
    if (type == "character_create")
    {
        const std::string name = mind::trim(j.string("name"));
        const double age = wire::strictNumber(j, "age", -1);
        Appearance appearance;
        const std::string commandId = j.string("commandId");
        if (!accounts::validDisplayName(name) || age < 6 || age > 99 || age != std::floor(age) ||
            !wire::readAppearance(j["appearance"], appearance) || !accounts::validCommandId(commandId))
        {
            lobby(c, false, "Choose a 2\xe2\x80\x93" "32 character name, a whole age from 6\xe2\x80\x93" "99, and a complete valid wolf appearance.");
            return true;
        }
        auto canonical = Value::object();
        canonical.add("name", name);
        canonical.add("age", age);
        canonical.add("appearance", wire::appearance(appearance));
        const std::string print = accounts::fingerprint(json::dump(canonical));
        bool conflict = false;
        const std::string previous = accounts_.createdCharacter(c->accountUsername, commandId, print, conflict);
        if (conflict)
        {
            lobby(c, false, "That creation request ID was already used for different choices.");
            return true;
        }
        if (!previous.empty())
        {
            lobby(c, true, "That character was already saved; no duplicate was created.");
            return true;
        }
        if (accounts_.characters(c->accountUsername).size() >= accounts::CharacterSlots)
        {
            lobby(c, false, "This local account already has six characters. Deletion is not available.");
            return true;
        }
        // Undone exactly if it fails (no copy of the whole world first: doc 31). The new character has nothing
        // anywhere yet but what is made here.
        const auto beforeAccounts = accounts_;
        const std::string newId = "wolf-" + guid();
        if (characters_.count(newId) || !accounts_.addCharacter(c->accountUsername, newId, commandId, print))
        {
            lobby(c, false, "Character ownership could not be allocated; please try again.");
            return true;
        }
        auto& player = world_.addPlayer(newId, name);
        if (!world_.society().account(newId))
        {
            world_.removePlayer(newId);
            accounts_ = beforeAccounts;
            lobby(c, false, "Character creation could not allocate a valid economy account; no character was saved.");
            return true;
        }
        player.age = int(age);
        player.lastBirthdayDay = world_.calendarDays();
        player.ageNoticePending = 0;
        player.appearance = appearance;
        player.speakingColor = int(characters_.size() * 9) % 32;
        characters_[player.id] = player;
        world_.removePlayer(newId);
        logEvent("character created", newId);
        record(Accounts | Economy | Character, newId);
        if (!storageReady_)
        {
            characters_.erase(newId);
            accounts_ = beforeAccounts;
            lobby(c, false, "Character creation could not be saved. No character was acknowledged.");
            return true;
        }
        lobby(c, true, "Character saved. Select it to enter the world.");
        return true;
    }
    const std::string id = j.string("id");
    if (!accounts_.owns(c->accountUsername, id) || !characters_.count(id))
    {
        lobby(c, false, "That character is not available on this account.");
        return true;
    }
    enterCharacter(c, id, characters_.at(id).name);
    return true;
}

void Game::login(Connection* c, const Value& j)
{
    if (!options_.devIdentity)
    {
        lobby(c, false, "Development identity entry is disabled. Register or sign in to a local account.");
        return;
    }
    if (!storageReady_)
    {
        lobby(c, false, "World storage is unavailable.");
        return;
    }
    if (!c->entityId.empty())
        return;
    const std::string id = lowerAscii(j.string("id", "ash"));
    if (id.size() < 2 || id.size() > 32)
    {
        system(c, "Development identity must be 2\xe2\x80\x93" "32 letters, digits, hyphens or underscores.");
        return;
    }
    for (char ch : id)
        if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '-' && ch != '_')
        {
            system(c, "Invalid development identity.");
            return;
        }
    std::string name = mind::trim(mind::left(j.string("name", id), 32));
    if (name.empty())
        name = id;
    enterCharacter(c, "player-" + id, name, id);
}

// --------------------------------------------------------------------------- The world, twenty times a second

void Game::tick(double dt)
{
    perf::Scope timed(meter_, perf::TickOther);
    std::map<std::string, std::string> beforeCells;
    for (auto* c : clients_)
        if (const auto* e = world_.entity(c->entityId))
            beforeCells[e->id] = e->cellId;
    {
        perf::Scope world(meter_, perf::World);
        world_.tick(dt);
    }
    if (meter_)
    {
        const auto& last = world_.tickProfile().last;
        meter_->note("streaming=" + perf::fixed(last[0]) + " schedules=" + perf::fixed(last[1]) + " movement=" +
                     perf::fixed(last[2]) + " separation=" + perf::fixed(last[3]) + " views=" + perf::fixed(last[4]));
    }
    ++revision_;
    mind_.poll();                                   // NPC Mind answers that have arrived.
    ambient(dt);
    barks(dt);
    // What happened to players that no action of theirs answered (a bandit's blow, a caravan arriving...).
    for (const auto& [who, words] : world_.takeNotices())
        if (auto* c = clientOf(who))
            system(c, words);
    for (const auto& [id, cell] : beforeCells)
        if (const auto* e = world_.entity(id); e && e->cellId != cell)
            followTransition(id, cell);
    for (auto& [id, until] : typingExpiry_)
        if (until <= world_.time())
            if (auto* e = world_.entity(id))
                e->typing = false;
    companionTick(dt);                              // Residents travelling with a party (doc 32, Phase 3).
    partyTick(dt);
    refreshSocialViews(dt);
    chapterTick(dt);
    factionTick(dt);
    refreshChapterViews(dt);
    refreshLabels(dt);
    snapshotAccumulator_ += dt;
    // Small observer-filtered poses at simulation cadence; full snapshots five times a second, and at once on a
    // change of cell.
    {
        perf::Scope motion(meter_, perf::Motion);
        std::vector<Connection*> framed, arrived;
        for (auto* c : clients_)
            if (const auto* e = world_.entity(c->entityId))
            {
                if (c->motionCell != e->cellId)
                    arrived.push_back(c);
                framed.push_back(c);
            }
        if (!arrived.empty())
        {
            // Into a new cell: its snapshot first, all at once on the pool (a new cell's comes with its tiles).
            perf::Scope view(meter_, perf::Views);
            sendSnapshots(arrived);
        }
        // Each player's frame reads the world and writes only its own client's state: all at once on the pool.
        world_.prepareReading();
        ++frameTick_;
        const auto frameFor = [&](Connection* c) {
            const auto* e = world_.entity(c->entityId);
            // Far wolves in one frame of four for each client (by a phase of its own, so the full ones spread out).
            auto frame = motion::frame(world_, e->id, (frameTick_ + c->id) % 4 == 0);
            stampFrame(c, frame, e->cellId);
            c->motion(frame);
        };
        if (pool_)
            pool_->run(framed.size(), [&](std::size_t i) { frameFor(framed[i]); });
        else
            for (auto* c : framed)
                frameFor(c);
    }
    saveAccumulator_ += dt;
    ambientAccumulator_ += dt;
    if (snapshotAccumulator_ >= 0.2)
    {
        snapshotAccumulator_ = 0;
        movementSounds();
        updateMovementModes();
        releaseLingering();
    }
    // A quarter of the clients' snapshots in each tick (by a phase fixed per connection).
    snapshotPhase_ = (snapshotPhase_ + 1) % SnapshotPhases;
    {
        perf::Scope views(meter_, perf::Views);
        std::vector<Connection*> sending;
        for (auto* c : clients_)
            if (!c->entityId.empty() && c->id % SnapshotPhases == snapshotPhase_)
                sending.push_back(c);
        sendSnapshots(sending);
        for (auto* c : sending)
            if (auto* e = world_.entity(c->entityId))
                e->transitioned = false;
    }
    watchReleases(dt);
    applyDmActions(dt);
    runSpawns(dt);
    makeResidents();
    if (prefetching && (prefetchAccumulator_ += dt) >= 1)
    {
        prefetchAccumulator_ = 0;
        prefetcher().Want(world_.cellsSoonNeeded());
    }
    if (streamedBuild_ && (streamLogAccumulator_ += dt) >= 60)
    {
        streamLogAccumulator_ = 0;
        note("info", "RATW_STREAM loaded=" + std::to_string(world_.loadedCells()) + " of " + std::to_string(world_.cells().size()) +
                         " cells; " + std::to_string(prefetcher().HitCount()) + " loads found ready");
    }
    if (saveSoonIn_ >= 0 && (saveSoonIn_ -= dt) < 0)
    {
        snapshotSaveAccumulator_ = 0;
        autosave();
    }
    if (saveAccumulator_ >= AutosaveSeconds)
    {
        saveAccumulator_ = 0;
        consolidate();
        social_.tick(now());
        afterSocial();
    }
    if ((snapshotSaveAccumulator_ += dt) >= SnapshotSeconds)
    {
        snapshotSaveAccumulator_ = 0;
        autosave();
    }
    finishSignIns();
    {
        perf::Scope timed(meter_, perf::Saves);
        reapSnapshot(false);
        trimStored();
        releaseCommitted();
    }
    if (storageReady_ && director_.enabled())
    {
        std::set<std::string> online;
        for (auto* c : clients_)
            if (!c->entityId.empty())
                online.insert(c->entityId);
        director_.tick(dt, world_, characters_, online, operatorActivity_, revision_,
                       [this] { ++revision_; save(); return storageReady_; },
                       [this](const std::set<std::string>& targets, const std::string& text) {
                           for (auto* c : clients_)
                               if (targets.count(c->entityId))
                                   system(c, text);
                       });
    }
    if (ambientAccumulator_ >= 45)
    {
        ambientAccumulator_ = 0;
        for (const auto& [id, e] : world_.entities())
            if (e.npc && e.cellId == "tavern" && e.posture != "lying" &&
                (!world_.society().resident(id) || world_.society().resident(id)->task != "sleep") &&
                npcLastSpeech_[id] + 30 < world_.time())
            {
                bool heard = false;
                for (auto* c : clients_)
                    if (world_.hearingClarity(c->entityId, id) > 0.5)
                        heard = true;
                if (heard)
                {
                    publish(id, parsePost("The rain has eased along the sill. There may be a clear stretch of road before dusk."), Voice::Speak);
                    npcLastSpeech_[id] = world_.time();
                    break;
                }
            }
    }
}

void Game::movementSounds()
{
    // Audio awareness is separate from map identification: an unseen wolf may be heard without its ID, name or place.
    // Who hears what is worked out for every listener at once (on the pool: doc 31, Phase 4), and told here.
    std::vector<Connection*> listeners;
    for (auto* c : clients_)
    {
        if (c->entityId.empty() || !world_.entity(c->entityId))
            continue;
        const auto prior = lastMovementSound_.find(c->entityId);
        if (prior == lastMovementSound_.end() || world_.time() - prior->second >= 3.0)
            listeners.push_back(c);
    }
    std::vector<double> loudest(listeners.size(), 0);
    world_.prepareReading();
    const auto hear = [&](std::size_t i) {
        const auto& listener = listeners[i]->entityId;
        const auto* me = world_.entity(listener);
        const double range = world_.sightRange(*me);
        double best = 0;
        for (const Entity* e : world_.entitiesIn(me->cellId))   // (Footsteps carry within the cell.)
            if (!e->npc && e->id != listener && e->cellId == me->cellId && world_.visionClarity(*me, *e, range) <= 0)
                best = std::max(best, world_.movementAudibility(listener, e->id));
        loudest[i] = best;
    };
    if (pool_)
        pool_->run(listeners.size(), hear);
    else
        for (std::size_t i = 0; i < listeners.size(); ++i)
            hear(i);
    for (std::size_t i = 0; i < listeners.size(); ++i)
    {
        const double best = loudest[i];
        if (best <= 0)
            continue;
        auto e = Value::object();
        e.add("type", "system");
        e.add("speaker", "Sound");
        e.add("text", best < 0.5 ? "You catch faint pawsteps nearby." : "You hear pawsteps nearby.");
        e.add("anonymous", true);
        e.add("sequence", sequence_++);
        e.add("color", 7);
        send(listeners[i], e);
        lastMovementSound_[listeners[i]->entityId] = world_.time();
    }
}

void Game::stampFrame(Connection* c, Value& root, const std::string& cell)
{
    if (c->motionCell != cell)
    {
        c->motionCell = cell;
        ++c->motionGeneration;
    }
    root.set("motionSession", c->motionSession);
    root.set("cellGeneration", c->motionGeneration);
    root.set("revision", revision_);
    const auto* self = world_.entity(c->entityId);
    root.set("mode", int(c->movementMode));
    root.set("inputAck", self ? double(self->inputSeq) : 0.0);
    root.set("poseAck", self ? double(self->poseSeq) : 0.0);
}

// Snapshots for several clients at once: what a view would change done first, here, so the views themselves only
// read and can be built at once on the pool (doc 31, Phase 4): each observer's memory, the cell index, each client's
// kept rows, the notices.
void Game::sendSnapshots(const std::vector<Connection*>& sending)
{
    std::vector<std::string> due;
    for (auto* c : sending)
        if (!c->entityId.empty())
            due.push_back(c->entityId);
    {
        perf::Scope sight(meter_, perf::Sight);
        world_.prepareViews(due);
        for (auto* c : sending)
        {
            auto* e = world_.entity(c->entityId);
            if (e && e->ageNoticePending > 0)
            {
                system(c, "A birthday has passed. You are now " + std::to_string(e->age) + " years old (" +
                              std::to_string(e->ageNoticePending) + " year" + (e->ageNoticePending == 1 ? "" : "s") +
                              " gained). Your character sheet reflects annual growth and age-related changes.");
                e->ageNoticePending = 0;
            }
            cellRows_[c];
        }
        world_.observeAll(due);
    }
    world_.prepareReading();
    batching_ = true;
    if (pool_)
        pool_->run(sending.size(), [&](std::size_t i) { sendSnapshot(sending[i]); });
    else
        for (auto* c : sending)
            sendSnapshot(c);
    batching_ = false;
}

void Game::sendSnapshot(Connection* c)
{
    const auto id = c->entityId;
    if (!world_.entity(id))
        return;
    const auto view = world_.snapshot(id, false, !batching_);
    auto root = Value::object();
    root.add("time", view.time);
    root.add("revision", revision_);
    stampFrame(c, root, view.cell.id);
    auto self = wire::entity(*world_.entity(id), view.time);
    if (const auto* portrait = portraitOf(id))
    {
        self.set("artwork", portrait->id);           // The owner sees their own at once, and how it stands.
        self.set("artworkStatus", portrait->status);
    }
    {
        const auto points = social_.points.find(id);       // (Read only: views are built in parallel.)
        self.set("socialXp", points == social_.points.end() ? 0 : points->second);
    }
    if (const auto view = socialViews_.find(id); view != socialViews_.end())
        self.set("social", view->second);             // Scene, stars, Stories, title (doc 32, Part 1).
    if (const auto view = chapterViews_.find(id); view != chapterViews_.end())
        self.set("chapter", view->second);            // Their Chapter (doc 32, Part 3).
    self.set("socialLevel", social_.level(id));
    self.set("hearing", view.self.hearing * view.self.earHealth * ageHearingFactor(view.self) * (1.0 + 0.75 * view.self.hearingSkill / 100.0));
    self.set("sneakSkill", view.self.sneakSkill);
    self.set("hearingSkill", view.self.hearingSkill);
    self.set("vision", view.self.vision * view.self.eyeHealth * ageVisionFactor(view.self));
    self.set("smell", view.self.smell * view.self.noseHealth * (1.0 + 0.75 * view.self.scentSkill / 100.0));
    self.set("scentSkill", view.self.scentSkill);
    self.set("noseHealth", view.self.noseHealth);
    wire::privatePace(self, *world_.entity(id));
    const auto* purse = world_.society().account(id);
    self.set("cash", purse ? purse->cash : 0);
    // Crime and law: what the watch wants of this wolf, and how long they are held, if they are.
    if (const auto* w = world_.warrantFor(id))
    {
        auto wanted = Value::object();
        std::string charges;
        for (const auto& inc : world_.crime().incidents)
            if (std::find(w->incidents.begin(), w->incidents.end(), inc.id) != w->incidents.end())
                charges += (charges.empty() ? "" : ", ") + inc.kind;
        wanted.add("town", w->town);
        wanted.add("fine", w->fine);
        wanted.add("owed", world_.owedBy(*w));
        wanted.add("charges", charges);
        self.set("wanted", wanted);
    }
    if (const auto* held = world_.custodyOf(id))
    {
        auto custody = Value::object();
        custody.add("seconds", std::max(0.0, (held->until - world_.calendarDays()) * calendar::SecondsPerDay));
        custody.add("cell", held->cell);
        self.set("custody", custody);
    }
    // What a client walking its own wolf needs to walk it as the server would (doc 31, Phase 3): its speed on flat
    // ground at the pace it can keep, and how much the weather where it stands slows it.
    if (const auto* me = world_.entity(id))
    {
        self.set("walkSpeed", paceSpeed(*me));
        self.set("sightRange", world_.sightRange(*me));    // (For a client shading the terrain itself.)
        self.set("moveFactor", world_.environmentAt(me->cellId, me->position).movement);
    }
    // Health and being Downed (doc 33): how hurt, how long they have, whether they can get up by themselves today.
    if (const auto* me = world_.entity(id))
    {
        self.set("health", std::round(100 - me->hurt));
        self.set("fightingSkill", std::round(me->fightingSkill));
        if (!me->mouth.empty())
            self.set("mouth", me->mouth);
        if (!me->gift.empty())
        {
            self.set("gift", me->gift);
            self.set("quickened", me->quickened);
            self.set("mana", std::floor(me->mana));
            self.set("manaMax", std::round(battle::manaMax(me->wisdom, true)));
        }
        if (const auto* purse = world_.society().account(id); purse && Society::stock(*purse, "sword") > 0)
            self.set("swords", Society::stock(*purse, "sword"));
        if (me->downedLeft > 0)
        {
            self.set("downedLeft", std::round(me->downedLeft));
            self.set("canStruggle", world_.recoveryAvailable(*me) && me->struggleUntil <= 0);
            self.set("struggling", me->struggleUntil > 0);
        }
    }
    self.set("names", namesView(id));                 // Their name and aliases (doc 32).
    // The party (doc 32): its members and where they are, an invitation waiting, a party mate's fight calling.
    if (auto party = partyView(id); !party.isNull())
        self.set("party", std::move(party));
    root.add("self", self);
    auto cell = Value::object();
    cell.add("id", view.cell.id);
    cell.add("name", view.cell.name);
    cell.add("description", view.cell.description);
    cell.add("width", view.cell.width);
    cell.add("height", view.cell.height);
    // Where the cell lies in the world, in tiles: the client keeps its camera steady across a crossing with it.
    cell.add("x", view.cell.worldX);
    cell.add("y", view.cell.worldY);
    cell.add("z", view.cell.worldZ);
    cell.add("outdoors", view.cell.outdoors);
    cell.add("seasonalWeather", view.cell.seasonalWeather);
    cell.add("weather", weatherName(view.cell.weather));
    cell.add("wind", wire::wind(view.cell.wind));
    cell.add("environment", wire::environment(view.environment));
    {
        // The regional weather (doc 29, phase 7): what it is where the wolf stands, and a coarse grid of the field over
        // the cell for drawing (one sample each 16 tiles: a letter for the kind, a digit 0-9 for the strength).
        auto local = Value::object();
        local.add("kind", weatherName(view.environment.weather));
        local.add("intensity", std::round(view.environment.intensity * 100) / 100);
        cell.add("localWeather", local);
        int cols = 0, rows = 0;
        const auto grid = world_.weatherGrid(view.cell.id, 16, cols, rows);
        if (!grid.empty())
        {
            std::string kinds, amounts;
            for (const auto& s : grid)
            {
                static const char letters[] = {'c', 'r', 'f', 'n', 'o', 's', 'd'};
                const int k = std::clamp(int(s.kind), 0, int(sizeof letters) - 1);
                kinds += letters[k];
                amounts += char('0' + std::clamp(int(std::lround(s.intensity * 9)), 0, 9));
            }
            auto field = Value::object();
            field.add("step", 16);
            field.add("cols", cols);
            field.add("rows", rows);
            field.add("kinds", kinds);
            field.add("amounts", amounts);
            cell.add("weatherField", field);
        }
    }
    {
        // What kind of day it is here (Phase 9): an ordinary one, Marketday, Restday, or a festival and its name.
        const auto plan = world_.dayPlan(world_.communityOf(view.cell.id));
        auto day = Value::object();
        day.add("kind", plan.kind);
        day.add("name", plan.name);
        day.add("foul", plan.foul);
        cell.add("day", day);
    }
    root.add("senses", wire::senses(view));
    // The cell as rows of text, one character a tile: its glyph (a space where the wolf knows nothing, its memory of
    // it where it remembers), whether it is visible now ('2'), remembered ('1') or unknown ('0'), and its height (level
    // ground where not seen now). Built again only when what the wolf sees or remembers, or the cell, has changed.
    auto& kept = batching_ ? cellRows_.find(c)->second : cellRows_[c];   // (Made beforehand when built in parallel.)
    const auto* source = view.source;
    if (!source || kept.cell != view.cell.id || kept.tiles != source->tiles.data() || kept.visible != view.visibleTiles ||
        kept.remembered != view.rememberedTiles)
    {
        auto rows = Value::array(), visibility = Value::array(), heights = Value::array();
        const int width = view.cell.width;
        const auto& tiles = source ? source->tiles : view.cell.tiles;
        for (int y = 0; y < view.cell.height; ++y)
        {
            std::string row, seen, height;
            row.reserve(std::size_t(width));
            seen.reserve(std::size_t(width));
            height.reserve(std::size_t(width));
            for (int x = 0; x < width; ++x)
            {
                const auto i = std::size_t(y * width + x);
                const bool known = i < tiles.size();
                const bool visible = known && i < view.visibleTiles.size() && view.visibleTiles[i];
                const bool remembered = known && i < view.rememberedTiles.size() && view.rememberedTiles[i];
                const char memoryGlyph = view.memory && i < view.memory->glyphs.size() ? view.memory->glyphs[i] : ' ';
                row += visible ? tiles[i].glyph : remembered ? (source ? memoryGlyph : tiles[i].glyph) : ' ';
                seen += visible ? '2' : remembered ? '1' : '0';
                height += visible ? wire::heightChar(tiles[i].height) : wire::heightChar(0);
            }
            rows.push(row);
            visibility.push(seen);
            heights.push(height);
        }
        kept.cell = view.cell.id;
        kept.tiles = source ? static_cast<const void*>(source->tiles.data()) : nullptr;
        kept.visible = view.visibleTiles;
        kept.remembered = view.rememberedTiles;
        kept.rows = rows;
        kept.visibility = visibility;
        kept.heights = heights;
    }
    cell.add("rows", kept.rows);
    cell.add("heights", kept.heights);
    if (!c->clientSight)                           // (It works out what it sees itself: Phase 4.6.)
        root.add("visibility", kept.visibility);
    root.add("cell", cell);
    const auto near = [&](const Entity& e) {
        return std::hypot(e.position.x - view.self.position.x, e.position.y - view.self.position.y) <= 3;
    };
    auto entities = Value::array();
    const auto relations = relationsFor(id);
    std::map<std::string, std::string> enemyFactions;
    if (const auto* mine = chapters_.of(id))
    {
        for (const auto& h : mine->hostiles)
            if (h.kind == "faction")
                enemyFactions[h.target.substr(8)] = "hostile to your Chapter" + (h.reason.empty() ? std::string() : ": " + h.reason);
        for (const auto& [fid, f] : factions_.all())
            if (factions_.stanceOf(fid, mine->id, chapterMembers(mine->id)) == "war")
                enemyFactions[fid] = f.name + " is at war with your Chapter";
    }
    // What this wolf calls each one it sees (doc 32): a name it was given, else how they look. Two strangers who look
    // alike are told apart by number, in a fixed order.
    std::map<std::string, std::string> called;
    {
        std::map<std::string, std::vector<std::string>> alike;
        for (const auto& e : view.entities)
        {
            called[e.id] = labelFor(id, e.id);
            if (!knowsName(id, e.id))
                alike[called[e.id]].push_back(e.id);
        }
        for (auto& [label, ids] : alike)
            if (ids.size() > 1)
            {
                std::sort(ids.begin(), ids.end());
                for (std::size_t n = 1; n < ids.size(); ++n)
                    called[ids[n]] = label + " (" + std::to_string(n + 1) + ")";
            }
    }
    for (const auto& e : view.entities)
    {
        auto j = wire::entity(e, view.time);
        if (e.id != id)
        {
            j.set("name", names::capitalised(called[e.id]));
            if (options_.hiddenNames && !knowsName(id, e.id))
                j.set("known", false);
        }
        // Who they are to this wolf (doc 32): a party mate, a Chapter mate, or hostile (and why). Bandits always are.
        if (relations.mates.count(e.id))
            j.set("rel", "party");
        else if (relations.chapterMates.count(e.id))
        {
            j.set("rel", "chapter");
            j.set("colour", relations.colour);
        }
        else if (const auto* member = e.npc ? factions_.memberOf(e.id) : nullptr; member && enemyFactions.count(member->first))
        {
            j.set("rel", "hostile");                  // A faction at war with their Chapter, or marked hostile by it (doc 32, 2.4).
            j.set("why", enemyFactions.at(member->first));
        }
        else if (const auto found = relations.hostile.find(e.id); found != relations.hostile.end())
        {
            j.set("rel", "hostile");
            j.set("why", found->second);
        }
        else if (e.transient && world_.hostile(e.id))
        {
            j.set("rel", "hostile");
            j.set("why", "bandit");
        }
        if (!e.npc)
            if (const auto portrait = visiblePortrait(e.id, id); !portrait.empty())
                j.set("artwork", portrait);         // Approved portraits only (doc 29, phase 9).
        auto actions = Value::array();
        actions.push("inspect");
        if (e.transient)
        {
            // Folk of the road: a caravan is only to be looked at; bandits are paid off or fought.
            if (world_.hostile(e.id))
            {
                j.set("hostile", true);
                if (world_.banditDemand(view.self.id) > 0)
                    actions.push("pay");
                if (std::hypot(e.position.x - view.self.position.x, e.position.y - view.self.position.y) <= battle::StartReach)
                    actions.push("attack");
            }
        }
        else if (e.npc)
        {
            // What they do, for the In Sight list: their post's title, or the trade they were written with.
            if (const auto* post = world_.society().jobOf(e.id); post && !post->title.empty())
                j.set("work", post->title);
            else if (const auto* spec = world_.society().spec(e.id); spec && !spec->workLabel.empty() && spec->workLabel != "-")
                j.set("work", spec->workLabel);
            actions.push("talk");
            if (world_.society().merchant(e.id))
                actions.push("trade");
            // Crime and law: a close resident can be robbed or struck (never killed: they are beaten down); the
            // watch takes reports, and fines from the wanted.
            const auto* post = world_.society().jobOf(e.id);
            const bool guard = post && post->role == "guard";
            const bool reach = std::hypot(e.position.x - view.self.position.x, e.position.y - view.self.position.y) <= 2;
            if (!e.dead && reach && e.state != "beaten down" && e.downedLeft <= 0)
            {
                actions.push("steal");
                actions.push("attack");
            }
            if (guard && near(e) && !e.dead)
            {
                actions.push("report");
                if (world_.warrantFor(view.self.id))
                    actions.push("pay fine");
            }
            // Residents who might travel with the party (doc 32, Phase 3), and orders for those who do.
            if (near(e) && !parties_.of(e.id))
                if (const auto* mine = parties_.of(view.self.id); (!mine || mine->leader == view.self.id) && whyNotJoin(e.id).empty())
                {
                    actions.push("ask to join");
                    actions.push("hire for " + std::to_string(wageFor(e.id)) + "p a day");
                }
            if (const auto* c = parties_.companion(e.id); c && parties_.together(view.self.id, e.id))
            {
                actions.push(c->waiting ? "follow me" : "wait here");
                actions.push("go home");
                actions.push("dismiss");
            }
            // Merchants know what work is going in town (contracts), and a player can take it on from them.
            if (world_.society().merchant(e.id) && near(e))
            {
                const auto work = world_.contractsNear(view.self.id);
                if (!work.empty())
                    actions.push("ask for work");
                for (std::size_t i = 0; i < work.size() && i < 3; ++i)
                    actions.push("take " + work[i]->id);
            }
            // Ask to learn their trade: close by, a master with no apprentice, and not already learning one.
            if (const auto* job = world_.society().jobOf(e.id);
                job && job->role != "guard" && !e.dead && world_.society().state().careers.positions.at(job->id).apprentice.empty() &&
                !world_.society().apprenticedTo(view.self.id) && near(e))
                actions.push("apprentice");
        }
        // Fights (doc 33): a player close by may be challenged; anyone lying Downed close by (out of a fight) tended.
        const double apart = std::hypot(e.position.x - view.self.position.x, e.position.y - view.self.position.y);
        if (!e.npc && e.id != view.self.id && !e.dead && e.downedLeft <= 0 && apart <= battle::StartReach)
            actions.push("challenge");
        // Introductions (doc 32): to anyone close who doesn't know this wolf's name, by any of its names.
        if (options_.hiddenNames && e.id != view.self.id && !e.transient && !e.dead && apart <= 6 && !knowsName(e.id, view.self.id))
        {
            actions.push("introduce");
            if (const auto found = aliases_.find(view.self.id); found != aliases_.end())
                for (const auto& alias : found->second)
                    actions.push("introduce as " + alias);
        }
        // A faction's officials (doc 32, Part 4): standing, reports, tithes, missions, news of an expulsion.
        if (e.npc && near(e) && chapters_.of(view.self.id))
            if (const auto factionId = officialOf(e.id); !factionId.empty())
            {
                actions.push("ask about our standing");
                actions.push("pay for a report (5p)");
                actions.push("ask for missions");
                actions.push("give a tithe (20p)");
                if (!factions_.stillCounted(factionId, chapters_.of(view.self.id)->id).empty())
                    actions.push("tell of an expulsion");
            }
        for (const auto& m : factions_.missions())
            if (m.state == "taken" && m.taker == view.self.id && m.kind != "guard" && (m.kind == "message" ? m.to : m.official) == e.id && near(e))
                actions.push("deliver " + m.id);
        // Chapters (doc 32, Part 3): an Officer invites players, and marks anyone hostile to the Chapter.
        if (e.id != view.self.id)
            if (const auto* member = chapters_.member(view.self.id); member && member->rank <= chapter::RankOfficer)
            {
                if (!e.npc && !chapters_.of(e.id))
                    actions.push("invite to chapter");
                if (!relations.chapterMates.count(e.id))
                {
                    bool marked = false;
                    for (const auto& h : chapters_.of(view.self.id)->hostiles)
                        marked |= h.target == e.id;
                    actions.push(marked ? "unmark hostile" : "mark hostile");
                }
            }
        // Parties (doc 32): any player in sight may be invited, by one in no party or who leads theirs.
        if (!e.npc && e.id != view.self.id && !relations.mates.count(e.id))
            if (const auto* mine = parties_.of(view.self.id); !mine || mine->leader == view.self.id)
                actions.push("invite");
        if (e.id != view.self.id && !e.dead && e.downedLeft > 0 && apart <= 2 && !world_.inBattle(e.id))
            actions.push("tend");
        if (e.downedLeft > 0)
            j.set("downed", true);
        j.set("actions", actions);
        entities.push(j);
    }
    root.add("entities", entities);
    auto doors = Value::array();
    for (const auto& d : view.doors)
    {
        auto j = Value::object();
        j.add("id", d.id);
        j.add("name", d.name);
        j.add("x", d.position.x);
        j.add("y", d.position.y);
        j.add("open", d.open);
        j.add("portal", d.portal);
        auto actions = Value::array();
        for (const auto& a : world_.actions(id, d.id))
            actions.push(a);
        j.add("actions", actions);
        doors.push(j);
    }
    root.add("doors", doors);
    auto map = Value::array();
    for (const auto& m : view.worldMap)
        map.push(wire::mapCell(m));
    root.add("worldMap", map);
    auto travelMap = Value::array();
    for (const auto& m : world_.travelMap(id))
        travelMap.push(wire::mapCell(m, false));
    root.add("travelMap", travelMap);
    root.add("travel", wire::travel(world_.travelState(id)));
    // Fights (doc 33): the arena, for a fighter or a watcher; the red squares, for anyone who can see one; and a
    // challenge waiting for an answer.
    if (const auto* fight = world_.battleOf(id) ? world_.battleOf(id) : world_.watching(id))
        root.add("battle", battleView(*fight, id));
    if (auto fights = fightsInView(view.self); fights.size() > 0)
        root.add("fights", fights);
    // Things lying on the ground nearby (a sword knocked loose in a fight).
    {
        auto lying = Value::array();
        for (const auto& g : world_.groundItems())
            if (g.cellId == view.self.cellId && std::hypot(g.x - view.self.position.x, g.y - view.self.position.y) <= world_.sightRange(view.self))
            {
                auto o = Value::object();
                o.add("id", g.id);
                o.add("item", g.item);
                o.add("x", g.x);
                o.add("y", g.y);
                o.add("near", std::hypot(g.x - view.self.position.x, g.y - view.self.position.y) <= 1.8);
                lying.push(o);
            }
        if (lying.size() > 0)
            root.add("ground", lying);
    }
    if (const auto* challenge = world_.challengeTo(id))
        if (const auto* from = world_.entity(challenge->from))
        {
            auto o = Value::object();
            o.add("from", from->id);
            o.add("name", names::capitalised(labelFor(id, from->id)));
            o.add("left", std::max(0.0, challenge->until - world_.time()));
            root.add("challenge", o);
        }
    root.add("isometric", view.isometric);
    root.add("connection", options_.connectionLabel);
    root.add("dialogueProvider", mind_.label());
    auto inventory = Value::array();
    const auto item = [&](const char* itemId, const char* name, const char* icon, const char* description, bool equipped, int quantity) {
        auto i = Value::object();
        i.add("id", itemId);
        i.add("name", name);
        i.add("icon", icon);
        i.add("description", description);
        i.add("equipped", equipped);
        i.add("quantity", quantity);
        inventory.push(i);
    };
    item("satchel", "Shoulder satchel", "bag", "A small travel bag made for a wolf's shoulders.", true, 1);
    if (purse)
    {
        const int herbs = Society::stock(*purse, "herbs"), meals = Society::stock(*purse, "meal");
        if (herbs > 0)
            item("herbs", "Cooking herbs", "herb", "Finite ingredients. Sell to a trader who needs supplies.", false, herbs);
        if (meals > 0)
            item("meal", "Prepared meal", "food", "Consume one to restore 10 stamina. Cooking uses real ingredients.", false, meals);
        if (const int swords = Society::stock(*purse, "sword"); swords > 0)
            item("sword", "Dull bronze sword", "weapon",
                 "An old bronze blade, its edge long gone, carried in the jaws. In a fight it reaches two tiles and hits hard, "
                 "but tires you.",
                 view.self.mouth == "sword", swords);
    }
    item("token", "Wooden token", "token", "A smooth keepsake carved with a branch.", false, 1);
    root.add("inventory", inventory);
    const Entity* trader = nullptr;
    double traderAt = 1e18;
    for (const Entity* e : world_.entitiesIn(view.self.cellId))   // (Its own cell's: doc 31, Phase 4.)
        if (e->cellId == view.self.cellId && world_.society().merchant(e->id))
            if (const double d = std::hypot(e->position.x - view.self.position.x, e->position.y - view.self.position.y); d < traderAt)
            {
                trader = e;                         // The nearest (a smith and a shopkeeper may share a square).
                traderAt = d;
            }
    const auto* traderLife = trader ? world_.society().resident(trader->id) : nullptr;
    if (trader && purse && trader->posture != "lying" && (!traderLife || traderLife->task != "sleep") &&
        world_.visionClarity(id, trader->id) > 0 &&
        std::hypot(trader->position.x - view.self.position.x, trader->position.y - view.self.position.y) <= 2)
        if (const auto* account = world_.society().account(trader->id))
        {
            auto m = Value::object();
            m.add("id", trader->id);
            m.add("name", names::capitalised(labelFor(id, trader->id)));
            m.add("cash", account->cash);
            auto goods = Value::array();
            for (const auto& ware : world_.society().wares(trader->id))
            {
                const char* itemId = ware.c_str();
                auto good = Value::object();
                good.add("id", itemId);
                good.add("name", Society::itemName(itemId));
                good.add("stock", Society::stock(*account, itemId));
                good.add("owned", Society::stock(*purse, itemId));
                const auto buy = world_.society().quote(id, trader->id, itemId, 1, true);
                const auto sell = world_.society().quote(id, trader->id, itemId, 1, false);
                good.add("buyPrice", buy.unitPrice);
                good.add("sellPrice", sell.unitPrice);
                good.add("canBuy", buy.ok);
                good.add("canSell", sell.ok);
                good.add("buyReason", buy.message);
                good.add("sellReason", sell.message);
                goods.push(good);
            }
            m.add("items", goods);
            root.add("merchant", m);
        }
    const auto patch = world_.herbPatchPosition();
    const int patchX = int(patch.x), patchY = int(patch.y);
    if (world_.society().state().enabled && view.self.cellId == world_.herbPatchCell() && view.cell.width > patchX &&
        view.cell.height > patchY && std::size_t(patchY * view.cell.width + patchX) < view.visibleTiles.size() &&
        view.visibleTiles[std::size_t(patchY * view.cell.width + patchX)])
    {
        auto resource = Value::object();
        resource.add("id", "herb_patch");
        resource.add("x", patch.x);
        resource.add("y", patch.y);
        resource.add("remaining", world_.society().state().herbPatch);
        root.add("resource", resource);
    }
    auto memory = Value::object();
    int turns = 0, count = 0;
    double due = 0;
    for (const auto& [key, m] : memories_.active)
        if (m.subject == id)
        {
            turns += int(m.turns.size());
            due = std::max(due, m.lastActivity + 3600 - now());
        }
    for (const auto& s : memories_.summaries)
        if (s.subject == id)
            ++count;
    memory.add("activeTurns", turns);
    memory.add("summaries", count);
    memory.add("nextConsolidationSeconds", due);
    root.add("memory", memory);
    root.add("persistenceHealthy", storageReady_);
    root.add("devTools", options_.devTools);
    // Leave out what this client already holds (RatwSections.h), and remember what this one carries.
    finishSnapshot(c, std::move(root), double(revision_));
}

// What the client holds left out, written, sent: the client's own state alone, so many can go at once on the pool.
void Game::finishSnapshot(Connection* c, json::Value root, double revision)
{
    if (!options_.fullSnapshots)
        c->held.sending(revision, sections::strip(root, c->held.known, &c->held.bases));
    c->snapshotValue(root);
}


std::vector<std::string> Game::publish(const std::string& author, const ParsedPost& post, Voice voice, const std::vector<std::string>& to,
                                       const std::string& group)
{
    auto* speaker = world_.entity(author);
    if (!speaker)
        return {};
    const std::uint64_t event = sequence_++;
    if (post.speech)
        speaker->speakingUntil = world_.time() + 4;
    std::vector<std::string> heard;
    for (auto* c : clients_)
    {
        if (c->entityId.empty())
            continue;
        const auto& listener = c->entityId;
        auto sense = world_.perceive(listener, author, voice);
        if (listener == author)
            sense = {1, 1, true};
        const auto segments = perceivePost(post, sense.hearing, sense.vision, event * 7919 + std::hash<std::string>{}(listener));
        if (segments.empty())
            continue;
        heard.push_back(listener);
        auto e = Value::object();
        e.add("type", "roleplay");
        e.add("channel", "ic");
        e.add("id", event);
        e.add("sequence", event);
        e.add("color", speaker->speakingColor);
        e.add("anonymous", !sense.identifiable);
        // By the name the listener knows them by, or as they look (doc 32).
        e.add("speaker", sense.identifiable ? names::capitalised(labelFor(listener, author)) : std::string("A voice"));
        if (post.speech && !speaker->mouth.empty())
            e.add("muffled", true);                // Words around a sword held in the jaws (doc 33).
        // Deliberately no author ID: even anonymous records can't be tied to hidden actors.
        auto output = Value::array();
        std::string text;
        for (const auto& segment : segments)
        {
            auto j = Value::object();
            j.add("kind", segment.kind);
            j.add("text", segment.text);
            output.push(j);
            if (!text.empty())
                text += ' ';
            text += segment.text;
        }
        e.add("segments", output);
        e.add("text", text);
        // An introduction heard ("I'm Kestrel"): the listener knows them by that name now (doc 32).
        {
            std::string spoken;
            for (const auto& segment : segments)
                if (segment.kind == "speech")
                    spoken += (spoken.empty() ? "" : " ") + segment.text;
            if (sense.identifiable)
                noticeIntroduction(author, listener, spoken);
        }
        // Said to the party or Chapter: its members who hear it are told so; anyone else overhears it as plain speech.
        if (!group.empty() && (listener == author || (group == "party" ? parties_.together(author, listener) : chapters_.together(author, listener))))
            e.add(group, true);
        if (!to.empty())
        {
            // Whom it was meant for, as this listener can tell: "you", a name they can see, or "someone".
            auto names = Value::array();
            for (const auto& whom : to)
                if (whom == listener)
                    names.push("you");
                else if (const auto* w = world_.entity(whom); w && (whom == author || world_.visionClarity(listener, whom) > 0))
                    names.push(labelFor(listener, whom));
                else
                    names.push("someone");
            e.add("to", names);
        }
        send(c, e);
    }
    sendIntroductionReceipt(author);
    return heard;
}

void Game::logEvent(const char* kind, const std::string& actor, const std::string& target, const std::string& detail)
{
    WorldEvent e;
    e.kind = kind;
    e.actor = actor;
    e.target = target;
    e.detail = detail;
    world_.recordEvent(std::move(e));
}

void Game::followTransition(const std::string& id, const std::string& previousCell)
{
    const auto* leader = world_.entity(id);
    if (!leader)
        return;
    // A party's residents cross with whoever they follow: the leader, or the one in the world (doc 32).
    const auto* p = parties_.of(id);
    if (!p || (p->leader != id && clientOf(p->leader)))
        return;
    for (const auto& c : p->companions)
    {
        auto* npc = world_.entity(c.id);
        if (npc && !c.waiting && npc->cellId == previousCell && !world_.inBattle(c.id) && npc->downedLeft <= 0)
        {
            npc->cellId = leader->cellId;
            npc->position = leader->position;
            world_.stop(npc->id);
        }
    }
}

// --------------------------------------------------------------------------- NPC conversation

namespace
{
// Whether `text` names `name` as a whole word: "ash, a word?" addresses Ash; "washing ashes" does not.
bool namesWord(const std::string& text, const std::string& name)
{
    if (name.empty())
        return false;
    const auto letter = [](char ch) { return std::isalnum(static_cast<unsigned char>(ch)) || ch == '\'' || ch == '_'; };
    for (auto at = text.find(name); at != std::string::npos; at = text.find(name, at + 1))
    {
        const auto end = at + name.size();
        if ((at == 0 || !letter(text[at - 1])) && (end >= text.size() || !letter(text[end])))
            return true;
    }
    return false;
}
} // namespace

bool Game::talk(const std::string& npcId, const std::string& playerId, const std::string& heardText, Voice voice,
                const SensoryResult* perceived, const std::string& alsoHeard)
{
    auto* npc = world_.entity(npcId);
    auto* player = world_.entity(playerId);
    if (!npc || !player || !npc->npc)
        return false;
    if (pendingNpc_.count(npcId))
    {
        // Still answering: remember this to answer next. A crowd talking over each other keeps only the latest few.
        auto& queue = queuedTalk_[npcId];
        queue.push_back({playerId, heardText, voice, perceived != nullptr, perceived ? *perceived : SensoryResult{}});
        while (queue.size() > MaxQueuedTalk)
            queue.pop_front();
        return true;
    }
    const auto sense = perceived ? *perceived : world_.perceive(npcId, playerId, voice);
    if (sense.hearing <= 0 && sense.vision <= 0)
        return false;
    const bool identified = sense.identifiable;
    const std::string subjectId = identified ? playerId : "unidentified-voice-" + std::to_string(sequence_);
    auto context = dialogueContext(npcId, playerId, heardText, identified);
    if (!alsoHeard.empty())
        context.scene += " Others were spoken to at the same time, and just answered: " + mind::left(alsoHeard, 600);
    memories_.record(npcId, subjectId, {sequence_++, now(), context.playerName, heardText});
    // That they talked, never what was said (that stays in the NPC's memory); an unrecognised voice stays anonymous.
    logEvent("conversation", identified ? playerId : std::string(), npcId);
    pendingNpc_.insert(npcId);
    replyingTo_[npcId] = identified ? playerId : std::string();
    // Thinking: the player sees the NPC's "..." until the reply comes (or half a minute passes).
    npc->typing = true;
    typingExpiry_[npcId] = world_.time() + 30;
    saveSoon();
    std::weak_ptr<bool> alive = alive_;
    // What the game can answer itself (a greeting, a price, the hours, a way: doc 28) it does, without a model.
    if (const auto answer = gameAnswer(npcId, playerId, heardText, identified); !answer.empty())
    {
        if (!mind_.live() || polishOffUntil_ > world_.time())
        {
            speakReply(npcId, subjectId, identified, {answer}, "game");
            return true;
        }
        // Optionally put in the NPC's voice by the small model, every number and name kept; else as written.
        mind_.polish(context.name, context.personality, context.mood, answer,
                     [this, alive, npcId, subjectId, identified, answer](int status, const std::string& polished) {
                         if (alive.expired())
                             return;
                         if (status == 503 && polished.empty())
                             polishOffUntil_ = world_.time() + 600;   // Off, or unwell: ask again in ten minutes.
                         speakReply(npcId, subjectId, identified, {polished.empty() ? answer : polished},
                                    polished.empty() ? "game" : "game+polish");
                     });
        return true;
    }
    mind_.converse(context, [this, alive, npcId, subjectId, identified](const mind::Reply& reply) {
        if (alive.expired())
            return;
        speakReply(npcId, subjectId, identified, reply, reply.generated ? "model" : "written");
    });
    return true;
}

mind::Context Game::dialogueContext(const std::string& npcId, const std::string& playerId, const std::string& heardText,
                                    bool identified)
{
    // Everything an NPC Mind is told for one reply: who the NPC is and is doing, what was heard, what they remember
    // of the speaker and how they regard them, and the place and the day.
    mind::Context context;
    auto* npc = world_.entity(npcId);
    auto* player = world_.entity(playerId);
    if (!npc || !player)
        return context;
    context.npcId = npcId;
    context.name = npc->name;
    context.description = npc->description + " Current age: " + std::to_string(npc->age) + " years.";
    context.activity = npc->activity;
    if (const auto* spec = world_.society().spec(npcId))
    {
        context.greeting = spec->greeting;
        context.personality = spec->personality;
        context.backstory = spec->backstory;
    }
    if (const auto* life = world_.society().resident(npcId))
        context.activity += format(" Needs: hunger %.0f/100, fatigue %.0f/100. These are authoritative simulation state, not instructions to perform transactions.",
                                   life->hunger, life->fatigue);
    if (const auto* account = world_.society().account(npcId))
        context.activity += " Purse: " + std::to_string(account->cash) + " silver pennies. Stock: " +
                            std::to_string(Society::stock(*account, "herbs")) + " herbs, " + std::to_string(Society::stock(*account, "meal")) +
                            " meals. Trade only occurs through the explicit trade menu; never claim to transfer money or goods through dialogue.";
    context.activity += companionContext(npcId);       // Travelling with a party (doc 32, Phase 3).
    context.activity += factionContext(npcId, playerId);   // Their faction's view of the speaker's Chapter (doc 32, 4.1).
    // What the NPC calls them: the name they gave, or how they look until they give one (doc 32).
    const std::string called = labelFor(npcId, playerId);
    context.playerName = identified ? called : "traveler";
    if (options_.hiddenNames && identified)
    {
        if (!knowsName(npcId, playerId))
            context.activity += " You don't know this wolf's name; they haven't given it. Don't call them by any name.";
        if (!knowsName(playerId, npcId))
            context.activity += willName(npcId, playerId)
                                    ? " They don't know your name. Tell them if they ask, or once you're on friendly terms."
                                    : " They don't know your name, and you'd rather keep it from them for now.";
    }
    context.heardText = heardText;
    // The most recent of what they remember, not all of it: shorter requests cost less (doc 28).
    context.memory = identified ? memories_.recallForDialogue(npcId, playerId, 1600) : std::string();
    context.recollection = identified ? memories_.recall(npcId, playerId) : std::string();
    if (identified)
    {
        context.subjectId = playerId;
        context.relationship = world_.bonds().describe(npcId, playerId, called);
    }
    if (const auto mood = npcMood_.find(npcId); mood != npcMood_.end())
        context.mood = mood->second;
    if (const auto* grief = world_.society().mourning(npcId))
    {
        const auto* lost = world_.entity(grief->whom);
        context.activity += " Grieving for " + (lost ? lost->name : std::string("someone close")) + ", who died recently.";
        if (context.mood.empty())
            context.mood = "sad";
    }
    if (const auto* job = world_.society().jobOf(npcId))
    {
        const double skill = world_.society().skill(npcId, job->id);
        context.activity += " Trade: " + job->title + " (" +
                            (skill >= 80 ? "a master of it" : skill >= 60 ? "skilled" : skill >= 35 ? "capable" : "still learning") + ").";
    }
    if (identified)
    {
        if (const auto open = world_.promisesBetween(npcId, playerId, called); !open.empty())
            context.relationship += (context.relationship.empty() ? "" : " ") + open;
        if (const auto heard = world_.rumoursAbout(npcId, playerId, called); !heard.empty())
            context.relationship += (context.relationship.empty() ? "" : " ") + heard;
    }
    if (const auto* cell = world_.cell(npc->cellId))
    {
        context.environment = wire::environmentDescription(*cell, world_.environmentAt(npc->cellId));
        context.scene = mind::left(cell->description, 3300) + " Current local conditions: " + context.environment;
        // What day it is (Phase 9), and what the NPC is about: at the market, keeping a festival, resting.
        const auto plan = world_.dayPlan(world_.communityOf(npc->cellId));
        std::string day = "Today is " + calendar::weekdayName(calendar::weekdayOf(world_.calendarDays()));
        if (plan.kind == "festival")
            day += ", and " + plan.name + " (a festival)" + (plan.foul ? ", kept indoors for the weather" : "");
        else if (plan.kind == "market")
            day += plan.foul ? ": market day, but the weather keeps the stalls away" : ": market day, with stalls at the market";
        else if (plan.kind == "rest")
            day += ", the day of rest";
        context.scene += " " + day + ".";
    }
    return context;
}

void Game::speakReply(const std::string& npcId, const std::string& subjectId, bool identified, const mind::Reply& reply,
                      const char* route)
{
    pendingNpc_.erase(npcId);
    auto* npc = world_.entity(npcId);
    if (!npc)
        return;
    npc->typing = false;
    typingExpiry_.erase(npcId);
    ParsedPost post;
    post.ok = true;
    post.speech = true;
    post.segments.push_back({"speech", reply.text});
    std::vector<std::string> to;
    if (const auto whom = replyingTo_.find(npcId); whom != replyingTo_.end())
    {
        if (!whom->second.empty())
            to.push_back(whom->second);
        replyingTo_.erase(whom);
    }
    publish(npcId, post, Voice::Speak, to);
    npcSpokeTo(npcId, subjectId);
    npcLastSpeech_[npcId] = world_.time();
    memories_.record(npcId, subjectId, {sequence_++, now(), npcId, reply.text});
    logEvent("conversation", npcId, identified ? subjectId : std::string());
    heed(npcId, subjectId, identified, reply);
    voiced("dialogue", route, npcId);
    saveSoon();
    // Spoken to with others: the next one answers now, having heard this.
    if (const auto chain = chainAfter_.find(npcId); chain != chainAfter_.end())
    {
        TalkChain next = std::move(chain->second);
        chainAfter_.erase(chain);
        next.said += npc->name + ": \"" + mind::left(reply.text, 200) + "\" ";
        continueChain(std::move(next));
    }
    talkNext(npcId);
}

void Game::continueChain(TalkChain chain)
{
    while (!chain.rest.empty())
    {
        const TalkTurn turn = std::move(chain.rest.front());
        chain.rest.pop_front();
        // Kept before asking: a reply the game gives at once continues the chain from inside talk().
        if (!chain.rest.empty())
            chainAfter_[turn.npcId] = chain;
        if (talk(turn.npcId, chain.playerId, turn.heardText, chain.voice, &turn.sense, chain.said))
            return;
        if (const auto kept = chainAfter_.find(turn.npcId); kept != chainAfter_.end())
            chainAfter_.erase(kept);              // Couldn't take it up: the next one in line answers instead.
    }
}

void Game::consolidate()
{
    // Conversations quiet for an hour become permanent summaries at once; with the NPC Mind running, each is then
    // summarised properly from the NPC's point of view, replacing the extractive one when it arrives.
    const double at = now();
    const auto closing = memories_.due(at);
    memories_.consolidate(at);
    std::weak_ptr<bool> alive = alive_;
    for (const auto& conversation : closing)
    {
        const auto* npc = world_.entity(conversation.npc);
        const std::string name = npc ? npc->name : conversation.npc;
        std::vector<std::pair<std::string, std::string>> turns;
        for (const auto& t : conversation.turns)
            turns.emplace_back(t.who == conversation.npc ? name : t.who, t.text);
        mind_.summarize(name, turns, [this, alive, id = conversation.id, npc = conversation.npc](const std::string& summary) {
            if (alive.expired())
                return;
            voiced("summary", summary.empty() ? "written" : "model", npc);
            if (!summary.empty() && memories_.rewrite(id, summary))
                saveSoon();
        });
    }
}

void Game::heed(const std::string& npcId, const std::string& subjectId, bool identified, const mind::Reply& reply)
{
    // A generated reply's mood, and its nudges on bounded terms: at most 3 a reply and 6 an hour in each of liking
    // and trust, so a flatterer can't talk anyone into adoring them. A note or promise joins the conversation's memory.
    if (!reply.generated)
        return;
    if (!reply.emotion.empty())
        npcMood_[npcId] = reply.emotion;
    if (!identified)
        return;
    auto& budget = nudgeBudget_[npcId + "|" + subjectId];
    const auto hour = std::int64_t(std::floor(world_.calendarDays() * 24));
    if (budget.hour != hour)
        budget = {hour, 0, 0};
    const auto spend = [](int& used, int wanted) {
        const int room = 6 - std::abs(used);
        const int allowed = std::clamp(wanted, -room, room);
        used += std::abs(allowed);
        return allowed;
    };
    const int affinity = spend(budget.affinity, reply.affinity), trust = spend(budget.trust, reply.trust);
    if (affinity || trust)
        world_.bonds().change(npcId, subjectId, {double(affinity), double(trust), 0, 0, 0}, world_.calendarDays());
    if (!reply.remember.empty())
        memories_.record(npcId, subjectId, {sequence_++, now(), "(your note)", reply.remember});
    if (!reply.promise.empty())
    {
        const bool byNpc = reply.promiseBy == "npc";
        memories_.record(npcId, subjectId, {sequence_++, now(), byNpc ? "(your promise)" : "(their promise)", reply.promise});
        logEvent("promise", byNpc ? npcId : subjectId, byNpc ? subjectId : npcId, reply.promise);
        world_.promise(byNpc ? npcId : subjectId, byNpc ? subjectId : npcId, reply.promise, 3);
    }
}

void Game::talkNext(const std::string& npcId)
{
    while (!pendingNpc_.count(npcId))
    {
        const auto found = queuedTalk_.find(npcId);
        if (found == queuedTalk_.end())
            return;
        if (found->second.empty())
        {
            queuedTalk_.erase(found);
            return;
        }
        const QueuedTalk next = std::move(found->second.front());
        found->second.pop_front();
        talk(npcId, next.playerId, next.heardText, next.voice, next.hasSense ? &next.sense : nullptr);
    }
}

// --------------------------------------------------------------------------- Commands

void Game::command(Connection* c, const std::string& raw)
{
    if (!c || raw.size() > 65536)
        return;
    struct Hold
    {
        Game& game;
        ~Hold() { game.endHolding(); }
    } hold{*this};
    beginHolding(c);
    Value j;
    std::string error;
    if (!json::parse(raw, j, error) || !j.isObject())
        return;
    const std::string type = j.string("type");
    if (accountCommand(c, j, type))
        return;
    if (artworkCommand(c, j, type))
        return;
    if (type == "hello")
    {
        login(c, j);
        return;
    }
    const auto id = c->entityId;
    auto* player = world_.entity(id);
    if (!player)
        return;
    if (player->dead && type != "typing")
    {
        system(c, "You are dead. You cannot act until you are brought back.");
        return;
    }
    const std::string commandId = j.string("commandId");
    if (commandId.size() > 128)
    {
        system(c, "Command identifier exceeds the supported length.");
        return;
    }
    currentCommands_[id] = commandId;
    // Unsolicited events (a later birthday notice, say) must never replace this command's receipt after it returns.
    struct Clear
    {
        std::map<std::string, std::string>& commands;
        std::string id;
        ~Clear() { commands.erase(id); }
    } clear{currentCommands_, id};
    auto& receipts = commandReceipts_[id];
    if (!commandId.empty())
    {
        if (std::find(receipts.begin(), receipts.end(), commandId) != receipts.end())
        {
            const auto reply = responseReceipts_[id].find(commandId);
            if (reply != responseReceipts_[id].end())
                c->event(reply->second);
            else
            {
                auto ack = Value::object();
                ack.add("type", type == "chat" ? "chatAccepted" : "commandAccepted");
                ack.add("commandId", commandId);
                ack.add("requestId", j.string("requestId"));
                send(c, ack);
            }
            return;
        }
        receipts.push_back(commandId);
        if (receipts.size() > 256)
        {
            responseReceipts_[id].erase(receipts.front());
            receipts.erase(receipts.begin());
        }
    }
    Result result;
    bool report = false;
    const auto num = [&](const char* key) { return wire::number(j, key); };
    const bool walkingCommand = type == "move" || type == "path" || type == "pose" || type == "face" || type == "travel";
    if (walkingCommand && c->movementMode == Fighting)
    {
        result = {false, player->downedLeft > 0 && !world_.inBattle(id) ? "You are down." : "You are in a fight.", {}};
        report = type != "move" && type != "pose";
    }
    else if (type == "walking")
    {
        // The client walks its own wolf where it may (free movement), and says where it is with "pose".
        c->clientWalking = j.string("mode") == "client";
        c->clientSight = j.string("sight") == "client";
        world_.setClientWalks(id, c->clientWalking && c->movementMode == FreeMovement && !c->keysWalking);
    }
    else if (type == "pose")
        pose(c, std::uint32_t(std::max(0.0, num("seq"))), num("x"), num("y"), num("facing"), num("ix"), num("iy"));
    else if (type == "move")
    {
        if (std::hypot(num("x"), num("y")) > 0 && !c->keysWalking)
        {
            c->keysWalking = true;                 // Keys: the server walks it, until the client sends poses again.
            world_.setClientWalks(id, false);
        }
        result = world_.move(id, num("x"), num("y"));
        if (const double seq = num("seq"); seq > 0)
            player->inputSeq = std::uint32_t(seq);        // Held movement: the last input applied (the motion frame says).
    }
    else if (type == "path")
    {
        // A route the server walks: held until the wolf arrives (or the player takes over with the keys).
        world_.setClientWalks(id, false);
        c->movementMode = std::max<std::uint8_t>(c->movementMode, HeldMovement);
        result = world_.moveTo(id, num("x"), num("y"));
        report = !result.ok;
    }
    else if (type == "face")
    {
        result = world_.face(id, num("x"), num("y"));
        report = !result.ok;
    }
    else if (type == "stop")
        world_.stop(id);
    else if (type == "pace")
    {
        result = wire::paceCommand(world_, id, j);
        report = !result.ok;
    }
    else if (type == "travel")
    {
        const std::string target = j.string("target");
        world_.setClientWalks(id, false);          // The journey is the server's to walk (held until it ends).
        c->movementMode = std::max<std::uint8_t>(c->movementMode, HeldMovement);
        result = target.size() <= 96 ? world_.travelTo(id, target) : Result{false, "No known route to that destination.", {}};
        report = true;
    }
    else if (type == "cancel_travel")
    {
        result = world_.cancelTravel(id);
        report = true;
    }
    else if (type == "typing")
    {
        player->typing = j.boolean("active");
        typingExpiry_[id] = world_.time() + 3.5;
        // A deliberate overland journey can continue while writing; ordinary movement stops.
        if (!world_.travelState(id).active)
            world_.stop(id);
    }
    else if (type == "color")
    {
        player->speakingColor = int(std::clamp(num("index"), 0.0, 31.0));
        saveSoon();
    }
    else if (type == "weather" || type == "time" || type == "lighting" || type == "calendar")
    {
        result = wire::environmentCommand(world_, player->cellId, type, j.string("value"), options_.devTools);
        report = true;
        if (result.ok)
            saveSoon();
    }
    else if (type == "trade")
    {
        const double qty = wire::strictNumber(j, "quantity", -1);
        const auto* buy = j.find("buy");
        // A faction's merchant and the buyer's Chapter (doc 32, 4.3): turned away, or a surcharge after.
        Result refusal;
        const auto* purse = world_.society().account(id);
        const std::int64_t before = purse ? purse->cash : 0;
        result = qty >= 1 && qty <= 99 && qty == std::floor(qty) && buy && buy->isBool()
                     ? (!factionTrade(id, j.string("target"), buy->asBool(), refusal)
                            ? refusal
                            : world_.trade(id, j.string("target"), j.string("item"), int(qty), buy->asBool()))
                     : Result{false, "Invalid trade request.", {}};
        if (result.ok && buy && buy->asBool())
            if (const auto* after = world_.society().account(id))
                afterFactionTrade(id, j.string("target"), before - after->cash);
        report = true;
        if (result.ok)
            record(Economy | Character, id);
    }
    else if (type == "gather" || type == "eat")
    {
        result = type == "gather" ? world_.gather(id) : world_.eat(id);
        report = true;
        if (result.ok)
            record(Economy | Character, id);
    }
    else if (type == "front" && options_.devTools)
    {
        // A squall of the given weather over the player, 45 tiles across, drifting east for six hours (doc 29, phase 7).
        Weather kind = Weather::Rain;
        const auto value = j.string("value");
        const std::map<std::string, Weather> kinds{{"rain", Weather::Rain}, {"storm", Weather::Storm}, {"fog", Weather::Fog},
                                                   {"snow", Weather::Snow}, {"overcast", Weather::Overcast}, {"sandstorm", Weather::Sandstorm}};
        if (const auto k = kinds.find(value); k != kinds.end())
            kind = k->second;
        const auto* c = world_.cell(player->cellId);
        result = c && c->outdoors ? world_.spawnFront(kind, c->worldX + player->position.x, c->worldY + player->position.y, 45, 0, 6, true)
                                  : Result{false, "Fronts are called up outdoors.", {}};
        if (result.ok && c && !c->seasonalWeather)
            world_.useSeasonalWeather(c->id);      // The cell follows the field again, so the front shows.
        report = true;
        if (result.ok)
            saveSoon();
    }
    else if (type == "wind" && options_.devTools)
    {
        const std::string value = j.string("value");
        if (value != "east" && value != "west" && value != "north" && value != "calm" && value != "live")
        {
            system(c, "Unknown wind preset.");
            return;
        }
        const double pi = std::acos(-1.0);
        const double direction = value == "west" ? pi : value == "north" ? -pi * .5 : 0.0;
        result = world_.setWind(player->cellId, direction, value == "calm" ? 0.0 : .5, value == "live");
        report = true;
        saveSoon();
    }
    else if (type == "faction")
    {
        if (!factionCommand(c, j, result))
            result = {false, "That isn't something a faction does.", {}};
        report = !result.message.empty();
    }
    else if (type == "chapter")
    {
        if (!chapterCommand(c, j, result))
            result = {false, "That isn't something a Chapter does.", {}};
        report = !result.message.empty();
    }
    else if (type == "social")
    {
        if (!socialCommand(c, j, result))
            result = {false, "That isn't something you can do.", {}};
        report = !result.message.empty();
    }
    else if (type == "names")
    {
        if (!namesCommand(c, j, result))
            result = {false, "That isn't something you can do with a name.", {}};
        report = true;
    }
    else if (type == "party")
    {
        if (!partyCommand(c, j, result))
            result = {false, "That isn't something a party does.", {}};
        report = true;
    }
    else if ((type == "gift" || type == "grant") && options_.devTools)
    {
        // Development only: a Gift (who has one is the setting's to decide: the Dungeon Master, in time), or goods made.
        if (type == "gift")
            result = world_.giveGift(id, j.string("gift"), j.boolean("quickened"));
        else
            result = world_.society().create(id, j.string("item"), std::clamp(int(j.number("quantity", 1)), 1, 9), "development grant")
                         ? Result{true, "Granted.", {}}
                         : Result{false, "Not a good that can be made.", {}};
        report = true;
        if (result.ok)
            record(Economy | Character, id);
    }
    else if (type == "battle")
    {
        if (!battleCommand(c, j, result))
            result = {false, "You can't do that in a fight.", {}};
        report = !result.ok;                       // What happened is in the fight's own log; only a refusal is said.
        if (result.ok)
            record(Character, id);
    }
    else if (type == "action")
    {
        const std::string target = j.string("target"), action = j.string("action");
        const auto beforeCell = player->cellId;
        const auto distanceTo = [&](const Entity& e) {
            return std::hypot(e.position.x - player->position.x, e.position.y - player->position.y);
        };
        if (target.empty() && (action == "look" || action == "listen" || action == "smell" || action == "wait" || action == "sit" ||
                               action == "lay" || action == "stand" || action == "session_end"))
        {
            operatorActivity_[id] = now();
            if (action == "look")
                system(c, world_.cell(player->cellId)->description + "\n" +
                              wire::environmentDescription(*world_.cell(player->cellId), world_.environmentAt(player->cellId)));
            else if (action == "listen")
                system(c, "You pause to listen. Hearing skill, ear health, distance and barriers shape what you hear. Weather and "
                          "wind can mask sound. Sneaking quiets pawsteps; voices keep their selected volume.");
            else if (action == "smell")
            {
                const auto cues = world_.scentCues(id);
                if (player->smell <= 0 || player->noseHealth <= 0)
                    system(c, "You cannot distinguish scents with your nose in its current condition.");
                else if (cues.empty())
                {
                    bool visibleScent = false;
                    for (const auto& [other, e] : world_.entities())
                        if (other != id && world_.visionClarity(id, other) > 0 && world_.scentClarity(id, other) > 0)
                            visibleScent = true;
                    system(c, visibleScent ? "You catch the scent of nearby wolves you can already see. No additional unseen wolf scent reaches you."
                                           : "No distinct wolf scent reaches you right now. That does not mean you are alone.");
                }
                else
                {
                    static const char* bearings[] = {"east", "southeast", "south", "southwest", "west", "northwest", "north", "northeast"};
                    std::string message;
                    for (const auto& cue : cues)
                    {
                        if (!message.empty())
                            message += ' ';
                        message += std::string(cue.strength == 1 ? "Faint" : "A distinct") + " wolf scent reaches you from roughly " +
                                   bearings[cue.sector] + (cue.windborne ? ", carried on the wind." : " through the nearby air.");
                    }
                    system(c, message);
                }
            }
            else if (action == "session_end")
            {
                social_.endFor(id, now());
                afterSocial();
                system(c, "Your active scene has ended. Qualified contributions have been settled by the server.");
            }
            else if (action == "wait")
            {
                world_.stop(id);
                system(c, "You settle and let the scene unfold.");
            }
            else
            {
                result = world_.setPosture(id, action == "sit" ? "sitting" : action == "lay" ? "lying" : "standing");
                auto post = parsePost("/" + action);
                if (player->posture == "rising")
                    for (auto& segment : post.segments)
                        if (segment.kind == "state" && segment.text == "stands up.")
                            segment.text = "begins to stand.";
                publish(id, post, Voice::Speak);
            }
        }
        else if (action == "talk" || action == "speak")
        {
            const auto* npc = world_.entity(target);
            if (!npc || !npc->npc || world_.visionClarity(id, target) <= 0)
            {
                system(c, "You cannot see that wolf.");
                return;
            }
            if (npc->transient)
            {
                system(c, world_.hostile(target) ? "They want your purse, not your conversation." : "The carters are too busy with the road to talk.");
                return;
            }
            if (world_.hearingClarity(target, id) < 0.25)
            {
                system(c, "Move closer so that wolf can hear you.");
                return;
            }
            // Choosing whom to speak to (Docs/Design/29, phase 4): the client adds them to its talk targets, and what
            // the player writes next goes to them. Nothing is said for the player.
            auto chosen = Value::object();
            chosen.add("type", "talkTarget");
            chosen.add("id", target);
            chosen.add("name", names::capitalised(labelFor(id, target)));
            send(c, chosen);
        }
        else if (action == "steal" || action == "report" || action == "pay fine")
        {
            const auto done = action == "steal" ? world_.steal(id, target) : action == "report" ? world_.report(id, target) : world_.payFine(id, target);
            system(c, done.message);
            if (done.ok)
                record(Economy | Crime | Character, id);
        }
        else if (action == "attack" || action == "pay" || action == "challenge")
        {
            const auto done = action == "pay" ? world_.payBandits(id, target) : world_.attack(id, target);
            system(c, done.message);
            if (done.ok)
                record(Economy | Crime | Roads | Character, id);
            updateMovementModes();
        }
        else if (action == "accept" || action == "decline")
        {
            const auto done = world_.answerChallenge(id, action == "accept");
            system(c, done.message);
            updateMovementModes();
        }
        else if (action == "hold sword" || action == "stow sword" || action == "take")
        {
            const auto done = action == "hold sword" ? (world_.inBattle(id) ? world_.battleAct(id, "hold") : world_.holdItem(id, "sword"))
                              : action == "stow sword" ? (world_.inBattle(id) ? world_.battleAct(id, "stow") : world_.stowItem(id))
                                                       : world_.takeItem(id, target);
            system(c, done.message);
            if (done.ok)
                record(Economy | Character, id);
        }
        else if (action == "struggle" || action == "tend")
        {
            const auto done = action == "struggle" ? world_.struggleUp(id) : world_.tendWounds(id, target);
            system(c, done.message);
            if (done.ok)
                record(Character, id);
        }
        else if (action == "ask for work")
        {
            std::string list;
            for (const auto* k : world_.contractsNear(id))
                list += "\n  " + k->id + "  " + k->detail + (k->reward ? " (" + std::to_string(k->reward) + " pennies)" : std::string());
            system(c, list.empty() ? "There is no work to be had here just now." : "Work to be had:" + list);
        }
        else if (action.rfind("take ", 0) == 0)
        {
            const auto taken = world_.takeContract(id, action.substr(5));
            system(c, taken.message);
            if (taken.ok)
                record(Roads | Economy | Character, id);
        }
        else if (action == "apprentice")
        {
            // Whether they take you on is theirs to decide: they must know and trust you (Society::apprentice).
            const auto* npc = world_.entity(target);
            if (!npc || !npc->npc || world_.visionClarity(id, target) <= 0 || distanceTo(*npc) > 3)
            {
                system(c, "Come closer to the one you would learn from.");
                return;
            }
            const auto taken = world_.apprentice(id, target);
            system(c, taken.message);
            if (taken.ok)
                record(Careers | Economy | Character, id);
        }
        else if (action == "ask to join" || action.rfind("hire for ", 0) == 0)
        {
            result = askAlong(id, target, action != "ask to join");
            report = true;
        }
        else if (action == "wait here" || action == "follow me" || action == "go home" || action == "dismiss")
        {
            result = orderCompanion(id, target, action);
            report = !result.ok;
        }
        else if (action.rfind("introduce", 0) == 0)
        {
            // An introduction is said aloud, so whoever hears it learns the name: the page sends it as speech ("I'm
            // Kestrel."). Said here as an action, it is only a reminder of how.
            system(c, "Introduce yourself aloud: say \"I'm\" and the name you go by.");
        }
        else if (action == "ask about our standing" || action == "pay for a report (5p)" || action == "ask for missions" ||
                 action == "give a tithe (20p)" || action == "tell of an expulsion" || action.rfind("deliver mission-", 0) == 0)
        {
            auto k = Value::object();
            k.add("verb", action == "ask about our standing" ? "report" : action == "pay for a report (5p)" ? "payreport"
                          : action == "ask for missions"     ? "missions"
                          : action == "give a tithe (20p)"   ? "tithe"
                          : action == "tell of an expulsion" ? "tellexpulsion"
                                                             : "deliver");
            k.add("target", target);
            if (action.rfind("deliver ", 0) == 0)
                k.add("mission", action.substr(8));
            factionCommand(c, k, result);
            report = !result.message.empty();
        }
        else if (action == "invite to chapter" || action == "mark hostile" || action == "unmark hostile")
        {
            auto k = Value::object();
            k.add("verb", action == "invite to chapter" ? "invite" : action == "mark hostile" ? "hostile" : "unhostile");
            k.add("target", target);
            k.add("reason", j.string("reason"));
            chapterCommand(c, k, result);
            report = true;
        }
        else if (action == "invite")
        {
            result = partyInvite(id, target);
            report = true;
        }
        else if (action == "inspect" && !world_.door(target))
        {
            const auto* other = world_.entity(target);
            // Missing and hidden characters answer alike, so guessed IDs can't turn anonymous scent into an identity.
            if (!other || (target != id && world_.visionClarity(id, target) <= 0))
            {
                system(c, "You cannot inspect someone you cannot see.");
                return;
            }
            auto e = Value::object();
            e.add("type", "inspect");
            e.add("id", other->id);
            e.add("name", names::capitalised(labelFor(id, other->id)));
            e.add("title", names::capitalised(labelFor(id, other->id)));
            e.add("appearance", wire::appearance(other->appearance));
            if (const auto portrait = visiblePortrait(other->id, id); !portrait.empty())
                e.add("artwork", portrait);
            e.add("lifeStage", lifeStageName(lifeStage(other->age)));
            e.add("shoulderHeightCm", shoulderHeightCm(other->appearance, other->age));
            const auto described = veilFor(id, other->description);
            // How they regard this wolf, in words; and this wolf's own note on them (doc 32, 1.4).
            if (target != id)
            {
                e.add("regard", regardWords(other->id, id));
                if (const auto mine = notes_.find(id); mine != notes_.end())
                    if (const auto note = mine->second.find(other->id); note != mine->second.end())
                        e.add("note", note->second);
            }
            e.add("description", described);
            e.add("posture", other->posture);
            e.add("state", other->state);
            e.add("text", described + " Current posture: " + other->posture + ". " + other->state);
            send(c, e);
        }
        else
        {
            result = world_.interact(id, target, action);
            report = true;
        }
        if (player->cellId != beforeCell)
            followTransition(id, beforeCell);
        if (result.ok)
            operatorActivity_[id] = now();
        saveSoon();
    }
    else if (type == "chat")
    {
        const auto feedback = [&](bool accepted, const std::string& message) {
            auto e = Value::object();
            e.add("type", accepted ? "chatAccepted" : "error");
            e.add("context", "chat");
            e.add("requestId", j.string("requestId"));
            e.add("commandId", commandId);
            e.add("text", message);
            if (!commandId.empty())
                responseReceipts_[id][commandId] = json::dump(e);
            send(c, e);
            saveSoon();
        };
        const std::string text = mind::trim(j.string("text"));
        if (text.empty() || mind::left(text, 16385).size() > mind::left(text, 16384).size())
        {
            feedback(false, "Post must contain 1\xe2\x80\x93" "16,384 characters.");
            return;
        }
        if (lastChat_.count(id) && world_.time() - lastChat_[id] < 0.5)
        {
            feedback(false, "Please leave a moment between posts.");
            return;
        }
        lastChat_[id] = world_.time();
        player->typing = false;
        if (!world_.travelState(id).active)
            world_.stop(id);
        const std::string channel = j.string("channel");
        if ((channel == "party" || channel == "partyooc") && !parties_.of(id))
        {
            feedback(false, "You are not in a party.");
            return;
        }
        if ((channel == "chapter" || channel == "chapterooc") && !chapters_.of(id))
        {
            feedback(false, "You are not in a Chapter.");
            return;
        }
        if (channel == "partyooc" || channel == "chapterooc")
        {
            partyChat(c, *player, text, channel);
            feedback(true, "");
            return;
        }
        if (channel == "ooc")
        {
            auto e = Value::object();
            e.add("type", "ooc");
            e.add("channel", "ooc");
            e.add("sequence", sequence_++);
            e.add("text", text);
            e.add("color", player->speakingColor);
            for (auto* other : clients_)
                if (const auto* actor = world_.entity(other->entityId); actor && actor->cellId == player->cellId)
                {
                    e.set("speaker", names::capitalised(labelFor(other->entityId, id)));
                    send(other, e);
                }
            feedback(true, "");
            return;
        }
        auto post = parsePost(text);
        if (!post.ok)
        {
            feedback(false, post.error);
            return;
        }
        if (!post.posture.empty())
        {
            world_.setPosture(id, post.posture);
            if (player->posture == "rising")
                for (auto& segment : post.segments)
                    if (segment.kind == "state" && segment.text == "stands up.")
                        segment.text = "begins to stand.";
        }
        if (post.hasState)
            player->state = post.state;
        const std::string volume = j.string("volume");
        const Voice voice = volume == "whisper" ? Voice::Whisper : volume == "yell" ? Voice::Yell : Voice::Speak;
        const std::uint64_t event = sequence_;
        // Who is spoken to (Docs/Design/29, phase 4): the talk targets the player chose (up to four) who can hear it,
        // anyone named, a companion asked for their thoughts; with none of those, the one resident close enough to
        // be plainly addressed, if there is exactly one. Only those answer, one after another.
        std::vector<std::string> targets;
        for (const auto& t : j.array("targets"))
            if (t.isString() && targets.size() < 4 && std::find(targets.begin(), targets.end(), t.asString()) == targets.end())
                targets.push_back(t.asString());
        struct Heard
        {
            std::string npcId, text;
            SensoryResult sense;
            bool targeted, named, near;
        };
        std::vector<Heard> hearers;
        for (const auto& [npcId, e] : world_.entities())
        {
            if (!e.npc || e.transient || e.dead || e.cellId != player->cellId)
                continue;
            const auto sense = world_.perceive(npcId, id, voice);
            const auto segments = perceivePost(post, sense.hearing, sense.vision, event * 7919 + std::hash<std::string>{}(npcId));
            std::string perceived;
            for (const auto& segment : segments)
            {
                if (!perceived.empty())
                    perceived += ' ';
                perceived += segment.text;
            }
            if (perceived.empty())
                continue;
            const std::string lower = mind::lower(perceived);
            const bool targeted = std::find(targets.begin(), targets.end(), npcId) != targets.end();
            const bool inParty = parties_.companion(npcId) && parties_.together(npcId, id);
            const bool invited = inParty && (lower.find("what do you think") != std::string::npos || lower.find("your thoughts") != std::string::npos);
            const bool interject = inParty && npcId == "npc_scout" && npcLastSpeech_[npcId] + 45 < world_.time() &&
                                   (lower.find("road") != std::string::npos || lower.find("danger") != std::string::npos);
            const bool named = namesWord(lower, mind::lower(e.name)) || invited || interject;
            const bool near = sense.hearing >= 0.5 && std::hypot(e.position.x - player->position.x, e.position.y - player->position.y) <= 6;
            hearers.push_back({npcId, perceived, sense, targeted, named, near});
            {
                std::string spoken;
                for (const auto& segment : segments)
                    if (segment.kind == "speech")
                        spoken += (spoken.empty() ? "" : " ") + segment.text;
                if (sense.identifiable)
                    noticeIntroduction(id, npcId, spoken);
            }
        }
        std::vector<const Heard*> addressed;
        for (const auto& target : targets)          // In the order chosen.
            for (const auto& h : hearers)
                if (h.npcId == target)
                    addressed.push_back(&h);
        for (const auto& h : hearers)
            if (h.named && !h.targeted)
                addressed.push_back(&h);
        if (addressed.empty() && targets.empty())
        {
            const Heard* only = nullptr;
            int close = 0;
            for (const auto& h : hearers)
                if (h.near)
                {
                    ++close;
                    only = &h;
                }
            if (close == 1 && post.speech)
                addressed.push_back(only);
        }
        std::vector<std::string> to;
        for (const auto* h : addressed)
            to.push_back(h->npcId);
        const auto heard = publish(id, post, voice, to, channel == "party" || channel == "chapter" ? channel : std::string());
        if (const auto* mine = parties_.of(id))
            lastPartySpeech_[mine->id] = world_.time();
        const auto evidence = roleplayEvidence(post);
        // Actions count at half weight; said with a party mate listening, it is the party's scene (doc 32, 1.1).
        std::string partyScene;
        if (const auto* mine = parties_.of(id))
            for (const auto& listener : heard)
                if (listener != id && parties_.together(id, listener) && clientOf(listener))
                    partyScene = mine->id;
        social_.record({event, now(), id, player->cellId, evidence.words + evidence.actionWords / 2, false, evidence.contentHash, {},
                        partyScene},
                       heard);
        // A chosen wolf who didn't hear: say so, rather than leave the player waiting.
        for (const auto& target : targets)
            if (std::none_of(hearers.begin(), hearers.end(), [&](const Heard& h) { return h.npcId == target; }))
                if (const auto* npc = world_.entity(target); npc && npc->npc)
                    system(c, npc->name + (npc->cellId == player->cellId ? " is too far away to hear you." : " is no longer here."));
        // One after another, each having heard the ones before (copies: talking may change the world's entities).
        TalkChain chain;
        chain.playerId = id;
        chain.voice = voice;
        for (const auto* h : addressed)
            chain.rest.push_back({h->npcId, h->text, h->sense});
        continueChain(std::move(chain));
        feedback(true, "");
        operatorActivity_[id] = now();
    }
    if (result.ok && (type == "path" || type == "travel" || type == "trade" || type == "gather" || type == "eat" ||
                      (type == "move" && (num("x") != 0 || num("y") != 0))))
        operatorActivity_[id] = now();
    if (report && !result.message.empty())
        system(c, result.message);
    ++revision_;
}

// --------------------------------------------------------------------------- Saving

void Game::saveSoon()
{
    if (saveSoonIn_ < 0)
        saveSoonIn_ = SaveSoonSeconds;
}

DbStore::Build Game::capture()
{
    // Copied here, on the game thread; the document is made from the copy on the store's worker.
    for (const auto& [id, e] : world_.entities())
        if (!e.npc)
            characters_[id] = e;
    for (auto& [id, e] : characters_)
        advanceAge(e, world_.calendarDays());
    struct Capture
    {
        checkpoint::ServerState server;
        PersistedWorld saved;
        std::vector<Entity> npcs;
        double time = 0;
    };
    auto c = std::make_shared<Capture>();
    c->server.accounts = accounts_.state();
    c->server.director = director_.state();
    c->server.sequence = sequence_;
    c->server.revision = revision_;
    c->server.characters = characters_;
    c->server.companions = companionOwner_;
    c->server.parties = parties_.save();
    c->server.acquaintances = known_.save();
    c->server.notes = notesSave();
    c->server.chapters = chapters_.save();
    c->server.factions = factions_.save();
    {
        auto aliases = Value::object();
        for (const auto& [who, list] : aliases_)
        {
            auto names = Value::array();
            for (const auto& a : list)
                names.push(a);
            aliases.add(who, names);
        }
        c->server.aliases = aliases;
    }
    for (const auto& [who, heard] : scenesHeard_)
        c->server.scenesHeard[who].assign(heard.order.begin(), heard.order.end());
    c->server.memories = memories_;
    c->server.social = social_;
    c->server.commandReceipts = commandReceipts_;
    c->server.responseReceipts = responseReceipts_;
    c->time = world_.time();
    c->saved = world_.save();
    for (const auto& [id, e] : world_.entities())
        if (e.npc && !e.transient)                 // Road folk come back from the roads' own state.
            c->npcs.push_back(e);
    const bool database = store_ && store_->database();
    const auto journalSeq = journalSeq_;
    return [c, database, journalSeq](Value& document, std::string& npcStates) {
        document = checkpoint::encode(c->saved, c->server, c->npcs, c->time);
        document.set("journal", double(journalSeq));   // The journal records it covers (RatwJournal.h).
        if (database)
            npcStates = checkpoint::npcStates(c->saved, c->npcs);
    };
}

void Game::autosave()
{
    perf::Scope timed(meter_, perf::Saves);
    saveSoonIn_ = -1;
    if (!store_ || !storageReady_)
        return;
    if (options_.forkSnapshots && forkFailures_ < 3 && forkSnapshot())
        return;
    store_->queueEvents(world_.takeEvents());
    const auto seq = journalSeq_;
    if (!store_->saveInBackground(capture(), revision_))
    {
        storageReady_ = false;
        note("error", "RATW persistence commit failed: " + store_->error());
    }
    else
        trimWhenStored_.push_back({revision_, seq});
}

void Game::save()
{
    perf::Scope timed(meter_, perf::Saves);
    saveSoonIn_ = -1;
    if (!store_)
        return;
    reapSnapshot(true);                            // A snapshot under way is finished and handed over first.
    store_->queueEvents(world_.takeEvents());
    const auto seq = journalSeq_;
    if (storageReady_ && !store_->save(capture(), revision_))
    {
        storageReady_ = false;
        note("error", "RATW persistence commit failed: " + store_->error());
    }
    else if (auto* writer = store_->journal(); writer && storageReady_)
    {
        trimWhenStored_.clear();
        writer->trim(seq);
    }
}

void Game::load(const std::string& payload)
{
    if (payload.empty())
        return;
    Value document;
    PersistedWorld saved;
    checkpoint::ServerState state;
    std::string problem;
    if (!json::parse(payload, document, problem) || !replayJournal(document, problem) ||
        !checkpoint::decode(document, saved, state, problem))
    {
        storageReady_ = false;
        note("error", "RATW restore rejected (" + problem + "); checkpoint preserved, autosave disabled");
        return;
    }
    accounts::Accounts restored;
    if (document.has("accounts") && !restored.restore(state.accounts))
    {
        storageReady_ = false;
        note("error", "RATW invalid account checkpoint; preserved and autosave disabled.");
        return;
    }
    sequence_ = state.sequence;
    revision_ = state.revision;
    if (document.has("director") && !director_.restore(state.director))
    {
        storageReady_ = false;
        note("error", "RATW invalid operator receipt checkpoint; autosave disabled.");
        return;
    }
    std::set<std::string> characterIds;
    for (const auto& player : saved.players)
        characterIds.insert(player.id);
    if (!restored.referencesOnly(characterIds))
    {
        storageReady_ = false;
        note("error", "RATW character/account ownership is incomplete; autosave disabled.");
        return;
    }
    const auto result = world_.restore(saved);
    if (!result.ok)
    {
        storageReady_ = false;
        note("error", "RATW restore rejected; checkpoint preserved: " + result.message);
        return;
    }
    accounts_ = std::move(restored);
    for (const auto& player : saved.players)
    {
        // The core's restored, normalized posture, never stale transient rise or turn state from the record.
        if (const auto* e = world_.entity(player.id))
            characters_[player.id] = *e;
        world_.removePlayer(player.id);
    }
    parties_.load(state.parties);
    aliases_.clear();
    for (const auto& [who, list] : state.aliases.fields())
        for (const auto& a : list.items())
            if (a.isString() && aliases_[who].size() < names::MaxAliases)
                aliases_[who].push_back(a.asString());
    // Who knows whom (doc 32). A save from before introductions has none: those well acquainted keep each other's names.
    if (state.acquaintances.isObject())
        known_.load(state.acquaintances);
    else
        seedAcquaintances();
    adoptOldCompanions(state.companions);           // (Older saves: residents following a player.)
    for (const auto& [partyId, p] : parties_.all())
        for (const auto& c : p.companions)
            if (auto* e = world_.entity(c.id))
                e->leaderId = "party:" + partyId;
    scenesHeard_.clear();
    for (const auto& [who, list] : state.scenesHeard)
        for (const auto& id : list)
            scenesHeard_[who].add(id);
    memories_ = state.memories;
    social_.entries = state.social.entries;
    social_.points = state.social.points;
    social_.recent = state.social.recent;
    social_.sessions = state.social.sessions;
    social_.stars = state.social.stars;
    social_.stories = state.social.stories;
    social_.nextStory = state.social.nextStory;
    socialSeen_ = social_.entries.size();             // (Scenes settled before the restart were told then.)
    notesLoad(state.notes);
    chapters_.load(state.chapters);
    factions_.load(state.factions);
    for (const auto& [who, ids] : state.commandReceipts)
        for (const auto& receipt : ids)
            commandReceipts_[who].push_back(receipt);
    for (const auto& [actor, entries] : state.responseReceipts)
        for (const auto& [key, value] : entries)
            responseReceipts_[actor][key] = value;
    note("info", "RATW_RESTORE characters=" + std::to_string(characters_.size()) + " summaries=" + std::to_string(memories_.summaries.size()) +
                     " ledger=" + std::to_string(social_.entries.size()));
}
} // namespace ratw::game
