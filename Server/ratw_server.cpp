// The standalone headless world server (Docs/Design/26-living-npcs.md, Phase 6): the portable game (RatwGame.h) over
// TCP (RatwLink.h). No Unreal: it starts in about a second, runs under ordinary profilers and sanitizers, and serves
// the same saves as the Unreal server. The Unreal game connects to it with -RatwServer=host:port.
//
//   ratw_server --database dev|prod          the world in the database (RATW_DATABASE_URL), as tools/live.sh runs it
//   ratw_server [--world MANIFEST] --save F  a world from files (or the built-in demo), saved to a private file
//   options: --port 7788, --bind 127.0.0.1, --dialogue URL (the NPC Mind), --dm-directory DIR (the operator bridge),
//            --dev-tools, --dev-identity,
//            --full-snapshots, --for SECONDS (stop after, saving: for tests)
//
// Exits 75 when a new release has been published and nobody is playing (tools/live.sh restarts it on the new build).
#include "RatwAccountsCore.h"
#include "RatwGame.h"
#include "RatwLink.h"
#include "RatwMotionCore.h"
#include "RatwSystemLibs.h"

#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <map>
#include <memory>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <vector>

using namespace ratw;
namespace
{
volatile std::sig_atomic_t stopping = 0;
void stop(int) { stopping = 1; }

// Replaceable frames (snapshots, motion) are dropped for a client this far behind; past the second, it is let go.
constexpr std::size_t DropReplaceableAt = 4u << 20, DisconnectAt = 32u << 20;

class Client final : public game::Connection
{
  public:
    int fd = -1;
    std::string address, in, out;
    bool closing = false, local = false;
    std::size_t dropped = 0;

    void event(const std::string& json) override { queue(link::Event, json, false); }
    void snapshot(const std::string& json) override { queue(link::Snapshot, json, true); }
    void motion(const json::Value& frame) override
    {
        const auto bytes = motion::pack(frame);
        queue(link::Motion, std::string(bytes.begin(), bytes.end()), true);
    }
    bool allowsLocalCredentials() const override { return local; }

  private:
    void queue(link::Kind kind, const std::string& raw, bool replaceable)
    {
        if (closing)
            return;
        if (replaceable && out.size() > DropReplaceableAt)
        {
            ++dropped;                              // A newer one follows; this one would only be stale.
            return;
        }
        std::vector<std::uint8_t> packed;
        if (!sys::compress(reinterpret_cast<const std::uint8_t*>(raw.data()), raw.size(), packed))
            return;
        std::string payload(4 + packed.size(), '\0');
        const auto rawLength = std::uint32_t(raw.size());
        for (int i = 0; i < 4; ++i)
            payload[std::size_t(i)] = char(rawLength >> (8 * i));
        std::memcpy(payload.data() + 4, packed.data(), packed.size());
        link::appendFrame(out, kind, payload.data(), payload.size());
        if (out.size() > DisconnectAt)
            closing = true;                         // Hopelessly behind: let it reconnect.
    }
};

void usage()
{
    std::cerr << "usage: ratw_server (--database dev|prod | [--world MANIFEST] --save FILE) [--port N] [--bind ADDR]\n"
                 "                   [--dialogue URL] [--dm-directory DIR] [--dev-tools] [--dev-identity] [--full-snapshots]\n"
                 "                   [--for SECONDS]\n";
}

void blocking(int fd, bool on)
{
    const int flags = ::fcntl(fd, F_GETFL, 0);
    ::fcntl(fd, F_SETFL, on ? flags & ~O_NONBLOCK : flags | O_NONBLOCK);
}
} // namespace

