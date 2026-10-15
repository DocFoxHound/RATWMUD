// The labour market (Docs/Design/64-economy-migration.md, Phase 6; doc 63's model): every employer its own wage, moved
// by its open posts; the going rate from what the unskilled trades pay; wolves judging their posts by what they paid.
// See RatwSociety.h.
#include "RatwItems.h"
#include "RatwSociety.h"

#include <algorithm>
#include <cmath>
#include <functional>
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
    if (const auto found = going_.find(town); found != going_.end())
        return found->second;
    if (const auto realm = going_.find(""); realm != going_.end())
        return realm->second;
    const auto& start = orchestratorDials().wageStart;
    const auto labour = start.find("labour");
    return labour != start.end() ? labour->second : 12;
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
        state_.memory.employers.try_emplace(key, EmployerBook{employerWage(key)}).first->second.shortPay = true;
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

std::vector<std::string> Society::helpPosts(const std::string& till) const
{
    std::vector<std::string> posts;
    for (const auto& p : positions_)
        if (p.role != "merchant")
            if (const auto key = postKey_.find(p.id); key != postKey_.end() && key->second == till)
                posts.push_back(p.id);
    return posts;
}

int Society::handsWanted(const std::string& till) const
{
    const auto* p = till.rfind("till:", 0) == 0 ? position(till.substr(5)) : nullptr;
    const auto rates = sellRate_.find(till);
    const auto* shelves = account(till);
    if (!p || !shelves)
        return 1;
    // A good's sales a day, as its maker plans them: a fifth over for a staple, half again with its shelf near bare, a
    // fifth less with it glutted (doc 63's model).
    const auto planned = [&](const std::string& good) {
        const double rate = rates == sellRate_.end() || !rates->second.count(good) ? 0. : rates->second.at(good);
        const auto* g = items::good(good);
        if (rate <= 0 || !g)
            return 0.;
        const double days = stockAll(*shelves, good) / rate, keep = keepDays(good);
        return rate * (g->staple ? StapleSlack : 1.) * (days < keep * .3 ? 1.5 : days > keep * 1.7 ? .8 : 1.);
    };
    // The hours a batch (or a yield) takes, for the most-wanted of what it makes.
    const auto hoursFor_ = [&](const std::vector<std::pair<std::string, int>>& out, double hours) {
        double most = 0;
        for (const auto& [good, n] : out)
            most = std::max(most, planned(good) / std::max(1, n));
        return most * hours;
    };
    double hours = 0;
    if (const auto* producer = items::producerFor(p->title))
        hours = hoursFor_(producer->out, hoursFor(*producer));
    else if (const auto* business = items::businessFor(p->title))
        for (const auto* k : items::craftsFor(business->id))
            hours += hoursFor_(k->out, hoursFor(*k));
    return std::max(1, int(std::ceil(hours / PaidSpells - 1e-9)));
}

bool Society::idleHand(const std::string& resident, const Payer& payer) const
{
    // A hand whose business has more work in hand than it can use (its shelves full, its materials short): free to take
    // a hire elsewhere (doc 63's harvest call: hands from trades with nothing to do go where they're wanted).
    if (payer.account.rfind("till:", 0) != 0 || jobOf(resident) == nullptr || jobOf(resident)->role == "merchant")
        return false;
    const auto banked = workBanked_.find(payer.account);
    return banked != workBanked_.end() && banked->second >= WorkBankedMost - 1;
}

