// The Dev Console: commands for a player marked Dungeon Master (from the Dungeon Master app's Players tab, saved with
// the character as Entity::dungeonMaster), for trying things out where they stand. Each command is answered with a
// {"type": "devResult", "command", "ok", "text"} for the console's log. Nobody else may use it, whatever the page sends.
#include "RatwGame.h"

#include <algorithm>
#include <cctype>

namespace ratw::game
{
namespace
{
struct DevCommand
{
    const char* name;
    const char* help;
};
// What the console offers (the page lists the same in Client/src/ui/hud/dialogs.ts).
constexpr DevCommand DevCommands[] = {
    {"help", "Lists these commands."},
    {"fight-test-1", "A fight where you stand: one weak bandit on the far side of the arena, with a clear way to you."},
    {"fight-end-myself", "Ends the fight you are in, as a draw."},
};
} // namespace

void Game::devCommand(Connection* c, const json::Value& j)
{
    const auto& id = c->entityId;
    std::string command = j.string("command");
    command.erase(command.begin(), std::find_if(command.begin(), command.end(), [](unsigned char ch) { return !std::isspace(ch); }));
    while (!command.empty() && std::isspace(static_cast<unsigned char>(command.back())))
        command.pop_back();
    if (!command.empty() && command.front() == '/')
        command.erase(0, 1);
    std::transform(command.begin(), command.end(), command.begin(), [](unsigned char ch) { return char(std::tolower(ch)); });
    if (command.size() > 60)
        command.resize(60);

    const auto* me = world_.entity(id);
    Result result;
    if (!me || me->npc || !me->dungeonMaster)
        result = {false, "Only a Dungeon Master has the Dev Console.", {}};
    else if (command == "help" || command.empty())
    {
        std::string text = "Commands:";
        for (const auto& d : DevCommands)
            text += std::string("\n/") + d.name + ": " + d.help;
        result = {true, text, {}};
    }
    else if (command == "fight-test-1")
        result = world_.testFight(id);
    else if (command == "fight-end-myself")
        result = world_.endFightInDraw(id);
    else
        result = {false, "No such command: /" + command + ". Try /help.", {}};

    if (me && me->dungeonMaster)
    {
        note("info", "RATW_DEV /" + command + " by " + id + ": " + (result.ok ? "done" : "refused") + ": " + result.message);
        if (result.ok && command != "help" && !command.empty())
        {
            logEvent("operator", id, id, "dev console /" + command);
            record(Character, id);
        }
    }
    auto reply = json::Value::object();
    reply.add("type", "devResult");
    reply.add("command", "/" + command);
    reply.add("ok", result.ok);
    reply.add("text", result.message);
    send(c, reply);
}
} // namespace ratw::game
