// The server's parts (Phase 6): accounts, delta snapshots between a server
// and a client, the NPC Mind client against a small local stand-in, the operator bridge's request contract, and the web side
// (HTTP requests, WebSocket handshakes and frames, which files may be served).
#include "RatwAccountsCore.h"
#include "RatwDirector.h"
#include "RatwMind.h"
#include "RatwMotionCore.h"
#include "RatwSections.h"
#include "RatwSystemLibs.h"
#include "RatwWeb.h"
#include "RatwWire.h"

#include <arpa/inet.h>
#include <atomic>
#include <cmath>
#include <fstream>
#include <functional>
#include <set>
#include <iostream>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <thread>
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

void accountsTests()
{
    std::string error;
    expect(sys::cryptoAvailable(error), "libcrypto loads: " + error);
    // Known answer: PBKDF2-HMAC-SHA256("password", "salt", 1 iteration, 32 bytes) (RFC 7914 test vectors).
    std::uint8_t out[32];
    const std::string salt = "salt";
    expect(sys::pbkdf2Sha256("password", reinterpret_cast<const std::uint8_t*>(salt.data()), salt.size(), 1, out, 32) &&
               sys::hex(out, 32) == "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b",
           "PBKDF2 matches the published vector");
    expect(accounts::fingerprint("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "SHA-256 too");
    accounts::Accounts a;
    expect(!a.registerAccount("x", "short", error), "a bad username or password is refused");
    expect(a.registerAccount("Ash_Wolf", "correct horse battery", error), "an account registers: " + error);
    expect(a.exists("ash_wolf") && !a.exists("Ash_Wolf"), "under its normalised name");
    expect(a.authenticate("ASH_wolf", "correct horse battery") && !a.authenticate("ash_wolf", "wrong horse battery!"),
           "the password, and only the password, lets them in");
    expect(!a.authenticate("nobody_here", "correct horse battery"), "nor anyone else");
    const std::string wolf = "wolf-0123456789abcdef0123456789abcdef";
    const auto print = accounts::fingerprint("{\"name\":\"Ash\"}");
    expect(a.addCharacter("ash_wolf", wolf, "create-1", print) && a.owns("ash_wolf", wolf), "a character is theirs");
    bool conflict = false;
    expect(a.createdCharacter("ash_wolf", "create-1", print, conflict) == wolf && !conflict, "a repeated request finds it");
    a.createdCharacter("ash_wolf", "create-1", accounts::fingerprint("other"), conflict);
    expect(conflict, "the same request ID for other choices is a conflict");
    // Saved and read back, as the checkpoint keeps it.
    accounts::Accounts b;
    expect(b.restore(a.state()) && b.authenticate("ash_wolf", "correct horse battery") && b.owns("ash_wolf", wolf),
           "accounts survive a save");
    expect(b.referencesOnly({wolf}) && !b.referencesOnly({}) && !b.referencesOnly({wolf, "wolf-ffffffffffffffffffffffffffffffff"}),
           "every owned character exists and every generated one has an owner");
    auto bad = a.state();
    bad.find("entries")->items()[0].set("iterations", 1000);
    expect(!b.restore(bad), "a weakened verifier is refused");
    expect(accounts::isLoopbackAddress("127.0.0.1") && accounts::isLoopbackAddress("::ffff:127.0.0.9") &&
               !accounts::isLoopbackAddress("10.0.0.1") && !accounts::isLoopbackAddress("127.0.0.01"),
           "only loopback peers count as local");
    accounts::RateLimit limit;
    int allowed = 0;
    for (int i = 0; i < 10; ++i)
        allowed += limit.allow("peer", 100 + i);
    expect(allowed == 6 && limit.allow("peer", 170), "six tries a minute per peer, then more after it");
}

json::Value parsed(const std::string& text);

// The input rules, exactly.
void accountRulesTests()
{
    std::string normalized;
    expect(accounts::normalizeUsername("Ash_Wolf-9", normalized) && normalized == "ash_wolf-9", "a username is lowercased, nothing else");
    for (const std::string bad : {std::string("ab"), std::string("1wolf"), std::string(" ash"), std::string("ash wolf"),
                                  std::string("ash@wolf"), std::string("w\xc3\xb6lf"), std::string("Ash\n"), std::string(33, 'a')})
        expect(!accounts::normalizeUsername(bad, normalized), "not a username: " + bad);
    expect(accounts::validPassword("correct horse battery staple, and then some more words") &&
               accounts::validPassword(std::string(128, 'p')),
           "a long passphrase, up to 128 bytes");
    for (const std::string bad : {std::string("short"), std::string("a long password\twith a tab"), std::string("a long password\nsplit"),
                                  std::string(129, 'p')})
        expect(!accounts::validPassword(bad), "not a password: " + bad.substr(0, 20));
    expect(accounts::validDisplayName("\xc3\x81sta Lj\xc3\xb3sd\xc3\xb3ttir") && accounts::validDisplayName("Ash"), "Unicode names are names");
    expect(!accounts::validDisplayName("Ash\nWolf") && !accounts::validDisplayName("A"), "a newline, or one letter, is not");
    expect(!accounts::validDisplayName(" Ash") && !accounts::validDisplayName("Ash "), "an untrimmed name is refused, not trimmed");
    expect(accounts::validCommandId("create-1") && accounts::validCommandId("A_b-9") && accounts::validCommandId(std::string(128, 'c')),
           "safe request IDs");
    for (const std::string bad : {std::string(), std::string("../x"), std::string("a/b"), std::string("a.b"),
                                  std::string("a b"), std::string("a\\b"), std::string(129, 'c')})
        expect(!accounts::validCommandId(bad), "not a request ID: " + bad.substr(0, 20));
    for (const std::string local : {"127.2.3.4", "::1", "[::1]"})
        expect(accounts::isLoopbackAddress(local), "loopback: " + local);
    for (const std::string remote : {"", "localhost", "0.0.0.0", "127.0.0.1.evil", "127.0.0.1:7787", "127.0.0.256", "::ffff:192.168.1.2"})
        expect(!accounts::isLoopbackAddress(remote), "not loopback: " + remote);
}

std::string wolfId(int n)
{
    std::string hex = "0123456789abcdef0123456789abcde" + std::string(1, "0123456789abcdef"[n % 16]);
    return "wolf-" + hex;
}

void accountOwnershipTests()
{
    std::string error;
    const std::string password = "the same long passphrase";
    accounts::Accounts a;
    expect(a.registerAccount("ash_wolf", password, error) && a.registerAccount("bram_wolf", password, error), "two accounts: " + error);
    error.clear();
    expect(!a.registerAccount("ASH_WOLF", "another long passphrase", error) && !error.empty(),
           "a name that normalises to an existing one is refused");
    expect(a.authenticate("ash_wolf", password), "and the first keeps its password");
    const auto state = a.state();
    const auto& entries = state.array("entries");
    expect(entries.size() == 2 && entries[0].number("iterations") == 600000 && entries[1].number("iterations") == 600000 &&
               accounts::PasswordIterations == 600000,
           "600000 PBKDF2 iterations are recorded");
    expect(entries[0].string("salt") != entries[1].string("salt") && entries[0].string("verifier") != entries[1].string("verifier"),
           "the same password gets a different salt and verifier");
    expect(json::dump(state).find(password) == std::string::npos && json::dump(state).find("password") == std::string::npos,
           "no password anywhere in what is saved");

    const std::string wolf = wolfId(0), print = accounts::fingerprint("{\"name\":\"Ash\"}");
    expect(a.addCharacter("ash_wolf", wolf, "create-1", print), "ash makes a character");
    expect(!a.addCharacter("bram_wolf", wolf, "create-1", print) && !a.owns("bram_wolf", wolf), "another account cannot take it");
    expect(!a.owns("bram_wolf", wolfId(15)) && !a.owns("bram_wolf", "player-ash"), "nor own one it guesses");
    expect(!a.addCharacter("bram_wolf", "player-ash", "dev-1", print) && !a.owns("bram_wolf", "player-ash"),
           "the development character cannot be claimed");
    for (int i = 1; i <= 6; ++i)
        expect(a.addCharacter("bram_wolf", wolfId(i), "create-" + std::to_string(i), print), "slot " + std::to_string(i));
    expect(!a.addCharacter("bram_wolf", wolfId(7), "create-7", print) && a.characters("bram_wolf").size() == accounts::CharacterSlots &&
               accounts::CharacterSlots == 6,
           "six character slots, the seventh refused");
    bool conflict = false;
    expect(a.createdCharacter("ash_wolf", "create-1", accounts::fingerprint("other choices"), conflict).empty() && conflict,
           "a conflicting replay finds nothing");
    expect(a.createdCharacter("bram_wolf", "create-1", print, conflict) == wolfId(1) && !conflict, "each account's receipts are its own");

    std::set<std::string> ids{wolf, "player-ash"};
    for (int i = 1; i <= 6; ++i)
        ids.insert(wolfId(i));
    expect(a.referencesOnly(ids), "a legacy development character may be unowned");
    auto withBad = ids;
    withBad.insert("wolf-invalid");
    expect(!a.referencesOnly(withBad), "a malformed ID in the generated namespace fails closed");
    accounts::Accounts legacy;
    expect(legacy.referencesOnly({"player-ash", "player-bram"}), "a legacy save has no accounts and needs none");
    expect(legacy.restore(parsed(R"({"version":1,"entries":[]})")) && !legacy.exists("ash_wolf"), "and no accounts restore as none");

    accounts::Accounts b;
    expect(b.restore(a.state()) && b.owns("ash_wolf", wolf) && b.characters("bram_wolf").size() == 6, "restored");
    const auto broken = [&](const std::function<void(json::Value&)>& change) {
        auto s = a.state();
        change(s);
        return b.restore(s);
    };
    expect(!broken([](json::Value& s) { s.find("entries")->items()[0].set("salt", std::string(64, 'z')); }), "a salt must be hexadecimal");
    expect(!broken([](json::Value& s) { s.find("entries")->items()[0].set("salt", "00"); }), "and 32 bytes");
    expect(!broken([&](json::Value& s) { s.find("entries")->items()[0].add("password", password); }), "an unknown password field is refused");
    expect(!broken([&](json::Value& s) {
               auto& bram = s.find("entries")->items()[1];
               bram.set("characters", json::Value(json::Array{json::Value(wolf)}));
               auto receipt = json::Value::object(), creations = json::Value::object();
               receipt.add("character", wolf);
               receipt.add("fingerprint", print);
               creations.add("create-1", receipt);
               bram.set("creations", creations);
           }),
           "one character owned by two accounts is refused");
    expect(b.owns("ash_wolf", wolf) && !b.owns("bram_wolf", wolf) && b.characters("bram_wolf").size() == 6 &&
               b.authenticate("bram_wolf", password),
           "a failed restore leaves what was there");

    accounts::RateLimit limit;
    int allowed = 0;
    for (int peer = 0; peer < 5; ++peer)
        for (int i = 0; i < 6; ++i)
            allowed += limit.allow("peer" + std::to_string(peer), 100 + (peer * 6 + i) * .1);
    expect(allowed == 24, "24 tries a minute across every peer, whoever they are (" + std::to_string(allowed) + ")");
    for (int peer = 0; peer < 5; ++peer)
        limit.forget("peer" + std::to_string(peer));
    expect(!limit.allow("peer0", 104) && !limit.allow("fresh", 104), "disconnecting does not refill the shared budget");
    expect(limit.allow("peer0", 164), "a minute does");
}

json::Value snapshot(const std::string& seen, int revealed)
{
    auto root = json::Value::object(), cell = json::Value::object(), map = json::Value::array();
    cell.add("id", "yard");
    cell.add("rows", json::Value(json::Array{json::Value("...."), json::Value("..~.")}));
    root.add("cell", cell);
    root.add("visibility", json::Value(json::Array{json::Value(seen)}));
    for (int i = 0; i < 5; ++i)
    {
        auto entry = json::Value::object();
        entry.add("id", "c" + std::to_string(i));
        entry.add("glyphs", i == revealed ? std::string("~~~~") : std::string("...") + char('a' + i));
        map.push(entry);
    }
    root.add("worldMap", map);
    root.add("revision", 3);
    return root;
}

void sectionsTests()
{
    sections::Held held;
    sections::Cache client;
    auto first = snapshot("2", -1);
    held.sending(1, sections::strip(first, held.known));
    expect(first.has("worldMap") && first.object("cell").has("rows"), "the first goes whole");
    expect(sections::fill(first, client) && first == snapshot("2", -1), "and reads as it was");
    held.acknowledged(1, false);
    auto second = snapshot("2", -1);
    held.sending(2, sections::strip(second, held.known));
    expect(!second.has("worldMap") && !second.object("cell").has("rows"), "what is held is left out");
    expect(sections::fill(second, client) && second == snapshot("2", -1), "and put back");
    held.acknowledged(2, false);
    auto reveal = snapshot("1", 3);
    held.sending(3, sections::strip(reveal, held.known));
    int references = 0;
    for (const auto& e : reveal.array("worldMap"))
        references += e.has("$held");
    expect(references == 4, "a reveal sends one map entry whole, the rest by key");
    expect(sections::fill(reveal, client) && reveal == snapshot("1", 3), "put back exactly");
    sections::Cache fresh;
    auto again = snapshot("1", 3);
    held.acknowledged(3, false);
    sections::strip(again, held.known);
    expect(!sections::fill(again, fresh), "a client without its copies notices");
    held.acknowledged(4, true);
    expect(held.known.empty(), "and asking for everything forgets what it held");
}

// What the wolf can see, sent as row edits as it walks (Docs/Design/29-client-polish.md).
json::Value seeing(const std::vector<std::string>& rows)
{
    auto root = json::Value::object(), cell = json::Value::object(), visibility = json::Value::array();
    cell.add("id", "yard");
    root.add("cell", cell);
    for (const auto& r : rows)
        visibility.push(json::Value(r));
    root.add("visibility", visibility);
    return root;
}

void visibilityDeltaTests()
{
    const std::string blank(64, '0');
    std::vector<std::string> rows(40, blank);
    rows[10] = std::string(20, '0') + std::string(10, '2') + std::string(34, '0');
    sections::Held held;
    sections::Cache client;
    auto first = seeing(rows);
    held.sending(1, sections::strip(first, held.known, &held.bases));
    expect(first.array("visibility").size() == 40, "the first view goes whole");
    expect(sections::fill(first, client), "and is kept");
    held.acknowledged(1, false);
    auto walked = rows;
    walked[10][19] = '2';
    walked[10][29] = '1';
    walked[11][20] = '2';
    auto second = seeing(walked);
    held.sending(2, sections::strip(second, held.known, &held.bases));
    const auto& sent = second["visibility"];
    expect(sent.isObject() && sent.has("$delta") && sent.array("edits").size() == 2, "a step sends only the rows that changed");
    expect(sections::fill(second, client) && second == seeing(walked), "and the client rebuilds it exactly");
    // An unacknowledged base: the next change is still against what the client is known to hold.
    auto further = walked;
    further[12][5] = '2';
    auto third = seeing(further);
    held.sending(3, sections::strip(third, held.known, &held.bases));
    expect(third["visibility"].has("$delta") && sections::fill(third, client) && third == seeing(further), "edits stack on a held view");
    sections::Cache fresh;
    held.acknowledged(3, false);
    auto last = further;
    last[0][0] = '2';
    auto fourth = seeing(last);
    sections::strip(fourth, held.known, &held.bases);
    expect(!sections::fill(fourth, fresh), "a client without the base asks for everything");
    // A view changing almost everywhere goes whole.
    std::vector<std::string> all(40, std::string(64, '2'));
    auto fifth = seeing(all);
    sections::strip(fifth, held.known, &held.bases);
    expect(fifth["visibility"].isArray(), "a wholesale change goes whole");
    json::Value edits, out;
    expect(!sections::applyRowDelta(seeing(rows)["visibility"], json::Value(json::Array{json::Value(json::Array{json::Value(99), json::Value(0),
        json::Value("2")})}), out), "edits outside the view are refused");
}

// A snapshot with every part a client can hold.
json::Value fullSnapshot(const std::string& seen, const std::string& row)
{
    auto root = json::Value::object(), cell = json::Value::object(), map = json::Value::array(), travel = json::Value::array();
    cell.add("id", "yard");
    cell.add("rows", json::Value(json::Array{json::Value(row), json::Value("..~.")}));
    cell.add("heights", json::Value(json::Array{json::Value("PPPP"), json::Value("PPQP")}));
    root.add("cell", cell);
    root.add("visibility", json::Value(json::Array{json::Value(seen)}));
    for (int i = 0; i < 40; ++i)
    {
        auto entry = json::Value::object();
        entry.add("id", "m" + std::to_string(i));
        entry.add("glyphs", std::string(64, char('a' + i % 26)));
        map.push(entry);
    }
    root.add("worldMap", map);
    for (int i = 0; i < 2; ++i)
    {
        auto entry = json::Value::object();
        entry.add("id", "t" + std::to_string(i));
        entry.add("name", "Road " + std::to_string(i));
        travel.push(entry);
    }
    root.add("travelMap", travel);
    auto door = json::Value::object();
    door.add("id", "gate");
    door.add("open", false);
    root.add("doors", json::Value(json::Array{door}));
    auto item = json::Value::object();
    item.add("item", "meal");
    item.add("quantity", 2);
    root.add("inventory", json::Value(json::Array{item}));
    root.add("revision", 3);
    return root;
}

void sectionKeysTests()
{
    const char* names[] = {"visibility", "cell.rows", "cell.heights", "worldMap", "travelMap", "doors", "inventory"};
    sections::Held held;
    sections::Cache client;
    auto first = fullSnapshot("2", "....");
    const auto keys = sections::strip(first, held.known);
    for (const std::string name : names)
        expect(keys.count(name) && first.object("sectionKeys").has(name), "every section gets a key: " + name);
    for (const std::string entry : {"travelMap#t0", "travelMap#t1", "worldMap#m0", "worldMap#m39"})
        expect(keys.count(entry) && first.object("sectionKeys").has(entry), "and every map entry: " + entry);
    held.sending(1, keys);
    const auto whole = json::dump(first).size();
    auto second = fullSnapshot("2", "....");
    held.sending(2, sections::strip(second, held.known));
    expect(second.has("visibility") && second.has("worldMap") && second.has("travelMap") && second.has("doors") && second.has("inventory") &&
               second.object("cell").has("rows") && second.object("cell").has("heights"),
           "nothing is left out before the client's first acknowledgement");
    expect(sections::fill(first, client) && first == fullSnapshot("2", "...."), "the first reads whole");
    expect(sections::fill(second, client), "and the second");
    held.acknowledged(1, false);
    auto looked = fullSnapshot("3", "....");
    const auto lookedKeys = sections::strip(looked, held.known);
    expect(looked.has("visibility"), "a changed view is sent again");
    expect(!looked.has("worldMap") && !looked.has("travelMap") && !looked.has("doors") && !looked.has("inventory") &&
               !looked.object("cell").has("rows") && !looked.object("cell").has("heights"),
           "but not the maps, doors, inventory or terrain");
    expect(json::dump(looked).size() * 5 < whole,
           "a held delta is much smaller (" + std::to_string(json::dump(looked).size()) + " of " + std::to_string(whole) + " bytes)");
    expect(sections::fill(looked, client) && looked == fullSnapshot("3", "...."), "and reads as the whole");
    held.sending(3, lookedKeys);
    held.acknowledged(3, false);
    auto dug = fullSnapshot("3", "#...");
    sections::strip(dug, held.known);
    expect(dug.object("cell").has("rows") && !dug.object("cell").has("heights"), "changed rows under a stale key are sent again");
    auto stale = held.known;
    stale["cell.rows"] = std::string(32, '0');
    auto guessed = fullSnapshot("3", "....");
    sections::strip(guessed, stale);
    expect(guessed.object("cell").has("rows"), "a key the rows no longer match never leaves them out");
    expect(sections::fill(dug, client) && dug == fullSnapshot("3", "#..."), "and the client takes the new rows");
}

// A stand-in for the NPC Mind: answers one POST with the given body.
int serveOnce(const std::string& reply, std::string& got, std::atomic<bool>& ready)
{
    const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    ::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof addr);
    socklen_t len = sizeof addr;
    ::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len);
    ::listen(fd, 1);
    const int port = ntohs(addr.sin_port);
    std::thread([fd, reply, &got, &ready] {
        ready = true;
        const int c = ::accept(fd, nullptr, nullptr);
        char buf[65536];
        std::string in;
        for (;;)
        {
            const auto n = ::recv(c, buf, sizeof buf, 0);
            if (n <= 0) break;
            in.append(buf, std::size_t(n));
            const auto head = in.find("\r\n\r\n");
            const auto at = in.find("Content-Length: ");
            if (head != std::string::npos && at != std::string::npos && in.size() >= head + 4 + std::stoul(in.substr(at + 16)))
                break;
        }
        got = in;
        const std::string out = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: " + std::to_string(reply.size()) +
                                "\r\n\r\n" + reply;
        ::send(c, out.data(), out.size(), 0);
        ::close(c);
        ::close(fd);
    }).detach();
    return port;
}

