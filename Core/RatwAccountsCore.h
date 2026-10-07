#pragma once
// Local development accounts: usernames, PBKDF2 password verifiers, the characters each account owns, and the rate
// limit on sign-in attempts. Saved as {"version":1,"entries":[...]} in the checkpoint (the same form the Unreal
// server wrote, so its saves still load). The owner map and password verifiers are saved only in the private
// checkpoint, never in a view.
#include "RatwJsonDoc.h"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

namespace ratw::accounts
{
constexpr int PasswordIterations = 600000;
constexpr std::size_t CharacterSlots = 6;
constexpr std::size_t AccountLimit = 4096;   // (Doc 50: room for a thousand players and more.)

bool normalizeUsername(const std::string& input, std::string& normalized);
bool validPassword(const std::string& password);
bool validDisplayName(const std::string& name);
bool validCommandId(const std::string& id);
bool isLoopbackAddress(const std::string& address);
std::string fingerprint(const std::string& value);   // SHA-256, hex.

// Password work, slow on purpose (PBKDF2, PasswordIterations rounds: about 0.2 s), done by a Hasher off the game's
// thread (Docs/Design/31-responsiveness.md). A job carries what the work needs and nothing the game changes.
struct PasswordJob
{
    std::uint64_t ticket = 0;
    bool registering = false;
    std::string username, password;
    std::string salt, verifier;               // Signing in: the account's (or stand-ins, for a name that has none).
    int iterations = PasswordIterations;
    bool known = false;                       // Signing in: the account exists.
};
struct PasswordResult
{
    std::uint64_t ticket = 0;
    bool ok = false;                          // Registering: a verifier was made. Signing in: the password matched.
    std::string salt, verifier;               // Registering: the new account's.
};
// The work itself: the same for a Hasher's thread and for the synchronous calls below.
PasswordResult passwordWork(const PasswordJob& job);

class Accounts
{
  public:
    bool registerAccount(const std::string& username, const std::string& password, std::string& error);
    bool authenticate(const std::string& username, const std::string& password) const;
    // The same in two steps, for a Hasher: whether a name may be registered (checked again when it is added), the job
    // that checks a password, and the account added from a finished job.
    bool mayRegister(const std::string& username, std::string& error) const;
    PasswordJob signInJob(const std::string& username, const std::string& password) const;
    bool addRegistered(const std::string& username, const PasswordResult& done, std::string& error);
    void removeAccount(const std::string& username);
    bool exists(const std::string& username) const { return accounts_.count(username) > 0; }
    bool owns(const std::string& username, const std::string& characterId) const;
    std::vector<std::string> characters(const std::string& username) const;
    // The account a character belongs to ("" for none: an NPC, or a development identity's wolf).
    std::string ownerOf(const std::string& characterId) const;
    std::vector<std::string> usernames() const;   // Every account, in name order.
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
    std::map<std::string, std::string> owners_;   // Character -> account, kept with `accounts_`.
};

// Runs password jobs on threads of its own; the game collects what is finished each tick.
class Hasher
{
  public:
    explicit Hasher(int threads = 2);
    ~Hasher();
    Hasher(const Hasher&) = delete;
    Hasher& operator=(const Hasher&) = delete;
    void submit(PasswordJob job);
    std::vector<PasswordResult> finished();
    void waitIdle();                          // Until every job submitted is finished (tests and tools).

  private:
    void run();
    std::mutex lock_;
    std::condition_variable wake_, idle_;
    std::deque<PasswordJob> queue_;
    std::vector<PasswordResult> done_;
    int busy_ = 0;
    bool stopping_ = false;
    std::vector<std::thread> threads_;
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
