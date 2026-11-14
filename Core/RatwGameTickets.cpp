// Ticket mode (Docs/Design/77-central-accounts-and-hosted-worlds.md, sections 5 and 5.1): wolves join this world with a
// ticket from central, checked here offline with central's public keys (RatwTickets.h), and arrive once, choosing what
// today's character_create chose that isn't their identity: where they arrive, who they know (doc 52), and the Gift
// tier and strengths (doc 77, Q12 (a): gated by this world's standing, doc 49). After that a ticket brings them straight
// in. The world agent's files (doc 77, 7) are kept here too: online.json for the heartbeat's honest count, and
// bans.json, central's banned accounts, refused at hello and sent out. Everything here is opt-in (Options::central);
// without it the world keeps its passwords, as ever.
#include "RatwBuildInfo.h"
#include "RatwCreation.h"
#include "RatwGame.h"
#include "RatwGifts.h"
#include "RatwSystemLibs.h"
#include "RatwWire.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unistd.h>

namespace ratw::game
{
using json::Value;

namespace
{
constexpr int MaxHellos = 5;                        // A ticket's tries on one connection.
constexpr double ArrivalWindow = 3600;              // The arrival limits count the last hour.
constexpr double OnlineEvery = 15;                  // online.json is written this often (seconds)...
constexpr double BansEvery = 5;                     // ...and bans.json looked at this often.
constexpr std::uintmax_t MaxBansFile = 8u << 20;    // (Many bans fit: an account's id is 37 bytes.)
constexpr std::size_t MaxUnlocksListed = 1000;      // unlocks.json's wolves (central takes as many in a heartbeat).
const char* const BannedMessage = "This account is banned from central's worlds.";
const char* const WorldBannedMessage = "This account is banned from this world by its host.";

void prune(std::deque<double>& times, double now)
{
    while (!times.empty() && times.front() <= now - ArrivalWindow)
        times.pop_front();
}
} // namespace

double Game::ticketNow() const { return ticketClock ? ticketClock() : now(); }

bool Game::startTickets(std::string& problem)
{
    const auto& o = options_.central;
    std::string error;
    if (!tickets::worldId(o.worldId))
        problem = "Ticket mode (--central) needs this world's id on central (--world-id world-<32 hex>).";
    else if (o.keysFile.empty())
        problem = "Ticket mode (--central) needs central's keys, as the world agent keeps them (--central-keys FILE: tools/world_agent.py keys).";
    else if (options_.devIdentity)
        problem = "Ticket mode refuses --dev-identity: anyone reaching this world through a tunnel or a proxy on this "
                  "machine arrives from loopback (doc 77, 5).";
    else if (o.official && (options_.devTools || options_.hostCheats || options_.openTiers))
        problem = "An official world runs without --dev-tools, --host-cheats and --open-tiers (doc 77, 5; doc 78, 8).";
    else if (o.playerLimit < 1 || o.arrivalsPerHour < 1 || o.accountArrivalsPerHour < 1)
        problem = "Ticket mode needs a player limit and arrival limits of at least 1.";
    else if (o.worldName.empty() || o.worldName.size() > 48 ||
             std::any_of(o.worldName.begin(), o.worldName.end(), [](char ch) { return static_cast<unsigned char>(ch) < 32 || ch == 127; }))
        problem = "A world's name is 1-48 bytes without control characters (--world-name).";
    else if (!sys::ed25519Available(error))
        problem = error;
    if (!problem.empty())
        return false;
    tickets_.configure(o.worldId, o.official, ticketNow());
    std::string trouble;
    if (!reloadCentralKeys(trouble))
        note("warning", "RATW_TICKETS no keys from central yet (" + trouble + "): nobody can join until the world agent fetches them");
    if (std::filesystem::exists(agentFile("bans.json")) && !reloadBans(trouble))
        note("warning", "RATW_BANS central's ban list can't be read yet (" + trouble + ")");
    // (An official world has no host: its agent's host.json says null, and nothing is read from it.)
    if (communityWorld() && std::filesystem::exists(agentFile("host.json")) && !reloadHost(trouble))
        note("warning", "RATW_HOST the world's host can't be read yet (" + trouble + ")");
    if (options_.scratch)
        note("info", "RATW_ONLINE a scratch server writes no online.json: the world agent's directory is the real world's");
    note("info", "RATW_TICKETS ticket mode: world " + o.worldId + " (" + (o.official ? "official" : "community") + "), central " +
                     o.url + ", " + std::to_string(tickets_.keys().size()) + " key(s), protocol " + std::to_string(build::Protocol) +
                     ", client data " + build::clientDataHash());
    return true;
}

bool Game::reloadCentralKeys(std::string& problem)
{
    // The world agent writes the file whole and atomically (tools/world_agent.py); it is read again only when it changed.
    const auto& path = options_.central.keysFile;
    std::error_code failed;
    const auto size = std::filesystem::file_size(path, failed);
    if (failed)
    {
        problem = "no keys file at " + path;
        return false;
    }
    const auto written = std::filesystem::last_write_time(path, failed);
    if (failed || size > 64 * 1024)
    {
        problem = "the keys file can't be read";
        return false;
    }
    const std::string stamp = std::to_string(written.time_since_epoch().count()) + ":" + std::to_string(size);
    if (stamp == keysStamp_ && !tickets_.keys().empty())
        return true;
    std::ifstream in(path, std::ios::binary);
    std::stringstream text;
    text << in.rdbuf();
    Value jwks;
    std::string error;
    if (!json::parse(text.str(), jwks, error) || !jwks.isObject())
    {
        problem = "the keys file isn't JSON (" + error + ")";
        return false;
    }
    auto keys = tickets::readKeys(jwks);
    if (keys.empty())
    {
        problem = "the keys file holds no Ed25519 keys";
        return false;
    }
    std::string kids;
    for (const auto& [kid, key] : keys)
        kids += (kids.empty() ? "" : ", ") + kid;
    tickets_.setKeys(std::move(keys));
    keysStamp_ = stamp;
    keysReadAt_ = clock();
    // Central's clock, as the agent last saw it (doc 77, 5: logged; the leeway is what admits a clock that is off).
    const double offset = jwks.number("offset", 0);
    note(std::fabs(offset) >= 60 ? "warning" : "info",
         "RATW_KEYS central's keys: " + kids + "; this machine's clock is " + std::to_string(int(std::round(offset))) +
             " s from central's" + (std::fabs(offset) >= 60 ? " (tickets allow two minutes: set the clock right)" : ""));
    return true;
}

Value Game::worldStatus() const
{
    // GET /status (doc 77, 5.1): public, small, and nothing the game thread must work for. The server caps it at 2 KB.
    std::size_t players = 0;
    for (const auto* c : clients_)
        players += c->entityId.empty() ? 0 : 1;
    auto o = Value::object();
    o.add("ratw", "world");
    o.add("id", options_.central.worldId);
    o.add("name", options_.central.worldName);
    o.add("kind", options_.central.official ? "official" : "community");
    o.add("protocol", build::Protocol);
    o.add("build", build::id());
    o.add("clientData", build::clientDataHash());
    o.add("players", players);
    o.add("limit", options_.central.playerLimit);
    o.add("voices", !options_.dialogueEndpoint.empty());
    o.add("private", false);                        // (Private worlds, Q9: a password kept on central, later.)
    o.add("cheats", cheatsOn());                    // (Dev tools, or the host's: listed "cheats on", doc 77, 5; doc 78, 8.)
    return o;
}

void Game::forgetTicket(Connection* c)
{
    c->ticketWolf.clear();
    c->ticketIdentity = Value();
    c->ticketQuickened = false;
    if (ticketMode())
        c->accountUsername.clear();                 // (A ticket's account lasts as long as its wolf is here.)
}

void Game::ticketLobby(Connection* c, bool ok, const std::string& message, const std::string& code)
{
    if (!c)
        return;
    if (!c->entityId.empty())
    {
        system(c, message);
        return;
    }
    const std::string& account = c->accountUsername;
    const bool arriving = !account.empty() && !c->ticketWolf.empty() && !accounts_.owns(account, c->ticketWolf);
    auto e = Value::object();
    e.add("type", "lobby");
    e.add("mode", "ticket");
    e.add("stage", arriving ? "arrive" : "ticket");
    e.add("ok", ok);
    if (!code.empty())
        e.add("code", code);
    e.add("protocol", build::Protocol);
    e.add("clientData", build::clientDataHash());
    e.add("world", options_.central.worldId);
    e.add("worldName", options_.central.worldName);
    e.add("kind", options_.central.official ? "official" : "community");
    e.add("localOnly", false);
    e.add("credentialsAllowed", false);
    e.add("message", !message.empty() ? message
                     : arriving       ? "Your wolf's first time here: choose where they arrive and who they know."
                                      : "Join this world with a ticket from central.");
    if (arriving)
    {
        // The arrival (doc 77, 5.1): what today's creator asked that isn't the wolf's identity, which came in the ticket.
        e.add("wolf", c->ticketIdentity);
        auto choices = Value::array();              // (No family: Normal only; a family: Gifted or Quickened.)
        if (c->ticketIdentity.string("family").empty())
            choices.push("normal");
        else
        {
            choices.push("gifted");
            choices.push("quickened");
        }
        e.add("tierChoices", choices);
        e.add("gifts", gifts::creatorCatalog());
        e.add("creation", practice::creationCatalog());
        auto tiers = tiersView(account);
        if (quickenedByCentral(c) && !tiers.object("quickened").boolean("open"))
        {
            auto q = Value::object();                // (Opened for the whole account, on central: doc 49, 9.1.)
            q.add("open", true);
            q.add("by", "account");
            q.add("progress", Value::array());
            tiers.set("quickened", q);
        }
        e.add("tiers", tiers);
        e.add("account", accountView(account));
        e.add("starts", startsView());
        e.add("firstCharacter", accounts_.characters(account).empty());
        auto ties = Value::array();
        for (const auto& s : newcomers::tieRules().starters)
        {
            auto o = Value::object();
            o.add("id", s.id);
            o.add("line", s.newcomer);
            ties.push(o);
        }
        e.add("ties", ties);
        e.add("tieRequired", accounts_.characters(account).empty() && !newcomers::tieRules().starters.empty() && !options_.tiesOptional);
    }
    send(c, e);
}

void Game::ticketHello(Connection* c, const Value& j)
{
    // hello{ticket, protocol} (doc 77, 5): the only way in.
    if (!c->entityId.empty())
    {
        system(c, "Your wolf is in the world already.");
        return;
    }
    for (const auto& [key, v] : j.fields())
        if (key != "type" && key != "ticket" && key != "protocol")
        {
            ticketLobby(c, false, "A hello is a ticket and a protocol, and nothing else.", "invalid");
            return;
        }
    if (++c->hellos > MaxHellos)
    {
        ticketLobby(c, false, "Too many tries on one connection. Connect again with a new ticket.", "rate_limited");
        return;
    }
    if (const auto* p = j.find("protocol"); !p || !p->isNumber() || p->asNumber() != build::Protocol)
    {
        ticketLobby(c, false, "This world speaks protocol " + std::to_string(build::Protocol) +
                                  " and the client another: one of the two needs an update.", "protocol");
        return;
    }
    if (!c->accountUsername.empty())
    {
        ticketLobby(c, false, "This connection has joined already. Leave first.", "joined");
        return;
    }
    if (!storageReady_)
    {
        ticketLobby(c, false, "This world's storage is not healthy: nobody can join just now.", "storage");
        return;
    }
    const auto* given = j.find("ticket");
    const std::string ticket = given && given->isString() ? given->asString() : std::string();
    tickets::Claims claims;
    std::string why;
    auto verdict = tickets_.check(ticket, ticketNow(), claims, why);
    if (verdict == tickets::Verdict::UnknownKey)
    {
        // A key this world doesn't hold: perhaps the world agent has fetched it already; if not, it is asked to (at
        // most once a minute), and the ticket, which is still unused, can be tried again.
        std::string trouble;
        if (reloadCentralKeys(trouble))
            verdict = tickets_.check(ticket, ticketNow(), claims, why);
        if (verdict == tickets::Verdict::UnknownKey)
        {
            if (clock() - keysAskedAt_ >= 60)
            {
                keysAskedAt_ = clock();
                std::ofstream(options_.central.keysFile + ".wanted") << ticketNow() << '\n';
                note("info", "RATW_KEYS a ticket named a key this world doesn't hold: the world agent is asked for central's keys again");
            }
            ticketLobby(c, false, "This world doesn't hold central's newest key yet and is fetching it. Try again in a moment.", "keys");
            return;
        }
    }
    if (verdict != tickets::Verdict::Admitted)
    {
        note("info", "RATW_TICKET refused (" + why + ")");
        ticketLobby(c, false, "That ticket was not accepted (" + why + "). Get a new one from central and try again.", "ticket");
        return;
    }
    if (bannedByCentral(claims.account))
    {
        // Central's account bans (bans.json, from the heartbeat's reply: doc 77, 5.1). A ticket issued just before the
        // ban is refused here; one issued after it never is, since central issues none.
        note("info", "RATW_TICKET refused " + claims.wolf + ": its account is banned by central");
        ticketLobby(c, false, BannedMessage, "banned");
        return;
    }
    if (bannedHere(claims.account))
    {
        // The host's own world ban (doc 77, 3.2): by account, so a fresh wolf of the same account is refused too.
        note("info", "RATW_TICKET refused " + claims.wolf + ": its account is banned from this world by its host");
        ticketLobby(c, false, WorldBannedMessage, "world_banned");
        return;
    }
    if (hostOnly_ && (hostSub_.empty() || claims.account != hostSub_))
    {
        // Open only to the host (doc 78, 10.3): the host is looking at a new map first. With no host known (host.json
        // names none: a bad token), nobody is let in, which is why the Editor greys the choice without a valid token.
        note("info", "RATW_TICKET refused " + claims.wolf + ": the world is open only to its host");
        ticketLobby(c, false, "This world is open only to its host just now: a new map is being looked at. Try again soon.", "host_only");
        return;
    }
    std::size_t inWorld = 0;
    for (const auto* other : clients_)
        inWorld += other->entityId.empty() ? 0 : 1;
    if (inWorld >= std::size_t(options_.central.playerLimit))
    {
        ticketLobby(c, false, "This world is full. Try again later.", "full");
        return;
    }
    const auto owner = accounts_.ownerOf(claims.wolf);
    if ((!owner.empty() && owner != claims.account) || (owner.empty() && characters_.count(claims.wolf)))
    {
        ticketLobby(c, false, "That wolf belongs to another account in this world.", "wolf_elsewhere");
        return;
    }
    c->accountUsername = claims.account;
    c->ticketWolf = claims.wolf;
    auto identity = Value::object();
    identity.add("id", claims.wolf);
    identity.add("name", claims.name);
    identity.add("age", claims.age);
    identity.add("sex", claims.sex);
    identity.add("appearance", claims.look);
    identity.add("family", claims.family);
    c->ticketIdentity = identity;
    c->ticketQuickened = claims.quickened;
    note("info", "RATW_TICKET admitted " + claims.wolf + " (" + claims.kind + ")");
    const auto saved = characters_.find(claims.wolf);
    if (owner.empty() || saved == characters_.end())
    {
        ticketLobby(c, true, "Welcome to " + options_.central.worldName + ". Choose where your wolf arrives, and who they know here.");
        return;
    }
    // Been here before: straight in, the identity refreshed from the ticket (doc 77, 5.1). The look may have changed on
    // central; the sex never does, and the name never changes (residents remember it).
    Appearance look;
    wire::readAppearance(claims.look, look);
    if (look.sex != saved->second.appearance->sex)
    {
        forgetTicket(c);
        ticketLobby(c, false, "That ticket's wolf does not match the wolf this world knows.", "identity");
        return;
    }
    if (claims.name != saved->second.name)
        note("warning", "RATW_TICKET " + claims.wolf + " is named " + claims.name + " on central and " + saved->second.name +
                            " here: the name here stays");
    const Appearance before = *saved->second.appearance;
    saved->second.appearance = look;
    if (!enterCharacter(c, claims.wolf, saved->second.name))
    {
        if (const auto again = characters_.find(claims.wolf); again != characters_.end())
            again->second.appearance = before;
        forgetTicket(c);
        return;
    }
    if (auto* e = world_.entity(claims.wolf))
        e->appearance = look;                       // (A body that lingered in a fight came back as it was.)
}

bool Game::arrive(Connection* c, const Value& j)
{
    // character_arrive{commandId, start?, tie?, tier?, build?} (doc 77, 5.1): the rest of today's character_create, once
    // per world: where the wolf arrives, the tie, the tier (gated by this world's standing) and the strengths; then the
    // purse opens with the welcome (Society::addPlayer), the receipt is kept, and the wolf comes in.
    if (!c->entityId.empty())
    {
        system(c, "Your wolf is in the world already.");
        return true;
    }
    const std::string account = c->accountUsername;
    if (account.empty() || c->ticketWolf.empty())
    {
        ticketLobby(c, false, "Join with a ticket first.", "ticket");
        return true;
    }
    const std::string wolf = c->ticketWolf;
    if (accounts_.owns(account, wolf))
    {
        ticketLobby(c, false, "Your wolf has arrived here already.", "arrived");
        return true;
    }
    for (const auto& [key, v] : j.fields())
        if (key != "type" && key != "commandId" && key != "start" && key != "tie" && key != "tier" && key != "build")
        {
            ticketLobby(c, false, "The arrival contains unsupported fields.", "invalid");
            return true;
        }
    if (!storageReady_)
    {
        ticketLobby(c, false, "This world's storage is not healthy: nobody can arrive just now.", "storage");
        return true;
    }
    const std::string commandId = j.string("commandId");
    if (!accounts::validCommandId(commandId))
    {
        ticketLobby(c, false, "An arrival needs a request ID (commandId).", "invalid");
        return true;
    }
    const auto& identity = c->ticketIdentity;
    const std::string family = identity.string("family");
    // The tier (the user, 2026-11-07): a wolf with no Gift family is Normal, everywhere; a wolf with one arrives Gifted
    // or Quickened, never Normal; Quickened only where this account's standing here has opened it, or its account on
    // central has (the ticket's `quickened`: doc 49, 9.1).
    const auto* tierValue = j.find("tier");
    const std::string tier = !tierValue ? (family.empty() ? std::string("normal") : std::string())
                             : tierValue->isString() ? tierValue->asString() : std::string();
    if (family.empty() && tier != "normal")
    {
        ticketLobby(c, false, "Your wolf has no Gift family, so they arrive as Normal.", "invalid");
        return true;
    }
    if (!family.empty() && tier != "gifted" && tier != "quickened")
    {
        ticketLobby(c, false, "Your wolf has a Gift family: choose Gifted or Quickened.", "invalid");
        return true;
    }
    std::string start, tie;
    practice::Build build;
    if (!arrivalChoices(c, j, tier, start, tie, build))
        return true;
    auto canonical = Value::object();
    canonical.add("wolf", wolf);
    canonical.add("tier", tier);
    auto grades = Value::object();
    for (const auto& [attribute, grade] : build.grades)
        grades.add(attribute, grade);
    canonical.add("grades", grades);
    canonical.add("specialty", build.specialty);
    canonical.add("start", start);
    canonical.add("tie", tie);
    const std::string print = accounts::fingerprint(json::dump(canonical));
    bool conflict = false;
    if (!accounts_.createdCharacter(account, commandId, print, conflict).empty() || conflict)
    {
        ticketLobby(c, false, "That arrival request ID was already used.", "invalid");
        return true;
    }
    if (accounts_.characters(account).size() >= accounts::CharacterSlots)
    {
        ticketLobby(c, false, "This account has brought six wolves here already.", "full");
        return true;
    }
    // The arrival limits (doc 77, 5.1): each first arrival costs the world an economy account and a welcome, and a
    // world holds at most MaxPlayerAccounts player accounts; past that it would fail every arrival, so it says so.
    const double at = ticketNow();
    prune(arrivals_, at);
    auto& mine = accountArrivals_[account];
    prune(mine, at);
    if (arrivals_.size() >= std::size_t(options_.central.arrivalsPerHour))
    {
        ticketLobby(c, false, "Many new wolves have arrived in this world in the last hour. Try again later.", "arrivals");
        return true;
    }
    if (mine.size() >= std::size_t(options_.central.accountArrivalsPerHour))
    {
        ticketLobby(c, false, "Your account has brought several new wolves here in the last hour. Try again later.", "arrivals");
        return true;
    }
    std::size_t playerAccounts = 0;
    for (const auto& [id, held] : world_.society().state().accounts)
        playerAccounts += id.rfind("wolf-", 0) == 0 || id.rfind("player-", 0) == 0 ? 1 : 0;
    if (playerAccounts >= MaxPlayerAccounts)
    {
        note("warning", "RATW_ARRIVAL refused: this world holds " + std::to_string(playerAccounts) + " player economy accounts, its most");
        ticketLobby(c, false, "This world has room for no more new wolves.", "world_full");
        return true;
    }
    const bool newAccount = !accounts_.exists(account);
    if (!accounts_.addCentral(account))
    {
        ticketLobby(c, false, "This world cannot take another account.", "world_full");
        return true;
    }
    NewWolf w;
    w.id = wolf;
    w.name = identity.string("name");
    w.age = identity.number("age");
    wire::readAppearance(identity["appearance"], w.appearance);
    w.tier = tier;
    w.family = family;
    w.build = build;
    w.start = start.empty() ? suggestedStart() : start;
    w.tie = tie;
    w.commandId = commandId;
    w.print = print;
    if (!makeWolf(c, w))
    {
        if (newAccount && accounts_.characters(account).empty())
            accounts_.removeAccount(account);
        return true;
    }
    arrivals_.push_back(at);
    mine.push_back(at);
    note("info", "RATW_ARRIVAL " + wolf + " arrived for the first time (" + tier + ")");
    if (!enterCharacter(c, wolf, w.name))
        forgetTicket(c);                            // (Made and kept: a new ticket brings them in.)
    return true;
}
// ------------------------------------------------------------------------------------------------ The agent's files

std::filesystem::path Game::agentFile(const std::string& name) const
{
    // The world agent's directory is the one that holds central's keys (tools/world_agent.py run --dir DIR).
    auto dir = std::filesystem::path(options_.central.keysFile).parent_path();
    return (dir.empty() ? std::filesystem::path(".") : dir) / name;
}

void Game::ticketFiles()
{
    if (!ticketMode())
        return;
    std::string trouble;
    const double at = ticketFilesClock ? ticketFilesClock() : clock();
    if (!options_.scratch && at - onlineWrittenAt_ >= OnlineEvery)
    {
        onlineWrittenAt_ = at;
        const bool wrote = writeOnline(trouble);
        if (wrote == onlineFailing_)                // (Said once when it starts failing, and once when it mends.)
            note(wrote ? "info" : "warning", wrote ? "RATW_ONLINE online.json is written again"
                                                   : "RATW_ONLINE online.json can't be written (" + trouble +
                                                         "): central will count nobody here");
        onlineFailing_ = !wrote;
        if (options_.central.official)
        {
            const bool fine = writeUnlocks(trouble);
            if (fine == unlocksFailing_)            // (Said once when it starts failing, and once when it mends.)
                note(fine ? "info" : "warning", fine ? "RATW_UNLOCKS unlocks.json is written again"
                                                     : "RATW_UNLOCKS unlocks.json can't be written (" + trouble +
                                                           "): central hears of no new Quickened accounts");
            unlocksFailing_ = !fine;
        }
    }
    if (at - bansCheckedAt_ >= BansEvery)
    {
        bansCheckedAt_ = at;
        reloadBans(trouble);                        // (No file yet is no bans yet; a bad one keeps the last list.)
        if (communityWorld())
            reloadHost(trouble);                    // (Likewise the host; an official world has none.)
    }
}

bool Game::writeOnline(std::string& problem)
{
    // online.json (doc 77, 7): the wolves in the world now, for the heartbeat's honest count. Written whole to a
    // temporary file of this process's own (another server on the same directory never shares it) and renamed over
    // the last, so the agent never reads half of one. A scratch server writes nothing: beside the real world, with
    // the same --central-keys, it would count its own players over the real world's.
    if (!ticketMode())
    {
        problem = "not in ticket mode";
        return false;
    }
    if (options_.scratch)
    {
        problem = "a scratch server writes nothing";
        return false;
    }
    std::vector<std::string> wolves;
    for (const auto* c : clients_)
        if (!c->entityId.empty())
            wolves.push_back(c->entityId);
    std::sort(wolves.begin(), wolves.end());
    wolves.erase(std::unique(wolves.begin(), wolves.end()), wolves.end());
    auto online = Value::array();
    for (const auto& wolf : wolves)
        online.push(wolf);
    auto o = Value::object();
    o.add("world", options_.central.worldId);
    o.add("at", std::floor(ticketNow()));
    o.add("players", wolves.size());
    o.add("online", online);
    return writeAgentFile("online.json", o, problem);
}

bool Game::writeAgentFile(const std::string& name, const Value& doc, std::string& problem)
{
    const auto path = agentFile(name);
    const auto temp = agentFile("." + name + "." + std::to_string(::getpid()) + ".new");
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        out << json::dump(doc) << '\n';
        out.close();
        if (!out)
        {
            std::error_code ignored;
            std::filesystem::remove(temp, ignored);
            problem = "can't write " + temp.string();
            return false;
        }
    }
    std::error_code failed;
    std::filesystem::rename(temp, path, failed);
    if (failed)
    {
        std::filesystem::remove(temp, failed);
        problem = "can't replace " + path.string();
        return false;
    }
    return true;
}

