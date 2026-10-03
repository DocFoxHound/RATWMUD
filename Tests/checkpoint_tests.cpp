// The portable checkpoint (RatwCheckpoint.h): a running world's document reads back to the same world, through text,
// and a malformed one is refused rather than half-read.
#include "RatwCheckpoint.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <map>
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

std::vector<Entity> npcsOf(const World& w)
{
    std::vector<Entity> out;
    for (const auto& e : w.save().npcs)
        out.push_back(e);
    return out;
}

checkpoint::ServerState serverWith(const World& w)
{
    checkpoint::ServerState s;
    s.sequence = 42;
    s.revision = 7;
    s.accounts = json::Value::object();
    s.accounts.set("ash", "opaque");
    for (const auto& e : w.save().players)
        s.characters[e.id] = e;
    s.companions["npc_scout"] = "player-ash";
    s.memories.record("npc_keeper", "player-ash", {3, 100, "player-ash", "A meal, please."});
    s.memories.summaries.push_back({"m1", "npc_keeper", "player-ash", "They ate well.", 10, 20, {1, 2}});
    s.social.entries.push_back({5, 50, "player-ash", "npc_keeper", "reply", 2, "s1"});
    s.social.sessions["s1"] = {"s1", "tavern", 40, 60, 0, {{"player-ash", {2, 12, 1, 60, 40, {"npc_keeper"}}}}};
    s.commandReceipts["player-ash"] = {"c1", "c2"};
    s.responseReceipts["player-ash"]["c1"] = "{\"ok\":true}";
    return s;
}

// Every field of a character the checkpoint keeps (not motion, typing or speech, which are never reloaded).
bool sameCharacter(const Entity& a, const Entity& b)
{
    const auto& x = a.appearance;
    const auto& y = b.appearance;
    return a.id == b.id && a.name == b.name && a.cellId == b.cellId && a.position.x == b.position.x &&
           a.position.y == b.position.y && a.facing == b.facing && a.npc == b.npc && a.dead == b.dead &&
           x.species == y.species && x.sex == y.sex && x.stature == y.stature && x.pattern == y.pattern &&
           x.baseColor == y.baseColor && x.gradientColor == y.gradientColor && x.markingColor == y.markingColor &&
           x.gradientAmount == y.gradientAmount && x.patternAmount == y.patternAmount &&
           a.speakingColor == b.speakingColor && a.posture == b.posture && a.state == b.state &&
           a.description == b.description && a.activity == b.activity && a.hearing == b.hearing && a.vision == b.vision &&
           a.earHealth == b.earHealth && a.eyeHealth == b.eyeHealth && a.sneakSkill == b.sneakSkill &&
           a.hearingSkill == b.hearingSkill && a.smell == b.smell && a.noseHealth == b.noseHealth &&
           a.scentSkill == b.scentSkill && a.dexterity == b.dexterity && a.stamina == b.stamina && a.pace == b.pace &&
           a.exhausted == b.exhausted && a.age == b.age && a.ageNoticePending == b.ageNoticePending &&
           a.strength == b.strength && a.wisdom == b.wisdom && a.lastBirthdayDay == b.lastBirthdayDay &&
           a.postureTarget == b.postureTarget && a.postureRemaining == b.postureRemaining;
}

// Greyfen (Data/Worlds/Greyfen) with copies of Sorrel added until it has `residents` residents, as a build in memory.
std::map<std::string, std::string> crowdedGreyfen(int residents)
{
    namespace fs = std::filesystem;
    const fs::path root = fs::path(__FILE__).parent_path().parent_path() / "Data" / "Worlds" / "Greyfen";
    const auto read = [](const fs::path& path) {
        std::ifstream in(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    };
    std::map<std::string, std::string> files{{"world.ratw", read(root / "world.ratw")}};
    for (const auto& entry : fs::directory_iterator(root / "cells"))
        files["cells/" + entry.path().filename().string()] = read(entry.path());
    auto& manifest = files["world.ratw"];
    const auto at = manifest.find("resident \"sorrel\"");
    expect(at != std::string::npos, "Greyfen is where it should be: " + root.string());
    const std::string sorrel = manifest.substr(at, manifest.find('\n', at) - at);
    int authored = 0;                               // Greyfen's own, counted (a smith joined them: doc 33).
    for (std::size_t from = manifest.find("\nresident "); from != std::string::npos; from = manifest.find("\nresident ", from + 1))
        ++authored;
    for (int i = authored; i < residents; ++i)
    {
        std::string copy = sorrel;
        copy.replace(copy.find("\"sorrel\""), 8, "\"extra_" + std::to_string(i) + "\"");
        copy.replace(copy.find("\"Sorrel\""), 8, "\"Extra " + std::to_string(i) + "\"");
        manifest += copy + "\n";
    }
    return files;
}
} // namespace

