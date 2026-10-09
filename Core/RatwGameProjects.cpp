// Town projects in the game (Docs/Design/57-changing-the-world.md, 4; the rules are RatwProjects.cpp). A town posts a
// project from a real need, or a Dungeon Master does; it stands on the Chapters' structure layer as a town's site, never
// a terrain edit. Wolves give coin (into the project's own account, "project:<id>"), hand in goods at the site, and work
// on it at doc 53's cooperation rate. Its coin does work: each day it posts contracts for the materials still missing,
// which residents and players fill, and hires residents as hands for the hours still missing. When materials and hours
// are met, the materials are used up and the structure stands, with a plaque of its chief givers by the names they
// chose, perhaps named for one of them; the givers' deeds carry the word. Never minted, never lost: a cancelled
// project's goods and coin go back to their givers pro rata.
#include "RatwGame.h"

#include "RatwItems.h"
#include "RatwTogether.h"

#include <algorithm>
#include <cmath>

namespace ratw::game
{
using json::Value;

namespace
{
constexpr double WorkReach = 2.2, SiteReach = 4, HandInReach = 3;
constexpr double GranaryDays = 3;                   // Days more food a head a standing granary keeps (placeholder).
bool standing(const projects::Project& p) { return p.state == "built" || p.state == "worn"; }
double hourOf(double days) { return (days - std::floor(days)) * 24; }
}

double Game::projectStrength(const projects::Project& p)
{
    // Under half its repair, half its effect; at nought, none (doc 57, 4: "Wear").
    return !standing(p) ? 0 : p.condition >= 50 ? 1 : p.condition > 0 ? .5 : 0;
}

double Game::projectHourValue(const std::string& town) const
{
    return std::max(.5, world_.society().dayWage(town, "labour") * projects::rules().hourShare);
}

int Game::projectHas(const projects::Project& p, const std::string& item) const
{
    const auto* purse = world_.society().account(p.purse());
    return purse ? Society::stockAll(*purse, item) : 0;
}

bool Game::projectReady(const projects::Project& p) const
{
    if (p.state != "open" || p.worked + 1e-6 < p.hours)
        return false;
    for (const auto& [item, n] : p.needs)
        if (projectHas(p, item) < n)
            return false;
    return true;
}

bool Game::placeProject(projects::Project& p)
{
    // On open ground by the town's square (or the tile given), never a road, a doorway, water or a stall, by the camps'
    // own checks; a mending stands as nothing, worked at the square.
    const auto* k = projects::rules().kind(p.kind);
    std::vector<Spot> about;
    if (const auto* square = world_.marketSpot(p.town))
        about.push_back(*square);
    const auto& crowd = world_.crowdSpots(p.town);
    about.insert(about.end(), crowd.begin(), crowd.end());
    if (about.empty() && !p.cell.empty())
        about.push_back({p.cell, p.x + .5, p.y + .5});
    if (about.empty())
        return false;
    // Not where the crowd stands, the stalls are set up, or anyone works or serves (a post under a roof or behind a wall).
    std::set<std::tuple<std::string, int, int>> busy;
    const auto keep = [&](const Spot& s) { busy.insert({s.cell, int(std::floor(s.x)), int(std::floor(s.y))}); };
    for (const auto& s : crowd)
        keep(s);
    for (const auto& s : world_.stallSpots(p.town))
        keep(s);
    for (const auto& pos : world_.society().positions())
        keep(pos.work), keep(pos.serve);
    const auto fits = [&](const std::string& cell, int x, int y) {
        const auto* c = world_.cell(cell);
        const auto* tile = c ? c->tile(x, y) : nullptr;
        return tile && tile->glyph != 'u' && tile->glyph != 'T' && !busy.count({cell, x, y}) && whyNotGround(cell, x, y, true).empty() &&
               !camps_.structureAt(cell, x, y);
    };
    if (k && k->structure.empty())
    {
        if (p.cell.empty())
            p.cell = about.front().cell, p.x = int(std::floor(about.front().x)), p.y = int(std::floor(about.front().y));
        return true;
    }
    bool found = !p.cell.empty() && fits(p.cell, p.x, p.y);
    for (std::size_t i = 0; !found && i < about.size(); ++i)
        for (int r = 2; !found && r <= 10; ++r)
            for (int dy = -r; !found && dy <= r; ++dy)
                for (int dx = -r; !found && dx <= r; ++dx)
                {
                    if (std::max(std::abs(dx), std::abs(dy)) != r)
                        continue;
                    const int x = int(std::floor(about[i].x)) + dx, y = int(std::floor(about[i].y)) + dy;
                    if (fits(about[i].cell, x, y))
                        p.cell = about[i].cell, p.x = x, p.y = y, found = true;
                }
    if (!found)
        return false;
    const auto owner = "town:" + p.town;
    const auto* site = camps_.siteAt(p.cell, p.x + .5, p.y + .5, owner);
    std::string siteId = site ? site->id : std::string();
    if (siteId.empty())
    {
        const auto made = camps_.found(owner, p.cell, p.x, p.y, townWords(p.town).substr(0, 40), world_.calendarDays(), now());
        if (!made.ok)
            return false;
        siteId = made.message;
    }
    const auto planned = camps_.plan(siteId, k ? k->structure : std::string(), p.x, p.y);
    if (!planned.ok)
        return false;
    p.site = siteId;
    p.structure = planned.message;
    chapterViewsDirty_ = true;
    return true;
}

Result Game::postProject(const std::string& kind, const std::string& town, const std::string& cell, int x, int y,
                         const std::string& title, const std::string& by)
{
    const auto& rules = projects::rules();
    const auto* k = rules.kind(kind);
    if (!k)
        return {false, "No such kind of project.", {}};
    if (town.empty() || !world_.marketSpot(town))
        return {false, "No such town (or none with a square).", {}};
    const int most = town == world_.society().capital() ? rules.perCity : rules.perTown;
    if (projects_.openIn(town) >= most)
        return {false, "The town has as many projects open as it can manage.", {}};
    for (const auto& [id, p] : projects_.all())
        if (p.town == town && p.kind == kind && p.state == "open")
            return {false, "The town has one of those under way already.", {}};
    auto& p = projects_.post(*k, town, cell, x, y, title.empty() ? projects::titleFor(*k, townWords(town)) : title.substr(0, 80),
                             world_.calendarDays(), by);
    if (!placeProject(p))
    {
        const auto id = p.id;
        projects_.erase(id);
        return {false, "There is no open ground for it there.", {}};
    }
    world_.society().openAccount(p.purse());
    world_.recordEvent({"project posted", by, p.id, p.cell, double(p.x), double(p.y), p.kind, 0, 0, p.title});
    saveSoon();
    return {true, p.id, p.id};
}

const projects::Project* Game::projectAt(const std::string& viewer) const
{
    const auto* me = world_.entity(viewer);
    if (!me)
        return nullptr;
    const projects::Project* best = nullptr;
    double nearest = SiteReach;
    for (const auto& [id, p] : projects_.all())
        if ((p.state == "open" || p.state == "built" || p.state == "worn") && p.cell == me->cellId)
            if (const double d = std::hypot(p.x + .5 - me->position.x, p.y + .5 - me->position.y); d <= nearest)
                nearest = d, best = &p;
    return best;
}

bool Game::projectCommand(Connection* c, const Value& j, Result& result)
{
    const auto id = c ? c->entityId : std::string();
    const auto* me = world_.entity(id);
    auto* p = projects_.find(j.string("project"));
    const auto verb = j.string("verb");
    if (!me || me->npc)
        return false;
    const bool upkeep = p && standing(*p);       // (A standing one takes coin for its upkeep, and work to mend it.)
    if (!p || (p->state != "open" && !(upkeep && (verb == "give" || verb == "work"))))
    {
        result = {false, "There is no such project under way.", {}};
        return true;
    }
    auto& society = world_.society();
    const double today = world_.calendarDays();
    const double apart = me->cellId == p->cell ? std::hypot(p->x + .5 - me->position.x, p->y + .5 - me->position.y) : 1e9;
    // The name a giver chose at its first gift: one of its own names, or none ("a friend of the town").
    const auto chooseName = [&] {
        if (p->shown.count(id))
            return true;
        const auto shown = j.string("shown");
        const auto mine = namesOf(id);
        if (!shown.empty() && std::find(mine.begin(), mine.end(), shown) == mine.end())
            return false;
        p->shown[id] = shown;
        return true;
    };
    if (verb == "give")
    {
        const auto coins = std::int64_t(j.number("coins"));
        const auto* purse = society.account(id);
        if (world_.communityOf(me->cellId) != p->town)
            result = {false, "Give to it in " + townWords(p->town) + ".", {}};
        else if (coins < 1 || coins > 100000)
            result = {false, "How many pennies?", {}};
        else if (!purse || purse->cash < coins)
            result = {false, "You haven't so many pennies.", {}};
        else if (!upkeep && !chooseName())
            result = {false, "Give under one of your own names, or as a friend of the town.", {}};
        else if (!society.shift(id, p->purse(), "", 0, coins, upkeep ? "a gift to a town project's upkeep" : "a gift to a town project"))
            result = {false, "It can't be given just now.", {}};
        else
        {
            if (!upkeep)                            // (Its plaque is fixed once it stands; upkeep is a kindness.)
                projects::Ledger::give(*p, id, "coin", "", double(coins), double(coins), today);
            world_.recordEvent({"project gift", id, p->id, me->cellId, 0, 0, "coin", 0, coins, p->title});
            record(Economy | Character, id);
            projectSpend(*p);                       // (The coin goes to work at once: contracts and hands.)
            result = {true, "You give " + std::to_string(coins) + " pennies to " + p->title + (upkeep ? ", for its upkeep." : "."), {}};
            saveSoon();
        }
        return true;
    }
    if (verb == "handin")
    {
        const auto item = j.string("item");
        const auto base = items::baseOf(items::unmarked(item));
        const int quantity = int(j.number("quantity", 1));
        const auto* purse = society.account(id);
        const int held = purse ? Society::stock(*purse, item) : 0;
        const auto need = p->needs.find(base);
        const int wanted = need == p->needs.end() ? 0 : std::max(0, need->second - projectHas(*p, base));
        if (apart > HandInReach)
            result = {false, "Hand it in at the site.", {}};
        else if (wanted <= 0)
            result = {false, "It doesn't want that.", {}};
        else if (quantity < 1 || held < 1)
            result = {false, "You have none of that.", {}};
        else if (!chooseName())
            result = {false, "Give under one of your own names, or as a friend of the town.", {}};
        else
        {
            const int n = std::min({quantity, held, wanted});
            if (!society.shift(id, p->purse(), item, n, 0, "handed in to a town project"))
                result = {false, "It can't be handed in just now.", {}};
            else
            {
                const double value = n * society.townPrice(p->town, base);
                projects::Ledger::give(*p, id, "goods", base, double(n), value, today);
                world_.recordEvent({"project gift", id, p->id, me->cellId, 0, 0, base, n, 0, p->title});
                record(Economy | Character, id);
                const auto* good = items::good(base);
                result = {true, "You hand in " + std::to_string(n) + " " + (good ? good->name : base) + ".", {}};
                saveSoon();
            }
        }
        return true;
    }
    if (verb == "work")
    {
        const auto* k = projects::rules().kind(p->kind);
        if (projectWork_.count(id))
        {
            projectWork_.erase(id);
            result = {true, "You leave off the work.", {}};
        }
        else if (apart > WorkReach)
            result = {false, "Go to the site first.", {}};
        else if (upkeep && p->condition >= 100)
            result = {false, "It wants no mending.", {}};
        else if (!upkeep && !chooseName())
            result = {false, "Work under one of your own names, or as a friend of the town.", {}};
        else
        {
            // The role least taken among those at it (two angles work faster than one: doc 53).
            std::map<std::string, int> taken;
            for (const auto& [wolf, w] : projectWork_)
                if (w.project == p->id)
                    ++taken[w.role];
            std::string role = k && !k->roles.empty() ? k->roles.front() : "worker";
            if (k)
                for (const auto& r : k->roles)
                    if (taken[r] < taken[role])
                        role = r;
            projectWork_[id] = {p->id, role};
            result = {true, std::string(upkeep ? "You set to mending " : "You set to work on ") + p->title + " as its " + role + ". Stay by it.", {}};
        }
        return true;
    }
    result = {false, "There's nothing like that to do here.", {}};
    return true;
}

void Game::projectSpend(projects::Project& p)
{
    // The project's coin does work: contracts for the materials still missing (not counting those already asked for),
    // and the day's hired hands for the hours still missing, each only as far as it can pay.
    auto& society = world_.society();
    const auto* purse = society.account(p.purse());
    const auto& rules = projects::rules();
    const bool mending = standing(p) && p.condition < rules.upkeepUnder;
    if (!purse || (p.state != "open" && !mending))
        return;
    std::int64_t cash = purse->cash;
    for (const auto& [item, need] : p.needs)
    {
        if (mending)
            break;                                  // (Mending takes hands; its materials are in it.)
        int asked = 0;
        for (const auto& k : world_.roads().contracts)
            if (k.poster == p.purse() && k.kind == "procure" && k.item == item && (k.status == "open" || k.status == "taken"))
                asked += std::max(0, k.quantity - k.delivered);
        const int missing = need - projectHas(p, item) - asked;
        if (missing <= 0)
            continue;
        const auto price = Society::pennies(society.townPrice(p.town, item));
        const auto reward = std::int64_t(std::ceil(double(price) * missing * items::contractPremium()));
        if (cash < reward)
            continue;
        const auto* good = items::good(item);
        auto& k = world_.postContract("procure", p.purse(), p.town, p.purse(), reward, 7,
                                      names::capitalised(p.title) + " wants " + std::to_string(missing) + " " +
                                          (good ? good->name : item));
        k.item = item;
        k.quantity = missing;
        cash -= reward;
    }
    const double missingHours = mending ? (100 - p.condition) / 100 * p.hours : p.hours - p.worked;
    const auto today = std::int64_t(std::floor(world_.calendarDays()));
    auto& posted = projectHands_[p.id];
    if (posted.first != today)
        posted = {today, 0};
    const int slots = std::min(rules.handsMost - posted.second,
                               int(std::ceil(std::max(0.0, missingHours - posted.second * rules.handHours) / std::max(.1, rules.handHours))));
    const auto pay = std::max<std::int64_t>(1, std::int64_t(std::ceil(society.dayWage(p.town, "odd job") - 1e-9)));
    if (slots > 0 && cash >= pay * slots)
    {
        const Spot at{p.cell, p.x + .5, p.y + 1.5};
        if (!society.postOddJob(p.purse(), p.town, "project", slots, pay * slots, at, "work on " + p.title).empty())
            posted.second += slots;
    }
}

void Game::projectsFromEvent(const WorldEvent& e)
{
    if (e.kind == "raid")                           // (A caravan robbed on a town's road: the proposer's watch post.)
    {
        auto& raids = recentRaids_[e.target];
        raids.push_back({e.cell, world_.calendarDays()});
        if (raids.size() > 16)
            raids.erase(raids.begin());
        return;
    }
    // A hired hand's spell done (its pay, from the project's purse): so many work-hours more.
    if (e.kind != "economy" || e.detail != "an odd job: project" || e.actor.rfind("project:", 0) != 0)
        return;
    if (auto* p = projects_.find(e.actor.substr(8)); p && p->state == "open")
        p->worked = std::min(p->hours, p->worked + projects::rules().handHours);
    else if (p && standing(*p))
    {
        p->condition = std::min(100.0, p->condition + projects::rules().handHours / p->hours * 100);
        p->state = p->condition < 50 ? "worn" : "built";
        if (auto* st = camps_.structure(p->structure))
            st->condition = p->condition;
    }
}

void Game::projectTick(double dt)
{
    projectAccumulator_ += dt;
    if (projectAccumulator_ < .5)
        return;
    const double step = projectAccumulator_;
    projectAccumulator_ = 0;
    const double today = world_.calendarDays();
    // Players at work, by project: together, at the cooperation rate; each its share of the hours, on the plaque.
    std::map<std::string, std::vector<std::string>> at;
    for (auto it = projectWork_.begin(); it != projectWork_.end();)
    {
        const auto* e = world_.entity(it->first);
        const auto* p = projects_.find(it->second.project);
        if (!e || !clientOf(it->first) || !p || !(p->state == "open" || (standing(*p) && p->condition < 100)) || e->cellId != p->cell ||
            std::hypot(p->x + .5 - e->position.x, p->y + .5 - e->position.y) > WorkReach || world_.inBattle(it->first))
        {
            if (auto* c = clientOf(it->first))
                system(c, "You leave off the work.");
            it = projectWork_.erase(it);
            continue;
        }
        at[it->second.project].push_back(it->first);
        ++it;
    }
    for (const auto& [id, wolves] : at)
    {
        auto* p = projects_.find(id);
        std::vector<together::Hand> hands;
        for (const auto& w : wolves)
            hands.push_back({projectWork_[w].role, false});
        const double hours = step / camp::SecondsPerWorkHour * together::rate(hands, 6);
        if (standing(*p))
        {
            // Mending: a work-hour restores what the kind's hours would build, spread over its repair.
            p->condition = std::min(100.0, p->condition + hours / p->hours * 100);
            p->state = p->condition < 50 ? "worn" : "built";
            if (auto* st = camps_.structure(p->structure))
                st->condition = p->condition;
            continue;
        }
        const double done = std::min(hours, p->hours - p->worked);
        p->worked += done;
        const double each = done / double(wolves.size()), value = each * projectHourValue(p->town);
        for (const auto& w : wolves)
            projects::Ledger::give(*p, w, "labour", "", each, value, today);
    }
    // The structure shows its progress.
    for (auto& [id, p] : projects_.every())
        if (p.state == "open" && !p.structure.empty())
            if (auto* st = camps_.structure(p.structure); st && !st->built)
                if (const auto* k = camp::kind(st->kind))
                    st->work = std::min(k->hours - 1e-3, k->hours * p.worked / p.hours);
    // Daily, at the turn of the day: wear, lapses, the coin's work; and the day's proposal at dawn.
    if (std::floor(today) != projectDay_)
    {
        if (projectDay_ >= 0)
            wearProjects(std::floor(today) - projectDay_);
        projectDay_ = std::floor(today);
        std::vector<std::string> lapsed;
        for (auto& [id, p] : projects_.every())
            if (p.state == "open")
            {
                if (today - p.posted >= projects::rules().lapseDays)
                    lapsed.push_back(id);
                else
                    projectSpend(p);
            }
        for (const auto& id : lapsed)
            cancelProject(id, "unfinished after " + std::to_string(int(projects::rules().lapseDays)) + " days");
        for (auto& [id, p] : projects_.every())
            if (standing(p))
                projectSpend(p);                    // (Its upkeep: hands hired to mend it when it is worn.)
        refreshWorks();
    }
    if (std::floor(today) != proposedDay_ && hourOf(today) >= 6)
    {
        proposedDay_ = std::floor(today);
        proposeProjects();
    }
    std::vector<std::string> ready;
    for (const auto& [id, p] : projects_.all())
        if (projectReady(p))
            ready.push_back(id);
    for (const auto& id : ready)
        finishProject(*projects_.find(id));
    if (!projects_.all().empty())
        refreshWorks();                             // (A project mended past half, or worn under it: its effect follows.)
}

void Game::finishProject(projects::Project& p)
{
    // Its materials used up (goods only: the coin left stays as its upkeep), the structure standing, the plaque and
    // perhaps a name, and the givers' deeds. A mending stands as nothing: its materials go to the Town Works' stock,
    // used by its own rule, and its hours mend the town now.
    auto& society = world_.society();
    const auto* kindRule = projects::rules().kind(p.kind);
    const bool mendingKind = kindRule && kindRule->structure.empty();
    if (mendingKind)
    {
        const auto works = "town:" + p.town + ":works";
        society.openAccount(works);
        if (const auto* purse = society.account(p.purse()))
            for (const auto& [kind, n] : std::map<std::string, int>(purse->stock.begin(), purse->stock.end()))
                if (p.needs.count(items::baseOf(items::unmarked(kind))))
                    society.shift(p.purse(), works, kind, n, 0, "for the Town Works, from " + p.title.substr(0, 50));
        society.mendTown(p.town, p.hours);
    }
    for (const auto& [item, need] : p.needs)
    {
        if (mendingKind)
            break;
        int left = need;
        if (const auto* purse = society.account(p.purse()))
            for (const auto& kind : Society::kindsHeld(*purse, item))
                if (left > 0)
                    left -= society.consume(p.purse(), kind, std::min(left, Society::stock(*society.account(p.purse()), kind)),
                                            "used in " + p.title.substr(0, 60));
    }
    p.state = mendingKind ? "done" : "built";
    p.finished = world_.calendarDays();
    p.condition = 100;
    p.worked = p.hours;
    if (auto* st = camps_.structure(p.structure))
    {
        st->built = true;
        st->condition = 100;
        if (const auto* k = camp::kind(st->kind))
            st->work = k->hours;
    }
    for (auto it = projectWork_.begin(); it != projectWork_.end();)
        it = it->second.project == p.id ? projectWork_.erase(it) : std::next(it);
    const auto& rules = projects::rules();
    const auto* k = rules.kind(p.kind);
    if (const auto chief = projects::Ledger::chief(p); !chief.empty() && k && !k->structure.empty())
    {
        p.namedFor = chief;
        p.title = p.shown[chief] + "'s " + k->name;
    }
    // Deeds (doc 56): the chief giver's notable (great for a great work), others who gave a tenth or more, small.
    const double whole = projects::Ledger::worth(p);
    const auto givers = projects::Ledger::givers(p);
    for (std::size_t i = 0; i < givers.size(); ++i)
    {
        if (!playerAccountId(givers[i].who) || (i > 0 && givers[i].value < rules.otherShare * whole))
            continue;
        const int weight = i == 0 ? (whole >= double(rules.greatWorth) ? fame::Great : fame::Notable) : fame::Small;
        recordDeed("built_project", {givers[i].who}, "town:" + p.town, p.cell, "project", p.title, weight);
    }
    world_.recordEvent({"project built", "town:" + p.town, p.id, p.cell, double(p.x), double(p.y), p.kind, 0, std::int64_t(whole), p.title});
    for (auto* c : clients_)
        if (const auto* e = world_.entity(c->entityId); e && world_.communityOf(e->cellId) == p.town)
            system(c, names::capitalised(p.title) + " stands finished in " + townWords(p.town) + ".");
    chapterViewsDirty_ = true;
    groundKey_.clear();
    refreshGround();
    refreshWorks();
    saveSoon();
}

void Game::refundProject(projects::Project& p)
{
    // Every open contract withdrawn (its reward back in the purse); the goods back to their givers in the shares they
    // gave (what its coin bought, with no giver, to the Town Works); its coin to the coin givers pro rata (none: to the
    // town's treasury). All moves.
    auto& society = world_.society();
    for (const auto& k : std::vector<Contract>(world_.roads().contracts))
        if (k.poster == p.purse())
            world_.withdrawContract(k.id);
    if (const auto* purse = society.account(p.purse()))
        for (const auto& [kind, n] : std::map<std::string, int>(purse->stock.begin(), purse->stock.end()))
        {
            int left = n;
            for (const auto& [who, share] : projects::Ledger::shares(p, "goods", items::baseOf(items::unmarked(kind)), n))
                if (share > 0 && left > 0 && society.shift(p.purse(), who, kind, int(std::min<std::int64_t>(share, left)), 0, "returned from a town project"))
                    left -= int(std::min<std::int64_t>(share, left));
            if (left > 0)
                society.shift(p.purse(), "town:" + p.town + ":works", kind, left, 0, "returned from a town project");
        }
    if (const auto* purse = society.account(p.purse()); purse && purse->cash > 0)
    {
        auto left = purse->cash;
        for (const auto& [who, share] : projects::Ledger::shares(p, "coin", "", left))
            if (share > 0 && society.shift(p.purse(), who, "", 0, share, "returned from a town project"))
                left -= share;
        if (left > 0)
            society.shift(p.purse(), society.treasuryOf(p.town), "", 0, left, "returned from a town project");
    }
    record(Economy);
}

Result Game::cancelProject(const std::string& id, const std::string& why)
{
    auto* p = projects_.find(id);
    if (!p || (p->state != "open" && p->state != "built" && p->state != "worn"))
        return {false, "No such project.", {}};
    const bool built = p->state != "open";
    refundProject(*p);
    if (!built && !p->structure.empty())
        camps_.unplan(p->structure);
    for (auto it = projectWork_.begin(); it != projectWork_.end();)
        it = it->second.project == id ? projectWork_.erase(it) : std::next(it);
    if (!built)
        p->state = "cancelled";
    world_.recordEvent({"project cancelled", "town:" + p->town, p->id, p->cell, 0, 0, p->kind, 0, 0, why});
    chapterViewsDirty_ = true;
    saveSoon();
    return {true, p->title + (built ? ": its coin returned." : " is cancelled; what was given goes back."), id};
}

Result Game::completeProject(const std::string& id)
{
    // The Dungeon Master's hand: finished now, with whatever materials it holds.
    auto* p = projects_.find(id);
    if (!p || p->state != "open")
        return {false, "No such project under way.", {}};
    for (auto& [item, n] : p->needs)
        n = std::min(n, projectHas(*p, item));
    p->worked = p->hours;
    finishProject(*p);
    return {true, p->title + " stands finished.", id};
}

Result Game::removeProject(const std::string& id)
{
    // The structure taken out of the world and its effect ended; any coin left goes back to its givers.
    auto* p = projects_.find(id);
    if (!p || p->state == "removed")
        return {false, "No such project.", {}};
    refundProject(*p);
    if (!p->structure.empty())
        camps_.unplan(p->structure);
    for (auto it = projectWork_.begin(); it != projectWork_.end();)
        it = it->second.project == id ? projectWork_.erase(it) : std::next(it);
    p->state = "removed";
    world_.recordEvent({"project removed", "town:" + p->town, p->id, p->cell, 0, 0, p->kind, 0, 0, p->title});
    chapterViewsDirty_ = true;
    groundKey_.clear();
    refreshGround();
    refreshWorks();
    saveSoon();
    return {true, p->title + " is taken down.", id};
}

Value Game::projectView(const std::string& viewer, const projects::Project& p, bool here)
{
    const auto& society = world_.society();
    const auto* k = projects::rules().kind(p.kind);
    auto o = Value::object();
    o.add("id", p.id);
    o.add("kind", p.kind);
    o.add("name", k ? k->name : p.kind);
    o.add("title", names::capitalised(p.title));
    o.add("town", townWords(p.town));
    o.add("place", world_.cell(p.cell) ? world_.cell(p.cell)->name : p.cell);
    o.add("cell", p.cell);
    o.add("x", p.x);
    o.add("y", p.y);
    o.add("state", p.state);
    o.add("hours", std::round(p.hours * 10) / 10);
    o.add("worked", std::round(p.worked * 10) / 10);
    o.add("condition", std::round(p.condition));
    auto needs = Value::array();
    for (const auto& [item, n] : p.needs)
    {
        const auto* good = items::good(item);
        auto m = Value::object();
        m.add("item", item);
        m.add("name", good ? good->name : item);
        m.add("need", n);
        m.add("have", std::min(n, projectHas(p, item)));
        needs.push(m);
    }
    o.add("needs", needs);
    const auto* purse = society.account(p.purse());
    o.add("coin", double(purse ? purse->cash : 0));
    auto plaque = Value::array();
    for (const auto& [who, name] : projects::Ledger::plaque(p))
        plaque.push(name);
    o.add("plaque", plaque);
    double mine = 0;
    for (const auto& g : p.gifts)
        if (g.who == viewer)
            mine += g.value;
    o.add("given", std::round(mine));
    if (const auto shown = p.shown.find(viewer); shown != p.shown.end())
        o.add("shownAs", shown->second.empty() ? std::string("a friend of the town") : shown->second);
    else
    {
        auto names = Value::array();
        for (const auto& n : namesOf(viewer))
            names.push(n);
        o.add("names", names);                      // (The first gift chooses how it is shown.)
    }
    o.add("here", here);
    if (const auto w = projectWork_.find(viewer); w != projectWork_.end() && w->second.project == p.id)
        o.add("working", w->second.role);
    if (here && p.state == "open")
    {
        // What it could take from this wolf's pack.
        auto handable = Value::array();
        if (const auto* pack = society.account(viewer))
            for (const auto& [item, n] : pack->stock)
                if (const auto need = p.needs.find(items::baseOf(items::unmarked(item))); n > 0 && need != p.needs.end() &&
                                                                                         projectHas(p, need->first) < need->second)
                {
                    auto h = Value::object();
                    h.add("item", item);
                    h.add("count", n);
                    handable.push(h);
                }
        o.add("handable", handable);
    }
    return o;
}

Value Game::projectsView(const std::string& viewer, const std::string& town)
{
    auto list = Value::array();
    for (const auto& [id, p] : projects_.all())
        if (p.town == town && (p.state == "open" || p.state == "built" || p.state == "worn"))
            list.push(projectView(viewer, p, false));
    return list;
}

void Game::projectsSave(Value& root) const
{
    root.add("projects", projects_.save());
}

void Game::projectsLoad(const Value& saved)
{
    projects_.load(saved.find("projects") ? saved["projects"] : Value::array());
}

// ------------------------------------------------------------------ Phase 4: projects that change the world

void Game::refreshWorks()
{
    // What the standing projects do, handed to the world (and the economy) when it changes.
    World::StandingWorks works;
    std::map<std::string, double> granary;
    for (const auto& [id, p] : projects_.all())
    {
        const double s = projectStrength(p);
        if (s <= 0)
            continue;
        if (p.kind == "watchpost")
            works.watchposts[p.cell] = std::max(works.watchposts[p.cell], s);
        else if (p.kind == "cover")
            works.covers[p.town] = std::max(works.covers[p.town], s);
        else if (p.kind == "waystation")
            works.waystations[p.cell] = std::max(works.waystations[p.cell], s);
        else if (p.kind == "granary")
            granary[p.town] = std::max(granary[p.town], GranaryDays * s);
    }
    if (!(works == world_.standingWorks()))
        world_.setStandingWorks(std::move(works));
    world_.society().setGranaryExtra(std::move(granary));
    if (projects_.all().size() != projectTowns_)
    {
        std::map<std::string, std::string> towns;   // (Players' gifts to a project count as help to its town: doc 57, 5.)
        for (const auto& [id, p] : projects_.all())
            towns[p.purse()] = p.town;
        projectTowns_ = towns.size();
        world_.society().setProjectTowns(std::move(towns));
    }
}

void Game::wearProjects(double days)
{
    // Like a Chapter's buildings (doc 32, 5.6), by the kind's own rate; a town's never lies abandoned. Worn under half,
    // half its effect; at nought a ruin, standing as scenery, doing nothing.
    if (days <= 0)
        return;
    for (auto& [id, p] : projects_.every())
    {
        if (!standing(p))
            continue;
        const auto* k = projects::rules().kind(p.kind);
        p.condition = std::max(0.0, p.condition - (k ? k->wear : 1) * days);
        const auto was = p.state;
        p.state = p.condition <= 0 ? "ruin" : p.condition < 50 ? "worn" : "built";
        if (auto* st = camps_.structure(p.structure))
            st->condition = p.condition;
        if (p.state == "ruin" && was != "ruin")
        {
            world_.recordEvent({"project ruined", "town:" + p.town, p.id, p.cell, 0, 0, p.kind, 0, 0, p.title});
            for (auto it = projectWork_.begin(); it != projectWork_.end();)
                it = it->second.project == id ? projectWork_.erase(it) : std::next(it);
            groundKey_.clear();
        }
    }
    chapterViewsDirty_ = true;
    refreshGround();
}

void Game::proposeProjects()
{
    // One town a day, in turn, at dawn: a project proposed where a real need holds and none of that kind is open. It
    // only reads.
    const auto& towns = world_.towns();
    if (towns.empty())
        return;
    const auto& town = towns[proposeNext_++ % towns.size()].id;
    const auto& society = world_.society();
    const double today = world_.calendarDays();
    const auto post = [&](const std::string& kind, const std::string& cell, const std::string& why) {
        for (const auto& [id, p] : projects_.all())
            if (p.town == town && p.kind == kind && (p.state == "open" || ((p.state == "built" || p.state == "worn") && kind != "mend")))
                return false;                       // (One of the kind under way, or standing: a worn one is mended, not rebuilt.)
        const auto made = postProject(kind, town, cell, cell.empty() ? -1 : 8, cell.empty() ? -1 : 8, "", "town");
        if (made.ok)
            world_.recordEvent({"project proposed", "town:" + town, made.targetId, cell, 0, 0, kind, 0, 0, why});
        return made.ok;
    };
    // The town in disrepair: its mending.
    if (society.condition(town) < 50 && post("mend", "", "the town is in disrepair"))
        return;
    // A caravan robbed on one of its roads lately: a watch post where it happened most.
    {
        std::map<std::string, int> robbed;
        if (const auto raids = recentRaids_.find(town); raids != recentRaids_.end())
            for (const auto& [cell, day] : raids->second)
                if (today - day < 14)
                    ++robbed[cell];
        std::string worst;
        for (const auto& [cell, n] : robbed)
            if (worst.empty() || n > robbed[worst])
                worst = cell;
        if (!worst.empty() && post("watchpost", worst, "caravans robbed on its road"))
            return;
    }
    // A market kept in by foul weather on two market days this season: a cover.
    if (world_.foulMarketDays(town) >= 2 && post("cover", "", "its stalls kept in by the weather"))
        return;
    // Caravans for it waiting out storms on three days this season: a waystation where they waited most.
    {
        std::string most;
        int days = 0;
        for (const auto& [cell, n] : world_.stormWaits(town))
            if (n > days)
                most = cell, days = n;
        if (days >= 3 && post("waystation", most, "caravans held up by storms on its road"))
            return;
    }
    // Empty shelves at two decisions running, or its granary empty at the turn of autumn: a granary.
    const auto& decision = society.orchestrator().decision;
    if (decision.decided && double(decision.day) != distressDay_)
    {
        distressDay_ = double(decision.day);
        for (const auto& t : decision.towns)
        {
            auto& run = distressRun_[t.id];
            run.push_back(t.kind);
            if (run.size() > 2)
                run.erase(run.begin());
        }
    }
    const auto run = distressRun_.find(town);
    const bool shelves = run != distressRun_.end() && run->second.size() == 2 && run->second[0] == "empty shelves" && run->second[1] == "empty shelves";
    const auto* granary = society.account("town:" + town + ":granary");
    const auto date = calendar::calendarAt(today);
    const bool emptyAutumn = int(date.season) == 2 && date.dayOfSeason <= 7 && (!granary || granary->stock.empty());
    if (shelves || emptyAutumn)
        post("granary", "", shelves ? "empty shelves" : "an empty granary at the turn of autumn");
}
} // namespace ratw::game
