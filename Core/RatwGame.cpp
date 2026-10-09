#include "RatwGame.h"

#include "RatwCellPrefetch.h"
#include "RatwMotionCore.h"
#include "RatwWire.h"
#include "RatwGifts.h"
#include "RatwCreation.h"
#include "RatwLevels.h"
#include "RatwItems.h"
#include "RatwWild.h"

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

class ScratchStoreImpl final : public Store
{
  public:
    explicit ScratchStoreImpl(std::unique_ptr<Store> inner) : inner_(std::move(inner)) {}
    std::unique_ptr<journal::Writer> writer = journal::memoryWriter(std::make_shared<std::vector<journal::Record>>());
    journal::Writer* journal() override { return writer.get(); }
    bool journalAfter(std::uint64_t after, std::vector<journal::Record>& out, std::string& error) override
    {
        return inner_->journalAfter(after, out, error);     // The real server's journal, read to replay over its save.
    }
    bool database() const override { return false; }
    std::string load() override { return inner_->load(); }
    bool save(DbStore::Build, std::uint64_t) override { return true; }
    bool saveInBackground(DbStore::Build, std::uint64_t) override { return true; }
    bool flush() override { return true; }
    void queueEvents(std::vector<WorldEvent>) override {}
    std::vector<std::pair<std::string, std::string>> externalNpcStates() override { return inner_->externalNpcStates(); }
    std::string error() const override { return {}; }

