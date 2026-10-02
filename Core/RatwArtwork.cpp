#include "RatwArtwork.h"

#include "RatwJsonDoc.h"
#include "RatwPg.h"
#include "RatwSystemLibs.h"

#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

namespace ratw::art
{
namespace
{
std::uint32_t crc32(const unsigned char* data, std::size_t length, std::uint32_t crc = 0)
{
    static std::uint32_t table[256];
    static bool ready = false;
    if (!ready)
    {
        for (std::uint32_t n = 0; n < 256; ++n)
        {
            std::uint32_t c = n;
            for (int k = 0; k < 8; ++k)
                c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            table[n] = c;
        }
        ready = true;
    }
    crc = ~crc;
    for (std::size_t i = 0; i < length; ++i)
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return ~crc;
}

void be32(std::string& out, std::uint32_t v)
{
    out += char((v >> 24) & 0xFF);
    out += char((v >> 16) & 0xFF);
    out += char((v >> 8) & 0xFF);
    out += char(v & 0xFF);
}

void chunk(std::string& out, const char* type, const std::string& data)
{
    be32(out, std::uint32_t(data.size()));
    std::string body = std::string(type, 4) + data;
    out += body;
    be32(out, crc32(reinterpret_cast<const unsigned char*>(body.data()), body.size()));
}

json::Value metaJson(const Meta& m)
{
    auto o = json::Value::object();
    o.add("id", m.id);
    o.add("account", m.account);
    o.add("character", m.character);
    o.add("status", m.status);
    o.add("reason", m.reason);
    o.add("sha", m.sha);
    o.add("createdUnix", m.createdUnix);
    o.add("reported", m.reported);
    return o;
}

Meta readMeta(const json::Value& o)
{
    Meta m;
    m.id = o.string("id");
    m.account = o.string("account");
    m.character = o.string("character");
    m.status = o.string("status", "pending");
    m.reason = o.string("reason");
    m.sha = o.string("sha");
    m.createdUnix = o.number("createdUnix");
    m.reported = o.boolean("reported");
    return m;
}

bool safeId(const std::string& id)
{
    if (id.size() < 5 || id.size() > 48 || id.rfind("art-", 0) != 0)
        return false;
    for (char c : id)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-'))
            return false;
    return true;
}

class MemoryStore final : public Store
{
  public:
    std::map<std::string, std::pair<Meta, std::string>> items;
    bool put(const Meta& meta, const std::string& png, std::string&) override
    {
        items[meta.id] = {meta, png};
        return true;
    }
    bool image(const std::string& id, std::string& png) override
    {
        const auto found = items.find(id);
        if (found == items.end())
            return false;
        png = found->second.second;
        return true;
    }
    bool update(const Meta& meta) override
    {
        const auto found = items.find(meta.id);
        if (found == items.end())
            return false;
        found->second.first = meta;
        return true;
    }
    std::vector<Meta> all() override
    {
        std::vector<Meta> out;
        for (const auto& [id, item] : items)
            out.push_back(item.first);
        return out;
    }
};

class FolderStore final : public Store
{
  public:
    std::string dir;
    std::map<std::string, Meta> index;
    bool writeIndex()
    {
        auto list = json::Value::array();
        for (const auto& [id, m] : index)
            list.push(metaJson(m));
        const auto temp = dir + "/index.json.tmp";
        {
            std::ofstream out(temp, std::ios::trunc);
            if (!out)
                return false;
            out << json::dump(list);
        }
        return std::rename(temp.c_str(), (dir + "/index.json").c_str()) == 0;
    }
    bool put(const Meta& meta, const std::string& png, std::string& error) override
    {
        if (!safeId(meta.id))
        {
            error = "A portrait ID is art- and letters.";
            return false;
        }
        std::ofstream out(dir + "/" + meta.id + ".png", std::ios::binary | std::ios::trunc);
        if (!out || !(out << png) || !out.flush())
        {
            error = "Cannot write the portrait.";
            return false;
        }
        index[meta.id] = meta;
        if (!writeIndex())
        {
            error = "Cannot write the portrait index.";
            return false;
        }
        return true;
    }
    bool image(const std::string& id, std::string& png) override
    {
        if (!safeId(id) || !index.count(id))
            return false;
        std::ifstream in(dir + "/" + id + ".png", std::ios::binary);
        if (!in)
            return false;
        std::ostringstream text;
        text << in.rdbuf();
        png = text.str();
        return true;
    }
    bool update(const Meta& meta) override
    {
        if (!index.count(meta.id))
            return false;
        index[meta.id] = meta;
        return writeIndex();
    }
    std::vector<Meta> all() override
    {
        std::vector<Meta> out;
        for (const auto& [id, m] : index)
            out.push_back(m);
        return out;
    }
};

class DatabaseStore final : public Store
{
  public:
    PgClient pg;
    std::string world;
    bool put(const Meta& m, const std::string& png, std::string& error) override
    {
        // The image as base64 text: small (a 256-pixel portrait), and plain to read back.
        static const char* digits = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::string b64;
        for (std::size_t i = 0; i < png.size(); i += 3)
        {
            const std::uint32_t n = (std::uint8_t(png[i]) << 16) | (i + 1 < png.size() ? std::uint8_t(png[i + 1]) << 8 : 0) |
                                    (i + 2 < png.size() ? std::uint8_t(png[i + 2]) : 0);
            b64 += digits[(n >> 18) & 63];
            b64 += digits[(n >> 12) & 63];
            b64 += i + 1 < png.size() ? digits[(n >> 6) & 63] : '=';
            b64 += i + 2 < png.size() ? digits[n & 63] : '=';
        }
        const auto r = pg.exec("INSERT INTO game.artwork (world_id, id, account, character_id, status, reason, sha256, png_base64, reported) "
                               "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9)",
                               {world, m.id, m.account, m.character, m.status, m.reason, m.sha, b64, std::string(m.reported ? "true" : "false")});
        if (!r.ok)
            error = r.error;
        return r.ok;
    }
    bool image(const std::string& id, std::string& png) override
    {
        const auto r = pg.exec("SELECT png_base64 FROM game.artwork WHERE world_id = $1 AND id = $2", {world, id});
        if (!r.ok || r.rows.empty() || !r.rows[0][0])
            return false;
        const std::string& b64 = *r.rows[0][0];
        png.clear();
        std::uint32_t acc = 0;
        int bits = 0;
        for (char c : b64)
        {
            int v = c >= 'A' && c <= 'Z' ? c - 'A' : c >= 'a' && c <= 'z' ? c - 'a' + 26 : c >= '0' && c <= '9' ? c - '0' + 52
                    : c == '+' ? 62 : c == '/' ? 63 : -1;
            if (v < 0)
                continue;
            acc = (acc << 6) | std::uint32_t(v);
            bits += 6;
            if (bits >= 8)
            {
                bits -= 8;
                png += char((acc >> bits) & 0xFF);
            }
        }
        return true;
    }
    bool update(const Meta& m) override
    {
        return pg.exec("UPDATE game.artwork SET status = $3, reason = $4, reported = $5, reviewed_at = now() WHERE world_id = $1 AND id = $2",
                       {world, m.id, m.status, m.reason, std::string(m.reported ? "true" : "false")})
            .ok;
    }
    std::vector<Meta> all() override
    {
        std::vector<Meta> out;
        const auto r = pg.exec("SELECT id, account, character_id, status, reason, sha256, extract(epoch FROM created_at), reported "
                               "FROM game.artwork WHERE world_id = $1 ORDER BY created_at",
                               {world});
        if (!r.ok)
            return out;
        for (const auto& row : r.rows)
        {
            Meta m;
            m.id = row[0].value_or("");
            m.account = row[1].value_or("");
            m.character = row[2].value_or("");
            m.status = row[3].value_or("pending");
            m.reason = row[4].value_or("");
            m.sha = row[5].value_or("");
            m.createdUnix = std::atof(row[6].value_or("0").c_str());
            m.reported = row[7].value_or("f") == "t";
            out.push_back(m);
        }
        return out;
    }
};
} // namespace

std::string encodePng(const std::vector<unsigned char>& rgba, int width, int height)
{
    if (width <= 0 || height <= 0 || rgba.size() != std::size_t(width) * height * 4)
        return {};
    std::vector<std::uint8_t> raw;
    raw.reserve(rgba.size() + std::size_t(height));
    for (int y = 0; y < height; ++y)
    {
        raw.push_back(0);                          // No filter.
        raw.insert(raw.end(), rgba.begin() + std::ptrdiff_t(y) * width * 4, rgba.begin() + std::ptrdiff_t(y + 1) * width * 4);
    }
    std::vector<std::uint8_t> packed;
    if (!sys::compress(raw.data(), raw.size(), packed))
        return {};
    std::string out = "\x89PNG\r\n\x1a\n";
    std::string header;
    be32(header, std::uint32_t(width));
    be32(header, std::uint32_t(height));
    header += char(8);                             // Bits per channel.
    header += char(6);                             // RGBA.
    header += std::string(3, '\0');                // Deflate, adaptive filtering, no interlace.
    chunk(out, "IHDR", header);
    chunk(out, "IDAT", std::string(packed.begin(), packed.end()));
    chunk(out, "IEND", {});
    return out;
}

std::unique_ptr<Store> memoryStore() { return std::make_unique<MemoryStore>(); }

std::unique_ptr<Store> folderStore(const std::string& directory, std::string& error)
{
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    if (ec)
    {
        error = "Cannot make the portrait folder " + directory + ".";
        return nullptr;
    }
    auto store = std::make_unique<FolderStore>();
    store->dir = directory;
    std::ifstream in(directory + "/index.json");
    if (in)
    {
        std::ostringstream text;
        text << in.rdbuf();
        json::Value list;
        std::string problem;
        if (json::parse(text.str(), list, problem))
            for (const auto& o : list.items())
                if (const auto m = readMeta(o); safeId(m.id))
                    store->index[m.id] = m;
    }
    return store;
}

std::unique_ptr<Store> databaseStore(const std::string& conninfo, const std::string& worldId, std::string& error)
{
    auto store = std::make_unique<DatabaseStore>();
    store->world = worldId;
    if (!store->pg.connect(conninfo, error))
        return nullptr;
    const auto check = store->pg.exec("SELECT to_regclass('game.artwork') IS NOT NULL");
    if (!check.ok || check.rows.empty() || check.rows[0][0].value_or("f") != "t")
    {
        error = "The database has no game.artwork table yet (migration 0027).";
        return nullptr;
    }
    return store;
}
} // namespace ratw::art
