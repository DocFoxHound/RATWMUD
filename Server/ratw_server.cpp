// The game server (Docs/Design/26-living-npcs.md, Phase 6; 27-browser-client.md): the portable game (RatwGame.h) over
// one port. A browser opens http://host:port/ for the client's files (--web) and plays over a WebSocket at /ws
// (RatwWeb.h), each message one of the game's (RatwLink.h).
//
//   ratw_server --database dev|prod          the world in the database (RATW_DATABASE_URL), as tools/live.sh runs it
//   ratw_server [--world MANIFEST] --save F  a world from files (or the built-in demo), saved to a private file
//   ratw_server --world-export DIR --save F  a world build exported as files (tools/world_build.py export), with its
//                                            residents as built: for tests and trying a build offline
//   options: --port 7788, --bind 127.0.0.1, --web DIR (the built browser client), --dialogue URL (the NPC Mind),
//            --dm-directory DIR (the operator bridge), --dev-tools, --dev-identity,
//            --open-tiers (every Gift tier open to every account: doc 49),
//            --full-snapshots, --for SECONDS (stop after, saving: for tests),
//            --perf-log SECONDS (where the game thread's time went, logged this often; 60 by default, 0 for never),
//            --workers N (threads finishing players' snapshots in parallel; by default the cores less two, 0 for none),
//            --port 0 (any free port, printed as it starts),
//            --speed N (fast-forward: N ticks where there was one, 1 to 1000; nothing is skipped, the world just runs
//            faster, as fast as the machine can if it can't keep up; /speed in the Dev Console changes it; Game::setSpeed)
//   --scratch: a scratch server (game::Options::scratch): it reads the world and its save but writes nothing, and
//            needn't own the world (so it runs beside the real server). Whoever starts it stops it when done; it stops
//            by itself after an hour unless --for says otherwise. --idle-exit SECONDS (any server) stops it once nobody
//            has been connected that long. For sessions and tools that want a server of their own (tools/scratch.sh).
//
// Two threads (doc 31, Phase 4.11): the network thread owns the sockets (accepting, reading, the WebSocket and HTTP
// parsing, the client's files, pings, writing); the game thread runs the game, taking each client's messages from a
// queue the network thread fills, and writing its replies into each client's buffer for the network thread to send.
//
// Exits 75 when a new release has been published and nobody is playing (tools/live.sh restarts it on the new build).
#include "RatwAccountsCore.h"
#include "RatwGame.h"
#include "RatwLink.h"
#include "RatwMotionCore.h"
#include "RatwPack.h"
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
#include <set>
#include <string>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include <atomic>
#include <deque>
#include <mutex>
#include <poll.h>
#include <sys/eventfd.h>
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

class Network;
bool readFile(const std::string& path, std::string& out);
void blocking(int fd, bool on);

class Client final : public game::Connection
{
  public:
    enum class Mode
    {
        Http,                                       // Serving a file, or about to become a WebSocket.
        WebSocket,                                  // A browser (or a test) playing.
    };
    // The network thread's alone.
    int fd = -1;
    std::string in;
    Mode mode = Mode::Http;
    web::Reader reader;
    bool watchingOut = false;                       // epoll also wakes for this socket being writable.
    // Set once at accept.
    std::string address;
    bool local = false;
    // The game thread's alone: it has been shown the lobby (Game::connect) and not yet let go.
    bool playing = false;
    // Shared, under `lock`: what is waiting to be sent, and whether the connection is ending.
    std::mutex lock;
    std::string out;
    std::atomic<bool> closing{false}, finishing{false};
    std::atomic<std::size_t> dropped{0};
    Network* network = nullptr;

    void event(const std::string& json) override { queue(link::Event, json, false); }
    void snapshot(const std::string& json) override { queue(link::Snapshot, json, true); }
    // Packed, not JSON (doc 31, Phase 4.8): quicker to write and to read, and smaller.
    void snapshotValue(const json::Value& root) override { queue(link::PackedSnapshot, pack::encode(root), true); }
    void motion(const json::Value& frame) override
    {
        const auto bytes = motion::pack(frame);
        queue(link::Motion, std::string(bytes.begin(), bytes.end()), true);
    }
    bool allowsLocalCredentials() const override { return local; }
    // Sends what is queued and then closes (an HTTP response, a WebSocket closing handshake).
    void finish() { finishing = true; }
    // Raw bytes (a frame or an HTTP response) for the network thread to send.
    void append(const std::string& bytes);

