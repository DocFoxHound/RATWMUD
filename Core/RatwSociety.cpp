#include "RatwSociety.h"

#include "RatwCalendar.h"
#include "RatwItems.h"
#include <algorithm>
#include <unordered_map>
#include <cctype>
#include <cmath>
#include <limits>
#include <cstdlib>
#include <functional>
#include <map>

namespace ratw
{
bool itemValid(const std::string& item)
{
    return item == "herbs" || item == "meal" || item == "sword" || items::good(item);
}

bool playerAccountId(const std::string& id)
{
    if (id.rfind("player-", 0) == 0) return id.size() <= 80; // Existing development saves.
    if (id.rfind("wolf-", 0) != 0 || id.size() != 37) return false;
    return std::all_of(id.begin() + 5, id.end(), [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}

namespace
{
// The bronze sword's price: the catalog's (doc 47; it was 40 here, more than an iron one).
double swordPrice()
{
    const auto* g = items::good("sword");
    return g && g->price > 0 ? g->price : 30;
}

constexpr std::int64_t MoneyLimit = 1000000000;
constexpr int StockLimit = 10000;
bool near(const LifeBody& body, const std::string& cell, double x, double y)
{
    return body.cell == cell && std::hypot(body.x - x, body.y - y) <= 1.2;
}
bool validNumber(double value, double low, double high)
{
    return std::isfinite(value) && value >= low && value <= high;
}
void defaultHome(const std::string& id, ResidentLife& life)
{
    if (!life.homeCell.empty()) return;
    life.homeCell = id == "npc_scribe" ? "loft" : "tavern";
    life.homeX = id == "npc_scribe" ? 8.5 : id == "npc_cook" ? 25.5 : id == "npc_keeper" ? 9.5 : 19.5;
    life.homeY = id == "npc_scribe" ? 6.5 : id == "npc_porter" ? 19.5 : id == "npc_scout" ? 15.5 : 12.5;
}
} // namespace
Society::Society(bool demo)
{
    reset(demo);
}
Society::Society(Roster roster)
{
    reset(roster);
}
void Society::reset(bool demo)
{
    reset(demo ? Roster::Demo : Roster::None);
}
void Society::reset(Roster roster)
{
    roster_ = roster;
    state_ = {};
    farmsLooked_ = false;
    status_.clear();                                // (Doc 79, Phase 2: nothing staged yet, and no real money.)
    stagedHomes_.clear();
    stagedHour_ = stagedDay_ = -1;
    rippled_.clear();
    rippledDay_ = -1;
    postedAtPass_.clear();
    provisions_.clear();
    runnerWaiting_.clear();                         // (Doc 79, Phase 6: no buy waits, no runner walks.)
    foodDeferred_.clear();
    fedOn_.clear();
    buysDeferred_.clear();
    realOpening_ = faucetTotal_ = sinkTotal_ = unregisteredMade_ = 0;
    tills_.clear();
    friendGroups_.clear();
    groupsDay_ = -1;
    ++rosterRevision_;
    state_.enabled = roster != Roster::None;
    state_.accounts["treasury"] = {1000, {{"herbs", 100}, {"meal", 50}}};
    if (roster == Roster::Authored)
        return resetAuthored();
    if (roster != Roster::Demo)
        return;
    const std::pair<const char*, const char*> jobs[] = {{"npc_keeper", "merchant"}, {"npc_cook", "cook"},
                                                        {"npc_porter", "forager"},  {"npc_scout", "resident"},
                                                        {"npc_smith", "resident"},  {"npc_scribe", "resident"}};
    for (const auto& job : jobs)
    {
        state_.accounts[job.first] = {80, {{"meal", 2}}};
        ResidentLife life;
        life.role = job.second;
        defaultHome(job.first, life);
        state_.residents[job.first] = life;
    }
    state_.accounts["npc_keeper"].stock = {{"herbs", 8}, {"meal", 12}};
    state_.accounts["npc_cook"].stock = {{"herbs", 12}, {"meal", 4}};
    state_.accounts["npc_porter"].stock = {{"herbs", 10}, {"meal", 2}};
    state_.accounts["npc_smith"].stock["sword"] = SmithSwords;    // Ash keeps a few dull bronze blades for sale.
    record("initial funding", "outside", "settlement", "", 0, 1000 + 80 * std::int64_t(std::size(jobs)));
}
bool Society::cook(const std::string& who)
{
    // Two herb bundles become one prepared meal (as the cook makes them): nothing made from nothing.
    const auto found = state_.accounts.find(who);
    if (found == state_.accounts.end() || stock(found->second, "herbs") < 2 || stock(found->second, "meal") >= StockLimit)
        return false;
    found->second.stock["herbs"] -= 2;
    ++found->second.stock["meal"];
    record("cook", who, who, "meal", 1, 0);
    return true;
}

int Society::stock(const EconomyAccount& account, const std::string& item)
{
    const auto it = account.stock.find(item);
    return it == account.stock.end() ? 0 : it->second;
}
int Society::stockAll(const EconomyAccount& account, const std::string& base)
{
    int n = 0;
    for (const auto& kind : kindsHeld(account, base))
        n += stock(account, kind);
    return n;
}
std::vector<std::string> Society::kindsHeld(const EconomyAccount& account, const std::string& base)
{
    // Every kind of it in the purse: each quality, and masterworks under their makers' marks; plainest first.
    const auto b = items::baseOf(base);
    std::vector<std::string> out;
    for (const auto& [item, n] : account.stock)
        if (n > 0 && items::baseOf(item) == b)
            out.push_back(item);
    const auto rank = [](const std::string& id) { const int q = items::qualityOf(id); return q == 1 ? 0 : q == 0 ? 1 : q; };
    std::stable_sort(out.begin(), out.end(), [&](const std::string& x, const std::string& y) { return rank(x) < rank(y); });
    return out;
}
const EconomyAccount* Society::account(const std::string& id) const
{
    const auto it = state_.accounts.find(id);
    return it == state_.accounts.end() ? nullptr : &it->second;
}
const ResidentLife* Society::resident(const std::string& id) const
{
    const auto it = state_.residents.find(id);
    return it == state_.residents.end() ? nullptr : &it->second;
}
bool Society::moveHome(const std::string& npc, const std::string& cell, double x, double y)
{
    auto found = state_.residents.find(npc);
    if (found == state_.residents.end() || !found->second.relocationCell.empty() || cell.empty() || cell.size() > 80 ||
        !validNumber(x, 0, 256) || !validNumber(y, 0, 256))
        return false;
    auto& life = found->second;
    life.relocationCell = cell;
    life.relocationX = x;
    life.relocationY = y;
    life.progress = 0;
    return true;
}
bool Society::relocate(const std::string& npc, const std::string& cell, double x, double y)
{
    auto found = state_.residents.find(npc);
    if (found == state_.residents.end() || (found->second.role != "resident" && found->second.role != "civilian") ||
        !found->second.relocationCell.empty() || cell.empty() || cell.size() > 80 ||
        !validNumber(x, 0, 256) || !validNumber(y, 0, 256)) return false;
    auto& life = found->second;
    life.relocationCell = cell; life.relocationX = x; life.relocationY = y;
    life.progress = 0; life.task = "relocate";
    return true;
}
EconomyResult Society::operatorTransfer(const std::string& from, const std::string& to,
                                      const std::string& item, int quantity, std::int64_t coins)
{
    const auto a = state_.accounts.find(from), b = state_.accounts.find(to);
    if (a == state_.accounts.end() || b == state_.accounts.end() || from == to ||
        coins < 0 || coins > 1000000 || quantity < 0 || quantity > 99 ||
        (item.empty() ? quantity != 0 : !itemValid(item) || quantity == 0) ||
        (coins == 0 && quantity == 0)) return {false, "Invalid finite operator transfer."};
    if (a->second.cash < coins || b->second.cash > MoneyLimit - coins ||
        (!item.empty() && (stock(a->second, item) < quantity || stock(b->second, item) > StockLimit - quantity)))
        return {false, "Transfer exceeds real funds, goods or recipient capacity."};
    a->second.cash -= coins; b->second.cash += coins;
    if (!item.empty()) { a->second.stock[item] -= quantity; b->second.stock[item] += quantity; }
    record("operator transfer", from, to, item, quantity, coins);
    return {true, "Existing money and goods transferred; no currency or stock created."};
}
void Society::configure(const AuthoredRoster& roster)
{
    authored_ = roster;
    forgetSpecs();
    buildPositions();
    buildRules();
    buildFarms();                                   // (The farms and works as places of work: doc 71.)
    reset(Roster::Authored);
}
const ResidentSpec* Society::unplaced(const std::string& id) const
{
    if (roster_ != Roster::Authored)
        return nullptr;
    const auto found = std::find_if(authored_.unplaced.begin(), authored_.unplaced.end(),
                                    [&](const ResidentSpec& r) { return r.id == id; });
    return found == authored_.unplaced.end() ? nullptr : &*found;
}
const ResidentSpec* Society::spec(const std::string& id) const
{
    if (roster_ != Roster::Authored)
        return nullptr;
    if (!specsIndexed_ || specIndex_.size() > authored_.residents.size())
    {
        ++rosterRevision_;
        specIndex_.clear();
        for (std::size_t i = 0; i < authored_.residents.size(); ++i)
            specIndex_.emplace(authored_.residents[i].id, i);   // The first of a duplicated ID wins, as before.
        specsIndexed_ = true;
    }
    const auto found = specIndex_.find(id);
    if (found == specIndex_.end() || found->second >= authored_.residents.size())
        return nullptr;
    return &authored_.residents[found->second];
}
bool Society::merchant(const std::string& id) const
{
    if (roster_ == Roster::Demo)
        return id == "npc_keeper" || id == "npc_smith";
    const auto* r = spec(id);
    return r && r->role == "merchant";
}
bool Society::smith(const std::string& id) const
{
    if (roster_ == Roster::Demo)
        return id == "npc_smith";
    const auto* r = spec(id);
    if (!r || r->role != "merchant")
        return false;
    const auto& trade = r->workLabel;
    thread_local std::unordered_map<std::string, bool> known;   // By work label: asked of every shop, every decision.
    if (const auto found = known.find(trade); found != known.end())
        return found->second;
    std::string work = trade;
    std::transform(work.begin(), work.end(), work.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return known.emplace(trade, work.find("smith") != std::string::npos || work.find("forge") != std::string::npos).first->second;
}
bool Society::foodShop(const std::string& merchant) const
{
    static const std::vector<std::string> food = {"general", "provisioner", "stall", "bakery", "butcher", "fishmonger", "brewery", "inn"};
    const auto* r = roster_ == Roster::Demo ? nullptr : spec(merchant);
    const auto* business = r ? items::businessFor(r->workLabel) : nullptr;
    return business && std::find(food.begin(), food.end(), business->id) != food.end();
}

std::vector<std::string> Society::wares(const std::string& merchant) const
{
    const auto* r = roster_ == Roster::Demo ? nullptr : spec(merchant);
    if (!r)
        return smith(merchant) ? std::vector<std::string>{"sword"} : std::vector<std::string>{"herbs", "meal"};
    const auto& key = r->workLabel;                // (Kept by its label.)
    if (const auto kept = waresCache_.find(merchant); kept != waresCache_.end() && kept->second.first == key)
        return kept->second.second;
    std::vector<std::string> out;
    const auto* business = items::businessFor(r->workLabel);
    const auto crafts = business ? items::craftsFor(business->id) : std::vector<const items::Craft*>{};
    const auto& supplies = business ? items::suppliesFor(business->id) : std::vector<std::string>{};
    if (!crafts.empty() || !supplies.empty())
    {
        // A maker sells what it makes (Data/Items/crafts.json), a supplier the ingredients it sells to the makers.
        for (const auto* craft : crafts)
            for (const auto& made : craft->out)
                if (std::find(out.begin(), out.end(), made.first) == out.end())
                    out.push_back(made.first);
        for (const auto& item : supplies)
            if (std::find(out.begin(), out.end(), item) == out.end())
                out.push_back(item);
        out.erase(std::remove_if(out.begin(), out.end(), [](const std::string& id) { return id == "meal" || id == "herbs"; }), out.end());
    }
    else if (business)
    {
        // A handful of the kind's cheap goods (or its cheapest few, where little of it is cheap), chosen by the shop.
        auto cheap = items::goodsSold(*business, CheapPrice);
        if (cheap.size() < 3)
        {
            auto all = items::goodsSold(*business, 1000000);
            std::stable_sort(all.begin(), all.end(), [](const std::string& a, const std::string& b) {
                return items::good(a)->price < items::good(b)->price;
            });
            all.resize(std::min<std::size_t>(all.size(), 3));
            for (const auto& id : all)
                if (std::find(cheap.begin(), cheap.end(), id) == cheap.end())
                    cheap.push_back(id);
        }
        std::uint64_t h = 1469598103934665603ULL;
        for (unsigned char ch : merchant)
            h = (h ^ ch) * 1099511628211ULL;
        const std::size_t want = std::min<std::size_t>(cheap.size(), 3 + h % 4);
        std::vector<std::pair<std::uint64_t, std::string>> ranked;
        for (const auto& id : cheap)
        {
            std::uint64_t k = h;
            for (unsigned char ch : id)
                k = (k ^ ch) * 1099511628211ULL;
            ranked.push_back({k, id});
        }
        std::sort(ranked.begin(), ranked.end());
        for (std::size_t i = 0; i < want; ++i)
            out.push_back(ranked[i].second);
        // In catalog order, as a shop's shelves read.
        std::sort(out.begin(), out.end(), [&](const std::string& a, const std::string& b) {
            return std::find(cheap.begin(), cheap.end(), a) < std::find(cheap.begin(), cheap.end(), b);
        });
        out.erase(std::remove_if(out.begin(), out.end(), [](const std::string& id) { return id == "meal" || id == "herbs"; }), out.end());
    }
    // What it buys from players (gathered and hunted goods, doc 41), to sell on.
    if (business)
        for (const auto& item : items::buysFor(business->id))
            if (item != "meal" && item != "herbs" && std::find(out.begin(), out.end(), item) == out.end())
                out.push_back(item);
    // The staged economy's own buys (market.json `buys`; doc 79, D5: the smithy, the ironworks and the foundry buy ore).
    if (business)
        if (const auto extra = market::data().buys.find(business->id); extra != market::data().buys.end())
            for (const auto& item : extra->second)
                if (std::find(out.begin(), out.end(), item) == out.end())
                    out.push_back(item);
    static const char* Food[] = {"general", "provisioner", "stall", "bakery", "butcher", "fishmonger", "brewery", "inn"};   // (foodShop)
    const bool food = business && std::find(std::begin(Food), std::end(Food), business->id) != std::end(Food);
    if (smith(merchant) && std::find(out.begin(), out.end(), "sword") == out.end())
        out.insert(out.begin(), "sword");
    if (!business || food)
        out.push_back("meal");
    if (!business || business->id == "herbalist" || business->id == "apothecary" || business->id == "general" || business->id == "stall" ||
        business->id == "inn")
        out.push_back("herbs");
    waresCache_[merchant] = {key, out};
    return out;
}
const char* Society::itemName(const std::string& id)
{
    if (const auto* worn = items::wearable(id))
        return worn->name.c_str();
    if (id != "herbs" && id != "meal" && id != "sword")
        if (const auto* good = items::good(id))
            return good->name.c_str();
    return id == "herbs" ? "Cooking herbs" : id == "meal" ? "Prepared meal" : id == "sword" ? "Dull bronze sword" : "Unknown goods";
}
void Society::record(const std::string& kind, const std::string& from, const std::string& to, const std::string& item,
                     int quantity, std::int64_t coins)
{
    stagedDoor(kind, from, to, item, quantity, coins);   // (The faucet door: doc 79, 9.3.)
    state_.ledger.push_back({state_.nextEntry++, state_.budgetDay, coins, kind, from, to, item, quantity});
    if (state_.ledger.size() > 128)
        state_.ledger.erase(state_.ledger.begin());
    journal_.push_back(state_.ledger.back());
    if (journal_.size() > JournalKept + JournalKept / 4)
        journal_.erase(journal_.begin(), journal_.begin() + std::ptrdiff_t(journal_.size() - JournalKept));
}
std::vector<EconomyEntry> Society::takeJournal()
{
    std::vector<EconomyEntry> out;
    out.swap(journal_);
    return out;
}
void Society::addPlayer(const std::string& id)
{
    if (!playerAccountId(id) || state_.accounts.count(id) || state_.accounts.size() >= MaxAccounts)
        return;
    {
        // The welcome as a faucet (doc 79, 9.2): made, not taken from the treasury, so a dry treasury never starts a
        // player with nothing; its coins and herbs once per account holder per world per game week (D23: setHolder), a
        // later wolf on the same account that week starting with its meals only.
        state_.accounts[id] = EconomyAccount{};
        const auto& t = statusTable();
        const auto holder = holderOf(id);
        const auto week = state_.budgetDay / std::max<std::int64_t>(1, t.cap("welcomeDays"));
        const auto last = state_.staged.welcomed.find(holder);
        if (last == state_.staged.welcomed.end() || last->second != week)
        {
            const auto coins = std::clamp<std::int64_t>(t.caps.count("welcomeCoins") ? t.cap("welcomeCoins") : PlayerWelcome, 0, 1000);
            state_.accounts.at(id).cash = coins;
            state_.staged.welcomed[holder] = week;
            stagedChanged_.insert({state_.budgetDay, holder});   // (Journalled with the welcome: D23 across a crash.)
            if (coins > 0)
                record("welcome", "made", id, "", 0, coins);
            if (const auto herbs = int(std::clamp<std::int64_t>(t.caps.count("welcomeHerbs") ? t.cap("welcomeHerbs") : 2, 0, 20)); herbs > 0)
                create(id, "herbs", herbs, "welcome");
        }
        if (const auto meals = int(std::clamp<std::int64_t>(t.caps.count("welcomeMeals") ? t.cap("welcomeMeals") : StartingMeals, 0, 20)); meals > 0)
            create(id, "meal", meals, "provisions to start with");
    }
}
bool Society::transfer(const std::string& seller, const std::string& buyer, const std::string& item, int quantity,
                       std::int64_t price, const std::string& kind)
{
    // No cap on what a piece costs (the user, 2026-10-30): only a price whose total would overflow is refused.
    if (seller == buyer || quantity < 1 || quantity > 99 || !itemValid(item) || price < 1 ||
        price > std::numeric_limits<std::int64_t>::max() / quantity)
        return false;
    const auto* from = account(seller);
    const auto* to = account(buyer);
    const auto total = price * quantity;
    if (!from || !to || stock(*from, item) < quantity || to->cash < total || from->cash > MoneyLimit - total ||
        stock(*to, item) > StockLimit - quantity || (!to->stock.count(item) && to->stock.size() >= MaxGoodsKinds))
        return false;
    auto& s = state_.accounts.at(seller);
    auto& b = state_.accounts.at(buyer);
    const int left = s.stock[item] -= quantity;
    if (left == 0 && item != "herbs" && item != "meal" && item != "sword")
        s.stock.erase(item);                                // (Room for other kinds; the old three keep their place.)
    b.stock[item] += quantity;
    b.cash -= total;
    s.cash += total;
    record(kind, buyer, seller, item, quantity, total);
    return true;
}
std::int64_t Society::catalogPrice(const std::string& item)
{
    // Meals, herbs and the sword keep their old prices; anything else of the catalogue its own (its quality's: good()).
    if (item == "meal")
        return 6;
    if (item == "herbs")
        return 2;
    if (item == "sword")
        return std::int64_t(swordPrice());
    const auto* good = items::good(item);
    return good && good->price > 0 ? good->price : 1;
}
EconomyResult Society::quote(const std::string& player, const std::string& seller, const std::string& item,
                             int quantity, bool buy, const std::string& holder) const
{
    return stagedQuote(player, seller, item, quantity, buy, holder, nullptr);   // (The Market Board's: doc 79, 7.)
}
EconomyResult Society::trade(const std::string& player, const std::string& trader, const std::string& item,
                             int quantity, bool buy, const std::string& holder)
{
    return stagedTrade(player, trader, item, quantity, buy, holder);   // (The Market Board's: doc 79, 7.)
}
EconomyResult Society::gather(const std::string& player)
{
    if (!state_.enabled || !playerAccountId(player) || !account(player))
        return {false, "No gathering opportunity here."};
    if (state_.herbPatch < 1)
        return {false, "This patch needs time to recover."};
    auto& a = state_.accounts.at(player);
    if (stock(a, "herbs") >= StockLimit)
        return {false, "You cannot carry more herbs."};
    --state_.herbPatch;
    ++a.stock["herbs"];
    record("gather", "herb patch", player, "herbs", 1, 0);
    return {true, "You gather one bundle of cooking herbs. The patch has finite supplies."};
}
void Society::furnishHomes(const std::set<std::string>& homeCells)
{
    if (!state_.enabled)
        return;
    std::map<std::string, std::vector<std::string>> households;
    for (const auto& [id, life] : state_.residents)
        if (!life.homeCell.empty() && homeCells.count(life.homeCell))
            households[life.homeCell].push_back(id);
    for (const auto& [home, members] : households)
    {
        (void)members;
        for (const char* kind : StoreKinds)
            openAccount(homeStore(home, kind));
    }
}

bool Society::create(const std::string& accountId, const std::string& item, int quantity, const std::string& reason)
{
    const auto found = state_.accounts.find(accountId);
    if (found == state_.accounts.end() || !itemValid(item) || quantity < 1 || stock(found->second, item) > StockLimit - quantity ||
        (!found->second.stock.count(item) && found->second.stock.size() >= MaxGoodsKinds))
        return false;
    found->second.stock[item] += quantity;
    record(reason, "made", accountId, item, quantity, 0);
    return true;
}
EconomyResult Society::eat(const std::string& player)
{
    if (!playerAccountId(player) || !account(player) || stock(*account(player), "meal") < 1)
        return {false, "You have no prepared meal."};
    --state_.accounts.at(player).stock["meal"];
    record("eat", player, "consumed", "meal", 1, 0);
    return {true, "You eat a prepared meal and recover a little stamina."};
}
void Society::tick(double seconds, double absoluteDay, int season, const std::map<std::string, LifeBody>& bodies)
{
    if (!state_.enabled || !validNumber(seconds, 0, 60) || !validNumber(absoluteDay, 0, 365000000) || season < 0 ||
        season > 3)
        return;
    state_.decisionRemainder += seconds;
    while (state_.decisionRemainder + 1e-8 >= 1.)
    {
        state_.decisionRemainder -= 1.;
        state_.decisionRemainder = std::max(0., state_.decisionRemainder);
        decide(absoluteDay, season, bodies);
    }
}
bool Society::needsBodies(double seconds, double absoluteDay, bool anySeen) const
{
    if (!decidesWithin(seconds))
        return false;
    return anySeen || roster_ != Roster::Authored || (secondsDecided_ + 1) % unseenStep_ == 0 ||
           std::int64_t(std::floor(absoluteDay)) > state_.budgetDay;
}

void Society::decide(double absoluteDay, int season, const std::map<std::string, LifeBody>& bodies)
{
    decidingAt_ = absoluteDay;
    unseenDecided_ = ++secondsDecided_ % unseenStep_ == 0;
    const auto day = std::int64_t(std::floor(absoluteDay));
    if (day > state_.budgetDay)
    {
        // Missed days do not accumulate unbounded grants, orders or harvests.
        state_.budgetDay = day;
        for (auto& resident : state_.residents)
            resident.second.wagesToday = 0;
        const int recovery[] = {20, 30, 12, 4};
        state_.herbPatch = std::min(60, state_.herbPatch + recovery[season]);
    }
    if (roster_ == Roster::Authored)
    {
        setYearDay(absoluteDay);                    // (The season's price on the Board: doc 79, 7.1.)
        marketTick(absoluteDay);                    // (The Market Board's dawn: doc 79, 5.)
        // The staged economy (doc 79): food by band from the shelf, the nightly purse and treasury set.
        if (!needsPaused())
            stagedHourly(std::int64_t(std::floor(absoluteDay * 24)));
        else
            stagedHour_ = -1;
        return decideAuthored(absoluteDay, bodies);
    }
    const double hour = (absoluteDay - std::floor(absoluteDay)) * 24.;
    const bool night = hour < 6 || hour >= 22;
    const auto available = [&](const std::string& id) {
        const auto it = bodies.find(id);
        return it != bodies.end() && !it->second.companion && !it->second.cell.empty() &&
               it->second.cell.size() <= 80 && validNumber(it->second.x, 0, 256) && validNumber(it->second.y, 0, 256);
    };
    const auto hungryCustomer = [&]() {
        for (const auto& buyer : state_.residents)
            if (buyer.first != "npc_cook" && buyer.first != "npc_keeper" && available(buyer.first) &&
                buyer.second.hunger >= 55 && stock(*account(buyer.first), "meal") == 0 &&
                account(buyer.first)->cash >= 6)
                return true;
        return false;
    };
    // Exclusive benches/beds are reserved deterministically in stable actor-ID order.
    std::map<std::string, std::string> reservations;
    for (auto& pair : state_.residents)
    {
        const auto bodyIt = bodies.find(pair.first);
        if (bodyIt == bodies.end())
            continue;
        const auto& body = bodyIt->second;
        if (body.cell.empty() || body.cell.size() > 80 || !validNumber(body.x, 0, 256) || !validNumber(body.y, 0, 256))
            continue;
        auto& life = pair.second;
        auto& wallet = state_.accounts.at(pair.first);
        life.hunger = std::min(100., life.hunger + .0035);
        life.fatigue = std::min(100., life.fatigue + .0025);
        if (body.companion)
        {
            life.task = "companion";                   // (Its work and its walking are the game's.)
            life.goalCell.clear();
            life.reason = "Ordinary work is suspended while recruited.";
            life.progress = 0;
            continue;
        }
        std::string task, goal = "tavern", reason;
        double x = 14.5, y = 13.5;
        if (!life.relocationCell.empty())
        {
            task = "relocate"; goal = life.relocationCell; x = life.relocationX; y = life.relocationY;
            reason = "Travelling to an operator-approved new home; arrival is not instantaneous.";
            if (body.cell == goal && std::hypot(body.x - x, body.y - y) <= .35)
            {
                life.homeCell = goal; life.homeX = x; life.homeY = y;
                life.relocationCell.clear(); life.relocationX = life.relocationY = 0;
                reason = "Arrived at the new home. Existing work and food routes remain in use.";
            }
        }
        else if (life.hunger >= 60 && stock(wallet, "meal") > 0)
        {
            task = "eat";
            goal = body.cell;
            // A gentle crowd bump must not restart an eight-second meal every
            // decision. Anchor on entry; a real interruption still resets it.
            const bool continuing = life.task == "eat" && life.goalCell == body.cell;
            x = continuing ? life.goalX : body.x;
            y = continuing ? life.goalY : body.y;
            reason = "Hungry; a carried meal is available.";
        }
        else if (life.fatigue >= 80 || (life.task == "sleep" && life.fatigue > 15) || (night && life.hunger < 80))
        {
            task = "sleep";
            reason = "Resting on the shared daily schedule.";
            goal = life.homeCell; x = life.homeX; y = life.homeY;
        }
        else if (life.hunger >= 55 && stock(wallet, "meal") == 0 && pair.first != "npc_keeper" && wallet.cash >= 6 &&
                 available("npc_keeper") &&
                 (stock(*account("npc_keeper"), "meal") > 0 ||
                  (pair.first != "npc_cook" && available("npc_cook") && stock(*account("npc_cook"), "meal") > 0)))
        {
            task = "buy food";
            x = 10.5;
            y = 6.5;
            reason = "Seeking available food from tavern stock or the cook's delivery.";
        }
        else if (life.role == "forager")
        {
            if (stock(wallet, "herbs") >= 6 || (stock(wallet, "herbs") > 0 && state_.herbPatch == 0) ||
                (stock(wallet, "herbs") >= 2 && stock(*account("npc_cook"), "herbs") < 2))
            {
                task = "deliver herbs";
                x = 25.5;
                y = 6.5;
                reason = "Delivering finite harvested ingredients to the cook.";
            }
            else
            {
                task = "gather";
                goal = "exterior";
                x = 17.5;
                y = 7.5;
                reason = "Collecting ingredients from the regenerating patch.";
            }
        }
        else if (life.role == "cook")
        {
            if (stock(wallet, "meal") > 0 &&
                (stock(wallet, "meal") >= 6 || stock(*account("npc_keeper"), "meal") < 2 || wallet.cash < 2) &&
                stock(*account("npc_keeper"), "meal") < 20 &&
                (account("npc_keeper")->cash >= 5 || hungryCustomer()))
            {
                task = "deliver meals";
                x = 10.5;
                y = 6.5;
                reason = "Bringing meals to funded tavern orders or hungry customers.";
            }
            else if (stock(wallet, "herbs") < 2 && available("npc_porter") && wallet.cash >= 1 &&
                     (stock(*account("npc_porter"), "herbs") > 0 || state_.herbPatch > 0))
            {
                task = "receive herbs";
                x = 25.5;
                y = 6.5;
                reason = "Meeting the forager at the kitchen for affordable ingredients.";
            }
            else if (stock(wallet, "herbs") < 2)
            {
                task = "buy ingredients";
                x = 10.5;
                y = 6.5;
                reason = "Buying ingredients, including supplies sold by travelers.";
            }
            else
            {
                task = "cook";
                x = 26.5;
                y = 6.5;
                reason = "Two herb bundles become one prepared meal.";
            }
        }
        else if (life.role == "merchant")
        {
            task = "trade";
            x = 9.5;
            y = 6.5;
            reason = "Serving customers.";
        }
        else if (hour >= 8 && hour < 18 && life.wagesToday < 3)
        {
            task = "paid work";
            reason = "Completing a bounded service contract paid from the keeper's real purse.";
            if (pair.first == "npc_scribe")
            {
                goal = "loft";
                x = 8.5;
                y = 6.5;
            }
            else
            {
                goal = "exterior";
                x = pair.first == "npc_smith" ? 11.5 : 18.5;
                y = 14.5;
            }
        }
        else
        {
            task = "socialize";
            reason = "Fed and rested; spending time in the common room.";
            x = pair.first == "npc_scout" ? 19.5 : pair.first == "npc_smith" ? 20.5 : 14.5;
            ResidentLife original;
            defaultHome(pair.first, original);
            if (life.homeCell != original.homeCell || life.homeX != original.homeX || life.homeY != original.homeY)
            { goal = life.homeCell; x = life.homeX; y = life.homeY; reason = "Spending free time at the new home."; }
        }
        if (task != life.task || goal != life.goalCell || x != life.goalX || y != life.goalY)
            life.progress = 0;
        life.task = task;
        life.reason = reason;
        life.goalCell = goal;
        life.goalX = x;
        life.goalY = y;
        if (!near(body, goal, x, y))
        {
            life.progress = 0;
            continue;
        }
        const std::string station = goal + ":" + std::to_string(x) + ":" + std::to_string(y);
        const bool exclusive = task == "sleep" || task == "cook" || task == "paid work";
        if (exclusive && reservations.count(station))
        {
            life.reason = "Waiting for an occupied work or rest place.";
            continue;
        }
        if (exclusive)
            reservations[station] = pair.first;
        if (task == "sleep")
        {
            life.fatigue = std::max(0., life.fatigue - .012);
            continue;
        }
        life.progress += 1.;
        const double duration = task == "paid work" ? 1200.
                                : task == "gather"  ? 30.
                                : task == "cook"    ? 45.
                                : task == "eat"     ? 8.
                                                    : 12.;
        if (life.progress < duration)
            continue;
        life.progress = 0;
        if (task == "eat")
        {
            --wallet.stock["meal"];
            life.hunger = std::max(0., life.hunger - 55.);
            record("eat", pair.first, "consumed", "meal", 1, 0);
        }
        else if (task == "gather")
        {
            if (state_.herbPatch > 0 && stock(wallet, "herbs") < StockLimit)
            {
                --state_.herbPatch;
                ++wallet.stock["herbs"];
                record("gather", "herb patch", pair.first, "herbs", 1, 0);
            }
            else
                life.reason = "The depleted patch must recover before more can be gathered.";
        }
        else if (task == "cook")
        {
            if (stock(wallet, "herbs") >= 2 && stock(wallet, "meal") < StockLimit)
            {
                wallet.stock["herbs"] -= 2;
                ++wallet.stock["meal"];
                record("cook", pair.first, pair.first, "meal", 1, 0);
            }
            else
                life.reason = "Waiting for ingredient deliveries; cannot create meals from nothing.";
        }
        else if (task == "deliver herbs")
        {
            const auto target = bodies.find("npc_cook");
            if (available("npc_cook") && target != bodies.end() && near(target->second, body.cell, body.x, body.y) &&
                stock(*account("npc_cook"), "herbs") < 20)
            {
                const auto& buyer = *account("npc_cook");
                const int quantity = std::min({4, stock(wallet, "herbs"), 20 - stock(buyer, "herbs"),
                                               int(std::min<std::int64_t>(4, buyer.cash))});
                if (!transfer(pair.first, "npc_cook", "herbs", quantity, 1, "ingredient delivery"))
                    life.reason = "The cook cannot afford even one available ingredient bundle.";
            }
            else
                life.reason = "Waiting for the cook and real ingredient demand.";
        }
        else if (task == "deliver meals")
        {
            const auto target = bodies.find("npc_keeper");
            if (available("npc_keeper") && target != bodies.end() && near(target->second, body.cell, body.x, body.y) &&
                stock(*account("npc_keeper"), "meal") < 20)
            {
                const auto& buyer = *account("npc_keeper");
                const int quantity = std::min({3, stock(wallet, "meal"), 20 - stock(buyer, "meal"),
                                               int(std::min<std::int64_t>(3, buyer.cash / 5))});
                if (!transfer(pair.first, "npc_keeper", "meal", quantity, 5, "meal delivery"))
                    life.reason = "The keeper cannot afford even one prepared meal.";
            }
            else
                life.reason = "Waiting for the keeper and space on the shelves.";
        }
        else if (task == "buy food")
        {
            const auto target = bodies.find("npc_keeper");
            if (pair.first != "npc_keeper" && target != bodies.end() && near(target->second, body.cell, body.x, body.y))
            {
                if (stock(*account("npc_keeper"), "meal") > 0)
                {
                    if (!transfer("npc_keeper", pair.first, "meal", 1, 6, "resident food purchase"))
                        life.reason = "Cannot buy food: stock or money is unavailable.";
                }
                else
                {
                    // NPC-only consignment bridges the keeper's working-capital
                    // shortage without debt, free inventory, or altered prices.
                    // A buyer pays the same six pennies: five to the cook, one
                    // to the keeper. All three bodies and both capacity checks
                    // are required before the one-meal transaction can commit.
                    const auto cook = bodies.find("npc_cook");
                    auto& producer = state_.accounts.at("npc_cook");
                    auto& keeper = state_.accounts.at("npc_keeper");
                    if (pair.first != "npc_cook" && available("npc_cook") && available("npc_keeper") &&
                        cook != bodies.end() && near(cook->second, body.cell, body.x, body.y) &&
                        near(cook->second, target->second.cell, target->second.x, target->second.y) &&
                        wallet.cash >= 6 && keeper.cash < MoneyLimit && producer.cash <= MoneyLimit - 5 &&
                        stock(wallet, "meal") < StockLimit && stock(producer, "meal") > 0 &&
                        transfer("npc_cook", pair.first, "meal", 1, 5, "consigned meal sale"))
                    {
                        --wallet.cash;
                        ++keeper.cash;
                        record("market commission", pair.first, "npc_keeper", "", 0, 1);
                    }
                    else
                        life.reason =
                            "Waiting for the cook's actual meal, buyer funds, and all three market participants.";
                }
            }
            else
                life.reason = "Waiting for the food seller.";
        }
        else if (task == "buy ingredients")
        {
            const auto target = bodies.find("npc_keeper");
            if (target != bodies.end() && near(target->second, body.cell, body.x, body.y))
            {
                const int quantity = std::min(
                    {4, stock(*account("npc_keeper"), "herbs"), int(std::min<std::int64_t>(4, wallet.cash / 2))});
                if (!transfer("npc_keeper", pair.first, "herbs", quantity, 2, "ingredient purchase"))
                    life.reason = "Ingredient purchase blocked by stock or cash.";
            }
            else
                life.reason = "Waiting for the ingredient merchant.";
        }
        else if (task == "paid work")
        {
            auto& employer = state_.accounts.at("npc_keeper");
            if (employer.cash >= 2 && wallet.cash <= MoneyLimit - 2 && life.wagesToday < 3)
            {
                employer.cash -= 2;
                wallet.cash += 2;
                ++life.wagesToday;
                record("service wages", "npc_keeper", pair.first, "", 0, 2);
            }
            else
                life.reason = "Work completed, but the employer cannot afford another contract.";
        }
        // (A merchant's "trade" is serving at the counter: the outside's export orders and imports, which made and sank
        // money, went with doc 79's Phase 3.)
    }
}
bool Society::handOverQuarantine(const std::string& from, const std::string& to)
{
    // (The goods an escrow set aside follow its goods: a stall's to its keeper, a chest's to its renter. Kept at 256 kinds
    // an account, the save's limit, or refused whole.)
    auto& quarantine = state_.staged.quarantine;
    const auto q = quarantine.find(from);
    if (q == quarantine.end())
        return true;
    if (to.empty() || to == from || !state_.accounts.count(to))
        return false;
    const auto into = quarantine.find(to);
    std::size_t kinds = into == quarantine.end() ? 0 : into->second.size();
    for (const auto& [item, n] : q->second)
        if (into == quarantine.end() || !into->second.count(item))
            ++kinds;
    if (kinds > 256)
        return false;
    auto& goods = quarantine[to];
    for (const auto& [item, n] : q->second)
        goods[item] = std::min(StockLimit, goods[item] + n);
    quarantine.erase(from);
    return true;
}

bool Society::restore(const SocietyState& saved)
{
    // Lenient (Docs/Design/79-a-staged-economy.md, 14): a save is reconciled with the world it is loaded into, never
    // refused over its economy. Dead accounts (9.1) go with their goods; an account of a resident the world no longer has
    // goes if it is staged; a resident whose role changed takes the authored role; departed residents leave and newcomers
    // start fresh at their authored purse; unknown goods are dropped from staged accounts and set aside in real and
    // escrow ones (quarantined: they come back when the good does). Staged accounts out of range are settled after
    // (stagedRestored). Real and escrow accounts are kept to the penny; only a broken one (negative or overflowing money,
    // broken goods limits) or a broken save (its ledger, its counters) refuses.
    ++rosterRevision_;
    // A checkpoint names its own population; World rejects one for another world. A society without one (None) accepts
    // the legacy demo population.
    const Roster roster = !saved.enabled ? roster_ : roster_ == Roster::Authored ? Roster::Authored : Roster::Demo;
    if (roster == Roster::Authored && authored_.residents.empty() && authored_.unplaced.empty())
        return false;
    Society fresh(Roster::None);
    fresh.authored_ = authored_;
    fresh.forgetSpecs();
    fresh.reset(roster);
    SocietyState s = saved;
    if (s.accounts.size() > MaxAccounts || s.residents.size() > MaxResidents || s.ledger.size() > 128 || s.nextEntry < 1 ||
        s.nextEntry > 1000000000000LL || s.budgetDay < 0 || s.budgetDay > 365000000)
        return false;
    s.herbPatch = std::clamp(s.herbPatch, 0, 60);
    if (!validNumber(s.decisionRemainder, 0, 1))
        s.decisionRemainder = 0;
    const auto& now = fresh.state_.residents;
    // Only an authored world reconciles its people (a world rebuilt since, perhaps every resident new: doc 60): the demo's
    // six are fixed, so a save with others is another world's, refused.
    if (s.enabled && roster != Roster::Authored &&
        (s.residents.size() != now.size() ||
         std::any_of(s.residents.begin(), s.residents.end(), [&](const auto& r) { return !now.count(r.first); })))
        return false;
    // The accounts.
    auto& quarantine = s.staged.quarantine;
    for (auto it = s.accounts.begin(); it != s.accounts.end();)
    {
        const auto& id = it->first;
        auto& a = it->second;
        const auto kind = classify(id);
        const bool known = id == "treasury" || playerAccountId(id) || facilityAccount(id) || now.count(id);
        if (id.empty() || id.size() > 80 || kind == AccountKind::Dead || (kind == AccountKind::Staged && !known))
        {
            quarantine.erase(id);
            it = s.accounts.erase(it);              // (Dead, or a resident's who is gone: dropped with its goods.)
            continue;
        }
        if (kind == AccountKind::Staged)
        {
            // Settled: money within range, unknown goods dropped, counts within the limits, at most MaxGoodsKinds kinds.
            a.cash = std::clamp<std::int64_t>(a.cash, 0, MoneyLimit);
            for (auto g = a.stock.begin(); g != a.stock.end();)
                if (!itemValid(g->first) || g->second < 0)
                    g = a.stock.erase(g);
                else
                    (g->second = std::min(g->second, StockLimit)), ++g;
            while (a.stock.size() > MaxGoodsKinds)
                a.stock.erase(std::prev(a.stock.end()));
            quarantine.erase(id);
        }
        else
        {
            // Real and escrow: to the penny. Goods the world no longer knows are set aside, not lost; those it knows again
            // come back.
            if (a.cash < 0 || a.cash > MoneyLimit || a.stock.size() > MaxGoodsKinds)
                return false;
            for (auto g = a.stock.begin(); g != a.stock.end();)
            {
                if (g->second < 0 || g->second > StockLimit)
                    return false;
                if (!itemValid(g->first) && g->second > 0)
                {
                    auto& q = quarantine[id][g->first];
                    q = std::min(StockLimit, q + g->second);
                    g = a.stock.erase(g);
                }
                else
                    ++g;
            }
            if (const auto q = quarantine.find(id); q != quarantine.end())
            {
                for (auto g = q->second.begin(); g != q->second.end();)
                    if (itemValid(g->first) && (a.stock.count(g->first) || a.stock.size() < MaxGoodsKinds))
                    {
                        a.stock[g->first] = std::min(StockLimit, stock(a, g->first) + g->second);
                        g = q->second.erase(g);
                    }
                    else
                        ++g;
                if (q->second.empty())
                    quarantine.erase(q);
            }
        }
        ++it;
    }
    for (auto it = quarantine.begin(); it != quarantine.end();)
        it = s.accounts.count(it->first) ? std::next(it) : quarantine.erase(it);
    if (!s.accounts.count("treasury"))
        s.accounts["treasury"] = fresh.state_.accounts.at("treasury");
    // The residents: the departed leave, newcomers start fresh, a changed role takes the authored one. (A broken life, its
    // needs or places not numbers in range, is a broken save, not the economy's: refused.)
    if (!s.enabled)
        s.residents.clear();
    for (auto it = s.residents.begin(); it != s.residents.end();)
        it = now.count(it->first) ? std::next(it) : s.residents.erase(it);
    for (const auto& [id, life] : now)
    {
        const auto was = s.residents.find(id);
        if (s.enabled && (was == s.residents.end() || was->second.role != life.role))
            s.residents[id] = life;
        if (s.enabled && !s.accounts.count(id))
            s.accounts[id] = fresh.state_.accounts.at(id);   // (A newcomer: its authored purse, settled to its band after.)
    }
    for (const auto& [id, l] : s.residents)
        if (!validNumber(l.hunger, 0, 100) || !validNumber(l.fatigue, 0, 100) || !validNumber(l.progress, 0, 1200) || l.wagesToday < 0 ||
            l.wagesToday > PaidSpells || !validNumber(l.goalX, 0, 256) || !validNumber(l.goalY, 0, 256) || l.task.size() > MaxTaskText ||
            l.reason.size() > 256 || l.goalCell.size() > 80 || l.homeCell.size() > 80 || l.relocationCell.size() > 80 ||
            !validNumber(l.homeX, 0, 256) || !validNumber(l.homeY, 0, 256) || !validNumber(l.relocationX, 0, 256) ||
            !validNumber(l.relocationY, 0, 256))
            return false;
    // Operators can't move those in essential work (see relocate); the only other move is to a spouse's home.
    for (const auto& [id, l] : s.residents)
    {
        if (l.relocationCell.empty() || l.role == "resident" || l.role == "civilian")
            continue;
        const auto spouse = s.careers.spouses.find(id);
        const auto home = spouse == s.careers.spouses.end() ? s.residents.end() : s.residents.find(spouse->second);
        if (home == s.residents.end() || home->second.homeCell != l.relocationCell || s.careers.spouses.count(spouse->second) == 0 ||
            s.careers.spouses.at(spouse->second) != id)
            return false;
    }
    // The ledger: a broken one refuses (it is the save's own order); an entry of a good the world no longer knows goes.
    std::int64_t previous = 0;
    for (const auto& e : s.ledger)
    {
        if (e.sequence <= previous || e.sequence >= s.nextEntry || e.day < 0 || e.day > s.budgetDay || e.coins < 0 ||
            e.coins > MoneyLimit || e.quantity < 0 || e.quantity > 99 || e.kind.size() > 80 || e.from.size() > 80 || e.to.size() > 80)
            return false;
        previous = e.sequence;
    }
    s.ledger.erase(std::remove_if(s.ledger.begin(), s.ledger.end(), [](const EconomyEntry& e) { return !e.item.empty() && !itemValid(e.item); }),
                   s.ledger.end());
    // The accounts whose goods went into or came out of quarantine: the save no longer says what they hold, so the journal
    // is told (Game::journalRestored), or a crash before the next checkpoint would count the goods twice or lose them.
    quarantineMoved_.clear();
    tillsMoved_.clear();
    using Quarantine = std::map<std::string, std::map<std::string, int>>;
    for (const Quarantine* side : {&saved.staged.quarantine, static_cast<const Quarantine*>(&s.staged.quarantine)})
        for (const auto& [id, goods] : *side)
        {
            const auto a = saved.staged.quarantine.find(id);
            const auto b = s.staged.quarantine.find(id);
            if (a == saved.staged.quarantine.end() || b == s.staged.quarantine.end() || a->second != b->second)
                quarantineMoved_.insert(id);
        }
    state_ = s;
    farmsLooked_ = false;                           // (A restored day may have been tended already: tendCareers.)
    indexTills();
    friendGroups_.clear();
    groupsDay_ = -1;
    roster_ = roster;
    for (auto& life : state_.residents)
        if (roster_ == Roster::Demo) defaultHome(life.first, life.second);
    if (roster_ == Roster::Authored)
    {
        if (state_.careers.positions.empty())
            defaultCareers();                       // A save from before careers: everyone holds their own job.
        else
            reconcileCareers();
        foundShops();                               // (A save from before tills: its shops' own, once.)
    }
    stagedRestored();                               // (Doc 79, 14: the staged day and the bands as saved, out-of-range settled.)
    return true;
}
void Society::setNeeds(const std::string& id, double hunger, double fatigue)
{
    const auto life = state_.residents.find(id);
    if (life == state_.residents.end())
        return;
    if (std::isfinite(hunger))
        life->second.hunger = std::clamp(hunger, 0., 100.);
    if (std::isfinite(fatigue))
        life->second.fatigue = std::clamp(fatigue, 0., 100.);
}
bool Society::rehome(const std::string& id)
{
    const auto* r = spec(id);
    const auto life = state_.residents.find(id);
    if (!r || life == state_.residents.end())
        return false;
    life->second.homeCell = r->home.cell;
    life->second.homeX = r->home.x;
    life->second.homeY = r->home.y;
    life->second.relocationCell.clear();
    life->second.relocationX = life->second.relocationY = 0;
    return true;
}

// Food (doc 35, Part 7).
bool Society::edible(const std::string& item)
{
    const auto* good = items::good(item);
    return good && good->nourish > 0 && !good->drink;
}

int Society::nourishment(const std::string& item)
{
    const auto* good = items::good(item);
    return good && !good->drink ? good->nourish : 0;
}

std::string Society::bestFood(const EconomyAccount& account)
{
    const std::string* best = nullptr;
    int most = 0;
    for (const auto& [item, n] : account.stock)
        if (n > 0)
            if (const auto* good = items::good(item); good && !good->drink && good->nourish > most)
            {
                most = good->nourish;
                best = &item;
            }
    return best ? *best : std::string();
}

std::string Society::eatFirst(const EconomyAccount& account)
{
    const std::string* first = nullptr;
    const items::Item* firstGood = nullptr;
    const auto keeps = [](const items::Item* g) { return g->keeps > 0 ? g->keeps : 1e9; };
    for (const auto& [item, n] : account.stock)
        if (n > 0)
            if (const auto* good = items::good(item); good && !good->drink && good->nourish > 0)
                if (!first || keeps(good) < keeps(firstGood) || (keeps(good) == keeps(firstGood) && good->nourish > firstGood->nourish))
                {
                    first = &item;
                    firstGood = good;
                }
    return first ? *first : std::string();
}

bool Society::hasFood(const EconomyAccount& account)
{
    for (const auto& [item, n] : account.stock)
        if (n > 0)
            if (const auto* good = items::good(item); good && !good->drink && good->nourish > 0)
                return true;
    return false;
}

// Places.
std::string Society::communityOfResident(const std::string& id) const
{
    const auto* r = spec(id);
    if (!r || !day_.communityOf)
        return {};
    // A producer who works out in the country (a farmer in the fields) belongs to the town they live in.
    auto here = day_.communityOf(r->work.cell);
    return here.empty() ? day_.communityOf(r->home.cell) : here;
}

std::string Society::treasuryOf(const std::string& community) const
{
    if (community == "treasury" || community.rfind("stores:", 0) == 0)
        return community;                              // (Already a treasury.)
    const auto own = "stores:" + community;
    return state_.accounts.count(own) ? own : std::string("treasury");
}

bool Society::goesToChurch(const std::string& resident, std::int64_t day)
{
    // A third of the town, a different third from week to week (a steady choice for the week).
    return std::hash<std::string>{}(resident + "|church|" + std::to_string(day / 7)) % 3 == 0;
}

bool Society::clergy(const std::string& title)
{
    thread_local std::unordered_map<std::string, bool> known;      // (Asked of the same titles every decision.)
    if (const auto found = known.find(title); found != known.end())
        return found->second;
    std::string t = title;
    std::transform(t.begin(), t.end(), t.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    bool is = false;
    for (const char* word : {"chapel", "priest", "acolyte", "cathedral", "choir", "prelate", "shrine", "chaplain", "sexton", "deacon",
                             "abbey", "temple", "church", "the mass", "confession", "the crypt", "healer"})
        is = is || t.find(word) != std::string::npos;
    return known.emplace(title, is).first->second;
}

bool Society::houseHead(const std::string& title)
{
    // The head of a great house (a lord, a lady, a hall's keeper).
    thread_local std::unordered_map<std::string, bool> known;      // (As clergy.)
    if (const auto found = known.find(title); found != known.end())
        return found->second;
    std::string t = title;
    std::transform(t.begin(), t.end(), t.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    // (Keeping a house's own seat: "keeping Grayrock Hall", "keeping Timberland Court"; not a caretaker of a public
    // hall, "keeping the Rain Hall".)
    bool seat = false;
    for (const char* word : {" Hall", " House", " Manor", " Court"})
        seat = seat || title.find(word) != std::string::npos;
    const bool is = t.rfind("ruling ", 0) == 0 || t.rfind("holding court", 0) == 0 ||
                    (t.rfind("keeping ", 0) == 0 && t.rfind("keeping the ", 0) != 0 && seat);
    return known.emplace(title, is).first->second;
}

const std::vector<Society::House>& Society::houses() const
{
    // (Again once the world has said which town a cell is in: before, a house's town isn't known.)
    if (housesKnown_ && (housesTold_ || !day_.communityOf))
        return houses_;
    housesKnown_ = true;
    housesTold_ = bool(day_.communityOf);
    houses_.clear();
    const auto slug = [](const std::string& name) {
        std::string out;
        for (const unsigned char c : name)
            out += std::isalnum(c) ? char(std::tolower(c)) : '_';
        return out;
    };
    for (const auto& p : positions_)
    {
        if (p.role != "civilian" || !houseHead(p.title))
            continue;
        // "ruling House Fell" -> House Fell; "keeping Vesk Manor" -> Vesk Manor; "holding court" -> the Court of a town.
        const auto community = communityOfResident(p.founder);
        std::string name = p.title.rfind("ruling ", 0) == 0 ? p.title.substr(7) : p.title.rfind("keeping ", 0) == 0 ? p.title.substr(8)
                                                                                                                    : "the Court of " + community;
        const auto id = "house:" + slug(name);
        if (std::none_of(houses_.begin(), houses_.end(), [&](const House& h) { return h.id == id; }))
            houses_.push_back({id, name, p.id, community});
    }
    return houses_;
}

bool Society::communalHome(const std::string& home)
{
    return home.find("barracks") != std::string::npos || home.find("bunkhouse") != std::string::npos ||
           home.find("quarters") != std::string::npos || home.find("watch_house") != std::string::npos ||
           home.find("lodging") != std::string::npos;
}

// Children's friend groups: each town's children, by age, in fours.
void Society::groupChildren(const std::map<std::string, LifeBody>& bodies)
{
    friendGroups_.clear();
    std::map<std::string, std::vector<std::pair<int, std::string>>> byTown;
    for (const auto& [id, life] : state_.residents)
        if (const auto body = bodies.find(id); body != bodies.end() && body->second.age < 16 && body->second.age >= 4)
            byTown[communityOfResident(id)].push_back({body->second.age, id});
    for (auto& [town, kids] : byTown)
    {
        std::sort(kids.begin(), kids.end());
        for (std::size_t k = 0; k < kids.size(); ++k)
            friendGroups_[kids[k].second] = town + "|friends|" + std::to_string(k / 4);
    }
}

const std::string& Society::friendGroup(const std::string& child) const
{
    static const std::string none;
    const auto found = friendGroups_.find(child);
    return found == friendGroups_.end() ? none : found->second;
}

// Shops' tills.
void Society::indexTills()
{
    tills_.clear();
    for (auto it = state_.accounts.lower_bound("till:"); it != state_.accounts.end() && it->first.rfind("till:", 0) == 0; ++it)
        tills_[it->first.substr(5)] = it->first;
}

std::string Society::tillOf(const std::string& keeper) const
{
    const auto* job = jobOf(keeper);
    if (!job)
        return keeper;
    const auto till = tills_.find(job->id);
    return till != tills_.end() ? till->second : keeper;
}

void Society::foundShops()
{
    for (const auto& p : positions_)
    {
        const auto till = "till:" + p.id;
        if (p.role != "merchant" || account(till) || !openAccount(till))
            continue;
        // The shop's goods are the till's: its keeper's wares (and anything it holds that isn't food; what it eats stays
        // its own). Its money is the Board's: set to what it pays when it buys (doc 79, 7.5), nothing made here.
        const auto& keeper = state_.careers.positions[p.id].holder;
        if (const auto* purse = keeper.empty() ? nullptr : account(keeper))
        {
            const auto sold = wares(keeper);
            std::vector<std::pair<std::string, int>> goods(purse->stock.begin(), purse->stock.end());
            for (const auto& [item, n] : goods)
            {
                const bool ware = std::find(sold.begin(), sold.end(), items::baseOf(item)) != sold.end() ||
                                  std::find(sold.begin(), sold.end(), item) != sold.end();
                if (n > 0 && (ware || !edible(item)))
                    for (int left = n; left > 0; left -= 99)
                        shift(keeper, till, item, std::min(left, 99), 0, "the shop's till");
            }
        }
    }
}

// The staged economy, Phase 0 (Docs/Design/79-a-staged-economy.md, 19): the households, their food by class, the nightly
// purse and treasury set. Raw sets: no ledger entry, no journal record, no WorldEvent.
namespace
{
std::uint64_t stagedHash(const std::string& text)
{
    std::uint64_t h = 1469598103934665603ULL;
    for (const unsigned char c : text)
        h = (h ^ c) * 1099511628211ULL;
    return h;
}
// A lot from 0 to 1, fixed by its words (the same on every platform and thread).
double stagedLot(const std::string& words)
{
    return double(stagedHash(words) % 1000003ULL) / 1000003.;
}
// What an account holds to eat, in nourishment.
double stagedOnHand(const EconomyAccount& a)
{
    double n = 0;
    for (const auto& [item, count] : a.stock)
        if (count > 0)
            n += double(Society::nourishment(item)) * count;
    return n;
}
} // namespace

std::map<std::string, std::vector<std::string>> Society::households(bool homeless) const
{
    // A household: the residents sharing a home (in a communal home, each alone), with a child under 16 and a spouse who
    // live elsewhere (a child given a room of its own upstairs): whoever earns feeds the family.
    std::map<std::string, std::string> keyOf;          // Resident -> its home's key.
    for (const auto& [id, life] : state_.residents)
        if (spec(id) && !dead_.count(id) && !life.homeCell.empty())
            keyOf[id] = communalHome(life.homeCell) ? life.homeCell + "|" + id : life.homeCell;
    std::map<std::string, std::string> joined;         // Home key -> the key it shops with (union by kin).
    const std::function<std::string(const std::string&)> root = [&](const std::string& k) {
        const auto found = joined.find(k);
        return found == joined.end() || found->second == k ? k : (joined[k] = root(found->second));
    };
    const auto join = [&](const std::string& a, const std::string& b) {
        const auto ka = keyOf.find(a), kb = keyOf.find(b);
        if (ka == keyOf.end() || kb == keyOf.end())
            return;
        const auto ra = root(ka->second), rb = root(kb->second);
        if (ra != rb)
            joined[std::max(ra, rb)] = std::min(ra, rb);
    };
    for (const auto& [child, parents] : state_.careers.parents)
        if (const auto* c = spec(child); c && c->age < 16)
            for (const auto& parent : parents)
                join(child, parent);
    for (const auto& [a, b] : state_.careers.spouses)
        join(a, b);
    std::map<std::string, std::vector<std::string>> homes;
    for (const auto& [id, key] : keyOf)
        homes[root(key)].push_back(id);
    // The homeless (doc 79, 6.2): each a household of its own, joined to nobody.
    if (homeless)
        for (const auto& [id, life] : state_.residents)
            if (life.homeCell.empty() && spec(id) && !dead_.count(id))
                homes["|" + id].push_back(id);
    return homes;
}

std::string Society::householdClass(const std::vector<std::string>& members, ClassPass* pass) const
{
    std::map<std::string, int> count;
    for (const auto& m : members)
    {
        auto c = classOf(m, 0, pass);               // (The public classOf's: middle while unknown.)
        ++count[c.empty() ? std::string("middle") : c];
    }
    std::string cls = count.count("upper") ? "upper" : "middle";
    if (!count.count("upper"))
        for (const auto& [c, n] : count)
            if (n > count[cls])
                cls = c;
    return cls;
}

void Society::stagedHourly(std::int64_t hour)
{
    // Each game hour as it turns, hours missed run once each (at most a day of them); the first hour is not run (a fresh
    // world, a restore).
    if (stagedHour_ < 0 || hour < stagedHour_)
    {
        stagedHour_ = hour;
        stagedHouseholds(hour / 24);
        state_.staged.hour = stagedHour_;
        state_.staged.day = stagedDay_;
        return;
    }
    for (auto h = std::max(stagedHour_ + 1, hour - 23); h <= hour; ++h)
    {
        const int hod = int(h % 24);
        const auto day = h / 24;
        if (hod == 0)
            stagedNight(day);                       // (The night's set by yesterday's households, then today's.)
        if (day != stagedDay_)
            stagedHouseholds(day);
        if (hod == 0)
            stagedLarders();                        // (The morning's purses and larders: who is in want of food, 10.2.)
        if (hod >= ShopOpen && hod < ShopShut)
        {
            for (const auto& [home, h0] : stagedHomes_)
                if (h0.hour == hod)
                {
                    stagedFood(home, h0.members, h0.cls);
                    stagedShopping(home);
                }
            streetHour(hod);                        // (Doc 79, Phase 6: what waited for a player to leave, tried again.)
        }
    }
    if (stagedRecount_ && stagedDay_ >= 0)
        stagedHouseholds(stagedDay_);               // (A new resident in a household of its own: the bands again.)
    stagedHour_ = hour;
    state_.staged.hour = stagedHour_;               // (Saved, so a restart runs the hours it missed: Phase 2.)
    state_.staged.day = stagedDay_;
}

std::map<std::string, std::string> Society::householdClasses(const std::map<std::string, std::vector<std::string>>& homes) const
{
    ClassPass pass;
    std::map<std::string, std::string> out;
    for (const auto& [home, members] : homes)
        out.emplace(home, home.rfind("|", 0) == 0 ? std::string("homeless") : householdClass(members, &pass));
    return out;
}

void Society::stagedHouseholds(std::int64_t day)
{
    stagedHomesFor(day);
    stagedBands(day);                               // (Doc 79, Phase 2: each household's band, food shop and food.)
}

void Society::stagedHomesFor(std::int64_t day)
{
    if (day != stagedDay_)
    {
        // (A new day: yesterday's waiting food and buys, and runners on the way, are let go: doc 79, Phase 6.)
        runnerWaiting_.clear();
        foodDeferred_.clear();
        buysDeferred_.clear();
    }
    stagedDay_ = day;
    stagedRecount_ = false;
    countFarmPosts();                               // (The farms' posts held, counted each day.)
    stagedHomes_.clear();
    auto homes = households(true);
    auto classes = householdClasses(homes);
    for (auto& [home, members] : homes)
    {
        StagedHome h;
        h.cls = std::move(classes.at(home));
        h.hour = ShopOpen + int(stagedHash(home) % std::uint64_t(ShopShut - ShopOpen));
        h.members = std::move(members);
        stagedHomes_.emplace(home, std::move(h));
    }
}

void Society::stagedFood(const std::string& home, const std::vector<std::string>& members, const std::string& cls)
{
    (void)cls;
    stagedFoodBy(home, members, FoodBy::Hour);
}

void Society::stagedFoodBy(const std::string& home, const std::vector<std::string>& members, FoodBy by, const std::string* runnerTill)
{
    // Food from the shelf by the household's band (doc 79, 6.5): its band's meals a head a day, of the first of its foods its
    // food shop sells, up to two days' on hand (more the day before a Restday or a festival: buying ahead), never beyond.
    // A short day (a fixed lot by the household and the day, against its band's chance at the slider, the poor's raised
    // by their staple's pressure) gives half a day's food; a bare shelf gives the poor and destitute a short day, the
    // others are fed from elsewhere. Never Hungry: no short days. A raw stock change: no ledger entry, no journal record,
    // no WorldEvent.
    // The standing-still rule (doc 79, 7.7 and 6.6; Phase 6): where a player is in the food shop's cell or the next, the
    // stock moves only with a visible buyer: the household's errand-runner walks to the counter (runnerBought), or, with
    // none to send, the food waits for the next hour; still watched at the shutting, the household is fed from elsewhere
    // and the shelf is left as it stands (a bare one still gives the poor their short day). Once a day a household.
    const auto found = stagedHomes_.find(home);
    if (found == stagedHomes_.end() || found->second.food.empty())
        return;
    const auto& h = found->second;
    if (const auto fed = fedOn_.find(home); fed != fedOn_.end() && fed->second == stagedDay_)
        return;
    if (by == FoodBy::Hour && !h.till.empty() && street_.watched && street_.watched(*this, h.till))
    {
        if (runnerWaiting_.count(home))
            return;
        // (The runner goes to the household's own food shop, or, that shut, to another open one selling its food: 6.6.)
        std::string to = h.till;
        if (ownRule("errandRunners") > 0 && street_.runner && street_.runner(*this, home, to) && !to.empty())
        {
            runnerWaiting_[home] = to;
            foodDeferred_.erase(home);
        }
        else
            foodDeferred_.insert(home);
        return;
    }
    fedOn_[home] = stagedDay_;
    foodDeferred_.erase(home);
    runnerWaiting_.erase(home);
    // (The shelf it comes from: its runner's, or its own food shop's.)
    const std::string from = runnerTill && !runnerTill->empty() ? *runnerTill : h.till;
    const auto& t = statusTable();
    const int band = std::clamp(h.band, 0, status::Bands - 1);
    const auto& row = t.rows[std::size_t(band)];
    const auto day = stagedDay_;
    const bool neverHungry = ownRule("neverHungry") > 0;
    const bool poor = band <= status::Poor && !h.lifted && !neverHungry;
    bool shortDay = false;
    if (!neverHungry && !h.lifted)
    {
        double chance = status::shortChance(t, band, prosperity());
        if (band <= status::Poor && hooks_.pressure)
        {
            const double p = hooks_.pressure(*this, h.market, h.food);
            if (std::isfinite(p) && p > 0)
                chance *= std::min(t.shortPressureMost, 1 + (t.shortPressureMost - 1) * p / t.shortPressureAt);
        }
        shortDay = status::lot(home + "|short|" + std::to_string(day)) < std::clamp(chance, 0., 1.);
    }
    const double tomorrow = double(day + 1) + .5;
    const int ahead = calendar::weekdayOf(tomorrow) == calendar::Restday || calendar::festivalDay(tomorrow) ? std::max(0, ownRule("buyAheadDays")) : 0;
    const double ceiling = (2 + ahead) * NourishADay;
    const double mealWorth = std::max(1, nourishment("meal"));
    const int each = std::max(1, nourishment(h.food));
    // Each member's pieces, and the household's from the shelf.
    std::vector<std::pair<EconomyAccount*, int>> wants;
    int total = 0;
    for (const auto& id : members)
    {
        const auto acc = state_.accounts.find(id);
        if (acc == state_.accounts.end() || dead_.count(id) || !spec(id))
            continue;
        auto& wallet = acc->second;
        const double have = stagedOnHand(wallet);
        // (A short day gives half a day's food: half of the band's meals, and never more than half of what a wolf eats in
        // a day. Two meals a day are more than a day's need, so half of them alone was still a full day and the poor's
        // short days never showed: Review A.)
        const double ration = row.meals * mealWorth;
        const double provision = (shortDay ? .5 * std::min(ration, NourishADay) : ration) + ahead * NourishADay;
        const double want = std::min(ceiling, have + provision);
        // (Whole pieces: up on a full day, down on a short one, so a short day of a big dish is short indeed.)
        int pieces = want <= have ? 0 : shortDay ? int(std::floor((want - have) / each + 1e-9)) : int(std::ceil((want - have) / each - 1e-9));
        if (pieces < 1 || stock(wallet, h.food) > StockLimit - pieces || (!wallet.stock.count(h.food) && wallet.stock.size() >= MaxGoodsKinds))
            pieces = 0;
        wants.push_back({&wallet, pieces});
        total += pieces;
    }
    if (total <= 0)
        return;
    int got = total;
    if (!h.till.empty() && by == FoodBy::Unseen)
    {
        // (Watched to the last: what the shelf holds says whether the poor go short; nothing leaves it.)
        const auto* shelf = account(h.till);
        got = std::clamp(shelf ? stock(*shelf, h.food) : 0, 0, total);
    }
    else if (!from.empty() && hooks_.shelf)
    {
        got = std::clamp(hooks_.shelf(*this, from, h.food, total), 0, total);
        if (got > 0)
            noteSale(from, h.food, got, false, true);
    }
    if (got < total && poor && !shortDay)
    {
        // A bare shelf: the poor and destitute it serves go short today (half of what they'd have had, at most what came).
        int halfTotal = 0;
        for (auto& w : wants)
            halfTotal += (w.second = w.second / 2);
        if (got > halfTotal)
        {
            // (More came than half: the rest goes back on the shelf it came from.)
            if (const auto till = state_.accounts.find(from); till != state_.accounts.end() && by != FoodBy::Unseen)
            {
                till->second.stock[h.food] += got - halfTotal;
                noteSale(from, h.food, -(got - halfTotal), false, true);
            }
            got = halfTotal;
        }
        total = halfTotal;
    }
    int left = poor ? std::min(got, total) : total;   // (The comfortable and rich: what the shelf lacked, from elsewhere.)
    for (auto& [wallet, pieces] : wants)
    {
        const int n = std::min(pieces, left);
        if (n > 0)
            wallet->stock[h.food] += n;
        left -= n;
    }
    // The familiarity step between one member and the keeper (6.6): no WorldEvent, so nothing remembered as a gift.
    if (from.rfind("till:", 0) == 0 && !members.empty() && provisions_.size() < 8192)
        if (const auto held = state_.careers.positions.find(from.substr(5)); held != state_.careers.positions.end() &&
                                                                               !held->second.holder.empty() &&
                                                                               held->second.holder != members.front())
            provisions_.push_back({members.front(), held->second.holder});
}

std::int64_t Society::stagedPurse(const std::string& id, const std::string& home, const std::string& cls, std::int64_t day) const
{
    // A stable point in its band's range from its home's lot, with a little jitter by the day (doc 79, 6.5: destitute
    // 0-3p, poor 2-12p, comfortable 12-60p, rich 60-300p, as status.json says, at most 500p).
    (void)cls;
    const auto found = stagedHomes_.find(home);
    const int band = found == stagedHomes_.end() ? status::Comfortable : std::clamp(found->second.band, 0, status::Bands - 1);
    const auto& row = statusTable().rows[std::size_t(band)];
    const auto low = std::min(row.purseLow, row.purseHigh), high = std::min<std::int64_t>(500, row.purseHigh);
    const double at = .8 * stagedLot(home + "|purse") + .2 * stagedLot(id + "|" + std::to_string(day));
    return low + std::int64_t(std::llround(double(std::max<std::int64_t>(0, high - low)) * at));
}

void Society::stagedNight(std::int64_t day)
{
    businessDay(day);                               // (Doc 79, Phase 6: yesterday's business for every shop, 10.3.)
    // Each resident's purse halfway toward its band's target (down as well as up), except while held (a theft, a gift, a
    // prize, players' pay: 6.5) and never up in its lean days; staged wallets' goods trimmed to the band's; the treasury and
    // each town treasury set within status.json's floor and ceiling (doc 79, 9.2); the old tallies, events and holds pruned.
    for (const auto& [home, h] : stagedHomes_)
        for (const auto& id : h.members)
            if (const auto* a = account(id); a && !dead_.count(id))
            {
                stagedTrim(id, h.band);
                if (heldNow(id))
                    continue;
                const auto target = stagedPurse(id, home, h.cls, day);
                const auto gap = target - a->cash;
                if (gap > 0 && leanNow(id))
                    continue;
                if (gap != 0)
                    stagedSet(id, a->cash + (gap > 0 ? (gap + 1) / 2 : -((-gap + 1) / 2)));
            }
    const auto& t = statusTable();
    std::vector<std::string> treasuries;
    for (const auto& [id, a] : state_.accounts)
        if (id == "treasury" || id.rfind("stores:", 0) == 0)
            treasuries.push_back(id);
    for (const auto& id : treasuries)
        stagedSet(id, std::clamp<std::int64_t>(cash(id), t.treasuryFloor, std::max(t.treasuryFloor, t.treasuryCeiling)));
    stagedPrune(day);
}

void Society::stagedSet(const std::string& accountId, std::int64_t coins)
{
    const auto found = state_.accounts.find(accountId);
    if (found == state_.accounts.end())
        return;
    found->second.cash = std::clamp<std::int64_t>(coins, 0, MoneyLimit);
}

// A Gift or a paw lent to a maker (docs 43 and 53), kept for the economy rules to come.
void Society::lendGift(const std::string& maker, double lift, double untilDay)
{
    auto& lent = giftLift_[maker];
    lent.first = std::min(30.0, (lent.second >= untilDay - 1 ? lent.first : 0) + lift);   // (Several Gifts help, to a point.)
    lent.second = untilDay;
}

void Society::lendPaw(const std::string& maker, double rate, double untilDay)
{
    pawLent_[maker] = {rate, untilDay};
}

double Society::giftLiftOf(const std::string& maker) const
{
    const auto lent = giftLift_.find(maker);
    return lent == giftLift_.end() ? 0 : lent->second.first;
}

double Society::pawRateOf(const std::string& maker) const
{
    const auto lent = pawLent_.find(maker);
    return lent == pawLent_.end() || lent->second.second < decidingAt_ ? 1 : lent->second.first;
}
} // namespace ratw
