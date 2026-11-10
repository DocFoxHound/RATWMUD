// The roads between towns (RatwRoads.h; Docs/Design/26-living-npcs.md, Phase 5). World members, kept here.
#include "RatwRoads.h"
#include "RatwItems.h"
#include "RatwWorld.h"

#include "RatwCalendar.h"

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
// Why a reward paid less than was posted, or nothing (doc 79, 9.3: a staged-sourced reward within the day's rewards cap).
const char* const DayPaid = "you have had all the rewards one day pays";
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

bool World::arrivalIn(const std::string& townId, std::string& cellOut, Vec2& positionOut, const std::string& cell,
                      Vec2 position)
{
    const auto spawnTown = townOf(spawnCell_);
    if (spawnTown && spawnTown->id == townId)
    {
        cellOut = spawnCell_, positionOut = spawnPosition_;
        return true;
    }
    const auto town = std::find_if(towns_.begin(), towns_.end(), [&](const Town& t) { return t.id == townId; });
    if (town == towns_.end())
        return false;
    // Beside the market's merchant: the tiles around where it stands, the nearest first, where someone could stand.
    if (cells_.count(town->market) && ensureLoaded(town->market).ok)
        for (const Vec2 step : {Vec2{0, 1}, Vec2{1, 0}, Vec2{-1, 0}, Vec2{0, -1}, Vec2{1, 1}, Vec2{-1, 1}, Vec2{1, -1}, Vec2{-1, -1},
                                Vec2{0, 2}, Vec2{2, 0}, Vec2{-2, 0}, Vec2{0, -2}})
            if (const Vec2 at{town->marketX + step.x, town->marketY + step.y}; standable(town->market, at))
            {
                cellOut = town->market, positionOut = at;
                return true;
            }
    if (!cell.empty() && cells_.count(cell) && ensureLoaded(cell).ok && standable(cell, position))
    {
        cellOut = cell, positionOut = position;
        return true;
    }
    return false;
}

const Town* World::townOf(const std::string& cellId) const
{
    const auto found = townIndex_.find(cellId);
    return found == townIndex_.end() || found->second >= towns_.size() ? nullptr : &towns_[found->second];
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
    townIndex_.clear();
    society_.forgetPlaces();
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
    // Where each place is on the map: an outdoor cell by its corner, an interior by its door (one upstairs, or further
    // in, by the door that leads to it in turn); and so where each merchant works.
    std::map<std::string, std::pair<Vec2, bool>> placeOf;           // Cell -> (where, outdoors).
    for (const auto& [id, c] : cells_)
        if (c.outdoors)
            placeOf[id] = {{c.worldX, c.worldY}, true};
    for (int depth = 0; depth < 4; ++depth)
    {
        bool more = false;
        for (const auto& [id, d] : doors_)
            if (const auto from = placeOf.find(d.cellId); from != placeOf.end() && !placeOf.count(d.targetCell))
            {
                const auto& [at, outdoors] = from->second;
                placeOf[d.targetCell] = {outdoors ? Vec2{at.x + d.position.x, at.y + d.position.y} : at, false};
                more = true;
            }
        if (!more)
            break;
    }
    std::map<std::string, std::vector<std::pair<std::string, Vec2>>> merchantsAt;   // Region -> (cell, where) each.
    for (const auto& p : society_.positions())
        if (p.role == "merchant")
            if (const auto region = regionOf(p.work.cell); !region.empty())
                if (const auto place = placeOf.find(p.work.cell); place != placeOf.end())
                {
                    const auto& [at, outdoors] = place->second;
                    merchantsAt[region].push_back({p.work.cell, outdoors ? Vec2{at.x + p.work.x, at.y + p.work.y} : at});
                }
    // The Country's communities stay plain places until they are made towns (communities.json `town`; the user,
    // 2026-11-03: doc 71, Phase C Progress): a restart, after their people came, finds them as a running world left
    // them, no stores, roads, camps or caravans of their own; their markets, posts and homes are kept as they are.
    const auto plain = items::farmPeople().plainRegions([&](const std::string& area) { return cells_.count(area) > 0; });
    for (const auto& [region, count] : living)
    {
        if (count < 5 || !markets.count(region) || plain.count(region))
            continue;
        Town t;
        t.id = region;
        t.residents = count;
        t.guards = guards[region];
        // The market is in the thick of the town's merchants (the user, 2026-10-12): the place with the most of them
        // (the first of two alike, as ever), of those with another place's merchant within ClusterReach and at least
        // half as many merchants within reach as the busiest. A mill or a press out in the town's fields, with two
        // merchants to a town shop's one, never is.
        constexpr double ClusterReach = 40;
        const auto& theirs = merchantsAt[region];
        std::map<std::string, std::pair<int, int>> around;   // Cell -> merchants within reach, and other places' of them.
        int busiest = 0;
        bool anyNeighbours = false;
        for (const auto& [cellId, n] : markets[region])
        {
            const auto first = std::find_if(theirs.begin(), theirs.end(), [&](const auto& m) { return m.first == cellId; });
            if (first == theirs.end())
                continue;
            auto& [near, others] = around[cellId];
            for (const auto& m : theirs)
                if (std::hypot(m.second.x - first->second.x, m.second.y - first->second.y) <= ClusterReach)
                    ++near, others += m.first != cellId;
            busiest = std::max(busiest, near);
            anyNeighbours = anyNeighbours || others > 0;
        }
        int best = -1;
        for (const auto& [cellId, n] : markets[region])
        {
            const auto found = around.find(cellId);
            const bool thick = around.empty() || (found != around.end() && found->second.first * 2 >= busiest &&
                                                  (found->second.second > 0 || !anyNeighbours));
            if (n > best && thick)
                best = n, t.market = cellId;
        }
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
    for (auto& t : towns_)
    {
        t.store = &t == &towns_.front() ? "treasury" : "stores:" + t.id;
        if (t.store != "treasury")
            society_.openAccount(t.store);
    }
    for (const auto& [cellId, c] : cells_)
        for (const auto& t : towns_)
            if (c.region == t.id)
            {
                townOfCell_[cellId] = t.id;
                townIndex_[cellId] = std::size_t(&t - towns_.data());
            }
    society_.setCapital(towns_.front().id);
    // (The roads between markets, and the bandits along them, are the road layer's since doc 79's Phase 4: ensureRoadMarkets.)
    absorbJournal();
}

Contract& World::postContract(const std::string& kind, const std::string& poster, const std::string& town,
                              const std::string& target, std::int64_t reward, double days, const std::string& detail,
                              const std::string& from)
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
    c.from = from;
    // The reward is set aside at once, from the poster's own purse (or the treasury): it exists, or there's no reward.
    const auto escrow = "contract:" + c.id;
    if (reward > 0 && society_.openAccount(escrow) && society_.shift(poster, escrow, "", 0, reward, "reward set aside"))
        c.reward = reward;
    else
        society_.closeAccount(escrow, poster);
    roads_.contracts.push_back(c);
    recordEvent({"contract posted", poster, target, {}, 0, 0, {}, 0, c.reward, kind + ": " + detail});
    return roads_.contracts.back();
}

