// The society's side of the economy orchestrator (Docs/Design/46-economy-orchestrator.md): what it counts between
// snapshots, the snapshot it takes once a day at SnapshotHour, the brief it takes when the day turns, and the Dungeon
// Master's steers. Phase 1 (shadow): the brief is kept and shown; nothing in it is applied.
#include "RatwItems.h"
#include "RatwSociety.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace ratw
{
namespace
{
// Data/Economy: RATW_DATA_DIR (the Data directory), else the working directory or one above it, else the source tree.
std::filesystem::path economyDir()
{
    namespace fs = std::filesystem;
    std::error_code ec;
    if (const char* dir = std::getenv("RATW_DATA_DIR"); dir && *dir)
        return fs::path(dir) / "Economy";
    for (auto at = fs::current_path(ec); !ec && !at.empty(); at = at.parent_path())
    {
        if (fs::exists(at / "Data" / "Economy" / "orchestrator.json", ec))
            return at / "Data" / "Economy";
        if (at == at.parent_path())
            break;
    }
#ifdef RATW_SOURCE_DIR
    return fs::path(RATW_SOURCE_DIR) / "Data" / "Economy";
#else
    return fs::path("Data") / "Economy";
#endif
}

orchestra::Dials loadDials()
{
    orchestra::Dials dials;
    std::ifstream file(economyDir() / "orchestrator.json", std::ios::binary);
    if (file)
    {
        std::ostringstream text;
        text << file.rdbuf();
        std::string problem;
        orchestra::Dials read;
        if (orchestra::readDialsText(text.str(), read, problem))
            dials = read;
    }
    if (const char* mode = std::getenv("RATW_ORCHESTRATOR"); mode && (std::string(mode) == "off" || std::string(mode) == "shadow"))
        dials.mode = mode;
    return dials;
}

// Money a resident is given rather than earns (for "idle": a wolf that earned nothing in three days).
bool unearned(const std::string& kind)
{
    static const char* const words[] = {"inheritance", "operator", "the shop's till", "sale of a business", "starting money",
                                        "a child's", "stipend", "household purse", "dole", "the church's share", "alms",
                                        "beggar", "money for the road", "welcome grant", "world's money", "for the poor",
                                        "a gift", "enclosure"};   // (Doc 55: given, not earned.)
    for (const auto* w : words)
        if (kind.find(w) != std::string::npos)
            return true;
    return false;
}
// What a holder spends in the ordinary way: not what it passes to its own house or another collector, and not what the
// rules against hoarding or the orchestrator make it spend.
bool ordinarySpending(const std::string& kind)
{
    return kind.rfind("surplus", 0) != 0 && kind.rfind("orders:", 0) != 0 && kind != "house takings" && kind != "the shop's till" &&
           kind != "sale of a business" && kind != "the household purse" && kind != "the churches' purses joined" &&
           kind != "capital's share" && kind != "starting money";
}
} // namespace

const orchestra::Dials& Society::orchestratorDials() const
{
    if (dialsOverride_)
        return *dialsOverride_;
    static const orchestra::Dials dials = loadDials();
    return dials;
}

bool Society::orchestrating() const
{
    return roster_ == Roster::Authored && orchestratorDials().mode != "off";
}

void Society::forgetOrchestra()
{
    if (orchestraRunner_ && orchestraRunner_->pending())
        orchestraRunner_->take();                    // (A plan for the old society is dropped.)
    snapshotDay_ = countingFrom_ = -1;
    accountTown_.clear();
    holderSpent_.clear();
    townFlow_.clear();
    earned_.clear();
}

void Society::setOrchestratorThread(bool threaded)
{
    if (threaded == orchestraThreaded_ && (orchestraRunner_ || !threaded))
        return;
    orchestraThreaded_ = threaded;
    // A plan under way is finished first (on the old runner), so nothing is lost.
    std::optional<std::pair<orchestra::Brief, orchestra::Memory>> pending;
    if (orchestraRunner_ && orchestraRunner_->pending())
        pending = orchestraRunner_->take();
    orchestraRunner_.runner = std::make_shared<orchestra::Runner>(threaded);
    if (pending)
    {
        // (Kept as the brief it was: handed back when the day turns, as if planned on the new runner.)
        state_.orchestrator.memory = pending->second;
        state_.orchestrator.last = pending->first;
        if (pending->first.decided)
            state_.orchestrator.decision = pending->first;
        applyPrices();
        if (keepBriefs_)
            briefs_.push_back(pending->first);
    }
}

std::vector<orchestra::Brief> Society::takeBriefs()
{
    std::vector<orchestra::Brief> out;
    out.swap(briefs_);
    return out;
}

EconomyResult Society::steer(orchestra::Steer s, int days)
{
    auto& o = state_.orchestrator;
    s.from = state_.budgetDay + 1;
    s.until = s.from + days;
    std::string problem;
    if (!orchestra::validSteer(s, problem))
        return {false, problem};
    if (o.steers.size() >= 64)
        return {false, "Sixty-four steers are in force: end one first."};
    if (s.id.empty())
        s.id = "steer-" + std::to_string(o.nextSteer++);
    for (const auto& other : o.steers)
        if (other.id == s.id)
            return {false, "That steer is already in force."};
    o.steers.push_back(s);
    return {true, "From tomorrow's plan, for " + std::to_string(days) + (days == 1 ? " day." : " days.")};
}

bool Society::unsteer(const std::string& id)
{
    auto& steers = state_.orchestrator.steers;
    const auto before = steers.size();
    steers.erase(std::remove_if(steers.begin(), steers.end(), [&](const orchestra::Steer& s) { return s.id == id; }), steers.end());
    return steers.size() != before;
}

void Society::noteForOrchestra(const std::string& kind, const std::string& from, const std::string& to, std::int64_t coins)
{
    if (coins <= 0 || accountTown_.empty())
        return;
    // Reach (Phase 7): what a channel's fund pays, and to whom; what a till pays out, and how much to the poorer half.
    if (from.rfind("fund:", 0) == 0 && to.rfind("fund:", 0) != 0 && from != LandFund)
        if (const auto second = from.find(':', 5); second != std::string::npos)
        {
            auto channel = from.substr(second + 1);
            std::replace(channel.begin(), channel.end(), '_', ' ');
            auto& week = channelWeek_[channel];
            week.second += double(coins);
            if (poorHalf_.count(to))
                week.first += double(coins);
            else if (to.rfind("till:", 0) == 0)
                tillFrom_[to][channel] += double(coins);
        }
    if (from.rfind("till:", 0) == 0)
    {
        auto& week = tillWeek_[from];
        week.second += double(coins);
        if (poorHalf_.count(to))
            week.first += double(coins);
    }
    if (ordinarySpending(kind))
        if (const auto h = holderSpent_.find(from); h != holderSpent_.end())
            h->second += coins;
    // Coins that go from one town to another (an account of no town, the road's or the land church's, doesn't count: what
    // the church takes in tithes it gives back as alms and wages in every town).
    const auto a = accountTown_.find(from), b = accountTown_.find(to);
    if (a != accountTown_.end() && b != accountTown_.end() && !a->second.empty() && !b->second.empty() && a->second != b->second)
    {
        townFlow_[a->second].second += coins;
        townFlow_[b->second].first += coins;
    }
    // What a resident earns.
    if (!unearned(kind))
        if (const auto e = earned_.find(to); e != earned_.end())
        {
            const auto slot = std::size_t(((state_.budgetDay % 7) + 7) % 7);
            if (e->second.day[slot] != state_.budgetDay)
                e->second.day[slot] = state_.budgetDay, e->second.coins[slot] = 0;
            e->second.coins[slot] += coins;
        }
}

bool Society::wantsSnapshot(double absoluteDay) const
{
    if (!orchestrating())
        return false;
    const auto day = std::int64_t(std::floor(absoluteDay));
    return (absoluteDay - double(day)) * 24 >= SnapshotHour && snapshotDay_ < day + 1;
}

void Society::orchestrate(double absoluteDay, const std::map<std::string, LifeBody>& bodies)
{
    if (!orchestrating())
        return;
    if (!orchestraRunner_)
        orchestraRunner_.runner = std::make_shared<orchestra::Runner>(orchestraThreaded_);
    const auto day = std::int64_t(std::floor(absoluteDay));
    // The day turns: the brief planned for it (the game waits for it here, so every run sees it at the same moment).
    if (day > state_.budgetDay && orchestraRunner_->pending())
    {
        const auto forDay = orchestraRunner_->pendingDay();
        auto [brief, memory] = orchestraRunner_->take();
        if (forDay == day)                           // (A plan for a day already gone is dropped.)
        {
            state_.orchestrator.memory = std::move(memory);
            state_.orchestrator.last = brief;
            if (brief.decided)
                state_.orchestrator.decision = brief;
            applyPrices();                           // Its prices and margin, when it is on (doc 46, Phase 3).
            applyOrders(brief);                      // And the week's orders: money into the channels' funds (Phase 5).
            applyGrants(brief);                      // And the banks' grants to those short (money that stops).
            if (keepBriefs_)
                briefs_.push_back(std::move(brief));
        }
    }
    // An hour before the day turns: the snapshot for tomorrow, handed to the orchestrator's thread.
    if (wantsSnapshot(absoluteDay) && !bodies.empty())
    {
        snapshotDay_ = day + 1;
        orchestraRunner_->submit(orchestraSnapshot(day + 1, bodies), state_.orchestrator.memory);
    }
}

orchestra::Snapshot Society::orchestraSnapshot(std::int64_t forDay, const std::map<std::string, LifeBody>& bodies)
{
    using orchestra::HolderKind;
    const auto& dials = orchestratorDials();
    orchestra::Snapshot s;
    s.day = forDay;
    s.season = season_;
    s.dials = dials;
    s.moneySupply = moneySupply();
    if (countingFrom_ < 0)
        countingFrom_ = forDay;
    s.counted = int(std::min<std::int64_t>(1000, forDay - countingFrom_));
    // The week's decisions: the evening of the weekly reckoning, once a day has been counted.
    s.decide = reckonedDay_ == forDay - 1 && s.counted >= 1;
    const auto of = [&](const std::string& cell) { return day_.communityOf ? day_.communityOf(cell) : storeFor(cell); };

    // The steers in force tomorrow; those that have ended are forgotten.
    auto& steers = state_.orchestrator.steers;
    steers.erase(std::remove_if(steers.begin(), steers.end(), [&](const orchestra::Steer& st) { return st.until <= forDay; }),
                 steers.end());
    s.steers = steers;

    // Residents (the living: those the world gave a body), and where each lives.
    const auto worths = bankWorths();
    std::unordered_map<std::string, std::string> nextTown;
    std::map<std::string, int> people;
    std::map<std::string, std::vector<const std::string*>> homes;
    std::size_t everyone = 0;
    for (const auto& [id, life] : state_.residents)
    {
        const auto body = bodies.find(id);
        if (body == bodies.end())
            continue;
        ++everyone;
        orchestra::ResidentSnap r;
        r.id = id;
        r.home = life.homeCell;
        r.town = life.homeCell.empty() ? communityOfResident(id) : of(life.homeCell);
        r.age = body->second.age;
        r.hunger = life.hunger;
        if (const auto* a = account(id))
            r.cash = a->cash;
        r.cash += savingsWorth(id, worths);          // (What its savings are worth is its money: money that stops.)
        const auto* job = jobOf(id);
        // (Not looking for work: a child, the retired, one keeping the house, or one in an unpaid post: a lord, a student.)
        r.dependent = r.age < 16 || r.age >= RetireAge || (job && (homemaking(job->title) || !job->paid));
        if (const auto e = earned_.find(id); e != earned_.end())
            for (int i = 0; i < 7; ++i)
            {
                const auto ago = forDay - 1 - e->second.day[i];
                if (e->second.day[i] >= 0 && ago >= 0 && ago < 7)
                {
                    r.earned7 += e->second.coins[i];
                    if (ago < 3)
                        r.earned3 += e->second.coins[i];
                }
            }
        nextTown[id] = r.town;
        if (!r.town.empty())
            ++people[r.town];
        if (!life.homeCell.empty())
            homes[life.homeCell].push_back(&id);
        s.residents.push_back(std::move(r));
    }
    // Households: what they hold, in purse and food; and the poorer half of them, by purse a head (Phase 7's reach).
    std::vector<std::pair<double, const std::vector<const std::string*>*>> byHead;
    const auto nourishIn = [&](const EconomyAccount& a, const std::string& holder) {
        std::int64_t n = 0;
        for (const auto& [item, q] : a.stock)
            if (q > 0 && nourishment(item) > 0 && (holder.empty() || !forSale(holder, item)))
                n += std::int64_t(q) * nourishment(item);
        return n;
    };
    for (const auto& [home, members] : homes)
    {
        orchestra::HomeSnap h;
        h.home = home;
        h.town = of(home);
        h.members = int(members.size());
        if (const auto* larder = account(homeStore(home, "larder")))
            h.nourishment += nourishIn(*larder, {});
        for (const auto* id : members)
            if (const auto* a = account(*id))
            {
                const bool till = merchant(*id) && tillOf(*id) == *id;
                if (!till)
                    h.cash += a->cash + savingsWorth(*id, worths);
                h.nourishment += nourishIn(*a, *id);
            }
        byHead.push_back({double(std::max<std::int64_t>(0, h.cash)) / std::max(1, h.members), &members});
        s.homes.push_back(std::move(h));
    }
    std::sort(byHead.begin(), byHead.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    poorHalf_.clear();
    for (const auto& [perHead, members] : byHead)
    {
        if (poorHalf_.size() * 2 >= everyone)
            break;
        for (const auto* id : *members)
            poorHalf_.insert(*id);
    }

    // The banks (money that stops): each one's coins and its savers' savings.
    for (auto it = state_.accounts.lower_bound("bank:"); it != state_.accounts.end() && it->first.rfind("bank:", 0) == 0; ++it)
        s.banks[it->first.substr(5)].first = it->second.cash;
    for (const auto& [id, saved] : state_.orchestrator.deposits)
        s.banks[saved.first].second += saved.second;

    // The holders, and each one's town.
    std::unordered_map<std::string, std::int64_t> spentNext;
    std::map<std::string, std::string> houseTown;
    for (const auto& house : houses())
        houseTown[house.id] = house.community;
    const auto add = [&](const std::string& id, HolderKind kind, const std::string& town, std::int64_t floor, std::int64_t cash) {
        orchestra::HolderSnap h;
        h.id = id;
        h.kind = kind;
        h.town = town;
        h.cash = cash;
        h.floor = floor;
        if (const auto spent = holderSpent_.find(id); spent != holderSpent_.end())
            h.spent = spent->second;
        spentNext[id] = 0;
        s.holders.push_back(std::move(h));
    };
    const auto head = [&](const std::string& town, double each) { return std::int64_t(std::ceil(each * people[town])); };
    for (const auto& [id, a] : state_.accounts)
    {
        if (id == "treasury")
            add(id, HolderKind::Capital, capital_, head(capital_, dials.floorTownHead), a.cash);
        else if (id.rfind("stores:", 0) == 0)
            add(id, HolderKind::Treasury, id.substr(7), head(id.substr(7), dials.floorTownHead), a.cash);
        else if (id == SharedChurch)
            add(id, HolderKind::Church, {}, std::int64_t(std::ceil(dials.floorChurchHead * double(everyone))), a.cash);
        else if (id.rfind("town:", 0) == 0)
        {
            const auto colon = id.find(':', 5);
            if (colon != std::string::npos && id.compare(colon, std::string::npos, ":church") != 0 &&
                id.compare(colon, std::string::npos, ":granary") != 0)
                add(id, HolderKind::Buyer, id.substr(5, colon - 5), std::int64_t(dials.floorBuyer), a.cash);
        }
        else if (id.rfind("bank:", 0) == 0)
        {
            // A bank: its floor is its reserve: four weeks of what its savers draw, twice over, and a tenth of what they
            // have in at least (money that stops; the playbook: a third kept a pool of its own, the savers drawing little).
            const auto town = id.substr(5);
            const auto bank = s.banks.find(town);
            const double drawn = bankDrawn_.count(town) ? bankDrawn_.at(town) : 0;
            const auto reserve = std::max<std::int64_t>(bank == s.banks.end() ? 0 : bank->second.second / 10, std::int64_t(std::ceil(drawn * 28 * 2)));
            add(id, HolderKind::Bank, town, reserve, a.cash);
        }
        else if (id.rfind("house:", 0) == 0)
            add(id, HolderKind::House, houseTown[id], std::max<std::int64_t>(std::int64_t(dials.floorHouse), houseFloor(id)), a.cash);
        else if (id.rfind("till:", 0) == 0)
        {
            // A business's till: a shop's, or a farm's or a site's (doc 46, Phase 2), a house's or its own.
            const auto positionId = id.substr(5);
            const auto* p = position(positionId);
            const bool farm = p && p->role != "merchant" && items::producerFor(p->title);
            add(id, farm ? HolderKind::Producer : HolderKind::Till, p ? (farm ? communityOfResident(p->founder) : of(p->work.cell)) : std::string(),
                std::max<std::int64_t>(std::int64_t(farm ? dials.floorProducer : dials.floorTill), floatOf(positionId)), a.cash);
        }
    }
    // Owner-run shops and producers without a till of their own yet (an older save before its first day's pass with
    // tills): their keepers' and workers' own purses, so their floor is their own comfortable line too.
    for (const auto& r : s.residents)
    {
        const auto* job = jobOf(r.id);
        const auto* sp = spec(r.id);
        if (tillOf(r.id) != r.id)
            continue;
        if (job && merchant(r.id))
            add(r.id, HolderKind::Keeper, of(job->work.cell),
                std::max({std::int64_t(dials.floorKeeper), floatOf(job->id), wealthLine(r.id)}), r.cash);
        else if (sp && items::producerFor(sp->workLabel))
            add(r.id, HolderKind::Producer, r.town, std::max(std::int64_t(dials.floorProducer), wealthLine(r.id)), r.cash);
    }

    // The shops: today's takings against their running, and what each has of each good, sells and asks.
    std::map<std::pair<std::string, std::string>, orchestra::GoodSnap> goods;
    std::map<std::pair<std::string, std::string>, int> priced;
    for (const auto& r : authored_.residents)
    {
        if (r.role != "merchant" || !state_.residents.count(r.id) || !bodies.count(r.id))
            continue;
        const auto* job = jobOf(r.id);
        const auto till = tillOf(r.id);
        const auto* a = account(till);
        if (!job || !a)
            continue;
        const auto town = of(job->work.cell);
        orchestra::ShopSnap shop;
        shop.till = till;
        shop.town = town;
        if (const auto t = takings_.find(till); t != takings_.end())
            shop.takings = t->second;
        shop.running = floatOf(job->id) / FloatDays;
        s.shops.push_back(shop);
        // What it sells: its wares, what it supplies to the makers, and any food it has in.
        auto sold = wares(r.id);
        const auto* business = items::businessFor(r.workLabel);
        const auto supplies = business ? items::suppliesFor(business->id) : std::vector<std::string>{};
        for (const auto& item : supplies)
            if (std::find(sold.begin(), sold.end(), item) == sold.end())
                sold.push_back(item);
        for (const auto& [item, q] : a->stock)
            if (q > 0 && nourishment(item) > 0 && std::find(sold.begin(), sold.end(), item) == sold.end())
                sold.push_back(item);
        const auto rates = sellRate_.find(till);
        for (const auto& item : sold)
        {
            const auto base = items::baseOf(item);
            const auto* good = storePriced(base) ? nullptr : items::good(base);
            if (!good || good->price <= 0)
                continue;
            auto& g = goods[{town, base}];
            g.town = town;
            g.item = base;
            g.catalog = good->price;
            g.nourish = nourishment(base);
            g.staple = (g.nourish > 0 && good->price <= 3) || base == "firewood";
            g.stock += stockAll(*a, base);
            // What it means to keep of it (Society::craft): a store of what it supplies or what is traded, else a few.
            const bool store = items::traded(base) || std::find(supplies.begin(), supplies.end(), base) != supplies.end();
            g.kept += store ? SuppliesKept / 2 : GoodsKept;
            if (rates != sellRate_.end())
                if (const auto rate = rates->second.find(base); rate != rates->second.end())
                    g.rate += rate->second;
            g.price += double(shopPrice(r.id, base));
            ++priced[{town, base}];
        }
    }
    for (auto& [key, g] : goods)
    {
        g.price /= std::max(1, priced[key]);
        s.goods.push_back(g);
    }

    // The towns: coins in and out since the last snapshot, and the odd jobs and hires posted.
    std::map<std::string, orchestra::TownSnap> towns;
    for (const auto& [town, n] : people)
        towns[town].id = town;
    for (const auto& [town, flow] : townFlow_)
        if (towns.count(town))
            towns[town].inflow = flow.first, towns[town].outflow = flow.second;
    for (const auto& job : oddJobs_)
        if (const auto t = towns.find(job.community); t != towns.end())
        {
            ++t->second.oddJobs;
            t->second.oddJobSlots += job.slots;
            // Places nobody took by the evening: a business's hire, or a hand's share of the day's odd job.
            if (const int open = job.slots - int(job.stage.size()); open > 0)
                t->second.unfilled[job.until >= 0 ? "hand" : "odd job"] += open;
        }
    // Its workers owed wages, and those paid by wage support today (Phase 6); what its failing businesses lack of their
    // floats; and what its funds hold.
    for (const auto& [who, since] : state_.memory.unpaidSince)
        if (const auto home = nextTown.find(who); home != nextTown.end())
            if (const auto t = towns.find(home->second); t != towns.end())
                ++t->second.unpaid;
    for (const auto& [town, spells] : supportedToday_)
        if (const auto t = towns.find(town); t != towns.end())
            t->second.supported += (spells + PaidSpells - 1) / PaidSpells;
    supportedToday_.clear();
    for (const auto& p : positions_)
        if (const auto till = account("till:" + p.id); till && (p.role == "merchant" || items::producerFor(p.title)))
            if (const auto floatCash = floatOf(p.id); till->cash < floatCash / 2)
                if (const auto t = towns.find(p.role == "merchant" ? of(p.work.cell) : communityOfResident(p.founder)); t != towns.end())
                    t->second.rescueNeed += floatCash - till->cash;
    for (auto& [town, t] : towns)
        if (const auto* granary = account("town:" + town + ":granary"))
            for (const auto& [item, n] : granary->stock)
                t.granary += std::int64_t(n) * nourishment(item);
    // A decision's day: each channel's week (Phase 7), what it paid and what reached the poorer half, directly or through
    // the tills it paid (in the share of each till's outgoings that went to them). Then the week's count starts afresh.
    if (s.decide)
    {
        for (const auto& [channel, week] : channelWeek_)
            s.reach[channel] = week;
        for (const auto& [till, channels] : tillFrom_)
            if (const auto paid = tillWeek_.find(till); paid != tillWeek_.end() && paid->second.second > 0)
                for (const auto& [channel, coins] : channels)
                    s.reach[channel].first += coins * paid->second.first / paid->second.second;
        channelWeek_.clear();
        tillFrom_.clear();
        tillWeek_.clear();
    }
    for (auto it = state_.accounts.lower_bound("fund:"); it != state_.accounts.end() && it->first.rfind("fund:", 0) == 0; ++it)
        if (const auto second = it->first.find(':', 5); second != std::string::npos)
            if (const auto t = towns.find(it->first.substr(5, second - 5)); t != towns.end())
            {
                auto channel = it->first.substr(second + 1);
                std::replace(channel.begin(), channel.end(), '_', ' ');
                t->second.funds[channel] = it->second.cash;
            }
    // Posts standing empty, by their kind of pay (the wage table, doc 46, Phase 4).
    for (const auto& p : positions_)
        if (const auto held = state_.careers.positions.find(p.id); held != state_.careers.positions.end() && held->second.holder.empty() && p.paid)
            if (const auto t = towns.find(communityOfResident(p.founder)); t != towns.end())
                ++t->second.unfilled[p.role == "guard" ? "guard" : p.role == "merchant" ? "keeper" : clergy(p.title) ? "clergy" : "help"];
    for (auto& [town, t] : towns)
        s.towns.push_back(t);

    // What is counted until the next snapshot: these holders' spending, coins between these towns, these residents'
    // earnings (kept for the week).
    holderSpent_ = std::move(spentNext);
    townFlow_.clear();
    accountTown_ = std::move(nextTown);
    for (const auto& h : s.holders)
        if (!h.town.empty())
            accountTown_[h.id] = h.town;
        else
            accountTown_.erase(h.id);                // (The land church's: of no town.)
    for (const auto& r : s.residents)
        earned_.try_emplace(r.id);
    for (auto it = earned_.begin(); it != earned_.end();)
        it = accountTown_.count(it->first) || state_.residents.count(it->first) ? std::next(it) : earned_.erase(it);
    return s;
}
} // namespace ratw
