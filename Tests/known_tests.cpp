// Known wolves and scene recaps (Docs/Design/50-player-card-friends-safety.md, Phase 4): a shared scene puts each
// member on the others' lists, counted, named as each knows them; a recap is written from only what that member
// perceived (a whisper out of earshot missing, a blocked wolf's lines missing, strangers by their look), by the model
// when there was enough of a scene and the player allows it, else plainly; notes, tags (custom, residents), forgetting
// and deleting a recap; the unread and noted marks; the caps; and all of it kept across a restart.
#include "RatwGame.h"
#include "RatwPeople.h"

#include <arpa/inet.h>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <iostream>
#include <mutex>
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

json::Value parsed(const std::string& text)
{
    json::Value v;
    std::string error;
    expect(json::parse(text, v, error), "JSON: " + error);
    return v;
}

// A stand-in for the NPC Mind: answers every POST /recap with a fixed recap and keeps the requests; anything else 503.
struct FakeMind
{
    int port = 0, fd = -1;
    std::mutex lock;
    std::vector<json::Value> recaps;
    std::atomic<bool> stop{false};
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
                if (in.rfind("POST /recap ", 0) == 0)
                {
                    json::Value body;
                    std::string error;
                    json::parse(in.substr(in.find("\r\n\r\n") + 4), body, error);
                    std::lock_guard<std::mutex> hold(lock);
                    recaps.push_back(body);
                    reply = R"({"recap": "You and the others talked of the river running high."})";
                    status = "200 OK";
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
    // The recap request made for the wolf named `you`.
    json::Value requestFor(const std::string& you)
    {
        std::lock_guard<std::mutex> hold(lock);
        for (const auto& r : recaps)
            if (r.string("you") == you)
                return r;
        return {};
    }
};

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
    bool said(const std::string& text) const
    {
        for (const auto& e : events)
            if (e.string("text").find(text) != std::string::npos)
                return true;
        return false;
    }
};

std::string cmd(std::initializer_list<std::pair<const char*, json::Value>> fields)
{
    auto o = json::Value::object();
    for (const auto& [k, v] : fields)
        o.add(k, v);
    return json::dump(o);
}

json::Value appearance(int colour)
{
    auto a = parsed(R"({"species": "timber", "sex": "female", "stature": "average", "pattern": "solid", "baseColor": 3, "gradientColor": 1,
                        "markingColor": 5, "gradientAmount": 0.5, "patternAmount": 0.5})");
    a.set("baseColor", colour);
    return a;
}

game::Options options(const std::string& mind = {}, const std::string& save = {})
{
    game::Options o;
    o.hiddenNames = true;
    o.forkSnapshots = false;
    o.oneWolfPerAccount = true;
    o.tiesOptional = true;                      // (Ties: doc 52, tested in newcomer_tests.)
    o.recapModelSeconds = 0;
    o.dialogueEndpoint = mind;
    if (!save.empty())
        o.savePath = save;
    return o;
}

