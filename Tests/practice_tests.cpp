// Practice (Docs/Design/49-characters-and-earned-gifts.md, Phase 1): the catalog in Data/Progression/skills.json; how a
// skill grows (room, the daily soft limit, rested practice, a better wolf near, the partner, variety, the same partner
// again); growth lines; and what a save keeps.
#include "RatwBattle.h"
#include "RatwCreation.h"
#include "RatwPractice.h"
#include "RatwWire.h"
#include "RatwWorld.h"

#include <cmath>
#include <iostream>
#include <map>
#include <sstream>
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
bool near(double a, double b, double within = 1e-6)
{
    return std::abs(a - b) <= within;
}

double clockNow = 1e9;          // The game's real clock, as World::realClock sees it.

World field()
{
    std::ostringstream cell;
    cell << "id: field\nname: The Field\ndescription: Open grass.\nworld: 0 0 0\nsize: 40 30\noutdoors: true\n"
            "weather: clear\nwind: 0 0.5 1\nlighting: 1 1 warm\ngrid:\n";
    for (int y = 0; y < 30; ++y)
        cell << std::string(40, ',') << '\n';
    std::map<std::string, std::string> files;
    files["cells/field.cell"] = cell.str();
    files["world.ratw"] = "RATW_WORLD 2\ncell \"field\" \"cells/field.cell\"\nterritory \"field\" \"field\" \"-\" 0\n"
                          "spawn \"field\" 20.5 15.5\neconomy 1000 100 50 10 12\n";
    World w;
    const auto loaded = w.loadWorldFiles(files, "field");
    expect(loaded.ok, "the field loads: " + loaded.message);
    w.realClock = [] { return clockNow; };
    return w;
}

Entity& wolf(World& w, const std::string& id, double x = 20.5, double y = 15.5)
{
    auto& e = w.addPlayer(id, id);
    e.cellId = "field";
    e.position = {x, y};
    return e;
}

std::vector<std::string> told(World& w, const std::string& who)
{
    std::vector<std::string> out;
    for (const auto& [to, words] : w.takeNotices())
        if (to == who)
            out.push_back(words);
    return out;
}

void theCatalog()
{
    std::string error;
    expect(practice::load(&error), "the catalog loads: " + error);
    for (const char* id : {"strength", "dexterity", "wisdom", "stamina", "hearing", "vision", "smell"})
        expect(practice::skill(id) && practice::skill(id)->attribute, std::string("an attribute: ") + id);
    for (const char* id : {"fighting", "sneak", "listening", "tracking", "craft", "labour", "commerce", "service", "travel", "watch"})
        expect(practice::skill(id) && !practice::skill(id)->attribute, std::string("a skill: ") + id);
    expect(practice::skill("tracking")->field == "scentSkill" && practice::skill("craft")->field.empty(), "tracking is the scent skill; trades are their own");
    expect(practice::skill("stamina")->field == "endurance", "stamina the attribute lives in `endurance`, not the bar");
    expect(practice::capFor(*practice::skill("fighting"), false) == 86 && practice::capFor(*practice::skill("fighting"), true) == 100,
           "fighting to 86, a Quickened wolf's to 100");
    const auto* nose = practice::source("nose.use");
    expect(nose && nose->grows.size() == 2, "using the nose grows the nose and tracking");
    for (const char* id : {"sneak.world", "sneak.arena", "sneak.ambush", "notice.sound", "notice.scent", "track.found"})
        expect(practice::source(id) != nullptr, std::string("a source: ") + id);
}

