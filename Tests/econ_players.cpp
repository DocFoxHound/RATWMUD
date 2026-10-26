// Players who push (Docs/Design/64-economy-migration.md, Phase 13). See econ_players.h.
#include "econ_players.h"

#include "RatwCalendar.h"
#include "RatwCamps.h"
#include "RatwItems.h"
#include "RatwTogether.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <sstream>

namespace ratw::econ
{
namespace
{
// A shell command's first line of output ("" for none).
std::string firstLine(const std::string& command)
{
    std::string out;
    if (FILE* p = popen(command.c_str(), "r"))
    {
        char line[256];
        if (std::fgets(line, sizeof line, p))
            out = line;
        pclose(p);
    }
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r'))
        out.pop_back();
    return out;
}
double lot(const std::string& words)
{
    return double(std::hash<std::string>{}(words) % 100000) / 100000;
}
std::string quoted(const std::string& s)
{
    std::string out = "\"";
    for (const char c : s)
        if (c == '"' || c == '\\')
            out += std::string("\\") + c;
        else if (static_cast<unsigned char>(c) >= 0x20)
            out += c;
    return out + "\"";
}
double apart(Vec2 a, Vec2 b)
{
    return std::hypot(a.x - b.x, a.y - b.y);
}
// Every coin a player has is the capital's treasury's, moved by the ledger ("test:" money: nobody's earnings).
const char* Funding = "test: a player's money, from the capital's treasury";
constexpr int Quarters = 96;                       // Quarters of an hour in a day: when the players act.
constexpr int PackKinds = 56;                      // A pack this full of kinds goes to a chest (MaxGoodsKinds 64).
constexpr int Lot = 99;                            // The most moved at once (Society::trade's; the journal's too).
constexpr double Reach = 1.5;                      // Tiles: beside its mark (World::steal reaches 1.6).
constexpr double LookEvery = .5, AimEvery = 2;     // World seconds between a body's looks about it, and its new aims.
constexpr double QuarterSeconds = calendar::SecondsPerDay / Quarters;   // A quarter of an hour, in world seconds.
const std::vector<std::string> None;
} // namespace

std::string sourceDrift(const std::string& source, const std::string& commit, const std::string& code, const std::string& paths)
{
    // The changes from the commit it was built from to the code it builds from, as the build worked them out (a docs
    // edit, or a docs-only commit since, changes nothing). Built from no git checkout, there is nothing to set it by.
    if (commit.empty() || code.empty() || commit == "(unknown)" || code == "(unknown)")
        return "source: not a git checkout when it was built, so not checked";
    const std::string git = "git -C '" + source + "' ";
    const auto now = firstLine(git + "rev-parse -q --verify '" + commit + "^{commit}' >/dev/null 2>&1 && " + git +
                               "diff --no-color --no-ext-diff --full-index '" + commit + "' -- " + paths +
                               " 2>/dev/null | sha1sum | cut -c1-12");
    if (now.empty())
        return "source: no git here, or no commit " + commit.substr(0, 12) + ", so not checked";
    if (now != code)
        return "WARNING: the code this binary was built from (" + paths + " in " + source + ") has changed since: rebuild it";
    return {};
}

std::string dataDir(const std::string& source)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    if (const char* dir = std::getenv("RATW_DATA_DIR"); dir && *dir)
        return fs::absolute(dir, ec).lexically_normal().string();
    for (auto at = fs::current_path(ec); !ec && !at.empty(); at = at.parent_path())
    {
        if (fs::exists(at / "Data" / "Items" / "items.json", ec))
            return (at / "Data").string();
        if (at == at.parent_path())
            break;
    }
    return (fs::path(source) / "Data").string();
}

std::string dataState(const std::string& dir)
{
    // Its checkout's describe (dirty or not, the whole tree's), and Data's own changes since its commit: none, or the
    // SHA-1 of their diff and of its untracked files (their contents with their names), so two runs either side of a
    // Data edit read apart.
    const std::string vc = "git -C '" + dir + "' ";
    if (firstLine(vc + "rev-parse --show-toplevel 2>/dev/null").empty())
        return "data: " + dir + " (in no git checkout, so its state isn't known)";
    const auto describe = firstLine(vc + "describe --always --dirty --abbrev=12 2>/dev/null");
    const bool changed = !firstLine(vc + "status --porcelain --untracked-files=all -- . 2>/dev/null").empty();
    std::string mine = "none";
    if (changed)
        mine = firstLine("{ " + vc + "diff --no-color --no-ext-diff --full-index HEAD -- . 2>/dev/null; cd '" + dir + "' && " + vc +
                         "ls-files -z --others --exclude-standard -- . 2>/dev/null | xargs -0 -r sha1sum; } | sha1sum | cut -c1-12");
    return "data: " + dir + " at " + (describe.empty() ? std::string("(no describe)") : describe) + "; its own changes since: " + mine;
}

std::map<std::int64_t, double> readPace(const std::string& text)
{
    // A run's pace.csv: its header, then day, when, the society's day and its pace. Any line that isn't one, or a pace
    // out of 0 to 1, and none is read: a held run on half a file would be no comparison.
    std::map<std::int64_t, double> byDay;
    std::istringstream in(text);
    std::string line;
    if (!std::getline(in, line) || line != "day,when,society_day,pace")
        return {};
    while (std::getline(in, line))
    {
        std::vector<std::string> cells;
        for (std::stringstream row(line); row.good();)
        {
            std::string cell;
            std::getline(row, cell, ',');
            cells.push_back(cell);
        }
        char* end = nullptr;
        const auto day = cells.size() == 4 ? std::strtoll(cells[2].c_str(), &end, 10) : -1;
        if (cells.size() != 4 || cells[2].empty() || *end || day < 0)
            return {};
        const double pace = std::strtod(cells[3].c_str(), &end);
        if (cells[3].empty() || *end || !std::isfinite(pace) || pace < 0 || pace > 1)
            return {};
        byDay[day] = pace;
    }
    return byDay;
}

const std::vector<std::string>& Players::names()
{
    static const std::vector<std::string> all{"hoard", "corner", "dump",  "whale",  "poach",  "kill", "theft",
                                              "visit", "winter", "flood", "strike", "buyout", "drain", "gift"};
    return all;
}

bool Players::stapleKnown(const std::string& item)
{
    return items::good(item) && !Society::storePriced(items::baseOf(item));
}

std::string Players::benchOf(const std::string& body) const
{
    const auto found = bench_.find(body);
    return found == bench_.end() ? std::string() : found->second;
}

bool Players::bodiesNamed(const std::string& name, Bodies& out)
{
    if (name == "walk" || name == "still" || name == "jump")
    {
        out = name == "walk" ? Bodies::Walk : name == "still" ? Bodies::Still : Bodies::Jump;
        return true;
    }
    return false;
}

Players::Players(World& world, std::vector<std::string> scenarios, std::int64_t fromDay, std::int64_t days, Bodies bodies)
    : world_(world), scenarios_(std::move(scenarios)), from_(fromDay), days_(days), bodies_(bodies)
{
}

const std::vector<std::string>& Players::playersOf(const std::string& scenario) const
{
    const auto found = who_.find(scenario);
    return found == who_.end() ? None : found->second;
}

const std::vector<std::string>& Players::accountsOf(const std::string& scenario) const
{
    const auto found = holds_.find(scenario);
    return found == holds_.end() ? None : found->second;
}

const std::vector<std::string>& Players::staffOf(const std::string& scenario) const
{
    const auto found = staff_.find(scenario);
    return found == staff_.end() ? None : found->second;
}

std::string Players::markOf(const std::string& body) const
{
    const auto found = walkers_.find(body);
    return found == walkers_.end() ? std::string() : found->second.mark;
}

void Players::step()
{
    if (!leaving_.empty())
        leave();
    if (started_ && !ended_ && bodies_ != Bodies::Jump)
        lookAbout();
    if (started_ && !ended_ && !bench_.empty())
        drainLook();
    const auto quarter = std::int64_t(std::floor(world_.calendarDays() * Quarters));
    if (quarter == quarter_)
        return;
    quarter_ = quarter;
    const auto day = quarter / Quarters;
    if (firstDay_ < 0)
    {
        // Their days count from when they start: a run that starts after --players-from says so.
        firstDay_ = day;
        if (day > from_)
            std::cerr << "players: the run starts on day " << day << ", after their day " << from_ << ": they start on day " << day
                      << (days_ > 0 ? " and stay " + std::to_string(days_) + " days from then" : std::string()) << "\n";
    }
    if (ended_ || day < from_)
        return;
    if (started_ && days_ > 0 && day >= startDay_ + days_)
    {
        end();
        return;
    }
    refreshShops(day);
    if (!started_)
        start(day);
    for (const auto& s : scenarios_)
        act(s, day, int(quarter % Quarters));
}

