// The game's own answers to sample lines, as JSON, for the review page (tools/ai_review.py; doc 28):
//   voice_check VOICE_DIR SAMPLES.json [WORLD_FILE]
// Each sample is {"npc": ID, "say": TEXT}; each answer {"npc", "say", "route": "game"|"model", "answer"}.
#include "RatwGame.h"

#include <cstdio>
#include <fstream>
#include <unistd.h>
#include <iostream>
#include <sstream>

using namespace ratw;

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        std::cerr << "usage: voice_check VOICE_DIR SAMPLES.json [WORLD_FILE]\n";
        return 2;
    }
    game::Options o;
    o.voiceData = argv[1];
    if (argc > 3)
        o.worldFile = argv[3];
    o.savePath = "/tmp/ratw-voice-check-" + std::to_string(::getpid()) + ".json";
    game::Game g(o);
    std::string problem;
    if (!g.start(problem))
    {
        std::cerr << "the world did not start: " << problem << "\n";
        return 1;
    }
    std::ifstream in(argv[2]);
    std::stringstream text;
    text << in.rdbuf();
    json::Value samples;
    if (!json::parse(text.str(), samples, problem))
    {
        std::cerr << "samples: " << problem << "\n";
        return 1;
    }
    g.world().addPlayer("player-reviewer", "Reviewer");
    auto out = json::Value::array();
    for (const auto& s : samples.array("samples"))
    {
        const auto npc = s.string("npc");
        if (!g.world().entity(npc))
            continue;
        const auto answer = g.gameAnswer(npc, "player-reviewer", s.string("say"), true);
        auto j = json::Value::object();
        j.add("npc", npc);
        j.add("name", g.world().entity(npc)->name);
        j.add("say", s.string("say"));
        j.add("route", answer.empty() ? "model" : "game");
        j.add("answer", answer);
        // Exactly what an NPC Mind would be told for this reply, as the game builds it in play.
        const auto c = g.dialogueContext(npc, "player-reviewer", s.string("say"), true);
        auto context = json::Value::object();
        for (const auto& [k, v] : std::initializer_list<std::pair<const char*, const std::string*>>{
                 {"npc", &c.name}, {"player", &c.playerName}, {"description", &c.description}, {"activity", &c.activity},
                 {"heard", &c.heardText}, {"memory", &c.memory}, {"scene", &c.scene}, {"personality", &c.personality},
                 {"backstory", &c.backstory}, {"npcId", &c.npcId}, {"subjectId", &c.subjectId},
                 {"relationship", &c.relationship}, {"mood", &c.mood}})
            context.add(k, *v);
        j.add("context", context);
        out.push(j);
    }
    std::cout << json::dump(out) << "\n";
    std::remove(o.savePath.c_str());
    return 0;
}