void roomAndTheSoftLimit()
{
    clockNow = 1e9;
    World w = field();
    auto& ada = wolf(w, "ada");
    const auto& tracking = *practice::skill("tracking");
    // Each try far enough apart that variety never counts against it.
    const auto tryOnce = [&] {
        clockNow += 700;
        const double before = ada.scentSkill;
        w.practise("ada", "track.found");
        return ada.scentSkill - before;
    };
    expect(near(tryOnce(), .3), "a trail found teaches 0.3 at the start (all room left)");
    double gainedToday = .3;
    while (gainedToday < tracking.softPerDay)
        gainedToday += tryOnce();
    const double eased = tryOnce();
    expect(near(eased, .3 * (100 - ada.scentSkill + eased) / 100 * .2, 1e-4), "past the day's soft limit, a fifth as much");
    // Near the cap it slows, but never below a tenth; at the cap it stops, and says so once.
    ada.scentSkill = 95;
    expect(near(tryOnce(), .3 * .1 * .2, 1e-9), "at 95 of 100 the room is its floor, a tenth");
    ada.scentSkill = 99.999;
    told(w, "ada");
    tryOnce();
    expect(ada.scentSkill == 100, "it reaches the cap");
    auto lines = told(w, "ada");
    expect(lines.size() == 1 && lines[0] == "Your tracking is as good as it will get.", "and says so: " + (lines.empty() ? "" : lines[0]));
    expect(tryOnce() == 0 && told(w, "ada").empty(), "and nothing more at the cap");
}

void restedPractice()
{
    clockNow = 2e9;
    World w = field();
    auto& ada = wolf(w, "ada");
    w.practise("ada", "track.found");
    expect(ada.practice->lastGainAt == clockNow && ada.practice->rested == 0, "a first gain: nothing rested yet");
    clockNow += 2 * 86400;                          // Two days away.
    const double before = ada.scentSkill;
    w.practise("ada", "track.found");
    const double gain = .3 * (100 - before) / 100;
    expect(near(ada.scentSkill - before, 2 * gain, 1e-9), "after two days away a gain is doubled");
    expect(near(ada.practice->rested, 10 - gain, 1e-9), "from a pool of 5 a day away, spent by what it gave");
    expect(near(ada.practice->days["tracking"].gained, gain, 1e-9), "and what it gave is outside the soft limit");
    clockNow += 50 * 86400;
    ada.practice->rested = 0;
    w.practise("ada", "track.found");
    expect(ada.practice->rested <= 30, "the pool holds 30 at most");
}

void aBetterWolfNear()
{
    clockNow = 3e9;
    World w = field();
    auto& ada = wolf(w, "ada");
    auto& bo = wolf(w, "bo", 23.5, 15.5);           // Three tiles off.
    bo.scentSkill = 50;
    w.practise("ada", "track.found");
    expect(near(ada.scentSkill, .3 * 1.5), "a better player within 6 tiles: half again as fast");
    // One of her own account's wolves teaches nothing.
    clockNow = 3.1e9;
    World w2 = field();
    auto& ada2 = wolf(w2, "ada");
    auto& bo2 = wolf(w2, "bo", 23.5, 15.5);
    bo2.scentSkill = 50;
    w2.accountOf = [](const std::string&) { return std::string("same-account"); };
    w2.practise("ada", "track.found");
    expect(near(ada2.scentSkill, .3), "her own account's other wolf doesn't count");
    // Too far, or not better enough.
    clockNow = 3.2e9;
    World w3 = field();
    auto& ada3 = wolf(w3, "ada");
    wolf(w3, "bo", 30.5, 15.5).scentSkill = 50;     // Ten tiles off.
    wolf(w3, "cy", 21.5, 15.5).scentSkill = 9;      // Close, but only 9 better.
    w3.practise("ada", "track.found");
    expect(near(ada3.scentSkill, .3), "too far, or not 10 better: no faster");
}

