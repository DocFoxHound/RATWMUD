// Newcomers (Docs/Design/52-newcomers.md, Phase 1): where a new character arrives, and the newcomer flag. The rolling
// means (equal counts go to Upper Accord; a steady crowd in Ser Ferro wins; one busy minute doesn't); the thresholds;
// through the game, on a strip of three towns named as the start towns are: the busiest preselected, a first
// character free to choose another (the user, 2026-10-07), a forged start refused, each placed where it chose (the
// spawn for the capital, beside the market merchant elsewhere) without moving the spawn or the capital; a world of one
// town starting everyone at its spawn; the mark, the card and the residents' briefing; graduating for good, kept
// across a restart.
#include "RatwGame.h"
#include "RatwNewcomers.h"
#include "RatwPeople.h"
#include "RatwPractice.h"
#include "RatwVoice.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <arpa/inet.h>
#include <atomic>
#include <mutex>
#include <netinet/in.h>
#include <sys/socket.h>
#include <chrono>
#include <thread>

using namespace ratw;
namespace fs = std::filesystem;
namespace
{
int checks = 0;
void expect(bool condition, const std::string& message)
{
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}

json::Value parsed(const std::string& text)
{
    json::Value v;
    std::string error;
    expect(json::parse(text, v, error), "JSON: " + error);
    return v;
}

struct Client final : game::Connection
{
    std::vector<json::Value> events, snapshots;
    sections::Cache cache;
    void event(const std::string& text) override { events.push_back(parsed(text)); }
    void snapshot(const std::string& text) override
    {
        auto v = parsed(text);
        expect(sections::fill(v, cache), "a snapshot can be filled");
        snapshots.push_back(v);
    }
    void motion(const json::Value&) override {}
    bool allowsLocalCredentials() const override { return true; }
    const json::Value* last(const std::string& type) const
    {
        for (auto it = events.rbegin(); it != events.rend(); ++it)
            if (it->string("type") == type)
                return &*it;
        return nullptr;
    }
    const json::Value* seen(const std::string& id) const
    {
        if (snapshots.empty())
            return nullptr;
        for (const auto& e : snapshots.back().array("entities"))
            if (e.string("id") == id)
                return &e;
        return nullptr;
    }
};

bool contains(const std::string& text, const std::string& part) { return text.find(part) != std::string::npos; }

std::string cmd(std::initializer_list<std::pair<const char*, json::Value>> fields)
{
    auto o = json::Value::object();
    for (const auto& [k, v] : fields)
        o.add(k, v);
    return json::dump(o);
}

json::Value appearance(int colour, const char* sex = "female")
{
    auto a = parsed(R"({"species": "timber", "sex": "female", "stature": "average", "pattern": "solid", "baseColor": 3, "gradientColor": 1,
                        "markingColor": 5, "gradientAmount": 0.5, "patternAmount": 0.5})");
    a.set("baseColor", colour);
    a.set("sex", sex);
    return a;
}

// --------------------------------------------------------------------------- A strip of three towns, on disk

constexpr int Side = 16;
std::string cellId(int i) { return "c_" + std::to_string(i) + "_0"; }

std::string resident(const std::string& who, const std::string& name, const std::string& role, const std::string& work,
                     int home, double hx, int at, double wx)
{
    std::ostringstream r;
    r << "resident \"" << who << "\" \"" << name << "\" \"" << role << "\" \"" << work << "\" \"Someone.\" \"Hello.\" 30 \"timber\" "
      << "\"female\" \"average\" \"saddle\" 3 1 5 1 1 8 17 \"-\" 40 0 1 \"" << cellId(home) << "\" " << hx << " 4.5 \"" << cellId(at)
      << "\" " << wx << " 8.5 \"" << cellId(home) << "\" " << hx << " 5.5\n";
    return r.str();
}

// Nine cells in a row: Upper Accord (the capital, with the spawn) in the first two, wild country, Ser Ferro in the
// middle, Ridgemere at the far end. Each town's first cell is where its people live, its second its market. Written as
// a world export (world.ratw, cells/ID.cell, seams/ID) for Options::worldExport.
std::string writeStrip(const fs::path& root)
{
    const std::string layout = "UU..SS.RR";
    fs::remove_all(root);
    fs::create_directories(root / "cells");
    fs::create_directories(root / "seams");
    std::map<std::string, std::string> seams;
    std::ostringstream m;
    m << "RATW_WORLD 3\n";
    const int cells = int(layout.size());
    int seam = 0;
    for (int i = 0; i < cells; ++i)
    {
        std::ofstream cell(root / "cells" / (cellId(i) + ".cell"));
        cell << "id: " << cellId(i) << "\nname: Stretch " << i << "\ndescription: Open ground.\nworld: " << i * Side
             << " 0 0\noutdoors: true\nweather: clear\nsize: " << Side << ' ' << Side << "\ngrid:\n";
        for (int y = 0; y < Side; ++y)
            cell << std::string(Side, '.') << '\n';
        m << "area \"" << cellId(i) << "\"\n";
        if (i + 1 < cells)
            for (int k = 0; k < Side; ++k)
            {
                const std::string a = "seam_" + std::to_string(seam) + "_a", b = "seam_" + std::to_string(seam) + "_b";
                std::ostringstream one, two;
                one << "door \"" << a << "\" \"Open boundary\" \"" << cellId(i) << "\" " << Side - .5 << ' ' << k + .5 << " \""
                    << cellId(i + 1) << "\" .5 " << k + .5 << " \"" << b << "\" 1 0 1 1 \"E\"\n";
                two << "door \"" << b << "\" \"Open boundary\" \"" << cellId(i + 1) << "\" .5 " << k + .5 << " \"" << cellId(i)
                    << "\" " << Side - .5 << ' ' << k + .5 << " \"" << a << "\" 1 0 1 1 \"W\"\n";
                seams[cellId(i)] += one.str();
                seams[cellId(i + 1)] += two.str();
                ++seam;
            }
    }
    for (const auto& [id, text] : seams)
        std::ofstream(root / "seams" / id) << text;
    const auto region = [](char c) { return c == 'U' ? "upper_accord" : c == 'S' ? "ser_ferro" : c == 'R' ? "ridgemere" : "wilds"; };
    for (int i = 0; i < cells; ++i)
    {
        m << "exits \"" << cellId(i) << "\" " << ((i > 0) + (i + 1 < cells));
        if (i > 0)
            m << " \"" << cellId(i - 1) << '"';
        if (i + 1 < cells)
            m << " \"" << cellId(i + 1) << '"';
        m << '\n';
        m << "territory \"" << cellId(i) << "\" \"" << region(layout[std::size_t(i)]) << "\" \"-\" 0\n";
    }
    m << "spawn \"" << cellId(0) << "\" 8.5 8.5\n";
    m << "economy 20000 100 50 10 12\n";
    for (const auto& [c, tag] : std::map<char, std::string>{{'U', "u"}, {'S', "s"}, {'R', "r"}})
    {
        const int home = int(layout.find(c));
        m << resident(tag + "m", "Merchant " + tag, "merchant", "keeping the stall", home, 2.5, home + 1, 8.5);
        for (int n = 1; n <= 5; ++n)
            m << resident(tag + std::to_string(n), "Neighbour " + tag + std::to_string(n), "civilian", "working", home, 3.5 + n, home,
                          3.5 + n);
    }
    // For ties (Phase 3): a fisher in Upper Accord (the river starter), an innkeeper in Ser Ferro (showing one around).
    m << resident("uf", "Fen Marsh", "civilian", "mends the nets", 0, 11.5, 1, 11.5);
    m << resident("si", "Ivy Hearth", "civilian", "serves at the inn", 4, 11.5, 5, 11.5);
    // For matchmaking (Phase 4): Upper Accord's innkeeper.
    m << resident("ui", "Wren Tallow", "civilian", "serves at the inn", 0, 12.5, 1, 12.5);
    std::ofstream(root / "world.ratw") << m.str();
    return root.string();
}

