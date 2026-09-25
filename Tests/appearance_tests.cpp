#include "RatwAppearance.h"
#include "RatwWorld.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
bool same(const ratw::Appearance& a, const ratw::Appearance& b)
{
    return a.species == b.species && a.sex == b.sex && a.stature == b.stature && a.pattern == b.pattern &&
           a.baseColor == b.baseColor && a.gradientColor == b.gradientColor && a.markingColor == b.markingColor &&
           a.gradientAmount == b.gradientAmount && a.patternAmount == b.patternAmount;
}
bool near(double a, double b) { return std::abs(a - b) < 1e-9; }
const ratw::Entity* actor(const ratw::Snapshot& snapshot, const std::string& id)
{
    const auto found = std::find_if(snapshot.entities.begin(), snapshot.entities.end(),
                                   [&](const ratw::Entity& e) { return e.id == id; });
    return found == snapshot.entities.end() ? nullptr : &*found;
}
} // namespace

int main()
{
    int checks = 0;
    auto check = [&](bool condition, const char* message) {
        ++checks;
        if (!condition) throw std::runtime_error(message);
    };
    try
    {
        using namespace ratw;
        const Appearance defaults;
        check(validAppearance(defaults) && defaults.species == "timber" && defaults.sex == "male" &&
                  defaults.stature == "average" && defaults.pattern == "saddle" && defaults.baseColor == 3 &&
                  defaults.gradientColor == 1 && defaults.markingColor == 5 && defaults.gradientAmount == .35 &&
                  defaults.patternAmount == .55, "Canonical legacy/default appearance is stable");
        const char* paletteNames[] = {"ivory", "silver", "ash", "stone", "sable", "charcoal", "rust", "sand"};
        const char* paletteHex[] = {"#E1D9C6", "#ADB3B2", "#777D7B", "#8E8271", "#65513F", "#303534", "#A26843", "#BEAA84"};
        for (int index = 0; index < CoatColorCount; ++index)
        {
            check(std::string(coatColorName(index)) == paletteNames[index] &&
                      std::string(coatColorHex(index)) == paletteHex[index], "Natural palette indices are stable");
            auto a = defaults;
            a.baseColor = a.gradientColor = a.markingColor = index;
            check(validAppearance(a), "Every palette endpoint is usable for all coat layers");
        }
        for (int index : {-1, 8, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()})
            check(std::string(coatColorName(index)).empty() && std::string(coatColorHex(index)).empty(),
                  "Out-of-range palette lookups are safe and never clamp to a different color");
        for (const auto* species : {"timber", "maned", "arctic", "red", "ethiopian"})
            for (const auto* sex : {"female", "male"})
                for (const auto* stature : {"short", "average", "tall"})
                    for (const auto* pattern : {"solid", "saddle", "mantle", "piebald"})
                    {
                        auto a = defaults;
                        a.species = species; a.sex = sex; a.stature = stature; a.pattern = pattern;
                        check(validAppearance(a), "Every specified species/sex/stature/pattern combination is valid");
                        const double height = shoulderHeightCm(a, 18);
                        check(std::isfinite(height) && height > 0 && height <= 100.8 + 1e-9,
                              "Every valid combination has finite bounded descriptive height");
                    }

        std::vector<Appearance> invalid;
        for (const auto* bad : {"", "Timber", "wolf", "timber ", "../timber", "<script>"})
        {
            auto a = defaults; a.species = bad; invalid.push_back(a);
        }
        for (const auto* bad : {"", "Male", "unknown", "female "})
        {
            auto a = defaults; a.sex = bad; invalid.push_back(a);
        }
        for (const auto* bad : {"", "giant", "Tall", " average"})
        {
            auto a = defaults; a.stature = bad; invalid.push_back(a);
        }
        for (const auto* bad : {"", "striped", "Saddle", "solid "})
        {
            auto a = defaults; a.pattern = bad; invalid.push_back(a);
        }
        for (int bad : {-1, 8, std::numeric_limits<int>::min(), std::numeric_limits<int>::max()})
        {
            auto a = defaults; a.baseColor = bad; invalid.push_back(a);
            a = defaults; a.gradientColor = bad; invalid.push_back(a);
            a = defaults; a.markingColor = bad; invalid.push_back(a);
        }
        for (double bad : {-.001, 1.001, std::numeric_limits<double>::quiet_NaN(),
                           std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity()})
        {
            auto a = defaults; a.gradientAmount = bad; invalid.push_back(a);
            a = defaults; a.patternAmount = bad; invalid.push_back(a);
        }
        auto embeddedNull = defaults;
        embeddedNull.species = std::string("timber\0hidden", 13);
        invalid.push_back(embeddedNull);
        for (const auto& a : invalid)
            check(!validAppearance(a) && shoulderHeightCm(a, 18) == 0,
                  "Malformed appearance is rejected without deriving plausible public height");
        for (double amount : {0., .25, .5, 1.})
        {
            auto a = defaults; a.gradientAmount = a.patternAmount = amount;
            check(validAppearance(a), "Both blend amounts accept inclusive normalized range");
        }

        for (int age : {0, 6, 12}) check(lifeStage(age) == LifeStage::Young, "Young age boundary");
        for (int age : {13, 17}) check(lifeStage(age) == LifeStage::Adolescent, "Adolescent age boundary");
        for (int age : {18, 64}) check(lifeStage(age) == LifeStage::Adult, "Adult age boundary");
        for (int age : {65, 99, 10000}) check(lifeStage(age) == LifeStage::Old, "Old age boundary supports long saves");
        check(std::string(lifeStageName(LifeStage::Young)) == "young" &&
                  std::string(lifeStageName(LifeStage::Adolescent)) == "adolescent" &&
                  std::string(lifeStageName(LifeStage::Adult)) == "adult" &&
                  std::string(lifeStageName(LifeStage::Old)) == "old", "Stable public stage identifiers");
        check(std::string(lifeStageName(static_cast<LifeStage>(999))).empty(), "Unknown stage enum is safe");
        auto small = defaults; small.stature = "short";
        auto tall = defaults; tall.stature = "tall";
        check(near(shoulderHeightCm(small, 18), 76 * .88) && near(shoulderHeightCm(tall, 18), 76 * 1.12),
              "Descriptive stature scales are deterministic");
        check(near(shoulderHeightCm(defaults, 6), 76 * .65) && near(shoulderHeightCm(defaults, 13), 76 * .87) &&
                  near(shoulderHeightCm(defaults, 18), 76) && near(shoulderHeightCm(defaults, 65), 76 * .96),
              "Portrait stage scales follow authoritative age");
        auto female = defaults; female.sex = "female";
        check(shoulderHeightCm(female, 18) == shoulderHeightCm(defaults, 18),
              "Sex choice does not apply an automatic height/stat adjustment");

        World world;
        auto& p = world.addPlayer("player-appearance", "Juniper");
        auto& q = world.addPlayer("player-observer", "Observer");
        check(same(p.appearance, defaults) && p.age == 18, "New core actor uses safe appearance and age defaults");
        auto custom = defaults;
        custom.species = "ethiopian"; custom.sex = "female"; custom.stature = "tall"; custom.pattern = "piebald";
        custom.baseColor = 6; custom.gradientColor = 7; custom.markingColor = 0;
        custom.gradientAmount = .8; custom.patternAmount = .2;
        p.position = {17.5, 12.5}; q.position = {16.5, 12.5};
        const auto original = p;
        const double speed = paceSpeed(p), vision = world.visionClarity(q.id, p.id), hearing = world.hearingClarity(q.id, p.id);
        p.appearance = custom;
        check(p.strength == original.strength && p.dexterity == original.dexterity && p.wisdom == original.wisdom &&
                  p.hearing == original.hearing && p.vision == original.vision && p.smell == original.smell &&
                  p.stamina == original.stamina && p.sneakSkill == original.sneakSkill && p.pace == original.pace &&
                  paceSpeed(p) == speed && world.visionClarity(q.id, p.id) == vision &&
                  world.hearingClarity(q.id, p.id) == hearing,
              "Appearance is descriptive only and cannot change senses, speed, skills, attributes or stamina");
        const auto self = world.snapshot(p.id);
        check(same(self.self.appearance, custom), "Self snapshot carries authoritative appearance");
        const auto visible = world.snapshot(q.id);
        check(actor(visible, p.id) && same(actor(visible, p.id)->appearance, custom),
              "A sight-authorized visible actor carries its inspectable appearance");
        q.position = {22.5, 6.5}; p.position = {26.5, 6.5};
        check(world.visionClarity(q.id, p.id) == 0 && world.hearingClarity(q.id, p.id, Voice::Yell) > 0,
              "Opaque barrier fixture allows anonymous hearing but no visual identity");
        check(!actor(world.snapshot(q.id), p.id) && !world.interact(q.id, p.id, "inspect").ok,
              "Hearing cannot disclose an unseen actor's appearance or unlock forged inspection");
        check(!world.perceive(q.id, p.id, Voice::Yell).identifiable,
              "Anonymous voice remains anonymous regardless of distinctive coat");
        p.cellId = "exterior"; p.position = {16.5, 1.5};
        check(!actor(world.snapshot(q.id), p.id), "Other-cell actors never enter local appearance snapshots");
        check(world.snapshot("missing-observer").entities.empty(), "Unknown observer has no appearance records");

        // Save two distinct player coats and one NPC coat; live mutations after
        // capture must not mutate the checkpoint or couple different actors.
        auto npcCoat = defaults; npcCoat.species = "arctic"; npcCoat.baseColor = 0; npcCoat.pattern = "mantle";
        world.entity("npc_keeper")->appearance = npcCoat;
        p.age = 64;
        const auto checkpoint = world.save();
        p.appearance = defaults;
        World restored;
        check(restored.restore(checkpoint).ok, "Appearance-bearing world checkpoint restores");
        check(same(restored.entity(p.id)->appearance, custom) && same(restored.entity(q.id)->appearance, defaults) &&
                  same(restored.entity("npc_keeper")->appearance, npcCoat),
              "Players and NPCs retain independent complete appearance choices across restart");
        const auto before = restored.save();
        const auto beforeTreasury = restored.society().account("treasury")->cash;
        for (const auto& bad : invalid)
        {
            auto damaged = checkpoint;
            damaged.players.front().appearance = bad;
            damaged.calendarDays += 1;
            check(!restored.restore(damaged).ok && restored.calendarDays() == before.calendarDays &&
                      same(restored.entity(p.id)->appearance, custom) &&
                      restored.society().account("treasury")->cash == beforeTreasury,
                  "Malformed player appearance rejects complete restore atomically");
            damaged = checkpoint;
            damaged.npcs.front().appearance = bad;
            check(!restored.restore(damaged).ok && same(restored.entity("npc_keeper")->appearance, npcCoat),
                  "Malformed NPC appearance rejects complete restore atomically");
        }
        check(restored.advanceCalendar(365).ok && restored.entity(p.id)->age == 65 &&
                  lifeStage(restored.entity(p.id)->age) == LifeStage::Old &&
                  same(restored.entity(p.id)->appearance, custom),
              "Birthday advances portrait stage without overwriting coat or descriptive choices");
        World afterBirthday;
        check(afterBirthday.restore(restored.save()).ok && afterBirthday.entity(p.id)->age == 65 &&
                  same(afterBirthday.entity(p.id)->appearance, custom) &&
                  shoulderHeightCm(afterBirthday.entity(p.id)->appearance, afterBirthday.entity(p.id)->age) ==
                      shoulderHeightCm(custom, 65), "Post-birthday restart derives identical descriptive portrait height");
        // Account-created characters use opaque wolf- IDs, not the old player-
        // development alias. Every economy entry point must recognize them.
        World owned;
        const std::string ownedId = "wolf-0123456789abcdef0123456789abcdef";
        const auto reserveBefore = owned.society().account("treasury")->cash;
        auto& ownedWolf = owned.addPlayer(ownedId, "Owned wolf");
        ownedWolf.appearance = custom;
        check(owned.society().account(ownedId) && owned.society().account(ownedId)->cash == 20 &&
                  owned.society().account("treasury")->cash == reserveBefore - 20 && owned.society().conserved(),
              "Authenticated wolf IDs receive exactly one finite welcome purse");
        const auto* purse = owned.society().account(ownedId);
        check(Society::stock(*purse, "herbs") == 2 && Society::stock(*purse, "meal") == 1,
              "Authenticated wolf IDs receive real starter goods from the reserve");
        owned.addPlayer(ownedId, "Repeated entry");
        check(owned.society().account("treasury")->cash == reserveBefore - 20,
              "Re-entering same authenticated wolf does not repeat welcome grant");
        check(owned.society().quote(ownedId, "npc_keeper", "meal", 1, true).ok &&
                  owned.society().trade(ownedId, "npc_keeper", "meal", 1, true).ok && owned.society().conserved(),
              "Authenticated wolf can quote and complete ordinary conserved trade");
        check(owned.society().gather(ownedId).ok && owned.society().eat(ownedId).ok,
              "Authenticated wolf IDs participate in gathering and eating");
        const auto ownedSave = owned.save();
        World ownedReload;
        check(ownedReload.restore(ownedSave).ok && ownedReload.society().account(ownedId) &&
                  ownedReload.society().account(ownedId)->cash == owned.society().account(ownedId)->cash &&
                  ownedReload.society().account(ownedId)->stock == owned.society().account(ownedId)->stock &&
                  same(ownedReload.entity(ownedId)->appearance, custom),
              "Owned wolf purse, traded goods, and portrait survive combined restart");
        auto missingPurse = ownedSave;
        missingPurse.society.accounts.at("treasury").cash += missingPurse.society.accounts.at(ownedId).cash;
        missingPurse.society.accounts.erase(ownedId);
        check(!ownedReload.restore(missingPurse).ok &&
                  ownedReload.society().account(ownedId)->cash == ownedSave.society.accounts.at(ownedId).cash &&
                  ownedReload.society().account("treasury")->cash == ownedSave.society.accounts.at("treasury").cash,
              "Missing owned-wolf purse rejects balanced checkpoint atomically, never recreating starter funds");
        for (const std::string badId : {"wolf-", "wolf-ash", "wolf-0123456789abcdef0123456789abcde", "wolf-0123456789abcdef0123456789abcdef0",
                                       "wolf-0123456789ABCDEF0123456789abcdef", "wolf-0123456789abcdef0123456789abcdeg", "wolf-../../outside"})
        {
            const auto beforeRejected = ownedReload.society().account("treasury")->cash;
            ownedReload.society().addPlayer(badId);
            check(!ownedReload.society().account(badId) && ownedReload.society().account("treasury")->cash == beforeRejected,
                  "Malformed owned-wolf ID cannot open an economy account or debit reserve");
            auto forged = ownedSave;
            forged.society.accounts.emplace(badId, EconomyAccount{});
            check(!ownedReload.restore(forged).ok && ownedReload.society().conserved(),
                  "Malformed wolf account ID is rejected by checkpoint allowlist");
        }
        std::cout << checks << " appearance checks passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "Appearance check " << checks << " failed: " << error.what() << '\n';
        return 1;
    }
}
