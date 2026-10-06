#include "RatwCalendar.h"
#include "RatwItems.h"
#include "RatwSociety.h"

#include <algorithm>
#include <functional>
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <tuple>
#include <set>
#include <string_view>

// Daily routines for residents authored in a world file (Atlas Workshop).
// Everything here is driven by ResidentSpec data: roles, hours, places,
// patrol routes and the town economy. Nothing names a particular world.
namespace ratw
{
using namespace std::string_view_literals;
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
// A map keyed by resident ID looked up as the residents go, in ID order: walked once a pass instead of searched for
// each. What find() would give, so long as nothing at or after the cursor is erased in between (inserts are fine).
template <class Map>
class InOrder
{
  public:
    explicit InOrder(Map& map) : map_(map), at_(map.begin()) {}
    auto find(const std::string& key)
    {
        while (at_ != map_.end() && at_->first < key)
            ++at_;
        return at_ != map_.end() && at_->first == key ? at_ : map_.end();
    }

  private:
    Map& map_;
    decltype(std::declval<Map&>().begin()) at_;
};
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
// Thrown by a resident deciding at once with the others, on a thread, when its deciding would change something shared:
// it decides in its turn instead (Society::decideAuthored).
struct DecideInTurn
{
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
    for (const auto& r : authored_.residents)
        stockMaterials(r.id);                       // What the makers make things from, and the suppliers sell them.
    state_.craftingStocked = CraftingStock;
    defaultCareers();
    foundTills();                                   // Every business its own till (RatwTills.cpp, doc 46).
}

bool Society::adoptResident(const Society& from, const std::string& id)
{
    ++rosterRevision_;
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
            r.home = "\n";                          // (Its larder, below.)
            recordOf_.emplace(entry.first, records_.size());
            records_.push_back(std::move(r));
        }
        recordsRevision_ = rosterRevision_;
        recordsOf_ = this;
    }
    // A home's larder: again when the home changes, or while it has none (a home's stores are furnished later).
    for (auto& r : records_)
    {
        const auto& home = r.entry->second.homeCell;
        if (r.home != home)
        {
            r.home = home;
            r.larder = home.empty() ? std::string() : homeStore(home, "larder");
            r.larderAccount = nullptr;
            r.community = day_.communityOf ? day_.communityOf(home) : std::string();
        }
        if (!r.larderAccount && !r.larder.empty())
            r.larderAccount = account(r.larder);
    }
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
    if (bodies.empty())
        return;                                     // (A second with no one due: see needsBodies.)
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
    bool sharing = false;                           // While the threads decide (below): the caches here are only read.
    const auto place = [&](const std::string& cell) -> const Place& {
        auto found = places.find(cell);
        if (found == places.end())
        {
            if (sharing)
                throw DecideInTurn{};
            found = places.emplace(cell, makePlace(cell)).first;
        }
        return found->second;
    };
    const auto planFor = [&](const std::string& cell) -> const DayPlan& { return *place(cell).plan; };
    const auto sky = [&](const std::string& cell) { return day_.sky && !cell.empty() ? day_.sky(cell) : 0; };
    const auto pick = [](const std::vector<Spot>& spots, const std::string& id) -> const Spot* {
        return spots.empty() ? nullptr : &spots[std::hash<std::string>{}(id) % spots.size()];
    };
    // Where a merchant trades now: a stall on the square on Marketday mornings (unless the weather is foul), else the
    // shop. Only a city has stalls built (Docs/Design/39), and its own stallholders keep them every day; a town has
    // none, and no market day.
    const auto tradingAt = [&](const Position& p, const std::string& holder) -> Spot {
        const auto& plan = planFor(p.work.cell);
        if (plan.kind == "market"sv && !plan.foul && hour >= 7 && hour < 14)
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
    indexEmployers();
    (void)spec(std::string());
    (void)houses();
    (void)richestAt(std::string(), std::string());
    rollOutwork();
    refreshRecords();                               // (Each resident by number: see ResidentRecord.)
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
    // Whether each shop's keeper is keeping it (on the threads: each looks only at its own), then the lists, in order.
    struct Keeper
    {
        const Position* position;
        const std::string* holder;
        Spot at, serve;
        bool stall = false, keeping = false;
        std::string community;                      // Of where it serves.
    };
    std::vector<Keeper> keepers;
    for (const auto& shop : shopkeeping_)
    {
        if (shop.holder->empty())
            continue;                               // An empty shop is shut.
        place(shop.position->work.cell);            // (Known before the threads ask: where each trade trades.)
        keepers.push_back({shop.position, shop.holder, {}, {}, false, false, {}});
    }
    sharing = true;
    spread(keepers.size(), 16, [&](std::size_t i) {
        auto& k = keepers[i];
        const auto& p = *k.position;
        const auto number = recordOf_.find(*k.holder);
        const auto* body = number == recordOf_.end() ? nullptr : bodyOf[number->second];
        const auto* life = number == recordOf_.end() ? nullptr : &records_[number->second].entry->second;
        k.at = tradingAt(p, *k.holder);
        k.stall = k.at.cell != p.work.cell || k.at.x != p.work.x || k.at.y != p.work.y;
        k.serve = k.stall ? k.at : p.serve;
        k.keeping = body && !body->companion && life && life->task == "trade"sv && near(*body, k.at);
        if (k.keeping && day_.communityOf)
            k.community = day_.communityOf(k.serve.cell);
    });
    sharing = false;
    for (const auto& k : keepers)
    {
        if (!k.keeping)
            continue;
        const auto& holder = *k.holder;
        trading[holder] = {k.position, k.serve, k.community};   // Open for trade: where wolves sell what they bring in (doc 42).
        if (smith(holder))
            continue;                               // A forge sells to players, not food to the town.
        if (shopHasFood(holder))                    // (Any food: bread, porridge, a meal... doc 35, Part 7.)
        {
            openShops[holder] = {k.position, k.serve, k.community};
            if (k.stall)
                atStall_.insert(holder);
        }
    }
    // The open shops indexed for the nearest-shop search below: by the cell they serve in and by community, each list in
    // ID order (openShops' order), so the search finds what walking them all in that order found.
    std::vector<const std::pair<const std::string, Open>*> shopList;
    std::unordered_map<std::string, std::vector<std::size_t>> shopsInCell, shopsOfCommunity;
    for (const auto& open : openShops)
    {
        shopsInCell[open.second.serve.cell].push_back(shopList.size());
        if (!open.second.community.empty())
            shopsOfCommunity[open.second.community].push_back(shopList.size());
        shopList.push_back(&open);
    }
    // Each home's household (doc 42, the user: who keeps the house shops for it and minds its children): those living
    // there who are about this second, asked only of homes whose keeper wants to know.
    if (householdsDay_ != today || householdsOf_ != state_.residents.size())
    {
        households_.clear();
        for (const auto& [id, life] : state_.residents)
            if (!life.homeCell.empty())
                households_[life.homeCell].push_back(id);
        householdsDay_ = today;
        householdsOf_ = state_.residents.size();
    }
    const auto householdSize = [&](const std::string& home) {
        int n = 0;
        if (const auto found = households_.find(home); found != households_.end())
            for (const auto& id : found->second)
                n += bodies.count(id) > 0;
        return n;
    };
    const auto withChildren = [&](const std::string& home) {
        if (const auto found = households_.find(home); found != households_.end())
            for (const auto& id : found->second)
                if (const auto body = bodies.find(id); body != bodies.end() && body->second.age < 16)
                    return true;
        return false;
    };
    const auto larderLow = [&](const std::string& home) {
        const auto* store = account(homeStore(home, "larder"));
        int held = 0;
        if (store)
            for (const auto& [item, n] : store->stock)
                if (edible(item))
                    held += n * nourishment(item);
        return held < 50 * 2 * std::max(1, householdSize(home));
    };
    // What each resident does this second comes in two halves (Docs/Design/31-responsiveness.md, "Fast-forward"): what
    // it decides to do, and doing it. Everyone decides at once, from where things stood as the pass began, spread over
    // the pool's threads; then, in ID order, each does what it decided. One whose deciding would change something others
    // see (claim an odd job, take a ground out of town for the day, put food away, draw rations, give up waiting on
    // wages) decides in its turn instead, as everyone did before, after those before it have done theirs.
    struct Choice
    {
        enum class Kind : std::uint8_t
        {
            Skip,                                   // No spec or no body: nothing this second.
            InTurn,                                 // To decide in its turn.
            Decided
        };
        Kind kind = Kind::Skip;
        int step = 1;                               // The seconds this decision stands for (UnseenStep for the unseen).
        ResidentRecord* record = nullptr;
        const ResidentSpec* r = nullptr;
        const LifeBody* body = nullptr;
        EconomyAccount* wallet = nullptr;
        Place home;                                 // Where it lives: its community and the day's plan there.
        std::string larder;                         // Its home's larder, and the account (null for none).
        const EconomyAccount* larderAccount = nullptr;
        double hunger = 0, fatigue = 0;             // As they will be this second.
        // The job it works today (`stand`: one made up for it, when its own isn't), and the trade it learns.
        Position stand;
        const Position* job = nullptr;
        const Position* learning = nullptr;
        bool guard = false, merchantRole = false, onHours = false;
        bool walksRoute = false;                    // A patrol or a traveller: counted among its route's walkers.
        bool routeGoal = false;                     // Its goal is a post on its route, by its place among the walkers.
        bool companion = false, arrivedHome = false;
        const std::string* seller = nullptr;        // Where it sells what it brought in.
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

    // The first half: what a resident decides. At once (on a thread), nothing is changed, and anything that would change
    // something throws DecideInTurn; in its turn, it is changed as it goes.
    const auto decide = [&](std::pair<const std::string, ResidentLife>& pair, Choice& c, bool atOnce) {
        const auto& id = pair.first;
        const auto& life = pair.second;
        const auto* r = c.r;
        const auto& body = *c.body;
        auto& wallet = *c.wallet;
        // Its community (kept by its record) and the day's plan there.
        c.home.community = c.record->community;
        c.home.plan = &ordinary;
        if (day_.communityOf)
            if (const auto plan = day_.plans.find(c.home.community); plan != day_.plans.end())
                c.home.plan = &plan->second;
        const Place& homePlace = c.home;
        c.larder = c.record->larder;
        c.larderAccount = c.record->larderAccount;
        c.hunger = std::min(100., life.hunger + .0035 * c.step);
        c.fatigue = std::min(100., life.fatigue + .0025 * c.step);
        const double hunger = c.hunger, fatigue = c.fatigue;
        const auto outwork = [&](const std::string& community) -> const WorkGround* {
            if (!atOnce)
                return outworkOf(id, community);
            bool known = false;
            const auto* ground = outworkKnown(id, community, known);
            if (!known)
                throw DecideInTurn{};               // (Chosen in its turn: the grounds have room for so many.)
            return ground;
        };
        // The job comes from the position this resident holds (see Position). With none: an apprentice works
        // beside their master, unpaid, in the master's hours; anyone else looks for work.
        const Position* job = c.record->job;
        const Position* learning = c.record->learning;
        c.learning = learning;
        Position& stand = c.stand;
        stand = Position{};
        // A grown wolf whose post is only idling (doc 42, Phase 3b) works out of town like one without a post.
        if (job && job->role == "civilian"sv && body.age >= 16 && body.age < RetireAge && idlePost(job->title) &&
            outwork(homePlace.community))
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
                else if (const auto* ground = outwork(homePlace.community))
                {
                    // A trade of its own out of town (Phase 3b): it lives by what it brings back.
                    stand.title = outworkTitle(ground->trade);
                    stand.work = ground->spot;
                    stand.paid = true;
                }
                else
                {
                    stand.title = LabourTitle;
                    if (const auto* square = pick(homePlace.plan->crowd, id))
                        stand.work = *square;
                    stand.paid = true;
                }
            }
            job = &stand;
        }
        else if (const auto unpaid = state_.memory.unpaidSince.find(id); unpaid != state_.memory.unpaidSince.end() && job->role == "civilian"sv)
        {
            // A week without wages (doc 42, Phase 3): a week of day labour for the Town Works, then back to try again.
            const double since = absoluteDay - unpaid->second;
            if (since >= 14)
            {
                if (atOnce)
                    throw DecideInTurn{};
                state_.memory.unpaidSince.erase(unpaid), ++state_.memory.revision;
            }
            else if (since >= 7 && body.age < RetireAge)
            {
                stand = *job;
                stand.id.clear();
                stand.title = LabourTitle;
                stand.paid = true;
                if (const auto* square = pick(homePlace.plan->crowd, id))
                    stand.work = *square;
                job = &stand;
            }
        }
        // A household's keeper stays home (RatwHouseholds.cpp); a poor household's homemaker goes out to labour.
        const auto keeperOf = state_.memory.keeper.find(life.homeCell);
        const bool keepsHouse = keeperOf != state_.memory.keeper.end() && keeperOf->second == id && body.age >= 16;
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
                if (const auto* square = pick(homePlace.plan->crowd, id))
                    stand.work = *square;
            job = &stand;
        }
        c.job = job;
        const bool guard = job->role == "guard"sv, merchantRole = job->role == "merchant"sv;
        c.guard = guard;
        c.merchantRole = merchantRole;
        // A house's manager (doc 42, Phase 5b): the shop's goods are its till's, so it feeds itself like anyone.
        const bool managed = merchantRole && tillOf(id) != id;
        const auto route = authored_.routes.find(job->route);
        const bool patrols = guard && route != authored_.routes.end() && !route->second.posts.empty();
        // A civilian or merchant with a route is a traveller: they walk it through their working hours and, being
        // on the road, rest wherever the day leaves them rather than going home.
        const bool travels = !guard && route != authored_.routes.end() && !route->second.posts.empty();
        c.walksRoute = patrols || travels;          // (Its place among the route's walkers is counted in its turn.)
        if (body.companion)
        {
            c.companion = true;
            c.kind = Choice::Kind::Decided;
            return;
        }
        // Home storage (doc 36): food beyond the one carried (the best of it) is put away in the larder whenever they're
        // home (a shopkeeper's are the shop's stock, kept), and a larder with food in it is where a hungry wolf goes first.
        const std::string& larder = c.larder;
        const auto* larderAccount = c.larderAccount;
        if (larderAccount && (!merchantRole || managed) && body.cell == life.homeCell)
        {
            const auto keep = bestFood(wallet);
            std::vector<std::pair<std::string, int>> spare;
            for (const auto& [item, n] : wallet.stock)
                if (n > 0 && edible(item) && !forSale(id, item))
                    spare.push_back({item, item == keep ? n - 1 : n});
            for (const auto& [item, n] : spare)
                if (n > 0)
                {
                    if (atOnce)
                        throw DecideInTurn{};
                    shift(id, larder, item, std::min(n, 99), 0, "put away in the larder");
                }
        }
        // Rations (the user, 2026-10-05): a hungry guard eats from the watch's mess, a miner or quarryman from its works'
        // bread, before spending its own pennies: the town bought them for it.
        if (hunger >= 55 && !hasFood(wallet))
        {
            const auto& community = homePlace.community;
            const auto* producer = items::producerFor(job->title);
            const std::string mess = community.empty() ? std::string()
                                   : guard ? "town:" + community + ":watch"
                                   : producer && producer->id == "mine"sv ? "town:" + community + ":mines"
                                   : producer && producer->id == "quarry"sv ? "town:" + community + ":quarries"
                                                                          : std::string();
            if (const auto* rations = mess.empty() ? nullptr : account(mess))
                if (const auto food = bestFood(*rations); !food.empty())
                {
                    if (atOnce)
                        throw DecideInTurn{};
                    shift(mess, id, food, 1, 0, "rations");
                }
        }
        const bool carriesFood = hasFood(wallet);
        const bool larderHasFood = larderAccount && hasFood(*larderAccount) && !travels;
        const auto larderSpot = [&]() -> Spot {
            if (const auto home = homeStores_.find(life.homeCell); home != homeStores_.end())
                if (const auto at = home->second.find("larder"); at != home->second.end())
                    return at->second;
            return {life.homeCell, life.homeX, life.homeY};
        };
        // The day and the sky (Phase 9). The watch keeps its hours whatever the day and the weather.
        const auto& plan = *homePlace.plan;
        const int workSky = sky(job->work.cell);
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
            else if (plan.kind == "rest"sv && !(merchantRole && hour >= ServiceEnd && hour < ServiceEnd + 4))
                resting = "Restday: no work today.";   // Shops open after the service, so everyone can eat (doc 42).
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
        if (const auto* bed = c.record->bed; bed && bed->cell == life.homeCell)
            goal = *bed;                            // Their place on a bed at home (doc 36).
        // Nearest open shop: in the same cell, else in the same town, else any (a long walk for a meal). The nearest of
        // the best: everyone hungry in a cell went to the same shop (the first in ID order), and walked there together.
        // (Searched by the index: of the same cell, the nearest, the first of equals; else the first of the town; else the
        // first of all; never its own.)
        const Spot* shop = nullptr;
        const auto& home = homePlace.community;
        if (const auto here = shopsInCell.find(body.cell); here != shopsInCell.end())
            for (const auto i : here->second)
            {
                const auto& open = *shopList[i];
                const auto& at = open.second.serve;
                if (open.first != id &&
                    (!shop || std::hypot(at.x - body.x, at.y - body.y) < std::hypot(shop->x - body.x, shop->y - body.y)))
                    shop = &at;
            }
        if (!shop && !home.empty())
            if (const auto town = shopsOfCommunity.find(home); town != shopsOfCommunity.end())
                for (const auto i : town->second)
                    if (shopList[i]->first != id)
                    {
                        shop = &shopList[i]->second.serve;
                        break;
                    }
        if (!shop)
            for (const auto* open : shopList)
                if (open->first != id)
                {
                    shop = &open->second.serve;
                    break;
                }
        // Marketday: each of the townsfolk goes for an hour, some time between eight and one.
        const auto visit = [&] { return 8 + int(std::hash<std::string>{}(id + "market") % 5); };
        // Where a wolf who works out of town sells what it brings in: an open shop of its town that takes it.
        const std::string* seller = nullptr;
        if (outworkTitled(job->title))
            for (const auto& open : trading)
                if (open.first != id && (home.empty() || open.second.community == home) && sellsTo(id, open.first))
                {
                    seller = &open.first;
                    break;
                }
        c.seller = seller;
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
        else if ((!merchantRole || managed) && hunger >= 55 && !carriesFood && larderHasFood)
        {
            task = "fetch food";
            goal = larderSpot();
            reason = onDuty ? "A short meal break at home." : "Hungry; fetching a meal from the larder at home.";
        }
        // (A keeper whose own shelves hold nothing to eat buys a meal elsewhere too.) Only with the price of a meal.
        else if (hunger >= 55 && !carriesFood && spendable(id) >= 4 && shop)
        {
            task = "buy food";
            goal = *shop;
            reason = onDuty ? "A short meal break at the shop." : "Hungry; buying food at the shop.";
        }
        else if (hunger >= 55 && !carriesFood && !onDuty && shop && shop->cell != ""sv)
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
            goal = job->work;
            c.routeGoal = patrols;
            reason = "On watch; paid by the town treasury.";
        }
        else if (plan.kind == "rest"sv && !plan.pews.empty() && hour >= ServiceStart && hour < ServiceEnd && !guard &&
                 (clergy(job->title) || goesToChurch(id, std::int64_t(std::floor(absoluteDay)))))
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
                goal = *pick(plan.pews, id);
                reason = "Restday: at the morning service.";
            }
        }
        else if (merchantRole && onHours)
        {
            task = "trade";
            goal = tradingAt(*job, id);
            reason = atStall_.count(id) || goal.cell != job->work.cell || goal.x != job->work.x
                         ? plan.kind == "market"sv ? "Marketday: trading from a stall at the market."
                                                 : "A morning at a food stall in the market square."
                         : "Keeping shop; restocks from the town stores.";
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
            reason = "Marketday: buying and gossiping at the stalls.";
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
            reason = "The shop is shut; tallying accounts.";
        }
        else if (const auto* odd = [&]() -> const OddJob* {
                     bool claims = false;
                     const auto* held =
                         oddJobFor(id, *job, job == &stand || job->title == LabourTitle || outworkTitled(job->title) || idlePost(job->title),
                                   body.age, wallet.cash < 12 || (hunger >= 55 && !carriesFood), hour,
                                   // (What its own post pays a day, to weigh a better-paid hire against.)
                                   job->paid && job != &stand && body.age >= 16 && hour >= 8 && hour < 10
                                       ? std::int64_t(dayWage(homePlace.community, wageKind(payerOf(id, *job, body.age), *job)))
                                       : 0,
                                   atOnce ? &claims : nullptr);
                     if (claims)
                         throw DecideInTurn{};      // (Taken in its turn: a job has room for so many.)
                     return held;
                 }())
        {
            // An odd job (RatwOddJobs.cpp): to where it begins, then to where it ends.
            task = "odd job: " + odd->kind;
            goal = odd->stage.at(id) == 0 ? odd->from : odd->to;
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
            else if (withChildren(life.homeCell))
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
            c.routeGoal = true;
            reason = "On the road; their business keeps them travelling.";
        }
        else if (onHours && outworkTitled(job->title) && hour >= endHour - 1.5 && carriesForSale(id) && seller)
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
                     : job->title == "retired"sv   ? "Retired; spending the day in familiar company."
                     : job->paid                 ? "Daily work, for wages."
                                                 : "Spending the day in familiar company.";
            if (state_.memory.unpaidSince.count(id) && job->title != LabourTitle)
            {
                const auto payer = payerOf(id, *job, body.age);
                // (A shop pays from its till: the one owed is its keeper, doc 46.)
                const auto held = payer.account.rfind("till:", 0) == 0 ? state_.careers.positions.find(payer.account.substr(5))
                                                                         : state_.careers.positions.end();
                const auto* boss = spec(held != state_.careers.positions.end() ? held->second.holder : payer.account);
                reason = "Waiting on wages from " + (boss ? boss->name : payer.whom) + ".";
            }
        }
        else if (!resting.empty() && within(hour, job->startHour, job->endHour))
        {
            // A day off: with friends where the evenings are spent, unless the sky sends everyone home.
            task = workSky == 2 || sky(r->evening.cell) == 2 ? "sheltering" : "resting";
            if (task == "resting"sv)
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
        if (travels && !(onHours && task == job->title) && task != "eat"sv && task != "buy food"sv && task != "fetch food"sv && task != "relocate"sv &&
            task != "festival"sv && task != "at the market"sv && task != "at church"sv && task != "preaching"sv)
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
        c.kind = Choice::Kind::Decided;
    };

    // Everyone at once: over the threads where there are some (setParallel), a share of the residents each.
    // Who it is, where its body is, and its purse (a resident with neither spec nor body does nothing this second).
    const auto find = [&](std::size_t i) {
        auto& c = choices[i];
        c.kind = Choice::Kind::Skip;
        c.companion = false;
        c.body = nullptr;
        c.wallet = nullptr;
        c.record = &records_[i];
        c.r = c.record->spec;
        const auto* body = bodyOf[i];
        if (!c.r || !body || body->cell.empty() || body->cell.size() > 80 || !validNumber(body->x, 0, 256) || !validNumber(body->y, 0, 256))
            return false;
        c.body = body;
        if (body->unseen)
        {
            // Out of sight: only on the unseen's second, and then for every second since it last decided.
            if (!unseenDecided_)
                return false;
            const double since = order[i]->second.decidedAt < 0 ? 1
                                 : std::round((absoluteDay - order[i]->second.decidedAt) * calendar::SecondsPerDay);
            c.step = int(std::clamp(since, 1., double(UnseenStep)));
        }
        else
            c.step = 1;
        c.wallet = c.record->wallet;
        c.kind = Choice::Kind::InTurn;
        return c.wallet != nullptr;                 // (Without a purse: in its turn, where that fails as it always did.)
    };
    const auto atOnce = [&](std::size_t i) {
        auto& c = choices[i];
        if (!find(i))
            return;
        try
        {
            decide(*order[i], c, true);
        }
        catch (const DecideInTurn&)
        {
            c.kind = Choice::Kind::InTurn;
        }
    };
    sharing = true;
    spread(order.size(), 64, atOnce);
    sharing = false;

    // The second half, in ID order: each does what it decided (deciding first, if it is its turn to).
    std::unordered_set<Station, StationHash> reservations;
    reservations.reserve(order.size());
    std::map<std::string, int> routeWalkers;
    for (std::size_t i = 0; i < order.size(); ++i)
    {
        auto& pair = *order[i];
        auto& c = choices[i];
        if (c.kind == Choice::Kind::Skip)
            continue;
        if (c.kind == Choice::Kind::InTurn)
        {
            if (!c.wallet)
                c.wallet = &state_.accounts.at(pair.first);
            decide(pair, c, false);
        }
        const auto& id = pair.first;
        auto& life = pair.second;
        const auto& body = *c.body;
        auto& wallet = *c.wallet;
        life.hunger = c.hunger;
        life.fatigue = c.fatigue;
        life.decidedAt = absoluteDay;
        const int step = c.step;
        const Position* job = c.job;
        const Position* learning = c.learning;
        const bool guard = c.guard, merchantRole = c.merchantRole, onHours = c.onHours;
        const int walker = c.walksRoute ? routeWalkers[job->route]++ : 0;
        if (c.companion)
        {
            life.task = "companion";
            life.reason = "Ordinary work is suspended while recruited.";
            life.progress = 0;
            life.goalCell.clear();
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
        const auto& home = c.home.community;
        const std::string& larder = c.larder;
        const auto* larderAccount = c.larderAccount;
        const std::string* seller = c.seller;
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
        if (task.rfind("odd job: ", 0) == 0)
        {
            advanceOddJob(id, step);
            continue;
        }
        if (task == "seeking alms"sv)
        {
            const auto church = churchOf(treasuryOf(home));
            for (const auto& open : openShops)
                if (open.second.serve.cell == goal.cell && open.second.serve.x == goal.x && open.second.serve.y == goal.y)
                {
                    const auto till = tillOf(open.first);
                    if (const auto* shelves = account(till); shelves && account(church))
                        if (const auto food = bestFood(*shelves); !food.empty())
                            if (const auto price = shopPrice(open.first, food);
                                account(church)->cash >= price && transfer(till, church, food, 1, price, "alms bought for the hungry"))
                                shift(church, id, food, 1, 0, "alms");
                    break;
                }
            continue;
        }
        if (task == "shopping for the household"sv)
        {
            // A trip's worth for everyone at home (the household's habit), put away in the larder once home.
            for (const auto& open : openShops)
                if (open.second.serve.cell == goal.cell && open.second.serve.x == goal.x && open.second.serve.y == goal.y)
                {
                    buyFood(id, open.first, true);
                    break;
                }
            continue;
        }
        // At a festival the town's stores feed everyone once (goods only: nothing is bought).
        if (task == "festival"sv && feasted_.insert(id).second)
            shift(storeFor(life.homeCell), id, "meal", 1, 0, "festival feast");
        // An unpaid post held by a grown wolf (a foreman, a clerk) is paid by whoever it works for too (doc 42, Phase 3);
        // not a beggar's, nor the head of a great house's, who live on alms and on the house.
        const bool earns = job->paid || (!merchantRole && body.age >= 16 && body.age < RetireAge && !houseHead(job->title) &&
                                         job->title.find("beg") == std::string::npos);
        const bool paidWork = earns && (task == "patrol"sv || task == "watch"sv || (task == job->title && onHours));
        // Working at their post in its hours, they grow more skilled at it; an apprentice beside a master who is
        // there too learns three times as fast. Nothing else depends on skill yet but who succeeds whom.
        double skilled = -1;                        // Its skill at its post, once known.
        if (onHours && job != &c.stand && (task == job->title || task == "trade"sv || task == "watch"sv || task == "patrol"sv))
        {
            // (Its skill where the record keeps it: practise without the lookup.)
            auto& record = *c.record;
            double* at = job != record.job ? skillSlot(id, job->id, true)
                                           : record.skill ? record.skill : (record.skill = skillSlot(id, job->id, true));
            *at = std::min(100.0, *at + .002 * step * (1 - *at / 100));
            skilled = *at;
        }
        if (learning && body.cell == learning->work.cell && within(hour, learning->startHour, learning->endHour))
        {
            const auto& master = state_.careers.positions[learning->id].holder;
            const auto there = bodies.find(master);
            if (there != bodies.end() && there->second.cell == body.cell)
            {
                const double learnt = practise(id, learning->id, .006 * step);
                if (learning->id == job->id)
                    skilled = learnt;
            }
        }
        // (Out in the fields or on the water a producer's place is no one's alone: two fishers fish the same shore.)
        if (task == "sleep"sv || (paidWork && !items::producerFor(job->title)))
        {
            // A place to work or rest is one wolf's.
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
        life.progress += step;
        // The skilled finish paid work sooner (workPace: 1 at skill 50).
        const double duration = paidWork ? (guard ? 120. : 600.) * workPace(skilled >= 0 ? skilled : skill(id, job->id))
                                         : task == "eat"sv ? 8. : 12.;
        if (life.progress < duration)
            continue;
        // (A step past the end carries on into the next: work takes the same time in steps as by the second.)
        life.progress = step > 1 ? std::max(0., life.progress - duration) : 0;
        if (task == "fetch food"sv)
        {
            const auto food = larderAccount ? eatFirst(larder, *larderAccount) : std::string();   // (What spoils soonest.)
            if (food.empty() || !shift(larder, pair.first, food, 1, 0, "taken from the larder"))
                life.reason = "The larder is empty.";
            else
                larderTaken_[larder] += nourishment(food);
        }
        else if (task == "eat"sv)
        {
            // What it carries that spoils soonest, the most nourishing of those (doc 35, Part 7: a meal fills a wolf, a
            // loaf less, by what each feeds; doc 42, "Spoilage").
            const auto food = eatFirst(pair.first, wallet);
            if (!food.empty())
            {
                consume(pair.first, food, 1, "eat");
                life.hunger = std::max(0., life.hunger - nourishment(food) * 1.1);
            }
        }
        else if (task == "at church"sv && hour >= ServiceEnd - .5 && offered_.insert(pair.first).second)
        {
            // The plate goes round as the service ends: a penny or two from those who can spare it.
            if (const std::int64_t gift = wallet.cash > 60 ? 2 : wallet.cash > 20 ? 1 : 0; gift > 0)
            {
                const auto church = churchOf(treasuryOfResident(pair.first));
                openAccount(church);
                shift(pair.first, church, "", 0, gift, "the collection");
            }
        }
        else if (task == "selling"sv)
        {
            if (!seller || sellBroughtIn(pair.first, *seller) == 0)
                life.reason = "The shop wants none of it today.";
        }
        else if (paidWork && outworkTitled(job->title))
        {
            // A spell's work out of town: what the ground gives (doc 41's patches and game, shared with players).
            if (const auto* ground = outworkOf(pair.first, home); ground && day_.harvest)
                for (const auto& [item, n] : day_.harvest(pair.first, *ground, season_))
                    create(pair.first, item, n, ground->trade == "hunting"sv ? "hunted" : "brought in");
        }
        else if (paidWork)
        {
            produce(pair.first, absoluteDay);       // A farmer's, fisher's... yield (Data/Items/crafts.json).
            // Wages from whoever the work is for (doc 42, Phase 2): the shop, the town, the church, the house.
            const auto payer = payerOf(pair.first, *job, body.age);
            if (!payer.account.empty() && life.wagesToday < PaidSpells)
            {
                // The town's wage table (doc 46, Phase 4): a day's pay for its kind of post, a PaidSpells-th of it a spell,
                // the pennies' fractions carried to the next (never more than a day's owed).
                const bool business = payer.whom == "the shop"sv || payer.whom == "the house"sv;
                const double day = dayWage(home, wageKind(payer, *job));
                auto& carry = wageCarry_[pair.first];
                carry = std::min(carry + day / PaidSpells, day);
                const std::int64_t wage = std::int64_t(std::floor(carry));
                // An employer that can't pay: its town (or church) covers the wage if it has plenty, so the work goes on
                // (the user, 2026-10-05).
                std::string subsidy;
                if (wage > 0 && business && spendable(payer.account) < wage)
                    subsidy = subsidiser(home);
                // (TRIAL town_budget: a treasury covers wages only from its day's budget.)
                if (!subsidy.empty() && subsidy.rfind("stores:", 0) == 0 && townBudget(subsidy, wage) < wage)
                    subsidy.clear();
                if (wage <= 0)
                    ++life.wagesToday;                   // (Under a penny this spell: carried.)
                else if (!subsidy.empty() && shift(subsidy, pair.first, "", 0, wage, "a wage subsidised by the town"))
                {
                    ++life.wagesToday;
                    carry -= double(wage);
                    state_.memory.revision += state_.memory.unpaidSince.erase(pair.first);
                }
                else if (spendable(payer.account) >= wage &&
                    shift(payer.account, pair.first, "", 0, wage, guard ? "watch wages" : job->title == LabourTitle ? "day labour" : "service wages"))
                {
                    ++life.wagesToday;
                    carry -= double(wage);
                    if (job->title != LabourTitle && state_.memory.unpaidSince.erase(pair.first))
                        ++state_.memory.revision;
                }
                else
                    state_.memory.revision += state_.memory.unpaidSince.emplace(pair.first, absoluteDay).second;
            }
        }
        else if (task == "buy food"sv)
        {
            const std::string* keeper = nullptr;
            for (const auto& open : openShops)
                if (open.second.serve.cell == goal.cell && open.second.serve.x == goal.x && open.second.serve.y == goal.y)
                    keeper = &open.first;
            // Something to eat and, where there is a larder, a couple of days more (doc 36): one trip feeds them for
            // days, and a town doesn't all queue at the shop each morning. What, by what gives the most for the money
            // to this wolf's taste (doc 35, Part 7: bread and porridge, a meal now and then).
            if (!keeper || buyFood(pair.first, *keeper, larderAccount != nullptr) == 0)
                life.reason = "Cannot buy food: the shop, its stock or the purse is unavailable.";
        }
        else if (task == "trade"sv)
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
            // A stall on Marketday carries more. Prepared meals keep three days: a shop carries about two days of what it
            // sells (mealsSold_), at least three (doc 42, "Spoilage").
            const auto till = tillOf(pair.first);   // The shop's shelves (its house's till, doc 42).
            const auto sells = mealsSold_.count(till) ? mealsSold_.at(till) : 6.;
            const int carried = std::clamp(int(std::ceil(sells * 2)) + 1, 3, atStall_.count(pair.first) ? 20 : 12);
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