// A stand-in for the NPC Mind (doc 52, Phase 4): answers POST /dialogue with a fixed reply naming the mentor, keeping
// each request; while `speak` is off, 503 (the game then says a written line).
struct FakeMind
{
    int port = 0, fd = -1;
    std::mutex lock;
    std::vector<json::Value> asked;
    std::atomic<bool> stop{false}, speak{false};
    std::thread worker;
    FakeMind()
    {
        fd = ::socket(AF_INET, SOCK_STREAM, 0);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        ::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof addr);
        socklen_t len = sizeof addr;
        ::getsockname(fd, reinterpret_cast<sockaddr*>(&addr), &len);
        ::listen(fd, 8);
        port = ntohs(addr.sin_port);
        worker = std::thread([this] {
            while (!stop)
            {
                const int c = ::accept(fd, nullptr, nullptr);
                if (c < 0)
                    break;
                char buf[65536];
                std::string in;
                for (;;)
                {
                    const auto n = ::recv(c, buf, sizeof buf, 0);
                    if (n <= 0)
                        break;
                    in.append(buf, std::size_t(n));
                    const auto head = in.find("\r\n\r\n");
                    const auto at = in.find("Content-Length: ");
                    if (head != std::string::npos && at != std::string::npos && in.size() >= head + 4 + std::stoul(in.substr(at + 16)))
                        break;
                }
                std::string reply = R"({"error": "busy"})", status = "503 Service Unavailable";
                if (in.rfind("POST /dialogue ", 0) == 0)
                {
                    json::Value body;
                    std::string error;
                    json::parse(in.substr(in.find("\r\n\r\n") + 4), body, error);
                    std::lock_guard<std::mutex> hold(lock);
                    asked.push_back(body);
                    if (speak)
                    {
                        reply = R"({"text": "Go and find Mo, just over there by the hearth. Mo knows these streets.", "emotion": "warm"})";
                        status = "200 OK";
                    }
                }
                const std::string out = "HTTP/1.1 " + status + "\r\nContent-Type: application/json\r\nContent-Length: " +
                                        std::to_string(reply.size()) + "\r\n\r\n" + reply;
                ::send(c, out.data(), out.size(), 0);
                ::close(c);
            }
        });
    }
    ~FakeMind()
    {
        stop = true;
        ::shutdown(fd, SHUT_RDWR);
        ::close(fd);
        worker.join();
    }
    std::string endpoint() const { return "http://127.0.0.1:" + std::to_string(port) + "/dialogue"; }
    json::Value last()
    {
        std::lock_guard<std::mutex> hold(lock);
        return asked.empty() ? json::Value{} : asked.back();
    }
};

game::Options options(const std::string& exportDir = {}, const std::string& save = {}, bool tiesOptional = true)
{
    game::Options o;
    o.hiddenNames = true;
    o.forkSnapshots = false;
    o.oneWolfPerAccount = true;
    o.tiesOptional = tiesOptional;                  // (Ties are tested on their own, below.)
    o.worldExport = exportDir;
    o.savePath = save;
    return o;
}

// Registers (or signs in) an account on a fresh connection; the lobby it got.
json::Value signIn(game::Game& g, Client& c, const char* user, bool fresh = true)
{
    g.connect(&c);
    g.command(&c, cmd({{"type", fresh ? "auth_register" : "auth_login"}, {"username", user}, {"password", "a long enough password"}}));
    g.settle();
    expect(c.last("lobby") != nullptr, std::string("a lobby for ") + user);
    return *c.last("lobby");
}

// Creates a wolf (with a start town when one is given); its id, or "" if refused.
std::string create(game::Game& g, Client& c, const std::string& name, const std::string& start, const std::string& key,
                   const std::string& tie = {})
{
    const auto before = c.last("lobby") ? c.last("lobby")->array("characters").size() : 0;
    auto command = parsed(cmd({{"type", "character_create"}, {"name", name}, {"age", 24}, {"appearance", appearance(4)}, {"commandId", key}}));
    if (!start.empty())
        command.add("start", start);
    if (!tie.empty())
        command.add("tie", tie);
    g.command(&c, json::dump(command));
    g.settle();
    const auto* lobby = c.last("lobby");
    if (!lobby || !lobby->boolean("ok") || lobby->array("characters").size() == before)
        return {};
    for (const auto& ch : lobby->array("characters"))
        if (ch.string("name") == name)
            return ch.string("id");
    return {};
}

// Enters a wolf and says where it stands.
std::string enter(game::Game& g, Client& c, const std::string& id)
{
    g.command(&c, cmd({{"type", "character_enter"}, {"id", id}}));
    g.settle();
    const auto* e = g.world().entity(id);
    expect(e != nullptr, "entered: " + id);
    return e->cellId;
}

void tick(game::Game& g, std::initializer_list<Client*> clients, double seconds, double step = .25)
{
    for (double t = 0; t < seconds; t += step)
    {
        g.tick(step);
        for (auto* c : clients)
            if (!c->snapshots.empty())
                g.acknowledge(c, c->snapshots.back().number("revision"), false);
    }
}

// --------------------------------------------------------------------------- Pure

void rules()
{
    const auto& r = newcomers::rules();
    expect(r.starts.size() == 3 && r.starts[0].id == "upper_accord" && r.starts[1].id == "ser_ferro" && r.starts[2].id == "ridgemere",
           "the three start towns, Upper Accord first");
    expect(r.hours == 15 && r.socialLevel == 3, "new until 15 hours or social level 3");
    expect(r.sampleEvery == 60 && r.window == 30, "counted once a minute, the mean of 30");
    expect(!newcomers::graduates(14.9 * 3600, 2) && newcomers::graduates(15 * 3600, 0) && newcomers::graduates(0, 3),
           "either threshold ends it");
    const auto other = newcomers::parse(R"({"starts": [{"id": "a"}], "newcomer": {"hours": 2}})");
    expect(other.starts.size() == 1 && other.starts[0].name == "a" && other.hours == 2 && other.socialLevel == 3, "parsed, defaults kept");
}

void means()
{
    const std::vector<std::string> towns{"upper_accord", "ser_ferro", "ridgemere"};
    newcomers::Counts equal;
    for (int i = 0; i < 30; ++i)
        equal.add({{"upper_accord", 3}, {"ser_ferro", 3}, {"ridgemere", 3}}, towns, 30);
    expect(newcomers::busiest(towns, equal) == "upper_accord", "equal counts go to Upper Accord");
    newcomers::Counts none;
    expect(newcomers::busiest(towns, none) == "upper_accord", "no counts yet: Upper Accord");
    newcomers::Counts crowd;
    for (int i = 0; i < 30; ++i)
        crowd.add({{"upper_accord", 2}, {"ser_ferro", 4}}, towns, 30);
    expect(newcomers::busiest(towns, crowd) == "ser_ferro" && std::abs(crowd.mean("ser_ferro") - 4) < 1e-9, "a steady crowd in Ser Ferro wins");
    newcomers::Counts spike;
    for (int i = 0; i < 29; ++i)
        spike.add({{"upper_accord", 3}}, towns, 30);
    spike.add({{"ser_ferro", 50}}, towns, 30);
    expect(newcomers::busiest(towns, spike) == "upper_accord", "one busy minute in Ser Ferro doesn't");
    for (int i = 0; i < 40; ++i)
        spike.add({}, towns, 30);
    expect(spike.mean("upper_accord") == 0 && spike.mean("ser_ferro") == 0, "only the last 30 counts");
}

// --------------------------------------------------------------------------- Through the game

void oneTown()
{
    // The demo world has one settlement: no start towns, everyone at the spawn, a named one refused.
    game::Game g(options());
    std::string problem;
    expect(g.start(problem), "starts: " + problem);
    Client c;
    c.id = 1;
    const auto lobby = signIn(g, c, "una");
    expect(lobby.array("starts").empty() && lobby.boolean("firstCharacter"), "no start towns to choose in a world of one town");
    expect(create(g, c, "Una", "ser_ferro", "u1").empty() && c.last("lobby")->string("message").find("one place") != std::string::npos,
           "a start town this world hasn't is refused: " + c.last("lobby")->string("message"));
    const auto id = create(g, c, "Una", "", "u2");
    expect(!id.empty(), "created without one");
    expect(!c.last("lobby")->boolean("firstCharacter"), "the next isn't the first");
    // A name always starts with a capital (the user, 2026-10-07), so it is veiled wherever it is written.
    expect(create(g, c, "wren", "", "u3").empty(), "\"wren\" isn't kept as typed");
    bool capital = false;
    for (const auto& ch : c.last("lobby")->array("characters"))
        capital = capital || ch.string("name") == "Wren";
    expect(capital, "but as Wren: ");
    const auto cell = enter(g, c, id);
    const auto* e = g.world().entity(id);
    g.world().addPlayer("probe", "Probe");          // (Where anyone arrives in this world: its spawn.)
    const auto* probe = g.world().entity("probe");
    expect(cell == probe->cellId && e->position.x == probe->position.x && e->position.y == probe->position.y, "at the spawn: " + cell);
    g.world().removePlayer("probe");
}

