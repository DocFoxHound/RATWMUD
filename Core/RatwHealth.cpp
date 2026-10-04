#include "RatwHealth.h"

#include "RatwPg.h"

#include <chrono>
#include <ctime>
#include <fstream>

namespace ratw::health
{
std::unique_ptr<Recorder> Recorder::database(std::string conninfo, std::string worldId)
{
    return std::unique_ptr<Recorder>(new Recorder(std::move(conninfo), std::move(worldId), {}));
}

std::unique_ptr<Recorder> Recorder::file(std::string path)
{
    return std::unique_ptr<Recorder>(new Recorder({}, {}, std::move(path)));
}

Recorder::Recorder(std::string conninfo, std::string worldId, std::string path)
    : conninfo_(std::move(conninfo)), worldId_(std::move(worldId)), path_(std::move(path)),
      pg_(conninfo_.empty() ? nullptr : std::make_unique<PgClient>()), thread_([this] { run(); })
{
}

Recorder::~Recorder()
{
    {
        std::lock_guard lock(mutex_);
        stopping_ = true;
    }
    wake_.notify_all();
    thread_.join();
}

void Recorder::record(const std::string& kind, std::string body)
{
    {
        std::lock_guard lock(mutex_);
        if (queue_.size() >= Queued)
            return;
        queue_.emplace_back(kind, std::move(body));
    }
    wake_.notify_all();
}

void Recorder::run()
{
    using namespace std::chrono;
    auto nextPrune = steady_clock::now();
    for (;;)
    {
        std::deque<std::pair<std::string, std::string>> batch;
        bool stopping;
        {
            std::unique_lock lock(mutex_);
            wake_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
            batch.swap(queue_);
            stopping = stopping_;
        }
        const auto now = std::time(nullptr);
        char at[32];
        std::strftime(at, sizeof at, "%Y-%m-%dT%H:%M:%SZ", std::gmtime(&now));
        for (const auto& [kind, body] : batch)
            write(kind, body, at);
        if (pg_ && pg_->connected() && steady_clock::now() >= nextPrune)
        {
            pg_->exec("DELETE FROM dm.health WHERE world_id = $1 AND at < now() - make_interval(days => $2::int)",
                      {worldId_, std::to_string(KeepDays)});
            nextPrune = steady_clock::now() + hours(1);
        }
        if (stopping)
            return;
    }
}

void Recorder::write(const std::string& kind, const std::string& body, const std::string& at)
{
    if (pg_)
    {
        std::string error;
        if (!pg_->connected() && !pg_->connect(conninfo_, error))
            return;                                 // The database away: this record is lost, the game unaffected.
        pg_->exec("INSERT INTO dm.health (world_id, kind, body) VALUES ($1, $2, $3::jsonb)", {worldId_, kind, body});
        return;
    }
    std::ofstream out(path_, std::ios::app);
    out << "{\"at\":\"" << at << "\",\"kind\":\"" << kind << "\",\"body\":" << body << "}\n";
}
} // namespace ratw::health