bool Game::writeUnlocks(std::string& problem)
{
    // unlocks.json (doc 49, 9.1): the wolves of every account whose standing here has opened Quickened (earned, or a
    // Dungeon Master's grant; not while the account is held), the latest first and at most MaxUnlocksListed, for the
    // agent's heartbeat. Central records Quickened on those accounts, for every world, from official worlds only (a
    // community host could write anything here), so a community world writes none. Written only when it changed.
    if (!ticketMode() || !options_.central.official)
    {
        problem = ticketMode() ? "a community world's unlocks stay its own" : "not in ticket mode";
        return false;
    }
    if (options_.scratch)
    {
        problem = "a scratch server writes nothing";
        return false;
    }
    std::vector<std::pair<double, std::string>> opened;
    for (const auto& [account, r] : standing_)
        if (!r.hold && r.quickenedAt >= 0)
            for (const auto& wolf : accounts_.characters(account))
                if (tickets::wolfId(wolf))
                    opened.emplace_back(r.quickenedAt, wolf);
    std::sort(opened.begin(), opened.end(), [](const auto& a, const auto& b) {
        return a.first != b.first ? a.first > b.first : a.second < b.second;
    });
    auto list = Value::array();
    for (std::size_t i = 0; i < opened.size() && i < MaxUnlocksListed; ++i)
        list.push(opened[i].second);
    const std::string listed = json::dump(list);
    if (listed == unlocksWritten_)
        return true;
    auto o = Value::object();
    o.add("world", options_.central.worldId);
    o.add("at", std::floor(ticketNow()));
    o.add("quickened", list);
    if (!writeAgentFile("unlocks.json", o, problem))
        return false;
    unlocksWritten_ = listed;
    return true;
}

