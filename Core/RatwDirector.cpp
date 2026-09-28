#include "RatwDirector.h"

#include "RatwMind.h"
#include "RatwSystemLibs.h"
#include "RatwWire.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>

#if defined(__unix__) || defined(__APPLE__)
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace ratw::director
{
using json::Value;
namespace fs = std::filesystem;

namespace
{
bool keys(const Value& o, std::initializer_list<const char*> allowed)
{
    if (!o.isObject())
        return false;
    for (const auto& [k, v] : o.fields())
    {
        bool found = false;
        for (const char* a : allowed)
            found |= k == a;
        if (!found)
            return false;
    }
    return true;
}
bool stringField(const Value& o, const char* key) { const auto* v = o.find(key); return v && v->isString(); }
std::string text(const Value& o, const char* key) { const auto* v = o.find(key); return v && v->isString() ? v->asString() : std::string(); }
bool safeId(const std::string& id)
{
    if (id.empty() || id.size() > 80)
        return false;
    for (char c : id)
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_'))
            return false;
    return true;
}
std::string digest(const std::string& raw)
{
    std::uint8_t hash[20] = {};
    sys::sha1(raw, hash);
    std::string out = sys::hex(hash, 20);
    for (auto& c : out)
        c = char(std::toupper(static_cast<unsigned char>(c)));
    return out;
}
// A GUID as Unreal writes it with hyphens: 8-4-4-4-12 upper-case hex digits.
bool guidText(const std::string& s)
{
    if (s.size() != 36)
        return false;
    for (std::size_t i = 0; i < s.size(); ++i)
    {
        const bool dash = i == 8 || i == 13 || i == 18 || i == 23;
        if (dash ? s[i] != '-' : !std::isxdigit(static_cast<unsigned char>(s[i])))
            return false;
    }
    return true;
}
std::string newGuid()
{
    std::random_device device;
    std::mt19937_64 random(device());
    std::uint8_t bytes[16];
    for (auto& b : bytes)
        b = std::uint8_t(random());
    std::string h = sys::hex(bytes, 16);
    for (auto& c : h)
        c = char(std::toupper(static_cast<unsigned char>(c)));
    return h.substr(0, 8) + "-" + h.substr(8, 4) + "-" + h.substr(12, 4) + "-" + h.substr(16, 4) + "-" + h.substr(20);
}
bool writeFile(const std::string& path, const Value& value)
{
    const std::string temp = path + "." + newGuid() + ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out)
            return false;
        out << json::dump(value);
        if (!out.flush())
            return false;
    }
#if defined(__unix__) || defined(__APPLE__)
    if (::chmod(temp.c_str(), 0600) != 0)
    {
        std::remove(temp.c_str());
        return false;
    }
#endif
    std::error_code error;
    fs::rename(temp, path, error);
    if (error)
    {
        std::remove(temp.c_str());
        return false;
    }
    return true;
}
bool privateDirectory(const fs::path& p, bool made)
{
#if defined(__unix__) || defined(__APPLE__)
    if (made && ::chmod(p.c_str(), 0700) != 0)
        return false;
    struct stat info{};
    return ::stat(p.c_str(), &info) == 0 && info.st_uid == ::getuid() && !(info.st_mode & 0077);
#else
    (void)p; (void)made;
    return true;
#endif
}
double unixNow()
{
    return double(std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count());
}
} // namespace

Bridge::Bridge() : worldId_(newGuid()) {}

Bridge::~Bridge()
{
    if (write_.valid())
        write_.wait();
}

Value Bridge::result(const std::string& id, bool ok, const std::string& detail, double now) const
{
    auto r = Value::object();
    r.add("version", 1);
    r.add("id", id);
    r.add("worldId", worldId_);
    r.add("ok", ok);
    r.add("detail", detail);
    r.add("appliedAtUnix", now);
    return r;
}

bool Bridge::configure(const std::string& path)
{
    if (path.empty())
        return true;
    const fs::path root(path);
    std::error_code error;
    if (!root.is_absolute() || path.size() > 2048 || root == root.root_path() || fs::is_symlink(root, error))
        return false;
    error.clear();
    const bool created = fs::create_directories(root, error);
    if (error || !fs::is_directory(root, error) || !privateDirectory(root, created))
        return false;
    for (const char* child : {"outbox", "inbox"})
    {
        const auto sub = root / child;
        if (fs::is_symlink(sub, error))
            return false;
        error.clear();
        const bool made = fs::create_directory(sub, error);
        if (error || !fs::is_directory(sub, error) || !privateDirectory(sub, made))
            return false;
    }
    directory_ = fs::absolute(root).string();
    return true;
}