std::int64_t World::npcCouriersToday() const
{
    const auto today = std::int64_t(std::floor(calendarDays_));
    std::int64_t n = 0;
    for (const auto& k : roads_.contracts)
        if (k.kind == "courier" && std::int64_t(std::floor(k.created)) == today)
            if (const auto* poster = entity(k.poster); poster && poster->npc)
                ++n;
    return n;
}

bool World::npcCourierRoom() const
{
    return npcCouriersToday() < society_.statusTable().cap("courierADay");
}

bool World::withdrawContract(const std::string& id)
{
    for (auto& c : roads_.contracts)
        if (c.id == id && (c.status == "open" || c.status == "taken"))
        {
            settleContract(c, "expired", {});
            return true;
        }
    return false;
}

std::int64_t World::settleContract(Contract& c, const std::string& status, const std::string& paidTo)
{
    const auto escrow = "contract:" + c.id;
    // (Staged: a contract's, bounty's or escort's staged-sourced reward held to the taker's day's rewards cap, the rest back
    // to its poster: doc 79, 9.3.)
    const auto reward = paidTo.empty() ? c.reward : society_.cappedPayout(escrow, paidTo, c.reward, "rewardsADay", c.poster);
    const bool moved = reward > 0 && society_.shift(escrow, paidTo.empty() ? c.poster : paidTo, "", 0, reward,
                                                    paidTo.empty() ? "reward returned" : "contract reward");
    society_.closeAccount(escrow, paidTo.empty() ? c.poster : paidTo);
    if (const auto* who = entity(paidTo); who && !who->npc)
        practise(paidTo, "contract.done", {c.id});   // A contract fulfilled teaches commerce (by practice: doc 49).
    c.status = status;
    // (The event's coins: what was paid, which the day's cap may have made less than was posted; a reward gone home, whole.)
    const std::int64_t paid = paidTo.empty() ? 0 : moved ? reward : 0;
    recordEvent({"contract " + status, paidTo.empty() ? c.poster : paidTo, c.target, {}, 0, 0, {}, 0, paidTo.empty() ? c.reward : paid,
                 c.kind + ": " + c.detail});
    return paid;
}

std::vector<const Contract*> World::contractsNear(const std::string& player) const
{
    std::vector<const Contract*> out;
    const auto* p = entity(player);
    const auto* here = p ? roadMarketOf(p->cellId) : nullptr;
    if (!here)
        return out;
    // (An escort is asked for by the market the wagons go to, and taken there or where they set out: at the board of the
    // market its road starts from, while a wagon to it is committed there, where the wagon will wait for its escort. Doc
    // 79, 8.1. An older save's escort names no start: any market a committed wagon to its town leaves from.)
    const auto& committed = society_.board().committed;
    const auto leavesHere = [&](const std::string& to) {
        return std::any_of(committed.begin(), committed.end(),
                           [&](const market::Departure& d) { return d.kind == "wagon" && d.from == here->id && d.to == to; });
    };
    for (const auto& c : roads_.contracts)
        if (c.status == "open" && (c.town == here->id || (c.kind == "courier" && roadMarketOf(entity(c.poster) ? entity(c.poster)->cellId : "") == here) ||
                                   (c.kind == "escort" && (c.from.empty() || c.from == here->id) && leavesHere(c.town))))
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
                    if (!c.offeredTo.empty() && c.offeredTo != player && c.offeredUntil > calendarDays_)
                        return {false, "That work has been offered to someone else for now.", c.id};
                    c.status = "taken";
                    c.taker = player;
                    // (An older save's escort names no road: taken where a wagon to its town sets out, it is that road's
                    // from now on, and no other market's wagon takes its escort along. Doc 79, Phase 4, third repair.)
                    if (const auto* here = roadMarketOf(p->cellId); c.kind == "escort" && c.from.empty() && here && here->id != c.town)
                        c.from = here->id;
                    recordEvent({"contract taken", player, c.poster, {}, 0, 0, {}, 0, c.reward, c.kind + ": " + c.detail});
                    return {true, "You take on: " + c.detail + (c.reward ? " (" + std::to_string(c.reward) + " pennies)" : "") + ".", c.id};
                }
    return {false, "There is no such work to be had here.", contractId};
}