bool Game::quickenedByCentral(const Connection* c) const
{
    if (!c || !ticketMode() || !c->ticketQuickened || c->accountUsername.empty())
        return false;
    const auto record = standing_.find(c->accountUsername);
    return record == standing_.end() || !record->second.hold;
}

bool Game::reloadBans(std::string& problem)
{
    // bans.json (doc 77, 5.1): {"at": unix seconds, "accounts": [the tickets' sub of every banned account]}, written by
    // the world agent from each heartbeat's reply. Read again only when it changed; a missing or unreadable file
    // leaves the list as it was (as the keys are kept), and a new list sends out any banned wolf here now.
    if (!ticketMode())
    {
        problem = "not in ticket mode";
        return false;
    }
    const auto path = agentFile("bans.json");
    std::error_code failed;
    const auto size = std::filesystem::file_size(path, failed);
    if (failed)
    {
        problem = "no bans file at " + path.string();
        return false;
    }
    const auto written = std::filesystem::last_write_time(path, failed);
    if (failed || size > MaxBansFile)
    {
        problem = "the bans file can't be read";
        return false;
    }
    const std::string stamp = std::to_string(written.time_since_epoch().count()) + ":" + std::to_string(size);
    if (stamp == bansStamp_)
        return true;
    std::ifstream in(path, std::ios::binary);
    std::stringstream text;
    text << in.rdbuf();
    Value doc;
    std::string error;
    if (!json::parse(text.str(), doc, error) || !doc.isObject())
    {
        problem = "the bans file isn't JSON (" + error + ")";
        return false;
    }
    const auto* accounts = doc.find("accounts");
    if (!accounts || !accounts->isArray())
    {
        problem = "the bans file has no list of accounts";
        return false;
    }
    std::set<std::string> bans;
    for (const auto& a : accounts->items())
    {
        if (!a.isString() || a.asString().empty() || a.asString().size() > 64)
        {
            problem = "the bans file names something that isn't an account";
            return false;
        }
        bans.insert(a.asString());
    }
    bansStamp_ = stamp;
    if (bans != centralBans_)
        note("info", "RATW_BANS central's ban list: " + std::to_string(bans.size()) + " account(s)");
    centralBans_ = std::move(bans);
    sendOutBanned();
    return true;
}

