// Working together (Docs/Design/53-hunting-and-working-together.md, 2): the cooperation scaling, the activities, and the
// world's joints: Lend a paw, asking, leaving, the members who wander off or idle, and the bond and record at the end.
#include "RatwTogether.h"
#include "RatwCalendar.h"
#include "RatwItems.h"
#include "RatwJsonDoc.h"
#include "RatwWorld.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace ratw::together
{
namespace
{
std::filesystem::path patternsFile()
{
    // Data/Together: RATW_DATA_DIR, else the working directory or one above it, else the source tree.
    namespace fs = std::filesystem;
    std::error_code ec;
    if (const char* dir = std::getenv("RATW_DATA_DIR"); dir && *dir)
        return fs::path(dir) / "Together" / "patterns.json";
    for (auto at = fs::current_path(ec); !ec && !at.empty(); at = at.parent_path())
    {
        if (fs::exists(at / "Data" / "Together" / "patterns.json", ec))
            return at / "Data" / "Together" / "patterns.json";
        if (at == at.parent_path())
            break;
    }
#ifdef RATW_SOURCE_DIR
    return fs::path(RATW_SOURCE_DIR) / "Data" / "Together" / "patterns.json";
#else
    return fs::path("Data") / "Together" / "patterns.json";
#endif
}

Rules build()
{
    std::ifstream in(patternsFile());
    std::stringstream text;
    text << in.rdbuf();
    if (!in)
        std::cerr << "[warn] RATW_TOGETHER no Data/Together/patterns.json: no one works together\n";
    return parse(text.str());
}
} // namespace

Rules parse(const std::string& text)
{
    Rules r;
    json::Value doc;
    std::string error;
    if (text.empty() || !json::parse(text, doc, error) || !doc.isObject())
        return r;
    if (const auto* steps = doc.find("steps"); steps && steps->isArray())
    {
        r.steps.clear();
        for (const auto& v : steps->items())
            r.steps.push_back(v.asNumber(0));
    }
    r.leaveTiles = doc.number("leaveTiles", r.leaveTiles);
    r.idleSeconds = doc.number("idleSeconds", r.idleSeconds);
    r.lendTiles = doc.number("lendTiles", r.lendTiles);
    const auto& bond = doc.object("bond");
    r.bondAffinity = bond.number("affinity", r.bondAffinity);
    r.bondTrust = bond.number("trust", r.bondTrust);
    r.bondFamiliarity = bond.number("familiarity", r.bondFamiliarity);
    r.residentAffinity = bond.number("residentAffinity", r.residentAffinity);
    r.residentTrust = bond.number("residentTrust", r.residentTrust);
    r.bondBeats = int(bond.number("beats", r.bondBeats));
    for (const auto& a : doc.array("activities"))
    {
        Activity act;
        act.id = a.string("id");
        act.name = a.string("name", act.id);
        act.pattern = a.string("pattern");
        act.most = std::max(1, int(a.number("most", act.most)));
        act.beat = std::max(.5, a.number("beat", act.beat));
        act.verb = a.string("verb");
        act.at = a.string("at");
        act.residentRole = std::max(0, int(a.number("residentRole", 1)));
        act.season = int(a.number("season", -1));
        act.share = std::max(0., a.number("share", 0));
        for (const auto& p : a.array("producers"))
            act.producers.push_back(p.asString());
        for (const auto& g : a.array("gifts"))
            act.gifts.push_back(g.asString());
        for (const auto& role : a.array("roles"))
        {
            act.roles.push_back({role.string("id"), role.string("words"), role.string("yours"), role.string("practice")});
            if (act.roles.back().yours.empty())
                act.roles.back().yours = act.roles.back().words;
        }
        if (!act.id.empty() && act.roles.size() >= 2)
            r.activities.push_back(act);
    }
    return r;
}

const Rules& rules()
{
    static const Rules r = build();
    return r;
}

const Activity* activity(const std::string& id, const Rules& r)
{
    for (const auto& a : r.activities)
        if (a.id == id)
            return &a;
    return nullptr;
}

double rate(std::vector<Hand> members, int most, const Rules& r)
{
    // Players first (in the order they joined), resident hands after.
    std::stable_partition(members.begin(), members.end(), [](const Hand& h) { return !h.resident; });
    double out = members.empty() ? 0 : 1;
    for (std::size_t k = 1; k < members.size() && int(k) < most; ++k)
    {
        if (k - 1 >= r.steps.size())
            break;
        double step = r.steps[k - 1];
        const bool oneRole = std::all_of(members.begin(), members.begin() + std::ptrdiff_t(k + 1),
                                         [&](const Hand& h) { return h.role == members.front().role; });
        if (oneRole)
            step /= 2;
        if (members[k].resident)
            step /= 2;
        out += step;
    }
    return out;
}

std::vector<int> split(int count, int n, const std::vector<int>& order)
{
    std::vector<int> parts(std::size_t(std::max(0, n)), n > 0 ? count / n : 0);
    if (n <= 0)
        return parts;
    for (int r = 0; r < count % n; ++r)
        ++parts[std::size_t(order.empty() ? r : order[std::size_t(r) % order.size()] % n)];
    return parts;
}

std::vector<Hand> handsOf(const JointWork& j, double now)
{
    std::vector<Hand> out;
    for (const auto& m : j.members)
        out.push_back({m.watch ? std::string("watch") : m.giftUntil > now && now >= 0 ? "gift:" + m.gift : m.role, m.resident});
    return out;
}

double rateOf(const JointWork& j, double now)
{
    const auto* a = activity(j.kind);
    double lift = 0;
    for (const auto& m : j.members)
        if (m.giftUntil > now && now >= 0)
            lift += GiftLift;
    return rate(handsOf(j, now), a ? a->most : 4) + lift;
}
} // namespace ratw::together

