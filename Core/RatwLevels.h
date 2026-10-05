#pragma once
// Levels (Docs/Design/44-levelling.md): how much XP a level takes, and what a level does in a fight. Placeholders for
// play-testing.
#include <algorithm>

namespace ratw::levels
{
// Going from level L to L + 1 costs 100 + 50 × (L − 1) XP: 100, 150, 200... (level 25 at 16,200 in all).
inline int stepCost(int level) { return 100 + 50 * (std::max(1, level) - 1); }
// The XP a level starts at (level 1 at 0).
inline long long xpFor(int level)
{
    long long total = 0;
    for (int l = 1; l < level; ++l)
        total += stepCost(l);
    return total;
}
// The level `xp` has reached. No cap.
inline int levelFor(long long xp)
{
    int level = 1;
    while (xp >= stepCost(level))
        xp -= stepCost(level++);
    return level;
}
// What a level does in a fight, and nothing else: a player's fighting skill, 50 at level 1, +0.5 a level to 62 at 25.
constexpr int FightingLevelCap = 25;
inline double fightingSkill(int level) { return 50 + .5 * (std::clamp(level, 1, FightingLevelCap) - 1); }
}