Result World::takeOfferedContract(const std::string& player, const std::string& contractId)
{
    // Offered by letter (doc 55, 5): the one it was offered to takes it on from wherever they are, while it holds.
    for (auto& c : roads_.contracts)
        if (c.id == contractId)
        {
            if (c.status != "open")
                return {false, "Someone has taken that work already, or it is done.", c.id};
            if (c.offeredTo != player)
                return {false, "That work wasn't offered to you.", c.id};
            c.status = "taken";
            c.taker = player;
            recordEvent({"contract taken", player, c.poster, {}, 0, 0, {}, 0, c.reward, c.kind + ": " + c.detail});
            return {true, "You take on: " + c.detail + (c.reward ? " (" + std::to_string(c.reward) + " pennies)" : "") + ".", c.id};
        }
    return {false, "That work is gone.", contractId};
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
                    const std::string& source, double confidence, const std::string& incident, const std::string& as)
{
    if (holder.empty() || subject.empty() || holder == subject || claim.empty() || !(confidence > .05))
        return;
    auto& mine = beliefs_[holder];
    for (auto& b : mine)
        if (b.subject == subject && b.claim == claim)
        {
            if (confidence > b.confidence)
                b = {holder, subject, claim, source, std::min(1.0, confidence), calendarDays_, incident, as.empty() ? b.as : as};
            else if (b.as.empty())
                b.as = as;
            return;
        }
    mine.push_back({holder, subject, claim, source, std::min(1.0, confidence), calendarDays_, incident, as});
    if (mine.size() > BeliefsKept)
        mine.erase(std::min_element(mine.begin(), mine.end(), [](const Belief& a, const Belief& b) { return a.confidence < b.confidence; }));
}