void mindTests()
{
    mind::Client offline;
    offline.configure("http://example.com:80/dialogue");
    expect(!offline.live() && offline.label() == "Authored offline dialogue", "only loopback endpoints are used");
    for (const std::string bad : {"http://127.0.0.1:8080@remote.example/dialogue", "http://localhost:8080\\@remote.example/",
                                  "http://127.0.0.1:8080\n/", "http://localhost:99999/dialogue", "http://localhost:80evil/dialogue",
                                  "http://127.0.0.1.evil:80/"})
    {
        offline.configure(bad);
        expect(!offline.live(), "a malformed or look-alike endpoint is refused: " + bad);
    }
    mind::Context c;
    c.npcId = "npc_keeper";
    c.name = "Rowan";
    c.playerName = "Ash";
    c.heardText = "Do you remember me?";
    mind::Reply got;
    offline.converse(c, [&](const mind::Reply& r) { got = r; });
    expect(!got.generated && got.text.find("We have only just met, Ash") == 0, "offline, an authored line: " + got.text);
    c.recollection = "I promised to return the blue scarf.";
    offline.converse(c, [&](const mind::Reply& r) { got = r; });
    expect(got.text.find("blue scarf") != std::string::npos, "offline, what it remembers is recalled");
    c.recollection.clear();

    std::string request;
    std::atomic<bool> ready{false};
    const int port = serveOnce(R"({"text":" Well met, Ash. ","emotion":"warm","affinity":9,"trust":-1,"remember":"likes pears",)"
                               R"("promise":{"by":"npc","what":"save a pear"}})", request, ready);
    while (!ready) std::this_thread::yield();
    mind::Client live;
    live.configure("http://localhost:" + std::to_string(port) + "/dialogue");
    expect(live.live(), "a loopback endpoint is live");
    bool answered = false;
    live.converse(c, [&](const mind::Reply& r) { got = r; answered = true; });
    live.settle(5);
    expect(answered && got.generated && got.text == "Well met, Ash." && got.emotion == "warm", "the Mind's reply, trimmed");
    expect(got.affinity == 3 && got.trust == -1, "its nudges held to -3..3");
    expect(got.remember == "likes pears" && got.promiseBy == "npc" && got.promise == "save a pear", "its note and promise");
    expect(request.find("POST /dialogue HTTP/1.1") == 0 && request.find("\"heard\":\"Do you remember me?\"") != std::string::npos,
           "what was sent");
    // A Mind that isn't there: the authored line, quickly.
    mind::Client gone;
    gone.configure("http://127.0.0.1:1/dialogue");
    answered = false;
    gone.converse(c, [&](const mind::Reply& r) { got = r; answered = true; });
    gone.settle(10);
    expect(answered && !got.generated, "an unreachable Mind falls back to the authored line");
    // A Mind that answers nonsense: the authored line too.
    ready = false;
    const int badPort = serveOnce("{\"text\":", request, ready);
    while (!ready) std::this_thread::yield();
    mind::Client garbled;
    garbled.configure("http://127.0.0.1:" + std::to_string(badPort) + "/dialogue");
    answered = false;
    garbled.converse(c, [&](const mind::Reply& r) { got = r; answered = true; });
    garbled.settle(5);
    expect(answered && !got.generated, "a malformed reply falls back to the authored line");
    // Asked about the weather in the snow, the authored reply is what the snapshot says, never rain.
    Cell snowy;
    snowy.id = "exterior";
    snowy.outdoors = true;
    snowy.weather = Weather::Snow;
    Environment env;
    env.phase = "morning";
    mind::Context weather = c;
    weather.heardText = "What is the weather like?";
    weather.environment = wire::environmentDescription(snowy, env);
    const auto reply = mind::Client::authoredReply(weather);
    expect(!weather.environment.empty() && reply == weather.environment && mind::lower(reply).find("snow") != std::string::npos,
           "the weather is described as it is: " + reply);
    expect(mind::lower(reply).find("rain") == std::string::npos, "and snow is never called rain");

    std::string error;
    expect(sys::zlibAvailable(error), "zlib loads: " + error);
    const std::string text(5000, 'w');
    std::vector<std::uint8_t> packed, back;
    expect(sys::compress(reinterpret_cast<const std::uint8_t*>(text.data()), text.size(), packed) && packed.size() < 100 &&
               sys::uncompress(packed.data(), packed.size(), text.size(), back) && std::string(back.begin(), back.end()) == text,
           "zlib round trip");
}

