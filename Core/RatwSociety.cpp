#include "RatwSociety.h"

#include "RatwCalendar.h"
#include "RatwItems.h"
#include <algorithm>
#include <unordered_map>
#include <cctype>
#include <cmath>
#include <limits>

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
    tills_.clear();
    farmsDay_ = -1;                                 // (The farms' carried fractions and crews' wants start again: doc 71.)
    farmsMinted_ = 0;
    for (auto& f : farms_)
    {
        f.grown.clear();
        f.eaten.clear();
        f.hunger = 0;
    }
    friendGroups_.clear();
    groupsDay_ = -1;
    ++rosterRevision_;
    state_.enabled = roster != Roster::None;
    state_.accounts["treasury"] = {1000, {{"herbs", 100}, {"meal", 50}}};
    state_.minted = 1000;
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
        state_.minted += 80;
    }
    state_.accounts["npc_keeper"].stock = {{"herbs", 8}, {"meal", 12}};
    state_.accounts["npc_cook"].stock = {{"herbs", 12}, {"meal", 4}};
    state_.accounts["npc_porter"].stock = {{"herbs", 10}, {"meal", 2}};
    state_.accounts["npc_smith"].stock["sword"] = SmithSwords;    // Ash keeps a few dull bronze blades for sale.
    record("initial funding", "outside", "settlement", "", 0, state_.minted);
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
    buildSeasons();
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
    if (const auto kept = waresCache_.find(merchant); kept != waresCache_.end() && kept->second.first == r->workLabel)
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
    static const char* Food[] = {"general", "provisioner", "stall", "bakery", "butcher", "fishmonger", "brewery", "inn"};   // (foodShop)
    const bool food = business && std::find(std::begin(Food), std::end(Food), business->id) != std::end(Food);
    if (smith(merchant) && std::find(out.begin(), out.end(), "sword") == out.end())
        out.insert(out.begin(), "sword");
    if (!business || food)
        out.push_back("meal");
    if (!business || business->id == "herbalist" || business->id == "apothecary" || business->id == "general" || business->id == "stall" ||
        business->id == "inn")
        out.push_back("herbs");
    waresCache_[merchant] = {r->workLabel, out};
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
std::int64_t Society::moneySupply() const
{
    std::int64_t result = 0;
    for (const auto& pair : state_.accounts)
        result += pair.second.cash;
    return result;
}
bool Society::conserved() const
{
    return moneySupply() == state_.minted - state_.sunk;
}
void Society::addPlayer(const std::string& id)
{
    if (!playerAccountId(id) || state_.accounts.count(id) || state_.accounts.size() >= MaxAccounts)
        return;
    auto& reserve = state_.accounts.at("treasury");
    EconomyAccount created;
    created.cash = std::min<std::int64_t>(PlayerWelcome, reserve.cash);
    reserve.cash -= created.cash;
    created.stock["herbs"] = std::min(2, stock(reserve, "herbs"));
    reserve.stock["herbs"] -= created.stock["herbs"];
    state_.accounts[id] = created;
    record("settlement welcome grant", "treasury", id, "", 0, created.cash);
    // Everyone starts with food (doc 36): a few days' meals of their own, not taken from stores that may be empty.
    create(id, "meal", StartingMeals, "provisions to start with");
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
    if (seller.rfind("till:", 0) == 0)
    {
        soldToday_[seller][item] += quantity;      // (A shop's shelves follow its sales: doc 68, 7.)
        if (left == 0)
            soldOut_[seller].insert(item);          // (And its prices its stock: doc 68, "Self-correcting rules".)
    }
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
                             int quantity, bool buy) const
{
    if (!state_.enabled || !merchant(seller) || !playerAccountId(player) || !account(player) || !account(seller))
        return {false, "This trader is unavailable."};
    if (quantity < 1 || quantity > 99)
        return {false, "Choose a whole quantity from 1 to 99."};
    const auto deals = wares(seller);
    // A shop deals in its goods of every quality (doc 35, Part 4): a tanner takes a fine hide as well as a common one.
    if (!itemValid(item) || std::find(deals.begin(), deals.end(), items::baseOf(item)) == deals.end())
        return {false, "This trader has no use for those goods."};
    if (!account(tillOf(seller)))
        return {false, "This trader is unavailable."};
    const auto& m = *account(tillOf(seller));          // The shop's shelves and money (its till).
    const auto& p = *account(player);
    // The shop's price now (doc 68, Progress, "Players in the shifting market"; the user, 2026-11-01: "players should
    // experience the economy shifts as well"): the catalogue's times the season's and the shop's own from its stock, for
    // the lot to the nearest penny and never under a penny a piece; market stalls sell a little cheaper (a tenth off,
    // rounded down); and a shop buys at SellBackShare of it, so a scarce ware fetches more and a glutted one less, its
    // margin kept. (A lot's, not a piece's rounded: a penny's bread at a quarter over is 1p alone, 5p for four.)
    const double f = shopFactor(tillOf(seller), item);
    const auto each = catalogPrice(item);
    const bool stall = buy && atStall(seller);
    const double share = !buy ? SellBackShare : stall ? .9 : 1.;
    if (each > std::numeric_limits<std::int64_t>::max() / 2 / quantity)
        return {false, "That trade exceeds safe account capacity.", each, 0};
    const auto lot = [&](int n) {
        if (f == 1 && share == 1)
            return each * n;
        const double x = double(each) * f * share * n;
        return std::max<std::int64_t>(n, stall ? std::int64_t(std::floor(x + 1e-9)) : std::llround(x));
    };
    // What it takes from a player is as much as it keeps of a good: a supplier's or buyer's store of what it supplies or
    // buys (crafts.json), a few of anything else.
    const auto* business = spec(seller) ? items::businessFor(spec(seller)->workLabel) : nullptr;
    const auto dealsIn = [&](const std::vector<std::string>& list) { return std::find(list.begin(), list.end(), items::baseOf(item)) != list.end(); };
    const bool stocked = business && (dealsIn(items::suppliesFor(business->id)) || dealsIn(items::buysFor(business->id)));
    const int held = stock(m, item);
    const int cap = stocked ? SuppliesKept : items::wearable(item) ? 4 : item == "meal" ? 24 : item == "sword" ? 4 : item == "herbs" ? 20 : GoodsKept;
    const std::int64_t price = lot(1), total = lot(quantity);   // (A piece's, for the list; the lot's, paid.)
    if (!buy && held + quantity > cap)
        return {false, "The trader already has enough of those goods.", price, total};
    if (stock(buy ? m : p, item) < quantity)
        return {false, "There is not enough stock.", price, total};
    if ((buy ? p : m).cash < total)
        return {false, buy ? "You cannot afford that purchase." : "The trader cannot afford that purchase.", price,
                total};
    if ((buy ? m : p).cash > MoneyLimit - total || stock(buy ? p : m, item) > StockLimit - quantity)
        return {false, "That trade exceeds safe account capacity.", price, total};
    return {true, "Available", price, total};
}
EconomyResult Society::trade(const std::string& player, const std::string& trader, const std::string& item,
                             int quantity, bool buy)
{
    auto result = quote(player, trader, item, quantity, buy);
    if (!result.ok)
        return result;
    // The lot's price (quote): its whole pennies a piece in the sale, and the odd pennies after.
    const auto till = tillOf(trader);
    const auto& seller = buy ? till : player;
    const auto& buyer = buy ? player : till;
    const auto each = result.total / quantity, odd = result.total - each * quantity;
    result.ok = transfer(seller, buyer, item, quantity, each, "local trade") &&
                (odd == 0 || shift(buyer, seller, "", 0, odd, "local trade: the odd pennies"));
    result.message = result.ok ? std::string(buy ? "Bought " : "Sold ") + std::to_string(quantity) + " " +
                                     itemName(item) + " for " + std::to_string(result.total) + " silver pennies."
                               : "The trade could not be completed.";
    return result;
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
        state_.exportsRemaining = 8;
        state_.importsRemaining = 4;
        for (auto& resident : state_.residents)
            resident.second.wagesToday = 0;
        const int recovery[] = {20, 30, 12, 4};
        state_.herbPatch = std::min(60, state_.herbPatch + recovery[season]);
    }
    if (roster_ == Roster::Authored)
    {
        setYearDay(absoluteDay);                    // (The seasons in the Country's supply: doc 68, Progress.)
        // The city's loop (doc 68), each game hour as it turns: hours missed run once each, at most a day of them; the
        // first hour a society decides (a fresh world, a restore) is not run, so nothing is paid twice.
        if (!needsPaused())
        {
            const auto hour = std::int64_t(std::floor(absoluteDay * 24));
            if (loopHour_ < 0 || hour < loopHour_)
            {
                loopHour_ = hour;
                // (Which areas are on today, before that hour's draws: the farms' switch as of the date it now knows, an
                // area switched on while it was down founded; doc 71, 4. The farms are tended from the next hour.)
                refreshAreas(-1);
            }
            for (auto h = std::max(loopHour_ + 1, hour - 23); h <= hour; ++h)
                loopHourly(h, season);
            loopHour_ = hour;
        }
        else
            loopHour_ = -1;
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
                (account("npc_keeper")->cash >= 5 || hungryCustomer() ||
                 (state_.exportsRemaining > 0 && stock(wallet, "meal") + stock(*account("npc_keeper"), "meal") > 8)))
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
            reason = "Serving customers and limited outside orders.";
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
        else if (task == "trade")
        {
            if (state_.exportsRemaining > 0 && stock(wallet, "meal") > 8 && wallet.cash <= MoneyLimit - 7 &&
                state_.minted <= MoneyLimit - 7)
            {
                --wallet.stock["meal"];
                --state_.exportsRemaining;
                wallet.cash += 7;
                state_.minted += 7;
                record("outside export order", "outside", pair.first, "meal", 1, 7);
            }
            else if (state_.exportsRemaining > 0 && wallet.cash < 5 && available("npc_cook"))
            {
                // The same outside order may buy a cook-owned surplus meal at
                // the counter. Its seven pennies are split 5/2, not minted in
                // addition to the existing order allowance. Eight real meals
                // remain across the co-located seller and shop as a reserve.
                const auto& producerBody = bodies.at("npc_cook");
                auto& producer = state_.accounts.at("npc_cook");
                if (near(producerBody, "tavern", 10.5, 6.5) && near(producerBody, body.cell, body.x, body.y) &&
                    stock(producer, "meal") > 0 && stock(producer, "meal") + stock(wallet, "meal") > 8 &&
                    producer.cash <= MoneyLimit - 5 && wallet.cash <= MoneyLimit - 2 && state_.minted <= MoneyLimit - 7)
                {
                    --producer.stock["meal"];
                    --state_.exportsRemaining;
                    producer.cash += 5;
                    wallet.cash += 2;
                    state_.minted += 7;
                    record("outside consigned sale", "outside", "npc_cook", "meal", 1, 5);
                    record("export commission", "outside", pair.first, "", 0, 2);
                }
            }
            // Expensive, capped import fallback is a true money sink, not a free restock.
            const bool localIngredients =
                (available("npc_cook") && stock(*account("npc_cook"), "herbs") >= 2) ||
                (available("npc_porter") && (stock(*account("npc_porter"), "herbs") >= 2 || state_.herbPatch >= 2));
            if (stock(wallet, "herbs") < 2 && !localIngredients && state_.importsRemaining > 0 && wallet.cash >= 4)
            {
                ++wallet.stock["herbs"];
                --state_.importsRemaining;
                wallet.cash -= 4;
                state_.sunk += 4;
                record("outside import", pair.first, "outside", "herbs", 1, 4);
            }
        }
    }
}
bool Society::restore(const SocietyState& saved)
{
    ++rosterRevision_;
    // A checkpoint names its own population; World rejects one for another world.
    // A checkpoint is validated against this society's own population; a
    // society without one (None) accepts the legacy demo population.
    const Roster roster = !saved.enabled ? roster_ : roster_ == Roster::Authored ? Roster::Authored : Roster::Demo;
    if (roster == Roster::Authored && authored_.residents.empty() && authored_.unplaced.empty())
        return false;
    Society fresh(Roster::None);
    fresh.authored_ = authored_;
    fresh.forgetSpecs();
    fresh.reset(roster);
    SocietyState s = saved;
    if (s.enabled && roster == Roster::Authored && s.accounts.count("treasury"))
    {
        auto& treasury = s.accounts.at("treasury");
        const auto& now = fresh.state_.residents;
        for (auto it = s.residents.begin(); it != s.residents.end();)
            it = now.count(it->first) ? std::next(it) : s.residents.erase(it);
        for (auto it = s.accounts.begin(); it != s.accounts.end();)
            if (it->first != "treasury" && !playerAccountId(it->first) && !facilityAccount(it->first) && !now.count(it->first))
            {
                treasury.cash += it->second.cash;           // A departed resident's coins go back to the town.
                it = s.accounts.erase(it);
            }
            else
                ++it;
        for (const auto& life : now)
        {
            const auto was = s.residents.find(life.first);
            if (was == s.residents.end() || !s.accounts.count(life.first))
            {
                s.residents[life.first] = life.second;      // A newcomer: a fresh life and the authored purse.
                if (!s.accounts.count(life.first))
                {
                    s.accounts[life.first] = fresh.state_.accounts.at(life.first);
                    s.minted += s.accounts[life.first].cash;
                }
            }
            else if (was->second.role != life.second.role)
                was->second = life.second;                  // Re-authored as something else: start that life fresh.
        }
    }
    if (s.accounts.empty() || s.accounts.size() > MaxAccounts || !s.accounts.count("treasury") || s.residents.size() > MaxResidents ||
        s.ledger.size() > 128 || s.minted < 0 || s.minted > MoneyLimit || s.sunk < 0 || s.sunk > s.minted ||
        s.nextEntry < 1 || s.nextEntry > 1000000000000LL || s.budgetDay < 0 || s.budgetDay > 365000000 ||
        s.exportsRemaining < 0 || s.exportsRemaining > 8 || s.importsRemaining < 0 || s.importsRemaining > 4 ||
        s.herbPatch < 0 || s.herbPatch > 60 || !validNumber(s.decisionRemainder, 0, 1))
        return false;
    std::int64_t sum = 0;
    for (const auto& a : s.accounts)
    {
        if (a.first.empty() || a.first.size() > 80 || a.second.cash < 0 || a.second.cash > MoneyLimit ||
            a.second.stock.size() > MaxGoodsKinds)
            return false;
        if (a.first != "treasury" && !playerAccountId(a.first) && !facilityAccount(a.first) &&
            !fresh.state_.residents.count(a.first))
            return false;
        sum += a.second.cash;
        for (const auto& item : a.second.stock)
            if (!itemValid(item.first) || item.second < 0 || item.second > StockLimit)
                return false;
    }
    if (sum != s.minted - s.sunk)
        return false;
    for (const auto& l : s.residents)
        if (!s.accounts.count(l.first) ||
            !fresh.state_.residents.count(l.first) || fresh.state_.residents.at(l.first).role != l.second.role ||
            !validNumber(l.second.hunger, 0, 100) || !validNumber(l.second.fatigue, 0, 100) ||
            !validNumber(l.second.progress, 0, 1200) || l.second.wagesToday < 0 || l.second.wagesToday > PaidSpells ||
            !validNumber(l.second.goalX, 0, 256) || !validNumber(l.second.goalY, 0, 256) || l.second.task.size() > MaxTaskText ||
            l.second.reason.size() > 256 || l.second.goalCell.size() > 80 ||
            l.second.homeCell.size() > 80 || l.second.relocationCell.size() > 80 ||
            !validNumber(l.second.homeX, 0, 256) || !validNumber(l.second.homeY, 0, 256) ||
            !validNumber(l.second.relocationX, 0, 256) || !validNumber(l.second.relocationY, 0, 256))
            return false;
    // Operators can't move those in essential work (see relocate); the only other move is to a spouse's home.
    for (const auto& l : s.residents)
    {
        if (l.second.relocationCell.empty() || l.second.role == "resident" || l.second.role == "civilian")
            continue;
        const auto spouse = s.careers.spouses.find(l.first);
        const auto home = spouse == s.careers.spouses.end() ? s.residents.end() : s.residents.find(spouse->second);
        if (home == s.residents.end() || home->second.homeCell != l.second.relocationCell ||
            s.careers.spouses.count(spouse->second) == 0 || s.careers.spouses.at(spouse->second) != l.first)
            return false;
    }
    std::int64_t previous = 0;
    for (const auto& e : s.ledger)
    {
        if (e.sequence <= previous || e.sequence >= s.nextEntry || e.day < 0 || e.day > s.budgetDay || e.coins < 0 ||
            e.coins > MoneyLimit || e.quantity < 0 || e.quantity > 99 || e.kind.size() > 80 || e.from.size() > 80 ||
            e.to.size() > 80 || (!e.item.empty() && !itemValid(e.item)))
            return false;
        previous = e.sequence;
    }
    if (s.residents.size() != (s.enabled ? fresh.state_.residents.size() : 0u))
        return false;
    if (s.enabled)
        for (const auto& life : fresh.state_.residents)
            if (!s.accounts.count(life.first) || !s.residents.count(life.first) ||
                s.residents.at(life.first).role != life.second.role)
                return false;
    state_ = s;
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
        for (const auto& market : items::country().markets)
            if (!state_.accounts.count(marketFund(market.id)))
                openAccount(marketFund(market.id)); // (A save from before the Country: its purse, empty.)
        // (A save from before doc 68: what the store room's fund took in is the Country's now.)
        if (const auto* old = account(StoreFund))
        {
            if (old->cash > 0)
                shift(StoreFund, marketFund(items::country().markets.front().id), "", 0, old->cash, "fund:store is the Country's now");
            if (account(StoreFund)->cash == 0 && account(StoreFund)->stock.empty())
                closeAccount(StoreFund);
        }
        loopHour_ = -1;
        farmsDay_ = -1;                             // (The farms are tended again from the next hour; their accounts are the save's.)
    }
    return true;
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
        // its own). Its money to begin with is as much as its founder's authored purse, made with the world as that is.
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
        if (const auto* founder = spec(p.founder); founder && founder->purse > 0)
        {
            state_.accounts.at(till).cash += founder->purse;
            state_.minted += founder->purse;
            record("a shop's founding money", "made", till, "", 0, founder->purse);
        }
    }
}

