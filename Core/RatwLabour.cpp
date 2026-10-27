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

int Society::handsWanted(const std::string& till, bool bare) const
{
    const auto* p = till.rfind("till:", 0) == 0 ? position(till.substr(5)) : nullptr;
    const auto rates = sellRate_.find(till);
    const auto* shelves = account(till);
    if (!p || !shelves)
        return 1;
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
    return std::max(1, int(std::ceil(hours / PaidSpells - 1e-9)));
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
    // or a stall alike (round 8, the user, 2026-10-27; in round 7 four were let go as hands).
    std::set<std::string> learners;
    if (weekly && wageRule(KeepApprentices))
        for (const auto& post : posts)
            if (const auto ps = state_.careers.positions.find(post); ps != state_.careers.positions.end() && !ps->second.holder.empty() &&
                                                                     apprenticedTo(ps->second.holder))
                learners.insert(ps->second.holder);
    const auto letGo = [&](int excess) {
        // Half the excess, one at least, the post kept empty.
        int n = std::max(1, excess / 2);
        for (const auto& post : posts)
        {
            auto& ps = state_.careers.positions[post];
            if (n <= 0 || ps.holder.empty() || learners.count(ps.holder))
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
        const auto pay = std::int64_t(std::ceil(employerWage(till) * (harvest ? HarvestPay : 1.) - 1e-9));
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
    if (workByHours())
        for (auto& j : oddJobs_)
            if (j.until >= day && ((j.payer.rfind("till:", 0) == 0 && (j.kind == "a hand" || j.kind == "a hand at the shop")) || crewPlace(j)))
                for (auto it = j.stage.begin(); it != j.stage.end();)
                {
                    const auto k = keyOf.find(it->first);
                    if (k == keyOf.end() || k->second == j.payer || keepers.count(it->first) ||
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
    // The idle: grown wolves of working age with no post.
    int grown = 0, idle = 0;
    for (const auto& [id, life] : state_.residents)
        if (const int age = ageOf(id); age >= 16 && age < RetireAge)
        {
            ++grown;
            idle += !jobOf(id) && !apprenticedTo(id);
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
        int hungry = 0;
        for (const auto& [id, life] : state_.residents)
            hungry += life.hunger >= 60;
        const bool fed = state_.residents.empty() || double(hungry) / double(state_.residents.size()) < PaceHungry;
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
        // (Its hires' grown hands: a child's place stands open, above.)
        for (const auto& j : oddJobs_)
            if (j.until >= day && j.payer.rfind("till:", 0) == 0 && (j.kind == "a hand" || j.kind == "a hand at the shop"))
                onHires[j.payer] += int(j.stage.size()) - children(j);
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
        const int weight = wageRule(ByEmployees) ? std::max(1, n - keeper + onHires[key]) : std::clamp(wantedOf(key), 1, std::max(1, n));
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
        if (book.shortPay)
        {
            book.shortPay = false;
            book.wage *= 1 - lot(words + "|short") * WageDown * 2;
        }
        else if (book.openDays >= OpenDays)
        {
            const double short_ = double(open) / std::max(1, open + hands);
            const double step = WageUp + (WageUpShort - WageUp) * std::min(1., short_ * 2);
            book.wage = std::min(book.wage * (1 + (.3 + .7 * lot(words + "|up")) * step), rate * WageMost);
            book.calm = 0;
        }
        else if (++book.calm >= CalmWeeks && slack_ > .03)
            book.wage *= 1 - lot(words + "|down") * WageDown;
        if (open == 0 && book.wage > rate * WageHigh)
            book.wage *= .95;
        const bool clergy = key.size() > 7 && key.compare(key.size() - 7, 7, "|clergy") == 0;
        book.wage = std::max(book.wage, rate * WageFloor * (clergy ? ClergyShare : 1.));
        book.openDays = 0;
    }
    // Whose work in hand is full at this turn of the day: a business full at the next too has more than it uses.
    fullBanks_.clear();
    for (const auto& [account, banked] : workBanked_)
        if (banked >= WorkBankedMost - 1)
            fullBanks_.insert(account);
    ++memory.revision;
}
} // namespace ratw
