#include "RatwJsonDoc.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace ratw::json
{
namespace
{
const std::string EmptyString;
const Array EmptyArray;
const std::vector<Field> EmptyFields;
const Value NullValue;
constexpr std::size_t IndexFrom = 16;              // Objects with more fields than this keep an index of them.
constexpr int MaxDepth = 256;
} // namespace

Value::Value(Array a) : type_(Type::List), array_(std::make_shared<Array>(std::move(a))) {}

Value Value::object()
{
    Value v;
    v.type_ = Type::Object;
    v.object_ = std::make_shared<std::vector<Field>>();
    return v;
}

Value Value::array()
{
    Value v;
    v.type_ = Type::List;
    v.array_ = std::make_shared<Array>();
    return v;
}

const std::string& Value::asString() const { return type_ == Type::String ? string_ : EmptyString; }
const Array& Value::items() const { return type_ == Type::List && array_ ? *array_ : EmptyArray; }
const std::vector<Field>& Value::fields() const { return type_ == Type::Object && object_ ? *object_ : EmptyFields; }
const Array& Value::array(const std::string& key) const { const auto* v = find(key); return v ? v->items() : EmptyArray; }
const Value& Value::object(const std::string& key) const { const auto* v = find(key); return v && v->isObject() ? *v : NullValue; }

void Value::own()
{
    if (array_ && array_.use_count() > 1)
        array_ = std::make_shared<Array>(*array_);
    if (object_ && object_.use_count() > 1)
    {
        object_ = std::make_shared<std::vector<Field>>(*object_);
        index_.reset();
    }
    if (index_ && index_.use_count() > 1)
        index_ = std::make_shared<std::map<std::string, std::size_t>>(*index_);
}

void Value::reindex()
{
    if (!object_ || object_->size() <= IndexFrom)
    {
        index_.reset();
        return;
    }
    index_ = std::make_shared<std::map<std::string, std::size_t>>();
    for (std::size_t i = 0; i < object_->size(); ++i)
        (*index_)[(*object_)[i].first] = i;
}

Array& Value::items()
{
    if (type_ != Type::List)
        *this = array();
    own();
    return *array_;
}

const Value* Value::find(const std::string& key) const
{
    if (type_ != Type::Object || !object_)
        return nullptr;
    if (index_)
    {
        const auto at = index_->find(key);
        return at == index_->end() ? nullptr : &(*object_)[at->second].second;
    }
    for (const auto& [k, v] : *object_)
        if (k == key)
            return &v;
    return nullptr;
}

Value* Value::find(const std::string& key)
{
    if (type_ != Type::Object || !object_)
        return nullptr;
    own();
    return const_cast<Value*>(static_cast<const Value*>(this)->find(key));
}

const Value& Value::operator[](const std::string& key) const
{
    const auto* v = find(key);
    return v ? *v : NullValue;
}

Value& Value::set(const std::string& key, Value v)
{
    if (type_ != Type::Object)
        *this = object();
    own();
    if (auto* existing = find(key))
        return *existing = std::move(v);
    return add(key, std::move(v));
}

Value& Value::add(std::string key, Value v)
{
    if (type_ != Type::Object)
        *this = object();
    own();
    object_->emplace_back(std::move(key), std::move(v));
    if (index_)
        (*index_)[object_->back().first] = object_->size() - 1;
    else if (object_->size() > IndexFrom)
        reindex();
    return object_->back().second;
}

bool Value::erase(const std::string& key)
{
    if (type_ != Type::Object || !object_)
        return false;
    own();
    const auto at = std::find_if(object_->begin(), object_->end(), [&](const Field& f) { return f.first == key; });
    if (at == object_->end())
        return false;
    object_->erase(at);
    reindex();
    return true;
}

std::size_t Value::size() const
{
    return type_ == Type::Object && object_ ? object_->size() : type_ == Type::List && array_ ? array_->size() : 0;
}

Value& Value::push(Value v)
{
    auto& list = items();
    list.push_back(std::move(v));
    return list.back();
}

bool Value::operator==(const Value& o) const
{
    if (type_ != o.type_)
        return false;
    switch (type_)
    {
    case Type::Null: return true;
    case Type::Bool: return bool_ == o.bool_;
    case Type::Number: return number_ == o.number_;
    case Type::String: return string_ == o.string_;
    case Type::List: return items() == o.items();
    case Type::Object:
    {
        if (size() != o.size())
            return false;
        for (const auto& [k, v] : fields())
        {
            const auto* other = o.find(k);
            if (!other || !(*other == v))
                return false;
        }
        return true;
    }
    }
    return false;
}

// --------------------------------------------------------------------------- Writing

namespace
{
void writeString(std::string& out, const std::string& s)
{
    out += '"';
    for (unsigned char c : s)
    {
        switch (c)
        {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c < 0x20)
            {
                char buf[8];
                std::snprintf(buf, sizeof buf, "\\u%04x", c);
                out += buf;
            }
            else
                out += char(c);
        }
    }
    out += '"';
}

void writeNumber(std::string& out, double n)
{
    if (!std::isfinite(n))
    {
        out += "null";                             // JSON has no infinities; Unreal writes these as null too.
        return;
    }
    if (n == std::floor(n) && std::fabs(n) < 9007199254740992.0)
    {
        char buf[32];
        const auto r = std::to_chars(buf, buf + sizeof buf, static_cast<long long>(n));
        out.append(buf, r.ptr);                    // (-0 prints as 0.)
        return;
    }
    char buf[40];
    const auto r = std::to_chars(buf, buf + sizeof buf, n);   // Shortest form that reads back exactly.
    out.append(buf, r.ptr);
}

void write(std::string& out, const Value& v)
{
    switch (v.type())
    {
    case Value::Type::Null: out += "null"; break;
    case Value::Type::Bool: out += v.asBool() ? "true" : "false"; break;
    case Value::Type::Number: writeNumber(out, v.asNumber()); break;
    case Value::Type::String: writeString(out, v.asString()); break;
    case Value::Type::List:
    {
        out += '[';
        bool first = true;
        for (const auto& item : v.items())
        {
            if (!first) out += ',';
            first = false;
            write(out, item);
        }
        out += ']';
        break;
    }
    case Value::Type::Object:
    {
        out += '{';
        bool first = true;
        for (const auto& [k, item] : v.fields())
        {
            if (!first) out += ',';
            first = false;
            writeString(out, k);
            out += ':';
            write(out, item);
        }
        out += '}';
        break;
    }
    }
}
} // namespace