void Game::sendOutBanned()
{
    // A wolf already here when its account's ban arrives is sent out: told so, taken out of the world as on leaving,
    // and its connection closed. A connection that has shown a ticket but not yet arrived is let go the same way.
    const std::vector<Connection*> everyone = clients_;
    for (auto* c : everyone)
    {
        if (c->accountUsername.empty() || !bannedByCentral(c->accountUsername))
            continue;
        const std::string wolf = !c->entityId.empty() ? c->entityId : c->ticketWolf;
        sendOut(c, std::string(BannedMessage) + " Your wolf has left this world.", "banned");
        note("info", "RATW_BANS sent out " + wolf + ": its account is banned by central");
    }
}

void Game::sendOut(Connection* c, const std::string& message, const std::string& code)
{
    // Told why, taken out of the world as on leaving, its ticket's account forgotten, and the connection closed. It
    // may come back with a new ticket unless it is banned.
    leaveCharacter(c);
    forgetTicket(c);                                // (Its account goes with it: a ticket's lasts as long as its wolf.)
    ticketLobby(c, false, message, code);
    c->closeAfterSending();
}

bool Game::bannedHere(const std::string& account) const
{
    const auto it = people_.find(account);
    return it != people_.end() && it->second.worldBan.on;
}

bool Game::isHost(const Connection* c) const
{
    return c && communityWorld() && !hostSub_.empty() && c->accountUsername == hostSub_ && !c->entityId.empty();
}

