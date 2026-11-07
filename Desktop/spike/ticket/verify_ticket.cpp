// Phase 0 spike (doc 77, section 5): a world's side of a ticket, in C++, with OpenSSL's libcrypto loaded at run time
// the way Core/RatwSystemLibs.cpp loads it (dlopen, bind by name), plus the six-or-so EVP functions doc 77 4.3 says
// the loader would gain. Standalone: it shares only Core/RatwJsonDoc with the server, and changes nothing in Core.
//
//   verify_ticket KEYS.json WORLD_ID WORLD_START_UNIX CASES.txt [BULK.txt]
//
// CASES lines are `label accept|refuse ticket` (from sign_ticket.py). Prints each verdict and why; exits 1 if any
// verdict differs from the expected one. With BULK, times verifying every ticket in it.
#include "RatwJsonDoc.h"

#include <dlfcn.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace
{
// ---- libcrypto, bound at run time ----
struct Ed
{
    bool ok = false;
    void* (*newRawPublic)(int, void*, const unsigned char*, std::size_t) = nullptr;   // EVP_PKEY_new_raw_public_key
    void (*freeKey)(void*) = nullptr;                                                 // EVP_PKEY_free
    void* (*newCtx)() = nullptr;                                                      // EVP_MD_CTX_new
    void (*freeCtx)(void*) = nullptr;                                                 // EVP_MD_CTX_free
    int (*verifyInit)(void*, void**, const void*, void*, void*) = nullptr;            // EVP_DigestVerifyInit
    int (*verify)(void*, const unsigned char*, std::size_t, const unsigned char*, std::size_t) = nullptr;  // EVP_DigestVerify
};
constexpr int EvpPkeyEd25519 = 1087;   // NID_ED25519 (stable across OpenSSL 1.1.1 and 3.x)

template <class F>
bool bind(void* lib, const char* name, F& out)
{
    out = reinterpret_cast<F>(dlsym(lib, name));
    return out != nullptr;
}

const Ed& ed()
{
    static Ed e;
    static bool tried = false;
    if (tried)
        return e;
    tried = true;
    void* lib = nullptr;
    for (const char* name : {"libcrypto.so.3", "libcrypto.so", "libcrypto.3.dylib", "libcrypto.dylib"})
        if ((lib = dlopen(name, RTLD_NOW | RTLD_LOCAL)))
            break;
    if (!lib)
        return e;
    e.ok = bind(lib, "EVP_PKEY_new_raw_public_key", e.newRawPublic) && bind(lib, "EVP_PKEY_free", e.freeKey) &&
           bind(lib, "EVP_MD_CTX_new", e.newCtx) && bind(lib, "EVP_MD_CTX_free", e.freeCtx) &&
           bind(lib, "EVP_DigestVerifyInit", e.verifyInit) && bind(lib, "EVP_DigestVerify", e.verify);
    return e;
}

bool ed25519Verify(const std::uint8_t pub[32], const std::string& message, const std::string& signature)
{
    const auto& c = ed();
    if (!c.ok || signature.size() != 64)
        return false;
    void* key = c.newRawPublic(EvpPkeyEd25519, nullptr, pub, 32);
    if (!key)
        return false;
    void* ctx = c.newCtx();
    bool good = false;
    if (ctx && c.verifyInit(ctx, nullptr, nullptr, nullptr, key) == 1)
        good = c.verify(ctx, reinterpret_cast<const unsigned char*>(signature.data()), signature.size(),
                        reinterpret_cast<const unsigned char*>(message.data()), message.size()) == 1;
    if (ctx)
        c.freeCtx(ctx);
    c.freeKey(key);
    return good;
}

// ---- strict base64url: the URL alphabet, no padding, no stray bits ----
bool b64url(const std::string& in, std::string& out)
{
    static int8_t table[256];
    static bool made = false;
    if (!made)
    {
        std::memset(table, -1, sizeof table);
        const char* abc = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
        for (int i = 0; i < 64; ++i)
            table[static_cast<unsigned char>(abc[i])] = int8_t(i);
        made = true;
    }
    if (in.size() % 4 == 1)
        return false;
    out.clear();
    std::uint32_t acc = 0;
    int bits = 0;
    for (unsigned char ch : in)
    {
        if (table[ch] < 0)
            return false;
        acc = (acc << 6) | std::uint32_t(table[ch]);
        bits += 6;
        if (bits >= 8)
        {
            bits -= 8;
            out.push_back(char((acc >> bits) & 0xFF));
        }
    }
    return (acc & ((1u << bits) - 1)) == 0;   // Leftover bits must be zero (one encoding per value).
}

// A JSON object. Finding (Phase 0): RatwJsonDoc keeps the LAST copy of a repeated key (Value::set), as Python's json
// does, so central and a world read a doubled `aud` or `alg` the same way and the cases below are refused. Phase 2
// should still refuse repeated keys outright (a strict mode in the parser); the fields() scan here can't see them.
bool parseObject(const std::string& text, ratw::json::Value& out)
{
    std::string error;
    if (!ratw::json::parse(text, out, error) || !out.isObject())
        return false;
    std::set<std::string> seen;
    for (const auto& field : out.fields())
        if (!seen.insert(field.first).second)
            return false;
    return true;
}

bool isInteger(const ratw::json::Value* v)
{
    return v && v->isNumber() && std::floor(v->asNumber()) == v->asNumber() && std::fabs(v->asNumber()) < 1e15;
}

bool isStr(const ratw::json::Value* v) { return v && v->isString() && !v->asString().empty(); }

bool wolfId(const std::string& id)
{
    if (id.size() != 37 || id.compare(0, 5, "wolf-") != 0)
        return false;
    for (std::size_t i = 5; i < id.size(); ++i)
        if (!std::isxdigit(static_cast<unsigned char>(id[i])) || std::isupper(static_cast<unsigned char>(id[i])))
            return false;
    return true;
}

struct World
{
    std::string id;
    std::int64_t started = 0;
    std::int64_t leeway = 120;   // Doc 77: two minutes for a home machine's clock.
    std::map<std::string, std::vector<std::uint8_t>> keys;
    std::map<std::string, std::int64_t> seen;   // jti -> forget after
};

// The check, in doc 77 section 5's order. Returns "" when admitted, else why not.
std::string check(World& world, const std::string& ticket, std::int64_t now, ratw::json::Value* claimsOut = nullptr)
{
    if (ticket.size() > 4096)
        return "too long";
    const auto dot1 = ticket.find('.');
    const auto dot2 = dot1 == std::string::npos ? dot1 : ticket.find('.', dot1 + 1);
    if (dot2 == std::string::npos || ticket.find('.', dot2 + 1) != std::string::npos)
        return "not three parts";
    std::string headerJson, payloadJson, signature;
    if (!b64url(ticket.substr(0, dot1), headerJson))
        return "header not base64url";
    ratw::json::Value header;
    if (!parseObject(headerJson, header))
        return "header not a plain object";
    // The algorithm first: exactly Ed25519, nothing else read before this.
    const auto* alg = header.find("alg");
    if (!alg || !alg->isString() || alg->asString() != "Ed25519")
        return "alg is not Ed25519";
    const auto* typ = header.find("typ");
    if (typ && (!typ->isString() || typ->asString() != "JWT"))
        return "typ is not JWT";
    for (const auto& field : header.fields())
        if (field.first != "alg" && field.first != "typ" && field.first != "kid")
            return "unexpected header field " + field.first;   // No crit, jku, x5u...: nothing that steers the check.
    const auto* kid = header.find("kid");
    if (!isStr(kid))
        return "no kid";
    const auto key = world.keys.find(kid->asString());
    if (key == world.keys.end())
        return "unknown kid (the world would refetch /keys, at most once a minute)";
    if (!b64url(ticket.substr(dot2 + 1), signature) || signature.size() != 64)
        return "signature not 64 bytes of base64url";
    if (!ed25519Verify(key->second.data(), ticket.substr(0, dot2), signature))
        return "bad signature";
    // Signed by central: now the claims.
    if (!b64url(ticket.substr(dot1 + 1, dot2 - dot1 - 1), payloadJson))
        return "payload not base64url";
    ratw::json::Value claims;
    if (!parseObject(payloadJson, claims))
        return "payload not a plain object";
    const auto* aud = claims.find("aud");
    if (!aud || !aud->isString() || aud->asString() != world.id)
        return "aud is not this world";
    const auto* exp = claims.find("exp");
    const auto* iat = claims.find("iat");
    if (!isInteger(exp) || !isInteger(iat))
        return "exp/iat not whole numbers";
    if (now > std::int64_t(exp->asNumber()) + world.leeway)
        return "expired";
    if (std::int64_t(iat->asNumber()) > now + world.leeway)
        return "issued in the future";
    if (std::int64_t(iat->asNumber()) + world.leeway < world.started)
        return "issued before this world started";
    for (const char* field : {"sub", "wolf", "name", "sex", "gift", "kind", "jti"})
        if (!isStr(claims.find(field)))
            return std::string("missing ") + field;
    if (!claims.find("look") || !claims.find("look")->isObject() || !claims.find("build") ||
        !claims.find("build")->isObject())
        return "look/build not objects";
    if (!wolfId(claims.string("wolf")))
        return "wolf id malformed";
    const std::string kind = claims.string("kind");
    if (kind != "official" && kind != "community")
        return "kind unknown";
    const std::string sex = claims.string("sex");
    if (sex != "male" && sex != "female")
        return "sex unknown";
    // Replay: remember each jti until its exp plus the leeway (doc 77; restarts are covered by the iat rule).
    for (auto it = world.seen.begin(); it != world.seen.end();)
        it = it->second < now ? world.seen.erase(it) : std::next(it);
    const std::string jti = claims.string("jti");
    if (world.seen.count(jti))
        return "jti already used";
    world.seen[jti] = std::int64_t(exp->asNumber()) + world.leeway;
    // (The world's own identity check of name/look/build/gift, ratw_identity, comes here in Phase 2.)
    if (claimsOut)
        *claimsOut = std::move(claims);
    return "";
}

std::string slurp(const char* path)
{
    std::ifstream f(path, std::ios::binary);
    std::stringstream s;
    s << f.rdbuf();
    return s.str();
}
} // namespace

