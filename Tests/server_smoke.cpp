// The server end to end: started as its own process, a client connects over a WebSocket (RatwWeb.h) as the browser
// does, enters, walks and talks, and a restart finds everything where it was left. The client's files are served, and
// nothing outside them; another site's page can't connect.
#include "RatwJsonDoc.h"
#include "RatwLink.h"
#include "RatwMotionCore.h"
#include "RatwSections.h"
#include "RatwSystemLibs.h"
#include "RatwWeb.h"

#include <arpa/inet.h>
#include <algorithm>
#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

using namespace ratw;
namespace
{
int checks = 0;
pid_t running = 0;                                     // The server under test, stopped if a check fails.
void expect(bool condition, const std::string& message)
{
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}

pid_t launch(const std::string& server, int port, const std::string& save, const std::string& web)
{
    const pid_t pid = ::fork();
    if (pid == 0)
    {
        const std::string p = std::to_string(port);
        ::execl(server.c_str(), server.c_str(), "--save", save.c_str(), "--port", p.c_str(), "--dev-identity", "--dev-tools",
                "--web", web.c_str(), static_cast<char*>(nullptr));
        std::_Exit(127);
    }
    running = pid;
    return pid;
}

int connectTo(int port)
{
    for (int attempt = 0; attempt < 100; ++attempt)
    {
        const int fd = ::socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(std::uint16_t(port));
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof addr) == 0)
            return fd;
        ::close(fd);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return -1;
}

// Sends a request as written, and returns everything the server answers before it closes.
std::string httpExchange(int port, const std::string& request)
{
    const int fd = connectTo(port);
    ::send(fd, request.data(), request.size(), MSG_NOSIGNAL);
    std::string answer;
    char buffer[65536];
    for (;;)
    {
        pollfd p{fd, POLLIN, 0};
        if (::poll(&p, 1, 3000) <= 0)
            break;
        const auto n = ::recv(fd, buffer, sizeof buffer, 0);
        if (n <= 0)
            break;
        answer.append(buffer, std::size_t(n));
    }
    ::close(fd);
    return answer;
}

// A server's WebSocket frame (never masked) off the front of `buffer`; false until a whole one has arrived.
bool takeServerFrame(std::string& buffer, std::uint8_t& op, std::string& payload)
{
    if (buffer.size() < 2)
        return false;
    const auto* b = reinterpret_cast<const unsigned char*>(buffer.data());
    expect((b[0] & 0x80) && !(b[1] & 0x80), "the server sends whole, unmasked frames");
    std::uint64_t length = b[1] & 0x7f;
    std::size_t at = 2;
    if (length == 126)
    {
        if (buffer.size() < 4)
            return false;
        length = std::uint64_t(b[2]) << 8 | b[3];
        at = 4;
    }
    else if (length == 127)
    {
        if (buffer.size() < 10)
            return false;
        length = 0;
        for (int i = 0; i < 8; ++i)
            length = length << 8 | b[2 + i];
        at = 10;
    }
    if (buffer.size() < at + length)
        return false;
    op = b[0] & 0x0f;
    payload = buffer.substr(at, std::size_t(length));
    buffer.erase(0, at + std::size_t(length));
    return true;
}

