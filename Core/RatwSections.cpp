#include "RatwSections.h"

#include <algorithm>
#include <cstdio>

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
};
const Section Sections[] = {{"", "visibility", "visibility", false}, {"cell", "rows", "cell.rows", false},
                            {"cell", "heights", "cell.heights", false}, {"", "worldMap", "worldMap", true},
                            {"", "travelMap", "travelMap", true}, {"", "doors", "doors", false},
                            {"", "inventory", "inventory", false}};
constexpr std::size_t KeptPerSection = 6, EntriesKept = 4096;

json::Value* parentOf(json::Value& root, const Section& s)
{
    if (!*s.parent)
        return &root;
    auto* p = root.find(s.parent);
    return p && p->isObject() ? p : nullptr;
}

// FNV-1a over the text, both halves: a key is only ever compared with keys this server made.
std::string keyOf(const json::Value& v)
{
    const std::string text = json::dump(v);
    std::uint64_t a = 1469598103934665603ULL, b = 0x9E3779B97F4A7C15ULL;
    for (unsigned char c : text)
    {
        a = (a ^ c) * 1099511628211ULL;
        b = (b ^ c) * 0x100000001B3ULL + 0x632BE59BD9B4E019ULL;
    }
    char out[33];
    std::snprintf(out, sizeof out, "%016llx%016llx", static_cast<unsigned long long>(a), static_cast<unsigned long long>(b));
    return out;
}
} // namespace

Keys strip(json::Value& root, const Keys& known)
{
    Keys keys;
    auto keysJson = json::Value::object();
    for (const auto& s : Sections)
    {
        auto* parent = parentOf(root, s);
        auto* value = parent ? parent->find(s.field) : nullptr;
        if (!value)
            continue;
        const std::string key = keyOf(*value);
        keys[s.name] = key;
        keysJson.add(s.name, key);
        const auto held = known.find(s.name);
        const bool whole = held != known.end() && held->second == key;
        if (!s.entries || !value->isArray())
        {
            if (whole)
                parent->erase(s.field);
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
            const std::string name = prefix + id->asString(), entryKey = keyOf(entry);
            keys[name] = entryKey;
            if (const auto h = known.find(name); h != known.end() && h->second == entryKey)
            {
                auto reference = json::Value::object();
                reference.add("$held", entryKey);
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
                        whole.push(found->second);
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
