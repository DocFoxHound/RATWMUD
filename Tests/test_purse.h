// A test's own purse (doc 79, 9.4). The coins a test puts in a player's purse to set up a scene come from no path of
// play, so they are no registered faucet, and since D16 an unregistered faucet fails realBalanced(). The test's coins
// are taken into the gate's opening instead: the gate then measures play from there. A gate already broken before the
// purse stays broken (nothing is forgiven), and a purse that isn't paid changes nothing.
#pragma once

#include "RatwSociety.h"

#include <cstdint>
#include <string>

inline bool testPurse(ratw::Society& society, const std::string& to, std::int64_t coins,
                      const std::string& reason = "test purse")
{
    const bool balanced = society.realBalanced();
    if (!society.shift("treasury", to, "", 0, coins, reason))
        return false;
    if (balanced)
        society.openRealBalance();
    return true;
}
