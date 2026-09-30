#include "RatwSociety.h"

#include <algorithm>
#include <functional>
#include <cmath>

// Daily routines for residents authored in a world file (Atlas Workshop).
// Everything here is driven by ResidentSpec data: roles, hours, places,
// patrol routes and the town economy. Nothing names a particular world.
namespace ratw
{
namespace
{
constexpr std::int64_t MoneyLimit = 1000000000;

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
}

bool Society::adoptResident(const Society& from, const std::string& id)
{
    if (roster_ != Roster::Authored || from.roster_ != Roster::Authored)
        return false;
    const ResidentSpec* incoming = from.spec(id);
    const auto current = std::find_if(authored_.residents.begin(), authored_.residents.end(),
                                      [&](const ResidentSpec& r) { return r.id == id; });
    if (!incoming && current == authored_.residents.end())
        return false;
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

void Society::decideAuthored(double absoluteDay, const std::map<std::string, LifeBody>& bodies)
{
    const double hour = (absoluteDay - std::floor(absoluteDay)) * 24.;
    const bool night = hour < 6 || hour >= 22;
    const auto slot = std::int64_t(std::floor(absoluteDay * 96)); // Patrols move post every 15 game minutes.
    const auto today = std::int64_t(std::floor(absoluteDay));
    if (today != feastDay_)
    {
        feasted_.clear();
        feastDay_ = today;
    }
    // The day's plan for a place's community, and its sky (Phase 9): an ordinary day under a fair sky without them.
    static const DayPlan ordinary;
    const auto planFor = [&](const std::string& cell) -> const DayPlan& {
        if (!day_.communityOf)
            return ordinary;
        const auto found = day_.plans.find(day_.communityOf(cell));
        return found == day_.plans.end() ? ordinary : found->second;
    };
    const auto sky = [&](const std::string& cell) { return day_.sky && !cell.empty() ? day_.sky(cell) : 0; };
    const auto pick = [](const std::vector<Spot>& spots, const std::string& id) -> const Spot* {
        return spots.empty() ? nullptr : &spots[std::hash<std::string>{}(id) % spots.size()];
    };
    // Where a merchant trades now: a market stall on Marketday mornings (unless the weather is foul), else the shop.
    const auto tradingAt = [&](const Position& p, const std::string& holder) -> Spot {
        const auto& plan = planFor(p.work.cell);
        if (plan.kind == "market" && !plan.foul && hour >= 7 && hour < 14)
            if (const auto* stall = pick(plan.stalls, holder))
                return *stall;
        return p.work;
    };
    // A shop is open while whoever holds a merchant's position is trading there (by holder), at its counter or its
    // stall, and customers are served where the merchant stands.
    struct Open
    {
        const Position* position;
        Spot serve;
        std::string community;                      // Of where it serves.
    };
    std::map<std::string, Open> openShops;
    atStall_.clear();
    for (const auto& p : positions_)
    {
        if (p.role != "merchant")
            continue;
        const auto held = state_.careers.positions.find(p.id);
        if (held == state_.careers.positions.end() || held->second.holder.empty())
            continue;                               // An empty shop is shut.
        const auto& holder = held->second.holder;
        const auto body = bodies.find(holder);
        const auto* life = resident(holder);
        const Spot at = tradingAt(p, holder);
        const bool stall = at.cell != p.work.cell || at.x != p.work.x || at.y != p.work.y;
        if (body != bodies.end() && !body->second.companion && life && life->task == "trade" &&
            near(body->second, at) && stock(*account(holder), "meal") > 0)
        {
            const Spot serve = stall ? at : p.serve;
            openShops[holder] = {&p, serve, day_.communityOf ? day_.communityOf(serve.cell) : std::string()};
            if (stall)
                atStall_.insert(holder);
        }
    }
    std::map<std::string, std::string> reservations;
    std::map<std::string, int> routeWalkers;
    for (auto& pair : state_.residents)
    {
        const auto* r = spec(pair.first);
        const auto bodyIt = bodies.find(pair.first);
        if (!r || bodyIt == bodies.end())
            continue;
        const auto& body = bodyIt->second;
        if (body.cell.empty() || body.cell.size() > 80 || !validNumber(body.x, 0, 256) || !validNumber(body.y, 0, 256))
            continue;
        auto& life = pair.second;
        auto& wallet = state_.accounts.at(pair.first);
        life.hunger = std::min(100., life.hunger + .0035);
        life.fatigue = std::min(100., life.fatigue + .0025);
        // The job comes from the position this resident holds (see Position). With none: an apprentice works
        // beside their master, unpaid, in the master's hours; anyone else looks for work.
        const Position* job = jobOf(pair.first);
        const Position* learning = apprenticedTo(pair.first);
        Position stand;
        if (!job)
        {
            stand.role = "civilian";
            stand.paid = false;
            stand.startHour = learning ? learning->startHour : r->startHour;
            stand.endHour = learning ? learning->endHour : r->endHour;
            stand.title = learning ? "apprenticed: " + learning->title : body.age < 16 ? "growing up" : "looking for work";
            stand.work = learning ? learning->work : r->evening;
            job = &stand;
        }
        const bool guard = job->role == "guard", merchantRole = job->role == "merchant";
        const auto route = authored_.routes.find(job->route);
        const bool patrols = guard && route != authored_.routes.end() && !route->second.posts.empty();
        // A civilian or merchant with a route is a traveller: they walk it through their working hours and, being
        // on the road, rest wherever the day leaves them rather than going home.
        const bool travels = !guard && route != authored_.routes.end() && !route->second.posts.empty();
        const int walker = patrols || travels ? routeWalkers[job->route]++ : 0;
        if (body.companion)
        {
            life.task = "companion";
            life.reason = "Ordinary work is suspended while recruited.";
            life.progress = 0;
            life.goalCell.clear();
            continue;
        }
        // The day and the sky (Phase 9). The watch keeps its hours whatever the day and the weather.
        const auto& plan = planFor(life.homeCell);
        const int workSky = sky(job->work.cell);
        const bool festival = plan.kind == "festival" && hour >= 12 && hour < 23;
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
            else if (plan.kind == "rest" && !(merchantRole && hour >= 8 && hour < 12))
                resting = "Restday: no work today.";   // Shops open the morning, so everyone can eat.
            if (!resting.empty())
                onHours = false;
        }
        const bool onDuty = guard && onHours;
        std::string task, reason;
        Spot goal{life.homeCell, life.homeX, life.homeY};
        // Nearest open shop: in the same cell, else in the same town, else any (a long walk for a meal).
        const Spot* shop = nullptr;
        int shopRank = -1;
        const auto home = day_.communityOf ? day_.communityOf(life.homeCell) : std::string();
        for (const auto& open : openShops)
        {
            if (open.first == pair.first)
                continue;
            const auto& at = open.second.serve;
            const int rank = at.cell == body.cell ? 2 : !home.empty() && open.second.community == home ? 1 : 0;
            if (rank > shopRank)
                shop = &at, shopRank = rank;
        }
        // Marketday: each of the townsfolk goes for an hour, some time between eight and one.
        const auto visit = 8 + int(std::hash<std::string>{}(pair.first + "market") % 5);
        const bool marketHour = plan.kind == "market" && !plan.foul && !plan.crowd.empty() && !guard && !merchantRole &&
                                hour >= visit && hour < visit + 1;
        if (!life.relocationCell.empty())
        {
            task = "relocate";
            goal = {life.relocationCell, life.relocationX, life.relocationY};
            reason = "Travelling to an operator-approved new home; arrival is not instantaneous.";
            if (body.cell == goal.cell && std::hypot(body.x - goal.x, body.y - goal.y) <= .35)
            {
                life.homeCell = goal.cell;
                life.homeX = goal.x;
                life.homeY = goal.y;
                life.relocationCell.clear();
                life.relocationX = life.relocationY = 0;
                reason = "Arrived at the new home.";
            }
        }
        else if (life.hunger >= 60 && stock(wallet, "meal") > 0)
        {
            task = "eat";
            const bool continuing = life.task == "eat" && life.goalCell == body.cell;
            goal = {body.cell, continuing ? life.goalX : body.x, continuing ? life.goalY : body.y};
            reason = "Hungry; a carried meal is available.";
        }
        else if (!merchantRole && life.hunger >= 55 && stock(wallet, "meal") == 0 && wallet.cash >= 6 && shop)
        {
            task = "buy food";
            goal = *shop;
            reason = onDuty ? "A short meal break at the shop." : "Hungry; buying a meal at the shop.";
        }
        else if (onDuty)
        {
            task = patrols ? "patrol" : "watch";
            goal = patrols ? route->second.posts[std::size_t(slot + walker * 3) % route->second.posts.size()]
                           : job->work;
            reason = "On watch; paid by the town treasury.";
        }
        else if (merchantRole && onHours)
        {
            task = "trade";
            goal = tradingAt(*job, pair.first);
            reason = atStall_.count(pair.first) || goal.cell != job->work.cell || goal.x != job->work.x
                         ? "Marketday: trading from a stall at the market."
                         : "Keeping shop; restocks from the town stores and pays market dues.";
        }
        else if (festival && !night && life.fatigue < 80 && !(guard && within(hour, job->startHour, job->endHour)))
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
                goal = *pick(plan.crowd, pair.first);
                reason = plan.name + ": the town gathers at the market.";
            }
        }
        else if (marketHour)
        {
            task = "at the market";
            goal = *pick(plan.crowd, pair.first);
            reason = "Marketday: buying and gossiping at the stalls.";
        }
        else if (guard)
        {
            task = "sleep";
            reason = "Off watch; resting.";
        }
        else if (night || life.fatigue >= 80 || (life.task == "sleep" && life.fatigue > 15 && hour < 8))
        {
            task = "sleep";
            reason = "Resting at home for the night.";
        }
        else if (merchantRole)
        {
            task = "at home";
            goal = r->evening;
            reason = "The shop is shut; tallying accounts.";
        }
        else if (onHours && travels)
        {
            task = job->title;
            goal = route->second.posts[std::size_t(slot + walker * 3) % route->second.posts.size()];
            reason = "On the road; their business keeps them travelling.";
        }
        else if (onHours)
        {
            task = job->title;
            goal = job->work;
            reason = job->paid ? "Daily work, paid by the town treasury." : "Spending the day in familiar company.";
        }
        else if (!resting.empty() && within(hour, job->startHour, job->endHour))
        {
            // A day off: with friends where the evenings are spent, unless the sky sends everyone home.
            task = workSky == 2 || sky(r->evening.cell) == 2 ? "sheltering" : "resting";
            if (task == "resting")
                goal = sky(r->evening.cell) >= 1 ? Spot{life.homeCell, life.homeX, life.homeY} : r->evening;
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
            if (const int evening = sky(r->evening.cell); evening >= 1)
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
        if (travels && !(onHours && task == job->title) && task != "eat" && task != "buy food" && task != "relocate" &&
            task != "festival" && task != "at the market")
        {
            // Off the road's hours: stay put, camped or lodged where the day ended.
            goal = {body.cell, std::floor(body.x) + .5, std::floor(body.y) + .5};
            if (task == "sleep")
                reason = "Resting on the road; home is far behind.";
        }
        // A civilian with a wander area roams it during work hours and evenings, moving on every half hour.
        if (!r->wander.empty() && !guard && !merchantRole && (task == job->title || task == "socialize"))
        {
            const auto turn = std::size_t(slot / 2) + std::hash<std::string>{}(r->id);
            goal = r->wander[turn % r->wander.size()];
            reason = task == "socialize" ? "Free time; roaming familiar ground." : "Roaming the area they keep to.";
        }
        if (task != life.task || goal.cell != life.goalCell || goal.x != life.goalX || goal.y != life.goalY)
            life.progress = 0;
        life.task = task;
        life.reason = reason;
        life.goalCell = goal.cell;
        life.goalX = goal.x;
        life.goalY = goal.y;
        if (!near(body, goal))
        {
            life.progress = 0;
            continue;
        }
        // At a festival the town's stores feed everyone once (goods only: nothing is bought).
        if (task == "festival" && feasted_.insert(pair.first).second)
            shift(storeFor(life.homeCell), pair.first, "meal", 1, 0, "festival feast");
        const bool paidWork = job->paid && (task == "patrol" || task == "watch" || (task == job->title && onHours));
        // Working at their post in its hours, they grow more skilled at it; an apprentice beside a master who is
        // there too learns three times as fast. Nothing else depends on skill yet but who succeeds whom.
        if (onHours && job != &stand && (task == job->title || task == "trade" || task == "watch" || task == "patrol"))
            practise(pair.first, job->id, .002);
        if (learning && body.cell == learning->work.cell && within(hour, learning->startHour, learning->endHour))
        {
            const auto& master = state_.careers.positions[learning->id].holder;
            const auto there = bodies.find(master);
            if (there != bodies.end() && there->second.cell == body.cell)
                practise(pair.first, learning->id, .006);
        }
        const std::string station = goal.cell + ":" + std::to_string(goal.x) + ":" + std::to_string(goal.y);
        const bool exclusive = task == "sleep" || paidWork;
        if (exclusive && reservations.count(station))
        {
            life.reason = "Waiting for an occupied work or rest place.";
            continue;
        }
        if (exclusive)
            reservations[station] = pair.first;
        if (task == "sleep")
        {
            life.fatigue = std::max(0., life.fatigue - .012);
            continue;
        }
        life.progress += 1.;
        // The skilled finish paid work sooner (workPace: 1 at skill 50).
        const double duration = paidWork ? (guard ? 120. : 600.) * workPace(skill(pair.first, job->id))
                                         : task == "eat" ? 8. : 12.;
        if (life.progress < duration)
            continue;
        life.progress = 0;
        if (task == "eat")
        {
            --wallet.stock["meal"];
            life.hunger = std::max(0., life.hunger - 55.);
            record("eat", pair.first, "consumed", "meal", 1, 0);
        }
        else if (paidWork)
        {
            auto& treasury = state_.accounts.at("treasury");
            if (treasury.cash >= 2 && wallet.cash <= MoneyLimit - 2 && life.wagesToday < 3)
            {
                treasury.cash -= 2;
                wallet.cash += 2;
                ++life.wagesToday;
                record(guard ? "watch wages" : "service wages", "treasury", pair.first, "", 0, 2);
            }
        }
        else if (task == "buy food")
        {
            const std::string* seller = nullptr;
            for (const auto& open : openShops)
                if (open.second.serve.cell == goal.cell && open.second.serve.x == goal.x && open.second.serve.y == goal.y)
                    seller = &open.first;
            if (!seller || !transfer(*seller, pair.first, "meal", 1, atStall_.count(*seller) ? 5 : 6, "resident food purchase"))
                life.reason = "Cannot buy food: the shop, its stock or the purse is unavailable.";
        }
        else if (task == "trade")
        {
            // From the town's own stores (the treasury, where there is one town): what the caravans have brought.
            const std::string& storeId = storeFor(job->work.cell);
            const auto& stores = *account(storeId);
            // A stall on Marketday carries more.
            const int carried = atStall_.count(pair.first) ? 20 : 12;
            const int meals = std::min({3, carried - stock(wallet, "meal"), stock(stores, "meal"),
                                        int(std::min<std::int64_t>(3, wallet.cash / 4))});
            if (meals > 0)
                transfer(storeId, pair.first, "meal", meals, 4, "wholesale restock");
            const int herbs = std::min({3, 8 - stock(wallet, "herbs"), stock(stores, "herbs"),
                                        int(std::min<std::int64_t>(3, wallet.cash))});
            if (herbs > 0)
                transfer(storeId, pair.first, "herbs", herbs, 1, "wholesale restock");
            if (wallet.cash > 150 && stores.cash <= MoneyLimit - 30)
            {
                wallet.cash -= 30;
                state_.accounts.at("treasury").cash += 30;
                record("market dues", pair.first, "treasury", "", 0, 30);
            }
        }
    }
}
} // namespace ratw
