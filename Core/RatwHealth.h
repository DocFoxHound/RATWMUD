#pragma once
// Server health (Docs/Design/31-responsiveness.md, "The health tracker"): how the game server ran, kept.
//
// The game hands this a record now and then: each minute's window (tick times and where they went, the world's own
// parts, traffic, players' pings as their pages report them, slow connections) and each tick that ran long. This
// writes them on its own thread, so the game thread never waits on a disk or the database: to dm.health (migration
// 0032) for a world in the database, where it also removes rows older than a fortnight; else to a JSON-lines file
// beside the save. tools/perf_report.py and the DM's Health tab read them back.
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace ratw
{
class PgClient;

namespace health
{
class Recorder
{
  public:
    // To the database, for this world.
    static std::unique_ptr<Recorder> database(std::string conninfo, std::string worldId);
    // To a file (appended to, a JSON object a line).
    static std::unique_ptr<Recorder> file(std::string path);
    ~Recorder();
    Recorder(const Recorder&) = delete;
    Recorder& operator=(const Recorder&) = delete;
    // A record: "window" or "spike", and its body (a JSON object). Dropped, not waited for, if the writer falls far
    // behind.
    void record(const std::string& kind, std::string body);
    static constexpr std::size_t Queued = 1000;
    static constexpr int KeepDays = 14;

  private:
    Recorder(std::string conninfo, std::string worldId, std::string path);
    void run();
    void write(const std::string& kind, const std::string& body, const std::string& at);
    std::string conninfo_, worldId_, path_;
    std::unique_ptr<PgClient> pg_;
    std::mutex mutex_;
    std::condition_variable wake_;
    std::deque<std::pair<std::string, std::string>> queue_;
    bool stopping_ = false;
    std::thread thread_;
};
} // namespace health
} // namespace ratw