// Three accounts: ada (Ada), bob (Bo) and cyd (Cy), side by side in one place; Cy a little further off.
struct World3
{
    game::Game g;
    Client ada, bo, cy;
    std::string adaId, boId, cyId;
    explicit World3(game::Options o, bool fresh = true) : g(o)
    {
        std::string problem;
        expect(g.start(problem), "starts: " + problem);
        int n = 1;
        for (auto* c : {&ada, &bo, &cy})
        {
            c->id = n++;
            g.connect(c);
        }
        const auto make = [&](Client& c, const char* user, const char* name, int colour) {
            g.command(&c, cmd({{"type", fresh ? "auth_register" : "auth_login"}, {"username", user}, {"password", "a long enough password"}}));
            g.settle();
            if (fresh)
                g.command(&c, cmd({{"type", "character_create"}, {"name", name}, {"age", 24}, {"appearance", appearance(colour)},
                                   {"commandId", std::string(user) + "0"}}));
            g.settle();
            expect(c.last("lobby") && c.last("lobby")->array("characters").size() == 1, std::string("a wolf for ") + user + ": " + (c.last("lobby") ? c.last("lobby")->string("message") : ""));
            return c.last("lobby")->array("characters")[0].string("id");
        };
        adaId = make(ada, "ada", "Ada", 3);
        boId = make(bo, "bob", "Bo", 6);
        cyId = make(cy, "cyd", "Cy", 1);
        for (auto [c, id, x] : {std::tuple{&ada, adaId, 0.0}, {&bo, boId, 1.2}, {&cy, cyId, 2.4}})
        {
            g.command(c, cmd({{"type", "character_enter"}, {"id", id}}));
            g.settle();
            auto* e = g.world().entity(id);
            auto* a = g.world().entity(adaId);
            expect(e != nullptr, "entered: " + id);
            if (a && e != a)
            {
                e->cellId = a->cellId;
                e->position = {a->position.x + x, a->position.y};
            }
        }
        tick(.5);
    }
    void tick(double seconds)
    {
        for (double t = 0; t < seconds; t += .05)
        {
            g.tick(.05);
            for (auto* c : {&ada, &bo, &cy})
                if (!c->snapshots.empty())
                    g.acknowledge(c, c->snapshots.back().number("revision"), false);
        }
    }
    void say(Client& c, const std::string& text, const char* volume = "speak")
    {
        g.command(&c, cmd({{"type", "chat"}, {"text", text}, {"channel", "ic"}, {"volume", volume}}));
        g.settle();
        tick(1.0);
        ::usleep(1050000);                          // (The ledger takes a wolf's line every two real seconds.)
    }
    void send(Client& c, const std::string& type, std::initializer_list<std::pair<const char*, json::Value>> fields)
    {
        auto o = json::Value::object();
        o.add("type", type);
        for (const auto& [k, v] : fields)
            o.add(k, v);
        g.command(&c, json::dump(o));
        g.settle();
    }
    void endScene(Client& by)
    {
        send(by, "action", {{"action", "session_end"}, {"target", ""}});
        tick(.2);
    }
    // Waits for the Mind's answers to come back (they arrive on the game thread, with the ticks).
    void waitForRecaps(const std::string& owner, const std::string& other, std::size_t count = 1)
    {
        for (int i = 0; i < 100; ++i)
        {
            const auto k = g.knownFor(owner, other);
            if (k.isObject() && k.array("recaps").size() >= count && k.array("recaps")[0].boolean("model"))
                return;
            ::usleep(50000);
            tick(.05);
        }
    }
};

// A scene of six lines: Ada and Bo talk; Bo whispers one to Ada; Cy (whom Ada has blocked) says one.
void talk(World3& w)
{
    w.say(w.ada, "\"Good evening. The river runs high tonight, doesn't it?\"");
    w.say(w.bo, "\"Higher than I've seen it in years. The ford is gone.\"");
    w.say(w.ada, "\"Then we'll have to go round by the old mill.\"");
    w.say(w.bo, "\"The key is under the third stone.\"", "whisper");
    w.say(w.cy, "\"Mind if I listen in? I know that mill.\"");
    w.say(w.ada, "\"Let's go before the light fails.\"");
    w.say(w.bo, "\"Lead on, then.\"");
}

