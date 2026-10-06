// Town purses and the reckoning (Docs/Design/42-money-in-circulation.md, Phase 1): each town keeps its own treasury, and
// every ReckonDays (a week, the user's choice on 2026-10-05; it was the month) each resident pays a tenth of the week's
// profit to it and a tenth to its church. Ground rents are still the month's.
// Money only moves; nothing is made.
#include "RatwSociety.h"

#include <algorithm>
#include <cstdlib>

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

std::int64_t Society::houseFloor(const std::string& house) const
{
    std::int64_t floor = 100;                       // (And a float for each of its businesses.)
    for (const auto& [pid, owner] : state_.houses.owner)
        if (owner == house)
            floor += floatOf(pid);
    return floor;
}

bool Society::trial(const char* name)
{
    static const std::string on = [] {
        const char* v = std::getenv("RATW_TRIAL");
        return "," + std::string(v ? v : "") + ",";
    }();
    return on.find("," + std::string(name) + ",") != std::string::npos;
}

std::int64_t Society::wealthLine(const std::string& id) const
{
    if (id.rfind("house:", 0) == 0)
        return houseFloor(id);
    if (id.rfind("till:", 0) == 0)
        return 2 * floatOf(id.substr(5));            // (An owner-run business's till: two floats, doc 46.)
    // (TRIAL tithe_relative: the line is at least twice the land's median purse, so a richer land isn't all tithed.)
    const std::int64_t living = std::max<std::int64_t>(MonthDays * FoodADay, trial("tithe_relative") ? 2 * landMedian_ : 0);
    if (const auto* job = jobOf(id); job && job->role == "merchant" && tillOf(id) == id)
        return living + KeeperReserve + floatOf(job->id);
    return living;
}

std::string Society::churchOf(const std::string& treasury) const
{
    (void)treasury;                                 // (Every town's church: one purse, SharedChurch.)
    return SharedChurch;
}

void Society::joinChurches()
{
    std::vector<std::string> old;
    for (const auto& [id, a] : state_.accounts)
        if (id != SharedChurch && id.rfind("town:", 0) == 0 && id.size() > 12 && id.compare(id.size() - 7, 7, ":church") == 0 && a.cash > 0)
            old.push_back(id);
    for (const auto& id : old)
        if (const auto cash = account(id)->cash; cash > 0)
        {
            openAccount(SharedChurch);
            shift(id, SharedChurch, "", 0, cash, "the churches' purses joined");
        }
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
        reckonedDay_ = day;                          // (The economy orchestrator decides that evening: doc 46.)
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
        // And every owner-run business's till (doc 46, Phase 2), where it works.
        for (const auto& p : positions_)
            if (const auto till = ownTill(p.id); !till.empty())
                payers.push_back({till, treasuryOf(communityOfResident(p.founder))});
        std::map<std::string, std::int64_t> tillProfits;   // What each owner-run till made, after its tax and tithe.
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
            // (Progressive: a fifth of the part of the week's profit above TaxBand.)
            // (The same rates for all (the user, 2026-10-06), but nothing from one under the poverty line.)
            if (const auto* r = spec(id); r && purse->cash < PovertyLine)
                continue;
            const auto tax = profit / TaxShare + std::max<std::int64_t>(0, profit - TaxBand) / TaxShare, tithe = profit / TitheShare;
            const bool paid = shift(id, treasury, "", 0, tax, "town tax");
            const bool tithed = shift(id, town.church, "", 0, tithe, "tithe");
            town.tax += paid ? tax : 0;
            town.tithes += tithed ? tithe : 0;
            town.payers += paid || tithed;
            if (id.rfind("till:", 0) == 0)
                tillProfits[id] = profit - (paid ? tax : 0) - (tithed ? tithe : 0);
        }
        ownersShare(tillProfits);                   // Its keeper's share of what the business made (RatwTills.cpp).
        // The towns send the capital a tenth of what they took in.
        for (auto& [treasury, town] : towns)
            if (treasury != "treasury" && town.tax >= CapitalShare && shift(treasury, "treasury", "", 0, town.tax / CapitalShare, "capital's share"))
                town.toCapital = town.tax / CapitalShare;
        // The wealth tithe: a WealthTitheShare-th of what anyone holds above its comfortable line, to the church.
        openAccount(SharedChurch);
        for (const auto& [id, treasury] : payers)
            if (const auto* purse = account(id))
                if (const auto due = (purse->cash - wealthLine(id)) / WealthTitheShare; due > 0 && shift(id, SharedChurch, "", 0, due, "a wealth tithe"))
                    towns[treasury].wealthTithe += due;
        // The capital shares what it holds above four weeks' spending: half of it, to the towns by how many of their folk
        // are short of a week's food.
        if (const auto* capital = account("treasury"))
        {
            const auto usual = state_.memory.outgoing.count("treasury") ? state_.memory.outgoing.at("treasury") : 0.;
            const auto spare = (capital->cash - std::int64_t(MonthDays * usual)) / 2;
            std::map<std::string, std::int64_t> short_;
            std::int64_t all = 0;
            for (const auto& [id, life] : state_.residents)
                if (const auto* p = account(id); p && p->cash < 7 * FoodADay)
                    if (const auto t = treasuryOfResident(id); t != "treasury")
                        ++short_[t], ++all;
            for (const auto& [treasury, n] : short_)
                if (const auto part = spare > 0 && all > 0 ? spare * n / all : 0;
                    part > 0 && account(treasury) && shift("treasury", treasury, "", 0, part, "from the capital, for the poor"))
                    towns[treasury].fromCapital += part;
        }
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
    for (const auto& p : positions_)
        if (const auto till = ownTill(p.id); !till.empty())
            open(till);
}

std::vector<Society::Reckoning> Society::takeReckonings()
{
    std::vector<Reckoning> out;
    out.swap(reckonings_);
    return out;
}
} // namespace ratw