void threeTowns()
{
    const auto root = fs::temp_directory_path() / ("ratw-newcomers-" + std::to_string(::getpid()));
    const auto save = (fs::temp_directory_path() / ("ratw-newcomers-save-" + std::to_string(::getpid()) + ".json")).string();
    fs::remove(save);
    std::string capital;
    {
        game::Game g(options(writeStrip(root), save));
        std::string problem;
        expect(g.start(problem), "starts: " + problem);
        tick(g, {}, 1);
        expect(g.world().towns().size() == 3, "three towns: " + std::to_string(g.world().towns().size()));
        capital = g.world().society().capital();
        const auto starts = g.startTowns();
        expect(starts == std::vector<std::string>{"upper_accord", "ser_ferro", "ridgemere"}, "the start towns, in the data file's order");

        // A new account: Upper Accord preselected (no one about yet), and a first character free to choose.
        Client nina, omar, pia;
        nina.id = 1, omar.id = 2, pia.id = 3;
        const auto lobby = signIn(g, nina, "nina");
        const auto& listed = lobby.array("starts");
        expect(listed.size() == 3 && listed[0].string("id") == "upper_accord" && listed[0].boolean("suggested") &&
                   !listed[1].boolean("suggested") && !listed[0].string("line").empty() && listed[1].string("name") == "Ser Ferro",
               "the creator's towns, Upper Accord preselected: " + json::dump(lobby));
        expect(lobby.boolean("firstCharacter"), "her first character");
        expect(create(g, nina, "Nina", "atlantis", "n0").empty() &&
                   contains(nina.last("lobby")->string("message"), "start towns"),
               "a forged start town is refused");
        const auto ninaId = create(g, nina, "Nina", "", "n1");
        expect(!ninaId.empty(), "Nina is made, no town named");
        expect(enter(g, nina, ninaId) == cellId(0), "the busiest (Upper Accord): at the spawn");
        const auto* at = g.world().entity(ninaId);
        expect(std::abs(at->position.x - 8.5) < 1e-6 && std::abs(at->position.y - 8.5) < 1e-6, "on the spawn's tile");

        // Another new account's first character chooses Ridgemere: beside its market's merchant.
        signIn(g, omar, "omar");
        const auto omarId = create(g, omar, "Omar", "ridgemere", "o1");
        expect(!omarId.empty(), "a first character may choose (the user)");
        expect(enter(g, omar, omarId) == cellId(8), "Ridgemere's market cell");
        const auto* o = g.world().entity(omarId);
        expect(std::hypot(o->position.x - 8.5, o->position.y - 8.5) <= 2.9, "beside the merchant's stall");

        // The newcomer: the mark, the card, the residents' briefing (Omar walks over to Nina).
        g.world().entity(omarId)->cellId = cellId(0);
        g.world().entity(omarId)->position = {9.5, 8.5};
        tick(g, {&nina, &omar}, 1);
        const auto* seenOmar = nina.seen(omarId);
        expect(seenOmar && seenOmar->boolean("nc"), "a newcomer's mark: " + (seenOmar ? json::dump(*seenOmar) : std::string("unseen")));
        g.command(&nina, cmd({{"type", "action"}, {"action", "inspect"}, {"target", omarId}}));
        g.settle();
        const auto* card = nina.last("inspect");
        expect(card && card->object("profile").boolean("newcomer"), "\"new to these parts\" on the card");
        const auto briefing = g.dialogueContext("rm", omarId, "Hello there.", true);
        expect(briefing.activity.find("new to these parts") != std::string::npos, "the resident is told: " + briefing.activity);

        // A steady crowd in Ser Ferro: Pia's first character is offered Ser Ferro.
        for (auto [c, id] : {std::pair{&nina, ninaId}, {&omar, omarId}})
        {
            auto* e = g.world().entity(id);
            e->cellId = cellId(4);
            e->position = {6.5 + (c == &omar), 6.5};
        }
        for (int minute = 0; minute < 3; ++minute)
        {
            for (auto* c : {&nina, &omar})
            {
                g.command(c, cmd({{"type", "chat"}, {"text", "*looks about the square.*"}, {"channel", "ic"}, {"volume", "speak"}}));
                g.settle();
            }
            tick(g, {&nina, &omar}, 61, 1);
        }
        expect(g.startTownMean("ser_ferro") > 1 && g.suggestedStart() == "ser_ferro",
               "Ser Ferro busiest now: " + std::to_string(g.startTownMean("ser_ferro")));
        const auto piaLobby = signIn(g, pia, "pia");
        bool offered = false;
        for (const auto& s : piaLobby.array("starts"))
            offered = offered || (s.string("id") == "ser_ferro" && s.boolean("suggested") && s.number("wolves") >= 1);
        expect(offered, "Pia's creator preselects Ser Ferro, with wolves about: " + json::dump(piaLobby));
        const auto piaId = create(g, pia, "Pia", "", "p1");
        expect(enter(g, pia, piaId) == cellId(5), "Pia arrives in Ser Ferro's market cell");

        // The spawn and the capital stay put.
        expect(g.world().society().capital() == capital, "the capital doesn't move");

        // Graduating: social level 3 ends it at the next count, for good.
        g.ledger().points[ninaId] += int(practice::xpFor(3));
        tick(g, {&nina, &omar, &pia}, 61, 1);
        expect(!g.isNewcomer("nina") && g.isNewcomer("omar"), "Nina is no longer new; Omar still is");
        g.ledger().points[ninaId] = 0;
        expect(!g.isNewcomer("nina"), "and stays so");
        tick(g, {&nina, &omar, &pia}, 1);
        expect(nina.seen(ninaId) && !nina.seen(ninaId)->boolean("nc"), "no mark for her");
        g.save();
    }
    {
        // Kept across a restart: graduated stays.
        game::Game g(options(root.string(), save));
        std::string problem;
        expect(g.start(problem), "starts again: " + problem);
        expect(!g.isNewcomer("nina") && g.isNewcomer("omar"), "graduated survives a restart");
    }
    fs::remove_all(root);
    fs::remove(save);
}
void mentorRules()
{
    std::string why;
    newcomers::MentorCheck m;
    m.socialLevel = 4;
    expect(!newcomers::mayMentor(m, why) && contains(why, "social level 5"), "level 4 may not mentor: " + why);
    m.socialLevel = 5;
    expect(newcomers::mayMentor(m, why) && why.empty(), "level 5 may");
    m.upheldReports = 1;
    expect(!newcomers::mayMentor(m, why) && contains(why, "30 days"), "not with a report upheld within 30 days");
    m.upheldReports = 0;      // (An older one isn't counted: upheldReportsWithin counts only the last 30 days.)
    m.newcomer = true;
    expect(!newcomers::mayMentor(m, why) && contains(why, "new to these parts"), "not a newcomer");
    m.newcomer = false, m.silenced = true;
    expect(!newcomers::mayMentor(m, why), "not while silenced");
    m.silenced = false, m.revoked = true;
    expect(!newcomers::mayMentor(m, why) && contains(why, "Dungeon Master"), "not once a Dungeon Master has turned it off");
}

