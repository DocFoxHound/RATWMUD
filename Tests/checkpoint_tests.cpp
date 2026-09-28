// The portable checkpoint (RatwCheckpoint.h): a running world's document reads back to the same world, through text,
// and a malformed one is refused rather than half-read.
#include "RatwCheckpoint.h"

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
        World again;
        const auto restored = again.restore(back);
        expect(restored.ok, "the world restores from it: " + restored.message);
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
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Checkpoint tests passed: " << checks << " checks.\n";
    return 0;
}
