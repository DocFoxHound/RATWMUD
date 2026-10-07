#include "RatwReports.h"

#include "RatwPg.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

namespace ratw::reports
{
json::Value toJson(const Report& r)
{
    auto o = json::Value::object();
    o.add("id", r.id);
    o.add("created", r.created);
    o.add("reporterAccount", r.reporterAccount);
    o.add("reporterCharacter", r.reporterCharacter);
    o.add("reportedAccount", r.reportedAccount);
    o.add("reportedCharacter", r.reportedCharacter);
    o.add("kind", r.kind);
    o.add("category", r.category);
    o.add("note", r.note);
    auto lines = json::Value::array();
    for (const auto& l : r.evidence)
    {
        auto line = json::Value::object();
        line.add("seq", double(l.seq));
        line.add("at", l.at);
        line.add("channel", l.channel);
        line.add("text", l.text);
        lines.push(line);
    }
    o.add("evidence", lines);
    o.add("status", r.status);
    if (!r.decidedBy.empty())
    {
        o.add("decidedBy", r.decidedBy);
        o.add("decidedAt", r.decidedAt);
        o.add("outcome", r.outcome);
        o.add("silenceHours", r.silenceHours);
    }
    return o;
}

Report fromJson(const json::Value& o)
{
    Report r;
    r.id = o.string("id").substr(0, 64);
    r.created = o.number("created");
    r.reporterAccount = o.string("reporterAccount").substr(0, 80);
    r.reporterCharacter = o.string("reporterCharacter").substr(0, 80);
    r.reportedAccount = o.string("reportedAccount").substr(0, 80);
    r.reportedCharacter = o.string("reportedCharacter").substr(0, 80);
    r.kind = o.string("kind", "speech").substr(0, 16);
    r.category = validCategory(o.string("category")) ? o.string("category") : "other";
    r.note = o.string("note").substr(0, 600);
    for (const auto& l : o.array("evidence"))
        if (r.evidence.size() < 40)
            r.evidence.push_back({std::uint64_t(l.number("seq")), l.number("at"), l.string("channel").substr(0, 16), l.string("text").substr(0, 4000)});
    r.status = o.string("status", "open");
    r.decidedBy = o.string("decidedBy").substr(0, 80);
    r.decidedAt = o.number("decidedAt", -1);
    r.outcome = o.string("outcome").substr(0, 16);
    r.silenceHours = int(o.number("silenceHours", 0));
    return r;
}

bool validCategory(const std::string& category)
{
    return category == "harassment" || category == "hateful" || category == "spam" || category == "cheating" || category == "other";
}

namespace
{
// Whether a report has outlived what is kept of it: false to keep it whole; true to delete it; and its evidence
// cleared for an old upheld one.
bool expired(Report& r, double now)
{
    if (r.status == "upheld")
    {
        if (!r.evidence.empty() && r.decidedAt >= 0 && now - r.decidedAt > EvidenceDays * 86400)
            r.evidence.clear();
        return false;
    }
    return now - r.created > KeepDays * 86400;
}

class MemoryStore final : public Store
{
  public:
    std::map<std::string, Report> reports;
    bool add(const Report& r, std::string&) override
    {
        reports[r.id] = r;
        return true;
    }
    bool update(const Report& r) override
    {
        if (!reports.count(r.id))
            return false;
        reports[r.id] = r;
        return true;
    }
    std::vector<Report> all() override
    {
        std::vector<Report> out;
        for (const auto& [id, r] : reports)
            out.push_back(r);
        std::sort(out.begin(), out.end(), [](const Report& a, const Report& b) { return a.created < b.created; });
        return out;
    }
    void purge(double now) override
    {
        for (auto it = reports.begin(); it != reports.end();)
            it = expired(it->second, now) ? reports.erase(it) : std::next(it);
    }
};

class FolderStore final : public Store
{
  public:
    std::string file;
    MemoryStore memory;
    void write()
    {
        auto list = json::Value::array();
        for (const auto& r : memory.all())
            list.push(toJson(r));
        std::ofstream out(file + ".tmp", std::ios::binary | std::ios::trunc);
        out << json::dump(list);
        out.close();
        std::error_code ec;
        std::filesystem::rename(file + ".tmp", file, ec);
    }
    bool add(const Report& r, std::string& error) override
    {
        memory.add(r, error);
        write();
        return true;
    }
    bool update(const Report& r) override
    {
        if (!memory.update(r))
            return false;
        write();
        return true;
    }
    std::vector<Report> all() override { return memory.all(); }
    void purge(double now) override
    {
        const auto before = memory.reports.size();
        memory.purge(now);
        if (memory.reports.size() != before)
            write();
    }
};

class DatabaseStore final : public Store
{
  public:
    PgClient pg;
    std::string world;
    bool add(const Report& r, std::string& error) override
    {
        const auto row = toJson(r);
        const auto res = pg.exec("INSERT INTO game.reports (world_id, id, created_at, reporter_account, reporter_character, reported_account, "
                                 "reported_character, kind, category, note, evidence, status) VALUES ($1, $2, to_timestamp($3), $4, $5, $6, $7, $8, $9, "
                                 "$10, $11::jsonb, $12)",
                                 {world, r.id, std::to_string(r.created), r.reporterAccount, r.reporterCharacter, r.reportedAccount,
                                  r.reportedCharacter, r.kind, r.category, r.note, json::dump(row["evidence"]), r.status});
        if (!res.ok)
            error = res.error;
        return res.ok;
    }
    bool update(const Report& r) override
    {
        return pg.exec("UPDATE game.reports SET status = $3, decided_by = $4, decided_at = to_timestamp($5), outcome = $6, silence_hours = $7 "
                       "WHERE world_id = $1 AND id = $2",
                       {world, r.id, r.status, r.decidedBy, std::to_string(std::max(0.0, r.decidedAt)), r.outcome, std::to_string(r.silenceHours)})
            .ok;
    }
    std::vector<Report> all() override
    {
        std::vector<Report> out;
        const auto res = pg.exec("SELECT id, extract(epoch FROM created_at), reporter_account, reporter_character, reported_account, "
                                 "reported_character, kind, category, note, evidence::text, status, coalesce(decided_by, ''), "
                                 "coalesce(extract(epoch FROM decided_at), -1), coalesce(outcome, ''), silence_hours FROM game.reports "
                                 "WHERE world_id = $1 ORDER BY created_at",
                                 {world});
        if (!res.ok)
            return out;
        for (const auto& row : res.rows)
        {
            auto o = json::Value::object();
            const char* keys[] = {"id", "created", "reporterAccount", "reporterCharacter", "reportedAccount", "reportedCharacter", "kind",
                                  "category", "note", "", "status", "decidedBy", "decidedAt", "outcome", "silenceHours"};
            for (std::size_t i = 0; i < row.size() && i < 15; ++i)
            {
                const auto v = row[i].value_or("");
                if (i == 9)
                {
                    json::Value lines;
                    std::string problem;
                    if (json::parse(v, lines, problem))
                        o.add("evidence", lines);
                }
                else if (i == 1 || i == 12 || i == 14)
                    o.add(keys[i], std::atof(v.c_str()));
                else
                    o.add(keys[i], v);
            }
            out.push_back(fromJson(o));
        }
        return out;
    }
    void purge(double) override
    {
        // In SQL: the open and dismissed after 30 days, upheld evidence after 180 (doc 50, 7).
        pg.exec("DELETE FROM game.reports WHERE world_id = $1 AND status <> 'upheld' AND created_at < now() - interval '30 days'", {world});
        pg.exec("UPDATE game.reports SET evidence = '[]'::jsonb WHERE world_id = $1 AND status = 'upheld' AND evidence <> '[]'::jsonb "
                "AND decided_at < now() - interval '180 days'",
                {world});
    }
};
} // namespace

std::unique_ptr<Store> memoryStore() { return std::make_unique<MemoryStore>(); }

std::unique_ptr<Store> folderStore(const std::string& directory, std::string& error)
{
    std::error_code ec;
    std::filesystem::create_directories(directory, ec);
    if (ec)
    {
        error = "Cannot make the reports folder " + directory + ".";
        return nullptr;
    }
    auto store = std::make_unique<FolderStore>();
    store->file = directory + "/reports.json";
    std::ifstream in(store->file, std::ios::binary);
    if (in)
    {
        std::ostringstream text;
        text << in.rdbuf();
        json::Value list;
        std::string problem;
        if (json::parse(text.str(), list, problem))
            for (const auto& o : list.items())
                if (const auto r = fromJson(o); !r.id.empty())
                    store->memory.reports[r.id] = r;
    }
    return store;
}

std::unique_ptr<Store> databaseStore(const std::string& conninfo, const std::string& worldId, std::string& error)
{
    auto store = std::make_unique<DatabaseStore>();
    store->world = worldId;
    if (!store->pg.connect(conninfo, error))
        return nullptr;
    const auto check = store->pg.exec("SELECT to_regclass('game.reports') IS NOT NULL");
    if (!check.ok || check.rows.empty() || check.rows[0][0].value_or("f") != "t")
    {
        error = "The database has no game.reports table yet (migration 0036).";
        return nullptr;
    }
    return store;
}
} // namespace ratw::reports