// Three accounts in the demo world, side by side: Mia (to mentor), Nell (a newcomer), Vic (a veteran, not new).
void mentors()
{
    const auto save = (fs::temp_directory_path() / ("ratw-mentors-save-" + std::to_string(::getpid()) + ".json")).string();
    fs::remove(save);
    std::string miaId;
    {
        game::Game g(options({}, save));
        std::string problem;
        expect(g.start(problem), "starts: " + problem);
        Client mia, nell, vic;
        mia.id = 1, nell.id = 2, vic.id = 3;
        std::map<Client*, std::string> ids;
        for (auto [c, user, name] : {std::tuple{&mia, "mia", "Mia"}, {&nell, "nell", "Nell"}, {&vic, "vic", "Vic"}})
        {
            signIn(g, *c, user);
            ids[c] = create(g, *c, name, "", std::string(user) + "1");
            enter(g, *c, ids[c]);
        }
        miaId = ids[&mia];
        auto* m = g.world().entity(miaId);
        for (auto* c : {&nell, &vic})
        {
            auto* e = g.world().entity(ids[c]);
            e->cellId = m->cellId;
            e->position = {m->position.x + (c == &nell ? 1 : 2), m->position.y};
        }
        const auto mentorCmd = [&](const char* verb) {
            g.command(&mia, cmd({{"type", "mentor"}, {"verb", verb}}));
            g.settle();
        };
        const auto said = [&](Client& c, const std::string& text) {
            for (auto it = c.events.rbegin(); it != c.events.rend(); ++it)
                if (contains(it->string("text"), text))
                    return true;
            return false;
        };
        // Level 4 can't; level 5 can, and is a Newcomer Guide.
        g.ledger().points[miaId] = int(practice::xpFor(4));
        g.ledger().points[ids[&vic]] = int(practice::xpFor(3));
        tick(g, {&mia, &nell, &vic}, 61, 1);        // (They stop being new at the minute's count.)
        expect(!g.isNewcomer("mia") && !g.isNewcomer("vic") && g.isNewcomer("nell"), "Mia and Vic aren't new; Nell is");
        mentorCmd("optin");
        expect(said(mia, "social level 5"), "level 4: not yet");
        expect(!mia.last("profile")->object("account").object("mentor").boolean("may"), "her panel says she may not");
        g.ledger().points[miaId] = int(practice::xpFor(5));
        mentorCmd("optin");
        const auto account = mia.last("profile")->object("account");
        expect(account.object("mentor").boolean("on") && account.string("experience") == "guide", "level 5: a mentor, and a Newcomer Guide");
        // Everyone sees "mentor"; only a newcomer sees her marked free on the map.
        tick(g, {&mia, &nell, &vic}, 1);
        const auto* toNell = nell.seen(miaId);
        const auto* toVic = vic.seen(miaId);
        expect(toNell && toNell->boolean("mentor") && toNell->boolean("mentorFree"), "Nell sees a mentor free to take her");
        expect(toVic && toVic->boolean("mentor") && !toVic->boolean("mentorFree"), "Vic sees a mentor, without the newcomer's mark");
        // Busy: still a mentor, not marked free.
        mentorCmd("busy");
        tick(g, {&mia, &nell, &vic}, 1);
        expect(nell.seen(miaId)->boolean("mentor") && !nell.seen(miaId)->boolean("mentorFree"), "busy: not marked free");
        g.command(&vic, cmd({{"type", "action"}, {"action", "inspect"}, {"target", miaId}}));
        g.settle();
        const auto card = vic.last("inspect")->object("profile");
        expect(card.string("mentor") == "busy" && card.number("guided", -1) == 0, "the card: a mentor, busy, guided none yet");
        mentorCmd("available");
        // An upheld report turns it off at once, and she can't opt in again while it counts.
        g.command(&mia, cmd({{"type", "chat"}, {"text", "Get lost, pup."}, {"channel", "ic"}, {"volume", "speak"}}));
        g.settle();
        g.command(&nell, cmd({{"type", "safety"}, {"verb", "report"}, {"target", miaId}, {"category", "harassment"}}));
        g.settle();
        expect(!g.reportsKept().empty(), "Nell's report is kept");
        expect(g.decideReport(g.reportsKept().begin()->first, "uphold", "warning", 0, "dm:test").ok, "upheld");
        expect(said(mia, "mentoring is turned off: a report about you was upheld"), "Mia is told why");
        expect(!mia.last("profile")->object("account").object("mentor").boolean("on"), "and it is off");
        mentorCmd("optin");
        expect(said(mia, "report upheld"), "no opting in again within 30 days");
        // The Dungeon Master's revoke holds through a restart.
        expect(g.revokeMentor("mia", true, "dm:test").ok, "revoked");
        expect(said(mia, "turned your mentoring off"), "told");
        g.save();
    }
    {
        game::Game g(options({}, save));
        std::string problem;
        expect(g.start(problem), "starts again: " + problem);
        std::string why;
        expect(!g.mayMentor("mia", why) && contains(why, "Dungeon Master"), "the revoke survives a restart: " + why);
        expect(g.revokeMentor("mia", false, "dm:test").ok, "restored");
        expect(!contains((g.mayMentor("mia", why), why), "Dungeon Master"), "restored: no longer the reason");
    }
    fs::remove(save);
}
void tieRules()
{
    const auto& r = newcomers::tieRules();
    expect(r.starters.size() == 8 && newcomers::starter("river") && newcomers::fallbackStarter() &&
               newcomers::fallbackStarter()->id == "show_around",
           "doc 48's eight starters, showing one around the fallback");
    expect(r.offerSeconds == 180 && r.lapseDays == 7 && r.lapseScenes == 3 && r.restSeconds == 86400, "3 minutes, 7 days or 3 scenes, a day's rest");
    const auto& debt = *newcomers::starter("debt");
    expect(newcomers::residentFits(debt, "farmer", 40, false) && !newcomers::residentFits(debt, "guard", 40, false), "the miller (a farmer) for the debt");
    expect(debt.owed == 4, "a small debt: 4 pennies");
    const auto& apprentice = *newcomers::starter("apprentice");
    expect(newcomers::residentFits(apprentice, "smith", 40, true) && !newcomers::residentFits(apprentice, "smith", 40, false),
           "a master with a free apprentice place");
    expect(!newcomers::residentFits(*newcomers::starter("cousins"), "none", 70, false), "cousins of an age");
    expect(newcomers::starter("cousins")->names && !newcomers::starter("river")->names, "only family know each other's names");
    const auto order = newcomers::mentorOrder({{"a", "wa", 500}, {"b", "wb", -1}, {"c", "wc", 100}}, 7);
    expect(order[0].account == "b" && order[1].account == "c" && order[2].account == "a", "the one longest without a tie first");
    newcomers::Tie t;
    t.state = "active", t.made = 1000;
    expect(!newcomers::lapsed(t, 2, 1000 + 6 * 86400) && newcomers::lapsed(t, 3, 1001) && newcomers::lapsed(t, 0, 1000 + 7 * 86400),
           "lapses at 3 scenes or 7 days");
    t.state = "seeking";
    expect(!newcomers::lapsed(t, 9, 1e12), "only an active tie lapses");
    t.asked = {"x"};
    t.id = "tie-1";
    const auto back = newcomers::loadTie(newcomers::saveTie(t));
    expect(back.id == "tie-1" && back.asked == t.asked && back.made == 1000, "a tie saved and read back");
}

