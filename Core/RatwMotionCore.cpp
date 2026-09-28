#include "RatwMotionCore.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ratw::motion
{
using json::Value;
namespace
{
constexpr std::uint32_t Magic = 0x31544d52;        // "RMT1"
constexpr int MaxPoses = 4096;

// FArchive's little-endian scalars.
template <class T>
void put(std::vector<std::uint8_t>& out, T v)
{
    std::uint8_t bytes[sizeof(T)];
    std::memcpy(bytes, &v, sizeof(T));
    out.insert(out.end(), bytes, bytes + sizeof(T));
}
// UTF-8 to UTF-16 code units.
std::u16string utf16(const std::string& s)
{
    std::u16string out;
    for (std::size_t i = 0; i < s.size();)
    {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        std::uint32_t cp;
        std::size_t n;
        if (c < 0x80) { cp = c; n = 1; }
        else if ((c >> 5) == 6) { cp = c & 0x1F; n = 2; }
        else if ((c >> 4) == 14) { cp = c & 0x0F; n = 3; }
        else { cp = c & 0x07; n = 4; }
        for (std::size_t k = 1; k < n && i + k < s.size(); ++k)
            cp = (cp << 6) | (static_cast<unsigned char>(s[i + k]) & 0x3F);
        i += n;
        if (cp >= 0x10000)
        {
            cp -= 0x10000;
            out += char16_t(0xD800 + (cp >> 10));
            out += char16_t(0xDC00 + (cp & 0x3FF));
        }
        else
            out += char16_t(cp);
    }
    return out;
}
// FString as FArchive writes it: ANSI text as its length with the terminator then the bytes; anything else as minus
// its UTF-16 length with the terminator then the units; the empty string as 0.
void putString(std::vector<std::uint8_t>& out, const std::string& s)
{
    if (s.empty())
    {
        put<std::int32_t>(out, 0);
        return;
    }
    const auto units = utf16(s);
    const bool ansi = std::all_of(units.begin(), units.end(), [](char16_t u) { return u < 0x80; });
    if (ansi)
    {
        put<std::int32_t>(out, std::int32_t(units.size() + 1));
        for (char16_t u : units)
            out.push_back(std::uint8_t(u));
        out.push_back(0);
    }
    else
    {
        put<std::int32_t>(out, -std::int32_t(units.size() + 1));
        for (char16_t u : units)
            put<std::uint16_t>(out, std::uint16_t(u));
        put<std::uint16_t>(out, 0);
    }
}

struct Reader
{
    const std::vector<std::uint8_t>& b;
    std::size_t at = 0;
    bool bad = false;
    template <class T>
    T get()
    {
        T v{};
        if (at + sizeof(T) > b.size()) { bad = true; return v; }
        std::memcpy(&v, b.data() + at, sizeof(T));
        at += sizeof(T);
        return v;
    }
    std::string string()
    {
        const auto n = get<std::int32_t>();
        if (bad || n == 0)
            return {};
        if (n > 0)
        {
            if (n > 4096 || at + std::size_t(n) > b.size()) { bad = true; return {}; }
            std::string s(reinterpret_cast<const char*>(b.data() + at), std::size_t(n - 1));
            at += std::size_t(n);
            return s;
        }
        const auto units = std::size_t(-std::int64_t(n));
        if (units > 4096 || at + units * 2 > b.size()) { bad = true; return {}; }
        std::string s;
        for (std::size_t i = 0; i + 1 < units; ++i)
        {
            std::uint32_t cp = get<std::uint16_t>();
            if (cp >= 0xD800 && cp < 0xDC00 && i + 2 < units)
            {
                cp = 0x10000 + ((cp - 0xD800) << 10) + (get<std::uint16_t>() - 0xDC00);
                ++i;
            }
            if (cp < 0x80) s += char(cp);
            else if (cp < 0x800) { s += char(0xC0 | (cp >> 6)); s += char(0x80 | (cp & 0x3F)); }
            else if (cp < 0x10000) { s += char(0xE0 | (cp >> 12)); s += char(0x80 | ((cp >> 6) & 0x3F)); s += char(0x80 | (cp & 0x3F)); }
            else { s += char(0xF0 | (cp >> 18)); s += char(0x80 | ((cp >> 12) & 0x3F)); s += char(0x80 | ((cp >> 6) & 0x3F)); s += char(0x80 | (cp & 0x3F)); }
        }
        get<std::uint16_t>();                      // The terminator.
        return s;
    }
};
} // namespace

Value frame(const World& world, const std::string& observer)
{
    auto root = Value::object();
    const auto* self = world.entity(observer);
    if (!self)
        return root;
    root.add("observer", observer);
    root.add("cellId", self->cellId);
    root.add("time", world.time());
    auto poses = Value::array();
    for (const auto& [id, e] : world.entities())
    {
        const bool isSelf = id == observer;
        if (!isSelf && (e.cellId != self->cellId || world.visionClarity(observer, id) <= 0))
            continue;
        auto pose = Value::object();
        pose.add("id", e.id);
        pose.add("x", e.position.x);
        pose.add("y", e.position.y);
        pose.add("facing", e.facing);
        pose.add("moving", std::abs(e.velocity.x) + std::abs(e.velocity.y) > .001 || e.postureRemaining > 0 ||
                               (isSelf && (!e.path.empty() || std::abs(e.input.x) + std::abs(e.input.y) > .001)));
        poses.push(pose);
    }
    root.add("entities", poses);
    return root;
}

std::vector<std::uint8_t> pack(const Value& f)
{
    std::vector<std::uint8_t> out;
    put<std::uint32_t>(out, Magic);
    putString(out, f.string("motionSession"));
    putString(out, f.string("observer"));
    putString(out, f.string("cellId"));
    put<std::int32_t>(out, std::int32_t(f.number("cellGeneration")));
    put<double>(out, f.number("revision"));
    put<double>(out, f.number("time"));
    const auto& poses = f.array("entities");
    const int count = int(std::min<std::size_t>(poses.size(), MaxPoses));
    put<std::int32_t>(out, count);
    for (int i = 0; i < count; ++i)
    {
        const auto& pose = poses[std::size_t(i)];
        putString(out, pose.string("id"));
        put<float>(out, float(pose.number("x")));
        put<float>(out, float(pose.number("y")));
        put<float>(out, float(pose.number("facing")));
        put<std::uint8_t>(out, pose.boolean("moving") ? 1 : 0);
    }
    return out;
}

Value unpack(const std::vector<std::uint8_t>& bytes)
{
    Reader in{bytes};
    if (in.get<std::uint32_t>() != Magic || in.bad)
        return {};
    auto root = Value::object();
    root.add("motionSession", in.string());
    root.add("observer", in.string());
    root.add("cellId", in.string());
    root.add("cellGeneration", in.get<std::int32_t>());
    root.add("revision", in.get<double>());
    root.add("time", in.get<double>());
    const auto count = in.get<std::int32_t>();
    if (in.bad || count < 0 || count > MaxPoses)
        return {};
    auto poses = Value::array();
    for (int i = 0; i < count; ++i)
    {
        auto pose = Value::object();
        pose.add("id", in.string());
        pose.add("x", double(in.get<float>()));
        pose.add("y", double(in.get<float>()));
        pose.add("facing", double(in.get<float>()));
        pose.add("moving", in.get<std::uint8_t>() != 0);
        if (in.bad)
            return {};
        poses.push(pose);
    }
    if (in.at != bytes.size())
        return {};
    root.add("entities", poses);
    return root;
}
} // namespace ratw::motion
