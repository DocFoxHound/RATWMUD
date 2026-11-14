#pragma once
// Central's tickets, as a world checks them (Docs/Design/77-central-accounts-and-hosted-worlds.md, section 5). A ticket
// is a JWT signed with the fully specified `Ed25519` algorithm of RFC 9864; central (Central/central_tickets.py) signs
// it, and `Checker` here admits it offline with central's public keys, by the same rules as central_tickets.verify:
//
//   - the algorithm is pinned before anything else is read: `alg` exactly "Ed25519" (no `none`, `EdDSA`, HS256...);
//   - the header holds only `alg`, `typ` ("JWT", if there) and `kid`, and the key named by `kid` must be known (an
//     unknown one is UnknownKey, so the world can fetch central's keys again);
//   - the signature; then the claims, typed strictly, with no key twice anywhere (json::parseUnique);
//   - `aud` this world's id, `kind` this world's kind, `exp` not passed and `iat` not ahead (both with the leeway for
//     a home machine's clock), `iat` not before this world started, and a lifetime of at most MaxLifetime;
//   - the identity in the game's own forms (identity::name, wire::readAppearance, identity::family), the sex the
//     look's own; `wolf` a wolf id, `sub` an account id; `quickened`, if there, true or false;
//   - each `jti` admitted once, remembered until its `exp` plus the leeway (a restart is covered by the `iat` rule).
//
// Base64url is strict: the URL alphabet, no padding, no stray bits, so one ticket has one spelling.
#include "RatwJsonDoc.h"

#include <array>
#include <cstdint>
#include <map>
#include <string>

namespace ratw::tickets
{
constexpr double Leeway = 120;                      // Seconds: a home machine's wrong clock (doc 77, 5).
constexpr double MaxLifetime = 600;                 // Central issues 60 s tickets; anything past this is refused.
constexpr std::size_t MaxTicket = 4096;             // Bytes; a ticket with a full look is about 700.

bool base64url(const std::string& in, std::string& out);
std::string toBase64url(const std::string& bytes);
bool accountId(const std::string& id);              // acct-<32 lower-case hex>: central's (or a pairwise) account id.
bool worldId(const std::string& id);                // world-<32 lower-case hex>.
bool wolfId(const std::string& id);                 // wolf-<32 lower-case hex>.

// What an admitted ticket says.
struct Claims
{
    std::string account;                            // `sub`: the account's id for this world.
    std::string wolf, name, sex, family;            // `family` from `gift`: "" for none.
    int age = 0;
    json::Value look;                               // The appearance, as central keeps it (wire::appearance's form).
    std::string world, kind, jti;
    double issued = 0, expires = 0;
    // `quickened`: central's word that the account has opened Quickened, for every world (the user, 2026-11-14: "once
    // per the whole account, not once per world"; doc 49, 9.1). Optional: a ticket without it (an older central) is
    // false. Anything but true or false there is refused.
    bool quickened = false;
};

// Central's public keys: {kid: raw Ed25519 key}, from a JWK set as GET /keys serves it (the world agent keeps a copy
// in a file: tools/world_agent.py). Keys that aren't OKP/Ed25519/alg Ed25519 with a 32-byte `x` are left out.
using Keys = std::map<std::string, std::array<std::uint8_t, 32>>;
Keys readKeys(const json::Value& jwks);

enum class Verdict
{
    Admitted,
    UnknownKey,                                     // Signed with a key this world doesn't hold: fetch the keys again.
    Refused,
};

class Checker
{
  public:
    // This world: its id, whether it is official (tickets name the kind; a ticket of the other kind is refused), and
    // when it started (Unix seconds).
    void configure(std::string world, bool official, double started, double leeway = Leeway);
    void setKeys(Keys keys) { keys_ = std::move(keys); }
    const Keys& keys() const { return keys_; }
    // The verdict on `ticket` at `now` (Unix seconds); `why` says what refused it, `claims` holds what it says once
    // admitted. An admitted ticket's `jti` is remembered: the same ticket again is refused.
    Verdict check(const std::string& ticket, double now, Claims& claims, std::string& why);
    std::size_t remembered() const { return seen_.size(); }

  private:
    std::string world_;
    std::string kind_ = "community";
    double started_ = 0, leeway_ = Leeway, nextPrune_ = 0;
    Keys keys_;
    std::map<std::string, double> seen_;            // jti -> when it may be forgotten.
};
} // namespace ratw::tickets