void aSceneAndItsRecaps()
{
    FakeMind mind;
    World3 w(options(mind.endpoint()));
    w.g.world().entity(w.cyId)->position.x += 6;     // (Out of a whisper's reach, still in earshot of speech.)
    w.tick(.3);
    w.send(w.ada, "safety", {{"verb", "block"}, {"target", w.cyId}});
    talk(w);
    w.endScene(w.ada);
    w.waitForRecaps(w.adaId, w.boId);
    w.waitForRecaps(w.boId, w.adaId);
    // On each other's lists, counted, as each knows the other: strangers, by their look.
    const auto adaOnBo = w.g.knownFor(w.adaId, w.boId), boOnAda = w.g.knownFor(w.boId, w.adaId);
    expect(adaOnBo.isObject() && boOnAda.isObject(), "a shared scene puts each on the other's list");
    expect(adaOnBo.number("scenes") == 1 && boOnAda.number("scenes") == 1, "one scene shared, each way");
    expect(adaOnBo.string("name") != "Bo" && !adaOnBo.string("name").empty(), "Bo by his look, not his name: " + adaOnBo.string("name"));
    expect(!adaOnBo.string("place").empty() && adaOnBo.number("lastMet") > 0, "where and when they last met");
    expect(!w.g.knownFor(w.adaId, w.cyId).isObject(), "Cy, whom Ada blocked, isn't on her list");
    // Ada's recap: by the model, from what she perceived, everyone as she knew them.
    const auto adaRecap = adaOnBo.array("recaps");
    expect(adaRecap.size() == 1 && adaRecap[0].boolean("model") && adaRecap[0].string("text").find("river running high") != std::string::npos,
           "Ada's recap, by the model: " + json::dump(adaOnBo));
    const auto asked = mind.requestFor("Ada");
    expect(asked.isObject(), "the Mind was asked for Ada's recap");
    std::string adaLines;
    for (const auto& l : asked.array("lines"))
        adaLines += l.string("who") + ": " + l.string("text") + "\n";
    expect(adaLines.find(asked.string("you") + ": ") != std::string::npos && adaLines.find("river runs high") != std::string::npos,
           "her own lines, by her name: " + adaLines);
    expect(adaLines.find("\"") != std::string::npos, "speech in quotes, so the model can tell it from what was done: " + adaLines);
    const auto wolves = asked.array("wolves");
    expect(!wolves.empty() && wolves[0].string("who") == asked.string("you"), "her own wolf first among the wolves: " + json::dump(asked));
    expect(std::all_of(wolves.begin(), wolves.end(), [](const json::Value& v) { return v.string("pronouns") == "she/her"; }),
           "each wolf's pronouns, from its sex, go to the model: " + json::dump(asked));
    expect(adaLines.find("third stone") != std::string::npos, "the whisper meant for her");
    expect(adaLines.find("I know that mill") == std::string::npos, "but nothing of Cy, whom she blocked");
    expect(adaLines.find("Bo:") == std::string::npos && adaLines.find(adaOnBo.string("name") + ": ") != std::string::npos,
           "and Bo by his look:\n" + adaLines);
    // Cy's: the speech, not the whisper out of his reach.
    const auto cyAsked = mind.requestFor("Cy");
    std::string cyLines;
    for (const auto& l : cyAsked.array("lines"))
        cyLines += l.string("who") + ": " + l.string("text") + "\n";
    expect(cyLines.find("ford is gone") != std::string::npos && cyLines.find("third stone") == std::string::npos,
           "Cy's lines hold the speech and not the whisper:\n" + cyLines);
    // Introductions: the list names them once known.
    w.say(w.bo, "\"I'm Bo.\"");
    expect(w.g.knownFor(w.adaId, w.boId).string("name") == "Bo", "introduced: Bo by name on her list");
}

void writtenRecaps()
{
    // No Mind: the written recap from the ledger alone.
    World3 w(options());
    talk(w);
    w.endScene(w.ada);
    const auto k = w.g.knownFor(w.adaId, w.boId);
    std::string scenes;
    for (const auto& [sid, sc] : w.g.ledger().sessions)
    {
        scenes += sid + " ended " + std::to_string(sc.ended) + " members";
        for (const auto& [m, c] : sc.members)
            scenes += " " + m + (c.left ? "(left)" : "");
        scenes += "; ";
    }
    expect(k.isObject() && k.array("recaps").size() == 1 && !k.array("recaps")[0].boolean("model"),
           "a written recap: " + json::dump(k) + " scenes: " + scenes);
    const auto text = k.array("recaps")[0].string("text");
    auto label = k.string("name");
    label[0] = char(std::tolower(static_cast<unsigned char>(label[0])));
    expect(text.rfind("You shared a scene with ", 0) == 0 && text.find(label) != std::string::npos && text.find(" at ") != std::string::npos,
           "whom with and where: " + text);
    expect(text.find("Bo ") == std::string::npos && text.find("Bo.") == std::string::npos && text.find("Bo,") == std::string::npos,
           "never a name Ada doesn't know: " + text);
    // A player who turns model recaps off gets the written one, and nothing of theirs is sent.
    FakeMind mind;
    World3 x(options(mind.endpoint()));
    x.send(x.bo, "profile", {{"verb", "settings"}, {"settings", parsed(R"({"recaps": false})")}});
    talk(x);
    x.endScene(x.ada);
    x.waitForRecaps(x.adaId, x.boId);
    const auto boOnAda = x.g.knownFor(x.boId, x.adaId);
    expect(boOnAda.array("recaps").size() == 1 && !boOnAda.array("recaps")[0].boolean("model"), "recaps off: Bo's is written");
    expect(!mind.requestFor("Bo").isObject(), "and none of what Bo perceived went to the Mind");
    expect(x.g.knownFor(x.adaId, x.boId).array("recaps")[0].boolean("model"), "Ada's still by the model");
}