// The store room (Docs/Design/67-ridgemere2-city.md, 6).
const Society::StoreList& Society::storeList(const std::string& keeper) const
{
    static const StoreList none;
    const auto* job = roster_ == Roster::Authored ? jobOf(keeper) : nullptr;
    if (!job || job->role != "merchant")
        return none;
    const auto& e = authored_.economy;
    const auto signature = job->id + '|' + job->title + '|' + (e.internalStore ? 'm' : '-') + (e.storeResale ? 'r' : '-');
    auto& kept = storeCache_[keeper];
    if (kept.signature == signature)
        return kept;
    kept = {};
    kept.signature = signature;
    std::set<std::string> inputs, made;
    const auto* business = items::businessFor(job->title);
    if (business)
        for (const auto* craft : items::craftsFor(business->id))
        {
            for (const auto& [item, n] : craft->in)
                if (itemValid(item))
                    inputs.insert(item);
            for (const auto& [item, n] : craft->out)
                made.insert(item);
        }
    // Making goods: every input of its own crafts, sorted. Resale goods: its supply list, then the kinds of wares its
    // founder started with (Society::wares) that its crafts don't make; a good both ways is a making good.
    std::vector<std::string> resale;
    const auto resell = [&](const std::string& item) {
        if (itemValid(item) && !inputs.count(item) && std::find(resale.begin(), resale.end(), item) == resale.end())
            resale.push_back(item);
    };
    if (business)
        for (const auto& item : items::suppliesFor(business->id))
            resell(item);
    if (spec(job->founder))
        for (const auto& ware : wares(job->founder))
            if (!made.count(ware))
                resell(ware);
    kept.anything = !inputs.empty() || !resale.empty();   // (The world's switches aside: storeClosed tells "nothing" from "off".)
    if (e.internalStore)
        kept.making.assign(inputs.begin(), inputs.end());
    if (e.storeResale)
        kept.resale = std::move(resale);
    kept.goods = kept.making;
    kept.goods.insert(kept.goods.end(), kept.resale.begin(), kept.resale.end());
    return kept;
}

