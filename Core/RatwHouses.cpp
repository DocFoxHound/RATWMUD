// Great houses and the businesses they own (Docs/Design/42-money-in-circulation.md, Phase 5b). A house owns the
// industry of its town and some of its shops. Each such business has a till of its own; its keeper is its manager, paid
// a wage from the till, and everything above the till's float goes to the house each night. A house props up a till
// that runs low, and sells a business that keeps needing it to another house. Money only moves.
#include "RatwItems.h"
#include "RatwSociety.h"

#include <algorithm>
#include <cctype>

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
        if (std::none_of(houses_.begin(), houses_.end(), [&](const House& h) { return h.id == id; }))
            houses_.push_back({id, name, p.id, community});
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
    if (state_.houses.owner.empty())
        return keeper;
    const auto* job = jobOf(keeper);
    if (!job || !state_.houses.owner.count(job->id))
        return keeper;
    const auto till = "till:" + job->id;
    return state_.accounts.count(till) ? till : keeper;
}

std::int64_t Society::floatOf(const std::string& positionId) const
{
    // A week of the business's running: its manager's wage and its help's (the posts that work where it does), and its
    // materials (placeholder).
    const auto* business = position(positionId);
    if (!business)
        return 0;
    int help = 0;
    for (const auto& p : positions_)
        help += p.id != positionId && p.role != "merchant" && p.work.cell == business->work.cell;
    return FloatDays * (ManagerWage + 6 * help) + 60;
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
        // The industry (workshops, yards) is the houses'; the shops, about half of them, by lot.
        const auto* business = items::businessFor(p.title);
        const auto lot = std::hash<std::string>{}(p.id + "|house");
        const bool industry = business && (business->kind == "works" || business->kind == "yard");
        if (!industry && lot % 2)
            continue;
        const auto& house = *town->second[lot / 2 % town->second.size()];
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
            const auto keep = std::int64_t(ManagerWage) * FloatDays;
            if (account(keeper)->cash > keep)
                shift(keeper, till, "", 0, account(keeper)->cash - keep, "the shop's till");
        }
    }
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
            shift(till, manager, "", 0, std::min<std::int64_t>(ManagerWage, account(till)->cash), "manager's wage");
        // Then everything above the float to the house; or, run low, the house props it up.
        const auto floatCash = floatOf(pid);
        const auto cash = account(till)->cash;
        auto& propped = state.propped[pid];
        propped.erase(std::remove_if(propped.begin(), propped.end(), [&](double d) { return d <= day - MonthDays; }), propped.end());
        if (cash > floatCash)
            shift(till, house, "", 0, cash - floatCash, "house takings");
        else if (cash < floatCash / 2)
        {
            // A struggling day, propped up or not (a house with nothing to spare can't).
            if (account(house)->cash > 0)
                shift(house, till, "", 0, std::min(floatCash - cash, account(house)->cash), "propped up by the house");
            propped.push_back(double(day));
        }
        if (propped.empty())
            state.propped.erase(pid);
        else if (int(propped.size()) >= ProppedDays)
        {
            // It keeps losing: sold to the house that can best afford it (one of its own town's first), for two floats.
            const auto price = floatCash * 2;
            const auto* here = position(pid);
            const auto town = here ? communityOfResident(here->founder) : std::string();
            const House* buyer = nullptr;
            for (const auto& h : houses())
            {
                const auto* purse = account(h.id);
                if (h.id == house || !purse || purse->cash < price)
                    continue;
                const auto* best = buyer ? account(buyer->id) : nullptr;
                if (!buyer || (h.community == town) > (buyer->community == town) ||
                    ((h.community == town) == (buyer->community == town) && purse->cash > best->cash))
                    buyer = &h;
            }
            if (buyer)
                sellBusiness(pid, buyer->id, price);
        }
    }
}

void Society::collectRents()
{
    // Each business in a town with great houses pays a house a month's rent for its ground (the user, 2026-10-05):
    // one of the town's houses that doesn't own it, the same one month to month. A house's own businesses pay it
    // nothing; another house's pay it like anyone's.
    std::map<std::string, std::vector<const House*>> byTown;
    for (const auto& h : houses())
        byTown[h.community].push_back(&h);
    for (const auto& p : positions_)
    {
        if (p.role != "merchant")
            continue;
        const auto town = byTown.find(communityOfResident(p.founder));
        if (town == byTown.end())
            continue;
        const auto owner = ownerOf(p.id);
        std::vector<const House*> landlords;
        for (const auto* h : town->second)
            if (h->id != owner)
                landlords.push_back(h);
        const auto held = state_.careers.positions.find(p.id);
        if (landlords.empty() || held == state_.careers.positions.end() || held->second.holder.empty())
            continue;
        const auto& landlord = *landlords[std::hash<std::string>{}(p.id + "|landlord") % landlords.size()];
        const auto till = tillOf(held->second.holder);
        openAccount(landlord.id);
        if (const auto* purse = account(till); purse && purse->cash > 0)
            shift(till, landlord.id, "", 0, std::min<std::int64_t>(MonthlyRent, purse->cash), "rent");
    }
}
} // namespace ratw