namespace ratw
{
const together::JointWork* World::jointOf(const std::string& id) const
{
    const auto found = jointOf_.find(id);
    if (found == jointOf_.end())
        return nullptr;
    const auto j = joints_.find(found->second);
    return j == joints_.end() ? nullptr : &j->second;
}

double World::workRate(const std::string& id) const
{
    const auto* j = jointOf(id);
    if (!j)
        return 1;
    const auto* a = together::activity(j->kind);
    (void)a;
    return together::rateOf(*j, time_);
}

double World::workLabour(const std::string& jointId) const
{
    // Doc 57's labour: each member's rate over the beats it worked.
    const auto j = joints_.find(jointId);
    if (j == joints_.end())
        return 0;
    const auto* a = together::activity(j->second.kind);
    (void)a;
    const double rate = together::rateOf(j->second, time_);
    double labour = 0;
    for (const auto& m : j->second.members)
        labour += rate * m.beats;
    return labour;
}

bool World::atWork(const std::string& id) const
{
    if (jointOf(id))
        return true;
    const auto last = lastWorked_.find(id);
    return last != lastWorked_.end() && time_ - last->second <= together::rules().idleSeconds;
}

bool World::askedToLend(const std::string& worker, const std::string& helper) const
{
    const auto found = lendAsked_.find(worker);
    return found != lendAsked_.end() && found->second.count(helper);
}

bool World::mayLend(const std::string& helper, const std::string& worker) const
{
    // Lend a paw: a worker at work within 6 tiles, whose Allow work partners is on (or who asked); not blocked either
    // way; not already in its joint; neither in a fight.
    const auto* h = entity(helper);
    const auto* w = entity(worker);
    if (!h || !w || h->npc || w->npc || helper == worker || h->dead || w->dead || inBattle(helper) || inBattle(worker))
        return false;
    if (h->cellId != w->cellId || std::hypot(h->position.x - w->position.x, h->position.y - w->position.y) > together::rules().lendTiles)
        return false;
    if (!atWork(worker) || (blocked_ && blocked_(helper, worker)))
        return false;
    if (const auto* j = jointOf(worker); j && jointOf_.count(helper) && jointOf_.at(helper) == j->id)
        return false;
    return !w->noWorkPartners || askedToLend(worker, helper);
}

Result World::lendAPaw(const std::string& helper, const std::string& worker, const std::string& role)
{
    if (!mayLend(helper, worker))
        return {false, "You can't lend them a paw just now.", {}};
    const auto* act = together::activity("forage");          // (Foraging is the one activity a wolf works on its own yet.)
    if (const auto* j = jointOf(worker))
        act = together::activity(j->kind);
    if (!act)
        return {false, "There is no such work.", {}};
    if (jointOf(helper))
        leaveWork(helper);
    const auto* w = entity(worker);
    std::string jointId;
    if (const auto* j = jointOf(worker))
        jointId = j->id;
    else
    {
        together::JointWork fresh;
        fresh.id = "work-" + std::to_string(jointNext_++);
        fresh.kind = act->id;
        fresh.cellId = w->cellId;
        fresh.x = w->position.x;
        fresh.y = w->position.y;
        fresh.started = time_;
        fresh.members.push_back({worker, act->roles[0].id, false, time_, time_, 0});
        jointId = fresh.id;
        joints_[jointId] = fresh;
        jointOf_[worker] = jointId;
    }
    const auto chosen = joinJoint(jointId, helper, role);
    if (chosen.empty())
        return {false, "As many are at it as can work it.", {}};
    if (auto asked = lendAsked_.find(worker); asked != lendAsked_.end())
        asked->second.erase(helper);
    std::string words, yours;
    for (const auto& r : act->roles)
        if (r.id == chosen)
        {
            words = r.words;
            yours = r.yours;
        }
    const auto* h = entity(helper);
    notice(worker, (h ? h->name : std::string("Someone")) + " lends you a paw and " + words + ".");
    return {true, "You lend a paw: you " + yours + ".", {}};
}

std::string World::joinJoint(const std::string& jointId, const std::string& helper, const std::string& role)
{
    // Into a joint: the role asked for, if it is one of this work's; else the free one (the one fewest players have: a
    // resident hand's angle is its own, and two players on two angles beside it work at 2.0).
    auto& j = joints_[jointId];
    const auto* act = together::activity(j.kind);
    if (!act || int(j.members.size()) >= act->most)
        return {};
    std::string chosen;
    for (const auto& r : act->roles)
        if (r.id == role)
            chosen = role;
    if (chosen.empty())
    {
        int fewest = 1 << 30;
        for (const auto& r : act->roles)
        {
            // (At a workshop the maker leads: a player hands, doc 53, 3.)
            if (act->at == "workshop" && std::any_of(j.members.begin(), j.members.end(), [&](const together::Member& m) { return m.resident && m.role == r.id; }))
                continue;
            const int n = int(std::count_if(j.members.begin(), j.members.end(), [&](const together::Member& m) { return !m.resident && m.role == r.id; }));
            if (n < fewest)
                fewest = n, chosen = r.id;
        }
    }
    j.members.push_back({helper, chosen, false, time_, time_, 0});
    jointOf_[helper] = jointId;
    return chosen;
}

Result World::askToLend(const std::string& worker, const std::string& target)
{
    const auto* w = entity(worker);
    const auto* t = entity(target);
    if (!w || !t || w->npc || t->npc || worker == target)
        return {false, "Ask whom?", {}};
    if (!atWork(worker))
        return {false, "You aren't at work.", {}};
    if (blocked_ && blocked_(worker, target))
        return {false, "You can't ask them.", {}};
    if (t->cellId != w->cellId || std::hypot(t->position.x - w->position.x, t->position.y - w->position.y) > together::rules().lendTiles)
        return {false, "They aren't near enough.", {}};
    lendAsked_[worker].insert(target);
    notice(target, w->name + " asks you to lend a paw.");
    return {true, "You ask them to lend a paw.", {}};
}

Result World::leaveWork(const std::string& id)
{
    if (!jointOf(id))
        return {false, "You aren't working with anyone.", {}};
    dropFromJoint(id);
    return {true, "You leave off working together.", {}};
}

void World::dropFromJoint(const std::string& id)
{
    const auto found = jointOf_.find(id);
    if (found == jointOf_.end())
        return;
    const auto jointId = found->second;
    jointOf_.erase(found);
    auto j = joints_.find(jointId);
    if (j == joints_.end())
        return;
    auto& members = j->second.members;
    members.erase(std::remove_if(members.begin(), members.end(), [&](const together::Member& m) { return m.id == id; }), members.end());
    const int players = int(std::count_if(members.begin(), members.end(), [&](const together::Member& m) { return !m.resident; }));
    if (members.size() < 2 || players == 0)
        endJoint(jointId);
}

void World::endJoint(const std::string& id)
{
    // The end: every pair of players who worked two beats together grows closer, a resident hand more so (residents
    // remember who helped); a record of IDs for docs 51 and 56.
    const auto j = joints_.find(id);
    if (j == joints_.end())
        return;
    const auto& r = together::rules();
    const auto& joint = j->second;
    std::vector<std::string> everyone;
    for (const auto& m : joint.members)
        everyone.push_back(m.id);
    for (const auto& [pair, beats] : joint.beatsTogether)
    {
        if (beats < r.bondBeats)
            continue;
        const auto bar = pair.find('|');
        const auto a = pair.substr(0, bar), b = pair.substr(bar + 1);
        const auto* ea = entity(a);
        const auto* eb = entity(b);
        if (!ea || !eb)
            continue;
        if (ea->npc || eb->npc)
        {
            const auto& resident = ea->npc ? a : b;
            const auto& player = ea->npc ? b : a;
            bonds_.change(resident, player, {r.residentAffinity, r.residentTrust, r.bondFamiliarity, 0, 0}, calendarDays_);
        }
        else
            bonds_.mutual(a, b, {r.bondAffinity, r.bondTrust, r.bondFamiliarity, 0, 0}, calendarDays_);
        recordEvent({"together", a, b, joint.cellId, 0, 0, joint.kind, beats, 0, id});
    }
    for (const auto& m : joint.members)
        if (const auto mine = jointOf_.find(m.id); mine != jointOf_.end() && mine->second == id)
            jointOf_.erase(mine);
    joints_.erase(j);
}

void World::tendJoints(double dt)
{
    // Once a second, over the joints alive only: a member gone 8 tiles off from all the others (they may range
    // together, patch to patch), idle a minute, down, in a fight or gone has left; a joint with fewer than two, or no
    // player, ends.
    jointsAccumulator_ += dt;
    if (jointsAccumulator_ < 1 || joints_.empty())
        return;
    jointsAccumulator_ = 0;
    const auto& r = together::rules();
    std::vector<std::string> leaving, ending;
    for (auto& [id, j] : joints_)
    {
        // Farm work: its beats by the clock; it ends when the farmer stops (or the farm can't pay, or its barn is full).
        const auto* act = together::activity(j.kind);
        if (act && act->byBeat())
        {
            if (j.nextBeat >= 0 && time_ >= j.nextBeat)
            {
                workBeat(j);
                if (j.nextBeat >= 0)
                    j.nextBeat += act->beat;
            }
            if (j.nextBeat < 0 || residentWorkAt(j.target) != act)
            {
                if (j.nextBeat >= 0)
                    if (const auto* f = entity(j.target))
                        for (const auto& m : j.members)
                            if (!m.resident)
                                notice(m.id, f->name + " stops work, and so do you.");
                ending.push_back(id);
                continue;
            }
        }
        // Farm work is paid by the beat to a hand who is there: idle a whole beat, it has left. Residents keep their
        // own hours (above).
        const double idle = act && act->byBeat() ? std::max(r.idleSeconds, act->beat) : r.idleSeconds;
        for (const auto& m : j.members)
        {
            if (m.resident)
                continue;
            const double idleHere = m.watch ? 1e18 : idle;     // (A watcher stands and watches: never idle.)
            const auto* e = entity(m.id);
            bool near = false;
            for (const auto& o : j.members)
                if (o.id != m.id)
                    if (const auto* f = entity(o.id); f && f->cellId == j.cellId && e &&
                                                      std::hypot(e->position.x - f->position.x, e->position.y - f->position.y) <= r.leaveTiles)
                        near = true;
            if (!e || e->dead || e->downedLeft > 0 || inBattle(m.id) || e->cellId != j.cellId || !near ||
                time_ - m.lastActed > idleHere)
                leaving.push_back(m.id);
        }
    }
    for (const auto& id : ending)
        endJoint(id);
    for (const auto& id : leaving)
    {
        if (const auto* e = entity(id); e && !e->npc)
            notice(id, "You have left off working together.");
        dropFromJoint(id);
    }
    for (auto it = lastWorked_.begin(); it != lastWorked_.end();)
        it = time_ - it->second > r.idleSeconds * 5 ? lastWorked_.erase(it) : std::next(it);
}

// ------------------------------------------------------------------ Farm work (doc 53, 2.6; Phase 4)
//
// A resident farmer at work at its post in the harvest (autumn) or at threshing (winter) may be helped by a player
// near: a joint with the farmer as a resident hand, so one player works at ×1.4 and two at ×2.0 each. Its beats come by
// the clock, a spell (30 game minutes) each: every player there, and doing something in it,
// brings in its rate times a share of a spell's yield for the farm (up to ProducerKept) and is paid the town's hand wage
// a spell (dayWage / PaidSpells) times its rate from the farm's till, a piece rate, fractions carried; a spell it came
// part of the way through, in part. Money only moves.
// The farmer's own spells go on as before.

namespace
{
double farmOdds(const std::string& key, std::int64_t n)
{
    std::uint64_t h = 1469598103934665603ULL ^ std::uint64_t(n) * 0x9E3779B97F4A7C15ULL;
    for (unsigned char c : key)
        h = (h ^ c) * 1099511628211ULL;
    h ^= h >> 31;
    h *= 0xBF58476D1CE4E5B9ULL;
    return double((h ^ (h >> 29)) % 100000) / 100000.0;
}

std::string lower(std::string name)                // ("Wheat" in a sentence: "wheat".)
{
    if (!name.empty())
        name[0] = char(std::tolower(static_cast<unsigned char>(name[0])));
    return name;
}

int farmWhole(double expected, double roll)
{
    const double base = std::floor(std::max(0.0, expected));
    return int(base) + (roll < expected - base ? 1 : 0);
}
} // namespace

const together::Activity* World::residentWorkAt(const std::string& farmer) const
{
    const auto* e = entity(farmer);
    if (!e || !e->npc || e->dead)
        return nullptr;
    const auto* spec = society_.spec(farmer);
    const auto* job = society_.jobOf(farmer);
    const auto* life = society_.resident(farmer);
    if (!spec || !job || !life)
        return nullptr;
    // At its post, working (the task its post's title, in its hours), and there.
    if (life->task != job->title || e->cellId != life->goalCell || std::hypot(e->position.x - life->goalX, e->position.y - life->goalY) > 1.5)
        return nullptr;
    // A farm's work in its season; a maker's bench (a business that crafts) at any time.
    const auto* producer = items::producerFor(spec->workLabel);
    const auto* business = items::businessFor(spec->workLabel);
    const bool crafts = business && !items::craftsFor(business->id).empty();
    const int season = int(calendar::calendarAt(calendarDays_).season);
    for (const auto& a : together::rules().activities)
        if (a.at == "farm" && producer && a.season == season &&
            std::find(a.producers.begin(), a.producers.end(), producer->id) != a.producers.end())
            return &a;
        else if (a.at == "workshop" && crafts && !producer)
            return &a;
    return nullptr;
}

Result World::helpAtWork(const std::string& player, const std::string& farmer)
{
    const auto* p = entity(player);
    const auto* f = entity(farmer);
    if (!p || p->npc || p->dead || !f)
        return {false, "Help whom?", {}};
    if (inBattle(player) || p->downedLeft > 0)
        return {false, "Not now.", {}};
    if (p->cellId != f->cellId || std::hypot(p->position.x - f->position.x, p->position.y - f->position.y) > together::rules().lendTiles)
        return {false, "Go nearer to " + f->name + " first.", {}};
    const auto* act = residentWorkAt(farmer);
    if (!act)
        return {false, f->name + " has no harvest or threshing to help with just now.", {}};
    if (const auto* j = jointOf(player); j && j->target == farmer)
        return {false, "You are already at it.", {}};
    // No work where the farm can't pay a hand's spell, or its barn is full.
    const auto till = society_.tillOf(farmer);
    const auto* account = society_.account(till);
    const auto* spec = society_.spec(farmer);
    const auto* producer = spec ? items::producerFor(spec->workLabel) : nullptr;
    if (!account || !spec || (act->at == "farm" && !producer))
        return {false, f->name + " has no work for hands.", {}};
    const auto town = [&] {
        auto t = communityOf(spec->work.cell);
        return t.empty() ? communityOf(spec->home.cell) : t;
    }();
    if (society_.spendable(till) < std::int64_t(std::ceil(society_.dayWage(town, "hand") / Society::PaidSpells * 2)))
        return {false, f->name + " can't pay for more hands today.", {}};
    bool room = act->at != "farm";
    if (producer && !room)
    {
        const bool inSeason = producer->seasons.empty() || std::find(producer->seasons.begin(), producer->seasons.end(), act->season) != producer->seasons.end();
        for (const auto& [item, count] : inSeason ? producer->out : producer->offSeason)
            room = room || Society::stock(*account, item) < Society::ProducerKept;
    }
    if (!room)
        return {false, "The barn is full; " + f->name + " has no more work for hands today.", {}};
    if (jointOf(player))
        leaveWork(player);
    std::string jointId;
    if (const auto found = jointOf_.find(farmer); found != jointOf_.end() && joints_.count(found->second))
        jointId = found->second;
    else
    {
        together::JointWork fresh;
        fresh.id = "work-" + std::to_string(jointNext_++);
        fresh.kind = act->id;
        fresh.cellId = f->cellId;
        fresh.target = farmer;
        fresh.x = f->position.x;
        fresh.y = f->position.y;
        fresh.started = time_;
        fresh.nextBeat = time_ + act->beat;
        fresh.members.push_back({farmer, act->roles[std::min<std::size_t>(std::size_t(act->residentRole), act->roles.size() - 1)].id, true, time_, time_, 0});
        jointId = fresh.id;
        joints_[jointId] = fresh;
        jointOf_[farmer] = jointId;
    }
    const auto chosen = joinJoint(jointId, player, {});
    if (chosen.empty())
        return {false, "As many are at it as can work it.", {}};
    std::string yours;
    for (const auto& r : act->roles)
        if (r.id == chosen)
            yours = r.yours;
    if (act->at == "workshop")                      // (The paw helps from the start: doc 53, 3.)
        society_.lendPaw(farmer, together::rateOf(joints_[jointId], time_),
                         calendarDays_ + act->beat * 1.5 / calendar::SecondsPerDay);
    return {true, "You set to work beside " + f->name + ": you " + yours + ". " + (act->at == "farm" ? act->name : std::string("The work")) +
                      " pays by the spell, from the " + (act->at == "farm" ? "farm" : "shop") + "'s till.", {}};
}

void World::noteActive(const std::string& player)
{
    if (const auto found = jointOf_.find(player); found != jointOf_.end())
        if (const auto j = joints_.find(found->second); j != joints_.end())
            if (const auto* a = together::activity(j->second.kind); a && a->byBeat())
                for (auto& m : j->second.members)
                    if (m.id == player)
                        m.lastActed = time_;
}

void World::workBeat(together::JointWork& joint)
{
    const auto* act = together::activity(joint.kind);
    const auto* f = entity(joint.target);
    const auto* spec = society_.spec(joint.target);
    const auto* producer = spec ? items::producerFor(spec->workLabel) : nullptr;
    const auto till = society_.tillOf(joint.target);
    const bool farm = act && act->at == "farm";
    if (!act || !f || !spec || (farm && !producer) || !society_.account(till))
        return;
    const double beatStart = joint.nextBeat - act->beat;
    const double rate = together::rateOf(joint, time_);
    const auto town = [&] {
        auto t = communityOf(spec->work.cell);
        return t.empty() ? communityOf(spec->home.cell) : t;
    }();
    const double wage = society_.dayWage(town, "hand") / Society::PaidSpells * rate;
    // A farm's hands bring in a share of a spell's yield; a workshop's make its maker's batches go faster (below).
    static const std::vector<std::pair<std::string, int>> none;
    const bool inSeason = producer && (producer->seasons.empty() || std::find(producer->seasons.begin(), producer->seasons.end(), act->season) != producer->seasons.end());
    const auto& yield = !farm ? none : inSeason ? producer->out : producer->offSeason;
    std::vector<std::string> worked;
    std::string stop;
    for (auto& m : joint.members)
    {
        // A hand doing something in the beat, paid and bringing in for the part of it it was there.
        if (m.resident || m.lastActed < beatStart)
            continue;
        const double part = std::clamp((joint.nextBeat - std::max(m.joined, beatStart)) / act->beat, 0., 1.);
        if (part <= 0)
            continue;
        const auto* account = society_.account(till);
        bool room = !farm;
        for (const auto& [item, count] : yield)
            room = room || Society::stock(*account, item) < Society::ProducerKept;
        if (!room)
        {
            stop = "The barn is full; " + f->name + " has no more work for hands today.";
            break;
        }
        m.carry += wage * part;
        const auto pay = std::int64_t(std::floor(m.carry));
        if (pay > 0 && society_.spendable(till) < pay)
        {
            m.carry -= wage * part;
            stop = f->name + " can't pay for more hands today.";
            break;
        }
        // What the hand's spell brings in, at the joint's rate: a share of a spell's yield.
        std::string brought;
        int goods = 0;
        for (const auto& [item, count] : yield)
        {
            const int held = Society::stock(*society_.account(till), item);
            // Weathereye (a Gifted Seer, doc 43) at the harvest, as at foraging: a quarter more (doc 53, 4).
            const auto* me = entity(m.id);
            const double seer = act->id == "harvest" && me && me->gift == "seer" && !me->quickened ? 1.25 : 1.;
            const int n = std::min(Society::ProducerKept - held,
                                   farmWhole(count * act->share * rate * part * seer, farmOdds(m.id + item, std::int64_t(time_ * 1000))));
            if (n > 0 && society_.create(till, item, n, "brought in by a hand"))
            {
                goods += n;
                brought += (brought.empty() ? "" : ", ") + std::to_string(n) + " " + lower(Society::itemName(item));
            }
        }
        if (pay > 0 && society_.shift(till, m.id, "", 0, pay, farm ? "farm work" : "a hand at the bench"))
        {
            m.carry -= double(pay);
            m.earned += int(pay);
        }
        ++m.beats;
        worked.push_back(m.id);
        // A spell's practice beside the resident, the master (doc 49's apprentice source by the family of work).
        PracticeContext context;
        context.teacher = joint.target;
        context.amount = 60 * part;
        practise(m.id, farm ? "apprentice.labour" : "apprentice.craft", context);
        notice(m.id, (farm ? "A spell's " + lower(act->name.rfind("The ", 0) == 0 ? act->name.substr(4) : act->name) + " beside " + f->name + ": " +
                                 (brought.empty() ? std::string("little to show for it") : brought + " brought in")
                           : "A spell at the bench beside " + f->name + ": the work goes faster for your paw") +
                         "; you are paid " + std::to_string(pay) + "p (" + std::to_string(m.earned) + "p so far).");
        recordEvent({farm ? "farm work" : "bench work", m.id, joint.target, joint.cellId, 0, 0, joint.kind, goods, int(pay), joint.id});
    }
    // A workshop: the maker's batches begun in the next spell and a half go faster by the joint's rate (doc 53, 3).
    if (!farm && !worked.empty() && stop.empty())
        society_.lendPaw(joint.target, rate, calendarDays_ + act->beat * 1.5 / calendar::SecondsPerDay);
    // Beats together: each hand who worked with each other and with the farmer.
    for (std::size_t a = 0; a < worked.size(); ++a)
    {
        const auto& x = worked[a];
        ++joint.beatsTogether[x < joint.target ? x + "|" + joint.target : joint.target + "|" + x];
        for (std::size_t b = a + 1; b < worked.size(); ++b)
            ++joint.beatsTogether[x < worked[b] ? x + "|" + worked[b] : worked[b] + "|" + x];
    }
    if (!stop.empty())
    {
        for (const auto& m : joint.members)
            if (!m.resident)
                notice(m.id, stop);
        joint.nextBeat = -1;                         // (Ended by tendJoints.)
    }
}
// ------------------------------------------------------------------ Gifted and Quickened at work (doc 53, 4; Phase 6)

bool World::jointTakesGift(const std::string& id, const std::string& ability) const
{
    const auto* j = jointOf(id);
    const auto* act = j ? together::activity(j->kind) : nullptr;
    return act && std::find(act->gifts.begin(), act->gifts.end(), ability) != act->gifts.end();
}

Result World::giftOnJoint(const std::string& id, const std::string& ability)
{
    // (useWorkGift has checked the Gift, the mana and the wait, and spent them.) It holds to the end of this beat (a
    // minute for work that has none).
    const auto found = jointOf_.find(id);
    if (found == jointOf_.end() || !joints_.count(found->second))
        return {false, "You aren't working with anyone.", {}};
    auto& j = joints_[found->second];
    const auto* act = together::activity(j.kind);
    for (auto& m : j.members)
        if (m.id == id)
        {
            m.gift = ability;
            m.giftUntil = act && act->byBeat() && j.nextBeat > time_ ? j.nextBeat : time_ + 60;
            m.lastActed = time_;
        }
    const auto* e = entity(id);
    for (const auto& m : j.members)
        if (m.id != id && !m.resident)
            notice(m.id, (e ? e->name : std::string("Someone")) + "'s Gift is at the work: it goes faster this spell.");
    char rate[16];
    std::snprintf(rate, sizeof rate, "%.1f", together::rateOf(j, time_));
    return {true, "Your Gift is at the work: together you work " + std::string(rate) + " times as fast this spell.", {}};
}

Result World::keepWatch(const std::string& id, bool on)
{
    const auto found = jointOf_.find(id);
    if (found == jointOf_.end() || !joints_.count(found->second))
        return {false, "You aren't working with anyone.", {}};
    auto& j = joints_[found->second];
    const auto* c = cell(j.cellId);
    if (on && (!c || !c->outdoors || townOf(j.cellId)))
        return {false, "There is nothing to keep watch for here; out in the wild there is.", {}};
    for (auto& m : j.members)
        if (m.id == id)
        {
            m.watch = on;
            m.lastActed = time_;
        }
    return {true, on ? "You keep watch over the others." : "You set to the work again.", {}};
}

bool World::keepingWatch(const std::string& id) const
{
    if (const auto* j = jointOf(id))
        for (const auto& m : j->members)
            if (m.id == id)
                return m.watch;
    return false;
}

std::vector<std::string> World::watchersOver(const std::string& id) const
{
    std::vector<std::string> out;
    if (const auto* j = jointOf(id))
    {
        if (keepingWatch(id))
            out.push_back(id);
        for (const auto& m : j->members)
            if (m.watch && m.id != id)
                if (const auto* e = entity(m.id); e && !e->dead && e->downedLeft <= 0)
                    out.push_back(m.id);
    }
    return out;
}

// ------------------------------------------------------------------ Keeping the secret (doc 53, 4)

bool World::partners(const std::string& a, const std::string& b) const
{
    // Working together, or party mates (Game's, through setPartnered).
    if (const auto* j = jointOf(a); j && jointOf(b) == j)
        return true;
    return partnered_ && partnered_(a, b);
}

Result World::tellWardens(const std::string& witness, const std::string& wolf)
{
    auto* w = entity(witness);
    auto* q = entity(wolf);
    if (!w || w->npc || !q)
        return {false, "Tell them of whom?", {}};
    if (!w->witnessed.count(wolf))
        return {false, "You have seen nothing of theirs to tell.", {}};
    if (w->toldWardens.count(wolf))
        return {false, "You have told the Wardens of " + q->name + " already.", {}};
    w->toldWardens.insert(wolf);
    q->wardenAttention += 3;                        // (Placeholder. The Quickened wolf isn't told who told.)
    attentionRose(wolf);
    recordEvent({"told the wardens", witness, wolf, w->cellId, 0, 0, {}, 0, 0, {}});
    return {true, "The Warden writes it down. \"The Order thanks you.\"", {}};
}

Result World::vouchToWardens(const std::string& witness, const std::string& wolf)
{
    auto* w = entity(witness);
    auto* q = entity(wolf);
    if (!w || w->npc || !q)
        return {false, "Vouch for whom?", {}};
    if (!w->witnessed.count(wolf))
        return {false, "You can only vouch for one whose Gift you have seen.", {}};
    if (w->wardenStanding <= -2)
        return {false, "The Wardens no longer take your word: those you vouched for proved you wrong.", {}};
    if (calendarDays_ - w->vouchedDay < 28)
        return {false, "You vouched for someone too lately; the Wardens won't hear it again this month.", {}};
    w->vouchedDay = calendarDays_;
    w->vouchedFor = wolf;                           // (Your word rides on it for the month: attentionRose.)
    q->wardenAttention = std::max(0., q->wardenAttention - 1);
    recordEvent({"vouched to the wardens", witness, wolf, w->cellId, 0, 0, {}, 0, 0, {}});
    return {true, "The Warden hears you out. \"Your word is noted, and so is your name.\"", {}};
}

// ------------------------------------------------------------------ Talker and doer (doc 53, 4)

void World::faceTalker(const std::string& resident, const std::string& talker, double seconds)
{
    const auto* r = entity(resident);
    if (!r || !r->npc || !entity(talker))
        return;
    talkFacing_[resident] = {talker, time_ + seconds};
    tendTalkers();
}

std::string World::facingTalker(const std::string& resident) const
{
    const auto found = talkFacing_.find(resident);
    return found != talkFacing_.end() && found->second.second >= time_ ? found->second.first : std::string();
}

void World::tendTalkers()
{
    for (auto it = talkFacing_.begin(); it != talkFacing_.end();)
    {
        auto* r = entity(it->first);
        const auto* t = entity(it->second.first);
        if (!r || !t || time_ > it->second.second || r->cellId != t->cellId)
        {
            it = talkFacing_.erase(it);
            continue;
        }
        // Standing (not walking anywhere), it turns to the one it talks with.
        if (r->path.empty() && std::hypot(r->velocity.x, r->velocity.y) < 1e-6)
        {
            const double at = std::atan2(t->position.y - r->position.y, t->position.x - r->position.x);
            r->facing = at;
            r->turnTarget = at;
            r->turning = false;
        }
        ++it;
    }
}

void World::attentionRose(const std::string& wolf)
{
    const auto* q = entity(wolf);
    for (auto& [id, e] : entities_)
        if (!e.npc && e.vouchedFor == wolf && calendarDays_ - e.vouchedDay <= 28)
        {
            e.wardenStanding -= 1;
            e.vouchedFor.clear();                   // (Once a vouch.)
            notice(id, "The Wardens hear more of " + (q ? q->name : std::string("one")) + ", whom you vouched for. They will weigh your word less" +
                           (e.wardenStanding <= -2 ? ": they take it no longer." : "."));
        }
}
} // namespace ratw
