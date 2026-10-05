// The roads between towns (RatwRoads.h; Docs/Design/26-living-npcs.md, Phase 5). World members, kept here.
#include "RatwRoads.h"
#include "RatwItems.h"
#include "RatwWorld.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <set>

namespace ratw
{
namespace
{
// Placeholder numbers, to be tuned with play.
constexpr std::size_t BeliefsKept = 30;        // What one character keeps hearing about, strongest first.
constexpr int ContractDays = 10;
constexpr double EscortWaitDays = 2.0 / 24;    // A caravan waits two game hours at the market for its escorts.
constexpr double StuckSeconds = 600;           // A wagon that can't get on across a cell this long is moved on.
constexpr double DemandSeconds = 20;           // Bandits wait this long for a purse before they come at you.
constexpr double Reach = 1.6;
constexpr double TakeWorkAfterDays = 2;        // Residents take work players have left this long.
const char* const BanditNames[] = {"a ragged bandit", "a scarred cutthroat", "a bandit with a torn ear",
                                   "a hungry-eyed outlaw", "a gaunt highwayman", "a grey-muzzled robber"};

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
double between(Vec2 a, Vec2 b)
{
    return std::hypot(a.x - b.x, a.y - b.y);
}
std::string pennies(std::int64_t n)
{
    return std::to_string(n) + (n == 1 ? " penny" : " pennies");
}
bool homeward(const Caravan& c)
{
    return c.status == "raided" || c.status == "returning";
}
} // namespace

const Town* World::town(const std::string& id) const
{
    for (const auto& t : towns_)
        if (t.id == id)
            return &t;
    return nullptr;
}

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
    roadRoutes_.clear();
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
        for (const auto& p : society_.positions())
            if (p.role == "merchant" && p.work.cell == t.market)
            {
                t.marketX = p.work.x;
                t.marketY = p.work.y;
                break;
            }
        towns_.push_back(t);
    }
    if (towns_.size() < 2)
    {
        society_.setCapital(towns_.empty() ? std::string() : towns_.front().id);
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
    society_.setCapital(towns_.front().id);
    society_.setTradeByCaravan(true);               // What a town lacks comes by caravan (doc 42, Phase 7).
    if (!roads_.purses)
    {
        // Once (doc 42): each town keeps its own purse, its share of the treasury's money by how many live there.
        const std::int64_t cash = society_.account("treasury") ? society_.account("treasury")->cash : 0;
        for (std::size_t i = 1; i < towns_.size(); ++i)
        {
            const auto share = std::int64_t(double(cash) * towns_[i].residents / std::max(1, total));
            if (share > 0)
                society_.shift("treasury", towns_[i].store, "", 0, share, "town purse");
        }
        roads_.purses = true;
    }
    // The roads between every two towns; bandits camp in wild country along them (one in six such cells, to begin
    // with).
    for (std::size_t i = 0; i < towns_.size(); ++i)
        for (std::size_t j = i + 1; j < towns_.size(); ++j)
            if (auto route = routeBetween(towns_[i].market, towns_[j].market); route.size() >= 2)
                roadRoutes_.push_back(std::move(route));
    if (roads_.camps.empty())
        for (const auto& route : roadRoutes_)
            for (const auto& cellId : route)
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
    if (const auto* who = entity(paidTo); who && !who->npc)
        award(paidTo, "story", "contract:" + c.id);   // A contract fulfilled (doc 44).
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
                    const std::string& source, double confidence, const std::string& incident)
{
    if (holder.empty() || subject.empty() || holder == subject || claim.empty() || !(confidence > .05))
        return;
    auto& mine = beliefs_[holder];
    for (auto& b : mine)
        if (b.subject == subject && b.claim == claim)
        {
            if (confidence > b.confidence)
                b = {holder, subject, claim, source, std::min(1.0, confidence), calendarDays_, incident};
            return;
        }
    mine.push_back({holder, subject, claim, source, std::min(1.0, confidence), calendarDays_, incident});
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
            if (other.npc && !other.dead && !other.transient && other.cellId == cellId && id != subject)
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
    noteReckonings();
    if (towns_.size() < 2)
        return;
    tendPrices();
    // A courier is done when the one who took it stands before the recipient.
    for (auto& c : roads_.contracts)
    {
        if (c.kind != "courier" || c.status != "taken" || c.taker.rfind("caravan:", 0) == 0)
            continue;
        const auto* carrier = entity(c.taker);
        const auto* to = entity(c.target);
        if (carrier && to && !to->dead && carrier->cellId == to->cellId && between(carrier->position, to->position) <= 3)
        {
            settleContract(c, "done", c.taker);
            bonds_.change(c.target, c.taker, {2, 3, 2, 0, 0}, calendarDays_);
            bonds_.change(c.poster, c.target, {1, 0, 2, 0, 0}, calendarDays_);
            bonds_.change(c.poster, c.taker, {1, 3, 1, 0, 0}, calendarDays_);
            if (!carrier->npc)
                notice(c.taker, "You hand " + to->name + " the letter" + (c.reward ? " and are paid " + pennies(c.reward) : "") + ".");
        }
    }
    tendContractCarriers();                         // Residents fetching and delivering goods for contracts (doc 42).
    tendNegotiators();                              // And renegotiating standing orders (RatwTrade.cpp).
    const auto today = std::int64_t(std::floor(calendarDays_));
    if (today != roads_.day)
    {
        roads_.day = today;
        roadsDaily();
    }
    postProcurements();                             // The town's own buyers' wants (doc 35, Part 7): contracts for goods.
    tendRoadFolk();
    absorbJournal();
}