std::string dump(const Value& v)
{
    std::string out;
    write(out, v);
    return out;
}

// --------------------------------------------------------------------------- Reading

namespace
{
struct Reader
{
    const char* p;
    const char* end;
    std::string error;

    bool fail(const char* why)
    {
        if (error.empty())
            error = why;
        return false;
    }
    void space()
    {
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r'))
            ++p;
    }
    bool literal(const char* word)
    {
        const auto n = std::strlen(word);
        if (std::size_t(end - p) < n || std::memcmp(p, word, n) != 0)
            return fail("Unexpected text.");
        p += n;
        return true;
    }
    static void utf8(std::string& out, std::uint32_t cp)
    {
        if (cp < 0x80) out += char(cp);
        else if (cp < 0x800) { out += char(0xC0 | (cp >> 6)); out += char(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) { out += char(0xE0 | (cp >> 12)); out += char(0x80 | ((cp >> 6) & 0x3F)); out += char(0x80 | (cp & 0x3F)); }
        else { out += char(0xF0 | (cp >> 18)); out += char(0x80 | ((cp >> 12) & 0x3F)); out += char(0x80 | ((cp >> 6) & 0x3F)); out += char(0x80 | (cp & 0x3F)); }
    }
    bool hex4(std::uint32_t& cp)
    {
        if (end - p < 4)
            return fail("Short \\u escape.");
        cp = 0;
        for (int i = 0; i < 4; ++i, ++p)
        {
            const char c = *p;
            cp <<= 4;
            if (c >= '0' && c <= '9') cp |= std::uint32_t(c - '0');
            else if (c >= 'a' && c <= 'f') cp |= std::uint32_t(c - 'a' + 10);
            else if (c >= 'A' && c <= 'F') cp |= std::uint32_t(c - 'A' + 10);
            else return fail("Bad \\u escape.");
        }
        return true;
    }
    bool string(std::string& out)
    {
        ++p;                                       // The opening quote.
        while (p < end)
        {
            const unsigned char c = static_cast<unsigned char>(*p++);
            if (c == '"')
                return true;
            if (c < 0x20)
                return fail("Control character in a string.");
            if (c != '\\')
            {
                out += char(c);
                continue;
            }
            if (p >= end)
                break;
            switch (*p++)
            {
            case '"': out += '"'; break;
            case '\\': out += '\\'; break;
            case '/': out += '/'; break;
            case 'b': out += '\b'; break;
            case 'f': out += '\f'; break;
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case 'u':
            {
                std::uint32_t cp = 0;
                if (!hex4(cp))
                    return false;
                if (cp >= 0xD800 && cp <= 0xDBFF)
                {
                    std::uint32_t low = 0;
                    if (end - p < 6 || p[0] != '\\' || p[1] != 'u')
                        return fail("Unpaired surrogate.");
                    p += 2;
                    if (!hex4(low) || low < 0xDC00 || low > 0xDFFF)
                        return fail("Unpaired surrogate.");
                    cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                }
                else if (cp >= 0xDC00 && cp <= 0xDFFF)
                    return fail("Unpaired surrogate.");
                utf8(out, cp);
                break;
            }
            default: return fail("Bad escape.");
            }
        }
        return fail("Unterminated string.");
    }
    bool number(double& out)
    {
        const char* start = p;
        if (p < end && *p == '-') ++p;
        if (p >= end) return fail("Bad number.");
        if (*p == '0') ++p;
        else if (*p >= '1' && *p <= '9') while (p < end && *p >= '0' && *p <= '9') ++p;
        else return fail("Bad number.");
        if (p < end && *p == '.')
        {
            ++p;
            if (p >= end || *p < '0' || *p > '9') return fail("Bad number.");
            while (p < end && *p >= '0' && *p <= '9') ++p;
        }
        if (p < end && (*p == 'e' || *p == 'E'))
        {
            ++p;
            if (p < end && (*p == '+' || *p == '-')) ++p;
            if (p >= end || *p < '0' || *p > '9') return fail("Bad number.");
            while (p < end && *p >= '0' && *p <= '9') ++p;
        }
        const auto r = std::from_chars(start, p, out);
        if (r.ec != std::errc() || !std::isfinite(out))
            return fail("Bad number.");
        return true;
    }
    bool value(Value& out, int depth)
    {
        if (depth > MaxDepth)
            return fail("Nested too deeply.");
        space();
        if (p >= end)
            return fail("Unexpected end.");
        switch (*p)
        {
        case 'n': out = nullptr; return literal("null");
        case 't': out = true; return literal("true");
        case 'f': out = false; return literal("false");
        case '"': { std::string s; if (!string(s)) return false; out = std::move(s); return true; }
        case '[':
        {
            ++p;
            out = Value::array();
            auto& list = out.items();
            space();
            if (p < end && *p == ']') { ++p; return true; }
            for (;;)
            {
                list.emplace_back();
                if (!value(list.back(), depth + 1)) return false;
                space();
                if (p < end && *p == ',') { ++p; continue; }
                if (p < end && *p == ']') { ++p; return true; }
                return fail("Expected , or ] in an array.");
            }
        }
        case '{':
        {
            ++p;
            out = Value::object();
            space();
            if (p < end && *p == '}') { ++p; return true; }
            for (;;)
            {
                space();
                if (p >= end || *p != '"') return fail("Expected a key.");
                std::string key;
                if (!string(key)) return false;
                space();
                if (p >= end || *p != ':') return fail("Expected :.");
                ++p;
                Value item;
                if (!value(item, depth + 1)) return false;
                out.set(key, std::move(item));
                space();
                if (p < end && *p == ',') { ++p; continue; }
                if (p < end && *p == '}') { ++p; return true; }
                return fail("Expected , or } in an object.");
            }
        }
        default:
        {
            double n = 0;
            if (!number(n)) return false;
            out = n;
            return true;
        }
        }
    }
};
} // namespace

bool parse(const std::string& text, Value& out, std::string& error)
{
    Reader r{text.data(), text.data() + text.size(), {}};
    Value v;
    if (!r.value(v, 0))
    {
        error = r.error;
        return false;
    }
    r.space();
    if (r.p != r.end)
    {
        error = "Text after the value.";
        return false;
    }
    out = std::move(v);
    return true;
}
} // namespace ratw::json