const std::vector<std::string>& Society::storeGoods(const std::string& keeper) const
{
    return storeList(keeper).goods;
}

std::string Society::storeClosed(const std::string& keeper) const
{
    if (roster_ != Roster::Authored)
        return "the store room is only an authored world's";
    if (!spec(keeper))
        return unplaced(keeper) ? keeper + " is unplaced: no body, no shop to keep" : keeper + " is not a resident here";
    const auto* job = jobOf(keeper);
    if (!job || job->role != "merchant")
        return keeper + " keeps no shop (only a shop's keeper draws, into its till)";
    const auto till = tillOf(keeper);
    if (till.rfind("till:", 0) != 0 || !account(till))
        return "the shop has no till";
    const auto& list = storeList(keeper);
    if (list.goods.empty())
        return list.anything ? "off for the whole world" : "this business has nothing to draw";
    if (const auto* founder = spec(job->founder); founder && !founder->internalStore)
        return "off for this business";
    return {};
}

Society::StoreUse Society::storeUse(const std::string& keeper, const std::string& item) const
{
    const auto& list = storeList(keeper);
    if (std::binary_search(list.making.begin(), list.making.end(), item))
        return StoreUse::Make;
    if (std::find(list.resale.begin(), list.resale.end(), item) != list.resale.end())
        return StoreUse::Resell;
    return StoreUse::None;
}

