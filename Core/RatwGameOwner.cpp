// One server per world: a game server owns the world it runs, and a second can't start on it. Two servers writing the
// same world would overwrite each other's saves and journals.
//
// For a world in the database, the owner holds a session advisory lock (OwnerLock) on a connection of its own: it
// lasts exactly as long as that connection, so a server that crashes or is killed lets go at once, with nothing left
// to clean up. A second server is refused, told which process holds it, from where and since when. Every half minute
// the owner checks it still holds the lock (a database restart drops it); if it has lost it and another server took
// it, it saves nothing more and exits.
//
// For a world saved to a file, the owner holds an exclusive lock on <save>.lock, released when the process ends.
#include "RatwGame.h"

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

namespace ratw::game
{
namespace
{
// pg_try_advisory_lock(OwnerSpace, OwnerKey): the two-key form, apart from tools/publish.py's one-key lock.
constexpr const char* OwnerSpace = "1380013143";   // "RATW"
constexpr const char* OwnerKey = "1";              // The game server.

std::string hostName()
{
    char name[256] = {};
    return ::gethostname(name, sizeof name - 1) == 0 ? std::string(name) : std::string("this computer");
}
} // namespace

bool Game::takeOwnership(std::string& problem)
{
    if (!options_.database.empty())
    {
        ownerPg_ = std::make_unique<PgClient>();
        std::string error;
        if (!ownerPg_->connect(options_.conninfo, error))
        {
            problem = "cannot reach the " + options_.database + " database: " + error;
            return false;
        }
        // Named, so a refused server can say who holds the world.
        ownerPg_->exec("SELECT set_config('application_name', $1, false)",
                       {"ratw_server pid " + std::to_string(::getpid()) + " on " + hostName()});
        const auto taken = ownerPg_->exec(std::string("SELECT pg_try_advisory_lock(") + OwnerSpace + ", " + OwnerKey + ")");
        if (taken.ok && !taken.rows.empty() && taken.rows[0][0] && *taken.rows[0][0] == "t")
            return true;
        const auto holder = ownerPg_->exec(std::string(
            "SELECT a.application_name, coalesce(host(a.client_addr), 'this computer'), to_char(a.backend_start, 'YYYY-MM-DD HH24:MI') "
            "FROM pg_locks l JOIN pg_stat_activity a ON a.pid = l.pid "
            "WHERE l.locktype = 'advisory' AND l.classid = ") + OwnerSpace + " AND l.objid = " + OwnerKey + " AND l.objsubid = 2");
        std::string who = "another game server";
        if (holder.ok && !holder.rows.empty() && holder.rows[0][0])
            who = *holder.rows[0][0] + (holder.rows[0][1] ? " (connected from " + *holder.rows[0][1] + ")" : "") +
                  (holder.rows[0][2] ? ", running since " + *holder.rows[0][2] : "");
        problem = "the " + options_.database + " world is already being run by " + who +
                  ". Only one game server may run a world at a time: stop that one first.";
        ownerPg_.reset();
        return false;
    }
    if (options_.savePath.empty())
        return true;                                // A world in memory: nothing to share.
    const std::string lockPath = options_.savePath + ".lock";
    ownerFile_ = ::open(lockPath.c_str(), O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if (ownerFile_ < 0)
    {
        problem = "cannot open " + lockPath + ": " + std::strerror(errno);
        return false;
    }
    if (::flock(ownerFile_, LOCK_EX | LOCK_NB) != 0)
    {
        char held[64] = {};
        const auto n = ::pread(ownerFile_, held, sizeof held - 1, 0);
        ::close(ownerFile_);
        ownerFile_ = -1;
        problem = "the world saved at " + options_.savePath + " is already being run by another game server" +
                  (n > 0 ? std::string(" (") + std::string(held, std::size_t(n)) + ")" : std::string()) +
                  ". Only one game server may run a world at a time: stop that one first.";
        return false;
    }
    const std::string mine = "ratw_server pid " + std::to_string(::getpid()) + " on " + hostName();
    if (::ftruncate(ownerFile_, 0) == 0 && ::pwrite(ownerFile_, mine.data(), mine.size(), 0) < 0)
        note("warn", "RATW_OWNER cannot note this server in " + lockPath);
    return true;
}

void Game::keepOwnership(double dt)
{
    if (!ownerPg_ || (ownerCheck_ -= dt) > 0)
        return;
    ownerCheck_ = OwnerCheckSeconds;
    // Still ours? (A query reconnects a dropped connection without the lock: look, don't assume.)
    const auto held = ownerPg_->exec(std::string(
        "SELECT EXISTS (SELECT 1 FROM pg_locks WHERE locktype = 'advisory' AND classid = ") + OwnerSpace + " AND objid = " +
        OwnerKey + " AND objsubid = 2 AND pid = pg_backend_pid())");
    if (!held.ok)
        return;                                     // The database away: try again next time.
    if (!held.rows.empty() && held.rows[0][0] && *held.rows[0][0] == "t")
        return;
    const auto taken = ownerPg_->exec(std::string("SELECT pg_try_advisory_lock(") + OwnerSpace + ", " + OwnerKey + ")");
    if (taken.ok && !taken.rows.empty() && taken.rows[0][0] && *taken.rows[0][0] == "t")
    {
        note("warn", "RATW_OWNER the database connection dropped; this server holds the " + options_.database + " world again.");
        return;
    }
    note("error", "RATW_OWNER another game server has taken the " + options_.database +
                      " world while this one's database connection was down; stopping so the two don't overwrite each other.");
    storageReady_ = false;                          // Nothing more saved from here.
    exit_ = 1;
}

void Game::letGoOfOwnership()
{
    ownerPg_.reset();                               // The connection closes, and the lock with it.
    if (ownerFile_ >= 0)
    {
        ::close(ownerFile_);
        ownerFile_ = -1;
    }
}
} // namespace ratw::game
