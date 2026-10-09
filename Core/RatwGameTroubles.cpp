// Residents' troubles in the game (Docs/Design/57-changing-the-world.md, 3). A resident's trouble is read from the
// simulation (RatwTroubles.cpp) when a player talks to them, and kept a game day. It is spoken of only to a wolf the
// resident trusts: its Mind is told it as a `trouble` field, and the game's own answer to "is something troubling you?"
// says it in a written line. A wolf who has heard one sees it in its unfinished business while it lasts.
#include "RatwGame.h"

#include "RatwBonds.h"

#include <algorithm>
#include <cmath>

namespace ratw::game
{
using json::Value;

troubles::Reads Game::troubleReads()
{
    troubles::Reads r;
    r.society = &world_.society();
    r.bonds = &world_.bonds();
    r.age = [this](const std::string& id) {
        const auto* e = world_.entity(id);
        if (e)
            return e->age;
        const auto* spec = world_.society().spec(id);
        return spec ? spec->age : 30;
    };
    r.alive = [this](const std::string& id) {
        const auto* e = world_.entity(id);
        return e && !e->dead;
    };
    r.communityOf = [this](const std::string& cell) { return world_.communityOf(cell); };
    r.idleDays = [this](const std::string& id) {
        const double today = world_.calendarDays();
        return today - idleSince_.try_emplace(id, today).first->second;
    };
    r.resting = [this](const std::string& id, const std::string& kind) {
        const auto rest = troubleRests_.find(id + "|" + kind);
        return rest != troubleRests_.end() && world_.calendarDays() - rest->second < troubles::rules().restDays;
    };
    r.day = world_.calendarDays();
    return r;
}

const troubles::Trouble& Game::troubleOf(const std::string& resident)
{
    // Kept a game day: troubles change as slowly as purses and careers do.
    auto& seen = troubles_[resident];
    const double today = std::floor(world_.calendarDays());
    if (seen.day != today)
    {
        seen.trouble = troubles::troubleOf(resident, troubleReads());
        seen.day = today;
        if (world_.society().jobOf(resident))
            idleSince_.erase(resident);
    }
    return seen.trouble;
}

bool Game::troubleSpoken(const std::string& npc, const std::string& wolf) const
{
    const auto& rules = troubles::rules();
    const auto* bond = world_.bonds().find(npc, wolf);
    return bond && bond->trust >= rules.trust && bond->familiarity >= rules.familiarity;
}

std::map<std::string, std::string> Game::troubleBlanks(const std::string& viewer, const troubles::Trouble& t) const
{
    // Residents as `viewer` knows them: the troubled resident itself names its own kin and neighbours by name.
    std::map<std::string, std::string> b;
    const auto name = [&](const std::string& id) {
        const auto* e = world_.entity(id);
        if (!e)
            return std::string("someone");
        const auto* v = world_.entity(viewer);
        return v && v->npc ? e->name : labelFor(viewer, id);
    };
    b["coins"] = std::to_string(t.coins);
    b["days"] = std::to_string(std::max(0, int(std::floor(t.days))));
    b["need"] = std::to_string(t.coins);
    b["age"] = std::to_string(t.age);
    b["who"] = name(t.resident);
    if (!t.other.empty())
        b["other"] = t.other.rfind("till:", 0) == 0 ? std::string("the shop") : name(t.other);
    const auto* other = world_.entity(t.other);
    const bool female = other && other->appearance->sex == "female";
    b["them"] = female ? "her" : "him";
    b["their"] = female ? "her" : "his";
    if (const auto* job = world_.society().jobOf(t.resident))
        b["shop"] = job->title;
    return b;
}

std::string Game::troubleBriefing(const std::string& npc, const std::string& wolf)
{
    // Told to the Mind only past the trust rule; what it was told is kept so a reply that speaks of it marks it heard.
    troubleBriefed_.erase(npc + "|" + wolf);
    const auto* e = world_.entity(npc);
    if (!e || !e->npc || !troubleSpoken(npc, wolf))
        return {};
    const auto& t = troubleOf(npc);
    const auto* kind = t ? troubles::rules().kind(t.kind) : nullptr;
    if (!kind || kind->briefing.empty())
        return {};
    troubleBriefed_[npc + "|" + wolf] = t.kind;
    auto line = troubles::fill(kind->briefing, troubleBlanks(npc, t));
    if (const auto heard = troublesHeard_.find(wolf); heard != troublesHeard_.end())
        if (const auto mine = heard->second.find(npc); mine != heard->second.end() && mine->second.kind == t.kind)
            line += " You have told them of it before.";
    return line;
}

std::string Game::troubleSaid(const std::string& npc, const std::string& wolf, std::size_t seed)
{
    // The game's own answer to "what's wrong?": a written line of the trouble to a wolf it trusts; to anyone else, or with
    // nothing wrong, a polite nothing.
    const auto& rules = troubles::rules();
    const auto nothing = [&] { return rules.refusal.empty() ? std::string("Oh, nothing worth your time.") : rules.refusal[seed % rules.refusal.size()]; };
    if (!troubleSpoken(npc, wolf))
        return nothing();
    const auto& t = troubleOf(npc);
    const auto* kind = t ? rules.kind(t.kind) : nullptr;
    if (!kind || kind->said.empty())
        return nothing();
    heardTrouble(wolf, npc, t.kind);
    return troubles::fill(kind->said[seed % kind->said.size()], troubleBlanks(npc, t));
}

void Game::heardTrouble(const std::string& wolf, const std::string& npc, const std::string& kind)
{
    auto& mine = troublesHeard_[wolf];
    const auto [at, fresh] = mine.try_emplace(npc, TroubleHeard{kind, world_.calendarDays()});
    if (!fresh && at->second.kind == kind)
        return;
    at->second = {kind, world_.calendarDays()};
    unfinished_.erase(wolf);
    logEvent("trouble heard", wolf, npc, kind);
    if (const auto* e = world_.entity(wolf); e && !e->npc)   // (Its storyline: doc 58, 3.)
    {
        auto cast = Value::object();
        cast.add("resident", npc);
        giveStoryline(wolf, "trouble-" + kind, cast, "trouble", npc + "|" + kind);
    }
    saveSoon();
}

void Game::troublesSave(Value& root) const
{
    // Who has heard of whose trouble (a few a wolf): the ledger of the troubles themselves is the simulation.
    auto list = Value::array();
    for (const auto& [wolf, heard] : troublesHeard_)
        for (const auto& [npc, h] : heard)
        {
            auto o = Value::object();
            o.add("wolf", wolf);
            o.add("resident", npc);
            o.add("kind", h.kind);
            o.add("day", h.day);
            list.push(o);
        }
    root.add("troublesHeard", list);
    // The limits on solving: who solved whose lately, and the kinds resting.
    auto solved = Value::object();
    for (const auto& [key, day] : troubleSolvedBy_)
        solved.add(key, day);
    root.add("troublesSolved", solved);
    auto rests = Value::object();
    for (const auto& [key, day] : troubleRests_)
        rests.add(key, day);
    root.add("troublesResting", rests);
    auto marks = Value::object();                   // (The Dungeon Master's protected and unprotected residents: doc 57, 6.)
    for (const auto& id : protectedMarks_)
        marks.add(id, true);
    for (const auto& id : unprotectedMarks_)
        marks.add(id, false);
    root.add("protectedMarks", marks);
}

void Game::troublesLoad(const Value& saved)
{
    troublesHeard_.clear();
    if (const auto* list = saved.find("troublesHeard"))
        for (const auto& o : list->items())
        {
            const auto wolf = o.string("wolf"), npc = o.string("resident"), kind = o.string("kind");
            if (!wolf.empty() && !npc.empty() && troubles::rules().kind(kind) && wolf.size() <= 80 && npc.size() <= 80)
                troublesHeard_[wolf][npc] = {kind, o.number("day")};
        }
    troubleSolvedBy_.clear();
    troubleRests_.clear();
    for (const auto& [key, day] : saved.object("troublesSolved").fields())
        if (day.isNumber() && key.size() <= 200)
            troubleSolvedBy_[key] = day.asNumber();
    for (const auto& [key, day] : saved.object("troublesResting").fields())
        if (day.isNumber() && key.size() <= 200)
            troubleRests_[key] = day.asNumber();
    protectedMarks_.clear();
    unprotectedMarks_.clear();
    for (const auto& [id, on] : saved.object("protectedMarks").fields())
        if (on.isBool() && id.size() <= 80)
            (on.asBool() ? protectedMarks_ : unprotectedMarks_).insert(id);
}

// ------------------------------------------------------------------ Phase 2: solving them

namespace
{
// "trouble:<verb>:<resident>|<label>" -> verb, resident.
bool troubleParts(const std::string& action, std::string& verb, std::string& resident)
{
    const auto body = action.substr(0, action.find('|'));
    const auto colon = body.find(':', 8);
    if (body.rfind("trouble:", 0) != 0 || colon == std::string::npos)
        return false;
    verb = body.substr(8, colon - 8);
    resident = body.substr(colon + 1);
    return !verb.empty() && !resident.empty();
}
} // namespace

std::string Game::vacancyFor(const std::string& employer, const std::string& resident) const
{
    // A post standing empty where the employer works (its own business's help, a farm's hands), not the watch's.
    const auto& s = world_.society();
    const auto* mine = s.jobOf(employer);
    if (!mine || mine->role == "guard" || employer == resident)
        return {};
    for (const auto& p : s.positions())
    {
        const auto held = s.state().careers.positions.find(p.id);
        if (p.id == mine->id || p.role == "guard" || p.work.cell != mine->work.cell ||
            (held != s.state().careers.positions.end() && !held->second.holder.empty()))
            continue;
        return p.id;
    }
    return {};
}

const Position* Game::mastersTrade(const std::string& master) const
{
    // A master who would take an apprentice under the careers' rules: 35 or skilled 70, not the watch, none yet.
    const auto& s = world_.society();
    const auto* job = s.jobOf(master);
    const auto* e = world_.entity(master);
    if (!job || !e || e->dead || job->role == "guard")
        return nullptr;
    const auto held = s.state().careers.positions.find(job->id);
    if (held == s.state().careers.positions.end() || !held->second.apprentice.empty())
        return nullptr;
    return e->age >= 35 || s.skill(master, job->id) >= 70 ? job : nullptr;
}

void Game::troubleActions(const Entity& self, const Entity& e, double apart, Value& actions)
{
    // Offered only for troubles this wolf has heard of, and only where it can act: beside the troubled resident, or
    // the employer or master who could help.
    const auto heard = troublesHeard_.find(self.id);
    if (heard == troublesHeard_.end() || !e.npc || e.dead || apart > 4)
        return;
    const auto& rules = troubles::rules();
    for (const auto& [resident, h] : heard->second)
    {
        if (const auto by = troubleSolvedBy_.find(resident + "|" + self.id);
            by != troubleSolvedBy_.end() && world_.calendarDays() - by->second < rules.perWolfDays)
            continue;
        const auto& t = troubleOf(resident);
        if (!t || t.kind != h.kind)
            continue;
        const auto* kind = rules.kind(t.kind);
        if (t.kind == "debt" && e.id == resident && apart <= 3)
            actions.push("trouble:loan:" + resident + "|Pay off their loan (" + std::to_string(t.coins) + "p)");
        else if (t.kind == "short" && e.id == resident && apart <= 3)
            actions.push("trouble:help:" + resident + "|Help the household (" + std::to_string(t.coins) + "p)");
        else if (t.kind == "feud" && (e.id == resident || e.id == t.other) && apart <= rules.peaceTiles)
            actions.push("trouble:peace:" + resident + "|Make peace with " + labelFor(self.id, e.id == resident ? t.other : resident));
        else if (t.kind == "work" && e.id != resident && apart <= 3 && !vacancyFor(e.id, resident).empty())
            actions.push("trouble:speak:" + resident + "|Speak for " + labelFor(self.id, resident));
        else if (t.kind == "child" && e.id != resident && e.id != t.other && apart <= 3 && mastersTrade(e.id))
            actions.push("trouble:sponsor:" + resident + "|Sponsor " + labelFor(self.id, t.other) + "'s apprenticeship (" +
                         std::to_string(kind ? kind->fee : 20) + "p)");
    }
}

Result Game::troubleAction(const std::string& wolf, const std::string& target, const std::string& action)
{
    std::string verb, resident;
    if (!troubleParts(action, verb, resident))
        return {false, "There's nothing to be done about that.", {}};
    const auto& rules = troubles::rules();
    const auto heard = troublesHeard_.find(wolf);
    const auto* w = world_.entity(wolf);
    const auto* at = world_.entity(target);
    const auto* r = world_.entity(resident);
    if (!w || !at || !r || at->cellId != w->cellId || heard == troublesHeard_.end() || !heard->second.count(resident))
        return {false, "You know of no trouble of theirs.", {}};
    const double apart = std::hypot(at->position.x - w->position.x, at->position.y - w->position.y);
    if (apart > (verb == "peace" ? rules.peaceTiles : 3))
        return {false, "Come closer.", {}};
    if (const auto by = troubleSolvedBy_.find(resident + "|" + wolf); by != troubleSolvedBy_.end() && world_.calendarDays() - by->second < rules.perWolfDays)
        return {false, "You have done enough for them this season; let them stand on their own a while.", {}};
    troubles_.erase(resident);                      // (Read afresh: the purse or the post may have changed since the morning.)
    const auto t = troubleOf(resident);
    const auto* kind = t ? rules.kind(t.kind) : nullptr;
    if (!kind || t.kind != heard->second.at(resident).kind)
        return {false, "That trouble has passed.", {}};
    auto& society = world_.society();
    const auto* purse = society.account(wolf);
    const auto cash = purse ? purse->cash : 0;
    const auto name = [&](const std::string& id) { return names::capitalised(labelFor(wolf, id)); };
    if (verb == "loan" && t.kind == "debt" && target == resident)
    {
        if (cash < t.coins)
            return {false, "You haven't " + std::to_string(t.coins) + " pennies.", {}};
        const auto paid = society.repayRescue(t.other, wolf, t.coins);
        if (paid <= 0)
            return {false, "The loan can't be paid just now.", {}};
        record(Economy | Character, wolf);
        troubleSolved(wolf, t, "paid_debt", paid >= rules.notableDebt ? fame::Notable : -1, paid,
                      "You pay off the loan: " + std::to_string(paid) + " pennies to the town's rescue fund. " + name(resident) +
                          "'s shop owes nothing now, and stays open.");
        return {true, {}, resident};
    }
    if (verb == "help" && t.kind == "short" && target == resident)
    {
        if (cash < t.coins)
            return {false, "You haven't " + std::to_string(t.coins) + " pennies.", {}};
        if (!society.shift(wolf, resident, "", 0, t.coins, "help for a household"))
            return {false, "It can't be given just now.", {}};
        record(Economy | Character, wolf);
        troubleSolved(wolf, t, "fed_household", -1, t.coins,
                      "You give " + std::to_string(t.coins) + " pennies. " + name(resident) + "'s household has food enough for a fortnight now.");
        return {true, {}, resident};
    }
    if (verb == "peace" && t.kind == "feud" && (target == resident || target == t.other))
    {
        const auto* other = world_.entity(t.other);
        if (!other || other->dead || other->cellId != w->cellId)
            return {false, "Bring them together first.", {}};
        const auto near = [&](const Entity& a, const Entity& b) {
            return std::hypot(a.position.x - b.position.x, a.position.y - b.position.y) <= rules.peaceTiles;
        };
        if (!near(*r, *other) || !near(*w, *r) || !near(*w, *other))
            return {false, "Bring the two of them together, close by you, first.", {}};
        for (const auto& id : {resident, t.other})
            if (const auto* b = world_.bonds().find(id, wolf); !b || b->trust < rules.peaceTrust)
                return {false, name(id) + " doesn't trust you enough to listen.", {}};
        world_.bonds().mutual(resident, t.other, {25, 10, 2, 0, 0}, world_.calendarDays());
        // Heads of two households: notable; heads of two great houses: great.
        const auto head = [&](const std::string& id) {
            const auto* job = society.jobOf(id);
            if (job && Society::houseHead(job->title))
                return 2;
            const auto* life = society.resident(id);
            if (!life || life->homeCell.empty())
                return 0;
            for (const auto& [other, l] : society.state().residents)
                if (other != id && l.homeCell == life->homeCell)
                    if (const auto* e = world_.entity(other); e && !e->dead && e->age > r->age)
                        return 0;
            return 1;
        };
        const int heads = std::min(head(resident), head(t.other));
        troubleSolved(wolf, t, "made_peace", heads >= 2 ? fame::Great : heads >= 1 ? fame::Notable : -1, 0,
                      name(resident) + " and " + labelFor(wolf, t.other) + " make their peace, grudgingly at first.");
        troubleRests_[t.other + "|feud"] = world_.calendarDays();
        return {true, {}, resident};
    }
    if (verb == "speak" && t.kind == "work" && target != resident)
    {
        const auto post = vacancyFor(target, resident);
        if (post.empty())
            return {false, name(target) + " has no post to fill.", {}};
        if (const auto* b = world_.bonds().find(target, wolf); !b || b->trust < rules.employerTrust)
            return {false, name(target) + " doesn't know you well enough to take your word for it.", {}};
        const auto done = world_.appointResident(post, resident);
        if (!done.ok)
            return {false, done.message, {}};
        const auto* p = society.position(post);
        troubleSolved(wolf, t, "found_work", t.days >= kind->notableDays ? fame::Notable : -1, 0,
                      name(target) + " takes your word for it, and takes " + labelFor(wolf, resident) + " on" +
                          (p ? ": " + p->title : std::string()) + ".");
        return {true, {}, resident};
    }
    if (verb == "sponsor" && t.kind == "child" && target != resident)
    {
        const auto* trade = mastersTrade(target);
        if (!trade)
            return {false, name(target) + " can't take an apprentice just now.", {}};
        if (const auto* b = world_.bonds().find(target, wolf); !b || b->trust < rules.masterTrust)
            return {false, name(target) + " doesn't know you well enough to take your word for it.", {}};
        if (cash < kind->fee)
            return {false, "The fee is " + std::to_string(kind->fee) + " pennies; you haven't so much.", {}};
        const auto done = world_.apprenticeResident(trade->id, t.other);
        if (!done.ok)
            return {false, done.message, {}};
        society.shift(wolf, target, "", 0, kind->fee, "an apprenticeship's fee");
        record(Economy | Character, wolf);
        troubleSolved(wolf, t, "sponsored_apprentice", -1, kind->fee,
                      "You pay the fee of " + std::to_string(kind->fee) + " pennies. " + name(target) + " takes " + labelFor(wolf, t.other) +
                          " on as an apprentice: " + trade->title + ".");
        return {true, {}, resident};
    }
    return {false, "That won't help with this.", {}};
}

void Game::troubleSolved(const std::string& wolf, const troubles::Trouble& t, const std::string& deedKind, int weight,
                         std::int64_t coins, const std::string& words)
{
    // What changed, said plainly; the bonds; the event; the deed (doc 56), which makes the rumour; the limits.
    const double today = world_.calendarDays();
    auto& bonds = world_.bonds();
    bonds.change(t.resident, wolf, {8, 8, 2, 0, 5}, today);
    if (t.kind == "feud")
        bonds.change(t.other, wolf, {8, 8, 2, 0, 5}, today);
    if (const auto* life = world_.society().resident(t.resident); life && !life->homeCell.empty() && !Society::communalHome(life->homeCell))
        for (const auto& [id, other] : world_.society().state().residents)
            if (id != t.resident && other.homeCell == life->homeCell)
                if (const auto* e = world_.entity(id); e && !e->dead)
                    bonds.change(id, wolf, {3, 3, 1, 0, 0}, today);
    const auto* w = world_.entity(wolf);
    world_.recordEvent({"trouble solved", wolf, t.resident, w ? w->cellId : std::string(), 0, 0, t.kind, 0, coins, {}});
    memories_.record(t.resident, wolf, {sequence_++, now(), "(what they did)", words});
    recordDeed(deedKind, {wolf}, t.resident, w ? w->cellId : std::string(), "trouble", t.kind, weight);
    troubleSolvedBy_[t.resident + "|" + wolf] = today;
    troubleRests_[t.resident + "|" + t.kind] = today;
    // The trouble's storylines (doc 58, 3): the solver's done; anyone else's ends, someone else having seen to it.
    for (const auto& [sid, s] : storylines_.all())
        if (s.source == "trouble" && s.sourceRef == t.resident + "|" + t.kind && s.live())
        {
            if (s.takesPart(wolf))
            {
                const auto story = sid;
                for (std::size_t step = storylines_.find(story)->current(); auto* run = storylines_.find(story);)
                {
                    if (!run->live() || step >= run->steps.size())
                        break;
                    for (std::size_t o = 0; o < run->steps[step].objectives.size(); ++o)
                        storylineProgress(storylines_.tick(story, step, o, wolf, now(), false));
                    step = storylines_.find(story)->current();
                }
            }
            else
                storylines_.end(sid, "ended", now());
        }
    troubles_.erase(t.resident);
    if (auto heard = troublesHeard_.find(wolf); heard != troublesHeard_.end())
        heard->second.erase(t.resident);
    unfinished_.erase(wolf);
    if (auto* c = clientOf(wolf))
        system(c, words);
    saveSoon();
}

troubles::Trouble Game::shortHousehold(const std::string& member)
{
    // The household a gift goes to, if it is short of coin (read afresh: before the gift moves).
    const auto* life = world_.society().resident(member);
    if (!life || life->homeCell.empty())
        return {};
    for (const auto& [id, other] : world_.society().state().residents)
        if (other.homeCell == life->homeCell)
            if (auto t = troubles::kindOf("short", id, troubleReads()))
                return t;
    return {};
}

void Game::troubleGiven(const std::string& giver, const troubles::Trouble& before)
{
    // A gift that brings a short household to its refill (doc 57, 3) solves its trouble, once a season by this wolf.
    const auto* kind = troubles::rules().kind("short");
    const auto* life = before ? world_.society().resident(before.resident) : nullptr;
    if (!kind || !life || !playerAccountId(giver))
        return;
    if (const auto by = troubleSolvedBy_.find(before.resident + "|" + giver);
        by != troubleSolvedBy_.end() && world_.calendarDays() - by->second < troubles::rules().perWolfDays)
        return;
    if (troubles::foodDays(before.household, life->homeCell, troubleReads()) < kind->refillDays)
        return;
    troubleSolved(giver, before, "fed_household", -1, 0,
                  names::capitalised(labelFor(giver, before.resident)) + "'s household has food enough for a fortnight now.");
}

// ------------------------------------------------------------------ Phase 6: protected residents

bool Game::isProtected(const std::string& id) const
{
    if (unprotectedMarks_.count(id))
        return false;
    if (protectedMarks_.count(id))
        return true;
    const auto& society = world_.society();
    if (const auto* spec = society.spec(id); spec && spec->protectedNpc)
        return true;
    if (const auto* job = society.jobOf(id); job && Society::houseHead(job->title))
        return true;
    if (const auto* member = factions_.memberOf(id); member && !member->second.empty() && factions_.explicitMember(id))
        return true;
    return false;
}
} // namespace ratw::game