void partnersAndVariety()
{
    const auto& r = practice::rules();
    expect(practice::partnerFactor("player", -1, 50, r) == 1 && practice::partnerFactor("resident", -1, 50, r) == .8 &&
               practice::partnerFactor("fierce", -1, 50, r) == .6 && practice::partnerFactor("post", -1, 50, r) == .25,
           "a player teaches most, a resident less, a fierce animal less, a post least");
    expect(practice::partnerFactor("player", 35, 50, r) == .5 && practice::partnerFactor("player", 60, 50, r) == 1.5 &&
               practice::partnerFactor("player", 55, 50, r) == 1,
           "a foe 15 weaker teaches half; 10 better, half again");
    // Variety: a fourth occasion of the same key in ten minutes counts a quarter; one occasion is one, however long.
    PracticeState p;
    double now = 0;
    for (int i = 0; i < 3; ++i, now += 61)
        expect(practice::variety(p, "k", "", now, r) == 1, "the first three occasions count fully");
    expect(practice::variety(p, "k", "", now, r) == .25, "a fourth in ten minutes counts a quarter");
    PracticeState q;
    for (int i = 0; i < 40; ++i)
        expect(practice::variety(q, "k", "", i * 10.0, r) == 1, "practice every ten seconds is one occasion");
    expect(practice::variety(q, "other", "", 400, r) == 1, "another key is its own");
    PracticeState f;
    for (int i = 0; i < 6; ++i)
        expect(practice::variety(f, "k", "fight-1", i * 300.0, r) == 1, "one fight is one occasion, however long");
    now += 700;
    expect(practice::variety(p, "k", "", now, r) == 1, "after ten minutes the key counts fully again");
    // The same partner on another occasion today: ×1, ×½, ×¼, nothing; another partner, or tomorrow, fully.
    PracticeState d;
    expect(practice::partnerDecay(d, "bo", "f1", 0, r) == 1 && practice::partnerDecay(d, "bo", "f1", 10, r) == 1, "a first fight with Bo");
    expect(practice::partnerDecay(d, "bo", "f2", 20, r) == .5 && practice::partnerDecay(d, "bo", "f3", 30, r) == .25 &&
               practice::partnerDecay(d, "bo", "f4", 40, r) == 0,
           "then half, a quarter, nothing");
    expect(practice::partnerDecay(d, "cy", "f5", 50, r) == 1, "Cy is someone new");
    expect(practice::partnerDecay(d, "bo", "f6", 86500, r) == 1, "and tomorrow Bo teaches again");
}

void growthLines()
{
    clockNow = 4e9;
    World w = field();
    auto& ada = wolf(w, "ada");
    ada.scentSkill = 30.9;
    w.practise("ada", "track.found");
    auto lines = told(w, "ada");
    expect(lines.size() == 1 && lines[0] == "Your tracking sharpened (31).", "a line as it passes a whole number: " + (lines.empty() ? "" : lines[0]));
    ada.scentSkill = 31.95;
    clockNow += 61;
    w.practise("ada", "track.found");
    expect(ada.scentSkill > 32 && told(w, "ada").empty(), "none again inside two minutes");
    ada.scentSkill = 32.95;
    clockNow += 120;
    w.practise("ada", "track.found");
    lines = told(w, "ada");
    expect(lines.size() == 1 && lines[0] == "Your tracking sharpened (33).", "then the latest number");
    // A sense is told as a percentage, at each hundredth.
    ada.smell = 1.119;
    clockNow += 700;
    w.trainNose("ada");
    bool nose = false;
    for (const auto& l : told(w, "ada"))
        nose = nose || l == "Your nose grows keener (112%).";
    expect(nose, "the nose as a percentage");
}

void theNose()
{
    clockNow = 5e9;
    World w = field();
    auto& ada = wolf(w, "ada");
    w.trainNose("ada");
    expect(ada.smell > 1 && ada.scentSkill > 0, "nosing about sharpens the nose and tracking");
    for (int i = 0; i < 4000; ++i)
    {
        clockNow += 700;
        w.trainNose("ada");
    }
    expect(near(ada.smell, practice::skill("smell")->cap), "to the nose's cap: " + std::to_string(ada.smell));
    ada.noseHealth = 0;
    const double before = ada.scentSkill;
    w.trainNose("ada");
    expect(ada.scentSkill == before, "a nose that doesn't work learns nothing");
}

