// Great houses and the businesses they own (Docs/Design/42-money-in-circulation.md, Phase 5b). A house owns the
// industry of its town and some of its shops. Each such business has a till of its own; its keeper is its manager, paid
// a wage from the till, and everything above the till's float goes to the house each night. A house props up a till
// that runs low, and sells a business that keeps needing it to another house. Money only moves.
#include "RatwItems.h"
#include "RatwSociety.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>

namespace ratw
{
namespace
{
std::string slug(const std::string& name)
{
    std::string out;
    for (const unsigned char c : name)
        out += std::isalnum(c) ? char(std::tolower(c)) : '_';
    return out;
}
} // namespace

const std::vector<House>& Society::houses() const
{
    if (housesKnown_)
        return houses_;
    housesKnown_ = true;
    houses_.clear();
    for (const auto& p : positions_)
    {
        if (p.role != "civilian" || !houseHead(p.title))
            continue;
        // "ruling House Fell" -> House Fell; "keeping Vesk Manor" -> Vesk Manor; "holding court" -> the Court of a town.
        const auto community = communityOfResident(p.founder);
        std::string name = p.title.rfind("ruling ", 0) == 0 ? p.title.substr(7) : p.title.rfind("keeping ", 0) == 0 ? p.title.substr(8)
                                                                                                                    : "the Court of " + community;
        const auto id = "house:" + slug(name);
        const auto* trade = items::houseTradeOf(name);
        if (std::none_of(houses_.begin(), houses_.end(), [&](const House& h) { return h.id == id; }))
            houses_.push_back({id, name, p.id, community, trade ? trade->id : std::string()});
    }
    return houses_;
}

std::string Society::ownerOf(const std::string& positionId) const
{
    const auto found = state_.houses.owner.find(positionId);
    return found == state_.houses.owner.end() ? std::string() : found->second;
}

std::string Society::tillOf(const std::string& keeper) const
{
    // A business's till: a great house's (doc 42, Phase 5b) or, since doc 46's Phase 2, its own; the keeper's purse for one
    // that has none (an older save not yet run with tills, a test's village).
    const auto* job = jobOf(keeper);
    if (!job)
        return keeper;
    const auto till = tills_.find(job->id);
    return till != tills_.end() ? till->second : keeper;
}

std::int64_t Society::floatOf(const std::string& positionId) const
{
    // FloatDays of the business's running: its manager's wage and its help's (the posts that work where it does), and
    // a little for materials (placeholders; three days since 2026-10-05, when a week drained the houses' fortunes). At
    // its town's going rate (doc 64, Phase 13).
    const auto* business = position(positionId);
    if (!business)
        return 0;
    const auto town = communityOfResident(business->founder);
    // (A farm's or a site's workers work the same ground, each on its own account: none is another's help. Doc 46.)
    int help = 0;
    if (business->role == "merchant")
        for (const auto& p : positions_)
            help += p.id != positionId && p.role != "merchant" && p.work.cell == business->work.cell;
    // And its materials (doc 61): a maker's store of them for its first craft (MaterialBatches batches, at the catalog's
    // prices at the going rate), so that a night's takings swept to its house never leave it unable to buy its ore and
    // coal.
    double store = 0;
    if (const auto* kind = business->role == "merchant" ? items::businessFor(business->title) : nullptr)
        if (const auto crafts = items::craftsFor(kind->id); !crafts.empty())
            for (const auto& [item, n] : crafts.front()->in)
                if (const auto* good = items::good(item))
                    store += double(good->price) * n * MaterialBatches;
    std::int64_t materials = store > 0 ? pennies(atGoingRate(store, town)) : 0;
    // And a week of its standing orders on the roads (doc 61), paid when the caravan comes: swept to the house each
    // night, a city smith's till held a quarter of what its iron cost when the caravan came.
    if (const auto orders = ordersCost_.find("till:" + positionId); orders != ordersCost_.end())
        materials += orders->second;
    return FloatDays * (pennies(ManagerWage, town) + help * pennies(HelpWage, town)) + pennies(FloatMaterials, town) + materials;
}

void Society::foundHouses()
{
    auto& state = state_.houses;
    state.founded = true;
    ++state.revision;
    std::map<std::string, std::vector<const House*>> byTown;
    for (const auto& h : houses())
    {
        byTown[h.community].push_back(&h);
        openAccount(h.id);                          // (Even one that owns nothing yet: it may buy.)
        // Its founding fortune (the user, 2026-10-05: houses start with large purses). Like the world's first treasury,
        // money made once, at the founding, and counted as made.
        if (auto& purse = state_.accounts.at(h.id); purse.cash == 0)
        {
            purse.cash = HouseFortune;
            state_.minted += HouseFortune;
            record("a great house's fortune", "outside", h.id, "", 0, HouseFortune);
        }
    }
    for (const auto& p : positions_)
    {
        if (p.role != "merchant")
            continue;
        const auto town = byTown.find(communityOfResident(p.founder));
        if (town == byTown.end() || town->second.empty())
            continue;                               // A town without a great house: its shops are their keepers' own.
        const auto owner = ownerFor(p, town->second);
        if (owner.empty())
            continue;
        const House& house = **std::find_if(town->second.begin(), town->second.end(), [&](const House* h) { return h->id == owner; });
        const auto till = "till:" + p.id;
        openAccount(house.id);
        openAccount(till);
        state.owner[p.id] = house.id;
        // The keeper's goods are the shop's; its money too, but for a week of its own wage, which stays its own.
        const auto& keeper = state_.careers.positions[p.id].holder;
        if (const auto* purse = keeper.empty() ? nullptr : account(keeper))
        {
            std::vector<std::pair<std::string, int>> goods(purse->stock.begin(), purse->stock.end());
            for (const auto& [item, n] : goods)
                if (n > 0 && edible(item) == false)
                    shift(keeper, till, item, n, 0, "the shop's till");
                else if (n > 1)
                    shift(keeper, till, item, n - 1, 0, "the shop's till");   // (It keeps one thing to eat.)
            const auto keep = pennies(ManagerWage, communityOfResident(keeper)) * FloatDays;
            if (account(keeper)->cash > keep)
                shift(keeper, till, "", 0, account(keeper)->cash - keep, "the shop's till");
        }
        // Its starting money (2026-10-05): the till begins with its float, so filling it doesn't drain the house.
        startingMoney(till, floatOf(p.id) - account(till)->cash);
    }
    state.plan = housePlan();
}

std::string Society::ownerFor(const Position& p, const std::vector<const House*>& town) const
{
    const auto* business = items::businessFor(p.title);
    if (std::any_of(town.begin(), town.end(), [](const House* h) { return !h->trade.empty(); }))
    {
        // By name: the Grayrock Foundry is Grayrock's, whatever it makes; then by trade: a sawmill is Fell's.
        for (const auto* h : town)
            if (const auto* trade = items::houseTradeOf(h->name))
                for (const auto& word : trade->names)
                    if (items::holdsWord(p.title, word) || items::holdsWord(p.work.cell, word))
                        return h->id;
        if (business)
            for (const auto* h : town)
                if (const auto* trade = items::houseTradeOf(h->name);
                    trade && std::find(trade->businesses.begin(), trade->businesses.end(), business->id) != trade->businesses.end())
                    return h->id;
        return {};
    }
    // The industry (workshops, yards) is the houses'; the shops, about half of them, by lot.
    const auto lot = std::hash<std::string>{}(p.id + "|house");
    const bool industry = business && (business->kind == "works" || business->kind == "yard");
    if (!industry && lot % 2)
        return {};
    return town[lot / 2 % town.size()]->id;
}

bool Society::ownTrade(const std::string& positionId, const std::string& houseId) const
{
    // A business of its owner's trade, or named for it (doc 61).
    const auto* p = position(positionId);
    const auto found = std::find_if(houses().begin(), houses().end(), [&](const House& h) { return h.id == houseId; });
    const auto* trade = p && found != houses().end() ? items::houseTradeOf(found->name) : nullptr;
    if (!trade)
        return false;
    for (const auto& word : trade->names)
        if (items::holdsWord(p->title, word) || items::holdsWord(p->work.cell, word))
            return true;
    const auto* business = items::businessFor(p->title);
    return business && std::find(trade->businesses.begin(), trade->businesses.end(), business->id) != trade->businesses.end();
}

std::string Society::housePlan() const
{
    std::string plan = items::houseRules() + "|";
    for (const auto& h : houses())
        plan += h.id + ",";
    return plan;
}

void Society::sortHouses()
{
    // The rules have changed since ownership was given out (doc 61: houses.json; a house no longer counted one, or
    // one counted now that wasn't): each business goes to the house it belongs to now, or to its keeper. A house no
    // longer a house gives its money and goods to its town's treasury; a new one gets its founding fortune. Only
    // ownership moves: every business keeps its till.
    auto& state = state_.houses;
    std::map<std::string, std::vector<const House*>> byTown;
    std::set<std::string> live;
    for (const auto& h : houses())
    {
        byTown[h.community].push_back(&h);
        live.insert(h.id);
        if (!account(h.id))
        {
            openAccount(h.id);
            // (The great houses are old families, not founded in the story: before the realm's money stands, a house's
            // purse is part of it; after, a house the rules newly count already had what it holds, and nothing is made
            // for it. Docs/Design/64-economy-migration.md, the user, 2026-10-15.)
            if (state_.memory.purses < PursesFounded)
            {
                state_.accounts.at(h.id).cash = HouseFortune;
                state_.minted += HouseFortune;
                record("a great house's fortune", "outside", h.id, "", 0, HouseFortune);
            }
        }
    }
    std::map<std::string, std::string> gone;        // A house no more -> where its money goes.
    for (const auto& [id, purse] : state_.accounts)
        if (id.rfind("house:", 0) == 0 && !live.count(id))
            gone[id] = treasuryOf(capital());
    for (const auto& [pid, house] : state.owner)
        if (gone.count(house))
            if (const auto* p = position(pid))
                gone[house] = treasuryOf(communityOfResident(p->founder));
    for (const auto& [house, treasury] : gone)
    {
        openAccount(treasury);
        if (const auto* purse = account(house))
        {
            std::vector<std::pair<std::string, int>> goods(purse->stock.begin(), purse->stock.end());
            for (const auto& [item, n] : goods)
                if (n > 0)
                    shift(house, treasury, item, n, 0, "a house no more");
            if (account(house)->cash > 0)
                shift(house, treasury, "", 0, account(house)->cash, "a house no more");
        }
    }
    for (const auto& p : positions_)
    {
        if (p.role != "merchant")
            continue;
        const auto town = byTown.find(communityOfResident(p.founder));
        const auto now = town == byTown.end() ? std::string() : ownerFor(p, town->second);
        const auto was = ownerOf(p.id);
        if (now == was)
            continue;
        if (now.empty())
            state.owner.erase(p.id);
        else
        {
            openAccount("till:" + p.id);
            state.owner[p.id] = now;
        }
        state.propped.erase(p.id);
    }
    state.plan = housePlan();
    ++state.revision;
}

bool Society::sellBusiness(const std::string& positionId, const std::string& house, std::int64_t price)
{
    auto& state = state_.houses;
    const auto found = state.owner.find(positionId);
    if (found == state.owner.end() || found->second == house || house.rfind("house:", 0) != 0)
        return false;
    openAccount(house);
    if (price > 0 && !shift(house, found->second, "", 0, price, "sale of a business"))
        return false;
    found->second = house;
    state.propped.erase(positionId);
    ++state.revision;
    return true;
}

void Society::tendHouses(std::int64_t day)
{
    auto& state = state_.houses;
    if (day == state.day)
        return;
    state.day = day;
    ++state.revision;
    if (!state.founded)
        foundHouses();
    else if (state.plan != housePlan())
        sortHouses();
    const std::vector<std::pair<std::string, std::string>> owned(state.owner.begin(), state.owner.end());
    for (const auto& [pid, house] : owned)
    {
        const auto till = "till:" + pid;
        if (!account(till))
            continue;
        openAccount(house);
        // The manager's wage first.
        const auto& manager = state_.careers.positions[pid].holder;
        if (!manager.empty() && account(manager))
            shift(till, manager, "", 0, std::min<std::int64_t>(std::int64_t(std::ceil(employerWage(till) - 1e-9)),
                                                               account(till)->cash), "manager's wage");   // (Its own wage: doc 64, Phase 6.)
        // (The takings above its float no longer go to the house each night, nor does the house prop it up or sell it
        // off: its month's profit is the house's, and the house tops it up when it runs short, as any owner does. Doc
        // 64, Phase 5: tendOwners.)
    }
}
} // namespace ratw