void World::tendPrices()
{
    // Once a game hour: a town's prices follow how much it has in store for the people living there.
    const auto hour = std::int64_t(std::floor(calendarDays_ * 24));
    if (hour == priceHour_)
        return;
    priceHour_ = hour;
    std::map<std::string, std::map<std::string, double>> factors;
    // Every good, every six hours (doc 42, Phase 7): dearer where the town's makers and suppliers want more than its
    // shops and producers have to spare, cheaper where there is plenty.
    if (hour / 6 != marketPriceAt_ || marketPrices_.empty())
    {
        marketPriceAt_ = hour / 6;
        marketPrices_.clear();
        for (const auto& t : towns_)
        {
            const auto m = marketOf(t.id);
            std::set<std::string> goods;
            for (const auto& [item, n] : m.want)
                goods.insert(item);
            for (const auto& [item, n] : m.spare)
                goods.insert(item);
            for (const auto& item : goods)
            {
                const double want = m.want.count(item) ? m.want.at(item) : 0, spare = m.spare.count(item) ? m.spare.at(item) : 0;
                marketPrices_[t.store][item] = std::clamp(1 + .5 * (want - spare) / (want + spare + 10), .8, 1.5);
            }
        }
    }
    factors = marketPrices_;
    for (const auto& t : towns_)
        if (const auto* store = society_.account(t.store))
            for (const auto& [item, each] : {std::pair<const char*, double>{"meal", .5}, {"herbs", .25}})
            {
                const double enough = std::max(1.0, t.residents * each);
                const double factor = std::clamp(1.4 - .4 * Society::stock(*store, item) / enough, .85, 1.6);
                factors[t.store][item] = factor;
                // A change worth talking of (doc 30): a tenth or more since the last one noticed.
                auto& seen = priceSeen_[t.id][item];
                if (seen.day < -50)
                    seen = {factor, calendarDays_ - 10, 0};      // First seen: the price as it is, not a change.
                else if (std::abs(factor - seen.factor) >= .1)
                {
                    seen.dir = factor > seen.factor ? 1 : -1;
                    seen.day = calendarDays_;
                    seen.factor = factor;
                }
            }
    society_.setPriceFactors(std::move(factors));
}

Entity& World::addRoadFolk(const std::string& id, const std::string& name, const std::string& description,
                           const std::string& cellId, Vec2 at, const std::string& kind, const std::string& of)
{
    Entity e;
    e.id = id;
    e.name = name;
    e.description = description;
    e.cellId = cellId;
    e.position = at;
    e.npc = true;
    e.transient = true;
    e.age = 30;
    e.strength = 55;
    e.offstage = tiered();                         // Until placeOnStage says otherwise (and loads the cell).
    auto& placed = entities_[id] = std::move(e);
    RoadFolk f;
    f.kind = kind;
    f.of = of;
    f.lastCell = cellId;
    f.enteredAt = time_;
    folk_[id] = f;
    return placed;
}

void World::removeRoadFolk(const std::string& id)
{
    entities_.erase(id);
    index_.dirty = true;
    folk_.erase(id);
    legs_.erase(id);
    pathRetryAt_.erase(id);
    pendingPortals_.erase(id);
}

bool World::withCaravan(const std::string& who, const Entity& wagon) const
{
    const auto* e = entity(who);
    if (!e || e->dead)
        return false;
    if (e->cellId == wagon.cellId)
        return true;
    std::set<std::string> near;
    nearCells(wagon, near);
    return near.count(e->cellId) > 0;
}

void World::tendRoadFolk()
{
    const auto stage = stageCells();
    RouteBudget budget{0, searchExpanded_, 2, 30000};   // Caravans plan a couple of routes a turn at most.
    // Every caravan on the road has its wagon in the world, where it last was (or at its first market).
    std::set<std::string> wagons;
    for (auto& c : roads_.caravans)
    {
        if (c.status == "arrived")
            continue;
        const auto id = "road:" + c.id;
        wagons.insert(id);
        auto* wagon = entity(id);
        if (!wagon)
        {
            const auto* from = town(homeward(c) ? c.to : c.from);
            std::string cellId = c.cell;
            Vec2 at{c.x, c.y};
            if (cellId.empty() || !cell(cellId))
            {
                if (!from)
                    continue;
                cellId = from->market;
                at = {from->marketX, from->marketY};
                c.cell = cellId;
                c.x = at.x;
                c.y = at.y;
            }
            wagon = &addRoadFolk(id, "a caravan bound for " + (homeward(c) ? c.from : c.to),
                                 "A laden cart hauled in harness, its carters trotting alongside.", cellId, at,
                                 "caravan", c.id);
        }
        if (tiered())
            placeOnStage(*wagon, stage);
        tendCaravan(c, *wagon, folk_[id], budget);
    }
    // A Dev Console fight's camp outlives its fight only by a restart: then it goes.
    roads_.camps.erase(std::remove_if(roads_.camps.begin(), roads_.camps.end(),
                                      [&](const BanditCamp& c) {
                                          return testCamp(c.id) && std::none_of(battles_.begin(), battles_.end(),
                                                                                [&](const Battle& b) { return b.camp == c.id; });
                                      }),
                       roads_.camps.end());
    // Bandits, in person, only where someone is near to meet them.
    for (auto& camp : roads_.camps)
        tendCamp(camp, stage);
    std::vector<std::string> gone;
    for (const auto& [id, f] : folk_)
    {
        const auto& of = f.of;
        if (f.kind == "caravan" ? !wagons.count(id)
                                : std::none_of(roads_.camps.begin(), roads_.camps.end(), [&](const BanditCamp& b) { return b.id == of; }))
            gone.push_back(id);
    }
    for (const auto& id : gone)
        removeRoadFolk(id);
    roads_.caravans.erase(std::remove_if(roads_.caravans.begin(), roads_.caravans.end(),
                                         [](const Caravan& c) { return c.status == "arrived"; }),
                          roads_.caravans.end());
}

