// Social standing (Docs/Design/49-characters-and-earned-gifts.md, Phase 3; it replaced doc 44's levels): the social
// level curve and titles from Data/Progression/standing.json; social XP is scenes, stars and Stories alone (work,
// practice, places, contracts and rested time teach skills instead); the day's cap; and the ledger's per-actor index.
// (A level's old fighting skill, levels::fightingSkill, stays only for level_sim's "@20" rows.)
#include "RatwLevels.h"
#include "RatwPractice.h"
#include "RatwSocialCore.h"
#include "RatwStanding.h"

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
    expect(practice::levelFor(0) == 1 && practice::levelFor(99) == 1 && practice::levelFor(100) == 2, "100 social XP for level 2");
    expect(practice::xpFor(3) == 250 && practice::xpFor(5) == 700 && practice::xpFor(8) == 1750 && practice::xpFor(10) == 2700,
           "250 for 3, 700 for 5, 1,750 for 8, 2,700 for 10 (doc 44's curve, now in standing.json)");
    for (int l = 1; l <= 40; ++l)
        expect(practice::levelFor(practice::xpFor(l)) == l && practice::levelFor(practice::xpFor(l) - 1) == std::max(1, l - 1),
               "each level's start: " + std::to_string(l));
    expect(practice::titleFor(1) == "Stranger" && practice::titleFor(3) == "Known" && practice::titleFor(7) == "Familiar Face" &&
               practice::titleFor(25) == "Legend" && socialTitle(12) == "Notable",
           "titles from standing.json");
    expect(levels::fightingSkill(1) == 50 && levels::fightingSkill(25) == 86, "a level's old fighting skill, for level_sim's \"@20\" alone");
}

void socialAlone()
{
    for (const char* r : {"qualified_session_settlement", "gold_star", "story_star", "story_closure"})
        expect(practice::socialReason(r), std::string("social XP: ") + r);
    for (const char* r : {"work", "practice", "milestone", "discovery", "story", "rested_bonus"})
        expect(!practice::socialReason(r), std::string("not social XP any more: ") + r);
    // An old ledger: what counts toward the day (and, on loading, toward the level) is the social receipts alone.
    SocialLedger L;
    const double now = 1000 * 86400.0;
    for (const auto& [reason, amount] : std::initializer_list<std::pair<const char*, int>>{
             {"qualified_session_settlement", 20}, {"gold_star", 2}, {"discovery", 5}, {"story", 25}, {"rested_bonus", 20}})
    {
        LedgerEntry e;
        e.actor = "ada";
        e.reason = reason;
        e.amount = amount;
        e.at = now;
        L.entries.push_back(e);
    }
    L.reindex();
    expect(L.receiptsOf("ada").size() == 5 && L.receiptsOf("bo").empty(), "the index finds each actor's receipts");
    expect(L.usedToday("ada", now + 10) == 22, "today's social XP: the scene and the star, nothing else");
    expect(SocialLedger::dailyCap() == 150, "the social day's cap, from standing.json");
}
void earnedTiers()
{
    // Doc 49, Phase 5: the thresholds from standing.json, and what a locked tier says it still needs.
    const auto& t = standing::thresholds();
    expect(t.giftedLevel == 3 && t.giftedScenes == 10 && t.quickenedLevel == 8 && t.quickenedStars == 100 && t.quickenedGivers == 30 &&
               t.quickenedStories == 2,
           "the user's measures");
    standing::Measures m;
    m.socialLevel = 3;
    m.normalScenes = 9;
    expect(!standing::meetsGifted(m, t), "nine scenes isn't ten");
    m.normalScenes = 10;
    expect(standing::meetsGifted(m, t), "ten scenes and level 3: Gifted (no count of different wolves)");
    m.socialLevel = 8;
    m.stars = 62;
    m.starGivers = 30;
    m.closedStories = 1;
    expect(!standing::meetsQuickened(m, t), "62 stars, one Story: not Quickened");
    expect(standing::lockedMessage("quickened", m, t) == "Quickened isn't open to your account yet: Stars: 62 of 100; Stories closed: 1 of 2.",
           standing::lockedMessage("quickened", m, t));
    m.stars = 100;
    m.closedStories = 2;
    expect(standing::meetsQuickened(m, t), "100 stars from 30, two Stories, level 8: Quickened");
    m.upheldReports = 1;
    expect(!standing::meetsQuickened(m, t), "but not with a report upheld");
    standing::Record r;
    expect(standing::open(r, "normal") && !standing::open(r, "gifted"), "Normal is always open");
}
} // namespace

int main()
{
    try
    {
        curve();
        socialAlone();
        earnedTiers();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Level tests passed: " << checks << " checks.\n";
    return 0;
}
