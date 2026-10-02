#pragma once
// The journal (Docs/Design/31-responsiveness.md, Phase 2): valuable changes written while the game plays on.
//
// A valuable command (a trade, eating, a contract taken, an account registered...) changes the world at once, and its
// changes are appended here as one record: a JSON array of changes to the checkpoint document. A writer thread commits
// records in batches (each batch forms while the one before is being written) and says how far it has got; the
// game tells the player the command is done once its record is committed. On start-up, the records after the
// checkpoint's own ("journal" in its document) are applied over it, so a crash loses no committed valuable.
//
// Changes, in order:
//   {"set": ["society", "accounts", "player-ash"], "value": {...}}   the value at that path of object keys
//   {"erase": ["companions", "npc_wren"]}                            remove what is at that path
//   {"upsert": "players", "key": "player-ash", "value": {...}}       the entry of a top-level list with that "id"
//                                                                    replaced, or added
// Every change says what something now is, never by how much it changed, so applying a record twice is harmless.
#include "RatwJsonDoc.h"

#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace ratw::journal
{
struct Record
{
    std::uint64_t seq = 0;
    std::string text;                             // A JSON array of changes.
};

// Applies one record's changes to a checkpoint document. False, with the problem, for anything malformed (the
// document may then be part-changed: the caller refuses it).
bool apply(json::Value& document, const json::Value& changes, std::string& problem);
// Applies records in order (those at or before `after` are skipped). The highest seq applied, in `last`.
bool replay(json::Value& document, const std::vector<Record>& records, std::uint64_t after, std::uint64_t& last,
            std::string& problem);

// Commits records on its own thread, in order and in batches, retrying a batch that fails until it is written.
class Writer
{
  public:
    // Writes a batch durably (all of it or none), and removes records up to a seq. Both run on the writer's thread.
    using Commit = std::function<bool(const std::vector<Record>& batch, std::string& error)>;
    using Trim = std::function<bool(std::uint64_t upTo, std::string& error)>;

    // `threaded`: on a thread of its own (false writes as each record is appended: the memory journal of tests).
    Writer(Commit commit, Trim trim, bool threaded = true);
    ~Writer();                                    // Commits what is queued, then stops.
    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;

    void append(Record record);
    void trim(std::uint64_t upTo);                // Done after everything appended before it.
    std::uint64_t committed() const;              // The highest seq written.
    bool failing() const;                         // The last attempt failed (it is being retried).
    std::string error() const;
    bool flush();                                 // Waits until everything appended is written; false if it isn't.

    static constexpr int RetryMs = 500;

  private:
    void run();

    Commit commit_;
    Trim trim_;
    mutable std::mutex lock_;
    std::condition_variable wake_, idle_;
    std::vector<Record> queue_;
    std::uint64_t committed_ = 0, trimTo_ = 0, appended_ = 0;
    bool stopping_ = false, failing_ = false, threaded_ = true;
    std::string error_;
    std::thread thread_;
};

// The journal of a world saved to a file: its records beside the save ("<save>.journal"), one per line
// ("<seq>\t<record>"), each batch synced to disk before it counts as written.
std::unique_ptr<Writer> fileWriter(const std::string& path);
// Its records after `after`, in order. A last line cut short by a crash is ignored; anything else unreadable is not.
bool readFile(const std::string& path, std::uint64_t after, std::vector<Record>& out, std::string& problem);
// Kept in memory: for tests.
std::unique_ptr<Writer> memoryWriter(std::shared_ptr<std::vector<Record>> into);
} // namespace ratw::journal