void World::forgetClaim(const std::string& claim)
{
    for (auto& [holder, mine] : beliefs_)
        mine.erase(std::remove_if(mine.begin(), mine.end(), [&](const Belief& b) { return b.claim == claim; }), mine.end());
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
            if (b.subject == subject && b.claim.rfind("deed:", 0) != 0)   // (Deeds are told in their own words: doc 56.)
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

void World::tendRoads()
{
    if (!townsReady_)
        setupTowns();
    // The road layer runs on the markets (doc 79, 5.1 and Phase 4), not the law's towns: one market still has its farm
    // carts; two or more, wagons and bandits between them.
    ensureRoadMarkets();
    if (roadMarkets_.empty())
        return;
    // A courier is done when the one who took it stands before the recipient.
    for (auto& c : roads_.contracts)
    {
        if (c.kind != "courier" || c.status != "taken" || c.taker.rfind("caravan:", 0) == 0)
            continue;
        const auto* carrier = entity(c.taker);
        const auto* to = entity(c.target);
        if (carrier && to && !to->dead && carrier->cellId == to->cellId && between(carrier->position, to->position) <= 3)
        {
            const auto paid = settleContract(c, "done", c.taker);
            bonds_.change(c.target, c.taker, {2, 3, 2, 0, 0}, calendarDays_);
            bonds_.change(c.poster, c.target, {1, 0, 2, 0, 0}, calendarDays_);
            bonds_.change(c.poster, c.taker, {1, 3, 1, 0, 0}, calendarDays_);
            if (!carrier->npc)
                notice(c.taker, "You hand " + to->name + " the letter" +
                                    (paid >= c.reward ? (paid ? " and are paid " + pennies(paid) : std::string()) + "."
                                     : paid ? " and are paid " + pennies(paid) + " of the " + pennies(c.reward) + ": " + DayPaid + ", and the rest goes back."
                                            : ". Its pay goes back: " + std::string(DayPaid) + "."));
        }
    }
    const auto today = std::int64_t(std::floor(calendarDays_));
    if (today != roads_.day)
    {
        roads_.day = today;
        roadsDaily();
        roadsMorning(today);
    }
    setOut();
    tendRoadFolk();
    absorbJournal();
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
            // Where it last was, else where it sets out from: its yard (or farm), or, homeward, the far market.
            std::string cellId = c.cell;
            Vec2 at{c.x, c.y};
            if (cellId.empty() || !cell(cellId))
            {
                cellId = homeward(c) ? c.endCell : c.startCell;
                at = homeward(c) ? Vec2{c.endX, c.endY} : Vec2{c.startX, c.startY};
                if (cellId.empty() || !cell(cellId))
                    continue;
                c.cell = cellId;
                c.x = at.x;
                c.y = at.y;
            }
            wagon = &addRoadFolk(id, caravanName(c), caravanLook(c), cellId, at, "caravan", c.id);
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
    // Set out only with its escorts and its carter alongside (or once it has waited long enough for them).
    if (c.status == "travelling" && c.leg == 0 && calendarDays_ < c.waitUntil &&
        (!std::all_of(c.escorts.begin(), c.escorts.end(), [&](const std::string& who) { return withCaravan(who, wagon); }) ||
         (!c.driver.empty() && entity(c.driver) && !withCaravan(c.driver, wagon))))
    {
        wagon.activity = c.manifest.empty() ? "waiting at the market for its escort"
                                            : c.kind == "cart" ? "being loaded" : "loading the wagon";
        return;
    }
    // A seeded mishap holds it where it happens (8.1: late, with a cause people talk about).
    if (c.status == "travelling" && c.delayAt > 0 && c.leg >= c.delayAt)
    {
        if (c.delayUntil <= 0)
        {
            const char* const causes[] = {"a broken axle", "mud to the axles", "a wheel off in a rut", "a washed-out ford"};
            c.lateCause = causes[roll(c.id, 9) % std::size(causes)];
            c.delayUntil = calendarDays_ + 1.6 + chance(c.id, 10) * .8;
            recordEvent({"wagon held up", c.id, c.to, wagon.cellId, 0, 0, c.departure, 0, 0, c.lateCause});
        }
        if (calendarDays_ < c.delayUntil)
        {
            wagon.activity = "held up by " + c.lateCause;
            stop(wagon.id);
            return;
        }
        c.delayAt = 0;
    }
    // Caravans wait out a storm (or a snowstorm, a sandstorm) where it finds them (Phase 9); within a cell of a waystation
    // they press on, sheltered on that stretch (doc 57, Phase 4). The days they wait are counted, for the town's proposals.
    const auto stationed = [&] {
        double best = 0;
        for (const auto& [cell, strength] : works_.waystations)
            if (cell == wagon.cellId || (exits_.count(cell) && exits_.at(cell).count(wagon.cellId)))
                best = std::max(best, strength);
        return best >= 1 || (best > 0 && std::int64_t(std::floor(calendarDays_)) % 2 == 0);
    };
    if (c.status != "arrived" && skyAt(wagon.cellId, wagon.position) == 2 && (works_.waystations.empty() || !stationed()))
    {
        stormWaits_[homeward(c) ? c.from : c.to][wagon.cellId].insert(std::int64_t(std::floor(calendarDays_)));
        if (c.lateCause.empty() && !homeward(c))
            c.lateCause = "the weather";
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
    // Where it is going: the far market's place, or, homeward, where it set out (staged goods: nothing to carry back).
    const std::string destCell = homeward(c) ? c.startCell : c.endCell;
    const Vec2 goal = homeward(c) ? Vec2{c.startX, c.startY} : Vec2{c.endX, c.endY};
    const auto lost = [&] {
        // No way on (a host closed the road): its letters open again at home for another courier, its escorts' work open
        // again for the next wagon (or their pay back, the market gone), its carter free, its notices down; it is done.
        for (auto& k : roads_.contracts)
        {
            if (k.kind == "courier" && k.status == "taken" && k.taker == "caravan:" + c.id)
                k.status = "open", k.taker.clear();
            else if (escortOn(k, c.from, c.to) && k.status == "taken" &&
                     std::find(c.escorts.begin(), c.escorts.end(), k.taker) != c.escorts.end())
            {
                if (roadMarket(k.town))
                    k.status = "open", k.taker.clear();
                else
                    settleContract(k, "expired", {});
            }
        }
        c.escorts.clear();
        c.status = "arrived";
        drivers_.erase(c.driver);
        recordEvent({"caravan lost", c.id, c.to, wagon.cellId, 0, 0, c.departure, 0, 0, "no road on from here"});
    };
    if (destCell.empty() || !cell(destCell))
        return lost();
    if (wagon.cellId == destCell && (between(wagon.position, goal) <= 2.0 || time_ - f.enteredAt > StuckSeconds))
        return caravanArrived(c);
    if (wagon.cellId != destCell && !cachedSteps(wagon.cellId).count(destCell))
        return lost();
    if (!wagon.offstage && time_ - f.enteredAt > StuckSeconds)
    {
        // Stuck where it walks (a crowd, a bad corner): it goes on to the next cell as if offstage.
        // Copies: ensureLoaded may rebuild the fixture index, clearing stepsCache_ and doorsIn_ under them.
        const std::string next = cachedSteps(wagon.cellId).at(destCell);
        const auto ways = doorsIn(wagon.cellId);
        for (const Door* way : ways)
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
    const std::string task = homeward(c) ? "going home to " + marketWords(c.from) : "on the road to " + marketWords(c.to);
    if (wagon.activity != task)
    {
        stop(wagon.id);
        wagon.activity = task;
    }
    headFor(wagon, task, destCell, goal, budget);
}

void World::caravanEntered(Caravan& c, const Entity& wagon)
{
    for (const auto& who : c.escorts)
        if (withCaravan(who, wagon))
            ++c.with[who];
    if (homeward(c) || c.kind == "cart")
        return;                                    // Empty on the way back, or a farm's cart on its own track: not robbed.
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
        const double odds = raidOdds(camp.strength, camp.hunger, guards, cellId);
        if (chance(c.id, std::int64_t(c.leg)) >= odds)
        {
            recordEvent({"caravan passes", c.id, camp.id, cellId, 0, 0, {}, 0, 0, "the guards kept the bandits off"});
            camp.strength = std::max(0.0, camp.strength - .2 * guards);
            for (const auto& id : watching)
                notice(id, "Figures watch the caravan from cover here, weigh up its guards, and let it pass.");
            break;
        }
        // Raided (doc 79, 8.1): the load is taken to the camp (staged goods, made there: recoverable), the guards give as
        // good as they can, the market it was bound for short of its goods at once, its carter robbed or hurt.
        int taken = 0;
        for (const auto& [g, n] : c.manifest)
            taken += n;
        raidCargo(c, camp);
        c.manifest.clear();
        camp.hunger = std::max(0.0, camp.hunger - taken * 4.0);
        // Its strength grows with each raid, within the world's cap (D14).
        const double strongest = std::clamp(double(society_.rule("campStrengthMost")), 2.0, 20.0);
        camp.strength = std::clamp(camp.strength + .5 - .3 * guards, 0.0, strongest);
        camp.lastRaid = calendarDays_;
        c.status = "raided";
        if (auto* w = entity(wagon.id))
            w->name = "a robbed wagon limping home to " + marketWords(c.from);
        recordEvent({"raid", camp.id, c.to, cellId, 0, 0, c.departure, taken, 0, "the wagon to " + marketWords(c.to) + " was robbed"});
        for (const auto& id : watching)
            notice(id, "Bandits burst from cover and fall on the wagon! In the struggle they make off with its load, "
                       "and the carters turn back for " + marketWords(c.from) + ".");
        // The market posts a bounty on the camp (20p and 3p a point of the camp's strength, at most the world's bountyMost:
        // D14), and asks for guards for the next wagon (12p); its treasury sets the rewards aside (staged-sourced).
        const bool bounty = std::any_of(roads_.contracts.begin(), roads_.contracts.end(), [&](const Contract& k) {
            return k.kind == "bounty" && k.target == camp.id && (k.status == "open" || k.status == "taken");
        });
        const auto payer = society_.treasuryOf(c.to);
        if (!bounty)
            postContract("bounty", payer, c.to, camp.id,
                         std::min<std::int64_t>(std::max(0, society_.rule("bountyMost")), 20 + std::int64_t(camp.strength * 3)), 20,
                         "the bandits robbing the road at " + cellId);
        const bool escort = std::any_of(roads_.contracts.begin(), roads_.contracts.end(), [&](const Contract& k) {
            return escortOn(k, c.from, c.to) && (k.status == "open" || k.status == "taken");
        });
        if (!escort)
            postContract("escort", payer, c.to, c.to, 12, 7,
                         "guarding the next wagon from " + marketWords(c.from) + " to " + marketWords(c.to), c.from);
        for (const auto& p : society_.positions())
            if (p.role == "merchant")
                if (const auto* m = roadMarketOf(p.work.cell); m && m->id == c.to)
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
        // Back where it set out: it is done, its carter back to his post.
        c.status = "arrived";
        drivers_.erase(c.driver);
        recordEvent({"caravan home", c.id, c.from, c.cell, 0, 0, c.departure, 0, 0, "back from " + c.to});
        return;
    }
    if (c.kind == "cart")
    {
        // A farm's cart in at its market (8.2): its goods on the shelves, its harvest in the price; one way.
        stagedArrival(c);
        c.status = "arrived";
        drivers_.erase(c.driver);
        recordEvent({"cart arrives", c.id, c.to, c.cell, 0, 0, c.from, int(c.manifest.size()), 0, "from " + c.from});
        return;
    }
    const auto* from = roadMarket(c.from);
    const auto* to = roadMarket(c.to);
    if (!from || !to)
    {
        c.status = "returning";
        return;
    }
    // Its goods onto the far market's shelves and into its prices, at this moment (doc 79, 7.4 and 8.6; staged: nothing is
    // bought or sold on the road).
    stagedArrival(c);
    // Escorts are paid for having been there: at the end, and for at least half the road.
    for (const auto& who : c.escorts)
        for (auto& k : roads_.contracts)
            if (escortOn(k, c.from, c.to) && k.taker == who && k.status == "taken")
            {
                const bool there = wagon && withCaravan(who, *wagon) && c.with[who] * 2 >= int(c.leg);
                const auto paid = settleContract(k, there ? "done" : "expired", there ? who : std::string());
                if (const auto* e = entity(who); e && !e->npc)
                    notice(who, !there ? "The wagon reached " + marketWords(c.to) + " without you, and the escort's pay went back."
                                : "The wagon reaches " + marketWords(c.to) + " safely. " +
                                      (paid >= k.reward ? "You are paid " + pennies(paid) + " for guarding it."
                                       : paid ? "You are paid " + pennies(paid) + " of the " + pennies(k.reward) + " for guarding it: " + DayPaid +
                                                    ", and the rest goes back."
                                              : "The pay for guarding it goes back: " + std::string(DayPaid) + "."));
                if (there)
                    for (const auto& p : society_.positions())
                        if (p.role == "merchant" && roadMarketOf(p.work.cell) == to)
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
    c.letters.clear();
    // The carters tell what they heard in the market they came from: more of it, and surer, when a player guarded the
    // road with them (doc 57, 5).
    bool guarded = false;
    for (const auto& who : c.escorts)
        if (const auto* e = entity(who); e && !e->npc && wagon && withCaravan(who, *wagon))
            guarded = true;
    std::vector<Belief> told;
    for (const auto& [holder, mine] : beliefs_)
        if (const auto* job = society_.jobOf(holder); job && job->role == "merchant" && roadMarketOf(job->work.cell) == from)
            for (const auto& b : mine)
                if (b.confidence >= .4)
                    told.push_back(b);
    for (const auto& p : society_.positions())
        if (p.role == "merchant" && roadMarketOf(p.work.cell) == to)
            if (const auto& holder = society_.state().careers.positions.at(p.id).holder; !holder.empty())
                for (std::size_t i = 0; i < told.size() && i < (guarded ? 16u : 8u); ++i)
                    believe(holder, told[i].subject, told[i].claim, "a carter from " + from->id, told[i].confidence * (guarded ? .75 : .6), {},
                            told[i].as);
    recordEvent({"caravan arrives", c.id, c.to, c.cell, 0, 0, c.departure, int(c.manifest.size()), 0, "from " + c.from + (guarded ? ", guarded" : "")});
    // Home again, empty.
    c.status = "returning";
    c.leg = 0;
    c.with.clear();
    c.escorts.clear();
    if (auto* w = entity("road:" + c.id))
    {
        w->name = "an empty wagon going home to " + marketWords(c.from);
        w->description = caravanLook(c);
    }
}

std::string World::caravanName(const Caravan& c) const
{
    if (c.kind == "cart")
        return "a farm cart bound for " + marketWords(c.to);
    if (c.status == "raided")
        return "a robbed wagon limping home to " + marketWords(c.from);
    if (homeward(c))
        return "an empty wagon going home to " + marketWords(c.from);
    return "a wagon bound for " + marketWords(c.to);
}

std::string World::caravanLook(const Caravan& c) const
{
    // What a player sees of it (T4): who drives it, its guard, and what it carries.
    std::string goods;
    for (std::size_t i = 0; i < c.manifest.size(); ++i)
        goods += (i == 0 ? "" : i + 1 == c.manifest.size() ? " and " : ", ") + goodWords(c.manifest[i].first);
    const auto* driver = entity(c.driver);
    std::string out = c.kind == "cart" ? "A farm cart in harness" : goods.empty() ? "An empty wagon in harness" : "A laden wagon in harness";
    out += driver ? ", " + driver->name + " at the traces." : ", its carters trotting alongside.";
    if (c.kind != "cart" && c.guards > 0)
    {
        const char* const counts[] = {"Two", "Three", "Four", "Five"};
        out += c.guards == 1 ? " A guard rides on the box beside the carter."
                             : std::string(" ") + counts[std::min(3, c.guards - 2)] + " guards ride with it.";
    }
    if (!goods.empty())
        out += " It carries " + goods + ".";
    return out;
}

Caravan* World::sendCaravan(const Town& from, const Town& to, const std::map<std::string, int>& load)
{
    // A wagon at once (tests, the Dev Console): between the road layer's markets of these ids where there are, else the
    // towns given; a free carter of the source's drives it if it has one. (Copies first: `from` and `to` may be the road
    // layer's own markets, which ensureRoadMarkets finds again.)
    const Town fromTown = from, toTown = to;
    ensureRoadMarkets();
    const auto* a = roadMarket(fromTown.id);
    const auto* b = roadMarket(toTown.id);
    Caravan c;
    c.id = "cv" + std::to_string(roads_.nextId++);
    c.from = fromTown.id;
    c.to = toTown.id;
    for (const auto& [item, n] : load)
        if (n > 0 && c.manifest.size() < 5)
            c.manifest.push_back({items::baseOf(item), std::min(n, 999)});
    for (const auto& who : cartersOf(fromTown.id))
        if (const auto* e = entity(who); e && !e->dead && !drivers_.count(who))
        {
            c.driver = who;
            break;
        }
    return launchCaravan(std::move(c), a ? *a : fromTown, b ? *b : toTown);
}

bool World::errand(const std::string& resident, const ResidentLife& life, std::string& task, std::string& reason,
                   std::string& goalCell, Vec2& goal) const
{
    // (Nobody seated, no occasion, no contract taken: no errand for anyone. Asked of every resident every pass.)
    if (seated_.empty() && occasions_.empty() && errandContracts_.empty() && drivers_.empty())
        return false;
    // A hungry traveller eats first (its provisions where it stands, else food bought or fetched), then goes on: the
    // errand never starves it.
    if (life.task == "eat" || ((life.task == "buy food" || life.task == "fetch food") && life.hunger >= 55))
        return false;
    // At a tavern game (doc 54, 5): in its seat until the game is done.
    if (const auto seat = seated_.find(resident); seat != seated_.end() && life.task != "sleep")
    {
        task = "playing";
        reason = seat->second.reason;
        goalCell = seat->second.cell;
        goal = {seat->second.x, seat->second.y};
        return true;
    }
    // An occasion it hosts (doc 55, 6): a wedding, a funeral, from a little before the hour to its end.
    for (const auto& o : occasions_)
        if (calendarDays_ >= o.start - 1. / 48 && calendarDays_ < o.end && std::find(o.hosts.begin(), o.hosts.end(), resident) != o.hosts.end())
        {
            const auto at = std::size_t(std::find(o.hosts.begin(), o.hosts.end(), resident) - o.hosts.begin());
            task = o.kind == "wedding" ? "at a wedding" : o.kind == "funeral" ? "at a funeral" : "at a gathering";
            reason = o.kind == "wedding" ? "Their wedding, at the church." : "Mourning, at the church.";
            goalCell = o.cell;
            goal = {o.x + double(at % 3) - 1, o.y + 1 + double(at / 3)};
            return true;
        }
    // A carter driving a wagon or a farm's cart (doc 79, 8.1-8.2; D26): with it, away from his post, until it is home
    // (a cart: at its market).
    if (const auto d = drivers_.find(resident); d != drivers_.end())
        for (const auto& c : roads_.caravans)
            if (c.id == d->second && c.status != "arrived")
            {
                const auto* wagon = entity("road:" + c.id);
                task = c.kind == "cart" ? "taking the cart to " + marketWords(c.to)
                       : homeward(c)    ? "driving the wagon home to " + marketWords(c.from)
                                        : "on the " + marketWords(c.to) + " run";
                reason = c.kind == "cart" ? "with the morning's load" : c.status == "raided" ? "robbed on the road" : "driving the wagon";
                if (wagon)
                {
                    goalCell = wagon->cellId;
                    goal = {wagon->position.x - 1, wagon->position.y};
                    if (!standable(goalCell, goal))
                        goal = wagon->position;
                }
                else
                {
                    goalCell = c.startCell;
                    goal = {c.startX, c.startY};
                }
                return !goalCell.empty();
            }
    static const std::vector<std::size_t> none;
    const auto contractsOf = errandContracts_.find(resident);
    for (const auto i : contractsOf != errandContracts_.end() ? contractsOf->second : none)
    {
        if (i >= roads_.contracts.size())
            continue;
        const auto& k = roads_.contracts[i];
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
            if (roadMarketOf(me->cellId) != roadMarketOf(to->cellId) && home && !home->homeCell.empty())
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
        if (k.kind == "escort")
            for (const auto& c : roads_.caravans)
                if (c.status == "travelling" && std::find(c.escorts.begin(), c.escorts.end(), resident) != c.escorts.end())
                {
                    const auto* wagon = entity("road:" + c.id);
                    if (!wagon || c.endCell.empty())
                        return false;
                    task = "guarding a caravan";
                    reason = "on the road to " + marketWords(c.to);
                    // Where the wagon is, while it waits; then the same road to the same market, alongside it.
                    if (c.leg == 0 && wagon->cellId != c.endCell)
                    {
                        goalCell = wagon->cellId;
                        goal = wagon->position;
                    }
                    else
                    {
                        goalCell = c.endCell;
                        goal = {c.endX, c.endY + 1};
                    }
                    return true;
                }
    }
    return false;
}

void World::indexErrands()
{
    drivers_.clear();
    for (const auto& c : roads_.caravans)
        if (!c.driver.empty() && c.status != "arrived")
            drivers_[c.driver] = c.id;
    errandContracts_.clear();
    for (std::size_t i = 0; i < roads_.contracts.size(); ++i)
        if (roads_.contracts[i].status == "taken")
            errandContracts_[roads_.contracts[i].taker].push_back(i);
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
        const auto* here = poster ? roadMarketOf(poster->cellId) : nullptr;
        if (!here)
            continue;
        // Of those free to go, one (by lot).
        std::string chosen;
        std::uint64_t best = ~0ULL;
        for (const auto& [id, life] : society_.state().residents)
        {
            const auto* e = entity(id);
            if (!e || e->dead || e->transient || e->age < 16 || id == k.poster || id == k.target || busy.count(id) || drivers_.count(id) ||
                society_.jobOf(id) || society_.apprenticedTo(id) || roadMarketOf(life.homeCell) != here)
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
}

void World::roadsDaily()
{
    const auto today = roads_.day;
    // (The wagons and carts set out from the committed departures since doc 79's Phase 4: roadsMorning, setOut.)
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
            recoverHoard(camp, society_.treasuryOf(society_.capital()), "found in an abandoned camp");   // (Doc 42: not lost to the world.)
        }
    }
    // (Camps gather a little more in hard times and in winter, a little less in a boom, never by players' wealth, and a
    // camp's strength is capped by the world's rule: D14.)
    const double strongest = std::clamp(double(society_.rule("campStrengthMost")), 2.0, 20.0);
    for (auto& camp : roads_.camps)
        camp.strength = std::min(camp.strength, strongest);
    if (today % 7 == 0)
        for (std::size_t r = 0; r < roadRoutes_.size(); ++r)
        {
            std::vector<std::string> wild;
            for (const auto& cellId : roadRoutes_[r])
                if (!marketIndex_.count(cellId))
                    wild.push_back(cellId);
            const bool held = std::any_of(roads_.camps.begin(), roads_.camps.end(), [&](const BanditCamp& b) {
                return b.active && std::find(wild.begin(), wild.end(), b.cell) != wild.end();
            });
            const auto* farTown = roadMarketOf(roadRoutes_[r].back());
            const std::string far = farTown ? farTown->id : std::string();
            if (held || wild.empty() || chance(far + std::to_string(r), today) > campGatherOdds(far))
                continue;
            const auto& cellId = wild[roll(far + std::to_string(r), today) % wild.size()];
            if (std::any_of(roads_.camps.begin(), roads_.camps.end(), [&](const BanditCamp& b) { return b.cell == cellId && b.active; }))
                continue;
            if (!campMayGather(cellId))
                continue;                           // (No camp gathers within two cells of a watch post: doc 57.)
            roads_.camps.erase(std::remove_if(roads_.camps.begin(), roads_.camps.end(),
                                              [&](const BanditCamp& b) { return b.cell == cellId; }),
                               roads_.camps.end());
            roads_.camps.push_back({"camp_" + cellId, cellId, std::min(strongest, 2.0 + (campHardness(far) <= -.5 ? 1 : 0)), 50, -100, true});
            recordEvent({"bandits gather", "camp_" + cellId, far, cellId, 0, 0, {}, 0, 0, "on the road to " + far});
        }

    // The watch goes after camps with a price on them: the more guards a town has, the better its chances.
    for (auto& k : roads_.contracts)
    {
        if (k.kind != "bounty" || (k.status != "open" && k.status != "taken"))
            continue;
        auto camp = std::find_if(roads_.camps.begin(), roads_.camps.end(), [&](const BanditCamp& b) { return b.id == k.target; });
        const auto* town = roadMarket(k.town);
        if (camp == roads_.camps.end() || !camp->active)
        {
            settleContract(k, "done", std::string());
            continue;
        }
        const double watch = !town ? 0 : std::min(10.0, town->guards / 3.0);
        if (watch > 0 && chance(k.id, today) < watch / (watch + camp->strength * 2))
        {
            camp->strength -= watch * .5;
            recordEvent({"patrol", k.town, camp->id, camp->cell, 0, 0, {}, 0, 0, "the watch went out after the bandits"});
            if (camp->strength <= .5)
            {
                camp->active = false;
                settleContract(k, "done", std::string());      // The watch did it: the reward goes back.
                if (town)
                    recoverHoard(*camp, society_.treasuryOf(town->id), "recovered from the bandits");
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

    // Letters: someone who cares for another in a different town may pay to have word carried (with 10p in its purse,
    // 3p to 7p for the carrying).
    const std::int64_t writes = 10;
    std::set<std::string> writing;
    // (Staged: an NPC-posted courier contract is a faucet, at most courierADay posted a day across the realm: doc 79, 9.3.)
    // (Counted once here, then as this loop posts: npcCourierRoom's count without its scan each time.)
    std::int64_t postedToday = npcCouriersToday();
    const std::int64_t mostToday = society_.statusTable().cap("courierADay");
    for (const auto& k : roads_.contracts)
        if (k.kind == "courier" && (k.status == "open" || k.status == "taken"))
            writing.insert(k.poster);
    for (const auto& [id, e] : entities_)
    {
        if (mostToday >= 0 && postedToday >= mostToday)
            break;
        const auto* mine = bonds_.of(id);
        const auto* purse = society_.account(id);
        const auto* here = roadMarketOf(e.cellId);
        if (!e.npc || e.dead || !mine || !purse || purse->cash < writes || writing.count(id) || !here)
            continue;
        for (const auto& [other, b] : *mine)
        {
            const auto* o = entity(other);
            const auto* there = o ? roadMarketOf(o->cellId) : nullptr;
            if (!o || !o->npc || o->dead || b.affinity < 40 || !there || there == here || chance(id + other, today) > .025)
                continue;
            postContract("courier", id, there->id, other, 3 + std::int64_t(roll(id, today) % 5), ContractDays,
                         "a letter from " + e.name + " to " + o->name + " in " + there->id);
            writing.insert(id);
            ++postedToday;
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
            for (const auto& w : watchersOver(id))       // (One keeping watch over it notices too: doc 53, 4.)
                if (const auto* we = entity(w); we && w != id)
                    for (const auto* b : standing)
                        noticed = std::max(noticed, noticeSenses(*we, we->position, we->facing, *b, b->position, false, false, 0, camp.cell).total());
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
        for (const auto& w : watchersOver(enc->player))  // (And past whoever keeps watch over the traveller: doc 53, 4.)
            if (const auto* we = entity(w); we && w != enc->player)
                for (auto* b : standing)
                    most = std::max(most, noticeSenses(*we, we->position, we->facing, *b, b->position, true,
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
    for (const auto& p : society_.positions())
        if (p.role == "merchant" && roadMarketOf(p.work.cell))
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
    // Raided cargo the watch or the weather finds (staged goods: back to the staged side, doc 79, 8.1).
    if (const auto* takings = society_.account(loot))
        for (const auto& [item, n] : std::map<std::string, int>(takings->stock.begin(), takings->stock.end()))
            if (n > 0)
                society_.consume(loot, item, n, "recovered cargo");
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
    // The cargo they took from the wagons (doc 79, 8.1: recoverable), for whoever breaks the camp. Staged goods to a
    // player: a reward at the door (9.3, counted there by their catalogue worth), so only what the day's rewards cap has
    // room for; the rest goes back to the staged side. Before the bounty, which is held to the room that is left.
    if (const auto* takings = society_.account(loot))
    {
        const bool capped = Society::classify(by) == Society::AccountKind::Real;
        std::vector<std::string> found;
        bool over = false;
        for (const auto& [item, n] : std::map<std::string, int>(takings->stock.begin(), takings->stock.end()))
        {
            if (n <= 0)
                continue;
            const auto each = std::max<std::int64_t>(1, Society::catalogPrice(item));
            const int take = capped ? int(std::min<std::int64_t>(n, society_.faucetRoom(by, "rewardsADay") / each)) : n;
            int got = 0;
            for (int left = take; left > 0; left -= 99)
                if (const int some = std::min(left, 99); society_.shift(loot, by, item, some, 0, "reward: recovered cargo"))
                {
                    found.push_back(std::to_string(some) + " " + Society::itemName(item));
                    got += some;
                }
            // (What isn't taken, over the cap or more kinds of thing than its finder can hold, goes back: nothing is left at
            // a camp that is gone, and its account closes.)
            if (got < n)
                over |= society_.consume(loot, item, n - got, "recovered cargo") > 0;
        }
        if (!found.empty())
        {
            std::string list;
            for (std::size_t i = 0; i < found.size(); ++i)
                list += (i == 0 ? "" : i + 1 == found.size() ? " and " : ", ") + found[i];
            words += " The cargo they took from the wagons is here too: " + list + ".";
        }
        if (over)
            words += found.empty() ? " The cargo they took from the wagons is here too, and goes back to its carters."
                                   : " The rest of it goes back to its carters.";
    }
    society_.closeAccount(loot, by);
    for (auto& k : roads_.contracts)
        if (k.kind == "bounty" && k.target == camp.id && (k.status == "open" || k.status == "taken"))
        {
            const auto paid = settleContract(k, "done", by);
            if (k.reward)
                words += paid >= k.reward ? " The bounty on them, " + pennies(paid) + ", is yours."
                         : paid ? " Of the bounty on them, " + pennies(k.reward) + ", you are paid " + pennies(paid) + ": " + DayPaid +
                                      ", and the rest goes back."
                                : " The bounty on them goes back to those who posted it: " + std::string(DayPaid) + ".";
        }
    const auto* who = entity(by);
    recordEvent({"camp cleared", by, camp.id, camp.cell, 0, 0, {}, 0, 0, "by " + (who ? who->name : by)});
    // (Word of it travels as a notable deed, from its witnesses and the town's talk: doc 56. Every merchant no longer
    // hears of it at once.)
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
                    telling.push_back({listeners[l].second, {listeners[l].second, b->subject, b->claim, holder, b->confidence * .7, calendarDays_, b->incident, b->as}});
    }
    for (auto& [holder, mine] : beliefs_)
    {
        for (auto& b : mine)
            b.confidence *= .97;
        mine.erase(std::remove_if(mine.begin(), mine.end(), [](const Belief& b) { return b.confidence < .1; }), mine.end());
    }
    for (const auto& [listener, b] : telling)
        believe(listener, b.subject, b.claim, b.source, b.confidence, b.incident, b.as);

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


double World::campHardness(const std::string& market) const
{
    // How hard the times are for the bandits' road (D14, 5.3): the slider, and for a road's far market its own Times' shift
    // from its harvests, raids and late wagons (never players' trade or wealth); -1 the hardest, 1 a boom.
    double score = society_.board().slider;
    if (const auto shift = society_.board().timesShift.find(market); !market.empty() && shift != society_.board().timesShift.end())
        score += shift->second;
    return std::clamp(score / 100., -1., 1.);
}

double World::campGatherOdds(const std::string& market) const
{
    // A road with no camp gathers one a week at these odds (D14): mildly more in hard times (x1.25 at -100, the realm's
    // slider and the far market's own Times together) and in winter (x1.1), mildly fewer in a boom (x0.8 at most), never
    // by players' wealth.
    const double slider = campHardness(market);
    const double hard = std::clamp(1 - .25 * slider, .8, 1.25) *
                        (calendar::calendarAt(calendarDays_).season == calendar::Season::Winter ? 1.1 : 1.);
    return .34 * hard;
}

double World::raidOdds(double strength, double hunger, int guards, const std::string& cellId) const
{
    // Hungrier and stronger bandits are bolder; guards keep them off, and a town's watch post in this cell or the next
    // counts two more at its strength (doc 57, Phase 4). (Doc 42, Phase 7: with traders' caravans on the roads too, a
    // camp wins less easily than it did.)
    const double bold = strength * (1 + hunger / 100);
    const double posted = 2 * watchedNear(cellId, 1);
    return bold / (bold + (guards + posted) * 8.0 + 20.0);
}

double World::watchedNear(const std::string& cellId, int within) const
{
    // The strongest of a town's watch posts within `within` cells (by the ways between them) of this one.
    if (works_.watchposts.empty())
        return 0;
    double best = 0;
    std::set<std::string> seen{cellId};
    std::vector<std::string> ring{cellId};
    for (int step = 0; step <= within && !ring.empty(); ++step)
    {
        std::vector<std::string> next;
        for (const auto& cell : ring)
        {
            if (const auto post = works_.watchposts.find(cell); post != works_.watchposts.end())
                best = std::max(best, post->second);
            if (const auto ways = exits_.find(cell); ways != exits_.end())
                for (const auto& to : ways->second)
                    if (seen.insert(to).second)
                        next.push_back(to);
        }
        ring = std::move(next);
    }
    return best;
}

int World::foulMarketDays(const std::string& community) const
{
    const auto found = foulMarkets_.find(community);
    if (found == foulMarkets_.end())
        return 0;
    const auto season = calendar::calendarAt(calendarDays_).season;
    int n = 0;
    for (const auto day : found->second)
        n += calendar::calendarAt(double(day)).season == season && calendarDays_ - double(day) < 92;
    return n;
}

std::map<std::string, int> World::stormWaits(const std::string& community) const
{
    std::map<std::string, int> out;
    const auto found = stormWaits_.find(community);
    if (found == stormWaits_.end())
        return out;
    for (const auto& [cell, days] : found->second)
        for (const auto day : days)
            if (calendarDays_ - double(day) < 92)
                ++out[cell];
    return out;
}

} // namespace ratw