void notesTagsAndMarks()
{
    World3 w(options());
    talk(w);
    w.endScene(w.ada);
    // The older note verb still works, and lands on her list.
    w.send(w.ada, "social", {{"verb", "note"}, {"target", w.boId}, {"text", "Knows the mill."}});
    expect(w.g.knownFor(w.adaId, w.boId).string("note") == "Knows the mill.", "a note, on her list");
    w.send(w.ada, "known", {{"verb", "note"}, {"target", w.boId}, {"text", std::string(501, 'x')}});
    expect(w.ada.said("A note is at most 500 letters."), "500 letters at most");
    w.send(w.ada, "known", {{"verb", "tag"}, {"target", w.boId}, {"tag", "friendly"}});
    expect(w.g.knownFor(w.adaId, w.boId).string("tag") == "friendly", "a tag");
    w.send(w.ada, "known", {{"verb", "tag"}, {"target", w.boId}, {"tag", "nemesis"}});
    expect(w.ada.said("Not a tag."), "not one of the tags");
    w.send(w.ada, "known", {{"verb", "tag"}, {"target", w.boId}, {"tag", "River guide"}, {"custom", true}});
    const auto tagged = w.g.knownFor(w.adaId, w.boId);
    expect(tagged.string("tag") == "River guide" && tagged.boolean("customTag"), "one she names herself");
    // The card shows what she keeps; the map marks a wolf with a note.
    w.send(w.ada, "action", {{"action", "inspect"}, {"target", w.boId}});
    const auto* card = w.ada.last("inspect");
    expect(card && card->string("note") == "Knows the mill." && card->object("known").string("tag") == "River guide" &&
               card->object("known").array("recaps").size() == 1,
           "her card on Bo: note, tag, recaps");
    // Pronouns follow the character's sex, and aren't a profile setting (the user: male or female, nothing else).
    expect(card && card->object("profile").string("pronouns") == "she/her", "Bo was made female: she/her on his card");
    w.tick(1.0);
    expect(w.ada.seen(w.boId) && w.ada.seen(w.boId)->boolean("noted"), "noted, on the map");
    // Unread: Bo changes his profile; it's marked until Ada looks again.
    w.send(w.bo, "profile", {{"verb", "set"}, {"fields", parsed(R"({"currently": "watching the water"})")}});
    w.tick(1.0);
    expect(w.ada.seen(w.boId) && w.ada.seen(w.boId)->boolean("unread"), "his profile changed: unread");
    w.send(w.ada, "action", {{"action", "inspect"}, {"target", w.boId}});
    w.tick(1.0);
    expect(w.ada.seen(w.boId) && !w.ada.seen(w.boId)->boolean("unread"), "looked at: read");
    // A resident joins the list when tagged; the list, newest first.
    std::string resident;
    for (const auto& [id, e] : w.g.world().entities())
        if (e.npc && e.cellId == w.g.world().entity(w.adaId)->cellId && w.g.world().visionClarity(w.adaId, id) > 0)
        {
            resident = id;
            break;
        }
    expect(!resident.empty(), "a resident in sight");
    w.send(w.ada, "known", {{"verb", "list"}});
    const auto before = w.ada.last("known")->array("wolves").size();
    w.send(w.ada, "known", {{"verb", "tag"}, {"target", resident}, {"tag", "business"}});
    w.send(w.ada, "known", {{"verb", "list"}});
    const auto& wolves = w.ada.last("known")->array("wolves");
    bool hasResident = false;
    for (const auto& k : wolves)
        hasResident |= k.string("id") == resident && k.boolean("resident");
    expect(wolves.size() == before + 1 && hasResident, "a resident, tagged, is on her list");
    // Deleting a recap; forgetting a wolf.
    const auto recap = w.g.knownFor(w.adaId, w.boId).array("recaps")[0].string("id");
    w.send(w.ada, "known", {{"verb", "unrecap"}, {"recap", recap}});
    expect(w.g.knownFor(w.adaId, w.boId).array("recaps").empty(), "a recap deleted");
    w.send(w.ada, "known", {{"verb", "forget"}, {"target", w.boId}});
    expect(!w.g.knownFor(w.adaId, w.boId).isObject(), "Bo forgotten");
    expect(w.g.knownFor(w.boId, w.adaId).isObject(), "while Bo still remembers her");
}

