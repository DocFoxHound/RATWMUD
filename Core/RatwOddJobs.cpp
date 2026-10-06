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
std::string Society::subsidiser(const std::string& community) const
{
    const auto treasury = treasuryOf(community);
    std::int64_t people = 0;
    for (const auto& [id, life] : state_.residents)
        people += treasuryOfResident(id) == treasury;
    if (const auto* purse = account(treasury); purse && people > 0 && purse->cash > std::int64_t(LeanTreasury) * 2 * people)
        return treasury;
    // The church, holding twice its floor (the land's: one purse for every town).
    const auto church = churchOf(treasury);
    if (const auto* purse = account(church); purse && people > 0 && purse->cash > ChurchHead * 2 * std::int64_t(state_.residents.size()))
        return church;
    return {};
}

std::int64_t Society::postOddJobs(const std::string& payer, const std::string& community, std::int64_t budget)
{
    // What a hand is paid: its town's table for a hand's share of an odd job (the wage table, doc 46, Phase 4).
    const std::int64_t handPay = std::max<std::int64_t>(1, std::int64_t(std::ceil(dayWage(community, "odd job") - 1e-9)));
    const int most = int(std::min<std::int64_t>(30, budget / handPay));
    static const DayPlan none;
    const auto& plan = day_.plans.count(community) ? day_.plans.at(community) : none;
    const bool church = payer.rfind("town:", 0) == 0;
    // Where jobs begin and end in town: its market square, or else before its shops.
    std::vector<Spot> about = plan.crowd;
    if (about.empty())
        for (const auto& p : positions_)
            if (p.role == "merchant" && communityOfResident(p.founder) == community)
                about.push_back(p.serve);
    if (most <= 0 || about.empty())
        return 0;
    const auto square = [&](std::size_t k) -> Spot { return about[(std::hash<std::string>{}(payer) + k) % about.size()]; };
    int posted = 0;
    // Each job's hands: a few by its kind (a trip goes in a party), the pay OddJobPay for each, split among them.
    const auto post = [&](OddJob j) {
        const auto [fewest, most_] = j.kind == "gathering" || j.kind == "hunting" ? std::pair{2, 5}
                                   : j.kind == "repairs" || j.kind == "scouting" ? std::pair{1, 3}
                                                                                 : std::pair{1, 2};
        j.id = "odd" + std::to_string(++nextOddJob_);
        j.payer = payer;
        j.community = community;
        j.slots = std::min(most - posted, fewest + int(std::hash<std::string>{}(j.id) % std::size_t(most_ - fewest + 1)));
        if (j.slots < 1)
            return;
        j.pay = handPay * j.slots;
        posted += j.slots;
        oddJobs_.push_back(std::move(j));
    };
    // Deliveries: food bought at a shop for the watch's mess (or the church's table), carried there.
    const auto buyer = church ? payer : "town:" + community + ":watch";
    if (const auto* mess = account(buyer); mess && mess->cash >= 12 && !hasFood(*mess))
        for (const auto& p : positions_)
        {
            if (posted >= std::min(most, 3))
                break;
            const auto held = state_.careers.positions.find(p.id);
            if (p.role != "merchant" || held == state_.careers.positions.end() || held->second.holder.empty() ||
                communityOfResident(held->second.holder) != community)
                continue;
            const auto* shelves = account(tillOf(held->second.holder));
            if (!shelves || !hasFood(*shelves))
                continue;
            Spot to = church && !plan.pulpit.cell.empty() ? plan.pulpit : square(posted);
            if (!church)
                for (const auto& g : positions_)
                    if (g.role == "guard" && communityOfResident(g.founder) == community)
                    {
                        to = g.work;
                        break;
                    }
            OddJob j;
            j.kind = "delivery";
            j.what = church ? "carrying bread for the church's table" : "carrying food for the watch's mess";
            j.from = p.serve;
            j.to = to;
            j.source = held->second.holder;
            j.buyer = buyer;
            post(std::move(j));
        }
    // A hand at a producer short of hands (a farm, a quarry, a dairy...): one with little of its yield in store.
    for (const auto& p : positions_)
    {
        if (posted >= most * 2 / 3)
            break;
        const auto held = state_.careers.positions.find(p.id);
        const auto* producer = items::producerFor(p.title);
        if (!producer || producer->out.empty() || held == state_.careers.positions.end() || held->second.holder.empty() ||
            communityOfResident(held->second.holder) != community)
            continue;
        const auto* store = account(tillOf(held->second.holder));
        if (!store || stockAll(*store, producer->out.front().first) >= ProducerKept / 2)
            continue;
        OddJob j;
        j.kind = "a hand";
        j.what = "a hand at " + p.title.substr(0, 40);
        j.from = j.to = p.work;
        j.producer = held->second.holder;
        j.forChildren = false;
        post(std::move(j));
    }
    // Repairs (a town's): the Town Works' materials carried out and used about the town.
    if (!church && condition(community) < 95)
        if (const auto* works = account("town:" + community + ":works"); works && !works->stock.empty())
            for (int k = 0; k < 2 && posted < most; ++k)
            {
                OddJob j;
                j.kind = "repairs";
                j.what = "mending about the town for the Town Works";
                j.from = square(posted);
                j.to = square(posted + 7);
                post(std::move(j));
            }
    // Gathering, hunting and scouting trips to the ground around the town.
    if (day_.grounds)
        if (const auto found = day_.grounds->find(community); found != day_.grounds->end() && !found->second.empty())
            for (std::size_t k = 0; posted < most; ++k)
            {
                const auto index = (std::hash<std::string>{}(payer) + std::size_t(nextOddJob_) + k) % found->second.size();
                const auto& ground = found->second[index];
                OddJob j;
                j.kind = k % 5 == 4 ? "scouting" : ground.trade == "hunting" ? "hunting" : "gathering";
                j.what = j.kind == "scouting" ? "scouting the country for what it has" : j.kind + " for the town, to sell to its makers";
                j.from = ground.spot;
                j.to = square(posted);
                j.ground = index;
                j.forChildren = j.kind != "hunting";
                post(std::move(j));
            }
    return std::int64_t(posted) * handPay;
}