Value Bridge::state() const
{
    auto o = Value::object();
    o.add("worldId", worldId_);
    o.add("receipts", receipts_);
    return o;
}

bool Bridge::restore(const Value& o)
{
    const std::string identity = text(o, "worldId");
    const auto* saved = o.find("receipts");
    if (!keys(o, {"worldId", "receipts"}) || !guidText(identity) || !saved || !saved->isArray() || saved->size() > 512)
        return false;
    std::set<std::string> unique;
    for (const auto& r : saved->items())
    {
        const auto& reply = r["result"];
        const auto* ok = reply.find("ok");
        const std::string hash = text(r, "fingerprint");
        bool hex = hash.size() == 40;
        for (char c : hash)
            hex &= std::isxdigit(static_cast<unsigned char>(c)) != 0;
        if (!keys(r, {"id", "fingerprint", "result"}) || r.size() != 3 || !safeId(text(r, "id")) || !unique.insert(text(r, "id")).second ||
            !hex || !keys(reply, {"version", "id", "worldId", "ok", "detail", "appliedAtUnix"}) || reply.size() != 6 ||
            wire::strictNumber(reply, "version", -1) != 1 || !stringField(reply, "detail") || !ok || !ok->isBool() ||
            text(reply, "worldId") != identity || text(reply, "id") != text(r, "id") || text(reply, "detail").size() > 8192 * 3 ||
            wire::strictNumber(reply, "appliedAtUnix", -1) < 0)
            return false;
    }
    worldId_ = identity;
    receipts_ = *saved;
    return true;
}

Value Bridge::snapshot(const World& world, const std::map<std::string, Entity>& characters, const std::set<std::string>& online,
                       const std::map<std::string, double>& activity, std::uint64_t revision, double now) const
{
    auto o = Value::object();
    o.add("version", 1);
    o.add("worldId", worldId_);
    o.add("sequence", revision);
    o.add("generatedAtUnix", now);
    o.add("calendarDays", world.calendarDays());
    auto capabilities = Value::array(), cells = Value::array(), factions = Value::array(), chapters = Value::array(),
         actors = Value::array(), accounts = Value::array();
    for (const char* capability : {"notice", "weather", "npc_relocate", "economy_transfer"})
        capabilities.push(capability);
    o.add("capabilities", capabilities);
    for (const auto& [id, c] : world.cells())
    {
        auto j = Value::object(), territory = Value::object();
        j.add("id", c.id);
        j.add("name", c.name);
        j.add("x", c.worldX);
        j.add("y", c.worldY);
        j.add("z", c.worldZ);
        j.add("width", c.width);
        j.add("height", c.height);
        j.add("outdoors", c.outdoors);
        j.add("weather", weatherName(c.weather));
        territory.add("region", c.region);
        territory.add("chapter", c.chapter);
        auto claims = Value::array();
        for (const auto& claim : c.factionClaims)
            claims.push(claim);
        territory.add("claims", claims);
        j.add("territory", territory);
        // A streamed cell nobody is near has no tiles in memory: its ground is reported unknown (spaces).
        std::uint64_t fingerprint = 1469598103934665603ULL;
        const auto mix = [&fingerprint](std::uint64_t v) { fingerprint = (fingerprint ^ v) * 1099511628211ULL; };
        mix(std::uint64_t(c.width));
        mix(std::uint64_t(c.height));
        mix(std::uint64_t(c.tiles.size()));
        for (const auto& t : c.tiles)
            mix(static_cast<unsigned char>(t.glyph));
        auto& cached = terrain_[c.id];
        if (cached.rows.size() != std::size_t(c.height) || cached.fingerprint != fingerprint)
        {
            cached.fingerprint = fingerprint;
            cached.rows = Value::array();
            for (int y = 0; y < c.height; ++y)
            {
                std::string row;
                row.reserve(std::size_t(c.width));
                for (int x = 0; x < c.width; ++x)
                {
                    const auto* t = c.tile(x, y);
                    row += t ? t->glyph : ' ';
                }
                cached.rows.push(row);
            }
        }
        j.add("terrain", cached.rows);
        cells.push(j);
    }
    for (const auto& [id, f] : world.factions())
    {
        auto j = Value::object();
        j.add("id", id);
        j.add("name", f.name);
        j.add("color", f.color);
        factions.push(j);
    }
    for (const auto& [id, c] : world.chapters())
    {
        auto j = Value::object();
        j.add("id", id);
        j.add("name", c.name);
        chapters.push(j);
    }
    auto all = characters;
    for (const auto& [id, e] : world.entities())
        all[id] = e;
    for (const auto& [id, e] : all)
    {
        auto j = Value::object(), stock = Value::object();
        j.add("id", e.id);
        j.add("name", e.name);
        j.add("cell", e.cellId);
        j.add("npc", e.npc);
        j.add("online", e.npc || online.count(e.id) > 0);
        const auto seen = activity.find(e.id);
        const double last = seen == activity.end() ? 0 : seen->second;
        j.add("active", !e.npc && online.count(e.id) && last > 0 && now >= last && now - last <= 300);
        j.add("lastActiveAtUnix", last);
        j.add("x", e.position.x);
        j.add("y", e.position.y);
        j.add("age", e.age);
        j.add("activity", e.activity);
        const auto* life = world.society().resident(e.id);
        const auto* account = world.society().account(e.id);
        j.add("role", life ? life->role : std::string("player"));
        j.add("recruited", !e.leaderId.empty() || e.state == "following");
        j.add("homeCell", life ? life->homeCell : e.cellId);
        j.add("homeX", life ? life->homeX : e.position.x);
        j.add("homeY", life ? life->homeY : e.position.y);
        j.add("relocating", life && !life->relocationCell.empty());
        j.add("relocationTarget", life ? life->relocationCell : std::string());
        j.add("cash", account ? account->cash : 0);
        if (account)
            for (const auto& [item, n] : account->stock)
                stock.add(item, n);
        j.add("stock", stock);
        actors.push(j);
    }
    for (const auto& [id, a] : world.society().state().accounts)
    {
        auto j = Value::object(), stock = Value::object();
        j.add("id", id);
        j.add("cash", a.cash);
        for (const auto& [item, n] : a.stock)
            stock.add(item, n);
        j.add("stock", stock);
        accounts.push(j);
    }
    auto economy = Value::object();
    const auto full = wire::society(world.society().state());
    economy.add("minted", world.society().state().minted);
    economy.add("sunk", world.society().state().sunk);
    economy.add("ledger", full["ledger"]);
    o.add("cells", cells);
    o.add("factions", factions);
    o.add("chapters", chapters);
    o.add("characters", actors);
    o.add("accounts", accounts);
    o.add("economy", economy);
    return o;
}

