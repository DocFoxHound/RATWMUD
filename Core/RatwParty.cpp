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
    {
        for (const auto& m : p->members)
            if (m != who)
                out.push_back(m);
        for (const auto& c : p->companions)
            if (c.id != who)
                out.push_back(c.id);
    }
    return out;
}

bool Parties::together(const std::string& a, const std::string& b) const
{
    const auto pa = partyOf_.find(a), pb = partyOf_.find(b);
    return a != b && pa != partyOf_.end() && pb != partyOf_.end() && pa->second == pb->second;
}

Outcome Parties::invite(const std::string& from, const std::string& to, double now)
{
    if (companion(from) || companion(to))
        return {false, "Residents travel with a party by their own choice, not by invitation."};
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
        if (of(from))
            return {false, "They are already in a party."};
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
    if (companion(who))
        return releaseCompanion(who);
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
    if (companion(who))
        return releaseCompanion(who);
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
    if (companion(who))
        return {false, "Only a player can lead a party."};
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

Outcome Parties::setGoal(const std::string& leader, const std::string& goal)
{
    const auto* p = of(leader);
    if (!p || p->leader != leader)
        return {false, "Only the party's leader can do that."};
    if (goal.size() > 120)
        return {false, "Say it in fewer words."};
    parties_[p->id].goal = goal;
    return {true, {}};
}

Outcome Parties::addCompanion(const std::string& player, Companion c)
{
    if (c.id.empty() || c.id == player)
        return {false, "There is no one to come."};
    if (of(c.id))
        return {false, "They are already travelling with someone."};
    const auto* mine = of(player);
    if (mine && mine->leader != player)
        return {false, "Only the party's leader can ask someone along."};
    if (mine && mine->companions.size() >= MaxCompanions)
        return {false, "Your party already has as many travelling with it as it can look after."};
    c.by = player;
    if (!mine)
    {
        Party p;
        p.id = "party-" + std::to_string(next_++);
        p.leader = player;
        p.members.push_back(player);
        partyOf_[player] = p.id;
        invites_.erase(player);
        mine = &(parties_[p.id] = std::move(p));
    }
    auto& p = parties_[mine->id];
    p.companions.push_back(c);
    partyOf_[c.id] = p.id;
    return {true, p.id};
}

const Companion* Parties::companion(const std::string& npc) const
{
    if (const auto* p = of(npc))
        for (const auto& c : p->companions)
            if (c.id == npc)
                return &c;
    return nullptr;
}

Companion* Parties::companion(const std::string& npc)
{
    const auto found = partyOf_.find(npc);
    if (found == partyOf_.end())
        return nullptr;
    for (auto& c : parties_[found->second].companions)
        if (c.id == npc)
            return &c;
    return nullptr;
}

Outcome Parties::releaseCompanion(const std::string& npc)
{
    const auto found = partyOf_.find(npc);
    if (found == partyOf_.end() || !companion(npc))
        return {false, "They aren't travelling with a party."};
    const std::string id = found->second;
    auto& p = parties_[id];
    p.companions.erase(std::remove_if(p.companions.begin(), p.companions.end(), [&](const Companion& c) { return c.id == npc; }),
                       p.companions.end());
    partyOf_.erase(npc);
    released_.push_back(npc);
    if (p.members.size() + p.companions.size() < 2)
        end(id);
    return {true, {}};
}

std::size_t Parties::companionCount() const
{
    std::size_t n = 0;
    for (const auto& [id, p] : parties_)
        n += p.companions.size();
    return n;
}

std::vector<std::string> Parties::takeReleased()
{
    auto out = std::move(released_);
    released_.clear();
    return out;
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
    // No player left, or nobody left to travel with: the party ends (its residents go home).
    if (p.members.empty() || p.members.size() + p.companions.size() < 2)
    {
        end(id);
        return;
    }
    for (auto& c : p.companions)
        if (c.by == who)
            c.by = p.leader == who ? p.members.front() : p.leader;   // Whoever leads now looks after them.
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
    for (const auto& c : found->second.companions)
    {
        partyOf_.erase(c.id);
        released_.push_back(c.id);
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
        auto companions = Value::array();
        for (const auto& c : p.companions)
        {
            auto k = Value::object();
            k.add("id", c.id);
            k.add("by", c.by);
            k.add("reason", c.reason);
            k.add("wage", double(c.wage));
            k.add("joined", c.joined);
            k.add("paidThrough", c.paidThrough);
            k.add("waiting", c.waiting);
            companions.push(k);
        }
        j.add("companions", companions);
        if (!p.goal.empty())
            j.add("goal", p.goal);
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
        for (const auto& k : j.array("companions"))
        {
            Companion c{k.string("id"), k.string("by"), k.string("reason", "friend"), std::int64_t(k.number("wage")),
                        k.number("joined"), k.number("paidThrough"), k.boolean("waiting")};
            if (!c.id.empty() && !partyOf_.count(c.id) && p.companions.size() < MaxCompanions)
                p.companions.push_back(c);
        }
        p.goal = j.string("goal");
        if (p.id.empty() || parties_.count(p.id) || p.members.empty() || p.members.size() + p.companions.size() < 2)
        {
            for (const auto& m : p.members)
                offline_.erase(m);
            continue;
        }
        if (std::find(p.members.begin(), p.members.end(), p.leader) == p.members.end())
            p.leader = p.members.front();
        for (const auto& m : p.members)
            partyOf_[m] = p.id;
        for (auto& c : p.companions)
        {
            partyOf_[c.id] = p.id;
            if (std::find(p.members.begin(), p.members.end(), c.by) == p.members.end())
                c.by = p.leader;
        }
        parties_[p.id] = std::move(p);
    }
    for (const auto& who : saved.array("neverAutoJoin"))
        if (who.isString())
            neverAutoJoin_.insert(who.asString());
}
} // namespace ratw::party
