// Chapters (RatwChapters.h; Docs/Design/32-parties-chapters-factions.md, Part 3).
#include "RatwChapters.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

namespace ratw::chapter
{
using json::Value;

namespace
{
std::string lower(std::string s)
{
    for (auto& ch : s)
        ch = char(std::tolower(static_cast<unsigned char>(ch)));
    return s;
}

std::string nameProblem(const std::string& name)
{
    if (name.size() < 3 || name.size() > 32)
        return "A Chapter's name is 3 to 32 letters.";
    for (std::size_t i = 0; i < name.size(); ++i)
    {
        const char ch = name[i];
        if (!std::isalpha(static_cast<unsigned char>(ch)) && !(i > 0 && (ch == ' ' || ch == '-' || ch == '\'')))
            return "A Chapter's name is letters, with spaces, hyphens or apostrophes between them.";
    }
    return {};
}
} // namespace

const Gate& gateFor(int level)
{
    // Placeholders (doc 32, 3.4): renown, active members, Chapter Stories told, and the ground.
    static const Gate gates[] = {{0, 3, 0, "a meeting place"},
                                 {0, 3, 0, "a meeting place"},
                                 {400, 5, 1, ""},
                                 {1500, 8, 3, "a rented hall held two weeks"},
                                 {4000, 12, 6, "a camp standing"},
                                 {10000, 16, 10, "a fortified camp, and a faction's friendship for its land"}};
    return gates[std::clamp(level, 0, 5)];
}

const char* levelName(int level)
{
    static const char* names[] = {"", "Gathering", "Lodge", "Company", "Hall", "Hold"};
    return names[std::clamp(level, 1, 5)];
}

std::string colourProblem(const std::string& hex)
{
    if (hex.size() != 7 || hex[0] != '#' || !std::all_of(hex.begin() + 1, hex.end(), [](char ch) { return std::isxdigit(static_cast<unsigned char>(ch)); }))
        return "Choose a colour.";
    const double r = std::strtol(hex.substr(1, 2).c_str(), nullptr, 16) / 255.0, g = std::strtol(hex.substr(3, 2).c_str(), nullptr, 16) / 255.0,
                 b = std::strtol(hex.substr(5, 2).c_str(), nullptr, 16) / 255.0;
    const double hi = std::max({r, g, b}), lo = std::min({r, g, b});
    const double chroma = hi - lo;
    double hue = 0;
    if (chroma > 0)
        hue = hi == r ? std::fmod((g - b) / chroma + 6, 6.0) * 60 : hi == g ? ((b - r) / chroma + 2) * 60 : ((r - g) / chroma + 4) * 60;
    if (chroma > 0.25 && (hue < 25 || hue > 335))
        return "Red is kept for the hostile. Choose another colour.";
    if (hi < 0.25)
        return "Choose a colour that shows against the dark.";
    return {};
}

const Chapter* Chapters::of(const std::string& who) const
{
    const auto found = chapterOf_.find(who);
    return found == chapterOf_.end() ? nullptr : byId(found->second);
}

const Chapter* Chapters::byId(const std::string& id) const
{
    const auto found = chapters_.find(id);
    return found == chapters_.end() ? nullptr : &found->second;
}

Chapter* Chapters::byId(const std::string& id)
{
    const auto found = chapters_.find(id);
    return found == chapters_.end() ? nullptr : &found->second;
}

const Member* Chapters::member(const std::string& who) const
{
    const auto* c = of(who);
    if (!c)
        return nullptr;
    const auto found = c->members.find(who);
    return found == c->members.end() ? nullptr : &found->second;
}

bool Chapters::together(const std::string& a, const std::string& b) const
{
    const auto ca = chapterOf_.find(a), cb = chapterOf_.find(b);
    return a != b && ca != chapterOf_.end() && cb != chapterOf_.end() && ca->second == cb->second;
}

Outcome Chapters::needRank(const std::string& by, int atLeast, const Chapter** c) const
{
    *c = of(by);
    const auto* m = member(by);
    if (!*c || !m)
        return {false, "You are not in a Chapter."};
    if (m->rank > atLeast)
        return {false, "Your rank doesn't allow that."};
    return {true, {}};
}

void Chapters::log(const std::string& chapterId, LogEntry entry)
{
    if (auto* c = byId(chapterId))
    {
        c->log.push_back(std::move(entry));
        if (c->log.size() > LogKept)
            c->log.erase(c->log.begin());
    }
}

Outcome Chapters::propose(const std::string& by, const std::vector<std::string>& founders, const std::string& name,
                          const std::string& colour, const std::string& charter, const std::string& scene, double now)
{
    std::set<std::string> all(founders.begin(), founders.end());
    all.insert(by);
    if (all.size() != Founders)
        return {false, "A Chapter is founded by three together."};
    for (const auto& f : all)
        if (of(f))
            return {false, "Each founder must be in no Chapter."};
    if (const auto problem = nameProblem(name); !problem.empty())
        return {false, problem};
    for (const auto& [id, c] : chapters_)
        if (lower(c.name) == lower(name))
            return {false, "There is already a Chapter of that name."};
    if (const auto problem = colourProblem(colour); !problem.empty())
        return {false, problem};
    if (charter.size() > 600)
        return {false, "A charter is at most 600 letters."};
    if (scene.empty())
        return {false, "Found it in a scene, together."};
    Proposal p{by, name, colour, charter, scene, std::vector<std::string>(all.begin(), all.end()), {by}, now + 600};
    proposals_[by] = p;
    return {true, {}};
}

const Proposal* Chapters::proposalFor(const std::string& who, double now) const
{
    for (const auto& [by, p] : proposals_)
        if (p.expires >= now && std::find(p.founders.begin(), p.founders.end(), who) != p.founders.end())
            return &p;
    return nullptr;
}

Outcome Chapters::agree(const std::string& who, double now)
{
    for (auto& [by, p] : proposals_)
        if (p.expires >= now && std::find(p.founders.begin(), p.founders.end(), who) != p.founders.end())
        {
            p.agreed.insert(who);
            if (p.agreed.size() < p.founders.size())
                return {true, {}};
            const Proposal done = p;
            proposals_.erase(by);
            return found(done, now);
        }
    return {false, "No founding is waiting for your word."};
}

Outcome Chapters::withdraw(const std::string& who)
{
    for (auto it = proposals_.begin(); it != proposals_.end(); ++it)
        if (std::find(it->second.founders.begin(), it->second.founders.end(), who) != it->second.founders.end())
        {
            proposals_.erase(it);
            return {true, {}};
        }
    return {false, "No founding is waiting for your word."};
}

Outcome Chapters::found(const Proposal& p, double now)
{
    for (const auto& f : p.founders)
        if (of(f))
            return {false, "A founder has joined another Chapter meanwhile."};
    for (const auto& [id, c] : chapters_)
        if (lower(c.name) == lower(p.name))
            return {false, "There is already a Chapter of that name."};
    Chapter c;
    c.id = "chapter-" + std::to_string(next_++);
    c.name = p.name;
    c.colour = p.colour;
    c.charter = p.charter;
    c.founded = now;
    for (const auto& f : p.founders)
    {
        // The proposer heads it; the others are its first Officers.
        c.members[f] = {f, f == p.by ? RankHead : RankOfficer, now, now};
        chapterOf_[f] = c.id;
    }
    c.log.push_back({"founded", p.by, "", p.name, 0, now});
    const auto id = c.id;
    chapters_[id] = std::move(c);
    return {true, id};
}

Outcome Chapters::invite(const std::string& by, const std::string& to, double now)
{
    const Chapter* c = nullptr;
    if (auto r = needRank(by, RankOfficer, &c); !r.ok)
        return r;
    if (by == to || of(to))
        return {false, "They are already in a Chapter."};
    if (const auto e = c->expelled.find(to); e != c->expelled.end() && now - e->second < RejoinSeconds)
        return {false, "They were sent away too lately to come back yet."};
    invites_[to] = {c->id, now + InviteSeconds};
    return {true, c->id};
}

const std::pair<std::string, double>* Chapters::inviteFor(const std::string& to, double now) const
{
    const auto found = invites_.find(to);
    return found == invites_.end() || found->second.second < now || !byId(found->second.first) ? nullptr : &found->second;
}

Outcome Chapters::accept(const std::string& to, double now)
{
    const auto* waiting = inviteFor(to, now);
    if (!waiting)
        return {false, "No invitation is waiting for you."};
    const std::string id = waiting->first;
    invites_.erase(to);
    if (of(to))
        return {false, "You are already in a Chapter."};
    auto* c = byId(id);
    c->members[to] = {to, RankInitiate, now, now};
    chapterOf_[to] = id;
    c->log.push_back({"joined", to, "", "", 0, now});
    return {true, id};
}

Outcome Chapters::decline(const std::string& to)
{
    return invites_.erase(to) ? Outcome{true, {}} : Outcome{false, "No invitation is waiting for you."};
}

Outcome Chapters::leave(const std::string& who, double now)
{
    const auto found = chapterOf_.find(who);
    if (found == chapterOf_.end())
        return {false, "You are not in a Chapter."};
    const std::string id = found->second;
    auto& c = chapters_[id];
    const bool head = c.members[who].rank == RankHead;
    c.members.erase(who);
    chapterOf_.erase(found);
    c.log.push_back({"left", who, "", "", 0, now});
    if (c.members.empty())
    {
        chapters_.erase(id);                      // The last one out: it is no more.
        return {true, "ended"};
    }
    if (head)
    {
        // The longest-serving Officer, else the longest-serving of anyone.
        const Member* next = nullptr;
        for (const auto& [m, mm] : c.members)
            if (!next || (mm.rank < next->rank) || (mm.rank == next->rank && mm.joined < next->joined))
                next = &mm;
        c.members[next->id].rank = RankHead;
        c.log.push_back({"head", next->id, who, "succession", 0, now});
        return {true, next->id};
    }
    return {true, {}};
}

Outcome Chapters::remove(const std::string& by, const std::string& who, double now)
{
    const Chapter* c = nullptr;
    if (auto r = needRank(by, RankOfficer, &c); !r.ok)
        return r;
    if (!together(by, who))
        return {false, "They are not in your Chapter."};
    const auto& them = c->members.at(who);
    const auto& me = c->members.at(by);
    if (me.rank != RankHead && them.rank != RankInitiate)
        return {false, "Only the Head sends away any but Initiates."};
    if (them.rank == RankHead)
        return {false, "The Head can't be sent away."};
    const std::string id = c->id;
    chapters_[id].expelled[who] = now;
    chapters_[id].log.push_back({"removed", by, who, "", 0, now});
    leave(who, now);
    return {true, {}};
}

Outcome Chapters::setRank(const std::string& by, const std::string& who, int rank, double now)
{
    const Chapter* c = nullptr;
    if (auto r = needRank(by, RankHead, &c); !r.ok)
        return r;
    if (!together(by, who))
        return {false, "They are not in your Chapter."};
    if (rank < RankHead || rank > RankInitiate)
        return {false, "No such rank."};
    auto& chapter = chapters_[c->id];
    if (rank == RankHead)
        chapter.members[by].rank = RankOfficer;       // Handing on the headship.
    chapter.members[who].rank = rank;
    chapter.log.push_back({"rank", by, who, chapter.rankNames[std::size_t(rank)], 0, now});
    return {true, {}};
}

Outcome Chapters::renameRank(const std::string& by, int rank, const std::string& name)
{
    const Chapter* c = nullptr;
    if (auto r = needRank(by, RankHead, &c); !r.ok)
        return r;
    if (c->level < 2)
        return {false, "A Chapter names its own ranks from Lodge (level II)."};
    if (rank < RankHead || rank > RankInitiate || name.size() < 2 || name.size() > 24)
        return {false, "A rank's name is 2 to 24 letters."};
    chapters_[c->id].rankNames[std::size_t(rank)] = name;
    return {true, {}};
}

Outcome Chapters::setMeeting(const std::string& by, const std::string& cell, double x, double y, const std::string& place)
{
    const Chapter* c = nullptr;
    if (auto r = needRank(by, RankOfficer, &c); !r.ok)
        return r;
    auto& chapter = chapters_[c->id];
    chapter.meetingCell = cell;
    chapter.meetingX = x;
    chapter.meetingY = y;
    chapter.meetingName = place;
    return {true, {}};
}

Outcome Chapters::markHostile(const std::string& by, const std::string& target, const std::string& kind, const std::string& reason,
                              double now)
{
    const Chapter* c = nullptr;
    if (auto r = needRank(by, RankOfficer, &c); !r.ok)
        return r;
    if (target.empty() || target == c->id || together(by, target))
        return {false, "Not your own."};
    if (reason.size() > 200)
        return {false, "Give the reason in fewer words."};
    auto& chapter = chapters_[c->id];
    if (chapter.hostiles.size() >= 100)
        return {false, "Your hostile list is full."};
    chapter.hostiles.erase(std::remove_if(chapter.hostiles.begin(), chapter.hostiles.end(), [&](const Hostile& h) { return h.target == target; }),
                           chapter.hostiles.end());
    chapter.hostiles.push_back({target, kind, reason, by, now});
    return {true, {}};
}

Outcome Chapters::unmarkHostile(const std::string& by, const std::string& target)
{
    const Chapter* c = nullptr;
    if (auto r = needRank(by, RankOfficer, &c); !r.ok)
        return r;
    auto& h = chapters_[c->id].hostiles;
    const auto before = h.size();
    h.erase(std::remove_if(h.begin(), h.end(), [&](const Hostile& x) { return x.target == target; }), h.end());
    return {h.size() < before, h.size() < before ? std::string() : std::string("They aren't on the list.")};
}

void Chapters::touch(const std::string& who, double now)
{
    if (const auto found = chapterOf_.find(who); found != chapterOf_.end())
        chapters_[found->second].members[who].active = now;
}

int Chapters::addRenown(const std::string& chapterId, const std::string& kind, int amount, const std::string& source,
                        const std::string& actor, double now)
{
    auto* c = byId(chapterId);
    if (!c || amount <= 0)
        return 0;
    int week = 0;
    for (const auto& e : c->renownLog)
        if (now - e.at < WeekSeconds && e.kind != "award")
            week += e.amount;
    if (kind != "award")                          // (A Dungeon Master's award is outside the cap.)
        amount = std::max(0, std::min(amount, WeeklyRenownCap - week));
    if (amount <= 0)
        return 0;
    c->renown += amount;
    c->renownLog.push_back({kind, source, actor, amount, now});
    if (c->renownLog.size() > RenownKept)
        c->renownLog.erase(c->renownLog.begin());
    return amount;
}

int Chapters::activeMembers(const Chapter& c, double now) const
{
    int n = 0;
    for (const auto& [id, m] : c.members)
        n += now - m.active < ActiveSeconds;
    return n;
}

std::vector<int> Chapters::advance(const std::string& chapterId, double now)
{
    std::vector<int> reached;
    auto* c = byId(chapterId);
    while (c && c->level < 5)
    {
        const int next = c->level + 1;
        const auto& g = gateFor(next);
        const bool ground = next == 2   ? true
                            : next == 3 ? c->hallHeldTwoWeeks
                            : next == 4 ? c->campStanding
                                        : c->fortified && c->friendlyFaction;
        if (c->renown < g.renown || activeMembers(*c, now) < g.active || c->storiesTold < g.stories || !ground)
            break;
        c->level = next;
        c->log.push_back({"level", "", "", levelName(next), next, now});
        reached.push_back(next);
    }
    return reached;
}

std::vector<std::string> Chapters::tick(double now)
{
    std::vector<std::string> changed;
    for (auto it = proposals_.begin(); it != proposals_.end();)
        it = it->second.expires < now ? proposals_.erase(it) : std::next(it);
    for (auto it = invites_.begin(); it != invites_.end();)
        it = it->second.second < now ? invites_.erase(it) : std::next(it);
    // A Head away a month: the longest-serving active Officer takes over.
    for (auto& [id, c] : chapters_)
        for (auto& [m, mm] : c.members)
            if (mm.rank == RankHead && now - mm.active > HeadAwaySeconds)
            {
                Member* next = nullptr;
                for (auto& [o, om] : c.members)
                    if (om.rank == RankOfficer && now - om.active < ActiveSeconds && (!next || om.joined < next->joined))
                        next = &om;
                if (next)
                {
                    mm.rank = RankOfficer;
                    next->rank = RankHead;
                    c.log.push_back({"head", next->id, m, "the Head was away a month", 0, now});
                    changed.push_back(id);
                }
                break;
            }
    return changed;
}

Value Chapters::save() const
{
    auto root = Value::object();
    root.add("next", double(next_));
    auto list = Value::array();
    for (const auto& [id, c] : chapters_)
    {
        auto j = Value::object();
        j.add("id", c.id);
        j.add("name", c.name);
        j.add("colour", c.colour);
        j.add("charter", c.charter);
        j.add("level", c.level);
        j.add("renown", c.renown);
        j.add("storiesTold", c.storiesTold);
        j.add("founded", c.founded);
        auto members = Value::array();
        for (const auto& [m, mm] : c.members)
        {
            auto k = Value::object();
            k.add("id", mm.id);
            k.add("rank", mm.rank);
            k.add("joined", mm.joined);
            k.add("active", mm.active);
            members.push(k);
        }
        j.add("members", members);
        auto ranks = Value::array();
        for (const auto& r : c.rankNames)
            ranks.push(r);
        j.add("rankNames", ranks);
        auto renown = Value::array();
        for (const auto& e : c.renownLog)
        {
            auto k = Value::object();
            k.add("kind", e.kind);
            k.add("source", e.source);
            k.add("actor", e.actor);
            k.add("amount", e.amount);
            k.add("at", e.at);
            renown.push(k);
        }
        j.add("renownLog", renown);
        auto log = Value::array();
        for (const auto& e : c.log)
        {
            auto k = Value::object();
            k.add("kind", e.kind);
            k.add("by", e.by);
            k.add("other", e.other);
            k.add("detail", e.detail);
            k.add("amount", double(e.amount));
            k.add("at", e.at);
            log.push(k);
        }
        j.add("log", log);
        auto hostiles = Value::array();
        for (const auto& h : c.hostiles)
        {
            auto k = Value::object();
            k.add("target", h.target);
            k.add("kind", h.kind);
            k.add("reason", h.reason);
            k.add("by", h.by);
            k.add("at", h.at);
            hostiles.push(k);
        }
        j.add("hostiles", hostiles);
        j.add("meetingCell", c.meetingCell);
        j.add("meetingName", c.meetingName);
        j.add("meetingX", c.meetingX);
        j.add("meetingY", c.meetingY);
        auto counted = Value::array();
        for (const auto& s : c.scenesCounted)
            counted.push(s);
        j.add("scenesCounted", counted);
        auto expelled = Value::object();
        for (const auto& [who, at] : c.expelled)
            expelled.add(who, at);
        j.add("expelled", expelled);
        j.add("hallHeldTwoWeeks", c.hallHeldTwoWeeks);
        j.add("campStanding", c.campStanding);
        j.add("fortified", c.fortified);
        j.add("friendlyFaction", c.friendlyFaction);
        j.add("claimCell", c.claimCell);
        j.add("houseOf", c.houseOf);
        auto sworn = Value::array();
        for (const auto& n : c.sworn)
            sworn.push(n);
        j.add("sworn", sworn);
        list.push(j);
    }
    root.add("chapters", list);
    return root;
}

void Chapters::load(const Value& saved)
{
    *this = Chapters{};
    if (!saved.isObject())
        return;
    next_ = std::max<std::uint64_t>(1, std::uint64_t(saved.number("next", 1)));
    for (const auto& j : saved.array("chapters"))
    {
        Chapter c;
        c.id = j.string("id");
        c.name = j.string("name");
        c.colour = j.string("colour", "#92bacd");
        c.charter = j.string("charter");
        c.level = std::clamp(int(j.number("level", 1)), 1, 5);
        c.renown = int(j.number("renown"));
        c.storiesTold = int(j.number("storiesTold"));
        c.founded = j.number("founded");
        for (const auto& k : j.array("members"))
        {
            const auto id = k.string("id");
            if (id.empty() || chapterOf_.count(id))
                continue;
            c.members[id] = {id, std::clamp(int(k.number("rank", RankInitiate)), RankHead, RankInitiate), k.number("joined"), k.number("active")};
        }
        std::size_t r = 0;
        for (const auto& name : j.array("rankNames"))
            if (name.isString() && r < c.rankNames.size())
                c.rankNames[r++] = name.asString();
        for (const auto& k : j.array("renownLog"))
            c.renownLog.push_back({k.string("kind"), k.string("source"), k.string("actor"), int(k.number("amount")), k.number("at")});
        for (const auto& k : j.array("log"))
            c.log.push_back({k.string("kind"), k.string("by"), k.string("other"), k.string("detail"), std::int64_t(k.number("amount")),
                             k.number("at")});
        for (const auto& k : j.array("hostiles"))
            c.hostiles.push_back({k.string("target"), k.string("kind", "wolf"), k.string("reason"), k.string("by"), k.number("at")});
        c.meetingCell = j.string("meetingCell");
        c.meetingName = j.string("meetingName");
        c.meetingX = j.number("meetingX");
        c.meetingY = j.number("meetingY");
        for (const auto& s : j.array("scenesCounted"))
            if (s.isString())
                c.scenesCounted.insert(s.asString());
        for (const auto& [who, at] : j.object("expelled").fields())
            c.expelled[who] = at.asNumber();
        c.hallHeldTwoWeeks = j.boolean("hallHeldTwoWeeks");
        c.campStanding = j.boolean("campStanding");
        c.fortified = j.boolean("fortified");
        c.friendlyFaction = j.boolean("friendlyFaction");
        c.claimCell = j.string("claimCell");
        c.houseOf = j.string("houseOf");
        for (const auto& n : j.array("sworn"))
            if (n.isString())
                c.sworn.insert(n.asString());
        if (c.id.empty() || c.members.empty() || chapters_.count(c.id))
            continue;
        bool headed = false;
        for (const auto& [id, m] : c.members)
            headed |= m.rank == RankHead;
        if (!headed)
            c.members.begin()->second.rank = RankHead;
        for (const auto& [id, m] : c.members)
            chapterOf_[id] = c.id;
        chapters_[c.id] = std::move(c);
    }
}
} // namespace ratw::chapter
