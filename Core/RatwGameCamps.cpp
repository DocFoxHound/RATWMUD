// A Chapter's camps, Halls and Holds as players make them (Docs/Design/32-parties-chapters-factions.md, 5.3–5.7; the
// rules are RatwCamps.cpp): where ground may be taken, plans paid for from the treasury, members building by work,
// wear and mending, residents hired to work there, the shared stores, and the levels' ground.
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

std::string Game::whyNotGround(const std::string& cellId, int x, int y) const
{
    // Not on a road or bridge, in water, inside a wall, through a door, or in a town (5.3).
    const auto* cell = world_.cell(cellId);
    const auto* tile = cell ? cell->tile(x, y) : nullptr;
    if (!tile)
        return "Not here.";
    if (tile->solid || tile->terrain == Terrain::Wall || tile->terrain == Terrain::Water)
        return "The ground won't take it here.";
    if (tile->glyph == 'd' || tile->glyph == '_' || tile->glyph == '8')
        return "Not on a road or a bridge.";
    if (townCells_.count(cellId))
        return "Not in a town. Rent there instead.";
    for (const auto& d : world_.doorsIn(cellId))
        if (int(std::floor(d->position.x)) == x && int(std::floor(d->position.y)) == y)
            return "Not in a doorway.";
    if (camps_.structureAt(cellId, x, y))
        return "Something stands there already.";
    return {};
}