std::int64_t Society::businessSpends(const std::string& payer, const std::string& keeper, const Position& job, std::int64_t budget)
{
    // Spare money put to work (the user, 2026-10-05: something to sink it into, not just charity): hands hired for the
    // premises (a workshop's next batch sooner, a field's yield worked), and the premises improved, with materials bought
    // from the town's makers and builders hired. Each pays wolves for real work or goods.
    const auto community = communityOfResident(keeper);
    const auto* purse = account(payer);
    if (!purse || budget < OddJobPay || community.empty())
        return 0;
    const bool shop = job.role == "merchant";
    const Spot at = shop ? job.serve : job.work;
    std::int64_t committed = 0;
    const auto hire = [&](const std::string& kind, const std::string& what, int slots) {
        OddJob j;
        j.id = "odd" + std::to_string(++nextOddJob_);
        j.payer = payer;
        j.community = community;
        j.kind = kind;
        j.what = what;
        j.from = j.to = at;
        j.producer = keeper;
        j.slots = slots;
        j.pay = OddJobPay * slots;
        j.forChildren = kind == "a hand at the shop";
        committed += j.pay;
        oddJobs_.push_back(std::move(j));
    };
    // Hands: about four tenths of what it spends, hired for HireDays (unless its hire is running still).
    const bool hiring = std::any_of(oddJobs_.begin(), oddJobs_.end(), [&](const OddJob& j) {
        return j.payer == payer && j.producer == keeper && j.until >= surplusDay_ && (j.kind == "a hand" || j.kind == "a hand at the shop");
    });
    const auto dayPay = std::int64_t(std::ceil(dayWage(community, "hand") - 1e-9));   // (The wage table: doc 46.)
    if (const int hands = int(std::min<std::int64_t>(3, budget * 4 / 10 / dayPay)); hands > 0 && !hiring)
    {
        hire(shop ? "a hand at the shop" : "a hand", "a hand at " + job.title.substr(0, 40) + ", hired for the week", hands);
        auto& j = oddJobs_.back();
        j.until = surplusDay_ + HireDays - 1;
        j.pay = dayPay * hands;
        committed += (dayPay - OddJobPay) * hands;
    }
    // Its premises improved, up to MostImprovement: materials first (bought and used), then the builders.
    const int level = improvement(job.id);
    if (level < MostImprovement && budget - committed >= 10)
    {
        std::vector<std::string> shops;
        for (const auto& p : positions_)
            if (p.role == "merchant" && p.id != job.id)
                if (const auto held = state_.careers.positions.find(p.id); held != state_.careers.positions.end() &&
                                                                           !held->second.holder.empty() &&
                                                                           communityOfResident(held->second.holder) == community)
                    shops.push_back(held->second.holder);
        std::map<std::string, int> got;
        const auto spent = buyForSurplus(payer, shops, [](const std::string& item) {
            return item == "bricks" || item == "planks" || item == "nails" || item == "stone" || item == "roof_tiles" ||
                   item == "timber" || item == "lime" || item == "fittings";
        }, budget - committed, "improving the premises", &got);
        for (const auto& [item, n] : got)
            consume(payer, item, n, "used in improving the premises");
        committed += spent;
        auto& progress = improving_[job.id];
        progress += spent;
        if (progress >= 60 * (level + 1))
        {
            progress = 0;
            state_.memory.improved[job.id] = level + 1;
            ++state_.memory.revision;
            hire("building work", "building work at " + job.title.substr(0, 40), 2);
        }
    }
    return committed;
}

