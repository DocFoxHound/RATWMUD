#pragma once
// The operator bridge, portable: Source/RATWMUD/Runtime/RatwDMBridge.h (see Docs/DM_BRIDGE_CONTRACT.md) for the
// standalone server. An opt-in, same-OS-user channel through a private directory: requests in outbox/, receipts in
// inbox/, and the world as snapshot.json. Never routed through a character command or a client snapshot.
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
                        const std::set<std::string>& online, double now, const std::function<bool()>& commit, const Notice& notice);
    void tick(double dt, World& world, const std::map<std::string, Entity>& characters, const std::set<std::string>& online,
              const std::map<std::string, double>& activity, std::uint64_t revision, const std::function<bool()>& commit,
              const Notice& notice);
    std::function<void(const std::string&)> log;

  private:
    std::string directory_, worldId_;
    json::Value receipts_ = json::Value::array();
    double accumulator_ = 2;
    bool healthy_ = true;
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