void savedAndShown()
{
    Entity e;
    e.id = "wolf-ada";
    e.name = "Ada";
    e.grades = {{"strength", "strong"}, {"wisdom", "weak"}};
    e.specialty = "tracker";
    e.skills = {{"craft", 12.5}};
    e.endurance = 61;
    e.progressVersion = 2;
    e.practice->days["tracking"] = {100, 1.5};
    e.practice->rested = 4;
    e.practice->lastGainAt = 99;
    auto saved = wire::persistEntity(e, 0);
    {
        auto grades = json::Value::object();
        grades.add("strength", "strong");
        grades.add("wisdom", "weak");
        grades.add("charm", "strong");              // Not an attribute: dropped.
        grades.add("dexterity", "mighty");          // Not a grade: dropped.
        saved.set("grades", grades);
        auto skills = json::Value::object();
        skills.add("craft", 12.5);
        skills.add("tracking", 40);                 // Not a trade skill (it has its own field): dropped here.
        saved.set("skills", skills);
    }
    const auto back = wire::readEntity(saved);
    expect(back.grades.size() == 2 && back.grades.at("strength") == "strong" && back.grades.at("wisdom") == "weak", "grades round trip; unknown ones dropped");
    expect(back.specialty == "tracker" && back.skills.size() == 1 && back.skills.at("craft") == 12.5, "the specialty and trade skills");
    expect(back.endurance == 61 && back.progressVersion == 2, "stamina the attribute, and the migrations had");
    expect(back.practice->days.count("tracking") && back.practice->days.at("tracking").start == 100 &&
               back.practice->days.at("tracking").gained == 1.5 && back.practice->rested == 4 && back.practice->lastGainAt == 99,
           "today's practice and the rested pool");
    Entity plain;
    plain.id = "wolf-bo";
    plain.name = "Bo";
    const auto quiet = wire::persistEntity(plain, 0);
    for (const char* key : {"grades", "specialty", "skills", "endurance", "progressVersion", "practice"})
        expect(!quiet.has(key), std::string("a plain wolf saves no ") + key);
    const auto old = wire::readEntity(quiet);
    expect(old.endurance == 50 && old.progressVersion == 0 && old.practice->lastGainAt == -1 && old.skills.empty(), "an old save reads as plain");

    // The self view: every attribute and skill with its cap; easing off once today's practice reaches the soft limit.
    e.scentSkill = 20;
    e.practice->days["tracking"] = {1000, 2};
    auto self = json::Value::object();
    wire::practiceView(self, e, 1000 + 60);
    expect(self["attributes"].items().size() == 7 && self["skills"].items().size() == 10, "seven attributes and ten skills");
    bool easing = false, strong = false;
    for (const auto& s : self["skills"].items())
        if (s.string("id") == "tracking")
            easing = s.boolean("easing") && s.number("value") == 20 && s.number("cap") == 100;
    for (const auto& a : self["attributes"].items())
        if (a.string("id") == "strength")
            strong = a.string("grade") == "strong" && a.string("short") == "STR";
    expect(easing, "tracking 20 of 100, easing off for today");
    expect(strong, "Strength, strong");
    auto tomorrow = json::Value::object();
    wire::practiceView(tomorrow, e, 1000 + 86400);
    for (const auto& s : tomorrow["skills"].items())
        if (s.string("id") == "tracking")
            expect(!s.boolean("easing"), "and not once the day has come round");
    expect(self.number("restedPractice") == 4, "the rested practice waiting");
}