  private:
    void queue(link::Kind kind, const std::string& raw, bool replaceable);
};

// The sockets, on a thread of their own.
class Network
{
  public:
    struct Message
    {
        enum Type
        {
            Connected,                              // A WebSocket opened: the game shows it the lobby.
            Received,                               // One of the game's messages from it.
            Gone,                                   // It has closed: the game lets it go.
        } type;
        std::shared_ptr<Client> client;
        link::Kind kind = link::Command;
        std::string payload;
    };

    Network(int listener, std::string webRoot) : listener_(listener), webRoot_(std::move(webRoot))
    {
        events_ = ::epoll_create1(EPOLL_CLOEXEC);
        wake_ = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        gameWake_ = ::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
        watch(listener_, false, EPOLL_CTL_ADD);
        watch(wake_, false, EPOLL_CTL_ADD);
    }
    ~Network()
    {
        stopNow();
        ::close(events_);
        ::close(wake_);
        ::close(gameWake_);
    }
    void start() { thread_ = std::thread([this] { run(); }); }
    void stopNow()
    {
        if (!thread_.joinable())
            return;
        stop_ = true;
        signal(wake_);
        thread_.join();
    }
    // Waits until a message is queued for the game, or `ms` pass.
    void waitForGame(int ms)
    {
        pollfd p{gameWake_, POLLIN, 0};
        if (::poll(&p, 1, std::max(0, ms)) > 0)
        {
            std::uint64_t n;
            [[maybe_unused]] const auto r = ::read(gameWake_, &n, sizeof n);
        }
    }
    std::vector<Message> take()
    {
        std::lock_guard<std::mutex> guard(inboxLock_);
        std::vector<Message> out;
        out.swap(inbox_);
        return out;
    }
    // A client has output (or is ending): the network thread sends it.
    void wantWrite(Client* c)
    {
        {
            std::lock_guard<std::mutex> guard(writeLock_);
            writable_.push_back(c->fd);
        }
        signal(wake_);
    }
    // Every client still open, after the thread has stopped (to let them go as the server stops).
    std::vector<std::shared_ptr<Client>> remaining()
    {
        std::vector<std::shared_ptr<Client>> out;
        for (auto& [fd, c] : clients_)
            out.push_back(c);
        return out;
    }
    void closeAll()
    {
        for (auto& [fd, c] : clients_)
            ::close(fd);
        clients_.clear();
    }
    std::size_t connected() const { return connected_.load(); }