json::Value parsed(const std::string& text)
{
    json::Value v;
    std::string error;
    if (!json::parse(text, v, error))
        throw std::runtime_error("bad test JSON: " + error);
    return v;
}

// The operator bridge (Docs/DM_BRIDGE_CONTRACT.md): exactly-once requests, receipts that survive a restart, and a
// failed checkpoint undoing what a request changed.
void directorTests()
{
    director::Bridge bridge;
    World world;
    world.addPlayer("player-ash", "Ash");
    const std::set<std::string> online{"player-ash"};
    int commits = 0, notices = 0;
    const auto commit = [&]() { ++commits; return true; };
    const director::Bridge::Notice notice = [&](const std::set<std::string>& to, const std::string&) { notices += int(to.size()); };
    const auto envelope = [&](const director::Bridge& b, const std::string& id, const std::string& kind, const std::string& payload) {
        auto o = json::Value::object();
        o.add("version", 1);
        o.add("id", id);
        o.add("worldId", b.state().string("worldId"));
        o.add("createdAtUnix", 1000);
        o.add("expiresAtUnix", 1120);
        o.add("kind", kind);
        o.add("payload", parsed(payload));
        return o;
    };
    const auto ok = [](const json::Value& result) { return result.boolean("ok"); };
    const std::string hash = "0123456789012345678901234567890123456789";
    const auto transfer = envelope(bridge, "transfer-1", "economy_transfer",
                                   R"({"from":"treasury","to":"player-ash","item":"meal","quantity":1,"coins":3})");
    expect(ok(bridge.execute(transfer, "transfer-1", hash, world, online, 1001, commit, notice)), "operator transfers existing resources");
    expect(world.society().account("player-ash")->cash == 23 && world.society().conserved(), "exactly three pennies, money conserved");
    expect(ok(bridge.execute(transfer, "transfer-1", hash, world, online, 1200, commit, notice)), "a retry replays its receipt after expiry");
    expect(commits == 1 && world.society().account("player-ash")->cash == 23, "a retry neither commits again nor pays twice");

    director::Bridge restart;
    expect(restart.restore(parsed(json::dump(bridge.state()))), "receipts survive a restore");
    const auto broken = [&](const std::function<void(json::Value&)>& change) {
        auto state = parsed(json::dump(bridge.state()));
        change(state.find("receipts")->items()[0]);
        return restart.restore(state);
    };
    expect(!broken([](json::Value& r) { r.find("result")->set("version", 2); }), "an unsupported receipt version fails closed");
    expect(!broken([](json::Value& r) { r.find("result")->erase("detail"); }), "a receipt without its detail is refused");
    expect(!broken([](json::Value& r) { r.set("fingerprint", std::string(40, 'z')); }), "a fingerprint must be hexadecimal");
    expect(ok(restart.execute(transfer, "transfer-1", hash, world, online, 1200, commit, notice)), "a restarted bridge replays the request");
    expect(!ok(bridge.execute(transfer, "transfer-1", "different", world, online, 1001, commit, notice)), "a reused ID with new content is refused");

    auto cross = envelope(bridge, "cross-world", "weather", R"({"cell":"exterior","preset":"fog"})");
    cross.set("worldId", "wrong-world");
    expect(!ok(bridge.execute(cross, "cross-world", hash, world, online, 1001, commit, notice)), "another world's request is refused");
    // A weather front (doc 29, phase 7): a kind, its middle, reach, heading and hours; anything else refused.
    const auto front = envelope(bridge, "front-1", "weather_front", R"({"kind":"storm","x":10,"y":10,"radius":300,"heading":0,"hours":6})");
    expect(ok(bridge.execute(front, "front-1", hash, world, online, 1001, commit, notice)) && world.save().fronts.size() == 1,
           "the DM calls up a storm front");
    const auto badFront = envelope(bridge, "front-2", "weather_front", R"({"kind":"lava","x":10,"y":10,"radius":300,"heading":0,"hours":6})");
    expect(!ok(bridge.execute(badFront, "front-2", hash, world, online, 1001, commit, notice)), "an unknown kind of front is refused");
    const auto bad = envelope(bridge, "bad-bool", "economy_transfer", R"({"from":"treasury","to":"player-ash","item":false,"quantity":0,"coins":3})");
    expect(!ok(bridge.execute(bad, "bad-bool", hash, world, online, 1001, commit, notice)), "a boolean item is not an empty item");
    const auto announcement = envelope(bridge, "notice-1", "notice",
                                       R"({"scope":"players","targets":["player-ash","player-absent"],"text":"A bell sounds across the yard."})");
    expect(ok(bridge.execute(announcement, "notice-1", hash, world, online, 1001, commit, notice)) && notices == 1,
           "a notice reaches only those online");
    bridge.execute(announcement, "notice-1", hash, world, online, 1002, commit, notice);
    expect(notices == 1, "a retried notice is not sent again");
    const auto badNotice = envelope(bridge, "bad-notice", "notice", R"({"scope":"world","target":false,"text":"Must not broadcast."})");
    expect(!ok(bridge.execute(badNotice, "bad-notice", hash, world, online, 1001, commit, notice)) && notices == 1,
           "a malformed notice fails closed and sends nothing");
    expect(!ok(bridge.execute(envelope(bridge, "army-1", "army", "{}"), "army-1", hash, world, online, 1001, commit, notice)),
           "an unimplemented kind is never reported applied");
    const auto expired = envelope(bridge, "expired-1", "weather", R"({"cell":"exterior","preset":"fog"})");
    expect(!ok(bridge.execute(expired, "expired-1", hash, world, online, 1121, commit, notice)), "an expired request is refused");

    const auto hidden = world.addPlayer("player-offline", "Offline");
    world.removePlayer("player-offline");
    const auto view = bridge.snapshot(world, {{"player-offline", hidden}}, online, {{"player-ash", 1000}}, 42, 1002);
    expect(view.array("cells").size() == 3, "the operator sees every cell, whatever players have seen");
    expect(view.array("characters").size() == 8, "six NPCs plus online and saved offline players");
    expect(!view.has("activeMemory"), "no conversation memory in the operator's view");

    director::Bridge failing;
    const auto rollback = envelope(failing, "rollback-1", "economy_transfer", R"({"from":"treasury","to":"player-ash","item":"","quantity":0,"coins":4})");
    const auto cash = world.society().account("player-ash")->cash;
    world.entity("player-ash")->path = {{17.5, 12.5}};
    world.entity("player-ash")->typing = true;
    expect(!ok(failing.execute(rollback, "rollback-1", hash, world, online, 1001, [] { return false; }, notice)),
           "a failed checkpoint is not acknowledged");
    expect(world.society().account("player-ash")->cash == cash, "a failed checkpoint undoes the transfer");
    expect(world.entity("player-ash")->typing && world.entity("player-ash")->path.size() == 1, "and leaves movement and typing alone");
}