struct Link
{
    int fd = -1;
    std::vector<std::uint8_t> controls;                // WebSocket control frames received (close, pong).
    std::string in;
    sections::Cache cache;
    std::vector<json::Value> events, snapshots, motions;
    bool open(int port, const std::string& origin = {})
    {
        fd = connectTo(port);
        if (fd < 0)
            return false;
        const std::string request = "GET /ws HTTP/1.1\r\nHost: 127.0.0.1:" + std::to_string(port) +
                                    "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Version: 13\r\n"
                                    "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n" +
                                    (origin.empty() ? std::string() : "Origin: " + origin + "\r\n") + "\r\n";
        ::send(fd, request.data(), request.size(), MSG_NOSIGNAL);
        while (in.find("\r\n\r\n") == std::string::npos)
        {
            pollfd p{fd, POLLIN, 0};
            if (::poll(&p, 1, 3000) <= 0)
                return false;
            char buffer[4096];
            const auto n = ::recv(fd, buffer, sizeof buffer, 0);
            if (n <= 0)
                return false;
            in.append(buffer, std::size_t(n));
        }
        const auto end = in.find("\r\n\r\n");
        const std::string head = in.substr(0, end);
        in.erase(0, end + 4);
        return head.rfind("HTTP/1.1 101 ", 0) == 0 && head.find("Sec-WebSocket-Accept: s3pPLMBiTxaQ9kYGzzhZRbK+xOo=") != std::string::npos;
    }
    // A client's WebSocket frame, masked as the protocol requires.
    void sendWeb(std::uint8_t op, const std::string& data)
    {
        std::string out;
        out += char(0x80 | op);
        const unsigned char mask[4] = {0xa1, 0xb2, 0xc3, 0xd4};
        if (data.size() < 126)
            out += char(0x80 | data.size());
        else
        {
            out += char(0x80 | 126);
            out += char(data.size() >> 8);
            out += char(data.size());
        }
        out.append(reinterpret_cast<const char*>(mask), 4);
        for (std::size_t i = 0; i < data.size(); ++i)
            out += char(data[i] ^ mask[i % 4]);
        ::send(fd, out.data(), out.size(), MSG_NOSIGNAL);
    }
    void send(link::Kind kind, const void* payload, std::size_t length)
    {
        std::string message(1, char(kind));
        message.append(static_cast<const char*>(payload), length);
        sendWeb(web::Binary, message);
    }
    void command(const std::string& json) { send(link::Command, json.data(), json.size()); }
    void ack(double revision)
    {
        char payload[9];
        std::memcpy(payload, &revision, 8);
        payload[8] = 0;
        send(link::Ack, payload, 9);
    }
    // Reads for this long, decoding everything that arrives.
    void read(double seconds)
    {
        const auto until = std::chrono::steady_clock::now() + std::chrono::duration<double>(seconds);
        while (std::chrono::steady_clock::now() < until)
        {
            // What is already here first (a WebSocket's first messages can arrive with its handshake), then more.
            if (in.empty())
            {
                pollfd p{fd, POLLIN, 0};
                if (::poll(&p, 1, 20) <= 0)
                    continue;
                char buffer[65536];
                const auto n = ::recv(fd, buffer, sizeof buffer, 0);
                if (n <= 0)
                    return;
                in.append(buffer, std::size_t(n));
            }
            const std::size_t before = in.size();
            link::Kind kind;
            std::string payload;
            for (;;)
            {
                std::uint8_t op = 0;
                if (!takeServerFrame(in, op, payload))
                    break;
                if (op != web::Binary)
                {
                    controls.push_back(op);
                    continue;
                }
                expect(!payload.empty(), "every message has its kind");
                kind = link::Kind(std::uint8_t(payload[0]));
                payload.erase(0, 1);
                expect(payload.size() >= 4, "every frame from the server has its raw length");
                std::uint32_t raw = 0;
                for (int i = 0; i < 4; ++i)
                    raw |= std::uint32_t(std::uint8_t(payload[std::size_t(i)])) << (8 * i);
                std::vector<std::uint8_t> bytes;
                expect(sys::uncompress(reinterpret_cast<const std::uint8_t*>(payload.data() + 4), payload.size() - 4, raw, bytes),
                       "and is zlib");
                if (kind == link::Motion)
                {
                    motions.push_back(motion::unpack(bytes));
                    continue;
                }
                json::Value v;
                std::string error;
                expect(json::parse(std::string(bytes.begin(), bytes.end()), v, error), "JSON: " + error);
                if (kind == link::Snapshot)
                {
                    expect(sections::fill(v, cache), "a snapshot fills from what this client holds");
                    snapshots.push_back(v);
                    ack(v.number("revision"));
                }
                else
                    events.push_back(v);
            }
            if (!in.empty() && in.size() == before)
            {
                // Part of a frame: wait for the rest.
                pollfd p{fd, POLLIN, 0};
                if (::poll(&p, 1, 20) <= 0)
                    continue;
                char buffer[65536];
                const auto n = ::recv(fd, buffer, sizeof buffer, 0);
                if (n <= 0)
                    return;
                in.append(buffer, std::size_t(n));
            }
        }
    }
    const json::Value* last(const std::string& type) const
    {
        for (auto it = events.rbegin(); it != events.rend(); ++it)
            if (it->string("type") == type)
                return &*it;
        return nullptr;
    }
};
} // namespace

