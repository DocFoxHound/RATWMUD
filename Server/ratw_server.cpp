// The game server (Docs/Design/26-living-npcs.md, Phase 6; 27-browser-client.md): the portable game (RatwGame.h) over
// one port. A browser opens http://host:port/ for the client's files (--web) and plays over a WebSocket at /ws
// (RatwWeb.h), each message one of the game's (RatwLink.h).
//
//   ratw_server --database dev|prod          the world in the database (RATW_DATABASE_URL), as tools/live.sh runs it
//   ratw_server [--world MANIFEST] --save F  a world from files (or the built-in demo), saved to a private file
//   options: --port 7788, --bind 127.0.0.1, --web DIR (the built browser client), --dialogue URL (the NPC Mind),
//            --dm-directory DIR (the operator bridge), --dev-tools, --dev-identity,
//            --full-snapshots, --for SECONDS (stop after, saving: for tests),
//            --perf-log SECONDS (where the game thread's time went, logged this often; 60 by default, 0 for never),
//            --workers N (threads finishing players' snapshots in parallel; by default the cores less two, 0 for none)
//
// Exits 75 when a new release has been published and nobody is playing (tools/live.sh restarts it on the new build).
#include "RatwAccountsCore.h"
#include "RatwGame.h"
#include "RatwLink.h"
#include "RatwMotionCore.h"
#include "RatwPerf.h"
#include "RatwSystemLibs.h"
#include "RatwWeb.h"

#include <algorithm>
#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/epoll.h>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <thread>
#include <vector>

using namespace ratw;
namespace
{
volatile std::sig_atomic_t stopping = 0;
perf::Meter meter;                                  // The game thread's time (Docs/Design/31-responsiveness.md).
void stop(int) { stopping = 1; }

// Replaceable frames (snapshots, motion) are dropped for a client this far behind; past the second, it is let go.
constexpr std::size_t DropReplaceableAt = 4u << 20, DisconnectAt = 32u << 20;

class Client final : public game::Connection
{
  public:
    enum class Mode
    {
        Http,                                       // Serving a file, or about to become a WebSocket.
        WebSocket,                                  // A browser (or a test) playing.
    };
    int fd = -1;
    std::string address, in, out;
    Mode mode = Mode::Http;
    web::Reader reader;
    bool closing = false, local = false, playing = false, finishing = false;
    bool watchingOut = false;                       // epoll also wakes for this socket being writable.
    std::size_t dropped = 0;

    void event(const std::string& json) override { queue(link::Event, json, false); }
    void snapshot(const std::string& json) override { queue(link::Snapshot, json, true); }
    void motion(const json::Value& frame) override
    {
        const auto bytes = motion::pack(frame);
        queue(link::Motion, std::string(bytes.begin(), bytes.end()), true);
    }
    bool allowsLocalCredentials() const override { return local; }
    // Sends what is queued and then closes (an HTTP response, a WebSocket closing handshake).
    void finish() { finishing = true; }

