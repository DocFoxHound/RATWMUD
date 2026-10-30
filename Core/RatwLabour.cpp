// The labour market (Docs/Design/64-economy-migration.md, Phase 6; doc 63's model): every employer its own wage, moved
// by its open posts; the going rate from what the unskilled trades pay; wolves judging their posts by what they paid.
// See RatwSociety.h.
#include "RatwItems.h"
#include "RatwSociety.h"

#include "RatwCalendar.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <set>

namespace ratw
{
namespace
{
double lot(const std::string& words)
{
    return double(std::hash<std::string>{}(words) % 100000) / 100000;
}
}

std::string Society::employerKey(const Payer& payer, const Position& job) const
{
    if (payer.account.empty())
        return {};
    if (payer.account.rfind("till:", 0) == 0)
        return payer.account;                        // (A business: its till.)
    return payer.account + "|" + (job.title == LabourTitle ? std::string("works") : wageKind(payer, job));
}

std::string Society::postEmployer(const Position& p, const std::string& holder, int age) const
{
    std::string key;
    if (p.role == "merchant" || items::producerFor(p.title))
        key = "till:" + p.id;                        // (Its keeper or farmer: its business's first hand.)
    else
        key = employerKey(payerOf(holder.empty() ? p.founder : holder, p, holder.empty() ? 30 : age), p);
    return key.empty() || (key.rfind("till:", 0) == 0 && !account(key)) ? std::string() : key;
}

double Society::employerSkill(const std::string& key) const
{
    // A business's: the most any of its crafts needs (a smithy's blades, a mill's none).
    const auto* p = key.rfind("till:", 0) == 0 ? position(key.substr(5)) : nullptr;
    const auto* business = p ? items::businessFor(p->title) : nullptr;
    double most = 0;
    if (business)
        for (const auto* k : items::craftsFor(business->id))
            most = std::max(most, double(k->skill));
    return most;
}

double Society::goingRate(const std::string& town) const
{
    const auto& going = state_.memory.going;
    if (const auto found = going.find(town); found != going.end())
        return found->second;
    if (const auto realm = going.find(""); realm != going.end())
        return realm->second;
    return items::ladder().catalogueDay;            // (Before the first measure: the catalogue's labourer's day.)
}

double Society::employerWage(const std::string& key) const
{
    if (const auto found = state_.memory.employers.find(key); found != state_.memory.employers.end() && found->second.wage > 0)
        return found->second.wage;
    // A new employer: the going rate for its skill, where it is.
    const auto town = employerTown_.count(key) ? employerTown_.at(key) : std::string();
    const auto bar = key.rfind('|');
    const auto kind = bar == std::string::npos ? std::string() : key.substr(bar + 1);
    const double share = kind == "works" ? WorksShare : kind == "clergy" ? ClergyShare : 1.;
    return goingRate(town) * (1 + employerSkill(key) / items::ladder().skillWage) * share;
}

int Society::vacanciesOf(const std::string& key) const
{
    const auto found = vacancies_.find(key);
    return found == vacancies_.end() ? 0 : found->second;
}

int Society::handsOf(const std::string& key) const
{
    const auto found = hands_.find(key);
    return found == hands_.end() ? 0 : found->second;
}

double Society::paidLately(const std::string& resident) const
{
    const auto found = pay7_.find(resident);
    return found == pay7_.end() ? 1 : found->second;
}

void Society::notePay(const std::string& resident, const std::string& key, std::int64_t coins, std::int64_t owed)
{
    paidToday_[resident] += double(coins);
    owedToday_[resident] += double(owed);
    if (coins < owed && !key.empty())
    {
        state_.memory.employers.try_emplace(key, EmployerBook{employerWage(key)}).first->second.shortPay = true;
        if (noteShorts_)
        {
            const auto* purse = account(key.substr(0, key.find('|')));
            shorts_.push_back({key, "wage", owed - coins, purse ? purse->cash : 0});
        }
    }
}

std::int64_t Society::lookingFor(const std::string& resident, const Payer& payer, const Position& job, double day) const
{
    const auto key = employerKey(payer, job);
    if (key.empty() || job.role == "merchant")
        return 0;
    // Its employer paying under 60% of what it owed this last week or so: it looks every morning, for anything paying
    // better than it's been getting.
    const double lately = paidLately(resident), going = goingRate(employerTown_.count(key) ? employerTown_.at(key) : std::string());
    if (lately < LookBelow)
        return std::max<std::int64_t>(1, std::int64_t(std::floor(std::max(lately * employerWage(key), .5 * going))));
    // Paid well enough: drawn off now and then, never from an employer short of hands, nor as its only hand.
    if (vacanciesOf(key) > 0 || handsOf(key) <= 1 || lot(resident + "|poach|" + std::to_string(std::int64_t(day))) >= PoachChance)
        return 0;
    return std::max<std::int64_t>(1, std::int64_t(std::ceil(employerWage(key) * (1 + PoachGap) - 1e-9)));
}

bool Society::calledToTheLand(const std::string& resident, const Position& job, int age, std::int64_t day) const
{
    // The harvest call (doc 63's model; doc 64, Phase 3b): at sowing and the harvest, hands from trades that aren't
    // staples (finery, cloth, building, the blades) go to the fields, a tenth of them a day. Not the land's own, nor
    // those who make what the realm eats or burns every day, nor the clergy.
    if (!fieldsOn() || !job.paid || job.role != "civilian" || age < 16 || fieldWindow(double(day)) == 0 ||
        lot(resident + "|land|" + std::to_string(day)) >= LandCall)
        return false;
    const auto key = employerKey(payerOf(resident, job, age), job);
    if (key.empty() || (key.size() > 7 && key.compare(key.size() - 7, 7, "|clergy") == 0))
        return false;
    if (key.rfind("till:", 0) != 0)
        return true;                                 // (The town's own posts, its works.)
    const auto* p = position(key.substr(5));
    if (!p || items::producerFor(p->title))
        return false;
    const auto* business = items::businessFor(p->title);
    if (!business)
        return false;
    for (const auto* k : items::craftsFor(business->id))
        for (const auto& [good, n] : k->out)
            if (const auto* g = items::good(good); g && g->staple)
                return false;
    return true;
}

std::vector<std::string> Society::helpPosts(const std::string& till) const
{
    std::vector<std::string> posts;
    for (const auto& p : positions_)
        if (p.role != "merchant")
            if (const auto key = postKey_.find(p.id); key != postKey_.end() && key->second == till)
                posts.push_back(p.id);
    return posts;
}

int Society::handsWanted(const std::string& till, bool bare, bool more) const
{
    int hands = std::max(1, int(std::ceil(hoursBySales(till, bare) / PaidSpells - 1e-9)));
    if (!workByHours() || (state_.memory.shortTills.empty() && !wageRule(MoreHands)))
        return hands;
    // An idle business wants its keeper, no more (round 11, lane 1: IdleTills).
    if (wageRule(IdleTills) && tillIdle(till))
        return 1;
    // One holding over its working money (and its debts) takes on a hand more for each month of a hand's wage it holds
    // over, up to doubling: what its buyers pay goes in wages, not in its owner's month's profit (MoreHands, the user,
    // 2026-10-29). Not a stall (it hires no one) nor a business with no work for more (its review hires none then).
    if (more && wageRule(MoreHands) && makesThings(till))
        if (const auto* purse = account(till); purse && till.rfind("till:", 0) == 0)
        {
            const auto spare = double(purse->cash - workingMoney(till.substr(5)) - owed(till));
            const double month = MonthDays * employerWage(till);
            if (spare > month && month > 0)
                hands += std::min(hands, int(spare / month));
        }
    return hands;
}

double Society::hoursBySales(const std::string& till, bool bare) const
{
    const auto* p = till.rfind("till:", 0) == 0 ? position(till.substr(5)) : nullptr;
    const auto rates = sellRate_.find(till);
    const auto* shelves = account(till);
    if (!p || !shelves)
        return 0;
    // A craft short of materials today or yesterday, and what it could make a day (doc 63's model, feasible; doc 64,
    // Phase 13): what it made lately, or what a week of the materials in hand would make.
    const auto shortOf = [&](const items::Craft& k) {
        for (const auto* days : {&shortToday_, &shortYesterday_})
            if (const auto found = days->find(till); found != days->end() && found->second.count(k.id))
                return true;
        return false;
    };
    const auto feasible = [&](const items::Craft& k) {
        double batches = 1e18, made = 0;
        for (const auto& [in, q] : k.in)
            batches = std::min(batches, stockAll(*shelves, in) / double(std::max(1, q)));
        if (const auto m = madeRate_.find(till); m != madeRate_.end())
            if (const auto g = m->second.find(k.out.front().first); g != m->second.end())
                made = g->second;
        return std::max(made, std::max(1, k.out.front().second) * batches / 7);
    };
    // A good's sales a day, as its maker plans them: a fifth over for a staple, half again with its shelf near bare, a
    // fifth less with it glutted (doc 63's model). A craft short of materials: not half again (its shelf is bare for want
    // of them), and no more than MadeSlack over what it could make. (Not half again, either, for the pace's measure.)
    const auto planned = [&](const std::string& good, const items::Craft* k) {
        const double rate = rates == sellRate_.end() || !rates->second.count(good) ? 0. : rates->second.at(good);
        const auto* g = items::good(good);
        if (rate <= 0 || !g)
            return 0.;
        const bool short_ = k && !k->in.empty() && shortOf(*k);
        const double days = stockAll(*shelves, good) / rate, keep = keepDays(good);
        const double shelf = days < keep * .3 ? (bare && !short_ ? 1.5 : 1.) : days > keep * 1.7 ? .8 : 1.;
        const double want = rate * (g->staple ? StapleSlack : 1.) * shelf;
        return short_ ? std::min(want, MadeSlack * feasible(*k)) : want;
    };
    double hours = 0;
    const auto* producer = items::producerFor(p->title);
    if (const auto f = state_.memory.fields.find(till); producer && fieldsOn() && fieldWork(producer->id) && f != state_.memory.fields.end())
    {
        // A field: the work left in its season over the days left of it (between, a hand keeps it).
        const auto d = std::int64_t(state_.budgetDay) % calendar::DaysPerYear;
        const double acre = hoursFor(*producer) * FieldYields;
        const int window = d < SowEnd ? 1 : d >= HarvestStart && d < HarvestEnd ? 2 : 0;
        if (window == 1)
            hours = (f->second.acres - f->second.sown) * acre * SowShare / double(SowEnd - d);
        else if (window == 2)
            hours = f->second.toHarvest * acre * (1 - SowShare) / double(HarvestEnd - d);
    }
    else if (producer)
    {
        // A yield: its hours for the most-wanted of what it brings in (one yield brings them all).
        double most = 0;
        for (const auto& [good, n] : producer->out)
            most = std::max(most, planned(good, nullptr) / std::max(1, n));
        hours = most * hoursFor(*producer);
    }
    if (const auto mending = mendRate_.find(till); mending != mendRate_.end())
        hours += mending->second;                    // (And its mending: Phase 9.)
    if (const auto works = worksRate_.find(till); works != worksRate_.end())
        hours += works->second;                      // (And its works on homes: Phase 13.)
    if (const auto* business = items::businessFor(p->title))
    {
        // A craft by what it's for, its first good, as craft() judges it (a butcher's meat, not the hides that come with
        // it); a good several recipes make counted once, by the first of them in craft()'s order with its materials in
        // hand (doc 64, Phase 13: pig and sheep both counted for meat).
        const auto crafts = items::craftsFor(business->id);
        const auto inHand = [&](const items::Craft& k) {
            return std::all_of(k.in.begin(), k.in.end(), [&](const auto& i) { return stockAll(*shelves, i.first) >= i.second; });
        };
        std::set<std::string> counted;
        for (const auto* k : crafts)
            if (!k->out.empty() && counted.insert(k->out.front().first).second)
            {
                const auto* use = k;
                for (const auto* other : crafts)
                    if (!other->out.empty() && other->out.front().first == k->out.front().first && inHand(*other))
                    {
                        use = other;
                        break;
                    }
                hours += planned(use->out.front().first, use) / std::max(1, use->out.front().second) * hoursFor(*use);
            }
    }
    return hours;
}

bool Society::brakesOn(const items::Producer& p)
{
    return !fieldWork(p.id) && (p.seasons.empty() || !p.offSeason.empty());
}

bool Society::idleHand(const std::string& resident, const Payer& payer) const
{
    return idleHandOf(resident, nullptr, payer);
}

bool Society::idleHand(const ResidentRecord& resident, const Payer& payer) const
{
    return idleHandOf(resident.entry->first, &resident, payer);
}

bool Society::idleHandOf(const std::string& resident, const ResidentRecord* record, const Payer& payer) const
{
    // A hand whose business has more work in hand than it can use (its shelves full, its materials short): free to take
    // a hire elsewhere (doc 63's harvest call: hands from trades with nothing to do go where they're wanted).
    // (Asked at every decision of a paid wolf: the cheap tests first. Its job, and its title's producer, by its record
    // while the records hold.)
    const bool holds = record && recordsHold();
    if (payer.account.rfind("till:", 0) == 0)
    {
        const auto* job = holds ? record->job : jobOf(resident);
        if (!job || job->role == "merchant")
            return false;
        // (A producer's help: free as its business is, its field's work done or its holder's work in hand full
        // (idleBusiness). Its hours go to the till's own bank, which nothing draws on: full, it read as idle, and a
        // field's help would be freed each morning and called back each day's pass. Doc 64, Phase 13.)
        if (const auto* p = position(payer.account.substr(5)); p && items::producerFor(p->title))
            return idleBusiness(payer.account);
        return bankFull(payer.account);
    }
    // A field's farmer with no work in its window, as free (doc 64, Phase 13: the model's farm keeps a hand between its
    // windows, but each of ours is a one-hand farm, and it stood idle from sowing's end to the harvest).
    if (!payer.account.empty() || !fieldsOn())
        return false;
    const auto* job = holds ? record->job : jobOf(resident);
    const auto* producer = holds ? record->jobProducer : job ? items::producerFor(job->title) : nullptr;
    if (!producer || !fieldWork(producer->id))
        return false;
    const auto till = tillOf(resident);
    const auto f = state_.memory.fields.find(till);
    return f != state_.memory.fields.end() && fieldIdle(till, f->second, *producer);
}

bool Society::fieldIdle(const std::string& till, const FieldYear& f, const items::Producer& producer) const
{
    // A field: its window's work done (all sown, all brought in), or between them nothing it brings in wanted.
    if (const int window = fieldWindow(double(state_.budgetDay)); window != 0)
        return window == 1 ? f.sown >= f.acres : f.toHarvest <= 0;
    const auto* a = account(till);
    for (const auto& [item, n] : producer.offSeason)
        if (a && std::none_of(producer.out.begin(), producer.out.end(), [&](const auto& o) { return o.first == item; }) &&
            stock(*a, item) < shelfTarget(till, item, ProducerKept))
            return false;
    return true;
}

bool Society::idleBusiness(const std::string& till) const
{
    // (A keeper's purse its till: an older save's, a test's village.)
    const auto* p = till.rfind("till:", 0) == 0 ? position(till.substr(5)) : jobOf(till);
    const auto* producer = p ? items::producerFor(p->title) : nullptr;
    if (const auto f = state_.memory.fields.find(till); producer && fieldsOn() && fieldWork(producer->id) && f != state_.memory.fields.end())
        return fieldIdle(till, f->second, *producer);
    // Its work in hand: a business's at its till, a producer's its own (its holder's).
    if (!producer)
        return bankFull(till);
    const auto held = state_.careers.positions.find(p->id);
    return held != state_.careers.positions.end() && bankFull(held->second.holder);
}

bool Society::bankFull(const std::string& account) const
{
    // Full now, and at the last turn of the day (doc 64, Phase 13): not a moment's, a hire's day of hours come in after
    // its keeper's last look, but a business with more work in hand than it uses day to day.
    const auto banked = workBanked_.find(account);
    return banked != workBanked_.end() && banked->second >= WorkBankedMost - 1 && fullBanks_.count(account) > 0;
}

bool Society::makesThings(const std::string& till) const
{
    const auto* p = till.rfind("till:", 0) == 0 ? position(till.substr(5)) : jobOf(till);
    if (!p)
        return false;
    if (items::producerFor(p->title))
        return true;
    const auto* business = items::businessFor(p->title);
    if (!business)
        return false;
    thread_local std::unordered_map<std::string, bool> crafts;     // (Asked every spell of help: each thread remembers.)
    if (const auto found = crafts.find(business->id); found != crafts.end())
        return found->second;
    return crafts.emplace(business->id, !items::craftsFor(business->id).empty()).first->second;
}

bool Society::sellsOnly(const std::string& till) const
{
    const auto* p = till.rfind("till:", 0) == 0 ? position(till.substr(5)) : jobOf(till);
    const auto* business = p && !items::producerFor(p->title) ? items::businessFor(p->title) : nullptr;
    return business && (business->id == "stall" || business->id == "moneychanger");
}

void Society::reviewHands(const std::string& till, std::int64_t day, bool weekly, int wanted)
{
    const auto pid = till.substr(5);
    const auto* p = position(pid);
    const auto held = state_.careers.positions.find(pid);
    // A business that makes nothing, a stall or a moneychanger, hires no one: its help sells, and it wants no hands for
    // its sales (Phase 13). At its weekly review it lets its surplus help go as a maker does (round 7, the user,
    // 2026-10-26; the model's firm_week): before, it never let go, and the Heights' cheese and honey stalls held 31 and
    // 24 hands for the 1 each wanted, the realm's idle hands, out on every hire. Whatever the pace: what it wants
    // doesn't grow with it. The hand keeps its home (doc 64, question 3) and is a jobless wolf, taking hires and posts
    // as the jobless do. Only a market stall or a moneychanger (sellsOnly): the other businesses with no craft of their
    // own, a fletcher, a cartographer, an arsenal, a dye works, a builders' yard, keep their hands (round 8, the user,
    // 2026-10-27; round 7 let every one of them go, doc 64, round 7's review).
    const bool stall = !makesThings(till);
    if (!p || held == state_.careers.positions.end() || held->second.holder.empty() || !account(till) ||
        (stall && (!weekly || !wageRule(StallsShed) || (wageRule(SellersOnly) && !sellsOnly(till)))))
        return;
    const auto posts = helpPosts(till);
    // An apprentice is a hand over and above (the model's firm_week): never let go, nor counted in the excess, at a maker
    // or a stall alike (round 8, the user, 2026-10-27; in round 7 four were let go as hands). With work by the hours only
    // the business's own apprentice, its keeper's (the round 9 decision, the user, 2026-10-27; the model's apprentice
    // learns its firm's trade beside its master): one apprenticed to a fellow hand is help as any other. (The Heights'
    // cheese stall kept three hands apprenticed to its own dock and wash hands for good, and its till ran down to 29p.
    // Its owner is a House or its keeper, so the keeper is the one master who works there.)
    std::set<std::string> learners;
    if (weekly && wageRule(KeepApprentices))
        for (const auto& post : posts)
            if (const auto ps = state_.careers.positions.find(post); ps != state_.careers.positions.end() && !ps->second.holder.empty())
                if (const auto* master = apprenticedTo(ps->second.holder);
                    master && (!workByHours() || !wageRule(KeepersApprentice) || master->id == pid))
                    learners.insert(ps->second.holder);
    const auto letGo = [&](int excess) {
        // Half the excess, one at least, the post kept empty. (Never its producer's own post, among its till's: its
        // farmer, its fisher. Round 8's review.)
        int n = std::max(1, excess / 2);
        for (const auto& post : posts)
        {
            auto& ps = state_.careers.positions[post];
            if (n <= 0 || ps.holder.empty() || learners.count(ps.holder) || post == pid)
                continue;
            record("laid off", till, ps.holder, "", 0, 0);
            ps.lastHolder = ps.holder;
            ps.holder.clear();
            ps.vacantSince = double(day);
            state_.memory.closed.insert(post);
            ++state_.memory.revision;
            forgetCareers();
            --n;
        }
    };
    if (stall)
    {
        if (const int excess = handsOf(till) - int(learners.size()) - wanted; excess > 0)
            letGo(excess);
        return;
    }
    const auto& keeper = held->second.holder;
    // With more work in hand than its hands can use, it takes on no more, and its hires end: their hours would be lost
    // (doc 64, Phase 13; the model lets its excess go).
    const bool idle = idleBusiness(till);
    int hired = 0;
    for (auto& j : oddJobs_)
        if (j.payer == till && j.until >= day && (j.kind == "a hand" || j.kind == "a hand at the shop"))
        {
            if (!idle)
            {
                hired += j.slots;
                continue;
            }
            j.until = day - 1;
            j.ended = true;
            ++oddVersion_;
        }
    const int hands = handsOf(till) + hired;
    wanted = std::min(wanted, 2 * hands + 2);
    if (wanted > hands && !idle)
    {
        int more = wanted - hands;
        // The posts it let go first (succession fills them), then hires.
        for (const auto& post : posts)
            if (more > 0 && state_.memory.closed.erase(post))
            {
                --more;
                ++state_.memory.revision;
            }
        const auto* producer = items::producerFor(p->title);
        // (Sowing in spring, the harvest at summer's end: doc 63. A field's own windows, with seasonal fields.)
        const bool field = producer && fieldsOn() && fieldWork(producer->id) && state_.memory.fields.count(till) &&
                           fieldWindow(double(day)) != 0;
        const bool harvest = field || (producer && farmWork(producer->id) && !(fieldsOn() && fieldWork(producer->id)) &&
                                       (producer->seasons.empty() || std::find(producer->seasons.begin(), producer->seasons.end(), season_) != producer->seasons.end()) &&
                                       (season_ == 0 || season_ == 2));
        auto pay = std::int64_t(std::ceil(employerWage(till) * (harvest ? HarvestPay : 1.) - 1e-9));
        // A producer short of what its buyers want pays its hire more by its shortage (round 16, lane 1, the user,
        // 2026-10-30: ScarcityToHires, with WageBySales and work by the hours): its yields' buyers turned away over its
        // rate (missedRate over sellRate, its whole `out`), from UnmetFrom to UnmetFull as reviewMargins' `scarce`,
        // takes a slot from its wage to HirePremium over it, never past WageHigh times the going rate for its skill nor
        // under its wage. The money a shortage brings goes to the day's hands it draws, not into the wage book: its bid
        // reads its trade at the ladder's margin (noteTrade), and hire pay is in neither the going rate (the book's
        // middle) nor any normal cost. Ridgemere's boats ran 2.5-10.4 hire slots each against 5-24 wanted for two years
        // and turned away a fifth to a third of their buyers, and the owners' profit from their tills was 424-558k a
        // quarter in year 4. (Not a harvest's hire: the land's sowing and harvest keep HarvestPay.)
        if (producer && !harvest && workByHours() && wageRule(ScarcityToHires) && wageRule(WageBySales))
        {
            double missed = 0, rate = 0;
            const auto rates = sellRate_.find(till);
            for (const auto& [good, n] : producer->out)
            {
                missed += missedRate(till, good);
                if (rates != sellRate_.end())
                    if (const auto r = rates->second.find(good); r != rates->second.end())
                        rate += r->second;
            }
            const double scarce = rate > .05 ? std::clamp((missed / rate - UnmetFrom) / (UnmetFull - UnmetFrom), 0., 1.) : 0.;
            const double wage = employerWage(till);
            const double most = goingRate(employerTownOf(till)) * (1 + employerSkill(till) / items::ladder().skillWage) * WageHigh;
            pay = std::int64_t(std::ceil(std::max(wage, std::min(wage * (1 + HirePremium * scarce), most)) - 1e-9));
        }
        // (Already hiring: its hire runs on, and what it still wants comes with the next.)
        const bool hiring = std::any_of(oddJobs_.begin(), oddJobs_.end(), [&](const OddJob& j) {
            return j.payer == till && j.until >= day && int(j.stage.size()) < j.slots;
        });
        const int slots = std::min(more, 8);
        if (slots > 0 && !hiring && spendable(till) >= pay * slots)
        {
            OddJob j;
            j.id = "odd" + std::to_string(++nextOddJob_);
            j.payer = till;
            j.producer = keeper;
            j.community = communityOfResident(keeper);
            j.kind = p->role == "merchant" ? "a hand at the shop" : "a hand";
            j.what = std::string(field && fieldWindow(double(day)) == 1 ? "a hand at the sowing at " : harvest ? "a hand at the harvest at " : "a hand at ") +
                     p->title.substr(0, 40) + ", hired for the week";
            j.from = j.to = p->role == "merchant" ? p->serve : p->work;
            j.slots = slots;
            j.pay = pay * slots;
            j.forChildren = false;
            j.field = field;
            j.until = day + HireDays - 1;
            oddJobs_.push_back(std::move(j));
            ++oddVersion_;
        }
    }
    // (Not while the realm is still on its way to the calibrated hours: what its sales take grows week by week.)
    else if (const int excess = hands - int(learners.size()) - wanted - hired; weekly && excess > 0 && state_.memory.pace >= 1)
        letGo(excess);                               // (Over: its hires run out by themselves; then help let go.)
}

void Society::tendLabour(std::int64_t day, const std::map<std::string, LifeBody>& bodies)
{
    auto& memory = state_.memory;
    // Every post's employer, and whether it's filled.
    vacancies_.clear();
    hands_.clear();
    const auto ageOf = [&](const std::string& id) {
        const auto b = bodies.find(id);
        return b != bodies.end() ? b->second.age : spec(id) ? spec(id)->age : 30;
    };
    std::map<std::string, std::string> keyOf;           // Holder -> its employer.
    for (const auto& p : positions_)
    {
        const auto held = state_.careers.positions.find(p.id);
        const auto holder = held == state_.careers.positions.end() ? std::string() : held->second.holder;
        const auto key = postEmployer(p, holder, holder.empty() ? 30 : ageOf(holder));
        if (key.empty())
            continue;
        postKey_[p.id] = key;
        employerTown_[key] = communityOfResident(p.founder);
        if (!holder.empty())
            ++hands_[key];
        else if (!memory.closed.count(p.id))
            ++vacancies_[key];                       // (A post it let go isn't offered.)
        if (!holder.empty())
            keyOf[holder] = key;
    }
    keepTradeNet();                                 // (Each business's trade lately: its wage bid's measure, WageBySales.)
    // A child's place on an institution's crew is the day's (doc 64, Phase 13, round 7: the grown first, the model's
    // staff_to): open again at dawn to the grown free for it, the child's again from mid-afternoon if none took it.
    // (Before, a child that took one held it the week.) (With work by the hours.)
    if (workByHours())
        for (auto& j : oddJobs_)
            if (j.until >= day && crewPlace(j))
                for (auto it = j.stage.begin(); it != j.stage.end();)
                    if (ageOf(it->first) < 16)
                    {
                        j.progress.erase(it->first);
                        it = j.stage.erase(it);
                        ++oddVersion_;
                    }
                    else
                        ++it;
    // Its hires not yet taken are posts open too. And a House's crews' places, under its own key (doc 64, Phase 13,
    // round 6: a House short of its crew showed in no measure); they pay the going rate (the model's), so no review moves
    // them, and no book is kept. (With work by the hours.) A place a child holds stands open too, with work by the hours
    // (round 7, the user, 2026-10-26; the model's: nobody under 16 works): the child keeps the work and its pay, but the
    // business is short of a grown hand, and its wage hears it. Before, children held a fifth of the businesses' hires
    // and half the crews', and the shortage never reached a wage.
    const auto children = [&](const OddJob& j) {
        int n = 0;
        if (workByHours() && wageRule(ChildrenOpen))
            for (const auto& [who, stage] : j.stage)
                n += ageOf(who) < 16;
        return n;
    };
    for (const auto& j : oddJobs_)
        if (j.until >= day && j.payer.rfind("till:", 0) == 0 && (int(j.stage.size()) < j.slots || children(j) > 0))
            vacancies_[j.payer] += j.slots - int(j.stage.size()) + children(j);
        else if (workByHours() && j.until >= day && !j.ended && crewPlace(j) && (int(j.stage.size()) < j.slots || children(j) > 0))
            vacancies_[j.payer + "|crew"] += j.slots - int(j.stage.size()) + children(j);
    const auto crewKey = [](const std::string& key) { return key.size() > 5 && key.compare(key.size() - 5, 5, "|crew") == 0; };
    // A post whose holder is away on another business's hire stands open while it's away (doc 63's model: a hire opens
    // the post it leaves; doc 64, Phase 13), unless its own business has no work for it: then its hands are free to go.
    // (Its hands stay as they are: it comes back.) A field's farmer, freed between its windows, is called back from its
    // hire once its own window has work: the farm's hands work its fields with it, not into a bank only it draws on. A
    // House's crews as a business's hire (round 6: they are posts now, taken as a hire is). (With work by the hours: the
    // seconds' pace is as it was.)
    const auto fieldOf = [&](const std::string& key) {
        const auto* p = key.rfind("till:", 0) == 0 ? position(key.substr(5)) : nullptr;
        const auto* producer = p ? items::producerFor(p->title) : nullptr;
        return producer && fieldsOn() && fieldWork(producer->id) && memory.fields.count(key) > 0;
    };
    // First, each field in its founding year's sowing looks at its acres again (reviewField), its farmer at work or away
    // on a hire, as its next year is planned below: founded already sown, its farmer took 7-day hires, and its weekly
    // look ran only in its farmer's own work, so its acres never grew and it was never called back (doc 64, Phase 13).
    if (fieldsOn() && fieldWindow(double(day)) == 1)
        for (const auto& [till, f] : memory.fields)
            if (const auto* p = f.founded >= 0 && f.founded / calendar::DaysPerYear == day / calendar::DaysPerYear && till.rfind("till:", 0) == 0
                                    ? position(till.substr(5))
                                    : nullptr)
                if (const auto* producer = items::producerFor(p->title); producer && fieldWork(producer->id))
                {
                    const auto held = state_.careers.positions.find(p->id);
                    reviewField(held != state_.careers.positions.end() && !held->second.holder.empty() ? held->second.holder : p->founder, till,
                                *producer, double(day));
                }
    // (A household's keeper left its post to keep the house, not for the hire: its post isn't opened by it, nor is it called
    // back to a field it doesn't work. Round 7's crews took keepers at any hour, and their posts stood open all week: a
    // House's staff's wage, held at the rate before, rose half again by day 119, and nobody could take the post.)
    std::set<std::string> keepers;
    if (workByHours())
        for (const auto& [home, who] : memory.keeper)
            keepers.insert(who);
    // With CalledBackLate (round 16, lane 1; the user, 2026-10-30; with work by the hours), a shop's keeper holding a
    // place on a hire is called back to its shop at the end of this pass (callShopsBack), not at tendTills' before it:
    // the day refills 95% of the places so left, so read as open they were holes that never stood a day. Each one blocked
    // its payer's next hire (reviewHands posts none while a hire of its has a place untaken) and counted in its
    // vacancies, so its open days reached OpenDays nearly every week and its wage was bid by sales. In days 1200-1399 of
    // round 14's six years, a Ridgemere boat whose hires were all full at 23h but stood open at the turn posted a new
    // hire on 15% of 1,245 days; with nothing open at the turn, on 100% of 338. (Round 12's principle for WagelessFirst's
    // release: after the day's measures.) Until then the keeper is its hire's hand, but its own post isn't left: it is
    // about to go back to it (below).
    const bool lateCall = workByHours() && wageRule(OwnersStop) && wageRule(CalledBackLate);
    const auto shopKeeper = [&](const std::string& id) {
        const auto* post = jobOf(id);
        return post && post->role == "merchant";
    };
    if (workByHours())
        for (auto& j : oddJobs_)
            if (j.until >= day && ((j.payer.rfind("till:", 0) == 0 && (j.kind == "a hand" || j.kind == "a hand at the shop")) || crewPlace(j)))
                for (auto it = j.stage.begin(); it != j.stage.end();)
                {
                    const auto k = keyOf.find(it->first);
                    if (k == keyOf.end() || k->second == j.payer || keepers.count(it->first) || (lateCall && shopKeeper(it->first)) ||
                        (k->second.size() > 6 && k->second.compare(k->second.size() - 6, 6, "|works") == 0))
                    {
                        ++it;
                        continue;
                    }
                    const bool idle = k->second.rfind("till:", 0) == 0 && idleBusiness(k->second);
                    if (!idle && fieldOf(k->second))
                    {
                        record("called back to the fields", it->first, k->second, "", 0, 0);
                        j.progress.erase(it->first);
                        it = j.stage.erase(it);
                        ++oddVersion_;
                        continue;
                    }
                    // (Nor is a keeper's whose till can't pay it, its owner having stopped topping it up: its business
                    // can't pay the post it left, and an open post would raise the wage it can't pay. Round 9's review.)
                    const auto* left = k->second.rfind("till:", 0) == 0 ? position(k->second.substr(5)) : nullptr;
                    if (!idle && !(left && ownerUnpaid(it->first, *left)))
                        ++vacancies_[k->second];
                    ++it;
                }
    for (const auto& [key, open] : vacancies_)
        if (open > 0 && !crewKey(key))
        {
            auto& book = memory.employers.try_emplace(key, EmployerBook{employerWage(key)}).first->second;
            book.openDays = std::min(7, book.openDays + 1);
        }
    // The idle: grown wolves of working age with no post. With LivingSlack (round 13, lane 1, the user, 2026-10-29; with
    // work by the hours), the living only, and a wolf holding a place on a week's hire (a business's, a field's, a town's,
    // the treasury's or the church's crew, a House's, about a home) is at work, not idle. The dead stay in the residents
    // (an estate is settled, the resident kept), and with no body their written age counted them: in the 2-year run
    // (8550cfe) the slack so counted stood over 3% on 9, 38, 34 and 64 days of the first year's quarters and on every
    // day of the second's, the living's on 7, 0, 0, 0, then 3, 54, 73 and 91; and the weekly calm cut (it wants the
    // slack over 3%) took about 47 log points off the going rate, about half of them on days the living's was 3% or
    // under. And the living jobless hold the week's hires' places (their days on them 1.04 to 1.21 times their own),
    // at about the going rate: working, not slack. (Bodies holds only the living: Demand and Institutions skip the rest
    // the same way. A day's odd job is the day's: not counted.)
    const bool livingSlack = workByHours() && wageRule(LivingSlack);
    const auto living = [&](const std::string& id) { return !livingSlack || bodies.empty() || bodies.count(id) > 0; };
    std::unordered_set<std::string> hired;
    if (livingSlack)
        for (const auto& j : oddJobs_)
            if (!j.ended && j.until >= day)
                for (const auto& [who, stage] : j.stage)
                    hired.insert(who);
    int grown = 0, idle = 0;
    for (const auto& [id, life] : state_.residents)
        if (const int age = ageOf(id); living(id) && age >= 16 && age < RetireAge)
        {
            ++grown;
            idle += !jobOf(id) && !apprenticedTo(id) && !hired.count(id);
        }
    slack_ = grown ? double(idle) / grown : 0;
    grown_ = grown;
    // The realm's pace toward the calibrated hours (doc 64, Phase 13): on while the week's food holds, back while the food
    // falls or runs thin, and back faster while the realm goes hungry. (Not held while the land's hands are short of what
    // its sales want, as it was: the pace stopped at the first shortage of hands, so no wage was ever bid up, and the
    // realm sat at the edge of one with its full banks' hours idle. The land's shortage is its posts open, and its wage
    // answers it, as the model's does; the food, falling as the land falls behind, steps the pace back.) A loose brake
    // instead (the review, 2026-10-26): it holds while the land's hours worked fall week on week, the realm no longer
    // following it to the land. (The model has no pace to borrow a rule from: its hours are the calibrated from day one.)
    if (workByHours())
    {
        // (With LivingSlack, the living's hungry share: a dead wolf's hunger stood where it died, a starved one's high, and
        // it counted in the realm's. Kept for the mint's harm, which reads the same: hungryShare_.)
        int hungry = 0, counted = 0;
        for (const auto& [id, life] : state_.residents)
            if (living(id))
            {
                hungry += life.hunger >= 60;
                ++counted;
            }
        const double hungryShare = counted ? double(hungry) / double(counted) : 0;
        if (livingSlack)
        {
            hungryShare_ = hungryShare;
            hungryShareSet_ = true;
        }
        const bool fed = counted == 0 || hungryShare < PaceHungry;
        // The realm's food in hand, days a head: its homes' and its shelves', each town's at most PaceCoverCap (a glutted
        // town doesn't hide a hungry one). The monitor's, measured last evening for today (if it missed the evening, none).
        const auto& last = state_.orchestrator.last;
        double held = 0, people = 0;
        if (last.day == day)
            for (const auto& t : last.towns)
                held += t.people * std::clamp(t.shopFoodDays + t.homeFoodDays, 0., PaceCoverCap), people += t.people;
        // (The last week's readings, today's among them, each by its day: one the monitor missed leaves a gap, and a
        // server away for days finds none of last week's.)
        auto& week = memory.paceCover;
        week.erase(std::remove_if(week.begin(), week.end(), [&](const auto& r) { return r.first > day || r.first <= day - PaceWeek; }),
                   week.end());
        if (people > 0)
        {
            if (!week.empty() && week.back().first == day)
                week.back().second = held / people;
            else
                week.push_back({day, held / people});
        }
        const bool weighed = week.size() >= 2 && week.front().first == day - (PaceWeek - 1) && week.back().first == day;
        // Today's against the same day last week, exactly (Restday at both ends or neither): the week's food made less
        // eaten; and the week's since then, as many of its days as were read.
        const double slide = weighed ? week.back().second - week.front().second : 0;
        double mean = 0;
        if (weighed)
        {
            for (auto r = week.begin() + 1; r != week.end(); ++r)
                mean += r->second;
            mean /= double(week.size() - 1);
        }
        // The land's hours since the last pass (its producers' but the seasonal ones', whose work is the calendar's; their
        // hands' are credited to them), counted as they are credited (memory.landToday: saved, so a restart reads on, and a
        // roster's change doesn't take a producer's whole life's hours out of the day's: round 6, the round-5 review), and
        // the brake: the last seven passes' against the seven before, by their days. (Not counting yet, a new world or an
        // older save: from now, and no reading today.)
        auto& lands = memory.paceLand;
        lands.erase(std::remove_if(lands.begin(), lands.end(), [&](const auto& r) { return r.first > day || r.first <= day - PaceLandDays; }),
                    lands.end());
        if (memory.landToday >= 0)
        {
            if (!lands.empty() && lands.back().first == day)
                lands.back().second += memory.landToday;
            else
                lands.push_back({day, memory.landToday});
        }
        memory.landToday = 0;
        bool landFalls = false;
        if (lands.size() == std::size_t(PaceLandDays) && lands.front().first == day - (PaceLandDays - 1) && lands.back().first == day)
        {
            double before = 0, since = 0;
            for (std::size_t i = 0; i < lands.size(); ++i)
                (i < lands.size() / 2 ? before : since) += lands[i].second;
            landFalls = since < before * (1 - PaceLandSlack);
        }
        double step = 0;
        if (!fed)
            step = -PaceBack;
        else if (weighed && (slide < -PaceSlide || mean < PaceFloor))
            step = -PaceStep;
        else if (weighed && slide >= -PaceNoise && !landFalls)
            step = PaceStep;
        if (const auto held = paceHeld_.find(day); held != paceHeld_.end())
            memory.pace = std::clamp(held->second, 0., 1.);   // (A test's: holdPace.)
        else
            memory.pace = std::clamp(memory.pace + step, 0., 1.);
    }
    // Each wolf's pay lately: the share of the wages owed it that its employer paid, a week's running average of its working
    // days (a new post starts a fresh record, paid in full). Paid under 40% of what it was owed, it leaves (not the only
    // hand; the watch keeps its posts). (Not the pay itself: a wolf whose work takes it about, a messenger, works fewer
    // spells at its post, and isn't badly paid for it.)
    std::vector<std::string> leaving;
    for (const auto& [id, key] : keyOf)
    {
        const auto was = payKey_.find(id);
        auto& lately = pay7_[id];
        if (was == payKey_.end() || was->second != key)
        {
            payKey_[id] = key;
            lately = 1;
        }
        else if (const auto owed = owedToday_.find(id); owed != owedToday_.end() && owed->second > 0)
            lately += ((paidToday_.count(id) ? paidToday_.at(id) : 0.) / owed->second - lately) / 7;
        const auto* job = jobOf(id);
        if (job && job->role != "merchant" && job->role != "guard" && lately < LeaveBelow && handsOf(key) > 1)
            leaving.push_back(id);
    }
    for (auto it = pay7_.begin(); it != pay7_.end();)
        it = keyOf.count(it->first) ? std::next(it) : (payKey_.erase(it->first), pay7_.erase(it));
    paidToday_.clear();
    owedToday_.clear();
    for (const auto& id : leaving)
        if (const auto* job = jobOf(id))
        {
            auto& ps = state_.careers.positions[job->id];
            ps.lastHolder = id;
            ps.holder.clear();
            ps.vacantSince = double(day);
            record("left a post that didn't pay", id, job->id, "", 0, 0);
            pay7_.erase(id);
            payKey_.erase(id);
            forgetCareers();
        }
    // What its makers made lately, and the crafts they were short of materials for yesterday (doc 64, Phase 13): a
    // fortnight's running average, for their hands wanted.
    for (auto& [till, goods] : madeRate_)
        for (auto& [good, rate] : goods)
            rate *= 13. / 14;
    for (const auto& [till, goods] : madeToday_)
        for (const auto& [good, n] : goods)
            madeRate_[till][good] += n / 14;
    madeToday_.clear();
    shortYesterday_ = std::move(shortToday_);
    shortToday_.clear();
    // A field's next year planned at the harvest's end, its farmer at work or away on a hire (doc 64, Phase 13).
    if (fieldsOn() && day % calendar::DaysPerYear >= HarvestEnd)
        for (const auto& [till, f] : memory.fields)
            if (const auto* p = f.year < day / calendar::DaysPerYear && till.rfind("till:", 0) == 0 ? position(till.substr(5)) : nullptr)
                if (const auto* producer = items::producerFor(p->title); producer && fieldWork(producer->id))
                {
                    const auto held = state_.careers.positions.find(p->id);
                    planField(held != state_.careers.positions.end() && !held->second.holder.empty() ? held->second.holder : p->founder, till,
                              *producer, double(day), false);
                }
    // Each business's hands wanted, once a day: its review's, and the going rate's weights. (A producer's by its sales as
    // they are, without the half again of a shelf near bare: its goods go on to the shops, and its shelf always is, so it
    // stood half again short for good. Its hire is sized by them too, so a slot nobody takes is a hand its sales want:
    // doc 64, Phase 13, round 6, the round-5 review. Before, only its posts open past its hire were.)
    std::unordered_map<std::string, int> wanted;
    std::map<std::string, int> onHires;             // (A business's hands at work on its hires: below.)
    std::map<std::string, int> recalled;            // (Of them, shops' keepers called back at the pass's end: CalledBackLate.)
    if (workByHours())
        for (const auto& [key, n] : hands_)
            if (key.rfind("till:", 0) == 0)
            {
                const auto* p = position(key.substr(5));
                wanted[key] = handsWanted(key, !(p && items::producerFor(p->title)));
            }
    const auto wantedOf = [&](const std::string& key) {
        const auto found = wanted.find(key);
        return found != wanted.end() ? found->second : handsWanted(key);
    };
    // The hands a business wants and has neither at work nor on its hires stand open too (doc 63's model: its posts are
    // the hands it wants less those it has, two and twice its hands at most; doc 64, Phase 13), though it couldn't post
    // a hire for them (one at a time, eight at most, its till short): short all the same, its weekly review bids its
    // wage up. Before, only a hire's slots nobody took counted, and the land's hands, short by a fifth, moved no wage.
    // (Not one with no work for more hands. A producer's by its sales as they are, above. With work by the hours: the
    // seconds' pace as it was.)
    if (workByHours())
    {
        // (Its hires' grown hands: a child's place stands open, above. With CalledBackLate, the shops' keepers among them
        // too, counted apart: they are its hands until the pass ends, and the day refills their places, so its posts
        // missing, its hiring and its bid read them; the going rate's weight leaves them out, as before.)
        for (const auto& j : oddJobs_)
            if (j.until >= day && j.payer.rfind("till:", 0) == 0 && (j.kind == "a hand" || j.kind == "a hand at the shop"))
            {
                onHires[j.payer] += int(j.stage.size()) - children(j);
                if (lateCall)
                    for (const auto& [who, stage] : j.stage)
                        if (shopKeeper(who) && !(wageRule(ChildrenOpen) && ageOf(who) < 16))
                            ++recalled[j.payer];
            }
        for (const auto& [key, n] : hands_)
            if (key.rfind("till:", 0) == 0 && makesThings(key) && !idleBusiness(key))
            {
                const int want = wantedOf(key);
                const int missing = std::min(want, 2 * n + 2) - n - onHires[key];
                if (auto& open = vacancies_[key]; missing > open)
                {
                    if (open == 0)                   // (Open today: its day counted, as any post open.)
                    {
                        auto& book = memory.employers.try_emplace(key, EmployerBook{employerWage(key)}).first->second;
                        book.openDays = std::min(7, book.openDays + 1);
                    }
                    open = missing;
                }
            }
    }
    // The going rate (doc 63's model, market_wage; doc 64, Phase 13): the middle of what the unskilled trades that make
    // things pay, each as a labourer's day (its skill's premium off), counted by its employees, one at least: its help
    // and its hires' grown hands, not its keeper or farmer (round 7, the user, 2026-10-26; the model's max(1, workers),
    // whose owners aren't workers). Before, by the hands its work wanted, one at least and those it had at most, keeper
    // and all: three in four of the weights were one-hand businesses that never bid, and the rate stood at 60p for 120
    // days. The makers and the land's producers, but its seasonal fields (their pay follows the rate); not a stall or a
    // moneychanger, whose wages no market moves. A town's own once TownRateWeight hands count in it, else the realm's:
    // one market, as the model's, so a small town's producers are kept to twice the realm's rate. (At the seconds' pace,
    // as it was: each town's own, the middle of what its unskilled shops pay, by their hands.)
    std::map<std::string, std::vector<double>> paid;
    for (const auto& [key, n] : hands_)
    {
        const auto* p = key.rfind("till:", 0) == 0 ? position(key.substr(5)) : nullptr;
        const auto* producer = p ? items::producerFor(p->title) : nullptr;
        if (!workByHours())
        {
            if (p && p->role == "merchant" && employerSkill(key) <= 10)
                for (int k = 0; k < n; ++k)
                {
                    paid[employerTown_[key]].push_back(employerWage(key));
                    paid[""].push_back(employerWage(key));
                }
            continue;
        }
        if (!p || employerSkill(key) > 10 || !makesThings(key) || (producer && fieldsOn() && fieldWork(producer->id)))
            continue;
        const auto held = state_.careers.positions.find(p->id);
        const int keeper = held != state_.careers.positions.end() && !held->second.holder.empty();
        // (With CalledBackLate, the shops' keepers on its hires are left out of its weight, as they were when called back
        // before the pass: a boat's hands weighed 111 of 437 on day 1364 of round 14's six years.)
        const auto back = lateCall ? recalled.find(key) : recalled.end();
        const int weight = wageRule(ByEmployees) ? std::max(1, n - keeper + onHires[key] - (back != recalled.end() ? back->second : 0))
                                                 : std::clamp(wantedOf(key), 1, std::max(1, n));
        const double labourer = employerWage(key) / (1 + employerSkill(key) / items::ladder().skillWage);
        for (int k = 0; k < weight; ++k)
        {
            paid[employerTown_[key]].push_back(labourer);
            paid[""].push_back(labourer);
        }
    }
    // (A realm founded at its founding day with no such makers to measure today keeps the rate it had: the catalogue's
    // 12p is no fallback for a realm founded at another, doc 64, Phase 13. A village, or a realm founded before, falls
    // back to it, as it did.)
    const auto realm = memory.going.find("");
    const double kept = realm != memory.going.end() && memory.foundedDay > 0 ? realm->second : 0;
    // (At the seconds' pace a town keeps its last, as before the hours; the realm's is as above at either pace. Merged
    // in round 5, doc 64, Phase 13: kept whole at that pace, a village's never fell back.)
    for (auto it = memory.going.begin(); it != memory.going.end();)
        it = workByHours() || it->first.empty() ? memory.going.erase(it) : std::next(it);
    for (auto& [town, wages] : paid)
        if (!wages.empty() && (town.empty() || !workByHours() || int(wages.size()) >= TownRateWeight))
        {
            std::nth_element(wages.begin(), wages.begin() + wages.size() / 2, wages.end());
            memory.going[town] = wages[wages.size() / 2];
        }
    if (kept > 0 && !memory.going.count(""))
        memory.going[""] = kept;
    // A business short of hands hires the day it finds itself short (the land can't wait for the week's end); it lets
    // hands go only at its weekly review.
    if (workByHours())
        for (const auto& [key, n] : hands_)
            if (key.rfind("till:", 0) == 0 && std::int64_t(std::hash<std::string>{}(key + "|wage") % 7) != day % 7)
                reviewHands(key, day, false, wantedOf(key));
    // Each employer's weekly review, on its own day: a business's hands by its sales, then every employer's wage.
    std::set<std::string> employers;
    for (const auto& [key, n] : hands_)
        employers.insert(key);
    for (const auto& [key, n] : vacancies_)
        if (!crewKey(key))
            employers.insert(key);
    for (const auto& [key, book] : memory.employers)
        employers.insert(key);
    for (const auto& key : employers)
    {
        if (std::int64_t(std::hash<std::string>{}(key + "|wage") % 7) != day % 7)
            continue;
        if (key.rfind("till:", 0) == 0 && workByHours())
            reviewHands(key, day, true, wantedOf(key));
        auto& book = memory.employers.try_emplace(key, EmployerBook{employerWage(key)}).first->second;
        const auto town = employerTown_.count(key) ? employerTown_.at(key) : std::string();
        const double rate = goingRate(town) * (1 + employerSkill(key) / items::ladder().skillWage);
        const auto words = key + "|" + std::to_string(day);
        const int open = vacanciesOf(key), hands = handsOf(key);
        if (book.wage <= 0)
            book.wage = employerWage(key);
        if (key.size() > 6 && key.compare(key.size() - 6, 6, "|works") == 0)
        {
            book.wage = goingRate(town) * WorksShare;     // (The works: the employer of last resort, at its share.)
            book.openDays = 0;
            continue;
        }
        // A week it fell short paying: cut, once (up to 4%). (With WageBookMended, nothing more this week: below.)
        const bool shortWeek = book.shortPay;
        if (book.shortPay)
        {
            book.shortPay = false;
            book.wage *= 1 - lot(words + "|short") * WageDown * 2;
        }
        // A business's bid by its sales (round 12, lane A, the user, 2026-10-29: WageBySales; with work by the hours). Its
        // posts open past the hands its sales want are MoreHands' extra, hands it could take on out of what it holds
        // over, not hands it lacks: they bid no wage (the round-11 year's spiral: tills holding spare money stood short
        // of twice their hands, bid, and the going rate, their middle, went 60p to 115p, and every price, built from it,
        // with it). And it bids only while its trade lately (memory.tradeNet: its sales less its goods and materials,
        // a day, over SalesDays) pays its hands, its hires' hands and its posts open by its sales at the higher wage:
        // short of hands but unable to pay more, it keeps its posts open (its hires still post) at the wage it pays.
        // (Not the town's, the church's or a House's own posts: their budgets pay them. A stall, which makes nothing,
        // bids as before.)
        const bool bySales = workByHours() && wageRule(WageBySales) && key.rfind("till:", 0) == 0 && makesThings(key);
        int bidOpen = open;
        if (bySales && wageRule(MoreHands))
        {
            const auto* p = position(key.substr(5));
            const int extra = wantedOf(key) - handsWanted(key, !(p && items::producerFor(p->title)), false);
            bidOpen = std::max(0, open - std::max(0, extra));
        }
        const double up = std::min(book.wage * (1 + (.3 + .7 * lot(words + "|up")) * (WageUp + (WageUpShort - WageUp) *
                                                                                   std::min(1., double(bidOpen) / std::max(1, bidOpen + hands) * 2))),
                                   rate * WageMost);
        const auto onHire = onHires.find(key);
        const bool pays = !bySales || (bidOpen > 0 && tradeNetOf(key) >= up * std::max(1, hands + (onHire != onHires.end() ? onHire->second : 0) + bidOpen));
        // The week's one move (round 13, lane 1, the user, 2026-10-29: WageBookMended; with work by the hours; every
        // employer, its household's help, the town's and the church's posts too). Round 12's short-pay cut came out of
        // the chain (its second, below it, could never fire: the flag was cleared), so a short week ran the bid and the
        // calm as well: calm was counted in 94-97% of the short weeks, 2 to 97 a quarter were cut twice (over 4% in one
        // review), and with round 12's lane A off 2,138 of 3,893 ended higher (cut, then bid). (The largest short payers
        // are households' help and the town's and the church's posts.) Now a short week is cut and that is all: no bid, and
        // no week of calm (it wasn't calm). And a business short of hands its sales want whose trade can't pay the
        // higher wage (WageBySales: bidOpen, its four open days, and not `pays`) keeps its wage and its weeks of calm as
        // doc 64 says it does: before, it fell through to the calm cut, a fifth to a third of those cuts by hands, at
        // tills with four open days. (Its drift back needs its posts filled: it is left out of that already, below.)
        if (workByHours() && wageRule(WageBookMended))
        {
            const bool shortCantPay = bySales && bidOpen > 0 && book.openDays >= OpenDays && !pays;
            if (!shortWeek && book.openDays >= OpenDays && pays)
            {
                book.wage = up;
                book.calm = 0;
            }
            else if (!shortWeek && !shortCantPay && ++book.calm >= CalmWeeks && slack_ > .03)
                book.wage *= 1 - lot(words + "|down") * WageDown;
        }
        else if (book.openDays >= OpenDays && pays)
        {
            book.wage = up;
            book.calm = 0;
        }
        else if (++book.calm >= CalmWeeks && slack_ > .03)
            book.wage *= 1 - lot(words + "|down") * WageDown;
        if (open == 0 && book.wage > rate * WageHigh)
            book.wage *= .95;
        // Its posts filled (none open, or none it bid for), it drifts WageBack of the way back to the rate a week, not
        // below it: the round-11 rule's one way out of a wage bid up was CalmWeeks (six) weeks calm and a slack realm,
        // two in a hundred at most, or 5% a week once it stood over WageHigh (1.3) of the rate. (WageBySales.)
        if (bySales && (bidOpen == 0 || book.openDays < OpenDays) && book.wage > rate)
            book.wage = rate + (book.wage - rate) * (1 - WageBack);
        const bool clergy = key.size() > 7 && key.compare(key.size() - 7, 7, "|clergy") == 0;
        book.wage = std::max(book.wage, rate * WageFloor * (clergy ? ClergyShare : 1.));
        book.openDays = 0;
    }
    // The shops' keepers on hires called back now, the day's measures and reviews done (CalledBackLate: round 16, lane 1;
    // the same keepers, at the same turn of the day, as tendTills' call, so the work done is as it was).
    if (lateCall)
        callShopsBack();
    // A business's hire (not a field's: the land's sowing and harvest keep every hand they can get) held by a keeper
    // drawing its own till's wage is the day's, as a child's crew place is: at dawn the jobless' again, the keeper's
    // from WagedFrom if none took it (round 12, lane A, the user, 2026-10-29: WagelessFirst). Released after the day's
    // measures, so a place a keeper took yesterday isn't read as open, and bids no wage, today. (The round-11 year's
    // producers, drawing their farms' keeper's wage, took 5.0k to 8.0k of the hires' days a quarter while the paid
    // hands' fell 13.5k to 8.6k, and those ran out of money by day 336.)
    if (workByHours() && wageRule(WagelessFirst))
        for (auto& j : oddJobs_)
            if (businessHire(j) && j.until >= day)
                for (auto it = j.stage.begin(); it != j.stage.end();)
                    if (keeperWaged(it->first))
                    {
                        j.progress.erase(it->first);
                        it = j.stage.erase(it);
                        ++oddVersion_;
                    }
                    else
                        ++it;
    // Whose work in hand is full at this turn of the day: a business full at the next too has more than it uses.
    fullBanks_.clear();
    for (const auto& [account, banked] : workBanked_)
        if (banked >= WorkBankedMost - 1)
            fullBanks_.insert(account);
    ++memory.revision;
}
} // namespace ratw