void Players::refreshShops(std::int64_t day, bool again)
{
    // Every shop whose keeper is living, by the town it works in (once a day, or again after a death: a keeper who dies
    // has a successor).
    if (day == shopsDay_ && !again)
        return;
    shopsDay_ = day;
    shops_.clear();
    const auto& society = world_.society();
    for (const auto& p : society.positions())
        if (p.role == "merchant")
            if (const auto held = society.state().careers.positions.find(p.id); held != society.state().careers.positions.end())
                if (const auto* e = world_.entity(held->second.holder); e && !e->dead)
                    shops_.push_back({held->second.holder, p.id, world_.communityOf(p.work.cell)});
}

bool Players::open(const Shop& shop) const
{
    // (A keeper who died since the list was made, or left its post, keeps no shop: its purse isn't one.)
    const auto* e = world_.entity(shop.keeper);
    const auto& posts = world_.society().state().careers.positions;
    const auto held = posts.find(shop.post);
    return e && !e->dead && held != posts.end() && held->second.holder == shop.keeper;
}

std::string Players::join(const std::string& scenario, int n)
{
    // As a new character arrives (Society::addPlayer: the treasury's welcome grant, a few meals).
    const auto id = "player-" + scenario + "-" + std::to_string(n);
    world_.society().addPlayer(id);
    who_[scenario].push_back(id);
    note(scenario, id, "joined", "", 0, world_.society().account(id) ? world_.society().account(id)->cash : 0);
    return id;
}

std::int64_t Players::fund(const std::string& scenario, const std::string& to, std::int64_t coins)
{
    auto& society = world_.society();
    const auto* treasury = society.account("treasury");
    coins = std::min(coins, treasury ? treasury->cash : 0);
    if (coins <= 0 || !society.shift("treasury", to, "", 0, coins, Funding))
        return 0;
    note(scenario, to, "funded", "", 0, coins);
    return coins;
}

std::int64_t Players::poachWage() const
{
    // Twice what a Hold pays (Game::keepWage: the realm's going rate, camp::HoldWage at least), read each day.
    return 2 * std::max<std::int64_t>(camp::HoldWage, std::int64_t(std::ceil(world_.society().labourDay("") - 1e-9)));
}

