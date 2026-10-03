// Halls and Holds (Docs/Design/32-parties-chapters-factions.md, Phase 9): treaties with a faction (4.6) and their
// tithes and levies (4.4), the Hold's claim on its place and the faction friendship the Hold level needs (3.4, 5.5),
// residents sworn to a Chapter for life and settled at its Hold, and recognition as a minor House. A treaty or a House
// is decided by a Dungeon Master; without one within a game day, by the faction's own rule.
#include "RatwGame.h"

#include <algorithm>
#include <cmath>

namespace ratw::game
{
using json::Value;

namespace
{
std::string treasuryOf(const std::string& chapterId) { return "chapter:" + chapterId; }
} // namespace

bool Game::treatyAllows(const std::string& factionId, const std::string& chapterId, const std::string&) const
{
    const auto* t = factions_.activeTreaty(factionId, chapterId);
    return t && t->build;
}

Result Game::decideTreaty(const std::string& id, bool approve, const std::string& by)
{
    auto* t = factions_.treaty(id);
    if (!t || t->state != "pending")
        return {false, "No such treaty is waiting.", id};
    const auto* f = factions_.find(t->faction);
    const std::string name = f ? f->name : t->faction;
    if (!approve)
    {
        t->state = "rejected";
        tellChapter(t->chapter, name + " turns down the treaty" + (by.empty() ? "." : " (" + by + ")."));
        return {true, "Turned down.", id};
    }
    if (const auto* old = factions_.activeTreaty(t->faction, t->chapter))
        factions_.treaty(old->id)->state = "ended";      // A new treaty replaces the old.
    t->state = "active";
    t->started = world_.calendarDays();
    t->paidTo = t->started;                           // The first tithe falls due at once.
    tellChapter(t->chapter, "The treaty with " + name + " is agreed" + (by.empty() ? "." : " (" + by + ").") +
                                (t->build ? " The Chapter may build on its land." : ""));
    chapterViewsDirty_ = true;
    saveSoon();
    return {true, "Agreed.", id};
}

Result Game::decideHouse(const std::string& chapterId, const std::string& factionId, bool approve, const std::string& by)
{
    for (auto& h : factions_.houseRequests())
        if (h.chapter == chapterId && h.faction == factionId && h.state == "pending")
        {
            h.state = approve ? "recognised" : "refused";
            auto* c = chapters_.byId(chapterId);
            const auto* f = factions_.find(factionId);
            if (approve && c)
            {
                c->houseOf = factionId;
                tellChapter(chapterId, (f ? f->name : factionId) + " recognises " + c->name + " as a minor House" + (by.empty() ? "." : " (" + by + ")."));
            }
            else if (c)
                tellChapter(chapterId, (f ? f->name : factionId) + " won't recognise the Chapter as a House yet.");
            chapterViewsDirty_ = true;
            saveSoon();
            return {true, approve ? "Recognised." : "Refused.", chapterId};
        }
    return {false, "No such request is waiting.", chapterId};
}

bool Game::holdCommand(Connection* c, const Value& j, Result& result)
{
    const std::string id = c->entityId, verb = j.string("verb"), target = j.string("target");
    const auto* me = world_.entity(id);
    const auto* chapter = chapters_.of(id);
    const auto* member = chapters_.member(id);
    if (!me || !chapter || !member)
        return false;
    const double day = world_.calendarDays();
    const auto* other = world_.entity(target);
    const bool near = other && other->cellId == me->cellId && std::hypot(other->position.x - me->position.x, other->position.y - me->position.y) <= 3 &&
                      world_.visionClarity(id, target) > 0;
    if (verb == "treaty")
    {
        const auto factionId = other ? officialOf(target) : std::string();
        if (member->rank != chapter::RankHead)
            result = {false, "Only the Head treats with a faction.", target};
        else if (chapter->level < 3)
            result = {false, "A Chapter treats with factions from Company (level III).", target};
        else if (!near || factionId.empty())
            result = {false, "Find one of the faction's officials, and go close.", target};
        else
        {
            faction::Treaty t;
            t.id = factions_.nextTreatyId();
            t.faction = factionId;
            t.chapter = chapter->id;
            t.build = j.boolean("build", true);
            t.tithe = std::clamp<std::int64_t>(std::int64_t(j.number("tithe", 20)), 0, 1000);
            t.levy = j.boolean("levy", true);
            t.weeks = std::clamp(int(j.number("weeks", 8)), 1, 52);
            t.proposed = day;
            factions_.treaties().push_back(t);
            note("info", "RATW_TREATY_PENDING id=" + t.id + " faction=" + factionId + " chapter=" + chapter->id);
            result = {true, "\"We'll put it before those who decide.\" (A treaty: " + std::string(t.build ? "building rights, " : "") +
                                std::to_string(t.tithe) + " pennies a week" + (t.levy ? ", answering levies" : "") + ", " +
                                std::to_string(t.weeks) + " weeks.)",
                      target};
        }
    }
    else if (verb == "house")
    {
        const auto factionId = other ? officialOf(target) : std::string();
        if (member->rank != chapter::RankHead)
            result = {false, "Only the Head asks that.", target};
        else if (chapter->level < 5)
            result = {false, "Only a Hold (level V) is recognised as a House.", target};
        else if (!near || factionId.empty())
            result = {false, "Find one of the faction's officials, and go close.", target};
        else if (standingOf(factionId, chapter->id) < 75)
            result = {false, "\"A House is made of our sworn friends, and you are not that.\"", target};
        else
        {
            factions_.houseRequests().push_back({chapter->id, factionId, "pending", day});
            note("info", "RATW_HOUSE_PENDING chapter=" + chapter->id + " faction=" + factionId);
            result = {true, "\"It will be considered.\"", target};
        }
    }
    else if (verb == "swear")
    {
        // A resident sworn to the Chapter for life (Hall, level IV): only from a deep bond.
        const auto* life = world_.society().resident(target);
        const auto* bond = world_.bonds().find(target, id);
        if (chapter->level < 4)
            result = {false, "Residents swear to a Hall (level IV) or more.", target};
        else if (!near || !other->npc || !life || (life->role != "resident" && life->role != "civilian"))
            result = {false, "Only an ordinary resident, close by, may swear to the Chapter.", target};
        else
        {
            bool taken = false;
            for (const auto& [cid, ch] : chapters_.all())
                taken |= ch.sworn.count(target) > 0;
            if (taken)
                result = {false, "They are sworn already.", target};
            else if (!bond || bond->affinity < 70 || bond->trust < 60)
            {
                companionSays(target, "That's a great deal to ask of me. Not yet.");
                result = {false, "They aren't ready to swear their life to the Chapter.", target};
            }
            else
            {
                chapters_.byId(chapter->id)->sworn.insert(target);
                companionSays(target, "Then I'm yours, and the Chapter's, for as long as I live.");
                tellChapter(chapter->id, names::capitalised(strangerLabel(target)) + " swears to the Chapter for life.", id);
                result = {true, "They swear to the Chapter.", target};
            }
        }
    }
    else if (verb == "settle")
    {
        // A sworn resident comes to live at the Hold (doc 16's relocation: they walk there, and it is their home).
        const auto* site = chapter->claimCell.empty() ? nullptr : [&]() -> const camp::Site* {
            for (const auto* s : camps_.sitesOf(chapter->id))
                if (s->cell == chapter->claimCell)
                    return s;
            return nullptr;
        }();
        if (member->rank > chapter::RankOfficer)
            result = {false, "An Officer or the Head asks that.", target};
        else if (!chapter->sworn.count(target))
            result = {false, "Only one sworn to the Chapter.", target};
        else if (!site)
            result = {false, "The Chapter has no Hold to settle them in.", target};
        else
        {
            const auto moved = world_.relocateResident(target, site->cell, site->x + .5, site->y + 1.5);
            result = {moved.ok, moved.ok ? "They set out to make their home at the Hold." : "They can't go: " + moved.message, target};
        }
    }
    else
        return false;
    if (result.ok)
    {
        chapterViewsDirty_ = true;
        saveSoon();
    }
    return true;
}

void Game::holdTick(double dt)
{
    holdAccumulator_ += dt;
    if (holdAccumulator_ < 5)
        return;
    const double step = holdAccumulator_;
    holdAccumulator_ = 0;
    const double day = world_.calendarDays();
    // Waiting a game day without a Dungeon Master: the faction's own rule decides.
    for (auto& t : factions_.treaties())
        if (t.state == "pending" && day - t.proposed >= 1)
            decideTreaty(t.id, standingOf(t.faction, t.chapter) >= 40, "by its own rule");
    for (auto h : factions_.houseRequests())
        if (h.state == "pending" && day - h.day >= 1)
        {
            const auto* c = chapters_.byId(h.chapter);
            decideHouse(h.chapter, h.faction, c && c->level >= 5 && standingOf(h.faction, h.chapter) >= 75, "by its own rule");
        }
    // Treaties: the weekly tithe from the treasury; missed, the treaty is broken. They run their course.
    for (auto& t : factions_.treaties())
    {
        if (t.state != "active")
            continue;
        const auto* f = factions_.find(t.faction);
        const std::string name = f ? f->name : t.faction;
        if (day >= t.started + t.weeks * faction::Factions::WeekDays)
        {
            t.state = "ended";
            tellChapter(t.chapter, "The treaty with " + name + " has run its course.");
            continue;
        }
        if (t.tithe > 0 && day >= t.paidTo)
        {
            if (world_.society().shift(treasuryOf(t.chapter), "treasury", "", 0, t.tithe, "treaty tithe"))
            {
                t.paidTo += faction::Factions::WeekDays;
                record(Economy);
            }
            else
            {
                t.state = "ended";
                factions_.change(t.faction, t.chapter, -10, "broke a treaty", day);
                tellChapter(t.chapter, "The treasury couldn't pay the tithe: the treaty with " + name + " is broken.");
            }
        }
        else if (t.tithe <= 0)
            t.paidTo = std::max(t.paidTo, day);
    }
    // Levies: a faction calls each week on a Chapter sworn to it, or bound by a treaty to answer.
    const double week = std::floor(day / faction::Factions::WeekDays);
    for (const auto& [cid, c] : chapters_.all())
        for (const auto& [fid, f] : factions_.all())
        {
            const auto* t = factions_.activeTreaty(fid, cid);
            if (standingOf(fid, cid) < 75 && !(t && t->levy))
                continue;
            const auto key = fid + "|" + cid;
            if (levyWeek_[key] >= week + 1)
                continue;
            levyWeek_[key] = week + 1;
            // Where: wherever one of its officials stands.
            std::string cell;
            for (const auto& [eid, e] : world_.entities())
                if (e.npc && officialOf(eid) == fid)
                {
                    cell = e.cellId;
                    break;
                }
            if (cell.empty())
                continue;
            faction::Levy l;
            l.id = "levy-" + std::to_string(++levyNext_);
            l.faction = fid;
            l.chapter = cid;
            l.cell = cell;
            l.place = world_.cell(cell) ? world_.cell(cell)->name : cell;
            l.due = (week + 1) * faction::Factions::WeekDays;
            factions_.levies().push_back(l);
            tellChapter(cid, f.name + " calls on the Chapter: keep watch at " + l.place +
                                 " this week (twenty minutes of members' time, all told).");
        }
    for (auto& l : factions_.levies())
    {
        if (l.state != "called")
            continue;
        if (const auto* c = chapters_.byId(l.chapter))
            for (const auto& [m, member] : c->members)
                if (const auto* e = world_.entity(m); e && clientOf(m) && e->cellId == l.cell)
                    l.done += step;
        const auto* f = factions_.find(l.faction);
        if (l.done >= l.needed)
        {
            l.state = "answered";
            factions_.change(l.faction, l.chapter, 5, "answered a levy", day);
            tellChapter(l.chapter, "The levy is answered. " + (f ? f->name : l.faction) + " is pleased.");
        }
        else if (day >= l.due)
        {
            l.state = "ignored";
            factions_.change(l.faction, l.chapter, -10, "ignored a levy", day);
            tellChapter(l.chapter, "The levy went unanswered. " + (f ? f->name : l.faction) + " remembers.");
        }
    }
    auto& levies = factions_.levies();
    levies.erase(std::remove_if(levies.begin(), levies.end(), [&](const faction::Levy& l) { return l.state != "called" && day - l.due > 7; }),
                 levies.end());
    // The Hold's claim, and the friendship the Hold level needs for its land.
    std::vector<std::string> ids;
    for (const auto& [id, c] : chapters_.all())
        ids.push_back(id);
    for (const auto& id : ids)
    {
        auto* c = chapters_.byId(id);
        if (!c)
            continue;
        std::string claim;
        if (camps_.hold(id))
            for (const auto* s : camps_.sitesOf(id))
            {
                int walls = 0;
                for (const auto* st : camps_.structuresOf(s->id))
                    walls += st->kind == "wall" && st->built && st->condition > 0;
                if (walls >= 8 && camps_.hasBuilt(s->id, "keep") && camps_.hasBuilt(s->id, "gatehouse"))
                    claim = s->cell;
            }
        if (claim != c->claimCell)
        {
            c->claimCell = claim;
            if (!claim.empty())
                tellChapter(id, "The Hold stands: the Chapter claims " + (world_.cell(claim) ? world_.cell(claim)->name : claim) + ".");
            chapterViewsDirty_ = true;
        }
        bool friendly = false;
        for (const auto* s : camps_.sitesOf(id))
        {
            const auto* cell = world_.cell(s->cell);
            const std::string owner = cell && !cell->factionClaims.empty() ? cell->factionClaims.front() : std::string();
            friendly |= owner.empty() || standingOf(owner, id) >= 15 || treatyAllows(owner, id, "hold");
        }
        if (friendly != c->friendlyFaction)
        {
            c->friendlyFaction = friendly;
            chapterAdvanced(id);
        }
    }
}

Value Game::holdView(const std::string& chapterId, const std::string& viewer) const
{
    auto v = Value::object();
    const auto* c = chapters_.byId(chapterId);
    if (!c)
        return v;
    if (const auto* f = factions_.find(c->houseOf))
        v.add("house", "a minor House of " + f->name);
    if (!c->claimCell.empty())
        v.add("claims", world_.cell(c->claimCell) ? world_.cell(c->claimCell)->name : c->claimCell);
    auto treaties = Value::array();
    for (const auto& t : factions_.treaties())
        if (t.chapter == chapterId && (t.state == "active" || t.state == "pending"))
        {
            auto o = Value::object();
            o.add("faction", factions_.find(t.faction) ? factions_.find(t.faction)->name : t.faction);
            o.add("state", t.state);
            o.add("build", t.build);
            o.add("tithe", double(t.tithe));
            o.add("levy", t.levy);
            o.add("weeksLeft", t.state == "active" ? std::max(0.0, std::ceil((t.started + t.weeks * 7 - world_.calendarDays()) / 7)) : double(t.weeks));
            treaties.push(o);
        }
    v.add("treaties", treaties);
    auto levies = Value::array();
    for (const auto& l : factions_.levies())
        if (l.chapter == chapterId && l.state == "called")
        {
            auto o = Value::object();
            o.add("faction", factions_.find(l.faction) ? factions_.find(l.faction)->name : l.faction);
            o.add("place", l.place);
            o.add("minutes", std::round((l.needed - l.done) / 60));
            levies.push(o);
        }
    v.add("levies", levies);
    auto sworn = Value::array();
    for (const auto& n : c->sworn)
        sworn.push(names::capitalised(labelFor(viewer, n)));
    v.add("sworn", sworn);
    return v;
}

std::string Game::swornContext(const std::string& npcId) const
{
    for (const auto& [id, c] : chapters_.all())
        if (c.sworn.count(npcId))
            return " You are sworn for life to the Chapter " + c.name + (c.houseOf.empty() ? "" : ", a minor House") + ".";
    return {};
}
} // namespace ratw::game