void caps()
{
    // 300 players at most: the oldest met without a note, tag or recap go first.
    std::map<std::string, people::KnownWolf> list;
    for (int i = 0; i < 305; ++i)
    {
        people::KnownWolf k;
        k.lastMet = 1000 + i;
        if (i < 3)
            k.note = "an old friend";
        list["w" + std::to_string(i)] = k;
    }
    people::Recap kept;
    kept.others = {"w3"};
    const auto dropped = people::trimKnown(list, {kept});
    expect(list.size() == 300 && dropped.size() == 5, "300 kept");
    expect(list.count("w0") && list.count("w1") && list.count("w2") && list.count("w3"), "the noted and recapped stay, oldest as they are");
    expect(!list.count("w4") && !list.count("w8") && list.count("w9"), "the oldest unremarked go");
    // Recaps: each wolf shows its newest 3.
    std::vector<people::Recap> recaps;
    for (int i = 0; i < 5; ++i)
    {
        people::Recap r;
        r.id = "r" + std::to_string(i);
        r.at = 100 + i;
        r.others = {"bo"};
        recaps.push_back(r);
    }
    recaps[0].others.push_back("cy");                 // (Cy's only one: it stays.)
    people::trimRecaps(recaps);
    expect(recaps.size() == 4 && recaps[0].id == "r0" && recaps.back().id == "r4", "Bo's newest 3, and Cy's one: " +
           std::to_string(recaps.size()));
}

void keptAcrossARestart()
{
    const std::string save = "/tmp/ratw-known-" + std::to_string(::getpid()) + ".json";
    std::remove(save.c_str());
    {
        World3 w(options({}, save));
        talk(w);
        w.endScene(w.ada);
        w.send(w.ada, "known", {{"verb", "note"}, {"target", w.boId}, {"text", "Knows the mill."}});
        w.g.save();
    }
    {
        World3 w(options({}, save), false);
        const auto k = w.g.knownFor(w.adaId, w.boId);
        expect(k.isObject() && k.string("note") == "Knows the mill." && k.number("scenes") == 1 && k.array("recaps").size() == 1,
               "her list after a restart: " + json::dump(k));
        w.tick(1.0);
        // (Scenes over before the restart aren't recapped again.)
        expect(w.g.knownFor(w.adaId, w.boId).array("recaps").size() == 1, "and no second recap");
    }
    std::remove(save.c_str());
}
} // namespace

int main()
{
    try
    {
        caps();
        writtenRecaps();
        notesTagsAndMarks();
        aSceneAndItsRecaps();
        keptAcrossARestart();
    }
    catch (const std::exception& e)
    {
        std::cerr << "known_tests failed after " << checks << " checks: " << e.what() << '\n';
        return 1;
    }
    std::cout << "known_tests passed (" << checks << " checks)\n";
    return 0;
}
