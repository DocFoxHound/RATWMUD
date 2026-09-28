// The roads between towns (RatwRoads.h; Docs/Design/26-living-npcs.md, Phase 5). World members, kept here.
#include "RatwRoads.h"
#include "RatwWorld.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace ratw
{
namespace
{
// Placeholder numbers, to be tuned with play.
constexpr double LegSeconds = 150;             // A loaded caravan takes two and a half minutes to cross a cell.
constexpr std::size_t BeliefsKept = 30;        // What one character keeps hearing about, strongest first.
constexpr int ContractDays = 10;

std::uint64_t roll(const std::string& a, std::int64_t b)
{
    return std::hash<std::string>{}(a) * 1099511628211ULL + std::uint64_t(b) * 2654435761ULL;
}
double chance(const std::string& a, std::int64_t b)
{
    return double(roll(a, b) % 10000) / 10000.0;
}
const char* sureness(double c)
{
    return c >= .8 ? "sure of it" : c >= .5 ? "fairly sure" : "not sure";
}
} // namespace

const Town* World::townOf(const std::string& cellId) const
{
    const auto found = townOfCell_.find(cellId);
    if (found == townOfCell_.end())
        return nullptr;
    for (const auto& t : towns_)
        if (t.id == found->second)
            return &t;
    return nullptr;
}

std::vector<std::string> World::routeBetween(const std::string& from, const std::string& to) const
{
    std::vector<std::string> route{from};
    for (std::string at = from; at != to && route.size() < 400;)
    {
        const auto& steps = cachedSteps(at);
        const auto next = steps.find(to);
        if (next == steps.end() || next->second.empty())
            return {};
        at = next->second;
        route.push_back(at);
    }
    return route.back() == to ? route : std::vector<std::string>{};
}

void World::setupTowns()
{
    townsReady_ = true;
    towns_.clear();
    townOfCell_.clear();
    // A town is a region people live in with a market: residents counted by home, markets by merchants' work.
    std::map<std::string, int> living, guards;
    std::map<std::string, std::map<std::string, int>> markets;
    const auto regionOf = [&](const std::string& cellId) -> std::string {
        const auto* c = cell(cellId);
        return c && !c->region.empty() && c->region != "unassigned" ? c->region : std::string();
    };
    for (const auto& [id, life] : society_.state().residents)
        if (const auto region = regionOf(life.homeCell); !region.empty())
            ++living[region];
    for (const auto& p : society_.positions())
    {
        const auto region = regionOf(p.work.cell);
        if (region.empty())
            continue;
        if (p.role == "merchant")
            ++markets[region][p.work.cell];
        if (p.role == "guard")
            ++guards[region];
    }
    for (const auto& [region, count] : living)
    {
        if (count < 5 || !markets.count(region))
            continue;
        Town t;
        t.id = region;
        t.residents = count;
        t.guards = guards[region];
        int best = -1;
        for (const auto& [cellId, n] : markets[region])
            if (n > best)
                best = n, t.market = cellId;
        towns_.push_back(t);
    }
    if (towns_.size() < 2)
    {
        towns_.clear();                            // One settlement: its store is the treasury, as ever.
        return;
    }
    // The capital is where people arrive (the spawn), or failing that the largest town; its store is the treasury.
    const auto spawnRegion = regionOf(spawnCell_);
    auto capital = std::find_if(towns_.begin(), towns_.end(), [&](const Town& t) { return t.id == spawnRegion; });
    if (capital == towns_.end())
        capital = std::max_element(towns_.begin(), towns_.end(), [](const Town& a, const Town& b) { return a.residents < b.residents; });
    std::rotate(towns_.begin(), capital, capital + 1);    // The capital first.
    std::map<std::string, std::string> stores;
    int total = 0;
    for (auto& t : towns_)
    {
        t.store = &t == &towns_.front() ? "treasury" : "stores:" + t.id;
        total += t.residents;
        if (t.store != "treasury")
            society_.openAccount(t.store);
    }
    for (const auto& [cellId, c] : cells_)
        for (const auto& t : towns_)
            if (c.region == t.id)
            {
                townOfCell_[cellId] = t.id;
                stores[cellId] = t.store;
            }
    society_.setStores(stores);
    if (!roads_.stocked)
    {
        // Once: each town's share of the treasury's goods, by how many live there.
        const auto* treasury = society_.account("treasury");
        const int meals = treasury ? Society::stock(*treasury, "meal") : 0, herbs = treasury ? Society::stock(*treasury, "herbs") : 0;
        for (std::size_t i = 1; i < towns_.size(); ++i)
        {
            const double share = double(towns_[i].residents) / std::max(1, total);
            society_.shift("treasury", towns_[i].store, "meal", int(meals * share), 0, "stores stocked");
            society_.shift("treasury", towns_[i].store, "herbs", int(herbs * share), 0, "stores stocked");
        }
        roads_.stocked = true;
    }
    // Bandits camp in wild country along the roads (one in six such cells, to begin with).
    if (roads_.camps.empty())
        for (std::size_t i = 1; i < towns_.size(); ++i)
            for (const auto& cellId : routeBetween(towns_.front().market, towns_[i].market))
                if (!townOfCell_.count(cellId) && roll(cellId, 0) % 6 == 0 &&
                    std::none_of(roads_.camps.begin(), roads_.camps.end(), [&](const BanditCamp& b) { return b.cell == cellId; }))
                    roads_.camps.push_back({"camp_" + cellId, cellId, 3.0 + double(roll(cellId, 1) % 4), 30, -100, true});
    absorbJournal();
}

Contract& World::postContract(const std::string& kind, const std::string& poster, const std::string& town,
                              const std::string& target, std::int64_t reward, double days, const std::string& detail)
{
    Contract c;
    c.id = "k" + std::to_string(roads_.nextId++);
    c.kind = kind;
    c.poster = poster;
    c.town = town;
    c.target = target;
    c.created = calendarDays_;
    c.due = calendarDays_ + days;
    c.detail = detail;
    // The reward is set aside at once, from the poster's own purse (or the treasury): it exists, or there's no reward.
    const auto escrow = "contract:" + c.id;
    if (reward > 0 && society_.openAccount(escrow) && society_.shift(poster, escrow, "", 0, reward, "reward set aside"))
        c.reward = reward;
    else
        society_.closeAccount(escrow);
    roads_.contracts.push_back(c);
    recordEvent({"contract posted", poster, target, {}, 0, 0, {}, 0, c.reward, kind + ": " + detail});
    return roads_.contracts.back();
}

void World::settleContract(Contract& c, const std::string& status, const std::string& paidTo)
{
    const auto escrow = "contract:" + c.id;
    if (c.reward > 0)
        society_.shift(escrow, paidTo.empty() ? c.poster : paidTo, "", 0, c.reward,
                       paidTo.empty() ? "reward returned" : "contract reward");
    society_.closeAccount(escrow);
    c.status = status;
    recordEvent({"contract " + status, paidTo.empty() ? c.poster : paidTo, c.target, {}, 0, 0, {}, 0, c.reward, c.kind + ": " + c.detail});
}

std::vector<const Contract*> World::contractsNear(const std::string& player) const
{
    std::vector<const Contract*> out;
    const auto* p = entity(player);
    const auto* here = p ? townOf(p->cellId) : nullptr;
    if (!here)
        return out;
    for (const auto& c : roads_.contracts)
        if (c.status == "open" && (c.town == here->id || (c.kind == "courier" && townOf(entity(c.poster) ? entity(c.poster)->cellId : "") == here)))
            out.push_back(&c);
    return out;
}

Result World::takeContract(const std::string& player, const std::string& contractId)
{
    const auto* p = entity(player);
    if (!p || p->npc)
        return {false, "No such character.", {}};
    for (const auto* near : contractsNear(player))
        if (near->id == contractId)
            for (auto& c : roads_.contracts)
                if (c.id == contractId)
                {
                    if (c.poster == player)
                        return {false, "That is your own contract.", c.id};
                    c.status = "taken";
                    c.taker = player;
                    recordEvent({"contract taken", player, c.poster, {}, 0, 0, {}, 0, c.reward, c.kind + ": " + c.detail});
                    return {true, "You take on: " + c.detail + (c.reward ? " (" + std::to_string(c.reward) + " pennies)" : "") + ".", c.id};
                }
    return {false, "There is no such work to be had here.", contractId};
}

Result World::completeContract(const std::string& contractId, const std::string& by)
{
    for (auto& c : roads_.contracts)
        if (c.id == contractId && (c.status == "open" || c.status == "taken"))
        {
            if (c.kind == "bounty")
                for (auto& camp : roads_.camps)
                    if (camp.id == c.target)
                        camp.active = false;
            settleContract(c, "done", by);
            return {true, "Done: " + c.detail + ".", c.id};
        }
    return {false, "No such open contract.", contractId};
}

void World::believe(const std::string& holder, const std::string& subject, const std::string& claim,
                    const std::string& source, double confidence)
{
    if (holder.empty() || subject.empty() || holder == subject || claim.empty() || !(confidence > .05))
        return;
    auto& mine = beliefs_[holder];
    for (auto& b : mine)
        if (b.subject == subject && b.claim == claim)
        {
            if (confidence > b.confidence)
                b = {holder, subject, claim, source, std::min(1.0, confidence), calendarDays_};
            return;
        }
    mine.push_back({holder, subject, claim, source, std::min(1.0, confidence), calendarDays_});
    if (mine.size() > BeliefsKept)
        mine.erase(std::min_element(mine.begin(), mine.end(), [](const Belief& a, const Belief& b) { return a.confidence < b.confidence; }));
}

const std::vector<Belief>* World::beliefsOf(const std::string& holder) const
{
    const auto found = beliefs_.find(holder);
    return found == beliefs_.end() ? nullptr : &found->second;
}

std::string World::rumoursAbout(const std::string& npc, const std::string& subject, const std::string& subjectName) const
{
    std::string out;
    if (const auto* mine = beliefsOf(npc))
        for (const auto& b : *mine)
            if (b.subject == subject)
            {
                const auto* from = entity(b.source);
                out += (out.empty() ? "" : " ") + std::string("You have heard that ") + subjectName + " " + b.claim + " (" +
                       (from ? "from " + from->name : b.source) + "; " + sureness(b.confidence) + ").";
            }
    return out;
}

void World::rumoursFromEvent(const WorldEvent& e)
{
    // Witnesses and those it happened to know it first; everyone else hears it later, or never.
    const auto witnesses = [&](const std::string& cellId, const std::string& subject, const std::string& claim, double confidence) {
        for (const auto& [id, other] : entities_)
            if (other.npc && !other.dead && other.cellId == cellId && id != subject)
                believe(id, subject, claim, "saw it", confidence);
    };
    if (e.kind == "death")
        witnesses(e.cell, e.actor, "died", 1);
    else if (e.kind == "mourning")
        believe(e.actor, e.target, "died", e.detail == "family" ? "the family" : "a friend", .95);
    else if (e.kind == "harm")
    {
        believe(e.target, e.actor, "is violent", "suffered it", 1);
        witnesses(e.cell, e.actor, "is violent", .8);
    }
    else if (e.kind == "promise broken")
        believe(e.target, e.actor, "breaks promises", "was let down", .9);
    else if (e.kind == "promise kept")
        believe(e.target, e.actor, "keeps promises", "saw it kept", .8);
    else if (e.kind == "marriage")
        witnesses(e.cell, e.actor, "married", .7);
    else if (e.kind == "newcomer" && !e.actor.empty())
        if (const auto* who = entity(e.actor))
            witnesses(who->cellId, e.actor, "is new in town", .8);
}

void World::contractsFromEvent(const WorldEvent& e)
{
    // A supply run is done by selling the goods to a merchant of that town while the contract is yours.
    if (e.kind != "economy" || e.detail != "local trade" || e.item.empty())
        return;
    for (auto& c : roads_.contracts)
    {
        if (c.kind != "supply" || c.status != "taken" || c.taker != e.target)
            continue;
        const auto* merchant = entity(e.actor);
        const auto* job = society_.jobOf(e.actor);
        const auto* town = job ? townOf(job->work.cell) : merchant ? townOf(merchant->cellId) : nullptr;
        if (town && town->id == c.town)
        {
            settleContract(c, "done", c.taker);
            bonds_.change(e.actor, c.taker, {2, 3, 1, 0, 1}, calendarDays_);
        }
    }
}

void World::tendRoads()
{
    if (!townsReady_)
        setupTowns();
    if (towns_.size() < 2)
        return;
    advanceCaravans();
    // A courier is done when the one who took it stands before the recipient.
    for (auto& c : roads_.contracts)
    {
        if (c.kind != "courier" || c.status != "taken" || c.taker.rfind("caravan:", 0) == 0)
            continue;
        const auto* carrier = entity(c.taker);
        const auto* to = entity(c.target);
        if (carrier && to && !to->dead && carrier->cellId == to->cellId &&
            std::hypot(carrier->position.x - to->position.x, carrier->position.y - to->position.y) <= 3)
        {
            settleContract(c, "done", c.taker);
            bonds_.change(c.target, c.taker, {2, 3, 2, 0, 0}, calendarDays_);
            bonds_.change(c.poster, c.target, {1, 0, 2, 0, 0}, calendarDays_);
            bonds_.change(c.poster, c.taker, {1, 3, 1, 0, 0}, calendarDays_);
        }
    }
    const auto today = std::int64_t(std::floor(calendarDays_));
    if (today != roads_.day)
    {
        roads_.day = today;
        roadsDaily();
    }
    absorbJournal();
}

void World::advanceCaravans()
{
    const auto& capital = towns_.front();
    for (auto& c : roads_.caravans)
    {
        if (c.status == "arrived" || c.route.empty() || time_ < c.nextAt)
            continue;
        c.nextAt = time_ + LegSeconds;
        ++c.leg;
        const bool homeward = c.status == "raided" || c.status == "returning";
        if (c.leg + 1 >= c.route.size())
        {
            if (homeward)
            {
                c.status = "arrived";                  // Back at the capital: done.
                society_.closeAccount(c.account);
                continue;
            }
            // Arrived: the load into the town's stores, escorts paid, letters handed over, news told.
            const auto found = std::find_if(towns_.begin(), towns_.end(), [&](const Town& t) { return t.id == c.to; });
            if (found != towns_.end())
            {
                const Town* town = &*found;
                for (const std::string item : {"meal", "herbs"})
                    if (const auto* load = society_.account(c.account); load && Society::stock(*load, item) > 0)
                        society_.shift(c.account, town->store, item, Society::stock(*load, item), 0, "caravan delivered");
                for (const auto& player : c.escorts)
                    for (auto& k : roads_.contracts)
                        if (k.kind == "escort" && k.taker == player && k.status == "taken" && k.town == c.to)
                            settleContract(k, "done", player);
                for (const auto& letter : c.letters)
                    for (auto& k : roads_.contracts)
                        if (k.id == letter && k.status == "taken")
                        {
                            settleContract(k, "done", std::string());   // Carried by the carters: the reward goes home.
                            bonds_.change(k.poster, k.target, {1, 0, 2, 0, 0}, calendarDays_);
                        }
                // The carters tell what they heard in the capital's market.
                std::vector<std::pair<std::string, Belief>> told;
                for (const auto& [holder, mine] : beliefs_)
                    if (const auto* job = society_.jobOf(holder); job && job->role == "merchant" && townOf(job->work.cell) &&
                                                                  townOf(job->work.cell)->id == capital.id)
                        for (const auto& b : mine)
                            if (b.confidence >= .4)
                                told.push_back({holder, b});
                for (const auto& p : society_.positions())
                    if (p.role == "merchant" && townOf(p.work.cell) && townOf(p.work.cell)->id == c.to)
                        if (const auto& holder = society_.state().careers.positions.at(p.id).holder; !holder.empty())
                            for (std::size_t i = 0; i < told.size() && i < 8; ++i)
                                believe(holder, told[i].second.subject, told[i].second.claim, "a carter from " + capital.id,
                                        told[i].second.confidence * .6);
                recordEvent({"caravan arrives", c.id, c.to, c.route.back(), 0, 0, {}, 0, 0, "from " + c.from});
            }
            c.status = "returning";
            std::reverse(c.route.begin(), c.route.end());
            c.leg = 0;
            continue;
        }
        if (homeward)
            continue;                                  // Empty on the way back: nothing worth robbing.
        const auto& cellId = c.route[c.leg];
        for (auto& camp : roads_.camps)
        {
            // A camp that has just robbed someone lies low with its loot for a couple of days.
            if (!camp.active || camp.cell != cellId || calendarDays_ - camp.lastRaid < 2)
                continue;
            // Hungrier and stronger bandits are bolder; guards, and the carters' own caution, keep them off.
            const double bold = camp.strength * (1 + camp.hunger / 100);
            const double odds = bold / (bold + c.guards * 5.0 + 10.0);
            if (chance(c.id, std::int64_t(c.leg)) >= odds)
            {
                recordEvent({"caravan passes", c.id, camp.id, cellId, 0, 0, {}, 0, 0, "the guards kept the bandits off"});
                camp.strength = std::max(0.0, camp.strength - .2 * c.guards);
                break;
            }
            // Raided: the load is taken (and soon eaten), the guards give as good as they can, the town is short.
            int taken = 0;
            for (const std::string item : {"meal", "herbs"})
                if (const auto* load = society_.account(c.account))
                    taken += society_.consume(c.account, item, Society::stock(*load, item), "stolen by bandits");
            camp.hunger = std::max(0.0, camp.hunger - taken * 4.0);
            camp.strength = std::clamp(camp.strength + .5 - .3 * c.guards, 0.0, 20.0);
            camp.lastRaid = calendarDays_;
            c.status = "raided";
            std::reverse(c.route.begin(), c.route.begin() + std::ptrdiff_t(c.leg + 1));
            c.route.resize(c.leg + 1);
            c.leg = 0;
            recordEvent({"raid", camp.id, c.to, cellId, 0, 0, {}, taken, 0, "the caravan to " + c.to + " was robbed"});
            // The town posts a bounty on the camp, and asks for guards for the next caravan.
            const bool bounty = std::any_of(roads_.contracts.begin(), roads_.contracts.end(), [&](const Contract& k) {
                return k.kind == "bounty" && k.target == camp.id && (k.status == "open" || k.status == "taken");
            });
            if (!bounty)
                postContract("bounty", "treasury", c.to, camp.id, 20 + std::int64_t(camp.strength * 3), 20,
                             "the bandits robbing the road at " + cellId);
            const bool escort = std::any_of(roads_.contracts.begin(), roads_.contracts.end(), [&](const Contract& k) {
                return k.kind == "escort" && k.town == c.to && (k.status == "open" || k.status == "taken");
            });
            if (!escort)
                postContract("escort", "treasury", c.to, c.to, 12, 7, "guarding the next caravan to " + c.to);
            for (const auto& p : society_.positions())
                if (p.role == "merchant" && townOf(p.work.cell) && townOf(p.work.cell)->id == c.to)
                    if (const auto& holder = society_.state().careers.positions.at(p.id).holder; !holder.empty())
                        believe(holder, camp.id, "raids the road at " + cellId, "the carters", .9);
            break;
        }
    }
    roads_.caravans.erase(std::remove_if(roads_.caravans.begin(), roads_.caravans.end(),
                                         [](const Caravan& c) { return c.status == "arrived"; }),
                          roads_.caravans.end());
}

void World::roadsDaily()
{
    const auto today = roads_.day;
    const auto& capital = towns_.front();
    const auto& economy = society_.authored().economy;
    int total = 0;
    for (const auto& t : towns_)
        total += t.residents;

    // Caravans set out for every other town with its share of what came into the capital.
    for (std::size_t i = 1; i < towns_.size(); ++i)
    {
        const auto& town = towns_[i];
        auto route = routeBetween(capital.market, town.market);
        if (route.size() < 2)
            continue;
        const double share = double(town.residents) / std::max(1, total);
        Caravan c;
        c.id = "cv" + std::to_string(roads_.nextId++);
        c.from = capital.id;
        c.to = town.id;
        c.account = "caravan:" + c.id;
        c.route = std::move(route);
        c.departed = calendarDays_;
        c.nextAt = time_ + LegSeconds;
        if (!society_.openAccount(c.account))
            continue;
        society_.shift("treasury", c.account, "meal", std::max(1, int(std::lround(economy.dailyMeals * share))), 0, "caravan loaded");
        society_.shift("treasury", c.account, "herbs", std::max(1, int(std::lround(economy.dailyHerbs * share))), 0, "caravan loaded");
        c.guards = 1 + std::min(3, town.guards / 10);
        for (auto& k : roads_.contracts)
        {
            if (k.kind == "escort" && k.status == "taken" && k.town == town.id)
            {
                c.guards += 2;
                c.escorts.push_back(k.taker);
            }
            // Letters nobody has taken in a week go with the carters.
            if (k.kind == "courier" && k.status == "open" && calendarDays_ - k.created >= 7)
                if (const auto* to = entity(k.target); to && townOf(to->cellId) && townOf(to->cellId)->id == town.id)
                {
                    k.status = "taken";
                    k.taker = c.account;
                    c.letters.push_back(k.id);
                }
        }
        recordEvent({"caravan departs", c.id, town.id, capital.market, 0, 0, {}, 0, 0, std::to_string(c.guards) + " guards"});
        roads_.caravans.push_back(std::move(c));
    }

    // Bandits: hunger grows; starving camps dwindle and scatter; where a road has none, some gather now and then.
    for (auto& camp : roads_.camps)
    {
        if (!camp.active)
            continue;
        camp.hunger = std::min(100.0, camp.hunger + 8);
        if (camp.hunger >= 100)
            camp.strength -= .5;
        if (camp.strength < .5)
        {
            camp.active = false;
            recordEvent({"bandits scattered", camp.id, {}, camp.cell, 0, 0, {}, 0, 0, "starved out"});
        }
    }
    if (today % 7 == 0)
        for (std::size_t i = 1; i < towns_.size(); ++i)
        {
            std::vector<std::string> wild;
            for (const auto& cellId : routeBetween(capital.market, towns_[i].market))
                if (!townOfCell_.count(cellId))
                    wild.push_back(cellId);
            const bool held = std::any_of(roads_.camps.begin(), roads_.camps.end(), [&](const BanditCamp& b) {
                return b.active && std::find(wild.begin(), wild.end(), b.cell) != wild.end();
            });
            if (held || wild.empty() || chance(towns_[i].id, today) > .34)
                continue;
            const auto& cellId = wild[roll(towns_[i].id, today) % wild.size()];
            roads_.camps.erase(std::remove_if(roads_.camps.begin(), roads_.camps.end(),
                                              [&](const BanditCamp& b) { return b.cell == cellId; }),
                               roads_.camps.end());
            roads_.camps.push_back({"camp_" + cellId, cellId, 2, 50, -100, true});
            recordEvent({"bandits gather", "camp_" + cellId, towns_[i].id, cellId, 0, 0, {}, 0, 0, "on the road to " + towns_[i].id});
        }

    // The watch goes after camps with a price on them: the more guards a town has, the better its chances.
    for (auto& k : roads_.contracts)
    {
        if (k.kind != "bounty" || (k.status != "open" && k.status != "taken"))
            continue;
        auto camp = std::find_if(roads_.camps.begin(), roads_.camps.end(), [&](const BanditCamp& b) { return b.id == k.target; });
        const auto town = std::find_if(towns_.begin(), towns_.end(), [&](const Town& t) { return t.id == k.town; });
        if (camp == roads_.camps.end() || !camp->active)
        {
            settleContract(k, "done", std::string());
            continue;
        }
        const double watch = town == towns_.end() ? 0 : std::min(10.0, town->guards / 3.0);
        if (watch > 0 && chance(k.id, today) < watch / (watch + camp->strength * 2))
        {
            camp->strength -= watch * .5;
            recordEvent({"patrol", k.town, camp->id, camp->cell, 0, 0, {}, 0, 0, "the watch went out after the bandits"});
            if (camp->strength <= .5)
            {
                camp->active = false;
                settleContract(k, "done", std::string());      // The watch did it: the reward goes back.
                recordEvent({"camp cleared", k.town, camp->id, camp->cell, 0, 0, {}, 0, 0, "by the watch"});
            }
        }
    }

    // Contracts out of time: the reward goes back to whoever set it aside (taken ones get three days' grace).
    for (auto& k : roads_.contracts)
        if ((k.status == "open" && calendarDays_ > k.due) || (k.status == "taken" && calendarDays_ > k.due + 3))
            settleContract(k, "expired", std::string());
    roads_.contracts.erase(std::remove_if(roads_.contracts.begin(), roads_.contracts.end(),
                                          [&](const Contract& k) {
                                              return (k.status == "done" || k.status == "expired") && calendarDays_ - k.due > 30;
                                          }),
                           roads_.contracts.end());

    // A town running short of food sends out for more: a supply run anyone can make.
    for (std::size_t i = 1; i < towns_.size(); ++i)
    {
        const auto* store = society_.account(towns_[i].store);
        const bool asked = std::any_of(roads_.contracts.begin(), roads_.contracts.end(), [&](const Contract& k) {
            return k.kind == "supply" && k.town == towns_[i].id && (k.status == "open" || k.status == "taken");
        });
        if (store && Society::stock(*store, "meal") < 4 && !asked)
            postContract("supply", "treasury", towns_[i].id, towns_[i].id, 15, 7, "bringing food to " + towns_[i].id + "'s market");
    }

    // Letters: someone who cares for another in a different town may pay to have word carried.
    std::set<std::string> writing;
    for (const auto& k : roads_.contracts)
        if (k.kind == "courier" && (k.status == "open" || k.status == "taken"))
            writing.insert(k.poster);
    for (const auto& [id, e] : entities_)
    {
        const auto* mine = bonds_.of(id);
        const auto* purse = society_.account(id);
        const auto* here = townOf(e.cellId);
        if (!e.npc || e.dead || !mine || !purse || purse->cash < 10 || writing.count(id) || !here)
            continue;
        for (const auto& [other, b] : *mine)
        {
            const auto* o = entity(other);
            const auto* there = o ? townOf(o->cellId) : nullptr;
            if (!o || !o->npc || o->dead || b.affinity < 40 || !there || there == here || chance(id + other, today) > .025)
                continue;
            postContract("courier", id, there->id, other, 3 + std::int64_t(roll(id, today) % 5), ContractDays,
                         "a letter from " + e.name + " to " + o->name + " in " + there->id);
            writing.insert(id);
            break;
        }
    }

    // Rumours spread along bonds (each tells a few they know well what they are surest of) and fade.
    std::vector<std::pair<std::string, Belief>> telling;
    for (const auto& [holder, mine] : beliefs_)
    {
        const auto* e = entity(holder);
        const auto* known = bonds_.of(holder);
        if (!e || !e->npc || e->dead || !known)
            continue;
        std::vector<const Belief*> best;
        for (const auto& b : mine)
            if (b.confidence >= .35)
                best.push_back(&b);
        std::sort(best.begin(), best.end(), [](const Belief* a, const Belief* b) { return a->confidence > b->confidence; });
        best.resize(std::min<std::size_t>(best.size(), 2));
        std::vector<std::pair<double, std::string>> listeners;
        for (const auto& [other, b] : *known)
            if (b.familiarity >= 30)
                if (const auto* o = entity(other); o && o->npc && !o->dead)
                    listeners.push_back({b.familiarity, other});
        std::sort(listeners.rbegin(), listeners.rend());
        for (std::size_t l = 0; l < listeners.size() && l < 3; ++l)
            for (const auto* b : best)
                if (listeners[l].second != b->subject)
                    telling.push_back({listeners[l].second, {listeners[l].second, b->subject, b->claim, holder, b->confidence * .7, calendarDays_}});
    }
    for (auto& [holder, mine] : beliefs_)
    {
        for (auto& b : mine)
            b.confidence *= .97;
        mine.erase(std::remove_if(mine.begin(), mine.end(), [](const Belief& b) { return b.confidence < .1; }), mine.end());
    }
    for (const auto& [listener, b] : telling)
        believe(listener, b.subject, b.claim, b.source, b.confidence);

    // Once a week the towns send what their stores took in back to the treasury, which pays the wages.
    if (today % 7 == 3)
        for (std::size_t i = 1; i < towns_.size(); ++i)
            if (const auto* store = society_.account(towns_[i].store); store && store->cash > 0)
                society_.shift(towns_[i].store, "treasury", "", 0, store->cash, "town tithe");
}
} // namespace ratw