int main()
{
    try
    {
        World w;
        w.addPlayer("player-ash", "Ash");
        w.setTimeOfDay(9);
        for (int i = 0; i < 1200; ++i)
            w.tick(.5);                                // Ten game minutes: residents at work, bonds forming.
        w.promise("player-ash", "npc_keeper", "come back tomorrow", 2);
        w.bonds().change("npc_keeper", "player-ash", {10, 5, 20, 0, 1}, w.calendarDays());
        w.believe("npc_keeper", "player-ash", "pays well", "saw it", .8);
        // A character unlike the default in every field that is kept, and a sky unlike the one it started with.
        auto& ash = *w.entity("player-ash");
        ash.appearance = {"maned", "female", "tall", "piebald", 6, 4, 2, .2, .8};
        ash.facing = 1.25;
        ash.speakingColor = 9;
        ash.state = "resting by the fire";
        ash.description = "A test wolf with a notched ear.";
        ash.hearing = .8; ash.vision = .9; ash.earHealth = .7; ash.eyeHealth = .95;
        ash.sneakSkill = 12.5; ash.hearingSkill = 30; ash.smell = .85; ash.noseHealth = .9; ash.scentSkill = 44;
        ash.dexterity = 61.5; ash.stamina = 72.25; ash.pace = 4;
        ash.age = 27; ash.ageNoticePending = 1; ash.strength = 55.5; ash.wisdom = 33;
        w.setWeather("exterior", Weather::Fog);
        expect(w.setWind("exterior", 1.25, .4, true).ok && w.setLighting("tavern", .3, .6, "cool").ok, "the sky is set");
        auto saved = w.save();
        const auto server = serverWith(w);
        const auto doc = checkpoint::encode(saved, server, npcsOf(w), w.time());
        const auto text = json::dump(doc);
        json::Value parsed;
        std::string error;
        expect(json::parse(text, parsed, error), "the document parses: " + error);
        PersistedWorld back;
        checkpoint::ServerState serverBack;
        expect(checkpoint::decode(parsed, back, serverBack, error), "and decodes: " + error);
        expect(serverBack.sequence == 42 && serverBack.revision == 7 && serverBack.accounts.string("ash") == "opaque",
               "the server's numbers and the accounts, as they were");
        expect(serverBack.memories.active.size() == 1 && serverBack.memories.summaries[0].sourceEvents.size() == 2,
               "conversations remembered");
        expect(serverBack.social.sessions["s1"].members["player-ash"].lastAudience == std::vector<std::string>{"npc_keeper"},
               "and who was listening");
        expect(serverBack.social.points["player-ash"] == 2, "social points counted from the ledger");
        expect(serverBack.characters.size() == server.characters.size() && back.players.size() == server.characters.size(),
               "every character, and they are the world's players");
        expect(sameCharacter(serverBack.characters.at("player-ash"), server.characters.at("player-ash")) &&
                   sameCharacter(back.players[0], server.characters.at("player-ash")),
               "the character as it was, to its coat and its skills");
        expect(back.clockOffsetHours == saved.clockOffsetHours && back.time == saved.time && back.calendarDays == saved.calendarDays,
               "the same hour of the same day");
        expect(back.weather == saved.weather && back.weather.at("exterior") == Weather::Fog, "the same weather");
        bool sameWinds = back.winds.size() == saved.winds.size(), sameLight = back.lighting.size() == saved.lighting.size();
        for (const auto& [cell, wind] : saved.winds)
            sameWinds = sameWinds && back.winds.count(cell) && back.winds.at(cell).direction == wind.direction &&
                        back.winds.at(cell).strength == wind.strength && back.winds.at(cell).variable == wind.variable;
        for (const auto& [cell, light] : saved.lighting)
            sameLight = sameLight && back.lighting.count(cell) && back.lighting.at(cell).artificial == light.artificial &&
                        back.lighting.at(cell).daylightAccess == light.daylightAccess && back.lighting.at(cell).tone == light.tone;
        expect(sameWinds && back.winds.at("exterior").variable && back.winds.at("exterior").strength == .4, "the same winds");
        expect(sameLight && back.lighting.at("tavern").tone == "cool" && back.lighting.at("tavern").artificial == .3,
               "the same lamps");
        World again;
        const auto restored = again.restore(back);
        expect(restored.ok, "the world restores from it: " + restored.message);
        {
            // A resident redescribed or renamed since the save keeps the new words; the save keeps where they are.
            auto stale = back;
            expect(!stale.npcs.empty(), "the save has residents");
            auto& old = stale.npcs.front();
            old.name = "Old Name";
            old.description = "Prince of somewhere, as the save remembers it.";
            World fresh;
            expect(fresh.restore(stale).ok, "a save with old words restores");
            const auto* now = fresh.entity(old.id);
            expect(now && now->description == w.entity(old.id)->description && now->name == w.entity(old.id)->name &&
                       now->position.x == old.position.x && now->position.y == old.position.y,
                   "who a resident is comes from the world as authored, where they are from the save");
        }
        again.addPlayer("player-ash", "Ash");
        again.entity("player-ash")->cellId = w.entity("player-ash")->cellId;
        again.entity("player-ash")->position = w.entity("player-ash")->position;
        // The same world: its own save encodes to the same document.
        auto redo = checkpoint::encode(again.save(), serverBack, npcsOf(again), w.time());
        auto first = doc;
        expect(json::dump(redo["society"]) == json::dump(first["society"]), "the same society");
        expect(redo["npcs"] == first["npcs"], "the same NPCs, where they were");
        expect(redo["bonds"] == first["bonds"] && redo["promises"] == first["promises"] && redo["beliefs"] == first["beliefs"],
               "the same bonds, promises and rumours");
        expect(redo["activeMemory"] == first["activeMemory"] && redo["socialSessions"] == first["socialSessions"],
               "the same memories and scenes");
        // Refused whole.
        auto bad = parsed;
        bad.set("schema", 2);
        expect(!checkpoint::decode(bad, back, serverBack, error), "a schema it doesn't know");
        bad = parsed;
        bad.set("weather", "rain");
        expect(!checkpoint::decode(bad, back, serverBack, error) && error.find("weather") != std::string::npos, "a weather record that isn't one");
        bad = parsed;
        bad.find("society")->set("minted", -5);
        expect(checkpoint::decode(bad, back, serverBack, error) && !World().restore(back).ok, "a society that doesn't add up");
        // A document from before the clock was saved starts from the noon epoch.
        auto older = parsed;
        older.erase("clockOffsetHours");
        expect(checkpoint::decode(older, back, serverBack, error) && back.clockOffsetHours == 12.0,
               "no clock offset reads as noon: " + error);
        older.set("clockOffsetHours", "nine");
        expect(checkpoint::decode(older, back, serverBack, error) && !World().restore(back).ok, "a clock offset that isn't a number");

        // Map memories are kept compactly (doc 31, Phase 2): a big cell barely seen is a few hundred bytes, and comes back
        // exactly; a save in the old plain form still reads.
        {
            auto withMemory = saved;
            CellMemory m;
            m.cellId = "big";
            m.name = "A Big Place";
            m.knowledge = Knowledge::Visited;
            m.width = m.height = 256;
            m.glyphs.assign(256 * 256, ' ');
            m.observed.assign(256 * 256, false);
            for (int i = 0; i < 300; ++i)
            {
                m.glyphs[std::size_t(1000 + i * 7)] = char('a' + i % 26);
                m.observed[std::size_t(1000 + i * 7)] = true;
            }
            withMemory.memories["player-ash"]["big"] = m;
            const auto compact = checkpoint::encode(withMemory, server, npcsOf(w), w.time());
            const auto* entry = &compact.array("mapMemories").back();
            expect(entry->has("glyphsZ") && json::dump(*entry).size() < 4000,
                   "a barely-seen 256x256 memory is small: " + std::to_string(json::dump(*entry).size()) + " bytes");
            json::Value reread;
            expect(json::parse(json::dump(compact), reread, error), "it parses");
            PersistedWorld memoryBack;
            checkpoint::ServerState memoryServer;
            expect(checkpoint::decode(reread, memoryBack, memoryServer, error), "and decodes: " + error);
            const auto& again = memoryBack.memories["player-ash"]["big"];
            expect(again.glyphs == m.glyphs && again.observed == m.observed && again.name == m.name && again.width == 256,
                   "the same glyphs and tiles seen");
            auto plain = reread;
            auto& list = plain.find("mapMemories")->items();
            auto& old = list.back();
            old.erase("glyphsZ");
            old.erase("observedZ");
            old.set("glyphs", std::string(m.glyphs.begin(), m.glyphs.end()));
            std::string bits;
            for (bool b : m.observed)
                bits += b ? '1' : '0';
            old.set("observed", bits);
            PersistedWorld plainBack;
            expect(checkpoint::decode(plain, plainBack, memoryServer, error) &&
                       plainBack.memories["player-ash"]["big"].glyphs == m.glyphs &&
                       plainBack.memories["player-ash"]["big"].observed == m.observed,
                   "the old plain form still reads: " + error);
            auto broken = reread;
            broken.find("mapMemories")->items().back().set("glyphsZ", "!!!");
            expect(!checkpoint::decode(broken, plainBack, memoryServer, error) && error.find("map memory") != std::string::npos,
                   "a damaged memory is refused, not read as blank");
        }

        // A town of two hundred, through text and back: every resident is there, where they were.
        const auto files = crowdedGreyfen(200);
        World crowd;
        const auto built = crowd.loadWorldFiles(files, "crowded Greyfen");
        expect(built.ok, "a town of two hundred loads: " + built.message);
        expect(crowd.society().state().residents.size() == 200, "with two hundred residents");
        crowd.setTimeOfDay(9);
        for (int i = 0; i < 60; ++i)
            crowd.tick(1);
        const auto crowdDoc = checkpoint::encode(crowd.save(), checkpoint::ServerState{}, npcsOf(crowd), crowd.time());
        json::Value crowdParsed;
        expect(json::parse(json::dump(crowdDoc), crowdParsed, error), "the town's document parses: " + error);
        PersistedWorld crowdBack;
        checkpoint::ServerState crowdServer;
        expect(checkpoint::decode(crowdParsed, crowdBack, crowdServer, error), "and decodes: " + error);
        expect(crowdBack.npcs.size() == 200 && crowdBack.society.residents.size() == 200, "with every resident in it");
        World crowdAgain;
        expect(crowdAgain.loadWorldFiles(files, "crowded Greyfen").ok, "the town loads again");
        const auto crowdRestored = crowdAgain.restore(crowdBack);
        expect(crowdRestored.ok, "and restores from it: " + crowdRestored.message + " " + crowdRestored.targetId);
        expect(crowdAgain.society().state().residents.size() == 200, "two hundred residents still");
        int placed = 0;
        for (const auto& [id, life] : crowd.society().state().residents)
        {
            const auto* was = crowd.entity(id);
            const auto* now = crowdAgain.entity(id);
            const auto* lives = crowdAgain.society().resident(id);
            placed += was && now && lives && now->cellId == was->cellId && now->position.x == was->position.x &&
                      now->position.y == was->position.y && lives->task == life.task && lives->hunger == life.hunger;
        }
        expect(placed == 200, "each where they were, doing what they were (" + std::to_string(placed) + " of 200)");
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Checkpoint tests passed: " << checks << " checks.\n";
    return 0;
}
