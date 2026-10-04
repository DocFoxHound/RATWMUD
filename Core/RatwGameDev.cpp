// The Dev Console: commands for a player marked Dungeon Master (from the Dungeon Master app's Players tab, saved with
// the character as Entity::dungeonMaster), for trying things out where they stand. Each command is answered with a
// {"type": "devResult", "command", "ok", "text"} for the console's log; {"type": "devCommands"} asks for the list the
// console suggests from as one types ({"type": "devCommands", "commands": [[name, help], ...]}). Nobody else may use
// it, whatever the page sends.
#include "RatwGame.h"
#include "RatwItems.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace ratw::game
{
namespace
{
struct DevCommand
{
    const char* name;
    const char* help;
};
// What the console offers: add a command here and in devCommand() below; the console learns of it by itself.
constexpr DevCommand DevCommands[] = {
    {"help", "Lists these commands."},
    {"fight-test-1", "A fight where you stand: one weak bandit on the far side of the arena, with a clear way to you."},
    {"fight-test-team-1", "A fight where you stand: you and two allies (passers-by, made up) against three weak bandits."},
    {"fight-end-myself", "Ends the fight you are in, as a draw."},
    {"give", "give <item id> [count]: wearables from the catalog into your own purse (doc 35), to try on."},
    {"wearables", "wearables [word]: the catalog's wearables, by id (those whose id or name has the word)."},
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
    if (j.string("type") == "devCommands")
    {
        auto list = json::Value::array();
        if (me && !me->npc && me->dungeonMaster)
            for (const auto& d : DevCommands)
            {
                auto row = json::Value::array();
                row.push(std::string("/") + d.name);
                row.push(d.help);
                list.push(std::move(row));
            }
        auto reply = json::Value::object();
        reply.add("type", "devCommands");
        reply.add("commands", std::move(list));
        send(c, reply);
        return;
    }
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
    else if (command == "fight-test-team-1")
        result = world_.testFightTeam(id);
    else if (command == "fight-end-myself")
        result = world_.endFightInDraw(id);
    else if (command.rfind("give ", 0) == 0)
    {
        std::string item;
        int count = 1;
        {
            const auto rest = command.substr(5);
            const auto space = rest.find(' ');
            item = rest.substr(0, space);
            if (space != std::string::npos)
                count = std::clamp(std::atoi(rest.c_str() + space + 1), 1, 20);
        }
        const auto* piece = items::wearable(item);
        result = !piece ? Result{false, "No wearable " + item + ". Try /wearables.", {}}
                 : world_.society().create(id, item, count, "dev console")
                     ? Result{true, "Given " + std::to_string(count) + " " + piece->name + ". Wear it from your status.", {}}
                     : Result{false, "Your purse can't take it.", {}};
    }
    else if (command == "wearables" || command.rfind("wearables ", 0) == 0)
    {
        const auto word = command.size() > 10 ? command.substr(10) : std::string();
        std::string text;
        int shown = 0;
        for (const auto& piece : items::wearables())
        {
            std::string name = piece.name;
            std::transform(name.begin(), name.end(), name.begin(), [](unsigned char ch) { return char(std::tolower(ch)); });
            if (!word.empty() && piece.id.find(word) == std::string::npos && name.find(word) == std::string::npos)
                continue;
            if (++shown > 60)
            {
                text += "\n...";
                break;
            }
            text += "\n" + piece.id + " (" + piece.slot + "): " + piece.name;
        }
        result = {shown > 0, shown > 0 ? "Wearables:" + text : "No wearable matches.", {}};
    }
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
