// Every business its own till (Docs/Design/46-economy-orchestrator.md, Phase 2; the user, 2026-10-06). A great house's
// shops have kept tills since doc 42's Phase 5b; now an owner-run shop, workshop, farm or site keeps one too
// ("till:<position>"), apart from its keeper's purse. What it sells goes in; its materials, wages, tools and upkeep come
// out. The keeper (or farmer) draws a wage from it each day and a share of the week's profit at the reckoning, and lives on
// that like anyone: what it does with its own purse is its own business, and the orchestrator manages the till.
#include "RatwItems.h"
#include "RatwSociety.h"

#include <algorithm>
#include <map>
#include <vector>

namespace ratw
{
void Society::indexTills()
{
    tills_.clear();
    for (auto it = state_.accounts.lower_bound("till:"); it != state_.accounts.end() && it->first.rfind("till:", 0) == 0; ++it)
        tills_[it->first.substr(5)] = it->first;
}

bool Society::ownsTill(const Position& p) const
{
    return (p.role == "merchant" || items::producerFor(p.title)) && !state_.houses.owner.count(p.id);
}

std::string Society::ownTill(const std::string& positionId) const
{
    const auto till = "till:" + positionId;
    if (state_.houses.owner.count(positionId) || !state_.accounts.count(till))
        return {};
    return till;
}

void Society::foundTills()
{
    auto& memory = state_.memory;
    memory.tills = TillsFounded;
    ++memory.revision;
    for (const auto& p : positions_)
    {
        const auto till = "till:" + p.id;
        if (!ownsTill(p) || account(till) || !openAccount(till))
            continue;
        const bool producer = p.role != "merchant";
        // The business's goods and money are the till's (a shop's wares and materials; a farm's yield, for sale), but for
        // a month's living and its keeper's own food (what the shop doesn't sell), which stay its keeper's.
        const auto& keeper = state_.careers.positions[p.id].holder;
        if (const auto* purse = keeper.empty() ? nullptr : account(keeper))
        {
            const auto sold = producer ? std::vector<std::string>{} : wares(keeper);
            std::vector<std::pair<std::string, int>> goods(purse->stock.begin(), purse->stock.end());
            for (const auto& [item, n] : goods)
            {
                const bool ware = std::find(sold.begin(), sold.end(), items::baseOf(item)) != sold.end() ||
                                  std::find(sold.begin(), sold.end(), item) != sold.end();
                const bool business = producer ? forSale(keeper, item) : ware || !edible(item);
                if (n > 0 && business)
                    shift(keeper, till, item, n, 0, "the shop's till");
            }
            const auto living = std::int64_t(MonthDays) * FoodADay;
            if (account(keeper)->cash > living)
                shift(keeper, till, "", 0, account(keeper)->cash - living, "the shop's till");
        }
        // A till begins with its float at least (made once, like a house's business's: RatwHouses.cpp).
        startingMoney(till, floatOf(p.id) - account(till)->cash);
        // Its books open now, so its first week is reckoned (what it was given is no profit).
        if (state_.books.month >= 0)
        {
            state_.books.start[till] = account(till)->cash;
            ++state_.books.revision;
        }
    }
}

void Society::tendTills(std::int64_t day)
{
    (void)day;
    for (const auto& p : positions_)
    {
        const auto till = ownTill(p.id);
        const auto held = state_.careers.positions.find(p.id);
        if (till.empty() || held == state_.careers.positions.end() || held->second.holder.empty() || !account(held->second.holder))
            continue;
        const auto& keeper = held->second.holder;
        const auto floatCash = floatOf(p.id);
        // The keeper's wage first: what the till can spare above half its float, up to OwnerWage.
        if (const auto wage = std::min<std::int64_t>(OwnerWage, account(till)->cash - floatCash / 2); wage > 0)
            shift(till, keeper, "", 0, wage, "the keeper's wage");
        // (Until the orchestrator's channels: doc 46, Phase 5.) Above two floats, a KeeperSurplusShare-th a day goes out:
        // half on hands and its premises (RatwOddJobs.cpp), and four tenths of the rest to its help, a share of the
        // takings, as an owner-run shop's keeper did from its own purse before it had a till (doc 42).
        auto budget = (account(till)->cash - 2 * floatCash) / KeeperSurplusShare;
        const auto town = communityOfResident(keeper);
        if (budget < 10 || town.empty())
            continue;
        Spending note{till, town, 0, {}};
        const auto invested = businessSpends(till, keeper, p, budget / 2);
        budget -= invested;
        std::int64_t shared = 0;
        int hands = 0;
        for (const auto& other : positions_)
        {
            if (other.id == p.id || other.work.cell != p.work.cell || shared + 2 > budget * 4 / 10)
                continue;
            const auto help = state_.careers.positions.find(other.id);
            if (help != state_.careers.positions.end() && !help->second.holder.empty() &&
                shift(till, help->second.holder, "", 0, 2, "surplus: a share of the takings"))
                shared += 2, ++hands;
        }
        note.total = invested + shared;
        note.detail = std::to_string(invested) + "p in hands and its premises, " + std::to_string(shared) + "p shared with " +
                      std::to_string(hands) + " of its help";
        if (note.total > 0)
            spendings_.push_back(std::move(note));
    }
}

void Society::ownersShare(const std::map<std::string, std::int64_t>& profits)
{
    // At the reckoning, after its tax and tithe: a third of what an owner-run business made in the week to its keeper.
    for (const auto& [till, profit] : profits)
    {
        const auto held = profit > 0 ? state_.careers.positions.find(till.substr(5)) : state_.careers.positions.end();
        if (held == state_.careers.positions.end() || held->second.holder.empty() || !account(held->second.holder))
            continue;
        if (const auto share = std::min<std::int64_t>(profit / OwnersShare, account(till)->cash); share > 0)
            shift(till, held->second.holder, "", 0, share, "the owner's share");
    }
}
} // namespace ratw
