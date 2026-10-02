// The journal (Core/RatwJournal.h; Docs/Design/31-responsiveness.md, Phase 2): changes applied to a checkpoint
// document, records replayed in order, the file journal written, read back after a crash and trimmed, and the writer
// keeping a batch it could not write until it can.
#include "RatwJournal.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unistd.h>

using namespace ratw;

namespace
{
int checks = 0;
void expect(bool condition, const std::string& message)
{
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}

json::Value parse(const std::string& text)
{
    json::Value v;
    std::string error;
    if (!json::parse(text, v, error))
        throw std::runtime_error("bad test JSON: " + error);
    return v;
}

void changesApply()
{
    auto doc = parse(R"({"society":{"accounts":{"a":{"cash":1}},"minted":5},"players":[{"id":"p1","x":1}],"crime":{}})");
    std::string problem;
    expect(journal::apply(doc, parse(R"([
        {"set":["society","accounts","b"],"value":{"cash":7}},
        {"set":["society","minted"],"value":9},
        {"erase":["society","accounts","a"]},
        {"upsert":"players","key":"p1","value":{"id":"p1","x":4}},
        {"upsert":"players","key":"p2","value":{"id":"p2","x":2}},
        {"set":["companions","npc_wren"],"value":"p1"},
        {"erase":["nothing","here"]}
    ])"), problem), "changes apply: " + problem);
    const auto& society = doc["society"];
    expect(society["accounts"]["b"].number("cash") == 7, "a value set");
    expect(!society["accounts"].has("a"), "a value removed");
    expect(society.number("minted") == 9, "a number set");
    expect(doc.array("players").size() == 2 && doc.array("players")[0].number("x") == 4 && doc.array("players")[1].string("id") == "p2",
           "a list entry replaced and another added");
    expect(doc["companions"].string("npc_wren") == "p1", "a missing object is made on the way");
    // Twice changes nothing more: every change says what something now is.
    auto again = doc;
    expect(journal::apply(again, parse(R"([{"set":["society","minted"],"value":9},{"upsert":"players","key":"p2","value":{"id":"p2","x":2}}])"),
                          problem) &&
               again == doc,
           "applying a record twice is harmless");
    for (const auto* bad : {R"({"set":["a"]})", R"([{"set":[],"value":1}])", R"([{"set":[1],"value":1}])", R"([{"upsert":"players","key":"","value":{}}])",
                            R"([{"frobnicate":true}])", R"([7])"})
        expect(!journal::apply(doc, parse(bad), problem), std::string("refused: ") + bad);
}

void recordsReplayInOrder()
{
    auto doc = parse(R"({"journal":2,"society":{"minted":1}})");
    std::vector<journal::Record> records = {{1, R"([{"set":["society","minted"],"value":100}])"},
                                            {2, R"([{"set":["society","minted"],"value":200}])"},
                                            {3, R"([{"set":["society","minted"],"value":3}])"},
                                            {4, R"([{"set":["society","sunk"],"value":4}])"}};
    std::uint64_t last = 0;
    std::string problem;
    expect(journal::replay(doc, records, 2, last, problem), "replays: " + problem);
    expect(doc["society"].number("minted") == 3 && doc["society"].number("sunk") == 4 && last == 4,
           "only the records after the checkpoint's own, in order");
    records.push_back({5, "not json"});
    expect(!journal::replay(doc, records, 2, last, problem) && problem.find("5") != std::string::npos, "a broken record is named");
}

void theFileJournal()
{
    const std::string path = "/tmp/ratw-journal-test-" + std::to_string(::getpid()) + ".journal";
    std::remove(path.c_str());
    {
        auto writer = journal::fileWriter(path);
        for (std::uint64_t seq = 1; seq <= 5; ++seq)
            writer->append({seq, "[{\"set\":[\"n\"],\"value\":" + std::to_string(seq) + "}]"});
        expect(writer->flush() && writer->committed() == 5, "five records written");
    }
    std::vector<journal::Record> read;
    std::string problem;
    expect(journal::readFile(path, 2, read, problem) && read.size() == 3 && read.front().seq == 3 && read.back().seq == 5,
           "read back after a seq: " + problem);
    {
        std::ofstream torn(path, std::ios::app);
        torn << "6\t[{\"set\":[\"n\"],\"va";   // A crash mid-write: never counted as written.
    }
    read.clear();
    expect(journal::readFile(path, 0, read, problem) && read.size() == 5, "a line cut short by a crash is ignored");
    {
        auto writer = journal::fileWriter(path);
        writer->append({6, "[]"});
        writer->trim(4);
        expect(writer->flush(), "written again");
    }
    // The trim waits for the writer: let it finish by closing it (above), then read.
    read.clear();
    expect(journal::readFile(path, 0, read, problem), "readable: " + problem);
    bool trimmed = !read.empty() && read.front().seq == 5;
    for (const auto& r : read)
        trimmed = trimmed && r.seq > 4;
    expect(trimmed, "records up to 4 trimmed, the rest kept");
    {
        std::ofstream bad(path, std::ios::trunc);
        bad << "x\t[]\n";
    }
    read.clear();
    expect(!journal::readFile(path, 0, read, problem), "a damaged line is refused, not skipped");
    std::remove(path.c_str());
    read.clear();
    expect(journal::readFile(path, 0, read, problem) && read.empty(), "no journal yet is an empty one");
}

void aFailedBatchIsKeptUntilWritten()
{
    std::atomic<int> attempts{0};
    std::atomic<bool> down{true};
    std::vector<journal::Record> stored;
    std::mutex lock;
    journal::Writer writer(
        [&](const std::vector<journal::Record>& batch, std::string& error) {
            ++attempts;
            if (down)
            {
                error = "the database is away";
                return false;
            }
            std::lock_guard<std::mutex> guard(lock);
            stored.insert(stored.end(), batch.begin(), batch.end());
            return true;
        },
        nullptr);
    writer.append({1, "[]"});
    writer.append({2, "[]"});
    for (int i = 0; i < 200 && !writer.failing(); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    expect(writer.failing() && writer.committed() == 0 && writer.error() == "the database is away", "a failure is seen and said");
    writer.append({3, "[]"});
    down = false;
    for (int i = 0; i < 400 && writer.committed() < 3; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    expect(writer.committed() == 3 && !writer.failing(), "and once it can, everything is written");
    std::lock_guard<std::mutex> guard(lock);
    expect(stored.size() == 3 && stored[0].seq == 1 && stored[1].seq == 2 && stored[2].seq == 3, "once each, in order");
    expect(attempts >= 2, "after trying again");
}
} // namespace

int main()
{
    try
    {
        changesApply();
        recordsReplayInOrder();
        theFileJournal();
        aFailedBatchIsKeptUntilWritten();
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAILED after " << checks << " checks: " << e.what() << '\n';
        return 1;
    }
    std::cout << "Journal tests passed: " << checks << " checks.\n";
    return 0;
}
