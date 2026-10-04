// Wearing (Docs/Design/35-items-crafting-industry.md, Phase 4): the catalog's wearables in the game, their slots and fur
// spots, putting on and taking off, what others see, saving it, and the shops that sell them.
#include "RatwCheckpoint.h"
#include "RatwItems.h"
#include "RatwWire.h"
#include "RatwStep.h"
#include "RatwWorld.h"

#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace ratw;
namespace
{
int checks = 0;
void expect(bool condition, const std::string& message)
{
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}

void theCatalog()
{
    std::string error;
    expect(items::load(&error), "The catalog loads: " + error);
    const auto* scarf = items::wearable("wool_scarf");
    expect(scarf && scarf->slot == "throat" && items::slotsFor(*scarf) == std::vector<std::string>{"neck"}, "A scarf goes at the neck");
    const auto* satchel = items::wearable("satchel");
    expect(satchel && items::slotsFor(*satchel) == std::vector<std::string>{"chest_left", "chest_right"}, "a satchel on either side of the chest");
    const auto* clip = items::wearable("silver_clip");
    expect(clip && clip->spots.size() == std::size(items::FurSpots), "a fur clip anywhere in the fur");
    const auto* cuff = items::wearable("brass_ear_cuff");
    expect(cuff && items::spotAllowed(*cuff, "ears") && !items::spotAllowed(*cuff, "tail"), "an ear cuff only at the ears");
    const auto* bracelet = items::wearable("silver_bracelet");
    expect(bracelet && items::spotAllowed(*bracelet, "foreleg_left") && !items::spotAllowed(*bracelet, "ruff"), "a bracelet on a leg");
    expect(!items::wearable("sword") && !items::wearable("herbs") && !items::wearable("junk"), "The sword stays the mouth's; herbs aren't worn");
    expect(itemValid("wool_scarf") && itemValid("sword") && !itemValid("junk"), "Wearables are goods");
    expect(std::string(Society::itemName("wool_scarf")) == "Wool scarf", "with their catalog names");
}

void shops()
{
    const auto* tailor = items::businessFor("keeping the tailor shop");
    expect(tailor && tailor->id == "tailor", "A tailor's shop is known by its keeper's work");
    const auto sold = items::wearablesSold(*tailor);
    expect(std::find(sold.begin(), sold.end(), "wool_scarf") != sold.end() &&
               std::find(sold.begin(), sold.end(), "linen_wraps") != sold.end(),
           "and sells scarves and paw wraps");
    expect(items::businessFor("keeps The Armoury") && items::businessFor("keeps The Armoury")->id == "armory", "The Armoury is an armourer's");
    expect(items::businessFor("keeping Cuoio Fino") && items::businessFor("keeping Cuoio Fino")->id == "tannery", "Cuoio Fino a tanner's");
    expect(!items::businessFor("keeping the taproom"), "a taproom sells no wearables");
}

void wearing()
{
    World w;
    auto& ada = w.addPlayer("player-ada", "Ada");
    auto& soc = w.society();
    expect(!w.wear("player-ada", "wool_scarf", "").ok, "Nothing to wear without owning it");
    expect(soc.create("player-ada", "wool_scarf", 1, "test") && soc.create("player-ada", "satchel", 2, "test") &&
               soc.create("player-ada", "brass_ear_cuff", 1, "test") && soc.create("player-ada", "silver_clip", 2, "test") &&
               soc.create("player-ada", "felt_cap", 1, "test"),
           "A scarf, two satchels, an ear cuff, two fur clips and a cap");
    expect(w.wear("player-ada", "wool_scarf", "").ok && ada.worn["neck"] == "wool_scarf", "The scarf goes round the neck");
    expect(!w.wear("player-ada", "wool_scarf", "").ok, "and there is only the one");
    expect(!w.wear("player-ada", "felt_cap", "neck").ok, "A cap doesn't go at the neck");
    expect(w.wear("player-ada", "felt_cap", "").ok && ada.worn["head"] == "felt_cap", "but on the head");
    expect(w.wear("player-ada", "satchel", "").ok && w.wear("player-ada", "satchel", "").ok &&
               ada.worn["chest_left"] == "satchel" && ada.worn["chest_right"] == "satchel",
           "One satchel on each side of the chest");
    expect(!w.wear("player-ada", "brass_ear_cuff", "tail").ok, "An ear cuff won't clip to the tail");
    expect(w.wear("player-ada", "brass_ear_cuff", "ears").ok, "but goes on an ear");
    expect(w.wear("player-ada", "silver_clip", "ruff").ok && w.wear("player-ada", "silver_clip", "ruff").ok,
           "Two clips in the ruff: a spot takes any number");
    expect(!w.wear("player-ada", "silver_clip", "tail").ok, "and no third clip that isn't there");
    expect(World::wornCount(ada, "silver_clip") == 2 && World::wornCount(ada, "satchel") == 2, "Worn counts");
    const auto words = World::wornWords(ada);
    expect(words.find("felt") != std::string::npos && words.find("wool scarf") != std::string::npos &&
               words.find("at the ears") != std::string::npos && words.find("at the ruff") != std::string::npos,
           "Others see what she wears and where her jewellery is: " + words);

    // Saved and read back.
    const auto back = wire::readEntity(wire::persistEntity(ada, 0));
    expect(back.worn == ada.worn && back.jewellery == ada.jewellery, "What is worn is saved");

    // Off again.
    expect(w.takeOff("player-ada", "neck", "").ok && !ada.worn.count("neck"), "The scarf comes off");
    expect(w.takeOff("player-ada", "ruff", "silver_clip").ok && World::wornCount(ada, "silver_clip") == 1, "A clip is unclipped");
    expect(!w.takeOff("player-ada", "tail", "").ok, "Nothing at the tail to take off");

    // Sold (or otherwise gone) while worn: let go of.
    expect(soc.shift("player-ada", "treasury", "satchel", 1, 0, "test: a satchel gone"), "One satchel leaves the purse");
    w.fitWorn("player-ada");
    expect(World::wornCount(ada, "satchel") == 1, "and only one is worn now");
    // The sword in the jaws, sold: the mouth is empty after.
    soc.create("player-ada", "sword", 1, "test");
    expect(w.holdItem("player-ada", "sword").ok, "She holds a sword");
    expect(soc.shift("player-ada", "treasury", "sword", 1, 0, "test: the sword gone"), "and it leaves her purse");
    w.fitWorn("player-ada");
    expect(ada.mouth.empty(), "so she holds it no longer");
}

void manyKindsSave()
{
    World w;
    w.addPlayer("player-bo", "Bo");
    auto& soc = w.society();
    for (const char* item : {"herbs", "meal", "sword", "wool_scarf", "satchel", "silver_clip", "felt_cap"})
        expect(soc.create("player-bo", item, 1, "test"), std::string("Bo gets ") + item);
    const auto saved = w.save();
    {
        checkpoint::ServerState server;
        PersistedWorld back;
        std::string problem;
        expect(checkpoint::decode(checkpoint::encode(saved, server, {}, 0), back, server, problem), "The checkpoint reads back: " + problem);
    }
    World again;
    expect(again.restore(saved).ok, "A purse of seven kinds of goods saves and restores");
    expect(Society::stock(*again.society().account("player-bo"), "felt_cap") == 1, "with the cap in it");
}
} // namespace

