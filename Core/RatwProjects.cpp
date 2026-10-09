// Town projects (Docs/Design/57-changing-the-world.md, 4): see RatwProjects.h.
#include "RatwProjects.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace ratw::projects
{
using json::Value;

namespace
{
std::filesystem::path projectsFile()
{
    // Data/Town: RATW_DATA_DIR, else the working directory or one above it, else the source tree.
    namespace fs = std::filesystem;
    std::error_code ec;
    if (const char* dir = std::getenv("RATW_DATA_DIR"); dir && *dir)
        return fs::path(dir) / "Town" / "projects.json";
    for (auto at = fs::current_path(ec); !ec && !at.empty(); at = at.parent_path())
    {
        if (fs::exists(at / "Data" / "Town" / "projects.json", ec))
            return at / "Data" / "Town" / "projects.json";
        if (at == at.parent_path())
            break;
    }
#ifdef RATW_SOURCE_DIR
    return fs::path(RATW_SOURCE_DIR) / "Data" / "Town" / "projects.json";
#else
    return fs::path("Data") / "Town" / "projects.json";
#endif
}
} // namespace

const Kind* Rules::kind(const std::string& id) const
{
    for (const auto& k : kinds)
        if (k.id == id)
            return &k;
    return nullptr;
}

Rules parse(const std::string& text)
{
    Rules r;
    Value doc;
    std::string error;
    if (text.empty() || !json::parse(text, doc, error) || !doc.isObject())
        return r;
    r.perTown = std::max(1, int(doc.number("perTown", r.perTown)));
    r.perCity = std::max(r.perTown, int(doc.number("perCity", r.perCity)));
    r.lapseDays = doc.number("lapseDays", r.lapseDays);
    r.hourShare = doc.number("hourShare", r.hourShare);
    r.nameShare = doc.object("name").number("share", r.nameShare);
    r.nameWorth = std::int64_t(doc.object("name").number("worth", double(r.nameWorth)));
    r.upkeepUnder = doc.number("upkeepUnder", r.upkeepUnder);
    r.greatWorth = std::int64_t(doc.object("deeds").number("greatWorth", double(r.greatWorth)));
    r.otherShare = doc.object("deeds").number("otherShare", r.otherShare);
    r.handHours = doc.object("hands").number("hours", r.handHours);
    r.handsMost = std::max(0, int(doc.object("hands").number("most", r.handsMost)));
    for (const auto& k : doc.array("kinds"))
    {
        Kind kind;
        kind.id = k.string("id");
        kind.name = k.string("name");
        kind.structure = k.string("structure");
        kind.title = k.string("title", "the {town} project");
        kind.hours = std::max(1.0, k.number("hours", kind.hours));
        kind.wear = std::max(0.0, k.number("wear", kind.wear));
        kind.city = k.boolean("city");
        for (const auto& [item, n] : k.object("materials").fields())
            if (n.isNumber() && n.asNumber() >= 1)
                kind.materials.push_back({item, int(n.asNumber())});
        for (const auto& role : k.array("roles"))
            if (role.isString())
                kind.roles.push_back(role.asString());
        if (kind.roles.empty())
            kind.roles = {"worker"};
        if (!kind.id.empty())
            r.kinds.push_back(std::move(kind));
    }
    return r;
}

const Rules& rules()
{
    static const Rules r = [] {
        std::ifstream in(projectsFile());
        std::stringstream text;
        text << in.rdbuf();
        if (!in)
            std::cerr << "[warn] RATW_PROJECTS no Data/Town/projects.json: towns propose no projects\n";
        return parse(text.str());
    }();
    return r;
}

std::string titleFor(const Kind& k, const std::string& townName)
{
    auto title = k.title;
    for (auto at = title.find("{town}"); at != std::string::npos; at = title.find("{town}", at + townName.size()))
        title.replace(at, 6, townName);
    return title;
}

Project* Ledger::find(const std::string& id)
{
    const auto found = all_.find(id);
    return found == all_.end() ? nullptr : &found->second;
}

const Project* Ledger::find(const std::string& id) const
{
    const auto found = all_.find(id);
    return found == all_.end() ? nullptr : &found->second;
}

Project& Ledger::post(const Kind& k, const std::string& town, const std::string& cell, int x, int y, const std::string& title,
                      double day, const std::string& by)
{
    Project p;
    p.id = "proj-" + std::to_string(next_++);
    p.kind = k.id;
    p.town = town;
    p.cell = cell;
    p.x = x;
    p.y = y;
    p.title = title;
    p.by = by;
    p.posted = day;
    p.hours = k.hours;
    for (const auto& [item, n] : k.materials)
        p.needs[item] = n;
    auto& kept = all_[p.id];
    kept = std::move(p);
    return kept;
}

void Ledger::give(Project& p, const std::string& who, const std::string& kind, const std::string& item, double amount, double value,
                  double day)
{
    for (auto it = p.gifts.begin(); it != p.gifts.end(); ++it)
        if (it->who == who && it->kind == kind && it->item == item)
        {
            it->amount += amount;
            it->value += value;
            it->day = day;
            if (it->amount <= 1e-9)
                p.gifts.erase(it);
            return;
        }
    if (amount > 0)
        p.gifts.push_back({who, kind, item, amount, value, day});
}

double Ledger::worth(const Project& p)
{
    double total = 0;
    for (const auto& g : p.gifts)
        total += std::max(0.0, g.value);
    return total;
}

std::vector<Giver> Ledger::givers(const Project& p)
{
    std::map<std::string, double> by;
    for (const auto& g : p.gifts)
        by[g.who] += std::max(0.0, g.value);
    std::vector<Giver> out;
    for (const auto& [who, value] : by)
        if (value > 0)
            out.push_back({who, value});
    std::stable_sort(out.begin(), out.end(), [](const Giver& a, const Giver& b) { return a.value > b.value; });
    return out;
}

std::vector<std::pair<std::string, std::string>> Ledger::plaque(const Project& p, int most)
{
    std::vector<std::pair<std::string, std::string>> out;
    for (const auto& g : givers(p))
    {
        if (int(out.size()) >= most)
            break;
        const auto shown = p.shown.find(g.who);
        out.push_back({g.who, shown == p.shown.end() || shown->second.empty() ? std::string("a friend of the town") : shown->second});
    }
    return out;
}

std::string Ledger::chief(const Project& p, const Rules& r)
{
    const double whole = worth(p);
    const auto all = givers(p);
    if (all.empty() || whole < double(r.nameWorth) || all.front().value < r.nameShare * whole)
        return {};
    const auto shown = p.shown.find(all.front().who);
    return shown == p.shown.end() || shown->second.empty() ? std::string() : all.front().who;   // (Named only by a name it chose.)
}

std::vector<std::pair<std::string, std::int64_t>> Ledger::shares(const Project& p, const std::string& kind, const std::string& item,
                                                                 std::int64_t pool)
{
    std::vector<std::pair<std::string, double>> given;
    double total = 0;
    for (const auto& g : p.gifts)
        if (g.kind == kind && (item.empty() || g.item == item) && g.amount > 0)
        {
            given.push_back({g.who, g.amount});
            total += g.amount;
        }
    std::vector<std::pair<std::string, std::int64_t>> out;
    if (pool <= 0 || total <= 0)
        return out;
    std::stable_sort(given.begin(), given.end(), [](const auto& a, const auto& b) { return a.second > b.second; });
    std::int64_t handed = 0;
    for (const auto& [who, amount] : given)
    {
        const auto part = std::int64_t(std::floor(double(pool) * amount / total));
        out.push_back({who, part});
        handed += part;
    }
    for (std::size_t i = 0; handed < pool && !out.empty(); i = (i + 1) % out.size())
        ++out[i].second, ++handed;
    return out;
}

int Ledger::openIn(const std::string& town) const
{
    int n = 0;
    for (const auto& [id, p] : all_)
        n += p.town == town && p.state == "open";
    return n;
}

Value Ledger::save() const
{
    // As a list (a game.sections table, game.projects): one row a project, its gifts in it.
    auto list = Value::array();
    for (const auto& [id, p] : all_)
    {
        auto o = Value::object();
        o.add("id", p.id); o.add("kind", p.kind); o.add("town", p.town); o.add("cell", p.cell); o.add("title", p.title);
        o.add("state", p.state); o.add("by", p.by); o.add("site", p.site); o.add("structure", p.structure);
        o.add("namedFor", p.namedFor); o.add("x", p.x); o.add("y", p.y); o.add("hours", p.hours); o.add("worked", p.worked);
        o.add("posted", p.posted); o.add("finished", p.finished); o.add("condition", p.condition);
        auto needs = Value::object();
        for (const auto& [item, n] : p.needs)
            needs.add(item, n);
        o.add("needs", needs);
        auto shown = Value::object();
        for (const auto& [who, name] : p.shown)
            shown.add(who, name);
        o.add("shown", shown);
        auto gifts = Value::array();
        for (const auto& g : p.gifts)
        {
            auto j = Value::object();
            j.add("who", g.who); j.add("kind", g.kind); j.add("item", g.item); j.add("amount", g.amount); j.add("value", g.value);
            j.add("day", g.day);
            gifts.push(j);
        }
        o.add("gifts", gifts);
        list.push(o);
    }
    return list;
}

void Ledger::load(const Value& saved)
{
    all_.clear();
    next_ = 1;
    for (const auto& o : saved.items())
    {
        Project p;
        p.id = o.string("id");
        p.kind = o.string("kind");
        if (p.id.rfind("proj-", 0) != 0 || p.id.size() > 40 || !rules().kind(p.kind))
            continue;
        p.town = o.string("town"); p.cell = o.string("cell"); p.title = o.string("title").substr(0, 120);
        p.state = o.string("state", "open"); p.by = o.string("by"); p.site = o.string("site"); p.structure = o.string("structure");
        p.namedFor = o.string("namedFor"); p.x = int(o.number("x")); p.y = int(o.number("y"));
        p.hours = std::max(1.0, o.number("hours", 20)); p.worked = std::clamp(o.number("worked"), 0.0, p.hours);
        p.posted = o.number("posted"); p.finished = o.number("finished", -1); p.condition = std::clamp(o.number("condition", 100), 0.0, 100.0);
        for (const auto& [item, n] : o.object("needs").fields())
            if (n.isNumber() && n.asNumber() >= 1)
                p.needs[item] = int(n.asNumber());
        for (const auto& [who, name] : o.object("shown").fields())
            if (name.isString() && who.size() <= 80)
                p.shown[who] = name.asString().substr(0, 60);
        for (const auto& j : o.array("gifts"))
            if (j.string("who").size() <= 80 && j.number("amount") > 0 && p.gifts.size() < 2000)
                p.gifts.push_back({j.string("who"), j.string("kind"), j.string("item"), j.number("amount"), j.number("value"), j.number("day")});
        const auto n = std::strtoull(p.id.c_str() + 5, nullptr, 10);
        next_ = std::max<std::uint64_t>(next_, n + 1);
        all_[p.id] = std::move(p);
    }
}
} // namespace ratw::projects
