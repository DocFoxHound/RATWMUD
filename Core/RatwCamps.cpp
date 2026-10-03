// A Chapter's own ground (RatwCamps.h; Docs/Design/32-parties-chapters-factions.md, 5.3–5.6).
#include "RatwCamps.h"

#include <algorithm>
#include <cmath>

namespace ratw::camp
{
using json::Value;

const std::vector<Kind>& catalogue()
{
    // Placeholders throughout (doc 32): cost in pennies, work-hours, the level it needs.
    static const std::vector<Kind> kinds{
        {"tent", "Tent", '^', 10, 2, 3, false},
        {"firepit", "Firepit", '*', 4, 1, 3, false},
        {"leanto", "Lean-to", '/', 8, 2, 3, false},
        {"storage", "Storage pile", '#', 6, 1, 3, false},
        {"hitching", "Hitching post", '|', 4, 1, 3, false},
        {"cookfire", "Cookfire", '&', 8, 2, 3, false},
        {"watchpost", "Watch post", 'T', 12, 4, 3, false},
        {"palisade", "Palisade section", '=', 5, 2, 4, true},
        {"gate", "Gate", 'H', 15, 4, 4, true},
        {"hall", "Timber hall", 'M', 60, 16, 4, false},
        {"workshop", "Workshop", 'w', 40, 10, 4, false},
        {"stable", "Stable", 's', 30, 8, 4, false},
        {"well", "Well", 'o', 25, 8, 4, false},
        {"keep", "Stone keep", 'K', 400, 80, 5, true},
        {"wall", "Stone wall", '#', 40, 10, 5, true},
        {"tower", "Tower", 'I', 120, 30, 5, true},
        {"gatehouse", "Gatehouse", 'G', 150, 40, 5, true},
    };
    return kinds;
}

const Kind* kind(const std::string& id)
{
    for (const auto& k : catalogue())
        if (id == k.id)
            return &k;
    return nullptr;
}

const Site* Camps::site(const std::string& id) const
{
    const auto found = sites_.find(id);
    return found == sites_.end() ? nullptr : &found->second;
}

Site* Camps::site(const std::string& id)
{
    const auto found = sites_.find(id);
    return found == sites_.end() ? nullptr : &found->second;
}

Structure* Camps::structure(const std::string& id)
{
    const auto found = structures_.find(id);
    return found == structures_.end() ? nullptr : &found->second;
}

std::vector<const Site*> Camps::sitesOf(const std::string& chapter) const
{
    std::vector<const Site*> out;
    for (const auto& [id, s] : sites_)
        if (s.chapter == chapter && s.state == "standing")
            out.push_back(&s);
    return out;
}

const Site* Camps::siteAt(const std::string& cell, double x, double y, const std::string& chapter) const
{
    for (const auto& [id, s] : sites_)
        if (s.cell == cell && s.state == "standing" && (chapter.empty() || s.chapter == chapter) &&
            std::hypot(s.x + .5 - x, s.y + .5 - y) <= SiteRadius)
            return &s;
    return nullptr;
}

std::vector<const Structure*> Camps::structuresOf(const std::string& siteId) const
{
    std::vector<const Structure*> out;
    for (const auto& [id, st] : structures_)
        if (st.site == siteId)
            out.push_back(&st);
    return out;
}

std::vector<const Structure*> Camps::structuresIn(const std::string& cell) const
{
    std::vector<const Structure*> out;
    for (const auto& [id, st] : structures_)
        if (const auto* s = site(st.site); s && s->cell == cell)
            out.push_back(&st);
    return out;
}

const Structure* Camps::structureAt(const std::string& cell, int x, int y) const
{
    for (const auto& [id, st] : structures_)
        if (st.x == x && st.y == y)
            if (const auto* s = site(st.site); s && s->cell == cell)
                return &st;
    return nullptr;
}

Outcome Camps::found(const std::string& chapter, const std::string& cell, int x, int y, const std::string& name, double day, double now)
{
    for (const auto& [id, s] : sites_)
        if (s.cell == cell && s.state == "standing" && s.chapter != chapter && std::hypot(double(s.x - x), double(s.y - y)) < Apart)
            return {false, "Another Chapter's ground is too close."};
    if (siteAt(cell, x + .5, y + .5, chapter))
        return {false, "Your Chapter already has ground here."};
    if (name.empty() || name.size() > 40)
        return {false, "Name the camp (up to 40 letters)."};
    Site s;
    s.id = "site-" + std::to_string(next_++);
    s.chapter = chapter;
    s.cell = cell;
    s.name = name;
    s.x = x;
    s.y = y;
    s.founded = day;
    s.visited = now;
    sites_[s.id] = s;
    return {true, s.id};
}

Outcome Camps::plan(const std::string& siteId, const std::string& kindId, int x, int y)
{
    const auto* s = site(siteId);
    const auto* k = kind(kindId);
    if (!s || s->state != "standing")
        return {false, "There is no camp here."};
    if (!k)
        return {false, "Nothing like that can be built."};
    if (std::hypot(s->x + .5 - (x + .5), s->y + .5 - (y + .5)) > SiteRadius)
        return {false, "That is beyond the camp's ground."};
    if (structureAt(s->cell, x, y))
        return {false, "Something stands there already."};
    int planned = 0;
    for (const auto& [id, st] : structures_)
        planned += st.site == siteId;
    if (planned >= 80)
        return {false, "The camp can hold no more."};
    Structure st;
    st.id = "structure-" + std::to_string(next_++);
    st.site = siteId;
    st.kind = kindId;
    st.x = x;
    st.y = y;
    structures_[st.id] = st;
    return {true, st.id};
}

Outcome Camps::unplan(const std::string& id)
{
    return structures_.erase(id) ? Outcome{true, {}} : Outcome{false, "Nothing like that is planned."};
}

bool Camps::work(const std::string& id, double hours)
{
    auto* st = structure(id);
    if (!st || hours <= 0)
        return false;
    const auto* k = kind(st->kind);
    if (!k)
        return false;
    if (!st->built)
    {
        st->work = std::min(k->hours, st->work + hours);
        if (st->work >= k->hours)
        {
            st->built = true;
            st->condition = 100;
            return true;
        }
        return false;
    }
    // Mending: a work-hour restores what the kind's hours would build, spread over its condition.
    const double before = st->condition;
    st->condition = std::min(100.0, st->condition + hours / k->hours * 100);
    return before < 100 && st->condition >= 100;
}

int Camps::built(const std::string& siteId, bool fortificationsOnly) const
{
    int n = 0;
    for (const auto& [id, st] : structures_)
        if (st.site == siteId && st.built && st.condition > 0)
            if (const auto* k = kind(st.kind); k && (!fortificationsOnly || k->fortification))
                ++n;
    return n;
}

bool Camps::hasBuilt(const std::string& siteId, const std::string& kindId) const
{
    for (const auto& [id, st] : structures_)
        if (st.site == siteId && st.kind == kindId && st.built && st.condition > 0)
            return true;
    return false;
}

bool Camps::campStanding(const std::string& chapter) const
{
    for (const auto* s : sitesOf(chapter))
        if (built(s->id) >= 3)
            return true;
    return false;
}

bool Camps::fortified(const std::string& chapter) const
{
    for (const auto* s : sitesOf(chapter))
    {
        int palisade = 0;
        for (const auto& [id, st] : structures_)
            palisade += st.site == s->id && st.kind == "palisade" && st.built && st.condition > 0;
        if (palisade >= 8 && hasBuilt(s->id, "gate") && hasBuilt(s->id, "hall"))
            return true;
    }
    return false;
}

bool Camps::hold(const std::string& chapter) const
{
    for (const auto* s : sitesOf(chapter))
    {
        int walls = 0;
        for (const auto& [id, st] : structures_)
            walls += st.site == s->id && st.kind == "wall" && st.built && st.condition > 0;
        if (walls >= 8 && hasBuilt(s->id, "keep") && hasBuilt(s->id, "gatehouse"))
            return true;
    }
    return false;
}

std::vector<std::string> Camps::wear(double days, double now)
{
    std::vector<std::string> ruined;
    for (auto& [id, s] : sites_)
    {
        if (s.state != "standing")
            continue;
        const double rate = (now - s.visited > AbandonedSeconds ? 4.0 : 1.0) * days;
        bool anything = false;
        for (auto& [sid, st] : structures_)
        {
            if (st.site != id || !st.built)
                continue;
            // Tents first, stone last: a wear by how long the kind takes to build.
            const auto* k = kind(st.kind);
            const double toughness = k ? std::max(1.0, std::sqrt(k->hours)) : 1;
            st.condition = std::max(0.0, st.condition - 2 * rate / toughness);
            anything |= st.condition > 0;
        }
        if (!anything && built(id) == 0 && !structuresOf(id).empty() && now - s.visited > AbandonedSeconds)
        {
            s.state = "ruin";
            ruined.push_back(id);
        }
    }
    return ruined;
}

void Camps::abandon(const std::string& id)
{
    if (auto* s = site(id))
        s->state = "ruin";
}

Value Camps::save() const
{
    auto root = Value::object();
    root.add("next", double(next_));
    auto sites = Value::array();
    for (const auto& [id, s] : sites_)
    {
        auto j = Value::object();
        j.add("id", s.id); j.add("chapter", s.chapter); j.add("cell", s.cell); j.add("name", s.name);
        j.add("x", s.x); j.add("y", s.y); j.add("founded", s.founded); j.add("visited", s.visited); j.add("state", s.state);
        sites.push(j);
    }
    root.add("sites", sites);
    auto structures = Value::array();
    for (const auto& [id, st] : structures_)
    {
        auto j = Value::object();
        j.add("id", st.id); j.add("site", st.site); j.add("kind", st.kind); j.add("x", st.x); j.add("y", st.y);
        j.add("work", st.work); j.add("condition", st.condition); j.add("built", st.built);
        structures.push(j);
    }
    root.add("structures", structures);
    auto staff = Value::array();
    for (const auto& [npc, st] : staff_)
    {
        auto j = Value::object();
        j.add("npc", st.npc); j.add("site", st.site); j.add("role", st.role); j.add("wage", double(st.wage)); j.add("paidTo", st.paidTo);
        staff.push(j);
    }
    root.add("staff", staff);
    return root;
}

void Camps::load(const Value& saved)
{
    *this = Camps{};
    if (!saved.isObject())
        return;
    next_ = std::max<std::uint64_t>(1, std::uint64_t(saved.number("next", 1)));
    for (const auto& j : saved.array("sites"))
    {
        Site s{j.string("id"), j.string("chapter"), j.string("cell"), j.string("name"), int(j.number("x")), int(j.number("y")),
               j.number("founded"), j.number("visited"), j.string("state", "standing")};
        if (!s.id.empty())
            sites_[s.id] = s;
    }
    for (const auto& j : saved.array("structures"))
    {
        Structure st{j.string("id"), j.string("site"), j.string("kind"), int(j.number("x")), int(j.number("y")), j.number("work"),
                     std::clamp(j.number("condition", 100), 0.0, 100.0), j.boolean("built")};
        if (!st.id.empty() && sites_.count(st.site) && kind(st.kind))
            structures_[st.id] = st;
    }
    for (const auto& j : saved.array("staff"))
    {
        Staff st{j.string("npc"), j.string("site"), j.string("role", "hand"), std::int64_t(j.number("wage")), j.number("paidTo")};
        if (!st.npc.empty() && sites_.count(st.site))
            staff_[st.npc] = st;
    }
}
} // namespace ratw::camp
