// Saves that never stop the game (Docs/Design/31-responsiveness.md, Phase 2): the journal of valuable changes, the
// replies that wait for it, and snapshots of the whole world taken by a forked copy of the server.
//
// A valuable command (a trade, eating, a contract, an account, a character...) changes the world at once and calls
// record(), which appends what changed to the journal (RatwJournal.h) without waiting. The command's replies are held
// and sent when the journal's writer says the record is written: a player is told "done" only for what a crash can't
// take back.
//
// Everything else is kept by a snapshot every SnapshotSeconds. The server forks; the child, a frozen copy of the
// game, captures and encodes the checkpoint and writes it (a file world's save in place; for a database, a private file
// the store's worker reads and stores) while the game plays on. The game's only cost is the fork itself. Once a
// snapshot is stored, the journal records it covers are removed.
#include "RatwGame.h"

#include "RatwCheckpoint.h"
#include "RatwWire.h"

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace ratw::game
{
using json::Value;

namespace
{
bool samePosition(const PositionState& a, const PositionState& b)
{
    return a.holder == b.holder && a.apprentice == b.apprentice && a.lastHolder == b.lastHolder && a.vacantSince == b.vacantSince &&
           a.newcomerAsked == b.newcomerAsked;
}

std::vector<std::int64_t> counters(const SocietyState& s)
{
    return {s.minted, s.sunk, s.nextEntry, s.budgetDay, s.exportsRemaining, s.importsRemaining, s.herbPatch,
            std::int64_t(s.decisionRemainder * 1e9), s.craftingStocked};
}

Value companionsOf(const std::map<std::string, std::string>& owners)
{
    auto out = Value::object();
    for (const auto& [npc, owner] : owners)
        if (!owner.empty())
            out.add(npc, owner);
    return out;
}

Value path(std::initializer_list<std::string> keys)
{
    auto out = Value::array();
    for (const auto& k : keys)
        out.push(k);
    return out;
}

// Everything written, or false. For the forked child: plain system calls, nothing that takes a lock.
bool writeFile(const std::string& file, const std::string& text, bool sync)
{
    const int fd = ::open(file.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    if (fd < 0)
        return false;
    std::size_t done = 0;
    while (done < text.size())
    {
        const auto n = ::write(fd, text.data() + done, text.size() - done);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
        {
            ::close(fd);
            return false;
        }
        done += std::size_t(n);
    }
    const bool ok = !sync || ::fdatasync(fd) == 0;
    return ::close(fd) == 0 && ok;
}

bool readText(const std::string& file, std::string& out)
{
    std::ifstream in(file, std::ios::binary);
    if (!in)
        return false;
    std::ostringstream text;
    text << in.rdbuf();
    out = text.str();
    return true;
}

// A private folder for the snapshots a database world hands its store, emptied when made.
std::string snapshotFolder()
{
    namespace fs = std::filesystem;
    std::error_code error;
    const auto folder = fs::temp_directory_path(error) / ("ratw-snapshots-" + std::to_string(::getpid()));
    fs::remove_all(folder, error);
    fs::create_directories(folder, error);
    ::chmod(folder.c_str(), 0700);                 // Account verifiers are in a snapshot: the owner's alone.
    return folder.string();
}

// Removes a snapshot's files when the last copy of the save that reads them goes, whether it ran or a newer one
// replaced it before it could.
struct Leftovers
{
    std::string document, states;
    ~Leftovers()
    {
        std::remove(document.c_str());
        if (!states.empty())
            std::remove(states.c_str());
    }
};
} // namespace

// --------------------------------------------------------------------------- The journal

void Game::prime(Shadow& s) const
{
    const auto& society = world_.society().state();
    s.accounts = society.accounts;
    s.economy = counters(society);
    s.ledgerSize = society.ledger.size();
    s.ledgerLast = society.ledger.empty() ? 0 : std::uint64_t(society.ledger.back().sequence);
    s.positions = society.careers.positions;
    s.roads = json::dump(checkpoint::roads(world_.roads()));
    s.crime = json::dump(checkpoint::crime(world_.crime()));
    s.companions = json::dump(companionsOf(companionOwner_));
    s.signIns = json::dump(accounts_.state());
    s.primed = true;
}

void Game::record(unsigned what, const std::string& character)
{
    auto* writer = store_ ? store_->journal() : nullptr;
    if (!writer)
    {
        ++revision_;
        save();                                    // No journal: whole, and waited for, as before.
        return;
    }
    if (!shadow_.primed)
        prime(shadow_);
    auto changes = Value::array();
    const auto set = [&](Value at, Value value) {
        auto change = Value::object();
        change.add("set", std::move(at));
        change.add("value", std::move(value));
        changes.push(std::move(change));
    };
    const auto& society = world_.society().state();
    if (what & Economy)
    {
        for (const auto& [id, account] : society.accounts)
        {
            const auto old = shadow_.accounts.find(id);
            if (old == shadow_.accounts.end() || old->second.cash != account.cash || old->second.stock != account.stock)
            {
                set(path({"society", "accounts", id}), wire::economyAccount(account));
                shadow_.accounts[id] = account;
            }
        }
        for (auto it = shadow_.accounts.begin(); it != shadow_.accounts.end();)
            if (!society.accounts.count(it->first))
            {
                auto change = Value::object();
                change.add("erase", path({"society", "accounts", it->first}));
                changes.push(std::move(change));
                it = shadow_.accounts.erase(it);
            }
            else
                ++it;
        if (const auto now = counters(society); now != shadow_.economy)
        {
            set(path({"society", "minted"}), society.minted);
            set(path({"society", "sunk"}), society.sunk);
            set(path({"society", "nextEntry"}), society.nextEntry);
            set(path({"society", "budgetDay"}), society.budgetDay);
            set(path({"society", "exportsRemaining"}), society.exportsRemaining);
            set(path({"society", "importsRemaining"}), society.importsRemaining);
            set(path({"society", "herbPatch"}), society.herbPatch);
            set(path({"society", "decisionRemainder"}), society.decisionRemainder);
            set(path({"society", "craftingStocked"}), society.craftingStocked);
            shadow_.economy = now;
        }
        const std::uint64_t last = society.ledger.empty() ? 0 : std::uint64_t(society.ledger.back().sequence);
        if (society.ledger.size() != shadow_.ledgerSize || last != shadow_.ledgerLast)
        {
            set(path({"society", "ledger"}), wire::economyLedger(society.ledger));
            shadow_.ledgerSize = society.ledger.size();
            shadow_.ledgerLast = last;
        }
    }
    if (what & Careers)
    {
        for (const auto& [id, p] : society.careers.positions)
        {
            const auto old = shadow_.positions.find(id);
            if (old == shadow_.positions.end() || !samePosition(old->second, p))
            {
                set(path({"society", "careers", "positions", id}), wire::careerPosition(p));
                shadow_.positions[id] = p;
            }
        }
    }
    const auto whole = [&](bool wanted, std::string& before, Value now, const char* key) {
        if (!wanted)
            return;
        auto text = json::dump(now);
        if (text == before)
            return;
        set(path({key}), std::move(now));
        before = std::move(text);
    };
    whole(what & Roads, shadow_.roads, checkpoint::roads(world_.roads()), "roads");
    whole(what & Crime, shadow_.crime, checkpoint::crime(world_.crime()), "crime");
    whole(what & Companions, shadow_.companions, companionsOf(companionOwner_), "companions");
    whole(what & Accounts, shadow_.signIns, accounts_.state(), "accounts");
    if ((what & Character) && !character.empty())
    {
        const Entity* e = world_.entity(character);
        if (!e)
            if (const auto saved = characters_.find(character); saved != characters_.end())
                e = &saved->second;
        if (e)
        {
            auto change = Value::object();
            change.add("upsert", "players");
            change.add("key", character);
            change.add("value", wire::persistEntity(*e, world_.time()));
            changes.push(std::move(change));
        }
    }
    if (changes.size() == 0)
        return;
    const auto seq = ++journalSeq_;
    writer->append({seq, json::dump(changes)});
    if (holding_)
        heldFor_ = seq;
}

void Game::beginHolding(Connection* c)
{
    holding_ = c;
    held_.clear();
    heldFor_ = 0;
}

void Game::endHolding()
{
    Connection* c = holding_;
    holding_ = nullptr;
    auto held = std::move(held_);
    held_.clear();
    std::uint64_t seq = heldFor_;
    heldFor_ = 0;
    if (!c)
        return;
    // Behind anything of this client's still waiting, so its replies keep their order.
    if (!seq)
        for (auto it = waiting_.rbegin(); it != waiting_.rend(); ++it)
            if (it->c == c)
            {
                seq = it->seq;
                break;
            }
    for (auto& text : held)
        if (seq)
            waiting_.push_back({c, seq, std::move(text)});
        else
            c->event(text);
    if (seq)
        releaseCommitted();                        // A memory journal (tests) has written it already.
}

void Game::settle()
{
    hasher_.waitIdle();
    finishSignIns();
    if (auto* writer = store_ ? store_->journal() : nullptr)
        writer->flush();
    releaseCommitted();
}

void Game::releaseCommitted()
{
    auto* writer = store_ ? store_->journal() : nullptr;
    if (!writer || waiting_.empty())
        return;
    const auto written = writer->committed();
    for (auto it = waiting_.begin(); it != waiting_.end();)
        if (it->seq <= written)
        {
            it->c->event(it->event);
            it = waiting_.erase(it);
        }
        else
            ++it;
    const bool failing = writer->failing();
    if (failing != journalFailing_)
        note(failing ? "error" : "info", failing ? "RATW_JOURNAL cannot write; retrying, and replies wait: " + writer->error()
                                                 : "RATW_JOURNAL writing again");
    journalFailing_ = failing;
}

bool Game::replayJournal(Value& document, std::string& problem)
{
    if (!store_ || !store_->journal())
        return true;
    std::vector<journal::Record> records;
    const auto after = std::uint64_t(std::max(0.0, document.number("journal")));
    if (!store_->journalAfter(after, records, problem))
        return false;
    std::uint64_t last = after;
    if (!journal::replay(document, records, after, last, problem))
        return false;
    journalSeq_ = std::max(journalSeq_, last);
    if (!records.empty())
        note("info", "RATW_JOURNAL replayed " + std::to_string(records.size()) + " record(s) over the checkpoint (to " +
                         std::to_string(last) + ")");
    return true;
}

// --------------------------------------------------------------------------- Snapshots

void Game::syncCharacters()
{
    for (const auto& [id, e] : world_.entities())
        if (!e.npc)
            characters_[id] = e;
    for (auto& [id, e] : characters_)
        advanceAge(e, world_.calendarDays());
}

bool Game::forkSnapshot()
{
    if (snapshotChild_ > 0)
    {
        reapSnapshot(false);
        if (snapshotChild_ > 0)
            return true;                           // One at a time: the next is taken when this one is done.
    }
    const bool database = store_->database();
    const std::string target = store_->snapshotFile();
    if (!database && target.empty())
        return false;
    static std::string folder;
    static std::uint64_t taken = 0;
    std::string document, states;
    if (database)
    {
        if (folder.empty())
            folder = snapshotFolder();
        const auto name = folder + "/snapshot-" + std::to_string(++taken);
        document = name + ".json";
        states = name + ".npc";
    }
    else
        document = target + ".snapshot";
    store_->queueEvents(world_.takeEvents());
    syncCharacters();
    const auto seq = journalSeq_;
    std::fflush(nullptr);                          // Nothing buffered is written twice by the child.
    const pid_t pid = ::fork();
    if (pid < 0)
    {
        note("error", std::string("RATW_SNAPSHOT cannot fork: ") + std::strerror(errno) + "; saving in place");
        ++forkFailures_;
        return false;
    }
    if (pid == 0)
    {
        // The child: a frozen copy of the game. Capture, encode, write, and leave without running any destructor or
        // touching anything the parent's other threads hold.
        int status = 1;
        try
        {
            ::nice(5);
            Value doc;
            std::string npcStates;
            capture()(doc, npcStates);
            if (writeFile(document, json::dump(doc), !database) && (!database || writeFile(states, npcStates.empty() ? "[]" : npcStates, false)) &&
                (database || std::rename(document.c_str(), target.c_str()) == 0))
                status = 0;
        }
        catch (...)
        {
        }
        ::_exit(status);
    }
    snapshotChild_ = pid;
    snapshotJournal_ = seq;
    snapshotRevision_ = revision_;
    snapshotDocument_ = document;
    snapshotStates_ = states;
    return true;
}

void Game::reapSnapshot(bool wait)
{
    if (snapshotChild_ <= 0)
        return;
    int status = 0;
    pid_t done;
    do
        done = ::waitpid(snapshotChild_, &status, wait ? 0 : WNOHANG);
    while (done < 0 && errno == EINTR);
    if (done == 0)
        return;                                    // Still writing.
    snapshotChild_ = -1;
    if (done < 0 || !WIFEXITED(status) || WEXITSTATUS(status) != 0)
    {
        ++forkFailures_;
        note("error", "RATW_SNAPSHOT the snapshot could not be written" + std::string(forkFailures_ >= 3 ? "; saving in place from now on" : ""));
        std::remove(snapshotDocument_.c_str());
        if (!snapshotStates_.empty())
            std::remove(snapshotStates_.c_str());
        return;
    }
    forkFailures_ = 0;
    if (!store_->database())
    {
        if (auto* writer = store_->journal())      // The save file is in place: what it covers can go.
            writer->trim(snapshotJournal_);
        return;
    }
    auto files = std::make_shared<Leftovers>();   // Made in place: a copy's destructor would remove the files now.
    files->document = snapshotDocument_;
    files->states = snapshotStates_;
    const bool queued = store_->saveInBackground(
        [files](Value& document, std::string& npcStates) {
            std::string text, problem;
            if (!readText(files->document, text) || !json::parse(text, document, problem) || !document.isObject())
            {
                std::fprintf(stderr, "error: RATW_SNAPSHOT %s could not be read back (%s); not stored\n", files->document.c_str(),
                             problem.c_str());
                document = Value();               // Nothing to write after all.
                return;
            }
            readText(files->states, npcStates);
        },
        snapshotRevision_);
    if (!queued)
    {
        storageReady_ = false;
        note("error", "RATW persistence commit failed: " + store_->error());
        return;
    }
    trimWhenStored_.push_back({snapshotRevision_, snapshotJournal_});
}

void Game::trimStored()
{
    auto* writer = store_ ? store_->journal() : nullptr;
    if (!writer || trimWhenStored_.empty())
        return;
    const auto stored = store_->storedRevision();
    std::uint64_t upTo = 0;
    while (!trimWhenStored_.empty() && trimWhenStored_.front().first <= stored)
    {
        upTo = std::max(upTo, trimWhenStored_.front().second);
        trimWhenStored_.erase(trimWhenStored_.begin());
    }
    if (upTo)
        writer->trim(upTo);
}
} // namespace ratw::game