  private:
    std::unique_ptr<Store> inner_;
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

std::unique_ptr<Store> scratchStore(std::unique_ptr<Store> inner)
{
    return inner ? std::make_unique<ScratchStoreImpl>(std::move(inner)) : nullptr;
}

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

Game::Game(Options options) : options_(std::move(options)), random_(std::random_device{}())
{
    // A Chapter's rented places are locked to all but its members and guests (doc 32, 5.2).
    world_.mayEnter = [this](const std::string& who, const std::string& cell) { return mayEnterPlace(who, cell); };
    world_.realClock = [] { return Game::now(); };                                  // (Practice's day: doc 49.)
    // A room scene starts Private in a rented place (doc 51, §5). (Residents' homes too, in the plan; but the world
    // doesn't tell a home from an inn or a shop its keeper lives in, so a home's scene starts Open for now.)
    social_.privatePlace = [this](const std::string& cell) { return estates_.lease(cell) != nullptr; };
    world_.accountOf = [this](const std::string& who) { return accounts_.ownerOf(who); };
    world_.playerActive = [this](const std::string& who) {   // (At the keys in the last five minutes: doc 49.)
        const auto at = operatorActivity_.find(who);
        const auto* e = world_.entity(who);              // An action or chat, or a step walked.
        return (at != operatorActivity_.end() && now() - at->second < 300) || (e && e->lastPoseAt >= 0 && world_.time() - e->lastPoseAt < 300);
    };
    setSpeed(options_.speed);
}

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
    letGoOfOwnership();                             // Last, once everything is written.
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
    if (!options_.voiceLog.empty() && !options_.scratch)    // (The ledger is the real server's.)
    {
        voiceLog_.open(options_.voiceLog, std::ios::app);
        if (!voiceLog_)
            note("warn", "RATW_VOICE the ledger " + options_.voiceLog + " cannot be written");
    }
    // One server per world, before anything is loaded or written. A scratch server writes nothing: it needn't own it.
    if (options_.scratch)
        note("info", "RATW_SCRATCH a scratch server (pid " + std::to_string(::getpid()) +
                         "): nothing it does is saved, and it runs beside any other server. Stop it when you're done.");
    else if (!takeOwnership(problem))
        return false;
    const bool live = !options_.database.empty();
    if (live)
    {
        if (!loadFromDatabase(problem))
            return false;
        store_ = databaseStore(options_.conninfo, liveWorldId_, problem);
        if (options_.scratch)
            store_ = scratchStore(std::move(store_));
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
        if (options_.scratch && store_)
            store_ = scratchStore(std::move(store_));
        if (!store_)
            return false;
    }
    storageReady_ = true;
    // A hunt (doc 41) may be joined by a hunter's friends: their party, or their Chapter.
    world_.setFriends([this](const std::string& a, const std::string& b) {
        const auto* pa = parties_.of(a);
        const auto* ca = chapters_.of(a);
        return (pa && pa == parties_.of(b)) || (ca && ca == chapters_.of(b));
    });
    // Hunting together (doc 53): no one blocked by a hunter may join or ask (doc 50's block).
    world_.setBlocked([this](const std::string& a, const std::string& b) { return blocked(a, b); });
    world_.setPartnered([this](const std::string& a, const std::string& b) { return parties_.together(a, b); });
    world_.setEventWatcher([this](const WorldEvent& e) { watchEvent(e); });
    world_.setNamer([this](const std::string& knower, const std::string& subject) {   // (Talk speaks only names given: doc 56.)
        return knowsName(knower, subject) ? labelFor(knower, subject) : std::string();
    });
    world_.setProtected([this](const std::string& id) { return isProtected(id); });   // (Doc 57, 6.)
    world_.setKnower([this](const std::string& knower, const std::string& subject) {   // (As the town knows them: doc 57, 5.)
        const bool named = knowsName(knower, subject);
        return World::Known{named ? labelFor(knower, subject) : strangerLabel(subject), named};
    });
    world_.setDeedWords([this](const std::string& teller, const std::string& claim, const std::string& subject) {
        return fameWords(teller, claim, subject);
    });   // (Deeds a resident may thank for: doc 55, 5.)
    world_.setBedRight([this](const std::string& id, const std::string& cell) { return hasBedRight(id, cell); });   // (Doc 54, 1.)
    if (!health_ && !options_.savePath.empty() && !options_.scratch)
        health_ = health::Recorder::file(options_.savePath + ".health.jsonl");
    if (options_.workerThreads > 0)
    {
        pool_ = std::make_unique<Pool>(options_.workerThreads);
        world_.setParallel([this](std::size_t count, const std::function<void(std::size_t)>& job) { pool_->run(count, job); });
        world_.setRoutesOffThread(true);
    }
    {
        // Uploaded portraits (doc 29, phase 9): beside the save, or in the database. Not needed to play.
        std::string trouble;
        artwork_ = options_.scratch ? art::memoryStore()
                   : live ? art::databaseStore(options_.conninfo, liveWorldId_, trouble)
                   : options_.savePath.empty() ? art::memoryStore() : art::folderStore(options_.savePath + ".art", trouble);
        if (artwork_)
            for (const auto& m : artwork_->all())
                artworkMeta_[m.id] = m;
        else
            note("warn", "RATW_ARTWORK portraits can't be uploaded: " + trouble);
    }
    {
        // Reports (doc 50, Phase 2): beside the save, or in the database; in memory where neither can keep them.
        std::string trouble;
        reports_ = options_.scratch ? reports::memoryStore()
                   : live ? reports::databaseStore(options_.conninfo, liveWorldId_, trouble)
                   : options_.savePath.empty() ? reports::memoryStore() : reports::folderStore(options_.savePath + ".reports", trouble);
        if (!reports_)
        {
            note("warn", "RATW_REPORTS kept in memory only: " + trouble);
            reports_ = reports::memoryStore();
        }
        for (const auto& r : reports_->all())
            reportCache_[r.id] = r;
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
    note("info", "RATW authoritative world ready; 20Hz; save=" +
                     std::string(options_.scratch ? "none (scratch: read from " + (live ? options_.database + " database" : options_.savePath) + ")"
                                 : live ? options_.database + " database" : options_.savePath.empty() ? "memory" : options_.savePath) +
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
    if (options_.scratch)
        worldDb_.exec("SET default_transaction_read_only = on");   // Whatever slips through is refused by the database.
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
    if (!options_.scratch)
    {
        watch_ = std::make_unique<watch::Feed>(options_.conninfo, liveWorldId_);
        health_ = health::Recorder::database(options_.conninfo, liveWorldId_);
    }
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
                 line.rfind("story ", 0) == 0 || line.rfind("faction ", 0) == 0 || line.rfind("joinable ", 0) == 0)
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
    if (options_.scratch)
        return;                                     // The DM's actions are for the real server.
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
        else if (kind == "deed.award" || kind == "deed.revoke")
        {
            // Fame (doc 56): a deed awarded (target the doer; payload {kind, weight, cell, beneficiary, detail}), or one
            // revoked (target the deed's id).
            json::Value payload;
            std::string problem;
            if (row.size() > 4 && row[4])
                json::parse(*row[4], payload, problem);
            if (kind == "deed.revoke")
                outcome = revokeDeed(target) ? Result{true, "Revoked.", target} : Result{false, "No such deed.", target};
            else if (const auto* doer = world_.entity(target); !doer || doer->npc)
                outcome = {false, "Award deeds to player characters.", target};
            else
            {
                const auto weight = fame::weightOf(payload.string("weight", "great"));
                const auto id = recordDeed(payload.string("kind", "award"), {target}, payload.string("beneficiary"),
                                           payload.string("cell", doer->cellId), "dm", payload.string("detail").substr(0, 160), weight);
                outcome = id.empty() ? Result{false, "No such kind of deed.", target} : Result{true, "Awarded " + id + ".", target};
            }
            saveSoon();
        }
        else if (kind == "storyteller.decide" || kind == "storyteller.revoke" || kind == "tale.pause" || kind == "tale.resume" ||
                 kind == "tale.stop" || kind == "milestone.credit" || kind == "visitors.sync")
        {
            // Storytellers (doc 58, 11): an application decided (target the account; payload {approve, reason}) or the
            // standing revoked; a tale paused, resumed or stopped (target its id); a milestone credited (payload as
            // creditMilestone); the visitors' list read again.
            json::Value payload;
            std::string problem;
            if (row.size() > 4 && row[4])
                json::parse(*row[4], payload, problem);
            const auto by = row[3] ? *row[3] : std::string("dm");
            if (kind == "storyteller.decide" || kind == "storyteller.revoke")
            {
                const auto s = storytellers_.find(target);
                if (kind == "storyteller.decide" && (s == storytellers_.end() || s->second.state != "applied"))
                    outcome = {false, "No application from that account.", target};
                else if (kind == "storyteller.revoke" && (s == storytellers_.end() || s->second.state != "approved"))
                    outcome = {false, "That account isn't a storyteller.", target};
                else
                {
                    if (kind == "storyteller.decide")
                        decideStoryteller(target, payload.boolean("approve"), by, payload.string("reason"));
                    else
                        revokeStoryteller(target, by, payload.string("reason"));
                    outcome = {true, kind == "storyteller.revoke" ? "Revoked." : payload.boolean("approve") ? "Approved." : "Refused.", target};
                }
            }
            else if (kind == "milestone.credit")
                outcome = creditMilestone(payload);
            else if (kind == "visitors.sync")
            {
                loadStoryVisitors();
                outcome = {true, std::to_string(storyVisitorDefs_.size()) + " story visitors.", target};
            }
            else if (auto* tale = storylines_.find(target); !tale || tale->kind != "tale")
                outcome = {false, "No such tale.", target};
            else
            {
                if (kind == "tale.stop")
                    endTale(*tale, "abandoned");
                else
                    tale->state = kind == "tale.pause" ? "paused" : "running";
                storylines_.touch();
                for (const auto& [who, p] : tale->participants)
                    if (auto* c = clientOf(who); c && p.active())
                        storylineToast(c, "\"" + tale->title + "\" is " + (kind == "tale.pause" ? "paused" : kind == "tale.stop" ? "stopped" : "running again") +
                                              " by the Dungeon Masters.");
                outcome = {true, "Done.", target};
            }
            saveSoon();
        }
        else if (kind == "storyline.give" || kind == "storyline.tick")
        {
            // Storylines (doc 58): one given to a character (payload {template, cast}), or an objective ticked by hand
            // (target the storyline; payload {step, objective}).
            json::Value payload;
            std::string problem;
            if (row.size() > 4 && row[4])
                json::parse(*row[4], payload, problem);
            if (kind == "storyline.give")
                outcome = giveStoryline(target, payload.string("template"), payload.object("cast"), "dm", "");
            else
            {
                const auto ticked = storylines_.tick(target, std::size_t(payload.number("step")), std::size_t(payload.number("objective")), "dm",
                                                     now(), true);
                storylineProgress(ticked);
                outcome = ticked.empty() ? Result{false, "Nothing to tick there.", target} : Result{true, "Ticked.", target};
            }
            saveSoon();
        }
        else if (kind == "npc.protect" || kind == "npc.unprotect")
        {
            // Protected residents (doc 57, 6): marked, or unmarked (even one Atlas or the game's rule protects).
            if (!world_.society().resident(target))
                outcome = {false, "No such resident.", target};
            else
            {
                (kind == "npc.protect" ? unprotectedMarks_ : protectedMarks_).erase(target);
                (kind == "npc.protect" ? protectedMarks_ : unprotectedMarks_).insert(target);
                outcome = {true, kind == "npc.protect" ? "Protected." : "No longer protected.", target};
                saveSoon();
            }
        }
        else if (kind == "project.post" || kind == "project.cancel" || kind == "project.complete" || kind == "project.remove")
        {
            // Town projects (doc 57, 7): one posted (target the town; payload {kind, cell, x, y, title}), or one cancelled
            // with its gifts returned, completed by hand, or taken down (target its id).
            json::Value payload;
            std::string problem;
            if (row.size() > 4 && row[4])
                json::parse(*row[4], payload, problem);
            outcome = kind == "project.post" ? postProject(payload.string("kind"), target, payload.string("cell"), int(payload.number("x", -1)),
                                                           int(payload.number("y", -1)), payload.string("title"), "dm")
                      : kind == "project.cancel"   ? cancelProject(target, "cancelled by the Dungeon Master")
                      : kind == "project.complete" ? completeProject(target)
                                                   : removeProject(target);
        }
        else if (kind == "nickname.drop" || kind == "nickname.restore")
        {
            // Doc 56, 4: a nickname out of use, or back in it (target its id).
            auto* n = fame_.nickname(target);
            if (!n)
                outcome = {false, "No such nickname.", target};
            else
            {
                n->dropped = kind == "nickname.drop";
                if (auto* d = fame_.find(n->deed))
                    d->noNickname = n->dropped;
                record(Character, n->wolf);
                outcome = {true, n->dropped ? "Dropped." : "Restored.", target};
            }
            saveSoon();
        }
        else if (kind == "estate.hold" || kind == "estate.release")
        {
            // Held for a story (doc 54, 4): the target a cell; payload {"days", "reason"}.
            json::Value payload;
            std::string problem;
            if (row.size() > 4 && row[4])
                json::parse(*row[4], payload, problem);
            if (!world_.cell(target))
                outcome = {false, "No such place.", target};
            else if (kind == "estate.release")
            {
                holds_.erase(target);
                outcome = {true, "No longer held.", target};
            }
            else
                outcome = holdPlace(target, wire::number(payload, "days", 7), payload.string("reason"));
            saveSoon();
        }
        else if (kind == "board.remove")
        {
            // A notice taken down by a Dungeon Master (doc 54, 2): the target its id. Audited, as every DM action is.
            const auto* d = documents_.find(target);
            if (!d || d->kind != "notice")
                outcome = {false, "No such notice.", target};
            else
            {
                world_.recordEvent({"notice removed", d->author, {}, {}, 0, 0, {}, 0, 0, target});
                documents_.erase(target);
                saveSoon();
                outcome = {true, "The notice is taken down.", target};
            }
        }
        else if (kind == "artwork.review")
        {
            // A Dungeon Master's decision on an uploaded portrait: payload {"decision": "approve"|"reject", "reason"}.
            Value payload;
            std::string problem;
            json::parse(row[4] ? *row[4] : "{}", payload, problem);
            outcome = reviewArtwork(target, payload.string("decision"), payload.string("reason"));
        }
        else if (kind == "treaty.decide" || kind == "house.decide")
        {
            // A Dungeon Master's word (Phase 9): the target a treaty's ID, or a Chapter's for a House (with its "faction").
            json::Value payload;
            std::string problem;
            if (row.size() > 4 && row[4])
                json::parse(*row[4], payload, problem);
            const bool approve = payload.isObject() && payload.boolean("approve");
            outcome = kind == "treaty.decide" ? decideTreaty(target, approve, "by a Dungeon Master")
                                              : decideHouse(target, payload.string("faction"), approve, "by a Dungeon Master");
        }
        else if (kind == "estate.set" || kind == "estate.clear")
        {
            // A place to let (doc 32, 5.2), the target its cell. Payload: {"name", "kind": "hall"|"warehouse",
            // "landlord": a resident or "treasury", "faction", "rent" (pennies a week), "level" (2..5)}.
            json::Value payload;
            std::string problem;
            if (row.size() > 4 && row[4])
                json::parse(*row[4], payload, problem);
            const auto* cell = world_.cell(target);
            if (!cell)
                outcome = {false, "No such place.", target};
            else if (kind == "estate.clear")
            {
                estates_.unmark(target);
                saveSoon();
                outcome = {true, "It is no longer to let once its lease (if any) ends.", target};
            }
            else
            {
                const auto rent = std::int64_t(wire::number(payload, "rent", 30));
                const int level = std::clamp(int(wire::number(payload, "level", 2)), 2, 5);
                estates_.define({target, payload.string("name", cell->name), payload.string("kind", "hall"), payload.string("landlord", "treasury"),
                                 payload.string("faction", cell->factionClaims.empty() ? std::string() : cell->factionClaims.front()),
                                 std::clamp<std::int64_t>(rent, 1, 100000), level, payload.boolean("individuals"),
                                 std::clamp<std::int64_t>(std::int64_t(wire::number(payload, "night", 0)), 0, 100000)});   // (Individuals too: doc 54, 4.)
                estates_.markAuthored(target);
                saveSoon();
                outcome = {true, cell->name + " is to let.", target};
            }
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
        else if (kind == "economy.steer" || kind == "economy.unsteer")
        {
            // A Dungeon Master's steer on the economy orchestrator (doc 46, Part 10): payload {"kind", "target", "item",
            // "strength", "days", "note"}; its id is "steer-<the action's id>". Unsteer: the target is the steer's id.
            json::Value payload;
            std::string problem;
            if (row.size() > 4 && row[4])
                json::parse(*row[4], payload, problem);
            auto& society = world_.society();
            if (kind == "economy.unsteer")
                outcome = society.unsteer(target) ? Result{true, "The steer has ended.", target} : Result{false, "No such steer in force.", target};
            else if (!payload.isObject())
                outcome = {false, "The steer can't be read.", {}};
            else
            {
                orchestra::Steer steer;
                steer.id = "steer-" + *row[0];
                steer.kind = payload.string("kind");
                steer.target = payload.string("target");
                steer.item = payload.string("item");
                steer.strength = payload.number("strength", 1);
                steer.note = payload.string("note");
                steer.by = row[3] ? *row[3] : std::string();
                const auto days = int(std::clamp(payload.number("days", 7), 0., 1000.));
                const auto done = society.steer(steer, days);
                outcome = {done.ok, done.ok ? "Steer " + steer.id + " set. " + done.message : done.message, steer.id};
            }
            if (outcome.ok)
                saveSoon();
        }
        else if (kind == "npc.kill" || kind == "npc.revive")
        {
            const auto* npc = world_.entity(target);
            outcome = npc && npc->npc ? world_.setDead(target, kind == "npc.kill") : Result{false, "No such NPC.", {}};
        }
        else if (kind == "bandits.call")
        {
            // Bandits called up near a character (doc 33): payload {"count": 1..6}.
            json::Value payload;
            std::string problem;
            if (row.size() > 4 && row[4])
                json::parse(*row[4], payload, problem);
            const int count = payload.isObject() ? int(wire::number(payload, "count", 1)) : 1;
            outcome = world_.entity(target) ? world_.callBandits(target, count)
                                            : Result{false, "They aren't in the world: bandits come only where someone is.", {}};
        }
        else if (kind == "book.storyline")
        {
            // A Dungeon Master ties a Story book to a world storyline (doc 51, Phase 7: the World shelf): payload
            // {"book": id, "storyline": "…"} ("" unties it).
            json::Value payload;
            std::string problem;
            if (row.size() > 4 && row[4])
                json::parse(*row[4], payload, problem);
            outcome = tieBookToStoryline(payload.string("book"), payload.string("storyline"), "dm:" + (row.size() > 3 && row[3] ? *row[3] : std::string("?")));
            outcome.targetId = target;
        }
        else if (kind == "tie.end")
        {
            // A Dungeon Master ends a character's tie (doc 52, Phase 3), as the newcomer or the mentor.
            outcome = endTieOf(target, "dm:" + (row.size() > 3 && row[3] ? *row[3] : std::string("?")));
            outcome.targetId = target;
        }
        else if (kind == "mentor.revoke" || kind == "mentor.restore")
        {
            // A Dungeon Master turns a character's account's mentoring off, or lets it mentor again (doc 52, Phase 2).
            outcome = revokeMentor(accountKey(target), kind == "mentor.revoke", "dm:" + (row.size() > 3 && row[3] ? *row[3] : std::string("?")));
            outcome.targetId = target;
        }
        else if (kind == "report.decide")
        {
            // A Dungeon Master's decision on a report (doc 50, Phase 2): payload {"report": id, "decision": "uphold" |
            // "dismiss", "outcome": "note" | "warning" | "silence", "hours": 1 | 6 | 24 | 72}.
            json::Value payload;
            std::string problem;
            if (row.size() > 4 && row[4])
                json::parse(*row[4], payload, problem);
            outcome = decideReport(payload.string("report"), payload.string("decision"), payload.string("outcome"), int(payload.number("hours", 0)),
                                   "dm:" + (row.size() > 3 && row[3] ? *row[3] : std::string("?")));
            outcome.targetId = target;
        }
        else if (kind == "account.unlock")
        {
            // An account's earned Gift tiers, by a Dungeon Master (doc 49, Phase 5), against one of its characters.
            // Payload: {"tier": "gifted" | "quickened", "op": "grant" | "revoke" | "hold" | "release"}.
            json::Value payload;
            std::string problem;
            if (row.size() > 4 && row[4])
                json::parse(*row[4], payload, problem);
            const auto tier = payload.isObject() ? payload.string("tier") : std::string();
            const auto op = payload.isObject() ? payload.string("op") : std::string();
            outcome = unlockTier(accounts_.ownerOf(target), tier, op, "dm:" + (row.size() > 3 && row[3] ? *row[3] : std::string("?")));
            outcome.targetId = target;
        }
        else if (kind == "character.gift")
        {
            // A Gift given or taken away (docs 33, 43), through the Dungeon Master.
            // Payload: {"gift": a playable family | "", "quickened": bool}.
            json::Value payload;
            std::string problem;
            if (row.size() > 4 && row[4])
                json::parse(*row[4], payload, problem);
            const std::string gift = payload.isObject() ? payload.string("gift") : std::string();
            const bool quickened = payload.isObject() && payload.boolean("quickened");
            const std::string family = gifts::name(gift);
            const auto told = [&](const Entity& e) {
                return gift.empty() ? e.name + " no longer has a Gift."
                                    : e.name + (quickened ? " is Quickened: " + family + ", enormous." : " has the Gift: " + family + ".");
            };
            if (!gift.empty() && !gifts::playable(gift))
                outcome = {false, "No such Gift for a player.", {}};
            else if (auto* online = world_.entity(target); online && !online->npc)
            {
                outcome = world_.giveGift(target, gift, quickened);
                if (outcome.ok)
                {
                    if (auto* c = clientOf(target))
                        system(c, gift.empty() ? "Your Gift has gone quiet." : quickened
                                                                                ? "Your Gift wakes, vast and frightening: you are Quickened (" + family + ")."
                                                                                : "A Gift wakes in you: " + family + ".");
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
        else if (kind == "character.injury")
        {
            // A Dungeon Master's correction or storyline (doc 38, phase 5): payload {"add": type, "severity": 1..3, "side":
            // "left" | "right" | ""} or {"remove": injury id}; online or offline.
            json::Value payload;
            std::string problem;
            if (row.size() > 4 && row[4])
                json::parse(*row[4], payload, problem);
            const std::string add = payload.isObject() ? payload.string("add") : std::string();
            const std::string remove = payload.isObject() ? payload.string("remove") : std::string();
            const int severity = payload.isObject() ? int(wire::number(payload, "severity", 2)) : 2;
            const std::string side = payload.isObject() ? payload.string("side") : std::string();
            if (auto* online = world_.entity(target); online && !online->npc)
            {
                outcome = !add.empty() ? world_.addInjury(target, add, severity, side, "a Dungeon Master")
                          : !remove.empty() ? world_.removeInjury(target, remove) : Result{false, "Add or remove an injury.", {}};
                if (outcome.ok)
                {
                    if (auto* c = clientOf(target))
                        system(c, !add.empty() ? "You find you have an injury: " + outcome.message + "." : "Your " + outcome.message + " is gone.");
                    characters_[target] = *online;
                    outcome.message = online->name + (!add.empty() ? ": " + outcome.message + " given." : ": " + outcome.message + " taken away.");
                }
            }
            else if (auto saved = characters_.find(target); saved != characters_.end())
            {
                auto& e = saved->second;
                if (!add.empty())
                {
                    auto i = injury::given(add, severity, side, "a Dungeon Master");
                    i.id = "injury-dm-" + std::to_string(std::int64_t(world_.calendarDays() * 14400)) + "-" + e.id;
                    i.gotDay = world_.calendarDays();
                    outcome = injury::give(e.injuries, i) ? Result{true, e.name + ": " + injury::name(i) + " given (offline).", {}}
                                                          : Result{false, i.type.empty() ? "No such injury." : "No room for another injury.", {}};
                }
                else if (const auto n = injury::takeAway(e.injuries, remove); !n.empty())
                    outcome = {true, e.name + ": " + n + " taken away (offline).", {}};
                else
                    outcome = {false, "No such injury.", {}};
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
        else if (kind == "npc.move" || kind == "character.move")
        {
            // Put someone on a tile from the LIVE map (doc 34): payload {"cell", "x", "y"}. A character not in the world
            // wakes there.
            json::Value payload;
            std::string problem;
            if (row.size() > 4 && row[4])
                json::parse(*row[4], payload, problem);
            const std::string cell = payload.isObject() ? payload.string("cell") : std::string();
            const double x = payload.isObject() ? wire::number(payload, "x", -1) : -1, y = payload.isObject() ? wire::number(payload, "y", -1) : -1;
            const auto* someone = world_.entity(target);
            if (someone && someone->npc != (kind == "npc.move"))
                outcome = {false, kind == "npc.move" ? "That is a player character." : "That is an NPC.", {}};
            else if (someone)
            {
                outcome = world_.teleport(target, cell, x, y);
                if (outcome.ok && !someone->npc)
                {
                    if (auto* c = clientOf(target))
                        system(c, "You find yourself somewhere else.");
                    characters_[target] = *world_.entity(target);
                }
            }
            else if (auto saved = characters_.find(target); kind == "character.move" && saved != characters_.end())
            {
                const auto* place = world_.ensureLoaded(cell).ok ? world_.cell(cell) : nullptr;
                const auto* tile = place && x >= 0 && y >= 0 ? place->tile(int(x), int(y)) : nullptr;
                if (!tile || tile->solid)
                    outcome = {false, "No one can stand there.", {}};
                else
                {
                    saved->second.cellId = cell;
                    saved->second.position = {std::floor(x) + .5, std::floor(y) + .5};
                    outcome = {true, saved->second.name + " will wake in " + place->name + " (offline).", {}};
                }
            }
            else
                outcome = {false, kind == "npc.move" ? "No such NPC in the world." : "No such character.", {}};
        }
        else if (kind == "character.dm")
        {
            // A player marked a Dungeon Master in the game, or no longer: payload {"dungeonMaster": bool}. They have the
            // Dev Console while they are one.
            json::Value payload;
            std::string problem;
            if (row.size() > 4 && row[4])
                json::parse(*row[4], payload, problem);
            const bool on = payload.isObject() && payload.boolean("dungeonMaster");
            const auto told = [&](const Entity& e) {
                return e.name + (on ? " is a Dungeon Master in the game." : " is no longer a Dungeon Master in the game.");
            };
            if (auto* online = world_.entity(target); online && !online->npc)
            {
                online->dungeonMaster = on;
                characters_[target] = *online;
                if (auto* c = clientOf(target))
                    system(c, on ? "You are a Dungeon Master: the Dev Console is yours (the ` key)." : "You are no longer a Dungeon Master.");
                outcome = {true, told(*online), {}};
            }
            else if (auto saved = characters_.find(target); saved != characters_.end())
            {
                saved->second.dungeonMaster = on;
                outcome = {true, told(saved->second) + " (offline)", {}};
            }
            else
                outcome = {false, "No such character.", {}};
        }
        else if (kind == "visitor.add" || kind == "visitor.leave")
        {
            // Temporary folk from the LIVE map (doc 34), the target their new ID. Payload {"name", "description",
            // "like": a resident whose looks (and, with no description, words) they take, "cell", "x", "y", "minutes"}.
            json::Value payload;
            std::string problem;
            if (row.size() > 4 && row[4])
                json::parse(*row[4], payload, problem);
            if (kind == "visitor.leave")
                outcome = world_.sendVisitorAway(target);
            else if (!payload.isObject())
                outcome = {false, "Say who and where.", {}};
            else
            {
                const auto* like = world_.entity(payload.string("like"));
                const auto description = payload.string("description", like ? like->description : std::string());
                outcome = world_.addVisitor(target, payload.string("name"), description, like ? like->appearance : Appearance{},
                                            payload.string("cell"), wire::number(payload, "x", -1), wire::number(payload, "y", -1),
                                            wire::number(payload, "minutes", 0));
            }
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
             std::to_string(request.age), child ? std::string("0") : std::to_string(Society::AdultPurse), home.cell, x, y, request.templateId});
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
    if (options_.database.empty() || options_.scratch || (spawnAccumulator_ += dt) < 30)
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
                for (const auto& [item, count] : stock.fields())
                    if (itemValid(item) && count.isNumber() && account->second.stock.size() < MaxGoodsKinds)
                        account->second.stock[item] = int(std::clamp(count.asNumber(), 0.0, 10000.0));
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
            if (!character.gift.empty())
            {
                item.add("gift", character.gift);
                item.add("quickened", character.quickened);
            }
            if (const auto* portrait = portraitOf(id))
            {
                item.add("artwork", portrait->id);
                item.add("artworkStatus", portrait->status);
            }
            roster.push(item);
        }
    e.add("characters", roster);
    if (!c->accountUsername.empty())                // What the creator's Gift tab describes (doc 43).
        e.add("gifts", gifts::creatorCatalog());
    if (!c->accountUsername.empty())                // And its Strengths tab: grades, budget, specialties, presets (doc 49).
        e.add("creation", practice::creationCatalog());
    if (!c->accountUsername.empty())                // Which Gift tiers the account has earned, and what the rest need.
        e.add("tiers", tiersView(c->accountUsername));
    if (!c->accountUsername.empty())                // The account as a person: its handle (asked for when it has none).
        e.add("account", accountView(c->accountUsername));
    if (!c->accountUsername.empty())                // Where a new wolf may arrive, the busiest preselected (doc 52, 1).
    {
        e.add("starts", startsView());
        e.add("firstCharacter", accounts_.characters(c->accountUsername).empty());
        auto ties = Value::array();                 // The story starters for its tie, required for a first wolf (doc 52, 4).
        for (const auto& s : newcomers::tieRules().starters)
        {
            auto o = Value::object();
            o.add("id", s.id);
            o.add("line", s.newcomer);
            ties.push(o);
        }
        e.add("ties", ties);
        e.add("tieRequired", accounts_.characters(c->accountUsername).empty() && !newcomers::tieRules().starters.empty() &&
                                 !options_.tiesOptional);
    }
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
    // One wolf per account in the world at a time (doc 49, Phase 5): two would let a player pay and star themselves.
    if (const auto owner = accounts_.ownerOf(actor); options_.oneWolfPerAccount && !owner.empty())
        for (auto* other : clients_)
            if (other != c && !other->entityId.empty() && other->entityId != actor && accounts_.ownerOf(other->entityId) == owner)
            {
                lobby(c, false, "Another of your wolves is in the world. Leave them first.");
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
    auto& player = world_.addPlayer(actor, names::capitalised(name));   // (Older characters too.)
    lastActiveReal_[actor] = now();                 // (Just come in: active, for a common room's company: doc 54.)
    if (!world_.society().account(actor))
    {
        undo();
        lobby(c, false, "Character entry could not allocate a valid economy account; no world change was saved.");
        return false;
    }
    const auto saved = characters_.find(actor);
    if (lingered)
    {
        world_.stopLingering(actor);
        lingering_.erase(actor);
    }
    else if (saved != characters_.end())
    {
        // How long they were gone (read before returnFromAway resets it): three real days or more is a break (doc 56).
        if (saved->second.leftAt > 0 && now() - saved->second.leftAt >= options_.awayBreakSeconds)
        {
            auto& away = absences_[actor];
            away = {(now() - saved->second.leftAt) / 86400, saved->second.awaySince, saved->second.leftAt, {}};
            for (const auto& [resident, life] : world_.society().state().residents)
                if (const auto* bond = world_.bonds().find(resident, actor); bond && bond->lastContact > 0 && bond->lastContact < away.leftDay)
                    away.unseen[resident] = bond->lastContact;   // (Who hasn't seen them since: greeted as long gone, once.)
        }
        else
            absences_.erase(actor);
        player = saved->second;
        world_.returnFromAway(player);             // Down or resting while away (doc 38).
    }
    else
    {
        player.description = "A road-worn quadrupedal wolf with a small shoulder satchel. Their coat and history are yours to imagine.";
        player.speakingColor = int(characters_.size() * 9) % 32;
    }
    world_.bonds().setAway(actor, false);           // (Their regard fades again from now: doc 56, 10.)
    world_.fitWorn(actor);                          // Nothing worn or held that the purse no longer has (doc 35).
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
    sendProfile(c);                                // Their profile and account as a person (doc 50).
    tiesOnEnter(actor);                            // A tie made while they were away (doc 52, 4).
    sendSafety(c);                                 // And their mutes and blocks.
    cameOrWent(c, true);                           // Their friends, and private messages kept for them (doc 50, 4).
    if (const auto away = absences_.find(actor); away != absences_.end())
    {
        // Back after a break (doc 56, 10): what happened while they were gone, from the log where there is a database.
        std::string problem;
        if (store_ && !options_.conninfo.empty() && !liveWorldId_.empty() && chronicleReader_.start(options_.conninfo, liveWorldId_, problem))
            chronicleReader_.askSince(actor, away->second.leftDay);
        else
        {
            std::vector<chronicle::Row> rows;
            for (const auto& e : world_.recentEvents())
                if (e.day >= away->second.leftDay)
                    rows.push_back({e.day, e.kind, e.actor, e.target, e.cell, e.item, e.detail, e.quantity, e.coins});
            sendWelcome(actor, rows);
        }
    }
    note("info", "RATW_LOGIN " + c->entityId + " connected=" + std::to_string(clients_.size()));
    return true;
}

void Game::leaveCharacter(Connection* c)
{
    const auto id = c->entityId;
    const bool had = !id.empty();
    if (had)
        cameOrWent(c, false);                      // (Their friends' lists change: doc 50, 4.)
    if (auto* e = world_.entity(id))
    {
        e->typing = false;
        world_.stop(e->id);
        if (const auto here = townFor(e->cellId); !here.empty())
            e->postTown = here;                     // (Its letters wait where it left: doc 55.)
        characters_[e->id] = *e;
        characters_[e->id].awaySince = world_.calendarDays();   // Away time counts as rest (doc 38).
        characters_[e->id].leftAt = now();          // (How long they were gone, for welcome back: doc 56, 10.)
        world_.bonds().setAway(e->id, true);
        characters_[e->id].awayInBed = world_.bedIsTheirs(*e);   // (Only a bed it has a right to rests it fully: doc 54.)
        characters_[e->id].awayAtInn = innCells_.count(e->cellId) > 0 || (lodgingOf(e->id) && innUpstairs(e->cellId) && lodgingOf(e->id)->cell == e->cellId);
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
            characters_[e->id].awaySince = world_.calendarDays();
            characters_[e->id].leftAt = now();
            world_.bonds().setAway(e->id, true);
            characters_[e->id].awayInBed = world_.bedIsTheirs(*e);
            characters_[e->id].awayAtInn = innCells_.count(e->cellId) > 0;
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
        type != "character_leave" && type != "auth_logout" && type != "account_handle")
        return false;
    if (type == "account_handle")
    {
        // The roster asks an account without a handle for one (doc 50, 1): the name friends and circles see.
        if (c->accountUsername.empty() || !c->entityId.empty())
            return false;
        const auto done = setHandle(c->accountUsername, c->accountUsername, j.string("handle"));
        lobby(c, done.ok, done.message);
        return true;
    }
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
        allowed.insert({"name", "age", "appearance", "gift", "build", "start", "tie"});   // (A build: doc 49; a start town, a tie: doc 52.)
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
        // A name always starts with a capital (the user, 2026-10-07): it reads as a name, and it is veiled from those
        // who don't know it wherever it is written (names::veil looks for capitalised names).
        const std::string name = names::capitalised(mind::trim(j.string("name")));
        const double age = wire::strictNumber(j, "age", -1);
        Appearance appearance;
        const std::string commandId = j.string("commandId");
        if (!accounts::validDisplayName(name) || age < 6 || age > 99 || age != std::floor(age) ||
            !wire::readAppearance(j["appearance"], appearance) || !accounts::validCommandId(commandId))
        {
            lobby(c, false, "Choose a 2\xe2\x80\x93" "32 character name, a whole age from 6\xe2\x80\x93" "99, and a complete valid wolf appearance.");
            return true;
        }
        // The Gift (doc 43): {"tier": "normal" | "gifted" | "quickened", "family": a playable family}; none is Normal.
        std::string tier = "normal", family;
        if (const auto* gift = j.find("gift"))
        {
            bool fine = gift->isObject() && gift->size() <= 2;
            for (const auto& [key, v] : gift->fields())
                fine = fine && (key == "tier" || key == "family") && v.isString();
            tier = fine ? gift->string("tier") : std::string();
            family = fine ? gift->string("family") : std::string();
            if (!fine || (tier != "normal" && tier != "gifted" && tier != "quickened") || (tier == "normal") != family.empty() ||
                (!family.empty() && !gifts::playable(family)))
            {
                lobby(c, false, "Choose Normal, or Gifted or Quickened with one of the eight Gift families.");
                return true;
            }
        }
        // Earned Gift tiers (doc 49, Phase 5): a tier the account hasn't opened is refused, with what it still needs.
        if (tier != "normal" && !options_.openTiers)
        {
            const auto record = standing_.find(c->accountUsername);
            const standing::Record r = record == standing_.end() ? standing::Record{} : record->second;
            const bool open = standing::open(r, "gifted") && (tier == "gifted" || standing::open(r, "quickened"));
            if (!open)
            {
                const auto m = measuresOf(c->accountUsername);
                lobby(c, false, standing::lockedMessage(standing::open(r, "gifted") ? tier : "gifted", m, standing::thresholds()));
                return true;
            }
        }
        // The start town (doc 52, 1): any of this world's start towns, the busiest when none is named; refused if it
        // names one the world doesn't offer. A world without start towns starts everyone at its spawn.
        const auto towns = startTowns();
        std::string start;
        if (const auto* given = j.find("start"); given && !(given->isString() && given->asString({}).empty()))
        {
            start = given->isString() ? given->asString({}) : std::string("?");
            if (std::find(towns.begin(), towns.end(), start) == towns.end())
            {
                lobby(c, false, towns.empty() ? "This world has one place to arrive: leave the start town out."
                                              : "Choose one of the start towns to arrive in.");
                return true;
            }
        }
        // The tie (doc 52, 4): a story starter; a new account's first wolf must take one, later ones may not.
        const std::string tie = j.find("tie") && j.find("tie")->isString() ? j.string("tie") : std::string();
        const bool tieRequired = accounts_.characters(c->accountUsername).empty() && !newcomers::tieRules().starters.empty() &&
                                 !options_.tiesOptional;
        if ((j.find("tie") && !j.find("tie")->isString()) || (!tie.empty() && !newcomers::starter(tie)))
        {
            lobby(c, false, "Choose one of the story starters for your tie.");
            return true;
        }
        if (tie.empty() && tieRequired)
        {
            lobby(c, false, "A first wolf takes a tie: choose a story starter that links them to someone here.");
            return true;
        }
        // The build (doc 49, Phase 4): grades and a specialty within the budget; none is a plain wolf with none.
        practice::Build build;
        if (const auto* given = j.find("build"))
        {
            std::string why;
            if (!practice::checkBuild(*given, tier, build, why))
            {
                lobby(c, false, why);
                return true;
            }
        }
        auto canonical = Value::object();
        canonical.add("name", name);
        canonical.add("age", age);
        canonical.add("appearance", wire::appearance(appearance));
        if (tier != "normal")                       // (Only then, so a Normal wolf's print is what it always was.)
        {
            auto gift = Value::object();
            gift.add("tier", tier);
            gift.add("family", family);
            canonical.add("gift", gift);
        }
        if (!build.grades.empty() || !build.specialty.empty())   // (Likewise: a plain wolf's print is unchanged.)
        {
            auto b = Value::object();
            auto grades = Value::object();
            for (const auto& [attribute, grade] : build.grades)
                grades.add(attribute, grade);
            b.add("grades", grades);
            b.add("specialty", build.specialty);
            canonical.add("build", b);
        }
        if (!start.empty())                         // (Only when named, so an older client's print is unchanged.)
            canonical.add("start", start);
        if (!tie.empty())
            canonical.add("tie", tie);
        if (start.empty())
            start = suggestedStart();
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
        std::string arriveCell;
        Vec2 arriveAt;
        if (!start.empty())
            for (const auto& s : newcomers::rules().starts)
                if (s.id == start && !world_.arrivalIn(start, arriveCell, arriveAt, s.cell, {s.x, s.y}))
                    arriveCell.clear();             // (No spot will do there: the spawn, as ever.)
        auto& player = arriveCell.empty() ? world_.addPlayer(newId, name) : world_.addPlayer(newId, name, arriveCell, arriveAt);
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
        World::applyBuild(player, build);
        if (auto& person = personOf(c->accountUsername); person.firstCharacter.empty())
            person.firstCharacter = newId;          // (The account's first wolf: doc 52's newcomers.)
        if (!family.empty())
            world_.giveGift(newId, family, tier == "quickened");
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
        if (!tie.empty())                           // (Seeking at once: a mentor may be found while they finish up.)
            startTie(newId, c->accountUsername, tie, start, characters_[newId].cellId);
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
    name = names::capitalised(name);                // (Always a capital, as a created character's.)
    enterCharacter(c, "player-" + id, name, id);
}

// --------------------------------------------------------------------------- The world, twenty times a second

void Game::setSpeed(double speed)
{
    speed_ = std::isfinite(speed) ? std::clamp(speed, 1.0, MaxSpeed) : 1.0;
}

void Game::tick(double dt)
{
    perf::Scope timed(meter_, perf::TickOther);
    // Fast-forward (setSpeed): the world's time is `dt`, real time `real`; what goes to the clients goes once a real
    // twentieth of a second.
    const double real = dt / speed_;
    frameDebt_ += 1 / speed_;
    const bool frame = frameDebt_ >= 1 - 1e-9;
    if (frame)
        frameDebt_ = std::max(0.0, frameDebt_ - 1);
    keepOwnership(real);
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
    ambient(real);                                  // (Voices look for something to say by real time.)
    barks(real);
    sermons(real);
    // What happened to players that no action of theirs answered (a bandit's blow, a caravan arriving...).
    for (const auto& [who, words] : world_.takeNotices())
        if (auto* c = clientOf(who))
            system(c, words);
    for (const auto& [id, cell] : beforeCells)
        if (const auto* e = world_.entity(id); e && e->cellId != cell)
        {
            followTransition(id, cell);
            payToll(id);                              // Into a Hold's claimed place (doc 32, 5.5).
        }
    for (auto& [id, until] : typingExpiry_)
        if (until <= world_.time())
            if (auto* e = world_.entity(id))
                e->typing = false;
    companionTick(dt);                              // Residents travelling with a party (doc 32, Phase 3).
    partyTick(dt);
    refreshSocialViews(real);
    tendPeople(real);                               // Played time (doc 50).
    tendNewcomers(real);                            // Start towns' counts; newcomers no longer new (doc 52).
    tendTies(real);                                 // Ties offered to mentors, made, lapsing (doc 52, 4).
    tendLetters(dt);                                // The courier's arrivals; letters collected at inns (doc 55).
    chapterTick(dt);
    factionTick(dt);
    estateTick(dt);
    campTick(dt);
    projectTick(dt);                                // (Town projects: doc 57, 4.)
    storylineTick(dt);                              // (Storylines' places, chains, idle tales: doc 58.)
    tendStorytellers(dt);                           // (Visitors, the kept log, credits: doc 58.)
    holdTick(dt);
    refreshChapterViews(real);
    refreshLabels(real);
    snapshotAccumulator_ += real;
    // Small observer-filtered poses at simulation cadence; full snapshots five times a second, and at once on a
    // change of cell.
    if (frame)
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
    saveAccumulator_ += real;
    ambientAccumulator_ += real;
    if (snapshotAccumulator_ >= 0.2)
    {
        snapshotAccumulator_ = 0;
        movementSounds();
        updateMovementModes();
        releaseLingering();
        tendFightScenes();
        tendWorkScenes();                           // (Working together is a scene: doc 53.)
        tendHowls();                                // (Choruses closing: doc 51, Phase 6.)
    }
    // A quarter of the clients' snapshots in each tick (by a phase fixed per connection).
    if (frame)
    {
        snapshotPhase_ = (snapshotPhase_ + 1) % SnapshotPhases;
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
    watchReleases(real);
    applyDmActions(real);
    feedWatch(real);
    runSpawns(dt);
    makeResidents();
    if (prefetching && (prefetchAccumulator_ += dt) >= 1)
    {
        prefetchAccumulator_ = 0;
        prefetcher().Want(world_.cellsSoonNeeded());
    }
    if (streamedBuild_ && (streamLogAccumulator_ += real) >= 60)
    {
        streamLogAccumulator_ = 0;
        note("info", "RATW_STREAM loaded=" + std::to_string(world_.loadedCells()) + " of " + std::to_string(world_.cells().size()) +
                         " cells; " + std::to_string(prefetcher().HitCount()) + " loads found ready");
    }
    if (saveSoonIn_ >= 0 && (saveSoonIn_ -= real) < 0)
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
    if ((snapshotSaveAccumulator_ += real) >= SnapshotSeconds)
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
        director_.tick(real, world_, characters_, online, operatorActivity_, revision_,
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
Value Game::storesView(const std::string& cellId) const
{
    // Whose they are (the household, by its first two names) and what is in them.
    auto list = Value::array();
    const auto spots = world_.homeStoreSpots().find(cellId);
    if (spots == world_.homeStoreSpots().end())
        return list;
    std::vector<std::string> household;
    for (const auto& [id, life] : world_.society().state().residents)
        if (life.homeCell == cellId)
            if (const auto* e = world_.entity(id))
                household.push_back(e->name);
    std::string owner = household.empty() ? std::string("No one's") : household[0];
    if (household.size() == 2)
        owner += " and " + household[1];
    else if (household.size() > 2)
        owner += " and " + std::to_string(household.size() - 1) + " others";
    if (!household.empty())
        owner += "'s";
    for (const auto& [kind, spot] : spots->second)
    {
        const auto* account = world_.society().account(Society::homeStore(cellId, kind));
        std::string contents;
        if (account)
            for (const auto& [item, count] : account->stock)
                if (count > 0)
                    contents += (contents.empty() ? "" : ", ") + std::to_string(count) + " " +
                                (item == "meal" ? count == 1 ? "meal" : "meals" : item == "herbs" ? "herbs" : item + (count == 1 ? "" : "s"));
        auto o = Value::object();
        o.add("id", Society::homeStore(cellId, kind));
        o.add("kind", kind);
        o.add("x", spot.x);
        o.add("y", spot.y);
        o.add("owner", owner);
        o.add("contents", contents.empty() ? std::string("empty") : contents);
        list.push(o);
    }
    return list;
}

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
        // The account's social standing (doc 49): its characters' social XP together, the level it makes, and where
        // that level began and the next begins, for the bar. (Read only: views are built in parallel.)
        const long long xp = socialXp(id);
        const int level = practice::levelFor(xp);
        self.set("socialXp", double(xp));
        self.set("socialXpLevel", double(practice::xpFor(level)));
        self.set("socialXpNext", double(practice::xpFor(level + 1)));
    }
    if (const auto view = socialViews_.find(id); view != socialViews_.end())
        self.set("social", view->second);             // Scene, stars, Stories, title (doc 32, Part 1).
    if (const auto view = chapterViews_.find(id); view != chapterViews_.end())
        self.set("chapter", view->second);            // Their Chapter (doc 32, Part 3).
    if (auto tie = tieView(id); !tie.isNull())
        self.set("tie", std::move(tie));              // Their tie: the starter, the other, the marker (doc 52, 4).
    if (auto place = placeView(id); !place.isNull())
        self.set("place", std::move(place));          // A place to let, where they stand (doc 32, 5.2).
    if (auto camp = campView(id); !camp.isNull())
        self.set("camp", std::move(camp));            // Their Chapter's ground, where they stand (doc 32, 5.3).
    self.set("socialLevel", socialLevel(id));
    if (const auto tiers = tiersViews_.find(accounts_.ownerOf(id)); tiers != tiersViews_.end())
        self.set("tiers", tiers->second);           // The account's earned Gift tiers (doc 49, Phase 5).
    const auto hurtSenses = injury::effects(view.self.injuries);     // (Injuries, doc 38.)
    self.set("hearing", view.self.hearing * view.self.earHealth * hurtSenses.hearing * ageHearingFactor(view.self) * (1.0 + 0.75 * view.self.hearingSkill / 100.0));
    self.set("sneakSkill", view.self.sneakSkill);
    self.set("hearingSkill", view.self.hearingSkill);
    self.set("vision", view.self.vision * view.self.eyeHealth * hurtSenses.vision * ageVisionFactor(view.self));
    self.set("smell", view.self.smell * view.self.noseHealth * hurtSenses.smell * (1.0 + 0.75 * view.self.scentSkill / 100.0));
    if (const double masked = view.self.scentMaskedUntil - world_.time(); masked > 0)
        self.set("scentMasked", masked);           // Masking oil (doc 35): seconds its scent stays hidden.
    self.set("scentSkill", view.self.scentSkill);
    self.set("noseHealth", view.self.noseHealth);
    wire::practiceView(self, view.self, now());     // Attributes and skills, with their caps (doc 49).
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
        // What it carries (doc 35, 1.2): pounds against what is comfortable, and what the load costs.
        const auto load = world_.loadOf(*me);
        auto l = Value::object();
        l.add("carried", std::round(load.carried * 10) / 10);
        l.add("comfortable", std::round(load.comfortable * 10) / 10);
        l.add("state", load.state);
        l.add("pace", load.pace);
        l.add("drain", std::round(load.drain * 100) / 100);
        self.set("load", l);
        self.set("sightRange", world_.sightRange(*me));    // (For a client shading the terrain itself.)
        self.set("moveFactor", world_.environmentAt(me->cellId, me->position).movement);
    }
    // Health and being Downed (doc 33): how hurt, how long they have, whether they can get up by themselves today.
    if (const auto* me = world_.entity(id))
    {
        self.set("health", std::round(100 - me->hurt));
        self.set("fightingSkill", std::round(me->fightingSkill));   // Their own, grown by fighting (doc 49).
        if (!me->mouth.empty())
            self.set("mouth", me->mouth);
        // What is worn (doc 35): slot to item id, and each piece of jewellery as [spot, item id]; names are in the inventory.
        if (!me->worn.empty())
        {
            auto worn = Value::object();
            for (const auto& [slot, item] : me->worn)
                worn.add(slot, item);
            self.set("worn", std::move(worn));
        }
        if (!me->jewellery.empty())
        {
            auto jewellery = Value::array();
            for (const auto& [spot, item] : me->jewellery)
            {
                auto piece = Value::array();
                piece.push(spot);
                piece.push(item);
                jewellery.push(std::move(piece));
            }
            self.set("jewellery", std::move(jewellery));
        }
        if (me->dungeonMaster)
            self.set("dungeonMaster", true);       // The Dev Console is theirs (RatwGameDev.cpp).
        if (me->noPvp)
            self.set("noPvp", true);               // Auto-decline fights with players (doc 40).
        if (me->noHuntPartners)
            self.set("noHuntPartners", true);      // (Doc 53: missing means on.)
        if (me->noWorkPartners)
            self.set("noWorkPartners", true);
        if (world_.trainingGround(me->cellId))
            self.set("trainingGround", true);       // (Practise at the post: doc 53, 5.)
        if (!me->witnessed.empty())
        {
            // Quickened partners whose magic this wolf saw (doc 53, 4), as it knows them, for the Wardens' menu.
            auto seen = Value::array();
            for (const auto& [wolf, day] : me->witnessed)
            {
                auto row = Value::object();
                row.add("id", wolf);
                row.add("name", names::capitalised(labelFor(id, wolf)));
                row.add("told", me->toldWardens.count(wolf) > 0);
                seen.push(row);
            }
            self.set("witnessed", seen);
        }
        if (world_.keepingWatch(id))
            self.set("keepingWatch", true);         // (Keep watch, doc 53, 4.)
        // Well-groomed (doc 55, 7): by whom, whether at half, how long it lasts; and a grooming asked of this wolf.
        if (const double f = world_.groomedFactor(*me); f > 0)
        {
            auto o = Value::object();
            o.add("by", me->groomedBy == id ? std::string("yourself") : names::capitalised(labelFor(id, me->groomedBy)));
            o.add("half", me->groomHalf);
            o.add("hours", std::round((me->groomedUntil - world_.calendarDays()) * 24 * 10) / 10);
            o.add("scent", me->groomScentUntil > world_.calendarDays());
            self.set("groomed", o);
        }
        // Loans (doc 55, 8): a loan offered to this wolf, and what it has borrowed and lent.
        if (const auto lend = lendOffers_.find(id); lend != lendOffers_.end() && lend->second.until > world_.time())
        {
            auto o = Value::object();
            o.add("from", lend->second.from);
            o.add("name", names::capitalised(labelFor(id, lend->second.from)));
            o.add("what", goodsWords(lend->second.item, lend->second.quantity, 0));
            o.add("days", lend->second.days);
            o.add("left", std::ceil(lend->second.until - world_.time()));
            self.set("lendOffer", o);
        }
        {
            auto loans = Value::array();
            for (const auto& l : loans_)
                if (l.borrower == id || l.lender == id)
                {
                    auto o = Value::object();
                    o.add("id", l.id);
                    o.add("borrowed", l.borrower == id);
                    o.add("who", names::capitalised(labelFor(id, l.borrower == id ? l.lender : l.borrower)));
                    o.add("what", goodsWords(l.item, l.quantity, 0));
                    o.add("days", std::round((l.due - world_.calendarDays()) * 10) / 10);
                    loans.push(o);
                }
            if (!loans.items().empty())
                self.set("loans", loans);
        }
        // A common room (doc 54, 1): one is in it, its company while resting, and whether one performs.
        if (const auto lodging = lodgingView(id); !lodging.isNull())
            self.set("lodging", lodging);           // (Beds, nights and one's lodging: doc 54, 4.)
        if (!boardNear(id).empty())
            self.set("nearBoard", true);            // (Read the board: doc 54, 2.)
        if (const auto stall = stallSelf(id); !stall.isNull())
            self.set("stall", stall);               // (One's market stall, one within reach, or a spot to rent: doc 54, 3.)
        if (archiveKeeperHere(me->cellId))
        {
            auto archive = Value::object();          // (A keeper of records here: archive work, doc 54, 7.)
            const auto task = archiveTasks_.find(id);
            archive.add("task", task == archiveTasks_.end() ? std::string() : task->second.kind);
            if (task != archiveTasks_.end() && task->second.kind == "copy")
                archive.add("copied", std::round(std::min(1., (world_.time() - task->second.begun) / 300) * 100));
            self.set("archive", archive);
        }
        if (const auto open = unfinishedView(id); !open.items().empty())
            self.set("unfinished", open);           // (Open threads, never a nag: doc 56, 9.)
        if (const auto journal = journalView(id); !journal.items().empty())
            self.set("journal", journal);           // (Storylines and tales: doc 58, 2.)
        if (const auto tracked = trackedView(id); !tracked.isNull())
            self.set("tracked", tracked);
        if (auto teller = storytellerSelf(id); teller.has("state") || teller.boolean("canApply") || teller.has("calls"))
            self.set("storyteller", teller);       // (Standing, tales, calls to answer: doc 58; nothing when there's nothing.)
        if (!projects_.all().empty())
        {
            if (const auto* here = projectAt(id))
                self.set("project", projectView(id, *here, true));   // (A town project's site: doc 57, 4.)
            if (const auto town = world_.communityOf(me->cellId); !town.empty())
                if (auto list = projectsView(id, town); !list.items().empty())
                    self.set("townProjects", list);  // (The town's projects, for its board.)
        }
        if (const auto nicknames = nicknamesView(id); !nicknames.items().empty())
            self.set("nicknames", nicknames);       // (What residents call them, and who first did: doc 56, 4.)
        if (const auto festival = festivalSelf(id); !festival.isNull())
            self.set("festival", festival);         // (A festival today or soon: its programme, and what one does now: doc 54, 6.)
        if (const auto table = tableSelf(id); !table.isNull())
            self.set("table", table);               // (A game at a table, one to watch, or a table to play at: doc 54, 5.)
        if (innCells_.count(me->cellId))
        {
            self.set("commonRoom", true);
            self.set("company", std::round(world_.companyFactor(*me) * 100) / 100);
        }
        if (performers_.count(id))
            self.set("performing", performers_.at(id).kind);
        if (me->fedUntil > world_.calendarDays())
            self.set("fedHours", std::round((me->fedUntil - world_.calendarDays()) * 24 * 10) / 10);   // (Fed: doc 55, 6.)
        if (const auto ask = groomOffers_.find(id); ask != groomOffers_.end() && ask->second.until > world_.time())
        {
            auto o = Value::object();
            o.add("from", ask->second.from);
            o.add("name", names::capitalised(labelFor(id, ask->second.from)));
            o.add("left", std::ceil(ask->second.until - world_.time()));
            self.set("groomOffer", o);
        }
        // A gift offered to this wolf (doc 55, 3): by whom, what, how long it has to answer.
        if (const auto offer = giveOffers_.find(id); offer != giveOffers_.end() && offer->second.until > world_.time())
        {
            auto o = Value::object();
            o.add("from", offer->second.from);
            o.add("name", names::capitalised(labelFor(id, offer->second.from)));
            o.add("what", goodsWords(offer->second.item, offer->second.quantity, offer->second.coins));
            o.add("left", std::ceil(offer->second.until - world_.time()));
            self.set("giveOffer", o);
        }
        // The work one is in with others (doc 53, 2.2): what, who in which role, each one's rate, beats so far.
        if (const auto* joint = world_.jointOf(id))
        {
            auto o = Value::object();
            const auto* act = together::activity(joint->kind);
            o.add("kind", joint->kind);
            o.add("name", act ? act->name : joint->kind);
            o.add("rate", std::round(world_.workRate(id) * 100) / 100);
            auto members = Value::array();
            for (const auto& m : joint->members)
            {
                auto row = Value::object();
                row.add("id", m.id);
                row.add("name", m.id == id ? std::string("you") : names::capitalised(labelFor(id, m.id)));
                std::string words = m.role;
                if (act)
                    for (const auto& r : act->roles)
                        if (r.id == m.role)
                            words = m.id == id ? r.yours : r.words;
                if (m.watch)
                    words = m.id == id ? "keep watch" : "keeps watch";   // (Doc 53, 4.)
                if (m.giftUntil > world_.time())
                    if (const auto* g = gifts::ability(m.gift))
                        words += " · " + g->name;
                row.add("role", words);
                row.add("beats", m.beats);
                members.push(row);
                if (m.id == id && act && act->byBeat())
                    o.add("earned", m.earned);
            }
            o.add("members", members);
            // Farm work: whose it is, and how long until the next spell is counted (and paid).
            if (act && act->byBeat())
            {
                o.add("farmer", names::capitalised(labelFor(id, joint->target)));
                o.add("nextBeat", std::max(0., std::round(joint->nextBeat - world_.time())));
            }
            self.set("work", o);
        }
        // One's share of one's last hunt, for the end card's Give my share (doc 53, 1.5).
        if (const auto* share = world_.huntShareOf(id))
        {
            auto o = Value::object();
            auto goods = Value::array();
            for (const auto& [kind, n] : share->goods)
                goods.push(std::to_string(n) + " " + Society::itemName(kind));
            o.add("goods", goods);
            auto hunters = Value::array();
            for (const auto& h : share->hunters)
            {
                auto row = Value::object();
                row.add("id", h);
                row.add("name", names::capitalised(labelFor(id, h)));
                hunters.push(row);
            }
            o.add("hunters", hunters);
            self.set("huntShare", o);
        }
        if (!me->gift.empty())
        {
            self.set("gift", me->gift);
            self.set("quickened", me->quickened);
            self.set("mana", std::floor(me->mana));
            self.set("manaMax", std::round(battle::manaMax(me->wisdom, true)));
            // Its Gift's ways of working (doc 43), for the character sheet: what each does, and its mana.
            auto work = Value::array();
            for (const auto* a : gifts::abilities(me->gift, me->quickened))
                if (a->work)
                {
                    auto w = Value::object();
                    w.add("id", a->id);
                    w.add("name", a->name);
                    w.add("summary", a->summary);
                    w.add("mana", a->mana);
                    w.add("passive", a->kind == "passive");
                    work.push(std::move(w));
                }
            self.set("giftWork", std::move(work));
            if (me->wardenAttention > 0)
                self.set("wardenAttention", std::round(me->wardenAttention));
        }
        if (const auto* purse = world_.society().account(id); purse && Society::stockAll(*purse, "sword") > 0)
            self.set("swords", Society::stockAll(*purse, "sword"));   // (Of any kind: doc 35, Part 4.)
        if (me->downedLeft > 0)
        {
            self.set("downedLeft", std::round(me->downedLeft));
            self.set("canStruggle", world_.recoveryAvailable(*me) && me->struggleUntil <= 0);
            self.set("struggling", me->struggleUntil > 0);
        }
        // Rest (doc 38): downings since a full rest, and the rest under way (in a bed, toward a full one).
        if (me->downsSinceRest > 0)
            self.set("downsSinceRest", me->downsSinceRest);
        // Injuries that outlast a fight (doc 38, phases 3 and 4): each as the sheet tells it, and what it does.
        if (!me->injuries.empty())
        {
            auto list = Value::array();
            for (const auto& i : me->injuries)
            {
                auto o = Value::object();
                o.add("id", i.id);
                o.add("kind", i.kind);
                o.add("name", injury::name(i));
                o.add("line", injury::describe(i));
                o.add("does", injury::does(i));
                if (i.kind == "acute")
                {
                    o.add("severity", injury::severityWord(injury::severityNow(i)));
                    o.add("daysLeft", std::round(i.restLeft / 24 * 10) / 10);
                }
                list.push(o);
            }
            self.set("injuries", list);
        }
        if (me->restRun > 0)
        {
            const bool bed = world_.bedIsTheirs(*me);   // (A bed it has a right to: doc 54.)
            auto rest = Value::object();
            rest.add("hours", std::floor((bed ? me->bedRun : me->restRun) * 10) / 10);
            rest.add("bed", bed);
            rest.add("full", battle::FullRestHours);
            rest.add("rate", std::round(world_.restRate(*me) * 100) / 100);
            if (world_.inBed(*me) && !bed)
                rest.add("notYours", true);         // ("Not your bed: a partial rest.")
            if (world_.companyFactor(*me) > 0)
                rest.add("room", true);
            self.set("rest", std::move(rest));
        }
    }
    self.set("names", namesView(id));                 // Their name and aliases (doc 32).
    if (const auto letters = lettersSelf(id); !letters.isNull())
        self.set("letters", letters);              // Unread letters, and those waiting at inns (doc 55).
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
    std::map<std::string, std::string> called = strangerNames(id);
    const auto viewerAccount = accountKey(id);
    const bool viewerNew = isNewcomer(viewerAccount);
    const auto knownList = knownWolves_.find(id);
    for (const auto& e : view.entities)
        if (!called.count(e.id))
            called[e.id] = labelFor(id, e.id);
    for (const auto& e : view.entities)
    {
        auto j = wire::entity(e, view.time);
        if (e.id != id)
        {
            j.set("name", names::capitalised(called[e.id]));
            if (options_.hiddenNames && !knowsName(id, e.id))
                j.set("known", false);
        }
        // A newcomer (doc 52, 2): a small mark by the label. A mentor: "mentor" for everyone, and for a newcomer, a mark
        // on the map when they're free to take one (doc 52, 3).
        if (!e.npc)
        {
            const auto account = accountKey(e.id);
            if (isNewcomer(account))
                j.set("nc", true);
            if (const auto person = people_.find(account); person != people_.end() && person->second.mentor.on)
            {
                j.set("mentor", true);
                if (viewerNew && e.id != id && availableMentor(account))
                    j.set("mentorFree", true);
            }
        }
        // A player's status mark, Currently line, walk-up and profile revision (doc 50), only when set.
        if (!e.npc)
            if (const auto p = profiles_.find(e.id); p != profiles_.end())
            {
                if (const auto mark = people::rules().statusMarks.find(p->second.status); mark != people::rules().statusMarks.end())
                    j.set("rp", mark->second);
                if (!p->second.currently.empty())
                    j.set("currently", veilFor(id, p->second.currently));
                if (p->second.walkup)
                    j.set("walkup", true);
                if (p->second.revision > 0)
                    j.set("prev", p->second.revision);
            }
        // Known wolves (doc 50, 5): a note kept on them, and a profile changed since this wolf last read it.
        if (e.id != id && knownList != knownWolves_.end())
            if (const auto k = knownList->second.find(e.id); k != knownList->second.end())
            {
                if (!k->second.note.empty())
                    j.set("noted", true);
                if (!e.npc)
                    if (const auto p = profiles_.find(e.id); p != profiles_.end() && p->second.revision > k->second.readRevision)
                        j.set("unread", true);
            }
        // A friend who shares their character with this one: their handle, under their label (doc 50, 4).
        if (!e.npc && e.id != id)
            if (const auto handle = sharedHandle(viewerAccount, e.id); !handle.empty())
                j.set("handle", handle);
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
        if (e.groomedUntil > world_.calendarDays() && !e.groomHalf)
            j.set("groomed", true);                 // "Freshly groomed", to anyone who looks (doc 55, 7).
        if (auto gear = gearView(e); !gear.items().empty())
            j.set("gear", std::move(gear));         // Armour and weapons, plain to see (doc 35): the card's little doll.
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
                if (e.age >= battle::YoungestFighter)
                    actions.push("attack");             // Never a youngster (doc 33: they don't fight; they run).
            }
            if (near(e) && world_.churchCares(view.self.id, e.id))
                actions.push("ask for the church's care");   // (Doc 42: free care for a poor wolf's wounds.)
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
                // Goods one has taken on to bring (doc 35, Part 7), handed in at any merchant of the town.
                for (const auto* k : world_.deliverable(view.self.id))
                    actions.push("hand in goods " + k->id);   // ("deliver" is a Chapter mission's.)
            }
            // Ask to learn their trade: close by, a master with no apprentice, and not already learning one.
            if (const auto* job = world_.society().jobOf(e.id);
                job && job->role != "guard" && !e.dead && world_.society().state().careers.positions.at(job->id).apprentice.empty() &&
                !world_.society().apprenticedTo(view.self.id) && near(e))
                actions.push("apprentice");
        }
        // Fights (doc 33): a player close by may be challenged; anyone lying Downed close by (out of a fight) tended.
        const double apart = std::hypot(e.position.x - view.self.position.x, e.position.y - view.self.position.y);
        if (!e.npc && e.id != view.self.id && !e.dead && e.downedLeft <= 0 && apart <= battle::StartReach && !e.noPvp)
            actions.push("challenge");               // (Not one who auto-declines fights with players.)
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
                if (const auto* head = chapters_.member(view.self.id); head && head->rank == chapter::RankHead)
                {
                    if (chapters_.of(view.self.id)->level >= 3)
                        actions.push("propose a treaty");
                    if (chapters_.of(view.self.id)->level >= 5)
                        actions.push("ask to be recognised as a House");
                }
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
        // Sworn residents (doc 32, Phase 9): asked of a resident by an Officer of a Hall; settled at the Hold.
        if (e.npc && near(e))
            if (const auto* member = chapters_.member(view.self.id); member && member->rank <= chapter::RankOfficer)
            {
                const auto* mine = chapters_.of(view.self.id);
                if (mine->sworn.count(e.id))
                {
                    if (!mine->claimCell.empty())
                        actions.push("settle at the Hold");
                }
                else if (mine->level >= 4 && world_.society().resident(e.id) && !e.transient)
                    actions.push("ask to swear to the Chapter");
            }
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
        // Guests of a Chapter's rented place (doc 32, 5.2): an Officer inside lets a wolf come and go.
        if (!e.npc && e.id != view.self.id && !relations.chapterMates.count(e.id))
            if (const auto* lease = estates_.lease(view.self.cellId); lease && chapters_.of(view.self.id) &&
                                                                     chapters_.of(view.self.id)->id == lease->chapter &&
                                                                     chapters_.member(view.self.id)->rank <= chapter::RankOfficer)
                actions.push(lease->guests.count(e.id) ? "no longer let in" : "let in");
        // Parties (doc 32): any player in sight may be invited, by one in no party or who leads theirs.
        if (!e.npc && e.id != view.self.id && !relations.mates.count(e.id))
            if (const auto* mine = parties_.of(view.self.id); !mine || mine->leader == view.self.id)
                actions.push("invite");
        if (e.id != view.self.id && !e.dead && e.downedLeft > 0 && apart <= 2 && !world_.inBattle(e.id))
            actions.push("tend");
        // A Warden of the Order (doc 53, 4): one who has seen a Quickened partner's magic may tell of it, or vouch.
        if (e.npc && apart <= 3 && !view.self.witnessed.empty())
            if (const auto* order = factions_.memberOf(e.id); order && order->first == "warden_order")
            {
                actions.push("tell the wardens");
                actions.push("vouch to the wardens");
            }
        // Lodging (doc 54, 4): ask a resident with a home for its spare bed, if one has none yet.
        if (e.npc && !e.dead && apart <= 3 && !lodgingOf(view.self.id))
            if (const auto* life = world_.society().resident(e.id); life && !life->homeCell.empty())
                actions.push("ask to lodge");
        // Grooming (doc 55, 7): a wolf within 1.5 tiles, both out of a fight.
        if (e.id != view.self.id && !e.dead && apart <= 1.5 && !world_.inBattle(view.self.id) && !world_.inBattle(e.id) && !e.transient)
            actions.push("groom");
        // Giving (doc 55, 3): to any wolf within 2 tiles, both out of a fight; lending, to a player.
        if (e.id != view.self.id && !e.dead && apart <= 2 && !world_.inBattle(view.self.id) && !world_.inBattle(e.id) && !e.transient)
        {
            actions.push("give");
            if (!e.npc)
                actions.push("lend");
        }
        // Residents' troubles (doc 57, 3): what this wolf may do about one it has heard of, beside whoever can help.
        troubleActions(view.self, e, apart, actions);
        // Training grounds (doc 53, 5): Ask to spar of a resident trainer there.
        if (e.npc && apart <= battle::StartReach && !world_.inBattle(view.self.id) && world_.trainer(e.id) && !world_.inBattle(e.id))
            actions.push("ask to spar");
        // Farm work (doc 53, 2.6): Help with the harvest (or the threshing) of a farmer at work at its post, near.
        if (e.npc && apart <= together::rules().lendTiles && !world_.inBattle(view.self.id))
            if (const auto* act = world_.residentWorkAt(e.id))
                if (const auto* mine = world_.jointOf(view.self.id); !mine || mine->target != e.id)
                    actions.push(act->verb);
        // Working together (doc 53, 2.3): Lend a paw to a wolf at work; Ask to lend a paw of one near, while at work.
        if (!e.npc && e.id != view.self.id)
        {
            if (world_.mayLend(view.self.id, e.id))
                actions.push("lend a paw");
            else if (world_.atWork(view.self.id) && !world_.atWork(e.id) && !world_.inBattle(e.id) && apart <= together::rules().lendTiles &&
                     !blocked(view.self.id, e.id))
                actions.push("ask to lend a paw");
        }
        // Vouching (doc 52, 7): a resident that trusts and likes this wolf enough to take its word for someone.
        if (e.npc && !e.dead)
            if (const auto* bond = world_.bonds().find(e.id, view.self.id);
                bond && bond->trust >= newcomers::rules().vouching.trust && bond->affinity >= newcomers::rules().vouching.liking)
                actions.push("vouch");
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
            o.add("terms", challenge->terms);
            // The challenger's lines and veils on injury and death (doc 50, 2): they inform; consent is the rules' already.
            if (const auto p = profiles_.find(from->id); p != profiles_.end())
            {
                auto limits = Value::object();
                for (const char* flag : {"injury", "death"})
                    if (const auto a = p->second.consent.find(flag); a != p->second.consent.end())
                        limits.add(flag, a->second);
                if (limits.size() > 0)
                    o.add("limits", limits);
            }
            root.add("challenge", o);
        }
    root.add("structures", structuresView(id, view.cell.id));   // Camps, Halls and Holds here (doc 32, 5.7).
    root.add("stores", storesView(view.cell.id));                // A home's larder, chest, wardrobe and woodpile (doc 36).
    root.add("boards", boardsView(view.cell.id));                // The town's notice board (doc 54, 2).
    root.add("stalls", stallsView(view.cell.id));                // Players' market stalls (doc 54, 3).
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
        if (const auto smell = scentOfItem(id, id, itemId); !smell.empty())
            i.add("scent", smell);                  // (Meals and herbs too: doc 55.)
        inventory.push(i);
    };
    item("starter_satchel", "Shoulder satchel", "bag", "A small travel bag made for a wolf's shoulders.", true, 1);
    if (purse)
    {
        const int herbs = Society::stock(*purse, "herbs"), meals = Society::stock(*purse, "meal");
        if (herbs > 0)
            item("herbs", "Cooking herbs", "herb", "Finite ingredients. Sell to a trader who needs supplies.", false, herbs);
        if (meals > 0)
            item("meal", "Prepared meal", "food", "Consume one to restore 10 stamina. Cooking uses real ingredients.", false, meals);
        const auto* self = world_.entity(id);
        if (const int swords = Society::stock(*purse, "sword"); swords > 0)
        {
            item("sword", "Dull bronze sword", "weapon",
                 "A cast-bronze bit-sword, carried in the jaws: a Basic blade. In a fight it reaches two tiles and cuts harder "
                 "than a bite, but tires you more.",
                 self && World::swordHeld(*self) == "sword", swords);
            inventory.items().back().add("blade", true);      // (Any blade may be taken up: doc 47.)
            inventory.items().back().add("tier", items::tierName(1));
        }
        // Wearables (doc 35): where each can go, and how many are worn.
        for (const auto& [itemId, quantity] : purse->stock)
            if (const auto* piece = quantity > 0 ? items::wearable(itemId) : nullptr)
            {
                const auto* me = world_.entity(id);
                const int worn = me ? World::wornCount(*me, itemId) : 0;
                auto i = Value::object();
                i.add("id", itemId);
                i.add("name", itemLabel(id, itemId));
                i.add("icon", piece->slot == "jewelry" ? "jewel" : piece->slot == "sling" || piece->slot == "harness" ? "bag" : "wear");
                i.add("description", piece->desc);
                i.add("equipped", worn > 0);
                i.add("quantity", quantity);
                i.add("worn", worn);
                i.add("slot", piece->slot);
                auto places = Value::array();
                for (const auto& p : piece->slot == "jewelry" ? piece->spots : items::slotsFor(*piece))
                    places.push(p);
                i.add("places", std::move(places));
                if (piece->status > 0)
                    i.add("status", piece->status);
                if (piece->warmth > 0)
                    i.add("warmth", piece->warmth);
                if (piece->protect > 0)
                    i.add("protect", std::round(piece->protect * 10) / 10);
                if (piece->tier > 0)
                    i.add("tier", items::tierName(piece->tier));   // (Doc 47: Basic, Professional, Exceptional.)
                inventory.push(std::move(i));
            }
        // Any other good of the catalog it holds (bought at a shop, given): listed, since it weighs (doc 35, 1.2).
        for (const auto& [itemId, quantity] : purse->stock)
            if (const auto* good = quantity > 0 && itemId != "herbs" && itemId != "meal" && itemId != "sword" &&
                                           !items::wearable(itemId) ? items::good(itemId) : nullptr)
            {
                auto i = Value::object();
                const bool blade = items::blade(itemId) != nullptr;      // A blade of any metal and make (doc 35, Part 4; 47).
                i.add("id", itemId);
                i.add("name", itemLabel(id, itemId));
                i.add("icon", blade ? "weapon" : "goods");
                i.add("description", good->desc);
                i.add("equipped", blade && world_.entity(id) && World::swordHeld(*world_.entity(id)) == itemId);
                if (blade)
                    i.add("blade", true);
                if (good->tier > 0)
                    i.add("tier", items::tierName(good->tier));
                i.add("quantity", quantity);
                if (const auto smell = scentOfItem(id, id, itemId); !smell.empty())
                    i.add("scent", smell);              // "it smells of Kestrel" (doc 55, 3 and 4).
                inventory.push(std::move(i));
            }
    }
    item("token", "Wooden token", "token", "A smooth keepsake carved with a branch.", false, 1);
    // What each piece weighs (doc 35, 1.2): the catalog's, a pound a piece.
    for (auto& i : inventory.items())
        if (const auto* good = items::good(i.string("id")))
            i.add("weight", good->weight);
    {
        // Wear and tear (doc 35): how much is left of each piece in service, and, beside a shop that deals in it, what
        // mending it would cost.
        const auto* me = world_.entity(id);
        const Entity* mender = nullptr;
        double menderAt = 2.5;
        for (const Entity* e : world_.entitiesIn(view.self.cellId))
            if (e->cellId == view.self.cellId && world_.society().merchant(e->id))
                if (const double d = std::hypot(e->position.x - view.self.position.x, e->position.y - view.self.position.y); d <= menderAt)
                {
                    mender = e;
                    menderAt = d;
                }
        for (auto& i : inventory.items())
        {
            const auto itemId = i.string("id");
            if (!me || World::durabilityOf(itemId) <= 0)
                continue;
            i.add("condition", std::round(world_.conditionOf(*me, itemId) * 100));
            if (mender && world_.canRepair(mender->id, itemId))
                if (const auto cost = world_.repairCost(*me, itemId); cost > 0)
                {
                    i.add("repairBy", mender->id);
                    i.add("repairCost", cost);
                }
        }
    }
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
        if (const auto* account = world_.society().account(world_.society().tillOf(trader->id)))
        {
            auto m = Value::object();
            m.add("id", trader->id);
            m.add("name", names::capitalised(labelFor(id, trader->id)));
            m.add("cash", account->cash);
            auto goods = Value::array();
            // Each of its goods, and any other quality of it the shop or this wolf has (doc 35, Part 4: "Fine hide").
            std::vector<std::string> kinds;
            for (const auto& ware : world_.society().wares(trader->id))
            {
                kinds.push_back(ware);
                for (const auto* holder : {account, purse})
                    for (const auto& kind : Society::kindsHeld(*holder, ware))
                        if (kind != ware && std::find(kinds.begin(), kinds.end(), kind) == kinds.end())
                            kinds.push_back(kind);
            }
            for (const auto& kind : kinds)
            {
                const char* itemId = kind.c_str();
                auto good = Value::object();
                good.add("id", itemId);
                good.add("quality", items::qualityName(items::qualityOf(kind)));
                good.add("name", itemLabel(id, itemId));
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
    {
        // Out in the wild (doc 41): whether one may hunt here, and what the ground beside one gives to forage.
        const auto here = world_.wildAround(id);
        auto wild = Value::object();
        wild.add("hunt", here.hunt);
        wild.add("huntWhy", here.huntWhy);
        wild.add("forage", here.forage);
        wild.add("forageWhat", here.forageWhat);
        // Trails this wolf has smelt out here: faint marks on the map's tiles, each the animal's colour (doc 41).
        auto tracks = Value::array();
        for (const auto& t : world_.tracksOf(id))
        {
            auto track = Value::object();
            track.add("species", t.species);
            if (const auto* s = wild::speciesById(t.species))
            {
                track.add("name", s->name);
                track.add("color", s->color);
            }
            track.add("fresh", t.fresh);
            track.add("left", std::max(0.0, t.until - world_.time()));
            auto tiles = Value::array();
            for (const auto& [x, y] : t.tiles)
            {
                auto tile = Value::array();
                tile.push(x);
                tile.push(y);
                tiles.push(tile);
            }
            track.add("tiles", tiles);
            tracks.push(track);
        }
        wild.add("tracks", tracks);
        root.add("wild", wild);
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
    // The scene this line belongs to (doc 51, §7): each listener's copy says so, as far as they may know it.
    std::string party;
    if (const auto* fight = world_.battleOf(author); fight && !fight->over)
        party = SocialLedger::fightTag(fight->id);
    else if (const auto* joint = world_.jointOf(author))
        party = SocialLedger::workTag(joint->id);    // Talk while working together (doc 53, 2.2).
    else if (const auto* mine = parties_.of(author))
        party = mine->id;
    const auto routed = speaker->npc ? std::string() : social_.routeFor(author, speaker->cellId, party, now());
    const auto routedScene = social_.sessions.find(routed);
    for (auto* c : clients_)
    {
        if (c->entityId.empty())
            continue;
        const auto& listener = c->entityId;
        if (hides(listener, author))
            continue;                               // Muted or blocked by this listener (doc 50): never delivered.
        auto sense = world_.perceive(listener, author, voice);
        if (listener == author)
            sense = {1, 1, true};
        auto segments = perceivePost(post, sense.hearing, sense.vision, event * 7919 + std::hash<std::string>{}(listener));
        if (post.veilNames)                         // (Wolves named as this listener knows them: doc 52.)
            for (auto& segment : segments)
                segment.text = veilFor(listener, segment.text);
        if (segments.empty())
            continue;
        if (!blocked(author, listener))              // (No scene between two where either blocks the other: doc 50.)
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
        std::string text, told;                    // (told: for a scene's story, its actions *starred*, its words "quoted".)
        for (const auto& segment : segments)
        {
            auto j = Value::object();
            j.add("kind", segment.kind);
            j.add("text", segment.text);
            output.push(j);
            if (!text.empty())
                text += ' ', told += ' ';
            text += segment.text;
            told += segment.kind == "speech" ? "\"" + segment.text + "\"" : "*" + segment.text + "*";
        }
        e.add("segments", output);
        e.add("text", text);
        // Its scene: "mine" for its own wolves; for others, an Open or Knock scene's door and place; nothing for a
        // Private scene's or a fight's, whose talk reads as ordinary talk. Never an id, so a voice stays anonymous.
        if (routedScene != social_.sessions.end())
        {
            const auto& sc = routedScene->second;
            const auto member = sc.members.find(listener);
            auto tag = Value::object();
            if ((member != sc.members.end() && !member->second.left) || listener == author)
                tag.add("mine", true);              // (The speaker's own copy: this line is theirs to the scene.)
            else if (!SocialLedger::isFight(sc) && (sc.openness == "open" || sc.openness == "knock"))
            {
                tag.add("openness", sc.openness);
                if (const auto* where = world_.cell(sc.cell))
                    tag.add("place", where->name);
            }
            if (!tag.fields().empty())
            {
                tag.add("colour", stars::sceneColour(sc.id));
                e.add("scene", tag);
            }
        }
        if (listener != author)
            heardLine(listener, event, author, group.empty() ? "ic" : group, text);   // (A report's evidence: doc 50.)
        // For a recap of the scene, as this wolf perceived it; and they have met (doc 50, 5).
        perceivedLine(listener, listener == author ? std::string("You") : sense.identifiable ? names::capitalised(labelFor(listener, author))
                                                                                             : std::string("A voice"), told);
        if (listener != author && sense.identifiable)
            meet(listener, author, "heard");
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
    if (!npc || !player || !npc->npc || world_.visitorLeaves(npcId) >= 0)
        return false;                               // (Visitors aren't voiced by the Mind: docs 34, 58.)
    world_.faceTalker(npcId, playerId);             // It turns to the one talking to it (doc 53, 4).
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
    storylineEvent({"talk", playerId, npc->cellId, npcId, {}, 0, 0, {}});   // (Even unrecognised: doc 58, 1.)
    auto context = dialogueContext(npcId, playerId, heardText, identified);
    if (identified)
    {
        context.activity += matchmake(npcId, playerId, heardText);   // (A resident as matchmaker: doc 52, 5.)
        context.activity += vouchBriefing(npcId, playerId);          // (Vouched for, or a vouch gone bad: doc 52, 7.)
        context.activity += residentLetterBriefing(npcId, playerId); // (Letters it wrote them: doc 55, 5.)
        context.activity += festivalBriefing(npcId, playerId);       // (Today's festival winners: doc 54, 6.)
        context.activity += scholarBriefing(npcId, playerId);        // (A wolf who has read the town's records: doc 54, 7.)
    }
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
    const auto answer = gameAnswer(npcId, playerId, heardText, identified);
    if (!context.away.empty())
        if (auto away = absences_.find(playerId); away != absences_.end())
            away->second.unseen.erase(npcId);       // (Greeted as long gone, by the game's line or the model's: once.)
    if (!answer.empty())
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
    context.description = names::fitCoat(npc->description, npc->appearance) + " Current age: " + std::to_string(npc->age) + " years.";
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
    context.activity += swornContext(npcId);              // Sworn to a Chapter for life (doc 32, Phase 9).
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
        context.fame = fameBriefing(npcId, playerId);   // (What it has heard of their deeds: doc 56, 5.)
        context.away = awayBriefing(npcId, playerId, false);   // (Back after a long while: doc 56, 10; used up below.)
        context.trouble = troubleBriefing(npcId, playerId);    // (Its own trouble, to a wolf it trusts: doc 57, 3.)
        context.activity += storylineBrief(npcId, playerId);   // (A storyline's step about this resident: doc 58, 9.)
    }
    context.seen = profileContext(npcId, playerId);   // What it can see of them, in their player's words (doc 50).
    context.activity += tieBriefing(npcId, playerId);   // (A tie with this wolf: doc 52, 4.)
    if (isNewcomer(accountKey(playerId)))            // (Doc 52, 2.)
        context.activity += " This wolf is new to these parts. Be patient with them; if it fits, tell them where the inn and "
                            "the notice board are.";
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
    // A howl heard in town a little while ago (doc 51, Phase 6): the game says what happened; the Mind only mentions it.
    if (const auto town = world_.communityOf(npc->cellId); !town.empty())
        if (const auto howled = recentHowls_.find(town); howled != recentHowls_.end())
            context.scene += " A little while ago, " + howled->second.second + ".";
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
    // A matchmaker's pointer (doc 52, 5): names veiled as the player knows them, the written line without a model.
    const bool pointing = matchPending_.count(npcId) > 0;
    const auto text = sayMatch(npcId, reply.text, std::string(route) == "model");
    ParsedPost post;
    post.ok = true;
    post.speech = true;
    post.veilNames = pointing;                     // (Each listener hears the wolf pointed at as it knows them.)
    post.segments.push_back({"speech", text});
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
    memories_.record(npcId, subjectId, {sequence_++, now(), npcId, text});
    logEvent("conversation", npcId, identified ? subjectId : std::string());
    heed(npcId, subjectId, identified, reply);
    voiced("dialogue", route, npcId);
    saveSoon();
    // Spoken to with others: the next one answers now, having heard this.
    if (const auto chain = chainAfter_.find(npcId); chain != chainAfter_.end())
    {
        TalkChain next = std::move(chain->second);
        chainAfter_.erase(chain);
        next.said += npc->name + ": \"" + mind::left(text, 200) + "\" ";
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
    {
        // First impressions (doc 55, 7): a Well-groomed wolf the resident hardly knows: warmer nudges a quarter more.
        double lift = 1;
        if (const auto* wolf = world_.entity(subjectId); wolf && !wolf->npc)
            if (const auto* bond = world_.bonds().find(npcId, subjectId); !bond || bond->familiarity < 25)
                lift = 1 + .25 * world_.groomedFactor(*wolf);
        world_.bonds().change(npcId, subjectId, {affinity > 0 ? affinity * lift : double(affinity), trust > 0 ? trust * lift : double(trust), 0, 0, 0},
                              world_.calendarDays());
    }
    if (!reply.remember.empty())
        memories_.record(npcId, subjectId, {sequence_++, now(), "(your note)", reply.remember});
    // It spoke of the trouble it was told of (doc 57, 3): the wolf has heard it.
    if (const auto briefed = troubleBriefed_.find(npcId + "|" + subjectId); briefed != troubleBriefed_.end())
    {
        if (reply.mentionsTrouble)
            heardTrouble(subjectId, npcId, briefed->second);
        troubleBriefed_.erase(briefed);
    }
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
    world_.noteActive(id);                          // (Farm work pays a hand who is there: doc 53, 2.6.)
    lastActiveReal_[id] = now();                    // (Company in a common room counts the active: doc 54, 1.)
    if (type == "chat")
        if (const auto p = performers_.find(id); p != performers_.end())
            p->second.lastSaid = now();             // (A performer keeping at it.)
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
    else if (type == "net")
    {
        // The page's line to the server (doc 31, the health tracker), every half minute: kept for the next window.
        const auto ms = [&](const char* key) { return std::clamp(num(key), 0.0, 60000.0); };
        c->netP50 = ms("p50");
        c->netP95 = ms("p95");
        c->netMax = ms("max");
        c->netCorrections = int(std::clamp(num("corrections"), 0.0, 1e9));
        c->netAt = world_.time();
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
        const auto* seller = world_.entity(id);
        const bool wearing = buy && buy->isBool() && !buy->asBool() && purse && seller && qty >= 1 &&
                             Society::stock(*purse, j.string("item")) - World::wornCount(*seller, j.string("item")) < qty;
        const bool lent = buy && buy->isBool() && !buy->asBool() && purse && qty >= 1 &&
                          Society::stock(*purse, j.string("item")) - lentTo(id, j.string("item")) < qty;   // (Lent to them: doc 55, 8.)
        result = wearing ? Result{false, "You are wearing or holding it: take it off first.", {}}
               : lent    ? Result{false, "That is lent to you; it isn't yours to sell.", {}}
               : qty >= 1 && qty <= 99 && qty == std::floor(qty) && buy && buy->isBool()
                     ? (!factionTrade(id, j.string("target"), buy->asBool(), refusal)
                            ? refusal
                            : world_.trade(id, j.string("target"), j.string("item"), int(qty), buy->asBool()))
                     : Result{false, "Invalid trade request.", {}};
        if (result.ok && buy && buy->asBool())
            if (const auto* after = world_.society().account(id))
                afterFactionTrade(id, j.string("target"), before - after->cash);
        if (result.ok && buy && buy->asBool())
            makersScent(id, j.string("target"), j.string("item"), int(qty));   // (Bought from its maker: doc 55, 4.)
        else if (result.ok && buy && !buy->asBool())
            moveScents(id, j.string("target"), j.string("item"), int(qty), false);   // (Sold: their records go, oldest first.)
        report = true;
        if (result.ok)
        {
            world_.fitWorn(id);
            record(Economy | Character, id);
        }
    }
    else if (type == "mask")
    {
        // Masking oil (doc 35): one's scent, and that of what one carries, hidden for a few hours.
        result = world_.maskScent(id);
        report = true;
        if (result.ok)
            record(Economy | Character, id);
    }
    else if (type == "giftwork")
    {
        // A Gift at work (doc 43): lent to a workshop near by, or Mend, Shortcut, Lighten Load, Dowse, Echo, Carry.
        result = world_.useWorkGift(id, j.string("ability"), j.string("target"));
        report = true;
        if (result.ok)
        {
            record(Economy | Character, id);
            if (const auto* a = gifts::ability(j.string("ability")))   // (A Gift used outside a fight: doc 58's storylines.)
                world_.recordEvent({"gift used", id, {}, player->cellId, 0, 0, a->family, 0, 0, a->id});
        }
    }
    else if (type == "repair")
    {
        // Mending worn gear at a shop that deals in it (doc 35): the fee to the shop.
        result = world_.repairGear(id, j.string("target"), j.string("item"));
        report = true;
        if (result.ok)
            record(Economy | Character, id);
    }
    else if (type == "hunt" || type == "forage" || type == "leaveHunt")
    {
        // Hunting and foraging (doc 41): out in the wild.
        result = type == "hunt" ? world_.startHunt(id) : type == "forage" ? world_.forage(id) : world_.leaveHunt(id);
        report = true;
        if (result.ok && type == "forage")
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
    else if (type == "safety")
    {
        // Mute, block and report (doc 50, Phase 2).
        if (!safetyCommand(c, j, result))
            result = {false, "That isn't something you can do.", {}};
        report = !result.message.empty();
    }
    else if (type == "book")
    {
        // Story books (doc 51, Phase 7): starting, linking scenes, chapters, sharing, finishing, volumes, the shelf.
        if (!bookCommand(c, j, result))
            result = {false, "That isn't something a book does.", {}};
        report = !result.message.empty();
    }
    else if (type == "tie")
    {
        // Ending one's tie early (doc 52, Phase 3).
        if (!tieCommand(c, j, result))
            result = {false, "That isn't something a tie does.", {}};
        report = !result.message.empty();
    }
    else if (type == "vouch")
    {
        // Vouching for a wolf to a resident (doc 52, Phase 5).
        if (!vouchCommand(c, j, result))
            result = {false, "That isn't something you can vouch for.", {}};
        report = !result.message.empty();
    }
    else if (type == "mentor")
    {
        // Mentoring newcomers (doc 52, Phase 2): opting in or out, available or busy.
        if (!mentorCommand(c, j, result))
            result = {false, "That isn't something a mentor does.", {}};
        report = !result.message.empty();
    }
    else if (type == "howl")
    {
        // The gathering howl (doc 51, Phase 6): a howl, or joining one near.
        result = howl(id);
        report = true;
    }
    else if (type == "board")
    {
        boardCommand(c, j, result);                 // Notice boards (doc 54, 2).
        report = !result.message.empty();
    }
    else if (type == "lodge")
    {
        lodgeCommand(c, j, result);                 // Renting by individuals (doc 54, 4).
        report = true;
    }
    else if (type == "archive")
    {
        archiveCommand(c, j, result);               // Archive work (doc 54, 7).
        report = !result.message.empty();
    }
    else if (type == "journal")
    {
        sendJournal(c);                             // Lore, bestiary, herbarium, places (doc 54, 7).
        return;
    }
    else if (type == "festival")
    {
        festivalCommand(c, j, result);              // Festivals' contests (doc 54, 6).
        report = !result.message.empty();
    }
    else if (type == "table")
    {
        tableCommand(c, j, result);                 // Tavern games (doc 54, 5).
        report = !result.message.empty();
    }
    else if (type == "stall")
    {
        stallCommand(c, j, result);                 // Market stalls (doc 54, 3).
        report = !result.message.empty();
    }
    else if (type == "perform")
    {
        performCommand(c, j, result);               // Performing in a common room (doc 54, 1).
        report = true;
    }
    else if (type == "pact")
    {
        pactCommand(c, j, result);                  // Pacts (doc 55, 8).
        report = true;
    }
    else if (type == "lend" || type == "lendAnswer" || type == "return")
    {
        // Lending (doc 55, 8): {"target", "item", "quantity", "days"}; the one offered answers {"accept"}; the borrower
        // returns {"loan"}.
        if (type == "lend")
            lendCommand(c, j, result);
        else if (type == "lendAnswer")
            lendAnswer(c, j.boolean("accept"), result);
        else
            returnLoan(c, j, result);
        report = true;
    }
    else if (type == "groom" || type == "groomAnswer")
    {
        // Grooming (doc 55, 7): {"target": id or "self", "words"}; the one asked answers {"accept": bool}.
        if (type == "groom")
            groomCommand(c, j, result);
        else
            groomAnswer(c, j.boolean("accept"), result);
        report = true;
    }
    else if (type == "storyteller")
    {
        // Storytellers and tales (doc 58): apply, save, start, invite, admit, tick, narrate, npc, roll, visitor, dismiss,
        // prize, call, end; participants accept, decline, ask, roll; star (a credits screen). Development only: "approve"
        // makes one's own account a storyteller.
        if (options_.devTools && j.string("verb") == "approve")
        {
            decideStoryteller(accountKey(c), true, "dev", {});
            result = {true, "You are a storyteller (development).", {}};
        }
        else
            storytellerCommand(c, j, result);
        report = true;
    }
    else if (type == "storyline")
    {
        // Storylines (doc 58): {"verb": "track"|"take"|"abandon"|"leave", "storyline", "step", "objective"}. Development only:
        // "give" {template, cast} begins one for oneself, as the DM's storyline.give would.
        if (options_.devTools && j.string("verb") == "give")
            result = giveStoryline(id, j.string("template"), j.object("cast"), "dm", "");
        else
            storylineCommand(c, j, result);
        report = true;
    }
    else if (type == "project")
    {
        // Town projects (doc 57, 4): {"verb": "give"|"handin"|"work", "project", "coins"|"item"+"quantity", "shown"}.
        // Development only: {"verb": "post", "kind"} posts one in the town one stands in; "complete" finishes one by hand.
        if (options_.devTools && j.string("verb") == "post")
        {
            result = postProject(j.string("kind", "cover"), world_.communityOf(player->cellId), "", -1, -1, "", "dev");
            if (const auto* p = result.ok ? projects_.find(result.targetId) : nullptr)
                result.message = names::capitalised(p->title) + " is posted (" + p->id + ").";
        }
        else if (options_.devTools && j.string("verb") == "complete")
            result = completeProject(j.string("project"));
        else
            projectCommand(c, j, result);
        report = true;
    }
    else if (type == "give" || type == "giveAnswer")
    {
        // Giving (doc 55, 3): {"target", "item", "quantity", "coins"}; the one offered answers {"accept": bool}.
        if (type == "give")
            giveCommand(c, j, result);
        else
            giveAnswer(c, j.boolean("accept"), result);
        report = true;
    }
    else if (type == "letter" || type == "letters")
    {
        // Letters (doc 55): write, read, keep, burn, send on, reply; "letters" asks for the case.
        auto k = j;
        if (type == "letters")
            k.set("verb", "list");
        letterCommand(c, k, result);
        report = !result.message.empty();
    }
    else if (type == "known")
    {
        // Known wolves: tags, notes, forgetting, recaps (doc 50, Phase 4).
        if (!knownCommand(c, j, result))
            result = {false, "That isn't something you can do.", {}};
        report = !result.message.empty();
    }
    else if (type == "circle")
    {
        // Circles: making, inviting, roles, nights, sharing (doc 50, Phase 5).
        if (!circleCommand(c, j, result))
            result = {false, "That isn't something a circle does.", {}};
        report = !result.message.empty();
    }
    else if (type == "friends")
    {
        // Friends: requests, answers, sharing (doc 50, Phase 3).
        if (!friendsCommand(c, j, result))
            result = {false, "That isn't something friends do.", {}};
        report = !result.message.empty();
    }
    else if (type == "profile")
    {
        // The roleplay profile, status, walk-up, handle, experience and settings (doc 50, Phase 1).
        if (!profileCommand(c, j, result))
            result = {false, "That isn't something a profile does.", {}};
        report = !result.message.empty();
        if (result.ok)
            record(Character, id);
    }
    else if (type == "wardens")
    {
        // Keeping the secret (doc 53, 4): at a resident of the Warden Order, tell of a Quickened wolf one saw, or vouch.
        const auto at = j.string("at"), about = j.string("about"), verb = j.string("verb");
        const auto* warden = world_.entity(at);
        const auto* order = warden ? factions_.memberOf(at) : nullptr;
        if (!warden || !order || order->first != "warden_order" ||
            std::hypot(warden->position.x - player->position.x, warden->position.y - player->position.y) > 3 || warden->cellId != player->cellId)
            result = {false, "Find a Warden of the Order to speak to.", {}};
        else
            result = verb == "tell" ? world_.tellWardens(id, about) : verb == "vouch" ? world_.vouchToWardens(id, about)
                                                                               : Result{false, "Tell or vouch?", {}};
        report = true;
    }
    else if (type == "post")
    {
        result = world_.practiseAtPost(id);         // At a training ground's post (doc 53, 5).
        report = true;
    }
    else if (type == "work")
    {
        // Working together (doc 53): {"verb": "lend", "with": id, "role": r} / "ask", "to": id / "leave".
        const auto verb = j.string("verb");
        result = verb == "lend" ? world_.lendAPaw(id, j.string("with"), j.string("role"))
                 : verb == "start" ? world_.helpAtWork(id, j.string("at"))
                 : verb == "watch" ? world_.keepWatch(id, j.boolean("on", true))
                 : verb == "ask" ? world_.askToLend(id, j.string("to"))
                 : verb == "leave" ? world_.leaveWork(id)
                                   : Result{false, "That isn't something work does.", {}};
        report = true;
    }
    else if (type == "partners")
    {
        // Settings (doc 53): Allow hunting partners, Allow work partners; kept with the character.
        result = world_.setPartners(id, j.string("kind"), j.boolean("on", true));
        report = true;
        if (result.ok)
            record(Character, id);
    }
    else if (type == "noPvp")
    {
        // Settings: auto-decline fights with players (doc 40's fight start), kept with the character.
        result = world_.setNoPvp(id, j.boolean("on"));
        report = true;
        if (result.ok)
            record(Character, id);
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
    else if (type == "practice" && options_.devTools)
    {
        // Development only (doc 49): set a skill, and what it has gained today ("today": its soft limit eases it off).
        result = world_.setPractice(id, j.string("skill"), j.number("value", -1), j.has("today") ? j.number("today", -1) : -1);
        report = true;
        if (result.ok)
            record(Character, id);
    }
    else if (type == "socialLevel" && options_.devTools)
    {
        // Development only (doc 52): one's own wolf's social XP set so the account stands at a social level (mentors are
        // level 5), for trying out what levels open.
        const int level = std::clamp(int(j.number("level", 1)), 1, 30);
        social_.points[id] = 0;
        social_.points[id] = int(std::max<long long>(0, practice::xpFor(level) - socialXp(id)));
        result = {true, "Social level " + std::to_string(level) + ".", {}};
        report = true;
        saveSoon();
    }
    else if (type == "deed" && options_.devTools)
    {
        // Development only (doc 56): a deed of `kind` by one's wolf here, for its town, seen by whoever is near; with
        // `coin`, the nearest resident who saw it coins its nickname at once.
        const auto made = recordDeed(j.string("kind", "broke_camp"), {id}, "town:" + world_.lawTown(player->cellId), player->cellId, "dev",
                                     j.string("detail"));
        if (auto* d = made.empty() ? nullptr : fame_.find(made); d && j.boolean("coin") && d->nickname.empty())
            for (const auto& w : d->witnesses)
                if (const auto* e = world_.entity(w.id); e && e->npc && coinNickname(*d, w.id))
                    break;
        result = made.empty() ? Result{false, "No such kind of deed.", {}} : Result{true, "A deed: " + made + ".", {}};
        report = true;
    }
    else if (type == "acquaint" && options_.devTools)
    {
        // Development only (doc 52): a resident comes to know one's wolf this much better (a matchmaker points only at
        // wolves it knows), for trying out what that opens.
        const auto* npc = world_.entity(j.string("npc"));
        if (!npc || !npc->npc)
            result = {false, "No such resident.", {}};
        else
        {
            world_.bonds().change(npc->id, id, {0, 0, std::clamp(j.number("familiarity", 10), 0.0, 100.0), 0, 0}, world_.calendarDays());
            result = {true, "They know you a little better.", {}};
            saveSoon();
        }
        report = true;
    }
    else if (type == "dev" || type == "devCommands")
        devCommand(c, j);
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
                // Out in the wild the nose finds game's trails too (doc 41), marked faintly on the map for a while.
                if (const auto tracks = world_.smellTracks(id); tracks.ok)
                    system(c, tracks.message);
                // Whose work the masterworks near by are, by the maker's scent on them (doc 35, Part 4).
                int told = 0;
                for (const auto& mark : world_.marksSmelt(id))
                    if (told++ < 3)
                        system(c, "A maker's scent reaches you from something " + labelFor(id, mark.holder) + " carries: " +
                                      itemLabel(id, mark.item) + ".");
                world_.trainNose(id);                   // The nose sharpens with use.
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
                system(c, world_.hostile(target)                 ? "They want your purse, not your conversation."
                          : world_.visitorLeaves(target) >= 0 ? npc->name + " has nothing to say to you just now."
                                                              : "The carters are too busy with the road to talk.");
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
        else if (action == "challenge" && blocked(id, target))
            system(c, names::capitalised(labelFor(id, target)) + " isn't taking challenges.");   // (Blocked: the ordinary refusal, doc 50.)
        else if (action == "attack" || action == "pay" || action == "challenge")
        {
            const auto done = action == "pay" ? world_.payBandits(id, target) : world_.attack(id, target, j.string("terms"));
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
        else if (action == "wear" || action == "take off")
        {
            // Doc 35: "wear" targets "<item>" or "<item>@<slot or fur spot>"; "take off" targets "<slot>" or "<spot>@<item>".
            const auto at = target.find('@');
            const auto first = target.substr(0, at), second = at == std::string::npos ? std::string() : target.substr(at + 1);
            const auto done = action == "wear" ? world_.wear(id, first, second) : world_.takeOff(id, first, second);
            system(c, done.message);
            if (done.ok)
                record(Character, id);
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
        else if (action.rfind("hand in goods ", 0) == 0)
        {
            const auto done = world_.deliverContract(id, action.substr(14));
            system(c, done.message);
            if (done.ok)
                record(Roads | Economy | Character, id);
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
        else if (action.rfind("trouble:", 0) == 0)
        {
            result = troubleAction(id, target, action);   // (A resident's trouble: doc 57, 3.)
            report = !result.message.empty();
        }
        else if (action == "ask for the church's care")
        {
            result = world_.churchCare(id, target);
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
        else if (action == "propose a treaty" || action == "ask to be recognised as a House" || action == "ask to swear to the Chapter" ||
                 action == "settle at the Hold")
        {
            auto k = Value::object();
            k.add("verb", action == "propose a treaty" ? "treaty" : action == "ask to be recognised as a House" ? "house"
                          : action == "ask to swear to the Chapter"                                    ? "swear"
                                                                                                      : "settle");
            k.add("target", target);
            holdCommand(c, k, result);
            report = true;
        }
        else if (action == "let in" || action == "no longer let in")
        {
            auto k = Value::object();
            k.add("verb", action == "let in" ? "guest" : "unguest");
            k.add("target", target);
            estateCommand(c, k, result);
            report = true;
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
        else if (action == "ask to spar")
        {
            result = world_.sparWithTrainer(id, target);
            report = true;
            updateMovementModes();
        }
        else if (action == "help with the harvest" || action == "help with the threshing" || action == "lend a paw at the workshop")
        {
            result = world_.helpAtWork(id, target);
            report = true;
        }
        else if (action == "lend a paw" || action == "ask to lend a paw")
        {
            // Working together (doc 53, 2.3): lending a paw to a wolf at work, or asking one to lend theirs.
            result = action == "lend a paw" ? world_.lendAPaw(id, target, j.string("role")) : world_.askToLend(id, target);
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
            const auto described = veilFor(id, other->npc ? names::fitCoat(other->description, other->appearance) : other->description);
            // How they regard this wolf, in words; and this wolf's own note on them (doc 32, 1.4).
            if (target != id)
            {
                e.add("regard", regardWords(other->id, id));
                // What this wolf keeps on them (doc 50, 5): its note, tag, last met, shared scenes and recaps.
                if (auto known = knownFor(id, other->id); known.isObject())
                {
                    if (known.has("note"))
                        e.add("note", known.string("note"));
                    e.add("known", std::move(known));
                }
                readProfile(id, other->id);
            }
            // The roleplay profile (doc 50): what this viewer may see of it; its description over the fixed line.
            auto profile = cardFor(id, other->id);
            const bool written = !other->npc && profile.has("description");
            e.add("description", written ? profile.string("description") : described);
            if (!other->npc)
            {
                e.add("profile", std::move(profile));
                e.add("stars", starsFor(id, other->id));   // (Their account's stars, as this viewer may see them: doc 51.)
            }
            e.add("posture", other->posture);
            e.add("state", other->state);
            // What a closer look shows of its injuries (doc 38: only Look shows them; decided 2026-10-04).
            if (const auto hurts = injury::visible(other->injuries); !hurts.empty())
                e.add("injuries", hurts);
            const auto wearing = World::wornWords(*other);   // (Doc 35: what they wear and where their jewellery is.)
            if (!wearing.empty())
                e.add("wearing", wearing);
            // And the same for their equipment page, to look at only: each slot's and spot's item, by id and name.
            {
                const auto named = [](const std::string& item) {
                    auto piece = Value::object();
                    piece.add("id", item);
                    piece.add("name", Society::itemName(item));
                    return piece;
                };
                auto worn = Value::object();
                for (const auto& [slot, item] : other->worn)
                    worn.add(slot, named(item));
                auto jewellery = Value::array();
                for (const auto& [spot, item] : other->jewellery)
                {
                    auto piece = named(item);
                    piece.add("spot", spot);
                    jewellery.push(std::move(piece));
                }
                auto equipment = Value::object();
                equipment.add("worn", std::move(worn));
                equipment.add("jewellery", std::move(jewellery));
                if (!other->mouth.empty())
                    equipment.add("mouth", named(other->mouth));
                e.add("equipment", std::move(equipment));
            }
            e.add("text", (written ? e.string("description") : described) + (wearing.empty() ? "" : " " + wearing) + " Current posture: " + other->posture + ". " + other->state);
            send(c, e);
        }
        else
        {
            result = world_.interact(id, target, action);
            report = true;
        }
        if (player->cellId != beforeCell)
        {
            followTransition(id, beforeCell);
            payToll(id);                              // Through a door into a Hold's claimed place (doc 32, 5.5).
        }
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
        if (std::string until; silenced(id, &until))
        {
            feedback(false, "A Dungeon Master has silenced your account for another " + until + ".");   // (Doc 50, 7.)
            return;
        }
        if (channel == "circle")
        {
            // Out of character, to a circle's members in the world (doc 50, 6).
            const auto sent = circleLine(c, j.string("circle"), text);
            feedback(sent.ok, sent.ok ? std::string() : sent.message);
            return;
        }
        if (channel == "private")
        {
            // Out of character, to a friend anywhere, or kept for them (doc 50, 4).
            const auto sent = privateMessage(c, j.string("to"), text);
            if (sent.ok && !sent.message.empty())
                system(c, sent.message);
            feedback(sent.ok, sent.ok ? std::string() : sent.message);
            return;
        }
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
                if (const auto* actor = world_.entity(other->entityId); actor && actor->cellId == player->cellId && !hides(other->entityId, id))
                {
                    e.set("speaker", names::capitalised(labelFor(other->entityId, id)));
                    send(other, e);
                    if (other->entityId != id)
                        heardLine(other->entityId, e.number("sequence"), id, "ooc", text);
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
        Voice voice = volume == "whisper" ? Voice::Whisper : volume == "yell" ? Voice::Yell : Voice::Speak;
        if (voice == Voice::Speak && world_.time() < player->carryVoiceUntil)
            voice = Voice::Yell;                    // Carry (a Gifted Sound wolf, doc 43): it is heard far off.
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
            if (voice != Voice::Whisper && sense.hearing >= .35)
                world_.heardVoice(npcId, id);       // Speaking aloud gives a sneak away; a whisper stays a whisper (doc 40).
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
        // Words while working together are the work's scene (doc 53, 2.2), as a fighter's are the fight's.
        if (const auto* joint = world_.jointOf(id))
            partyScene = SocialLedger::workTag(joint->id);
        // A fighter's words are the fight's scene (doc 33): roleplaying it through pays as a scene does (doc 51).
        if (const auto* fight = world_.battleOf(id); fight && !fight->over)
            if (const auto* me = fight->fighter(id); me && me->status != "fled")
                partyScene = SocialLedger::fightTag(fight->id);
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
    // The journal's accounts now start from what this checkpoint holds: an account opened since the last record (a
    // contract's escrow, say) and closed before the next is erased by that record, not left in the checkpoint beside the
    // coins it paid out (which made a restart refuse the save: doc 57's tests found it).
    if (shadow_.primed)
        shadow_.accounts = world_.society().state().accounts;
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
    c->server.standing = standingSave();
    c->server.people = peopleSave();
    c->server.chapters = chapters_.save();
    c->server.factions = factions_.save();
    c->server.estates = estates_.save();
    c->server.camps = camps_.save();
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
    social_.reindex();
    social_.points = state.social.points;
    social_.recent = state.social.recent;
    social_.sessions = state.social.sessions;
    social_.reindexScenes();                          // (Who is in which open scene, and each one's last: doc 51.)
    social_.stars = state.social.stars;
    social_.stories = state.social.stories;
    social_.nextStory = state.social.nextStory;
    socialSeen_ = social_.entries.size();             // (Scenes settled before the restart were told then.)
    standingLoad(state.standing);                    // (After the accounts and the ledger: it counts from both.)
    peopleLoad(state.people);                        // Handles, played time, profiles (doc 50).
    seedScenes();                                    // (Scenes over before the restart: recapped then, doc 50.)
    chapters_.load(state.chapters);
    factions_.load(state.factions);
    estates_.load(state.estates);
    camps_.load(state.camps);
    for (const auto& [who, ids] : state.commandReceipts)
        for (const auto& receipt : ids)
            commandReceipts_[who].push_back(receipt);
    for (const auto& [actor, entries] : state.responseReceipts)
        for (const auto& [key, value] : entries)
            responseReceipts_[actor][key] = value;
    for (const auto& [id, saved] : characters_)
        world_.bonds().setAway(id, !world_.entity(id));   // (Nobody's regard fades for a wolf who is away: doc 56, 10.)
    note("info", "RATW_RESTORE characters=" + std::to_string(characters_.size()) + " summaries=" + std::to_string(memories_.summaries.size()) +
                     " ledger=" + std::to_string(social_.entries.size()));
}
std::string Game::itemLabel(const std::string& viewer, const std::string& item) const
{
    std::string name = Society::itemName(item);
    const auto maker = items::makerOf(item);
    if (maker.empty())
        return name;
    if (maker == viewer)
        return name + " · your own mark";
    const auto* who = world_.entity(maker);
    const auto* spec = world_.society().spec(maker);
    if (who && knowsName(viewer, maker))
        return name + " · " + who->name + "'s mark";
    if (!who && spec)
        return name + " · " + spec->name + "'s mark";
    return name + " · a maker's mark, and a scent you don't know";
}
} // namespace ratw::game
