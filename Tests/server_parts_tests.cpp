// The standalone server's parts (Phase 6): accounts as the Unreal runtime keeps them, delta snapshots between a server
// and a client, and the NPC Mind client against a small local stand-in.
#include "RatwAccountsCore.h"
#include "RatwMind.h"
#include "RatwSections.h"
#include "RatwSystemLibs.h"

#include <arpa/inet.h>
#include <atomic>
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
    mind::Context c;
    c.npcId = "npc_keeper";
    c.name = "Rowan";
    c.playerName = "Ash";
    c.heardText = "Do you remember me?";
    mind::Reply got;
    offline.converse(c, [&](const mind::Reply& r) { got = r; });
    expect(!got.generated && got.text.find("We have only just met, Ash") == 0, "offline, an authored line: " + got.text);

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
    std::string error;
    expect(sys::zlibAvailable(error), "zlib loads: " + error);
    const std::string text(5000, 'w');
    std::vector<std::uint8_t> packed, back;
    expect(sys::compress(reinterpret_cast<const std::uint8_t*>(text.data()), text.size(), packed) && packed.size() < 100 &&
               sys::uncompress(packed.data(), packed.size(), text.size(), back) && std::string(back.begin(), back.end()) == text,
           "zlib round trip");
}
} // namespace

int main()
{
    try
    {
        accountsTests();
        sectionsTests();
        mindTests();
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Server parts tests passed: " << checks << " checks.\n";
    return 0;
}
