// Odd jobs and subsidies (the user, 2026-10-05; Docs/Design/42-money-in-circulation.md). A town or church with money to
// spare posts a day's menial work, paid when it is done, to put money in the pockets of those with none and keep the
// producers turning; and covers the wage of a worker whose employer can't pay. Nothing is made: the work is real (goods
// bought and carried, materials used, what the land gives brought in), and so is the pay.
#include "RatwItems.h"
#include "RatwSociety.h"

#include <algorithm>
#include <cmath>

namespace ratw
{
std::string Society::postOddJob(const std::string& payer, const std::string& community, const std::string& kind, int slots,
                                std::int64_t pay, const Spot& at, const std::string& what)
{
    const auto* purse = account(payer);
    if (!purse || slots < 1 || pay < slots || purse->cash < pay || community.empty() || kind.empty())
        return {};
    OddJob j;
    j.id = "odd" + std::to_string(++nextOddJob_);
    j.payer = payer;
    j.community = community;
    j.kind = kind;
    j.what = what.substr(0, 80);
    j.from = j.to = at;
    j.slots = std::min(slots, 5);
    j.pay = pay;
    j.forChildren = false;
    oddJobs_.push_back(std::move(j));
    ++oddVersion_;
    return oddJobs_.back().id;
}

void Society::keepUpPremises()
{
    for (auto it = state_.memory.improved.begin(); it != state_.memory.improved.end();)
    {
        const auto& [pid, level] = *it;
        const auto* job = position(pid);
        const auto held = state_.careers.positions.find(pid);
        std::int64_t spent = 0, upkeep = 0;
        if (job && level > 0 && held != state_.careers.positions.end() && !held->second.holder.empty())
        {
            const auto& keeper = held->second.holder;
            const auto community = communityOfResident(keeper);
            upkeep = pennies(UpkeepALevel * level, community);
            // Who pays: the business's till; a house's business, its house, if the till can't.
            std::string payer = tillOf(keeper);
            if (const auto owner = state_.houses.owner.find(pid);
                owner != state_.houses.owner.end() && (!account(payer) || spendable(payer) < upkeep))
                payer = owner->second;
            std::vector<std::string> shops;
            for (const auto& p : positions_)
                if (p.role == "merchant" && p.id != pid)
                    if (const auto h = state_.careers.positions.find(p.id); h != state_.careers.positions.end() && !h->second.holder.empty() &&
                                                                          communityOfResident(h->second.holder) == community)
                        shops.push_back(h->second.holder);
            std::map<std::string, int> got;
            if (account(payer))
                spent = buyForSurplus(payer, shops, [](const std::string& item) {
                    return item == "planks" || item == "nails" || item == "lime" || item == "bricks" || item == "roof_tiles" || item == "timber";
                }, std::min<std::int64_t>(upkeep, spendable(payer)), "keeping up the premises", &got);
            for (const auto& [item, n] : got)
                consume(payer, item, n, "used in keeping up the premises");
        }
        auto& weeks = state_.memory.neglected[pid];
        weeks = level <= 0 || (upkeep > 0 && spent * 2 >= upkeep) ? 0 : weeks + 1;
        if (weeks >= 2)
        {
            weeks = 0;
            if (--it->second <= 0)
            {
                state_.memory.neglected.erase(pid);
                it = state_.memory.improved.erase(it);
                ++state_.memory.revision;
                continue;
            }
        }
        ++state_.memory.revision;
        ++it;
    }
}

const std::string& Society::friendGroup(const std::string& child) const
{
    static const std::string none;
    const auto found = friendGroups_.find(child);
    return found == friendGroups_.end() ? none : found->second;
}

void Society::indexOddJobs()
{
    if (oddIndexed_ == oddVersion_)
        return;
    oddIndexed_ = oddVersion_;
    oddHeld_.clear();
    oddByTown_.clear();
    oddHiresByTown_.clear();
    oddFarmHires_.clear();
    for (std::size_t i = 0; i < oddJobs_.size(); ++i)
    {
        const auto& j = oddJobs_[i];
        if (j.ended)
            continue;                                // (A hire ended today: shut, to its holders and to those looking.)
        for (const auto& [who, stage] : j.stage)
            if (stage < 2)
                oddHeld_.try_emplace(who, i);        // (The first it holds, as a search down the list finds.)
        oddByTown_[j.community].push_back(i);
        if (j.until >= 0 && (j.kind == "a hand" || j.kind == "a hand at the shop" || crewPlace(j)))
            oddHiresByTown_[j.community].push_back(i);   // (And a House's crews: its posts, doc 64, Phase 13, round 6.)
        if (j.until >= 0 && j.kind == "a hand" && farmHire(j))
            oddFarmHires_.push_back(i);
    }
}

const Society::OddJob* Society::oddJobFor(const std::string& id, const Position& job, bool jobless, int age, bool poor, double hour,
                                         std::int64_t ownDayPay, bool* claims, bool crewFree)
{
    if (!claims)
        indexOddJobs();                              // (On the game thread: the threads' was made before they began.)
    const bool indexed = oddIndexed_ == oddVersion_;
    // Its job under way (one whose share it has done no longer holds it: it may take another).
    if (indexed)
    {
        if (const auto held = oddHeld_.find(id); held != oddHeld_.end() && held->second < oddJobs_.size())
            if (const auto mine = oddJobs_[held->second].stage.find(id); mine != oddJobs_[held->second].stage.end() && mine->second < 2)
                return &oddJobs_[held->second];
    }
    else
        for (const auto& j : oddJobs_)
            if (const auto mine = j.stage.find(id); !j.ended && mine != j.stage.end() && mine->second < 2)
                return &j;
    // Who seeks them out: those without other work, and children (eight and up, not apprenticed), in the day; the poor
    // and hungry from eight, others from ten. Children would rather not work (the user, 2026-10-05): they look from noon,
    // on one day in three unless poor or hungry; and work that isn't for children (a hand at a farm, a hunt) is theirs only
    // when no grown wolf has taken it by mid-afternoon. (Later: such work should teach a skill, toward apprenticeships and
    // in time taking over a business: Docs/Design/42, "Later".)
    const bool child = age >= 8 && age < 16;
    if (child && (hour < 12 || (!poor && std::hash<std::string>{}(id + "|work|" + std::to_string(surplusDay_)) % 3 != 0)))
        return nullptr;
    // (A grown wolf free for an institution's crew looks for one at any hour of its day: round 7.)
    crewFree = crewFree && !child && workByHours();
    const bool early = hour < (poor ? 8 : 10);
    if (hour >= 18 || (early && !crewFree) || job.role != "civilian")
        return nullptr;
    const auto community = communityOfResident(id);
    // Its town's jobs, and the farms' hires (which anyone from a city may take), in the list's order.
    static const std::vector<std::size_t> none;
    const auto town = indexed ? oddByTown_.find(community) : oddByTown_.end();
    const auto& here = town != oddByTown_.end() ? town->second : none;
    std::vector<std::size_t> all;
    const auto candidates = [&]() -> const std::vector<std::size_t>& {
        if (!indexed)
        {
            all.resize(oddJobs_.size());
            for (std::size_t i = 0; i < all.size(); ++i)
                all[i] = i;
            return all;
        }
        return here;
    };
    // A wolf with a post of its own looks, of a morning, for a business's hire paying it better (the user, 2026-10-05:
    // unfilled work pulls workers off other employment): `ownDayPay` is what it must pay (Society::lookingFor, doc 64).
    const bool better = ownDayPay > 0 && hour >= 8 && hour < 10 && [&] {
        for (const auto i : candidates())
            if (const auto& j = oddJobs_[i]; j.until >= 0 && !j.ended && j.slots > 0 && j.pay / j.slots >= ownDayPay &&
                                             int(j.stage.size()) < j.slots && j.community == community)
                return true;
        return false;
    }();
    // (Called to the land: a field's hire only.)
    const bool called = !jobless && !child && !better && hour >= 8 && hour < 10 && calledToTheLand(id, job, age, surplusDay_);
    const bool looks = !early && (jobless || child || better || called || (poor && !job.paid));
    if (!looks && !crewFree)
        return nullptr;
    const auto& group = child ? friendGroup(id) : std::string();
    // (A farm's hire may be taken by anyone living in a city too: RatwFarmhands.cpp.)
    const bool fromCity = !child && cities_.count(community) > 0;
    // (A hand about the home is for another household: its own would be paying itself. Money that stops, doc 46.)
    const auto ownHome = [&](const OddJob& j) {
        const auto payer = state_.residents.find(j.payer), me = state_.residents.find(id);
        return payer != state_.residents.end() && me != state_.residents.end() && payer->second.homeCell == me->second.homeCell;
    };
    const auto open = [&](const OddJob& j) {
        return !j.ended && (j.community == community || (fromCity && j.until >= 0 && j.kind == "a hand" && farmHire(j))) &&
               int(j.stage.size()) < j.slots && !j.stage.count(id) && (!child || j.forChildren || hour >= 14) &&
               (j.kind != "about the home" || !ownHome(j));
    };
    // The places to look, in order: its town's, and a city wolf's the farms' hires too.
    const auto* look = &candidates();
    if (indexed && fromCity && !oddFarmHires_.empty())
    {
        std::merge(here.begin(), here.end(), oddFarmHires_.begin(), oddFarmHires_.end(), std::back_inserter(all));
        all.erase(std::unique(all.begin(), all.end()), all.end());
        look = &all;
    }
    // A child joins a friend on a job first, then one with room for a party; anyone else, the first open one. The
    // jobless and the idle (not a child, nor one called to the land) take the best-paid business's hire open to them
    // first, the first of the best by the list, before chores (doc 63's model: the unemployed take the best-paid post
    // they find; doc 64, Phase 13), so a business that raised its wage draws them. A House's crews are its posts, weighed
    // with them by their pay (round 6: they were taken after every business's hire, by their place in the list, and went
    // mostly untaken). (With work by the hours: at the seconds' pace, the first open one, as it was.)
    OddJob* chosen = nullptr;
    // A wolf free for a crew (round 7; the model's staff_to, the idle first): the best-paid institution's crew place open to
    // it, and if it is out of work the businesses' hires too, at any hour; at equal pay a business's hire before a crew
    // (the land first: its hands stay on the farms), then the first by the list. None: at the hour it may look at all, on.
    if (crewFree)
    {
        std::int64_t best = 0;
        bool bestCrew = false;
        const auto bestOf = [&](const std::vector<std::size_t>& list) {
            for (const auto i : list)
            {
                auto& j = oddJobs_[i];
                const bool crew = crewPlace(j);
                if (j.until < 0 || j.slots <= 0 || !(crew || (jobless && (j.kind == "a hand" || j.kind == "a hand at the shop"))))
                    continue;
                const auto pay = j.pay / j.slots;
                if ((pay > best || (pay == best && chosen && (crew == bestCrew ? &j < chosen : !crew))) && open(j))
                    best = pay, bestCrew = crew, chosen = &j;
            }
        };
        if (!indexed)
            bestOf(*look);
        else
        {
            if (const auto hires = oddHiresByTown_.find(community); hires != oddHiresByTown_.end())
                bestOf(hires->second);
            if (fromCity && jobless)
                bestOf(oddFarmHires_);
        }
        if (!chosen && !looks)
            return nullptr;
    }
    if (!chosen && jobless && !child && !called && workByHours())
    {
        std::int64_t best = 0;
        const auto bestOf = [&](const std::vector<std::size_t>& list) {
            for (const auto i : list)
                if (auto& j = oddJobs_[i]; j.until >= 0 && j.slots > 0 && (j.kind == "a hand" || j.kind == "a hand at the shop" || crewPlace(j)) &&
                                           (j.pay / j.slots > best || (j.pay / j.slots == best && chosen && &j < chosen)) && open(j))
                    best = j.pay / j.slots, chosen = &j;
        };
        // (Its town's hires, and a city wolf's the farms', from the index: the list is long with chores.)
        if (!indexed)
            bestOf(*look);
        else
        {
            if (const auto hires = oddHiresByTown_.find(community); hires != oddHiresByTown_.end())
                bestOf(hires->second);
            if (fromCity)
                bestOf(oddFarmHires_);
        }
    }
    if (child && !group.empty())
        for (const auto i : *look)
            if (auto& j = oddJobs_[i]; open(j) && std::any_of(j.stage.begin(), j.stage.end(), [&](const auto& t) { return friendGroup(t.first) == group; }))
            {
                chosen = &j;
                break;
            }
    for (int pass = child ? 0 : 1; !chosen && pass < 2; ++pass)
        for (const auto i : *look)
            if (auto& j = oddJobs_[i]; open(j) && (pass == 1 || j.slots >= 2) && (!better || jobless || (j.until >= 0 && j.pay / j.slots >= ownDayPay)) &&
                                       (!called || j.field))
            {
                chosen = &j;
                break;
            }
    if (!chosen)
        return nullptr;
    if (claims)
    {
        *claims = true;                             // (Only asked: nothing is claimed.)
        return nullptr;
    }
    chosen->stage[id] = 0;
    chosen->progress[id] = 0;
    if (indexed)
        oddHeld_.try_emplace(id, std::size_t(chosen - oddJobs_.data()));
    if (chosen->until >= 0 && farmHire(*chosen))
        lodgeHand(id, *chosen);                     // (From elsewhere: to the farm's bunkhouse.)
    return chosen;
}

void Society::advanceOddJob(const std::string& id, int seconds)
{
    indexOddJobs();
    auto it = oddJobs_.end();
    if (const auto held = oddHeld_.find(id); held != oddHeld_.end() && held->second < oddJobs_.size() && !oddJobs_[held->second].ended)
        if (const auto mine = oddJobs_[held->second].stage.find(id); mine != oddJobs_[held->second].stage.end() && mine->second < 2)
            it = oddJobs_.begin() + std::ptrdiff_t(held->second);
    if (it == oddJobs_.end())
        return;
    auto& j = *it;
    auto& stage = j.stage[id];
    if (stage == 0)
    {
        // A spell's work where it was sent (a hand, a trip), then on.
        // A spell's work (a business's hire: a day's, HireSpells).
        const int spells = j.until >= 0 ? HireSpells : 1;
        if ((j.kind == "a hand" || j.kind == "a hand at the shop" || j.kind == "building work" || j.kind == "gathering" ||
             j.kind == "hunting" || j.kind == "about the home" || j.kind == "project") && (j.progress[id] += seconds) < 600 * spells)
            return;
        // A hand's spells are hours of the work it was taken on for (doc 64, Phase 3): a shop's, made into its next
        // batches; a farm's or other producer's, into its yields.
        // (Not a stall's, with work by the hours: doc 64, Phase 13.)
        if (j.kind == "a hand at the shop" && (!workByHours() || makesThings(tillOf(j.producer))))
        {
            creditWork(tillOf(j.producer), double(spells));
            craftNext_.erase(j.producer);           // (Its keeper looks again at once.)
        }
        if (j.kind == "a hand")
            if (const auto* producer = spec(j.producer); producer && items::producerFor(producer->workLabel))
            {
                creditWork(j.producer, double(spells));
                if (auto& land = state_.memory.landToday; land >= 0 && workByHours() && brakesOn(*items::producerFor(producer->workLabel)))
                    land += double(spells);         // (The pace's brake's reading: RatwLabour.cpp.)
            }
        if ((j.kind == "gathering" || j.kind == "hunting") && day_.grounds && day_.harvest)
            if (const auto found = day_.grounds->find(j.community); found != day_.grounds->end() && j.ground < found->second.size())
                for (const auto& [item, n] : day_.harvest(id, found->second[j.ground], season_))
                    create(id, item, n, j.kind == "hunting" ? "hunted" : "foraged");
        stage = 1;
        if (j.kind != "a hand" && j.kind != "a hand at the shop" && j.kind != "building work" && j.kind != "project")
            return;
    }
    if (stage == 2)
        return;                                     // (Done: waiting for the day to end.)
    // Its share done, where it was to end.
    if (j.kind == "delivery")
        if (const auto* shop = account(tillOf(j.source)))
            if (const auto food = bestFood(*shop); !food.empty())
            {
                const std::int64_t price = shopPrice(j.source, food);   // (The town's price: doc 46.)
                const int n = int(std::min<std::int64_t>({3, stock(*shop, food), account(j.buyer) ? account(j.buyer)->cash / price : 0}));
                if (n > 0)
                    transfer(tillOf(j.source), j.buyer, food, n, price, "delivered on an odd job");
            }
    if (j.kind == "repairs")
        if (const auto* works = account("town:" + j.community + ":works"); works && !works->stock.empty())
        {
            const auto item = works->stock.begin()->first;
            if (consume("town:" + j.community + ":works", item, 1, "used in repairs") > 0)
            {
                auto& c = state_.memory.condition.try_emplace(j.community, 100.0).first->second;
                c = std::min(100.0, c + 1.5);
                ++state_.memory.revision;
            }
        }
    if (j.kind == "gathering" || j.kind == "hunting")
    {
        // Sold cheaply to the town's makers; the proceeds are the poster's.
        std::int64_t made = 0;
        for (const auto& p : positions_)
        {
            const auto held = state_.careers.positions.find(p.id);
            if (p.role == "merchant" && held != state_.careers.positions.end() && !held->second.holder.empty() &&
                communityOfResident(held->second.holder) == j.community && sellsTo(id, held->second.holder))
                made += sellBroughtIn(id, held->second.holder);
        }
        if (made > 0)
            shift(id, j.payer, "", 0, made, "proceeds of an odd job");
    }
    // (A keeper paying from its own purse keeps its food money: spendable.)
    const auto share = j.pay / std::max(1, j.slots);
    if (spendable(j.payer) >= share)
    {
        shift(j.payer, id, "", 0, share, "an odd job: " + j.kind);
        // A House's estate crew keeps its town's fabric, as the Town Works' materials do, while it isn't in full repair
        // (doc 64, Phase 13, round 6); in full repair, its grounds and hedges, nothing made (the model's estate crew).
        if (CrewMend > 0 && houseCrew(j) && j.kind == "building work" && workByHours() && condition(j.community) < 100)
            mendTown(j.community, CrewMend);
    }
    else if (crewPlace(j) && workByHours())
    {
        // A House (a town, the church: round 7) that can't pay a day's share: that gang ends, shut from now, and next
        // week's budget posts what it pays (the model's staff_to). (Its other hands are owed nothing more for a day it
        // can't pay, as a business's.)
        j.until = surplusDay_ - 1;
        j.ended = true;
    }
    else if (j.until >= 0 && j.payer.rfind("till:", 0) == 0 && workByHours())
    {
        // A business that can't pay its hire's share: the hire ends, shut from now (its other hands are owed nothing
        // more for a day it can't pay), and its weekly review hears it fell short (doc 64, Phase 13: no more unpaid days,
        // and a wage it can't pay comes down). (With work by the hours: at the seconds' pace, as it was.)
        j.until = surplusDay_ - 1;
        j.ended = true;
        state_.memory.employers.try_emplace(j.payer, EmployerBook{employerWage(j.payer)}).first->second.shortPay = true;
    }
    stage = 2;
    ++oddVersion_;                                   // (Its share done: the next it holds, if any, or another.)
}
} // namespace ratw
