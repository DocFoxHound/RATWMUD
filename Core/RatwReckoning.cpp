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
    std::int64_t floor = pennies(HouseFloor, "");     // (And a float for each of its businesses.)
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
    const auto town = communityOfResident(id);
    const std::int64_t living = pennies(MonthDays * FoodADay, town);
    if (const auto* job = jobOf(id); job && job->role == "merchant" && tillOf(id) == id)
        return living + pennies(KeeperReserve, accountTown(id)) + floatOf(job->id);   // (Its reserve as spendable's.)
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
        reckonedDay_ = day;                          // (The economy's monitor weighs the week that evening: doc 46.)
        std::map<std::string, Reckoning> towns;
        // Everyone, and the great houses (on their businesses' takings, doc 42 Phase 5b).
        std::vector<std::pair<std::string, std::string>> payers;    // Account -> its treasury.
        for (const auto& [id, life] : state_.residents)
            payers.push_back({id, treasuryOfResident(id)});
        for (const auto& h : houses())
            if (account(h.id))
                payers.push_back({h.id, treasuryOf(h.community)});
        // (A business isn't taxed: its profit is its owner's, and taxed as theirs. Doc 64, Phase 7.)
        flushEarnings();                            // (Today's too, when reckoned on demand.)
        // (Each town's tax this week at the full rate, round 10: what its target is reckoned on. And its taxpayers' week's
        // incomes, for the rebate: an option, off unless asked.)
        const bool ease = workByHours() && wageRule(TaxEase);
        std::map<std::string, double> fullTax;
        std::map<std::string, std::vector<std::pair<std::string, double>>> taxpayers;
        for (const auto& [id, treasury] : payers)
        {
            const auto* purse = account(id);
            if (!purse)
                continue;
            auto& town = towns[treasury];
            town.treasury = treasury;
            town.church = churchOf(treasury);
            ++town.residents;
            // The tax rate (the Dungeon Master's: TaxRate to start) and TitheRate of the week's income (doc 64, Phases 7 and
            // 12; doc 63's model), as far as its purse goes.
            const auto income = state_.memory.weekIncome.count(id) ? state_.memory.weekIncome.at(id) : 0.;
            if (income < 1)
                continue;
            openAccount(town.church);
            // (A town's own rate since round 10, eased while its stores hold over their target: Society::easeTownTax.)
            const double rate = ease && treasury != "treasury" ? townTaxRate(treasury) : taxRate();
            if (ease && treasury != "treasury")
            {
                fullTax[treasury] += income * taxRate();
                taxpayers[treasury].push_back({id, income});
            }
            const auto tax = std::min<std::int64_t>(purse->cash, std::llround(income * rate)),
                       tithe = std::min<std::int64_t>(purse->cash - tax, std::llround(income * TitheRate));
            const bool paid = shift(id, treasury, "", 0, tax, "town tax");
            const bool tithed = shift(id, town.church, "", 0, tithe, "tithe");
            town.tax += paid ? tax : 0;
            town.tithes += tithed ? tithe : 0;
            town.payers += paid || tithed;
        }
        // (An owner's share of its business's profit is paid monthly now, on the business's own day: doc 64, Phase 5.)
        // The towns send the capital a tenth of what they took in.
        // Since round 11 (the user, 2026-10-29: "make the tax fluctuate depending on the wealth of the towns";
        // ShareByWealth, with work by the hours), by the town's wealth and the capital's (shareByWealth, below).
        const bool byWealth = workByHours() && wageRule(ShareByWealth) && day >= MonthDays;
        // (Round 13, lane 2, SurplusByNeed: the capital's holding leaves its holdings-tax pot out, as a town's does, now
        // that the towns' surplus goes into it.)
        const bool byNeed = byWealth && surplusByNeed();
        const auto* capitalPurse = account("treasury");
        const double capitalMonth = CapitalMonths * monthIncomeOf("treasury");
        const double capitalCash = capitalPurse ? std::max(0., double(capitalPurse->cash) - (byNeed ? biteHeld("treasury") : 0.)) : 0.;
        const double capitalHeld = capitalPurse && capitalMonth >= 1 ? capitalCash / capitalMonth : 1.;
        // What each town is asked to send on to the city's poor (SurplusByNeed, below), and its pull.
        struct Ask
        {
            std::string treasury;
            double asked = 0, pull = 0;
        };
        std::vector<Ask> asks;
        pullSent_.clear();
        for (auto& [treasury, town] : towns)
        {
            if (treasury == "treasury" || !account(treasury))
                continue;
            auto share = std::int64_t(double(town.tax / CapitalShare));
            double held = 0, target = 0;            // (ShareByWealth's: the town's holding and its target.)
            if (byWealth)
            {
                // The town's holding (its stores' week's mean, as far as they hold it now: the rebate's) against its target
                // (TaxEase's, two months of its tax at the full rate; none, and it counts as at its target). A tenth of its
                // tax times its holding over its target (nothing empty, a tenth at its target, ShareTownMost tenths at
                // twice it and over), times the capital's want: 2 less its holding over its own target (a month of its
                // income), ShareCapitalLeast at 1.5 and over, ShareCapitalMost empty. So 0 to 30% of the tax. And a
                // ShareSurplus of what the town holds over its target a week, all of it while the capital is under its
                // target, less as it holds more, none at twice: a trading town's tax eases to nothing (TaxEase), so the
                // tax alone can't bring its surplus to the realm. Never below its target by the surplus part, nor more
                // than it holds.
                // (Its holdings tax not yet spent back left out: the town's to spend, at its week, on its works. Round 12,
                // lane B, HoldingsBite: so the bite isn't sent to the capital, nor rebated, nor saved.)
                const double cash = std::max(0., double(account(treasury)->cash) - biteHeld(treasury));
                target = townTaxTarget(treasury);
                held = target >= 1 ? std::min(cash, townHeldMean(treasury) > 0 ? townHeldMean(treasury) : cash) : 0;
                const double wealth = target >= 1 ? std::clamp(held / target, 0., ShareTownMost) : 1.;
                const double want = std::clamp(2 - capitalHeld, ShareCapitalLeast, ShareCapitalMost);
                const double ofTax = double(town.tax) / CapitalShare * wealth * want;
                const double over = std::max(0., held - target), surplus = over * ShareSurplus * std::clamp(2 - capitalHeld, 0., 1.);
                share = std::llround(std::min(ofTax + std::min(surplus, std::max(0., cash - target - ofTax)), cash));
            }
            if (share > 0 && shift(treasury, "treasury", "", 0, std::min(share, account(treasury)->cash), "capital's share"))
            {
                town.toCapital = share;
                held = std::max(0., held - double(share));
                // (The town's mean holding is what's left after it: TaxEase and the rebate, next, weigh what the town still
                // holds, not what it sent, so its surplus isn't counted twice: to the capital, then back to its taxpayers.)
                if (byWealth)
                    if (const auto mean = state_.memory.heldMean.find(treasury); mean != state_.memory.heldMean.end())
                        mean->second = std::max(0., mean->second - double(share));
                if (noteTowns_)
                    sharesSent_[treasury] += share;
            }
            // A town's surplus where the need is (round 13, lane 2; SurplusByNeed; the user, 2026-10-29): its pull (the
            // city's need less its own) times NeedShare of what it still holds over two targets (where TaxEase has eased
            // its tax to nothing and the rebate begins; about two weeks of a trading town's road money), asked of it for
            // the capital's holdings-tax pot, which pays the city's crews, relief and day labour. In round 12's run
            // Ridgemere lost 1.43M between days 91 and 728 while Oakhaven gained 667k and Larkfield 533k, and the share's
            // surplus part was nothing all along (the capital held 2.5-3.6 months of its income under CapitalByIncome).
            if (byNeed)
                if (const double pull = surplusPull(treasury.substr(7)); pull > 0)
                {
                    pullSent_[treasury] = pull;
                    if (const double asked = pull * NeedShare * std::max(0., held - 2 * target); asked >= 1)
                        asks.push_back({treasury, asked, pull});
                }
        }
        // Sent, every town's ask scaled to the room the capital's pot has (CapitalBiteRoom weeks of its crew at the most):
        // not the capital's income (Society::record leaves the kind out), and taken off the town's mean holding as its
        // share is, so TaxEase and its rebate weigh what it still holds. Its pull as sent cuts its rebate (rebateTownTax).
        // (Without HoldingsBite there is no pot: what is sent is the capital's own, spent by its week's budget.)
        if (!asks.empty())
        {
            double all = 0;
            for (const auto& a : asks)
                all += a.asked;
            const double scale = std::min(1., capitalPotRoom() / all);
            for (const auto& a : asks)
            {
                const auto sent = std::min<std::int64_t>(std::llround(a.asked * scale),
                                                         std::int64_t(std::max(0., double(account(a.treasury)->cash) - biteHeld(a.treasury))));
                if (sent > 0 && shift(a.treasury, "treasury", "", 0, sent, "a town's surplus sent on"))
                {
                    if (wageRule(HoldingsBite))
                        state_.memory.biteLeft["treasury"] += double(sent);
                    if (const auto mean = state_.memory.heldMean.find(a.treasury); mean != state_.memory.heldMean.end())
                        mean->second = std::max(0., mean->second - double(sent));
                    if (noteTowns_)
                        sentOn_[a.treasury] += sent;
                    pullSent_[a.treasury] = a.pull * std::min(1., double(sent) / std::max(1., std::round(a.asked)));
                }
                else
                    pullSent_[a.treasury] = 0;
            }
        }
        // (No wealth tithe: tax and tithe are on income now, doc 64, Phase 7.)
        state_.memory.weekIncome.clear();
        if (ease)
        {
            easeTownTax(fullTax);
            if (wageRule(TaxRebate))
                rebateTownTax(taxpayers);
        }
        openAccount(SharedChurch);
        institutionsWeek(day);                      // Each institution's week's budget, spent (RatwInstitutions.cpp).
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

double Society::capitalPotRoom() const
{
    // (Round 13, lane 2, SurplusByNeed: CapitalBiteRoom weeks of the most the capital's crew can be paid, estateMost at the
    // city's going rate, less what its holdings-tax pot holds.)
    return std::max(0., CapitalBiteRoom * 7 * double(estateMost()) * std::ceil(goingRate(capital()) - 1e-9) - biteHeld("treasury"));
}

std::vector<Society::Reckoning> Society::takeReckonings()
{
    std::vector<Reckoning> out;
    out.swap(reckonings_);
    return out;
}
} // namespace ratw