bool Game::campCommand(Connection* c, const Value& j, Result& result)
{
    const std::string id = c->entityId, verb = j.string("verb"), target = j.string("target");
    const auto* me = world_.entity(id);
    const auto* chapter = chapters_.of(id);
    const auto* member = chapters_.member(id);
    if (!me || !chapter || !member)
        return false;
    const bool officer = member->rank <= chapter::RankOfficer;
    const int x = int(std::floor(me->position.x)), y = int(std::floor(me->position.y));
    const auto* here = camps_.siteAt(me->cellId, me->position.x, me->position.y, chapter->id);
    const double day = world_.calendarDays();
    if (verb == "camp")
    {
        const std::size_t allowed = chapter->level >= 4 ? 3 : chapter->level >= 3 ? 1 : 0;
        std::string claim;
        if (const auto* cell = world_.cell(me->cellId); cell && !cell->factionClaims.empty())
            claim = cell->factionClaims.front();
        if (!officer)
            result = {false, "Only an Officer or the Head chooses the Chapter's ground.", {}};
        else if (!allowed)
            result = {false, "A Chapter makes camp from Company (level III).", {}};
        else if (camps_.sitesOf(chapter->id).size() >= allowed)
            result = {false, "Your Chapter holds as much ground as it can for now.", {}};
        else if (const auto why = whyNotGround(me->cellId, x, y); !why.empty())
            result = {false, why, {}};
        else if (!claim.empty() && standingOf(claim, chapter->id) < 40 && !treatyAllows(claim, chapter->id, "camp"))
            result = {false, "This land is " + (factions_.find(claim) ? factions_.find(claim)->name : claim) +
                                 "'s. It lets a Chapter it trusts make camp, or one with a treaty.",
                      {}};
        else
        {
            const auto o = camps_.found(chapter->id, me->cellId, x, y, mind::trim(j.string("name", chapter->name + "'s camp")), day, now());
            result = {o.ok, o.ok ? "Your Chapter's camp is begun here. Plan what to build." : o.message, {}};
            if (o.ok)
                tellChapter(chapter->id, "The Chapter has made camp at " + (world_.cell(me->cellId) ? world_.cell(me->cellId)->name : me->cellId) + ".", id);
        }
    }
    else if (verb == "plan")
    {
        const auto* k = camp::kind(j.string("kind"));
        if (!officer)
            result = {false, "Only an Officer or the Head plans the camp.", {}};
        else if (!here)
            result = {false, "Stand on your Chapter's ground to plan it.", {}};
        else if (!k)
            result = {false, "Nothing like that can be built.", {}};
        else if (chapter->level < k->level)
            result = {false, std::string("A ") + k->name + " takes a " + chapter::levelName(k->level) + ".", {}};
        else if (const auto* cell = world_.cell(me->cellId); k->fortification && cell && !cell->factionClaims.empty() &&
                                                             standingOf(cell->factionClaims.front(), chapter->id) < 75 &&
                                                             !treatyAllows(cell->factionClaims.front(), chapter->id, "fortify"))
            result = {false, "Fortifying " + (factions_.find(cell->factionClaims.front()) ? factions_.find(cell->factionClaims.front())->name
                                                                                           : cell->factionClaims.front()) +
                                 "'s land takes its sworn friendship, or a treaty.",
                      {}};
        else if (const auto why = whyNotGround(me->cellId, x, y); !why.empty())
            result = {false, why, {}};
        else if (!world_.society().shift(treasuryOf(chapter->id), "treasury", "", 0, k->cost, "chapter building materials"))
            result = {false, std::string("The materials for a ") + k->name + " cost " + std::to_string(k->cost) + " pennies, from the treasury.", {}};
        else
        {
            const auto o = camps_.plan(here->id, k->id, x, y);
            if (!o.ok)
                world_.society().shift("treasury", treasuryOf(chapter->id), "", 0, k->cost, "chapter building materials refunded");
            else
                record(Economy);
            result = {o.ok, o.ok ? std::string("A ") + k->name + " is planned here; the materials are bought. Set to work on it." : o.message, {}};
        }
    }
    else if (verb == "unplan")
    {
        auto* st = camps_.structure(target);
        const auto* site = st ? camps_.site(st->site) : nullptr;
        if (!st || !site || site->chapter != chapter->id || !officer || st->built)
            result = {false, "Only an Officer takes back a plan not yet built.", target};
        else
        {
            if (st->work <= 0)
                if (const auto* k = camp::kind(st->kind))
                    world_.society().shift("treasury", treasuryOf(chapter->id), "", 0, k->cost, "chapter building materials refunded");
            camps_.unplan(target);
            result = {true, "The plan is taken back.", target};
        }
    }
    else if (verb == "build")
    {
        auto* st = camps_.structure(target);
        const auto* site = st ? camps_.site(st->site) : nullptr;
        if (!st || !site || site->chapter != chapter->id || site->cell != me->cellId)
            result = {false, "That isn't your Chapter's to build.", target};
        else if (std::hypot(st->x + .5 - me->position.x, st->y + .5 - me->position.y) > 2.2)
            result = {false, "Go to it first.", target};
        else
        {
            building_[id] = target;
            const auto* k = camp::kind(st->kind);
            result = {true, std::string(st->built ? "You set to mending the " : "You set to work on the ") + (k ? k->name : "work") +
                                ". Stay by it.",
                      target};
        }
    }
    else if (verb == "stopbuild")
    {
        building_.erase(id);
        result = {true, "You stop work.", {}};
    }
    else if (verb == "abandon")
    {
        auto* site = camps_.site(target);
        if (!site || site->chapter != chapter->id || member->rank != chapter::RankHead)
            result = {false, "Only the Head gives up the Chapter's ground.", target};
        else
        {
            camps_.abandon(target);
            tellChapter(chapter->id, "The Chapter gives up its ground at " + site->name + ". It will stand as a ruin.");
            result = {true, {}, target};
        }
    }
    else if (verb == "station")
    {
        // A resident travelling with the party, hired to stay and work at the camp (5.4).
        const auto* companion = parties_.companion(target);
        if (!officer || !here)
            result = {false, "An Officer stations help, standing on the Chapter's ground.", target};
        else if (!companion || !parties_.together(id, target))
            result = {false, "Only one travelling with your party can be asked to stay.", target};
        else
        {
            camp::Staff st{target, here->id, j.string("role", "hand"), companion->wage > 0 ? companion->wage : wageFor(target),
                           std::floor(day) + 1};
            parties_.releaseCompanion(target);
            releaseCompanions();
            camps_.staff()[target] = st;
            if (auto* npc = world_.entity(target))
                npc->leaderId = "camp:" + here->id;
            companionSays(target, "I'll keep the camp.");
            result = {true, "They stay to work at the camp, paid from the Chapter's treasury.", target};
        }
    }
    else if (verb == "dismissstaff")
    {
        auto& staff = camps_.staff();
        const auto found = staff.find(target);
        const auto* site = found != staff.end() ? camps_.site(found->second.site) : nullptr;
        if (!site || site->chapter != chapter->id || !officer)
            result = {false, "They don't work for your Chapter.", target};
        else
        {
            staff.erase(found);
            if (auto* npc = world_.entity(target))
                npc->leaderId.clear();
            result = {true, "They go back to their own life.", target};
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

bool Game::campHere(const std::string& who) const
{
    // The Chapter's stores reach a camp with a storage pile built.
    const auto* me = world_.entity(who);
    const auto* chapter = chapters_.of(who);
    if (!me || !chapter)
        return false;
    const auto* site = camps_.siteAt(me->cellId, me->position.x, me->position.y, chapter->id);
    return site && camps_.hasBuilt(site->id, "storage");
}

void Game::campTick(double dt)
{
    campAccumulator_ += dt;
    if (campAccumulator_ < 0.5)
        return;
    const double step = campAccumulator_;
    campAccumulator_ = 0;
    const double t = now(), day = world_.calendarDays();
    // Building: a member by their work, while they stay by it.
    for (auto it = building_.begin(); it != building_.end();)
    {
        const auto* e = world_.entity(it->first);
        auto* st = camps_.structure(it->second);
        const auto* site = st ? camps_.site(st->site) : nullptr;
        if (!e || !clientOf(it->first) || !st || !site || e->cellId != site->cell || std::hypot(st->x + .5 - e->position.x, st->y + .5 - e->position.y) > 2.2)
        {
            if (auto* c = clientOf(it->first))
                system(c, "You leave off the work.");
            it = building_.erase(it);
            continue;
        }
        if (camps_.work(st->id, step / camp::SecondsPerWorkHour))
        {
            const auto* k = camp::kind(st->kind);
            tellChapter(site->chapter, std::string("A ") + (k ? k->name : "structure") + (st->condition >= 100 && st->work >= (k ? k->hours : 0) ? " stands" : " is mended") +
                                           " at " + site->name + ".");
            for (auto b = building_.begin(); b != building_.end();)
                b = b->second == st->id && b != it ? building_.erase(b) : std::next(b);
            it = building_.erase(it);
            chapterViewsDirty_ = true;
            saveSoon();
            continue;
        }
        ++it;
    }
    // Members on their ground keep it from being abandoned.
    for (const auto* c : clients_)
        if (const auto* e = world_.entity(c->entityId))
            if (const auto* ch = chapters_.of(e->id))
                if (const auto* site = camps_.siteAt(e->cellId, e->position.x, e->position.y, ch->id))
                    if (auto* s = camps_.site(site->id))
                        s->visited = t;
    // Wear, by the game day.
    if (lastWearDay_ < 0)
        lastWearDay_ = day;
    if (day - lastWearDay_ >= 0.25)
    {
        for (const auto& ruined : camps_.wear(day - lastWearDay_, t))
            if (const auto* s = camps_.site(ruined))
                tellChapter(s->chapter, s->name + " has fallen to ruin.");
        lastWearDay_ = day;
        chapterViewsDirty_ = true;
    }
    // Staff: by their post, paid each game dawn; unpaid, they go home.
    std::vector<std::string> leaving;
    for (auto& [npcId, st] : camps_.staff())
    {
        const auto* site = camps_.site(st.site);
        auto* npc = world_.entity(npcId);
        if (!site || site->state != "standing" || !npc || npc->dead)
        {
            leaving.push_back(npcId);
            continue;
        }
        npc->leaderId = "camp:" + site->id;
        npc->activity = "working at " + site->name;
        if (npc->cellId == site->cell && npc->path.empty() && std::hypot(npc->position.x - site->x - .5, npc->position.y - site->y - .5) > 4)
            world_.moveTo(npcId, site->x + .5 + (std::hash<std::string>{}(npcId) % 3) - 1.0, site->y + 1.5);
        if (std::floor(day) >= st.paidTo)
        {
            if (world_.society().shift(treasuryOf(site->chapter), npcId, "", 0, st.wage, "camp wage"))
            {
                st.paidTo = std::floor(day) + 1;
                record(Economy);
            }
            else
            {
                tellChapter(site->chapter, names::capitalised(strangerLabel(npcId)) + " leaves the camp: the treasury couldn't pay them.");
                leaving.push_back(npcId);
            }
        }
    }
    for (const auto& npcId : leaving)
    {
        camps_.staff().erase(npcId);
        if (auto* npc = world_.entity(npcId))
            npc->leaderId.clear();
    }
    // The levels' ground: a camp standing (Hall), a fortified camp (Hold).
    std::vector<std::string> ids;
    for (const auto& [id, c] : chapters_.all())
        ids.push_back(id);
    for (const auto& id : ids)
        if (auto* c = chapters_.byId(id))
        {
            const bool standing = camps_.campStanding(id), fortified = camps_.fortified(id);
            if (standing != c->campStanding || fortified != c->fortified)
            {
                c->campStanding = standing;
                c->fortified = fortified;
                chapterAdvanced(id);
            }
        }
}

Value Game::structuresView(const std::string& viewer, const std::string& cellId) const
{
    auto list = Value::array();
    const auto* mine = chapters_.of(viewer);
    for (const auto* st : camps_.structuresIn(cellId))
    {
        const auto* site = camps_.site(st->site);
        const auto* k = camp::kind(st->kind);
        const auto* owner = site ? chapters_.byId(site->chapter) : nullptr;
        if (!site || !k)
            continue;
        auto o = Value::object();
        o.add("id", st->id);
        o.add("kind", st->kind);
        o.add("name", k->name);
        o.add("glyph", std::string(1, k->glyph));
        o.add("x", st->x);
        o.add("y", st->y);
        o.add("built", st->built);
        o.add("progress", std::round(st->work / k->hours * 100));
        o.add("condition", std::round(st->condition));
        o.add("ruin", site->state == "ruin");
        if (owner)
            o.add("colour", owner->colour);
        o.add("mine", mine && site->chapter == mine->id);
        list.push(o);
    }
    return list;
}

Value Game::campView(const std::string& viewer) const
{
    // The Chapter's ground where this wolf stands: what is built and planned, who works there, what can be planned.
    const auto* me = world_.entity(viewer);
    const auto* chapter = chapters_.of(viewer);
    const auto* site = me && chapter ? camps_.siteAt(me->cellId, me->position.x, me->position.y, chapter->id) : nullptr;
    if (!site)
        return {};
    auto v = Value::object();
    v.add("id", site->id);
    v.add("name", site->name);
    auto structures = Value::array();
    for (const auto* st : camps_.structuresOf(site->id))
    {
        const auto* k = camp::kind(st->kind);
        auto o = Value::object();
        o.add("id", st->id);
        o.add("name", k ? k->name : st->kind);
        o.add("built", st->built);
        o.add("progress", k ? std::round(st->work / k->hours * 100) : 0.0);
        o.add("condition", std::round(st->condition));
        o.add("near", std::hypot(st->x + .5 - me->position.x, st->y + .5 - me->position.y) <= 2.2);
        o.add("working", [&] {
            const auto b = building_.find(viewer);
            return b != building_.end() && b->second == st->id;
        }());
        structures.push(o);
    }
    v.add("structures", structures);
    auto kinds = Value::array();
    for (const auto& k : camp::catalogue())
        if (chapter->level >= k.level)
        {
            auto o = Value::object();
            o.add("id", k.id);
            o.add("name", k.name);
            o.add("cost", double(k.cost));
            o.add("hours", k.hours);
            kinds.push(o);
        }
    v.add("kinds", kinds);
    auto staff = Value::array();
    for (const auto& [npc, st] : camps_.staff())
        if (st.site == site->id)
        {
            auto o = Value::object();
            o.add("id", npc);
            o.add("name", names::capitalised(labelFor(viewer, npc)));
            o.add("wage", double(st.wage));
            staff.push(o);
        }
    v.add("staff", staff);
    return v;
}

Value Game::sitesView(const std::string& chapterId) const
{
    auto list = Value::array();
    for (const auto* s : camps_.sitesOf(chapterId))
    {
        auto o = Value::object();
        o.add("id", s->id);
        o.add("name", s->name);
        o.add("cell", s->cell);
        o.add("x", s->x);
        o.add("y", s->y);
        o.add("place", world_.cell(s->cell) ? world_.cell(s->cell)->name : s->cell);
        o.add("built", camps_.built(s->id));
        list.push(o);
    }
    return list;
}
} // namespace ratw::game