int main(int argc, char** argv)
{
    try
    {
        expect(argc == 2, "usage: server_smoke PATH_TO_ratw_server");
        std::string error;
        expect(sys::zlibAvailable(error), error);
        const std::string save = "/tmp/ratw-server-smoke-" + std::to_string(::getpid()) + ".json";
        const std::string web = "/tmp/ratw-server-smoke-web-" + std::to_string(::getpid());
        const int port = 20000 + ::getpid() % 20000;
        std::remove(save.c_str());
        std::filesystem::create_directories(web + "/assets");
        std::ofstream(web + "/index.html") << "<!doctype html><title>RATW</title>";
        std::ofstream(web + "/assets/index-abc.js") << "console.log(1)";
        std::ofstream(web + "/../ratw-secret-" + std::to_string(::getpid())) << "secret";
        auto pid = launch(argv[1], port, save, web);
        double x = 0;
        {
            Link c;
            expect(c.open(port), "the server listens within five seconds");
            c.read(.3);
            expect(c.last("lobby") != nullptr, "a client is shown the lobby");
            c.command(R"({"type":"hello","id":"ash","name":"Ash"})");
            c.read(.5);
            expect(c.last("entered") != nullptr && !c.snapshots.empty(), "enters, and gets snapshots");
            expect(c.snapshots.back()["self"].string("id") == "player-ash", "of itself");
            const double start = c.snapshots.back()["self"].number("x");
            c.command(R"({"type":"move","x":1,"y":0})");
            c.read(1.5);
            c.command(R"({"type":"stop"})");
            c.read(.4);
            x = c.snapshots.back()["self"].number("x");
            expect(x > start + .5, "a move moves them (" + std::to_string(start) + " -> " + std::to_string(x) + ")");
            expect(c.motions.size() > 20 && !c.motions.back().isNull(), "motion frames, twenty a second, in binary");
            expect(c.snapshots.size() >= 8, "snapshots five a second (" + std::to_string(c.snapshots.size()) + ")");
            c.command(R"({"type":"chat","text":"\"Is anyone here?\"","commandId":"c1"})");
            c.read(.5);
            expect(c.last("roleplay") && c.last("roleplay")->string("text").find("Is anyone here?") != std::string::npos, "what they say, heard");
            expect(c.last("chatAccepted") != nullptr, "and accepted");
            ::close(c.fd);
        }
        // The browser client's files, and nothing else.
        const auto page = httpExchange(port, "GET / HTTP/1.1\r\nHost: x\r\n\r\n");
        expect(page.rfind("HTTP/1.1 200 OK\r\n", 0) == 0 && page.find("text/html") != std::string::npos &&
                   page.find("Cache-Control: no-cache") != std::string::npos && page.find("<title>RATW</title>") != std::string::npos,
               "the page is served, never cached stale");
        const auto asset = httpExchange(port, "GET /assets/index-abc.js HTTP/1.1\r\nHost: x\r\n\r\n");
        expect(asset.find("text/javascript") != std::string::npos && asset.find("immutable") != std::string::npos &&
                   asset.find("console.log(1)") != std::string::npos,
               "a built asset is served, cached for good");
        const auto head = httpExchange(port, "HEAD / HTTP/1.1\r\nHost: x\r\n\r\n");
        expect(head.rfind("HTTP/1.1 200", 0) == 0 && head.find("<title>") == std::string::npos, "HEAD has no body");
        for (const std::string& path : std::vector<std::string>{"/../ratw-secret-" + std::to_string(::getpid()), "/%2e%2e/x", "/.hidden", "/missing.js"})
            expect(httpExchange(port, "GET " + path + " HTTP/1.1\r\nHost: x\r\n\r\n").rfind("HTTP/1.1 404", 0) == 0,
                   "not served: " + path);
        expect(httpExchange(port, "POST / HTTP/1.1\r\nHost: x\r\nContent-Length: 0\r\n\r\n").rfind("HTTP/1.1 405", 0) == 0,
               "only GET");
        expect(httpExchange(port, "GET /ws HTTP/1.1\r\nHost: x\r\n\r\n").rfind("HTTP/1.1 426", 0) == 0,
               "the game's address wants a WebSocket");
        expect(httpExchange(port, "NONSENSE\r\n\r\n").rfind("HTTP/1.1 400", 0) == 0, "anything but HTTP is refused");
        {
            Link foreign;
            expect(!foreign.open(port, "https://evil.example"), "another site's page is refused");
            ::close(foreign.fd);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        ::kill(pid, SIGTERM);
        int status = 0;
        ::waitpid(pid, &status, 0);
        expect(WIFEXITED(status) && WEXITSTATUS(status) == 0, "it stops cleanly on SIGTERM, saving");
        // Again, from the save, played as a browser plays.
        pid = launch(argv[1], port, save, web);
        {
            Link c;
            expect(c.open(port, "http://127.0.0.1:" + std::to_string(port)), "a second server takes a WebSocket from its own page");
            c.read(.3);
            expect(c.last("lobby") && c.last("lobby")->boolean("credentialsAllowed"), "a local browser may sign in");
            c.command(R"({"type":"hello","id":"ash","name":"Ash"})");
            c.read(.8);
            expect(!c.snapshots.empty() && std::abs(c.snapshots.back()["self"].number("x") - x) < .5, "Ash is where they were left");
            expect(c.motions.size() > 5 && !c.motions.back().isNull(), "motion frames over the WebSocket");
            const double before = c.snapshots.back()["self"].number("x");
            c.command(R"({"type":"move","x":-1,"y":0})");
            c.read(1);
            c.command(R"({"type":"stop"})");
            c.read(.3);
            expect(c.snapshots.back()["self"].number("x") < before - .5, "and walks");
            c.sendWeb(web::Ping, "hi");
            c.read(.2);
            expect(std::find(c.controls.begin(), c.controls.end(), web::Pong) != c.controls.end(), "a ping is answered");
            c.sendWeb(web::Text, "{}");
            c.read(.3);
            expect(std::find(c.controls.begin(), c.controls.end(), web::Close) != c.controls.end(), "text messages end the connection");
            ::close(c.fd);
        }
        {
            Link c;
            expect(c.open(port), "a program may connect without an Origin");
            c.sendWeb(web::Close, std::string("\x03\xe8", 2));
            c.read(.3);
            expect(std::find(c.controls.begin(), c.controls.end(), web::Close) != c.controls.end(), "a close is answered with a close");
            ::close(c.fd);
        }
        ::kill(pid, SIGTERM);
        ::waitpid(pid, &status, 0);
        std::remove(save.c_str());
        std::filesystem::remove_all(web);
        std::filesystem::remove(web + "/../ratw-secret-" + std::to_string(::getpid()));
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        if (running > 0)
        {
            ::kill(running, SIGKILL);
            ::waitpid(running, nullptr, 0);
        }
        return 1;
    }
    std::cout << "Server smoke passed: " << checks << " checks.\n";
    return 0;
}