void World::tendCaravan(Caravan& c, Entity& wagon, RoadFolk& f, RouteBudget& budget)
{
    // Set out only with its escorts alongside (or once it has waited long enough for them).
    if (c.status == "travelling" && c.leg == 0 && calendarDays_ < c.waitUntil &&
        !std::all_of(c.escorts.begin(), c.escorts.end(), [&](const std::string& who) { return withCaravan(who, wagon); }))
    {
        wagon.activity = "waiting at the market for its escort";
        return;
    }
    // Caravans wait out a storm (or a snowstorm, a sandstorm) where it finds them (Phase 9).
    if (c.status != "arrived" && skyOf(wagon.cellId) == 2)
    {
        wagon.activity = "waiting out the weather";
        stop(wagon.id);
        return;
    }
    if (wagon.cellId != c.cell)
    {
        c.cell = wagon.cellId;
        ++c.leg;
        f.enteredAt = time_;
        caravanEntered(c, wagon);
        if (c.status == "arrived")
            return;
    }
    c.x = wagon.position.x;
    c.y = wagon.position.y;
    const auto* dest = town(homeward(c) ? c.from : c.to);
    const auto lost = [&] {
        // No way on: what it carries goes back to where it came from, and it is done.
        const auto* home = town(c.from);
        if (const auto* load = society_.account(c.account))
            for (const auto& [item, n] : std::map<std::string, int>(load->stock.begin(), load->stock.end()))
                if (n > 0 && !(home && society_.shift(c.account, home->store, item, n, 0, "caravan turned back")))
                    society_.consume(c.account, item, n, "caravan lost");
        society_.closeAccount(c.account);
        c.status = "arrived";
        recordEvent({"caravan lost", c.id, c.to, wagon.cellId, 0, 0, {}, 0, 0, "no road on from here"});
    };
    if (!dest)
        return lost();
    const Vec2 goal{dest->marketX, dest->marketY};
    if (wagon.cellId == dest->market && (between(wagon.position, goal) <= 2.0 || time_ - f.enteredAt > StuckSeconds))
        return caravanArrived(c);
    if (wagon.cellId != dest->market && !cachedSteps(wagon.cellId).count(dest->market))
        return lost();
    if (!wagon.offstage && time_ - f.enteredAt > StuckSeconds)
    {
        // Stuck where it walks (a crowd, a bad corner): it goes on to the next cell as if offstage.
        const auto& next = cachedSteps(wagon.cellId).at(dest->market);
        for (const Door* way : doorsIn(wagon.cellId))
            if (way->portal && !way->locked && way->targetCell == next && ensureLoaded(next).ok)
            {
                stop(wagon.id);
                wagon.cellId = way->targetCell;
                wagon.position = way->arrival;
                wagon.transitioned = true;
                break;
            }
        f.enteredAt = time_;
        return;
    }
    const std::string task = homeward(c) ? "going home to " + c.from : "on the road to " + c.to;
    if (wagon.activity != task)
    {
        stop(wagon.id);
        wagon.activity = task;
    }
    headFor(wagon, task, dest->market, goal, budget);
}

