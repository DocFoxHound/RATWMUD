// The standalone server end to end: started as its own process, a client connects over TCP (RatwLink.h), enters,
// walks and talks, and a restart finds everything where it was left.
#include "RatwJsonDoc.h"
#include "RatwLink.h"
#include "RatwMotionCore.h"
#include "RatwSections.h"
#include "RatwSystemLibs.h"

#include <arpa/inet.h>
#include <chrono>
#include <csignal>
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
void expect(bool condition, const std::string& message)
{
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}

pid_t launch(const std::string& server, int port, const std::string& save)
{
    const pid_t pid = ::fork();
    if (pid == 0)
    {
        const std::string p = std::to_string(port);
        ::execl(server.c_str(), server.c_str(), "--save", save.c_str(), "--port", p.c_str(), "--dev-identity", "--dev-tools",
                static_cast<char*>(nullptr));
        std::_Exit(127);
    }
    return pid;
}

struct Link
{
    int fd = -1;
    std::string in;
    sections::Cache cache;
    std::vector<json::Value> events, snapshots, motions;
    bool open(int port)
    {
        for (int attempt = 0; attempt < 100; ++attempt)
        {
            fd = ::socket(AF_INET, SOCK_STREAM, 0);
            sockaddr_in addr{};
            addr.sin_family = AF_INET;
            addr.sin_port = htons(std::uint16_t(port));
            addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof addr) == 0)
                return true;
            ::close(fd);
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        return false;
    }
    void command(const std::string& json)
    {
        std::string out;
        link::appendFrame(out, link::Command, json.data(), json.size());
        ::send(fd, out.data(), out.size(), MSG_NOSIGNAL);
    }
    void ack(double revision)
    {
        char payload[9];
        std::memcpy(payload, &revision, 8);
        payload[8] = 0;
        std::string out;
        link::appendFrame(out, link::Ack, payload, 9);
        ::send(fd, out.data(), out.size(), MSG_NOSIGNAL);
    }
    // Reads for this long, decoding everything that arrives.
    void read(double seconds)
    {
        const auto until = std::chrono::steady_clock::now() + std::chrono::duration<double>(seconds);
        while (std::chrono::steady_clock::now() < until)
        {
            pollfd p{fd, POLLIN, 0};
            if (::poll(&p, 1, 20) <= 0)
                continue;
            char buffer[65536];
            const auto n = ::recv(fd, buffer, sizeof buffer, 0);
            if (n <= 0)
                return;
            in.append(buffer, std::size_t(n));
            link::Kind kind;
            std::string payload;
            bool bad = false;
            while (link::takeFrame(in, kind, payload, bad))
            {
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
            expect(!bad, "frames are well formed");
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
        const int port = 20000 + ::getpid() % 20000;
        std::remove(save.c_str());
        auto pid = launch(argv[1], port, save);
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
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        ::kill(pid, SIGTERM);
        int status = 0;
        ::waitpid(pid, &status, 0);
        expect(WIFEXITED(status) && WEXITSTATUS(status) == 0, "it stops cleanly on SIGTERM, saving");
        // Again, from the save.
        pid = launch(argv[1], port, save);
        {
            Link c;
            expect(c.open(port), "a second server listens");
            c.command(R"({"type":"hello","id":"ash","name":"Ash"})");
            c.read(.6);
            expect(!c.snapshots.empty() && std::abs(c.snapshots.back()["self"].number("x") - x) < .5, "Ash is where they were left");
            ::close(c.fd);
        }
        ::kill(pid, SIGTERM);
        ::waitpid(pid, &status, 0);
        std::remove(save.c_str());
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAILED after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "Server smoke passed: " << checks << " checks.\n";
    return 0;
}
