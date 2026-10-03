// Factions in play (RatwFactions.h; Docs/Design/32-parties-chapters-factions.md, Part 4).
#include "RatwFactions.h"

#include <algorithm>
#include <cmath>

namespace ratw::faction
{
using json::Value;

const char* band(double d)
{
    return d >= 75 ? "Sworn" : d >= 40 ? "Trusted" : d >= 15 ? "Known well" : d > -15 ? "Neutral" : d > -40 ? "Distrusted" : d > -75 ? "Hostile" : "Enemy";
}

std::string defaultStance(double d)
{
    return d >= 75 ? "allied" : d >= 15 ? "friendly" : d > -15 ? "neutral" : d > -40 ? "tense" : "hostile";
}

void Factions::define(const Faction& f)
{
    if (!f.id.empty())
        factions_[f.id] = f;
}

void Factions::clear()
{
    factions_.clear();
    members_.clear();
    explicit_.clear();
    relations_.clear();
}

const Faction* Factions::find(const std::string& id) const
{
    const auto found = factions_.find(id);
    return found == factions_.end() ? nullptr : &found->second;
}

void Factions::setMember(const std::string& npc, const std::string& faction, const std::string& rank, bool explicitly)
{
    if (!explicitly && explicit_.count(npc))
        return;
    members_[npc] = {faction, rank};
    if (explicitly)
        explicit_.insert(npc);
}

void Factions::clearDerived()
{
    for (auto it = members_.begin(); it != members_.end();)
        it = explicit_.count(it->first) ? std::next(it) : members_.erase(it);
}

const std::pair<std::string, std::string>* Factions::memberOf(const std::string& npc) const
{
    const auto found = members_.find(npc);
    return found == members_.end() ? nullptr : &found->second;
}

void Factions::setRelation(const std::string& a, const std::string& b, const Relation& r)
{
    relations_[a + "|" + b] = r;
}

Relation Factions::relation(const std::string& a, const std::string& b) const
{
    const auto found = relations_.find(a + "|" + b);
    return found == relations_.end() ? Relation{} : found->second;
}

double Factions::earned(const std::string& faction, const std::string& chapter) const
{
    const auto found = standings_.find(faction + "|" + chapter);
    return found == standings_.end() ? 0 : found->second.earned;
}

std::vector<std::string> Factions::stillCounted(const std::string& faction, const std::string& chapter) const
{
    std::vector<std::string> out;
    for (const auto& e : expulsions_)
        if (e.chapter == chapter && !e.heardBy.count(faction))
            out.push_back(e.who);
    return out;
}

double Factions::effective(const std::string& faction, const std::string& chapter, const std::vector<std::string>& members) const
{
    const double base = earned(faction, chapter);
    // A Trusted Chapter's members carry double weight (4.4: scrutiny).
    const double weight = base >= 40 ? 2 : 1;
    double weighs = 0;
    for (const auto& m : members)
        weighs += burden(faction, m);
    for (const auto& m : stillCounted(faction, chapter))
        if (std::find(members.begin(), members.end(), m) == members.end())
            weighs += burden(faction, m);
    return std::clamp(base - weight * weighs, -100.0, 100.0);
}

std::string Factions::stanceOf(const std::string& faction, const std::string& chapter, const std::vector<std::string>& members) const
{
    if (const auto found = standings_.find(faction + "|" + chapter); found != standings_.end() && !found->second.stance.empty())
        return found->second.stance;
    return defaultStance(effective(faction, chapter, members));
}

double Factions::change(const std::string& faction, const std::string& chapter, double delta, const std::string& reason, double day,
                        double weeklyCap, bool ripple)
{
    if (!find(faction) || delta == 0)
        return 0;
    auto& s = standings_[faction + "|" + chapter];
    const double week = std::floor(day / WeekDays);
    if (s.week != week)
    {
        s.week = week;
        s.weekly.clear();
    }
    if (weeklyCap > 0)
    {
        const double room = weeklyCap - std::abs(s.weekly[reason]);
        if (room <= 0)
            return 0;
        delta = delta > 0 ? std::min(delta, room) : std::max(delta, -room);
        s.weekly[reason] += delta;
    }
    const double before = s.earned;
    s.earned = std::clamp(s.earned + delta, -100.0, 100.0);
    s.log.push_back({reason, s.earned - before, day});
    if (s.log.size() > 60)
        s.log.erase(s.log.begin());
    // Ripples through the faction relations: allies a quarter with, enemies a quarter against.
    if (ripple)
        for (const auto& [id, f] : factions_)
        {
            if (id == faction)
                continue;
            const auto r = relation(id, faction);
            const double towards = r.stance == "allied" || r.stance == "friendly" ? 0.25 : r.stance == "hostile" || r.stance == "war" ? -0.25 : 0;
            if (towards != 0)
                change(id, chapter, (s.earned - before) * towards, "word from " + factions_.at(faction).name, day, 0, false);
        }
    return s.earned - before;
}

void Factions::setStance(const std::string& faction, const std::string& chapter, const std::string& stance)
{
    standings_[faction + "|" + chapter].stance = stance;
}

const Standing* Factions::standing(const std::string& faction, const std::string& chapter) const
{
    const auto found = standings_.find(faction + "|" + chapter);
    return found == standings_.end() ? nullptr : &found->second;
}

void Factions::drift(double day)
{
    const double week = std::floor(day / WeekDays);
    if (lastDrift_ < 0)
        lastDrift_ = week;
    while (lastDrift_ < week)
    {
        ++lastDrift_;
        for (auto& [key, s] : standings_)
            s.earned = s.earned > 0 ? std::max(0.0, s.earned - 1) : std::min(0.0, s.earned + 1);
        for (auto& [faction, wolves] : burdens_)
            for (auto it = wolves.begin(); it != wolves.end();)
            {
                it->second.amount = std::max(0.0, it->second.amount - (it->second.restitution ? 2 : 1));
                it = it->second.amount <= 0 ? wolves.erase(it) : std::next(it);
            }
    }
}

void Factions::addBurden(const std::string& faction, const std::string& who, double amount, const std::string& incident, double day)
{
    if (!find(faction) || amount <= 0)
        return;
    auto& b = burdens_[faction][who];
    if (!incident.empty() && std::find(b.incidents.begin(), b.incidents.end(), incident) != b.incidents.end())
        return;                                   // (One incident weighs once.)
    b.amount = std::min(100.0, b.amount + amount);
    if (!incident.empty())
        b.incidents.push_back(incident);
    if (b.incidents.size() > 20)
        b.incidents.erase(b.incidents.begin());
    b.last = day;
    b.restitution = false;
}

void Factions::restitution(const std::string& who)
{
    for (auto& [faction, wolves] : burdens_)
        if (const auto found = wolves.find(who); found != wolves.end())
            found->second.restitution = true;
}

double Factions::burden(const std::string& faction, const std::string& who) const
{
    const auto f = burdens_.find(faction);
    if (f == burdens_.end())
        return 0;
    const auto w = f->second.find(who);
    return w == f->second.end() ? 0 : w->second.amount;
}

const std::map<std::string, Burden>* Factions::burdensOf(const std::string& faction) const
{
    const auto found = burdens_.find(faction);
    return found == burdens_.end() ? nullptr : &found->second;
}

void Factions::expelled(const std::string& chapter, const std::string& who, double day)
{
    expulsions_.push_back({chapter, who, day, {}});
    if (expulsions_.size() > 500)
        expulsions_.erase(expulsions_.begin());
}

void Factions::hear(const std::string& faction, const std::string& chapter, double day)
{
    (void)day;
    for (auto& e : expulsions_)
        if (e.chapter == chapter)
            e.heardBy.insert(faction);
}

void Factions::spread(double day)
{
    for (auto& e : expulsions_)
        if (day - e.at >= ExpulsionNewsDays)
            for (const auto& [id, f] : factions_)
                e.heardBy.insert(id);
    expulsions_.erase(std::remove_if(expulsions_.begin(), expulsions_.end(),
                                     [&](const Expulsion& e) { return e.heardBy.size() >= factions_.size() && day - e.at > 30; }),
                      expulsions_.end());
}

Mission* Factions::mission(const std::string& id)
{
    for (auto& m : missions_)
        if (m.id == id)
            return &m;
    return nullptr;
}

Value Factions::save() const
{
    // Factions, members and relations come from the world each start; standing, burdens, news and missions are saved.
    auto root = Value::object();
    auto standings = Value::array();
    for (const auto& [key, s] : standings_)
    {
        auto j = Value::object();
        j.add("key", key);
        j.add("earned", s.earned);
        j.add("stance", s.stance);
        j.add("week", s.week);
        auto weekly = Value::object();
        for (const auto& [reason, v] : s.weekly)
            weekly.add(reason, v);
        j.add("weekly", weekly);
        auto log = Value::array();
        for (const auto& c : s.log)
        {
            auto k = Value::object();
            k.add("reason", c.reason);
            k.add("delta", c.delta);
            k.add("at", c.at);
            log.push(k);
        }
        j.add("log", log);
        standings.push(j);
    }
    root.add("standings", standings);
    auto burdens = Value::array();
    for (const auto& [faction, wolves] : burdens_)
        for (const auto& [who, b] : wolves)
        {
            auto j = Value::object();
            j.add("faction", faction);
            j.add("who", who);
            j.add("amount", b.amount);
            j.add("last", b.last);
            j.add("restitution", b.restitution);
            auto incidents = Value::array();
            for (const auto& i : b.incidents)
                incidents.push(i);
            j.add("incidents", incidents);
            burdens.push(j);
        }
    root.add("burdens", burdens);
    auto expulsions = Value::array();
    for (const auto& e : expulsions_)
    {
        auto j = Value::object();
        j.add("chapter", e.chapter);
        j.add("who", e.who);
        j.add("at", e.at);
        auto heard = Value::array();
        for (const auto& f : e.heardBy)
            heard.push(f);
        j.add("heardBy", heard);
        expulsions.push(j);
    }
    root.add("expulsions", expulsions);
    auto missions = Value::array();
    for (const auto& m : missions_)
    {
        auto j = Value::object();
        j.add("id", m.id); j.add("faction", m.faction); j.add("kind", m.kind); j.add("tier", m.tier);
        j.add("item", m.item); j.add("quantity", m.quantity); j.add("official", m.official); j.add("to", m.to);
        j.add("cell", m.cell); j.add("place", m.place); j.add("seconds", m.seconds); j.add("held", m.held);
        j.add("coins", double(m.coins)); j.add("standing", m.standing); j.add("renown", m.renown);
        j.add("chapter", m.chapter); j.add("taker", m.taker); j.add("state", m.state); j.add("expires", m.expires);
        missions.push(j);
    }
    root.add("missions", missions);
    root.add("nextMission", double(nextMission_));
    root.add("lastDrift", lastDrift_);
    return root;
}

void Factions::load(const Value& saved)
{
    standings_.clear();
    burdens_.clear();
    expulsions_.clear();
    missions_.clear();
    if (!saved.isObject())
        return;
    for (const auto& j : saved.array("standings"))
    {
        Standing s;
        s.earned = std::clamp(j.number("earned"), -100.0, 100.0);
        s.stance = j.string("stance");
        s.week = j.number("week", -1);
        for (const auto& [reason, v] : j.object("weekly").fields())
            s.weekly[reason] = v.asNumber();
        for (const auto& k : j.array("log"))
            s.log.push_back({k.string("reason"), k.number("delta"), k.number("at")});
        standings_[j.string("key")] = s;
    }
    for (const auto& j : saved.array("burdens"))
    {
        Burden b;
        b.amount = j.number("amount");
        b.last = j.number("last");
        b.restitution = j.boolean("restitution");
        for (const auto& i : j.array("incidents"))
            if (i.isString())
                b.incidents.push_back(i.asString());
        burdens_[j.string("faction")][j.string("who")] = b;
    }
    for (const auto& j : saved.array("expulsions"))
    {
        Expulsion e{j.string("chapter"), j.string("who"), j.number("at"), {}};
        for (const auto& f : j.array("heardBy"))
            if (f.isString())
                e.heardBy.insert(f.asString());
        expulsions_.push_back(e);
    }
    for (const auto& j : saved.array("missions"))
    {
        Mission m;
        m.id = j.string("id"); m.faction = j.string("faction"); m.kind = j.string("kind"); m.tier = int(j.number("tier", 1));
        m.item = j.string("item"); m.quantity = int(j.number("quantity")); m.official = j.string("official"); m.to = j.string("to");
        m.cell = j.string("cell"); m.place = j.string("place"); m.seconds = j.number("seconds"); m.held = j.number("held");
        m.coins = std::int64_t(j.number("coins")); m.standing = j.number("standing"); m.renown = int(j.number("renown"));
        m.chapter = j.string("chapter"); m.taker = j.string("taker"); m.state = j.string("state", "open"); m.expires = j.number("expires");
        if (!m.id.empty())
            missions_.push_back(m);
    }
    nextMission_ = std::max<std::uint64_t>(1, std::uint64_t(saved.number("nextMission", 1)));
    lastDrift_ = saved.number("lastDrift", -1);
}
} // namespace ratw::faction
