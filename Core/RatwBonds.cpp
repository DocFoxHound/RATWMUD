#include "RatwBonds.h"

#include <algorithm>
#include <cmath>

namespace ratw
{
namespace
{
// A step towards a limit shrinks as the value nears it; a step back from it doesn't.
double grow(double value, double step, double low, double high)
{
    if (!std::isfinite(step) || step == 0)
        return value;
    const double limit = step > 0 ? high : low;
    const double room = std::abs(limit - value) / std::max(1.0, std::abs(high - low) / 2);
    const bool towards = (step > 0) == (limit > value);
    const double moved = value + (towards ? step * std::clamp(room, 0.0, 1.0) : step);
    return std::clamp(moved, low, high);
}
double strength(const Bond& b)
{
    return b.familiarity + std::abs(b.affinity) + std::abs(b.trust) + b.fear + std::abs(b.respect) +
           (b.owed != 0 ? 50.0 : 0.0);
}
bool empty(const Bond& b)
{
    return b.familiarity < .5 && std::abs(b.affinity) < .5 && std::abs(b.trust) < .5 && b.fear < .5 &&
           std::abs(b.respect) < .5 && b.owed == 0;
}
bool valid(const Bond& b)
{
    const auto in = [](double v, double low, double high) { return std::isfinite(v) && v >= low && v <= high; };
    return in(b.affinity, -100, 100) && in(b.trust, -100, 100) && in(b.familiarity, 0, 100) && in(b.fear, 0, 100) &&
           in(b.respect, -100, 100) && std::isfinite(b.lastContact) && std::llabs(b.owed) <= 100000000;
}
const char* degree(double v, const char* faint, const char* some, const char* strong)
{
    const double a = std::abs(v);
    return a >= 60 ? strong : a >= 25 ? some : a >= 8 ? faint : nullptr;
}
} // namespace

const Bond* Bonds::find(const std::string& holder, const std::string& other) const
{
    const auto mine = bonds_.find(holder);
    if (mine == bonds_.end())
        return nullptr;
    const auto found = mine->second.find(other);
    return found == mine->second.end() ? nullptr : &found->second;
}

const std::map<std::string, Bond>* Bonds::of(const std::string& holder) const
{
    const auto mine = bonds_.find(holder);
    return mine == bonds_.end() ? nullptr : &mine->second;
}

void Bonds::change(const std::string& holder, const std::string& other, const BondChange& by, double day)
{
    if (holder.empty() || other.empty() || holder == other)
        return;
    auto& mine = bonds_[holder];
    auto& b = mine[other];
    b.affinity = grow(b.affinity, by.affinity, -100, 100);
    b.trust = grow(b.trust, by.trust, -100, 100);
    b.familiarity = grow(b.familiarity, by.familiarity, 0, 100);
    b.fear = grow(b.fear, by.fear, 0, 100);
    b.respect = grow(b.respect, by.respect, -100, 100);
    if (std::isfinite(day))
        b.lastContact = std::max(b.lastContact, day);
    trim(mine, other);
}

void Bonds::mutual(const std::string& a, const std::string& b, const BondChange& by, double day)
{
    change(a, b, by, day);
    change(b, a, by, day);
}

void Bonds::addOwed(const std::string& holder, const std::string& other, std::int64_t pennies, double day)
{
    if (holder.empty() || other.empty() || holder == other || pennies == 0)
        return;
    auto& mine = bonds_[holder];
    auto& b = mine[other];
    b.owed = std::clamp<std::int64_t>(b.owed + pennies, -100000000, 100000000);
    if (std::isfinite(day))
        b.lastContact = std::max(b.lastContact, day);
    trim(mine, other);
}

void Bonds::trim(std::map<std::string, Bond>& mine, const std::string& keep)
{
    while (mine.size() > MaxPerHolder)
    {
        auto faintest = mine.end();
        for (auto it = mine.begin(); it != mine.end(); ++it)
            if (it->first != keep && (faintest == mine.end() || strength(it->second) < strength(faintest->second)))
                faintest = it;
        if (faintest == mine.end())
            return;
        mine.erase(faintest);
    }
}

void Bonds::fade(double day)
{
    for (auto holder = bonds_.begin(); holder != bonds_.end();)
    {
        auto& mine = holder->second;
        for (auto it = mine.begin(); it != mine.end();)
        {
            auto& b = it->second;
            if (day - b.lastContact > 3)
                b.familiarity = std::max(0.0, b.familiarity - .5);
            b.affinity *= .99;
            b.fear *= .9;
            b.respect *= .995;
            it = empty(b) ? mine.erase(it) : std::next(it);
        }
        holder = mine.empty() ? bonds_.erase(holder) : std::next(holder);
    }
}

void Bonds::forget(const std::string& id)
{
    bonds_.erase(id);
    for (auto holder = bonds_.begin(); holder != bonds_.end();)
    {
        holder->second.erase(id);
        holder = holder->second.empty() ? bonds_.erase(holder) : std::next(holder);
    }
}

std::string Bonds::describe(const std::string& holder, const std::string& other, const std::string& otherName) const
{
    const auto* b = find(holder, other);
    if (!b)
        return {};
    std::vector<std::string> parts;
    const char* known = b->familiarity >= 60 ? "know them well" : b->familiarity >= 25 ? "know them"
                        : b->familiarity >= 5 ? "have met them" : nullptr;
    if (known)
        parts.push_back(known);
    if (const char* d = degree(b->affinity, "a little", "", "very much"))
        parts.push_back(std::string(b->affinity > 0 ? "like them" : "dislike them") + (*d ? std::string(" ") + d : ""));
    if (const char* d = degree(b->trust, "a little", "", "completely"))
        parts.push_back(std::string(b->trust > 0 ? "trust them" : "distrust them") + (*d ? std::string(" ") + d : ""));
    if (const char* d = degree(b->fear, "a little", "", "greatly"))
        parts.push_back(std::string("fear them") + (*d ? std::string(" ") + d : ""));
    if (const char* d = degree(b->respect, "a little", "", "deeply"))
        parts.push_back(std::string(b->respect > 0 ? "respect them" : "look down on them") + (*d ? std::string(" ") + d : ""));
    std::string out;
    if (!parts.empty())
    {
        out = "You ";
        for (std::size_t i = 0; i < parts.size(); ++i)
            out += (i == 0 ? "" : i + 1 == parts.size() ? " and " : ", ") + parts[i];
        out += ".";
        // "You know them" reads better with the name first.
        const auto them = out.find("them");
        if (them != std::string::npos && !otherName.empty())
            out.replace(them, 4, otherName);
    }
    if (b->owed > 0)
        out += (out.empty() ? "" : " ") + otherName + " owes you " + std::to_string(b->owed) + " penn" +
               (b->owed == 1 ? "y." : "ies.");
    else if (b->owed < 0)
        out += (out.empty() ? "" : " ") + std::string("You owe ") + otherName + " " + std::to_string(-b->owed) + " penn" +
               (b->owed == -1 ? "y." : "ies.");
    return out;
}

std::size_t Bonds::count() const
{
    std::size_t n = 0;
    for (const auto& [holder, mine] : bonds_)
        n += mine.size();
    return n;
}

std::vector<SavedBond> Bonds::save() const
{
    std::vector<SavedBond> out;
    out.reserve(count());
    for (const auto& [holder, mine] : bonds_)
        for (const auto& [other, bond] : mine)
            out.push_back({holder, other, bond});
    return out;
}

bool Bonds::restore(const std::vector<SavedBond>& saved)
{
    std::map<std::string, std::map<std::string, Bond>> restored;
    for (const auto& s : saved)
    {
        if (s.holder.empty() || s.other.empty() || s.holder == s.other || s.holder.size() > 80 || s.other.size() > 80 ||
            !valid(s.bond) || !restored[s.holder].emplace(s.other, s.bond).second)
            return false;
        if (restored[s.holder].size() > MaxPerHolder)
            return false;
    }
    bonds_ = std::move(restored);
    return true;
}
} // namespace ratw