// On the strip: four mentors (two in Upper Accord, one in Ridgemere, one Out of character), and newcomers' ties.
void ties()
{
    const auto root = fs::temp_directory_path() / ("ratw-ties-" + std::to_string(::getpid()));
    const auto save = (fs::temp_directory_path() / ("ratw-ties-save-" + std::to_string(::getpid()) + ".json")).string();
    fs::remove(save);
    std::string nellId, quinId, tessId;
    std::string tessOther;
    std::size_t kept = 0;
    {
        auto o = options(writeStrip(root), save, false);
        o.tieLapseSeconds = 8;                      // (Real seconds: ties are timed by the clock, not the world.)
        o.tieOfferSeconds = 2;
        game::Game g(o);
        std::string problem;
        expect(g.start(problem), "starts: " + problem);
        tick(g, {}, 1);
        Client mara, milo, far, ooc, nell, oona, pia, quin, uma, sam, ria, tess;
        int n = 1;
        for (auto* c : {&mara, &milo, &far, &ooc, &nell, &oona, &pia, &quin, &uma, &sam, &ria, &tess})
            c->id = n++;
        std::vector<Client*> everyone{&mara, &milo, &far, &ooc};
        const auto tickAll = [&](double seconds, double realSeconds = 0) {
            if (realSeconds > 0)
                std::this_thread::sleep_for(std::chrono::milliseconds(int(realSeconds * 1000)));
            for (double t = 0; t < seconds; t += 1)
            {
                g.tick(1);
                for (auto* c : everyone)
                    if (!c->snapshots.empty())
                        g.acknowledge(c, c->snapshots.back().number("revision"), false);
            }
        };
        const auto said = [](Client& c, const std::string& text) {
            for (const auto& e : c.events)
                if (contains(e.string("text"), text))
                    return true;
            return false;
        };
        const auto offers = [](Client& c) {
            return std::count_if(c.events.begin(), c.events.end(), [](const json::Value& e) { return e.string("type") == "tieOffer"; });
        };
        const auto tieOf = [&](const std::string& id) {
            for (const auto& [tid, t] : g.ties())
                if (t.newcomer == id && (t.state == "seeking" || t.state == "offered" || t.state == "active"))
                    return t;
            return newcomers::Tie{};
        };
        // The mentors: new accounts too, so each first wolf takes a tie (ended at once), then raised and opted in.
        std::map<Client*, std::string> ids;
        bool refused = false;
        for (auto [c, user, cell] : {std::tuple{&mara, "mara", 1}, {&milo, "milo", 1}, {&far, "faro", 8}, {&ooc, "otto", 0}})
        {
            signIn(g, *c, user);
            if (!refused)
            {
                expect(create(g, *c, "Nobody", "", std::string(user) + "0").empty() && contains(c->last("lobby")->string("message"), "takes a tie"),
                       "a first wolf can't be made without a tie");
                refused = true;
            }
            ids[c] = create(g, *c, std::string(1, char(std::toupper(user[0]))) + std::string(user + 1), "", std::string(user) + "1", "letter");
            expect(!ids[c].empty(), std::string("a mentor's wolf: ") + user);
            enter(g, *c, ids[c]);
            g.command(c, cmd({{"type", "tie"}, {"verb", "end"}}));
            g.settle();
            g.ledger().points[ids[c]] = int(practice::xpFor(5));
            auto* e = g.world().entity(ids[c]);
            e->cellId = cellId(cell);
            e->position = {6.5, 6.5};
        }
        tickAll(61);                                // (They stop being new at the minute's count.)
        for (auto* c : everyone)
        {
            g.command(c, cmd({{"type", "mentor"}, {"verb", "optin"}}));
            g.settle();
            expect(c->last("profile")->object("account").object("mentor").boolean("on"), "a mentor");
        }
        g.command(&ooc, cmd({{"type", "profile"}, {"verb", "status"}, {"value", "ooc"}}));
        g.settle();
        std::map<std::string, Client*> byId{{ids[&mara], &mara}, {ids[&milo], &milo}};

        // Nell takes "river": one of the two in town is asked; a pass, then a time-out, then the fisher.
        signIn(g, nell, "nell");
        nellId = create(g, nell, "Nell", "upper_accord", "n1", "river");
        expect(!nellId.empty() && tieOf(nellId).state == "seeking", "Nell's wolf is made, its tie seeking");
        tickAll(2);
        auto t = tieOf(nellId);
        expect(t.state == "offered" && byId.count(t.offeredTo), "offered to a mentor in town: " + t.offeredTo);
        auto* first = byId[t.offeredTo];
        auto* second = first == &mara ? &milo : &mara;
        const auto* offer = first->last("tieOffer");
        expect(offer && !offer->string("look").empty() && contains(offer->string("starter"), "pulled a wolf out of the river") &&
                   offer->string("town") == "Upper Accord" && offer->number("seconds") == 2,
               "the offer: the newcomer's look, the starter from the mentor's side, the town, how long it stands");
        g.command(first, cmd({{"type", "mentor"}, {"verb", "pass"}, {"tie", t.id}}));
        g.settle();
        tickAll(2);
        t = tieOf(nellId);
        expect(t.state == "offered" && byId[t.offeredTo] == second, "a pass: the next mentor is asked");
        tickAll(2, 3.6);                            // (Past the offer's time, by the clock: whole seconds.)
        t = tieOf(nellId);
        expect(t.state == "active" && t.resident && t.other == "uf", "silence too: then the fisher, for the river (" + t.other + ")");
        expect(said(*second, "gone to someone else"), "the silent mentor is told it went elsewhere");
        const auto* bond = g.world().bonds().find("uf", nellId);
        expect(bond && bond->familiarity > 0 && bond->trust > 0, "the fisher knows of them a little, and trusts them a little");
        expect(contains(g.knownFor(nellId, "uf").string("tie"), "pulled you out of the river"), "on Nell's known wolves, the starter as the note");
        enter(g, nell, nellId);
        expect(said(nell, "Your tie: They pulled you out of the river") && said(nell, "Look for"), "Nell is told on entering, and where to look");
        everyone.push_back(&nell);
        tickAll(1);
        const auto self = nell.snapshots.back().object("self").object("tie");
        expect(self.string("state") == "active" && contains(self.string("line"), "river") && self.object("marker").string("cell") == cellId(1),
               "her tie in the snapshot, with the marker where the fisher was: " + json::dump(self));
        expect(contains(g.dialogueContext("uf", nellId, "Hello.", true).activity, "share a tie"), "the fisher's briefing carries it");

        // Oona takes "letter": whoever is asked accepts. Both get known-wolf entries with the starter.
        signIn(g, oona, "oona");
        const auto oonaId = create(g, oona, "Oona", "upper_accord", "o1", "letter");
        tickAll(2);
        t = tieOf(oonaId);
        expect(t.state == "offered" && byId.count(t.offeredTo), "Oona's tie offered");
        auto* holder = byId[t.offeredTo];
        auto* free = holder == &mara ? &milo : &mara;
        g.command(holder, cmd({{"type", "mentor"}, {"verb", "accept"}, {"tie", t.id}}));
        g.settle();
        t = tieOf(oonaId);
        expect(t.state == "active" && !t.resident && t.other == ids[holder], "the mentor takes it");
        expect(said(*holder, "Your tie: A newcomer carries a letter for you"), "the mentor is told the starter from their side");
        expect(contains(g.knownFor(ids[holder], oonaId).string("tie"), "carries a letter for you") &&
                   contains(g.knownFor(oonaId, ids[holder]).string("tie"), "You carry a letter"),
               "both on each other's known wolves");

        // Pia takes "cart": the mentor holding a tie isn't asked; the other passes; a resident who fits.
        const auto heldOffers = offers(*holder);
        signIn(g, pia, "pia");
        const auto piaId = create(g, pia, "Pia", "upper_accord", "p1", "cart");
        tickAll(2);
        t = tieOf(piaId);
        expect(t.state == "offered" && t.offeredTo == ids[free] && offers(*holder) == heldOffers, "one holding a tie is never asked");
        g.command(free, cmd({{"type", "mentor"}, {"verb", "pass"}, {"tie", t.id}}));
        g.settle();
        tickAll(2);
        t = tieOf(piaId);
        expect(t.state == "active" && t.resident && t.other == "um", "the merchant, for the cart (" + t.other + ")");

        // Oona ends hers (the mentor doesn't rest); Quin's tie goes first to the one never tied.
        enter(g, oona, oonaId);
        expect(said(oona, "Your tie: You carry a letter"), "Oona is told on entering");
        g.command(&oona, cmd({{"type", "tie"}, {"verb", "end"}}));
        g.settle();
        expect(said(*holder, "is ended"), "her mentor is told it's ended");
        expect(g.ties().at(t.id).state == "active", "(Pia's stands)");
        signIn(g, quin, "quin");
        quinId = create(g, quin, "Quin", "upper_accord", "q1", "road");
        tickAll(2);
        t = tieOf(quinId);
        expect(t.state == "offered" && t.offeredTo == ids[free], "the mentor longest without a tie is asked first");
        g.command(free, cmd({{"type", "mentor"}, {"verb", "accept"}, {"tie", t.id}}));
        g.settle();
        // It lapses (here after 8 seconds, for the test); that mentor rests a day; the other is asked next.
        tickAll(61, 9.6);
        expect(g.ties().at(t.id).state == "lapsed", "Quin's tie has run its course");
        expect(said(*free, "has run its course"), "the mentor is told");
        signIn(g, uma, "uma");
        const auto umaId = create(g, uma, "Uma", "upper_accord", "u1", "letter");
        tickAll(2);
        t = tieOf(umaId);
        expect(t.state == "offered" && t.offeredTo == ids[holder], "a resting mentor isn't asked; the other is");
        expect(offers(far) == 0 && offers(ooc) == 0, "a mentor in another town, or Out of character, is never asked");
        g.command(holder, cmd({{"type", "mentor"}, {"verb", "pass"}, {"tie", t.id}}));
        g.settle();

        // Elsewhere: Ser Ferro has no mentor near and no fisher (the innkeeper shows Sam around); Ridgemere's mentor
        // passes, and its masters take an apprentice; a debt to someone who keeps a trade.
        signIn(g, sam, "sam");
        const auto samId = create(g, sam, "Sam", "ser_ferro", "s1", "river");
        signIn(g, ria, "ria");
        const auto riaId = create(g, ria, "Ria", "ridgemere", "r1", "apprentice");
        signIn(g, tess, "tess");
        tessId = create(g, tess, "Tess", "ser_ferro", "t1", "debt");
        tickAll(2);
        t = tieOf(samId);
        expect(t.state == "active" && t.other == "si" && t.starter == "show_around", "none fits: the innkeeper shows them around (" + t.other + ")");
        t = tieOf(riaId);
        expect(t.state == "offered" && t.offeredTo == ids[&far], "Ridgemere's own mentor is asked for a Ridgemere newcomer");
        g.command(&far, cmd({{"type", "mentor"}, {"verb", "pass"}, {"tie", t.id}}));
        g.settle();
        tickAll(2);
        t = tieOf(riaId);
        bool master = false;
        for (const auto& [pid, st] : g.world().society().state().careers.positions)
            master = master || (st.holder == t.other && st.apprentice.empty());
        expect(t.state == "active" && t.other.rfind("r", 0) == 0 && master, "a Ridgemere master with a free apprentice place: " + t.other);
        t = tieOf(tessId);
        expect(t.state == "active" && (t.other == "sm" || t.other == "si") && g.world().bonds().find(t.other, tessId) &&
                   g.world().bonds().find(t.other, tessId)->owed == 4,
               "a debt of 4 pennies to someone with a trade (" + t.other + ")");
        kept = g.ties().size();
        tessOther = t.other;
        g.save();
    }
    {
        // Ties survive a restart.
        game::Game g(options(root.string(), save, false));
        std::string problem;
        expect(g.start(problem), "starts again: " + problem);
        expect(g.ties().size() == kept, "every tie kept");
        bool tess = false, quin = false;
        for (const auto& [id, t] : g.ties())
        {
            tess = tess || (t.newcomer == tessId && t.state == "active" && t.other == tessOther);
            quin = quin || (t.newcomer == quinId && t.state == "lapsed");
        }
        expect(tess && quin, "Tess's still active, Quin's lapsed");
    }
    fs::remove_all(root);
    fs::remove(save);
}
void matchRules()
{
    const auto& r = newcomers::matchRules();
    expect(r.matchmakers == std::vector<std::string>{"innkeeper", "priest"} && r.marketMerchant, "innkeepers, priests and market merchants");
    expect(r.perPlayerHours == 1 && r.perMatchmakerHour == 6 && r.pointedPerHour == 3, "once an hour, six an hour, three times pointed at");
    const auto* work = newcomers::askIn(voice::Rules::normalise("I'm looking for work, if you know of any.", "Wren"), r);
    expect(work && work->id == "work", "\"looking for work\" is an ask");
    expect(!newcomers::askIn(voice::Rules::normalise("What a fine evening.", "Wren"), r), "small talk is not");
    newcomers::MatchSide newcomer;
    newcomer.newcomer = true;
    std::vector<newcomers::MatchCandidate> c(3);
    c[0].id = "helper", c[0].side.helper = true;
    c[1].id = "ooc", c[1].side.helper = true, c[1].excluded = true;
    c[2].id = "looking", c[2].side.looking = true;
    auto p = newcomers::pickPairing(newcomer, c, 1, r);
    expect(p.id == "helper" && p.reason == "newcomerToHelper", "a newcomer to a helper, never to an excluded one");
    c[0].excluded = true;
    expect(newcomers::pickPairing(newcomer, c, 1, r).id.empty(), "nothing fits: no one");
    newcomers::MatchSide looking;
    looking.looking = true;
    expect(newcomers::pickPairing(looking, c, 1, r).reason == "looking", "both looking");
    c[2].tiedUnmet = true;
    expect(newcomers::pickPairing(looking, c, 1, r).reason == "tie", "a tie not yet met comes before both looking");
    expect(newcomers::fill("Try {who}, {where}.", {{"who", "a dun wolf"}, {"where", "by the fire"}}) == "Try a dun wolf, by the fire.", "the blanks");
}

