// Parties (RatwParty.h; Docs/Design/32-parties-chapters-factions.md, Part 2).
#include "RatwParty.h"

#include <algorithm>

namespace ratw::party
{
using json::Value;

const Party* Parties::of(const std::string& who) const
{
    const auto found = partyOf_.find(who);
    return found == partyOf_.end() ? nullptr : byId(found->second);
}

const Party* Parties::byId(const std::string& id) const
{
    const auto found = parties_.find(id);
    return found == parties_.end() ? nullptr : &found->second;
}

std::vector<std::string> Parties::mates(const std::string& who) const
{
    std::vector<std::string> out;
    if (const auto* p = of(who))
        for (const auto& m : p->members)
            if (m != who)
                out.push_back(m);
    return out;
}

bool Parties::together(const std::string& a, const std::string& b) const
{
    const auto pa = partyOf_.find(a), pb = partyOf_.find(b);
    return a != b && pa != partyOf_.end() && pb != partyOf_.end() && pa->second == pb->second;
}

Outcome Parties::invite(const std::string& from, const std::string& to, double now)
{
    if (from == to)
        return {false, "You are already in your own company."};
    const auto* mine = of(from);
    if (mine && mine->leader != from)
        return {false, "Only the party's leader can invite."};
    if (mine && mine->members.size() >= MaxPlayers)
        return {false, "Your party is full."};
    if (together(from, to))
        return {false, "They are already in your party."};
    if (of(to))
        return {false, "They are already in a party."};
    if (const auto* waiting = inviteFor(to, now); waiting && waiting->from != from)
        return {false, "Someone else's invitation is waiting for them."};
    invites_[to] = {from, to, now + InviteSeconds};
    return {true, {}};
}

const Invite* Parties::inviteFor(const std::string& to, double now) const
{
    const auto found = invites_.find(to);
    return found == invites_.end() || found->second.expires < now ? nullptr : &found->second;
}

Outcome Parties::accept(const std::string& to, double now)
{
    const auto* waiting = inviteFor(to, now);
    if (!waiting)
        return {false, "No invitation is waiting for you."};
    const std::string from = waiting->from;
    invites_.erase(to);
    if (of(to))
        return {false, "You are already in a party."};
    const auto* theirs = of(from);
    if (theirs && theirs->leader != from)
        return {false, "They no longer lead their party."};
    if (theirs && theirs->members.size() >= MaxPlayers)
        return {false, "Their party is full."};
    if (!theirs)
    {
        Party p;
        p.id = "party-" + std::to_string(next_++);
        p.leader = from;
        p.members.push_back(from);
        partyOf_[from] = p.id;
        theirs = &(parties_[p.id] = std::move(p));
        invites_.erase(from);                    // (One waiting for the founder is moot now.)
    }
    auto& p = parties_[theirs->id];
    p.members.push_back(to);
    partyOf_[to] = p.id;
    return {true, p.id};
}

Outcome Parties::decline(const std::string& to)
{
    if (!invites_.erase(to))
        return {false, "No invitation is waiting for you."};
    return {true, {}};
}

Outcome Parties::leave(const std::string& who)
{
    if (!of(who))
        return {false, "You are not in a party."};
    drop(who);
    return {true, {}};
}

Outcome Parties::remove(const std::string& leader, const std::string& who)
{
    const auto* p = of(leader);
    if (!p || p->leader != leader)
        return {false, "Only the party's leader can do that."};
    if (who == leader)
        return {false, "Leave the party instead."};
    if (!together(leader, who))
        return {false, "They are not in your party."};
    drop(who);
    return {true, {}};
}

Outcome Parties::lead(const std::string& leader, const std::string& who)
{
    const auto* p = of(leader);
    if (!p || p->leader != leader)
        return {false, "Only the party's leader can do that."};
    if (!together(leader, who))
        return {false, "They are not in your party."};
    parties_[p->id].leader = who;
    return {true, {}};
}

Outcome Parties::disband(const std::string& leader)
{
    const auto* p = of(leader);
    if (!p || p->leader != leader)
        return {false, "Only the party's leader can do that."};
    end(p->id);
    return {true, {}};
}

void Parties::drop(const std::string& who)
{
    const auto found = partyOf_.find(who);
    if (found == partyOf_.end())
        return;
    const std::string id = found->second;
    partyOf_.erase(found);
    offline_.erase(who);
    auto& p = parties_[id];
    p.members.erase(std::remove(p.members.begin(), p.members.end(), who), p.members.end());
    if (p.members.size() < 2)
    {
        end(id);
        return;
    }
    if (p.leader == who)
        p.leader = p.members.front();
}

void Parties::end(const std::string& partyId)
{
    const auto found = parties_.find(partyId);
    if (found == parties_.end())
        return;
    for (const auto& m : found->second.members)
    {
        partyOf_.erase(m);
        offline_.erase(m);
    }
    parties_.erase(found);
}

std::vector<std::pair<std::string, std::string>> Parties::tick(double now, const std::function<bool(const std::string&)>& online)
{
    std::vector<std::pair<std::string, std::string>> lost;
    for (auto it = invites_.begin(); it != invites_.end();)
        it = it->second.expires < now ? invites_.erase(it) : std::next(it);
    std::vector<std::string> ending;
    for (const auto& [id, p] : parties_)
    {
        bool anyone = false;
        for (const auto& m : p.members)
            if (online(m))
            {
                anyone = true;
                offline_.erase(m);
            }
            else
                offline_.emplace(m, now);
        // Nobody in the world: the party waits as long as its last member's place does, then goes.
        double latest = 0;
        for (const auto& m : p.members)
            if (!online(m))
                latest = std::max(latest, offline_[m]);
        if (!anyone && now - latest > HoldSeconds)
            ending.push_back(id);
    }
    for (const auto& id : ending)
        end(id);
    std::vector<std::string> gone;
    for (const auto& [who, since] : offline_)
        if (now - since > HoldSeconds)
            gone.push_back(who);
    for (const auto& who : gone)
    {
        const auto* p = of(who);
        if (p)
            lost.emplace_back(who, p->id);
        drop(who);
    }
    return lost;
}

double Parties::offlineSince(const std::string& who) const
{
    const auto found = offline_.find(who);
    return found == offline_.end() ? -1 : found->second;
}

void Parties::setAutoJoin(const std::string& who, bool on)
{
    if (on)
        neverAutoJoin_.erase(who);
    else
        neverAutoJoin_.insert(who);
}

Value Parties::save() const
{
    auto root = Value::object();
    root.add("next", double(next_));
    auto list = Value::array();
    for (const auto& [id, p] : parties_)
    {
        auto j = Value::object();
        j.add("id", id);
        j.add("leader", p.leader);
        auto members = Value::array();
        for (const auto& m : p.members)
        {
            auto k = Value::object();
            k.add("id", m);
            if (const auto found = offline_.find(m); found != offline_.end())
                k.add("offline", found->second);
            members.push(k);
        }
        j.add("members", members);
        list.push(j);
    }
    root.add("parties", list);
    auto never = Value::array();
    for (const auto& who : neverAutoJoin_)
        never.push(who);
    root.add("neverAutoJoin", never);
    return root;
}

void Parties::load(const Value& saved)
{
    *this = Parties{};
    if (!saved.isObject())
        return;
    next_ = std::max<std::uint64_t>(1, std::uint64_t(saved.number("next", 1)));
    for (const auto& j : saved.array("parties"))
    {
        Party p;
        p.id = j.string("id");
        p.leader = j.string("leader");
        for (const auto& k : j.array("members"))
        {
            const auto m = k.string("id");
            if (m.empty() || partyOf_.count(m) || std::find(p.members.begin(), p.members.end(), m) != p.members.end() ||
                p.members.size() >= MaxPlayers)
                continue;
            p.members.push_back(m);
            if (k["offline"].isNumber())
                offline_[m] = k.number("offline");
        }
        if (p.id.empty() || parties_.count(p.id) || p.members.size() < 2)
        {
            for (const auto& m : p.members)
                offline_.erase(m);
            continue;
        }
        if (std::find(p.members.begin(), p.members.end(), p.leader) == p.members.end())
            p.leader = p.members.front();
        for (const auto& m : p.members)
            partyOf_[m] = p.id;
        parties_[p.id] = std::move(p);
    }
    for (const auto& who : saved.array("neverAutoJoin"))
        if (who.isString())
            neverAutoJoin_.insert(who.asString());
}
} // namespace ratw::party