void Society::keepUpPremises()
{
    for (auto it = state_.memory.improved.begin(); it != state_.memory.improved.end();)
    {
        const auto& [pid, level] = *it;
        const auto* job = position(pid);
        const auto held = state_.careers.positions.find(pid);
        std::int64_t spent = 0;
        if (job && level > 0 && held != state_.careers.positions.end() && !held->second.holder.empty())
        {
            const auto& keeper = held->second.holder;
            const auto community = communityOfResident(keeper);
            // Who pays: the business's till; a house's business, its house, if the till can't.
            std::string payer = tillOf(keeper);
            if (const auto owner = state_.houses.owner.find(pid);
                owner != state_.houses.owner.end() && (!account(payer) || spendable(payer) < UpkeepALevel * level))
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
                }, std::min<std::int64_t>(UpkeepALevel * level, spendable(payer)), "keeping up the premises", &got);
            for (const auto& [item, n] : got)
                consume(payer, item, n, "used in keeping up the premises");
        }
        auto& weeks = state_.memory.neglected[pid];
        weeks = spent * 2 >= UpkeepALevel * level ? 0 : weeks + 1;
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

const Society::OddJob* Society::oddJobFor(const std::string& id, const Position& job, bool jobless, int age, bool poor, double hour,
                                         std::int64_t ownDayPay, bool* claims)
{

    // Its job under way (one whose share it has done no longer holds it: it may take another).
    for (const auto& j : oddJobs_)
        if (const auto mine = j.stage.find(id); mine != j.stage.end() && mine->second < 2)
            return &j;
    // Who seeks them out: those without other work, and children (eight and up, not apprenticed), in the day; the poor
    // and hungry from eight, others from ten. Children would rather not work (the user, 2026-10-05): they look from noon,
    // on one day in three unless poor or hungry; and work that isn't for children (a hand at a farm, a hunt) is theirs only
    // when no grown wolf has taken it by mid-afternoon. (Later: such work should teach a skill, toward apprenticeships and
    // in time taking over a business: Docs/Design/42, "Later".)
    const bool child = age >= 8 && age < 16;
    if (child && (hour < 12 || (!poor && std::hash<std::string>{}(id + "|work|" + std::to_string(surplusDay_)) % 3 != 0)))
        return nullptr;
    // A wolf with a post of its own looks, of a morning, for a business's hire paying it better (the user, 2026-10-05:
    // unfilled work pulls workers off other employment).
    const bool better = ownDayPay > 0 && hour >= 8 && hour < 10 &&
                        std::any_of(oddJobs_.begin(), oddJobs_.end(), [&](const OddJob& j) {
                            return j.until >= 0 && j.slots > 0 && j.pay / j.slots >= ownDayPay + HireRaise &&
                                   int(j.stage.size()) < j.slots && j.community == communityOfResident(id);
                        });
    if (hour >= 18 || hour < (poor ? 8 : 10) || job.role != "civilian" || !(jobless || child || better || (poor && !job.paid)))
        return nullptr;
    const auto community = communityOfResident(id);
    const auto& group = child ? friendGroup(id) : std::string();
    // (A farm's hire may be taken by anyone living in a city too: RatwFarmhands.cpp.)
    const bool fromCity = !child && cities_.count(community) > 0;
    const auto open = [&](const OddJob& j) {
        return (j.community == community || (fromCity && j.until >= 0 && j.kind == "a hand" && farmHire(j))) &&
               int(j.stage.size()) < j.slots && !j.stage.count(id) && (!child || j.forChildren || hour >= 14);
    };
    // A child joins a friend on a job first, then one with room for a party; anyone else, the first open one.
    OddJob* chosen = nullptr;
    if (child && !group.empty())
        for (auto& j : oddJobs_)
            if (open(j) && std::any_of(j.stage.begin(), j.stage.end(), [&](const auto& t) { return friendGroup(t.first) == group; }))
            {
                chosen = &j;
                break;
            }
    for (int pass = child ? 0 : 1; !chosen && pass < 2; ++pass)
        for (auto& j : oddJobs_)
            if (open(j) && (pass == 1 || j.slots >= 2) && (!better || jobless || (j.until >= 0 && j.pay / j.slots >= ownDayPay + HireRaise)))
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
    if (chosen->until >= 0 && farmHire(*chosen))
        lodgeHand(id, *chosen);                     // (From elsewhere: to the farm's bunkhouse.)
    return chosen;
}