bool posed(const json::Value& frame, const std::string& id)
{
    for (const auto& pose : frame.array("entities"))
        if (pose.string("id") == id)
            return true;
    return false;
}

void motionTests()
{
    World world;
    world.addPlayer("player-ash", "Ash");
    world.addPlayer("player-bram", "Bram");
    world.addPlayer("player-far", "Far");
    const auto* keeper = world.entity("npc_keeper");
    expect(keeper != nullptr, "the keeper is here");
    auto* ash = world.entity("player-ash");
    ash->cellId = keeper->cellId;
    ash->position = {keeper->position.x + 1, keeper->position.y};
    auto* bram = world.entity("player-bram");
    bram->cellId = ash->cellId;
    bram->position = ash->position;
    std::string elsewhere;
    for (const auto& [id, cell] : world.cells())
        if (id != ash->cellId)
            elsewhere = id;
    expect(!elsewhere.empty(), "there is another cell");
    auto* far = world.entity("player-far");
    far->cellId = elsewhere;
    far->position = ash->position;
    auto f = motion::frame(world, "player-ash");
    expect(f.string("observer") == "player-ash" && f.string("cellId") == ash->cellId, "a frame is the observer's");
    expect(posed(f, "player-ash") && posed(f, "npc_keeper") && posed(f, "player-bram"), "it poses the observer and who it can see");
    expect(!posed(f, "player-far") && json::dump(f).find("player-far") == std::string::npos, "never another cell's actors, not even their IDs");
    ash->eyeHealth = 0;
    bram->position = {bram->position.x + 10, bram->position.y};
    f = motion::frame(world, "player-ash");
    expect(!posed(f, "player-bram") && json::dump(f).find("player-bram") == std::string::npos && posed(f, "player-ash"),
           "a pose the observer cannot see is dropped");

    ash->eyeHealth = 1;
    f = motion::frame(world, "player-ash");
    f.add("motionSession", "session-1");
    f.add("cellGeneration", 4);
    f.set("revision", 42);
    const auto back = motion::unpack(motion::pack(f));
    expect(back.string("observer") == "player-ash" && back.string("cellId") == ash->cellId && back.number("revision") == 42 &&
               back.string("motionSession") == "session-1" && back.number("cellGeneration") == 4,
           "a frame's stamps survive packing");
    const auto& sent = f.array("entities");
    const auto& got = back.array("entities");
    bool same = sent.size() == got.size() && !sent.empty();
    for (std::size_t i = 0; same && i < sent.size(); ++i)
        same = got[i].string("id") == sent[i].string("id") && std::abs(got[i].number("x") - sent[i].number("x")) < 1e-4 &&
               std::abs(got[i].number("y") - sent[i].number("y")) < 1e-4;
    expect(same, "and every pose, to within float precision");

    // The bytes the browser client's test reads (Client/src/net/motion.golden.json): this pack() must make exactly them.
    std::ifstream in(std::string(RATW_SOURCE_DIR) + "/Client/src/net/motion.golden.json");
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    const auto golden = parsed(text);
    const auto bytes = motion::pack(golden.object("frame"));
    const auto hex = sys::hex(bytes.data(), bytes.size());
    expect(!bytes.empty() && hex == golden.string("bytes"), "the golden motion frame packs to its committed bytes: " + hex);
}

