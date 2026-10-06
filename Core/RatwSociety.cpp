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
    forgetOrchestra();
    prices_.clear();
    wages_.clear();
    wageCarry_.clear();
    margin_ = .55;
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
    reset(Roster::Authored);
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
    thread_local std::unordered_map<std::string, bool> known;   // By work label: asked of every shop, every decision.
    if (const auto found = known.find(r->workLabel); found != known.end())
        return found->second;
    std::string work = r->workLabel;
    std::transform(work.begin(), work.end(), work.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return known.emplace(r->workLabel, work.find("smith") != std::string::npos || work.find("forge") != std::string::npos).first->second;
}
bool Society::foodShop(const std::string& merchant) const
{
    static const std::vector<std::string> food = {"general", "provisioner", "stall", "bakery", "butcher", "fishmonger", "brewery", "inn"};
    const auto* r = roster_ == Roster::Demo ? nullptr : spec(merchant);
    const auto* business = r ? items::businessFor(r->workLabel) : nullptr;
    return business && std::find(food.begin(), food.end(), business->id) != food.end();
}

bool Society::sellsFood(const std::string& merchant, const std::string& item, const std::vector<std::string>& sold) const
{
    if (!edible(item))
        return false;
    if (std::find(sold.begin(), sold.end(), items::baseOf(item)) != sold.end())
        return true;
    const auto* good = items::good(item);
    return good && !good->drink && foodShop(merchant);
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
    noteOutgoing(from, kind, coins);                   // A collector's usual spending, for its reserve (RatwSurplus.cpp).
    noteIncoming(to, kind, coins);
    noteForOrchestra(kind, from, to, coins);           // What the economy orchestrator counts (RatwOrchestrate.cpp).
    // Money that wasn't earned or spent stays out of the month's profit (doc 42): an estate, a Dungeon Master's gift.
    if (coins > 0 && (kind == "inheritance" || kind == "operator transfer" || kind == "the shop's till" || kind == "sale of a business" ||
                      kind == "starting money" || kind == "a child's first pennies" || kind == "a child's stipend" ||
                      kind == "the household purse" || kind == "the church's dole" || kind == "the church's share"))
    {
        auto& books = state_.books;
        if (books.start.count(from))
            books.unearned[from] -= coins;
        if (books.start.count(to))
            books.unearned[to] += coins;
        ++books.revision;
    }
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
int Society::stockingDays(const std::string& homeCell)
{
    return 2 + int(std::hash<std::string>{}(homeCell + "|stocking") % 4);   // (2 to 5 days: the user, 2026-10-06.)
}

int Society::shutAhead(std::int64_t today)
{
    // The days from tomorrow when the shops aren't properly open: Restday (open only after the service) and a festival
    // (work stops at noon). A household lays in to last over them.
    int n = 0;
    for (std::int64_t d = today + 1; n < 3; ++d, ++n)
        if (calendar::weekdayOf(double(d) + .5) != calendar::Restday && !calendar::festivalDay(double(d) + .5))
            break;
    return n;
}

int Society::startingLarderDays(const std::string& homeCell)
{
    return 2 + int(std::hash<std::string>{}(homeCell + "|larder") % 4);
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
    created.cash = std::min<std::int64_t>(20, reserve.cash);
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
    if (seller == buyer || quantity < 1 || quantity > 99 || !itemValid(item) || price < 1 || price > 1000)
        return false;
    const auto* from = account(seller);
    const auto* to = account(buyer);
    const auto total = price * quantity;
    if (!from || !to || stock(*from, item) < quantity || to->cash < total || from->cash > MoneyLimit - total ||
        stock(*to, item) > StockLimit - quantity || (!to->stock.count(item) && to->stock.size() >= MaxGoodsKinds))
        return false;
    auto& s = state_.accounts.at(seller);
    auto& b = state_.accounts.at(buyer);
    if ((s.stock[item] -= quantity) == 0 && item != "herbs" && item != "meal" && item != "sword")
        s.stock.erase(item);                                // (Room for other kinds; the old three keep their place.)
    b.stock[item] += quantity;
    b.cash -= total;
    s.cash += total;
    // A staple held under its price (price support, doc 46, Phase 6): its till is owed the gap on what it sells.
    if (!support_.empty() && seller.rfind("till:", 0) == 0 && buyer.rfind("till:", 0) != 0 && buyer.rfind("fund:", 0) != 0)
        if (const auto town = shopTown_.find(seller); town != shopTown_.end())
            if (const auto t = support_.find(town->second); t != support_.end())
                if (const auto gap = t->second.find(items::baseOf(item)); gap != t->second.end())
                    supportOwed_[seller] += gap->second * quantity;
    if (seller.rfind("till:", 0) == 0 || merchant(seller))
    {
        takings_[seller] += total;                  // (A shop's day's takings: its help share them.)
        soldToday_[seller][items::baseOf(item)] += quantity;
    }
    record(kind, buyer, seller, item, quantity, total);
    return true;
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
    const auto& m = *account(tillOf(seller));          // The shop's shelves and money (its house's till, doc 42).
    const auto& p = *account(player);
    const auto* worn = items::wearable(item);
    // Anything else of the catalog (Docs/Design/39): its own price, a few kept.
    const auto* good = item == "meal" || item == "herbs" || item == "sword" ? nullptr : items::good(item);
    // A supplier's or buyer's goods (crafts.json `supplies` and `buys`), a store's worth; anything else, a few.
    const auto* business = spec(seller) ? items::businessFor(spec(seller)->workLabel) : nullptr;
    const auto dealsIn = [&](const std::vector<std::string>& list) { return std::find(list.begin(), list.end(), items::baseOf(item)) != list.end(); };
    const bool stocked = business && (dealsIn(items::suppliesFor(business->id)) || dealsIn(items::buysFor(business->id)));
    const int held = stock(m, item), cap = stocked ? SuppliesKept : worn ? 4 : good ? GoodsKept : item == "meal" ? 24 : item == "sword" ? 4 : 20;
    // A good of the catalog at its town's price (doc 46, Phase 3), and bought from a player at the town's buying price
    // (its price times the orchestrator's margin); meals, herbs and swords as before: dearer as the trader runs short of
    // them, and as the town's stores do (priceFactor).
    const bool catalog = worn || good;
    const double each = catalog ? townPrice(shopTown(seller), item)
                                : (item == "meal" ? 6 : item == "sword" ? swordPrice() : 2) * (held < cap / 4 ? 1.5 : held > cap * 3 / 4 ? .85 : 1.) *
                                      priceFactor(seller, item);
    const double sells = catalog ? margin() : .55;
    // Market stalls sell a little cheaper (Phase 9): a tenth off, rounded down.
    const std::int64_t price = buy && atStall(seller) ? std::max<std::int64_t>(1, std::int64_t(std::floor(each * .9)))
                               : buy ? std::max<std::int64_t>(1, std::int64_t(std::ceil(each - 1e-9)))
                                   : std::max<std::int64_t>(1, std::int64_t(std::floor(each * sells)));
    const std::int64_t total = price * quantity;
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
    const auto till = tillOf(trader);
    result.ok = transfer(buy ? till : player, buy ? player : till, item, quantity, result.unitPrice, "local trade");
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
        bool fresh = false;
        for (const char* kind : StoreKinds)
            if (openAccount(homeStore(home, kind)) && std::string(kind) == "larder")
                fresh = true;
        if (!fresh)
            continue;                               // Stocked when first opened; after that, by the household.
        const int n = int(members.size());
        const auto stockUp = [&](const std::string& store, const char* item, int quantity) {
            for (; quantity > 0; quantity -= 99)            // (A ledger entry is at most 99 of anything.)
                create(store, item, std::min(quantity, 99), "household provisions");
        };
        stockUp(homeStore(home, "larder"), "meal", startingLarderDays(home) * n);
        stockUp(homeStore(home, "chest"), "herbs", ChestHerbsEach * n);
        for (const auto& id : members)
            if (const auto* a = account(id); a && stock(*a, "meal") == 0)
                create(id, "meal", 1, "household provisions");
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
    return anySeen || roster_ != Roster::Authored || (secondsDecided_ + 1) % UnseenStep == 0 ||
           std::int64_t(std::floor(absoluteDay)) > state_.budgetDay || state_.memory.purses < 1 || state_.books.month < 0 ||
           wantsSnapshot(absoluteDay);                 // (The orchestrator's snapshot: everyone, doc 46.)
}

void Society::decide(double absoluteDay, int season, const std::map<std::string, LifeBody>& bodies)
{
    season_ = season;
    unseenDecided_ = ++secondsDecided_ % UnseenStep == 0;
    const auto day = std::int64_t(std::floor(absoluteDay));
    orchestrate(absoluteDay, bodies);                  // The economy orchestrator's brief and snapshot (doc 46).
    if (day > state_.budgetDay)
    {
        // Missed days do not accumulate unbounded grants, orders or harvests.
        state_.budgetDay = day;
        state_.exportsRemaining = 8;
        state_.importsRemaining = 4;
        for (auto& resident : state_.residents)
            resident.second.wagesToday = 0;
        offered_.clear();                              // (The Restday plate, doc 42.)
        if (roster_ == Roster::Authored)
            spoil(day);                                // Food older than it keeps goes bad (RatwDemand.cpp).
        const int recovery[] = {20, 30, 12, 4};
        state_.herbPatch = std::min(60, state_.herbPatch + recovery[season]);
        if (roster_ == Roster::Authored)
        {
            // A daily carter refills the town stores with goods, never money,
            // up to the authored starting store levels.
            const auto& e = authored_.economy;
            auto& stores = state_.accounts.at("treasury");
            stores.stock["meal"] =
                std::max(stock(stores, "meal"), std::min(e.storeMeals, stock(stores, "meal") + e.dailyMeals));
            stores.stock["herbs"] =
                std::max(stock(stores, "herbs"), std::min(e.storeHerbs, stock(stores, "herbs") + e.dailyHerbs));
            record("carter delivery", "outside", "treasury", "", 0, 0);
            reckon(day);                               // The month's tax and tithes, when one is due (RatwReckoning.cpp).
            tendHouses(day);                           // Great houses' businesses: wages, takings, props (RatwHouses.cpp).
            tendTills(day);                            // Owner-run businesses' tills: their keepers' wages (RatwTills.cpp).
            spendSurpluses(day);                       // What the treasuries, churches and houses hold above need goes back out.
            householdShopping(day, season, bodies);    // Each household's errands for the day (RatwDemand.cpp).
            tendHouseholds(day, bodies);               // The household purse, and who keeps the house (RatwHouseholds.cpp).
            childrenAndStipends(day, bodies);          // The children's stipends, and what they spend them on.
            wants(day, bodies);                        // And what the grown spend on things they just want.
            townBuyers(day, bodies);                   // And the town's own buyers'.
            tradeUpkeep(day, bodies);                  // Tools worn out, horses fed, beggars given a penny (doc 42).
            producersSell(day);                        // What the land gave, to the town's food shops.
            worldMoney(bodies);                        // The world's money made up, once (RatwFounding.cpp).
        }
    }
    if (roster_ == Roster::Authored && state_.memory.tills < TillsFounded)
        foundTills();                                  // Every business its own till, once (RatwTills.cpp, doc 46).
    if (roster_ == Roster::Authored && state_.memory.purses < 1)
        foundPurses(bodies);                           // Everyone's starting money, once (RatwFounding.cpp).
    if (roster_ == Roster::Authored && state_.books.month < 0)
        reckon(day);                                   // The first month's books open at once (doc 42).
    if (roster_ == Roster::Authored)
        return decideAuthored(absoluteDay, bodies);
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
            life.task = "companion";
            life.reason = "Ordinary work is suspended while recruited.";
            life.progress = 0;
            life.goalCell.clear();
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
    if (roster == Roster::Authored && authored_.residents.empty())
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
        s.herbPatch < 0 || s.herbPatch > 60 || !validNumber(s.decisionRemainder, 0, 1) || s.craftingStocked < 0 ||
        s.craftingStocked > 100)
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
            !validNumber(l.second.goalX, 0, 256) || !validNumber(l.second.goalY, 0, 256) || l.second.task.size() > 40 ||
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
    forgetOrchestra();
    applyPrices();
    roster_ = roster;
    for (auto& life : state_.residents)
        if (roster_ == Roster::Demo) defaultHome(life.first, life.second);
    if (roster_ == Roster::Authored)
    {
        if (state_.careers.positions.empty())
            defaultCareers();                       // A save from before careers: everyone holds their own job.
        else
            reconcileCareers();
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
} // namespace ratw