// Carrying (doc 35, 1.2): what the purse weighs against 12 + STR/4, and what a heavy load or too much costs.
void carrying()
{
    World w;
    auto& ada = w.addPlayer("player-ada", "Ada");
    auto& bo = w.addPlayer("player-bo", "Bo");
    auto& soc = w.society();
    ada.strength = 50;
    const auto start = w.loadOf(ada);
    expect(start.comfortable == 24.5 && start.state == "comfortable" && start.pace == 10 && start.drain == 1,
           "STR 50 carries 24.5 lb in comfort, and a new wolf is well within it");
    expect(soc.create("player-ada", "iron_bar", 4, "test"), "four iron bars (20 lb)");
    const auto heavy = w.loadOf(ada);
    const double over = heavy.carried / heavy.comfortable;
    expect(heavy.carried == start.carried + 20 && heavy.state == (over > 1 ? "heavy" : "comfortable"), "the bars weigh");
    expect(soc.create("player-ada", "iron_bar", 2, "test"), "two more");
    const auto heavier = w.loadOf(ada);
    expect(heavier.state == "heavy" && heavier.pace < 10 && heavier.pace >= 6 && heavier.drain > 1 && heavier.drain <= 1.5,
           "heavy: the top pace falls, and running tires more (pace " + std::to_string(heavier.pace) + ")");
    ada.pace = 10;
    w.refreshLoad(ada);
    expect(effectivePace(ada) == heavier.pace, "so a sprint is held to it");
    expect(w.tooLoadedToFight("player-ada").empty(), "heavy is no bar to a fight");
    double light = 100, laden = 100, rate = 0;
    bool tired = false;
    step::updateStamina(light, tired, rate, 10, 1, 1);
    step::updateStamina(laden, tired, rate, 10, 1, 1, heavier.drain);
    expect(laden < light, "a second's sprint costs more stamina laden");
    expect(soc.create("player-ada", "mail_coat", 1, "test"), "and a mail coat");
    const auto over2 = w.loadOf(ada);
    expect(over2.state == "overloaded" && over2.pace == 0, "overloaded: a walk");
    w.refreshLoad(ada);
    expect(effectivePace(ada) == 0, "and only a walk");
    expect(!w.tooLoadedToFight("player-ada").empty(), "no fighting so laden");
    bo.position = {ada.position.x + 1, ada.position.y};
    bo.cellId = ada.cellId;
    const auto refused = w.challenge("player-ada", "player-bo", "yield");
    expect(!refused.ok && refused.message.find("carrying too much") != std::string::npos, "she can't challenge: " + refused.message);
    expect(w.challenge("player-bo", "player-ada", "yield").ok, "Bo may challenge her");
    const auto answer = w.answerChallenge("player-ada", true);
    expect(!answer.ok && answer.message.find("carrying too much") != std::string::npos, "but she can't accept, so laden");
    ada.strength = 100;
    expect(w.loadOf(ada).comfortable == 37, "a stronger wolf carries more in comfort");
    ada.npc = true;
    w.refreshLoad(ada);
    ada.loadPace = 10;
    expect(w.tooLoadedToFight("player-ada").empty(), "residents carry freely");
}

int main()
{
    try
    {
        theCatalog();
        shops();
        wearing();
        manyKindsSave();
        carrying();
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAILED after " << checks << " checks: " << e.what() << "\n";
        return 1;
    }
    std::cout << "Wear tests passed: " << checks << " checks.\n";
    return 0;
}
