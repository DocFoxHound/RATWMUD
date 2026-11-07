#include "RatwTickets.h"

#include "RatwIdentity.h"
#include "RatwSystemLibs.h"
#include "RatwWire.h"

#include <cmath>
#include <cstring>

namespace ratw::tickets
{
namespace
{
const char* const Alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

bool hexId(const std::string& id, const char* prefix)
{
    const std::size_t n = std::strlen(prefix);
    if (id.size() != n + 32 || id.compare(0, n, prefix) != 0)
        return false;
    for (std::size_t i = n; i < id.size(); ++i)
        if (!(id[i] >= '0' && id[i] <= '9') && !(id[i] >= 'a' && id[i] <= 'f'))
            return false;
    return true;
}

// A whole number of seconds, as central writes them (never a bool, a string or a fraction).
bool whole(const json::Value* v, double& out)
{
    if (!v || !v->isNumber() || std::floor(v->asNumber()) != v->asNumber() || std::fabs(v->asNumber()) > 1e12)
        return false;
    out = v->asNumber();
    return true;
}

const std::string* text(const json::Value& o, const char* key)
{
    const auto* v = o.find(key);
    return v && v->isString() ? &v->asString() : nullptr;
}
} // namespace

bool base64url(const std::string& in, std::string& out)
{
    static const auto table = [] {
        std::array<std::int8_t, 256> t{};
        t.fill(-1);
        for (int i = 0; i < 64; ++i)
            t[static_cast<unsigned char>(Alphabet[i])] = std::int8_t(i);
        return t;
    }();
    out.clear();
    if (in.size() % 4 == 1)
        return false;
    std::uint32_t acc = 0;
    int bits = 0;
    for (const unsigned char ch : in)
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
        acc &= (1u << bits) - 1;
    }
    return acc == 0;                                // Stray bits must be zero: one spelling per value.
}

std::string toBase64url(const std::string& bytes)
{
    std::string out;
    std::uint32_t acc = 0;
    int bits = 0;
    for (const unsigned char ch : bytes)
    {
        acc = (acc << 8) | ch;
        bits += 8;
        while (bits >= 6)
        {
            bits -= 6;
            out += Alphabet[(acc >> bits) & 63];
        }
        acc &= (1u << bits) - 1;
    }
    if (bits > 0)
        out += Alphabet[(acc << (6 - bits)) & 63];
    return out;
}

bool accountId(const std::string& id) { return hexId(id, "acct-"); }
bool worldId(const std::string& id) { return hexId(id, "world-"); }
bool wolfId(const std::string& id) { return hexId(id, "wolf-"); }

Keys readKeys(const json::Value& jwks)
{
    Keys out;
    for (const auto& k : jwks.array("keys"))
    {
        std::string raw;
        const std::string kid = k.string("kid");
        if (k.string("kty") == "OKP" && k.string("crv") == "Ed25519" && k.string("alg") == "Ed25519" && !kid.empty() &&
            kid.size() <= 64 && base64url(k.string("x"), raw) && raw.size() == 32)
        {
            std::array<std::uint8_t, 32> key{};
            std::memcpy(key.data(), raw.data(), 32);
            out[kid] = key;
        }
    }
    return out;
}

void Checker::configure(std::string world, bool official, double started, double leeway)
{
    world_ = std::move(world);
    kind_ = official ? "official" : "community";
    started_ = started;
    leeway_ = leeway;
    seen_.clear();
}

