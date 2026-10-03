#pragma once
// A fixed pool of worker threads for the game's per-player work (Docs/Design/31-responsiveness.md, Phase 4): after the
// world's tick, each due player's snapshot is keyed, written and compressed on its own core. run() hands out the jobs
// and returns when every one is done; the calling thread takes jobs too.
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace ratw
{
class Pool
{
  public:
    // `threads` workers besides the caller (0: run every job on the caller).
    explicit Pool(unsigned threads)
    {
        for (unsigned i = 0; i < threads; ++i)
            workers_.emplace_back([this] { work(); });
    }
    ~Pool()
    {
        {
            std::lock_guard<std::mutex> guard(lock_);
            stopping_ = true;
        }
        wake_.notify_all();
        for (auto& t : workers_)
            t.join();
    }
    Pool(const Pool&) = delete;
    Pool& operator=(const Pool&) = delete;

    std::size_t threads() const { return workers_.size(); }

    // job(i) for every i below count, spread over the workers and the caller; returns once all are done.
    void run(std::size_t count, const std::function<void(std::size_t)>& job)
    {
        if (count == 0)
            return;
        if (workers_.empty() || count == 1)
        {
            for (std::size_t i = 0; i < count; ++i)
                job(i);
            return;
        }
        auto round = std::make_shared<Round>();
        round->job = &job;
        round->count = count;
        {
            std::lock_guard<std::mutex> guard(lock_);
            current_ = round;
            ++rounds_;
        }
        wake_.notify_all();
        take(*round);
        std::unique_lock<std::mutex> guard(lock_);
        finished_.wait(guard, [&] { return round->done.load() == count; });
        current_.reset();
    }

  private:
    // One run's jobs. Each run has its own counters, so a worker still finishing an old run can't take a new run's.
    struct Round
    {
        const std::function<void(std::size_t)>* job = nullptr;
        std::size_t count = 0;
        std::atomic<std::size_t> next{0}, done{0};
    };
    void take(Round& round)
    {
        for (std::size_t i = round.next.fetch_add(1); i < round.count; i = round.next.fetch_add(1))
        {
            (*round.job)(i);
            if (round.done.fetch_add(1) + 1 == round.count)
            {
                std::lock_guard<std::mutex> guard(lock_);
                finished_.notify_all();
            }
        }
    }
    void work()
    {
        std::uint64_t seen = 0;
        for (;;)
        {
            std::shared_ptr<Round> round;
            {
                std::unique_lock<std::mutex> guard(lock_);
                wake_.wait(guard, [&] { return stopping_ || (rounds_ != seen && current_); });
                if (stopping_)
                    return;
                seen = rounds_;
                round = current_;
            }
            take(*round);
        }
    }

    std::vector<std::thread> workers_;
    std::mutex lock_;
    std::condition_variable wake_, finished_;
    std::shared_ptr<Round> current_;
    std::uint64_t rounds_ = 0;
    bool stopping_ = false;
};
} // namespace ratw