  private:
    void queue(link::Kind kind, const std::string& raw, bool replaceable)
    {
        if (closing || finishing)
            return;
        if (replaceable && out.size() > DropReplaceableAt)
        {
            ++dropped;                              // A newer one follows; this one would only be stale.
            return;
        }
        std::string payload;
        {
            perf::Scope timed(&meter, perf::Compression);
            if (!link::encode(kind, raw, payload))
                return;
        }
        meter.sent(payload.size());
        web::appendFrame(out, web::Binary, payload.data(), payload.size());
        if (out.size() > DisconnectAt)
            closing = true;                         // Hopelessly behind: let it reconnect.
    }
};

// A file of the browser client, or false. Small files only: the client is a few hundred kilobytes.
bool readFile(const std::string& path, std::string& out)
{
    struct stat info{};
    if (::stat(path.c_str(), &info) != 0 || !S_ISREG(info.st_mode) || info.st_size > (16 << 20))
        return false;
    std::ifstream file(path, std::ios::binary);
    std::ostringstream text;
    text << file.rdbuf();
    out = text.str();
    return bool(file) || file.eof();
}

void usage()
{
    std::cerr << "usage: ratw_server (--database dev|prod | [--world MANIFEST] --save FILE) [--port N] [--bind ADDR]\n"
                 "                   [--web DIR] [--dialogue URL] [--voice-data DIR] [--voice-log FILE] [--ambient-model-calls N] [--dm-directory DIR] [--dev-tools] [--dev-identity] [--full-snapshots]\n"
                 "                   [--for SECONDS] [--perf-log SECONDS] [--workers N]\n";
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
    std::string bind = "127.0.0.1", webRoot;
    double runFor = -1, perfLog = 60;
    int workers = -1;
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
        else if (a == "--web") webRoot = next();
        else if (a == "--dialogue") options.dialogueEndpoint = next();
        else if (a == "--voice-data") options.voiceData = next();
        else if (a == "--ambient-model-calls") options.ambientModelCallsPerHour = std::max(0, std::atoi(next().c_str()));
        else if (a == "--voice-log") options.voiceLog = next();
        else if (a == "--dm-directory") options.directorDirectory = next();
        else if (a == "--dev-tools") options.devTools = true;
        else if (a == "--dev-identity") options.devIdentity = true;
        else if (a == "--full-snapshots") options.fullSnapshots = true;
        else if (a == "--for") runFor = std::atof(next().c_str());
        else if (a == "--perf-log") perfLog = std::atof(next().c_str());
        else if (a == "--workers") workers = std::atoi(next().c_str());
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
    options.workerThreads = workers >= 0 ? unsigned(workers)
                                         : unsigned(std::clamp(int(std::thread::hardware_concurrency()) - 2, 0, 16));
    game::Game g(options);
    g.setMeter(&meter);
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
    std::cout << "RATW server listening on port " << port << " (" << bind << "); ready in "
              << std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count() << " s" << std::endl;

    std::map<int, std::unique_ptr<Client>> clients;
    std::uint64_t nextId = 1;
    using Clock = std::chrono::steady_clock;
    auto nextTick = Clock::now();
    const auto until = runFor > 0 ? Clock::now() + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(runFor))
                                  : Clock::time_point::max();
    double slowest = 0, total = 0;
    std::size_t ticks = 0;
    // The game thread's time: what it did between ticks (summed over the loop's passes), reported every perfLog seconds.
    double busy = 0;
    auto nextReport = perfLog > 0 ? Clock::now() + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(perfLog))
                                  : Clock::time_point::max();
    const auto drop = [&](int fd) {
        auto found = clients.find(fd);
        if (found == clients.end())
            return;
        const bool playing = found->second->playing;
        if (playing)
            g.disconnect(found->second.get());
        ::close(fd);
        const std::string address = found->second->address;
        clients.erase(found);
        if (playing)
            std::cout << "RATW_DISCONNECT " << address << " (" << clients.size() << " connected)" << std::endl;
    };
    const auto play = [&](Client& c) {
        c.playing = true;
        std::cout << "RATW_CONNECT " << c.address << std::endl;
        g.connect(&c);
    };
    // One message of the game from a client: a command or a snapshot acknowledgement. False for anything else.
    const auto receive = [&](Client& c, link::Kind kind, const std::string& payload) {
        perf::Scope timed(&meter, perf::Commands);
        if (kind == link::Command && payload.size() <= link::MaxCommand)
            g.command(&c, payload);
        else if (kind == link::Pose && payload.size() == link::PoseBytes)
        {
            std::uint32_t seq;
            float f[3];
            std::memcpy(&seq, payload.data(), 4);
            std::memcpy(f, payload.data() + 4, 12);
            g.pose(&c, seq, f[0], f[1], f[2], double(std::int8_t(payload[16])), double(std::int8_t(payload[17])));
        }
        else if (kind == link::Ping && payload.size() == link::PingBytes)
        {
            const std::string pong = char(link::Pong) + payload;
            web::appendFrame(c.out, web::Binary, pong.data(), pong.size());
        }
        else if (kind == link::Ack && payload.size() == 9)
        {
            double revision;
            std::memcpy(&revision, payload.data(), 8);
            g.acknowledge(&c, revision, payload[8] != 0);
        }
        else
            return false;                           // Nothing else is ever sent by a client.
        return true;
    };
    // An HTTP request: the game's WebSocket, or one of the client's files.
    const auto serve = [&](Client& c, const web::Request& r) {
        if (r.path == web::GamePath)
        {
            if (!web::upgradeRequested(r))
                c.out += web::response(426, "text/plain; charset=utf-8", "The game is played over a WebSocket.\n",
                                       "Upgrade: websocket\r\n");
            else if (!web::originAllowed(r))
                c.out += web::response(403, "text/plain; charset=utf-8", "This page may not open the game.\n");
            else if (const std::string accept = web::acceptKey(r.header("sec-websocket-key")); accept.empty())
                c.out += web::response(500, "text/plain; charset=utf-8", "The server cannot accept WebSockets.\n");
            else
            {
                c.out += web::handshake(accept);
                c.mode = Client::Mode::WebSocket;
                play(c);
                return;
            }
            c.finish();
            return;
        }
        std::string relative, body;
        if (r.method != "GET" && r.method != "HEAD")
            c.out += web::response(405, "text/plain; charset=utf-8", "Only GET.\n", "Allow: GET, HEAD\r\n");
        else if (webRoot.empty() || !web::filePath(r.path, relative) || !readFile(webRoot + "/" + relative, body))
            c.out += web::response(404, "text/plain; charset=utf-8", webRoot.empty() ? "This server has no client to serve (--web).\n" : "Not found.\n");
        else
        {
            // Built assets have content hashes in their names, so they never change; the page itself always might.
            const bool immutable = relative.rfind("assets/", 0) == 0;
            std::string reply = web::response(200, web::contentType(relative), body,
                                              immutable ? "Cache-Control: public, max-age=31536000, immutable\r\n" : "Cache-Control: no-cache\r\n");
            if (r.method == "HEAD")
                reply.resize(reply.size() - body.size());
            c.out += reply;
        }
        c.finish();
    };
    // Everything a client has sent so far, by what kind of connection it turned out to be.
    const auto handle = [&](Client& c) {
        if (c.mode == Client::Mode::Http && !c.finishing)
        {
            web::Request r;
            const int got = web::takeRequest(c.in, r);
            if (got < 0)
            {
                c.out += web::response(400, "text/plain; charset=utf-8", "Bad request.\n");
                c.finish();
            }
            else if (got > 0)
                serve(c, r);
        }
        if (c.mode == Client::Mode::WebSocket)
        {
            web::Opcode op;
            std::string payload;
            int got;
            while (!c.closing && !c.finishing && (got = web::takeMessage(c.in, c.reader, op, payload, link::MaxCommand + 1)) != 0)
            {
                if (got < 0)
                {
                    const char status[2] = {char(1002 >> 8), char(1002 & 255)};     // Protocol error.
                    web::appendFrame(c.out, web::Close, status, 2);
                    c.finish();
                }
                else if (op == web::Ping)
                    web::appendFrame(c.out, web::Pong, payload.data(), payload.size());
                else if (op == web::Close)
                {
                    web::appendFrame(c.out, web::Close, payload.data(), std::min<std::size_t>(payload.size(), 2));
                    c.finish();
                }
                else if (op == web::Binary && !payload.empty() && receive(c, link::Kind(std::uint8_t(payload[0])), payload.substr(1)))
                    continue;
                else if (op != web::Pong)
                {
                    const char status[2] = {char(1003 >> 8), char(1003 & 255)};     // Unsupported data.
                    web::appendFrame(c.out, web::Close, status, 2);
                    c.finish();
                }
            }
        }
    };
    // epoll (doc 31, Phase 4): each wake costs only the sockets with something to do, not every socket every time.
    const int events = ::epoll_create1(EPOLL_CLOEXEC);
    const auto watch = [&](int fd, bool out, int op) {
        epoll_event e{};
        e.events = EPOLLIN | (out ? EPOLLOUT : 0u);
        e.data.fd = fd;
        ::epoll_ctl(events, op, fd, &e);
    };
    watch(listener, false, EPOLL_CTL_ADD);
    // Writes what it can now; anything left wakes epoll when the socket can take more.
    const auto flush = [&](Client& c) {
        if (!c.out.empty())
        {
            const auto n = ::send(c.fd, c.out.data(), c.out.size(), MSG_NOSIGNAL);
            if (n > 0)
                c.out.erase(0, std::size_t(n));
            else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
                c.closing = true;
        }
        if (c.out.empty() != !c.watchingOut && !c.closing)
        {
            c.watchingOut = !c.out.empty();
            watch(c.fd, c.watchingOut, EPOLL_CTL_MOD);
        }
    };
    std::vector<epoll_event> ready(256);
    while (!stopping && g.exitRequested() < 0 && Clock::now() < until)
    {
        const int wait = std::max(0, int(std::chrono::duration_cast<std::chrono::milliseconds>(nextTick - Clock::now()).count()));
        const int count = ::epoll_wait(events, ready.data(), int(ready.size()), wait);
        const auto woke = Clock::now();
        bool ticked = false;
        {
        perf::Scope sockets(&meter, perf::Sockets);
        std::vector<int> gone;
        for (int k = 0; k < count; ++k)
        {
            const int fd = ready[std::size_t(k)].data.fd;
            const auto flags = ready[std::size_t(k)].events;
            if (fd == listener)
            {
                for (;;)
                {
                    sockaddr_in peer{};
                    socklen_t length = sizeof peer;
                    const int accepted = ::accept(listener, reinterpret_cast<sockaddr*>(&peer), &length);
                    if (accepted < 0)
                        break;
                    blocking(accepted, false);
                    ::setsockopt(accepted, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof yes);
                    auto c = std::make_unique<Client>();
                    char text[64];
                    ::inet_ntop(AF_INET, &peer.sin_addr, text, sizeof text);
                    c->fd = accepted;
                    c->address = text;
                    c->local = accounts::isLoopbackAddress(c->address);
                    c->id = nextId++;
                    watch(accepted, false, EPOLL_CTL_ADD);
                    clients[accepted] = std::move(c);   // It joins the game once it becomes a WebSocket (serve()).
                }
                continue;
            }
            const auto found = clients.find(fd);
            if (found == clients.end())
                continue;
            auto& c = *found->second;
            if (flags & (EPOLLERR | EPOLLHUP))
                c.closing = true;
            if (flags & EPOLLIN)
            {
                char buffer[65536];
                for (;;)
                {
                    const auto n = ::recv(c.fd, buffer, sizeof buffer, 0);
                    if (n > 0)
                    {
                        c.in.append(buffer, std::size_t(n));
                        meter.received(std::size_t(n));
                    }
                    else
                    {
                        if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK))
                            c.closing = true;
                        break;
                    }
                }
                if (!c.closing)
                    handle(c);
            }
            if (!c.closing)
                flush(c);                          // A reply now, and anything waiting for the socket.
            if (c.finishing && c.out.empty())
                c.closing = true;
            if (c.closing)
                gone.push_back(c.fd);
        }
        for (int fd : gone)
            drop(fd);
        }
        if (Clock::now() >= nextTick)
        {
            ticked = true;
            const auto begin = Clock::now();
            g.tick(0.05);
            const double took = std::chrono::duration<double, std::milli>(Clock::now() - begin).count();
            slowest = std::max(slowest, took);
            total += took;
            ++ticks;
            nextTick += std::chrono::milliseconds(50);
            if (Clock::now() - nextTick > std::chrono::seconds(1))
                nextTick = Clock::now();            // Fell far behind (a stall): carry on from now, not in a rush.
            // Send what the tick produced now, rather than on the next wake; what can't go yet waits on epoll.
            perf::Scope sockets(&meter, perf::Sockets);
            std::vector<int> stuck;
            for (auto& [fd, c] : clients)
            {
                flush(*c);
                if (c->closing)
                    stuck.push_back(fd);
            }
            for (int fd : stuck)
                drop(fd);
        }
        busy += std::chrono::duration<double, std::milli>(Clock::now() - woke).count();
        if (ticked)
        {
            meter.pass(busy, true);
            busy = 0;
        }
        if (Clock::now() >= nextReport)
        {
            nextReport = Clock::now() + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(perfLog));
            std::size_t playing = 0;
            for (const auto& [fd, c] : clients)
                playing += c->playing;
            for (const auto& line : perf::report(meter.take(), playing))
                std::cout << line << '\n';
            std::cout << perf::worldLine(g.world().tickProfile()) << std::endl;
            g.world().resetTickProfile();
        }
    }
    std::vector<int> all;
    for (const auto& [fd, c] : clients)
        all.push_back(fd);
    for (int fd : all)
        drop(fd);
    g.save();
    ::close(events);
    ::close(listener);
    std::cout << "RATW server stopped after " << ticks << " ticks; mean " << (ticks ? total / double(ticks) : 0)
              << " ms, slowest " << slowest << " ms" << std::endl;
    return g.exitRequested() >= 0 ? g.exitRequested() : 0;
}
