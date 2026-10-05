// Working out of town (Docs/Design/42-money-in-circulation.md, Phase 3b). A grown wolf without a post takes up a trade
// on the wild ground near its town (gathering, hunting, woodcutting, fishing), works it from the same finite sources
// players use (doc 41's forage patches and hunting pressure), and sells what it brings back to the shops that buy it.
// Nobody pays it a wage: shops pay for goods, and wolves pay shops for food.
#include "RatwItems.h"
#include "RatwWild.h"
#include "RatwWorld.h"

#include <algorithm>
#include <cmath>
#include <deque>
#include <unordered_map>

namespace ratw
{
// ------------------------------------------------------------------ The society: the trade, and selling

std::string Society::outworkTitle(const std::string& trade)
{
    return trade == "hunting" ? "hunting in the wild" : trade == "woodcutting" ? "cutting wood in the wild"
           : trade == "fishing" ? "fishing the waters"  : "gathering in the wild";
}

bool Society::outworkTitled(const std::string& title)
{
    return title == "hunting in the wild" || title == "cutting wood in the wild" || title == "fishing the waters" ||
           title == "gathering in the wild";
}

bool Society::goesToChurch(const std::string& resident, std::int64_t day)
{
    // A third of the town, a different third from week to week (a steady choice for the week).
    return std::hash<std::string>{}(resident + "|church|" + std::to_string(day / 7)) % 3 == 0;
}

bool Society::idlePost(const std::string& title)
{
    // Asked of the same few titles every decision: each thread remembers.
    thread_local std::unordered_map<std::string, bool> known;
    if (const auto found = known.find(title); found != known.end())
        return found->second;
    bool idle = false;
    for (const char* word : {"idling", "sits and", "loiter", "lounging", "dozing"})
        idle = idle || title.find(word) != std::string::npos;
    return known.emplace(title, idle).first->second;
}

void Society::rollOutwork() const
{
    if (state_.budgetDay != outworkDay_)
    {
        outwork_.clear();
        outworkTaken_.clear();
        outworkDay_ = state_.budgetDay;
    }
}

const WorkGround* Society::outworkKnown(const std::string& resident, const std::string& community, bool& known) const
{
    known = true;
    if (!day_.grounds)
        return nullptr;
    const auto found = day_.grounds->find(community);
    if (found == day_.grounds->end() || found->second.empty())
        return nullptr;
    const auto had = outwork_.find(resident);
    known = state_.budgetDay == outworkDay_ && had != outwork_.end();
    return known && had->second < found->second.size() ? &found->second[had->second] : nullptr;
}

const WorkGround* Society::outworkOf(const std::string& resident, const std::string& community) const
{
    if (!day_.grounds)
        return nullptr;
    const auto found = day_.grounds->find(community);
    if (found == day_.grounds->end() || found->second.empty())
        return nullptr;
    const auto& list = found->second;
    rollOutwork();
    if (const auto had = outwork_.find(resident); had != outwork_.end())
        return had->second < list.size() ? &list[had->second] : nullptr;
    // Its own leaning (a steady per-wolf choice), then the next ground along with room.
    auto& taken = outworkTaken_[community];
    const auto start = std::hash<std::string>{}(resident + "|outwork");
    for (std::size_t k = 0; k < list.size(); ++k)
    {
        const auto i = (start + k) % list.size();
        if (taken[i] < OutworkRoom)
        {
            ++taken[i];
            outwork_[resident] = i;
            return &list[i];
        }
    }
    outwork_[resident] = list.size();                // Every ground taken: the Town Works, then.
    return nullptr;
}

namespace
{
// A steady chance in [0, 1) from a key (as RatwHunt.cpp's).
double odds(const std::string& a, std::int64_t b)
{
    std::uint64_t h = 1469598103934665603ULL ^ std::uint64_t(b) * 0x9E3779B97F4A7C15ULL;
    for (unsigned char c : a)
        h = (h ^ c) * 1099511628211ULL;
    h ^= h >> 31;
    h *= 0xBF58476D1CE4E5B9ULL;
    return double((h ^ (h >> 29)) % 100000) / 100000.0;
}
// What a shop takes in from a wolf who brings goods in: what its kind buys (crafts.json `buys`) and supplies.
bool shopTakes(const items::Business* business, const std::string& item)
{
    if (!business)
        return false;
    for (const auto* list : {&items::buysFor(business->id), &items::suppliesFor(business->id)})
        if (std::find(list->begin(), list->end(), item) != list->end())
            return true;
    return false;
}
// Goods a wolf brings in from the land to sell (not what it wears, eats ready-made, or fights with).
bool broughtIn(const std::string& item)
{
    return item != "meal" && item != "sword" && !items::wearable(item) && items::good(item);
}
} // namespace

bool Society::carriesForSale(const std::string& resident) const
{
    const auto* purse = account(resident);
    if (!purse)
        return false;
    for (const auto& [item, n] : purse->stock)
        if (n > 0 && broughtIn(item))
            return true;
    return false;
}

bool Society::sellsTo(const std::string& resident, const std::string& shop) const
{
    const auto* keeper = spec(shop);
    const auto* business = keeper ? items::businessFor(keeper->workLabel) : nullptr;
    const auto* purse = account(resident);
    if (!business || !purse)
        return false;
    for (const auto& [item, n] : purse->stock)
        if (n > 0 && broughtIn(item) && shopTakes(business, item))
            return true;
    return false;
}

std::int64_t Society::sellBroughtIn(const std::string& resident, const std::string& shop)
{
    const auto* keeper = spec(shop);
    const auto* business = keeper ? items::businessFor(keeper->workLabel) : nullptr;
    const auto* purse = account(resident);
    const auto till = tillOf(shop);                  // (A house's business: its till.)
    if (!business || !purse || !account(till))
        return 0;
    std::vector<std::pair<std::string, int>> held;
    for (const auto& [item, n] : purse->stock)
        if (n > 0 && broughtIn(item) && shopTakes(business, item))
            held.push_back({item, n});
    std::int64_t made = 0;
    for (const auto& [item, n] : held)
    {
        // At the price a player gets (doc 15: a shop buys at a little over half what it sells for), while it wants
        // more and has the money.
        const auto* good = items::good(item);
        const std::int64_t price = std::max<std::int64_t>(1, std::int64_t(std::floor((good ? good->price : 1) * .55)));
        const int room = SuppliesKept - stock(*account(till), item);
        const int count = int(std::min<std::int64_t>({n, room, 99, account(till)->cash / price}));
        if (count > 0 && transfer(resident, till, item, count, price, "brought in and sold"))
            made += price * count;
    }
    return made;
}

// ------------------------------------------------------------------ The world: the ground near each town

void World::findWorkGrounds()
{
    auto grounds = std::make_shared<std::map<std::string, std::vector<WorkGround>>>();
    workGrounds_ = grounds;
    if (towns_.empty())
        return;                                      // (A world of one town has no country of its own to work, yet.)
    const auto& forage = wild::forage().ground;
    const auto groundAt = [&](const Cell& c, int x, int y) -> const wild::ForageGround* {
        if (const auto* t = c.tile(x, y))
            for (const auto& g : forage)
                if (g.tiles.find(t->glyph) != std::string::npos)
                    return &g;
        return nullptr;
    };
    for (const auto& town : towns_)
    {
        // The wild cells within two steps of the town, nearest first, up to MostCells of them.
        constexpr std::size_t MostCells = 10;
        std::vector<std::string> wilds;
        std::set<std::string> seen;
        std::deque<std::pair<std::string, int>> queue;
        for (const auto& [cellId, t] : townOfCell_)
            if (t == town.id)
            {
                queue.push_back({cellId, 0});
                seen.insert(cellId);
            }
        while (!queue.empty() && wilds.size() < MostCells)
        {
            const auto [at, depth] = queue.front();
            queue.pop_front();
            const auto* c = cell(at);
            if (c && c->outdoors && !townOfCell_.count(at))
                wilds.push_back(at);
            if (depth >= 2)
                continue;
            if (const auto next = exits_.find(at); next != exits_.end())
                for (const auto& n : next->second)
                    if (seen.insert(n).second && !townOfCell_.count(n) && cell(n) && cell(n)->outdoors)
                        queue.push_back({n, depth + 1});
        }
        auto& list = (*grounds)[town.id];
        for (const auto& cellId : wilds)
        {
            if (streamed() && !ensureLoaded(cellId).ok)
                continue;
            const auto* c = cell(cellId);
            if (!c || c->tiles.empty())
                continue;
            // Standing places a few strides apart, each beside ground worth working.
            int here = 0;
            constexpr int Step = 5, MostHere = 6;
            for (int y = 2; y < c->height - 2 && here < MostHere; y += Step)
                for (int x = 2 + (y / Step) % 2 * 2; x < c->width - 2 && here < MostHere; x += Step)
                {
                    const auto* t = c->tile(x, y);
                    if (!t || t->solid || t->movementCost > 2 || !passable(cellId, {x + .5, y + .5}))
                        continue;
                    const wild::ForageGround* g = nullptr;
                    for (int dy = -1; dy <= 1 && !g; ++dy)
                        for (int dx = -1; dx <= 1 && !g; ++dx)
                            g = groundAt(*c, x + dx, y + dy);
                    if (!g)
                        continue;
                    const bool wood = g->id == "broadleaf" || g->id == "pine" || g->id == "deadwood";
                    const auto trade = g->id == "shallows" || g->id == "reeds" ? std::string("fishing")
                                       : wood && (x / Step + y / Step) % 2 == 0 ? std::string("woodcutting")
                                                                                : std::string("gathering");
                    list.push_back({trade, g->id, {cellId, x + .5, y + .5}});
                    ++here;
                }
            // Game, where the ground suits any (doc 41's habitats): a hunter's place at the cell's heart.
            const auto ground = groundIn(cellId, 0, 0, c->width, c->height);
            bool game = false;
            for (const auto& s : wild::species())
                for (const auto& [g, w] : s.habitats)
                    game |= w > 0 && ground.count(g) && ground.at(g) > .2;
            if (game)
                huntGround_[cellId] = ground;        // (Kept: the cell may be out of memory when it is hunted.)
            if (game)
                for (int r = 0; r < std::max(c->width, c->height) / 2; ++r)
                {
                    const int x = c->width / 2 + r % 3 - 1, y = c->height / 2 + r / 3;
                    const auto* t = c->tile(x, y);
                    if (t && !t->solid && t->movementCost <= 2 && passable(cellId, {x + .5, y + .5}))
                    {
                        list.push_back({"hunting", "", {cellId, x + .5, y + .5}});
                        break;
                    }
                }
        }
    }
}

std::vector<std::pair<std::string, int>> World::harvestAt(const std::string& who, const WorkGround& at, int season)
{
    std::vector<std::pair<std::string, int>> out;
    const auto roll = [&](const std::string& what) { return odds(who + "|" + what, std::int64_t(calendarDays_ * 1440)); };
    if (at.trade == "hunting")
    {
        // A kill, or none, by how much game the ground holds and how hard it has been hunted (the same pressure a
        // player's hunt feels); each kill adds to it.
        const auto shares = huntGround_.find(at.spot.cell);
        if (shares == huntGround_.end())
            return out;
        const auto& ground = shares->second;
        const auto& pop = wild::population();
        const double chance = .4 / (1 + huntPressure(at.spot.cell) / pop.capacity);
        if (roll("hunt") > chance)
            return out;
        double total = 0;
        std::vector<std::pair<const wild::Species*, double>> fits;
        for (const auto& s : wild::species())
        {
            double fit = 0;
            for (const auto& [g, w] : s.habitats)
                if (const auto share = ground.find(g); share != ground.end())
                    fit = std::max(fit, w * share->second);
            if (fit > 0 && s.temper != "fierce")    // (An NPC hunter leaves the bears and wolves alone.)
            {
                fits.push_back({&s, fit / std::max(1.0, s.rarity)});
                total += fits.back().second;
            }
        }
        double r = roll("quarry") * total;
        for (const auto& [s, w] : fits)
            if ((r -= w) <= 0)
            {
                out = s->yield;
                huntKills_[at.spot.cell].push_back(calendarDays_);
                break;
            }
        return out;
    }
    if (at.trade == "fishing" && roll("fish") < .5)
        out.push_back({"fish", 1});
    // From the ground's own goods, out of the same patch a player forages (picked over, it gives nothing till it grows).
    const auto& data = wild::forage();
    const auto g = std::find_if(data.ground.begin(), data.ground.end(), [&](const wild::ForageGround& f) { return f.id == at.ground; });
    if (g == data.ground.end() || !takeFromPatch(at.spot.cell, int(at.spot.x), int(at.spot.y)))
        return out;
    double total = 0;
    for (const auto& good : g->goods)
        if (good.seasons.empty() || std::find(good.seasons.begin(), good.seasons.end(), season) != good.seasons.end())
            total += good.weight;
    double r = roll("gather") * total;
    for (const auto& good : g->goods)
        if ((good.seasons.empty() || std::find(good.seasons.begin(), good.seasons.end(), season) != good.seasons.end()) &&
            (r -= good.weight) <= 0)
        {
            out.push_back({good.item, good.count});
            break;
        }
    if (at.trade == "woodcutting")
    {
        out.push_back({"firewood", 2});             // An axe brings in more wood than a forager's armful.
        if (roll("timber") < .3)
            out.push_back({"timber", 1});
    }
    return out;
}

bool World::takeFromPatch(const std::string& cellId, int x, int y)
{
    const auto& data = wild::forage();
    const auto patch = cellId + "|" + std::to_string(x / data.patchTiles) + "|" + std::to_string(y / data.patchTiles);
    auto& [used, day] = forage_[patch];
    const double regrown = (calendarDays_ - day) * 24 / data.regrowHours;
    if (regrown >= 1)
    {
        used = std::max(0, used - int(regrown));
        day = calendarDays_;
    }
    if (used == 0)
        day = calendarDays_;
    if (used >= data.picks)
        return false;
    ++used;
    return true;
}
} // namespace ratw
