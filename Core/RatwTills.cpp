// Every business its own till (Docs/Design/46-economy-orchestrator.md, Phase 2; the user, 2026-10-06). A great house's
// shops have kept tills since doc 42's Phase 5b; now an owner-run shop, workshop, farm or site keeps one too
// ("till:<position>"), apart from its keeper's purse. What it sells goes in; its materials, wages, tools and upkeep come
// out. The keeper (or farmer) draws a wage from it each day, and the business's month's profit is its owner's (doc 64,
// Phase 5: tendOwners).
#include "RatwItems.h"
#include "RatwSociety.h"

#include "RatwCalendar.h"

#include <algorithm>
#include <cmath>
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
            const auto living = pennies(MonthDays * FoodADay, communityOfResident(keeper));
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
    for (const auto& p : positions_)
    {
        const auto till = ownTill(p.id);
        const auto held = state_.careers.positions.find(p.id);
        if (till.empty() || held == state_.careers.positions.end() || held->second.holder.empty() || !account(held->second.holder))
            continue;
        const auto& keeper = held->second.holder;
        // The keeper's wage (its town's table, doc 46, Phase 4), as far as the till can pay it: one short of working
        // money is topped up by its owner (tendOwners).
        const auto ownWage = std::int64_t(std::ceil(employerWage(till) - 1e-9));   // (Its business's wage: doc 64, Phase 6.)
        const auto wage = std::min<std::int64_t>(ownWage, account(till)->cash);
        if (wage > 0)
            shift(till, keeper, "", 0, wage, "the keeper's wage");
        // (A day it can't pay it, for a business short of its wages: its keeper is free for hire work while its owner has
        // stopped topping it up. Round 9.)
        if (const auto s = state_.memory.shortTills.find(till); s != state_.memory.shortTills.end() && wage < ownWage)
            s->second.unpaid = day;
    }
    tendOwners(day);
}

bool Society::ownerUnpaid(const std::string& resident, const Position& job) const
{
    // (Asked at a paid wolf's every decision: with none stopped, at once.)
    if (state_.memory.shortTills.empty() || !workByHours())
        return false;
    const auto s = state_.memory.shortTills.find("till:" + job.id);
    if (s == state_.memory.shortTills.end() || s->second.stopped < 0 || s->second.unpaid != state_.budgetDay)
        return false;
    const auto held = state_.careers.positions.find(job.id);
    return held != state_.careers.positions.end() && held->second.holder == resident;
}

std::string Society::ownerOfBusiness(const std::string& positionId) const
{
    if (const auto house = ownerOf(positionId); !house.empty())
        return house;
    const auto held = state_.careers.positions.find(positionId);
    return held == state_.careers.positions.end() ? std::string() : held->second.holder;
}

std::string Society::landlordOf(const std::string& positionId) const
{
    // The land's (doc 63: a farm, a woodlot, a sheep run, a mine): a producer in a town with great houses works one's
    // ground, the same House always. A House's own businesses, and the town's shops, pay no rent.
    const auto* p = position(positionId);
    if (!p || p->role == "merchant" || !items::producerFor(p->title) || !ownerOf(positionId).empty())
        return {};
    const auto town = communityOfResident(p->founder);
    std::vector<const House*> landlords;
    for (const auto& h : houses())
        if (h.community == town)
            landlords.push_back(&h);
    if (landlords.empty())
        return {};
    return landlords[std::hash<std::string>{}(positionId + "|landlord") % landlords.size()]->id;
}

double Society::staffing(const std::string& positionId) const
{
    const auto* business = position(positionId);
    if (!business)
        return 0;
    // Its staffing a day: its keeper, its help (the posts that work where it does and that it pays: doc 46's float), and
    // at most two of those standing open (posts it can't fill aren't wages it pays: doc 63). (Not every post where it
    // works: a market stall counted the whole market's, a month of dozens of wages for a stall selling a score a day.
    // Nor, with work by the hours, a business that makes nothing: the market's street posts it was given, doc 64,
    // Phase 13.)
    int help = 0, open = 0;
    const auto till = "till:" + positionId;
    if (business->role == "merchant" && (!workByHours() || makesThings(till)))
        for (const auto& p : positions_)
            if (p.id != positionId && p.role != "merchant" && p.work.cell == business->work.cell)
            {
                const auto h = state_.careers.positions.find(p.id);
                if (const auto key = postKey_.find(p.id); key != postKey_.end() && key->second == till)   // (Its employer: tendLabour.)
                    (h == state_.careers.positions.end() || h->second.holder.empty() ? open : help) += 1;
            }
    return (1 + help + std::min(open, 2)) * employerWage(till);   // (Its own wage: doc 64, Phase 6.)
}