// A client's WebSocket frame: always masked.
std::string clientFrame(std::uint8_t op, const std::string& data, bool fin = true)
{
    std::string out;
    out += char((fin ? 0x80 : 0) | op);
    const unsigned char mask[4] = {0x12, 0x34, 0x56, 0x78};
    if (data.size() < 126)
        out += char(0x80 | data.size());
    else if (data.size() <= 0xffff)
    {
        out += char(0x80 | 126);
        out += char(data.size() >> 8);
        out += char(data.size());
    }
    else
    {
        out += char(0x80 | 127);
        for (int shift = 56; shift >= 0; shift -= 8)
            out += char(std::uint64_t(data.size()) >> shift);
    }
    out.append(reinterpret_cast<const char*>(mask), 4);
    for (std::size_t i = 0; i < data.size(); ++i)
        out += char(data[i] ^ mask[i % 4]);
    return out;
}

void webTests()
{
    using namespace web;
    std::string buffer = "GET /ws?x=1 HTTP/1.1\r\nHost: 127.0.0.1:7788\r\nUpgrade: websocket\r\nConnection: keep-alive, Upgrade\r\n"
                         "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Version: 13\r\n";
    Request r;
    expect(takeRequest(buffer, r) == 0, "a request head without its blank line waits for more");
    buffer += "\r\nleftover";
    expect(takeRequest(buffer, r) == 1 && r.method == "GET" && r.path == "/ws" && r.query == "x=1" && buffer == "leftover",
           "a request head is taken whole, and only it");
    expect(r.header("HOST") == "127.0.0.1:7788" && upgradeRequested(r), "headers by any case; an upgrade is recognised");
    expect(acceptKey(r.header("sec-websocket-key")) == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=", "the accept key of RFC 6455's example");
    expect(originAllowed(r), "a program (no Origin) may connect");
    r.headers["origin"] = "http://127.0.0.1:7788";
    expect(originAllowed(r), "the page this server served may connect");
    r.headers["origin"] = "https://evil.example";
    expect(!originAllowed(r), "another site's page may not");
    r.headers["origin"] = "http://localhost:7788";
    expect(!originAllowed(r), "nor a different name for the same host");
    auto noUpgrade = r;
    noUpgrade.headers["sec-websocket-version"] = "8";
    expect(!upgradeRequested(noUpgrade), "only WebSocket version 13");
    noUpgrade = r;
    noUpgrade.headers["sec-websocket-key"] = "short";
    expect(!upgradeRequested(noUpgrade), "a malformed key is refused");
    for (const std::string bad : {"GET\r\n\r\n", "GET /x HTTP/2\r\n\r\n", "GET x HTTP/1.1\r\n\r\n", "GET /x HTTP/1.1\r\nno colon\r\n\r\n",
                                  "GET /x HTTP/1.1\r\nBad Name: 1\r\n\r\n"})
    {
        std::string b = bad;
        expect(takeRequest(b, r) == -1, "a malformed request is refused: " + bad.substr(0, 20));
    }
    std::string huge = "GET /x HTTP/1.1\r\n" + std::string(MaxRequestHead, 'a');
    expect(takeRequest(huge, r) == -1, "an endless request head is refused");
    expect(base64(reinterpret_cast<const std::uint8_t*>("ab"), 2) == "YWI=" && base64(reinterpret_cast<const std::uint8_t*>("abc"), 3) == "YWJj",
           "base64 pads");

    // Frames from a client.
    Reader reader;
    Opcode op;
    std::string payload;
    buffer = clientFrame(Binary, std::string("\x01{}", 3));
    buffer.pop_back();
    expect(takeMessage(buffer, reader, op, payload, 1000) == 0, "half a frame waits");
    buffer = clientFrame(Binary, std::string("\x01{}", 3));
    expect(takeMessage(buffer, reader, op, payload, 1000) == 1 && op == Binary && payload == std::string("\x01{}", 3) && buffer.empty(),
           "a masked binary message is unmasked");
    buffer = clientFrame(Binary, "ab", false) + clientFrame(Ping, "hi") + clientFrame(Continuation, "cd", false) +
             clientFrame(Continuation, "ef");
    expect(takeMessage(buffer, reader, op, payload, 1000) == 1 && op == Ping && payload == "hi", "a ping between fragments comes out first");
    expect(takeMessage(buffer, reader, op, payload, 1000) == 1 && op == Binary && payload == "abcdef", "fragments are joined");
    const std::string medium(300, 'm'), large(70000, 'l');
    buffer = clientFrame(Binary, medium) + clientFrame(Binary, large);
    expect(takeMessage(buffer, reader, op, payload, 100000) == 1 && payload == medium, "a 16-bit length");
    expect(takeMessage(buffer, reader, op, payload, 100000) == 1 && payload == large, "a 64-bit length");
    buffer = clientFrame(Binary, large);
    expect(takeMessage(buffer, reader, op, payload, 1000) == -1, "a message over the limit is refused");
    Reader fresh;
    buffer = clientFrame(Binary, std::string(600, 'a'), false) + clientFrame(Continuation, std::string(600, 'b'));
    expect(takeMessage(buffer, fresh, op, payload, 1000) == -1, "fragments over the limit together are refused");
    buffer = std::string("\x82\x02hi", 4);
    expect(takeMessage(buffer, fresh = Reader{}, op, payload, 1000) == -1, "an unmasked client frame is refused");
    buffer = clientFrame(Continuation, "x");
    expect(takeMessage(buffer, fresh = Reader{}, op, payload, 1000) == -1, "a continuation of nothing is refused");
    buffer = clientFrame(Ping, "x", false);
    expect(takeMessage(buffer, fresh = Reader{}, op, payload, 1000) == -1, "a fragmented control frame is refused");
    buffer = clientFrame(3, "x");
    expect(takeMessage(buffer, fresh = Reader{}, op, payload, 1000) == -1, "an unknown opcode is refused");
    buffer = clientFrame(Binary, "x");
    buffer[0] = char(buffer[0] | 0x40);
    expect(takeMessage(buffer, fresh = Reader{}, op, payload, 1000) == -1, "extension bits are refused");

    // Frames to a client.
    std::string out;
    appendFrame(out, Binary, medium.data(), 125);
    expect(out.size() == 127 && std::uint8_t(out[0]) == 0x82 && out[1] == 125, "a short frame has a one-byte length");
    out.clear();
    appendFrame(out, Binary, medium.data(), 126);
    expect(out.size() == 130 && std::uint8_t(out[1]) == 126, "a medium frame has a 16-bit length");
    out.clear();
    appendFrame(out, Binary, large.data(), large.size());
    expect(out.size() == large.size() + 10 && std::uint8_t(out[1]) == 127, "a large frame has a 64-bit length");

    // Files.
    std::string file;
    expect(filePath("/", file) && file == "index.html", "the page");
    expect(filePath("/assets/index-3f9a_b.js", file) && file == "assets/index-3f9a_b.js", "a built asset");
    for (const std::string bad : {"/../etc/passwd", "/assets/../../x", "/.env", "/assets/.hidden", "/a//b", "/assets/", "/a%2e%2e",
                                  "/a b", "/a\\b", "", "x"})
        expect(!filePath(bad, file), "not served: " + bad);
    expect(contentType("assets/a.js").rfind("text/javascript", 0) == 0 && contentType("index.html").rfind("text/html", 0) == 0 &&
               contentType("p/timber.png") == "image/png" && contentType("x.exe") == "application/octet-stream",
           "content types");
    expect(response(404, "text/plain", "no").find("Content-Length: 2\r\n") != std::string::npos, "a response says its length");
}
} // namespace

int main()
{
    try
    {
        accountsTests();
        accountRulesTests();
        accountOwnershipTests();
        sectionsTests();
        visibilityDeltaTests();
        sectionKeysTests();
        mindTests();
        motionTests();
        directorTests();
        webTests();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Server parts tests passed: " << checks << " checks.\n";
    return 0;
}