std::string Game::deciders(bool capital) const
{
    if (communityWorld())
        return capital ? "The host" : "the host";
    return capital ? "The Dungeon Masters" : "the Dungeon Masters";
}

bool Game::hostsOwnWolf(const std::string& characterId) const
{
    const auto& host = guardedHost();
    return communityWorld() && !host.empty() && !characterId.empty() && accounts_.ownerOf(characterId) == host;
}

bool Game::hostsOwnChapter(const std::string& chapterId) const
{
    if (const auto* chapter = chapters_.byId(chapterId))
        for (const auto& [member, rank] : chapter->members)
            if (hostsOwnWolf(member))
                return true;
    return false;
}

std::string Game::actorWords(bool capital) const
{
    if (communityWorld())
        return capital ? "The host" : "the host";
    return capital ? "A Dungeon Master" : "a Dungeon Master";
}

void Game::tellActedOn(const std::string& characterId, const std::string& words)
{
    if (auto* c = clientOf(characterId))
    {
        system(c, words);
        return;
    }
    if (!communityWorld())
        return;                                     // (The user's own worlds: as they were, told only while in.)
    // Kept with the wolf: on the world's copy while it lingers there, else on its saved one; the oldest dropped past six.
    Entity* e = world_.entity(characterId);
    if (!e || e->npc)
    {
        const auto saved = characters_.find(characterId);
        e = saved == characters_.end() ? nullptr : &saved->second;
    }
    if (!e)
        return;
    if (e->hostNotes.size() >= 6)
        e->hostNotes.erase(e->hostNotes.begin());
    e->hostNotes.push_back(json::utf8Prefix(words, 300));
    saveSoon();
}

