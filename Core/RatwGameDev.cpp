// The Dev Console: commands for a player marked Dungeon Master (from the Dungeon Master app's Players tab, saved with
// the character as Entity::dungeonMaster), for trying things out where they stand. Each command is answered with a
// {"type": "devResult", "command", "ok", "text"} for the console's log; {"type": "devCommands"} asks for the list the
// console suggests from as one types ({"type": "devCommands", "commands": [[name, help], ...]}). Nobody else may use
// it, whatever the page sends.
//
// The host's commands (Docs/Design/77-central-accounts-and-hosted-worlds.md, 3.2), below devCommand: on a community
// world in ticket mode, every wolf of the host's account (host.json, from central's heartbeat reply) has the same
// console with its own table: what waits on a decision (treaties, Houses, storytellers), and a world-local kick, ban
// and silence. Never the Dev Console's cheats. Each use is logged.
#include "RatwGame.h"
#include "RatwItems.h"
#include "RatwPeople.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <sstream>

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
    {"hunt", "hunt <animal> [animal...]: a hunt where you stand with just that game (ids: rabbit, hare, roe_deer, red_deer, boar...; doc 53)."},
    {"give", "give <item id> [count]: wearables from the catalog into your own purse (doc 35), to try on."},
    {"wearables", "wearables [word]: the catalog's wearables, by id (those whose id or name has the word)."},
    {"reckon", "The week's reckoning now (doc 42): every resident pays a tenth of its profit since the last to its town and its church."},
    {"speed", "speed [N]: how fast the world runs, 1 (as ever) to 1000 times; nothing is skipped, it just runs faster. Without N, the speed now."},
    {"store", "store <keeper> [item count]: a shop keeper's till: its wares, what it holds and each shelf's target (doc 79, 7.7); with an item and a count, adds them to its till. Dev worlds only."},
};

// A shop's till by hand (doc 79, 17: the store room's draw is a raw add into the till now): its wares and shelves, or an add.
Result storeCommand(Society& society, const std::string& rest)
{
    std::istringstream words(rest);
    std::string keeper, item, extra;
    int count = 0;
    words >> keeper >> item;
    const bool add = !item.empty();
    if (add && !(words >> count))
        return {false, "Usage: /store <keeper> [item count].", {}};
    if (keeper.empty() || (words >> extra))
        return {false, "Usage: /store <keeper> [item count].", {}};
    if (!society.merchant(keeper))
        return {false, keeper + " keeps no shop.", {}};
    const auto till = society.tillOf(keeper);
    const auto* held = society.account(till);
    if (!held)
        return {false, keeper + "'s shop has no till.", {}};
    if (add)
    {
        if (count < 1 || count > 1000 || !itemValid(item))
            return {false, "Add 1 to 1000 of a good the catalogue knows.", {}};
        int left = count;
        for (; left > 0; left -= std::min(left, 99))
            if (!society.create(till, item, std::min(left, 99), "dev console: added to the till"))
                break;
        return {left <= 0, std::to_string(count - std::max(0, left)) + " " + item + " added to " + till + ": " +
                               std::to_string(Society::stock(*society.account(till), item)) + " there now.", {}};
    }
    std::string text = keeper + "'s till " + till + ": " + std::to_string(held->cash) + "p.";
    for (const auto& ware : society.wares(keeper))
        text += "\n" + ware + ": " + std::to_string(Society::stock(*held, ware)) + " held, shelf " + std::to_string(society.shelfTarget(till, ware));
    return {true, text, {}};
}
} // namespace

