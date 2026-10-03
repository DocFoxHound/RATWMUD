#include "RatwSections.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace ratw::sections
{
namespace
{
struct Section
{
    const char* parent;
    const char* field;
    const char* name;
    bool entries;
    // Entries' fields that change from moment to moment: left out of an entry's key, and sent with each reference
    // to a held entry (the rest of it is sent only when it changes).
    const char* const* moving = nullptr;
};
// A visible wolf's pose and timers (doc 31, Phase 4): its details (name, looks, actions...) change seldom.
const char* const EntityMotion[] = {"x", "y", "facing", "turning", "moving", "postureRemaining", "speakingRemaining", nullptr};
bool isMoving(const Section& s, const std::string& field)
{
    for (auto* m = s.moving; m && *m; ++m)
        if (field == *m)
            return true;
    return false;
}
// Sent as row edits against a version the client holds, when that is much smaller: what the wolf can see, and the
// cell's ground and heights (unseen tiles are left blank, so every step that shows new ground changes them: whole,
// they were 67 KB a snapshot for a walking wolf in a city: doc 31, Phase 4).
bool deltaSection(const std::string& name) { return name == "visibility" || name == "cell.rows" || name == "cell.heights"; }
constexpr std::size_t BasesKept = 48;
const Section Sections[] = {{"", "visibility", "visibility", false}, {"cell", "rows", "cell.rows", false},
                            {"cell", "heights", "cell.heights", false}, {"", "worldMap", "worldMap", true},
                            {"", "travelMap", "travelMap", true}, {"", "doors", "doors", false},
                            {"", "inventory", "inventory", false}, {"", "entities", "entities", true, EntityMotion}};
constexpr std::size_t KeptPerSection = 6, EntriesKept = 4096;

json::Value* parentOf(json::Value& root, const Section& s)
{
    if (!*s.parent)
        return &root;
    auto* p = root.find(s.parent);
    return p && p->isObject() ? p : nullptr;
}

// Two 64-bit hashes walked straight over the value (types, strings, the bits of numbers), without writing it out as
// text first: keying sections was an eighth of the server's work that way (doc 31, Phase 4). A key is only ever
// compared with keys this server made.
struct Hash
{
    std::uint64_t a = 1469598103934665603ULL, b = 0x9E3779B97F4A7C15ULL;
    void byte(unsigned char c)
    {
        a = (a ^ c) * 1099511628211ULL;
        b = (b ^ c) * 0x100000001B3ULL + 0x632BE59BD9B4E019ULL;
    }
    void bytes(const void* data, std::size_t n)
    {
        const auto* p = static_cast<const unsigned char*>(data);
        for (std::size_t i = 0; i < n; ++i)
            byte(p[i]);
    }
    // Eight bytes at a time: a cell's rows run to tens of kilobytes a snapshot.
    void word(std::uint64_t w)
    {
        a = (a ^ w) * 0x100000001B3ULL;
        a ^= a >> 29;
        b = (b ^ w) * 0x9E3779B97F4A7C15ULL + 0x632BE59BD9B4E019ULL;
        b ^= b >> 31;
    }
    void text(const std::string& s)
    {
        word(std::uint64_t(s.size()));
        std::size_t i = 0;
        for (; i + 8 <= s.size(); i += 8)
        {
            std::uint64_t w;
            std::memcpy(&w, s.data() + i, 8);
            word(w);
        }
        std::uint64_t tail = 0;
        std::memcpy(&tail, s.data() + i, s.size() - i);
        word(tail);
    }
    void value(const json::Value& v)
    {
        byte(static_cast<unsigned char>(v.type()));
        switch (v.type())
        {
        case json::Value::Type::Null:
            break;
        case json::Value::Type::Bool:
            byte(v.asBool() ? 1 : 0);
            break;
        case json::Value::Type::Number:
        {
            const double d = v.asNumber();
            bytes(&d, sizeof d);
            break;
        }
        case json::Value::Type::String:
            text(v.asString());
            break;
        case json::Value::Type::List:
        {
            const auto n = std::uint64_t(v.items().size());
            bytes(&n, sizeof n);
            for (const auto& item : v.items())
                value(item);
            break;
        }
        case json::Value::Type::Object:
        {
            const auto n = std::uint64_t(v.fields().size());
            bytes(&n, sizeof n);
            for (const auto& [key, item] : v.fields())
            {
                text(key);
                value(item);
            }
            break;
        }
        }
    }
};

std::string keyOf(const json::Value& v, const Section* skipping = nullptr)
{
    Hash h;
    if (skipping && skipping->moving && v.isObject())
    {
        // An object's fields, but for those that move.
        for (const auto& [key, item] : v.fields())
            if (!isMoving(*skipping, key))
            {
                h.text(key);
                h.value(item);
            }
    }
    else
        h.value(v);
    char out[33];
    std::snprintf(out, sizeof out, "%016llx%016llx", static_cast<unsigned long long>(h.a), static_cast<unsigned long long>(h.b));
    return out;
}
} // namespace

void Bases::keep(const std::string& key, const json::Value& value)
{
    if (values.count(key))
        return;
    values.emplace(key, value);
    order.push_back(key);
    while (order.size() > BasesKept)
    {
        values.erase(order.front());
        order.erase(order.begin());
    }
}

const json::Value* Bases::find(const std::string& key) const
{
    const auto found = values.find(key);
    return found == values.end() ? nullptr : &found->second;
}

bool rowDelta(const json::Value& base, const json::Value& next, json::Value& edits)
{
    if (!base.isArray() || !next.isArray() || base.items().size() != next.items().size())
        return false;
    edits = json::Value::array();
    std::size_t whole = 0, changed = 0;
    for (std::size_t row = 0; row < next.items().size(); ++row)
    {
        const auto& a = base.items()[row];
        const auto& b = next.items()[row];
        if (!a.isString() || !b.isString())
            return false;
        const std::string& from = a.asString();
        const std::string& to = b.asString();
        whole += to.size() + 3;
        if (from == to)
            continue;
        if (from.size() != to.size())
            return false;
        std::size_t first = 0, last = to.size();
        while (first < last && from[first] == to[first])
            ++first;
        while (last > first && from[last - 1] == to[last - 1])
            --last;
        auto edit = json::Value::array();
        edit.push(double(row));
        edit.push(double(first));
        edit.push(to.substr(first, last - first));
        edits.push(edit);
        changed += last - first + 12;
    }
    return changed * 2 < whole;                    // Worth it only at under half the size.
}

bool applyRowDelta(const json::Value& base, const json::Value& edits, json::Value& out)
{
    if (!base.isArray() || !edits.isArray())
        return false;
    std::vector<std::string> rows;
    for (const auto& r : base.items())
    {
        if (!r.isString())
            return false;
        rows.push_back(r.asString());
    }
    for (const auto& e : edits.items())
    {
        if (!e.isArray() || e.items().size() != 3 || !e.items()[0].isNumber() || !e.items()[1].isNumber() || !e.items()[2].isString())
            return false;
        const double row = e.items()[0].asNumber(), column = e.items()[1].asNumber();
        const std::string& text = e.items()[2].asString();
        if (row < 0 || row >= double(rows.size()) || row != double(std::size_t(row)) || column < 0 || column != double(std::size_t(column)))
            return false;
        auto& target = rows[std::size_t(row)];
        if (std::size_t(column) + text.size() > target.size())
            return false;
        target.replace(std::size_t(column), text.size(), text);
    }
    out = json::Value::array();
    for (auto& r : rows)
        out.push(json::Value(std::move(r)));
    return true;
}

Keys strip(json::Value& root, const Keys& known, Bases* bases)
{
    Keys keys;
    auto keysJson = json::Value::object();
    for (const auto& s : Sections)
    {
        auto* parent = parentOf(root, s);
        auto* value = parent ? parent->find(s.field) : nullptr;
        if (!value)
            continue;
        std::string key;
        if (bases && value->storage() && !s.entries)
        {
            auto& last = bases->keys[s.name];
            if (last.first.storage() != value->storage())
                last = {*value, keyOf(*value)};
            key = last.second;
        }
        else
            key = keyOf(*value);
        keys[s.name] = key;
        keysJson.add(s.name, key);
        const auto held = known.find(s.name);
        const bool whole = held != known.end() && held->second == key;
        if (!s.entries || !value->isArray())
        {
            if (whole)
                parent->erase(s.field);
            else if (bases && deltaSection(s.name))
            {
                const json::Value next = *value;
                if (held != known.end())
                    if (const auto* base = bases->find(held->second))
                        if (json::Value edits; rowDelta(*base, next, edits))
                        {
                            auto delta = json::Value::object();
                            delta.add("$delta", held->second);
                            delta.add("edits", edits);
                            *value = delta;
                        }
                bases->keep(key, next);
            }
            continue;
        }
        const std::string prefix = std::string(s.name) + "#";
        if (whole)
        {
            for (const auto& [name, k] : known)
                if (name.rfind(prefix, 0) == 0)
                    keys[name] = k;
            parent->erase(s.field);
            continue;
        }
        auto sent = json::Value::array();
        bool changed = false;
        for (const auto& entry : value->items())
        {
            const auto* id = entry.find("id");
            if (!id || !id->isString())
            {
                sent.push(entry);
                continue;
            }
            const std::string name = prefix + id->asString(), entryKey = keyOf(entry, &s);
            keys[name] = entryKey;
            if (const auto h = known.find(name); h != known.end() && h->second == entryKey)
            {
                auto reference = json::Value::object();
                reference.add("$held", entryKey);
                for (const auto& [field, item] : entry.fields())
                    if (isMoving(s, field))
                        reference.add(field, item);
                sent.push(reference);
                changed = true;
            }
            else
            {
                keysJson.add(name, entryKey);
                sent.push(entry);
            }
        }
        if (changed)
            *value = sent;
    }
    root.set("sectionKeys", keysJson);
    return keys;
}

void Held::sending(double revision, Keys keys)
{
    sent.emplace(revision, std::move(keys));
    while (sent.size() > 32)
        sent.erase(sent.begin());
}

void Held::acknowledged(double revision, bool missing)
{
    if (missing)
    {
        reset();
        return;
    }
    const auto found = sent.find(revision);
    if (found == sent.end())
        return;                                    // Older than one already acknowledged (or never sent).
    known = found->second;
    sent.erase(sent.begin(), std::next(found));
}

bool fill(json::Value& root, Cache& cache)
{
    const auto* keysPtr = root.find("sectionKeys");
    if (!keysPtr || !keysPtr->isObject())
        return true;
    const json::Value keys = *keysPtr;
    bool complete = true;
    for (const auto& s : Sections)
    {
        const auto* keyValue = keys.find(s.name);
        if (!keyValue || !keyValue->isString())
            continue;
        const std::string key = keyValue->asString();
        auto* parent = parentOf(root, s);
        if (!parent)
        {
            complete = false;
            continue;
        }
        auto& kept = cache.kept[s.name];
        if (auto* value = parent->find(s.field))
        {
            if (const auto* base = value->isObject() ? value->find("$delta") : nullptr)
            {
                const auto found = base->isString()
                    ? std::find_if(kept.begin(), kept.end(), [&](const auto& p) { return p.first == base->asString(); })
                    : kept.end();
                const auto* edits = value->find("edits");
                json::Value whole;
                if (found == kept.end() || !edits || !applyRowDelta(found->second, *edits, whole))
                {
                    complete = false;
                    continue;
                }
                *value = whole;
            }
            if (s.entries && value->isArray())
            {
                auto whole = json::Value::array();
                bool referenced = false;
                for (const auto& entry : value->items())
                {
                    if (const auto* held = entry.find("$held"); held && held->isString())
                    {
                        const auto found = cache.entries.find(held->asString());
                        if (found == cache.entries.end())
                            return false;
                        // The held entry, with what moved since sent alongside.
                        auto filled = json::Value::object();
                        for (const auto& [field, item] : found->second.fields())
                            if (!entry.has(field))
                                filled.add(field, item);
                        for (const auto& [field, item] : entry.fields())
                            if (field != "$held")
                                filled.add(field, item);
                        whole.push(filled);
                        referenced = true;
                        continue;
                    }
                    if (const auto* id = entry.find("id"); id && id->isString())
                        if (const auto* entryKey = keys.find(std::string(s.name) + "#" + id->asString()); entryKey && entryKey->isString())
                            if (cache.entries.emplace(entryKey->asString(), entry).second)
                            {
                                cache.entryOrder.push_back(entryKey->asString());
                                if (cache.entryOrder.size() > EntriesKept)
                                {
                                    cache.entries.erase(cache.entryOrder.front());
                                    cache.entryOrder.erase(cache.entryOrder.begin());
                                }
                            }
                    whole.push(entry);
                }
                if (referenced)
                    *value = whole;
            }
            kept.erase(std::remove_if(kept.begin(), kept.end(), [&](const auto& p) { return p.first == key; }), kept.end());
            kept.emplace_back(key, *value);
            if (kept.size() > KeptPerSection)
                kept.erase(kept.begin());
            continue;
        }
        const auto found = std::find_if(kept.begin(), kept.end(), [&](const auto& p) { return p.first == key; });
        if (found == kept.end())
        {
            complete = false;
            continue;
        }
        parent->set(s.field, found->second);
    }
    root.erase("sectionKeys");
    return complete;
}
} // namespace ratw::sections
