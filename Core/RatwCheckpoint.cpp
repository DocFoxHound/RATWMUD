#include "RatwCheckpoint.h"

#include "RatwSystemLibs.h"
#include "RatwWire.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace ratw::checkpoint
{
using json::Value;

namespace
{
Value strings(const std::vector<std::string>& list)
{
    auto out = Value::array();
    for (const auto& s : list)
        out.push(s);
    return out;
}
std::vector<std::string> readStrings(const Value& o, const char* key)
{
    std::vector<std::string> out;
    for (const auto& item : o.array(key))
        if (item.isString())
            out.push_back(item.asString());
    return out;
}
double num(const Value& o, const char* key, double fallback = 0) { return wire::number(o, key, fallback); }

// Map memories compactly (Docs/Design/31-responsiveness.md, Phase 2): a remembered cell's glyphs, and its observed
// tiles packed eight to a byte, each deflated and in base64. A 256x256 cell a wolf has barely seen was 128 KB of
// spaces and '0's; it is now a few hundred bytes. Saves written before this ("glyphs", "observed") still read.
const char Base64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string toBase64(const std::vector<std::uint8_t>& in)
{
    std::string out;
    out.reserve((in.size() + 2) / 3 * 4);
    for (std::size_t i = 0; i < in.size(); i += 3)
    {
        const std::uint32_t n = std::uint32_t(in[i]) << 16 | (i + 1 < in.size() ? std::uint32_t(in[i + 1]) << 8 : 0) |
                                (i + 2 < in.size() ? in[i + 2] : 0);
        out += Base64[n >> 18 & 63];
        out += Base64[n >> 12 & 63];
        out += i + 1 < in.size() ? Base64[n >> 6 & 63] : '=';
        out += i + 2 < in.size() ? Base64[n & 63] : '=';
    }
    return out;
}

bool fromBase64(const std::string& in, std::vector<std::uint8_t>& out)
{
    out.clear();
    std::uint32_t bits = 0;
    int held = 0;
    for (char ch : in)
    {
        if (ch == '=')
            break;
        const char* at = std::strchr(Base64, ch);
        if (!at || !*at)
            return false;
        bits = bits << 6 | std::uint32_t(at - Base64);
        if ((held += 6) >= 8)
        {
            held -= 8;
            out.push_back(std::uint8_t(bits >> held & 255));
        }
    }
    return true;
}

// Deflated and in base64; "" if zlib isn't there (the caller then writes the old, plain form).
std::string packed(const std::vector<std::uint8_t>& raw)
{
    std::vector<std::uint8_t> out;
    if (raw.empty() || !sys::compress(raw.data(), raw.size(), out))
        return {};
    return toBase64(out);
}

bool unpacked(const Value& j, const char* key, std::size_t length, std::vector<std::uint8_t>& raw)
{
    std::vector<std::uint8_t> deflated;
    return fromBase64(j.string(key), deflated) && sys::uncompress(deflated.data(), deflated.size(), length, raw) && raw.size() == length;
}
} // namespace

Value roads(const RoadsState& r)
{
    auto roads = Value::object();
    roads.add("day", double(r.day));
    roads.add("nextId", double(r.nextId));
    roads.add("stocked", r.stocked);
    auto caravans = Value::array(), camps = Value::array(), contracts = Value::array();
    for (const auto& t : r.caravans)
    {
        auto j = Value::object();
        j.add("id", t.id); j.add("from", t.from); j.add("to", t.to); j.add("account", t.account);
        j.add("route", strings(t.route)); j.add("leg", double(t.leg));
        j.add("nextAt", t.nextAt); j.add("departed", t.departed);
        j.add("guards", t.guards); j.add("status", t.status);
        j.add("escorts", strings(t.escorts)); j.add("letters", strings(t.letters));
        j.add("cell", t.cell); j.add("x", t.x); j.add("y", t.y);
        j.add("waitUntil", t.waitUntil);
        auto with = Value::object();
        for (const auto& [who, cells] : t.with)
            with.add(who, cells);
        j.add("with", with);
        caravans.push(j);
    }
    for (const auto& b : r.camps)
    {
        auto j = Value::object();
        j.add("id", b.id); j.add("cell", b.cell); j.add("strength", b.strength);
        j.add("hunger", b.hunger); j.add("lastRaid", b.lastRaid); j.add("active", b.active);
        j.add("x", b.x); j.add("y", b.y);
        camps.push(j);
    }
    for (const auto& k : r.contracts)
    {
        auto j = Value::object();
        j.add("id", k.id); j.add("kind", k.kind); j.add("poster", k.poster); j.add("town", k.town);
        j.add("target", k.target); j.add("taker", k.taker); j.add("status", k.status);
        j.add("reward", double(k.reward)); j.add("created", k.created);
        j.add("due", k.due); j.add("detail", k.detail);
        contracts.push(j);
    }
    roads.add("caravans", caravans); roads.add("camps", camps); roads.add("contracts", contracts);
    return roads;
}

Value crime(const CrimeState& k)
{
    // Crime and law (RatwCrime.h): incidents and who saw them, warrants, and who is held in a gaol.
    auto crime = Value::object();
    crime.add("nextIncident", k.nextIncident);
    crime.add("day", k.day);
    auto incidents = Value::array();
    for (const auto& i : k.incidents)
    {
        auto j = Value::object();
        j.add("id", i.id); j.add("kind", i.kind); j.add("offender", i.offender); j.add("victim", i.victim);
        j.add("cell", i.cell); j.add("town", i.town); j.add("time", i.time); j.add("day", i.day);
        j.add("item", i.item); j.add("quantity", i.quantity); j.add("coins", i.coins); j.add("status", i.status);
        auto witnesses = Value::array();
        for (const auto& w : i.witnesses)
        {
            auto k = Value::object();
            k.add("id", w.id); k.add("identified", w.identified); k.add("clarity", w.clarity); k.add("reported", w.reported);
            witnesses.push(k);
        }
        j.add("witnesses", witnesses);
        incidents.push(j);
    }
    crime.add("incidents", incidents);
    auto warrants = Value::array();
    for (const auto& w : k.warrants)
    {
        auto j = Value::object();
        j.add("person", w.person); j.add("town", w.town); j.add("fine", w.fine); j.add("since", w.since);
        auto ids = Value::array();
        for (const auto& id : w.incidents)
            ids.push(id);
        j.add("incidents", ids);
        auto owed = Value::array();
        for (const auto& r : w.restitution)
        {
            auto k = Value::object();
            k.add("to", r.to); k.add("item", r.item); k.add("quantity", r.quantity); k.add("coins", r.coins);
            owed.push(k);
        }
        j.add("restitution", owed);
        warrants.push(j);
    }
    crime.add("warrants", warrants);
    auto custody = Value::array();
    for (const auto& held : k.custody)
    {
        auto j = Value::object();
        j.add("person", held.person); j.add("town", held.town); j.add("cell", held.cell);
        j.add("x", held.x); j.add("y", held.y); j.add("until", held.until);
        custody.push(j);
    }
    crime.add("custody", custody);
    return crime;
}

Value encode(const PersistedWorld& saved, const ServerState& c, const std::vector<Entity>& npcs, double time)
{
    auto root = Value::object();
    root.add("schema", 1);
    root.add("accounts", c.accounts.isNull() ? Value::object() : c.accounts);
    root.add("sequence", c.sequence);
    root.add("revision", c.revision);
    root.add("director", c.director.isNull() ? Value::object() : c.director);
    root.add("time", saved.time);
    root.add("clockOffsetHours", saved.clockOffsetHours);
    root.add("calendarDays", saved.calendarDays);
    root.add("society", wire::society(saved.society));
    auto modes = Value::object();
    for (const auto& [cell, on] : saved.seasonalWeather)
        modes.add(cell, on);
    root.add("seasonalWeather", modes);
    auto players = Value::array();
    for (const auto& [id, e] : c.characters)
        players.push(wire::persistEntity(e, time));
    root.add("players", players);
    auto doors = Value::object();
    for (const auto& [id, open] : saved.doorStates)
        doors.add(id, open);
    root.add("doors", doors);
    auto seen = Value::array();
    for (const auto& [observer, cells] : saved.memories)
        for (const auto& [id, m] : cells)
        {
            auto j = Value::object();
            j.add("observer", observer);
            j.add("id", m.cellId);
            j.add("name", m.name);
            j.add("knowledge", int(m.knowledge));
            j.add("width", m.width);
            j.add("height", m.height);
            j.add("x", m.worldX);
            j.add("y", m.worldY);
            j.add("z", m.worldZ);
            std::vector<std::uint8_t> bits((m.observed.size() + 7) / 8);
            for (std::size_t i = 0; i < m.observed.size(); ++i)
                if (m.observed[i])
                    bits[i / 8] |= std::uint8_t(1u << (i % 8));
            const auto glyphs = packed(std::vector<std::uint8_t>(m.glyphs.begin(), m.glyphs.end()));
            const auto observed = packed(bits);
            if ((m.glyphs.empty() || !glyphs.empty()) && (bits.empty() || !observed.empty()))
            {
                j.add("tiles", double(m.glyphs.size()));
                j.add("glyphsZ", glyphs);
                j.add("seenTiles", double(m.observed.size()));
                j.add("observedZ", observed);
            }
            else
            {
                j.add("glyphs", std::string(m.glyphs.begin(), m.glyphs.end()));
                std::string plain;
                plain.reserve(m.observed.size());
                for (bool b : m.observed)
                    plain += b ? '1' : '0';
                j.add("observed", plain);
            }
            seen.push(j);
        }
    root.add("mapMemories", seen);
    auto bonds = Value::array();
    for (const auto& b : saved.bonds)
    {
        auto j = Value::object();
        j.add("holder", b.holder);
        j.add("other", b.other);
        j.add("affinity", b.bond.affinity);
        j.add("trust", b.bond.trust);
        j.add("familiarity", b.bond.familiarity);
        j.add("fear", b.bond.fear);
        j.add("respect", b.bond.respect);
        j.add("owed", double(b.bond.owed));
        j.add("lastContact", b.bond.lastContact);
        bonds.push(j);
    }
    root.add("bonds", bonds);
    auto promises = Value::array();
    for (const auto& p : saved.promises)
    {
        auto j = Value::object();
        j.add("by", p.by); j.add("to", p.to); j.add("what", p.what);
        j.add("made", p.made); j.add("due", p.due); j.add("status", p.status);
        promises.push(j);
    }
    root.add("promises", promises);
    root.add("roads", checkpoint::roads(saved.roads));
    auto beliefs = Value::array();
    for (const auto& h : saved.roads.beliefs)
    {
        auto j = Value::object();
        j.add("holder", h.holder); j.add("subject", h.subject); j.add("claim", h.claim);
        j.add("source", h.source); j.add("confidence", h.confidence); j.add("day", h.day);
        if (!h.incident.empty())
            j.add("incident", h.incident);
        beliefs.push(j);
    }
    root.add("beliefs", beliefs);
    root.add("crime", checkpoint::crime(saved.crime));
    // Festivals the Dungeon Master called (Phase 9).
    auto festivals = Value::array();
    for (const auto& f : saved.festivals)
    {
        auto j = Value::object();
        j.add("community", f.community); j.add("name", f.name); j.add("day", double(f.day));
        festivals.push(j);
    }
    root.add("festivals", festivals);
    auto npcList = Value::array();
    for (const auto& e : npcs)
        npcList.push(wire::persistEntity(e, time));
    root.add("npcs", npcList);
    auto companions = Value::object();
    for (const auto& [npc, owner] : c.companions)
        if (!owner.empty())
            companions.add(npc, owner);
    // Weather fronts called up by a DM or developer (doc 29, phase 7); the world's own weather is worked out again.
    auto fronts = Value::array();
    for (const auto& f : saved.fronts)
    {
        auto j = Value::object();
        j.add("id", f.id);
        j.add("kind", int(f.kind));
        j.add("x", f.x);
        j.add("y", f.y);
        j.add("radius", f.radius);
        j.add("peak", f.peak);
        j.add("vx", f.vx);
        j.add("vy", f.vy);
        j.add("born", f.born);
        j.add("life", f.life);
        fronts.push(j);
    }
    root.add("weatherFronts", fronts);
    root.add("companions", companions);
    if (!c.parties.isNull())
        root.add("parties", c.parties);
    if (!c.acquaintances.isNull())
        root.add("acquaintances", c.acquaintances);
    if (!c.aliases.isNull())
        root.add("aliases", c.aliases);
    if (!c.notes.isNull())
        root.add("notes", c.notes);
    if (!c.chapters.isNull())
        root.add("chapters", c.chapters);
    if (!c.factions.isNull())
        root.add("factions", c.factions);
    if (!c.estates.isNull())
        root.add("estates", c.estates);
    if (!c.camps.isNull())
        root.add("camps", c.camps);
    auto heard = Value::object();
    for (const auto& [who, list] : c.scenesHeard)
    {
        auto ids = Value::array();
        for (const auto& id : list)
            ids.push(id);
        heard.add(who, ids);
    }
    root.add("scenesHeard", heard);
    root.add("nextConversation", c.memories.nextConversation);
    auto active = Value::array();
    for (const auto& [key, m] : c.memories.active)
    {
        auto j = Value::object();
        j.add("key", key); j.add("id", m.id); j.add("npc", m.npc); j.add("subject", m.subject); j.add("older", m.olderContext);
        j.add("started", m.started); j.add("lastActivity", m.lastActivity);
        auto turns = Value::array();
        for (const auto& t : m.turns)
        {
            auto k = Value::object();
            k.add("event", t.event); k.add("at", t.at); k.add("who", t.who); k.add("text", t.text);
            turns.push(k);
        }
        j.add("turns", turns);
        active.push(j);
    }
    root.add("activeMemory", active);
    auto summaries = Value::array();
    for (const auto& m : c.memories.summaries)
    {
        auto j = Value::object();
        j.add("id", m.id); j.add("npc", m.npc); j.add("subject", m.subject); j.add("text", m.text);
        j.add("started", m.started); j.add("consolidated", m.consolidated);
        auto sources = Value::array();
        for (auto id : m.sourceEvents)
            sources.push(double(id));
        j.add("sourceEvents", sources);
        summaries.push(j);
    }
    root.add("summaries", summaries);
    auto ledger = Value::array();
    for (const auto& l : c.social.entries)
    {
        auto j = Value::object();
        j.add("event", l.event); j.add("at", l.at); j.add("actor", l.actor); j.add("partner", l.partner);
        j.add("reason", l.reason); j.add("session", l.session); j.add("amount", l.amount);
        ledger.push(j);
    }
    root.add("ledger", ledger);
    auto recent = Value::array();
    for (const auto& [actor, p] : c.social.recent)
    {
        auto j = Value::object();
        j.add("event", p.event); j.add("at", p.at); j.add("actor", p.actor); j.add("cell", p.cell); j.add("words", p.words);
        recent.push(j);
    }
    root.add("socialRecent", recent);
    auto weather = Value::object();
    for (const auto& [cell, w] : saved.weather)
        weather.add(cell, int(w));
    root.add("weather", weather);
    auto winds = Value::object();
    for (const auto& [cell, w] : saved.winds)
        winds.add(cell, wire::wind(w));
    root.add("winds", winds);
    auto lights = Value::object();
    for (const auto& [cell, l] : saved.lighting)
        lights.add(cell, wire::lighting(l));
    root.add("lighting", lights);
    auto receipts = Value::object();
    for (const auto& [who, ids] : c.commandReceipts)
        receipts.add(who, strings(ids));
    root.add("commandReceipts", receipts);
    auto sessions = Value::array();
    for (const auto& [id, s] : c.social.sessions)
    {
        auto j = Value::object();
        j.add("id", s.id); j.add("cell", s.cell); j.add("started", s.started); j.add("last", s.last); j.add("ended", s.ended);
        if (!s.party.empty())
            j.add("party", s.party);
        auto members = Value::array();
        for (const auto& [actor, m] : s.members)
        {
            auto k = Value::object();
            k.add("actor", actor); k.add("turns", m.turns); k.add("words", m.words); k.add("replies", m.replies);
            k.add("last", m.last); k.add("joined", m.joined);
            if (m.left)
                k.add("left", true);
            members.push(k);
        }
        j.add("members", members);
        sessions.push(j);
    }
    root.add("socialSessions", sessions);
    // Gold Stars, Story Stars and Stories (doc 32, 1.2).
    auto stars = Value::array();
    for (const auto& st : c.social.stars)
    {
        auto j = Value::object();
        j.add("giver", st.giver); j.add("recipient", st.recipient); j.add("source", st.source); j.add("kind", st.kind);
        j.add("at", st.at); j.add("amount", st.amount);
        stars.push(j);
    }
    root.add("socialStars", stars);
    auto stories = Value::array();
    for (const auto& [id, st] : c.social.stories)
    {
        auto j = Value::object();
        j.add("id", st.id); j.add("name", st.name); j.add("owner", st.owner); j.add("state", st.state);
        j.add("created", st.created); j.add("last", st.last); j.add("chapter", st.chapter);
        j.add("scenes", strings(st.scenes));
        j.add("members", strings(std::vector<std::string>(st.members.begin(), st.members.end())));
        j.add("approvals", strings(std::vector<std::string>(st.approvals.begin(), st.approvals.end())));
        j.add("starred", strings(std::vector<std::string>(st.starred.begin(), st.starred.end())));
        stories.push(j);
    }
    root.add("socialStories", stories);
    root.add("nextStory", double(c.social.nextStory));
    auto older = Value::object();
    for (const auto& [key, m] : c.memories.active)
    {
        auto events = Value::array();
        for (auto e : m.olderEvents)
            events.push(double(e));
        older.add(key, events);
    }
    root.add("olderMemoryEvents", older);
    auto responses = Value::object();
    for (const auto& [actor, entries] : c.responseReceipts)
    {
        auto values = Value::object();
        for (const auto& [k, v] : entries)
            values.add(k, v);
        responses.add(actor, values);
    }
    root.add("responseReceipts", responses);
    auto audiences = Value::object();
    for (const auto& [sid, s] : c.social.sessions)
        for (const auto& [actor, m] : s.members)
            audiences.add(sid + "|" + actor, strings(m.lastAudience));
    root.add("socialAudiences", audiences);
    return root;
}

std::string npcStates(const PersistedWorld& saved, const std::vector<Entity>& npcs)
{
    auto out = Value::array();
    for (const auto& e : npcs)
    {
        auto j = Value::object();
        j.add("id", e.id);
        j.add("cell", e.cellId);
        j.add("x", e.position.x);
        j.add("y", e.position.y);
        j.add("age", e.age);
        j.add("dead", e.dead);
        if (const auto life = saved.society.residents.find(e.id); life != saved.society.residents.end())
        {
            j.add("task", life->second.task);
            j.add("hunger", life->second.hunger);
            j.add("fatigue", life->second.fatigue);
        }
        if (const auto account = saved.society.accounts.find(e.id); account != saved.society.accounts.end())
        {
            j.add("cash", double(account->second.cash));
            auto stock = Value::object();
            for (const auto& [item, n] : account->second.stock)
                stock.add(item, n);
            j.add("stock", stock);
        }
        out.push(j);
    }
    return json::dump(out);
}

bool decode(const Value& root, PersistedWorld& saved, ServerState& c, std::string& problem)
{
    saved = {};
    c = {};
    if (!root.isObject() || root.number("schema") != 1)
    {
        problem = "invalid save schema";
        return false;
    }
    if (root.has("accounts"))
        c.accounts = root["accounts"];
    if (root.has("director"))
        c.director = root["director"];
    c.sequence = std::uint64_t(num(root, "sequence", 1));
    c.revision = std::uint64_t(num(root, "revision"));
    saved.time = num(root, "time");
    saved.clockOffsetHours = wire::readClockOffset(root);
    saved.calendarDays = root.has("calendarDays") ? wire::strictNumber(root, "calendarDays", -2) : -1;
    saved.hasSociety = root.has("society");
    if (saved.hasSociety)
        saved.society = wire::readSociety(root["society"]);
    const auto& modes = root["seasonalWeather"];
    if (root.has("seasonalWeather") && !modes.isObject())
        saved.calendarDays = -2;
    for (const auto& [cell, v] : modes.fields())
    {
        if (!v.isBool())
            saved.calendarDays = -2;
        else
            saved.seasonalWeather[cell] = v.asBool();
    }
    for (const auto& v : root.array("players"))
    {
        auto e = wire::readEntity(v);
        saved.players.push_back(e);
        c.characters[e.id] = e;
    }
    for (const auto& [id, v] : root.object("doors").fields())
        saved.doorStates[id] = v.asBool();
    for (const auto& j : root.array("mapMemories"))
    {
        CellMemory m;
        m.cellId = j.string("id");
        m.name = j.string("name");
        m.knowledge = static_cast<Knowledge>(int(num(j, "knowledge")));
        m.width = int(num(j, "width"));
        m.height = int(num(j, "height"));
        m.worldX = num(j, "x");
        m.worldY = num(j, "y");
        m.worldZ = num(j, "z");
        if (j.has("glyphsZ") || j.has("observedZ"))
        {
            const auto tiles = std::size_t(std::max(0.0, num(j, "tiles"))), seenTiles = std::size_t(std::max(0.0, num(j, "seenTiles")));
            std::vector<std::uint8_t> glyphs, bits;
            if ((tiles && !unpacked(j, "glyphsZ", tiles, glyphs)) || (seenTiles && !unpacked(j, "observedZ", (seenTiles + 7) / 8, bits)))
            {
                problem = "a map memory of " + j.string("observer") + " (" + m.cellId + ") can't be read";
                return false;
            }
            m.glyphs.assign(glyphs.begin(), glyphs.end());
            m.observed.resize(seenTiles);
            for (std::size_t i = 0; i < seenTiles; ++i)
                m.observed[i] = bits[i / 8] >> (i % 8) & 1;
        }
        else
        {
            const auto glyphs = j.string("glyphs");
            m.glyphs.assign(glyphs.begin(), glyphs.end());
            for (char b : j.string("observed"))
                m.observed.push_back(b == '1');
        }
        saved.memories[j.string("observer")][m.cellId] = m;
    }
    for (const auto& v : root.array("npcs"))
        saved.npcs.push_back(wire::readEntity(v));
    if (const auto& roads = root["roads"]; roads.isObject())
    {
        saved.roads.day = std::int64_t(num(roads, "day", -1));
        saved.roads.nextId = std::max<std::int64_t>(1, std::int64_t(num(roads, "nextId", 1)));
        saved.roads.stocked = roads.boolean("stocked");
        for (const auto& j : roads.array("caravans"))
        {
            Caravan t;
            t.id = j.string("id"); t.from = j.string("from"); t.to = j.string("to");
            t.account = j.string("account"); t.route = readStrings(j, "route");
            t.leg = std::size_t(std::max(0.0, num(j, "leg"))); t.nextAt = num(j, "nextAt");
            t.departed = num(j, "departed"); t.guards = int(num(j, "guards", 1));
            t.status = j.string("status"); t.escorts = readStrings(j, "escorts"); t.letters = readStrings(j, "letters");
            t.cell = j.string("cell"); t.x = num(j, "x"); t.y = num(j, "y");
            t.waitUntil = num(j, "waitUntil");
            for (const auto& [who, cells] : j.object("with").fields())
                if (cells.isNumber())
                    t.with[who] = int(cells.asNumber());
            if (t.leg < t.route.size())
                saved.roads.caravans.push_back(std::move(t));
        }
        for (const auto& j : roads.array("camps"))
            saved.roads.camps.push_back({j.string("id"), j.string("cell"), num(j, "strength"), num(j, "hunger"), num(j, "lastRaid"),
                                         j.boolean("active"), num(j, "x", -1), num(j, "y", -1)});
        for (const auto& j : roads.array("contracts"))
        {
            Contract k;
            k.id = j.string("id"); k.kind = j.string("kind"); k.poster = j.string("poster");
            k.town = j.string("town"); k.target = j.string("target"); k.taker = j.string("taker");
            k.status = j.string("status"); k.reward = std::int64_t(num(j, "reward"));
            k.created = num(j, "created"); k.due = num(j, "due"); k.detail = j.string("detail");
            saved.roads.contracts.push_back(std::move(k));
        }
    }
    for (const auto& j : root.array("beliefs"))
        saved.roads.beliefs.push_back({j.string("holder"), j.string("subject"), j.string("claim"), j.string("source"),
                                       num(j, "confidence"), num(j, "day"), j.string("incident")});
    for (const auto& j : root.array("festivals"))
        saved.festivals.push_back({j.string("community"), j.string("name"), std::int64_t(num(j, "day"))});
    if (const auto& crime = root["crime"]; crime.isObject())
    {
        saved.crime.nextIncident = std::max<std::int64_t>(1, std::int64_t(num(crime, "nextIncident")));
        saved.crime.day = std::int64_t(num(crime, "day", -1));
        for (const auto& j : crime.array("incidents"))
        {
            Incident i;
            i.id = j.string("id"); i.kind = j.string("kind"); i.offender = j.string("offender"); i.victim = j.string("victim");
            i.cell = j.string("cell"); i.town = j.string("town"); i.time = num(j, "time"); i.day = num(j, "day");
            i.item = j.string("item"); i.quantity = int(num(j, "quantity")); i.coins = std::int64_t(num(j, "coins"));
            i.status = j.string("status", "open");
            for (const auto& k : j.array("witnesses"))
                i.witnesses.push_back({k.string("id"), k.boolean("identified"), num(k, "clarity"), k.boolean("reported")});
            saved.crime.incidents.push_back(std::move(i));
        }
        for (const auto& j : crime.array("warrants"))
        {
            Warrant w;
            w.person = j.string("person"); w.town = j.string("town"); w.fine = std::int64_t(num(j, "fine")); w.since = num(j, "since");
            for (const auto& id : j.array("incidents"))
                w.incidents.push_back(id.asString(""));
            for (const auto& k : j.array("restitution"))
                w.restitution.push_back({k.string("to"), k.string("item"), int(num(k, "quantity")), std::int64_t(num(k, "coins"))});
            saved.crime.warrants.push_back(std::move(w));
        }
        for (const auto& j : crime.array("custody"))
            saved.crime.custody.push_back({j.string("person"), j.string("town"), j.string("cell"), num(j, "x"), num(j, "y"), num(j, "until")});
    }
    for (const auto& j : root.array("promises"))
        saved.promises.push_back({j.string("by"), j.string("to"), j.string("what"), num(j, "made"), num(j, "due"), j.string("status")});
    for (const auto& j : root.array("bonds"))
    {
        SavedBond b;
        b.holder = j.string("holder");
        b.other = j.string("other");
        b.bond.affinity = num(j, "affinity");
        b.bond.trust = num(j, "trust");
        b.bond.familiarity = num(j, "familiarity");
        b.bond.fear = num(j, "fear");
        b.bond.respect = num(j, "respect");
        b.bond.owed = std::int64_t(num(j, "owed"));
        b.bond.lastContact = num(j, "lastContact");
        saved.bonds.push_back(std::move(b));
    }
    for (const char* key : {"weather", "winds", "lighting"})
        if (root.has(key) && !root[key].isObject())
        {
            problem = std::string("invalid ") + (key == std::string("winds") ? "wind" : key) + " record";
            return false;
        }
    for (const auto& [cell, v] : root.object("weather").fields())
        saved.weather[cell] = wire::readWeather(v);
    for (const auto& [cell, v] : root.object("winds").fields())
        saved.winds[cell] = wire::readWind(v.isObject() ? v : Value());
    for (const auto& [cell, v] : root.object("lighting").fields())
        saved.lighting[cell] = wire::readLighting(v.isObject() ? v : Value());
    // The server's own part.
    for (const auto& j : root.array("weatherFronts"))
    {
        WeatherSystem f;
        f.id = j.string("id");
        const double kind = j.number("kind", -1);
        f.kind = Weather(int(kind));
        f.x = j.number("x");
        f.y = j.number("y");
        f.radius = j.number("radius", 300);
        f.peak = std::clamp(j.number("peak", 1), 0.0, 1.0);
        f.vx = j.number("vx");
        f.vy = j.number("vy");
        f.born = j.number("born");
        f.life = j.number("life", .5);
        if (kind >= 1 && kind < WeatherKinds && !f.id.empty() && f.radius > 0 && f.radius <= 4000 && f.life > 0 && f.life <= 10 &&
            std::isfinite(f.x) && std::isfinite(f.y) && std::isfinite(f.vx) && std::isfinite(f.vy) && std::isfinite(f.born) &&
            saved.fronts.size() < 64)
            saved.fronts.push_back(f);
    }
    for (const auto& [npc, owner] : root.object("companions").fields())
        c.companions[npc] = owner.asString("");
    c.parties = root.object("parties");
    c.acquaintances = root["acquaintances"];
    c.aliases = root.object("aliases");
    c.notes = root.object("notes");
    c.chapters = root.object("chapters");
    c.factions = root.object("factions");
    c.estates = root.object("estates");
    c.camps = root.object("camps");
    // (Older saves have none: everyone starts having heard nothing.)
    for (const auto& [who, list] : root.object("scenesHeard").fields())
        for (const auto& id : list.items())
            if (id.isString() && c.scenesHeard[who].size() < 4000)
                c.scenesHeard[who].push_back(id.asString());
    c.memories.nextConversation = std::uint64_t(num(root, "nextConversation", 1));
    for (const auto& j : root.array("activeMemory"))
    {
        ActiveMemory m;
        m.id = j.string("id"); m.npc = j.string("npc"); m.subject = j.string("subject"); m.olderContext = j.string("older");
        m.started = num(j, "started"); m.lastActivity = num(j, "lastActivity");
        for (const auto& k : j.array("turns"))
            m.turns.push_back({std::uint64_t(num(k, "event")), num(k, "at"), k.string("who"), k.string("text")});
        c.memories.active[j.string("key")] = m;
    }
    for (const auto& j : root.array("summaries"))
    {
        MemorySummary m;
        m.id = j.string("id"); m.npc = j.string("npc"); m.subject = j.string("subject"); m.text = j.string("text");
        m.started = num(j, "started"); m.consolidated = num(j, "consolidated");
        for (const auto& s : j.array("sourceEvents"))
            m.sourceEvents.push_back(std::uint64_t(s.asNumber()));
        c.memories.summaries.push_back(m);
    }
    for (const auto& j : root.array("ledger"))
    {
        LedgerEntry l;
        l.event = std::uint64_t(num(j, "event")); l.at = num(j, "at"); l.actor = j.string("actor"); l.partner = j.string("partner");
        l.reason = j.string("reason"); l.session = j.string("session"); l.amount = int(num(j, "amount"));
        c.social.entries.push_back(l);
        c.social.points[l.actor] += l.amount;
    }
    for (const auto& j : root.array("socialRecent"))
    {
        SocialPost p;
        p.event = std::uint64_t(num(j, "event")); p.at = num(j, "at"); p.actor = j.string("actor"); p.cell = j.string("cell");
        p.words = int(num(j, "words"));
        c.social.recent[p.actor] = p;
    }
    for (const auto& [who, list] : root.object("commandReceipts").fields())
        for (const auto& id : list.items())
            c.commandReceipts[who].push_back(id.asString(""));
    for (const auto& j : root.array("socialSessions"))
    {
        SocialSession s;
        s.id = j.string("id"); s.cell = j.string("cell"); s.started = num(j, "started"); s.last = num(j, "last"); s.ended = num(j, "ended");
        s.party = j.string("party");
        for (const auto& k : j.array("members"))
        {
            Contribution m;
            m.turns = int(num(k, "turns")); m.words = int(num(k, "words")); m.replies = int(num(k, "replies"));
            m.last = num(k, "last"); m.joined = num(k, "joined");
            m.left = k.boolean("left");
            s.members[k.string("actor")] = m;
        }
        c.social.sessions[s.id] = s;
    }
    for (const auto& j : root.array("socialStars"))
        c.social.stars.push_back({j.string("giver"), j.string("recipient"), j.string("source"), j.string("kind", "gold"), num(j, "at"),
                                  int(num(j, "amount"))});
    for (const auto& j : root.array("socialStories"))
    {
        SocialStory st;
        st.id = j.string("id"); st.name = j.string("name"); st.owner = j.string("owner"); st.state = j.string("state", "pending");
        st.created = num(j, "created"); st.last = num(j, "last"); st.chapter = j.string("chapter");
        for (const auto& v : j.array("scenes")) st.scenes.push_back(v.asString(""));
        for (const auto& v : j.array("members")) st.members.insert(v.asString(""));
        for (const auto& v : j.array("approvals")) st.approvals.insert(v.asString(""));
        for (const auto& v : j.array("starred")) st.starred.insert(v.asString(""));
        if (!st.id.empty())
            c.social.stories[st.id] = st;
    }
    c.social.nextStory = std::max<std::uint64_t>(1, std::uint64_t(num(root, "nextStory", 1)));
    for (const auto& [key, list] : root.object("olderMemoryEvents").fields())
        for (const auto& e : list.items())
            c.memories.active[key].olderEvents.push_back(std::uint64_t(e.asNumber()));
    for (const auto& [actor, values] : root.object("responseReceipts").fields())
        for (const auto& [k, v] : values.fields())
            c.responseReceipts[actor][k] = v.asString("");
    const auto& audiences = root["socialAudiences"];
    for (auto& [sid, s] : c.social.sessions)
        for (auto& [actor, m] : s.members)
            for (const auto& id : audiences.array(sid + "|" + actor))
                m.lastAudience.push_back(id.asString(""));
    return true;
}
} // namespace ratw::checkpoint