std::string Game::dmActRefused(const std::string& kind, const std::string& target, const std::string& payloadText) const
{
    // Doc 78, 8 (Phase M1): what the host's tools may do on a community world, enforced here whatever the tool sent.
    if (!communityWorld())
        return {};
    if (kind == "character.dm" && !options_.devTools)
        return "The Dev Console is off on community worlds: only cheats = \"everyone\" in world.toml gives it (the world is then listed "
               "\"cheats on\").";
    if ((kind == "character.gift" || kind == "account.unlock") && !cheatsOn())
        return "Gifts and Gift tiers by hand are cheats on a community world: set cheats = \"host\" in world.toml (the world is then "
               "listed \"cheats on\").";
    if (cheatsOn() || guardedHost().empty())        // (host.json naming none keeps the last host guarded: R1 review, 8.)
        return {};
    const std::string refused = "That is one of the host's own wolves: the host's tools reward them only with cheats on (cheats = "
                                "\"host\" in world.toml; the world is then listed \"cheats on\").";
    json::Value payload;
    std::string problem;
    if (!payloadText.empty())
        json::parse(payloadText, payload, problem);
    if (!payload.isObject())
        payload = json::Value::object();
    if ((kind == "character.resurrect" || kind == "deed.award" || kind == "storyline.give" || kind == "mentor.restore") &&
        hostsOwnWolf(target))
        return refused;
    if (kind == "character.grant" && payload.number("coins", 0) > 0 && hostsOwnWolf(target))
        return refused;
    if (kind == "milestone.credit")
        for (const auto& p : payload.array("people"))
            if (p.isObject() && hostsOwnWolf(p.string("id")))
                return "Credit names " + p.string("id") + ", one of the host's own wolves: the host's tools reward them only with cheats on "
                       "(cheats = \"host\" in world.toml).";
    // A heal or a move of the host's own wolf (doc 78, R1 review): an injury added, or a kill, is no reward and stays.
    if (kind == "character.move" && hostsOwnWolf(target))
        return refused;
    if (kind == "character.injury" && !payload.string("remove").empty() && hostsOwnWolf(target))
        return refused;
    if (kind == "storyteller.decide" && payload.boolean("approve") && target == guardedHost())
        return "That is the host's own storyteller application: the host's tools approve it only with cheats on (cheats = \"host\" in "
               "world.toml).";
    if (kind == "house.decide" && payload.boolean("approve") && hostsOwnChapter(target))
        return "That Chapter is one of the host's own wolves': the host's tools recognise it as a House only with cheats on (cheats = "
               "\"host\" in world.toml).";
    if (kind == "treaty.decide" && payload.boolean("approve"))
        for (const auto& t : factions_.treaties())
            if (t.id == target && hostsOwnChapter(t.chapter))
                return "That treaty is with a Chapter of the host's own wolves: the host's tools agree it only with cheats on (cheats = "
                       "\"host\" in world.toml).";
    if (kind == "book.storyline" && !payload.string("storyline").empty())
        if (const auto book = books_.find(payload.string("book")); book != books_.end())
        {
            bool own = hostsOwnWolf(book->second.keeper);
            for (const auto& w : book->second.wolves)
                own = own || hostsOwnWolf(w);
            if (own)
                return "That book is one of the host's own wolves': the host's tools tie it to a storyline only with cheats on (cheats = "
                       "\"host\" in world.toml).";
        }
    if (kind == "estate.set")
    {
        // (A place the host's wolf holds, by its Chapter's lease or as a lodger of the whole place: its rent is theirs.)
        const auto* lease = estates_.lease(target);
        if (lease && hostsOwnChapter(lease->chapter))
            return "That place is let to a Chapter of the host's own wolves: the host's tools set its terms only with cheats on "
                   "(cheats = \"host\" in world.toml).";
        if (const auto* lodging = wholeLodgingAt(target); lodging && hostsOwnWolf(lodging->holder))
            return "That place is let to one of the host's own wolves: the host's tools set its terms only with cheats on (cheats = "
                   "\"host\" in world.toml).";
        // (Its rent paid to the host's own wolf; or a vacant place a Chapter the host's wolf leads could take at once, at
        // the terms the host just chose: R1 review. Individuals' lets are in doc 78, section 8.)
        if (hostsOwnWolf(payload.string("landlord")))
            return "That names one of the host's own wolves as the landlord: the host's tools do that only with cheats on (cheats = "
                   "\"host\" in world.toml).";
        const int level = std::clamp(int(wire::number(payload, "level", 2)), 2, 5);
        if (!lease)
            for (const auto& [id, chapter] : chapters_.all())
                for (const auto& [member, m] : chapter.members)
                    if (m.rank <= chapter::RankOfficer && chapter.level >= level && hostsOwnWolf(member))
                        return "A Chapter the host's own wolf leads could take that place at once: the host's tools set a vacant place's "
                               "terms only with cheats on (cheats = \"host\" in world.toml), or at a level above that Chapter's.";
    }
    return {};
}

