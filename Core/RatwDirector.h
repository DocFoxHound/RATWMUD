#pragma once
// The operator bridge (see Docs/DM_BRIDGE_CONTRACT.md), for both servers. An opt-in, same-OS-user channel through a
// private directory: requests in outbox/, receipts in inbox/, and the world as snapshot.json. Never routed through a character command or a client snapshot.
#include "RatwJsonDoc.h"
#include "RatwWorld.h"

#include <functional>
#include <future>
#include <map>
#include <set>
#include <string>

namespace ratw::director
{
class Bridge
{
  public:
    using Notice = std::function<void(const std::set<std::string>& recipients, const std::string& text)>;
    // The checkpoint after a request: NotCommitted (nothing kept: the request is undone), Committed, or Journalled (the
    // checkpoint failed, but the journal took the request's economy change, so it stands and is replayed after a restart:
    // never reported undone; doc 79, Phase 9's first repair), its record committed before this is said; or Pending (the
    // checkpoint failed and the journal holds the record but hasn't written it: Phase 9's second repair). A Pending economy
    // act is neither undone nor answered: its request stays in outbox/ and the channel halts, so the next server answers
    // it from its receipt, which the same record carries (Game: the operator bridge's state), if the record was written,
    // or applies it then if it wasn't; never "it stands" for an act a crash could still lose. A bool works: true
    // Committed, false NotCommitted.
    enum : int { NotCommitted = 0, Committed = 1, Journalled = 2, Pending = 3 };
    using Commit = std::function<int()>;
    Bridge();
    ~Bridge();
    bool enabled() const { return !directory_.empty() && healthy_; }
    // An absolute, owner-private directory (made if missing). False if it can't be used.
    bool configure(const std::string& path);
    json::Value state() const;
    bool restore(const json::Value& state);
    json::Value snapshot(const World& world, const std::map<std::string, Entity>& characters, const std::set<std::string>& online,
                         const std::map<std::string, double>& activity, std::uint64_t revision, double now) const;
    json::Value execute(const json::Value& request, const std::string& expectedId, const std::string& fingerprint, World& world,
                        const std::set<std::string>& online, double now, const Commit& commit, const Notice& notice);
    void tick(double dt, World& world, const std::map<std::string, Entity>& characters, const std::set<std::string>& online,
              const std::map<std::string, double>& activity, std::uint64_t revision, const Commit& commit,
              const Notice& notice);
    std::function<void(const std::string&)> log;
    // The game's guards on a grant to a player (Docs/Design/78, 8: Game::dmActRefused, on a community world): why it is
    // refused, or "" to let it through; and, once a grant or fine is committed, the player told (Game::tellActedOn; the
    // signed pennies moved, negative for a fine). Unset: neither.
    std::function<std::string(const std::string& player, std::int64_t coins)> grantRefused;
    std::function<void(const std::string& player, std::int64_t moved, const std::string& reason)> granted;
    // While a commit runs: whether its request is a grant, fine, band or transfer (the journal's kinds), whose receipt the
    // commit's journal record carries (Game: the bridge's state), so a Pending one is answered from it after a restart.
    bool committingEconomy() const { return committingEconomy_; }

  private:
    bool committingEconomy_ = false;
    std::string directory_, worldId_;
    json::Value receipts_ = json::Value::array();
    double accumulator_ = 2;
    bool healthy_ = true;
    bool held_ = false;                           // (execute's last request Pending: left in outbox/, unanswered.)
    struct CachedTerrain
    {
        std::uint64_t fingerprint = 0;
        json::Value rows;
    };
    mutable std::map<std::string, CachedTerrain> terrain_;
    std::future<bool> write_;
    json::Value result(const std::string& id, bool ok, const std::string& detail, double now) const;
};
} // namespace ratw::director
