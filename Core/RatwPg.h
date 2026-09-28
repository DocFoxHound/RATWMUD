#pragma once
// A small PostgreSQL client for the game server (world builds, saves, release
// notifications). It loads the system libpq at run time rather than linking
// it, so the engine's own toolchain never has to link against the system's C
// library, and a server that does not use the database never needs it.
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ratw
{
using PgValue = std::optional<std::string>;   // std::nullopt is SQL NULL.

struct PgResult
{
    bool ok = false;
    std::string error;
    std::vector<std::vector<PgValue>> rows;
};

class PgClient
{
  public:
    PgClient() = default;
    PgClient(const PgClient&) = delete;
    PgClient& operator=(const PgClient&) = delete;
    ~PgClient();

    // Whether libpq can be loaded on this machine; `error` says why not.
    static bool available(std::string& error);
    // `conninfo` is a libpq connection string or URL (e.g. "host=... port=... dbname=... user=... password=...").
    bool connect(const std::string& conninfo, std::string& error);
    bool connected() const;
    void close();
    // One statement with $1, $2... parameters sent separately from the SQL (never interpolated).
    // A dropped connection is re-established once before the statement is retried.
    PgResult exec(const std::string& sql, const std::vector<PgValue>& params = {});
    // LISTEN on a channel (lowercase letters, digits and _ only).
    bool listen(const std::string& channel, std::string& error);
    // Notifications received since the last call: (channel, payload). Never blocks.
    std::vector<std::pair<std::string, std::string>> notifications();

  private:
    void* conn_ = nullptr;
    std::vector<std::string> listening_;
};
} // namespace ratw
