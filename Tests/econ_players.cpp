// Players who push (Docs/Design/64-economy-migration.md, Phase 13). See econ_players.h.
#include "econ_players.h"

#include "RatwCalendar.h"
#include "RatwItems.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <sstream>

namespace ratw::econ
{
namespace
{
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
// Every coin a player has is the capital's treasury's, moved by the ledger ("test:" money: nobody's earnings).
const char* Funding = "test: a player's money, from the capital's treasury";
constexpr int Quarters = 96;                       // Quarters of an hour in a day: when the players act.
constexpr int PackKinds = 56;                      // A pack this full of kinds goes to a chest (MaxGoodsKinds 64).
} // namespace

const std::vector<std::string>& Players::names()
{
    static const std::vector<std::string> all{"hoard", "corner", "dump", "whale", "poach", "kill", "theft", "winter"};
    return all;
}

Players::Players(World& world, std::vector<std::string> scenarios, std::int64_t fromDay, std::int64_t days)
    : world_(world), scenarios_(std::move(scenarios)), from_(fromDay), days_(days)
{
}

void Players::step()
{
    const auto quarter = std::int64_t(std::floor(world_.calendarDays() * Quarters));
    if (quarter == quarter_)
        return;
    quarter_ = quarter;
    const auto day = quarter / Quarters;
    if (day < from_)
        return;
    if (days_ > 0 && day >= from_ + days_)
    {
        if (started_ && !ended_)
            end();
        return;
    }
    refreshShops(day);
    if (!started_)
        start(day);
    for (const auto& s : scenarios_)
        act(s, day, int(quarter % Quarters));
}

void Players::refreshShops(std::int64_t day)
{
    // Every shop whose keeper is living, by the town it works in (once a day: a keeper who dies has a successor).
    if (day == shopsDay_)
        return;
    shopsDay_ = day;
    shops_.clear();
    const auto& society = world_.society();
    for (const auto& p : society.positions())
        if (p.role == "merchant")
            if (const auto held = society.state().careers.positions.find(p.id); held != society.state().careers.positions.end())
                if (const auto* e = world_.entity(held->second.holder); e && !e->dead)
                    shops_.push_back({held->second.holder, world_.communityOf(p.work.cell)});
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

int Players::buyAll(const std::string& scenario, const std::string& player, const Shop& shop, const std::string& base)
{
    // Every one the shop has, of every quality, as the game sells them (Society::trade: 99 at most a trade).
    auto& society = world_.society();
    const auto deals = society.wares(shop.keeper);
    const auto till = society.tillOf(shop.keeper);
    if (std::find(deals.begin(), deals.end(), base) == deals.end() || !society.account(till))
        return 0;
    int got = 0;
    for (const auto& kind : Society::kindsHeld(*society.account(till), base))
        for (int k = std::min(99, Society::stock(*society.account(till), kind)); k > 0;)
        {
            const auto r = society.trade(player, shop.keeper, kind, k, true);
            if (!r.ok)
            {
                k /= 2;                             // (Short of money, or of room: fewer.)
                continue;
            }
            got += k;
            note(scenario, player, "bought", kind, k, r.total);
            k = std::min(99, Society::stock(*society.account(till), kind));
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
        const auto deals = society.wares(shop.keeper);
        if ((towns != towns_.end() && !trades(shop, towns->second)) || std::find(deals.begin(), deals.end(), base) == deals.end())
            continue;
        for (int k = std::min(99, held()); k > 0;)
        {
            const auto r = society.trade(player, shop.keeper, item, k, false);
            if (!r.ok)
            {
                k /= 2;
                continue;
            }
            sold += k;
            note(scenario, player, scenario == "theft" ? "fenced" : "sold", item, k, r.total);
            k = std::min(99, held());
        }
    }
    return sold;
}

void Players::stow(const std::string& scenario, const std::string& player)
{
    // A pack holds so many kinds of goods (MaxGoodsKinds); past that, they go into a rented chest (doc 54's "let:").
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
    {
        const auto* chest = chests.empty() ? nullptr : society.account(chests.back());
        if (!chest || (!chest->stock.count(item) && int(chest->stock.size()) >= PackKinds))
        {
            chests.push_back("let:" + player + "-" + std::to_string(chests.size() + 1));
            society.openAccount(chests.back());
        }
        if (society.shift(player, chests.back(), item, n, 0, "put in a chest"))
            note(scenario, player, "stowed", item, n, 0);
    }
}

void Players::start(std::int64_t day)
{
    started_ = true;
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
    // Player i of n trades in town i of the towns (and every nth after, when there are fewer players than towns).
    const auto spread = [&](const std::string& player, int i, int n) {
        auto& mine = towns_[player];
        for (int t = 0; t < int(towns.size()); ++t)
            if (t % n == i % n || i % int(towns.size()) == t)
                mine.insert(towns[std::size_t(t)]);
    };
    for (const auto& s : scenarios_)
        if (s == "hoard")
        {
            // Twenty wolves out on the capital's hunting and gathering grounds (any town's, if it has none), the same
            // game and patches its own hunters and gatherers work (World::harvestAt), selling in those towns' shops.
            // A player roams: each spell at another ground.
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
            for (std::size_t i = 0; i < hands.size() && i < 20; ++i)
            {
                const auto& id = hands[i].second;
                world_.entity(id)->leaderId = "camp:econ-poach";
                world_.stop(id);
                staff_.push_back(id);
                const auto* job = society.jobOf(id);
                note(s, chapter, "hired", id + " (" + (job ? job->title : std::string()) + ")", 1, 0);
            }
        }
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
        }
        else if (s == "theft")
        {
            // Five thieves with bodies in the world (World::steal needs one beside its mark), arriving at a capital shop.
            const Position* at = nullptr;
            for (const auto& p : society.positions())
                if (!at && p.role == "merchant" && world_.communityOf(p.work.cell) == capital_)
                    at = &p;
            for (int i = 1; i <= 5; ++i)
            {
                const auto id = "player-theft-" + std::to_string(i);
                if (at)
                    world_.addPlayer(id, "Thief " + std::to_string(i), at->work.cell, {at->work.x, at->work.y});
                else
                    world_.addPlayer(id, "Thief " + std::to_string(i));
                who_[s].push_back(id);
                towns_[id] = {capital_};
                note(s, id, "joined", "", 0, society.account(id) ? society.account(id)->cash : 0);
            }
        }
    (void)day;
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
        // At six each morning for a week, every loaf, sack of flour and meal on the capital's shelves; then, each morning,
        // back to whichever shops take them.
        const auto& id = who_[s].front();
        if (day < from_ + 7)
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
                if (held < 99 && society.create(id, item, 99 - held, hide ? "hunted" : "made at the Hold"))
                    note(s, id, hide ? "hunted" : "made", item, 99 - held, 0);
                sellAll(s, id, item);
            }
    }
    else if (s == "whale" && quarter == 9 * 4)
    {
        // At nine, every rapier and piece of finery (Society::belongingKind) on every shelf in the realm, while its
        // purse lasts.
        const auto& id = who_[s].front();
        for (const auto& shop : shops_)
            if (const auto* till = society.account(society.tillOf(shop.keeper)))
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
    else if (s == "poach" && quarter == 0)
    {
        // Each dawn, its hands paid (a week's wages put by each week first); one it can't pay goes home (campTick).
        const auto& chapter = holds_[s].front();
        const auto wage = 2 * std::max<std::int64_t>(5, std::int64_t(std::ceil(society.labourDay("") - 1e-9)));   // (Twice keepWage.)
        if ((day - from_) % 7 == 0)
            fund(s, chapter, 7 * wage * std::int64_t(staff_.size()));
        for (auto it = staff_.begin(); it != staff_.end();)
        {
            const auto* e = world_.entity(*it);
            if (e && !e->dead && society.shift(chapter, *it, "", 0, wage, "camp wage"))
            {
                note(s, chapter, "paid", *it, 1, wage);
                ++it;
                continue;
            }
            if (auto* npc = world_.entity(*it))
                npc->leaderId.clear();
            note(s, chapter, e && !e->dead ? "left unpaid" : "died", *it, 1, 0);
            it = staff_.erase(it);
        }
    }
    else if (s == "theft" && quarter >= 9 * 4 && quarter < 21 * 4)
    {
        // Every quarter of an hour from 9 to 21, each thief's hand in a purse (the capital's grown, the watch aside),
        // beside its mark (World::teleport) as a player walks up to one. The watch is the world's own: it may hold them.
        std::vector<std::string> marks;
        for (const auto& [id, life] : society.state().residents)
            if (const auto* e = world_.entity(id); e && !e->dead && e->age >= 16 && !world_.custodyOf(id) &&
                                                   world_.communityOf(life.homeCell) == capital_)
                if (const auto* job = society.jobOf(id); !job || job->role != "guard")
                    marks.push_back(id);
        for (const auto& thief : who_[s])
        {
            if (world_.custodyOf(thief))
            {
                if (held_.insert(thief).second)
                    note(s, thief, "held by the watch", "", 0, 0);
                continue;
            }
            held_.erase(thief);
            if (marks.empty())
                break;
            const auto& mark = marks[std::size_t(lot(thief + "|" + std::to_string(day) + "|" + std::to_string(quarter)) * double(marks.size()))];
            const auto* v = world_.entity(mark);
            if (!v || !world_.teleport(thief, v->cellId, v->position.x, v->position.y).ok)
                continue;
            const auto before = society.account(thief) ? *society.account(thief) : EconomyAccount{};
            world_.steal(thief, mark);
            const auto* after = society.account(thief);
            if (!after)
                continue;
            if (after->cash > before.cash)
                note(s, thief, "stole", "", 0, after->cash - before.cash);
            for (const auto& [item, n] : after->stock)
                if (const int was = Society::stock(before, item); n > was)
                {
                    note(s, thief, "stole", item, n - was, 0);
                    loot_[thief].insert(item);
                }
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
    else if (s == "winter" && quarter == 7 * 4 && int(std::fmod(double(day), calendar::DaysPerYear)) < Society::WinterStart)
    {
        // At seven each autumn morning (before the winter: Society::WinterStart), every bundle of firewood in its towns.
        for (const auto& id : who_[s])
            for (const auto& shop : shops_)
                if (towns_[id].count(shop.town))
                    buyAll(s, id, shop, "firewood");
    }
}

void Players::end()
{
    // The days over: the Hold's hands go back to their own lives (RatwGameCamps.cpp's "dismissstaff").
    ended_ = true;
    for (const auto& id : staff_)
        if (auto* npc = world_.entity(id))
        {
            npc->leaderId.clear();
            world_.stop(id);
            note("poach", holds_["poach"].empty() ? std::string() : holds_["poach"].front(), "released", id, 1, 0);
        }
    staff_.clear();
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
        const auto players = who_.find(s);
        for (const auto& id : players == who_.end() ? std::vector<std::string>{} : players->second)
            count(id);
        if (const auto extra = holds_.find(s); extra != holds_.end())
            for (const auto& id : extra->second)
                count(id);
        out << (first ? "" : ",") << quoted(s) << ":{\"players\":" << (players == who_.end() ? 0 : players->second.size())
            << ",\"cash\":" << cash << ",\"goods\":" << goods;
        first = false;
        if (s == "poach")
        {
            int hungry = 0;
            for (const auto& id : staff_)
                if (const auto* life = society.resident(id))
                    hungry += life->hunger >= 70;
            out << ",\"staff\":" << staff_.size() << ",\"staff_hungry\":" << hungry;
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