void fightingByFighting()
{
    // Doc 49, Phase 2: a blow landed teaches by who it lands on; a fight's end teaches; the caps hold.
    const auto blow = [](const std::string& kind, double foe, const std::string& partner, const std::string& fight) {
        clockNow += 700;
        World w = field();
        auto& ada = wolf(w, "ada");
        World::PracticeContext c(partner, kind, fight);
        c.partnerValue = foe;
        w.practise("ada", "fight.blow", c);
        return ada.fightingSkill - 50;
    };
    const double player = blow("player", 50, "bo", "f1"), resident = blow("resident", 50, "sorrel", "f1"),
                 game = blow("animal", 50, "rabbit", "f1"), fierce = blow("fierce", 50, "boar", "f1");
    const double room = (86 - 50) / 86.0;
    expect(near(player, .2 * room), "a blow on a player of her skill teaches 0.2 × the room left");
    expect(near(resident, player * .8) && near(fierce, player * .6) && near(game, player * .3),
           "a resident teaches 0.8 as much, a fierce animal 0.6, game 0.3");
    expect(near(blow("player", 30, "bo", "f1"), player * .5) && near(blow("player", 65, "bo", "f1"), player * 1.5),
           "a foe 15 weaker teaches half; one 10 better, half again");
    {
        clockNow += 86400;
        World w = field();
        auto& ada = wolf(w, "ada");
        double last = 0;
        std::vector<double> gains;
        for (const char* fight : {"f1", "f2", "f3", "f4"})
        {
            clockNow += 700;
            World::PracticeContext c("bo", "player", fight);
            c.partnerValue = 50;
            w.practise("ada", "fight.blow", c);
            gains.push_back(ada.fightingSkill - 50 - last);
            last = ada.fightingSkill - 50;
        }
        expect(gains[1] < gains[0] * .51 && gains[2] < gains[0] * .26 && gains[3] == 0, "the same foe in a second, third, fourth fight today: half, a quarter, nothing");
    }
    {
        World w = field();
        auto& ada = wolf(w, "ada");
        w.practise("ada", "fight.end", {"", "", "f9"});
        expect(ada.fightingSkill > 50, "standing to a fight's end teaches");
        ada.fightingSkill = 85.99;
        for (int i = 0; i < 20; ++i)
        {
            clockNow += 700;
            w.practise("ada", "fight.end", {"", "", "g" + std::to_string(i)});
        }
        expect(ada.fightingSkill == 86, "a plain wolf fights to 86 at most");
        ada.quickened = true;
        clockNow += 700;
        w.practise("ada", "fight.end", {"", "", "q"});
        expect(ada.fightingSkill > 86 && practice::capFor(*practice::skill("fighting"), true) == 100, "a Quickened wolf's goes on, to 100");
    }
    {
        World w = field();
        w.practising = false;
        auto& ada = wolf(w, "ada");
        w.practise("ada", "fight.end", {"", "", "f10"});
        expect(ada.fightingSkill == 50, "with practice off (the simulations), nothing grows");
    }
}

void workTeachesTrades()
{
    // Doc 49, Phase 3: doc 44's awards became practice. Foraging is labour; a contract, commerce; a Gift lent, wisdom;
    // an apprentice beside their master learns the trade three times as fast.
    clockNow += 86400;
    World w = field();
    auto& ada = wolf(w, "ada");
    w.practise("ada", "forage.pick");
    expect(ada.skills["labour"] > 0, "foraging grows labour");
    w.practise("ada", "contract.done", {"contract-1"});
    expect(ada.skills["commerce"] > 0, "a contract grows commerce");
    w.practise("ada", "gift.lend", {"smith"});
    expect(ada.wisdom > 30, "a Gift lent grows wisdom");
    World::PracticeContext master("hale");
    master.teacher = "master";
    clockNow += 700;
    w.practise("ada", "apprentice.craft", master);
    expect(near(ada.skills["craft"], .003 * 3), "an apprentice at the master's side: three times as fast");
}

