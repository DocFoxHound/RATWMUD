#include "RatwJournal.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

namespace ratw::journal
{
using json::Value;

namespace
{
bool pathOf(const Value& change, const char* key, std::vector<std::string>& path, std::string& problem)
{
    path.clear();
    for (const auto& part : change.array(key))
    {
        if (!part.isString())
        {
            problem = "a journal path that isn't names";
            return false;
        }
        path.push_back(part.asString());
    }
    if (path.empty())
        problem = "an empty journal path";
    return !path.empty();
}
} // namespace

bool apply(Value& document, const Value& changes, std::string& problem)
{
    if (!changes.isArray() || !document.isObject())
    {
        problem = "a journal record that isn't a list of changes";
        return false;
    }
    std::vector<std::string> path;
    for (const auto& change : changes.items())
    {
        if (!change.isObject())
        {
            problem = "a journal change that isn't an object";
            return false;
        }
        if (change.has("set") || change.has("erase"))
        {
            const bool set = change.has("set");
            if (!pathOf(change, set ? "set" : "erase", path, problem))
                return false;
            Value* at = &document;
            for (std::size_t i = 0; i + 1 < path.size(); ++i)
            {
                Value* next = at->find(path[i]);
                if (!next || !next->isObject())
                {
                    if (!set)
                    {
                        at = nullptr;
                        break;
                    }
                    next = &at->set(path[i], Value::object());
                }
                at = next;
            }
            if (!at)
                continue;                          // Nothing there to remove.
            if (set)
                at->set(path.back(), change["value"]);
            else
                at->erase(path.back());
        }
        else if (change.has("upsert"))
        {
            const std::string list = change.string("upsert"), key = change.string("key");
            if (list.empty() || key.empty() || !change["value"].isObject())
            {
                problem = "a journal upsert without its list, key or value";
                return false;
            }
            Value* entries = document.find(list);
            if (!entries || !entries->isArray())
                entries = &document.set(list, Value::array());
            bool replaced = false;
            for (auto& entry : entries->items())
                if (entry.string("id") == key)
                {
                    entry = change["value"];
                    replaced = true;
                    break;
                }
            if (!replaced)
                entries->push(change["value"]);
        }
        else
        {
            problem = "a journal change of no known kind";
            return false;
        }
    }
    return true;
}

bool replay(Value& document, const std::vector<Record>& records, std::uint64_t after, std::uint64_t& last, std::string& problem)
{
    last = after;
    for (const auto& r : records)
    {
        if (r.seq <= after)
            continue;
        Value changes;
        std::string error;
        if (!json::parse(r.text, changes, error) || !apply(document, changes, error))
        {
            problem = "journal record " + std::to_string(r.seq) + ": " + error;
            return false;
        }
        last = std::max(last, r.seq);
    }
    return true;
}

// --------------------------------------------------------------------------- The writer

Writer::Writer(Commit commit, Trim trim, bool threaded) : commit_(std::move(commit)), trim_(std::move(trim)), threaded_(threaded)
{
    if (threaded_)
        thread_ = std::thread([this] { run(); });
}

Writer::~Writer()
{
    {
        std::lock_guard<std::mutex> guard(lock_);
        stopping_ = true;
    }
    wake_.notify_all();
    if (thread_.joinable())
        thread_.join();
}

void Writer::append(Record record)
{
    if (!threaded_)
    {
        std::string error;
        std::lock_guard<std::mutex> guard(lock_);
        appended_ = std::max(appended_, record.seq);
        failing_ = !commit_({record}, error);
        error_ = error;
        if (!failing_)
            committed_ = std::max(committed_, record.seq);
        return;
    }
    {
        std::lock_guard<std::mutex> guard(lock_);
        appended_ = std::max(appended_, record.seq);
        queue_.push_back(std::move(record));
    }
    wake_.notify_all();
}

void Writer::trim(std::uint64_t upTo)
{
    if (!threaded_)
    {
        std::string error;
        std::lock_guard<std::mutex> guard(lock_);
        if (trim_)
            trim_(std::min(upTo, committed_), error);
        return;
    }
    {
        std::lock_guard<std::mutex> guard(lock_);
        trimTo_ = std::max(trimTo_, upTo);
    }
    wake_.notify_all();
}

std::uint64_t Writer::committed() const
{
    std::lock_guard<std::mutex> guard(lock_);
    return committed_;
}

bool Writer::failing() const
{
    std::lock_guard<std::mutex> guard(lock_);
    return failing_;
}

std::string Writer::error() const
{
    std::lock_guard<std::mutex> guard(lock_);
    return error_;
}

bool Writer::flush()
{
    std::unique_lock<std::mutex> guard(lock_);
    idle_.wait(guard, [this] { return committed_ >= appended_ || failing_; });
    return committed_ >= appended_;
}

void Writer::run()
{
    std::uint64_t trimmed = 0;
    std::unique_lock<std::mutex> guard(lock_);
    for (;;)
    {
        // A trim waits for the records it covers to be written first.
        wake_.wait(guard, [this, trimmed] { return stopping_ || !queue_.empty() || std::min(trimTo_, committed_) > trimmed; });
        if (queue_.empty() && std::min(trimTo_, committed_) <= trimmed && stopping_)
            break;
        std::vector<Record> batch;
        batch.swap(queue_);
        const std::uint64_t trimTo = std::min(trimTo_, committed_);
        guard.unlock();
        std::string error;
        bool ok = batch.empty() || commit_(batch, error);
        guard.lock();
        if (ok && !batch.empty())
            committed_ = std::max(committed_, batch.back().seq);
        failing_ = !ok;
        error_ = error;
        idle_.notify_all();
        if (!ok)
        {
            // Put the batch back in front of anything appended since, and try again after a moment. A stop gives up
            // here: the final checkpoint the game writes as it stops holds everything anyway.
            batch.insert(batch.end(), std::make_move_iterator(queue_.begin()), std::make_move_iterator(queue_.end()));
            queue_.swap(batch);
            if (stopping_)
                break;
            wake_.wait_for(guard, std::chrono::milliseconds(RetryMs), [this] { return stopping_; });
            continue;
        }
        if (trimTo > trimmed && trim_)
        {
            guard.unlock();
            std::string trimError;
            const bool trimmedOk = trim_(trimTo, trimError);
            guard.lock();
            if (trimmedOk)
                trimmed = trimTo;
            else
                trimTo_ = trimmed;                 // Not worth a retry loop: the next snapshot asks again.
        }
    }
    idle_.notify_all();
}

// --------------------------------------------------------------------------- Files

namespace
{
bool writeAll(int fd, const std::string& text)
{
    std::size_t done = 0;
    while (done < text.size())
    {
        const auto n = ::write(fd, text.data() + done, text.size() - done);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0)
            return false;
        done += std::size_t(n);
    }
    return true;
}

bool readLines(const std::string& path, std::vector<Record>& out, std::string& problem)
{
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return true;                               // No journal yet.
    std::ostringstream text;
    text << in.rdbuf();
    const std::string all = text.str();
    std::size_t at = 0;
    while (at < all.size())
    {
        const auto end = all.find('\n', at);
        if (end == std::string::npos)
            break;                                 // Cut short by a crash mid-write: never counted as written.
        const std::string line = all.substr(at, end - at);
        at = end + 1;
        const auto tab = line.find('\t');
        char* stop = nullptr;
        const unsigned long long seq = tab == std::string::npos ? 0 : std::strtoull(line.c_str(), &stop, 10);
        if (!seq || stop != line.c_str() + tab)
        {
            problem = "an unreadable line in " + path;
            return false;
        }
        out.push_back({std::uint64_t(seq), line.substr(tab + 1)});
    }
    return true;
}
} // namespace

