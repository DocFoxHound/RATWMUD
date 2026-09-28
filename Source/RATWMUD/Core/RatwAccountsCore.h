#pragma once
// Local development accounts, portable: what Source/RATWMUD/Runtime/RatwAccounts.h does for the Unreal runtime, for a
// standalone server. The same rules, the same PBKDF2 verifiers and the same saved form ({"version":1,"entries":[...]}
// in the checkpoint), so either server reads the other's accounts. The owner map and password verifiers are saved
// only in the private checkpoint, never in a view.
#include "RatwJsonDoc.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace ratw::accounts
{
constexpr int PasswordIterations = 600000;
constexpr std::size_t CharacterSlots = 6;
constexpr std::size_t AccountLimit = 128;

bool normalizeUsername(const std::string& input, std::string& normalized);
bool validPassword(const std::string& password);
bool validDisplayName(const std::string& name);
bool validCommandId(const std::string& id);
bool isLoopbackAddress(const std::string& address);
std::string fingerprint(const std::string& value);   // SHA-256, hex.

class Accounts
{
  public:
    bool registerAccount(const std::string& username, const std::string& password, std::string& error);
    bool authenticate(const std::string& username, const std::string& password) const;
    bool exists(const std::string& username) const { return accounts_.count(username) > 0; }
    bool owns(const std::string& username, const std::string& characterId) const;
    std::vector<std::string> characters(const std::string& username) const;
    bool addCharacter(const std::string& username, const std::string& characterId, const std::string& commandId,
                      const std::string& requestFingerprint);
    // Empty for no matching receipt; `conflict` says the ID was used for different choices.
    std::string createdCharacter(const std::string& username, const std::string& commandId,
                                 const std::string& requestFingerprint, bool& conflict) const;
    json::Value state() const;
    bool restore(const json::Value& state);
    // Every owner references a saved character, and every generated wolf character has an owner.
    bool referencesOnly(const std::set<std::string>& characterIds) const;

  private:
    struct Creation
    {
        std::string character, fingerprint;
    };
    struct Account
    {
        std::string salt, verifier;
        int iterations = PasswordIterations;
        std::vector<std::string> characters;
        std::map<std::string, Creation> creations;
    };
    std::map<std::string, Account> accounts_;
};

// The global budget also limits reconnects. Monotonic time, never game time.
class RateLimit
{
  public:
    bool allow(const std::string& key, double now);
    void forget(const std::string& key) { peers_.erase(key); }

  private:
    std::vector<double> global_;
    std::map<std::string, std::vector<double>> peers_;
};
} // namespace ratw::accounts
