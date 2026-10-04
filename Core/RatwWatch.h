#pragma once
// The watch feed (Docs/Design/34-dungeon-master-refresh.md, 1.1): where everyone is, for the Dungeon Master's LIVE map.
//
// While a Dungeon Master has the map open (dm.watchers has an unexpired row: the DM host renews it as the map asks for
// frames), the game offers a frame every couple of seconds and this writes the newest one to dm.watch, on its own
// thread and connection, so the game thread never waits on the database. With nobody watching, no frame is made.
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace ratw
{
class PgClient;

namespace watch
{
class Feed
{
  public:
    Feed(std::string conninfo, std::string worldId);
    ~Feed();
    Feed(const Feed&) = delete;
    Feed& operator=(const Feed&) = delete;
    // Someone is watching, as of the writer's last look (every couple of seconds).
    bool watching() const { return watching_.load(); }
    // The newest frame (JSON text); one not yet written is replaced.
    void offer(std::string frame);
    static constexpr double Interval = 2.0;      // Seconds between frames (placeholder).

  private:
    void run();
    std::string conninfo_, worldId_;
    std::unique_ptr<PgClient> pg_;
    std::atomic<bool> watching_{false};
    std::mutex mutex_;
    std::condition_variable wake_;
    std::string pending_;
    bool stopping_ = false;
    std::thread thread_;
};
} // namespace watch
} // namespace ratw