Verdict Checker::check(const std::string& ticket, double now, Claims& claims, std::string& why)
{
    const auto refuse = [&](const char* reason) {
        why = reason;
        return Verdict::Refused;
    };
    claims = Claims{};
    if (ticket.empty() || ticket.size() > MaxTicket)
        return refuse("not a ticket");
    const auto dot1 = ticket.find('.');
    const auto dot2 = dot1 == std::string::npos ? dot1 : ticket.find('.', dot1 + 1);
    if (dot2 == std::string::npos || ticket.find('.', dot2 + 1) != std::string::npos)
        return refuse("a ticket has three parts");
    std::string headerText, payloadText, signature, error;
    json::Value header, body;
    if (!base64url(ticket.substr(0, dot1), headerText) || !json::parseUnique(headerText, header, error) || !header.isObject())
        return refuse("the header is not a JSON object");
    // The algorithm first, before anything else in the ticket is believed.
    if (const auto* alg = text(header, "alg"); !alg || *alg != "Ed25519")
        return refuse("wrong algorithm");
    if (const auto* typ = header.find("typ"); typ && !(typ->isString() && typ->asString() == "JWT"))
        return refuse("unexpected header");
    for (const auto& [key, v] : header.fields())
        if (key != "alg" && key != "typ" && key != "kid")
            return refuse("unexpected header");     // (No crit, jku, x5u...: nothing that would steer the check.)
    const auto* kid = text(header, "kid");
    if (!kid || kid->empty())
        return refuse("unknown key");
    const auto key = keys_.find(*kid);
    if (key == keys_.end())
    {
        why = "unknown key";
        return Verdict::UnknownKey;
    }
    if (!base64url(ticket.substr(dot2 + 1), signature) || signature.size() != 64)
        return refuse("bad signature");
    if (!sys::ed25519Verify(key->second.data(), ticket.substr(0, dot2), signature))
        return refuse("bad signature");
    // Signed by central: now what it says.
    if (!base64url(ticket.substr(dot1 + 1, dot2 - dot1 - 1), payloadText) || !json::parseUnique(payloadText, body, error) ||
        !body.isObject())
        return refuse("the claims are not a JSON object");
    for (const char* field : {"aud", "jti", "wolf", "sub", "name", "sex", "kind", "gift"})
        if (!text(body, field))
            return refuse("missing or malformed claims");
    double age = 0;
    if (!whole(body.find("iat"), claims.issued) || !whole(body.find("exp"), claims.expires) || !whole(body.find("age"), age) ||
        !body.find("look") || !body.find("look")->isObject())
        return refuse("missing or malformed claims");
    claims.world = body.string("aud");
    if (claims.world != world_)
        return refuse("ticket for another world");
    if (now > claims.expires + leeway_)
        return refuse("expired");
    if (claims.issued > now + leeway_)
        return refuse("issued in the future");
    if (claims.issued < started_ - leeway_)
        return refuse("issued before this world started");
    if (claims.expires <= claims.issued || claims.expires - claims.issued > MaxLifetime)
        return refuse("wrong lifetime");
    claims.kind = body.string("kind");
    if (claims.kind != "official" && claims.kind != "community")
        return refuse("malformed identity");
    if (claims.kind != kind_)
        return refuse("ticket for another kind of world");
    claims.jti = body.string("jti");
    if (claims.jti.size() < 16 || claims.jti.size() > 64)
        return refuse("malformed identity");
    for (const char c : claims.jti)
        if (!std::strchr(Alphabet, c) || c == '\0')
            return refuse("malformed identity");
    // The identity, in the game's own forms (doc 77, 4.1: the world checks it again, in case its Data/ differs).
    claims.account = body.string("sub");
    claims.wolf = body.string("wolf");
    claims.name = body.string("name");
    claims.sex = body.string("sex");
    Appearance look;
    const json::Value gift = body["gift"];
    if (!accountId(claims.account) || !wolfId(claims.wolf) || identity::name(claims.name) != claims.name ||
        age < 6 || age > 99 || (claims.sex != "female" && claims.sex != "male") || !wire::readAppearance(body["look"], look) ||
        look.sex != claims.sex || !identity::family(&gift, claims.family))
        return refuse("malformed identity");
    claims.age = int(age);
    claims.look = wire::appearance(look);
    // Once only. What is remembered is forgotten once it could no longer be admitted anyway.
    if (now >= nextPrune_)
    {
        for (auto it = seen_.begin(); it != seen_.end();)
            it = it->second < now ? seen_.erase(it) : std::next(it);
        nextPrune_ = now + 30;
    }
    if (const auto seen = seen_.find(claims.jti); seen != seen_.end() && seen->second >= now)
        return refuse("ticket already used");
    seen_[claims.jti] = claims.expires + leeway_;
    why.clear();
    return Verdict::Admitted;
}
} // namespace ratw::tickets