int main(int argc, char** argv)
{
    game::Options options;
    int port = 7788;
    std::string bind = "127.0.0.1";
    double runFor = -1;
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        const auto next = [&]() -> std::string {
            if (i + 1 >= argc)
            {
                usage();
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "--database") options.database = next();
        else if (a == "--world") options.worldFile = next();
        else if (a == "--save") options.savePath = next();
        else if (a == "--port") port = std::atoi(next().c_str());
        else if (a == "--bind") bind = next();
        else if (a == "--dialogue") options.dialogueEndpoint = next();
        else if (a == "--dm-directory") options.directorDirectory = next();
        else if (a == "--dev-tools") options.devTools = true;
        else if (a == "--dev-identity") options.devIdentity = true;
        else if (a == "--full-snapshots") options.fullSnapshots = true;
        else if (a == "--for") runFor = std::atof(next().c_str());
        else
        {
            usage();
            return 2;
        }
    }
    if (options.database.empty() && options.savePath.empty())
    {
        usage();
        return 2;
    }
    if (!options.database.empty())
        if (const char* url = std::getenv("RATW_DATABASE_URL"))
            options.conninfo = url;
    options.connectionLabel = "Standalone server · 20 Hz";
    std::string problem;
    if (!sys::zlibAvailable(problem) || !sys::cryptoAvailable(problem))
    {
        std::cerr << "RATW_WORLD_REJECTED: " << problem << '\n';
        return 2;
    }
    const auto started = std::chrono::steady_clock::now();
    game::Game g(options);
    g.log = [](const char* level, const std::string& text) { std::cout << (std::string(level) == "info" ? "" : std::string(level) + ": ") << text << std::endl; };
    if (!g.start(problem))
    {
        std::cerr << "RATW_WORLD_REJECTED: " << problem << '\n';
        return 2;
    }

    const int listener = ::socket(AF_INET, SOCK_STREAM, 0);
    const int yes = 1;
    ::setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(std::uint16_t(port));
    if (::inet_pton(AF_INET, bind.c_str(), &addr.sin_addr) != 1 || ::bind(listener, reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0 ||
        ::listen(listener, 64) != 0)
    {
        std::cerr << "RATW_WORLD_REJECTED: cannot listen on " << bind << ":" << port << ": " << std::strerror(errno) << '\n';
        return 2;
    }
    blocking(listener, false);
    std::signal(SIGINT, stop);
    std::signal(SIGTERM, stop);
    std::signal(SIGPIPE, SIG_IGN);
    std::cout << "RATW standalone server listening on port " << port << " (" << bind << "); ready in "
              << std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count() << " s" << std::endl;

    std::map<int, std::unique_ptr<Client>> clients;
    std::uint64_t nextId = 1;
    using Clock = std::chrono::steady_clock;
    auto nextTick = Clock::now();
    const auto until = runFor > 0 ? Clock::now() + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(runFor))
                                  : Clock::time_point::max();
    double slowest = 0, total = 0;
    std::size_t ticks = 0;
    const auto drop = [&](int fd) {
        auto found = clients.find(fd);
        if (found == clients.end())
            return;
        g.disconnect(found->second.get());
        ::close(fd);
        std::cout << "RATW_DISCONNECT " << found->second->address << " (" << clients.size() - 1 << " connected)" << std::endl;
        clients.erase(found);
    };
    while (!stopping && g.exitRequested() < 0 && Clock::now() < until)
    {
        std::vector<pollfd> fds{{listener, POLLIN, 0}};
        for (const auto& [fd, c] : clients)
            fds.push_back({fd, short(POLLIN | (c->out.empty() ? 0 : POLLOUT)), 0});
        const int wait = std::max(0, int(std::chrono::duration_cast<std::chrono::milliseconds>(nextTick - Clock::now()).count()));
        ::poll(fds.data(), fds.size(), wait);
        if (fds[0].revents & POLLIN)
            for (;;)
            {
                sockaddr_in peer{};
                socklen_t length = sizeof peer;
                const int fd = ::accept(listener, reinterpret_cast<sockaddr*>(&peer), &length);
                if (fd < 0)
                    break;
                blocking(fd, false);
                ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof yes);
                auto c = std::make_unique<Client>();
                char text[64];
                ::inet_ntop(AF_INET, &peer.sin_addr, text, sizeof text);
                c->fd = fd;
                c->address = text;
                c->local = accounts::isLoopbackAddress(c->address);
                c->id = nextId++;
                auto* raw = c.get();
                clients[fd] = std::move(c);
                std::cout << "RATW_CONNECT " << raw->address << " (" << clients.size() << " connected)" << std::endl;
                g.connect(raw);
            }
        std::vector<int> gone;
        for (std::size_t i = 1; i < fds.size(); ++i)
        {
            auto& c = *clients.at(fds[i].fd);
            if (fds[i].revents & (POLLERR | POLLHUP | POLLNVAL))
                c.closing = true;
            if (fds[i].revents & POLLIN)
            {
                char buffer[65536];
                for (;;)
                {
                    const auto n = ::recv(c.fd, buffer, sizeof buffer, 0);
                    if (n > 0)
                        c.in.append(buffer, std::size_t(n));
                    else
                    {
                        if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK))
                            c.closing = true;
                        break;
                    }
                }
                link::Kind kind;
                std::string payload;
                bool bad = false;
                while (!c.closing && link::takeFrame(c.in, kind, payload, bad))
                {
                    if (kind == link::Command && payload.size() <= link::MaxCommand)
                        g.command(&c, payload);
                    else if (kind == link::Ack && payload.size() == 9)
                    {
                        double revision;
                        std::memcpy(&revision, payload.data(), 8);
                        g.acknowledge(&c, revision, payload[8] != 0);
                    }
                    else
                        c.closing = true;           // Nothing else is ever sent by a client.
                }
                if (bad)
                    c.closing = true;
            }
            if (!c.out.empty() && (fds[i].revents & POLLOUT))
            {
                const auto n = ::send(c.fd, c.out.data(), c.out.size(), MSG_NOSIGNAL);
                if (n > 0)
                    c.out.erase(0, std::size_t(n));
                else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
                    c.closing = true;
            }
            if (c.closing)
                gone.push_back(c.fd);
        }
        for (int fd : gone)
            drop(fd);
        if (Clock::now() >= nextTick)
        {
            const auto begin = Clock::now();
            g.tick(0.05);
            const double took = std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
            slowest = std::max(slowest, took);
            total += took;
            ++ticks;
            nextTick += std::chrono::milliseconds(50);
            if (Clock::now() - nextTick > std::chrono::seconds(1))
                nextTick = Clock::now();            // Fell far behind (a stall): carry on from now, not in a rush.
            // Try to send what the tick produced now, rather than on the next wake.
            for (auto& [fd, c] : clients)
                if (!c->out.empty())
                {
                    const auto n = ::send(fd, c->out.data(), c->out.size(), MSG_NOSIGNAL);
                    if (n > 0)
                        c->out.erase(0, std::size_t(n));
                }
        }
    }
    std::vector<int> all;
    for (const auto& [fd, c] : clients)
        all.push_back(fd);
    for (int fd : all)
        drop(fd);
    g.save();
    ::close(listener);
    std::cout << "RATW standalone server stopped after " << ticks << " ticks; mean " << (ticks ? total / double(ticks) : 0)
              << " ms, slowest " << slowest << " ms" << std::endl;
    return g.exitRequested() >= 0 ? g.exitRequested() : 0;
}
