#include "RatwPack.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace ratw::pack
{
using json::Value;

namespace
{
enum Tag : std::uint8_t
{
    Null = 0,
    False = 1,
    True = 2,
    Whole = 3,
    Float32 = 4,
    Float64 = 5,
    String = 6,
    List = 7,
    Object = 8,
};

struct Writer
{
    std::string out;
    std::unordered_map<std::string, std::uint64_t> table;

    void varint(std::uint64_t n)
    {
        while (n >= 0x80)
        {
            out += char(n & 0x7f | 0x80);
            n >>= 7;
        }
        out += char(n);
    }
    void text(const std::string& s)
    {
        if (const auto found = table.find(s); found != table.end())
        {
            varint(found->second * 2 + 1);
            return;
        }
        varint(std::uint64_t(s.size()) * 2);
        out += s;
        table.emplace(s, table.size());
    }
    void number(double d)
    {
        // Whole numbers a double holds exactly as varints; the rest as float32 where that loses nothing.
        if (std::isfinite(d) && d == std::floor(d) && std::abs(d) <= 9007199254740992.0)
        {
            out += char(Whole);
            const auto i = static_cast<std::int64_t>(d);
            varint((static_cast<std::uint64_t>(i) << 1) ^ static_cast<std::uint64_t>(i >> 63));
            return;
        }
        const float f = static_cast<float>(d);
        if (static_cast<double>(f) == d)
        {
            out += char(Float32);
            out.append(reinterpret_cast<const char*>(&f), 4);
            return;
        }
        out += char(Float64);
        out.append(reinterpret_cast<const char*>(&d), 8);
    }
    void value(const Value& v)
    {
        switch (v.type())
        {
        case Value::Type::Null:
            out += char(Null);
            break;
        case Value::Type::Bool:
            out += char(v.asBool() ? True : False);
            break;
        case Value::Type::Number:
            number(v.asNumber());
            break;
        case Value::Type::String:
            out += char(String);
            text(v.asString());
            break;
        case Value::Type::List:
            out += char(List);
            varint(v.items().size());
            for (const auto& item : v.items())
                value(item);
            break;
        case Value::Type::Object:
            out += char(Object);
            varint(v.fields().size());
            for (const auto& [key, item] : v.fields())
            {
                text(key);
                value(item);
            }
            break;
        }
    }
};

struct Reader
{
    const std::string& in;
    std::size_t at = 0;
    std::vector<std::string> table;

    bool varint(std::uint64_t& n)
    {
        n = 0;
        for (int shift = 0; shift < 64; shift += 7)
        {
            if (at >= in.size())
                return false;
            const auto b = static_cast<std::uint8_t>(in[at++]);
            n |= std::uint64_t(b & 0x7f) << shift;
            if (!(b & 0x80))
                return true;
        }
        return false;
    }
    bool text(std::string& s)
    {
        std::uint64_t n;
        if (!varint(n))
            return false;
        if (n & 1)
        {
            if ((n >> 1) >= table.size())
                return false;
            s = table[std::size_t(n >> 1)];
            return true;
        }
        const auto length = n >> 1;
        if (length > in.size() - at)
            return false;
        s.assign(in, at, std::size_t(length));
        at += std::size_t(length);
        table.push_back(s);
        return true;
    }
    bool value(Value& v, int depth)
    {
        if (depth > 256 || at >= in.size())
            return false;
        switch (static_cast<std::uint8_t>(in[at++]))
        {
        case Null:
            v = Value();
            return true;
        case False:
            v = false;
            return true;
        case True:
            v = true;
            return true;
        case Whole:
        {
            std::uint64_t z;
            if (!varint(z))
                return false;
            v = double(static_cast<std::int64_t>((z >> 1) ^ (~(z & 1) + 1)));
            return true;
        }
        case Float32:
        {
            if (in.size() - at < 4)
                return false;
            float f;
            std::memcpy(&f, in.data() + at, 4);
            at += 4;
            v = double(f);
            return true;
        }
        case Float64:
        {
            if (in.size() - at < 8)
                return false;
            double d;
            std::memcpy(&d, in.data() + at, 8);
            at += 8;
            v = d;
            return true;
        }
        case String:
        {
            std::string s;
            if (!text(s))
                return false;
            v = std::move(s);
            return true;
        }
        case List:
        {
            std::uint64_t n;
            if (!varint(n) || n > in.size() - at)
                return false;
            v = Value::array();
            for (std::uint64_t i = 0; i < n; ++i)
            {
                Value item;
                if (!value(item, depth + 1))
                    return false;
                v.push(std::move(item));
            }
            return true;
        }
        case Object:
        {
            std::uint64_t n;
            if (!varint(n) || n > in.size() - at)
                return false;
            v = Value::object();
            for (std::uint64_t i = 0; i < n; ++i)
            {
                std::string key;
                Value item;
                if (!text(key) || !value(item, depth + 1))
                    return false;
                v.set(key, std::move(item));
            }
            return true;
        }
        default:
            return false;
        }
    }
};
} // namespace

std::string encode(const Value& v)
{
    Writer w;
    w.value(v);
    return std::move(w.out);
}

bool decode(const std::string& bytes, Value& out)
{
    Reader r{bytes};
    return r.value(out, 0) && r.at == bytes.size();
}
} // namespace ratw::pack