std::unique_ptr<Writer> fileWriter(const std::string& path)
{
    auto commit = [path](const std::vector<Record>& batch, std::string& error) {
        const int fd = ::open(path.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0600);
        if (fd < 0)
        {
            error = "Cannot write " + path + ": " + std::strerror(errno);
            return false;
        }
        std::string text;
        for (const auto& r : batch)
            text += std::to_string(r.seq) + '\t' + r.text + '\n';
        const bool ok = writeAll(fd, text) && ::fdatasync(fd) == 0;
        if (!ok)
            error = "Cannot write " + path + ": " + std::strerror(errno);
        ::close(fd);
        return ok;
    };
    auto trim = [path](std::uint64_t upTo, std::string& error) {
        std::vector<Record> all;
        if (!readLines(path, all, error))
            return false;
        std::string text;
        for (const auto& r : all)
            if (r.seq > upTo)
                text += std::to_string(r.seq) + '\t' + r.text + '\n';
        const std::string temp = path + ".tmp";
        const int fd = ::open(temp.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
        if (fd < 0)
            return false;
        const bool ok = writeAll(fd, text) && ::fdatasync(fd) == 0;
        ::close(fd);
        return ok && std::rename(temp.c_str(), path.c_str()) == 0;
    };
    return std::make_unique<Writer>(commit, trim);
}

bool readFile(const std::string& path, std::uint64_t after, std::vector<Record>& out, std::string& problem)
{
    std::vector<Record> all;
    if (!readLines(path, all, problem))
        return false;
    for (auto& r : all)
        if (r.seq > after)
            out.push_back(std::move(r));
    std::stable_sort(out.begin(), out.end(), [](const Record& a, const Record& b) { return a.seq < b.seq; });
    return true;
}

std::unique_ptr<Writer> memoryWriter(std::shared_ptr<std::vector<Record>> into)
{
    auto commit = [into](const std::vector<Record>& batch, std::string&) {
        into->insert(into->end(), batch.begin(), batch.end());
        return true;
    };
    auto trim = [into](std::uint64_t upTo, std::string&) {
        into->erase(std::remove_if(into->begin(), into->end(), [upTo](const Record& r) { return r.seq <= upTo; }), into->end());
        return true;
    };
    return std::make_unique<Writer>(commit, trim, false);
}
} // namespace ratw::journal