  private:
    static void signal(int fd)
    {
        const std::uint64_t one = 1;
        [[maybe_unused]] const auto r = ::write(fd, &one, sizeof one);
    }
    void watch(int fd, bool out, int op)
    {
        epoll_event e{};
        e.events = EPOLLIN | (out ? EPOLLOUT : 0u);
        e.data.fd = fd;
        ::epoll_ctl(events_, op, fd, &e);
    }
    void post(Message m)
    {
        {
            std::lock_guard<std::mutex> guard(inboxLock_);
            inbox_.push_back(std::move(m));
        }
        signal(gameWake_);
    }
    // Writes what it can; anything left waits for the socket to be writable.
    void flush(Client& c)
    {
        bool left;
        {
            std::lock_guard<std::mutex> guard(c.lock);
            if (!c.out.empty())
            {
                const auto n = ::send(c.fd, c.out.data(), c.out.size(), MSG_NOSIGNAL);
                if (n > 0)
                    c.out.erase(0, std::size_t(n));
                else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
                    c.closing = true;
            }
            left = !c.out.empty();
            if (c.finishing && !left)
                c.closing = true;
        }
        if (left != c.watchingOut && !c.closing)
        {
            c.watchingOut = left;
            watch(c.fd, left, EPOLL_CTL_MOD);
        }
    }
    void drop(int fd)
    {
        const auto found = clients_.find(fd);
        if (found == clients_.end())
            return;
        auto c = found->second;
        c->closing = true;
        ::close(fd);
        clients_.erase(found);
        if (c->mode == Client::Mode::WebSocket)
        {
            --connected_;
            post({Message::Gone, c});
        }
    }
    // An HTTP request: the game's WebSocket, or one of the client's files.
    void serve(const std::shared_ptr<Client>& c, const web::Request& r)
    {
        if (r.path == web::GamePath)
        {
            if (!web::upgradeRequested(r))
                c->append(web::response(426, "text/plain; charset=utf-8", "The game is played over a WebSocket.\n", "Upgrade: websocket\r\n"));
            else if (!web::originAllowed(r))
                c->append(web::response(403, "text/plain; charset=utf-8", "This page may not open the game.\n"));
            else if (const std::string accept = web::acceptKey(r.header("sec-websocket-key")); accept.empty())
                c->append(web::response(500, "text/plain; charset=utf-8", "The server cannot accept WebSockets.\n"));
            else
            {
                c->append(web::handshake(accept));
                c->mode = Client::Mode::WebSocket;
                ++connected_;
                post({Message::Connected, c});
                return;
            }
            c->finish();
            return;
        }
        std::string relative, body;
        if (r.method != "GET" && r.method != "HEAD")
            c->append(web::response(405, "text/plain; charset=utf-8", "Only GET.\n", "Allow: GET, HEAD\r\n"));
        else if (webRoot_.empty() || !web::filePath(r.path, relative) || !readFile(webRoot_ + "/" + relative, body))
            c->append(web::response(404, "text/plain; charset=utf-8", webRoot_.empty() ? "This server has no client to serve (--web).\n" : "Not found.\n"));
        else
        {
            // Built assets have content hashes in their names, so they never change; the page itself always might.
            const bool immutable = relative.rfind("assets/", 0) == 0;
            std::string reply = web::response(200, web::contentType(relative), body,
                                              immutable ? "Cache-Control: public, max-age=31536000, immutable\r\n" : "Cache-Control: no-cache\r\n");
            if (r.method == "HEAD")
                reply.resize(reply.size() - body.size());
            c->append(reply);
        }
        c->finish();
    }
    // Everything a client has sent so far, by what kind of connection it turned out to be.
    void handle(const std::shared_ptr<Client>& c)
    {
        if (c->mode == Client::Mode::Http && !c->finishing)
        {
            web::Request r;
            const int got = web::takeRequest(c->in, r);
            if (got < 0)
            {
                c->append(web::response(400, "text/plain; charset=utf-8", "Bad request.\n"));
                c->finish();
            }
            else if (got > 0)
                serve(c, r);
        }
        if (c->mode != Client::Mode::WebSocket)
            return;
        web::Opcode op;
        std::string payload;
        int got;
        while (!c->closing && !c->finishing && (got = web::takeMessage(c->in, c->reader, op, payload, link::MaxCommand + 1)) != 0)
        {
            const auto end = [&](int status) {
                const char code[2] = {char(status >> 8), char(status & 255)};
                std::string frame;
                web::appendFrame(frame, web::Close, code, 2);
                c->append(frame);
                c->finish();
            };
            if (got < 0)
                end(1002);                         // Protocol error.
            else if (op == web::Ping)
            {
                std::string frame;
                web::appendFrame(frame, web::Pong, payload.data(), payload.size());
                c->append(frame);
            }
            else if (op == web::Close)
            {
                std::string frame;
                web::appendFrame(frame, web::Close, payload.data(), std::min<std::size_t>(payload.size(), 2));
                c->append(frame);
                c->finish();
            }
            else if (op == web::Binary && !payload.empty())
            {
                const auto kind = link::Kind(std::uint8_t(payload[0]));
                std::string body = payload.substr(1);
                if (kind == link::Ping && body.size() == link::PingBytes)
                {
                    // Answered here, at once: the latency overlay measures the line, not the game's queue.
                    std::string frame, pong = char(link::Pong) + body;
                    web::appendFrame(frame, web::Binary, pong.data(), pong.size());
                    c->append(frame);
                }
                else if ((kind == link::Command && body.size() <= link::MaxCommand) || (kind == link::Pose && body.size() == link::PoseBytes) ||
                         (kind == link::Ack && body.size() == 9))
                    post({Message::Received, c, kind, std::move(body)});
                else
                    end(1003);                     // Nothing else is ever sent by a client.
            }
            else if (op != web::Pong)
                end(1003);                         // Unsupported data.
        }
    }
    void run()
    {
        std::vector<epoll_event> ready(256);
        while (!stop_)
        {
            const int count = ::epoll_wait(events_, ready.data(), int(ready.size()), 200);
            std::vector<int> gone;
            for (int k = 0; k < count; ++k)
            {
                const int fd = ready[std::size_t(k)].data.fd;
                const auto flags = ready[std::size_t(k)].events;
                if (fd == wake_)
                {
                    std::uint64_t n;
                    [[maybe_unused]] const auto r = ::read(wake_, &n, sizeof n);
                    std::vector<int> due;
                    {
                        std::lock_guard<std::mutex> guard(writeLock_);
                        due.swap(writable_);
                    }
                    for (int w : due)
                        if (const auto found = clients_.find(w); found != clients_.end())
                        {
                            flush(*found->second);
                            if (found->second->closing)
                                gone.push_back(w);
                        }
                    continue;
                }
                if (fd == listener_)
                {
                    for (;;)
                    {
                        sockaddr_in peer{};
                        socklen_t length = sizeof peer;
                        const int accepted = ::accept(listener_, reinterpret_cast<sockaddr*>(&peer), &length);
                        if (accepted < 0)
                            break;
                        blocking(accepted, false);
                        const int yes = 1;
                        ::setsockopt(accepted, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof yes);
                        auto c = std::make_shared<Client>();
                        char text[64];
                        ::inet_ntop(AF_INET, &peer.sin_addr, text, sizeof text);
                        c->fd = accepted;
                        c->address = text;
                        c->local = accounts::isLoopbackAddress(c->address);
                        c->id = nextId_++;
                        c->network = this;
                        watch(accepted, false, EPOLL_CTL_ADD);
                        clients_[accepted] = c;    // It joins the game once it becomes a WebSocket (serve()).
                    }
                    continue;
                }
                const auto found = clients_.find(fd);
                if (found == clients_.end())
                    continue;
                auto c = found->second;
                if (flags & (EPOLLERR | EPOLLHUP))
                    c->closing = true;
                if (flags & EPOLLIN)
                {
                    char buffer[65536];
                    for (;;)
                    {
                        const auto n = ::recv(c->fd, buffer, sizeof buffer, 0);
                        if (n > 0)
                        {
                            c->in.append(buffer, std::size_t(n));
                            meter.received(std::size_t(n));
                        }
                        else
                        {
                            if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK))
                                c->closing = true;
                            break;
                        }
                    }
                    if (!c->closing)
                        handle(c);
                }
                if (!c->closing)
                    flush(*c);                     // A reply now, and anything waiting for the socket.
                if (c->closing)
                    gone.push_back(fd);
            }
            for (int fd : gone)
                drop(fd);
        }
    }

    int listener_, events_ = -1, wake_ = -1, gameWake_ = -1;
    std::string webRoot_;
    std::map<int, std::shared_ptr<Client>> clients_;        // The network thread's.
    std::uint64_t nextId_ = 1;
    std::atomic<std::size_t> connected_{0};
    std::mutex inboxLock_, writeLock_;
    std::vector<Message> inbox_;
    std::vector<int> writable_;
    std::atomic<bool> stop_{false};
    std::thread thread_;
};