namespace
{
// The host's table (doc 77, 3.2). A wolf is named by its name here or its wolf- id.
constexpr DevCommand HostCommands[] = {
    {"help", "Lists these commands."},
    {"pending", "What waits on you: treaties, House requests and storyteller applications."},
    {"treaty", "treaty <treaty-N> yes|no: agrees or turns down a treaty (one not decided in a game day settles by the faction's own rule)."},
    {"house", "house <chapter-N> <faction> yes|no: recognises a Chapter as a minor House, or not (likewise settles itself after a game day)."},
    {"storyteller", "storyteller <wolf> yes|no|revoke [reason]: decides an application to tell stories, or takes the standing back."},
    {"kick", "kick <wolf>: sends the wolf out of this world; it may come back."},
    {"ban", "ban <wolf> [reason]: bans the wolf's account from this world (any of its wolves), and sends it out."},
    {"unban", "unban <wolf>: lifts this world's ban on the wolf's account."},
    {"silence", "silence <wolf> 1|6|24|72: the wolf's account can't speak in world chat, or send private messages or letters, here for that many hours."},
    {"unsilence", "unsilence <wolf>: lifts the silence."},
};

std::string lower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) { return char(std::tolower(ch)); });
    return text;
}

std::string trimmed(std::string text)
{
    text.erase(text.begin(), std::find_if(text.begin(), text.end(), [](unsigned char ch) { return !std::isspace(ch); }));
    while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())))
        text.pop_back();
    return text;
}
} // namespace