Value Bridge::execute(const Value& request, const std::string& expectedId, const std::string& fingerprint, World& world,
                      const std::set<std::string>& online, double now, const std::function<bool()>& commit, const Notice& notice)
{
    if (!safeId(expectedId) || !keys(request, {"version", "id", "worldId", "createdAtUnix", "expiresAtUnix", "kind", "payload"}) ||
        wire::strictNumber(request, "version", -1) != 1 || text(request, "id") != expectedId || text(request, "worldId") != worldId_)
        return result(expectedId, false, "Invalid or cross-world operator envelope.", now);
    for (const auto& saved : receipts_.items())
        if (text(saved, "id") == expectedId)
            return text(saved, "fingerprint") == fingerprint ? saved["result"]
                                                             : result(expectedId, false, "Request ID was reused with different content.", now);
    const double created = wire::strictNumber(request, "createdAtUnix", -1), expires = wire::strictNumber(request, "expiresAtUnix", -1);
    if (created < 0 || created > now + 5 || expires < now || expires <= created || expires - created > 300)
        return result(expectedId, false, "Operator request expired or has an invalid clock window.", now);
    const auto& p = request["payload"];
    const std::string kind = text(request, "kind");
    const auto before = world;
    Result applied{false, "Unsupported or malformed operator action.", {}};
    std::set<std::string> recipients;
    std::string announcement;
    if (kind == "weather" && keys(p, {"cell", "preset"}) && p.size() == 2)
        applied = wire::environmentCommand(world, text(p, "cell"), "weather", text(p, "preset"), true);
    else if (kind == "npc_relocate" && keys(p, {"npc", "cell", "x", "y"}) && p.size() == 4)
        applied = world.relocateResident(text(p, "npc"), text(p, "cell"), wire::strictNumber(p, "x", -1), wire::strictNumber(p, "y", -1));
    else if (kind == "economy_transfer" && keys(p, {"from", "to", "item", "quantity", "coins"}) && p.size() == 5)
    {
        const double qty = wire::strictNumber(p, "quantity", -1), coins = wire::strictNumber(p, "coins", -1);
        if (stringField(p, "item") && qty >= 0 && qty <= 99 && coins >= 0 && coins <= 1000000 && qty == std::floor(qty) && coins == std::floor(coins))
        {
            const auto a = world.society().operatorTransfer(text(p, "from"), text(p, "to"), text(p, "item"), int(qty), std::int64_t(coins));
            applied = {a.ok, a.message, {}};
        }
    }
    else if (kind == "notice" && keys(p, {"scope", "target", "targets", "text"}))
    {
        announcement = text(p, "text");
        const std::string scope = text(p, "scope"), target = text(p, "target");
        bool valid = !mind::trim(announcement).empty() && mind::left(announcement, 8193).size() <= mind::left(announcement, 8192).size();
        if (scope == "world")
        {
            valid &= !p.has("targets") && (!p.has("target") || (stringField(p, "target") && target.empty()));
            recipients = online;
        }
        else if (scope == "player")
        {
            valid &= stringField(p, "target") && safeId(target) && !p.has("targets");
            if (online.count(target))
                recipients.insert(target);
        }
        else if (scope == "cell")
        {
            valid &= stringField(p, "target") && world.cell(target) && !p.has("targets");
            for (const auto& id : online)
                if (world.entity(id) && world.entity(id)->cellId == target)
                    recipients.insert(id);
        }
        else if (scope == "players")
        {
            valid &= !p.has("target");
            const auto* targets = p.find("targets");
            if (!targets || !targets->isArray() || targets->size() == 0 || targets->size() > 128)
                valid = false;
            else
                for (const auto& v : targets->items())
                {
                    if (!v.isString() || !safeId(v.asString()))
                        valid = false;
                    else if (online.count(v.asString()))
                        recipients.insert(v.asString());
                }
        }
        else
            valid = false;
        applied = valid && !recipients.empty()
                      ? Result{true, "Operator announcement queued for " + std::to_string(recipients.size()) + " connected character(s).", {}}
                      : Result{false, "Invalid announcement or no matching connected recipients.", {}};
    }
    auto reply = result(expectedId, applied.ok, applied.message, now);
    auto receipt = Value::object();
    receipt.add("id", expectedId);
    receipt.add("fingerprint", fingerprint);
    receipt.add("result", reply);
    const auto previous = receipts_;
    receipts_.push(receipt);
    if (receipts_.size() > 512)
        receipts_.items().erase(receipts_.items().begin());
    if (!commit())
    {
        if (applied.ok)
            world = before;
        receipts_ = previous;
        healthy_ = false;
        return result(expectedId, false, "Checkpoint failed; operator channel halted without committing the requested effect.", now);
    }
    if (applied.ok && !announcement.empty())
        notice(recipients, announcement);
    return reply;
}

