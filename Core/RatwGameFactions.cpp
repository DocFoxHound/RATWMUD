// Factions in play (Docs/Design/32-parties-chapters-factions.md, Part 4; the rules are RatwFactions.cpp): factions,
// their members and relations from the world, a Chapter's standing and how it moves, members' crimes as a burden the
// faction holds, reports asked of its officials, its merchants' prices, its mission boards, and what the NPC Mind and
// each Chapter member are told.
#include "RatwGame.h"

#include <algorithm>
#include <cmath>

namespace ratw::game
{
using json::Value;

namespace
{
std::string lowered(std::string s)
{
    for (auto& ch : s)
        ch = char(std::tolower(static_cast<unsigned char>(ch)));
    return s;
}
} // namespace

void Game::refreshFactions()
{
    // The world's factions; with a database, their kinds, named members and relations from the live tables.
    for (const auto& [id, f] : world_.factions())
        if (!factions_.find(id) || factions_.find(id)->name != f.name)
            factions_.define({id, f.name, f.color, factions_.find(id) ? factions_.find(id)->kind : std::string()});
    if (!options_.database.empty() && !liveWorldId_.empty())
    {
        if (const auto kinds = worldDb_.exec("SELECT id, kind FROM live.factions WHERE world_id = $1", {liveWorldId_}); kinds.ok)
            for (const auto& row : kinds.rows)
                if (row.size() >= 2 && row[0] && row[1])
                    if (const auto* f = factions_.find(*row[0]))
                        factions_.define({f->id, f->name, f->colour, *row[1]});
        if (const auto members = worldDb_.exec("SELECT faction_id, npc_id, rank FROM live.faction_members WHERE world_id = $1", {liveWorldId_});
            members.ok)
            for (const auto& row : members.rows)
                if (row.size() >= 3 && row[0] && row[1])
                    factions_.setMember(*row[1], *row[0], row[2] ? *row[2] : std::string(), true);
        if (const auto relations = worldDb_.exec(
                "SELECT faction_id, other_id, disposition, stance FROM live.faction_relations WHERE world_id = $1", {liveWorldId_});
            relations.ok)
            for (const auto& row : relations.rows)
                if (row.size() >= 4 && row[0] && row[1] && row[2] && row[3])
                    factions_.setRelation(*row[0], *row[1], {std::atoi(row[2]->c_str()), *row[3]});
    }
    // Everyone else: whoever works in a place a faction claims serves that faction (its guards, its traders).
    factions_.clearDerived();
    for (const auto& p : world_.society().positions())
    {
        const auto held = world_.society().state().careers.positions.find(p.id);
        if (held == world_.society().state().careers.positions.end() || held->second.holder.empty())
            continue;
        if (const auto* cell = world_.cell(p.work.cell); cell && !cell->factionClaims.empty())
            if (factions_.find(cell->factionClaims.front()))
                factions_.setMember(held->second.holder, cell->factionClaims.front(), p.role, false);
    }
}

std::string Game::officialOf(const std::string& npcId) const
{
    const auto* member = factions_.memberOf(npcId);
    if (!member)
        return {};
    const auto* job = world_.society().jobOf(npcId);
    const bool post = job && (job->role == "guard" || job->role == "merchant");
    return post || (!member->second.empty() && member->second != "civilian") ? member->first : std::string();
}

std::vector<std::string> Game::chapterMembers(const std::string& chapterId) const
{
    std::vector<std::string> out;
    if (const auto* c = chapters_.byId(chapterId))
        for (const auto& [m, member] : c->members)
            out.push_back(m);
    return out;
}

double Game::standingOf(const std::string& factionId, const std::string& chapterId) const
{
    return factions_.effective(factionId, chapterId, chapterMembers(chapterId));
}

void Game::factionTick(double dt)
{
    factionAccumulator_ += dt;
    if (factionAccumulator_ < 2)
        return;
    factionAccumulator_ = 0;
    if ((factionRefresh_ -= 2) <= 0)
    {
        refreshFactions();
        factionRefresh_ = 60;
    }
    const double day = world_.calendarDays();
    factions_.drift(day);
    factions_.spread(day);
    // Crimes the faction holds against a wolf (4.2a): an incident against one of its own where the offender was seen
    // and known by sight, weighted by kind and how clearly; a conviction by its Watch; and restitution paid.
    for (const auto& inc : world_.crime().incidents)
    {
        if (!factionIncidents_.count(inc.id))
        {
            double seen = 0;
            for (const auto& w : inc.witnesses)
                if (w.identified)
                    seen = std::max(seen, w.clarity);
            const auto* offender = world_.entity(inc.offender);
            const bool player = characters_.count(inc.offender) || (offender && !offender->npc);
            if (seen <= 0 && inc.status == "open")
                continue;                            // (Not yet known by sight: it may still be.)
            factionIncidents_.insert(inc.id);
            if (!player)
                continue;
            const auto* member = factions_.memberOf(inc.victim);
            std::string wronged = member ? member->first : std::string();
            if (wronged.empty())
                if (const auto* cell = world_.cell(inc.cell); cell && !cell->factionClaims.empty())
                    wronged = cell->factionClaims.front();   // Its property: the place it claims.
            if (wronged.empty() || seen <= 0)
                continue;
            const double weight = inc.kind == "assault" ? 6 : inc.kind == "theft" ? 4 : 2;
            factions_.addBurden(wronged, inc.offender, std::round(weight * std::clamp(seen, 0.5, 1.0) + (weight > 4 ? 2 : 0)), inc.id, day);
            chapterViewsDirty_ = true;
        }
        if (inc.status == "charged" && !factionCharged_.count(inc.id))
        {
            factionCharged_.insert(inc.id);
            if (const auto* cell = world_.cell(inc.cell); cell && !cell->factionClaims.empty())
                factions_.addBurden(cell->factionClaims.front(), inc.offender, 2, inc.id + ":conviction", day);
            owing_.insert(inc.offender);
        }
    }
    for (auto it = owing_.begin(); it != owing_.end();)
        if (!world_.warrantFor(*it) && !world_.custodyOf(*it))
        {
            factions_.restitution(*it);
            it = owing_.erase(it);
        }
        else
            ++it;
    if (factionIncidents_.size() > 5000)
        factionIncidents_.clear();
    missionTick(2, day);
}

std::string Game::factionReport(const std::string& askerId, const std::string& officialId, bool paid)
{
    const auto factionId = officialOf(officialId);
    const auto* f = factions_.find(factionId);
    const auto* c = chapters_.of(askerId);
    if (!f || !c)
        return "They have nothing to say to you about that.";
    const double eff = standingOf(factionId, c->id);
    const auto* bond = world_.bonds().find(officialId, askerId);
    if (eff <= -15 || (bond && bond->trust < -20))
        return "\"You know what your wolves did. Ask them.\"";
    if (eff < 15 && !paid)
        return "\"Information isn't free. Five pennies, and I'll tell you what we have on record.\" (Pay for a report.)";
    std::string out = "\"" + f->name + " holds " + c->name + " to be " + lowered(faction::band(eff)) + ".";
    // What weighs on the Chapter: each member (or one sent away whom they haven't heard of) and what was seen.
    auto counted = chapterMembers(c->id);
    for (const auto& m : factions_.stillCounted(factionId, c->id))
        counted.push_back(m);
    int told = 0;
    if (const auto* burdens = factions_.burdensOf(factionId))
        for (const auto& m : counted)
        {
            const auto found = burdens->find(m);
            if (found == burdens->end() || told >= 3)
                continue;
            for (const auto& incId : found->second.incidents)
                for (const auto& inc : world_.crime().incidents)
                {
                    if (inc.id != incId || told >= 3)
                        continue;
                    // As the witnesses knew them: a name if any of them had been given it, else how they looked.
                    std::string who;
                    for (const auto& w : inc.witnesses)
                        if (w.identified && who.empty())
                            who = known_.nameFor(w.id, m);
                    if (who.empty())
                        who = strangerLabel(m) + (inc.witnesses.empty() ? "" : ", wearing your colours");
                    std::string place = inc.cell;
                    if (const auto* cell = world_.cell(inc.cell))
                        place = cell->name;
                    out += " On day " + std::to_string(int(inc.day) + 1) + ", " + inc.kind + " at " + place + ": the one seen was " + who + ".";
                    ++told;
                }
        }
    if (!told)
        out += " Nothing of yours weighs with us.";
    return out + "\"";
}

void Game::missionBoard(const std::string& factionId, const std::string& officialId, double day)
{
    // Up to two open missions of each tier, made from what the faction has and needs (4.5); they lapse at day's end.
    auto& missions = factions_.missions();
    missions.erase(std::remove_if(missions.begin(), missions.end(),
                                  [&](const faction::Mission& m) { return m.state == "done" || m.state == "expired"; }),
                   missions.end());
    const auto* official = world_.entity(officialId);
    if (!official)
        return;
    // Officials of the faction elsewhere, for messages.
    std::vector<std::string> elsewhere;
    for (const auto& [id, e] : world_.entities())
        if (e.npc && id != officialId && e.cellId != official->cellId && officialOf(id) == factionId)
            elsewhere.push_back(id);
    for (int tier = 1; tier <= 2; ++tier)
    {
        int open = 0;
        for (const auto& m : missions)
            open += m.faction == factionId && m.tier == tier && m.state == "open";
        for (int n = open; n < 2; ++n)
        {
            faction::Mission m;
            m.id = factions_.nextMissionId();
            m.faction = factionId;
            m.tier = tier;
            m.official = officialId;
            m.expires = std::floor(day) + 1;
            const auto seed = std::hash<std::string>{}(m.id);
            const int pick = int(seed % 3);
            if (pick == 1 && !elsewhere.empty())
            {
                m.kind = "message";
                m.to = elsewhere[seed / 3 % elsewhere.size()];
                if (const auto* to = world_.entity(m.to); to && world_.cell(to->cellId))
                    m.place = world_.cell(to->cellId)->name;
            }
            else if (pick == 2)
            {
                m.kind = "guard";
                m.cell = official->cellId;
                m.place = world_.cell(official->cellId) ? world_.cell(official->cellId)->name : official->cellId;
                m.seconds = tier == 1 ? 300 : 600;
            }
            else
            {
                m.kind = "deliver";
                m.item = seed / 7 % 2 ? "meal" : "herbs";
                m.quantity = tier == 1 ? 2 + int(seed / 11 % 3) : 5 + int(seed / 11 % 4);
            }
            m.coins = tier == 1 ? 8 : 20;
            m.standing = tier == 1 ? 4 : 7;
            m.renown = tier == 1 ? 5 : 15;
            missions.push_back(m);
        }
    }
}

std::string Game::missionWords(const faction::Mission& m, const std::string& viewer) const
{
    const auto* f = factions_.find(m.faction);
    std::string what;
    if (m.kind == "deliver")
        what = "Bring " + std::to_string(m.quantity) + " " + (m.item == "meal" ? "meals" : "bundles of herbs") + " to " + labelFor(viewer, m.official);
    else if (m.kind == "message")
        what = "Carry a sealed letter to " + labelFor(viewer, m.to) + (m.place.empty() ? "" : " at " + m.place);
    else
        what = "Keep watch at " + m.place + " for " + std::to_string(int(m.seconds / 60)) + " minutes";
    return what + " (" + (f ? f->name : m.faction) + ", tier " + std::to_string(m.tier) + "; " + std::to_string(m.coins) + " pennies)";
}

void Game::completeMission(faction::Mission& m, double day)
{
    m.state = "done";
    const auto* c = chapters_.byId(m.chapter);
    factions_.change(m.faction, m.chapter, m.standing, "mission", day);
    if (c)
        chapters_.addRenown(m.chapter, "mission", m.renown, m.id, m.taker, now());
    const bool paid = m.coins > 0 && world_.society().shift("treasury", m.taker, "", 0, m.coins, "faction mission reward");
    if (paid)
        record(Economy, m.taker);
    if (auto* cl = clientOf(m.taker))
        system(cl, "The mission is done" + std::string(paid ? ": " + std::to_string(m.coins) + " pennies paid" : "") + ". The faction will remember it.");
    if (c)
    {
        tellChapter(c->id, "A mission for " + (factions_.find(m.faction) ? factions_.find(m.faction)->name : m.faction) + " is done. +" +
                               std::to_string(m.renown) + " renown.", m.taker);
        chapterAdvanced(c->id);
    }
    chapterViewsDirty_ = true;
    saveSoon();
}

void Game::missionTick(double dt, double day)
{
    for (auto& m : factions_.missions())
    {
        if ((m.state == "open" || m.state == "taken") && day >= m.expires)
        {
            if (m.state == "taken")
                if (auto* cl = clientOf(m.taker))
                    system(cl, "The day is out: the mission lapses.");
            m.state = "expired";
            continue;
        }
        // Keeping watch: time in the place counts while the one who took it is there.
        if (m.state == "taken" && m.kind == "guard")
            if (const auto* e = world_.entity(m.taker); e && clientOf(m.taker) && e->cellId == m.cell && !e->dead && e->downedLeft <= 0)
            {
                m.held += dt;
                if (m.held >= m.seconds)
                    completeMission(m, day);
            }
    }
}

bool Game::factionCommand(Connection* c, const Value& j, Result& result)
{
    const std::string id = c->entityId, verb = j.string("verb"), target = j.string("target");
    const double day = world_.calendarDays();
    const auto* me = world_.entity(id);
    const auto* official = world_.entity(target);
    const auto* chapter = chapters_.of(id);
    const auto near = [&](const Entity* e) {
        return me && e && e->cellId == me->cellId && std::hypot(e->position.x - me->position.x, e->position.y - me->position.y) <= 3 &&
               world_.visionClarity(id, e->id) > 0;
    };
    const auto factionId = official ? officialOf(target) : std::string();
    if (verb != "take" && verb != "deliver" && (!near(official) || factionId.empty()))
    {
        result = {false, "Find one of the faction's officials and go close to them.", target};
        return true;
    }
    if (!chapter && verb != "deliver")
    {
        result = {false, "Factions deal with Chapters, not with one wolf alone.", target};
        return true;
    }
    if (verb == "report" || verb == "payreport")
    {
        bool paid = false;
        if (verb == "payreport")
        {
            paid = world_.society().shift(id, target, "", 0, 5, "faction report fee");
            if (!paid)
            {
                result = {false, "You haven't five pennies.", target};
                return true;
            }
            record(Economy, id);
        }
        result = {true, factionReport(id, target, paid), target};
    }
    else if (verb == "tellexpulsion")
    {
        factions_.hear(factionId, chapter->id, day);
        chapterViewsDirty_ = true;
        result = {true, "\"So they're no longer yours. We'll mark it.\"", target};
    }
    else if (verb == "tithe")
    {
        const std::int64_t amount = 20;
        if (!world_.society().shift(id, "treasury", "", 0, amount, "faction tithe"))
        {
            result = {false, "A tithe is twenty pennies.", target};
            return true;
        }
        record(Economy, id);
        const double moved = factions_.change(factionId, chapter->id, 1, "tithe", day, 4);
        chapterViewsDirty_ = true;
        result = {true, moved > 0 ? "\"The faction thanks your Chapter.\"" : "\"Generous. We've had enough of your coin this week, though.\"", target};
    }
    else if (verb == "missions")
    {
        // The board: tier 1 for a Lodge or better that the faction knows well; tier 2 for a Company it trusts.
        missionBoard(factionId, target, day);
        const double eff = standingOf(factionId, chapter->id);
        auto e = Value::object();
        e.add("type", "missions");
        e.add("faction", factions_.find(factionId) ? factions_.find(factionId)->name : factionId);
        auto list = Value::array();
        for (const auto& m : factions_.missions())
        {
            if (m.faction != factionId || (m.state != "open" && m.taker != id))
                continue;
            const bool allowed = m.tier == 1 ? chapter->level >= 2 && eff >= 15 : chapter->level >= 3 && eff >= 40;
            auto o = Value::object();
            o.add("id", m.id);
            o.add("text", missionWords(m, id));
            o.add("tier", m.tier);
            o.add("state", m.state);
            o.add("allowed", allowed);
            list.push(o);
        }
        e.add("missions", list);
        send(c, e);
        result = {true, {}, target};
    }
    else if (verb == "take")
    {
        auto* m = factions_.mission(j.string("mission"));
        if (!m || m->state != "open")
            result = {false, "That mission isn't to be had.", {}};
        else
        {
            const double eff = standingOf(m->faction, chapter->id);
            const bool allowed = m->tier == 1 ? chapter->level >= 2 && eff >= 15 : chapter->level >= 3 && eff >= 40;
            if (!allowed)
                result = {false, m->tier == 1 ? "First missions go to a Lodge the faction knows well." : "These go to a Company the faction trusts.", {}};
            else
            {
                m->state = "taken";
                m->taker = id;
                m->chapter = chapter->id;
                result = {true, "You take it on: " + missionWords(*m, id) + ".", {}};
                saveSoon();
            }
        }
    }
    else if (verb == "deliver")
    {
        auto* m = factions_.mission(j.string("mission"));
        const auto* to = m ? world_.entity(m->kind == "message" ? m->to : m->official) : nullptr;
        if (!m || m->state != "taken" || m->taker != id)
            result = {false, "You have no such mission.", {}};
        else if (!near(to))
            result = {false, "Go to " + labelFor(id, to ? to->id : std::string()) + " first.", {}};
        else if (m->kind == "deliver")
        {
            if (!world_.society().shift(id, m->official, m->item, m->quantity, 0, "faction mission goods"))
                result = {false, "You haven't the goods.", {}};
            else
            {
                record(Economy, id);
                completeMission(*m, day);
                result = {true, {}, {}};
            }
        }
        else if (m->kind == "message")
        {
            completeMission(*m, day);
            result = {true, {}, {}};
        }
        else
            result = {false, "Keeping watch is done by staying there.", {}};
    }
    else
        return false;
    return true;
}

bool Game::factionTrade(const std::string& playerId, const std::string& merchantId, bool buy, Result& refusal)
{
    // A faction's merchants and a member's Chapter (4.3): Hostile and worse are turned away.
    const auto* member = factions_.memberOf(merchantId);
    const auto* c = chapters_.of(playerId);
    if (!member || !c || !buy)
        return true;
    const double eff = standingOf(member->first, c->id);
    if (eff <= -40)
    {
        refusal = {false, "\"I don't serve the " + c->name + "'s sort.\"", merchantId};
        return false;
    }
    return true;
}

void Game::afterFactionTrade(const std::string& playerId, const std::string& merchantId, std::int64_t spent)
{
    const auto* member = factions_.memberOf(merchantId);
    const auto* c = chapters_.of(playerId);
    if (!member || !c || spent <= 0)
        return;
    const double eff = standingOf(member->first, c->id);
    const double day = world_.calendarDays();
    if (eff <= -15)
    {
        // Distrusted: a quarter more, purse to purse.
        const auto extra = std::max<std::int64_t>(1, (spent + 3) / 4);
        if (world_.society().shift(playerId, merchantId, "", 0, extra, "faction surcharge"))
            if (auto* cl = clientOf(playerId))
                system(cl, "\"For your Chapter's sort, it's " + std::to_string(extra) + " more.\"");
    }
    // Its quartermasters' terms (4.3): a Chapter it trusts pays 15% less, one sworn to it 25% less (given back, purse
    // to purse, after the sale: the trade itself is untouched).
    if (eff >= 40)
        if (const auto back = spent * (eff >= 75 ? 25 : 15) / 100; back > 0 && world_.society().shift(merchantId, playerId, "", 0, back, "faction discount"))
            if (auto* cl = clientOf(playerId))
                system(cl, "\"For a friend of " + (factions_.find(member->first) ? factions_.find(member->first)->name : member->first) + ", " +
                               std::to_string(back) + " back.\"");
    // Trade with its merchants: +1 a mark... a little at a time, at most 3 a week (4.2).
    factionTradePennies_[member->first + "|" + c->id] += spent;
    auto& pennies = factionTradePennies_[member->first + "|" + c->id];
    while (pennies >= 50)
    {
        pennies -= 50;
        factions_.change(member->first, c->id, 1, "trade", day, 3);
    }
}

void Game::factionScene(const std::string& cellId, const std::string& chapterId)
{
    // A Chapter scene held in a faction's halls (a place it claims): +1, at most 5 a week.
    if (const auto* cell = world_.cell(cellId); cell && !cell->factionClaims.empty())
        factions_.change(cell->factionClaims.front(), chapterId, 1, "scenes in its halls", world_.calendarDays(), 5);
}

std::string Game::factionContext(const std::string& npcId, const std::string& playerId) const
{
    const auto* member = factions_.memberOf(npcId);
    const auto* c = chapters_.of(playerId);
    const auto* f = member ? factions_.find(member->first) : nullptr;
    if (!f || !c)
        return {};
    return " Your faction (" + f->name + ") regards " + c->name + ", whose colours this wolf wears, as " +
           lowered(faction::band(standingOf(f->id, c->id))) + ".";
}

Value Game::standingsView(const std::string& chapterId) const
{
    auto list = Value::array();
    for (const auto& [id, f] : factions_.all())
    {
        const double eff = standingOf(id, chapterId), earned = factions_.earned(id, chapterId);
        auto o = Value::object();
        o.add("id", id);
        o.add("name", f.name);
        o.add("band", faction::band(eff));
        o.add("stance", factions_.stanceOf(id, chapterId, chapterMembers(chapterId)));
        // Never the number, never whose deeds (4.2b): only that something weighs.
        if (std::string(faction::band(eff)) != faction::band(earned))
            o.add("weighs", "Something weighs on your name with " + f.name + ".");
        list.push(o);
    }
    return list;
}
} // namespace ratw::game
