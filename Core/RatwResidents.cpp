#include "RatwItems.h"
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
    for (const auto& r : authored_.residents)
        stockMaterials(r.id);                       // What the makers make things from, and the suppliers sell them.
    state_.craftingStocked = CraftingStock;
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
    if (state_.craftingStocked < CraftingStock)
    {
        // A world saved before crafting (doc 35, Phase 5): its makers and suppliers get their starting materials once.
        for (const auto& r : authored_.residents)
            stockMaterials(r.id);
        state_.craftingStocked = CraftingStock;
    }
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
    // Where a merchant trades now: a stall on the square on Marketday mornings (unless the weather is foul), else the
    // shop. Only a city has stalls built (Docs/Design/39), and its own stallholders keep them every day; a town has
    // none, and no market day.
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
    std::map<std::string, Open> openShops, trading;   // Open with food for sale; open for trade at all.
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
        const bool keeping = body != bodies.end() && !body->second.companion && life && life->task == "trade" && near(body->second, at);
        if (keeping)                                // Open for trade: where wolves sell what they bring in (doc 42).
            trading[holder] = {&p, stall ? at : p.serve, day_.communityOf ? day_.communityOf((stall ? at : p.serve).cell) : std::string()};
        if (smith(holder))
            continue;                               // A forge sells to players, not food to the town.
        if (keeping && shopHasFood(holder))         // (Any food: bread, porridge, a meal... doc 35, Part 7.)
        {
            const Spot serve = stall ? at : p.serve;
            openShops[holder] = {&p, serve, day_.communityOf ? day_.communityOf(serve.cell) : std::string()};
            if (stall)
                atStall_.insert(holder);
        }
    }
    std::map<std::string, std::string> reservations;
    std::map<std::string, int> routeWalkers;
    // Each home's household (doc 42, the user: who keeps the house shops for it and minds its children).
    std::map<std::string, int> householdSize;
    std::set<std::string> withChildren;
    for (const auto& [id, life] : state_.residents)
        if (const auto body = bodies.find(id); body != bodies.end() && !life.homeCell.empty())
        {
            ++householdSize[life.homeCell];
            if (body->second.age < 16)
                withChildren.insert(life.homeCell);
        }
    const auto larderLow = [&](const std::string& home) {
        const auto* store = account(homeStore(home, "larder"));
        int held = 0;
        if (store)
            for (const auto& [item, n] : store->stock)
                if (edible(item))
                    held += n * nourishment(item);
        return held < 50 * 2 * std::max(1, householdSize[home]);
    };
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
        // A grown wolf whose post is only idling (doc 42, Phase 3b) works out of town like one without a post.
        if (job && job->role == "civilian" && body.age >= 16 && body.age < RetireAge && idlePost(job->title) &&
            outworkOf(pair.first, day_.communityOf ? day_.communityOf(life.homeCell) : std::string()))
            job = nullptr;
        if (!job)
        {
            stand.role = "civilian";
            stand.paid = false;
            stand.startHour = learning ? learning->startHour : r->startHour;
            stand.endHour = learning ? learning->endHour : r->endHour;
            stand.title = learning ? "apprenticed: " + learning->title : body.age < 16 ? "growing up" : "looking for work";
            stand.work = learning ? learning->work : r->evening;
            // A living for every grown wolf (doc 42, Phase 3): one out of work labours for the Town Works, at the
            // town's square (or where it spends its days), paid by the town; one of RetireAge or more has retired.
            if (!learning && body.age >= 16)
            {
                if (body.age >= RetireAge)
                    stand.title = "retired";
                else if (const auto* ground = outworkOf(pair.first, day_.communityOf ? day_.communityOf(life.homeCell) : std::string()))
                {
                    // A trade of its own out of town (Phase 3b): it lives by what it brings back.
                    stand.title = outworkTitle(ground->trade);
                    stand.work = ground->spot;
                    stand.paid = true;
                }
                else
                {
                    stand.title = LabourTitle;
                    if (const auto* square = pick(planFor(life.homeCell).crowd, pair.first))
                        stand.work = *square;
                    stand.paid = true;
                }
            }
            job = &stand;
        }
        else if (const auto unpaid = state_.memory.unpaidSince.find(pair.first); unpaid != state_.memory.unpaidSince.end() && job->role == "civilian")
        {
            // A week without wages (doc 42, Phase 3): a week of day labour for the Town Works, then back to try again.
            const double since = absoluteDay - unpaid->second;
            if (since >= 14)
                state_.memory.unpaidSince.erase(unpaid), ++state_.memory.revision;
            else if (since >= 7 && body.age < RetireAge)
            {
                stand = *job;
                stand.id.clear();
                stand.title = LabourTitle;
                stand.paid = true;
                if (const auto* square = pick(planFor(life.homeCell).crowd, pair.first))
                    stand.work = *square;
                job = &stand;
            }
        }
        // A household's keeper stays home (RatwHouseholds.cpp); a poor household's homemaker goes out to labour.
        const auto keeperOf = state_.memory.keeper.find(life.homeCell);
        const bool keepsHouse = keeperOf != state_.memory.keeper.end() && keeperOf->second == pair.first && body.age >= 16;
        const bool sentToWork = !keepsHouse && state_.memory.toWork.count(life.homeCell) && body.age >= 16 && body.age < RetireAge &&
                                homemaking(job->title);
        if (keepsHouse || sentToWork)
        {
            stand = Position{};
            stand.role = "civilian";
            stand.paid = sentToWork;
            stand.startHour = 8;
            stand.endHour = 18;
            stand.title = keepsHouse ? "keeping the house" : LabourTitle;
            stand.work = {life.homeCell, life.homeX, life.homeY};
            if (sentToWork)
                if (const auto* square = pick(planFor(life.homeCell).crowd, pair.first))
                    stand.work = *square;
            job = &stand;
        }
        const bool guard = job->role == "guard", merchantRole = job->role == "merchant";
        // A house's manager (doc 42, Phase 5b): the shop's goods are its till's, so it feeds itself like anyone.
        const bool managed = merchantRole && tillOf(pair.first) != pair.first;
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
        // Home storage (doc 36): food beyond the one carried (the best of it) is put away in the larder whenever they're
        // home (a shopkeeper's are the shop's stock, kept), and a larder with food in it is where a hungry wolf goes first.
        const std::string larder = life.homeCell.empty() ? std::string() : homeStore(life.homeCell, "larder");
        const auto* larderAccount = larder.empty() ? nullptr : account(larder);
        if (larderAccount && (!merchantRole || managed) && body.cell == life.homeCell)
        {
            const auto keep = bestFood(wallet);
            std::vector<std::pair<std::string, int>> spare;
            for (const auto& [item, n] : wallet.stock)
                if (n > 0 && edible(item))
                    spare.push_back({item, item == keep ? n - 1 : n});
            for (const auto& [item, n] : spare)
                if (n > 0)
                    shift(pair.first, larder, item, std::min(n, 99), 0, "put away in the larder");
        }
        // Rations (the user, 2026-10-05): a hungry guard eats from the watch's mess, a miner or quarryman from its works'
        // bread, before spending its own pennies: the town bought them for it.
        if (life.hunger >= 55 && bestFood(wallet).empty())
        {
            const auto community = day_.communityOf ? day_.communityOf(life.homeCell) : std::string();
            const auto* producer = items::producerFor(job->title);
            const std::string mess = community.empty() ? std::string()
                                   : guard ? "town:" + community + ":watch"
                                   : producer && producer->id == "mine" ? "town:" + community + ":mines"
                                   : producer && producer->id == "quarry" ? "town:" + community + ":quarries"
                                                                          : std::string();
            if (const auto* rations = mess.empty() ? nullptr : account(mess))
                if (const auto food = bestFood(*rations); !food.empty())
                    shift(mess, pair.first, food, 1, 0, "rations");
        }
        const bool carriesFood = !bestFood(wallet).empty();
        const bool larderHasFood = larderAccount && !bestFood(*larderAccount).empty() && !travels;
        const auto larderSpot = [&]() -> Spot {
            if (const auto home = homeStores_.find(life.homeCell); home != homeStores_.end())
                if (const auto at = home->second.find("larder"); at != home->second.end())
                    return at->second;
            return {life.homeCell, life.homeX, life.homeY};
        };
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
            else if (plan.kind == "rest" && !(merchantRole && hour >= ServiceEnd && hour < ServiceEnd + 4))
                resting = "Restday: no work today.";   // Shops open after the service, so everyone can eat (doc 42).
            if (!resting.empty())
                onHours = false;
        }
        const bool onDuty = guard && onHours;
        std::string task, reason;
        Spot goal{life.homeCell, life.homeX, life.homeY};
        if (const auto bed = beds_.find(pair.first); bed != beds_.end() && bed->second.cell == life.homeCell)
            goal = bed->second;                     // Their place on a bed at home (doc 36).
        // Nearest open shop: in the same cell, else in the same town, else any (a long walk for a meal).
        const Spot* shop = nullptr;
        int shopRank = -1;
        const auto home = day_.communityOf ? day_.communityOf(life.homeCell) : std::string();
        for (const auto& open : openShops)
        {
            if (open.first == pair.first)
                continue;
            const auto& at = open.second.serve;
            // The nearest of the best: everyone hungry in a cell went to the same shop (the first in ID order), and
            // walked there together.
            const int rank = at.cell == body.cell ? 2 : !home.empty() && open.second.community == home ? 1 : 0;
            if (rank > shopRank || (rank == shopRank && rank == 2 &&
                                    std::hypot(at.x - body.x, at.y - body.y) < std::hypot(shop->x - body.x, shop->y - body.y)))
                shop = &at, shopRank = rank;
        }
        // Marketday: each of the townsfolk goes for an hour, some time between eight and one.
        const auto visit = 8 + int(std::hash<std::string>{}(pair.first + "market") % 5);
        // Where a wolf who works out of town sells what it brings in: an open shop of its town that takes it.
        const std::string* seller = nullptr;
        if (outworkTitled(job->title))
            for (const auto& open : trading)
                if (open.first != pair.first && (home.empty() || open.second.community == home) && sellsTo(pair.first, open.first))
                {
                    seller = &open.first;
                    break;
                }
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
        else if (life.hunger >= 60 && carriesFood)
        {
            task = "eat";
            const bool continuing = life.task == "eat" && life.goalCell == body.cell;
            goal = {body.cell, continuing ? life.goalX : body.x, continuing ? life.goalY : body.y};
            reason = "Hungry; carrying something to eat.";
        }
        else if ((!merchantRole || managed) && life.hunger >= 55 && !carriesFood && larderHasFood)
        {
            task = "fetch food";
            goal = larderSpot();
            reason = onDuty ? "A short meal break at home." : "Hungry; fetching a meal from the larder at home.";
        }
        // (A keeper whose own shelves hold nothing to eat buys a meal elsewhere too.) Only with the price of a meal.
        else if (life.hunger >= 55 && !carriesFood && spendable(pair.first) >= 4 && shop)
        {
            task = "buy food";
            goal = *shop;
            reason = onDuty ? "A short meal break at the shop." : "Hungry; buying food at the shop.";
        }
        else if (life.hunger >= 55 && !carriesFood && !onDuty && shop && shop->cell != "")
        {
            // Hungry, with nothing to eat and not the price of a meal: alms, at the shop, the church paying (the user,
            // 2026-10-05: no one starves with a church in town).
            task = "seeking alms";
            goal = *shop;
            reason = "Hungry and penniless; asking alms, which the church pays for.";
        }
        else if (onDuty)
        {
            task = patrols ? "patrol" : "watch";
            goal = patrols ? route->second.posts[std::size_t(slot + walker * 3) % route->second.posts.size()]
                           : job->work;
            reason = "On watch; paid by the town treasury.";
        }
        else if (plan.kind == "rest" && !plan.pews.empty() && hour >= ServiceStart && hour < ServiceEnd && !guard &&
                 (clergy(job->title) || goesToChurch(pair.first, std::int64_t(std::floor(absoluteDay)))))
        {
            // Restday's service (doc 42, Phase 6): the clergy at the pulpit, a third of the town on the benches.
            if (clergy(job->title))
            {
                task = "preaching";
                goal = plan.pulpit;
                reason = "Restday: preaching to the town.";
            }
            else
            {
                task = "at church";
                goal = *pick(plan.pews, pair.first);
                reason = "Restday: at the morning service.";
            }
        }
        else if (merchantRole && onHours)
        {
            task = "trade";
            goal = tradingAt(*job, pair.first);
            reason = atStall_.count(pair.first) || goal.cell != job->work.cell || goal.x != job->work.x
                         ? plan.kind == "market" ? "Marketday: trading from a stall at the market."
                                                 : "A morning at a food stall in the market square."
                         : "Keeping shop; restocks from the town stores.";
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
        else if (const auto* odd = guard ? nullptr
                                         : oddJobFor(pair.first, *job,
                                                     job == &stand || job->title == LabourTitle || outworkTitled(job->title) || idlePost(job->title),
                                                     body.age, wallet.cash < 12 || (life.hunger >= 55 && !carriesFood), hour,
                                                     // (What its own post pays a day, to weigh a better-paid hire against.)
                                                     job->paid && job != &stand && body.age >= 16 && hour >= 8 && hour < 10
                                                         ? std::int64_t(PaidSpells) * (spendable(payerOf(pair.first, *job, body.age).account) > ComfortableTill ? 2 : 1)
                                                         : 0))
        {
            // An odd job (RatwOddJobs.cpp): to where it begins, then to where it ends.
            task = "odd job: " + odd->kind;
            goal = odd->stage.at(pair.first) == 0 ? odd->from : odd->to;
            reason = "An odd job: " + odd->what + ".";
        }
        else if (keepsHouse && onHours)
        {
            // Keeping the house: the household's food first, then the children, then a look about the town.
            if (larderLow(life.homeCell) && wallet.cash >= 5 && shop)
            {
                task = "shopping for the household";
                goal = *shop;
                reason = "Laying in the household's food with the household purse.";
            }
            else if (withChildren.count(life.homeCell))
            {
                task = "minding the children";
                reason = "Keeping the house and minding the children.";
            }
            else
            {
                task = "about the town";
                goal = r->evening;
                reason = "Keeping the house; out and about while the others work.";
            }
        }
        else if (onHours && travels)
        {
            task = job->title;
            goal = route->second.posts[std::size_t(slot + walker * 3) % route->second.posts.size()];
            reason = "On the road; their business keeps them travelling.";
        }
        else if (onHours && outworkTitled(job->title) && hour >= endHour - 1.5 && carriesForSale(pair.first) && seller)
        {
            // The last of the day: back to town to sell what it brought in.
            task = "selling";
            goal = trading.at(*seller).serve;
            reason = "Back from the wild to sell what it brought in.";
        }
        else if (onHours)
        {
            task = job->title;
            goal = job->work;
            reason = job->title == LabourTitle ? "Day labour for the Town Works, paid by the town."
                     : outworkTitled(job->title) ? "Working the country out of town; lives by what it sells."
                     : job->title == "retired"   ? "Retired; spending the day in familiar company."
                     : job->paid                 ? "Daily work, for wages."
                                                 : "Spending the day in familiar company.";
            if (state_.memory.unpaidSince.count(pair.first) && job->title != LabourTitle)
            {
                const auto payer = payerOf(pair.first, *job, body.age);
                const auto* boss = spec(payer.account);
                reason = "Waiting on wages from " + (boss ? boss->name : payer.whom) + ".";
            }
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
        if (travels && !(onHours && task == job->title) && task != "eat" && task != "buy food" && task != "fetch food" && task != "relocate" &&
            task != "festival" && task != "at the market" && task != "at church" && task != "preaching")
        {
            // Off the road's hours: stay put, camped or lodged where the day ended.
            goal = {body.cell, std::floor(body.x) + .5, std::floor(body.y) + .5};
            if (task == "sleep")
                reason = "Resting on the road; home is far behind.";
        }
        // Children keep together (the user, 2026-10-05): at play and in the evenings a friend group meets at one place.
        if (body.age < 16 && !learning && !guard && !merchantRole && (task == job->title || task == "socialize"))
            if (const auto& group = friendGroup(pair.first); !group.empty())
                if (const auto* meet = pick(plan.crowd, group))
                {
                    goal = *meet;
                    reason = task == "socialize" ? "Out with friends for the evening." : "Playing with friends.";
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
        if (task.rfind("odd job: ", 0) == 0)
        {
            advanceOddJob(pair.first);
            continue;
        }
        if (task == "seeking alms")
        {
            const auto community = day_.communityOf ? day_.communityOf(life.homeCell) : std::string();
            const auto church = churchOf(treasuryOf(community));
            for (const auto& open : openShops)
                if (open.second.serve.cell == goal.cell && open.second.serve.x == goal.x && open.second.serve.y == goal.y)
                {
                    const auto till = tillOf(open.first);
                    if (const auto* shelves = account(till); shelves && account(church))
                        if (const auto food = bestFood(*shelves); !food.empty())
                            if (const auto price = shopPrice(open.first, food);
                                account(church)->cash >= price && transfer(till, church, food, 1, price, "alms bought for the hungry"))
                                shift(church, pair.first, food, 1, 0, "alms");
                    break;
                }
            continue;
        }
        if (task == "shopping for the household")
        {
            // A trip's worth for everyone at home (the household's habit), put away in the larder once home.
            for (const auto& open : openShops)
                if (open.second.serve.cell == goal.cell && open.second.serve.x == goal.x && open.second.serve.y == goal.y)
                {
                    buyFood(pair.first, open.first, true);
                    break;
                }
            continue;
        }
        // At a festival the town's stores feed everyone once (goods only: nothing is bought).
        if (task == "festival" && feasted_.insert(pair.first).second)
            shift(storeFor(life.homeCell), pair.first, "meal", 1, 0, "festival feast");
        // An unpaid post held by a grown wolf (a foreman, a clerk) is paid by whoever it works for too (doc 42, Phase 3);
        // not a beggar's, nor the head of a great house's, who live on alms and on the house.
        const bool earns = job->paid || (!merchantRole && body.age >= 16 && body.age < RetireAge && !houseHead(job->title) &&
                                         job->title.find("beg") == std::string::npos);
        const bool paidWork = earns && (task == "patrol" || task == "watch" || (task == job->title && onHours));
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
        // (Out in the fields or on the water a producer's place is no one's alone: two fishers fish the same shore.)
        if (task == "sleep" || (paidWork && !items::producerFor(job->title)))
        {
            // A place to work or rest is one wolf's (keyed only when it is: the key is dear to build).
            const std::string station = goal.cell + ":" + std::to_string(goal.x) + ":" + std::to_string(goal.y);
            if (reservations.count(station))
            {
                life.reason = "Waiting for an occupied work or rest place.";
                continue;
            }
            reservations[station] = pair.first;
        }
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
        if (task == "fetch food")
        {
            const auto food = larderAccount ? bestFood(*larderAccount) : std::string();
            if (food.empty() || !shift(larder, pair.first, food, 1, 0, "taken from the larder"))
                life.reason = "The larder is empty.";
        }
        else if (task == "eat")
        {
            // The best it carries (doc 35, Part 7): a meal fills a wolf (55), a loaf less, by what each feeds.
            const auto food = bestFood(wallet);
            if (!food.empty())
            {
                consume(pair.first, food, 1, "eat");
                life.hunger = std::max(0., life.hunger - nourishment(food) * 1.1);
            }
        }
        else if (task == "at church" && hour >= ServiceEnd - .5 && offered_.insert(pair.first).second)
        {
            // The plate goes round as the service ends: a penny or two from those who can spare it.
            if (const std::int64_t gift = wallet.cash > 60 ? 2 : wallet.cash > 20 ? 1 : 0; gift > 0)
            {
                const auto church = churchOf(treasuryOfResident(pair.first));
                openAccount(church);
                shift(pair.first, church, "", 0, gift, "the collection");
            }
        }
        else if (task == "selling")
        {
            if (!seller || sellBroughtIn(pair.first, *seller) == 0)
                life.reason = "The shop wants none of it today.";
        }
        else if (paidWork && outworkTitled(job->title))
        {
            // A spell's work out of town: what the ground gives (doc 41's patches and game, shared with players).
            if (const auto* ground = outworkOf(pair.first, home); ground && day_.harvest)
                for (const auto& [item, n] : day_.harvest(pair.first, *ground, season_))
                    create(pair.first, item, n, ground->trade == "hunting" ? "hunted" : "brought in");
        }
        else if (paidWork)
        {
            produce(pair.first, absoluteDay);       // A farmer's, fisher's... yield (Data/Items/crafts.json).
            // Wages from whoever the work is for (doc 42, Phase 2): the shop, the town, the church, the house.
            const auto payer = payerOf(pair.first, *job, body.age);
            if (!payer.account.empty() && life.wagesToday < PaidSpells)
            {
                // A shop or a house pays what its takings bear (doc 42): 2p a spell from a comfortable till, 1p from a
                // lean one; a town 1p from a treasury under LeanTreasury a head. The church pays 2p.
                const bool business = payer.whom == "the shop" || payer.whom == "the house";
                const auto* payerPurse = account(payer.account);
                const bool lean = business ? payerPurse && payerPurse->cash <= ComfortableTill
                                           : payer.account.rfind("stores:", 0) == 0 || payer.account == "treasury" ? treasuryLean(payer.account)
                                                                                                                    : false;
                const std::int64_t wage = lean ? 1 : 2;
                // An employer that can't pay: its town (or church) covers the wage if it has plenty, so the work goes on
                // (the user, 2026-10-05).
                std::string subsidy;
                if (business && spendable(payer.account) < wage)
                    subsidy = subsidiser(home);
                if (!subsidy.empty() && shift(subsidy, pair.first, "", 0, wage, "a wage subsidised by the town"))
                {
                    ++life.wagesToday;
                    state_.memory.revision += state_.memory.unpaidSince.erase(pair.first);
                }
                else if (spendable(payer.account) >= wage &&
                    shift(payer.account, pair.first, "", 0, wage, guard ? "watch wages" : job->title == LabourTitle ? "day labour" : "service wages"))
                {
                    ++life.wagesToday;
                    if (job->title != LabourTitle && state_.memory.unpaidSince.erase(pair.first))
                        ++state_.memory.revision;
                }
                else
                    state_.memory.revision += state_.memory.unpaidSince.emplace(pair.first, absoluteDay).second;
            }
        }
        else if (task == "buy food")
        {
            const std::string* seller = nullptr;
            for (const auto& open : openShops)
                if (open.second.serve.cell == goal.cell && open.second.serve.x == goal.x && open.second.serve.y == goal.y)
                    seller = &open.first;
            // Something to eat and, where there is a larder, a couple of days more (doc 36): one trip feeds them for
            // days, and a town doesn't all queue at the shop each morning. What, by what gives the most for the money
            // to this wolf's taste (doc 35, Part 7: bread and porridge, a meal now and then).
            if (!seller || buyFood(pair.first, *seller, larderAccount != nullptr) == 0)
                life.reason = "Cannot buy food: the shop, its stock or the purse is unavailable.";
        }
        else if (task == "trade")
        {
            const auto sold = wares(pair.first);
            // The shop's own goods (doc 35, Phase 5): a batch of whatever has run low, made from materials. A shop
            // that makes nothing yet sells what it has.
            craft(pair.first, job->work.cell, absoluteDay);
            const bool sellsMeals = std::find(sold.begin(), sold.end(), "meal") != sold.end();
            const bool sellsHerbs = std::find(sold.begin(), sold.end(), "herbs") != sold.end();
            if (!sellsMeals && !sellsHerbs)
                continue;
            // From the town's own stores (the treasury, where there is one town): what the caravans have brought.
            const std::string& storeId = storeFor(job->work.cell);
            const auto& stores = *account(storeId);
            // A stall on Marketday carries more.
            const int carried = atStall_.count(pair.first) ? 20 : 12;
            const auto till = tillOf(pair.first);   // The shop's shelves (its house's till, doc 42).
            const auto& shelves = state_.accounts.at(till);
            const int meals = !sellsMeals ? 0 : std::min({3, carried - stock(shelves, "meal"), stock(stores, "meal"),
                                        int(std::min<std::int64_t>(3, spendable(till) / 4))});
            if (meals > 0)
                transfer(storeId, till, "meal", meals, 4, "wholesale restock");
            const int herbs = !sellsHerbs ? 0 : std::min({3, 8 - stock(shelves, "herbs"), stock(stores, "herbs"),
                                        int(std::min<std::int64_t>(3, spendable(till)))});
            if (herbs > 0)
                transfer(storeId, till, "herbs", herbs, 1, "wholesale restock");
        }
    }
}
} // namespace ratw
