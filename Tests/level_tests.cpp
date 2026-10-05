// Levelling (Docs/Design/44-levelling.md): the curve, what a level does in a fight, XP from work, practice, places,
// skills and contracts (typed, once each, within the day's limits), and rested XP.
#include "RatwLevels.h"
#include "RatwSocialCore.h"

#include <iostream>
#include <stdexcept>
#include <string>

using namespace ratw;
namespace
{
int checks = 0;
void expect(bool ok, const std::string& what)
{
    ++checks;
    if (!ok)
        throw std::runtime_error(what);
}

void curve()
{
    expect(levels::levelFor(0) == 1 && levels::levelFor(99) == 1 && levels::levelFor(100) == 2, "100 XP for level 2");
    expect(levels::xpFor(3) == 250 && levels::xpFor(5) == 700 && levels::xpFor(10) == 2700, "250 for 3, 700 for 5, 2,700 for 10");
    expect(levels::xpFor(15) == 5950 && levels::xpFor(20) == 10450 && levels::xpFor(25) == 16200, "5,950, 10,450, 16,200 for 25");
    for (int l = 1; l <= 40; ++l)
        expect(levels::levelFor(levels::xpFor(l)) == l && levels::levelFor(levels::xpFor(l) - 1) == std::max(1, l - 1), "each level's start: " + std::to_string(l));
    expect(levels::levelFor(levels::xpFor(30)) == 30, "no cap");
    expect(levels::fightingSkill(1) == 50 && levels::fightingSkill(25) == 62 && levels::fightingSkill(40) == 62, "fighting skill 50 to 62, no more past 25");
    expect(socialTitle(18) == "Renowned" && socialTitle(25) == "Legend" && socialTitle(12) == "Notable", "Renowned and Legend");
}

void awards()
{
    SocialLedger L;
    const double day = 86400;
    double now = 1000 * day;
    expect(L.award("ada", "discovery", "visit:town", now) == 5, "a place first visited: 5");
    expect(L.award("ada", "discovery", "visit:town", now + 10) == 0, "and only once");
    expect(L.award("ada", "story", "contract:1", now) == 25, "a contract: 25");
    expect(L.award("ada", "chores", "x", now) == 0, "no such kind");
    // Practice: 5 each, 15 a day; the fourth waits for tomorrow, and is paid then (no receipt was written).
    for (const char* p : {"forage:1", "hunt:1", "repair:1"})
        expect(L.award("ada", "practice", p, now) == 5, std::string("practice: ") + p);
    expect(L.award("ada", "practice", "forage:2", now) == -1, "15 a day of practice");
    expect(L.award("ada", "practice", "forage:2", now + day + 1) == 10, "paid the next day (5, and 5 rested: a day without earning)");
    // Work: 10 each, 30 a day.
    for (int i = 0; i < 3; ++i)
        expect(L.award("bo", "work", "lend:" + std::to_string(i), now) == 10, "work");
    expect(L.award("bo", "work", "lend:9", now) == -1, "30 a day of work");
    // The daily cap: 150 across every kind.
    SocialLedger C;
    for (int i = 0; i < 6; ++i)
        C.award("cy", "story", "contract:" + std::to_string(i), now);
    expect(C.points["cy"] == 150, "150 a day at most: " + std::to_string(C.points["cy"]));
    expect(C.award("cy", "story", "contract:9", now) == -1, "the next waits for tomorrow");
    expect(C.level("cy") == 2, "150 XP is level 2");
}

void rested()
{
    // Back after three days away: 150 rested, paid again on top of what is earned, outside the cap, until used up.
    SocialLedger L;
    const double day = 86400;
    double now = 2000 * day;
    expect(L.award("dee", "discovery", "visit:a", now) == 5, "first XP: nothing rested yet");
    expect(L.restedLeft("dee", now + 3 * day + 5) == 150, "three days away: 150 rested");
    now += 3 * day + 5;
    expect(L.award("dee", "story", "contract:1", now) == 50, "a contract pays 25, and 25 again rested");
    expect(L.restedLeft("dee", now + 1) == 125, "125 rested left");
    for (int i = 2; i <= 6; ++i)
        L.award("dee", "story", "contract:" + std::to_string(i), now + i);
    expect(L.restedLeft("dee", now + 10) == 0, "used up");
    expect(L.usedToday("dee", now + 10) == 150, "the rested pay outside the day's cap");
    expect(L.points["dee"] == 5 + 150 + 150, "150 earned and 150 rested: " + std::to_string(L.points["dee"]));
    // Never more than 300, however long away.
    expect(L.restedLeft("dee", now + 40 * day) == 300, "300 at most");
}
} // namespace

int main()
{
    try
    {
        curve();
        awards();
        rested();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Level tests passed: " << checks << " checks.\n";
    return 0;
}
