#pragma once
#include "Core/RatwPg.h"
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

// Fetches streamed cells' files from the world database ahead of need, on its own thread and connection, so that
// the game thread loading a cell (World::ensureLoaded) usually finds its text already here instead of waiting on the
// database. A build's cells never change while a server runs it, so what is kept here can't go stale.
class FRatwCellPrefetch
{
  public:
    static constexpr std::size_t Kept = 48;       // Cells kept fetched (a large one is about half a megabyte).

    ~FRatwCellPrefetch() { Stop(); }

    // Query: (build, cell) -> one row of (file, seams).
    static constexpr const char* CellQuery = "SELECT body, seams FROM world.build_cells WHERE build_id = $1 AND cell_id = $2";

    bool Start(const std::string& ConnInfo, const std::string& Build, std::string& Problem,
               const std::string& Query = CellQuery)
    {
        if (!Pg.connect(ConnInfo, Problem))
            return false;
        BuildId = Build;
        Sql = Query;
        Worker = std::thread([this] { Run(); });
        return true;
    }
    void Stop()
    {
        {
            std::lock_guard<std::mutex> Guard(Lock);
            Stopping = true;
        }
        Wake.notify_all();
        if (Worker.joinable())
            Worker.join();
    }
    // The cells worth having ready now (replaces the previous wish list).
    void Want(const std::vector<std::string>& Ids)
    {
        {
            std::lock_guard<std::mutex> Guard(Lock);
            Queue.clear();
            for (const auto& Id : Ids)
                if (!Ready.count(Id))
                    Queue.push_back(Id);
        }
        Wake.notify_all();
    }
    // A fetched cell's file and seams, if it is here; it is handed over (and no longer kept).
    bool Take(const std::string& Id, std::string& Body, std::string& Seams)
    {
        std::lock_guard<std::mutex> Guard(Lock);
        const auto Found = Ready.find(Id);
        if (Found == Ready.end())
            return false;
        Body = std::move(Found->second.Body);
        Seams = std::move(Found->second.Seams);
        Ready.erase(Found);
        ++Hits;
        return true;
    }
    std::size_t HitCount() const
    {
        std::lock_guard<std::mutex> Guard(Lock);
        return Hits;
    }
    std::size_t ReadyCount() const
    {
        std::lock_guard<std::mutex> Guard(Lock);
        return Ready.size();
    }

  private:
    struct FFetched
    {
        std::string Body, Seams;
        std::uint64_t Order = 0;
    };
    void Run()
    {
        std::unique_lock<std::mutex> Guard(Lock);
        for (;;)
        {
            Wake.wait(Guard, [this] { return Stopping || !Queue.empty(); });
            if (Stopping)
                return;
            const std::string Id = Queue.front();
            Queue.pop_front();
            if (Ready.count(Id))
                continue;
            Guard.unlock();
            const auto Cell = Pg.exec(Sql, {BuildId, Id});
            Guard.lock();
            if (!Cell.ok || Cell.rows.empty() || !Cell.rows[0][0])
                continue;                                   // The game thread will ask the database itself.
            Ready[Id] = {*Cell.rows[0][0], Cell.rows[0][1] ? *Cell.rows[0][1] : std::string(), ++Fetches};
            while (Ready.size() > Kept)
            {
                auto Oldest = Ready.begin();
                for (auto It = Ready.begin(); It != Ready.end(); ++It)
                    if (It->second.Order < Oldest->second.Order)
                        Oldest = It;
                Ready.erase(Oldest);
            }
        }
    }

    ratw::PgClient Pg;                              // The worker's own connection.
    std::string BuildId, Sql;
    std::thread Worker;
    mutable std::mutex Lock;                        // Everything below.
    std::condition_variable Wake;
    std::deque<std::string> Queue;
    std::map<std::string, FFetched> Ready;
    std::uint64_t Fetches = 0;
    std::size_t Hits = 0;
    bool Stopping = false;
};