std::int64_t Society::storeCost(const std::string& keeper, const std::string& item, int quantity) const
{
    const auto use = storeUse(keeper, item);
    if (use == StoreUse::None || quantity < 1)
        return 0;
    const auto& e = authored_.economy;
    const std::int64_t markup = std::clamp(use == StoreUse::Make ? e.storeMarkup : e.resaleMarkup, 0, 1000);
    if (markup == 0)
        return 0;                                   // Free: the goods move, no money does.
    const std::int64_t scaled = catalogPrice(item) * std::int64_t(std::min(quantity, StoreDrawMost)) * markup;
    // The season's price for a good of a season group (doc 68, Progress, "Seasons"): dearer out of season; a farm's good of
    // an area that sells from store at a price a year (doc 71, 4: the granary), that price.
    const double year = farmPriceFactor(item);
    if (const double f = year > 0 ? year : priceFactor(item); f != 1)
        return std::max<std::int64_t>(1, std::int64_t(std::ceil(double(scaled) * f / 100. - 1e-9)));
    return std::max<std::int64_t>(1, (scaled + 99) / 100);
}

EconomyResult Society::drawFromStore(const std::string& keeper, const std::string& item, int quantity, const std::string& why)
{
    if (const auto closed = storeClosed(keeper); !closed.empty())
        return {false, "The store room is closed: " + closed + "."};
    const auto use = storeUse(keeper, item);
    if (use == StoreUse::None)
        return {false, "The store room holds no " + item + " for this business."};
    if (quantity < 1 || quantity > StoreDrawMost)
        return {false, "Draw a whole quantity from 1 to " + std::to_string(StoreDrawMost) + "."};
    // A good of an area the farms have taken over (doc 71, 4): theirs, never the Country's, as much as their stores hold.
    const int area = farmAreaOf(item);
    if (area < 0 && authored_.economy.countryOff.count(items::baseOf(item)))
        return {false, "The Country no longer supplies " + item + ": the city's own producers do."};
    if (area >= 0)
    {
        if (const int held = farmsHold(item); quantity > held)
        {
            ++loopToday_.areasOut[items::farms().areas[std::size_t(area)].id];
            ++loopToday_.goodsOut[items::baseOf(item)];
            return {false, "The farms have " + (held ? "only " + std::to_string(held) : std::string("no")) + " " + item +
                               " in store now."};
        }
    }
    // The season's supply (doc 68, Progress, "Seasons"): so much of a group a day, shared by every shop.
    else if (const int left = countryLeft(item); quantity > left)
        return {false, "The Country has " + (left ? "only " + std::to_string(left) : std::string("no more")) + " " +
                           seasonGroup(item)->name + " to send today (the season)."};
    // Exactly what shift and create will check, before either: a draw is whole or nothing. Its cost is split between the
    // Country (the market that supplies the good) and the treasury (the gate toll, doc 68, 5); carriage on its weight goes
    // to the carters (doc 68, 6).
    const auto till = tillOf(keeper);
    const auto& shelves = *account(till);
    const auto cost = storeCost(keeper, item, quantity);
    const auto toll = gateToll(till, cost, false);
    const auto unit = catalogPrice(item);
    const auto country = marketFund(marketFor(item));
    const auto carter = carters();
    // Carriage in (the carters carry their own free), less the treasury's share of it (doc 68, 5).
    const auto [carriage, subsidy] = carter == till ? std::pair<std::int64_t, std::int64_t>{0, 0} : carriageSplit(till, item, quantity, false);
    if (!itemValid(item))
        return {false, "No such goods: " + item + "."};
    if (stock(shelves, item) > StockCap - quantity)
        return {false, "The till can't hold that many " + item + ".", unit, cost};
    if (!shelves.stock.count(item) && shelves.stock.size() >= MaxGoodsKinds)
        return {false, "The till holds as many kinds of goods as it may.", unit, cost};
    if (shelves.cash < cost + carriage)
        return {false, "The till can't pay " + std::to_string(cost + carriage) + "p for it.", unit, cost};
    const auto fits = [&](const std::string& to, std::int64_t coins) {
        const auto* a = account(to);
        return coins == 0 || (a && a->cash <= MoneyCap - coins);
    };
    // The farms' goods, spread over the farms holding them by their stock (the largest remainders the odd pieces), and the
    // draw's cost less the toll to their tills by their pieces.
    std::vector<std::pair<FarmNow*, int>> lots;
    std::vector<std::int64_t> takings;
    std::vector<double> carries;
    if (area >= 0)
    {
        std::vector<std::pair<FarmNow*, int>> holders;
        int held = 0;
        for (auto& f : farms_)
            if (const auto* a = farmSells(f, std::size_t(area), item) ? account(f.till) : nullptr)
                if (const int n = stock(*a, item); n > 0)
                {
                    holders.push_back({&f, n});
                    held += n;
                }
        std::vector<std::pair<double, std::size_t>> rest;
        int given = 0;
        for (std::size_t i = 0; i < holders.size(); ++i)
        {
            const double exact = double(quantity) * holders[i].second / held;
            const int n = std::min(holders[i].second, int(std::floor(exact)));
            lots.push_back({holders[i].first, n});
            given += n;
            rest.push_back({exact - n, i});
        }
        std::stable_sort(rest.begin(), rest.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
        for (std::size_t k = 0; given < quantity && !rest.empty(); k = (k + 1) % rest.size())
            if (auto& lot = lots[rest[k].second]; lot.second < holders[rest[k].second].second)
            {
                ++lot.second;
                ++given;
            }
        // The pennies by their pieces: each farm's exact share with the fraction of a penny its earlier draws left it, in
        // whole pennies, the odd ones to the largest fractions, and what is over carried to its next draw. (Cut in the
        // list's order, the first farm lost the odd penny of nearly every small draw: about a third of its takings.)
        const auto due = cost - toll;
        std::vector<double> exact;
        std::int64_t sum = 0;
        for (const auto& [farm, n] : lots)
        {
            exact.push_back(n > 0 ? double(due) * n / quantity + farm->takingsCarry : 0);
            takings.push_back(n > 0 ? std::max<std::int64_t>(0, std::int64_t(std::floor(exact.back() + 1e-9))) : 0);
            sum += takings.back();
        }
        while (sum != due)
        {
            std::size_t pick = lots.size();
            for (std::size_t i = 0; i < lots.size(); ++i)
                if (lots[i].second > 0 && (sum < due || takings[i] > 0) &&
                    (pick == lots.size() || (sum < due ? exact[i] - takings[i] > exact[pick] - takings[pick]
                                                       : exact[i] - takings[i] < exact[pick] - takings[pick])))
                    pick = i;
            if (pick == lots.size())
                break;                              // (Not reached: some farm sold a piece of every draw.)
            takings[pick] += sum < due ? 1 : -1;
            sum += sum < due ? 1 : -1;
        }
        for (std::size_t i = 0; i < lots.size(); ++i)
        {
            carries.push_back(lots[i].second > 0 ? exact[i] - double(takings[i]) : lots[i].first->takingsCarry);
            if (!fits(lots[i].first->till, takings[i]))
                return {false, "A farm's till can hold no more money.", unit, cost};
        }
    }
    else if (cost - toll > 0 && !account(country))
        return {false, "The Country's purse is missing.", unit, cost};
    if (carriage + subsidy > 0 && carter == CarriageFund && !account(CarriageFund))
        openAccount(CarriageFund);
    if ((area < 0 && !fits(country, cost - toll)) || !fits("treasury", toll) || !fits(carter, carriage + subsidy))
        return {false, "The Country, the treasury or the carters can hold no more money.", unit, cost};
    const auto pay = [&](const std::string& to, std::int64_t coins, const std::string& kind) { return coins == 0 || shift(till, to, "", 0, coins, kind); };
    bool paid = true;
    if (area >= 0)
        for (std::size_t i = 0; i < lots.size() && paid; ++i)
            paid = pay(lots[i].first->till, takings[i], why);
    else
        paid = pay(country, cost - toll, why);
    if (!paid || !pay("treasury", toll, "gate toll: " + why) || !pay(carter, carriage, "carriage in"))
        return {false, "The store room could not be paid.", unit, cost};   // (Not reached: every check was made above.)
    // The goods: made in the till from the Country's store room, or moved from the farms' stores (in lots of 99, as the
    // ledger keeps them).
    const auto deliver = [&]() {
        if (area < 0)
            return create(till, item, quantity, why);
        for (const auto& [farm, n] : lots)
            for (int left = n; left > 0; left -= 99)
                if (!shift(farm->till, till, item, std::min(left, 99), 0, why))
                    return false;
        return true;
    };
    if (failStoreCreate_ || !deliver())
    {
        failStoreCreate_ = false;
        // (Paid back: nothing changed.)
        if (area >= 0)
            for (std::size_t i = 0; i < lots.size(); ++i)
            {
                if (takings[i] > 0)
                    shift(lots[i].first->till, till, "", 0, takings[i], "refunded: " + why);
            }
        else if (cost - toll > 0)
            shift(country, till, "", 0, cost - toll, "refunded: " + why);
        if (toll > 0)
            shift("treasury", till, "", 0, toll, "refunded: gate toll");
        if (carriage > 0)
            shift(carter, till, "", 0, carriage, "refunded: carriage");
        return {false, "The goods could not be put in the till; the money was paid back.", unit, cost};
    }
    gateToll(till, cost, true);
    if (carter != till)
        carriageSplit(till, item, quantity, true);
    if (subsidy > 0 && shift("treasury", carter, "", 0, subsidy, "carriage subsidy"))
        loopToday_.subsidy += subsidy;
    drawnToday_[till] += cost + carriage;
    if (area < 0)
        loopToday_.countrySales += cost - toll;     // (The Country's own goods, delivered: the sea trade's, doc 72's report.)
    if (const auto* group = seasonGroup(item))
    {
        if (area < 0)
            seasonDrawn_[group->id] += quantity;    // (The season's cap is the Country's: the farms' goods are their stock's.)
        loopToday_.groups[group->id].drawn += quantity;
    }
    for (std::size_t i = 0; i < lots.size(); ++i)
    {
        auto& day = loopToday_.farms[lots[i].first->def->id];
        day.sold += lots[i].second;
        day.goods[item].sold += lots[i].second;
        loopToday_.goodsSold[items::baseOf(item)] += lots[i].second;
        day.takings += takings[i];
        lots[i].first->rentTakings += takings[i];   // (Its land rent's base tonight: doc 72.)
        lots[i].first->takingsCarry = carries[i];
    }
    loopToday_.drawn += cost;
    loopToday_.toll += toll;
    loopToday_.carriage += carriage + subsidy;
    return {true, "Drew " + std::to_string(quantity) + " " + itemName(item) + (area >= 0 ? " from the farms" : " from the store room") + " for " + std::to_string(cost) +
                      "p (" + (use == StoreUse::Make ? "making" : "resale") + (carriage ? ", " + std::to_string(carriage) + "p carriage" : "") + ").",
            unit, cost};
}

int Society::startingStock(const std::string& ware)
{
    // A dear good keeps fewer (country.json `shelves`: theriac, one; doc 68, Progress, "Trades").
    if (const auto& shelves = items::country().shelves; !shelves.empty())
        if (const auto found = shelves.find(items::baseOf(ware)); found != shelves.end())
            return found->second;
    return ware == "sword" ? SmithSwords : GoodsKept;
}

// restockFromStore: RatwCityLoop.cpp (doc 68, 7).

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