std::int64_t Society::workingMoney(const std::string& positionId) const
{
    const auto* business = position(positionId);
    const auto held = state_.careers.positions.find(positionId);
    if (!business)
        return 0;
    const auto keeper = held == state_.careers.positions.end() ? std::string() : held->second.holder;
    const auto till = "till:" + positionId;
    double day = staffing(positionId);
    // And the materials (or goods to sell on) its sales use a day, at what they cost it: with work by the hours, goods to
    // sell on for ResaleDays, not a month (doc 64, Phase 13, round 8).
    double resale = 0;
    if (const auto rates = sellRate_.find(till); rates != sellRate_.end() && !keeper.empty())
        for (const auto& [good, rate] : rates->second)
            if (items::good(good) && !storePriced(good))
            {
                bool resold = false;
                const double cost = rate * materialsCost(keeper, good, &resold);
                (resold && workByHours() ? resale : day) += cost;
            }
    // And a week of its standing orders on the roads (doc 61), paid when the caravan comes; and a field's hands' wages
    // for its window ahead.
    double orders = 0;
    if (const auto o = ordersCost_.find(till); o != ordersCost_.end())
        orders = double(o->second);
    return std::int64_t(std::ceil(MonthDays * day + ResaleDays * resale + orders + fieldWages(till)));
}

double Society::fieldWages(const std::string& till) const
{
    // The work of its window still ahead (doc 64, Phase 13): what's left to sow, or what's sown to bring in, less what
    // its farmer does itself there (a day's hours a day); the rest its hands' days, at its wage and the harvest's pay.
    // From a month before the window opens, its owner's month's take before it, to the window's end: the land's hands
    // paid from what its last harvest sold for, not cut off for want of the money its owner took. (The founding's
    // fields, sown as founded, want none at their sowing.)
    const auto* p = fieldsOn() && till.rfind("till:", 0) == 0 ? position(till.substr(5)) : nullptr;
    const auto* producer = p ? items::producerFor(p->title) : nullptr;
    const auto f = state_.memory.fields.find(till);
    if (!producer || !fieldWork(producer->id) || f == state_.memory.fields.end())
        return 0;
    const auto d = state_.budgetDay % calendar::DaysPerYear;
    const double acre = hoursFor(*producer) * FieldYields;
    double hours = 0;
    std::int64_t days = 0;
    if (d < SowEnd || d >= calendar::DaysPerYear - MonthDays)
    {
        hours = (f->second.acres - f->second.sown) * acre * SowShare;
        days = SowEnd - (d < SowEnd ? d : 0);
    }
    else if (d >= HarvestStart - MonthDays && d < HarvestEnd)
    {
        hours = f->second.toHarvest * acre * (1 - SowShare);
        days = HarvestEnd - std::max<std::int64_t>(d, HarvestStart);
    }
    return std::max(0., hours - double(days * PaidSpells)) / HireSpells * employerWage(till) * HarvestPay;
}

std::int64_t Society::owed(const std::string& till) const
{
    std::int64_t sum = 0;
    for (auto it = state_.memory.credit.lower_bound(till + "|"); it != state_.memory.credit.end() && it->first.rfind(till + "|", 0) == 0; ++it)
        sum += it->second.first;
    return sum;
}

std::int64_t Society::creditLeft(const std::string& till) const
{
    // A week of its turnover (its sales a day, at what they cost it), less what it owes already.
    const auto rates = sellRate_.find(till);
    const auto* p = till.rfind("till:", 0) == 0 ? position(till.substr(5)) : nullptr;
    const auto held = p ? state_.careers.positions.find(p->id) : state_.careers.positions.end();
    if (rates == sellRate_.end() || held == state_.careers.positions.end() || held->second.holder.empty())
        return 0;
    double turnover = 0;
    for (const auto& [good, rate] : rates->second)
        if (items::good(good) && !storePriced(good))
            turnover += rate * normalCost(held->second.holder, good);
    return std::max<std::int64_t>(0, std::int64_t(CreditDays * turnover) - owed(till));
}

bool Society::onCredit(const std::string& seller, const std::string& buyer, const std::string& item, int quantity,
                       std::int64_t price)
{
    const auto a = state_.accounts.find(seller), b = state_.accounts.find(buyer);
    if (a == state_.accounts.end() || b == state_.accounts.end() || seller == buyer || quantity < 1 || quantity > 99 || price < 1 ||
        !itemValid(item) || stock(a->second, item) < quantity || stock(b->second, item) > StockCap - quantity ||
        (!b->second.stock.count(item) && b->second.stock.size() >= MaxGoodsKinds))
        return false;
    addCost(buyer, item, stock(b->second, item), quantity, double(price));
    if ((a->second.stock[item] -= quantity) == 0 && item != "herbs" && item != "meal" && item != "sword")
        a->second.stock.erase(item);
    b->second.stock[item] += quantity;
    auto& debt = state_.memory.credit[buyer + "|" + seller];
    if (debt.first == 0)
        debt.second = state_.budgetDay;
    debt.first += price * quantity;
    soldToday_[seller][items::baseOf(item)] += quantity;
    if (stockAll(a->second, items::baseOf(item)) == 0)
        ranOut_[seller].insert(items::baseOf(item));   // (Its last one: doc 64, Phase 3b.)
    record("bought on credit", buyer, seller, item, quantity, 0);
    return true;
}

