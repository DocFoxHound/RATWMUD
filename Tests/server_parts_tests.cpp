// The standalone server's parts (Phase 6): accounts as the Unreal runtime keeps them, delta snapshots between a server
// and a client, the NPC Mind client against a small local stand-in, and the operator bridge's request contract.
#include "RatwAccountsCore.h"
#include "RatwDirector.h"
#include "RatwMind.h"
#include "RatwSections.h"
#include "RatwSystemLibs.h"

#include <arpa/inet.h>
#include <atomic>
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
} // namespace

int main()
{
    try
    {
        accountsTests();
        sectionsTests();
        mindTests();
        directorTests();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Server parts tests passed: " << checks << " checks.\n";
    return 0;
}