void World::caravanEntered(Caravan& c, const Entity& wagon)
{
    for (const auto& who : c.escorts)
        if (withCaravan(who, wagon))
            ++c.with[who];
    if (homeward(c))
        return;                                    // Empty on the way back: nothing worth robbing.
    const auto& cellId = wagon.cellId;
    // Those travelling with it see what happens.
    std::vector<std::string> watching;
    for (const auto& [id, e] : entities_)
        if (!e.npc && withCaravan(id, wagon))
            watching.push_back(id);
    for (auto& camp : roads_.camps)
    {
        // A camp that has just robbed someone lies low with its loot for a couple of days.
        if (!camp.active || camp.cell != cellId || calendarDays_ - camp.lastRaid < 2)
            continue;
        // Hungrier and stronger bandits are bolder; guards, and the carters' own caution, keep them off. Escorts
        // count only if they are there.
        int guards = c.guards;
        for (const auto& who : c.escorts)
            guards += withCaravan(who, wagon) ? 2 : 0;
        const double bold = camp.strength * (1 + camp.hunger / 100);
        // (Doc 42, Phase 7: with traders' caravans on the roads too, a camp wins less easily than it did.)
        const double odds = bold / (bold + guards * 8.0 + 20.0);
        if (chance(c.id, std::int64_t(c.leg)) >= odds)
        {
            recordEvent({"caravan passes", c.id, camp.id, cellId, 0, 0, {}, 0, 0, "the guards kept the bandits off"});
            camp.strength = std::max(0.0, camp.strength - .2 * guards);
            for (const auto& id : watching)
                notice(id, "Figures watch the caravan from cover here, weigh up its guards, and let it pass.");
            break;
        }
        // Raided: the load is taken (and soon eaten), the guards give as good as they can, the town is short.
        int taken = 0;
        if (const auto* load = society_.account(c.account))
            for (const auto& [item, n] : std::map<std::string, int>(load->stock.begin(), load->stock.end()))
                if (n > 0)
                    taken += society_.consume(c.account, item, n, "stolen by bandits");
        // A trader's money too (doc 42, Phase 7): into the camp's hoard.
        if (const auto* load = society_.account(c.account); load && load->cash > 0)
        {
            society_.openAccount("bandits:" + camp.id);
            society_.shift(c.account, "bandits:" + camp.id, "", 0, load->cash, "robbed by bandits");
        }
        camp.hunger = std::max(0.0, camp.hunger - taken * 4.0);
        camp.strength = std::clamp(camp.strength + .5 - .3 * guards, 0.0, 20.0);
        camp.lastRaid = calendarDays_;
        c.status = "raided";
        if (auto* w = entity(wagon.id))
            w->name = "a robbed caravan limping home to " + c.from;
        recordEvent({"raid", camp.id, c.to, cellId, 0, 0, {}, taken, 0, "the caravan to " + c.to + " was robbed"});
        for (const auto& id : watching)
            notice(id, "Bandits burst from cover and fall on the caravan! In the struggle they make off with its load, "
                       "and the carters turn back for " + c.from + ".");
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

void World::caravanArrived(Caravan& c)
{
    const auto* wagon = entity("road:" + c.id);
    if (homeward(c))
    {
        if (!c.trader.empty())
            tradeCaravanHome(c);                    // A trader's: its takings home (doc 42, Phase 7).
        // Back where it set out: anything it still carries goes back into the stores, and it is done.
        const auto* home = town(c.from);
        if (const auto* load = society_.account(c.account); load && home)
            for (const auto& [item, n] : std::map<std::string, int>(load->stock.begin(), load->stock.end()))
                if (n > 0)
                    society_.shift(c.account, home->store, item, n, 0, "caravan unloaded");
        society_.closeAccount(c.account);
        c.status = "arrived";
        recordEvent({"caravan home", c.id, c.from, c.cell, 0, 0, {}, 0, 0, "back from " + c.to});
        return;
    }
    const auto* from = town(c.from);
    const auto* to = town(c.to);
    if (!from || !to)
    {
        c.status = "returning";
        return;
    }
    if (!c.trader.empty())
        tradeCaravanArrived(c);                     // A trader's: sold to the shops that want it (doc 42, Phase 7).
    // The load into the town's stores. Between two towns' stores it is a sale, at a wholesale price, as far as the
    // buyer can pay; from the capital it is the town's share, as ever.
    std::int64_t owed = 0;
    for (const std::string item : {"meal", "herbs"})
        if (const auto* load = society_.account(c.account); load && Society::stock(*load, item) > 0)
        {
            const int n = Society::stock(*load, item);
            if (society_.shift(c.account, to->store, item, n, 0, "caravan delivered"))
                owed += n * (item == "meal" ? 3 : 1);
        }
    if (from->store != "treasury" && to->store != "treasury" && owed > 0)
        if (const auto* buyer = society_.account(to->store); buyer && buyer->cash > 0)
            society_.shift(to->store, from->store, "", 0, std::min(owed, buyer->cash), "goods from " + from->id);
    // Escorts are paid for having been there: at the end, and for at least half the road.
    for (const auto& who : c.escorts)
        for (auto& k : roads_.contracts)
            if (k.kind == "escort" && k.taker == who && k.status == "taken" && k.town == c.to)
            {
                const bool there = wagon && withCaravan(who, *wagon) && c.with[who] * 2 >= int(c.leg);
                settleContract(k, there ? "done" : "expired", there ? who : std::string());
                if (const auto* e = entity(who); e && !e->npc)
                    notice(who, there ? "The caravan reaches " + c.to + " safely. You are paid " + pennies(k.reward) + " for guarding it."
                                      : "The caravan reached " + c.to + " without you, and the escort's pay went back.");
                if (there)
                    for (const auto& p : society_.positions())
                        if (p.role == "merchant" && townOf(p.work.cell) == to)
                            if (const auto& holder = society_.state().careers.positions.at(p.id).holder; !holder.empty())
                                bonds_.change(holder, who, {2, 3, 1, 0, 2}, calendarDays_);
            }
    for (const auto& letter : c.letters)
        for (auto& k : roads_.contracts)
            if (k.id == letter && k.status == "taken")
            {
                settleContract(k, "done", std::string());   // Carried by the carters: the reward goes home.
                bonds_.change(k.poster, k.target, {1, 0, 2, 0, 0}, calendarDays_);
            }
    // The carters tell what they heard in the market they came from.
    std::vector<Belief> told;
    for (const auto& [holder, mine] : beliefs_)
        if (const auto* job = society_.jobOf(holder); job && job->role == "merchant" && townOf(job->work.cell) == from)
            for (const auto& b : mine)
                if (b.confidence >= .4)
                    told.push_back(b);
    for (const auto& p : society_.positions())
        if (p.role == "merchant" && townOf(p.work.cell) == to)
            if (const auto& holder = society_.state().careers.positions.at(p.id).holder; !holder.empty())
                for (std::size_t i = 0; i < told.size() && i < 8; ++i)
                    believe(holder, told[i].subject, told[i].claim, "a carter from " + from->id, told[i].confidence * .6);
    recordEvent({"caravan arrives", c.id, c.to, c.cell, 0, 0, {}, 0, 0, "from " + c.from});
    // Home again, empty.
    c.status = "returning";
    c.leg = 0;
    c.with.clear();
    c.escorts.clear();
    if (auto* w = entity("road:" + c.id))
        w->name = "an empty caravan going home to " + c.from;
}

Caravan* World::sendCaravan(const Town& from, const Town& to, const std::map<std::string, int>& load)
{
    auto route = routeBetween(from.market, to.market);
    if (route.size() < 2)
        return nullptr;
    Caravan c;
    c.id = "cv" + std::to_string(roads_.nextId++);
    c.from = from.id;
    c.to = to.id;
    c.account = "caravan:" + c.id;
    c.route = std::move(route);
    c.departed = calendarDays_;
    if (!society_.openAccount(c.account))
        return nullptr;
    for (const auto& [item, n] : load)
        if (n > 0)
            society_.shift(from.store, c.account, item, n, 0, "caravan loaded");
    c.guards = 1 + std::min(3, from.guards / 10);
    for (auto& k : roads_.contracts)
    {
        if (k.kind == "escort" && k.town == to.id)
        {
            // Nobody has taken the escort in a couple of days: one of the town guard goes.
            if (k.status == "open" && calendarDays_ - k.created >= TakeWorkAfterDays)
                for (const auto& p : society_.positions())
                {
                    const auto& holder = society_.state().careers.positions.at(p.id).holder;
                    const auto* guard = holder.empty() ? nullptr : entity(holder);
                    if (p.role != "guard" || !guard || guard->dead || townOf(p.work.cell) != &from)
                        continue;
                    k.status = "taken";
                    k.taker = holder;
                    recordEvent({"contract taken", holder, k.poster, guard->cellId, 0, 0, {}, 0, k.reward, k.kind + ": " + k.detail});
                    break;
                }
            if (k.status == "taken" && std::find(c.escorts.begin(), c.escorts.end(), k.taker) == c.escorts.end())
                c.escorts.push_back(k.taker);
        }
        // Letters nobody has taken in a week go with the carters.
        if (k.kind == "courier" && k.status == "open" && calendarDays_ - k.created >= 7)
            if (const auto* recipient = entity(k.target), *poster = entity(k.poster);
                recipient && poster && townOf(recipient->cellId) == &to && townOf(poster->cellId) == &from)
            {
                k.status = "taken";
                k.taker = c.account;
                c.letters.push_back(k.id);
            }
    }
    if (!c.escorts.empty())
        c.waitUntil = calendarDays_ + EscortWaitDays;
    for (const auto& who : c.escorts)
        if (const auto* e = entity(who); e && !e->npc)
            notice(who, "The caravan for " + to.id + " is making ready at the market in " + from.id + ". It will wait a little for you.");
    recordEvent({"caravan departs", c.id, to.id, from.market, 0, 0, {}, 0, 0, std::to_string(c.guards) + " guards"});
    roads_.caravans.push_back(std::move(c));
    return &roads_.caravans.back();
}

bool World::errand(const std::string& resident, const ResidentLife& life, std::string& task, std::string& reason,
                   std::string& goalCell, Vec2& goal) const
{
    // A hungry traveller eats first (its provisions where it stands, else food bought or fetched), then goes on: the
    // errand never starves it.
    if (life.task == "eat" || ((life.task == "buy food" || life.task == "fetch food") && life.hunger >= 55))
        return false;
    // To another town's market, to renegotiate a standing order (RatwTrade.cpp); not by night.
    for (const auto& o : roads_.orders)
        if (o.negotiator == resident && life.task != "sleep")
            if (const auto* from = town(o.from))
            {
                task = "renegotiating an order";
                reason = "for " + o.item + ", at the market in " + o.from;
                goalCell = from->market;
                goal = {from->marketX, from->marketY + 1};
                return true;
            }
    for (const auto& k : roads_.contracts)
    {
        if (k.status != "taken" || k.taker != resident)
            continue;
        if (k.kind == "courier")
        {
            const auto* to = entity(k.target);
            const auto* home = society_.resident(k.target);
            const auto* me = entity(resident);
            if (!to || to->dead || !me || life.task == "sleep")
                return false;                      // A letter waits for the morning.
            task = "carrying a letter";
            reason = "to " + to->name;
            // To the recipient's home until they're in the same town, then to wherever they are.
            if (townOf(me->cellId) != townOf(to->cellId) && home && !home->homeCell.empty())
            {
                goalCell = home->homeCell;
                goal = {home->homeX, home->homeY};
            }
            else
            {
                goalCell = to->cellId;
                goal = to->position;
            }
            return true;
        }
        if (k.kind == "procure" && !k.source.empty())
        {
            // Goods for a contract (doc 42, Phase 4): to the shop that has them, then to the market that wants them.
            if (life.task == "sleep")
                return false;
            const auto* job = society_.jobOf(k.source);
            const auto* dest = town(k.town);
            if (!job || !dest)
                return false;
            const auto* good = items::good(k.item);
            const std::string what = good ? good->name : k.item;
            if (k.carried == 0)
            {
                const auto& at = job->role == "merchant" ? job->serve : job->work;
                task = "fetching goods";
                reason = what + " for " + k.detail;
                goalCell = at.cell;
                goal = {at.x, at.y};
            }
            else
            {
                task = "delivering goods";
                reason = what + " to the market in " + dest->id;
                goalCell = dest->market;
                goal = {dest->marketX, dest->marketY + 1};
            }
            return true;
        }
        if (k.kind == "escort")
            for (const auto& c : roads_.caravans)
                if (c.status == "travelling" && std::find(c.escorts.begin(), c.escorts.end(), resident) != c.escorts.end())
                {
                    const auto* wagon = entity("road:" + c.id);
                    const auto* dest = town(c.to);
                    if (!wagon || !dest)
                        return false;
                    task = "guarding a caravan";
                    reason = "on the road to " + c.to;
                    // Where the wagon is, while it waits; then the same road to the same market, alongside it.
                    if (c.leg == 0 && wagon->cellId != dest->market)
                    {
                        goalCell = wagon->cellId;
                        goal = wagon->position;
                    }
                    else
                    {
                        goalCell = dest->market;
                        goal = {dest->marketX, dest->marketY + 1};
                    }
                    return true;
                }
    }
    return false;
}

void World::residentsTakeWork()
{
    // A letter players have left for a couple of days: someone out of work in the writer's town carries it, for the
    // reward. (Escorts are taken by the town guard when the caravan sets out; bounties, by the watch.)
    const auto today = std::int64_t(std::floor(calendarDays_));
    std::set<std::string> busy;
    for (const auto& k : roads_.contracts)
        if (k.status == "taken")
            busy.insert(k.taker);
    for (auto& k : roads_.contracts)
    {
        if (k.kind != "courier" || k.status != "open" || calendarDays_ - k.created < TakeWorkAfterDays)
            continue;
        const auto* poster = entity(k.poster);
        const auto* here = poster ? townOf(poster->cellId) : nullptr;
        if (!here)
            continue;
        // Of those free to go, one (by lot).
        std::string chosen;
        std::uint64_t best = ~0ULL;
        for (const auto& [id, life] : society_.state().residents)
        {
            const auto* e = entity(id);
            if (!e || e->dead || e->transient || e->age < 16 || id == k.poster || id == k.target || busy.count(id) ||
                society_.jobOf(id) || society_.apprenticedTo(id) || townOf(life.homeCell) != here)
                continue;
            if (const auto lot = roll(k.id + id, today); lot < best)
                best = lot, chosen = id;
        }
        if (chosen.empty())
            continue;
        k.status = "taken";
        k.taker = chosen;
        busy.insert(chosen);
        recordEvent({"contract taken", chosen, k.poster, entity(chosen)->cellId, 0, 0, {}, 0, k.reward, k.kind + ": " + k.detail});
    }
    residentsFillContracts(busy);                   // Contracts for goods, after a day for the players (doc 42).
    residentsRenegotiate(busy);                     // Porters sent to renegotiate standing orders (RatwTrade.cpp).
}

void World::tradeBetweenTowns()
{
    // Towns other than the capital trade with each other: one with plenty for its people sends to one with little.
    for (std::size_t i = 1; i < towns_.size(); ++i)
        for (std::size_t j = 1; j < towns_.size(); ++j)
        {
            if (i == j)
                continue;
            const auto& from = towns_[i];
            const auto& to = towns_[j];
            const bool going = std::any_of(roads_.caravans.begin(), roads_.caravans.end(), [&](const Caravan& c) {
                return c.from == from.id && c.to == to.id && c.status == "travelling";
            });
            const auto* have = society_.account(from.store);
            const auto* want = society_.account(to.store);
            if (going || !have || !want)
                continue;
            std::map<std::string, int> load;
            for (const std::string item : {"meal", "herbs"})
            {
                const double mine = double(Society::stock(*have, item)) / std::max(1, from.residents);
                const double theirs = double(Society::stock(*want, item)) / std::max(1, to.residents);
                if (Society::stock(*have, item) < 8 || mine < 2 * theirs + .2)
                    continue;
                const int spare = int((Society::stock(*have, item) - theirs * from.residents) / 3);
                if (spare >= 3)
                    load[item] = std::min(12, spare);
            }
            if (!load.empty())
                sendCaravan(from, to, load);
        }
}

void World::roadsDaily()
{
    const auto today = roads_.day;
    const auto& capital = towns_.front();
    const auto& economy = society_.authored().economy;
    int total = 0;
    for (const auto& t : towns_)
        total += t.residents;

    // Caravans set out for every other town with its share of what came into the capital, and between the other
    // towns where one has plenty and another little.
    for (std::size_t i = 1; i < towns_.size(); ++i)
    {
        const double share = double(towns_[i].residents) / std::max(1, total);
        sendCaravan(capital, towns_[i], {{"meal", std::max(1, int(std::lround(economy.dailyMeals * share)))},
                                         {"herbs", std::max(1, int(std::lround(economy.dailyHerbs * share)))}});
    }
    tradeBetweenTowns();
    tradeCaravans();                                // Traders' caravans: goods where they're wanted (doc 42, Phase 7).
    residentsTakeWork();

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
            recoverHoard(camp, "treasury", "found in an abandoned camp");   // (Doc 42: not lost to the world.)
        }
    }
    if (today % 7 == 0)
        for (std::size_t r = 0; r < roadRoutes_.size(); ++r)
        {
            std::vector<std::string> wild;
            for (const auto& cellId : roadRoutes_[r])
                if (!townOfCell_.count(cellId))
                    wild.push_back(cellId);
            const bool held = std::any_of(roads_.camps.begin(), roads_.camps.end(), [&](const BanditCamp& b) {
                return b.active && std::find(wild.begin(), wild.end(), b.cell) != wild.end();
            });
            const auto& far = townOfCell_[roadRoutes_[r].back()];
            if (held || wild.empty() || chance(far + std::to_string(r), today) > .34)
                continue;
            const auto& cellId = wild[roll(far + std::to_string(r), today) % wild.size()];
            if (std::any_of(roads_.camps.begin(), roads_.camps.end(), [&](const BanditCamp& b) { return b.cell == cellId && b.active; }))
                continue;
            roads_.camps.erase(std::remove_if(roads_.camps.begin(), roads_.camps.end(),
                                              [&](const BanditCamp& b) { return b.cell == cellId; }),
                               roads_.camps.end());
            roads_.camps.push_back({"camp_" + cellId, cellId, 2, 50, -100, true});
            recordEvent({"bandits gather", "camp_" + cellId, far, cellId, 0, 0, {}, 0, 0, "on the road to " + far});
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
                if (town != towns_.end())
                    recoverHoard(*camp, town->store, "recovered from the bandits");
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

}
BanditCamp* World::campOf(const std::string& banditId)
{
    const auto f = folk_.find(banditId);
    if (f == folk_.end() || f->second.kind != "bandit")
        return nullptr;
    for (auto& camp : roads_.camps)
        if (camp.id == f->second.of)
            return &camp;
    return nullptr;
}

World::Encounter* World::encounterWith(const std::string& player)
{
    for (auto& e : encounters_)
        if (e.player == player)
            return &e;
    return nullptr;
}

void World::endEncounter(const std::string& camp, double spareFor)
{
    for (auto it = encounters_.begin(); it != encounters_.end();)
        if (it->camp == camp)
        {
            if (spareFor > 0)
                spared_[it->player] = time_ + spareFor;
            it = encounters_.erase(it);
        }
        else
            ++it;
}

Result World::callBandits(const std::string& near, int count)
{
    const auto* e = entity(near);
    if (!e || e->dead)
        return {false, "No such character.", near};
    count = std::clamp(count, 1, 6);
    if (!ensureLoaded(e->cellId).ok)
        return {false, "That place isn't loaded.", near};
    Vec2 spot{-1, -1};
    for (int ring = 5; ring <= 10 && spot.x < 0; ++ring)
        for (int k = 0; k < 16 && spot.x < 0; ++k)
        {
            const double a = k * 3.14159265358979323846 / 8;
            const Vec2 p{std::floor(e->position.x + std::cos(a) * ring) + .5, std::floor(e->position.y + std::sin(a) * ring) + .5};
            if (standable(e->cellId, p) && !nearPortal(e->cellId, p, 3))
                spot = p;
        }
    if (spot.x < 0)
        return {false, "There is no open ground near them for a camp.", near};
    int n = 0;
    for (const auto& c : roads_.camps)
        n += c.id.rfind("camp_dm_", 0) == 0;
    BanditCamp camp{"camp_dm_" + std::to_string(n + 1), e->cellId, 3.0 * count, 50, -100, true};
    camp.x = spot.x;
    camp.y = spot.y;
    roads_.camps.push_back(camp);
    recordEvent({"bandits called", "dungeon master", near, e->cellId, 0, 0, {}, count, 0, camp.id});
    return {true, std::to_string(count) + (count == 1 ? " bandit gathers" : " bandits gather") + " a few strides from " + e->name + ".",
            camp.id};
}

bool World::hostile(const std::string& id) const
{
    const auto f = folk_.find(id);
    const auto* e = entity(id);
    return f != folk_.end() && f->second.kind == "bandit" && e && !e->dead && e->downedLeft <= 0;
}

std::int64_t World::banditDemand(const std::string& player) const
{
    for (const auto& e : encounters_)
        if (e.player == player && !e.creeping)       // (Creeping up, they haven't asked yet.)
            return std::max<std::int64_t>(e.demand, e.fighting ? 1 : 0);
    return 0;
}

std::vector<std::pair<std::string, std::string>> World::takeNotices()
{
    std::vector<std::pair<std::string, std::string>> out;
    out.swap(notices_);
    return out;
}

void World::tendCamp(BanditCamp& camp, const std::set<std::string>& stage)
{
    std::vector<std::string> mine;
    for (const auto& [id, f] : folk_)
        if (f.kind == "bandit" && f.of == camp.id)
            mine.push_back(id);
    if (!stage.count(camp.cell))
    {
        // Nobody near: the bandits are only numbers again (and the fallen are gone).
        for (const auto& id : mine)
            removeRoadFolk(id);
        endEncounter(camp.id, 0);
        return;
    }
    if (mine.empty())
    {
        // Someone has come near: the camp is there in person, once a visit.
        if (!camp.active || camp.strength < .5 || !ensureLoaded(camp.cell).ok)
            return;
        const auto* c = cell(camp.cell);
        if (!c)
            return;
        if (camp.x < 0 || !standable(camp.cell, {camp.x, camp.y}))
        {
            // Where they camp: open ground near the middle, off the way through.
            double best = 1e18;
            for (int y = 1; y + 1 < c->height; ++y)
                for (int x = 1; x + 1 < c->width; ++x)
                {
                    const Vec2 p{x + .5, y + .5};
                    const double d = between(p, {c->width / 2.0, c->height / 2.0}) + (roll(camp.id, y * 1000 + x) % 5);
                    if (d < best && standable(camp.cell, p) && !nearPortal(camp.cell, p, 3))
                        best = d, camp.x = p.x, camp.y = p.y;
                }
            if (best >= 1e18)
                return;
        }
        const int n = std::clamp(int(std::ceil(camp.strength / 3)), 1, 6);
        for (int i = 0; i < n; ++i)
        {
            Vec2 at{camp.x + std::cos(i * 1.3) * 1.5 * (i > 0), camp.y + std::sin(i * 1.3) * 1.5 * (i > 0)};
            if (!standable(camp.cell, at))
                at = {camp.x, camp.y};
            const std::string id = "road:" + camp.id + ":" + std::to_string(i);
            const std::string name = i == 0 ? "the bandit leader" : BanditNames[roll(id, 7) % std::size(BanditNames)];
            auto& b = addRoadFolk(id, name, "Hard, hungry and armed, watching the road.", camp.cell, at, "bandit", camp.id);
            b.activity = "lying in wait";
            if (tiered())
                placeOnStage(b, stage);
            auto& f = folk_[id];
            f.hp = i == 0 ? 22 : 16;
            f.share = camp.strength / n;
        }
        return;
    }
    const Vec2 spot{camp.x, camp.y};
    std::vector<Entity*> standing;
    for (const auto& id : mine)
        if (auto* b = entity(id); b && !b->dead && b->downedLeft <= 0)
            standing.push_back(b);
    auto enc = std::find_if(encounters_.begin(), encounters_.end(), [&](const Encounter& e) { return e.camp == camp.id; });
    if (enc == encounters_.end())
    {
        // Back to the camp; and a traveller alone on the road, worth stopping, is stopped.
        for (auto* b : standing)
            if (between(b->position, spot) > 3 && b->path.empty() && time_ >= folk_[b->id].nextPath)
            {
                folk_[b->id].nextPath = time_ + 3;
                moveTo(b->id, spot.x, spot.y);
            }
        const bool bold = camp.active && calendarDays_ - camp.lastRaid >= 2 && (camp.hunger >= 25 || camp.strength >= 8);
        if (!bold || standing.empty())
            return;
        for (auto& [id, e] : entities_)
        {
            if (e.npc || e.dead || e.cellId != camp.cell || between(e.position, spot) > 14 || encounterWith(id))
                continue;
            if (const auto spare = spared_.find(id); spare != spared_.end() && time_ < spare->second)
                continue;
            const auto* purse = society_.account(id);
            const std::int64_t cash = purse ? purse->cash : 0;
            if (cash <= 0)
            {
                notice(id, "Rough-looking figures watch you from the camp, see you have nothing worth taking, and let you be.");
                spared_[id] = time_ + 600;
                continue;
            }
            const std::int64_t demand = std::min<std::int64_t>(cash, 4 + std::llround(camp.strength * 2));
            // A traveller who hasn't noticed them is crept up on instead, low through the grass, to be taken unawares
            // (doc 40); one who has sees them step out.
            double noticed = 0;
            for (const auto* b : standing)
                noticed = std::max(noticed, noticeSenses(e, e.position, e.facing, *b, b->position, false, false, 0, camp.cell).total());
            if (noticed < battle::AwareSuspicious)
            {
                Encounter creep{camp.id, id, demand, time_, false};
                creep.creeping = true;
                encounters_.push_back(creep);
                recordEvent({"bandits creep", camp.id, id, camp.cell, 0, 0, {}, 0, 0, "on the road"});
                break;
            }
            encounters_.push_back({camp.id, id, demand, time_, false});
            notice(id, "Bandits step out onto the road around you. \"Your purse, friend: " + pennies(demand) +
                           ", and you walk on.\" (Pay them, fight, or get clear of them.)");
            recordEvent({"bandits demand", camp.id, id, camp.cell, 0, 0, {}, 0, demand, "on the road"});
            break;
        }
        return;
    }
    if (inBattle(enc->player))
        return;                                     // The fight is in its arena now (RatwBattle.cpp).
    auto* player = entity(enc->player);
    if (!player || player->dead || player->cellId != camp.cell || between(player->position, spot) > 26 || standing.empty())
    {
        if (player && !player->dead && !standing.empty() && !enc->creeping)
            notice(enc->player, "You get clear of the bandits.");   // (Not of those it never knew were there.)
        endEncounter(camp.id, 60);
        return;
    }
    if (enc->creeping)
    {
        // Creeping up (doc 40): crouched, to a stride behind the traveller. What the traveller notices of them builds
        // as in a fight; a rustle half noticed is told; noticed, they rise and rush; unnoticed within reach, the fight
        // begins with the traveller taken unawares (startBattle's ambush). Too long at it, they give up.
        if (time_ - enc->since > battle::CreepGiveUp)
        {
            for (auto* b : standing)
                setPosture(b->id, "standing");
            endEncounter(camp.id, 300);
            return;
        }
        double most = 0;
        for (auto* b : standing)
            most = std::max(most, noticeSenses(*player, player->position, player->facing, *b, b->position, true,
                                               std::hypot(b->velocity.x, b->velocity.y) > 1e-6, 0, camp.cell)
                                      .total());
        enc->spotted = most >= battle::NoticeFloor ? enc->spotted + most * battle::NoticeGain : std::max(0.0, enc->spotted - battle::Calm);
        if (enc->spotted >= battle::AwareSuspicious && !enc->rustled)
        {
            enc->rustled = true;
            notice(enc->player, "Something rustles low in the grass behind you.");
        }
        if (enc->spotted < battle::AwareAlert)
        {
            const Vec2 behind{player->position.x - std::cos(player->facing) * 1.5, player->position.y - std::sin(player->facing) * 1.5};
            for (auto* b : standing)
            {
                if (b->posture != "crouching")
                    setPosture(b->id, "crouching");
                if (between(b->position, player->position) <= battle::CreepSpring * .9)
                {
                    stop(b->id);                    // Still a moment, then the spring: no pawstep gives it away.
                    if (const auto started = startBattle(b->id, player->id, false); started.ok)
                    {
                        enc->creeping = false;
                        enc->fighting = true;
                        return;
                    }
                }
                auto& f = folk_[b->id];
                if (time_ >= f.nextPath)
                {
                    f.nextPath = time_ + 1.5;
                    moveTo(b->id, behind.x, behind.y);
                }
            }
            return;
        }
        // Seen: no more creeping. They rise and rush.
        enc->creeping = false;
        enc->fighting = true;
        for (auto* b : standing)
            setPosture(b->id, "standing");
        notice(enc->player, "Bandits rise from the grass around you and rush you!");
        recordEvent({"fight", camp.id, enc->player, camp.cell, 0, 0, {}, 0, 0, "bandits rush"});
    }
    if (!enc->fighting && time_ - enc->since > DemandSeconds)
    {
        enc->fighting = true;
        notice(enc->player, "The bandits lose patience. They come at you!");
        recordEvent({"fight", camp.id, enc->player, camp.cell, 0, 0, {}, 0, 0, "bandits attack"});
    }
    const bool fighting = enc->fighting;
    for (auto* b : standing)
    {
        auto& f = folk_[b->id];
        const double gap = between(b->position, player->position);
        if (gap > Reach * .8)
        {
            // Close in (the demand made face to face, the fight hand to hand).
            if (time_ >= f.nextPath && (fighting || gap > 2.5))
            {
                f.nextPath = time_ + 1.5;
                moveTo(b->id, player->position.x, player->position.y);
            }
            continue;
        }
        if (!b->path.empty())
            stop(b->id);
        if (!fighting)
            continue;
        // Close enough: the fight begins, in its arena (RatwBattle.cpp), the whole band against the traveller.
        if (const auto started = startBattle(b->id, player->id, false); started.ok)
            return;
    }
}

void World::beaten(Entity& player, BanditCamp& camp)
{
    // Beaten to the ground, and robbed: half the purse, or what they asked if that is more.
    player.stamina = 0;
    player.exhausted = true;
    std::int64_t asked = 0;
    for (const auto& e : encounters_)
        if (e.camp == camp.id)
            asked = e.demand;
    const auto loot = "bandits:" + camp.id;
    society_.openAccount(loot);
    const auto* purse = society_.account(player.id);
    const std::int64_t taken = purse ? std::min(purse->cash, std::max(asked, purse->cash / 2)) : 0;
    if (taken > 0)
        society_.shift(player.id, loot, "", 0, taken, "robbed by bandits");
    camp.hunger = std::max(0.0, camp.hunger - taken * 2.0);
    setPosture(player.id, "lying");
    notice(player.id, "You are beaten to the ground." +
                          (taken > 0 ? " The bandits take " + pennies(taken) + " and leave you lying in the road." : std::string()));
    recordEvent({"robbed", camp.id, player.id, camp.cell, 0, 0, {}, 0, taken, "beaten and robbed on the road"});
    for (const auto& t : towns_)
        for (const auto& p : society_.positions())
            if (p.role == "merchant" && townOf(p.work.cell) == &t)
                if (const auto& holder = society_.state().careers.positions.at(p.id).holder; !holder.empty())
                    believe(holder, camp.id, "robs travellers at " + camp.cell, "a traveller", .7);
    endEncounter(camp.id, (calendar::SecondsPerDay / 24));
}

void World::recoverHoard(const BanditCamp& camp, const std::string& to, const std::string& kind)
{
    // What a camp had taken goes back into the world, not into the ground (doc 42).
    const auto loot = "bandits:" + camp.id;
    if (const auto* takings = society_.account(loot); takings && takings->cash > 0)
        society_.shift(loot, to, "", 0, takings->cash, kind);
}

void World::clearCamp(BanditCamp& camp, const std::string& by)
{
    camp.active = false;
    camp.strength = 0;
    std::string words = "The bandit camp is broken.";
    const auto loot = "bandits:" + camp.id;
    if (const auto* takings = society_.account(loot); takings && takings->cash > 0)
    {
        const auto found = takings->cash;
        if (society_.shift(loot, by, "", 0, found, "bandits' takings"))
            words += " Among their things you find " + pennies(found) + ".";
    }
    society_.closeAccount(loot);
    for (auto& k : roads_.contracts)
        if (k.kind == "bounty" && k.target == camp.id && (k.status == "open" || k.status == "taken"))
        {
            settleContract(k, "done", by);
            if (k.reward)
                words += " The bounty on them, " + pennies(k.reward) + ", is yours.";
        }
    const auto* who = entity(by);
    recordEvent({"camp cleared", by, camp.id, camp.cell, 0, 0, {}, 0, 0, "by " + (who ? who->name : by)});
    for (const auto& p : society_.positions())
        if (p.role == "merchant" && townOf(p.work.cell))
            if (const auto& holder = society_.state().careers.positions.at(p.id).holder; !holder.empty())
            {
                believe(holder, by, "drove the bandits off the road at " + camp.cell, "the carters", .8);
                bonds_.change(holder, by, {3, 2, 1, 0, 6}, calendarDays_);
            }
    notice(by, words);
    endEncounter(camp.id, 0);
}

// Once a day, in every world (Phase 7; with one town as with many).
void World::gossip()
{
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
                    telling.push_back({listeners[l].second, {listeners[l].second, b->subject, b->claim, holder, b->confidence * .7, calendarDays_, b->incident}});
    }
    for (auto& [holder, mine] : beliefs_)
    {
        for (auto& b : mine)
            b.confidence *= .97;
        mine.erase(std::remove_if(mine.begin(), mine.end(), [](const Belief& b) { return b.confidence < .1; }), mine.end());
    }
    for (const auto& [listener, b] : telling)
        believe(listener, b.subject, b.claim, b.source, b.confidence, b.incident);

}

