// The game's JSON shapes (RatwWire.h): what others may see of a character and what only its owner may, how a save
// writes a character and reads it back strictly, the weather, light and clock records, the society, and the pace and
// environment commands. A malformed record is refused, never quietly turned into a default.
#include "RatwWire.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace ratw;
using json::Value;
namespace
{
int checks = 0;
void expect(bool condition, const std::string& message)
{
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}

bool sameAppearance(const Appearance& a, const Appearance& b)
{
    return a.species == b.species && a.sex == b.sex && a.stature == b.stature && a.pattern == b.pattern &&
           a.baseColor == b.baseColor && a.gradientColor == b.gradientColor && a.markingColor == b.markingColor &&
           a.gradientAmount == b.gradientAmount && a.patternAmount == b.patternAmount;
}

// An appearance unlike the default in every field, so a round trip can't pass by falling back to it.
Appearance unusualAppearance()
{
    Appearance a;
    a.species = "arctic";
    a.sex = "female";
    a.stature = "tall";
    a.pattern = "piebald";
    a.baseColor = 0;
    a.gradientColor = 7;
    a.markingColor = 6;
    a.gradientAmount = .125;
    a.patternAmount = .875;
    return a;
}

// Whether a world restores with this record as its one player: the whole save stands or falls with it.
bool restoresWith(const Value& record)
{
    World w;
    w.addPlayer("player-ash", "Ash");
    auto saved = w.save();
    saved.players = {wire::readEntity(record)};
    return World().restore(saved).ok;
}


// Phase 9's appearance (Docs/Design/29-client-polish.md): free colours, eyes, build and up to six markings, each
// optional; only known masks and "#rrggbb" colours.
void appearanceV2Tests()
{
    Appearance a = unusualAppearance();
    a.coat = "#7a5c3e";
    a.eyes = "#e0b040";
    a.build = "lean";
    a.markings = {{"socks", "#f0ece0", .9}, {"blaze", "#ffffff", 1}, {"tail_tip", "#202020", .6}};
    expect(validAppearance(a), "a phase 9 appearance is valid");
    const auto j = wire::appearance(a);
    expect(j.size() == 13 && j.has("markings") && j.has("coat") && !j.has("gradientTint"), "only the choices made are written");
    Appearance back;
    expect(wire::readAppearance(j, back) && back.coat == a.coat && back.eyes == a.eyes && back.build == "lean" &&
               back.markings.size() == 3 && back.markings[1].mask == "blaze" && back.markings[2].opacity == .6,
           "and read back");
    const auto refused = [&](Value v, const std::string& what) { expect(!wire::readAppearance(v, back), "refused: " + what); };
    auto bad = j;
    bad.set("coat", "red");
    refused(bad, "a colour that is not #rrggbb");
    bad = j;
    bad.set("build", "enormous");
    refused(bad, "an unknown build");
    bad = j;
    bad.set("wings", true);
    refused(bad, "an unknown field");
    Appearance many = a;
    many.markings.assign(7, {"socks", "#ffffff", 1});
    refused(wire::appearance(many), "seven markings");
    Appearance strange = a;
    strange.markings = {{"tattoo", "#ffffff", 1}};
    refused(wire::appearance(strange), "an unknown marking");
    Appearance faint = a;
    faint.markings = {{"socks", "#ffffff", 1.5}};
    refused(wire::appearance(faint), "an opacity over one");
    expect(wire::appearance(unusualAppearance()).size() == 9, "an older appearance keeps its nine fields");
}

void appearanceTests()
{
    const auto canonical = wire::appearance(unusualAppearance());
    expect(canonical.size() == 9, "an appearance is exactly nine fields");
    for (const char* key : {"species", "sex", "stature", "pattern", "baseColor", "gradientColor", "markingColor",
                            "gradientAmount", "patternAmount"})
        expect(canonical.has(key), std::string("an appearance says its ") + key);
    Appearance back;
    expect(wire::readAppearance(canonical, back) && sameAppearance(back, unusualAppearance()), "and reads back exactly");
    json::Value parsed;
    std::string error;
    expect(json::parse(json::dump(canonical), parsed, error) && wire::readAppearance(parsed, back) &&
               sameAppearance(back, unusualAppearance()),
           "through text too");

    // Refused, and the one being read into is left as it was.
    const Appearance before = unusualAppearance();
    const auto refused = [&](const Value& o, const std::string& what) {
        Appearance out = before;
        expect(!wire::readAppearance(o, out), "refused: " + what);
        expect(sameAppearance(out, before), "and nothing is written on a refusal: " + what);
        expect(wire::readAppearance(o).species.empty(), "the one-argument read marks it invalid: " + what);
    };
    for (const auto& [key, _] : canonical.fields())
    {
        auto missing = canonical;
        missing.erase(key);
        refused(missing, "a missing " + key);
        auto null = canonical;
        null.set(key, nullptr);
        refused(null, "a null " + key);
        auto empty = canonical;
        empty.set(key, "");
        refused(empty, "an empty " + key);
        auto swapped = canonical;                  // Still nine fields, one of them unknown.
        swapped.erase(key);
        swapped.set("coat", key == "species" ? Value("timber") : Value(1));
        refused(swapped, "an unknown field in place of " + key);
    }
    auto extra = canonical;
    extra.set("glow", true);
    refused(extra, "a tenth, unknown field");
    refused(Value::object(), "an empty object");
    refused(Value(), "null");
    refused(Value("timber"), "a string");
    // Nothing is coerced: enums are words, palette indices and blends are numbers.
    auto numericSpecies = canonical;
    numericSpecies.set("species", 1);
    refused(numericSpecies, "a numeric species");
    auto boolSex = canonical;
    boolSex.set("sex", true);
    refused(boolSex, "a boolean sex");
    auto unknownPattern = canonical;
    unknownPattern.set("pattern", "stripes");
    refused(unknownPattern, "a pattern the game doesn't have");
    auto stringColor = canonical;
    stringColor.set("baseColor", "3");
    refused(stringColor, "a palette index as a numeric string");
    auto boolColor = canonical;
    boolColor.set("markingColor", true);
    refused(boolColor, "a palette index as a boolean");
    auto outOfPalette = canonical;
    outOfPalette.set("gradientColor", CoatColorCount);
    refused(outOfPalette, "a palette index past the palette");
    auto fractionalColor = canonical;
    fractionalColor.set("baseColor", 2.5);
    refused(fractionalColor, "a fractional palette index");
    auto stringBlend = canonical;
    stringBlend.set("gradientAmount", "0.5");
    refused(stringBlend, "a blend as a numeric string");
    auto boolBlend = canonical;
    boolBlend.set("patternAmount", true);
    refused(boolBlend, "a blend as a boolean");
    auto overBlend = canonical;
    overBlend.set("patternAmount", 1.5);
    refused(overBlend, "a blend past one (not clamped)");
}

void publicEntityTests()
{
    World w;
    w.addPlayer("player-ash", "Ash");
    auto e = *w.entity("player-ash");
    e.sneakSkill = 77;
    e.hearingSkill = 66;
    e.scentSkill = 55;
    e.stamina = 44;
    e.dexterity = 88;
    e.age = 30;
    e.strength = 71;
    const auto seen = wire::entity(e, w.time());
    // What any other observer is sent: never the body's numbers.
    for (const char* key : {"sneakSkill", "hearingSkill", "scentSkill", "smell", "noseHealth", "stamina", "dexterity",
                            "effectiveDexterity", "age", "strength", "wisdom", "pace", "effectivePace", "exhausted",
                            "lastBirthdayDay", "ageNoticePending", "staminaRate"})
        expect(!seen.has(key), std::string("others are not told a character's ") + key);
    const auto text = json::dump(seen);
    expect(text.find("77") == std::string::npos && text.find("66") == std::string::npos,
           "the sneak and hearing skills appear nowhere in what others receive");
    expect(seen.string("lifeStage") == "adult" && seen.number("shoulderHeightCm") > 0,
           "a character's life stage and size are public, its exact age is not");
    // The life stage follows the age.
    const std::vector<std::pair<int, std::string>> stages = {{10, "young"}, {15, "adolescent"}, {40, "adult"}, {70, "old"}};
    for (const auto& [age, stage] : stages)
    {
        e.age = age;
        expect(wire::entity(e, w.time()).string("lifeStage") == stage, "age " + std::to_string(age) + " is " + stage);
    }
    expect(wire::entity(e, w.time())["appearance"] == wire::appearance(e.appearance), "the portrait is the appearance");

    // Rising from a seat is shown as it happens.
    expect(w.setPosture("player-ash", "sitting").ok && w.setPosture("player-ash", "standing").ok, "Ash sits, then stands");
    const auto rising = wire::entity(*w.entity("player-ash"), w.time());
    expect(rising.string("posture") == "rising" && rising.string("postureTarget") == "standing" &&
               rising.number("postureRemaining") > 0,
           "a character getting up is sent as rising, toward standing");

    // Moving is reported the moment motion is asked for, before the character has gone anywhere.
    auto still = *w.entity("player-ash");
    still.velocity = {};
    still.input = {};
    still.path.clear();
    expect(!wire::entity(still, 0).boolean("moving"), "a character with nothing queued isn't moving");
    auto pushed = still;
    pushed.input = {1, 0};
    expect(wire::entity(pushed, 0).boolean("moving"), "held input is moving before any displacement");
    auto routed = still;
    routed.path = {{still.position.x + 3, still.position.y}};
    expect(wire::entity(routed, 0).boolean("moving"), "a queued path is moving before any displacement");

    // Speaking is a flag and the time left, from the clock given.
    auto talking = still;
    talking.speakingUntil = 10;
    expect(wire::entity(talking, 4).boolean("speaking") && wire::entity(talking, 4).number("speakingRemaining") == 6 &&
               !wire::entity(talking, 12).boolean("speaking") && wire::entity(talking, 12).number("speakingRemaining") == 0,
           "speaking until a time, and not after it");
}

void privateTests()
{
    World w;
    w.addPlayer("player-ash", "Ash");
    auto e = *w.entity("player-ash");
    e.age = 37;
    e.pace = 8;
    e.stamina = 12;
    e.exhausted = true;
    auto self = wire::entity(e, 0);
    wire::privatePace(self, e);
    expect(self.number("age") == 37, "the owner is told their exact age");
    expect(self.number("pace") == 8, "and the pace they asked for");
    expect(self.number("effectivePace") == 0 && self.string("paceName") == paceName(0),
           "but an exhausted character only walks");
    expect(self.boolean("exhausted") && self.number("stamina") == 12, "and is told why");
    expect(self.has("dexterity") && self.has("effectiveDexterity") && self.has("strength") && self.has("topSpeed"),
           "with the rest of their body's numbers");
    e.exhausted = false;
    e.stamina = 90;
    auto rested = wire::entity(e, 0);
    wire::privatePace(rested, e);
    expect(rested.number("effectivePace") == 8, "rested, the pace asked for is the pace kept");
    // privatePace() replaces rather than repeats a field.
    wire::privatePace(rested, e);
    std::size_t ages = 0;
    for (const auto& [key, _] : rested.fields())
        ages += key == "age";
    expect(ages == 1, "applying the self view twice leaves one of each field");
}

void persistTests()
{
    World w;
    w.addPlayer("player-ash", "Ash");
    auto e = *w.entity("player-ash");
    e.sneakSkill = 12.5;
    e.hearingSkill = 34;
    e.scentSkill = 56;
    e.smell = .9;
    e.noseHealth = .75;
    e.dexterity = 61;
    e.stamina = 15;
    e.pace = 7;
    e.exhausted = true;
    e.age = 23;
    e.strength = 64;
    e.wisdom = 41;
    e.lastBirthdayDay = 0;
    e.ageNoticePending = 2;
    e.appearance = unusualAppearance();
    // Travel intentions, which a save never keeps.
    e.input = {1, 0};
    e.path = {{e.position.x + 2, e.position.y}};
    e.turnTarget = e.facing + 1;
    const auto saved = wire::persistEntity(e, w.time());
    for (const char* key : {"path", "input", "travel", "destination", "route", "velocity", "turnTarget"})
        expect(!saved.has(key), std::string("travel intentions do not enter the persisted entity: ") + key);
    Value parsed;
    std::string error;
    expect(json::parse(json::dump(saved), parsed, error), "the persisted entity parses: " + error);
    const auto back = wire::readEntity(parsed);
    expect(back.id == e.id && back.name == e.name && back.cellId == e.cellId && back.position.x == e.position.x &&
               back.position.y == e.position.y && back.facing == e.facing,
           "who and where round trip");
    expect(back.sneakSkill == 12.5 && back.hearingSkill == 34 && back.scentSkill == 56, "the skills round trip");
    expect(back.smell == .9 && back.noseHealth == .75, "the nose round trips");
    expect(back.dexterity == 61 && back.stamina == 15 && back.pace == 7 && back.exhausted, "the body and pace round trip");
    expect(back.age == 23 && back.strength == 64 && back.wisdom == 41 && back.lastBirthdayDay == 0 &&
               back.ageNoticePending == 2,
           "age, strength and the birthday bookkeeping round trip");
    expect(sameAppearance(back.appearance, unusualAppearance()), "the appearance round trips");
    expect(back.path.empty() && back.input.x == 0 && back.input.y == 0 && back.turnTarget == back.facing,
           "and the travel intentions come back empty");
    expect(restoresWith(parsed), "a world restores with the round-tripped character");

    // A legacy record: only who and where. The body gets its defaults.
    auto legacy = Value::object();
    legacy.set("id", "player-ash");
    legacy.set("name", "Ash");
    legacy.set("cell", e.cellId);
    legacy.set("x", e.position.x);
    legacy.set("y", e.position.y);
    const auto old = wire::readEntity(legacy);
    expect(old.sneakSkill == 0 && old.hearingSkill == 0 && old.scentSkill == 0, "missing skills are zero");
    expect(old.smell == 1 && old.noseHealth == 1, "a missing nose is healthy");
    expect(old.dexterity == 50 && old.stamina == 100, "missing dexterity is 50, missing stamina is full");
    expect(old.pace == 0 && !old.exhausted, "a missing pace is walking, and not exhausted");
    expect(old.age == 18 && old.strength == 50 && old.lastBirthdayDay == -1 && old.ageNoticePending == 0,
           "a missing age is eighteen, with no birthday yet and nothing to announce");
    expect(validAppearance(old.appearance) && sameAppearance(old.appearance, Appearance{}),
           "a missing appearance is the canonical default");
    expect(restoresWith(legacy), "a world restores with the legacy character");

    // A field present but of the wrong type is never its default: the character, and so the save, is refused.
    const auto rejected = [&](const std::string& key, const Value& value, bool marked, const std::string& what) {
        auto bad = parsed;
        bad.set(key, value);
        expect(marked, "read as invalid: " + what);
        expect(!restoresWith(bad), "and the save is refused: " + what);
    };
    const auto with = [&](const std::string& key, const Value& value) {
        auto bad = parsed;
        bad.set(key, value);
        return wire::readEntity(bad);
    };
    rejected("sneakSkill", "12", with("sneakSkill", "12").sneakSkill < 0, "a sneak skill as a string");
    rejected("scentSkill", "56", with("scentSkill", "56").scentSkill < 0, "a scent skill as a string");
    rejected("noseHealth", true, with("noseHealth", true).noseHealth < 0, "nose health as a boolean");
    rejected("dexterity", "61", with("dexterity", "61").dexterity < 0, "dexterity as a string");
    rejected("stamina", "15", with("stamina", "15").stamina < 0, "stamina as a string");
    rejected("strength", "64", with("strength", "64").strength < 0, "strength as a string");
    rejected("exhausted", "true", with("exhausted", "true").stamina < 0, "exhausted as a string");
    rejected("pace", 4.5, with("pace", 4.5).pace < 0, "a fractional pace");
    rejected("pace", "7", with("pace", "7").pace < 0, "a pace as a string");
    rejected("age", 23.5, with("age", 23.5).age < 0, "a fractional age");
    rejected("age", "23", with("age", "23").age < 0, "an age as a string");
    rejected("ageNoticePending", .5, with("ageNoticePending", .5).ageNoticePending < 0, "a fractional notice count");
    rejected("lastBirthdayDay", "0", with("lastBirthdayDay", "0").lastBirthdayDay < -1, "a birthday day as a string");
    rejected("appearance", nullptr, with("appearance", nullptr).appearance->species.empty(), "a null appearance");
    rejected("appearance", "timber", with("appearance", "timber").appearance->species.empty(), "an appearance as a string");
    rejected("appearance", Value::object(), with("appearance", Value::object()).appearance->species.empty(),
             "an empty appearance");
    auto stringColor = wire::appearance(unusualAppearance());
    stringColor.set("baseColor", "0");
    rejected("appearance", stringColor, with("appearance", stringColor).appearance->species.empty(),
             "an appearance with a palette index as a string");
}

void recordTests()
{
    // Wind.
    Wind breeze;
    breeze.direction = 1.25;
    breeze.strength = .6;
    breeze.variable = true;
    const auto windBack = wire::readWind(wire::wind(breeze));
    expect(windBack.direction == 1.25 && windBack.strength == .6 && windBack.variable, "the wind round trips");
    auto stringDirection = wire::wind(breeze);
    stringDirection.set("direction", "east");
    expect(wire::readWind(stringDirection).strength < 0, "malformed wind cannot silently become calm weather");
    auto missingVariable = wire::wind(breeze);
    missingVariable.erase("variable");
    expect(wire::readWind(missingVariable).strength < 0, "nor can wind without its variable flag");
    expect(wire::readWind(Value("breezy")).strength < 0, "nor wind that isn't an object");
    {
        World w;
        auto saved = w.save();
        saved.winds["exterior"] = wire::readWind(stringDirection);
        expect(!World().restore(saved).ok, "and a save carrying it is refused");
    }

    // Lighting.
    Lighting lamp;
    lamp.artificial = .4;
    lamp.daylightAccess = .2;
    lamp.tone = "cool";
    const auto lightBack = wire::readLighting(wire::lighting(lamp));
    expect(lightBack.artificial == .4 && lightBack.daylightAccess == .2 && lightBack.tone == "cool", "lighting round trips");
    auto extraLight = wire::lighting(lamp);
    extraLight.set("colour", "blue");
    for (const auto& [bad, what] : std::vector<std::pair<Value, std::string>>{
             {Value::object(), "an empty object"}, {Value(true), "a boolean"}, {Value("warm"), "a string"},
             {Value(), "null"}, {extraLight, "an extra field"}})
    {
        expect(wire::readLighting(bad).artificial < 0, "lighting refused: " + what);
        World w;
        auto saved = w.save();
        saved.lighting["tavern"] = wire::readLighting(bad);
        expect(!World().restore(saved).ok, "and a save carrying it is refused: " + what);
    }

    // The clock's offset.
    auto clock = Value::object();
    expect(wire::readClockOffset(clock) == 12, "a missing clock offset is the legacy noon epoch");
    clock.set("clockOffsetHours", 6.5);
    expect(wire::readClockOffset(clock) == 6.5, "a clock offset reads back");
    for (const auto& [bad, what] : std::vector<std::pair<Value, std::string>>{
             {Value(true), "a boolean"}, {Value("12"), "a string"}, {Value(), "null"}})
    {
        clock.set("clockOffsetHours", bad);
        expect(wire::readClockOffset(clock) < 0, "clock offset refused: " + what);
    }

    // Weather, saved by number.
    for (int kind = 0; kind < WeatherKinds; ++kind)
        expect(int(wire::readWeather(Value(kind))) == kind, "weather " + std::to_string(kind) + " reads back");
    for (const auto& [bad, what] : std::vector<std::pair<Value, std::string>>{
             {Value(true), "a boolean"}, {Value("1"), "a string"}, {Value(1.5), "a fraction"}, {Value(1e100), "a huge number"},
             {Value(), "null"}, {Value(WeatherKinds), "a kind past the last"}, {Value(-1), "a negative"}})
        expect(int(wire::readWeather(bad)) < 0, "weather refused: " + what);
}

void environmentTests()
{
    World w;
    w.setTimeOfDay(0);
    const auto env = w.environmentAt("exterior");
    const auto o = wire::environment(env);
    expect(o.size() == 14, "the environment is exactly fourteen fields");
    for (const char* key : {"calendar", "hour", "phase", "daylight", "illumination", "sight", "hearing", "scent", "movement",
                            "artificialLight", "daylightAccess", "glowStrength", "lightingTone", "lightSource"})
        expect(o.has(key), std::string("the environment says its ") + key);
    expect(o["calendar"].has("year") && o["calendar"].number("year") == env.date.year, "the calendar carries the year");
    expect(o.string("phase") == env.phase && env.phase == "night", "the phase is the world's (night, at midnight)");
    expect(o.number("hearing") == env.hearing, "hearing is the world's");

    // An unlit room says so.
    expect(wire::environmentCommand(w, "tavern", "lighting", "unlit", true).ok, "the tavern's lamps go out");
    const auto dark = wire::environmentDescription(*w.cell("tavern"), w.environmentAt("tavern"));
    expect(dark.find("unlit and dark") != std::string::npos, "an unlit room is described as unlit and dark: " + dark);
    const auto yard = wire::environmentDescription(*w.cell("exterior"), w.environmentAt("exterior"));
    expect(yard.find("unlit") == std::string::npos, "the yard outside isn't a room: " + yard);
}

void sensesAndMapTests()
{
    Snapshot snapshot;
    snapshot.scentCues = {{2, 3, true}, {6, 1, false}};
    snapshot.movementHeard = true;
    const auto o = wire::senses(snapshot);
    expect(o["scentCues"].size() == 2 && o.boolean("movementHeard"), "two scent cues and heard movement");
    for (const auto& cue : o["scentCues"].items())
    {
        expect(cue.size() == 3 && cue.has("sector") && cue.has("strength") && cue.has("windborne"),
               "a scent cue is a sector, a strength and whether it is windborne");
        for (const char* key : {"id", "source", "name", "x", "y", "cell", "distance", "count"})
            expect(!cue.has(key), std::string("and nothing about its source: ") + key);
    }
    expect(o["scentCues"].items()[0].number("sector") == 2 && o["scentCues"].items()[0].number("strength") == 3 &&
               o["scentCues"].items()[0].boolean("windborne"),
           "the cue as sensed");

    // A remembered, distant cell: where it is and what is known of it, never its terrain again or who is there.
    MapCell far;
    far.id = "exterior";
    far.name = "Juniper Yard";
    far.width = 4;
    far.height = 2;
    far.knowledge = Knowledge::Visited;
    far.current = false;
    far.rememberedGlyphs = {'.', '.', 'T', '#', '.', '.', '.', '.'};
    const auto atlas = wire::mapCell(far, false);
    expect(!atlas.has("glyphs"), "the distant atlas does not resend terrain");
    expect(!atlas.has("entities") && !atlas.has("doors"), "and never contains remote entities or fixtures");
    expect(atlas.string("knowledge") == "visited" && !atlas.boolean("current"), "it says it is known and not here");
    const auto here = wire::mapCell(far);
    expect(here.string("glyphs") == "..T#....", "with glyphs asked for, the remembered tiles are sent");
    expect(!here.has("entities"), "and still nobody standing on them");
}

void societyTests()
{
    World w;
    const auto good = wire::society(w.save().society);
    expect(wire::readSociety(good).minted >= 0, "the world's society reads back");
    expect(!good["residents"].fields().empty() && !good["accounts"].fields().empty(), "and it has residents and purses");
    const auto refused = [&](const Value& bad, const std::string& what) {
        expect(wire::readSociety(bad).minted == -1, "the whole society is refused: " + what);
    };
    for (const char* key : {"minted", "sunk", "nextEntry", "budgetDay", "exportsRemaining", "importsRemaining", "herbPatch",
                            "decisionRemainder"})
    {
        auto bad = good;
        bad.set(key, "1");
        refused(bad, std::string(key) + " as a string");
    }
    auto ledger = good;
    ledger.set("ledger", Value::object());
    refused(ledger, "a ledger that isn't a list");
    auto noLedger = good;
    noLedger.erase("ledger");
    refused(noLedger, "no ledger");
    const auto firstResident = good["residents"].fields().front().first;
    for (const char* key : {"role", "task", "reason", "goalCell"})
    {
        auto bad = good;
        bad.find("residents")->find(firstResident)->set(key, 7);
        refused(bad, std::string("a resident's ") + key + " as a number");
    }
    const auto firstAccount = good["accounts"].fields().front().first;
    auto nullAccount = good;
    nullAccount.find("accounts")->set(firstAccount, nullptr);
    refused(nullAccount, "a null purse");
    auto stringAccount = good;
    stringAccount.find("accounts")->set(firstAccount, "rich");
    refused(stringAccount, "a purse as a string");
    auto stringCash = good;
    stringCash.find("accounts")->find(firstAccount)->set("cash", "10");
    refused(stringCash, "cash as a string");
    refused(Value("society"), "a society that isn't an object");
}

void commandTests()
{
    World w;
    w.addPlayer("player-ash", "Ash");
    const auto paceOf = [&] { return w.entity("player-ash")->pace; };
    const auto pace = [&](const Value& v) {
        auto o = Value::object();
        o.set("pace", v);
        return wire::paceCommand(w, "player-ash", o);
    };
    expect(pace(4).ok && paceOf() == 4, "a pace of four is taken");
    for (const auto& [bad, what] : std::vector<std::pair<Value, std::string>>{
             {Value(-1), "-1"}, {Value(11), "11"}, {Value(4.5), "4.5"}, {Value(true), "true"}, {Value("4"), "\"4\""},
             {Value(1e100), "1e100"}, {Value(), "null"}})
        expect(!pace(bad).ok && paceOf() == 4, "a pace of " + what + " is refused and the pace kept");
    expect(!wire::paceCommand(w, "player-ash", Value::object()).ok && paceOf() == 4, "no pace at all is refused");
    expect(pace(0).ok && paceOf() == 0 && pace(10).ok && paceOf() == 10, "the ends, walking and sprinting, are taken");
    expect(pace(5.0).ok && paceOf() == 5, "a whole number written as a decimal is still a step");
    expect(!wire::paceCommand(w, "nobody", Value::object()).ok, "nor for someone who isn't there");

    // Environment controls are for development sessions only.
    const double days = w.calendarDays();
    const auto weather = w.cell("exterior")->weather;
    for (const auto& [type, value] : std::vector<std::pair<std::string, std::string>>{
             {"time", "night"}, {"weather", "snow"}, {"lighting", "unlit"}, {"calendar", "year"}})
        expect(!wire::environmentCommand(w, "exterior", type, value, false).ok, "without dev tools, " + type + " is refused");
    expect(w.calendarDays() == days && w.cell("exterior")->weather == weather, "and nothing changed");
    for (const char* type : {"time", "weather", "lighting", "calendar"})
        for (const char* value : {"", "midnight", "nan", "-1", "24", "storm-typo", "laser"})
            expect(!wire::environmentCommand(w, "exterior", type, value, true).ok,
                   std::string("with dev tools, an unknown ") + type + " preset \"" + value + "\" is refused");
    expect(!wire::environmentCommand(w, "nowhere", "weather", "snow", true).ok, "weather for a cell that isn't there is refused");
    expect(!wire::environmentCommand(w, "exterior", "gravity", "low", true).ok, "an unknown control is refused");
    expect(wire::environmentCommand(w, "exterior", "time", "night", true).ok && w.environmentAt("exterior").phase == "night",
           "night falls");
    expect(wire::environmentCommand(w, "exterior", "weather", "snow", true).ok && w.cell("exterior")->weather == Weather::Snow,
           "snow falls");
    expect(wire::environmentCommand(w, "tavern", "lighting", "unlit", true).ok && w.cell("tavern")->lighting.artificial == 0 &&
               w.cell("tavern")->lighting.daylightAccess == 0,
           "the tavern goes dark");
    expect(wire::environmentCommand(w, "tavern", "lighting", "warm", true).ok && w.cell("tavern")->lighting.artificial == 1 &&
               w.cell("tavern")->lighting.tone == "warm",
           "and warm again");
    const double beforeYear = w.calendarDays();       // Night fell earlier today: the clock moved since `days`.
    expect(wire::environmentCommand(w, "exterior", "calendar", "year", true).ok && w.calendarDays() == beforeYear + 365,
           "a year passes");
}
} // namespace

void partnersSaved()
{
    // Doc 53: Allow hunting partners and Allow work partners, on unless turned off, kept with the character.
    Entity e;
    e.id = "player-ada";
    e.name = "Ada";
    expect(!wire::persistEntity(e, 0).has("noHuntPartners") && !wire::persistEntity(e, 0).has("noWorkPartners"), "on: nothing written");
    e.noHuntPartners = true;
    e.noWorkPartners = true;
    const auto back = wire::readEntity(wire::persistEntity(e, 0));
    expect(back.noHuntPartners && back.noWorkPartners, "off: kept, and read back");
    Entity npc = e;
    npc.npc = true;
    expect(!wire::readEntity(wire::persistEntity(npc, 0)).noHuntPartners, "a resident has no such setting");
}

int main()
{
    try
    {
        appearanceTests();
        appearanceV2Tests();
        publicEntityTests();
        privateTests();
        persistTests();
        recordTests();
        environmentTests();
        sensesAndMapTests();
        societyTests();
        commandTests();
        partnersSaved();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Wire tests passed: " << checks << " checks.\n";
    return 0;
}
