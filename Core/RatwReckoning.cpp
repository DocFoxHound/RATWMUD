// Town purses and the reckoning (Docs/Design/42-money-in-circulation.md, Phase 1): each town keeps its own treasury, and
// every ReckonDays (a week, the user's choice on 2026-10-05; it was the month) each resident pays a tenth of the week's
// profit to it and a tenth to its church. Ground rents are still the month's.
// Money only moves; nothing is made.
#include "RatwSociety.h"

#include <cmath>

namespace ratw
{
std::string Society::treasuryOf(const std::string& community) const
{
    if (community == "treasury" || community.rfind("stores:", 0) == 0)
        return community;                              // (Already a treasury: communityOfResident's fallback.)
    const auto own = "stores:" + community;
    return state_.accounts.count(own) ? own : std::string("treasury");
}

std::string Society::churchOf(const std::string& treasury) const
{
    if (treasury.rfind("stores:", 0) == 0)
        return "town:" + treasury.substr(7) + ":church";
    return "town:" + (capital_.empty() ? std::string("treasury") : capital_) + ":church";
}

std::string Society::treasuryOfResident(const std::string& id) const
{
    const auto* r = spec(id);
    if (!r)
        return "treasury";
    // Where it works; one who works out in the country (a farmer, a woodcutter) pays where it lives.
    const auto work = treasuryOf(communityOfResident(id));
    if (work != "treasury")
        return work;
    const auto of = [&](const std::string& cell) { return day_.communityOf ? day_.communityOf(cell) : storeFor(cell); };
    return treasuryOf(of(r->home.cell));
}

void Society::reckon(std::int64_t day, bool force)
{
    auto& books = state_.books;
    const auto month = day / ReckonDays;             // (books.month counts reckonings: weeks now.)
    if (books.month >= 0 && (month > books.month || force))
    {
        if (force || day % MonthDays < ReckonDays)
            collectRents();                             // A month's ground rents to the great houses first (doc 42, 5b).
        std::map<std::string, Reckoning> towns;
        // Everyone, and the great houses (on their businesses' takings, doc 42 Phase 5b).
        std::vector<std::pair<std::string, std::string>> payers;    // Account -> its treasury.
        for (const auto& [id, life] : state_.residents)
            payers.push_back({id, treasuryOfResident(id)});
        for (const auto& h : houses())
            if (account(h.id))
                payers.push_back({h.id, treasuryOf(h.community)});
        for (const auto& [id, treasury] : payers)
        {
            const auto start = books.start.find(id);
            const auto* purse = account(id);
            if (start == books.start.end() || !purse)
                continue;
            const auto unearned = books.unearned.count(id) ? books.unearned.at(id) : 0;
            const auto profit = purse->cash - start->second - unearned;
            auto& town = towns[treasury];
            town.treasury = treasury;
            town.church = churchOf(treasury);
            ++town.residents;
            if (profit < std::max(TaxShare, TitheShare))
                continue;                               // A week at a loss (or of nothing much) pays nothing.
            openAccount(town.church);
            const auto tax = profit / TaxShare, tithe = profit / TitheShare;
            const bool paid = shift(id, treasury, "", 0, tax, "town tax");
            const bool tithed = shift(id, town.church, "", 0, tithe, "tithe");
            town.tax += paid ? tax : 0;
            town.tithes += tithed ? tithe : 0;
            town.payers += paid || tithed;
        }
        // The towns send the capital a tenth of what they took in.
        for (auto& [treasury, town] : towns)
            if (treasury != "treasury" && town.tax >= CapitalShare && shift(treasury, "treasury", "", 0, town.tax / CapitalShare, "capital's share"))
                town.toCapital = town.tax / CapitalShare;
        for (auto& [treasury, town] : towns)
            reckonings_.push_back(std::move(town));
    }
    if (books.month < 0 || month > books.month || force)
    {
        // New books: everyone starts the week with what it has now.
        books.month = month;
        books.start.clear();
        books.unearned.clear();
        ++books.revision;
    }
    // Anyone new (a newcomer, a child born, an older save's residents, a house) starts its books today.
    const auto open = [&](const std::string& id) {
        if (!books.start.count(id))
            if (const auto* purse = account(id))
            {
                books.start[id] = purse->cash;
                ++books.revision;
            }
    };
    for (const auto& [id, life] : state_.residents)
        open(id);
    for (const auto& h : houses())
        open(h.id);
}

std::vector<Society::Reckoning> Society::takeReckonings()
{
    std::vector<Reckoning> out;
    out.swap(reckonings_);
    return out;
}
} // namespace ratw
