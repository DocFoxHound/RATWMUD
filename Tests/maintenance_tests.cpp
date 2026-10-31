// Closing for maintenance (Docs/Design/66-worlds-atlas-and-dm.md, section 4): the players are told and given a grace,
// then the world is saved and the host asked to exit with status 76; nobody connected, at once; called off, play goes
// on. (Hearing it from the database, and refusing to start in it: tools/test_world_sync.py, ServerMaintenanceTests.)
#include "RatwGame.h"

#include <cstdio>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unistd.h>

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

struct Client final : game::Connection
{
    std::vector<std::string> said;
    void event(const std::string& text) override
    {
        json::Value v;
        std::string error;
        if (json::parse(text, v, error) && v.string("type") == "system")
            said.push_back(v.string("text"));
    }
    void snapshot(const std::string&) override {}
    void motion(const json::Value&) override {}
    bool allowsLocalCredentials() const override { return true; }
    bool heard(const std::string& text) const
    {
        for (const auto& s : said)
            if (s.find(text) != std::string::npos)
                return true;
        return false;
    }
};

game::Options options(const std::string& save)
{
    game::Options o;
    o.forkSnapshots = false;
    o.savePath = save;
    return o;
}

void tick(game::Game& g, double seconds)
{
    for (double t = 0; t < seconds && g.exitRequested() < 0; t += .05)
        g.tick(.05);
}

// A save in a folder of its own (the game keeps its lock, portraits and reports beside it), removed afterwards.
std::string tempSave(const char* name)
{
    const auto folder = std::filesystem::temp_directory_path() / (std::string("ratw-maintenance-") + std::to_string(::getpid()) + "-" + name);
    std::filesystem::remove_all(folder);
    std::filesystem::create_directories(folder);
    return (folder / "save.json").string();
}

void removeSave(const std::string& save)
{
    std::filesystem::remove_all(std::filesystem::path(save).parent_path());
}

void playersAreToldAndGivenTheGrace()
{
    const auto save = tempSave("grace");
    {
        game::Game g(options(save));
        std::string problem;
        expect(g.start(problem), "starts: " + problem);
        Client ada;
        ada.id = 1;
        g.connect(&ada);
        g.beginMaintenance("adding a shop");
        expect(g.closingForMaintenance(), "closing");
        expect(ada.heard("The world is closing for maintenance") && ada.heard("adding a shop"), "the player is told why");
        tick(g, game::Game::MaintenanceGrace - 15);
        expect(g.exitRequested() < 0, "still open within the grace");
        expect(!ada.heard("in 10 seconds"), "no reminder yet");
        tick(g, 10);
        expect(ada.heard("in 10 seconds"), "reminded near the end");
        tick(g, 10);
        expect(g.exitRequested() == game::Game::MaintenanceExit, "then the host exits with 76");
        expect(ada.heard("closing for maintenance now"), "told it is closing now");
        expect(std::filesystem::exists(save), "saved");
        g.disconnect(&ada);
    }
    removeSave(save);
}

void nobodyPlayingClosesAtOnce()
{
    const auto save = tempSave("empty");
    {
        game::Game g(options(save));
        std::string problem;
        expect(g.start(problem), "starts: " + problem);
        g.beginMaintenance("");
        tick(g, .1);
        expect(g.exitRequested() == game::Game::MaintenanceExit, "nobody to wait for");
    }
    removeSave(save);
}

void calledOff()
{
    const auto save = tempSave("off");
    {
        game::Game g(options(save));
        std::string problem;
        expect(g.start(problem), "starts: " + problem);
        Client bo;
        bo.id = 1;
        g.connect(&bo);
        g.beginMaintenance("");
        tick(g, 20);
        g.endMaintenance();
        expect(!g.closingForMaintenance() && bo.heard("called off"), "called off, and the player told");
        tick(g, game::Game::MaintenanceGrace);
        expect(g.exitRequested() < 0, "play goes on");
        g.disconnect(&bo);
    }
    removeSave(save);
}
} // namespace

int main()
{
    try
    {
        playersAreToldAndGivenTheGrace();
        nobodyPlayingClosesAtOnce();
        calledOff();
    }
    catch (const std::exception& e)
    {
        std::cerr << "maintenance_tests failed after " << checks << " checks: " << e.what() << '\n';
        return 1;
    }
    std::cout << "maintenance_tests passed (" << checks << " checks)\n";
    return 0;
}
