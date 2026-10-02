#include "RatwAccountsCore.h"

#include "RatwSystemLibs.h"
#include "RatwWire.h"

#include <algorithm>
#include <cmath>

namespace ratw::accounts
{
namespace
{
// Characters in the UTF-8 text (what the Unreal rules count; outside the Basic Multilingual Plane they count two).
std::size_t units(const std::string& s)
{
    std::size_t n = 0;
    for (std::size_t i = 0; i < s.size();)
    {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        const std::size_t length = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : 4;
        n += length == 4 ? 2 : 1;
        i += length;
    }
    return n;
}
bool control(const std::string& s)
{
    return std::any_of(s.begin(), s.end(), [](char c) { return static_cast<unsigned char>(c) < 32 || c == 127; });
}
bool derive(const std::string& password, const std::uint8_t* salt, int iterations, std::uint8_t* out)
{
    return sys::pbkdf2Sha256(password, salt, 32, iterations, out, 32);
}
bool characterId(const std::string& id)
{
    if (id.rfind("wolf-", 0) != 0 || id.size() != 37)
        return false;
    for (std::size_t i = 5; i < id.size(); ++i)
        if (!(id[i] >= '0' && id[i] <= '9') && !(id[i] >= 'a' && id[i] <= 'f'))
            return false;
    return true;
}
bool exact(const json::Value& o, std::initializer_list<const char*> fields)
{
    if (!o.isObject() || o.size() != fields.size())
        return false;
    for (const char* key : fields)
        if (!o.has(key))
            return false;
    return true;
}
bool text(const json::Value& o, const char* key, std::string& out)
{
    const auto* v = o.find(key);
    if (!v || !v->isString())
        return false;
    out = v->asString();
    return true;
}
} // namespace

bool normalizeUsername(const std::string& input, std::string& normalized)
{
    if (input.size() < 3 || input.size() > 32)
        return false;
    std::string lower = input;
    for (auto& c : lower)
        if (c >= 'A' && c <= 'Z')
            c = char(c - 'A' + 'a');
    if (lower[0] < 'a' || lower[0] > 'z')
        return false;
    for (char c : lower)
        if (!(c >= 'a' && c <= 'z') && !(c >= '0' && c <= '9') && c != '_' && c != '-')
            return false;
    normalized = lower;
    return true;
}

bool validPassword(const std::string& password)
{
    return units(password) <= 128 && !control(password) && password.size() >= 12 && password.size() <= 128;
}

bool validDisplayName(const std::string& name)
{
    const auto space = [](char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'; };
    const std::size_t n = units(name);
    return n >= 2 && n <= 32 && !control(name) && !space(name.front()) && !space(name.back());
}

bool validCommandId(const std::string& id)
{
    if (id.empty() || id.size() > 128)
        return false;
    for (char c : id)
        if (!(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z') && !(c >= '0' && c <= '9') && c != '-' && c != '_')
            return false;
    return true;
}

bool isLoopbackAddress(const std::string& address)
{
    std::string ip = address;
    if (ip.size() >= 2 && ip.front() == '[' && ip.back() == ']')
        ip = ip.substr(1, ip.size() - 2);
    if (ip == "::1" || ip == "0:0:0:0:0:0:0:1")
        return true;
    if (ip.size() > 7)
    {
        std::string prefix = ip.substr(0, 7);
        for (auto& c : prefix)
            c = char(std::tolower(static_cast<unsigned char>(c)));
        if (prefix == "::ffff:")
            ip = ip.substr(7);
    }
    std::vector<std::string> parts;
    std::string part;
    for (char c : ip)
    {
        if (c == '.')
        {
            parts.push_back(part);
            part.clear();
        }
        else
            part += c;
    }
    parts.push_back(part);
    if (parts.size() != 4 || parts[0] != "127")
        return false;
    for (const auto& p : parts)
    {
        if (p.empty() || p.size() > 3 || (p.size() > 1 && p[0] == '0'))
            return false;
        int value = 0;
        for (char c : p)
        {
            if (c < '0' || c > '9')
                return false;
            value = value * 10 + (c - '0');
        }
        if (value > 255)
            return false;
    }
    return true;
}

std::string fingerprint(const std::string& value)
{
    std::uint8_t digest[32] = {};
    sys::sha256(value, digest);
    return sys::hex(digest, 32);
}

PasswordResult passwordWork(const PasswordJob& job)
{
    PasswordResult out;
    out.ticket = job.ticket;
    if (job.registering)
    {
        std::uint8_t salt[32], verifier[32];
        if (sys::randomBytes(salt, sizeof salt) && derive(job.password, salt, PasswordIterations, verifier))
        {
            out.ok = true;
            out.salt = sys::hex(salt, 32);
            out.verifier = sys::hex(verifier, 32);
        }
        sys::wipe(verifier, sizeof verifier);
        return out;
    }
    // Missing users pay the same bounded PBKDF2 cost; the answer is the same.
    std::uint8_t salt[32] = {}, expected[32] = {}, actual[32] = {};
    if (job.known && (!sys::readHex(job.salt, salt, 32) || !sys::readHex(job.verifier, expected, 32)))
        return out;
    const bool derived = derive(job.password, salt, job.known ? job.iterations : PasswordIterations, actual);
    out.ok = job.known && derived && sys::sameBytes(actual, expected, 32);
    sys::wipe(actual, sizeof actual);
    sys::wipe(expected, sizeof expected);
    return out;
}

bool Accounts::mayRegister(const std::string& username, std::string& error) const
{
    std::string user;
    if (!normalizeUsername(username, user))
    {
        error = "Use a 3\xe2\x80\x93" "32 character username starting with a letter (letters, digits, _ or -), and a 12\xe2\x80\x93" "128 byte password without control characters.";
        return false;
    }
    if (accounts_.count(user) || accounts_.size() >= AccountLimit)
    {
        error = "This account cannot be registered. Try another username, or sign in to an existing account.";
        return false;
    }
    return true;
}

PasswordJob Accounts::signInJob(const std::string& username, const std::string& password) const
{
    PasswordJob job;
    job.password = password;
    if (!normalizeUsername(username, job.username) || !validPassword(password))
        return job;                                // Unknown: checked at the same cost, and refused.
    if (const auto found = accounts_.find(job.username); found != accounts_.end())
    {
        job.known = true;
        job.salt = found->second.salt;
        job.verifier = found->second.verifier;
        job.iterations = found->second.iterations;
    }
    return job;
}

bool Accounts::addRegistered(const std::string& username, const PasswordResult& done, std::string& error)
{
    if (!mayRegister(username, error))            // Someone may have taken the name while the password was worked on.
        return false;
    if (!done.ok)
    {
        error = "Secure password storage is unavailable; no account was created.";
        return false;
    }
    std::string user;
    normalizeUsername(username, user);
    Account account;
    account.salt = done.salt;
    account.verifier = done.verifier;
    accounts_[user] = std::move(account);
    return true;
}

bool Accounts::registerAccount(const std::string& username, const std::string& password, std::string& error)
{
    if (!mayRegister(username, error))
        return false;
    if (!validPassword(password))
    {
        error = "Use a 3\xe2\x80\x93" "32 character username starting with a letter (letters, digits, _ or -), and a 12\xe2\x80\x93" "128 byte password without control characters.";
        return false;
    }
    PasswordJob job;
    job.registering = true;
    job.password = password;
    return addRegistered(username, passwordWork(job), error);
}

bool Accounts::authenticate(const std::string& username, const std::string& password) const
{
    return passwordWork(signInJob(username, password)).ok;
}

// --------------------------------------------------------------------------- The hasher

Hasher::Hasher(int threads)
{
    for (int i = 0; i < std::max(1, threads); ++i)
        threads_.emplace_back([this] { run(); });
}

Hasher::~Hasher()
{
    {
        std::lock_guard<std::mutex> guard(lock_);
        stopping_ = true;
    }
    wake_.notify_all();
    for (auto& t : threads_)
        t.join();
}

void Hasher::submit(PasswordJob job)
{
    {
        std::lock_guard<std::mutex> guard(lock_);
        queue_.push_back(std::move(job));
    }
    wake_.notify_one();
}

std::vector<PasswordResult> Hasher::finished()
{
    std::lock_guard<std::mutex> guard(lock_);
    std::vector<PasswordResult> out;
    out.swap(done_);
    return out;
}

void Hasher::waitIdle()
{
    std::unique_lock<std::mutex> guard(lock_);
    idle_.wait(guard, [this] { return queue_.empty() && busy_ == 0; });
}

void Hasher::run()
{
    std::unique_lock<std::mutex> guard(lock_);
    for (;;)
    {
        wake_.wait(guard, [this] { return stopping_ || !queue_.empty(); });
        if (queue_.empty())
            return;
        auto job = std::move(queue_.front());
        queue_.pop_front();
        ++busy_;
        guard.unlock();
        auto result = passwordWork(job);
        std::fill(job.password.begin(), job.password.end(), '\0');
        guard.lock();
        --busy_;
        done_.push_back(std::move(result));
        idle_.notify_all();
    }
}

bool Accounts::owns(const std::string& username, const std::string& id) const
{
    const auto found = accounts_.find(username);
    return found != accounts_.end() &&
           std::find(found->second.characters.begin(), found->second.characters.end(), id) != found->second.characters.end();
}

std::vector<std::string> Accounts::characters(const std::string& username) const
{
    const auto found = accounts_.find(username);
    return found == accounts_.end() ? std::vector<std::string>{} : found->second.characters;
}

bool Accounts::addCharacter(const std::string& username, const std::string& id, const std::string& commandId,
                            const std::string& requestFingerprint)
{
    const auto found = accounts_.find(username);
    if (found == accounts_.end())
        return false;
    auto& account = found->second;
    if (account.characters.size() >= CharacterSlots || !characterId(id) || !validCommandId(commandId) ||
        account.creations.count(commandId) || requestFingerprint.size() != 64)
        return false;
    for (const auto& [user, other] : accounts_)
        if (std::find(other.characters.begin(), other.characters.end(), id) != other.characters.end())
            return false;
    account.characters.push_back(id);
    account.creations[commandId] = {id, requestFingerprint};
    return true;
}

std::string Accounts::createdCharacter(const std::string& username, const std::string& commandId,
                                       const std::string& requestFingerprint, bool& conflict) const
{
    conflict = false;
    const auto found = accounts_.find(username);
    if (found == accounts_.end())
        return {};
    const auto creation = found->second.creations.find(commandId);
    if (creation == found->second.creations.end())
        return {};
    conflict = creation->second.fingerprint != requestFingerprint;
    return conflict ? std::string() : creation->second.character;
}

json::Value Accounts::state() const
{
    auto root = json::Value::object();
    root.add("version", 1);
    auto entries = json::Value::array();
    for (const auto& [user, account] : accounts_)     // (A map: in name order, as the Unreal accounts sort them.)
    {
        auto entry = json::Value::object();
        entry.add("username", user);
        entry.add("salt", account.salt);
        entry.add("verifier", account.verifier);
        entry.add("iterations", account.iterations);
        auto ids = json::Value::array();
        for (const auto& id : account.characters)
            ids.push(id);
        entry.add("characters", ids);
        auto creations = json::Value::object();
        for (const auto& [request, c] : account.creations)
        {
            auto receipt = json::Value::object();
            receipt.add("character", c.character);
            receipt.add("fingerprint", c.fingerprint);
            creations.add(request, receipt);
        }
        entry.add("creations", creations);
        entries.push(entry);
    }
    root.add("entries", entries);
    return root;
}

bool Accounts::restore(const json::Value& root)
{
    if (!exact(root, {"version", "entries"}) || wire::strictNumber(root, "version", -1) != 1)
        return false;
    const auto* entries = root.find("entries");
    if (!entries || !entries->isArray() || entries->size() > AccountLimit)
        return false;
    std::map<std::string, Account> candidate;
    std::set<std::string> owned;
    std::uint8_t bytes[32];
    for (const auto& entry : entries->items())
    {
        if (!exact(entry, {"username", "salt", "verifier", "iterations", "characters", "creations"}))
            return false;
        Account account;
        std::string user, normalized;
        if (!text(entry, "username", user) || !normalizeUsername(user, normalized) || user != normalized || candidate.count(user) ||
            !text(entry, "salt", account.salt) || !sys::readHex(account.salt, bytes, 32) ||
            !text(entry, "verifier", account.verifier) || !sys::readHex(account.verifier, bytes, 32) ||
            wire::strictNumber(entry, "iterations", -1) != PasswordIterations)
            return false;
        const auto* ids = entry.find("characters");
        if (!ids || !ids->isArray() || ids->size() > CharacterSlots)
            return false;
        for (const auto& id : ids->items())
        {
            if (!id.isString() || !characterId(id.asString()) || owned.count(id.asString()))
                return false;
            owned.insert(id.asString());
            account.characters.push_back(id.asString());
        }
        const auto* creations = entry.find("creations");
        if (!creations || !creations->isObject() || creations->size() != account.characters.size())
            return false;
        std::set<std::string> created;
        for (const auto& [request, receipt] : creations->fields())
        {
            Creation c;
            if (!validCommandId(request) || !exact(receipt, {"character", "fingerprint"}) || !text(receipt, "character", c.character) ||
                std::find(account.characters.begin(), account.characters.end(), c.character) == account.characters.end() ||
                created.count(c.character) || !text(receipt, "fingerprint", c.fingerprint) || !sys::readHex(c.fingerprint, bytes, 32))
                return false;
            created.insert(c.character);
            account.creations[request] = std::move(c);
        }
        candidate[user] = std::move(account);
    }
    accounts_ = std::move(candidate);
    return true;
}

bool Accounts::referencesOnly(const std::set<std::string>& ids) const
{
    std::set<std::string> owned;
    for (const auto& [user, account] : accounts_)
        for (const auto& id : account.characters)
        {
            if (!ids.count(id))
                return false;
            owned.insert(id);
        }
    // Legacy development characters are deliberately unowned; the generated wolf namespace never is.
    for (const auto& id : ids)
        if (id.rfind("wolf-", 0) == 0 && (!characterId(id) || !owned.count(id)))
            return false;
    return true;
}

bool RateLimit::allow(const std::string& key, double now)
{
    if (!std::isfinite(now) || key.empty())
        return false;
    const auto stale = [now](double at) { return at <= now - 60 || at > now; };
    global_.erase(std::remove_if(global_.begin(), global_.end(), stale), global_.end());
    auto& peer = peers_[key];
    peer.erase(std::remove_if(peer.begin(), peer.end(), stale), peer.end());
    if (global_.size() >= 24 || peer.size() >= 6)
        return false;
    global_.push_back(now);
    peer.push_back(now);
    return true;
}
} // namespace ratw::accounts
