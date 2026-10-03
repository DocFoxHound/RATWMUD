#include "RatwDbStore.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace ratw
{
namespace
{
// The lists of a save stored one row per entry, and how each entry is keyed: game.sections (migration 0012) exactly.
struct SaveList
{
    const char* name;
    const char* parent;                            // The list is document[parent][field], or document[field].
    const char* field;
    std::vector<const char*> key;                  // Fields joined with '|' (a missing one is empty).
};
const std::vector<SaveList>& saveLists()
{
    static const std::vector<SaveList> lists{
        {"accounts", "accounts", "entries", {"username"}},
        {"players", nullptr, "players", {"id"}},
        {"npcs", nullptr, "npcs", {"id"}},
        {"mapMemories", nullptr, "mapMemories", {"observer", "id"}},
        {"activeMemory", nullptr, "activeMemory", {"key"}},
        {"summaries", nullptr, "summaries", {"id"}},
        {"ledger", nullptr, "ledger", {"event", "actor", "partner", "reason"}},
        {"socialRecent", nullptr, "socialRecent", {"event", "actor", "cell"}},
        {"socialSessions", nullptr, "socialSessions", {"id"}},
        {"bonds", nullptr, "bonds", {"holder", "other"}},
        {"beliefs", nullptr, "beliefs", {"holder", "subject", "claim"}},
        {"chapters", "chapters", "chapters", {"id"}},
        {"campSites", "camps", "sites", {"id"}},
        {"campStructures", "camps", "structures", {"id"}},
        {"campStaff", "camps", "staff", {"npc"}},
        {"treaties", "factions", "treaties", {"id"}},
        {"levies", "factions", "levies", {"id"}},
        {"houseRequests", "factions", "houses", {"chapter", "faction"}}};
    return lists;
}
// A field as `e->>'field'` gives it: text as it is, whole numbers without a decimal point, missing or null as ''.
std::string keyText(const json::Value& entry, const char* field)
{
    const auto* v = entry.find(field);
    if (!v)
        return {};
    switch (v->type())
    {
    case json::Value::Type::String: return v->asString();
    case json::Value::Type::Number:
    {
        const double n = v->asNumber();
        if (std::isfinite(n) && n == std::floor(n) && std::fabs(n) < 1e15)
            return std::to_string(static_cast<long long>(n));
        return json::dump(*v);
    }
    case json::Value::Type::Bool: return v->asBool() ? "true" : "false";
    default: return {};
    }
}
void quote(std::string& out, const std::string& text) { out += json::dump(json::Value(text)); }
std::uint64_t fingerprint(const std::string& data, int position)
{
    std::uint64_t hash = 1469598103934665603ULL;
    for (const unsigned char ch : data)
        hash = (hash ^ ch) * 1099511628211ULL;
    return (hash ^ std::uint64_t(position)) * 1099511628211ULL;
}
bool yes(const PgResult& r) { return r.ok && !r.rows.empty() && r.rows[0][0] && *r.rows[0][0] == "t"; }
} // namespace

std::string withoutNul(std::string out)
{
    for (std::size_t at = out.find("\\u0000"); at != std::string::npos; at = out.find("\\u0000", at + 1))
    {
        std::size_t slashes = 0;
        while (at > slashes && out[at - 1 - slashes] == '\\')
            ++slashes;
        if (slashes % 2 == 0)
            out.replace(at, 6, "\\ufffd");
    }
    return out;
}

DbStore::~DbStore()
{
    flush();
    {
        std::lock_guard<std::mutex> guard(queueLock_);
        stopping_ = true;
    }
    wake_.notify_all();
    if (writerThread_.joinable())
        writerThread_.join();
}

bool DbStore::open(const std::string& conninfo, const std::string& worldId, std::string& error)
{
    if (!pg_.connect(conninfo, error))
        return false;
    worldId_ = worldId;
    eventsSupported_ = yes(pg_.exec("SELECT to_regprocedure('game.record_events(ratw_id,jsonb)') IS NOT NULL"));
    // A list the database doesn't store as rows (an older database) stays in the checkpoint row; a delta must too.
    for (const auto& row : pg_.exec("SELECT name FROM game.sections").rows)
        if (row[0])
            knownLists_.insert(*row[0]);
    deltasSupported_ = yes(pg_.exec("SELECT to_regprocedure('game.save_checkpoint_delta(ratw_id,bigint,text,jsonb)') IS NOT NULL"));
    journalSupported_ = yes(pg_.exec("SELECT to_regclass('game.journal') IS NOT NULL"));
    verify_ = verify_ || std::getenv("RATW_VERIFY_SAVES") != nullptr;
    return true;
}

void DbStore::queueEvents(std::vector<WorldEvent> events)
{
    if (events.empty() || !opened() || !eventsSupported_)
        return;
    std::lock_guard<std::mutex> guard(queueLock_);
    for (auto& e : events)
        pendingEvents_.push_back(std::move(e));
    if (pendingEvents_.size() > EventsQueued)
        pendingEvents_.erase(pendingEvents_.begin(), pendingEvents_.begin() + std::ptrdiff_t(pendingEvents_.size() - EventsQueued));
}

std::string DbStore::load()
{
    std::lock_guard<std::mutex> connection(pgLock_);
    const auto result = pg_.exec("SELECT game.load_checkpoint($1)", {worldId_});
    if (!result.ok)
    {
        pgError_ = result.error;
        return "{\"schema\":-1}";
    }
    if (result.rows.empty() || !result.rows[0][0])
        return {};                                 // A world nobody has played yet.
    if (result.rows[0][0]->empty())
        return "{\"schema\":-1}";
    return *result.rows[0][0];
}

bool DbStore::prepare(Checkpoint& next, std::map<std::string, std::map<std::string, std::uint64_t>>& nowWritten)
{
    json::Value document;
    std::string states;
    next.build(document, states);
    next.build = nullptr;
    next.npcStates = states.empty() ? "[]" : states;
    if (document.isNull())
        return false;                              // Nothing to write after all.
    if (!document.isObject())
        document = json::Value::object();
    const bool whole = !deltasSupported_ || !haveWritten_ || deltasSinceWhole_ >= DeltasBetweenWholeSaves;
    if (whole || verify_)
        (whole ? next.payload : next.whole) = withoutNul(json::dump(document));
    // Every list: each entry's key (repeats numbered "#2", "#3"... in order, as the database numbers them), and what
    // changed since the last write. The lists then come out of the document, which leaves what the checkpoint row holds.
    std::string changes = "{";
    for (const auto& list : saveLists())
    {
        if (!knownLists_.count(list.name))
            continue;
        json::Value* container = list.parent ? document.find(list.parent) : &document;
        if (!container || !container->isObject())
            continue;
        const auto* entries = container->find(list.field);
        if (!entries || !entries->isArray())
            continue;
        auto& now = nowWritten[list.name];
        const auto before = written_.find(list.name);
        std::map<std::string, int> seen;
        std::string rows;
        int position = 0;
        for (const auto& entry : entries->items())
        {
            std::string key;
            for (std::size_t part = 0; part < list.key.size(); ++part)
                key += (part ? "|" : "") + keyText(entry, list.key[part]);
            if (const int count = ++seen[key]; count > 1)
                key += "#" + std::to_string(count);
            const std::string data = json::dump(entry);
            const std::uint64_t mark = fingerprint(data, position);
            now[key] = mark;
            const int at = position++;
            if (whole)
                continue;
            if (before != written_.end())
                if (const auto old = before->second.find(key); old != before->second.end() && old->second == mark)
                    continue;
            if (!rows.empty())
                rows += ',';
            rows += "{\"key\":";
            quote(rows, key);
            rows += ",\"position\":" + std::to_string(at) + ",\"data\":" + data + "}";
        }
        if (!whole)
        {
            std::string deleted;
            if (before != written_.end())
                for (const auto& [key, mark] : before->second)
                    if (!now.count(key))
                    {
                        if (!deleted.empty())
                            deleted += ',';
                        quote(deleted, key);
                    }
            if (changes.size() > 1)
                changes += ',';
            quote(changes, list.name);
            changes += ":{\"rows\":[" + rows + "],\"deleted\":[" + deleted + "]}";
        }
        container->erase(list.field);
    }
    if (!whole)
    {
        next.payload = withoutNul(json::dump(document));
        next.changes = withoutNul(changes) + "}";
        next.delta = true;
    }
    return true;
}

bool DbStore::write(const Checkpoint& save, const std::vector<WorldEvent>& events)
{
    // One transaction: the checkpoint and the published NPC states share a timestamp (now() is the transaction's
    // start), which is how start-up tells this server's own NPC states from ones written by someone else.
    const auto failed = [this](const PgResult& r) {
        pgError_ = r.error;
        pg_.exec("ROLLBACK");
        return false;
    };
    auto result = pg_.exec("BEGIN");
    if (!result.ok)
        return failed(result);
    result = save.delta ? pg_.exec("SELECT game.save_checkpoint_delta($1, $2, $3, $4::jsonb)",
                                   {worldId_, std::to_string(save.revision), save.payload, save.changes})
                        : pg_.exec("SELECT game.save_checkpoint($1, $2, $3)", {worldId_, std::to_string(save.revision), save.payload});
    if (!result.ok)
        return failed(result);
    result = pg_.exec(
        "INSERT INTO live.npc_state (world_id, npc_id, alive, state, updated_at) "
        "SELECT $1, e->>'id', NOT coalesce((e->>'dead')::boolean, false), e - 'id', now() FROM jsonb_array_elements($2::jsonb) e "
        "ON CONFLICT (world_id, npc_id) DO UPDATE SET alive = excluded.alive, state = excluded.state, updated_at = excluded.updated_at "
        "WHERE live.npc_state.alive IS DISTINCT FROM excluded.alive OR live.npc_state.state IS DISTINCT FROM excluded.state",
        {worldId_, save.npcStates});
    if (!result.ok)
        return failed(result);
    if (!events.empty())
    {
        result = pg_.exec("SELECT game.record_events($1, $2::jsonb)", {worldId_, eventsJson(events)});
        if (!result.ok)
            return failed(result);
    }
    result = pg_.exec("COMMIT");
    pgError_ = result.error;
    return result.ok;
}

void DbStore::writer()
{
    std::unique_lock<std::mutex> queue(queueLock_);
    for (;;)
    {
        wake_.wait(queue, [this] { return stopping_ || pending_.has_value(); });
        if (!pending_)
            return;
        Checkpoint next = std::move(*pending_);
        pending_.reset();
        std::vector<WorldEvent> events;
        events.swap(pendingEvents_);
        writing_ = true;
        queue.unlock();
        bool built = static_cast<bool>(next.build);
        std::map<std::string, std::map<std::string, std::uint64_t>> nowWritten;
        if (built && !prepare(next, nowWritten))
        {
            queue.lock();
            writing_ = false;
            if (!events.empty())                   // Kept for the next checkpoint, in order.
                pendingEvents_.insert(pendingEvents_.begin(), std::make_move_iterator(events.begin()), std::make_move_iterator(events.end()));
            idle_.notify_all();
            continue;
        }
        bool ok;
        std::string problem;
        {
            std::lock_guard<std::mutex> connection(pgLock_);
            ok = write(next, events);
            problem = pgError_;
            if (ok && next.delta && !next.whole.empty())
            {
                const auto same = pg_.exec("SELECT game.load_checkpoint($1)::jsonb = $2::jsonb", {worldId_, next.whole});
                const std::string note = "RATW_SAVE_VERIFY revision=" + std::to_string(next.revision) + (yes(same) ? " matches" : " MISMATCH") +
                                         "; sent " + std::to_string(next.payload.size() + next.changes.size()) + " of " +
                                         std::to_string(next.whole.size()) + " bytes";
                if (log_)
                    log_(note);
                else
                    std::fprintf(stderr, "%s\n", note.c_str());
            }
        }
        haveWritten_ = ok && built;                // A save handed over as text leaves nothing to compare the next with.
        if (haveWritten_)
        {
            written_ = std::move(nowWritten);
            deltasSinceWhole_ = next.delta ? deltasSinceWhole_ + 1 : 0;
        }
        queue.lock();
        writing_ = false;
        if (ok)
            storedRevision_ = std::max(storedRevision_, next.revision);
        if (!ok)
        {
            backgroundFailed_ = true;
            backgroundError_ = problem;
        }
        idle_.notify_all();
    }
}

bool DbStore::saveInBackground(Build build, std::uint64_t revision)
{
    {
        std::lock_guard<std::mutex> guard(queueLock_);
        if (backgroundFailed_)
            return false;
        Checkpoint next;
        next.revision = revision;
        next.build = std::move(build);
        pending_ = std::move(next);
        if (!writerThread_.joinable())
            writerThread_ = std::thread([this] { writer(); });
    }
    wake_.notify_all();
    return true;
}

bool DbStore::saveInBackground(std::string payload, std::uint64_t revision, std::string npcStates)
{
    {
        std::lock_guard<std::mutex> guard(queueLock_);
        if (backgroundFailed_)
            return false;
        Checkpoint next;
        next.payload = withoutNul(std::move(payload));
        next.npcStates = npcStates.empty() ? "[]" : std::move(npcStates);
        next.revision = revision;
        pending_ = std::move(next);
        if (!writerThread_.joinable())
            writerThread_ = std::thread([this] { writer(); });
    }
    wake_.notify_all();
    return true;
}

bool DbStore::flush()
{
    std::unique_lock<std::mutex> queue(queueLock_);
    idle_.wait(queue, [this] { return !pending_ && !writing_; });
    return !backgroundFailed_;
}

std::uint64_t DbStore::storedRevision() const
{
    std::lock_guard<std::mutex> guard(queueLock_);
    return storedRevision_;
}

bool DbStore::journalAfter(std::uint64_t after, std::vector<journal::Record>& out, std::string& problem)
{
    if (!journalSupported_)
        return true;
    std::lock_guard<std::mutex> connection(pgLock_);
    const auto rows = pg_.exec("SELECT seq, record FROM game.journal WHERE world_id = $1 AND seq > $2 ORDER BY seq",
                               {worldId_, std::to_string(after)});
    if (!rows.ok)
    {
        problem = "the journal can't be read: " + rows.error;
        return false;
    }
    for (const auto& row : rows.rows)
        if (row[0] && row[1])
            out.push_back({std::uint64_t(std::stoull(*row[0])), *row[1]});
    return true;
}

std::vector<std::pair<std::string, std::string>> DbStore::externalNpcStates()
{
    std::vector<std::pair<std::string, std::string>> out;
    std::lock_guard<std::mutex> connection(pgLock_);
    const auto result = pg_.exec(
        "SELECT npc_id, state::text FROM live.npc_state WHERE world_id = $1 AND updated_at > "
        "coalesce((SELECT saved_at FROM game.checkpoints WHERE world_id = $1), '-infinity'::timestamptz)",
        {worldId_});
    for (const auto& row : result.rows)
        if (row[0] && row[1])
            out.emplace_back(*row[0], *row[1]);
    return out;
}

std::string DbStore::error() const
{
    std::lock_guard<std::mutex> guard(queueLock_);
    return backgroundFailed_ ? backgroundError_ : pgError_;
}
} // namespace ratw
