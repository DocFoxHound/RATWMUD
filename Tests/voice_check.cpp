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
        // What a model would be told of them, roughly as the game tells it (for the review's model answers).
        const auto* e = g.world().entity(npc);
        j.add("description", e->description);
        j.add("activity", e->activity);
        if (const auto* spec = g.world().society().spec(npc))
        {
            j.add("personality", spec->personality);
            j.add("backstory", spec->backstory);
        }
        if (const auto* c = g.world().cell(e->cellId))
            j.add("scene", c->description);
        out.push(j);
    }
    std::cout << json::dump(out) << "\n";
    std::remove(o.savePath.c_str());
    return 0;
}