int main(int argc, char** argv)
{
    if (argc < 5)
    {
        std::fprintf(stderr, "usage: verify_ticket KEYS.json WORLD_ID WORLD_START_UNIX CASES.txt [BULK.txt]\n");
        return 2;
    }
    if (!ed().ok)
    {
        std::fprintf(stderr, "libcrypto (with EVP_PKEY_new_raw_public_key / EVP_DigestVerify) could not be loaded\n");
        return 2;
    }
    World world;
    world.id = argv[2];
    world.started = std::atoll(argv[3]);
    ratw::json::Value keys;
    std::string error;
    if (!ratw::json::parse(slurp(argv[1]), keys, error))
    {
        std::fprintf(stderr, "keys: %s\n", error.c_str());
        return 2;
    }
    for (const auto& k : keys.array("keys"))
    {
        std::string raw;
        if (k.string("kty") == "OKP" && k.string("crv") == "Ed25519" && b64url(k.string("x"), raw) && raw.size() == 32)
            world.keys[k.string("kid")] = std::vector<std::uint8_t>(raw.begin(), raw.end());
    }
    std::printf("keys loaded: %zu\n", world.keys.size());

    const std::int64_t now = std::time(nullptr);
    std::ifstream cases(argv[4]);
    std::string line;
    int wrong = 0, total = 0;
    while (std::getline(cases, line))
    {
        std::istringstream in(line);
        std::string label, expect, ticket;
        in >> label >> expect >> ticket;
        if (label.empty())
            continue;
        const std::string why = check(world, ticket, now);
        const bool admitted = why.empty();
        const bool right = admitted == (expect == "accept");
        wrong += right ? 0 : 1;
        ++total;
        std::printf("%-4s %-28s %-7s %s\n", right ? "ok" : "FAIL", label.c_str(), admitted ? "admit" : "refuse",
                    why.c_str());
    }
    std::printf("%d of %d cases as expected\n", total - wrong, total);

    if (argc > 5)
    {
        std::vector<std::string> bulk;
        std::ifstream b(argv[5]);
        while (std::getline(b, line))
            if (!line.empty())
                bulk.push_back(line);
        const auto t0 = std::chrono::steady_clock::now();
        int admitted = 0;
        for (const auto& t : bulk)
            admitted += check(world, t, now).empty() ? 1 : 0;
        const double us =
            std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count() / double(bulk.size());
        std::printf("bulk: %d of %zu admitted, %.1f us per ticket (parse + Ed25519 verify + claims)\n", admitted,
                    bulk.size(), us);
    }
    return wrong == 0 ? 0 : 1;
}
