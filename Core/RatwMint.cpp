// The mint (Docs/Design/64-economy-migration.md, Phase 11; tools/econ_model/players.py's Mint): a weekly thermostat on
// the money in use. Players' and Holds' dormant money is what each holds beyond two months of its own spending (NPCs'
// purses aren't counted: their rules keep their money moving). The money in use is all the money less what's hoarded
// (dormant, a four weeks' average). The mint strikes into the capital's treasury only when that's MintBand under the
// money it first saw *and* there's harm: wages down MintWageFall over eight weeks, MintIdle of the grown idle, or
// MintHungry of the realm hungry. It melts only what it struck, evenly from every purse (a household purse under a
// week's food spared), when the money in use is MintBand over and wages aren't falling. Both are logged ("the mint
// strikes", "the mint melts"), and counted in `minted` and `sunk`. The Dungeon Master may turn it off (`mint.on`).
#include "RatwSociety.h"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace ratw
{
bool Society::hoarder(const std::string& account)
{
    return playerAccountId(account) || account.rfind("chapter:", 0) == 0;   // (A player's purse, a Hold's treasury.)
}

void Society::noteSpent(const std::string& from, std::int64_t coins)
{
    if (coins > 0 && hoarder(from))
        hoarderSpent_[from] += double(coins);
}

void Society::tendMint(std::int64_t day)
{
    auto& mint = state_.memory.mint;
    // Each hoarder's spending a day, a month's running average.
    for (const auto& [id, a] : state_.accounts)
        if (hoarder(id))
        {
            auto& avg = mint.spending[id];
            avg += ((hoarderSpent_.count(id) ? hoarderSpent_.at(id) : 0.) - avg) / MonthDays;
        }
    hoarderSpent_.clear();
    for (auto it = mint.spending.begin(); it != mint.spending.end();)
        it = state_.accounts.count(it->first) ? std::next(it) : mint.spending.erase(it);
    if (day % 7 != 0)
        return;
    const double money = double(state_.minted - state_.sunk);
    if (mint.target <= 0)
        mint.target = money;                         // (The money in use it first sees: the realm's as founded.)
    mint.wages.push_back(goingRate(""));
    if (mint.wages.size() > 8)
        mint.wages.erase(mint.wages.begin());
    double dormant = 0;
    for (const auto& [id, a] : state_.accounts)
        if (hoarder(id))
            dormant += std::max(0., double(a.cash) - 2 * MonthDays * (mint.spending.count(id) ? mint.spending.at(id) : 0.));
    mint.recent.push_back(dormant);
    if (mint.recent.size() > 4)
        mint.recent.erase(mint.recent.begin());
    const double hoarded = std::accumulate(mint.recent.begin(), mint.recent.end(), 0.) / double(mint.recent.size());
    const double trend = mint.wages.size() == 8 && mint.wages.front() > 0 ? mint.wages.back() / mint.wages.front() - 1 : 0;
    int hungry = 0;
    for (const auto& [id, life] : state_.residents)
        hungry += life.hunger >= 60;
    const bool harm = trend < -MintWageFall || slack_ > MintIdle ||
                      (!state_.residents.empty() && double(hungry) / double(state_.residents.size()) > MintHungry);
    const double gap = mint.target - (money - std::max(0., hoarded));
    if (!mint.on)
        return;
    if (mint.wages.size() == 8 && gap > MintBand * mint.target && harm)
    {
        // Too little in use, and it's doing harm: struck into the capital's treasury.
        const auto treasury = treasuryOf(capital());
        const auto coins = std::int64_t(std::floor(std::min(gap, MintStrikeMost * mint.target)));
        if (auto found = state_.accounts.find(treasury); found != state_.accounts.end() && coins > 0)
        {
            found->second.cash += coins;
            state_.minted += coins;
            mint.struck += double(coins);
            record("the mint strikes", "outside", treasury, "", 0, coins);
        }
    }
    else if (mint.struck > 0 && gap < -MintBand * mint.target && trend >= 0)
    {
        // Too much in use (the hoards spent again), and wages not falling: what it struck melted, the same share of
        // every purse, but not a wolf's with under a week's food money.
        const double amount = std::min({mint.struck, -gap, MintMeltMost * mint.target});
        const auto week = pennies(7 * FoodADay, "");
        double held = 0;
        for (const auto& [id, a] : state_.accounts)
            if (a.cash > 0 && !(state_.residents.count(id) && a.cash < week))
                held += double(a.cash);
        if (held <= 0)
            return;
        const double share = std::min(1., amount / held);
        std::int64_t melted = 0;
        for (auto& [id, a] : state_.accounts)
            if (a.cash > 0 && !(state_.residents.count(id) && a.cash < week))
                if (const auto take = std::int64_t(std::floor(double(a.cash) * share)); take > 0)
                {
                    a.cash -= take;
                    melted += take;
                    record("the mint melts", id, "outside", "", 0, take);
                }
        state_.sunk += melted;
        mint.struck = std::max(0., mint.struck - double(melted));
    }
}
} // namespace ratw
