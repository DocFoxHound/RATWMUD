#pragma once
// Parties (Docs/Design/32-parties-chapters-factions.md, Part 2): small, voluntary groups of players. One leader, up to
// six players, one party each. An invitation lasts a minute; a member who leaves the world keeps their place for ten,
// and a party with nobody in the world goes after ten. Pure rules, no world: the game (RatwGameParty.cpp) says who is
// in the world and words what happens; the checkpoint keeps it all (save/load).
#include "RatwJsonDoc.h"

#include <cstdint>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace ratw::party
{
constexpr std::size_t MaxPlayers = 6;
constexpr double InviteSeconds = 60, HoldSeconds = 600;
// Fights (doc 33): a member who can see a party mate's fight is pulled in after this, unless they choose to stay out;
// whoever was on the other side stays hostile to them for this long after.
constexpr double PullSeconds = 5, AggroSeconds = 300;

struct Party
{
    std::string id, leader;
    std::vector<std::string> members;     // In the order they joined; the leader among them.
};

struct Invite
{
    std::string from, to;
    double expires = 0;
};

struct Outcome
{
    bool ok = false;
    std::string message;                  // Why not, when not (no names: the game knows whom to tell what).
};

class Parties
{
  public:
    const Party* of(const std::string& who) const;
    const Party* byId(const std::string& id) const;
    // Everyone else in their party (empty when in none).
    std::vector<std::string> mates(const std::string& who) const;
    bool together(const std::string& a, const std::string& b) const;
    const std::map<std::string, Party>& all() const { return parties_; }

    // `from` asks `to`: from must lead their party, or be in none (one is made when `to` accepts).
    Outcome invite(const std::string& from, const std::string& to, double now);
    const Invite* inviteFor(const std::string& to, double now) const;
    // Takes up (or turns down) the invitation waiting for `to`. Accepting says which party they are now in.
    Outcome accept(const std::string& to, double now);
    Outcome decline(const std::string& to);
    // Leaving hands the lead to the next to have joined; a party of one is no party.
    Outcome leave(const std::string& who);
    Outcome remove(const std::string& leader, const std::string& who);
    Outcome lead(const std::string& leader, const std::string& who);
    Outcome disband(const std::string& leader);

    // Who is in the world now: members gone longer than HoldSeconds lose their place, parties with nobody in the world
    // for that long go, and old invitations lapse. Returns who lost their place (for the game to tell the rest).
    std::vector<std::pair<std::string, std::string>> tick(double now, const std::function<bool(const std::string&)>& online);
    double offlineSince(const std::string& who) const;

    // Fights: whether this wolf is pulled into a party mate's fight (a setting of their own; on by default).
    bool autoJoin(const std::string& who) const { return !neverAutoJoin_.count(who); }
    void setAutoJoin(const std::string& who, bool on);

    json::Value save() const;
    void load(const json::Value& saved);

  private:
    std::map<std::string, Party> parties_;
    std::map<std::string, std::string> partyOf_;
    std::map<std::string, Invite> invites_;          // By whom it is for: one waiting each.
    std::map<std::string, double> offline_;          // Members out of the world, and since when.
    std::set<std::string> neverAutoJoin_;
    std::uint64_t next_ = 1;

    void drop(const std::string& who);               // Out of their party, the party mended or ended.
    void end(const std::string& partyId);
};
} // namespace ratw::party
