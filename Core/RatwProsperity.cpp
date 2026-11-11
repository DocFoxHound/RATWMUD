// The staged economy's DM side (Docs/Design/79-a-staged-economy.md, 12; Phase 9): the per-world market layer and the
// host's overrides, grants and fines on players, "reset prices", the DM's harvest override lifted, and the Prosperity
// report the DM's tab reads (written with the checkpoint: SocietyState::report).
#include "RatwSociety.h"
#include "RatwWorld.h"

namespace ratw
{
bool Society::grantPlayer(const std::string&, std::int64_t, const std::string&, std::string& problem)
{
    problem = "not built";
    return false;
}

bool Society::resetPrices(const std::string&, double, const std::string&) { return false; }

void Society::adoptOverrides(const EconomySpec::Overrides&, std::vector<std::string>*) {}

const market::Data& Society::market() const { return marketLayer_ ? *marketLayer_ : market::data(); }

bool World::clearHarvest(const std::string&) { return false; }

json::Value World::prosperityReport() { return json::Value::object(); }
} // namespace ratw