void Society::reviewHands(const std::string& till, std::int64_t day, bool weekly)
{
    const auto pid = till.substr(5);
    const auto* p = position(pid);
    const auto held = state_.careers.positions.find(pid);
    if (!p || held == state_.careers.positions.end() || held->second.holder.empty() || !account(till))
        return;
    const auto& keeper = held->second.holder;
    const auto posts = helpPosts(till);
    int hired = 0;
    for (const auto& j : oddJobs_)
        if (j.payer == till && j.until >= day && (j.kind == "a hand" || j.kind == "a hand at the shop"))
            hired += j.slots;
    const int hands = handsOf(till) + hired;
    int wanted = std::min(handsWanted(till), 2 * hands + 2);
    if (wanted > hands)
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
        const bool harvest = producer && farmWork(producer->id) &&
                             (producer->seasons.empty() || std::find(producer->seasons.begin(), producer->seasons.end(), season_) != producer->seasons.end()) &&
                             (season_ == 0 || season_ == 2);   // (Sowing in spring, the harvest at summer's end: doc 63.)
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
            j.what = std::string(harvest ? "a hand at the harvest at " : "a hand at ") + p->title.substr(0, 40) + ", hired for the week";
            j.from = j.to = p->role == "merchant" ? p->serve : p->work;
            j.slots = slots;
            j.pay = pay * slots;
            j.forChildren = false;
            j.until = day + HireDays - 1;
            oddJobs_.push_back(std::move(j));
            ++oddVersion_;
        }
    }
    // (Not while the realm is still on its way to the calibrated hours: what its sales take grows week by week.)
    else if (const int excess = hands - wanted - hired; weekly && excess > 0 && state_.memory.pace >= 1)
    {
        // Over: its hires run out by themselves; then help let go, half the excess (one at least), the post kept empty.
        int letGo = std::max(1, excess / 2);
        for (const auto& post : posts)
        {
            auto& ps = state_.careers.positions[post];
            if (letGo <= 0 || ps.holder.empty())
                continue;
            record("laid off", till, ps.holder, "", 0, 0);
            ps.lastHolder = ps.holder;
            ps.holder.clear();
            ps.vacantSince = double(day);
            state_.memory.closed.insert(post);
            ++state_.memory.revision;
            forgetCareers();
            --letGo;
        }
    }
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
        std::string key;
        if (p.role == "merchant" || items::producerFor(p.title))
            key = "till:" + p.id;                    // (Its keeper or farmer: its business's first hand.)
        else
            key = employerKey(payerOf(holder.empty() ? p.founder : holder, p, holder.empty() ? 30 : ageOf(holder)), p);
        if (key.empty() || (key.rfind("till:", 0) == 0 && !account(key)))
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
    // Its hires not yet taken are posts open too.
    for (const auto& j : oddJobs_)
        if (j.until >= day && j.payer.rfind("till:", 0) == 0 && int(j.stage.size()) < j.slots)
            vacancies_[j.payer] += j.slots - int(j.stage.size());
    for (const auto& [key, open] : vacancies_)
        if (open > 0)
        {
            auto& book = memory.employers.try_emplace(key, EmployerBook{employerWage(key)}).first->second;
            book.openDays = std::min(7, book.openDays + 1);
        }
    // The going rate in each town: the middle of what its unskilled businesses pay (not the land's), by their hands.
    std::map<std::string, std::vector<double>> paid;
    for (const auto& [key, n] : hands_)
    {
        const auto* p = key.rfind("till:", 0) == 0 ? position(key.substr(5)) : nullptr;
        if (!p || p->role != "merchant" || employerSkill(key) > 10)
            continue;
        for (int k = 0; k < n; ++k)
        {
            paid[employerTown_[key]].push_back(employerWage(key));
            paid[""].push_back(employerWage(key));
        }
    }
    for (auto& [town, wages] : paid)
        if (!wages.empty())
        {
            std::nth_element(wages.begin(), wages.begin() + wages.size() / 2, wages.end());
            going_[town] = wages[wages.size() / 2];
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
    // The realm's pace toward the calibrated hours: on while it's fed, back while it goes hungry.
    if (workByHours())
    {
        int hungry = 0;
        for (const auto& [id, life] : state_.residents)
            hungry += life.hunger >= 60;
        const bool fed = state_.residents.empty() || double(hungry) / double(state_.residents.size()) < PaceHungry;
        memory.pace = std::clamp(memory.pace + (fed ? PaceStep : -PaceBack), 0., 1.);
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
    // A business short of hands hires the day it finds itself short (the land can't wait for the week's end); it lets
    // hands go only at its weekly review.
    if (workByHours())
        for (const auto& [key, n] : hands_)
            if (key.rfind("till:", 0) == 0 && std::int64_t(std::hash<std::string>{}(key + "|wage") % 7) != day % 7)
                reviewHands(key, day, false);
    // Each employer's weekly review, on its own day: a business's hands by its sales, then every employer's wage.
    std::set<std::string> employers;
    for (const auto& [key, n] : hands_)
        employers.insert(key);
    for (const auto& [key, n] : vacancies_)
        employers.insert(key);
    for (const auto& [key, book] : memory.employers)
        employers.insert(key);
    for (const auto& key : employers)
    {
        if (std::int64_t(std::hash<std::string>{}(key + "|wage") % 7) != day % 7)
            continue;
        if (key.rfind("till:", 0) == 0 && workByHours())
            reviewHands(key, day, true);
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
    ++memory.revision;
}
} // namespace ratw