void Bridge::tick(double dt, World& world, const std::map<std::string, Entity>& characters, const std::set<std::string>& online,
                  const std::map<std::string, double>& activity, std::uint64_t revision, const std::function<bool()>& commit,
                  const Notice& notice)
{
    if (!enabled())
        return;
    accumulator_ += dt;
    if (accumulator_ < 2)
        return;
    accumulator_ = 0;
    const double now = unixNow();
    const auto halt = [&](const char* why) {
        healthy_ = false;
        if (log)
            log(why);
    };
    std::error_code error;
    int count = 0;
    for (fs::directory_iterator it(fs::path(directory_) / "outbox", error), end; !error && it != end && count < 16; it.increment(error))
    {
        const auto path = it->path();
        if (path.extension() != ".json" || it->is_symlink(error) || !it->is_regular_file(error))
            continue;
        const std::string id = path.stem().string();
        if (!safeId(id))
            continue;
        ++count;
        const auto size = it->file_size(error);
        if (error)
            break;
        Value reply;
        if (size > 65536)
            reply = result(id, false, "Operator request exceeds 64 KiB.", now);
        else
        {
            std::ifstream in(path, std::ios::binary);
            if (!in)
                return halt("RATW operator request cannot be read; channel halted.");
            std::ostringstream raw;
            raw << in.rdbuf();
            Value request;
            std::string problem;
            json::parse(raw.str(), request, problem);
            reply = execute(request, id, digest(raw.str()), world, online, now, commit, notice);
        }
        if (!writeFile((fs::path(directory_) / "inbox" / (id + ".json")).string(), reply))
            return halt("RATW operator receipt cannot be published; channel halted.");
        fs::remove(path, error);
        if (!healthy_)
            return;
    }
    if (error)
        return halt("RATW operator queue filesystem failure; channel halted.");
    // The snapshot is taken here and written on a worker; while the last is still being written, this one is skipped.
    if (write_.valid())
    {
        if (write_.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
            return;
        if (!write_.get())
            return halt("RATW operator snapshot failed; channel halted.");
    }
    const std::string path = (fs::path(directory_) / "snapshot.json").string();
    auto value = snapshot(world, characters, online, activity, revision, now);
    write_ = std::async(std::launch::async, [path, value = std::move(value)] { return writeFile(path, value); });
}
} // namespace ratw::director