std::int64_t Players::companionWage(const std::string& id) const
{
    // What a hired companion is paid, and keeps once stationed at a Hold (Game::wageFor): 6p a day, 10p for a trade that
    // wanders for a living (RatwGameCompanions.cpp's `roving`, its words), at a 12p day, at its town's day for amounts
    // (Game::atTheDay: doc 64, Phase 13).
    const auto* job = world_.society().jobOf(id);
    const auto* spec = world_.society().spec(id);
    std::string trade = job ? job->title : spec ? spec->workLabel : std::string();
    std::transform(trade.begin(), trade.end(), trade.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    const auto* e = world_.entity(id);
    const auto town = e ? world_.lawTown(e->cellId) : std::string();
    for (const char* word : {"scout", "guide", "hunter", "travel", "pilgrim", "sellsword", "mercenary", "courier", "escort", "tracker"})
        if (trade.find(word) != std::string::npos)
            return world_.society().pennies(10 / 12., town);
    return world_.society().pennies(6 / 12., town);
}

void Players::take(const std::string& s, const std::string& chapter, const std::vector<std::string>& hands)
{
    for (const auto& id : hands)
    {
        world_.entity(id)->leaderId = "camp:econ-" + s;
        world_.stop(id);
        staff_[s].push_back(id);
        const auto* job = world_.society().jobOf(id);
        note(s, chapter, "hired", id + " (" + (job ? job->title : std::string()) + ")", 1, 0);
    }
    keepMesses();
}

void Players::keepMesses()
{
    // Each hand eats from its Hold's stores (Game::feedKeptHands: a stationed hand's mess is its Chapter's stores).
    std::map<std::string, std::string> messes;
    for (const auto& [s, staff] : staff_)
        if (const auto hold = holds_.find(s); hold != holds_.end() && !hold->second.empty())
            for (const auto& id : staff)
                messes[id] = hold->second.front();
    world_.society().setMesses(std::move(messes));
}

void Players::provision(const std::string& s)
{
    // The Hold's stores kept in food (the user, 2026-10-26: a Chapter has to keep food in for its hands): its
    // quartermaster, a player of the Chapter, buys at the capital's food shops what tops the stores up to ProvisionDays
    // of a wolf's day's food (Society::DayNourishment) a hand, and puts it in the stores as the game's "store" does. Its
    // purse is topped up from the capital's treasury first, to twice that food at a meal's price (a stand-in for the
    // Chapter's dues). Only food that keeps to the next top-up counts toward the days (lasting: ProvisionDays or more, or
    // that doesn't spoil), bought first (below, the order); what spoils sooner (porridge, a day) only toward a day's
    // food, when there isn't enough that lasts (doc 64, Phase 13, round 6, the companions' review: two days of porridge,
    // 44% of what was stored rotted in the stores).
    auto& society = world_.society();
    const auto hold = holds_.find(s);
    if (hold == holds_.end() || hold->second.empty() || staffOf(s).empty() || playersOf(s).empty())
        return;
    const auto chapter = hold->second.front();
    const auto qm = playersOf(s).front();
    const auto lasting = [](const std::string& item) {
        const auto* good = items::good(item);
        return good && (good->keeps <= 0 || good->keeps >= ProvisionDays);
    };
    const auto held = [&](bool lasts) {
        int n = 0;
        if (const auto* a = society.account(chapter))
            for (const auto& [item, k] : a->stock)
                if (k > 0 && Society::edible(item) && lasting(item) == lasts)
                    n += k * Society::nourishment(item);
        return n;
    };
    const int day = Society::DayNourishment * int(staffOf(s).size()), want = ProvisionDays * day;
    // (Still wanted, of food that lasts; and of what spoils sooner, a day's at most, and only what the lasting lacks.)
    const auto wanted = [&](bool lasts) { return lasts ? want - held(true) : std::min(day, want - held(true)) - held(false); };
    if (wanted(true) <= 0)
        return;
    const auto capital = capitalNow();
    const std::int64_t purse = std::int64_t(wanted(true) / 50 + 1) * society.pennies(6 / 12., capital) * 2;
    if (const auto* a = society.account(qm); a && a->cash < purse)
        fund(s, qm, purse - a->cash);
    // What the capital's open food shops have: prepared food before what a craft takes (the hands eat it first,
    // Society::messFood, and the raw would wait and spoil), then the longest-keeping, then the cheapest for what it
    // feeds (its catalogue price a point of nourishment); else in the shops' order, as before. (Round 6: the cheapest
    // first bought bread, which keeps four days, and in a store turned over every day it rotted wholesale every sixth
    // day, the stores bare till the next dawn and 21 hands gone home hungry in the month: Society::spoil reckons an
    // account's batches by the day's net, so stock taken in and eaten alike all day never comes in fresh, and its
    // first batch ages on. That is the society's, found here, not mended; the longest-keeping first steers clear of it.)
    struct Ware
    {
        const Shop* shop;
        std::string kind;
        bool raw;
        double cost, keeps;
    };
    std::vector<Ware> wares;
    for (const auto& shop : shops_)
    {
        if (shop.town != capital || !open(shop))
            continue;
        const auto* till = society.account(society.tillOf(shop.keeper));
        if (!till)
            continue;
        for (const auto& base : society.wares(shop.keeper))
            if (Society::edible(base))
                for (const auto& kind : Society::kindsHeld(*till, base))
                    if (const auto* good = items::good(kind))
                        wares.push_back({&shop, kind, items::ingredient(kind), double(good->price) / std::max(1, good->nourish),
                                         good->keeps <= 0 ? 1e9 : good->keeps});
    }
    std::stable_sort(wares.begin(), wares.end(), [](const Ware& a, const Ware& b) {
        return a.raw != b.raw ? b.raw : a.keeps != b.keeps ? a.keeps > b.keeps : a.cost < b.cost;
    });
    for (const bool lasts : {true, false})
        for (const auto& w : wares)
        {
            if (lasting(w.kind) != lasts || wanted(lasts) <= 0)
                continue;
            const int each = std::max(1, Society::nourishment(w.kind));
            const auto shelf = [&] { return Society::stock(*society.account(society.tillOf(w.shop->keeper)), w.kind); };
            for (int k = std::min({Lot, shelf(), (wanted(lasts) + each - 1) / each}); k > 0 && wanted(lasts) > 0;)
            {
                const auto r = society.trade(qm, w.shop->keeper, w.kind, k, true);
                if (!r.ok)
                {
                    k /= 2;                         // (Short of money: fewer.)
                    continue;
                }
                note(s, qm, "bought", w.kind, k, r.total);
                if (society.shift(qm, chapter, w.kind, k, 0, "chapter stores"))
                    note(s, qm, "stored", w.kind, k, 0);
                k = std::min({Lot, shelf(), (wanted(lasts) + each - 1) / each});
            }
        }
}

void Players::keepFed(const std::string& s, std::int64_t day)
{
    // As Game::feedKeptHands: a hand unfed at Society::KeptToldHunger told of (noted, once a day), and at KeptLeaveHunger
    // gone back to its own life.
    auto& society = world_.society();
    const auto hold = holds_.find(s);
    const auto staff = staff_.find(s);
    if (hold == holds_.end() || hold->second.empty() || staff == staff_.end())
        return;
    const auto chapter = hold->second.front();
    const auto before = staff->second.size();
    for (auto it = staff->second.begin(); it != staff->second.end();)
    {
        if (society.unfed(*it, Society::KeptLeaveHunger))
        {
            if (auto* npc = world_.entity(*it))
                npc->leaderId.clear();
            world_.stop(*it);
            note(s, chapter, "left hungry", *it, 1, 0);
            it = staff->second.erase(it);
            continue;
        }
        if (society.unfed(*it, Society::KeptToldHunger))
            if (const auto told = hungerTold_.find(s); told == hungerTold_.end() || told->second != day)
            {
                hungerTold_[s] = day;
                note(s, chapter, "told hungry", *it, 1, 0);
            }
        ++it;
    }
    if (staff->second.size() != before)
        keepMesses();
}

void Players::payStaff(const std::string& s, const std::function<std::int64_t(const std::string&)>& wage)
{
    // Each dawn its hands paid (campTick), the Hold's treasury first topped up to the day's pay at the day's rates (what
    // it can't get from the capital's treasury noted, "short": the pennies lacking as its quantity, none moved); one it
    // can't pay goes home.
    auto& society = world_.society();
    const auto& chapter = holds_[s].front();
    auto& staff = staff_[s];
    std::int64_t need = 0;
    for (const auto& id : staff)
        need += wage(id);
    const auto* purse = society.account(chapter);
    if (const auto lacking = need - (purse ? purse->cash : 0); lacking > 0)
        if (const auto got = fund(s, chapter, lacking); got < lacking)
            note(s, chapter, "short", "pennies lacking", int(std::min<std::int64_t>(lacking - got, 1'000'000'000)), 0);
    for (auto it = staff.begin(); it != staff.end();)
    {
        const auto* e = world_.entity(*it);
        const auto pay = wage(*it);
        if (e && !e->dead && society.shift(chapter, *it, "", 0, pay, "camp wage"))
        {
            note(s, chapter, "paid", *it, 1, pay);
            ++it;
            continue;
        }
        if (auto* npc = world_.entity(*it))
            npc->leaderId.clear();
        note(s, chapter, e && !e->dead ? "left unpaid" : "died", *it, 1, 0);
        it = staff.erase(it);
    }
    keepMesses();
}

void Players::letGo(const std::string& s)
{
    // Back to their own lives (RatwGameCamps.cpp's "dismissstaff"): their schedules walk them home or to work. (strike's
    // month is over, whether at its own dawn or the players' last day: the review's.)
    if (s == "strike" && std::find(scenarios_.begin(), scenarios_.end(), s) != scenarios_.end())
        struck_ = true;
    for (const auto& id : staffOf(s))
        if (auto* npc = world_.entity(id))
        {
            npc->leaderId.clear();
            world_.stop(id);
            note(s, accountsOf(s).empty() ? std::string() : accountsOf(s).front(), "released", id, 1, 0);
        }
    staff_.erase(s);
    keepMesses();
}

void Players::note(const std::string& scenario, const std::string& player, const std::string& action, const std::string& item,
                   int quantity, std::int64_t coins)
{
    moves_.push_back({std::int64_t(std::floor(world_.calendarDays())), player, scenario, action, item, quantity, coins});
    auto& t = today_[scenario][action];
    t.first += quantity;
    t.second += coins;
}

bool Players::trades(const Shop& shop, const std::set<std::string>& towns)
{
    return towns.empty() || towns.count(shop.town) > 0;
}

int Players::buyAll(const std::string& scenario, const std::string& player, const Shop& shop, const std::string& base,
                    std::int64_t* budget)
{
    // Every one the shop has, of every quality, as the game sells them (Society::trade: 99 at most a trade); with a
    // budget, as far as it goes (Society::quote first), and what it paid taken from it.
    auto& society = world_.society();
    if (!open(shop))
        return 0;
    const auto deals = society.wares(shop.keeper);
    const auto till = society.tillOf(shop.keeper);
    if (std::find(deals.begin(), deals.end(), base) == deals.end() || !society.account(till))
        return 0;
    int got = 0;
    for (const auto& kind : Society::kindsHeld(*society.account(till), base))
        for (int k = std::min(Lot, Society::stock(*society.account(till), kind)); k > 0;)
        {
            if (budget)
                if (const auto q = society.quote(player, shop.keeper, kind, k, true); !q.ok || q.total > *budget)
                {
                    k /= 2;                         // (Past what it may spend: fewer.)
                    continue;
                }
            const auto r = society.trade(player, shop.keeper, kind, k, true);
            if (!r.ok)
            {
                k /= 2;                             // (Short of money, or of room: fewer.)
                continue;
            }
            got += k;
            if (budget)
                *budget -= r.total;
            note(scenario, player, "bought", kind, k, r.total);
            k = std::min(Lot, Society::stock(*society.account(till), kind));
        }
    return got;
}

int Players::sellAll(const std::string& scenario, const std::string& player, const std::string& item)
{
    // As many as the shops of its towns take (Society::quote: what a shop wants more of, and can pay for).
    auto& society = world_.society();
    const auto base = items::baseOf(item);
    const auto towns = towns_.find(player);
    const auto held = [&] { return society.account(player) ? Society::stock(*society.account(player), item) : 0; };
    int sold = 0;
    for (const auto& shop : shops_)
    {
        if (held() <= 0)
            break;
        if (!open(shop) || (towns != towns_.end() && !trades(shop, towns->second)))
            continue;
        const auto deals = society.wares(shop.keeper);
        if (std::find(deals.begin(), deals.end(), base) == deals.end())
            continue;
        for (int k = std::min(Lot, held()); k > 0;)
        {
            const auto r = society.trade(player, shop.keeper, item, k, false);
            if (!r.ok)
            {
                k /= 2;
                continue;
            }
            sold += k;
            note(scenario, player, scenario == "theft" ? "fenced" : "sold", item, k, r.total);
            k = std::min(Lot, held());
        }
    }
    return sold;
}

void Players::stow(const std::string& scenario, const std::string& player)
{
    // A pack holds so many kinds of goods (MaxGoodsKinds); past that, they go into chests: accounts of the player's own
    // ("let:" as doc 54's lodgings' are), but free, with no lodging and no rent (a stand-in). In lots of 99 at most, as
    // a pack is unloaded, so the journal (which notes 99 a move at most) has every one.
    auto& society = world_.society();
    const auto* pack = society.account(player);
    if (!pack || int(pack->stock.size()) < PackKinds)
        return;
    std::vector<std::pair<std::string, int>> goods;
    for (const auto& [item, n] : pack->stock)
        if (n > 0 && item != "meal" && item != "herbs")
            goods.push_back({item, n});
    auto& chests = holds_[scenario];
    for (const auto& [item, n] : goods)
        for (int left = n, full = 0; left > 0 && full < 2;)
        {
            const auto* chest = chests.empty() ? nullptr : society.account(chests.back());
            if (!chest || full > 0 || (!chest->stock.count(item) && int(chest->stock.size()) >= PackKinds))
            {
                chests.push_back("let:" + player + "-" + std::to_string(chests.size() + 1));
                society.openAccount(chests.back());
                chest = society.account(chests.back());
            }
            const int before = chest ? Society::stock(*chest, item) : 0;
            const bool put = society.shift(player, chests.back(), item, std::min(Lot, left), 0, "put in a chest");
            const int moved = put ? Society::stock(*society.account(chests.back()), item) - before : 0;   // (A full one takes fewer.)
            if (moved <= 0)
            {
                ++full;                             // (That chest holds all of it that it can: another.)
                continue;
            }
            full = 0;
            note(scenario, player, "stowed", item, moved, 0);
            left -= moved;
        }
}

void Players::start(std::int64_t day)
{
    started_ = true;
    startDay_ = day;
    auto& society = world_.society();
    capital_ = society.capital();
    std::map<std::string, int> shopsIn;
    for (const auto& s : shops_)
        ++shopsIn[s.town];
    if (capital_.empty() && !shopsIn.empty())
        capital_ = std::max_element(shopsIn.begin(), shopsIn.end(), [](const auto& a, const auto& b) { return a.second < b.second; })->first;
    std::vector<std::string> towns;
    for (const auto& [town, n] : shopsIn)
        if (!town.empty())
            towns.push_back(town);
    // Player i of n trades in town i of the towns (and every nth after, when there are fewer players than towns); with
    // no towns, anywhere.
    const auto spread = [&](const std::string& player, int i, int n) {
        auto& mine = towns_[player];
        if (towns.empty())
            return;
        for (int t = 0; t < int(towns.size()); ++t)
            if (t % n == i % n || i % int(towns.size()) == t)
                mine.insert(towns[std::size_t(t)]);
    };
    for (const auto& s : scenarios_)
        if (s == "hoard")
        {
            // Twenty wolves out on the capital's hunting and gathering grounds (any town's, if it has none), the same
            // game and patches its own hunters and gatherers work, selling in those towns' shops. A player roams: each
            // spell at another ground. (A stand-in: what a spell gives is an outworker's, World::harvestAt through
            // Society::day().harvest, a hunt's kill at its odds, a forage without its wait; not a player's World::forage
            // and hunts, which would want twenty bodies in the wild and a fight each.)
            std::set<std::string> where;
            for (const bool anywhere : {false, true})
                if (const auto& all = society.day().grounds; all && grounds_.empty())
                    for (const auto& [town, list] : *all)
                        if (anywhere || town == capital_)
                            for (const auto& g : list)
                                if (g.trade == "hunting" || g.trade == "gathering")
                                {
                                    grounds_.push_back(g);
                                    where.insert(town);
                                }
            for (int i = 1; i <= 20; ++i)
                towns_[join(s, i)] = where;
        }
        else if (s == "corner")
        {
            const auto id = join(s, 1);
            fund(s, id, 30000);
            towns_[id] = {capital_};
        }
        else if (s == "dump" || s == "winter")
        {
            const int n = s == "dump" ? 5 : 10;
            for (int i = 1; i <= n; ++i)
            {
                const auto id = join(s, i);
                spread(id, i - 1, n);
                if (s == "winter")
                    fund(s, id, 3000);
            }
        }
        else if (s == "whale")
        {
            const auto id = join(s, 1);
            fund(s, id, society.moneySupply() / 20);
        }
        else if (s == "poach")
        {
            // A Hold's hands (RatwGameCamps.cpp's "station"): a wolf off its own schedule (World: a leader, as a
            // companion's), paid each dawn from the Chapter's treasury. Twenty of the capital's hands at its businesses.
            const std::string chapter = "chapter:econ-poach";
            society.openAccount(chapter);
            holds_[s].push_back(chapter);
            std::vector<std::pair<double, std::string>> hands;
            for (const auto& [id, life] : society.state().residents)
            {
                const auto* e = world_.entity(id);
                const auto* job = society.jobOf(id);
                if (!e || e->dead || !e->leaderId.empty() || e->age < 16 || e->age >= Society::retireAge(id) || !job || !job->paid ||
                    job->role != "civilian" || (life.role != "civilian" && life.role != "resident") ||
                    world_.communityOf(life.homeCell) != capital_ || society.payerOf(id, *job, e->age).account.rfind("till:", 0) != 0)
                    continue;
                hands.push_back({lot(id + "|poach"), id});
            }
            std::sort(hands.begin(), hands.end());
            std::vector<std::string> taken;
            for (std::size_t i = 0; i < hands.size() && i < 20; ++i)
                taken.push_back(hands[i].second);
            take(s, chapter, taken);
            // The first day's pay put by now; each dawn tops it up to that day's (act). Its quartermaster, and the first
            // days' food in the stores.
            fund(s, chapter, poachWage() * std::int64_t(staffOf(s).size()));
            join(s, 1);
            provision(s);
        }
        else if (s == "strike")
        {
            // A tenth of the capital's hands (wolves of working age paid by an employer: a business, a House, the town
            // or the church; not the keepers, the watch, nor the land's producers, who live by what they bring in),
            // taken from their work for StrikeDays as the game lets a Chapter take a wolf in work: hired as a party's
            // companion (out of its working hours), then stationed at the Hold (RatwGameCamps.cpp's "station"), off
            // its schedule, on its companion's wage. Its post stays its own. (A Hold takes into its own posts only
            // wolves without work. Companions are 4 in the realm at once and one more for each 4 players online, and
            // one stationed frees its place: a tenth of the capital's hands wants a few players over some days, done
            // here at once, whatever the hour: the game would ask each awake and out of its working hours.) Not those
            // Game::whyNotJoin refuses whatever the hour: the Quickened, a wolf beaten down, one the watch wants (a
            // warrant) or holds (custody).
            const std::string chapter = "chapter:econ-strike";
            society.openAccount(chapter);
            holds_[s].push_back(chapter);
            std::vector<std::pair<double, std::string>> hands;
            for (const auto& [id, life] : society.state().residents)
            {
                const auto* e = world_.entity(id);
                const auto* job = society.jobOf(id);
                if (!e || e->dead || e->transient || e->downedLeft > 0 || e->state == "beaten down" || e->quickened ||
                    world_.warrantFor(id) || world_.custodyOf(id) || !e->leaderId.empty() || e->age < 16 ||
                    e->age >= Society::retireAge(id) || !job || !job->paid || job->role != "civilian" ||
                    (life.role != "civilian" && life.role != "resident") || world_.communityOf(life.homeCell) != capital_)
                    continue;
                const auto payer = society.payerOf(id, *job, e->age).account;
                if (payer.empty() || payer == id)
                    continue;
                hands.push_back({lot(id + "|strike"), id});
            }
            std::sort(hands.begin(), hands.end());
            std::vector<std::string> chosen;
            for (std::size_t i = 0; i < hands.size() && i < (hands.size() + 9) / 10; ++i)
            {
                chosen.push_back(hands[i].second);
                jobWas_[hands[i].second] = society.jobOf(hands[i].second)->id;
            }
            take(s, chapter, chosen);
            taken_ = chosen;
            std::int64_t pay = 0;
            for (const auto& id : chosen)
                pay += companionWage(id);
            fund(s, chapter, pay);
            join(s, 1);                             // (Its quartermaster, and the first days' food in the stores.)
            provision(s);
        }
        else if (s == "flood")
        {
            // Ten players who bring in the staple and sell it to the capital's shops (whatever shop buys it); its makers
            // are the posts that bring it in worked in the capital or held by a wolf who lives there (its shops buy from
            // the capital's own first: Society::buyMaterials), watched.
            for (int i = 1; i <= 10; ++i)
                towns_[join(s, i)] = {capital_};
            findMakers();
            flooded_ = true;
        }
        else if (s == "buyout")
        {
            // Five players with big purses, buying in the capital.
            for (int i = 1; i <= 5; ++i)
            {
                const auto id = join(s, i);
                fund(s, id, 30000);
                towns_[id] = {capital_};
            }
        }
        else if (s == "gift")
        {
            // A rich player: a month of its gifts, 100p a day to each of the 50 poorest households (or its days').
            const auto id = join(s, 1);
            fund(s, id, std::int64_t(GiftHouseholds) * GiftEach * (days_ > 0 ? days_ : 30));
        }
        else if (s == "drain")
            drainStart();
        else if (s == "kill")
        {
            // The makers a town can't do without: its only mill's miller, the biggest farm's farmer (by its acres), and
            // every smith of the realm. They die as old age kills (World::setDead); succession fills their posts.
            std::map<std::string, std::vector<const Position*>> mills;
            std::vector<const Position*> dead;
            for (const auto& p : society.positions())
                if (const auto* b = p.role == "merchant" ? items::businessFor(p.title) : nullptr)
                {
                    if (b->id == "mill")
                        mills[world_.communityOf(p.work.cell)].push_back(&p);
                    if (b->id == "smithy")
                        dead.push_back(&p);
                }
            for (const auto& [town, list] : mills)
                if (list.size() == 1)
                    dead.push_back(list.front());
            const std::string* farm = nullptr;
            double acres = 0;
            for (const auto& [till, f] : society.state().memory.fields)
                if (f.acres > acres)
                    acres = f.acres, farm = &till;
            if (farm)
                if (const auto* p = society.position(farm->substr(5)))
                    dead.push_back(p);
            for (const auto* p : dead)
                if (const auto held = society.state().careers.positions.find(p->id); held != society.state().careers.positions.end())
                {
                    const auto holder = held->second.holder;
                    if (!holder.empty() && world_.setDead(holder, true).ok)
                    {
                        posts_.push_back(p->id);
                        note(s, holder, "killed", p->title, 1, 0);
                    }
                }
            refreshShops(day, true);                // (The dead keep no shops today.)
        }
        else if (s == "theft" || s == "visit")
        {
            // Five thieves (or visitors) with bodies in the world (World::steal needs one beside its mark), arriving at
            // the capital's shops, one at each of five.
            std::vector<const Position*> at;
            for (const auto& p : society.positions())
                if (p.role == "merchant" && world_.communityOf(p.work.cell) == capital_)
                    at.push_back(&p);
            for (int i = 1; i <= 5; ++i)
            {
                const auto id = "player-" + s + "-" + std::to_string(i);
                const auto name = (s == "theft" ? "Thief " : "Visitor ") + std::to_string(i);
                if (const auto* p = at.empty() ? nullptr : at[std::size_t(i - 1) % at.size()])
                    world_.addPlayer(id, name, p->work.cell, {p->work.x, p->work.y});
                else
                    world_.addPlayer(id, name);
                who_[s].push_back(id);
                towns_[id] = {capital_};
                note(s, id, "joined", "", 0, society.account(id) ? society.account(id)->cash : 0);
            }
        }
}

bool Players::markable(const std::string& id) const
{
    const auto& society = world_.society();
    const auto* life = society.resident(id);
    const auto* e = life ? world_.entity(id) : nullptr;
    if (!e || e->dead || e->age < 16 || world_.custodyOf(id) || world_.communityOf(life->homeCell) != capital_)
        return false;
    const auto* job = society.jobOf(id);
    return !job || job->role != "guard";
}

void Players::tryPurse(const std::string& s, const std::string& thief, const std::string& mark)
{
    auto& society = world_.society();
    const auto before = society.account(thief) ? *society.account(thief) : EconomyAccount{};
    if (!world_.steal(thief, mark).ok)
        return;
    note(s, thief, "tried", mark, 1, 0);            // (Taken or not: World::steal's own odds and the watch's eyes.)
    const auto* after = society.account(thief);
    if (!after)
        return;
    if (after->cash > before.cash)
        note(s, thief, "stole", "", 0, after->cash - before.cash);
    for (const auto& [item, n] : after->stock)
        if (const int was = Society::stock(before, item); n > was)
        {
            note(s, thief, "stole", item, n - was, 0);
            loot_[thief].insert(item);
        }
}

void Players::setOff(const std::string& s, const std::string& body, std::int64_t day, int quarter)
{
    // A quarter of an hour's purpose: whom to go up to (theft's mark; visit's, to stand beside and take nothing).
    const auto* me = world_.entity(body);
    if (!me || bodies_ == Bodies::Still)
        return;
    const auto pick = lot(body + "|" + std::to_string(day) + "|" + std::to_string(quarter));
    if (bodies_ == Bodies::Jump)
    {
        // Beside a mark anywhere in the capital (World::teleport: a Dungeon Master's move).
        std::vector<std::string> marks;
        for (const auto& [id, life] : world_.society().state().residents)
            if (markable(id))
                marks.push_back(id);
        if (marks.empty())
            return;
        const auto& mark = marks[std::size_t(pick * double(marks.size()))];
        const auto* v = world_.entity(mark);
        if (v && world_.teleport(body, v->cellId, v->position.x, v->position.y).ok && s == "theft")
            tryPurse(s, body, mark);
        return;
    }
    // Walking: someone in the place it is (World::moveTo up to them; lookAbout acts on arrival), not the one it went up
    // to last; on the hour, or with nobody there, on into the next of the capital's places (a seam walked across, a
    // door opened and entered: as a player clicks), so it goes about the capital as the jumping ones do, a step at a time.
    auto& w = walkers_[body];
    if (!w.mark.empty())
        return;                                     // (Still on its way to the last.)
    std::vector<std::string> marks;
    for (const auto* e : world_.entitiesIn(me->cellId))
        if (e->cellId == me->cellId && e->npc && e->id != w.last && markable(e->id))
            marks.push_back(e->id);
    std::sort(marks.begin(), marks.end());
    // Each place it could go next, and the ways in (a door; a seam's every tile, each body taking its own, as players
    // cross a street's width and don't all make for one tile).
    std::map<std::string, std::vector<const Door*>> ways;
    if (marks.empty() || quarter % 4 == 0)
        for (const auto* d : world_.doorsIn(me->cellId))
            if (d->portal && !d->locked && world_.communityOf(d->targetCell) == capital_)
                ways[d->targetCell].push_back(d);
    if (!ways.empty())
    {
        auto next = ways.begin();
        std::advance(next, std::ptrdiff_t(pick * double(ways.size())));
        const auto& in = next->second;
        const auto* d = in[std::size_t(lot(body + "|way|" + std::to_string(day) + "|" + std::to_string(quarter)) * double(in.size()))];
        ++aims_;
        if (world_.moveTo(body, d->position.x, d->position.y).ok)
        {
            w.mark = "door:" + d->id;
            w.aim = d->position;
            w.since = w.aimedAt = world_.time();
        }
        return;
    }
    if (!marks.empty())
    {
        const auto& mark = marks[std::size_t(pick * double(marks.size()))];
        const auto* v = world_.entity(mark);
        ++aims_;
        if (world_.moveTo(body, v->position.x, v->position.y).ok)
        {
            w.mark = w.last = mark;
            w.aim = v->position;
            w.since = w.aimedAt = world_.time();
        }
    }
}

void Players::lookAbout()
{
    // Twice a world second, each body sees how it is getting on: beside its mark, it acts; its mark moved off, it turns
    // after it (every two seconds at most: AimEvery since it last aimed); a quarter of an hour without getting there, it
    // gives up. One standing still tries a purse that comes within reach, once a quarter of an hour, passing over the
    // purses it has tried that day (it had tried the keeper beside it 400 times in ten days: the review's).
    const double now = world_.time();
    if (lookedAt_ >= 0 && now - lookedAt_ < LookEvery)
        return;
    lookedAt_ = now;
    const double calendar = world_.calendarDays();
    const double hour = (calendar - std::floor(calendar)) * 24;
    if (hour < 9 || hour >= 21)
        return;
    const auto quarter = std::int64_t(std::floor(calendar * Quarters));
    for (const char* s : {"theft", "visit"})
        for (const auto& body : playersOf(s))
        {
            const auto* me = world_.entity(body);
            if (!me || world_.custodyOf(body))
                continue;
            auto& w = walkers_[body];
            const bool thief = std::string(s) == "theft";
            if (bodies_ == Bodies::Still)
            {
                if (!thief || w.tried == quarter)
                    continue;
                if (const auto day = quarter / Quarters; w.day != day)
                    w.day = day, w.today.clear();
                std::string mark;
                double nearest = Reach;
                for (const auto* e : world_.entitiesIn(me->cellId))
                    if (const double d = apart(e->position, me->position); e->cellId == me->cellId && e->npc &&
                                                                         (d < nearest || (d == nearest && e->id < mark)) &&
                                                                         !w.today.count(e->id) && markable(e->id))
                        mark = e->id, nearest = d;
                if (!mark.empty())
                {
                    w.tried = quarter;
                    w.today.insert(mark);
                    tryPurse(s, body, mark);
                }
                continue;
            }
            if (w.mark.empty())
                continue;
            if (w.mark.rfind("door:", 0) == 0)
            {
                // On its way out: across a seam it is through by itself; a door it opens and goes in by, once there.
                const auto* d = world_.door(w.mark.substr(5));
                if (!d || d->cellId != me->cellId || now - w.since > QuarterSeconds)
                    w.mark.clear();
                else if (!d->boundary && apart(d->position, me->position) <= d->reach)
                {
                    if (!d->open)
                        world_.interact(body, d->id, "open");
                    world_.interact(body, d->id, "enter");
                    w.mark.clear();
                }
                continue;
            }
            const auto* m = world_.entity(w.mark);
            if (!m || m->cellId != me->cellId || !markable(w.mark) || now - w.since > QuarterSeconds)
            {
                w.mark.clear();
                world_.stop(body);
                continue;
            }
            if (apart(m->position, me->position) <= Reach)
            {
                const auto mark = w.mark;
                w.mark.clear();
                world_.stop(body);
                if (thief && w.tried != quarter)
                {
                    w.tried = quarter;
                    tryPurse(s, body, mark);
                }
                else if (!thief)
                    note(s, body, "came up to", "", 1, 0);
                continue;
            }
            if (apart(m->position, w.aim) > 1 && now - w.aimedAt >= AimEvery)
            {
                ++aims_;
                w.aimedAt = now;                    // (Tried, whether or not a way was found: no sooner again.)
                if (world_.moveTo(body, m->position.x, m->position.y).ok)
                    w.aim = m->position;
            }
        }
}

void Players::act(const std::string& s, std::int64_t day, int quarter)
{
    auto& society = world_.society();
    const int hour = quarter / 4;
    const bool onTheHour = quarter % 4 == 0;
    if (s == "hoard")
    {
        // A spell an hour from 8 to 17, each at the next of the grounds (player i starting at the ith); at 17 what it
        // brought in goes to the shops. It keeps its coin.
        const int season = int(calendar::calendarAt(world_.calendarDays()).season);
        if (onTheHour && hour >= 8 && hour < 17 && society.day().harvest && !grounds_.empty())
            for (std::size_t i = 0; i < who_[s].size(); ++i)
            {
                const auto& id = who_[s][i];
                const auto& g = grounds_[(i * 7 + std::size_t(hour - 8)) % grounds_.size()];
                for (const auto& [item, n] : society.day().harvest(id, g, season))
                    if (society.create(id, item, n, g.trade == "hunting" ? "hunted" : "foraged"))
                        note(s, id, g.trade == "hunting" ? "hunted" : "foraged", item, n, 0);
            }
        if (quarter == 17 * 4)
            for (const auto& id : who_[s])
                if (const auto* pack = society.account(id))
                {
                    std::vector<std::string> goods;
                    for (const auto& [item, n] : pack->stock)
                        if (n > 0 && item != "meal")        // (Its own provisions kept.)
                            goods.push_back(item);
                    for (const auto& item : goods)
                        sellAll(s, id, item);
                }
    }
    else if (s == "corner" && quarter == 6 * 4)
    {
        // At six each morning for its first week, every loaf, sack of flour and meal on the capital's shelves; then, each
        // morning, back to whichever shops take them.
        const auto& id = who_[s].front();
        if (day < startDay_ + 7)
        {
            for (const auto& shop : shops_)
                if (shop.town == capital_)
                    for (const char* base : {"bread", "flour", "meal"})
                        buyAll(s, id, shop, base);
        }
        else if (const auto* pack = society.account(id))
        {
            std::vector<std::string> goods;
            for (const auto& [item, n] : pack->stock)
                if (const auto base = items::baseOf(item); n > 0 && (base == "bread" || base == "flour" || base == "meal"))
                    goods.push_back(item);
            for (const auto& item : goods)
                sellAll(s, id, item);
        }
    }
    else if (s == "dump" && quarter == 10 * 4)
    {
        // At ten, a pack of each (a Hold's smithy's swords, "made at the Hold"; hides, hunted) sold to its towns' shops.
        for (const auto& id : who_[s])
            for (const char* item : {"sword", "iron_sword", "hide"})
            {
                const int held = society.account(id) ? Society::stock(*society.account(id), item) : 0;
                const bool hide = std::string(item) == "hide";
                if (held < Lot && society.create(id, item, Lot - held, hide ? "hunted" : "made at the Hold"))
                    note(s, id, hide ? "hunted" : "made", item, Lot - held, 0);
                sellAll(s, id, item);
            }
    }
    else if (s == "whale" && quarter == 9 * 4)
    {
        // At nine, every rapier and piece of finery (Society::belongingKind) on every shelf in the realm, while its
        // purse lasts.
        const auto& id = who_[s].front();
        for (const auto& shop : shops_)
            if (const auto* till = open(shop) ? society.account(society.tillOf(shop.keeper)) : nullptr)
            {
                std::set<std::string> bases;
                for (const auto& [item, n] : till->stock)
                    if (const auto kind = Society::belongingKind(item); n > 0 && (kind == "rapier" || kind == "finery"))
                        bases.insert(items::baseOf(item));
                for (const auto& base : bases)
                {
                    buyAll(s, id, shop, base);
                    stow(s, id);
                }
            }
    }
    else if (s == "poach")
    {
        // Each quarter, its hands' meals seen to (keepFed); each dawn the stores topped up and its hands paid at twice the
        // day's rate (payStaff).
        keepFed(s, day);
        if (quarter == 0)
        {
            provision(s);
            const auto wage = poachWage();
            payStaff(s, [wage](const std::string&) { return wage; });
        }
    }
    else if (s == "strike" && !struck_)
    {
        // As poach's, its hands paid their companions' wages; the month over, let go at that dawn.
        keepFed(s, day);
        if (quarter == 0 && day >= startDay_ + StrikeDays)
        {
            letGo(s);
            struck_ = true;
        }
        else if (quarter == 0)
        {
            provision(s);
            payStaff(s, [this](const std::string& id) { return companionWage(id); });
        }
    }
    else if (s == "flood" && flooded_ && onTheHour && hour >= 8 && hour <= 18)
    {
        // On the hour from 8 to 18 for FloodDays, what each has brought in sold to the capital's shops, as many as they
        // take (its pack topped up to 99 each morning: a stand-in, made in its pack, "landed"; no player's hunt or catch
        // is run). Unsold, it keeps: a player's goods never spoil (Society::spoil passes players by), and go to a shop
        // as a fresh lot, however long ago they came. A shop pays a player its own normal cost (Society::offerAt; one
        // that doesn't make it, its average paid), so buying the flood never lowers its cost; only its margin can fall.
        // Then they stop.
        if (day >= startDay_ + FloodDays)
        {
            flooded_ = false;
            for (const auto& id : who_[s])
                note(s, id, "stopped", staple_, 0, 0);
            return;
        }
        for (const auto& id : who_[s])
        {
            const int held = society.account(id) ? Society::stock(*society.account(id), staple_) : 0;
            if (hour == 8 && held < Lot && society.create(id, staple_, Lot - held, "landed"))
                note(s, id, "landed", staple_, Lot - held, 0);
            sellAll(s, id, staple_);
        }
    }
    else if ((s == "theft" || s == "visit") && quarter >= 9 * 4 && quarter < 21 * 4)
    {
        // Every quarter of an hour from 9 to 21, each thief's paw in a purse (the capital's grown, the watch aside): it
        // sets off for its mark (setOff; lookAbout tries the purse on arrival). The watch is the world's own: it may
        // hold them. A visitor goes about the same way and takes nothing (theft's control: bodies on the stage alone).
        for (const auto& body : who_[s])
        {
            if (world_.custodyOf(body))
            {
                if (held_.insert(body).second)
                    note(s, body, "held by the watch", "", 0, 0);
                continue;
            }
            held_.erase(body);
            setOff(s, body, day, quarter);
        }
    }
    else if (s == "theft" && quarter == 21 * 4 + 2)
    {
        // At half past nine of an evening, what isn't coin fenced at the capital's shops.
        for (const auto& thief : who_[s])
            if (!world_.custodyOf(thief))
                for (const auto& item : loot_[thief])
                    sellAll(s, thief, item);
    }
    else if (s == "buyout" && quarter % 4 == 0 && quarter >= BuyoutFrom * 4 && quarter <= BuyoutTo * 4 && day < startDay_ + BuyoutDays)
    {
        // On the hour from BuyoutFrom to BuyoutTo each day of its first week, every piece of food (a good that
        // nourishes: bread, meals, smoked fish, cheese, the raw stuff a provisioner sells...) on the capital's shelves,
        // as far as the five purses' hour goes: each its purse over the hours of the week still to come (this one among
        // them), so the purses last the week (since the third review: buying at six only, with nothing held back, they
        // ran dry on the fifth morning, and residents had what was made from a quarter past six). Each player buys until
        // its hour's money is spent, its pack full of kinds or its purse empty, then the next. Held after, never sold
        // back.
        auto& players = who_[s];
        const auto hoursLeft = (startDay_ + BuyoutDays - 1 - day) * (BuyoutTo - BuyoutFrom + 1) + (BuyoutTo - quarter / 4 + 1);
        std::map<std::string, std::int64_t> hour;
        for (const auto& id : players)
            hour[id] = society.account(id) ? society.account(id)->cash / std::max<std::int64_t>(1, hoursLeft) : 0;
        std::size_t next = 0;
        for (const auto& shop : shops_)
        {
            const auto* till = shop.town == capital_ && open(shop) ? society.account(society.tillOf(shop.keeper)) : nullptr;
            if (!till)
                continue;
            // (What it sells: food it holds and doesn't sell, its materials, isn't to be had.)
            const auto deals = society.wares(shop.keeper);
            std::set<std::string> bases;
            for (const auto& [item, n] : till->stock)
                if (const auto* good = items::good(item); n > 0 && ((good && good->nourish > 0) || items::baseOf(item) == "meal") &&
                                                          std::find(deals.begin(), deals.end(), items::baseOf(item)) != deals.end())
                    bases.insert(items::baseOf(item));
            for (const auto& base : bases)
                while (next < players.size())
                {
                    // One whose purse is empty, or whose pack has every kind it can hold, gives way to the next.
                    const auto* pack = society.account(players[next]);
                    if (!pack || pack->cash <= 0 || hour[players[next]] <= 0 || pack->stock.size() >= MaxGoodsKinds)
                    {
                        ++next;
                        continue;
                    }
                    buyAll(s, players[next], shop, base, &hour[players[next]]);
                    const auto* left = society.account(society.tillOf(shop.keeper));
                    if (!left || Society::stockAll(*left, base) == 0)
                        break;
                    ++next;                         // (Some left: more than its hour, its purse or its pack would take.)
                }
        }
    }
    else if (s == "gift" && quarter == 10 * 4)
        giveAway(s);
    else if (s == "winter" && quarter == 7 * 4 && calendar::calendarAt(double(day) + .5).season == calendar::Season::Autumn)
    {
        // At seven each autumn morning (the calendar's autumn, up to the winter: Society::WinterStart), every bundle of
        // firewood in its towns.
        for (const auto& id : who_[s])
            for (const auto& shop : shops_)
                if (towns_[id].count(shop.town))
                    buyAll(s, id, shop, "firewood");
    }
}

void Players::end()
{
    // The days over: the Hold's hands go back to their own lives (RatwGameCamps.cpp's "dismissstaff"), and the bodies
    // leave the world as a player's does who logs off (Game: World::removePlayer; one in a fight stays till it's over).
    // What they own they keep, as a player away does.
    ended_ = true;
    flooded_ = false;
    for (const char* s : {"poach", "strike"})
        letGo(s);
    for (const char* s : {"theft", "visit", "drain"})
        for (const auto& body : playersOf(s))
            leaving_.insert(body);
    walkers_.clear();
    bench_.clear();
    leave();
}

void Players::leave()
{
    for (auto it = leaving_.begin(); it != leaving_.end();)
    {
        if (world_.inBattle(*it))
        {
            ++it;
            continue;
        }
        if (world_.removePlayer(*it))
        {
            // (Its scenario from its name: "player-<scenario>-<n>".)
            const auto rest = it->substr(std::min(it->size(), std::size_t(7)));
            note(rest.substr(0, rest.find('-')), *it, "left", "", 0, 0);
        }
        it = leaving_.erase(it);
    }
}

std::string Players::capitalNow() const
{
    return capital_.empty() ? world_.society().capital() : capital_;
}

void Players::findMakers()
{
    // flood's makers: the posts that bring in the staple worked in the capital, or held by a wolf who lives there (its
    // shops buy from the capital's own first: Society::buyMaterials), found once (the first day's block, or the start).
    const auto capital = capitalNow();
    if (makersFound_ || capital.empty())
        return;                                     // (No capital named: the start's, by its shops.)
    makersFound_ = true;
    const auto& society = world_.society();
    const auto& held = society.state().careers.positions;
    for (const auto& p : society.positions())
    {
        const auto* producer = p.role == "merchant" ? nullptr : items::producerFor(p.title);
        const bool lands = producer && std::any_of(producer->out.begin(), producer->out.end(), [this](const auto& o) { return o.first == staple_; });
        if (!lands && !(p.role == "merchant" && crafts(p.title, staple_)))
            continue;                               // (Neither the land's nor a shop's that makes it: raw meat is a butcher's.)
        const auto h = held.find(p.id);
        const auto* life = h == held.end() ? nullptr : society.resident(h->second.holder);
        if (world_.communityOf(p.work.cell) == capital || (life && world_.communityOf(life->homeCell) == capital))
            makers_.push_back(p.id);
    }
}

bool Players::crafts(const std::string& workLabel, const std::string& item)
{
    const auto* business = items::businessFor(workLabel);
    if (!business)
        return false;
    for (const auto* k : items::craftsFor(business->id))
        for (const auto& [out, n] : k->out)
            if (out == item)
                return true;
    return false;
}

std::string Players::floodDay() const
{
    // The staple in the capital: what its shops that deal in it and don't make it ask and would pay a player (each
    // shop's own, averaged over them, before it is rounded to the penny: what it paid, and its margin), and what is on
    // the shelves of all that deal in it; its makers: the posts bringing it in or making it that are held, their tills'
    // cash and what they hold of it (their sales are in tills.csv, their hours in work.csv). From the run's first day,
    // before they start too: the days before the flood to set it by. (A shop that makes it is priced at its own cost of
    // making it, worked out once a day when it is first asked for and kept for the day: asked here at midnight, that
    // would be the day's for the game too, so it isn't asked. One that doesn't is priced at what it paid, whenever asked.)
    const auto& society = world_.society();
    const auto capital = capitalNow();
    const auto& posts = society.state().careers.positions;
    double asked = 0, offered = 0;
    int shops = 0, making = 0;
    std::int64_t shelves = 0;
    std::set<std::string> tills;
    for (const auto& p : society.positions())
    {
        const auto held = p.role == "merchant" ? posts.find(p.id) : posts.end();
        const Shop shop{held == posts.end() ? std::string() : held->second.holder, p.id, world_.communityOf(p.work.cell)};
        if (held == posts.end() || shop.town != capital || !open(shop))
            continue;
        const auto deals = society.wares(shop.keeper);
        if (std::find(deals.begin(), deals.end(), staple_) == deals.end())
            continue;
        if (const auto* spec = society.spec(shop.keeper); spec && crafts(spec->workLabel, staple_))
            ++making;
        else
        {
            asked += society.priceAt(shop.keeper, staple_);
            offered += society.offerAt(shop.keeper, staple_);
            ++shops;
        }
        if (const auto* till = tills.insert(society.tillOf(shop.keeper)).second ? society.account(society.tillOf(shop.keeper)) : nullptr)
            shelves += Society::stockAll(*till, staple_);
    }
    int held = 0;
    std::int64_t cash = 0, stock = 0;
    for (const auto& post : makers_)
    {
        if (const auto found = posts.find(post); found != posts.end() && !found->second.holder.empty())
            if (const auto* e = world_.entity(found->second.holder); e && !e->dead)
                ++held;
        if (const auto* till = society.account("till:" + post))
            cash += till->cash, stock += Society::stockAll(*till, staple_);
    }
    std::ostringstream out;
    out << ",\"staple\":" << quoted(staple_) << ",\"selling\":" << (flooded_ ? "true" : "false") << ",\"shops\":" << shops
        << ",\"shops_making\":" << making << ",\"asked\":" << (shops ? asked / shops : 0.)
        << ",\"offered\":" << (shops ? offered / shops : 0.) << ",\"shelves\":" << shelves << ",\"makers\":" << makers_.size()
        << ",\"makers_held\":" << held << ",\"makers_cash\":" << cash << ",\"makers_stock\":" << stock;
    return out.str();
}

std::string Players::strikeDay() const
{
    // The hands it took: how many have the post they had when taken (held or let go); and the capital's labour market:
    // its going rate, and its employers' posts open.
    const auto& society = world_.society();
    const auto capital = capitalNow();
    int kept = 0;
    for (const auto& id : taken_)
        if (const auto* job = society.jobOf(id); job && jobWas_.count(id) && job->id == jobWas_.at(id))
            ++kept;
    int open = 0;
    for (const auto& [key, book] : society.state().memory.employers)
        if (society.employerTownOf(key) == capital)
            open += society.vacanciesOf(key);
    std::ostringstream out;
    out << ",\"taken\":" << taken_.size() << ",\"kept_post\":" << kept << ",\"let_go\":" << (struck_ ? "true" : "false")
        << ",\"going\":" << society.goingRate(capital) << ",\"open\":" << open;
    return out.str();
}

void Players::giveAway(const std::string& s)
{
    // At ten each morning, GiftEach pennies to each of the GiftHouseholds poorest households of the realm (by what their
    // members hold, as econ_watch's measures count a household: a home; each wolf of a barracks or lodgings its own),
    // into the purse of its first grown member who isn't in the watch's hands, while the giver's purse lasts. As the
    // game gives (Game::gifted: Society::shift, kind "a gift"; at most 10,000p a give), but with no body walking up to
    // each (a stand-in: the game wants the two face to face), and without the warmth a gift brings.
    auto& society = world_.society();
    const auto& giver = who_[s].front();
    std::map<std::string, int> grownAt;
    for (const auto& [id, life] : society.state().residents)
        if (const auto* e = world_.entity(id); e && !e->dead && e->age >= 16 && !life.homeCell.empty())
            ++grownAt[life.homeCell];
    std::map<std::string, std::pair<std::int64_t, std::string>> homes;   // Household -> its money, and whom to give to.
    for (const auto& [id, life] : society.state().residents)
    {
        const auto* e = world_.entity(id);
        if (!e || e->dead)
            continue;
        const bool shared = !life.homeCell.empty() && (grownAt[life.homeCell] > 8 || Society::communalHome(life.homeCell));
        auto& home = homes[life.homeCell.empty() || shared ? id : life.homeCell];
        home.first += society.account(id) ? society.account(id)->cash : 0;
        if (home.second.empty() && e->age >= 16 && !world_.custodyOf(id))
            home.second = id;                       // (Residents in id order: the first grown.)
    }
    std::vector<std::pair<std::int64_t, std::string>> poorest;
    for (const auto& [home, h] : homes)
        if (!h.second.empty())
            poorest.push_back({h.first, h.second});
    std::sort(poorest.begin(), poorest.end());
    for (std::size_t i = 0; i < poorest.size() && i < std::size_t(GiftHouseholds); ++i)
    {
        const auto coins = std::min<std::int64_t>(GiftEach, society.account(giver) ? society.account(giver)->cash : 0);
        if (coins <= 0 || !society.shift(giver, poorest[i].second, "", 0, coins, "a gift"))
            break;
        note(s, giver, "gave", poorest[i].second, 1, coins);
    }
}

void Players::drainStart()
{
    // The town (not the capital, whose benches would want a hundred bodies and more) with the most makers at a bench, as
    // World::residentWorkAt has them: a wolf whose work is a business that crafts (not the land's), at its post at its
    // post's own task; so not a shop's keeper, whose task there is to trade. Two players at each (the workshop's `most`
    // is 3, its maker one of them), arriving at its work spot, as a player walks up to a bench.
    auto& society = world_.society();
    std::map<std::string, std::vector<std::string>> byTown;
    for (const auto& [id, life] : society.state().residents)
    {
        const auto* e = world_.entity(id);
        const auto* spec = society.spec(id);
        const auto* job = society.jobOf(id);
        const auto* business = spec ? items::businessFor(spec->workLabel) : nullptr;
        if (!e || e->dead || !job || life.role == "merchant" || !business || items::craftsFor(business->id).empty() ||
            items::producerFor(spec->workLabel))
            continue;
        if (const auto town = world_.communityOf(spec->work.cell); !town.empty() && town != capital_)
            byTown[town].push_back(id);
    }
    for (const auto& [town, makers] : byTown)
        if (makers.size() > benches_.size())
            drainTown_ = town, benches_ = makers;
    int n = 0;
    for (const auto& maker : benches_)
    {
        const auto* spec = society.spec(maker);
        for (int k = 0; k < 2; ++k)
        {
            const auto id = "player-drain-" + std::to_string(++n);
            world_.addPlayer(id, "Hand " + std::to_string(n), spec->work.cell, {spec->work.x + (k ? .8 : -.8), spec->work.y});
            who_["drain"].push_back(id);
            towns_[id] = {drainTown_};
            bench_[id] = maker;
            note("drain", id, "joined", maker, 0, society.account(id) ? society.account(id)->cash : 0);
        }
    }
}

void Players::drainLook()
{
    // Twice a world second, each at its bench: working beside its maker, it keeps at it (World::noteActive: a player
    // doing its part each beat, or the beat isn't paid); not, it asks to once a quarter of an hour (World::helpAtWork:
    // the maker at its bench, the till able to pay a hand, a place free), walking back to the bench first if it has been
    // pushed off. They work any hour their makers do.
    const double now = world_.time();
    if (drainLooked_ >= 0 && now - drainLooked_ < LookEvery)
        return;
    drainLooked_ = now;
    const auto quarter = std::int64_t(std::floor(world_.calendarDays() * Quarters));
    const bool ask = quarter != drainQuarter_;
    drainQuarter_ = quarter;
    const double reach = together::rules().lendTiles;
    for (const auto& [body, maker] : bench_)
    {
        const auto* me = world_.entity(body);
        const auto* m = world_.entity(maker);
        if (!me || !m || m->dead || world_.custodyOf(body))
            continue;
        if (const auto* j = world_.jointOf(body); j && j->target == maker)
        {
            world_.noteActive(body);
            continue;
        }
        if (!ask || !world_.residentWorkAt(maker))
            continue;
        if (m->cellId != me->cellId || apart(m->position, me->position) > reach)
        {
            const auto* spec = world_.society().spec(maker);
            if (spec && spec->work.cell == me->cellId)
                world_.moveTo(body, spec->work.x, spec->work.y);
            continue;
        }
        if (world_.helpAtWork(body, maker).ok)
            note("drain", body, "set to work", maker, 1, 0);
    }
}

std::string Players::drainDay() const
{
    // Its town, its benches and bodies; and its hires (the odd jobs its businesses post by the week: slots, and those
    // taken by its wolves), the work the players would take if they could (they can't: a player takes no odd job).
    const auto& society = world_.society();
    int slots = 0, taken = 0;
    const auto day = std::int64_t(std::floor(world_.calendarDays()));
    for (const auto& j : society.oddJobs())
        if (j.community == drainTown_ && j.until >= day - 1)
            slots += j.slots, taken += int(j.stage.size());
    int working = 0;
    for (const auto& [body, maker] : bench_)
        if (const auto* j = world_.jointOf(body); j && j->target == maker)
            ++working;
    std::ostringstream out;
    out << ",\"town\":" << quoted(drainTown_) << ",\"benches\":" << benches_.size() << ",\"working\":" << working
        << ",\"hire_slots\":" << slots << ",\"hire_taken\":" << taken;
    return out.str();
}

std::vector<PlayerMove> Players::takeMoves()
{
    std::vector<PlayerMove> out;
    out.swap(moves_);
    return out;
}

std::string Players::takeDay()
{
    const auto& society = world_.society();
    std::ostringstream out;
    out << "{";
    bool first = true;
    for (const auto& s : scenarios_)
    {
        std::int64_t cash = 0, goods = 0;
        const auto count = [&](const std::string& id) {
            if (const auto* a = society.account(id))
            {
                cash += a->cash;
                for (const auto& [item, n] : a->stock)
                    goods += n;
            }
        };
        const auto& players = playersOf(s);
        for (const auto& id : players)
            count(id);
        for (const auto& id : accountsOf(s))
            count(id);
        out << (first ? "" : ",") << quoted(s) << ":{\"players\":" << players.size() << ",\"cash\":" << cash << ",\"goods\":" << goods;
        first = false;
        if (s == "poach" || s == "strike")
        {
            int hungry = 0;
            for (const auto& id : staffOf(s))
                if (const auto* life = society.resident(id))
                    hungry += life->hunger >= 70;
            out << ",\"staff\":" << staffOf(s).size() << ",\"staff_hungry\":" << hungry;
        }
        if (s == "strike")
            out << strikeDay();
        if (s == "flood")
        {
            findMakers();
            out << floodDay();
        }
        if (s == "drain")
            out << drainDay();
        if (s == "buyout")
        {
            // The food it holds (pieces), and on the capital's shelves at the day's end (pieces, every shop's till once).
            std::int64_t held = 0, shelves = 0;
            const auto food = [](const EconomyAccount& a) {
                std::int64_t n = 0;
                for (const auto& [item, k] : a.stock)
                    if (const auto* good = items::good(item); (good && good->nourish > 0) || items::baseOf(item) == "meal")
                        n += k;
                return n;
            };
            for (const auto& id : players)
                if (const auto* a = society.account(id))
                    held += food(*a);
            std::set<std::string> tills;
            for (const auto& p : society.positions())
                if (const auto h = p.role == "merchant" ? society.state().careers.positions.find(p.id) : society.state().careers.positions.end();
                    h != society.state().careers.positions.end() && !h->second.holder.empty() && world_.communityOf(p.work.cell) == capitalNow())
                    if (const auto till = society.tillOf(h->second.holder); tills.insert(till).second)
                        if (const auto* a = society.account(till))
                            shelves += food(*a);
            out << ",\"food_held\":" << held << ",\"capital_shelves\":" << shelves;
        }
        if (s == "kill")
        {
            int filled = 0;
            for (const auto& post : posts_)
                if (const auto held = society.state().careers.positions.find(post);
                    held != society.state().careers.positions.end() && !held->second.holder.empty())
                    ++filled;
            out << ",\"posts\":" << posts_.size() << ",\"filled\":" << filled;
        }
        if (s == "theft")
            out << ",\"held\":" << held_.size();
        if (s == "theft" || s == "visit")
        {
            // Where their bodies stand at the day's end (the residents about them are on the stage), and how they go.
            std::map<std::string, int> at;
            for (const auto& id : players)
                if (const auto* e = world_.entity(id))
                    ++at[e->cellId];
            out << ",\"bodies\":" << quoted(bodies_ == Bodies::Walk ? "walk" : bodies_ == Bodies::Still ? "still" : "jump") << ",\"at\":{";
            for (auto it = at.begin(); it != at.end(); ++it)
                out << (it == at.begin() ? "" : ",") << quoted(it->first) << ":" << it->second;
            out << "}";
        }
        // What they did since the last day's end: each action, how many, how many pennies.
        if (const auto t = today_.find(s); t != today_.end())
            for (const auto& [action, n] : t->second)
                out << "," << quoted(action) << ":[" << n.first << "," << n.second << "]";
        out << "}";
    }
    out << "}";
    today_.clear();
    return out.str();
}
} // namespace ratw::econ