// On the strip, at Upper Accord's inn: Nell (a newcomer) and the wolves the innkeeper might point her at.
void matchmaking()
{
    const auto root = fs::temp_directory_path() / ("ratw-match-" + std::to_string(::getpid()));
    FakeMind mind;
    auto o = options(writeStrip(root));
    o.dialogueEndpoint = mind.endpoint();
    game::Game g(o);
    std::string problem;
    expect(g.start(problem), "starts: " + problem);
    tick(g, {}, 1);
    Client nell, mo, otto, opal, bea, vic, pip, pru, pax;
    std::vector<Client*> all{&nell, &mo, &otto, &opal, &bea, &vic, &pip, &pru, &pax};
    std::map<Client*, std::string> ids;
    int n = 1;
    for (auto* c : all)
        c->id = n++;
    const std::vector<std::pair<Client*, const char*>> who{{&nell, "Nell"}, {&mo, "Mo"}, {&otto, "Otto"}, {&opal, "Opal"}, {&bea, "Bea"},
                                                           {&vic, "Vic"}, {&pip, "Pip"}, {&pru, "Pru"}, {&pax, "Pax"}};
    double x = 3.5;
    for (auto [c, name] : who)
    {
        std::string user = name;
        for (auto& ch : user)
            ch = char(std::tolower(static_cast<unsigned char>(ch)));
        signIn(g, *c, (user + "x").c_str());
        ids[c] = create(g, *c, name, "", user + "1");
        enter(g, *c, ids[c]);
        auto* e = g.world().entity(ids[c]);
        e->cellId = cellId(1);
        e->position = {x, 10.5};
        x += 1;
        g.world().bonds().change("ui", ids[c], {0, 0, 20, 0, 0}, g.world().calendarDays());   // (The innkeeper knows them all.)
    }
    g.world().bonds().change("ui", "u1", {0, 0, 20, 0, 0}, g.world().calendarDays());          // (And a master next door.)
    auto* inn = g.world().entity("ui");
    inn->cellId = cellId(1);
    inn->position = {6.5, 9.5};

    const auto run = [&](double seconds) {
        for (double t = 0; t < seconds; t += 0.5)
        {
            g.tick(0.5);
            for (auto* c : all)
                if (!c->snapshots.empty())
                    g.acknowledge(c, c->snapshots.back().number("revision"), false);
        }
    };
    const auto say = [&](Client& c, const std::string& text, bool toInn = false) {
        auto command = parsed(cmd({{"type", "chat"}, {"text", text}, {"channel", "ic"}, {"volume", "speak"}}));
        if (toInn)
        {
            auto targets = json::Value::array();
            targets.push("ui");
            command.add("targets", targets);
        }
        g.command(&c, json::dump(command));
        g.settle();
    };
    // The innkeeper's next words to this wolf (waiting for the reply).
    const auto answer = [&](Client& c, const std::string& text) {
        const auto before = c.events.size();
        say(c, text, true);
        for (int i = 0; i < 80; ++i)
        {
            run(0.5);
            const auto* inn = c.seen("ui");
            for (auto k = before; inn && k < c.events.size(); ++k)
                if (c.events[k].string("speaker") == inn->string("name") && !c.events[k].string("text").empty())
                    return c.events[k].string("text");
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        return std::string();
    };
    const auto said = [](Client& c, const std::string& text) {
        return std::any_of(c.events.begin(), c.events.end(), [&](const json::Value& e) { return contains(e.string("text"), text); });
    };
    // Mentors: Mo, Otto (Out of character), Opal (matchmaking off), Bea (whom Nell blocks); Vic not new; the rest new.
    for (auto* c : {&mo, &otto, &opal, &bea})
    {
        g.ledger().points[ids[c]] = int(practice::xpFor(5));
        g.command(c, cmd({{"type", "mentor"}, {"verb", "optin"}}));
        g.settle();
    }
    g.ledger().points[ids[&vic]] = int(practice::xpFor(3));
    g.command(&otto, cmd({{"type", "profile"}, {"verb", "status"}, {"value", "ooc"}}));
    g.command(&opal, cmd({{"type", "profile"}, {"verb", "settings"}, {"settings", parsed(R"({"matchmaking": false})")}}));
    g.command(&nell, cmd({{"type", "safety"}, {"verb", "block"}, {"target", ids[&bea]}}));
    g.settle();
    for (auto* c : all)
        say(*c, "*stretches by the hearth.*");
    run(61);                                        // (The minute's count: who is about in town.)

    // How a wolf is called inside a sentence: "a brown wolf".
    const auto look = [](Client& c, const std::string& id) {
        const auto* seen = c.seen(id);
        expect(seen != nullptr, "in sight: " + id);
        auto name = seen->string("name");
        if (!name.empty())
            name[0] = char(std::tolower(static_cast<unsigned char>(name[0])));
        return name;
    };
    // Nell asks the innkeeper anything: pointed at Mo, by his look, the written line (no model).
    const auto moLook = look(nell, ids[&mo]);
    auto heard = answer(nell, "Good evening. I've only just arrived.");
    expect(contains(heard, "Try " + moLook) && contains(heard, "glad to show newcomers about") && !contains(heard, "Mo,"),
           "pointed at Mo, by his look (" + moLook + "): " + heard);
    expect(said(mo, "nods your way while talking with"), "Mo sees the innkeeper nod his way");
    heard = answer(nell, "And where might I sleep?");
    expect(!heard.empty() && !contains(heard, "Try "), "once a game hour: not again now: " + heard);
    // Bea, whom only Nell blocked, sets herself busy: a busy mentor isn't sent newcomers.
    g.command(&bea, cmd({{"type", "mentor"}, {"verb", "busy"}}));
    g.settle();
    // Pip and Pru, new too, are pointed at him; by the fourth time this hour, not Pax.
    for (auto* c : {&pip, &pru})
    {
        heard = answer(*c, "Hello there. I'm new here.");
        expect(contains(heard, "Try " + look(*c, ids[&mo])), "another newcomer pointed at Mo: " + heard);
    }
    heard = answer(pax, "Hello there. I'm new here.");
    expect(!heard.empty() && !contains(heard, "Try "), "Mo has been pointed at three times this hour: not a fourth: " + heard);
    // Vic, looking for work: the master next door, whom the innkeeper knows (stepped in, so Vic can see him).
    auto* master = g.world().entity("u1");
    master->cellId = cellId(1);
    master->position = {12.5, 12.5};
    run(1);
    heard = answer(vic, "I'm looking for work, if you know of any.");
    expect(contains(heard, "Try " + look(vic, "u1")) && contains(heard, "apprentice place"),
           "\"looking for work\": a master with a place to fill: " + heard);
    expect(!said(otto, "nods your way") && !said(opal, "nods your way") && !said(bea, "nods your way"),
           "never Out of character, opted out, blocked or busy");

    // With the Mind: the briefing goes to the model, and the name it writes becomes the look.
    mind.speak = true;
    g.ledger().points[ids[&nell]] = 0;
    auto later = g.world().save();
    later.calendarDays += 1.0 / 12;                 // (Two game hours on.)
    expect(g.world().restore(later).ok, "time passes");
    heard = answer(nell, "Is there anyone who could show me about?");
    const auto asked = mind.last();
    expect(contains(asked.string("activity"), "point this wolf to " + moLook) && !contains(asked.string("activity"), "Mo,"),
           "the model is told whom, by look: " + asked.string("activity"));
    expect(contains(heard, "Go and find " + moLook) && !contains(heard, " Mo ") && !contains(heard, "Mo,") && !contains(heard, "Mo knows"),
           "the name it wrote is the look she knows: " + heard);
    fs::remove_all(root);
}
void vouchRules()
{
    const auto& r = newcomers::rules();
    const auto share = newcomers::vouchShare(40, 30, r);
    expect(std::abs(share.trust - 12) < 1e-9 && std::abs(share.liking - 6) < 1e-9 && share.familiarity == 10, "30% of trust, 20% of liking, 10 familiarity");
    const auto most = newcomers::vouchShare(90, 90, r);
    expect(most.trust == 15 && most.liking == 10, "at most 15 and 10");
    std::string why;
    expect(newcomers::mayVouch(30, 20, 0, 0, false, why, r), "trust 30 and liking 20 may vouch");
    expect(!newcomers::mayVouch(29, 20, 0, 0, false, why, r) && contains(why, "think well enough"), "not below them");
    expect(!newcomers::mayVouch(40, 30, -20, 0, false, why, r) && contains(why, "distrust"), "not for a wolf it distrusts");
    expect(!newcomers::mayVouch(40, 30, 0, 0, true, why, r) && contains(why, "already"), "one vouch a pair");
    expect(!newcomers::mayVouch(40, 30, 0, 5, false, why, r) && contains(why, "5 wolves"), "five at once at most");
    newcomers::Vouch v;
    v.id = "vouch-1", v.household = {{"h", 6, 3, 5}}, v.given = {12, 6, 10};
    const auto back = newcomers::loadVouch(newcomers::saveVouch(v));
    expect(back.id == "vouch-1" && back.household.size() == 1 && back.household[0].trust == 6 && back.given.trust == 12, "saved and read back");
}

// The strip again: an evening at Upper Accord's inn, and vouching there.
struct Inn
{
    fs::path root;
    game::Game g;
    std::vector<Client*> all;
    std::map<Client*, std::string> ids;
    explicit Inn(const std::string& tag) : root(fs::temp_directory_path() / ("ratw-inn-" + tag + std::to_string(::getpid()))),
                                           g(options(writeStrip(root)))
    {
        std::string problem;
        expect(g.start(problem), "starts: " + problem);
        g.tick(1);
    }
    ~Inn() { fs::remove_all(root); }
    std::string add(Client& c, const char* name, int cell, double x)
    {
        c.id = int(all.size()) + 1;
        all.push_back(&c);
        std::string user = name;
        for (auto& ch : user)
            ch = char(std::tolower(static_cast<unsigned char>(ch)));
        signIn(g, c, (user + "i").c_str());
        ids[&c] = create(g, c, name, "", user + "1");
        enter(g, c, ids[&c]);
        place(ids[&c], cell, x, 10.5);
        return ids[&c];
    }
    void place(const std::string& id, int cell, double x, double y)
    {
        auto* e = g.world().entity(id);
        e->cellId = cellId(cell);
        e->position = {x, y};
    }
    void run(double seconds)
    {
        for (double t = 0; t < seconds; t += 0.5)
        {
            g.tick(0.5);
            for (auto* c : all)
                if (!c->snapshots.empty())
                    g.acknowledge(c, c->snapshots.back().number("revision"), false);
        }
    }
    void at(double day, double hour)
    {
        auto later = g.world().save();
        later.calendarDays = day + hour / 24;
        expect(g.world().restore(later).ok, "the hour set");
    }
    void knows(const std::string& resident, const std::string& id, double familiarity, double trust = 0, double liking = 0)
    {
        g.world().bonds().change(resident, id, {liking, trust, familiarity, 0, 0}, g.world().calendarDays());
    }
};

void evenings()
{
    Inn inn("evening");
    auto& g = inn.g;
    Client nell, vic, val, otto, bea, pip;
    const auto nellId = inn.add(nell, "Nell", 1, 8.5);
    const auto vicId = inn.add(vic, "Vic", 1, 9.5);
    const auto valId = inn.add(val, "Val", 1, 10.5);
    const auto ottoId = inn.add(otto, "Otto", 1, 11.5);
    const auto beaId = inn.add(bea, "Bea", 1, 13.5);
    const auto pipId = inn.add(pip, "Pip", 4, 8.5);  // (Another newcomer, alone in Ser Ferro's home cell for now.)
    for (auto* c : {&vic, &val, &otto, &bea})
        g.ledger().points[inn.ids[c]] = int(practice::xpFor(3));   // (Not new themselves.)
    inn.knows("ui", vicId, 20);
    inn.knows("ui", valId, 40);
    inn.knows("ui", ottoId, 20);
    inn.knows("ui", beaId, 20);
    g.command(&val, cmd({{"type", "profile"}, {"verb", "status"}, {"value", "lfs"}}));
    g.command(&otto, cmd({{"type", "profile"}, {"verb", "status"}, {"value", "ooc"}}));
    g.command(&nell, cmd({{"type", "safety"}, {"verb", "block"}, {"target", beaId}}));
    g.settle();
    inn.run(1);
    // Evening: the innkeeper at work in the common room.
    const double today = std::floor(g.world().calendarDays());
    inn.at(today, 19);
    inn.place("ui", 1, 12.5, 8.5);
    std::this_thread::sleep_for(std::chrono::milliseconds(5600));   // (Checked every five seconds, by the clock.)
    inn.run(1);
    const auto look = [](Client& c, const std::string& id) {
        auto name = c.seen(id) ? c.seen(id)->string("name") : std::string("?");
        if (!name.empty())
            name[0] = char(std::tolower(static_cast<unsigned char>(name[0])));
        return name;
    };
    const auto heard = [&](const std::string& text) {
        return std::any_of(nell.events.begin(), nell.events.end(), [&](const json::Value& e) { return contains(e.string("text"), text); });
    };
    expect(heard("Welcome, friend"), "Nell is welcomed aloud");
    expect(std::any_of(vic.events.begin(), vic.events.end(), [](const json::Value& e) { return contains(e.string("text"), "Welcome, friend"); }),
           "the room hears it too");
    expect(heard("That's " + look(nell, vicId) + " there: in now and then"), "Vic pointed out by look: in now and then");
    expect(heard("That's " + look(nell, valId) + " there: a regular here and looking for company tonight"), "Val: a regular, looking for company");
    expect(!heard("That's " + look(nell, ottoId)) && !heard("That's " + look(nell, beaId)), "not Otto (Out of character), not Bea (blocked)");
    expect(std::any_of(val.events.begin(), val.events.end(),
                       [&](const json::Value& e) { return contains(e.string("text"), "That's " + look(val, vicId) + " there"); }),
           "Val hears the same line with her own name for Vic: each listener as it knows him");
    const auto* toVic = vic.last("introducePrompt");
    expect(toVic && toVic->string("target") == nellId && contains(toVic->string("text"), "pointing you out to a newcomer"), "Vic is asked to introduce himself");
    expect(val.last("introducePrompt") && nell.last("introducePrompt") && nell.last("introducePrompt")->string("target").empty(),
           "Val too, and Nell (to the room)");
    expect(!otto.last("introducePrompt") && !bea.last("introducePrompt"), "not Otto or Bea");
    // Once a character.
    std::this_thread::sleep_for(std::chrono::milliseconds(5600));
    inn.run(1);
    expect(std::count_if(nell.events.begin(), nell.events.end(), [](const json::Value& e) { return contains(e.string("text"), "Welcome, friend"); }) == 1,
           "welcomed once");
    // Pip, alone at Ser Ferro's inn three evenings running: on the fourth, with company, nothing (three at most).
    for (int evening = 1; evening <= 4; ++evening)
    {
        inn.at(today + evening, 19);
        inn.place(pipId, 5, 10.5, 10.5);
        inn.place("si", 5, 11.5, 8.5);
        if (evening == 4)
        {
            inn.place(vicId, 5, 9.5, 10.5);
            inn.knows("si", vicId, 20);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5600));
        inn.run(1);
    }
    expect(!std::any_of(pip.events.begin(), pip.events.end(), [](const json::Value& e) { return contains(e.string("text"), "Welcome, friend"); }),
           "three evenings alone: no welcome on the fourth");
}