void buildingAWolf()
{
    // Doc 49, Phase 4: strengths and weaknesses within the budget, a specialty, presets; what they start at and cap at.
    const auto check = [](const std::string& text, const std::string& tier = "normal") {
        json::Value v;
        std::string error;
        expect(json::parse(text, v, error), "parses: " + text);
        practice::Build b;
        return std::make_pair(practice::checkBuild(v, tier, b, error), b);
    };
    const auto hunter = check(R"({"grades": {"smell": "strong", "hearing": "strong", "dexterity": "strong", "wisdom": "weak"}, "specialty": "tracker"})");
    expect(hunter.first && hunter.second.grades.size() == 4 && hunter.second.specialty == "tracker", "the Hunter preset fits the budget");
    expect(!check(R"({"grades": {"strength": "strong", "dexterity": "strong", "wisdom": "strong"}})").first, "three strengths with nothing back: over 2");
    expect(!check(R"({"grades": {"strength": "strong", "dexterity": "strong", "wisdom": "strong", "stamina": "strong", "smell": "weak", "hearing": "weak"}})").first,
           "four strengths: over the limit of three");
    expect(!check(R"({"grades": {"charm": "strong"}})").first && !check(R"({"grades": {"strength": "mighty"}})").first, "an unknown attribute or grade");
    expect(!check(R"({"specialty": "juggler"})").first && !check(R"({"colour": "red"})").first, "an unknown specialty, or anything else");
    expect(check(R"({"grades": {"strength": "plain"}, "specialty": ""})").second.grades.empty(), "plain is no grade at all");
    expect(check(R"({"grades": {"strength": "weak", "dexterity": "strong"}})").first, "the user's example: weak but quick");
    expect(check(R"({"grades": {"strength": "strong", "wisdom": "strong"}})", "quickened").first, "and a strong, wise Quickened wolf");
    // Applied to a wolf: each grade's start, and its cap.
    Entity e;
    World::applyBuild(e, hunter.second);
    const auto& smell = *practice::skill("smell");
    expect(near(e.smell, 1.15) && near(World::practiceCap(e, smell), 1.6), "a strong nose starts at 115% and can grow to 160%");
    expect(e.wisdom == 20 && World::practiceCap(e, *practice::skill("wisdom")) == 45, "weak wisdom starts at 20 and stops at 45");
    expect(e.strength == 50 && World::practiceCap(e, *practice::skill("strength")) == 70, "plain strength: 50, to 70");
    expect(e.scentSkill == 25 && e.specialty == "tracker", "a tracker starts tracking at 25");
    // A weak attribute stops at its cap; a value past it (made before grades) is kept but grows no more.
    clockNow += 86400;
    World w = field();
    auto& ada = wolf(w, "ada");
    World::applyBuild(ada, hunter.second);
    ada.wisdom = 44.999;
    w.practise("ada", "gift.lend", {"smith"});
    expect(ada.wisdom == 45, "weak wisdom stops at 45");
    ada.wisdom = 60;
    clockNow += 700;
    w.practise("ada", "gift.lend", {"smith"});
    expect(ada.wisdom == 60, "and one already past it is kept, growing no more");
    // Stamina the attribute: nothing changes at 50; a strong wolf's breath comes back faster and running drains it less.
    expect(near(practice::staminaRecovery(50), 1) && near(practice::staminaDrain(50), 1) && near(practice::staminaPerTurnExtra(50), 0), "at 50, as before");
    expect(practice::staminaRecovery(85) > 1.1 && practice::staminaDrain(85) < .9 && practice::staminaPerTurnExtra(85) > 1, "at 85, quicker breath");
    expect(battle::staminaPerTurn(0, 50, 85) > battle::staminaPerTurn(0, 50, 50) && battle::staminaPerTurn(0, 50, 50) == battle::staminaPerTurn(0, 50),
           "and a fight turn gives a little more back");
    // The creator's copy of all this.
    const auto& catalog = practice::creationCatalog();
    expect(catalog["attributes"].items().size() == 7 && catalog["presets"].items().size() == 4 && catalog.number("budget") == 2,
           "the creator is sent seven attributes, four presets and the budget");
}

void movingTeaches()
{
    // Running far trains stamina, walking under a heavy load strength (each 200 and 100 tiles).
    clockNow += 86400;
    World w = field();
    auto& ada = wolf(w, "ada");
    ada.practice->ran = 199.9;
    ada.pace = 10;
    ada.clientWalks = false;
    w.practise("ada", "run.far");
    expect(ada.endurance > 50, "running far grows stamina");
}

void forDevelopment()
{
    clockNow = 6e9;
    World w = field();
    auto& ada = wolf(w, "ada");
    expect(w.setPractice("ada", "tracking", 60, 2).ok && ada.scentSkill == 60 && ada.practice->days["tracking"].gained == 2,
           "the Dev Console sets a skill and today's practice");
    expect(w.setPractice("ada", "fighting", 300).ok && ada.fightingSkill == 86, "never past the cap");
    expect(w.setPractice("ada", "craft", 10).ok && ada.skills["craft"] == 10, "a trade skill");
    expect(!w.setPractice("ada", "charm", 10).ok && !w.setPractice("ada", "tracking", -1).ok, "not a skill, or not a value");
}
} // namespace

int main()
{
    try
    {
        theCatalog();
        roomAndTheSoftLimit();
        restedPractice();
        aBetterWolfNear();
        partnersAndVariety();
        growthLines();
        theNose();
        savedAndShown();
        fightingByFighting();
        workTeachesTrades();
        buildingAWolf();
        movingTeaches();
        forDevelopment();
    }
    catch (const std::exception& e)
    {
        std::cerr << "FAILED: " << e.what() << '\n';
        return 1;
    }
    std::cout << checks << " practice checks passed\n";
    return 0;
}