void Game::devCommand(Connection* c, const json::Value& j)
{
    const auto& id = c->entityId;
    if (const auto* who = world_.entity(id); isHost(c) && !(who && who->dungeonMaster && devConsoleAllowed()))
    {
        hostCommand(c, j);                          // (A community world's host: the host's table, never the cheats.)
        return;
    }
    std::string command = j.string("command");
    command.erase(command.begin(), std::find_if(command.begin(), command.end(), [](unsigned char ch) { return !std::isspace(ch); }));
    while (!command.empty() && std::isspace(static_cast<unsigned char>(command.back())))
        command.pop_back();
    if (!command.empty() && command.front() == '/')
        command.erase(0, 1);
    std::transform(command.begin(), command.end(), command.begin(), [](unsigned char ch) { return char(std::tolower(ch)); });
    if (command.size() > (command.rfind("store ", 0) == 0 ? 120u : 60u))
        command.resize(command.rfind("store ", 0) == 0 ? 120 : 60);   // (A keeper's ID and an item's take room.)

    const auto* me = world_.entity(id);
    if (j.string("type") == "devCommands")
    {
        auto list = json::Value::array();
        if (me && !me->npc && me->dungeonMaster && devConsoleAllowed())
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
    if (!me || me->npc || !me->dungeonMaster || !devConsoleAllowed())
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
    else if (command == "hunt" || command.rfind("hunt ", 0) == 0)
    {
        // Doc 53: try the game's new ways on a chosen animal or two, out in the wild.
        std::vector<std::string> game;
        std::istringstream words(command.size() > 5 ? command.substr(5) : std::string());
        for (std::string w; words >> w && game.size() < 6;)
            game.push_back(w);
        result = game.empty() ? Result{false, "Name the game: /hunt roe_deer, or /hunt rabbit hare.", {}} : world_.startHunt(id, game);
    }
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
    else if (command == "speed" || command.rfind("speed ", 0) == 0)
    {
        if (command.size() > 6)
        {
            const double asked = std::atof(command.c_str() + 6);
            if (command.substr(6) == "max")
                setSpeedMax(true);
            else if (!(asked >= 1 && asked <= MaxSpeed))
                result = {false, "A speed from 1 to " + std::to_string(int(MaxSpeed)) + ", or max.", {}};
            else
                setSpeed(asked);
        }
        if (result.message.empty())
        {
            std::ostringstream said;
            if (speedMax_)
                said << "The world runs at max: as fast as the machine goes while nobody is connected, " << int(MaxSpeed)
                     << "x while anyone is (now " << int(std::round(speed_)) << "x).";
            else
                said << "The world runs at " << speed_ << "x" << (speed_ > 1 ? " (a game day in " + std::to_string(int(std::round(240 / speed_))) + " real minutes, if the machine keeps up)." : ", as ever.");
            result = {true, said.str(), {}};
        }
    }
    else if (command == "store" || command.rfind("store ", 0) == 0)
        result = options_.worldKind != "dev" ? Result{false, "/store (adding goods to a till) is for dev worlds only (this world's kind: " +
                                                                 (options_.worldKind.empty() ? std::string("unknown") : options_.worldKind) + ").", {}}
                                             : storeCommand(world_.society(), command.size() > 6 ? command.substr(6) : std::string());
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

// ------------------------------------------------------------------------------------------------ The host's commands

std::string Game::hostTarget(const std::string& rest, std::string& wolf, std::string& why) const
{
    // A wolf this world knows, by its wolf- id or its name (central's names are unique; the longest name that begins
    // the text wins, so "Ash Tallow yes" finds Ash Tallow). Returns what follows it, trimmed.
    wolf.clear();
    const auto text = trimmed(rest);
    const auto first = text.substr(0, text.find(' '));
    if (lower(first).rfind("wolf-", 0) == 0)
    {
        if (!characters_.count(lower(first)))
        {
            why = "No wolf " + first + " in this world.";
            return {};
        }
        wolf = lower(first);
        return trimmed(text.substr(first.size()));
    }
    const auto low = lower(text);
    std::size_t best = 0;
    int ties = 0;
    for (const auto& [cid, e] : characters_)
    {
        const auto name = lower(e.name);
        if (name.empty() || name.size() < best || low.compare(0, name.size(), name) != 0 ||
            (low.size() > name.size() && low[name.size()] != ' '))
            continue;
        ties = name.size() == best ? ties + 1 : 1;
        best = name.size();
        wolf = cid;
    }
    if (wolf.empty())
    {
        why = text.empty() ? "Name a wolf (its name here, or its wolf- id)." : "No wolf in this world is named like that.";
        return {};
    }
    if (ties > 1)
    {
        wolf.clear();
        why = "More than one wolf here has that name: use its wolf- id.";
        return {};
    }
    return trimmed(text.substr(best));
}

void Game::hostCommand(Connection* c, const json::Value& j)
{
    const std::string id = c->entityId;
    // At most 300 characters, as the client's console takes (a wolf's id, a name and a reason fit): cut on a character,
    // never inside one, and nothing that isn't UTF-8, since a ban's or a storyteller's reason is saved from it.
    std::string line = people::clean(j.string("command"), 301);
    if (!line.empty() && line.front() == '/')
        line.erase(0, 1);
    line = people::clean(line, 300);
    const auto space = line.find(' ');
    const std::string command = lower(line.substr(0, space));
    const std::string rest = space == std::string::npos ? std::string() : trimmed(line.substr(space + 1));
    const bool host = isHost(c);
    if (j.string("type") == "devCommands")
    {
        auto list = json::Value::array();
        if (host)
            for (const auto& d : HostCommands)
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
    std::string target;                             // (The wolf a command was about, for the log.)
    // A wolf's account, as long as it isn't the host's own (the host can't kick, ban or silence itself).
    const auto targetAccount = [&](std::string& tail) -> std::string {
        std::string why;
        tail = hostTarget(rest, target, why);
        if (target.empty())
        {
            result = {false, why, {}};
            return {};
        }
        const auto account = accounts_.ownerOf(target);
        if (account.empty())
            result = {false, "That wolf has no account here.", {}};
        else if (account == hostSub_)
            result = {false, "That is your own account: the host can't do that to itself.", {}};
        return result.message.empty() ? account : std::string();
    };
    const auto nameOf = [&](const std::string& wolf) {
        const auto it = characters_.find(wolf);
        return it != characters_.end() ? it->second.name : wolf;
    };
    const auto yesNo = [](const std::string& word) { return word == "yes" ? 1 : word == "no" ? 0 : -1; };
    if (!host)
        result = {false, "Only this world's host has these commands.", {}};
    else if (command == "help" || command.empty())
    {
        std::string text = "The host's commands:";
        for (const auto& d : HostCommands)
            text += std::string("\n/") + d.name + ": " + d.help;
        result = {true, text, {}};
    }
    else if (command == "pending")
    {
        std::string text;
        const auto chapterName = [&](const std::string& cid) {
            const auto* ch = chapters_.byId(cid);
            return cid + (ch ? " (" + ch->name + ")" : std::string());
        };
        const auto factionName = [&](const std::string& fid) {
            const auto* f = factions_.find(fid);
            return fid + (f ? " (" + f->name + ")" : std::string());
        };
        for (const auto& t : factions_.treaties())
            if (t.state == "pending")
                text += "\nTreaty " + t.id + ": " + chapterName(t.chapter) + " with " + factionName(t.faction) + ": " +
                        (t.build ? "building rights, " : "") + std::to_string(t.tithe) + " pennies a week" +
                        (t.levy ? ", answering levies" : "") + (t.labour ? ", its people free to join the Hold" : "") + ", " +
                        std::to_string(t.weeks) + " weeks.";
        for (const auto& h : factions_.houseRequests())
            if (h.state == "pending")
                text += "\nHouse: " + chapterName(h.chapter) + " asks " + factionName(h.faction) + " to recognise it.";
        for (const auto& [account, st] : storytellers_)
            if (st.state == "applied")
                text += "\nStoryteller: " + nameOf(st.character) + " (" + st.character + "): \"" + st.note + "\"";
        result = {true, text.empty() ? "Nothing waits on you." : "Waiting on you:" + text +
                                                                  "\n(Treaties and House requests not decided within a game day settle by the faction's own rule.)", {}};
    }
    else if (command == "treaty")
    {
        std::istringstream words(lower(rest));
        std::string treaty, answer, extra;
        words >> treaty >> answer;
        if (treaty.empty() || yesNo(answer) < 0 || (words >> extra))
            result = {false, "Usage: /treaty <treaty-N> yes|no.", {}};
        else
            result = decideTreaty(treaty, yesNo(answer) == 1, "by the world's host");
    }
    else if (command == "house")
    {
        std::istringstream words(lower(rest));
        std::string chapter, faction, answer, extra;
        words >> chapter >> faction >> answer;
        if (chapter.empty() || faction.empty() || yesNo(answer) < 0 || (words >> extra))
            result = {false, "Usage: /house <chapter-N> <faction> yes|no.", {}};
        else
            result = decideHouse(chapter, faction, yesNo(answer) == 1, "by the world's host");
    }
    else if (command == "storyteller")
    {
        std::string why;
        const auto tail = hostTarget(rest, target, why);
        const auto answer = lower(tail.substr(0, tail.find(' ')));
        const auto reason = people::clean(trimmed(tail.substr(answer.size())), 200);
        const auto account = target.empty() ? std::string() : accounts_.ownerOf(target);
        const auto st = storytellers_.find(account);
        if (target.empty())
            result = {false, why, {}};
        else if (account.empty())
            result = {false, "That wolf has no account here.", {}};
        else if (answer != "yes" && answer != "no" && answer != "revoke")
            result = {false, "Usage: /storyteller <wolf> yes|no|revoke [reason].", {}};
        else if (answer == "revoke")
        {
            if (st == storytellers_.end() || st->second.state != "approved")
                result = {false, nameOf(target) + "'s account doesn't tell stories here.", {}};
            else
            {
                revokeStoryteller(account, "host", reason);
                result = {true, nameOf(target) + "'s account no longer tells stories here.", {}};
            }
        }
        else if (st == storytellers_.end() || st->second.state != "applied")
            result = {false, nameOf(target) + "'s account has no application waiting.", {}};
        else
        {
            decideStoryteller(account, answer == "yes", "host", reason);
            result = {true, nameOf(target) + (answer == "yes" ? " tells stories here now." : "'s application is refused."), {}};
        }
    }
    else if (command == "kick")
    {
        std::string tail;
        const auto account = targetAccount(tail);
        if (!account.empty())
        {
            int sent = 0;
            const std::vector<Connection*> everyone = clients_;
            for (auto* other : everyone)
                if (other->accountUsername == account && (other->entityId == target || other->ticketWolf == target))
                {
                    sendOut(other, "The host has sent your wolf out of this world. You may come back.", "kicked");
                    ++sent;
                }
            result = sent ? Result{true, nameOf(target) + " is sent out of this world.", {}}
                          : Result{false, nameOf(target) + " isn't in this world now.", {}};
        }
    }
    else if (command == "ban")
    {
        std::string tail;
        const auto account = targetAccount(tail);
        if (!account.empty())
        {
            auto& person = personOf(account);
            const bool already = person.worldBan.on;
            person.worldBan.on = true;
            person.worldBan.at = now();
            person.worldBan.reason = people::clean(tail, 200);
            int sent = 0;
            const std::vector<Connection*> everyone = clients_;
            for (auto* other : everyone)
                if (other->accountUsername == account)
                {
                    sendOut(other, std::string("This account is banned from this world by its host") +
                                       (person.worldBan.reason.empty() ? "." : ": " + person.worldBan.reason + ".") +
                                       " Your wolf has left this world.",
                            "world_banned");
                    ++sent;
                }
            saveSoon();
            result = {true, nameOf(target) + "'s account is " + (already ? "still" : "now") + " banned from this world" +
                                (sent ? " and sent out." : ".") + " Any of its wolves is refused here until /unban.", {}};
        }
    }
    else if (command == "unban")
    {
        std::string tail;
        const auto account = targetAccount(tail);
        if (!account.empty())
        {
            if (!bannedHere(account))
                result = {false, nameOf(target) + "'s account isn't banned here.", {}};
            else
            {
                personOf(account).worldBan = {};
                saveSoon();
                result = {true, nameOf(target) + "'s account may come to this world again.", {}};
            }
        }
    }
    else if (command == "silence" || command == "unsilence")
    {
        std::string tail;
        const auto account = targetAccount(tail);
        const int hours = command == "silence" ? std::atoi(tail.c_str()) : 0;
        if (account.empty())
        {
        }                                           // (Why is in result already.)
        else if (command == "silence" && ((hours != 1 && hours != 6 && hours != 24 && hours != 72) || tail != std::to_string(hours)))
            result = {false, "Usage: /silence <wolf> 1|6|24|72 (hours).", {}};
        else
        {
            auto& person = personOf(account);
            if (command == "unsilence" && person.silencedUntil <= now())
                result = {false, nameOf(target) + "'s account isn't silenced.", {}};
            else
            {
                person.silencedUntil = command == "silence" ? now() + hours * 3600.0 : -1;
                for (const auto& cid : accounts_.characters(account))
                    if (auto* other = clientOf(cid))
                        system(other, command == "silence" ? "The host has silenced your account in this world for " + std::to_string(hours) +
                                                                 (hours == 1 ? " hour" : " hours") +
                                                                 ": you can't speak, or send private messages or letters, here until then."
                                                           : "The host has lifted your silence.");
                saveSoon();
                result = {true, command == "silence" ? nameOf(target) + "'s account is silenced for " + std::to_string(hours) + (hours == 1 ? " hour." : " hours.")
                                                     : nameOf(target) + "'s account may speak again.", {}};
            }
        }
    }
    else
        result = {false, "No such command: /" + command + ". Try /help.", {}};

    // Every use is logged (doc 77, 3.2): the server's log, and the event log for what it changed.
    note("info", "RATW_HOST /" + line + " by " + (id.empty() ? std::string("?") : id) + " (account " + c->accountUsername + "): " +
                     (result.ok ? "done" : "refused") + ": " + people::clean(result.message, 200));
    if (host && result.ok && command != "help" && command != "pending" && !command.empty())
        logEvent("operator", id, target, "host /" + command + (rest.empty() ? "" : " " + people::clean(rest, 120)));
    auto reply = json::Value::object();
    reply.add("type", "devResult");
    reply.add("command", "/" + line);
    reply.add("ok", result.ok);
    reply.add("text", result.message);
    send(c, reply);
}
} // namespace ratw::game