void Client::append(const std::string& bytes)
{
    bool wasEmpty;
    {
        std::lock_guard<std::mutex> guard(lock);
        wasEmpty = out.empty();
        out += bytes;
    }
    if (wasEmpty && network)
        network->wantWrite(this);
}

void Client::queue(link::Kind kind, const std::string& raw, bool replaceable)
{
    if (closing || finishing)
        return;
    std::string payload;
    {
        perf::Scope timed(&meter, perf::Compression);
        if (!link::encode(kind, raw, payload))
            return;
    }
    std::string frame;
    web::appendFrame(frame, web::Binary, payload.data(), payload.size());
    bool wasEmpty, ending = false;
    {
        std::lock_guard<std::mutex> guard(lock);
        if (replaceable && out.size() > DropReplaceableAt)
        {
            ++dropped;                             // A newer one follows; this one would only be stale.
            return;
        }
        wasEmpty = out.empty();
        out += frame;
        if (out.size() > DisconnectAt)
            ending = true;                         // Hopelessly behind: let it reconnect.
    }
    meter.sent(payload.size());
    if (ending)
        closing = true;
    if ((wasEmpty || ending) && network)
        network->wantWrite(this);
}

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
    std::cerr << "usage: ratw_server (--database dev|prod | [--world MANIFEST | --world-export DIR] --save FILE) [--port N] [--bind ADDR]\n"
                 "                   [--web DIR] [--dialogue URL] [--voice-data DIR] [--voice-log FILE] [--ambient-model-calls N] [--dm-directory DIR] [--dev-tools] [--dev-identity] [--open-tiers] [--full-snapshots]\n"
                 "                   [--for SECONDS] [--perf-log SECONDS] [--workers N] [--speed N] [--scratch [--idle-exit SECONDS]]\n";
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
    double runFor = -1, perfLog = 60, idleExit = -1;
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
        else if (a == "--world-export") options.worldExport = next();
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
        else if (a == "--open-tiers") options.openTiers = true;   // Every Gift tier open to every account (doc 49).
        else if (a == "--dev-identity") options.devIdentity = true;
        else if (a == "--full-snapshots") options.fullSnapshots = true;
        else if (a == "--for") runFor = std::atof(next().c_str());
        else if (a == "--perf-log") perfLog = std::atof(next().c_str());
        else if (a == "--workers") workers = std::atoi(next().c_str());
        else if (a == "--scratch") options.scratch = true;
        else if (a == "--idle-exit") idleExit = std::atof(next().c_str());
        else if (a == "--speed") options.speed = std::atof(next().c_str());
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
    if (options.scratch && runFor <= 0)
        runFor = 3600;                              // A scratch server forgotten stops within the hour (--for to change it).
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
    if (port == 0)
    {
        // Any free port: say which.
        socklen_t length = sizeof addr;
        if (::getsockname(listener, reinterpret_cast<sockaddr*>(&addr), &length) == 0)
            port = ntohs(addr.sin_port);
    }
    blocking(listener, false);
    std::signal(SIGINT, stop);
    std::signal(SIGTERM, stop);
    std::signal(SIGPIPE, SIG_IGN);
    Network network(listener, webRoot);
    network.start();
    std::cout << "RATW server listening on port " << port << " (" << bind << "); ready in "
              << std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count() << " s" << std::endl;
    if (g.speed() > 1)
        std::cout << "RATW speed " << g.speed() << "x: the world runs " << g.speed() << " times as fast as real time" << std::endl;

    using Clock = std::chrono::steady_clock;
    auto nextTick = Clock::now();
    const auto until = runFor > 0 ? Clock::now() + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(runFor))
                                  : Clock::time_point::max();
    double slowest = 0, total = 0;
    std::size_t ticks = 0;
    // The game thread's time: what it did between ticks (summed over its passes), reported every perfLog seconds.
    double busy = 0;
    auto nextReport = perfLog > 0 ? Clock::now() + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(perfLog))
                                  : Clock::time_point::max();
    // One message of the game from a client: a command, a pose or a snapshot acknowledgement.
    const auto receive = [&](Client& c, link::Kind kind, const std::string& payload) {
        perf::Scope timed(&meter, perf::Commands);
        if (kind == link::Command)
            g.command(&c, payload);
        else if (kind == link::Pose)
        {
            std::uint32_t seq;
            float f[3];
            std::memcpy(&seq, payload.data(), 4);
            std::memcpy(f, payload.data() + 4, 12);
            g.pose(&c, seq, f[0], f[1], f[2], double(std::int8_t(payload[16])), double(std::int8_t(payload[17])));
        }
        else if (kind == link::Ack)
        {
            double revision;
            std::memcpy(&revision, payload.data(), 8);
            g.acknowledge(&c, revision, payload[8] != 0);
        }
    };
    std::size_t playing = 0;
    const auto letGo = [&](Client& c) {
        if (!c.playing)
            return;
        c.playing = false;
        --playing;
        g.disconnect(&c);
        std::cout << "RATW_DISCONNECT " << c.address << " (" << playing << " connected)" << std::endl;
    };
    // The clients the game knows (for the health tracker's slow connections), and how many long ticks it has recorded
    // this minute (at most SpikesAMinute: a stall that lasts doesn't fill the table).
    std::set<std::shared_ptr<Client>> live;
    constexpr int SpikesAMinute = 30;
    int spikes = 0;
    // When a client was last connected (or the server started): with --idle-exit, a server left idle stops.
    auto lastPlayed = Clock::now();
    bool idleStop = false;
    while (!stopping && g.exitRequested() < 0 && Clock::now() < until)
    {
        if (playing > 0)
            lastPlayed = Clock::now();
        else if (idleExit > 0 && Clock::now() - lastPlayed > std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(idleExit)))
        {
            idleStop = true;
            break;
        }
        network.waitForGame(int(std::chrono::duration_cast<std::chrono::milliseconds>(nextTick - Clock::now()).count()));
        const auto woke = Clock::now();
        bool ticked = false;
        for (auto& m : network.take())
        {
            auto& c = *m.client;
            if (m.type == Network::Message::Connected)
            {
                live.insert(m.client);
                c.playing = true;
                ++playing;
                std::cout << "RATW_CONNECT " << c.address << std::endl;
                g.connect(&c);
            }
            else if (m.type == Network::Message::Gone)
            {
                letGo(c);
                live.erase(m.client);
            }
            else if (c.playing && !c.closing)
                receive(c, m.kind, m.payload);
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
            // 50 ms of real time a tick, less when the world is fast-forwarded (Game::setSpeed).
            nextTick += std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(0.05 / g.speed()));
            if (Clock::now() - nextTick > std::chrono::seconds(1))
                nextTick = Clock::now();            // Fell far behind (a stall): carry on from now, not in a rush.
        }
        busy += std::chrono::duration<double, std::milli>(Clock::now() - woke).count();
        if (ticked)
        {
            meter.pass(busy, true);
            if (busy >= game::Game::SpikeMs && spikes < SpikesAMinute)
            {
                ++spikes;
                g.healthSpike(meter.lastPass());
            }
            busy = 0;
        }
        if (Clock::now() >= nextReport)
        {
            nextReport = Clock::now() + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(perfLog));
            const auto window = meter.take();
            for (const auto& line : perf::report(window, playing))
                std::cout << line << '\n';
            std::cout << perf::worldLine(g.world().tickProfile()) << std::endl;
            game::Game::Backlog backlog;
            for (const auto& c : live)
            {
                std::lock_guard<std::mutex> guard(c->lock);
                backlog.largest = std::max(backlog.largest, c->out.size());
                backlog.slow += c->out.size() > game::Game::Backlog::SlowBytes;
                backlog.dropped += c->dropped.load();
            }
            g.healthWindow(window, playing, backlog);
            g.world().resetTickProfile();
            spikes = 0;
        }
    }
    network.stopNow();
    for (auto& m : network.take())                  // Messages it queued before stopping: only the leavings matter.
        if (m.type == Network::Message::Gone)
            letGo(*m.client);
    for (auto& c : network.remaining())
        letGo(*c);
    network.closeAll();
    g.save();                                       // (A scratch server's store keeps nothing.)
    ::close(listener);
    if (idleStop)
        std::cout << "RATW_IDLE nobody connected for " << idleExit << " s: stopping." << std::endl;
    std::cout << "RATW server stopped after " << ticks << " ticks; mean " << (ticks ? total / double(ticks) : 0)
              << " ms, slowest " << slowest << " ms" << std::endl;
    return g.exitRequested() >= 0 ? g.exitRequested() : 0;
}