void vouching()
{
    Inn inn("vouch");
    auto& g = inn.g;
    Client mo, nell, vic;
    const auto moId = inn.add(mo, "Mo", 1, 9.5);
    const auto nellId = inn.add(nell, "Nell", 1, 10.5);
    const auto vicId = inn.add(vic, "Vic", 1, 11.5);
    inn.place("ui", 1, 10.5, 8.5);
    inn.knows("ui", moId, 10, 40, 30);
    inn.run(1);
    const auto actions = mo.seen("ui")->array("actions");
    expect(std::any_of(actions.begin(), actions.end(), [](const json::Value& a) { return a.asString({}) == "vouch"; }),
           "the innkeeper trusts Mo enough: Vouch for… in its menu");
    const auto vouch = [&](Client& c, const std::string& resident, const std::string& who) {
        g.command(&c, cmd({{"type", "vouch"}, {"resident", resident}, {"for", who}}));
        g.settle();
    };
    vouch(vic, "ui", nellId);
    expect(std::any_of(vic.events.begin(), vic.events.end(), [](const json::Value& e) { return contains(e.string("text"), "think well enough"); }) &&
               g.vouches().empty(),
           "Vic isn't trusted enough to vouch, and is told why");
    const auto trustIn = [&](const std::string& holder, const std::string& id) {
        const auto* b = g.world().bonds().find(holder, id);
        return b ? b->trust : 0.0;
    };
    std::string household;
    for (const auto& [id, life] : g.world().society().state().residents)
        if (household.empty() && g.world().society().household("ui", id))
            household = id;
    const double moBefore = trustIn("ui", moId);
    vouch(mo, "ui", nellId);
    expect(g.vouches().size() == 1 && g.vouches().begin()->second.state == "active", "Mo vouches for Nell");
    expect(std::any_of(nell.events.begin(), nell.events.end(), [](const json::Value& e) { return contains(e.string("text"), "I'll vouch for them"); }),
           "said aloud");
    const double nellTrust = trustIn("ui", nellId);
    expect(nellTrust > 9 && nellTrust <= 12.01, "the innkeeper's trust in Nell rises by a share of its trust in Mo: " + std::to_string(nellTrust));
    expect(!household.empty() && trustIn(household, nellId) > 4 && trustIn(household, nellId) < nellTrust,
           "its household, half as much (" + household + ": " + std::to_string(trustIn(household, nellId)) + ")");
    vouch(mo, "ui", nellId);
    expect(g.vouches().size() == 1, "one vouch a pair");
    // A crime elsewhere by one vouched for: nothing. (Mo vouches for Vic too, who steals in Ser Ferro.)
    vouch(mo, "ui", vicId);
    expect(g.vouches().size() == 2, "Mo vouches for Vic as well");
    auto& crime = g.world().crime();
    Incident far;
    far.id = "inc-9001", far.kind = "theft", far.offender = vicId, far.victim = "sm", far.cell = cellId(5);
    far.town = g.world().lawTown(cellId(5)), far.day = g.world().calendarDays();
    crime.incidents.push_back(far);
    inn.run(2);
    for (const auto& [id, v] : g.vouches())
        if (v.vouched == vicId)
            expect(v.state == "active", "a crime in another town, against someone else: the vouch stands");
    // Nell robs the innkeeper: the vouch breaks, Mo's standing takes twice the hit, the share is taken back.
    Incident theft;
    theft.id = "inc-9002", theft.kind = "theft", theft.offender = nellId, theft.victim = "ui", theft.cell = cellId(1);
    theft.town = g.world().lawTown(cellId(1)), theft.day = g.world().calendarDays();
    crime.incidents.push_back(theft);
    inn.run(2);
    for (const auto& [id, v] : g.vouches())
        if (v.vouched == nellId)
            expect(v.state == "broken", "a theft from the innkeeper by Nell breaks Mo's vouch");
    expect(moBefore - trustIn("ui", moId) > 15, "the innkeeper trusts Mo less by twice the share: " + std::to_string(trustIn("ui", moId)));
    expect(trustIn("ui", nellId) < 1 && trustIn(household, nellId) < 1, "the share taken back from Nell, by it and its household");
    expect(std::any_of(mo.events.begin(), mo.events.end(), [](const json::Value& e) { return contains(e.string("text"), "Word reaches you"); }),
           "Mo is told");
    // The innkeeper brings it up next time Mo speaks to it.
    g.command(&mo, cmd({{"type", "chat"}, {"text", "Evening."}, {"channel", "ic"}, {"volume", "speak"}, {"targets", parsed(R"(["ui"])")}}));
    g.settle();
    inn.run(2);
    bool raised = false;
    for (const auto& [id, v] : g.vouches())
        raised = raised || (v.vouched == nellId && v.raised);
    expect(raised, "the innkeeper's briefing brings it up");
}
} // namespace

int main()
{
    try
    {
        rules();
        means();
        oneTown();
        threeTowns();
        mentorRules();
        mentors();
        tieRules();
        ties();
        matchRules();
        matchmaking();
        vouchRules();
        evenings();
        vouching();
    }
    catch (const std::exception& error)
    {
        std::cerr << "newcomer_tests failed after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "newcomer_tests passed (" << checks << " checks)\n";
    return 0;
}
