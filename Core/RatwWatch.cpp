#include "RatwWatch.h"

#include "RatwPg.h"

#include <chrono>

namespace ratw::watch
{
Feed::Feed(std::string conninfo, std::string worldId)
    : conninfo_(std::move(conninfo)), worldId_(std::move(worldId)), pg_(std::make_unique<PgClient>()), thread_([this] { run(); })
{
}

Feed::~Feed()
{
    {
        std::lock_guard lock(mutex_);
        stopping_ = true;
    }
    wake_.notify_all();
    thread_.join();
}

void Feed::offer(std::string frame)
{
    {
        std::lock_guard lock(mutex_);
        pending_ = std::move(frame);
    }
    wake_.notify_all();
}

void Feed::run()
{
    using namespace std::chrono;
    auto nextLook = steady_clock::now();
    for (;;)
    {
        std::string frame;
        {
            std::unique_lock lock(mutex_);
            wake_.wait_until(lock, nextLook, [this] { return stopping_ || !pending_.empty(); });
            if (stopping_)
                return;
            frame.swap(pending_);
        }
        std::string error;
        if (!pg_->connected() && !pg_->connect(conninfo_, error))
        {
            watching_ = false;
            nextLook = steady_clock::now() + seconds(10);
            continue;
        }
        if (!frame.empty() && watching_)
            pg_->exec("INSERT INTO dm.watch (world_id, frame, written_at) VALUES ($1, $2, now()) "
                      "ON CONFLICT (world_id) DO UPDATE SET frame = excluded.frame, written_at = now()",
                      {worldId_, frame});
        if (steady_clock::now() >= nextLook)
        {
            const auto looked = pg_->exec("SELECT EXISTS (SELECT 1 FROM dm.watchers WHERE until > now())");
            watching_ = looked.ok && !looked.rows.empty() && looked.rows[0][0] && *looked.rows[0][0] == "t";
            nextLook = steady_clock::now() + duration_cast<steady_clock::duration>(duration<double>(Interval));
        }
    }
}
} // namespace ratw::watch
