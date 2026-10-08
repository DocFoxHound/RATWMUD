#pragma once
// The chronicle (Docs/Design/56-fame-and-memory.md, 8): a character's life written from the ledgers on request, as
// template lines (Data/Fame/chronicle.json), never stored prose. `compile` is pure: rows of game.events where the
// character is actor or target, newest or oldest, and a lens that names everyone only as the owner knows them. The
// `Reader` reads those rows on its own thread and connection, as the cell prefetcher does.
#include "RatwJsonDoc.h"
#include "RatwPg.h"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace ratw::chronicle
{
struct Row
{
    double day = 0;
    std::string kind, actor, target, cell, item, detail;
    int quantity = 0;
    std::int64_t coins = 0;
};
struct Entry
{
    double day = 0;
    std::string text;
};
struct Lens
{
    std::function<std::string(const std::string& id)> name;     // As the owner knows them ("the miller", "Wren").
    std::function<std::string(const std::string& cell)> place;  // A cell's name.
};
// Templates: Data/Fame/chronicle.json, or `templates` given (tests).
std::vector<Entry> compile(const std::string& owner, std::vector<Row> rows, const Lens& lens, const json::Value& templates = {});
// "Summer 9, Year 1".
std::string dateWords(double day);

class Reader
{
  public:
    static constexpr const char* Query =
        "SELECT game_day, kind, actor, target, cell, item, quantity, coins, detail FROM game.events "
        "WHERE world_id = $1 AND (actor = $2 OR target = $2) ORDER BY id DESC LIMIT 2000";
    // What happened in residents' lives since a day (welcome back, doc 56, 10).
    static constexpr const char* SinceQuery =
        "SELECT game_day, kind, actor, target, cell, item, quantity, coins, detail FROM game.events "
        "WHERE world_id = $1 AND game_day >= $2::float8 AND kind IN ('marriage', 'death', 'apprenticeship', "
        "'apprenticeship completed', 'succession', 'mourning', 'relocation') ORDER BY id DESC LIMIT 500";
    struct Done
    {
        std::string owner, error, mode;            // mode: "chronicle" or "welcome".
        std::vector<Row> rows;
    };
    ~Reader() { stop(); }
    bool start(const std::string& conninfo, const std::string& worldId, std::string& problem);
    void stop();
    bool running() const { return worker_.joinable(); }
    void ask(const std::string& owner);
    void askSince(const std::string& owner, double day);   // A "welcome" answer.
    std::vector<Done> take();

  private:
    void run();
    PgClient pg_;
    std::string world_;
    std::thread worker_;
    std::mutex lock_;
    std::condition_variable wake_;
    struct Asked
    {
        std::string owner, mode;
        double since = 0;
    };
    std::deque<Asked> asked_;
    std::vector<Done> done_;
    bool stopping_ = false;
};
} // namespace ratw::chronicle