bool Game::reloadHost(std::string& problem)
{
    // host.json (doc 77, 3.2): {"at", "host": the host account's ticket sub here, or null}, from the agent. Read again
    // only when it changed; a missing or unreadable file keeps the host as it was. An official world reads nothing:
    // it has no host, whatever a file says.
    if (!communityWorld())
    {
        problem = ticketMode() ? "an official world has no host" : "not in ticket mode";
        return false;
    }
    const auto path = agentFile("host.json");
    std::error_code failed;
    const auto size = std::filesystem::file_size(path, failed);
    if (failed)
    {
        problem = "no host file at " + path.string();
        return false;
    }
    const auto written = std::filesystem::last_write_time(path, failed);
    if (failed || size > 4096)
    {
        problem = "the host file can't be read";
        return false;
    }
    const std::string stamp = std::to_string(written.time_since_epoch().count()) + ":" + std::to_string(size);
    if (stamp == hostStamp_)
        return true;
    std::ifstream in(path, std::ios::binary);
    std::stringstream text;
    text << in.rdbuf();
    Value doc;
    std::string error;
    if (!json::parse(text.str(), doc, error) || !doc.isObject())
    {
        problem = "the host file isn't JSON (" + error + ")";
        return false;
    }
    const auto* host = doc.find("host");
    if (!host || !(host->isNull() || (host->isString() && !host->asString().empty() && host->asString().size() <= 64)))
    {
        problem = "the host file names no account (\"host\": an account's sub, or null)";
        return false;
    }
    hostStamp_ = stamp;
    const std::string was = hostSub_;
    hostSub_ = host->isString() ? host->asString() : std::string();
    if (!hostSub_.empty() && hostSub_ != lastHostSub_)
    {
        lastHostSub_ = hostSub_;                    // (Guarded while host.json names none: dmActRefused.)
        saveSoon();
    }
    if (hostSub_ != was)
    {
        note("info", hostSub_.empty() ? std::string("RATW_HOST this world has no host now") : "RATW_HOST this world's host: account " + hostSub_);
        for (auto* c : clients_)
            if (!c->entityId.empty() && (c->accountUsername == was || c->accountUsername == hostSub_))
                system(c, c->accountUsername == hostSub_ ? "You are this world's host: the host's console is yours (the ` key)."
                                                         : "You are no longer this world's host.");
    }
    return true;
}
} // namespace ratw::game
