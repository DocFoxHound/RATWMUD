// Ticket mode (Docs/Design/77-central-accounts-and-hosted-worlds.md, sections 5 and 5.1): central's tickets as a world
// checks them (RatwTickets.h), every rule; and the game in ticket mode (Options::central), driven as a client drives it:
// hello{ticket, protocol}, the first arrival (character_arrive), straight in after, the account entry by central id,
// the arrival limits, the save, and no password from anywhere. Tickets are signed here with a key of the test's own,
// through the same libcrypto (sys::ed25519Sign); Central/test_world_join.py does it with central itself.
#include "RatwBuildInfo.h"
#include "RatwGame.h"
#include "RatwSections.h"
#include "RatwSystemLibs.h"
#include "RatwTickets.h"
#include "RatwWire.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <cmath>

using namespace ratw;
using json::Value;

namespace
{
int checks = 0;
void expect(bool condition, const std::string& message)
{
    ++checks;
    if (!condition)
        throw std::runtime_error(message);
}

const std::string World1 = "world-0123456789abcdef0123456789abcdef";
const std::string World2 = "world-fedcba9876543210fedcba9876543210";
const std::string Wolf1 = "wolf-11111111111111111111111111111111";
const std::string Wolf2 = "wolf-22222222222222222222222222222222";
const std::string Wolf3 = "wolf-33333333333333333333333333333333";
const std::string Acct1 = "acct-aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
const std::string Acct2 = "acct-bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb";

// A signing key of central's, as the test plays central.
struct Key
{
    std::uint8_t secret[32] = {}, pub[32] = {};
    std::string kid;
    explicit Key(std::string id) : kid(std::move(id))
    {
        expect(sys::randomBytes(secret, 32) && sys::ed25519PublicKey(secret, pub), "a key is made");
    }
    Value jwk() const
    {
        auto k = Value::object();
        k.add("kty", "OKP");
        k.add("crv", "Ed25519");
        k.add("alg", "Ed25519");
        k.add("use", "sig");
        k.add("kid", kid);
        k.add("x", tickets::toBase64url(std::string(reinterpret_cast<const char*>(pub), 32)));
        return k;
    }
    // A ticket of raw header and payload text (so a test can repeat a key), signed.
    std::string signRaw(const std::string& header, const std::string& payload) const
    {
        const std::string input = tickets::toBase64url(header) + "." + tickets::toBase64url(payload);
        std::string signature;
        expect(sys::ed25519Sign(secret, input, signature), "a ticket is signed");
        return input + "." + tickets::toBase64url(signature);
    }
    std::string sign(const Value& claims, const std::string& alg = "Ed25519") const
    {
        auto header = Value::object();
        header.add("alg", alg);
        header.add("typ", "JWT");
        header.add("kid", kid);
        return signRaw(json::dump(header), json::dump(claims));
    }
};

Value jwks(std::initializer_list<const Key*> keys)
{
    auto set = Value::object();
    auto list = Value::array();
    for (const auto* k : keys)
        list.push(k->jwk());
    set.add("keys", list);
    return set;
}

Value look(const std::string& sex = "female", int baseColor = 3)
{
    auto a = Value::object();
    a.add("species", "timber"); a.add("sex", sex); a.add("stature", "average");
    a.add("pattern", "saddle"); a.add("baseColor", baseColor); a.add("gradientColor", 1);
    a.add("markingColor", 5); a.add("gradientAmount", .5); a.add("patternAmount", .5);
    Appearance out;
    expect(wire::readAppearance(a, out), "the test's look is a look");
    return wire::appearance(out);
}

int nextJti = 0;
Value claims(double now, const std::string& wolf = Wolf1, const std::string& account = Acct1, const std::string& name = "Wren Reed",
             const std::string& world = World1, const std::string& kind = "community")
{
    char jti[40];
    std::snprintf(jti, sizeof jti, "jti%029d", ++nextJti);
    auto c = Value::object();
    c.add("sub", account);
    c.add("wolf", wolf);
    c.add("name", name);
    c.add("sex", "female");
    c.add("age", 24);
    c.add("look", look());
    c.add("gift", "fire");
    c.add("aud", world);
    c.add("kind", kind);
    c.add("jti", std::string(jti));
    c.add("iat", now);
    c.add("exp", now + 60);
    return c;
}

std::string flip(std::string ticket, std::size_t at)
{
    // A character of the ticket swapped for another of the alphabet.
    ticket[at] = ticket[at] == 'A' ? 'B' : 'A';
    return ticket;
}

// ------------------------------------------------------------------------------------------------ The check alone

void theCheckerKeepsEveryRule()
{
    const double now = 1'800'000'000, started = now - 600;
    Key k("k1"), other("k1"), k2("k2");
    tickets::Checker checker;
    checker.configure(World1, false, started);
    checker.setKeys(tickets::readKeys(jwks({&k})));
    expect(checker.keys().size() == 1, "the keys are read from a JWK set");
    tickets::Claims got;
    std::string why;
    const auto verdict = [&](const std::string& ticket, double at = 0) { return checker.check(ticket, at == 0 ? now : at, got, why); };
    const auto admitted = [&](const std::string& ticket, double at = 0) { return verdict(ticket, at) == tickets::Verdict::Admitted; };

    const auto good = k.sign(claims(now));
    const bool first = admitted(good);
    expect(first, "a good ticket is admitted: " + why);
    expect(got.account == Acct1 && got.wolf == Wolf1 && got.name == "Wren Reed" && got.sex == "female" && got.age == 24 &&
               got.family == "fire" && got.kind == "community" && got.look == look(),
           "and says who the wolf is");
    expect(!admitted(good) && why == "ticket already used", "the same ticket twice is refused (its jti): " + why);
    expect(!admitted(k.sign(claims(now, Wolf1, Acct1, "Wren Reed", World2))) && why == "ticket for another world",
           "a ticket for another world is refused");
    expect(!admitted(k.sign(claims(now - 200))) && why == "expired", "an expired ticket is refused (past the leeway)");
    expect(admitted(k.sign(claims(now - 150))), "a ticket 90 s past its end is admitted: a wrong clock (" + why + ")");
    expect(admitted(k.sign(claims(now + 90))), "a central clock 90 s ahead is admitted (" + why + ")");
    expect(!admitted(k.sign(claims(now + 200))) && why == "issued in the future", "200 s ahead is refused: " + why);
    expect(!admitted(k.sign(claims(started - 200)), started - 150) && why == "issued before this world started",
           "a ticket issued before the world started is refused: " + why);
    expect(admitted(k.sign(claims(started - 90)), started - 80), "90 s before the start is the leeway's (" + why + ")");
    {
        auto c = claims(now);
        c.set("exp", now + 3600);
        expect(!admitted(k.sign(c)) && why == "wrong lifetime", "a ticket that lives an hour is refused: " + why);
        c = claims(now);
        c.set("exp", now);
        expect(!admitted(k.sign(c)) && why == "wrong lifetime", "and one that ends as it starts");
    }
    // The algorithm, before anything else.
    for (const char* alg : {"none", "EdDSA", "HS256", "ed25519", "RS256", ""})
        expect(!admitted(k.sign(claims(now), alg)) && why == "wrong algorithm", std::string("alg ") + alg + " is refused");
    {
        // HS256 with the public key as its secret (the classic confusion) can't even be signed here: its header alone
        // is refused, before the signature is looked at.
        const auto body = json::dump(claims(now));
        const auto noneTicket = tickets::toBase64url("{\"alg\":\"none\",\"kid\":\"k1\"}") + "." + tickets::toBase64url(body) + ".";
        expect(!admitted(noneTicket), "an unsigned ticket is refused");
        expect(!admitted(k.signRaw("{\"kid\":\"k1\"}", body)) && why == "wrong algorithm", "no alg is refused");
        expect(!admitted(k.signRaw("{\"alg\":\"none\",\"alg\":\"Ed25519\",\"kid\":\"k1\"}", body)),
               "a doubled alg is refused (the last copy would say Ed25519)");
        expect(!admitted(k.signRaw("{\"alg\":\"Ed25519\",\"kid\":\"k1\",\"jku\":\"http://evil/\"}", body)) && why == "unexpected header",
               "a header field that would steer the check is refused");
        expect(!admitted(k.signRaw("{\"alg\":\"Ed25519\",\"typ\":\"JOSE\",\"kid\":\"k1\"}", body)), "a typ other than JWT is refused");
        std::string doubled = body;
        doubled.insert(1, "\"aud\":\"" + World2 + "\",");
        expect(!admitted(k.signRaw("{\"alg\":\"Ed25519\",\"kid\":\"k1\"}", doubled)), "a doubled claim is refused, even signed");
        auto nested = claims(now);
        expect(admitted(k.signRaw("{\"alg\":\"Ed25519\",\"kid\":\"k1\"}", json::dump(nested))), "(the same, once, is fine)");
        std::string doubledLook = json::dump(claims(now));
        const auto at = doubledLook.find("\"look\":{") + 8;
        doubledLook.insert(at, "\"sex\":\"male\",");
        expect(!admitted(k.signRaw("{\"alg\":\"Ed25519\",\"kid\":\"k1\"}", doubledLook)), "a key repeated deep inside is refused too");
    }
    // Keys.
    const auto byK2 = k2.sign(claims(now));
    expect(verdict(byK2) == tickets::Verdict::UnknownKey, "a key the world doesn't hold asks for the keys again");
    expect(!admitted(other.sign(claims(now))) && why == "bad signature", "another key under a known kid is a bad signature");
    {
        auto header = Value::object();
        header.add("alg", "Ed25519");
        expect(!admitted(k.signRaw(json::dump(header), json::dump(claims(now)))) && why == "unknown key", "no kid is refused");
    }
    checker.setKeys(tickets::readKeys(jwks({&k, &k2})));
    expect(admitted(byK2), "once fetched, the new key's ticket is admitted (it wasn't used up): " + why);
    // Tampering and spelling.
    const auto fresh = k.sign(claims(now));
    const auto dot2 = fresh.rfind('.');
    expect(!admitted(flip(fresh, dot2 + 5)) && why == "bad signature", "a changed signature is refused");
    expect(!admitted(flip(fresh, fresh.find('.') + 10)), "a changed payload is refused");
    expect(!admitted(fresh + "="), "padding is refused");
    expect(!admitted(fresh.substr(0, dot2)), "two parts are refused");
    expect(!admitted(fresh + ".abc"), "four parts are refused");
    expect(!admitted(""), "nothing is refused");
    expect(!admitted(std::string(5000, 'A') + "." + fresh), "a ticket too long is refused");
    {
        // A last character with stray bits set spells the same bytes another way: refused, so one ticket has one spelling.
        std::string raw, alt = fresh;
        char& last = alt.back();
        const std::string alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
        const auto index = alphabet.find(last);
        last = alphabet[(index & ~std::size_t(15)) | ((index + 1) & 15)];
        expect(!tickets::base64url(alt.substr(dot2 + 1), raw), "stray bits in base64url are refused");
        expect(admitted(fresh), "(the right spelling is admitted)");
    }
    // The claims, typed and in the game's own forms.
    const auto refusedWith = [&](const std::function<void(Value&)>& change, const std::string& what) {
        auto c = claims(now);
        change(c);
        expect(!admitted(k.sign(c)), what + " is refused");
    };
    refusedWith([](Value& c) { c.set("exp", "1800000060"); }, "exp as a string");
    refusedWith([](Value& c) { c.set("iat", 1800000000.5); }, "a fractional iat");
    refusedWith([](Value& c) { c.set("age", true); }, "an age that is a bool");
    refusedWith([](Value& c) { c.erase("wolf"); }, "a ticket without a wolf");
    refusedWith([](Value& c) { c.set("wolf", "wolf-XYZ"); }, "a malformed wolf id");
    refusedWith([](Value& c) { c.set("wolf", "player-ash"); }, "a development identity's id");
    refusedWith([](Value& c) { c.set("sub", "ash"); }, "an account that isn't central's id");
    refusedWith([](Value& c) { c.set("name", "wren reed"); }, "a name not in the game's form (a capital first)");
    refusedWith([](Value& c) { c.set("name", "W"); }, "a name too short");
    refusedWith([](Value& c) { c.set("age", 5); }, "an age under 6");
    refusedWith([](Value& c) { c.set("sex", "male"); }, "a sex that isn't the look's");
    refusedWith([](Value& c) { c.set("sex", "other"); }, "a sex that is neither");
    refusedWith([](Value& c) { c.set("look", look("male")); }, "a look of the other sex");
    refusedWith([](Value& c) { auto l = c["look"]; l.set("species", "dragon"); c.set("look", l); }, "a look that isn't a wolf's");
    refusedWith([](Value& c) { c.set("gift", "storm-king"); }, "a family that isn't playable");
    refusedWith([](Value& c) { c.set("kind", "official"); }, "an official ticket on a community world");
    refusedWith([](Value& c) { c.set("kind", "secret"); }, "an unknown kind");
    refusedWith([](Value& c) { c.set("jti", "short"); }, "a jti too short");
    refusedWith([](Value& c) { c.set("jti", "has spaces in it, sixteen+"); }, "a jti with odd characters");
    {
        auto c = claims(now);
        c.set("gift", "");
        expect(admitted(k.sign(c)) && got.family.empty(), "no family is fine: " + why);
        c = claims(now);
        c.add("extra", "a claim central may add later");
        expect(admitted(k.sign(c)), "a claim the world doesn't know is ignored");
    }
    tickets::Checker official;
    official.configure(World1, true, started);
    official.setKeys(tickets::readKeys(jwks({&k})));
    expect(official.check(k.sign(claims(now, Wolf1, Acct1, "Wren Reed", World1, "official")), now, got, why) == tickets::Verdict::Admitted,
           "an official world admits official tickets");
    expect(official.check(k.sign(claims(now)), now, got, why) == tickets::Verdict::Refused, "and not community ones");
    // What a key set may hold.
    auto odd = jwks({&k});
    auto rsa = Value::object();
    rsa.add("kty", "RSA");
    rsa.add("kid", "r1");
    rsa.add("n", "AQAB");
    auto list = odd["keys"].items();
    list.push_back(rsa);
    auto shortKey = k.jwk();
    shortKey.set("kid", "s1");
    shortKey.set("x", "AAAA");
    list.push_back(shortKey);
    auto eddsa = k.jwk();
    eddsa.set("kid", "e1");
    eddsa.set("alg", "EdDSA");
    list.push_back(eddsa);
    odd.set("keys", Value(list));
    expect(tickets::readKeys(odd).size() == 1, "only Ed25519 keys of 32 bytes, alg Ed25519, are read from a key set");
    expect(checker.remembered() > 0, "admitted tickets are remembered");
    checker.check(k.sign(claims(now + 10'000)), now + 10'000, got, why);
    expect(checker.remembered() == 1, "and forgotten once they could no longer be admitted anyway");
}

void strictParsingAndTheLibrary()
{
    Value v;
    std::string error;
    expect(json::parse("{\"a\":1,\"a\":2}", v, error) && v.number("a") == 2, "the game's parser keeps the last copy (as ever)");
    expect(!json::parseUnique("{\"a\":1,\"a\":2}", v, error), "parseUnique refuses a repeated key");
    expect(!json::parseUnique("{\"o\":{\"b\":1,\"b\":1}}", v, error), "at any depth");
    expect(json::parseUnique("{\"a\":1,\"o\":{\"a\":2}}", v, error), "the same key in different objects is fine");
    std::string raw;
    expect(tickets::base64url(tickets::toBase64url("any bytes \x01\xff"), raw) && raw == "any bytes \x01\xff", "base64url round-trips");
    expect(!tickets::base64url("ab+/", raw) && !tickets::base64url("abc=", raw) && !tickets::base64url("a", raw),
           "only the URL alphabet, unpadded");
    std::string problem;
    expect(sys::ed25519Available(problem), "libcrypto's Ed25519 is there: " + problem);
    // RFC 8032's first test vector: the empty message.
    std::uint8_t secret[32], pub[32];
    sys::readHex("9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60", secret, 32);
    expect(sys::ed25519PublicKey(secret, pub) &&
               sys::hex(pub, 32) == "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a",
           "RFC 8032's public key");
    std::string signature;
    expect(sys::ed25519Sign(secret, "", signature) &&
               sys::hex(reinterpret_cast<const std::uint8_t*>(signature.data()), 64) ==
                   "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b",
           "and its signature");
    expect(sys::ed25519Verify(pub, "", signature) && !sys::ed25519Verify(pub, "x", signature), "which verifies, and only its message");
    expect(std::string(build::clientDataHash()).size() == 32 && build::Protocol >= 1, "the build knows its client-data hash");
}

// ------------------------------------------------------------------------------------------------ The game

struct Client final : game::Connection
{
    std::vector<Value> events;
    bool local = true;                              // (Loopback: in ticket mode it must grant nothing.)
    void event(const std::string& text) override
    {
        Value v;
        std::string error;
        expect(json::parse(text, v, error), "events are JSON");
        events.push_back(v);
    }
    std::vector<Value> snapshots;
    sections::Cache cache;
    void snapshot(const std::string& text) override
    {
        Value v;
        std::string error;
        expect(json::parse(text, v, error), "snapshots are JSON: " + error);
        expect(sections::fill(v, cache), "a snapshot can be filled from what the client holds");   // (As the client does.)
        snapshots.push_back(v);
    }
    void motion(const Value&) override {}
    bool allowsLocalCredentials() const override { return local; }
    bool closed = false;                            // (The game sent it out: the server would close the socket.)
    void closeAfterSending() override { closed = true; }
    const Value* last(const std::string& type) const
    {
        for (auto it = events.rbegin(); it != events.rend(); ++it)
            if (it->string("type") == type)
                return &*it;
        return nullptr;
    }
    std::string stage() const { return last("lobby") ? last("lobby")->string("stage") : std::string(); }
    std::string code() const { return last("lobby") ? last("lobby")->string("code") : std::string(); }
    std::string message() const { return last("lobby") ? last("lobby")->string("message") : std::string(); }
};

std::string cmd(std::initializer_list<std::pair<const char*, Value>> fields)
{
    auto o = Value::object();
    for (const auto& [k, v] : fields)
        o.add(k, v);
    return json::dump(o);
}

// A command, and its replies (which wait for the journal where there is one: doc 31).
void say(game::Game& g, Client* c, const std::string& text)
{
    g.command(c, text);
    g.settle();
}

std::string hello(const std::string& ticket, int protocol = build::Protocol)
{
    return cmd({{"type", "hello"}, {"ticket", ticket}, {"protocol", protocol}});
}

struct Fixture
{
    std::filesystem::path dir;
    std::string keys;
    Key k{"k1"};
    double clock = 1'800'000'000;
    Fixture()
    {
        dir = std::filesystem::temp_directory_path() / ("ratw-ticket-tests-" + std::to_string(::getpid()));
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir);
        keys = (dir / "central-keys.json").string();
        writeKeys({&k});
    }
    ~Fixture() { std::filesystem::remove_all(dir); }
    void writeKeys(std::initializer_list<const Key*> set)
    {
        const std::string temp = keys + ".new";
        std::ofstream(temp) << json::dump(jwks(set));
        std::filesystem::rename(temp, keys);
    }
    game::Options options(bool official = false, const std::string& save = {}) const
    {
        game::Options o;
        o.tiesOptional = true;
        o.hiddenNames = false;
        o.savePath = save;
        o.central.url = "http://127.0.0.1:1";
        o.central.worldId = World1;
        o.central.keysFile = keys;
        o.central.official = official;
        o.central.worldName = "Test Hollow";
        return o;
    }
    void wire(game::Game& g)
    {
        g.ticketClock = [this] { return clock; };
        g.log = [](const char*, const std::string&) {};
    }
    std::string ticket(const std::string& wolf = Wolf1, const std::string& account = Acct1, const std::string& name = "Wren Reed",
                       const std::function<void(Value&)>& change = {})
    {
        auto c = claims(clock, wolf, account, name);
        if (change)
            change(c);
        return k.sign(c);
    }
};

void ticketModeStartsOnlyAsItShould()
{
    Fixture f;
    const auto refused = [&](game::Options o, const std::string& why) {
        game::Game g(o);
        f.wire(g);
        std::string problem;
        expect(!g.start(problem) && !problem.empty(), why + " is refused at start");
        return problem;
    };
    auto o = f.options();
    o.central.worldId = "world-1";
    refused(o, "a malformed world id");
    o = f.options();
    o.central.keysFile.clear();
    refused(o, "no keys file");
    o = f.options();
    o.devIdentity = true;
    expect(refused(o, "--dev-identity").find("dev-identity") != std::string::npos, "saying why");
    o = f.options(true);
    o.devTools = true;
    refused(o, "--dev-tools on an official world");
    o = f.options(true);
    o.openTiers = true;
    refused(o, "--open-tiers on an official world");
    o = f.options();
    o.central.worldName = std::string(60, 'x');
    refused(o, "a world name past 48 bytes");
    o = f.options();
    o.devTools = true;                              // (A community world may: listed "cheats on".)
    {
        game::Game g(o);
        f.wire(g);
        std::string problem;
        expect(g.start(problem), "a community world with dev tools starts: " + problem);
        expect(g.worldStatus().boolean("cheats"), "and says so in its status");
    }
    o = f.options();
    o.central.keysFile = (f.dir / "none-yet.json").string();
    game::Game g(o);
    f.wire(g);
    std::string problem;
    expect(g.start(problem), "a world whose agent hasn't fetched the keys yet still starts: " + problem);
    const auto status = g.worldStatus();
    expect(status.string("id") == World1 && status.string("name") == "Test Hollow" && status.string("kind") == "community" &&
               status.number("protocol") == build::Protocol && status.string("clientData") == build::clientDataHash() &&
               status.number("players") == 0 && status.number("limit") == 100 && !status.boolean("voices") &&
               !status.boolean("private") && json::dump(status).size() < 2048,
           "its status has doc 77 5.1's fields: " + json::dump(status));
}

void wolvesJoinArriveAndComeStraightBack()
{
    Fixture f;
    const auto save = (f.dir / "save.json").string();
    game::Options o = f.options(false, save);
    {
        game::Game g(o);
        f.wire(g);
        std::string problem;
        expect(g.start(problem), "a ticket world starts: " + problem);
        Client a;
        a.id = 1;
        g.connect(&a);
        expect(a.stage() == "ticket" && a.last("lobby")->string("mode") == "ticket" && !a.last("lobby")->boolean("credentialsAllowed") &&
                   a.last("lobby")->number("protocol") == build::Protocol && a.last("lobby")->string("world") == World1,
               "a new client is asked for a ticket");
        // No passwords, from loopback or anywhere (doc 77, 5).
        say(g, &a, cmd({{"type", "auth_register"}, {"username", "wren"}, {"password", "a long enough password"}}));
        g.settle();
        expect(a.code() == "ticket_only" && a.accountUsername.empty(), "a password registration from loopback is refused");
        say(g, &a, cmd({{"type", "auth_login"}, {"username", "wren"}, {"password", "a long enough password"}}));
        g.settle();
        expect(a.code() == "ticket_only" && a.accountUsername.empty(), "and a sign-in");
        say(g, &a, cmd({{"type", "character_create"}, {"name", "Wren"}, {"age", 20}, {"appearance", look()}, {"commandId", "c1"}}));
        expect(a.code() == "ticket_only" && g.characters().empty(), "and creation");
        say(g, &a, cmd({{"type", "hello"}, {"id", "ash"}}));
        expect(a.entityId.empty() && a.code() == "invalid", "a development identity's hello is no way in");
        say(g, &a, hello(f.ticket(), build::Protocol + 1));
        expect(a.code() == "protocol", "a client on another protocol is told so");
        say(g, &a, cmd({{"type", "hello"}, {"ticket", f.ticket()}, {"protocol", build::Protocol}, {"password", "x"}}));
        expect(a.code() == "invalid", "a hello carries nothing else");
        say(g, &a, cmd({{"type", "character_arrive"}, {"commandId", "arrive-1"}}));
        expect(a.code() == "ticket" && g.characters().empty(), "nobody arrives without a ticket");
        // In, for the first time: the arrival.
        say(g, &a, hello(f.ticket()));
        const auto* lobby = a.last("lobby");
        expect(lobby->boolean("ok") && a.stage() == "arrive" && a.accountUsername == Acct1 && a.entityId.empty(),
               "a good ticket leads to the arrival: " + a.message());
        expect(lobby->object("wolf").string("name") == "Wren Reed" && lobby->object("wolf").string("family") == "fire" &&
                   lobby->object("gifts").array("families").size() == 8 && !lobby->object("creation").isNull() &&
                   !lobby->object("tiers").isNull() && lobby->boolean("firstCharacter"),
               "which shows the wolf from the ticket, and the tiers, strengths, starts and ties to choose from");
        expect(lobby->array("tierChoices").size() == 2 && lobby->array("tierChoices")[0].asString() == "gifted",
               "a wolf with a family chooses Gifted or Quickened");
        say(g, &a, cmd({{"type", "character_arrive"}, {"commandId", "arrive-1"}}));
        expect(a.code() == "invalid" && g.characters().empty(), "a wolf with a family can't arrive without a tier");
        say(g, &a, cmd({{"type", "character_arrive"}, {"commandId", "arrive-1"}, {"tier", "normal"}}));
        expect(a.code() == "invalid" && g.characters().empty(), "nor Normal (the user, 2026-11-07: only Normal wolves have no family)");
        say(g, &a, cmd({{"type", "character_arrive"}, {"commandId", "arrive-1"}, {"tier", "quickened"}}));
        expect(!a.last("lobby")->boolean("ok") && a.stage() == "arrive" && g.characters().empty(),
               "Quickened, which this account hasn't earned here, is refused (doc 49): " + a.message());
        say(g, &a, cmd({{"type", "character_arrive"}, {"commandId", "arrive-1"}, {"tier", "gifted"}, {"name", "Other"}}));
        expect(a.code() == "invalid", "an arrival can't rename the wolf");
        const auto treasuryBefore = g.world().society().account("treasury")->cash;
        say(g, &a, cmd({{"type", "character_arrive"}, {"commandId", "arrive-1"}, {"tier", "gifted"}}));
        g.settle();
        expect(a.entityId == Wolf1 && a.last("entered"), "the wolf arrives Gifted and comes in: " + a.message());
        const auto& wren = g.characters().at(Wolf1);
        expect(wren.name == "Wren Reed" && wren.age == 24 && wren.appearance->sex == "female" && wren.gift == "fire" && !wren.quickened,
               "as the ticket says: name, age, look, and its family as a Gifted wolf");
        const auto* purse = g.world().society().account(Wolf1);
        // (Doc 79, 9.2: the welcome is made, a faucet counted by the door; the treasury untouched.)
        expect(purse && purse->cash == 20 && g.world().society().account("treasury")->cash == treasuryBefore &&
                   g.world().society().faucetToday(g.world().society().holderOf(Wolf1), "welcome") >= 20 &&
                   g.world().society().realBalanced(),
               "with the welcome made (a faucet), the treasury untouched, real money balanced");
        // Leaving: back to the list; the ticket was used once.
        say(g, &a, cmd({{"type", "character_leave"}}));
        expect(a.entityId.empty() && a.accountUsername.empty() && a.stage() == "ticket", "leaving goes back to asking for a ticket");
        // One ticket per arrival: a used one again is refused.
        const auto again = f.ticket();
        Client b;
        b.id = 2;
        g.connect(&b);
        say(g, &b, hello(again));
        expect(b.entityId == Wolf1, "a new ticket brings the wolf straight in, no arrival");
        say(g, &b, cmd({{"type", "character_leave"}}));
        say(g, &b, hello(again));
        expect(b.entityId.empty() && b.code() == "ticket" && b.message().find("already used") != std::string::npos,
               "the same ticket again is refused: " + b.message());
        // The look changes on central: refreshed from the next ticket; the name never changes.
        f.clock += 30;
        say(g, &b, hello(f.ticket(Wolf1, Acct1, "Wren Renamed", [](Value& c) { c.set("look", look("female", 7)); })));
        expect(b.entityId == Wolf1 && g.characters().at(Wolf1).appearance->baseColor == 7 &&
                   g.world().entity(Wolf1)->appearance->baseColor == 7 && g.characters().at(Wolf1).name == "Wren Reed",
               "the look is refreshed from the ticket, and the name stays");
        // A second account's wolf; and a wolf can't be claimed by another account.
        Client c;
        c.id = 3;
        g.connect(&c);
        say(g, &c, hello(f.ticket(Wolf1, Acct2, "Wren Reed")));
        expect(c.entityId.empty() && c.code() == "wolf_elsewhere", "another account's ticket for this wolf is refused");
        say(g, &c, hello(f.ticket(Wolf2, Acct2, "Moss")));
        say(g, &c, cmd({{"type", "character_arrive"}, {"commandId", "m1"}, {"tier", "gifted"}}));
        expect(c.entityId == Wolf2, "a second account's wolf arrives: " + c.message());
        // One wolf per account in the world (doc 49): Acct1's second wolf waits.
        Client d;
        d.id = 4;
        g.connect(&d);
        say(g, &d, hello(f.ticket(Wolf3, Acct1, "Fern")));
        say(g, &d, cmd({{"type", "character_arrive"}, {"commandId", "f1"}, {"tier", "gifted"}}));
        expect(d.entityId.empty() && g.characters().count(Wolf3), "a second wolf of a playing account is made, but waits outside");
        say(g, &b, cmd({{"type", "character_leave"}}));
        say(g, &d, hello(f.ticket(Wolf3, Acct1, "Fern")));
        expect(d.entityId == Wolf3, "and comes straight in once the first has left");
        // The account entry: central's id and the wolves seen here; standing keyed by it too (doc 49).
        expect(g.unlockTier(Acct1, "gifted", "grant", "dm:test").ok && g.unlockTier(Acct1, "quickened", "grant", "dm:test").ok &&
                   g.tiersOf(Acct1) && g.tiersOf(Acct1)->object("quickened").boolean("open"),
               "an account's standing is kept by central's id");
        g.disconnect(&a);
        g.disconnect(&b);
        g.disconnect(&c);
        g.disconnect(&d);
        g.save();
    }
    // The save: central ids, no salt or verifier; and the world comes back from it.
    std::ifstream in(save);
    std::stringstream text;
    text << in.rdbuf();
    Value doc;
    std::string error;
    expect(json::parse(text.str(), doc, error), "the save is JSON");
    const auto& accounts = doc["accounts"];
    expect(accounts.number("version") == 2 && accounts.string("kind") == "central" && accounts.array("entries").size() == 2,
           "accounts are saved as central accounts: " + json::dump(accounts));
    for (const auto& entry : accounts.array("entries"))
        expect(tickets::accountId(entry.string("username")) && !entry.has("salt") && !entry.has("verifier") && !entry.has("iterations"),
               "each by central's id, with no password: " + json::dump(entry));
    expect(text.str().find("\"salt\"") == std::string::npos && text.str().find("\"verifier\"") == std::string::npos,
           "nothing of a password anywhere in the save");
    {
        game::Game g(o);
        f.wire(g);
        f.clock += 100;                             // (A restart: tickets from before it are refused.)
        std::string problem;
        expect(g.start(problem), "the world starts again from its save: " + problem);
        expect(g.characters().count(Wolf1) && g.characters().count(Wolf2) && g.characters().count(Wolf3), "its wolves are kept");
        expect(g.tiersOf(Acct1) && g.tiersOf(Acct1)->object("quickened").boolean("open"), "and the account's standing");
        Client a;
        a.id = 9;
        g.connect(&a);
        f.clock -= 150;
        const auto old = f.ticket();
        f.clock += 150;
        say(g, &a, hello(old));
        expect(a.entityId.empty() && a.message().find("before this world started") != std::string::npos,
               "a ticket issued before the restart is refused: " + a.message());
        say(g, &a, hello(f.ticket()));
        expect(a.entityId == Wolf1, "a new one brings the wolf straight in");
        // Earned standing opens the tier at a first arrival here.
        Client e;
        e.id = 10;
        g.connect(&e);
        say(g, &a, cmd({{"type", "character_leave"}}));
        const std::string wolf4 = "wolf-44444444444444444444444444444444";
        say(g, &e, hello(f.ticket(wolf4, Acct1, "Sedge")));
        say(g, &e, cmd({{"type", "character_arrive"}, {"commandId", "s1"}, {"tier", "quickened"}}));
        expect(e.entityId == wolf4 && g.characters().at(wolf4).gift == "fire" && g.characters().at(wolf4).quickened,
               "an account that has earned Quickened here may arrive Quickened, in its ticket's family: " + e.message());
        say(g, &e, cmd({{"type", "character_leave"}}));
        // A wolf with no family is Normal, everywhere.
        Client n;
        n.id = 14;
        g.connect(&n);
        const std::string wolf5 = "wolf-55555555555555555555555555555555";
        say(g, &n, hello(f.ticket(wolf5, Acct2, "Plain", [](Value& c) { c.set("gift", ""); })));
        expect(n.last("lobby")->array("tierChoices").size() == 1, "a wolf with no family is offered Normal alone");
        say(g, &n, cmd({{"type", "character_arrive"}, {"commandId", "p1"}, {"tier", "gifted"}}));
        expect(n.entityId.empty() && n.code() == "invalid", "and can't arrive Gifted");
        say(g, &n, cmd({{"type", "character_arrive"}, {"commandId", "p1"}}));
        expect(n.entityId == wolf5 && g.characters().at(wolf5).gift.empty(), "it arrives Normal: " + n.message());
    }
    // A world never mixes the two kinds: a password world's accounts don't load in ticket mode, nor the other way.
    {
        game::Options plain;
        plain.tiesOptional = true;
        plain.hiddenNames = false;
        plain.savePath = save;
        game::Game g(plain);
        g.log = [](const char*, const std::string&) {};
        std::string problem;
        expect(!g.start(problem), "a password world won't start on a ticket world's accounts");
    }
    const auto passwordSave = (f.dir / "password.json").string();
    {
        game::Options plain;
        plain.tiesOptional = true;
        plain.hiddenNames = false;
        plain.savePath = passwordSave;
        game::Game g(plain);
        g.log = [](const char*, const std::string&) {};
        std::string problem;
        expect(g.start(problem), "a password world starts: " + problem);
        Client a;
        a.id = 12;
        g.connect(&a);
        say(g, &a, cmd({{"type", "auth_register"}, {"username", "wren"}, {"password", "a long enough password"}}));
        g.settle();
        expect(a.accountUsername == "wren", "and registers an account");
        g.disconnect(&a);
        g.save();
    }
    {
        game::Game g(f.options(false, passwordSave));
        f.wire(g);
        std::string problem;
        expect(!g.start(problem), "and a ticket world won't start on a password world's accounts");
    }
}

void theArrivalLimitsHold()
{
    Fixture f;
    auto o = f.options();
    o.central.accountArrivalsPerHour = 1;
    o.central.arrivalsPerHour = 2;
    o.central.playerLimit = 2;
    o.oneWolfPerAccount = false;
    game::Game g(o);
    f.wire(g);
    std::string problem;
    expect(g.start(problem), "starts: " + problem);
    std::vector<std::unique_ptr<Client>> clients;
    const auto join = [&](const std::string& wolf, const std::string& account, const std::string& name) -> Client& {
        clients.push_back(std::make_unique<Client>());
        auto& c = *clients.back();
        c.id = 100 + clients.size();
        g.connect(&c);
        say(g, &c, hello(f.ticket(wolf, account, name)));
        if (c.stage() == "arrive")
            say(g, &c, cmd({{"type", "character_arrive"}, {"commandId", "a" + std::to_string(clients.size())}, {"tier", "gifted"}}));
        return c;
    };
    expect(join(Wolf1, Acct1, "Wren").entityId == Wolf1, "a first arrival");
    auto& second = join(Wolf2, Acct1, "Moss");
    expect(second.entityId.empty() && second.code() == "arrivals" && !g.characters().count(Wolf2),
           "a second first arrival for one account within the hour is refused: " + second.message());
    expect(join(Wolf3, Acct2, "Fern").entityId == Wolf3, "another account's arrives");
    auto& full = join(Wolf2, "acct-cccccccccccccccccccccccccccccccc", "Moss");
    expect(full.entityId.empty() && full.code() == "full", "the player limit holds: " + full.message());
    g.command(clients.front().get(), cmd({{"type", "character_leave"}}));
    auto& many = join(Wolf2, "acct-cccccccccccccccccccccccccccccccc", "Moss");
    expect(many.entityId.empty() && many.code() == "arrivals", "and the world's arrivals in an hour: " + many.message());
    f.clock += 3601;
    auto& later = join(Wolf2, "acct-cccccccccccccccccccccccccccccccc", "Moss");
    expect(later.entityId == Wolf2, "an hour on, it arrives: " + later.message());
    expect(g.world().society().realBalanced(), "every welcome counted: real money balanced");
    // Too many tries on one connection.
    Client spam;
    spam.id = 999;
    g.connect(&spam);
    for (int i = 0; i < 6; ++i)
        say(g, &spam, hello("not a ticket"));
    expect(spam.code() == "rate_limited", "a connection gets a few tries at a ticket, then no more");
}

void aNewKeyIsFetchedWhenATicketNamesIt()
{
    Fixture f;
    game::Game g(f.options());
    f.wire(g);
    std::string problem;
    expect(g.start(problem), "starts: " + problem);
    Key k2("k2");
    auto c = claims(f.clock);
    const auto ticket = k2.sign(c);
    Client a;
    a.id = 1;
    g.connect(&a);
    say(g, &a, hello(ticket));
    expect(a.code() == "keys" && std::filesystem::exists(f.keys + ".wanted"),
           "a ticket under a key this world doesn't hold asks the world agent for the keys (" + a.message() + ")");
    f.writeKeys({&f.k, &k2});                       // (The agent fetches them.)
    say(g, &a, hello(ticket));
    expect(a.stage() == "arrive", "and the same ticket, unused, is admitted once they come: " + a.message());
}
Value readJson(const std::filesystem::path& path)
{
    std::ifstream in(path);
    std::stringstream text;
    text << in.rdbuf();
    Value v;
    std::string error;
    expect(json::parse(text.str(), v, error), path.string() + " is JSON: " + error);
    return v;
}

// bans.json as the world agent writes it: whole, then renamed into place.
void writeBans(const std::filesystem::path& dir, const std::string& body)
{
    const auto temp = dir / ".bans.json.test";
    std::ofstream(temp) << body;
    std::filesystem::rename(temp, dir / "bans.json");
}

void theAgentsFilesCountTheWolvesAndCarryCentralsBans()
{
    // Doc 77, 7 and 5.1: online.json for the heartbeat's honest count; bans.json from its reply.
    Fixture f;
    const auto acct3 = std::string("acct-cccccccccccccccccccccccccccccccc");
    std::filesystem::path online = f.dir / "online.json";
    {
        game::Game g(f.options());
        f.wire(g);
        std::string problem;
        expect(g.start(problem), "starts: " + problem);
        expect(!std::filesystem::exists(online), "nothing is written before the world's first pass");
        Client a, b, c;
        a.id = 1;
        b.id = 2;
        c.id = 3;
        for (auto* x : {&a, &b, &c})
            g.connect(x);
        say(g, &a, hello(f.ticket(Wolf1, Acct1, "Wren")));
        say(g, &a, cmd({{"type", "character_arrive"}, {"commandId", "w1"}, {"tier", "gifted"}}));
        say(g, &b, hello(f.ticket(Wolf2, Acct2, "Moss")));
        say(g, &b, cmd({{"type", "character_arrive"}, {"commandId", "m1"}, {"tier", "gifted"}}));
        say(g, &c, hello(f.ticket(Wolf3, acct3, "Fern")));
        expect(a.entityId == Wolf1 && b.entityId == Wolf2 && c.stage() == "arrive", "two wolves in the world, a third arriving");
        g.ticketFiles();
        auto list = readJson(online);
        expect(list.string("world") == World1 && list.number("players") == 2 && list.array("online").size() == 2 &&
                   list.array("online")[0].asString() == Wolf1 && list.array("online")[1].asString() == Wolf2 &&
                   list.number("at") == std::floor(f.clock),
               "online.json names the wolves in the world now, and when: " + json::dump(list));
        for (const auto& entry : std::filesystem::directory_iterator(f.dir))
            expect(entry.path().filename().string().rfind(".online.json", 0) != 0,
                   "no temporary file is left behind: " + entry.path().string());
        // Atomically: each write is a new file renamed over the last (another inode), never the old one rewritten.
        struct stat before{}, after{};
        expect(::stat(online.c_str(), &before) == 0, "online.json is there");
        expect(g.writeOnline(problem), "written again: " + problem);
        expect(::stat(online.c_str(), &after) == 0 && after.st_ino != before.st_ino,
               "by a temporary file renamed into place (the inode changed)");
        // Central's bans: the wolf in the world is sent out, the one arriving let go, the third untouched.
        expect(!g.bannedByCentral(Acct2), "nobody is banned without a bans file");
        writeBans(f.dir, "{\"at\": 1800000000, \"accounts\": [\"" + Acct2 + "\", \"" + acct3 + "\"]}");
        expect(g.reloadBans(problem) && g.bannedByCentral(Acct2) && g.bannedByCentral(acct3) && !g.bannedByCentral(Acct1),
               "bans.json is read: " + problem);
        expect(b.entityId.empty() && b.accountUsername.empty() && b.code() == "banned" && b.closed &&
                   b.message().find("banned from central's worlds") != std::string::npos,
               "a banned wolf in the world is told so and its connection closed: " + b.message());
        expect(!g.world().entity(Wolf2) && g.characters().count(Wolf2), "taken out of the world as on leaving, and kept");
        expect(c.accountUsername.empty() && c.code() == "banned" && c.closed, "a banned account's ticket waiting to arrive is let go");
        expect(a.entityId == Wolf1 && !a.closed && g.world().entity(Wolf1), "a wolf not banned plays on");
        expect(g.writeOnline(problem) && readJson(online).array("online").size() == 1 && readJson(online).number("players") == 1,
               "and online.json no longer counts the one sent out");
        Client d;
        d.id = 4;
        g.connect(&d);
        say(g, &d, hello(f.ticket(Wolf2, Acct2, "Moss")));
        expect(d.entityId.empty() && d.accountUsername.empty() && d.code() == "banned" &&
                   d.message() == "This account is banned from central's worlds.",
               "a banned account's hello is refused with code 'banned': " + d.message());
        // A bad file keeps the last list; a changed one is picked up.
        writeBans(f.dir, "{\"accounts\": [7]}");
        expect(!g.reloadBans(problem) && g.bannedByCentral(Acct2), "a bans file that names no accounts keeps the last list");
        writeBans(f.dir, "{\"at\": 1, \"accounts\": [\"" + acct3 + "\"]}");
        expect(g.reloadBans(problem) && !g.bannedByCentral(Acct2) && g.bannedByCentral(acct3), "a changed bans file is read again");
        say(g, &d, hello(f.ticket(Wolf2, Acct2, "Moss")));
        expect(d.entityId == Wolf2, "and an account no longer banned comes back in: " + d.message());
        g.disconnect(&a);
        g.disconnect(&b);
        g.disconnect(&c);
        g.disconnect(&d);
        expect(g.writeOnline(problem) && readJson(online).array("online").empty(), "nobody left, nobody counted");
    }
    {
        game::Game g(f.options());
        f.wire(g);
        std::string problem;
        expect(g.start(problem), "starts again: " + problem);
        expect(g.bannedByCentral(acct3) && !g.bannedByCentral(Acct2), "a world reads bans.json as it starts");
    }
    // Password mode: no files at all, and nothing about central.
    const auto plainDir = f.dir / "password";
    std::filesystem::create_directories(plainDir);
    game::Options plain;
    plain.tiesOptional = true;
    plain.hiddenNames = false;
    plain.savePath = (plainDir / "save.json").string();
    game::Game g(plain);
    g.log = [](const char*, const std::string&) {};
    std::string problem;
    expect(g.start(problem), "a password world starts: " + problem);
    Client a;
    a.id = 5;
    g.connect(&a);
    say(g, &a, cmd({{"type", "auth_register"}, {"username", "wren"}, {"password", "a long enough password"}}));
    g.settle();
    g.ticketFiles();
    expect(a.accountUsername == "wren" && !g.writeOnline(problem) && !g.reloadBans(problem), "a password world registers as ever");
    for (const auto& entry : std::filesystem::directory_iterator(plainDir))
        expect(entry.path().filename() == "save.json" || entry.path().filename().string().rfind("save.json", 0) == 0,
               "and writes nothing of central's: " + entry.path().string());
    expect(!std::filesystem::exists("online.json") && !std::filesystem::exists("bans.json"), "not even where it runs");
    g.disconnect(&a);
}
void aScratchServerWritesNoOnlineFile()
{
    // A scratch server (Options::scratch) beside the real ticket world, with the same --central-keys, must not count
    // its own players into the real world's online.json (nor write the empty one as it stops); it still reads bans.json.
    Fixture f;
    std::filesystem::path online = f.dir / "online.json";
    const std::string real = "{\"world\": \"" + World1 + "\", \"at\": 1, \"players\": 1, \"online\": [\"" + Wolf3 + "\"]}\n";
    std::ofstream(online) << real;
    writeBans(f.dir, "{\"at\": 1, \"accounts\": [\"" + Acct2 + "\"]}");
    auto o = f.options();
    o.scratch = true;
    game::Game g(o);
    f.wire(g);
    std::string problem;
    expect(g.start(problem), "a scratch server starts in ticket mode: " + problem);
    Client a;
    a.id = 1;
    g.connect(&a);
    say(g, &a, hello(f.ticket(Wolf1, Acct1, "Wren")));
    say(g, &a, cmd({{"type", "character_arrive"}, {"commandId", "w1"}, {"tier", "gifted"}}));
    expect(a.entityId == Wolf1, "a wolf arrives on the scratch server");
    g.ticketFiles();
    expect(!g.writeOnline(problem) && problem.find("scratch") != std::string::npos, "writeOnline refuses: " + problem);
    std::ifstream in(online, std::ios::binary);
    std::stringstream text;
    text << in.rdbuf();
    expect(text.str() == real, "the real world's online.json is left as it was: " + text.str());
    for (const auto& entry : std::filesystem::directory_iterator(f.dir))
        expect(entry.path().filename().string().rfind(".online.json", 0) != 0, "and no temporary file: " + entry.path().string());
    expect(g.bannedByCentral(Acct2), "it still reads central's bans");
    g.disconnect(&a);
}

// host.json as the world agent writes it.
void writeHost(const std::filesystem::path& dir, const std::string& body)
{
    const auto temp = dir / ".host.json.test";
    std::ofstream(temp) << body;
    std::filesystem::rename(temp, dir / "host.json");
}

// A console command, and its answer.
const Value* console(game::Game& g, Client* c, const std::string& text)
{
    say(g, c, cmd({{"type", "dev"}, {"command", text}}));
    return c->last("devResult");
}

// What the client's page is told of itself (snapshot.self) after a few ticks, each snapshot acknowledged as a client does.
Value selfOf(game::Game& g, std::initializer_list<Client*> clients, Client& c)
{
    for (int i = 0; i < 4; ++i)
    {
        g.tick(.05);
        for (auto* x : clients)
            if (!x->snapshots.empty())
                g.acknowledge(x, x->snapshots.back().number("revision"), false);
    }
    return c.snapshots.empty() ? Value{} : c.snapshots.back()["self"];
}

bool saidSomething(const Client& c, const std::string& words)
{
    for (const auto& e : c.events)
        if (json::dump(e).find(words) != std::string::npos)
            return true;
    return false;
}

void theHostDecidesKicksBansAndSilences()
{
    // Doc 77, 3.2: central names a community world's host (host.json); every wolf of that account has the host's
    // commands, nobody else does, the host can't turn them on itself, and its bans and silences are the world's own.
    Fixture f;
    const auto save = (f.dir / "save.json").string();
    const auto acct3 = std::string("acct-cccccccccccccccccccccccccccccccc");
    const auto wolf4 = std::string("wolf-44444444444444444444444444444444");
    writeHost(f.dir, "{\"at\": 1, \"host\": \"" + Acct1 + "\"}");
    auto o = f.options(false, save);
    o.oneWolfPerAccount = false;
    o.central.accountArrivalsPerHour = 5;
    {
        game::Game g(o);
        f.wire(g);
        std::string problem;
        expect(g.start(problem), "starts: " + problem);
        expect(g.hostAccount() == Acct1, "the world reads its host as it starts");
        Client a, b, c;
        a.id = 1;
        b.id = 2;
        c.id = 3;
        for (auto* x : {&a, &b, &c})
            g.connect(x);
        say(g, &a, hello(f.ticket(Wolf1, Acct1, "Wren Reed")));
        say(g, &a, cmd({{"type", "character_arrive"}, {"commandId", "w1"}, {"tier", "gifted"}}));
        say(g, &b, hello(f.ticket(Wolf2, Acct2, "Moss Hollow")));
        say(g, &b, cmd({{"type", "character_arrive"}, {"commandId", "m1"}, {"tier", "gifted"}}));
        say(g, &c, hello(f.ticket(Wolf3, acct3, "Fern")));
        say(g, &c, cmd({{"type", "character_arrive"}, {"commandId", "f1"}, {"tier", "gifted"}}));
        expect(a.entityId == Wolf1 && b.entityId == Wolf2 && c.entityId == Wolf3, "three wolves in the world");
        expect(g.isHost(&a) && !g.isHost(&b) && !g.isHost(&c), "the host's wolf is the host, the others aren't");
        // The page is told (snapshot.self.host): the only thing that gives a host's client the console.
        expect(selfOf(g, {&a, &b, &c}, a).boolean("host") && !selfOf(g, {&a, &b, &c}, b).has("host") &&
                   !selfOf(g, {&a, &b, &c}, c).has("host"),
               "the host's page is told it is the host's, and no other wolf's page is");

        // Nobody else has them, and the host has no cheats.
        for (const auto* text : {"/pending", "/ban Wren Reed", "/kick Wren Reed", "/silence Wren Reed 6", "/help"})
            expect(!console(g, &b, text)->boolean("ok"), std::string("a wolf who isn't the host can't use ") + text);
        say(g, &b, cmd({{"type", "devCommands"}}));
        expect(b.last("devCommands")->array("commands").empty(), "and is offered none");
        say(g, &a, cmd({{"type", "devCommands"}}));
        const auto offered = json::dump(*a.last("devCommands"));
        expect(offered.find("/ban") != std::string::npos && offered.find("/pending") != std::string::npos &&
                   offered.find("/speed") == std::string::npos && offered.find("/give") == std::string::npos,
               "the host is offered the host's commands, none of the Dev Console's: " + offered);
        expect(!console(g, &a, "/speed 100")->boolean("ok") && !console(g, &a, "/give hat 1")->boolean("ok"),
               "and the Dev Console's cheats are refused to the host");
        expect(console(g, &a, "/help")->boolean("ok"), "the host's /help");

        // What waits: a treaty, a House request, a storyteller application.
        expect(console(g, &a, "/pending")->string("text") == "Nothing waits on you.", "nothing waits yet");
        faction::Treaty t;
        t.id = g.factions().nextTreatyId();
        t.faction = "crown";
        t.chapter = "chapter-9";
        g.factions().treaties().push_back(t);
        g.factions().houseRequests().push_back({"chapter-8", "crown", "pending", 0});
        g.ledger().points[Wolf2] = 1000000;
        say(g, &b, cmd({{"type", "storyteller"}, {"verb", "apply"}, {"note", "Tales of the drowned bell."}}));
        expect(saidSomething(b, "with the host"), "a community world's applicant is told the host decides");
        const auto pending = console(g, &a, "/pending")->string("text");
        expect(pending.find(t.id) != std::string::npos && pending.find("chapter-8") != std::string::npos &&
                   pending.find("Moss Hollow") != std::string::npos && pending.find("drowned bell") != std::string::npos,
               "/pending lists the treaty, the House request and the application: " + pending);
        expect(!console(g, &a, "/treaty " + t.id + " maybe")->boolean("ok"), "a treaty is yes or no");
        expect(console(g, &a, "/treaty " + t.id + " yes")->boolean("ok") && g.factions().treaties().back().state == "active",
               "the host agrees a treaty");
        expect(!console(g, &a, "/treaty " + t.id + " yes")->boolean("ok"), "once");
        expect(console(g, &a, "/house chapter-8 crown no")->boolean("ok") && g.factions().houseRequests().back().state == "refused",
               "the host refuses a House request");
        expect(console(g, &a, "/storyteller Moss Hollow yes")->boolean("ok") && saidSomething(b, "The host has made you a storyteller"),
               "the host approves a storyteller, by a name with a space in it");
        expect(console(g, &a, "/storyteller " + Wolf2 + " revoke too many bells")->boolean("ok") &&
                   saidSomething(b, "no longer a storyteller: too many bells"),
               "and revokes it, by the wolf's id, with a reason");
        {
            // Reasons and an application in three- and four-byte characters: 200 euro signs are 600 bytes, 101 wolves
            // 404, and the 166 dashes of a note 498. A cut by bytes (400, or the log's 200) would fall inside one, and
            // Postgres would refuse every save after; they're kept whole, or cut between characters.
            const auto repeat = [](const char* one, int n) {
                std::string out;
                for (int i = 0; i < n; ++i)
                    out += one;
                return out;
            };
            const auto euros = repeat("\xe2\x82\xac", 200), wolves = repeat("\xf0\x9f\x90\xba", 101), dashes = repeat("\xe2\x80\x94", 166);
            const auto saved = [&] {
                g.save();
                std::ifstream in(save, std::ios::binary);
                std::stringstream text;
                text << in.rdbuf();
                return text.str();
            };
            say(g, &b, cmd({{"type", "storyteller"}, {"verb", "apply"}, {"note", dashes}}));
            expect(saidSomething(b, "with the host"), "an application in dashes");
            expect(console(g, &a, "/storyteller Moss Hollow no " + euros)->boolean("ok") && saidSomething(b, "refused: " + euros),
                   "refused, the whole reason told");
            auto text = saved();
            expect(json::validUtf8(text) && text.find(euros) != std::string::npos && text.find(repeat("\xe2\x80\x94", 66)) != std::string::npos,
                   "the save is UTF-8 throughout, with the whole reason and the log's note cut between dashes");
            say(g, &b, cmd({{"type", "storyteller"}, {"verb", "apply"}, {"note", "Again."}}));
            expect(console(g, &a, "/storyteller Moss Hollow yes")->boolean("ok") && console(g, &a, "/storyteller Moss Hollow revoke " + wolves)->boolean("ok") &&
                       saidSomething(b, "no longer a storyteller: " + wolves),
                   "revoked, with a reason of 101 wolves");
            text = saved();
            expect(json::validUtf8(text) && text.find(wolves) != std::string::npos, "and the save is still UTF-8, the reason whole");
        }
        expect(console(g, &a, "/pending")->string("text") == "Nothing waits on you.", "nothing waits now");

        // The host can't kick, ban or silence its own account.
        for (const auto* text : {"/ban Wren Reed", "/kick Wren Reed", "/silence Wren Reed 6", "/unban Wren Reed"})
        {
            const auto* r = console(g, &a, text);
            expect(!r->boolean("ok") && r->string("text").find("your own account") != std::string::npos,
                   std::string("the host can't use ") + text + " on itself: " + r->string("text"));
        }
        expect(a.entityId == Wolf1 && !g.bannedHere(Acct1), "and is still here");
        expect(!console(g, &a, "/ban Nobody")->boolean("ok"), "an unknown wolf is no target");

        // A silence, world chat refused; a kick, which may come back; a ban, which refuses any of the account's wolves.
        expect(!console(g, &a, "/silence Fern 5")->boolean("ok"), "a silence is 1, 6, 24 or 72 hours");
        expect(console(g, &a, "/silence Fern 24")->boolean("ok") && saidSomething(c, "The host has silenced your account"),
               "the host silences an account");
        say(g, &c, cmd({{"type", "chat"}, {"text", "Hello all."}}));
        expect(c.last("error") && c.last("error")->string("text").find("silenced") != std::string::npos,
               "a silenced wolf's world chat is refused");
        for (int i = 0; i < 12; ++i)
            g.tick(0.05);                           // (Posts are half a second apart.)
        c.events.clear();
        say(g, &c, cmd({{"type", "chat"}, {"channel", "private"}, {"to", "Wren Reed"}, {"text", "Psst."}}));
        expect(c.last("error") && c.last("error")->string("text").find("silenced") != std::string::npos && !saidSomething(a, "Psst."),
               "and its private messages (the user's decision, 2026-11-08; letters likewise: letters_tests)");
        expect(console(g, &a, "/kick Fern")->boolean("ok") && c.code() == "kicked" && c.closed && c.entityId.empty() &&
                   !g.bannedHere(acct3),
               "the host kicks a wolf: sent out, not banned: " + c.message());
        expect(console(g, &a, "/ban Moss Hollow spamming the square")->boolean("ok") && g.bannedHere(Acct2) && b.code() == "world_banned" &&
                   b.closed && b.entityId.empty() && b.message().find("spamming the square") != std::string::npos,
               "the host bans an account: its wolf is sent out, told why: " + b.message());
        Client d;
        d.id = 4;
        g.connect(&d);
        say(g, &d, hello(f.ticket(wolf4, Acct2, "Birch")));
        expect(d.entityId.empty() && d.accountUsername.empty() && d.code() == "world_banned" && !g.characters().count(wolf4),
               "a fresh wolf of the banned account is refused at hello: " + d.message());
        g.disconnect(&b);
        g.disconnect(&c);
        g.disconnect(&d);
        expect(console(g, &a, "/ban Moss Hollow")->string("text").find("still banned") != std::string::npos,
               "a ban again says it holds already");
        {
            // A reason of 150 accented letters: 166 characters, as the console allows, but more than 300 bytes. The line
            // is cut on characters, never inside one: a broken byte in a saved reason would have Postgres refuse every
            // save after. Nor is a command that isn't UTF-8 taken at all.
            std::string reason;
            for (int i = 0; i < 150; ++i)
                reason += "\xc3\xa9";
            for (const auto* lead : {"", "x"})         // (Either way 300 bytes would fall, one of them inside a letter.)
            {
                const auto* r = console(g, &a, "/ban Moss Hollow " + std::string(lead) + reason);
                expect(r->boolean("ok") && json::validUtf8(r->string("command")) && json::validUtf8(r->string("text")),
                       "a long accented reason: banned, the reply whole UTF-8: " + r->string("command"));
            }
            const auto before = a.events.size();
            say(g, &a, "{\"type\":\"dev\",\"command\":\"/unban Moss Hollow \xc3\"}");
            expect(a.events.size() == before && g.bannedHere(Acct2), "a command that isn't UTF-8 is no command");
            g.save();
            std::ifstream in(save, std::ios::binary);
            std::stringstream text;
            text << in.rdbuf();
            expect(json::validUtf8(text.str()), "the save is UTF-8 throughout");
            std::string kept;
            for (int i = 0; i < 140; ++i)
                kept += "\xc3\xa9";
            expect(text.str().find(kept) != std::string::npos, "with the reason in it");
        }
        g.save();
        g.disconnect(&a);
    }
    {
        // A save and a load: the ban and the silence are the world's own, and stay.
        game::Game g(o);
        f.wire(g);
        std::string problem;
        expect(g.start(problem), "starts again from its save: " + problem);
        expect(g.bannedHere(Acct2) && !g.bannedHere(acct3), "the ban survives the restart");
        Client b, c, a;
        b.id = 5;
        c.id = 6;
        a.id = 7;
        for (auto* x : {&a, &b, &c})
            g.connect(x);
        say(g, &b, hello(f.ticket(Wolf2, Acct2, "Moss Hollow")));
        expect(b.entityId.empty() && b.code() == "world_banned", "the banned wolf is refused after the restart");
        say(g, &c, hello(f.ticket(Wolf3, acct3, "Fern")));
        expect(c.entityId == Wolf3, "the kicked wolf comes back: " + c.message());
        say(g, &c, cmd({{"type", "chat"}, {"text", "Hello again."}}));
        expect(c.last("error") && c.last("error")->string("text").find("silenced") != std::string::npos,
               "the silence survives the restart");
        say(g, &a, hello(f.ticket(Wolf1, Acct1, "Wren Reed")));
        expect(console(g, &a, "/unsilence Fern")->boolean("ok") && console(g, &a, "/unban Moss Hollow")->boolean("ok") &&
                   !g.bannedHere(Acct2),
               "the host lifts them");
        for (int i = 0; i < 12; ++i)
            g.tick(0.05);                           // (Posts are half a second apart.)
        say(g, &c, cmd({{"type", "chat"}, {"text", "Free to speak."}}));
        expect(c.last("chatAccepted"), "the unsilenced wolf speaks");
        Client b2;
        b2.id = 8;
        g.connect(&b2);
        say(g, &b2, hello(f.ticket(Wolf2, Acct2, "Moss Hollow")));
        expect(b2.entityId == Wolf2, "and the unbanned one comes back: " + b2.message());

        // The host changes: the old host's wolf loses the commands as the world next looks (ticketFiles, about every
        // 5 s, as the server calls it), and its page is told.
        double files = 1000;
        g.ticketFilesClock = [&files] { return files; };
        g.ticketFiles();                            // (A first look.)
        expect(selfOf(g, {&a, &b, &c, &b2}, a).boolean("host"), "the host's page, before");
        writeHost(f.dir, "{\"at\": 2, \"host\": null}");
        files += 1;
        g.ticketFiles();
        expect(g.hostAccount() == Acct1, "not looked at again within 5 s");
        files += 5;
        g.ticketFiles();
        expect(g.hostAccount().empty() && !g.isHost(&a), "a host cleared by the agent is no host, once the world looks");
        expect(!console(g, &a, "/pending")->boolean("ok"), "and has no commands");
        expect(!selfOf(g, {&a, &b, &c, &b2}, a).has("host"), "and its page is told it no longer is");
        writeHost(f.dir, "{\"at\": 3}");
        expect(!g.reloadHost(problem), "a host file that names nobody is refused");
        writeHost(f.dir, "{\"at\": 4, \"host\": \"" + Acct2 + "\"}");
        files += 5;
        g.ticketFiles();
        expect(g.hostAccount() == Acct2 && g.isHost(&b2) && !g.isHost(&a), "the new host's wolf is the host, by ticketFiles too");
        expect(selfOf(g, {&a, &b, &c, &b2}, b2).boolean("host"), "and its page is told");
        for (auto* x : {&a, &b, &c, &b2})
            g.disconnect(x);
    }
    {
        // An official world has no host, whatever host.json says; and keeps its Dungeon Masters' words.
        writeHost(f.dir, "{\"at\": 5, \"host\": \"" + Acct1 + "\"}");
        game::Game g(f.options(true));
        f.wire(g);
        std::vector<std::string> warnings;
        g.log = [&warnings](const char* level, const std::string& text) {
            if (std::string(level) == "warning")
                warnings.push_back(text);
        };
        std::string problem;
        expect(g.start(problem), "an official world starts: " + problem);
        for (const auto& w : warnings)
            expect(w.find("RATW_HOST") == std::string::npos, "an official world's start says nothing of a host file: " + w);
        expect(g.hostAccount().empty() && !g.reloadHost(problem), "an official world reads no host: " + problem);
        Client a;
        a.id = 9;
        g.connect(&a);
        say(g, &a, hello(f.ticket(Wolf1, Acct1, "Wren Reed", [](Value& claim) { claim.set("kind", "official"); })));
        say(g, &a, cmd({{"type", "character_arrive"}, {"commandId", "w1"}, {"tier", "gifted"}}));
        expect(a.entityId == Wolf1 && !g.isHost(&a), "the account central names is no host on an official world: " + a.message() + " " + a.code());
        expect(!selfOf(g, {&a}, a).has("host"), "nor is its page told it is");
        expect(!console(g, &a, "/pending")->boolean("ok") && !console(g, &a, "/ban Wren Reed")->boolean("ok"), "and has no host commands");
        g.ledger().points[Wolf1] = 1000000;
        say(g, &a, cmd({{"type", "storyteller"}, {"verb", "apply"}, {"note", "Tales."}}));
        expect(saidSomething(a, "with the Dungeon Masters"), "an official world's applicant waits on the Dungeon Masters");
        g.disconnect(&a);
    }
}

void portraitUploadsAreOffOnCommunityWorlds()
{
    Fixture f;
    for (const bool official : {false, true})
    {
        game::Game g(f.options(official));
        f.wire(g);
        std::string problem;
        expect(g.start(problem), "starts: " + problem);
        Client a;
        a.id = 1;
        g.connect(&a);
        say(g, &a, hello(f.ticket(Wolf1, Acct1, "Wren Reed", [&](Value& claim) { claim.set("kind", official ? "official" : "community"); })));
        expect(a.stage() == "arrive", "a ticket for the world: " + a.message());
        say(g, &a, cmd({{"type", "character_arrive"}, {"commandId", "w1"}, {"tier", "gifted"}}));
        say(g, &a, cmd({{"type", "artwork_upload"}, {"characterId", Wolf1}, {"uploadId", "u1"}, {"part", 0}, {"parts", 1}, {"data", "AAAA"}}));
        const auto* refused = a.last("artworkError");
        expect(official ? (!refused || refused->string("text").find("community") == std::string::npos)
                        : (refused && refused->string("text").find("off on community worlds") != std::string::npos),
               std::string(official ? "an official world takes uploads as ever" : "a community world refuses portrait uploads") +
                   (refused ? ": " + refused->string("text") : std::string()));
        g.disconnect(&a);
    }
}
} // namespace

int main()
{
    try
    {
        strictParsingAndTheLibrary();
        theCheckerKeepsEveryRule();
        ticketModeStartsOnlyAsItShould();
        wolvesJoinArriveAndComeStraightBack();
        theArrivalLimitsHold();
        aNewKeyIsFetchedWhenATicketNamesIt();
        theAgentsFilesCountTheWolvesAndCarryCentralsBans();
        aScratchServerWritesNoOnlineFile();
        theHostDecidesKicksBansAndSilences();
        portraitUploadsAreOffOnCommunityWorlds();
    }
    catch (const std::exception& error)
    {
        std::cerr << "ticket test failed after " << checks << " checks: " << error.what() << '\n';
        return 1;
    }
    std::cout << "ticket tests passed (" << checks << " checks)\n";
    return 0;
}