std::size_t Society::householdOf(const std::string& resident) const
{
    // Those living in its home, as its household rule counts them (RatwHouseholds.cpp): in a barracks, a bunkhouse,
    // quarters or lodgings, or a home of more than eight grown, each grown wolf is its own.
    const auto me = state_.residents.find(resident);
    if (me == state_.residents.end() || me->second.homeCell.empty() || communalHome(me->second.homeCell))
        return 1;
    std::size_t people = 0, grown = 0;
    for (const auto& [id, life] : state_.residents)
        if (life.homeCell == me->second.homeCell)
        {
            ++people;
            if (const auto* r = spec(id); r && r->age >= 16)
                ++grown;
        }
    return grown > 8 ? 1 : std::max<std::size_t>(1, people);
}

void Society::tendOwners(std::int64_t day)
{
    auto& memory = state_.memory;
    // Debts first, from what came in: the oldest first; a debt unpaid CreditWrittenOff days is the seller's loss.
    for (auto it = memory.credit.begin(); it != memory.credit.end();)
    {
        const auto bar = it->first.find('|');
        const auto buyer = it->first.substr(0, bar), seller = it->first.substr(bar + 1);
        const auto* purse = account(buyer);
        if (const auto pay = std::min(it->second.first, purse ? purse->cash : 0); pay > 0 && account(seller) &&
                                                                                  shift(buyer, seller, "", 0, pay, "a debt repaid"))
            it->second.first -= pay;
        if (it->second.first <= 0 || !account(seller))
            it = memory.credit.erase(it);
        else if (day - it->second.second > CreditWrittenOff)
        {
            record("a debt written off", buyer, seller, "", 0, 0);
            it = memory.credit.erase(it);
        }
        else
            ++it;
    }
    for (const auto& p : positions_)
    {
        const auto till = "till:" + p.id;
        const auto owner = ownerOfBusiness(p.id);
        const auto* purse = account(till);
        if (!purse || owner.empty() || !account(owner) || (p.role != "merchant" && !items::producerFor(p.title)))
            continue;
        const bool weekDay = ownersWeekday(p.id) == day % 7, monthDay = ownersDay(p.id) == day % MonthDays;
        if (!weekDay && !monthDay)
            continue;
        const auto working = workingMoney(p.id);
        auto& putIn = memory.putIn[till];
        if (monthDay)
        {
            // The month's profit: all it holds above its working money (and its debts), to its owner. What comes back
            // counts against what the owner put in.
            if (const auto spare = purse->cash - working - owed(till); spare > 0 && shift(till, owner, "", 0, spare, "the owner's profit"))
                putIn = std::max(0., putIn - double(spare));
        }
        if (weekDay)
        {
            // Rent on a House's land: a share of the week's takings.
            if (const auto landlord = landlordOf(p.id); !landlord.empty())
            {
                openAccount(landlord);
                if (const auto rent = std::min<std::int64_t>(std::int64_t(double(weekTakings_[till]) * RentShare), account(till)->cash); rent > 0)
                    shift(till, landlord, "", 0, rent, "rent");
            }
            weekTakings_.erase(till);
            // A lone owner's business short of a week's wages at StopWeeks looks in a row: its owner stops topping it up,
            // and it trades on with what it has, its hands paid while it can pay them; until a look finds it holding a
            // week's wages again (round 9, the user, 2026-10-27; the model's owner_tops_up stops when what it put in
            // hasn't come back: the business shrinks to what it can pay for). A House tops its own up as before.
            bool stopped = false;
            if (workByHours() && wageRule(OwnersStop) && owner.rfind("house:", 0) != 0)
            {
                const bool wagesShort = double(account(till)->cash) < 7 * staffing(p.id);
                auto s = memory.shortTills.find(till);
                if (wagesShort && s == memory.shortTills.end())
                    s = memory.shortTills.emplace(till, ShortTill{}).first;
                if (s != memory.shortTills.end())
                {
                    auto& t = s->second;
                    t.weeks = wagesShort ? t.weeks + 1 : 0;
                    if (t.weeks == 0)
                        memory.shortTills.erase(s);   // (Paid its way: its owner tops it up again.)
                    else
                    {
                        if (t.stopped < 0 && t.weeks >= StopWeeks)
                            t.stopped = day;
                        stopped = t.stopped >= 0;
                    }
                }
            }
            else
                memory.shortTills.erase(till);
            // Short of a week's working money: its owner puts in what it can spare, until two months' worth hasn't come back.
            putIn *= 12. / 13;
            const auto short_ = working / 4 - account(till)->cash;
            if (short_ > 0 && putIn <= 2. * working && !stopped)
            {
                // (A keeper keeps a week's food for its household; a House gives what it has.)
                const auto floor = owner.rfind("house:", 0) == 0 ? 0
                                   : workByHours()                ? pennies(7 * FoodADay * double(householdOf(owner)), accountTown(owner))
                                                                  : pennies(KeeperReserve, accountTown(owner));
                if (const auto give = std::min<std::int64_t>(short_, account(owner)->cash - floor); give > 0 &&
                                                                                                    shift(owner, till, "", 0, give, "an owner's investment"))
                    putIn += double(give);
            }
        }
        if (putIn <= 0)
            memory.putIn.erase(till);
    }
    ++memory.revision;
}
} // namespace ratw
