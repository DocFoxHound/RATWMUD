#include "RatwCalendar.h"
#include "RatwItems.h"
#include "RatwSociety.h"

#include <algorithm>
#include <functional>
#include <cmath>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <tuple>
#include <set>
#include <string_view>

// Daily routines for residents authored in a world file (Atlas Workshop).
// Everything here is driven by ResidentSpec data: roles, hours, places and
// patrol routes. Nothing names a particular world. The economy's rules that
// once ran through their days (wages, shopping, odd jobs, alms, larders,
// production) were erased in Docs/Design/66-worlds-atlas-and-dm.md, Phase 5.
namespace ratw
{
using namespace std::string_view_literals;
namespace
{
bool near(const LifeBody& body, const Spot& spot)
{
    return body.cell == spot.cell && std::hypot(body.x - spot.x, body.y - spot.y) <= 1.2;
}
bool validNumber(double value, double low, double high)
{
    return std::isfinite(value) && value >= low && value <= high;
}
// Half-open hour window that may wrap past midnight (18 -> 6).
bool within(double hour, double start, double end)
{
    return start <= end ? hour >= start && hour < end : hour >= start || hour < end;
}
// A place to work or rest: a cell (its name, kept where it is) and a point on it, to the millionth of a tile.
struct Station
{
    const std::string* cell;
    std::int64_t x, y;
    bool operator==(const Station& other) const { return x == other.x && y == other.y && *cell == *other.cell; }
};
struct StationHash
{
    std::size_t operator()(const Station& s) const
    {
        return std::hash<std::string>{}(*s.cell) ^ (std::size_t(s.x) * 0x9e3779b97f4a7c15ULL) ^ (std::size_t(s.y) * 0xc2b2ae3d27d4eb4fULL);
    }
};
} // namespace

void Society::resetAuthored()
{
    const auto& e = authored_.economy;
    auto& treasury = state_.accounts.at("treasury");
    state_.minted += e.treasury - treasury.cash;
    treasury = {e.treasury, {{"herbs", e.storeHerbs}, {"meal", e.storeMeals}}};
    for (const auto& r : authored_.residents)
    {
        EconomyAccount account;
        account.cash = r.purse;
        if (r.herbs > 0)
            account.stock["herbs"] = r.herbs;
        if (r.meals > 0)
            account.stock["meal"] = r.meals;
        state_.accounts[r.id] = account;
        if (smith(r.id))
            state_.accounts[r.id].stock["sword"] = SmithSwords;     // A smith's own work, for sale.
        if (r.role == "merchant")
            for (const auto& ware : wares(r.id))                      // A shop's own goods on its shelves (Docs/Design/39).
                if (ware != "meal" && ware != "herbs" && ware != "sword")
                    state_.accounts[r.id].stock[ware] = GoodsKept;
        state_.minted += account.cash;
        ResidentLife life;
        life.role = r.role;
        life.homeCell = r.home.cell;
        life.homeX = r.home.x;
        life.homeY = r.home.y;
        state_.residents[r.id] = life;
    }
    record("initial funding", "outside", "settlement", "", 0, state_.minted);
    defaultCareers();
    foundShops();                                   // Every shop's goods and money its own till's.
    for (const auto& market : items::cityLoop().markets)
        openAccount(marketFund(market.id));         // What the store room is paid: the Country's purse (doc 68), empty.
}

bool Society::adoptResident(const Society& from, const std::string& id)
{
    ++rosterRevision_;
    if (roster_ != Roster::Authored || from.roster_ != Roster::Authored)
        return false;
    const ResidentSpec* incoming = from.spec(id);
    const auto current = std::find_if(authored_.residents.begin(), authored_.residents.end(),
                                      [&](const ResidentSpec& r) { return r.id == id; });
    // Unplaced (doc 66, 2): kept aside as the candidate has them, a body or not. One given a home joins below as a
    // newcomer; one whose home is taken away leaves the society below, as one who left the world (their coins go back
    // to the town: an unplaced NPC holds none).
    const ResidentSpec* away = from.unplaced(id);
    const auto parked = std::find_if(authored_.unplaced.begin(), authored_.unplaced.end(),
                                     [&](const ResidentSpec& r) { return r.id == id; });
    const bool wasAway = parked != authored_.unplaced.end();
    if (wasAway)
        authored_.unplaced.erase(parked);
    if (away)
        authored_.unplaced.push_back(*away);
    if (!incoming && current == authored_.residents.end())
        return away || wasAway;
    authored_.routes = from.authored_.routes;
    if (!incoming)
    {
        authored_.residents.erase(current);
        forgetSpecs();
        buildPositions();
        reconcileCareers();
        auto account = state_.accounts.find(id);
        std::int64_t coins = 0;
        if (account != state_.accounts.end())
        {
            auto& treasury = state_.accounts.at("treasury");
            coins = account->second.cash;
            treasury.cash += coins;
            for (const auto& [item, count] : account->second.stock)
                treasury.stock[item] = std::min(10000, treasury.stock[item] + count);
            state_.accounts.erase(account);
        }
        state_.residents.erase(id);
        record("resident left", id, "treasury", "", 0, coins);
        return true;
    }
    if (current == authored_.residents.end())
    {
        authored_.residents.push_back(*incoming);
        forgetSpecs();
        buildPositions();
        const auto* account = from.account(id);
        const auto* life = from.resident(id);
        state_.accounts[id] = account ? *account : EconomyAccount{};
        state_.residents[id] = life ? *life : ResidentLife{};
        state_.minted += state_.accounts[id].cash;
        record("new resident", "outside", id, "", 0, state_.accounts[id].cash);
        reconcileCareers();                        // The newcomer holds the job they were written with.
        return true;
    }
    *current = *incoming;
    buildPositions();
    auto& life = state_.residents[id];
    life.role = incoming->role;
    life.homeCell = incoming->home.cell;
    life.homeX = incoming->home.x;
    life.homeY = incoming->home.y;
    life.relocationCell.clear();
    life.goalCell.clear();
    life.task = "idle";
    life.progress = 0;
    return true;
}

bool Society::addResident(const ResidentSpec& r, std::int64_t purse)
{
    if (roster_ != Roster::Authored || r.id.empty() || spec(r.id) || state_.accounts.count(r.id) || purse < 0)
        return false;
    ++rosterRevision_;
    authored_.residents.push_back(r);
    forgetSpecs();
    buildPositions();
    state_.accounts[r.id] = EconomyAccount{purse, {}};
    ResidentLife life;
    life.role = r.role;
    life.homeCell = r.home.cell;
    life.homeX = r.home.x;
    life.homeY = r.home.y;
    state_.residents[r.id] = life;
    state_.minted += purse;
    record("new resident", "outside", r.id, "", 0, purse);
    reconcileCareers();
    return true;
}

void Society::adoptLayers(const Society& from)
{
    if (roster_ != Roster::Authored || from.roster_ != Roster::Authored)
        return;
    authored_.routes = from.authored_.routes;
    for (auto& r : authored_.residents)
        if (const auto* incoming = from.spec(r.id))
        {
            r.wander = incoming->wander;
            r.route = incoming->route;
        }
    buildPositions();                              // Routes are part of the jobs.
}

void Society::refreshRecords()
{
    indexCareers();                                 // (Either may move the revision on.)
    (void)spec(std::string());
    if (recordsOf_ != this || recordsRevision_ != rosterRevision_ || records_.size() != state_.residents.size())
    {
        records_.clear();
        recordOf_.clear();
        records_.reserve(state_.residents.size());
        shopkeeping_.clear();
        for (const auto& p : positions_)
            if (p.role == "merchant")
                if (const auto held = state_.careers.positions.find(p.id); held != state_.careers.positions.end())
                    shopkeeping_.push_back({&p, &held->second.holder});
        for (auto& entry : state_.residents)
        {
            ResidentRecord r;
            r.entry = &entry;
            r.spec = spec(entry.first);
            const auto wallet = state_.accounts.find(entry.first);
            r.wallet = wallet == state_.accounts.end() ? nullptr : &wallet->second;
            r.job = jobOf(entry.first);
            r.learning = apprenticedTo(entry.first);
            r.skill = r.job ? skillSlot(entry.first, r.job->id, false) : nullptr;
            if (const auto bed = beds_.find(entry.first); bed != beds_.end())
                r.bed = &bed->second;
            r.home = "\n";                          // (Its community, below.)
            recordOf_.emplace(entry.first, records_.size());
            records_.push_back(std::move(r));
        }
        recordsRevision_ = rosterRevision_;
        recordsOf_ = this;
    }
    // A home's community: again when the home changes.
    for (auto& r : records_)
    {
        const auto& home = r.entry->second.homeCell;
        if (r.home != home)
        {
            r.home = home;
            r.community = day_.communityOf ? day_.communityOf(home) : std::string();
            r.planned = 0;                          // (Its plan again: decideAuthored.)
        }
    }
}


void Society::decideAuthored(double absoluteDay, const std::map<std::string, LifeBody>& bodies)
{
    const double hour = (absoluteDay - std::floor(absoluteDay)) * 24.;
    const bool night = hour < 6 || hour >= 22;
    const auto slot = std::int64_t(std::floor(absoluteDay * 96)); // Patrols move post every 15 game minutes.
    const auto today = std::int64_t(std::floor(absoluteDay));
    if (bodies.empty())
        return;                                     // (A second with no one due: see needsBodies.)
    if (groupsDay_ != today)
    {
        groupChildren(bodies);                      // Each town's children in their friend groups, for the day.
        groupsDay_ = today;
    }
    // The day's plan for a place's community, and its sky (Phase 9): an ordinary day under a fair sky without them.
    static const DayPlan ordinary;
    // A cell's community and its plan, asked of the same few hundred cells over and over in a pass: kept for the pass.
    struct Place
    {
        std::string community;
        const DayPlan* plan;
    };
    const auto makePlace = [&](const std::string& cell) {
        Place p{day_.communityOf ? day_.communityOf(cell) : std::string(), &ordinary};
        if (day_.communityOf)
            if (const auto plan = day_.plans.find(p.community); plan != day_.plans.end())
                p.plan = &plan->second;
        return p;
    };
    std::unordered_map<std::string, Place> places;
    bool sharing = false;                           // While the threads decide (below): the places are only read.
    const auto place = [&](const std::string& cell) -> const Place& {
        auto found = places.find(cell);
        if (found == places.end())
        {
            if (sharing)
            {
                thread_local Place spare;           // (Not kept: worked out for this asking.)
                spare = makePlace(cell);
                return spare;
            }
            found = places.emplace(cell, makePlace(cell)).first;
        }
        return found->second;
    };
    const auto planFor = [&](const std::string& cell) -> const DayPlan& { return *place(cell).plan; };
    // The plan at a spot: its community's, or in a city of several markets, the nearest market's (doc 60).
    const auto marketPlan = [&](const std::string& cell, double x, double y) -> const DayPlan* {
        if (day_.markets)
            if (const auto* key = day_.markets->nearest(cell, x, y))
                if (const auto plan = day_.plans.find(*key); plan != day_.plans.end())
                    return &plan->second;
        return nullptr;
    };
    const auto sky = [&](const Spot& at) { return day_.sky && !at.cell.empty() ? day_.sky(at) : 0; };
    const auto pick = [](const std::vector<Spot>& spots, const std::string& id) -> const Spot* {
        return spots.empty() ? nullptr : &spots[std::hash<std::string>{}(id) % spots.size()];
    };
    // Where a merchant trades now: a stall on the square on Marketday mornings (unless the weather is foul), else the
    // shop. Only a city has stalls built (Docs/Design/39), and its own stallholders keep them every day; a town has
    // none, and no market day. (Asked before the threads decide: places are kept as they're first asked.)
    const auto tradingAt = [&](const Position& p, const std::string& holder) -> Spot {
        const auto* nearest = marketPlan(p.work.cell, p.work.x, p.work.y);
        const auto& plan = nearest ? *nearest : planFor(p.work.cell);
        if (plan.kind == "market"sv && !plan.foul && hour >= 7 && hour < 14)
            if (const auto* stall = pick(plan.stalls, holder))
                return *stall;
        return p.work;
    };
    atStall_.clear();
    // Work spread over the threads where there are some (setParallel), `share` at a time; the same, alone, without.
    const auto spread = [&](std::size_t count, std::size_t share, const std::function<void(std::size_t)>& job) {
        if (parallel_ && count > share)
            parallel_((count + share - 1) / share, [&](std::size_t s) {
                for (std::size_t i = s * share; i < std::min(count, (s + 1) * share); ++i)
                    job(i);
            });
        else
            for (std::size_t i = 0; i < count; ++i)
                job(i);
    };
    // What the threads read that is otherwise worked out on first asking.
    indexCareers();
    (void)spec(std::string());
    refreshRecords();                               // (Each resident by number: see ResidentRecord.)
    // The day's plan at a resident's home: its community's (by its record), or in a city of several markets, the nearest
    // market's. Kept by the record for the day's plans and that home, made here before the threads.
    const auto planAt = [&](const ResidentRecord& rec) {
        const auto& life = rec.entry->second;
        const DayPlan* plan = &ordinary;
        if (day_.communityOf)
            if (const auto found = day_.plans.find(rec.community); found != day_.plans.end())
                plan = &found->second;
        if (const auto* nearest = marketPlan(life.homeCell, life.homeX, life.homeY))
            plan = nearest;
        return plan;
    };
    const auto planned = [&](const ResidentRecord& rec) {
        const auto& life = rec.entry->second;
        return rec.planned == dayRevision_ && rec.planX == life.homeX && rec.planY == life.homeY && rec.home == life.homeCell;
    };
    for (auto& r : records_)
        if (!planned(r))
        {
            r.plan = planAt(r);
            r.planned = dayRevision_;
            r.planX = r.entry->second.homeX;
            r.planY = r.entry->second.homeY;
        }
    // Each one's body, by number: the bodies walked alongside the residents (both in ID order).
    std::vector<const LifeBody*> bodyOf(records_.size(), nullptr);
    {
        auto body = bodies.begin();
        for (std::size_t i = 0; i < records_.size(); ++i)
        {
            const auto& id = records_[i].entry->first;
            while (body != bodies.end() && body->first < id)
                ++body;
            if (body != bodies.end() && body->first == id)
                bodyOf[i] = &body->second;
        }
    }
    // Each shop's keeper: where it trades now (worked out here, before the threads), and whether it is there keeping it.
    // A shop is open to players while its keeper keeps it; one keeping a stall sells at the stall's price (quote).
    for (const auto& shop : shopkeeping_)
    {
        if (shop.holder->empty())
            continue;                               // An empty shop is shut.
        const auto number = recordOf_.find(*shop.holder);
        const auto* body = number == recordOf_.end() ? nullptr : bodyOf[number->second];
        const auto* life = number == recordOf_.end() ? nullptr : &records_[number->second].entry->second;
        const auto at = tradingAt(*shop.position, *shop.holder);
        const bool stall = at.cell != shop.position->work.cell || at.x != shop.position->work.x || at.y != shop.position->work.y;
        if (stall && body && !body->companion && life && life->task == "trade"sv && near(*body, at))
            atStall_.insert(*shop.holder);
    }
    // What each resident does this second comes in two halves (Docs/Design/31-responsiveness.md, "Fast-forward"): what
    // it decides to do, and doing it. Everyone decides at once, from where things stood as the pass began, spread over
    // the pool's threads (nothing in deciding changes anything shared); then, in ID order, each does what it decided.
    struct Choice
    {
        bool skip = true;                           // No spec or no body: nothing this second.
        int step = 1;                               // The seconds this decision stands for (UnseenStep for the unseen).
        ResidentRecord* record = nullptr;
        const ResidentSpec* r = nullptr;
        const LifeBody* body = nullptr;
        double hunger = 0, fatigue = 0;             // As they will be this second.
        // The job it works today (`stand`: one made up for it, when it holds none), and the trade it learns.
        Position stand;
        const Position* job = nullptr;
        const Position* learning = nullptr;
        bool guard = false, merchantRole = false, onHours = false;
        bool walksRoute = false;                    // A patrol or a traveller: counted among its route's walkers.
        bool routeGoal = false;                     // Its goal is a post on its route, by its place among the walkers.
        bool companion = false, arrivedHome = false;
        std::string task, reason;
        Spot goal;
    };
    std::vector<std::pair<const std::string, ResidentLife>*> order;
    // (Kept from second to second, its strings' room with it; never resized during a pass: a choice's job may be its own
    // stand.)
    thread_local std::vector<Choice> kept;
    auto& choices = kept;
    choices.resize(state_.residents.size());
    order.reserve(choices.size());
    for (auto& r : records_)
        order.push_back(r.entry);

    const bool paused = needsPaused();
    // The first half: what a resident decides. Nothing is changed.
    const auto decide = [&](std::pair<const std::string, ResidentLife>& pair, Choice& c) {
        const auto& id = pair.first;
        const auto& life = pair.second;
        const auto* r = c.r;
        const auto& body = *c.body;
        const auto& rec = *c.record;
        const DayPlan& plan = *(planned(rec) ? rec.plan : planAt(rec));
        // Needs (doc 68: the city's loop feeds them; needsPaused holds hunger where it is); tiredness as ever.
        c.hunger = paused ? life.hunger : std::min(100., life.hunger + .0035 * c.step);
        c.fatigue = std::min(100., life.fatigue + .0025 * c.step);
        const double hunger = c.hunger, fatigue = c.fatigue;
        // The job comes from the position this resident holds (see Position). With none: an apprentice works beside its
        // master in the master's hours; a child grows up; one past its retiring age has retired; anyone else is out of
        // work, and spends its days where it spends its evenings.
        const Position* job = rec.job;
        const Position* learning = rec.learning;
        c.learning = learning;
        Position& stand = c.stand;                  // (Made afresh wherever it is made: unread otherwise.)
        if (!job)
        {
            stand = Position{};
            stand.role = "civilian";
            stand.paid = false;
            stand.startHour = learning ? learning->startHour : r->startHour;
            stand.endHour = learning ? learning->endHour : r->endHour;
            stand.title = learning ? "apprenticed: " + learning->title
                          : body.age < 16 ? "growing up"
                          : body.age >= retireAge(id) ? "retired"
                                                      : "looking for work";
            stand.work = learning ? learning->work : r->evening;
            job = &stand;
        }
        c.job = job;
        const bool guard = job->role == "guard"sv, merchantRole = job->role == "merchant"sv;
        c.guard = guard;
        c.merchantRole = merchantRole;
        const auto route = authored_.routes.find(job->route);
        const bool patrols = guard && route != authored_.routes.end() && !route->second.posts.empty();
        // A civilian or merchant with a route is a traveller: they walk it through their working hours and, being
        // on the road, rest wherever the day leaves them rather than going home.
        const bool travels = !guard && route != authored_.routes.end() && !route->second.posts.empty();
        c.walksRoute = patrols || travels;          // (Its place among the route's walkers is counted in its turn.)
        if (body.companion)
        {
            c.companion = true;                     // Off its own day, with whoever keeps it (a Hold, a party).
            c.task.clear();
            c.skip = false;
            return;
        }
        const bool carriesFood = rec.wallet && hasFood(*rec.wallet);
        // The day and the sky (Phase 9). The watch keeps its hours whatever the day and the weather.
        const int workSky = sky(job->work);
        const bool festival = plan.kind == "festival"sv && hour >= 12 && hour < 23;
        double endHour = job->endHour;
        if (!guard && workSky == 1)
            endHour -= 2;                           // Rain: outdoor work ends early.
        bool onHours = within(hour, job->startHour, endHour);
        std::string resting;                        // Why a working day isn't one.
        if (!guard)
        {
            if (workSky == 2)
                resting = "Foul weather: outdoor work has stopped.";
            else if (festival)
                resting = "Work stops for " + plan.name + ".";
            // Restday: no work, but the shops open for a few hours after the service.
            else if (plan.kind == "rest"sv && !(merchantRole && hour >= ServiceEnd && hour < ServiceEnd + 4))
                resting = "Restday: no work today.";
            if (!resting.empty())
                onHours = false;
        }
        c.onHours = onHours;
        const bool onDuty = guard && onHours;
        auto& task = c.task;
        auto& reason = c.reason;
        auto& goal = c.goal;
        task.clear();
        reason.clear();
        goal = {life.homeCell, life.homeX, life.homeY};
        if (const auto* bed = rec.bed; bed && bed->cell == life.homeCell)
            goal = *bed;                            // Their place on a bed at home (doc 36).
        // Marketday: each of the townsfolk goes for an hour, some time between eight and one.
        const auto visit = [&] { return 8 + int(std::hash<std::string>{}(id + "market") % 5); };
        const bool marketHour = plan.kind == "market"sv && !plan.foul && !plan.crowd.empty() && !guard && !merchantRole &&
                                hour >= visit() && hour < visit() + 1;
        c.routeGoal = false;
        c.arrivedHome = false;
        if (!life.relocationCell.empty())
        {
            task = "relocate";
            goal = {life.relocationCell, life.relocationX, life.relocationY};
            reason = "Travelling to an operator-approved new home; arrival is not instantaneous.";
            if (body.cell == goal.cell && std::hypot(body.x - goal.x, body.y - goal.y) <= .35)
            {
                c.arrivedHome = true;               // (Moved in, in its turn.)
                reason = "Arrived at the new home.";
            }
        }
        else if (hunger >= 60 && carriesFood)
        {
            task = "eat";
            const bool continuing = life.task == "eat"sv && life.goalCell == body.cell;
            goal = {body.cell, continuing ? life.goalX : body.x, continuing ? life.goalY : body.y};
            reason = "Hungry; carrying something to eat.";
        }
        else if (onDuty)
        {
            task = patrols ? "patrol" : "watch";
            goal = job->work;
            c.routeGoal = patrols;
            reason = "On watch for the town.";
        }
        else if (plan.kind == "rest"sv && !plan.pews.empty() && hour >= ServiceStart && hour < ServiceEnd && !guard &&
                 (clergy(job->title) || goesToChurch(id, today)))
        {
            // Restday's service: the clergy at the pulpit, a third of the town on the benches.
            if (clergy(job->title))
            {
                task = "preaching";
                goal = plan.pulpit;
                reason = "Restday: preaching to the town.";
            }
            else
            {
                task = "at church";
                goal = *pick(plan.pews, id);
                reason = "Restday: at the morning service.";
            }
        }
        else if (merchantRole && onHours)
        {
            task = "trade";
            goal = tradingAt(*job, id);
            reason = goal.cell != job->work.cell || goal.x != job->work.x || goal.y != job->work.y
                         ? plan.kind == "market"sv ? "Marketday: trading from a stall at the market."
                                                   : "A morning at a stall in the market square."
                         : "Keeping shop.";
        }
        else if (festival && !night && fatigue < 80 && !(guard && within(hour, job->startHour, job->endHour)))
        {
            // The town gathers at its market to eat and talk into the night; the watch on duty stays at it.
            if (plan.foul || plan.crowd.empty())
            {
                task = "at home";
                reason = plan.name + ": the weather keeps everyone indoors.";
            }
            else
            {
                task = "festival";
                goal = *pick(plan.crowd, id);
                reason = plan.name + ": the town gathers at the market.";
            }
        }
        else if (marketHour)
        {
            task = "at the market";
            goal = *pick(plan.crowd, id);
            reason = "Marketday: gossiping at the stalls.";
        }
        else if (guard)
        {
            task = "sleep";
            reason = "Off watch; resting.";
        }
        else if (night || fatigue >= 80 || (life.task == "sleep"sv && fatigue > 15 && hour < 8))
        {
            task = "sleep";
            reason = "Resting at home for the night.";
        }
        else if (merchantRole)
        {
            task = "at home";
            goal = r->evening;
            reason = "The shop is shut.";
        }
        else if (onHours && travels)
        {
            task = job->title;
            c.routeGoal = true;
            reason = "On the road; their business keeps them travelling.";
        }
        else if (onHours)
        {
            task = job->title;
            goal = job->work;
            reason = job->title == "retired"sv            ? "Retired; spending the day in familiar company."
                     : job->title == "looking for work"sv ? "Out of work; spending the day in familiar company."
                     : job == &stand                      ? "Spending the day in familiar company."
                                                          : "At work.";
        }
        else if (!resting.empty() && within(hour, job->startHour, job->endHour))
        {
            // A day off: with friends where the evenings are spent, unless the sky sends everyone home.
            task = workSky == 2 || sky(r->evening) == 2 ? "sheltering" : "resting";
            if (task == "resting"sv)
                goal = sky(r->evening) >= 1 ? Spot{life.homeCell, life.homeX, life.homeY} : r->evening;
            reason = resting;
        }
        else if (within(hour, job->startHour, job->endHour) && workSky == 1)
        {
            task = "at home";
            reason = "Rain ended the outdoor work early.";
        }
        else if (within(hour, job->endHour, 22))
        {
            task = "socialize";
            goal = r->evening;
            reason = "Work is done; spending the evening in company.";
            if (const int evening = sky(r->evening); evening >= 1)
            {
                task = "at home";
                goal = {life.homeCell, life.homeX, life.homeY};
                reason = evening == 2 ? "Foul weather keeps the evening indoors." : "Rain keeps the evening indoors.";
            }
        }
        else
        {
            task = "morning at home";
            reason = "Waking slowly before the working day.";
        }
        if (travels && !(onHours && task == job->title) && task != "eat"sv && task != "relocate"sv && task != "festival"sv &&
            task != "at the market"sv && task != "at church"sv && task != "preaching"sv)
        {
            // Off the road's hours: stay put, camped or lodged where the day ended.
            goal = {body.cell, std::floor(body.x) + .5, std::floor(body.y) + .5};
            c.routeGoal = false;
            if (task == "sleep"sv)
                reason = "Resting on the road; home is far behind.";
        }
        // Children keep together (the user, 2026-10-05): at play and in the evenings a friend group meets at one place.
        if (body.age < 16 && !learning && !guard && !merchantRole && (task == job->title || task == "socialize"sv))
            if (const auto& group = friendGroup(id); !group.empty())
                if (const auto* meet = pick(plan.crowd, group))
                {
                    goal = *meet;
                    c.routeGoal = false;
                    reason = task == "socialize"sv ? "Out with friends for the evening." : "Playing with friends.";
                }
        // A civilian with a wander area roams it during work hours and evenings, moving on every half hour.
        if (!r->wander.empty() && !guard && !merchantRole && (task == job->title || task == "socialize"sv))
        {
            const auto turn = std::size_t(slot / 2) + std::hash<std::string>{}(r->id);
            goal = r->wander[turn % r->wander.size()];
            c.routeGoal = false;
            reason = task == "socialize"sv ? "Free time; roaming familiar ground." : "Roaming the area they keep to.";
        }
        c.skip = false;
    };

    // Everyone at once: over the threads where there are some (setParallel), a share of the residents each.
    // Who it is and where its body is (a resident with neither spec nor body does nothing this second).
    const auto atOnce = [&](std::size_t i) {
        auto& c = choices[i];
        c.skip = true;
        c.companion = false;
        c.body = nullptr;
        c.record = &records_[i];
        c.r = c.record->spec;
        const auto* body = bodyOf[i];
        if (!c.r || !body || body->cell.empty() || body->cell.size() > 80 || !validNumber(body->x, 0, 256) || !validNumber(body->y, 0, 256))
            return;
        c.body = body;
        if (body->unseen)
        {
            // Out of sight: only on the unseen's second, and then for every second since it last decided.
            if (!unseenDecided_)
                return;
            const double since = order[i]->second.decidedAt < 0 ? 1
                                 : std::round((absoluteDay - order[i]->second.decidedAt) * calendar::SecondsPerDay);
            c.step = int(std::clamp(since, 1., double(unseenStep_)));
        }
        else
            c.step = 1;
        decide(*order[i], c);
    };
    // (Each record's plan and every work cell's place are known by now: the threads only read them.)
    for (const auto& r : records_)
        if (const auto* job = r.job ? r.job : r.learning)
            place(job->work.cell);
    sharing = true;
    spread(order.size(), 64, atOnce);
    sharing = false;

    // The second half, in ID order: each does what it decided.
    std::unordered_set<Station, StationHash> reservations;
    reservations.reserve(order.size());
    std::map<std::string, int> routeWalkers;
    for (std::size_t i = 0; i < order.size(); ++i)
    {
        auto& pair = *order[i];
        auto& c = choices[i];
        if (c.skip)
            continue;
        const auto& id = pair.first;
        auto& life = pair.second;
        const auto& body = *c.body;
        life.hunger = c.hunger;
        life.fatigue = c.fatigue;
        life.decidedAt = absoluteDay;
        const int step = c.step;
        const Position* job = c.job;
        const Position* learning = c.learning;
        const bool onHours = c.onHours;
        const int walker = c.walksRoute ? routeWalkers[job->route]++ : 0;
        if (c.companion)
        {
            life.task = "companion";                // (Its work and its walking are the game's: a Hold's, a party's.)
            life.goalCell.clear();
            life.reason = "Ordinary work is suspended while recruited.";
            life.progress = 0;
            // (A recruited keeper's shop still sets out its wares once a market day in its hours (doc 79, 7.7): the
            // households it feeds, its food shop's, don't go short while it's away.)
            if (job->role == "merchant"sv && staged() && within(hour, job->startHour, job->endHour))
                stagedRestock(id);
            continue;
        }
        const auto& task = c.task;
        auto& goal = c.goal;
        if (c.routeGoal)
        {
            const auto& posts = authored_.routes.at(job->route).posts;
            goal = posts[std::size_t(slot + walker * 3) % posts.size()];
        }
        if (c.arrivedHome)
        {
            life.homeCell = goal.cell;
            life.homeX = goal.x;
            life.homeY = goal.y;
            life.relocationCell.clear();
            life.relocationX = life.relocationY = 0;
        }
        if (task != life.task || goal.cell != life.goalCell || goal.x != life.goalX || goal.y != life.goalY)
            life.progress = 0;
        life.task = task;
        life.reason = c.reason;
        life.goalCell = goal.cell;
        life.goalX = goal.x;
        life.goalY = goal.y;
        if (!near(body, goal))
        {
            life.progress = 0;
            continue;
        }
        // At its post in its hours: working at it (nothing is made or paid for it since doc 66's Phase 5).
        const bool atWork = onHours && job != &c.stand &&
                            (task == job->title || task == "trade"sv || task == "watch"sv || task == "patrol"sv);
        // Working at their post in its hours, they grow more skilled at it; an apprentice beside a master who is
        // there too learns faster. Nothing else depends on skill yet but who succeeds whom.
        if (atWork)
        {
            // (Its skill where the record keeps it: practise without the lookup.)
            auto& record = *c.record;
            double* at = job != record.job ? skillSlot(id, job->id, true)
                                           : record.skill ? record.skill : (record.skill = skillSlot(id, job->id, true));
            // (A year's: a shop's or farm's head short of its trade's level LearnHead, else LearnAlone; never past
            // SkillTop, nor back from where it is.)
            const bool head = (job->role == "merchant"sv || items::producerFor(job->title)) && *at < job->level;
            const double year = calendar::DaysPerYear * PaidSpells * 600.;
            if (*at < SkillTop)
                *at = std::min(SkillTop, *at + (head ? LearnHead : LearnAlone) * step / year);
            // The store room's restocking rule (Docs/Design/67-ridgemere2-city.md, 6; off until the user confirms it,
            // Q2): a keeper at work brings its wares back to their starting stock, at most once a game hour.
            if (authored_.economy.storeRule && job->role == "merchant"sv)
                restockFromStore(id, absoluteDay);   // (Shelves and making by the crew's hours since doc 68, 7.)
            // The staged economy's morning restock (doc 79, 7.7): its wares set out to their shelf targets, once a market day,
            // the keeper seen "setting out the wares" for a few minutes.
            if (job->role == "merchant"sv && staged() && (stagedRestock(id) || settingOut(id)))
                life.reason = "Setting out the wares.";
        }
        if (learning && body.cell == learning->work.cell && within(hour, learning->startHour, learning->endHour))
        {
            const auto& master = state_.careers.positions[learning->id].holder;
            const auto there = bodies.find(master);
            if (there != bodies.end() && there->second.cell == body.cell)
            {
                // Beside a better hand: LearnBeside a year, up to five short of the master, or the trade's level where
                // the master is at it, and never past it and five.
                const double best = skill(master, learning->id), level = std::max(learning->level, TradeSkilled);
                const double reach = std::min(level + 5, std::max(best - 5, best >= level ? level : 0.));
                double* at = skillSlot(id, learning->id, true);
                if (*at < reach)
                    *at = std::min(reach, *at + LearnBeside * step / (calendar::DaysPerYear * PaidSpells * 600.));
            }
        }
        // A place to work or rest is one wolf's. (Out in the fields or on the water a producer's place is no one's alone:
        // two fishers fish the same shore.)
        if (task == "sleep"sv || ((task == "watch"sv || task == "patrol"sv || (atWork && task == job->title)) &&
                                  !items::producerFor(job->title)))
        {
            if (!reservations.insert({&goal.cell, std::llround(goal.x * 1e6), std::llround(goal.y * 1e6)}).second)
            {
                life.reason = "Waiting for an occupied work or rest place.";
                continue;
            }
        }
        if (task == "sleep"sv)
        {
            life.fatigue = std::max(0., life.fatigue - .012 * step);
            continue;
        }
        if (task != "eat"sv)
            continue;
        life.progress += step;
        if (life.progress < 8.)
            continue;
        life.progress = 0;
        // What it carries that spoils soonest, the most nourishing of those (doc 35, Part 7: a meal fills a wolf, a loaf
        // less, by what each feeds).
        if (const auto* wallet = c.record->wallet)
            if (const auto food = eatFirst(*wallet); !food.empty())
            {
                consume(id, food, 1, "eat");
                life.hunger = std::max(0., life.hunger - nourishment(food) * 1.1);
            }
    }
}
} // namespace ratw