Result World::payBandits(const std::string& player, const std::string& bandit)
{
    auto* p = entity(player);
    auto* camp = campOf(bandit);
    if (!p || p->npc || p->dead)
        return {false, "No such character.", {}};
    auto* enc = encounterWith(player);
    if (!camp || !enc || enc->camp != camp->id)
        return {false, "Nobody is asking you for anything.", bandit};
    const auto* purse = society_.account(player);
    const std::int64_t cash = purse ? purse->cash : 0;
    const std::int64_t amount = std::min(cash, enc->fighting ? std::max(enc->demand, cash / 2) : enc->demand);
    if (amount <= 0)
        return {false, "You have nothing to give them.", bandit};
    const auto loot = "bandits:" + camp->id;
    society_.openAccount(loot);
    if (!society_.shift(player, loot, "", 0, amount, "paid to bandits"))
        return {false, "They will not take that.", bandit};
    camp->hunger = std::max(0.0, camp->hunger - amount * 3.0);
    recordEvent({"paid off bandits", player, camp->id, p->cellId, 0, 0, {}, 0, amount, "on the road"});
    endEncounter(camp->id, (calendar::SecondsPerDay / 24));
    return {true, "You hand over " + pennies(amount) + ". The bandits step aside and let you go on your way.", bandit};
}

} // namespace ratw