void Society::advanceOddJob(const std::string& id, int seconds)
{
    const auto it = std::find_if(oddJobs_.begin(), oddJobs_.end(), [&](const OddJob& j) {
        const auto mine = j.stage.find(id);
        return mine != j.stage.end() && mine->second < 2;
    });
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
             j.kind == "hunting") && (j.progress[id] += seconds) < 600 * spells)
            return;
        if (j.kind == "a hand at the shop")
            craftNext_.erase(j.producer);           // (Its next batch is begun at once.)
        if (j.kind == "a hand")
            if (const auto* producer = spec(j.producer))
                if (const auto* p = items::producerFor(producer->workLabel))
                    for (const auto& [item, count] : p->out)
                        create(tillOf(j.producer), item, count, "brought in by a hand");
        if ((j.kind == "gathering" || j.kind == "hunting") && day_.grounds && day_.harvest)
            if (const auto found = day_.grounds->find(j.community); found != day_.grounds->end() && j.ground < found->second.size())
                for (const auto& [item, n] : day_.harvest(id, found->second[j.ground], season_))
                    create(id, item, n, j.kind == "hunting" ? "hunted" : "foraged");
        stage = 1;
        if (j.kind != "a hand" && j.kind != "a hand at the shop" && j.kind != "building work")
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
        shift(j.payer, id, "", 0, share, "an odd job: " + j.kind);
    stage = 2;
}
} // namespace ratw
