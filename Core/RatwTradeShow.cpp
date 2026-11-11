// Staged trade on the roads (Docs/Design/79-a-staged-economy.md, Phase 4; sections 5.1, 7.4, 7.7, 8.1, 8.2 and 8.6). World
// members, kept here beside RatwRoads.cpp (the wagons' walking, the bandits, the contracts).
//
// The road layer runs on the Market Board's markets (World::ensureTradePoints): every community with a merchant post is a
// market for trade, wagons and bandits, while law stays on the towns (lawTown, towns_). Wagons between markets are
// committed when posted, two days ahead, on both ends' boards (the Board's `committed`, saved), and driven by real carters,
// away from their posts for the trip. A wagon carries staged goods for show: a manifest of goods the destination's shops
// stock or buy. Every Board change it makes lands at a moment a player could see it: the source's glut halved as it loads,
// the gap closed as it arrives, a raid's +0.15 at the raid, an overdue wagon's +0.05 a day. Each staffed farm or works
// sends a cart to its market every morning (heavier on Marketday), its load and its chance of coming by its area's harvest.
// A market's imports (goods it doesn't make that a wagon or cart brings it) refill only on arrivals, from a back room between
// (D27). Nothing here moves money: the bounties and escorts are the roads' contracts (RatwRoads.cpp), staged-sourced.
#include "RatwCalendar.h"
#include "RatwInjury.h"
#include "RatwItems.h"
#include "RatwWorld.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numeric>
#include <set>
#include <tuple>

namespace ratw
{
namespace
{
std::uint64_t lot(const std::string& a, std::int64_t b)
{
    std::uint64_t h = std::hash<std::string>{}(a) * 1099511628211ULL + std::uint64_t(b) * 2654435761ULL;
    h ^= h >> 29;
    h *= 0xBF58476D1CE4E5B9ULL;
    h ^= h >> 32;
    return h;
}
double unit(const std::string& a, std::int64_t b)
{
    return double(lot(a, b) % 100000) / 100000.0;
}
std::string lowered(std::string s)
{
    for (auto& c : s)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
// "a", "a and b", "a, b and c".
std::string listed(const std::vector<std::string>& words)
{
    std::string out;
    for (std::size_t i = 0; i < words.size(); ++i)
        out += (i == 0 ? "" : i + 1 == words.size() ? " and " : ", ") + words[i];
    return out;
}
bool carterTitle(const std::string& title)
{
    if (const auto* b = items::businessFor(title); b && b->id == "carters")
        return true;
    const auto t = lowered(title);
    return t.find("carter") != std::string::npos || t.find("wagoner") != std::string::npos || t.find("carrier") != std::string::npos;
}
// The seeded mishaps that make a wagon late (8.1: a cause people talk about).
const char* const Mishaps[] = {"a broken axle", "mud to the axles", "a wheel off in a rut", "a washed-out ford"};
constexpr double ArrivalNudge = .2, RaidNudge = .15, LateNudge = .05, LateMost = .2, MissedCartNudge = .05;
constexpr double DueDays = .5, LateAfterDays = 1;    // A wagon is due half a day out, and late a day past that.
constexpr int ManifestMost = 5, PostAhead = 2, GlutMost = 2;
// A market's imports by wagon from one source (D27, T14): what its wagons bring in rotation, the thinnest first, so each
// comes at least every few wagons; more of that source's goods than this refill each morning, as nothing would bring them.
constexpr std::size_t ImportsPerSource = 2 * ManifestMost;
constexpr double CampLootMost = 40;                  // Pennies of goods a raided camp holds at most (8.1).
} // namespace

std::string World::goodWords(const std::string& base)
{
    // A good in plain words for notices, news and talk (8.1: "salt and nails for the general store"): the catalogue's name
    // without its pack ("Nails (bag of 50)" -> "nails"), lower case.
    std::string name = items::good(base) || items::wearable(base) || base == "herbs" || base == "meal" ? Society::itemName(base) : "";
    if (name.empty())
    {
        name = base;
        std::replace(name.begin(), name.end(), '_', ' ');
    }
    if (const auto cut = name.find(" ("); cut != std::string::npos)
        name.erase(cut);
    return lowered(name);
}

bool World::pluralGood(const std::string& words)
{
    // "nails are", "cheese is": a plain good word ending in a lone s is a plural, but for the few that aren't ("citrus",
    // "ground lens") and a thing named with what it has ("a leather cap with ear slits").
    if (words.size() < 2 || words.back() != 's' || words[words.size() - 2] == 's' || words.find(" with ") != std::string::npos)
        return false;
    for (const char* one : {"canvas", "kermes", "molasses", "harness", "citrus", "lens"})
        if (words.size() >= std::strlen(one) && words.compare(words.size() - std::strlen(one), std::string::npos, one) == 0)
            return false;
    return true;
}

std::string World::marketWords(const std::string& id) const
{
    if (id.empty())
        return "the road";
    std::string out;
    bool up = true;
    for (char c : id)
    {
        if (c == '_')
        {
            out += ' ';
            up = true;
            continue;
        }
        out += up ? char(std::toupper(static_cast<unsigned char>(c))) : c;
        up = false;
    }
    return out;
}

const Town* World::roadMarket(const std::string& id) const
{
    for (const auto& m : roadMarkets_)
        if (m.id == id)
            return &m;
    return nullptr;
}

const Town* World::roadMarketOf(const std::string& cellId) const
{
    const auto found = marketIndex_.find(cellId);
    return found == marketIndex_.end() || found->second >= roadMarkets_.size() ? nullptr : &roadMarkets_[found->second];
}

std::vector<std::string> World::cartersOf(const std::string& market) const
{
    const auto found = carters_.find(market);
    return found == carters_.end() ? std::vector<std::string>{} : found->second;
}

void World::ensureRoadMarkets()
{
    // The road layer's markets (5.1; Phase 4): the Board's, found again when they are, and the carters when the roster moves.
    ensureTradePoints();
    // (The DM's rules that stop the wagons or the carts change what can bring a market its imports: D27, 5.1.)
    const bool wagonsRun = society_.rule("caravanRate") > 0 && society_.rule("wagonsOnRoad") > 0;
    const bool cartsRun = society_.rule("cartsOnRoad") > 0;
    const int rules = int(wagonsRun) | int(cartsRun) << 1 | int(society_.rule("shipsPerWeek") > 0) << 2;   // (And the ships: Phase 5.)
    if (roadMarketsKey_ == tradePointsBuilt_ && roadMarketsRoster_ == society_.placesRevision() && roadMarketsDay_ == roads_.day &&
        roadMarketsRules_ == rules)
        return;
    roadMarketsKey_ = tradePointsBuilt_;
    roadMarketsRoster_ = society_.placesRevision();
    roadMarketsDay_ = roads_.day;
    roadMarketsRules_ = rules;
    const auto& tp = society_.tradePoints();
    roadMarkets_.clear();
    marketIndex_.clear();
    for (const auto& m : tp.markets)
    {
        if (m.id == "outside" || m.place.empty() || !cells_.count(m.place))
            continue;
        Town t;
        t.id = m.id;
        t.market = m.place;
        t.store = society_.treasuryOf(m.id);
        t.residents = int(std::max(0., m.catchment - market::PassingTrade));
        bool placed = false;
        for (const auto& till : m.tills)
            if (const auto* p = till.size() > 5 ? society_.position(till.substr(5)) : nullptr; p && p->work.cell == m.place)
            {
                t.marketX = p->work.x, t.marketY = p->work.y, placed = true;
                break;
            }
        if (!placed)
            if (const auto* c = cell(m.place))
                t.marketX = c->width / 2.0, t.marketY = c->height / 2.0;
        for (const auto& p : society_.positions())
            if (p.role == "guard" && m.cells.count(p.work.cell))
                ++t.guards;
        const auto at = roadMarkets_.size();
        for (const auto& c : m.cells)
            marketIndex_.emplace(c, at);
        marketIndex_[m.place] = at;
        roadMarkets_.push_back(std::move(t));
    }
    // The roads between markets (each market's nearest, both ways once), for the wagons and the bandits.
    roadRoutes_.clear();
    roadPairs_.clear();
    std::set<std::pair<std::string, std::string>> seen;
    for (const auto& m : tp.markets)
        for (const auto& other : m.routes)
        {
            const auto* a = roadMarket(m.id);
            const auto* b = roadMarket(other);
            if (!a || !b || !seen.insert(std::minmax(a->id, b->id)).second)
                continue;
            if (auto route = routeBetween(a->market, b->market); route.size() >= 2)
            {
                roadRoutes_.push_back(std::move(route));
                roadPairs_.push_back({a->id, b->id});
            }
        }
    // Bandits camp in wild country along the roads (one in six such cells), once a load (a save's camps kept).
    if (!campsSeeded_)
    {
        campsSeeded_ = true;
        if (roads_.camps.empty())
            for (const auto& route : roadRoutes_)
                for (const auto& cellId : route)
                    if (!marketIndex_.count(cellId) && std::hash<std::string>{}(cellId) * 1099511628211ULL % 6 == 0 &&
                        std::none_of(roads_.camps.begin(), roads_.camps.end(), [&](const BanditCamp& b) { return b.cell == cellId; }))
                        roads_.camps.push_back({"camp_" + cellId, cellId, 3.0 + double(lot(cellId, 1) % 4), 30, -100, true});
    }
    // The carters (D26): every held post of the carters' trade, by the market it works in (else the one its holder lives
    // in). A host who deletes them gets no wagons.
    carters_.clear();
    for (const auto& p : society_.positions())
    {
        if (!carterTitle(p.title))
            continue;
        const auto held = society_.state().careers.positions.find(p.id);
        if (held == society_.state().careers.positions.end() || held->second.holder.empty())
            continue;
        const auto& who = held->second.holder;
        const auto* e = entity(who);
        const auto* life = society_.resident(who);
        if (!e || e->dead || !life)
            continue;
        const auto* m = roadMarketOf(p.work.cell);
        if (!m)
            m = roadMarketOf(life->homeCell);
        if (m)
            carters_[m->id].push_back(who);
    }
    for (auto& [m, list] : carters_)
        std::sort(list.begin(), list.end());
    // The farm carts' ends (8.2): each farm or works with its work room in the world, to its community's market, else the
    // nearest by path.
    cartEnds_.clear();
    const auto& farms = society_.farms();
    std::set<int> worked;                           // (The farms with a post held now: the counts are a day's.)
    for (const auto& p : society_.positions())
        if (const int farm = society_.farmOfPost(p); farm >= 0)
            if (const auto held = society_.state().careers.positions.find(p.id);
                held != society_.state().careers.positions.end() && !held->second.holder.empty() && society_.resident(held->second.holder))
                worked.insert(farm);
    for (std::size_t i = 0; i < farms.size(); ++i)
    {
        const auto& f = farms[i];
        const auto* place = items::farmPeople().farm(f.def->id);
        if (!place || place->workplaces.empty() || !cells_.count(place->workplaces.front().area))
            continue;
        CartEnd end;
        end.farm = i;
        end.farmId = f.def->id;
        end.area = f.area < items::farms().areas.size() ? items::farms().areas[f.area].id : f.def->area;
        end.name = place->steading.empty() ? marketWords(f.def->id) : place->steading;
        end.cell = place->workplaces.front().area;
        const auto* m = roadMarket(place->community);
        if (!m && !worked.count(int(i)) && f.heldKeepers + f.heldHands <= 0)
            continue;                               // (No cart from a farm nobody works: not looked for by path. Its posts
                                                    // taken move the roster, and it is found then: at a fresh world's start
                                                    // that search ran over every road market for 30 farms not yet staffed,
                                                    // about 156 ms. The repair after doc 79's Phases 4 and 5 merged.)
        if (!m)
        {
            std::size_t best = std::numeric_limits<std::size_t>::max();
            for (const auto& candidate : roadMarkets_)
                if (const auto route = routeBetween(end.cell, candidate.market); !route.empty() && route.size() < best)
                    best = route.size(), m = &candidate;
        }
        if (!m)
            continue;
        end.market = m->id;
        cartEnds_.push_back(std::move(end));
    }
    // Each market's imports (D27): what a wagon (a route a carter can drive to it) or a cart brings it that its workshops and
    // areas don't make (7.7: a shop that only supplies a good doesn't make it), of what its shops set out. With none,
    // everything refills each morning: so too while the DM's rules stop the wagons (caravanRate or wagonsOnRoad 0) or the
    // carts (cartsOnRoad 0), as nothing then comes to refill them.
    std::map<std::string, std::set<std::string>> imports;
    const auto bring = [&](const std::string& to, const std::set<std::string>& goods) {
        const auto* m = tp.find(to);
        if (!m)
            return;
        for (const auto& g : goods)
            if (m->sold.count(g) && !m->crafted.count(g))
                imports[to].insert(g);
    };
    for (const auto& [a, b] : roadPairs_)
        for (const auto& [from, to] : {std::pair{a, b}, std::pair{b, a}})
            if (const auto* src = tp.find(from), *dst = tp.find(to); src && dst && carters_.count(from) && wagonsRun)
            {
                // What this source makes first (its nails: the plainest import), then the rest it sells, in an order seeded
                // by the pair and the good (the same every day), up to what its wagons bring in rotation.
                std::vector<std::tuple<bool, std::uint64_t, std::string>> goods;
                for (const auto& g : src->sold)
                    if (dst->sold.count(g) && !dst->crafted.count(g) && !src->crafted.count(g))
                        goods.push_back({true, lot(from + ">" + to + ":" + g, 6), g});
                for (const auto& g : src->crafted)
                    if (dst->sold.count(g) && !dst->crafted.count(g))
                        goods.push_back({false, lot(from + ">" + to + ":" + g, 6), g});
                std::sort(goods.begin(), goods.end());
                std::set<std::string> brought;
                for (std::size_t i = 0; i < goods.size() && i < ImportsPerSource; ++i)
                    brought.insert(std::get<2>(goods[i]));
                bring(to, brought);
            }
    for (const auto& end : cartEnds_)
        if (cartsRun && farms[end.farm].heldKeepers + farms[end.farm].heldHands > 0)   // (Only a staffed farm's cart comes.)
            bring(end.market, farms[end.farm].def->yields);
    findPorts(imports);                             // (The ports, and what waits for their ships: Phase 5, RatwPort.cpp.)
    society_.setImports(std::move(imports));
}

int World::harvestOf(const std::string& area) const
{
    // An area's harvest this season (8.2): the DM's override while it holds, else slider × season × a seeded weather hash.
    const auto& b = society_.board();
    if (const auto o = b.harvestOverride.find(area); o != b.harvestOverride.end() && calendarDays_ < o->second.until)
        return std::clamp(o->second.word, 0, 2);
    const auto date = calendar::calendarAt(calendarDays_);
    const auto season = std::int64_t(date.year) * 4 + int(date.season);
    // (The season leans it a little: the autumn's gathering in the best, the winter's stores the leanest.)
    double lean = date.season == calendar::Season::Autumn ? .08 : date.season == calendar::Season::Summer ? .04
                  : date.season == calendar::Season::Winter ? -.08 : 0;
    // (While the slider moves, the harvest leans its way too, so a cause and its effects agree: 8.5, Phase 7.)
    if (b.sliderTarget != b.slider)
        lean += b.sliderTarget > b.slider ? .08 : -.08;
    const double score = unit("harvest:" + area, season) - .5 + double(b.slider) / 400. + lean;
    return score < -.22 ? 0 : score > .22 ? 2 : 1;
}

std::vector<std::pair<std::string, int>> World::manifestFor(const std::string& from, const std::string& to, int size,
                                                            const std::string& seed) const
{
    // Up to 5 goods, each one some shop at the destination stocks or buys (8.1): first the source's player-sold surplus
    // (its glut: T3's ore, sold to a smithy that only buys it, goes out on the next wagon), then the destination's imports
    // it is thinnest of (T14), then what the source makes that the destination doesn't, then the largest P gaps, then a
    // luxury for a rich household's want.
    const auto& tp = society_.tradePoints();
    const auto* src = tp.find(from);
    const auto* dst = tp.find(to);
    std::vector<std::pair<std::string, int>> out;
    if (!src || !dst)
        return out;
    const auto wanted = [&](const std::string& g) { return dst->sold.count(g) || dst->bought.count(g); };
    const auto has = [&](const std::string& g) { return src->crafted.count(g) || src->made.count(g) || src->sold.count(g); };
    std::set<std::string> chosen;
    const auto take = [&](const std::string& g, bool glut = false) {
        if (out.size() >= std::size_t(ManifestMost) || chosen.count(g) || !wanted(g) || !(has(g) || (glut && src->bought.count(g))))
            return;
        chosen.insert(g);
        const int shelf = society_.marketShelf(to, g);
        const int count = std::clamp(int(std::lround((std::max(shelf, 4)) * (.5 + .25 * size))), 2, 40);
        out.push_back({g, count});
    };
    const auto& board = society_.board();
    // 1. The glut: the source's goods below 0, the lowest first.
    std::vector<std::pair<double, std::string>> glut, gaps;
    if (const auto m = board.cell.find(from); m != board.cell.end())
        for (const auto& [g, c] : m->second)
            if (const double P = society_.pressure(from, g); P < -.02)
                glut.push_back({P, g});
    std::sort(glut.begin(), glut.end());
    for (std::size_t i = 0; i < glut.size() && out.size() < std::size_t(GlutMost); ++i)
        take(glut[i].second, true);             // (At most two: the rest of the wagon for the far end's thin imports.)
    // 2. The destination's imports this source has, the thinnest first (shelves and back rooms), those not already on
    // their way to it on another wagon before those that are: so every import comes in rotation, a wagon or two apart.
    std::set<std::string> coming;
    for (const auto& d : board.committed)
        if (d.kind == "wagon" && d.to == to)
            for (const auto& [g, n] : d.manifest)
                coming.insert(g);
    for (const auto& c : roads_.caravans)
        if (c.to == to && c.status == "travelling")
            for (const auto& [g, n] : c.manifest)
                coming.insert(g);
    // (Brought longest ago first, then the barest: a rotation, so each of this source's imports, at most ImportsPerSource,
    // is on one of the next few wagons whenever it runs thin.)
    const auto brought = board.broughtIn.find(to);
    const auto broughtOn = [&](const std::string& g) -> std::int64_t {
        if (brought == board.broughtIn.end())
            return -1;
        const auto at = brought->second.find(g);
        return at == brought->second.end() ? -1 : at->second;
    };
    std::vector<std::tuple<bool, std::int64_t, double, std::uint64_t, std::string>> thin;
    for (const auto& g : dst->sold)
        if (society_.imported(to, g) && has(g))
            if (const double full = society_.marketFullness(to, g); full < .999)
                thin.push_back({coming.count(g) > 0, broughtOn(g), full, lot(seed + g, 2), g});
    std::sort(thin.begin(), thin.end());
    for (const auto& [on, when, full, l, g] : thin)
        take(g);
    // 3. What the source makes that the destination doesn't, in a seeded order.
    std::vector<std::pair<std::uint64_t, std::string>> makes;
    for (const auto& g : src->crafted)
        if (!dst->crafted.count(g) && wanted(g))
            makes.push_back({lot(seed + g, 3), g});
    std::sort(makes.begin(), makes.end());
    for (std::size_t i = 0; i < makes.size() && out.size() < 3; ++i)
        take(makes[i].second);
    // 4. The largest gaps (dearer there than here).
    std::set<std::string> touched;
    for (const auto& id : {from, to})
        if (const auto m = board.cell.find(id); m != board.cell.end())
            for (const auto& [g, c] : m->second)
                touched.insert(g);
    for (const auto& g : touched)
        if (const double gap = society_.pressure(to, g) - society_.pressure(from, g); gap > .02)
            gaps.push_back({-gap, g});
    std::sort(gaps.begin(), gaps.end());
    for (const auto& [gap, g] : gaps)
        take(g);
    for (const auto& [l, g] : makes)
        take(g);
    // 5. A rich household's want: a luxury the source sells and the destination's shops set out.
    std::vector<std::pair<std::uint64_t, std::string>> luxuries;
    for (const auto& g : src->sold)
        if (market::groupOf(g) == "luxuries" && dst->sold.count(g))
            luxuries.push_back({lot(seed + g, 4), g});
    std::sort(luxuries.begin(), luxuries.end());
    if (!luxuries.empty())
        take(luxuries.front().second);
    // Then whatever else the source has that the destination sets out and doesn't make.
    std::vector<std::pair<std::uint64_t, std::string>> rest;
    for (const auto& g : src->sold)
        if (dst->sold.count(g) && !dst->crafted.count(g))
            rest.push_back({lot(seed + g, 5), g});
    std::sort(rest.begin(), rest.end());
    for (const auto& [l, g] : rest)
        take(g);
    return out;
}

std::string World::departureNotice(const market::Departure& d, bool atEnd) const
{
    // A carters' notice (8.1), with the calendar's weekday names: "Wagon to Coldwater, Emberday: salt and nails for the
    // general store, and cloth. Tam Reed driving. Escort wanted."
    const auto weekday = calendar::weekdayName(calendar::weekdayOf(double(d.day) + d.hour / 24));
    std::vector<std::string> goods;
    for (const auto& [g, n] : d.manifest)
        goods.push_back(goodWords(g));
    // The shop at the far end that sets out the first good.
    std::string shop;
    if (!d.manifest.empty())
        if (const auto* m = society_.tradePoints().find(d.to))
            for (const auto& till : m->tills)
                if (const auto* p = till.size() > 5 ? society_.position(till.substr(5)) : nullptr)
                {
                    const auto wares = society_.stagedShelfGoods(p->founder);
                    if (std::any_of(wares.begin(), wares.end(), [&](const std::string& w) { return items::baseOf(w) == d.manifest.front().first; }))
                        if (const auto* b = items::businessFor(p->title))
                        {
                            // The shop in plain words: "the general store", "the food stall", "the village smithy".
                            shop = lowered(b->label);
                            if (const auto cut = shop.find(" ("); cut != std::string::npos)
                                shop.erase(cut);
                            if (shop.size() > 6 && shop.compare(shop.size() - 6, 6, " goods") == 0)
                                shop.replace(shop.size() - 6, 6, " store");   // ("General goods": the general store.)
                            break;
                        }
                }
    std::string what;
    if (goods.empty())
        what = "goods";
    else if (!shop.empty())
    {
        const std::size_t first = std::min<std::size_t>(2, goods.size());
        what = listed({goods.begin(), goods.begin() + long(first)}) + " for the " + shop;
        if (goods.size() > first)
            what += ", and " + listed({goods.begin() + long(first), goods.end()});
    }
    else
        what = listed(goods);
    std::string words = atEnd ? "Wagon from " + marketWords(d.from) + ", due " + weekday + ": " + what + "."
                              : "Wagon to " + marketWords(d.to) + ", " + weekday + ": " + what + ".";
    if (const auto* driver = entity(d.driver))
        words += " " + driver->name + " driving.";
    const bool escort = std::any_of(roads_.contracts.begin(), roads_.contracts.end(), [&](const Contract& k) {
        return escortOn(k, d.from, d.to) && k.status == "open";
    });
    if (escort)                                     // (Taken where the wagon sets out, which waits there for its escort.)
        words += atEnd ? " Escort wanted: ask at the board in " + marketWords(d.from) + "." : std::string(" Escort wanted.");
    if (!d.cause.empty())
        words += " Set back: " + d.cause + ".";
    return words;
}

void World::holdBoardToCalendar()
{
    // A Board saved on a calendar since set back, or edited, or damaged (the repair after Phases 4 and 5 merged): its
    // days held to what this calendar could have made, as the ships' weeks are (dropOrphanedShips). A Board day may be
    // one ahead of the market's (a crash that kept the Board's morning with the clock before dawn: boardsDayIsToday);
    // further ahead, its counts are of a day that never came here, so the next pass runs that day's morning (it used to
    // hold every cap shut and the morning back until the clock caught up: no decay, no rundown, no slider step). The
    // same for the slider's day, the faction discounts' week, each till's restock and the roads' rolled days.
    auto& b = society_.stagedBoard();
    const auto marketDay = std::int64_t(std::floor(calendarDays_ - market::DawnHour / 24));
    const auto today = std::int64_t(std::floor(calendarDays_));
    const auto lead = marketDay + 1;
    if (b.day > lead)
        b.day = std::max<std::int64_t>(-1, marketDay - 1);
    if (b.sliderDay > lead)
        b.sliderDay = std::max<std::int64_t>(-1, marketDay - 1);
    if (b.week > lead / 7)
        b.week = std::max<std::int64_t>(-1, marketDay / 7 - 1);
    for (auto it = b.restocked.begin(); it != b.restocked.end();)
        it = it->second > lead ? b.restocked.erase(it) : std::next(it);
    if (b.rolledThrough > today + PostAhead)
        b.rolledThrough = today + PostAhead;
}

void World::postDepartures(std::int64_t today)
{
    // Committed when posted (8.1): two days ahead each route rolls hash(route, d) < frequency for day d, read once, here.
    auto& b = society_.stagedBoard();
    const auto from = std::max(b.rolledThrough + 1, today + 1);
    const auto through = today + PostAhead;
    if (from > through)
        return;
    b.rolledThrough = through;
    if (roadPairs_.empty() || b.committed.size() >= market::CommittedMost)
        return;
    const double rate = std::clamp(society_.rule("caravanRate"), 0, 300) / 100.;
    const int onRoad = std::max(0, society_.rule("wagonsOnRoad"));
    const auto& tp = society_.tradePoints();
    int cityResidents = 0;
    std::string city;
    for (const auto& m : roadMarkets_)
        if (m.residents > cityResidents)
            cityResidents = m.residents, city = m.id;
    for (auto d = from; d <= through; ++d)
    {
        const auto date = calendar::calendarAt(double(d) + .5);
        const double season = date.season == calendar::Season::Winter ? .8 : 1.;
        const double slider = 1 + .4 * double(society_.board().slider) / 100.;
        for (const auto& [x, y] : roadPairs_)
            for (const auto& [a, c] : {std::pair{x, y}, std::pair{y, x}})
            {
                const auto carters = cartersOf(a);
                const auto* A = roadMarket(a);
                const auto* C = roadMarket(c);
                if (carters.empty() || !A || !C)
                    continue;
                // City to a community about every 2-4 days by its size; between communities every 6-10.
                const bool withCity = a == city || c == city;
                const int other = a == city ? C->residents : c == city ? A->residents : std::min(A->residents, C->residents);
                const double every = withCity ? 4 - 2 * std::min(1., other / 30.) : 10 - 4 * std::min(1., other / 30.);
                // × up to 1.5 when the biggest P gap on the route is over 0.2.
                double gap = 0;
                for (const auto& id : {a, c})
                    if (const auto m = b.cell.find(id); m != b.cell.end())
                        for (const auto& [g, cellP] : m->second)
                            gap = std::max(gap, std::abs(society_.pressure(c, g) - society_.pressure(a, g)));
                const double frequency = 1 / every * slider * season * (gap > .2 ? 1.5 : 1) * rate;
                const std::string key = a + ">" + c;
                if (unit(key, d) >= frequency)
                    continue;
                // Capped by carters free that day and wagons on the roads.
                int fromHere = 0, thatDay = 0;
                std::set<std::string> named;
                for (const auto& dep : b.committed)
                    if (dep.kind == "wagon" && dep.day == d)
                    {
                        ++thatDay;
                        fromHere += dep.from == a;
                        named.insert(dep.driver);
                    }
                if (fromHere >= int(carters.size()) || thatDay >= onRoad)
                    continue;
                std::string driver;
                std::uint64_t best = ~0ULL;
                for (const auto& who : carters)
                    if (!named.count(who) && !drivers_.count(who))
                        if (const auto l = lot(key + who, d); l < best)
                            best = l, driver = who;
                if (driver.empty())
                    continue;
                market::Departure dep;
                dep.id = "w" + std::to_string(b.nextDeparture++);
                dep.kind = "wagon";
                dep.from = a;
                dep.to = c;
                dep.driver = driver;
                dep.day = d;
                dep.hour = 7.5 + double(lot(key, d + 1) % 5) * .5;
                dep.size = std::clamp(1 + (A->residents >= 20 && C->residents >= 20) + (society_.board().slider > 40), 1, 3);
                dep.manifest = manifestFor(a, c, dep.size, dep.id);
                dep.posted = calendarDays_;
                if (dep.manifest.empty() || !tp.find(a) || !tp.find(c))
                    continue;                       // (Nothing the far end's shops want: no wagon.)
                // An escort contract posts when a camp is active on the route (8.1).
                const auto route = std::find(roadPairs_.begin(), roadPairs_.end(), std::pair{x, y}) - roadPairs_.begin();
                bool camp = false;
                if (std::size_t(route) < roadRoutes_.size())
                    for (const auto& cellId : roadRoutes_[std::size_t(route)])
                        camp = camp || std::any_of(roads_.camps.begin(), roads_.camps.end(),
                                                   [&](const BanditCamp& k) { return k.active && k.cell == cellId; });
                if (camp && std::none_of(roads_.contracts.begin(), roads_.contracts.end(), [&](const Contract& k) {
                        return escortOn(k, a, c) && (k.status == "open" || k.status == "taken");
                    }))
                    postContract("escort", society_.treasuryOf(c), c, c, 12, 7,
                                 "guarding the next wagon from " + marketWords(a) + " to " + marketWords(c), a);
                b.committed.push_back(dep);
                recordEvent({"wagon posted", dep.id, c, A->market, 0, 0, a, int(dep.manifest.size()), 0,
                             departureNotice(dep, false) + "\n" + departureNotice(dep, true)});
                if (b.committed.size() >= market::CommittedMost)
                    return;
            }
    }
}

int World::cartsCarrying(const std::string& market, const std::string& good) const
{
    // The staffed farms whose morning cart brings this good to this market (8.2): a missed cart's bump is shared out by them.
    int n = 0;
    const auto& farms = society_.farms();
    for (const auto& end : cartEnds_)
        if (end.market == market && end.farm < farms.size() && farms[end.farm].heldKeepers + farms[end.farm].heldHands > 0)
            for (const auto& g : farms[end.farm].def->yields)
                n += g == good;
    return n;
}

void World::missedCart(const std::string& market, const std::vector<std::string>& goods, const std::string& cause)
{
    // A cart that didn't come (8.2, 8.6): its goods a little dearer at its market, and why. Its share of the bump only (the
    // whole +0.05 when every cart of a good misses), so a market fed by many farms doesn't stack a step for each.
    for (const auto& g : goods)
        society_.setPressure(market, g, society_.pressure(market, g) + MissedCartNudge / std::max(1, cartsCarrying(market, g)), cause);
}

void World::rollCarts(std::int64_t today)
{
    // Every morning each staffed farm or works sends a small cart to its nearest market, heavier on Marketday (8.2); its
    // chance by its area's harvest (good 100%, fair 70%, poor 40%). A cart a poor harvest keeps home is a visible bump at its
    // market for its goods ("harvest"); one the roads have no room for (cartsOnRoad) simply doesn't go, with no bump.
    auto& b = society_.stagedBoard();
    const auto& farms = society_.farms();
    const auto& tp = society_.tradePoints();
    const bool marketday = calendar::weekdayOf(double(today) + .5) == calendar::Marketday;
    int carts = 0;
    for (const auto& dep : b.committed)
        carts += dep.kind == "cart";
    for (const auto& c : roads_.caravans)
        carts += c.kind == "cart";
    const int most = std::max(0, society_.rule("cartsOnRoad"));
    for (const auto& end : cartEnds_)
    {
        const auto& f = farms[end.farm];
        if (f.heldKeepers + f.heldHands <= 0)
            continue;                               // (Nobody at its posts: no cart.)
        const auto* m = tp.find(end.market);
        if (!m)
            continue;
        std::vector<std::string> goods;
        for (const auto& g : f.def->yields)
            if (m->sold.count(g) || m->bought.count(g))
                goods.push_back(g);
        if (goods.empty())
            continue;
        if (cartHeld(end.farmId, end.market, today))
            continue;                               // (A staged event holds it today, a lame ox: its own notice and push.)
        const int harvest = harvestOf(end.area);
        const double comes = harvest == 2 ? 1 : harvest == 1 ? .7 : .4;
        if (unit("cart:" + end.farmId, today) >= comes)
        {
            missedCart(end.market, goods, "harvest");   // (The cart that didn't come: a little dearer, and why.)
            continue;
        }
        if (carts >= most || b.committed.size() >= market::CommittedMost)
            continue;                               // (No room on the roads: no cart, and nothing said of a harvest.)
        market::Departure dep;
        dep.id = "f" + std::to_string(b.nextDeparture++);
        dep.kind = "cart";
        dep.from = end.farmId;
        dep.to = end.market;
        dep.day = today;
        dep.hour = 5.5 + double(lot(end.farmId, today) % 4) * .5;
        dep.size = harvest + 1;
        const double load = (harvest == 2 ? 1.5 : harvest == 1 ? 1 : .6) * (marketday ? 1.5 : 1);
        for (std::size_t i = 0; i < goods.size() && dep.manifest.size() < std::size_t(ManifestMost); ++i)
            dep.manifest.push_back({goods[i], std::max(1, int(std::lround(8 * load)))});
        dep.posted = calendarDays_;
        b.committed.push_back(std::move(dep));
        ++carts;
    }
}

void World::setOut()
{
    // The departures whose hour has come (or passed, after a restart) set out.
    auto& b = society_.stagedBoard();
    if (b.committed.empty())
        return;
    const auto today = std::int64_t(std::floor(calendarDays_));
    std::vector<market::Departure> due;
    for (auto it = b.committed.begin(); it != b.committed.end();)
        if (calendarDays_ >= double(it->day) + it->hour / 24)
        {
            due.push_back(std::move(*it));
            it = b.committed.erase(it);
        }
        else
            ++it;
    for (auto& d : due)
    {
        if (d.kind == "cart")
        {
            if (d.day >= today)                     // (A cart is the morning's: one from a day gone by, after a restart, never comes.)
                launchCart(d);
            continue;
        }
        const auto* from = roadMarket(d.from);
        const auto* to = roadMarket(d.to);
        if (!from || !to)
        {
            recordEvent({"wagon cancelled", d.id, d.to, {}, 0, 0, d.from, 0, 0, "its market is gone"});
            continue;
        }
        // Its carter, else another of the yard's free; none, or the roads full: set back a day, announced.
        int out = 0;
        for (const auto& c : roads_.caravans)
            out += c.kind == "wagon" && c.status != "arrived";
        std::string driver;
        const auto carters = cartersOf(d.from);
        const auto free = [&](const std::string& who) {
            const auto* e = entity(who);
            return e && !e->dead && !drivers_.count(who) && std::find(carters.begin(), carters.end(), who) != carters.end();
        };
        if (free(d.driver))
            driver = d.driver;
        else
            for (const auto& who : carters)
                if (free(who))
                {
                    driver = who;
                    break;
                }
        std::string cause = driver.empty() ? "no carter to be had" : out >= std::max(0, society_.rule("wagonsOnRoad")) ? "every wagon out on the roads" : "";
        if (!cause.empty())
        {
            if (double(today) - d.posted <= 5 && b.committed.size() < market::CommittedMost)   // (Set back a few days at most.)
            {
                d.cause = cause;
                d.day = std::max(d.day, today) + 1;
                b.committed.push_back(d);
                recordEvent({"wagon delayed", d.id, d.to, from->market, 0, 0, d.from, 0, 0,
                             departureNotice(d, false) + "\n" + departureNotice(d, true)});
            }
            else
                recordEvent({"wagon cancelled", d.id, d.to, from->market, 0, 0, d.from, 0, 0, cause});
            continue;
        }
        Caravan c;
        c.id = "cv" + std::to_string(roads_.nextId++);
        c.kind = "wagon";
        c.from = d.from;
        c.to = d.to;
        c.manifest = d.manifest;
        c.driver = driver;
        c.departure = d.id;
        if (!launchCaravan(c, *from, *to))           // (No road between them now, a host's edit: it can't go, and says so.)
            recordEvent({"wagon cancelled", d.id, d.to, from->market, 0, 0, d.from, 0, 0, "the road is closed"});
    }
}

Caravan* World::launchCaravan(Caravan c, const Town& from, const Town& to)
{
    // A wagon sets out (8.1, 8.6): from its carter's yard (where his post is, in its market), else the market's place; the
    // escorts who took the road's escort and the letters nobody took go with it; the source's glut on its goods halved
    // toward 0 and its tills' stock of them over target sent on, as it loads.
    c.startCell = from.market, c.startX = from.marketX, c.startY = from.marketY;
    if (const auto* job = c.driver.empty() ? nullptr : society_.jobOf(c.driver); job && roadMarketOf(job->work.cell) == &from &&
                                                                                 cells_.count(job->work.cell))
        c.startCell = job->work.cell, c.startX = job->work.x, c.startY = job->work.y;
    c.endCell = to.market, c.endX = to.marketX, c.endY = to.marketY;
    c.route = routeBetween(c.startCell, c.endCell);
    if (c.route.size() < 2)
        return nullptr;
    c.departed = calendarDays_;
    c.due = calendarDays_ + DueDays;
    c.guards = 1 + std::min(3, from.guards / 10);
    // A seeded mishap on the way, now and then (8.1: late, with a cause).
    if (unit(c.id, 7) < .08)
        c.delayAt = 1 + std::size_t(lot(c.id, 8) % std::max<std::size_t>(1, c.route.size() - 1));
    for (const auto& [g, n] : c.manifest)
    {
        const double P = society_.pressure(c.from, g);
        c.sourceP[g] = P;
        if (P < 0)
            society_.setPressure(c.from, g, P / 2, "caravan");
        society_.sendOn(c.from, g);
    }
    const std::string carrier = "caravan:" + c.id;
    for (auto& k : roads_.contracts)
    {
        // The escort of this road (from here to there: not another market's wagon to the same town), and not one already out
        // with an earlier wagon of it.
        // (An older save's escort names no road. One a player holds while standing in another market waits for that
        // market's wagon, never going with this one, which would reach the town without them and send their pay back;
        // one held anywhere else goes with the first wagon to its town, as before, which tells them where it waits. And
        // it is the road's of the first wagon it goes with. Third repair.)
        const auto elsewhere = [&] {
            if (!k.from.empty() || k.status != "taken")
                return false;
            const auto* holder = entity(k.taker);
            if (!holder)
                return true;                        // (Away from the world: kept for them.)
            const auto* at = holder->npc ? nullptr : roadMarketOf(holder->cellId);
            return at && at != &from && at != &to;
        }();
        if (escortOn(k, from.id, to.id) && !elsewhere &&
            std::none_of(roads_.caravans.begin(), roads_.caravans.end(), [&](const Caravan& out) {
                return out.status == "travelling" && out.from == from.id && out.to == to.id &&
                       std::find(out.escorts.begin(), out.escorts.end(), k.taker) != out.escorts.end();
            }))
        {
            // Nobody has taken the escort in a couple of days: one of the town guard goes.
            if (k.status == "open" && calendarDays_ - k.created >= 2)
                for (const auto& p : society_.positions())
                {
                    const auto& holder = society_.state().careers.positions.at(p.id).holder;
                    const auto* guard = holder.empty() ? nullptr : entity(holder);
                    if (p.role != "guard" || !guard || guard->dead || roadMarketOf(p.work.cell) != &from)
                        continue;
                    k.status = "taken";
                    k.taker = holder;
                    recordEvent({"contract taken", holder, k.poster, guard->cellId, 0, 0, {}, 0, k.reward, k.kind + ": " + k.detail});
                    break;
                }
            if (k.status == "taken" && std::find(c.escorts.begin(), c.escorts.end(), k.taker) == c.escorts.end())
            {
                c.escorts.push_back(k.taker);
                if (k.from.empty())
                    k.from = from.id;
            }
        }
        // Letters nobody has taken in a week go with the carters.
        if (k.kind == "courier" && k.status == "open" && calendarDays_ - k.created >= 7)
            if (const auto* recipient = entity(k.target), *poster = entity(k.poster);
                recipient && poster && roadMarketOf(recipient->cellId) == &to && roadMarketOf(poster->cellId) == &from)
            {
                k.status = "taken";
                k.taker = carrier;
                c.letters.push_back(k.id);
            }
    }
    if (!c.escorts.empty() || !c.driver.empty())
        c.waitUntil = calendarDays_ + 2.0 / 24;      // (It waits two game hours for its escorts and its carter.)
    for (const auto& who : c.escorts)
        if (const auto* e = entity(who); e && !e->npc)
            notice(who, "The wagon for " + marketWords(to.id) + " is making ready at the market in " + marketWords(from.id) +
                            ". It will wait a little for you.");
    const auto* driver = entity(c.driver);
    recordEvent({"caravan departs", c.id, to.id, c.startCell, 0, 0, c.departure, int(c.manifest.size()), 0,
                 std::to_string(c.guards) + " guards" + (driver ? ", " + driver->name + " driving" : std::string())});
    roads_.caravans.push_back(std::move(c));
    if (!roads_.caravans.back().driver.empty())
        drivers_[roads_.caravans.back().driver] = roads_.caravans.back().id;
    return &roads_.caravans.back();
}

void World::launchCart(const market::Departure& d)
{
    // A farm's cart (8.2): its carter the farm's keeper or a hand, a real NPC at a real post, from the farm's work room to
    // its market's place; one way (the carter walks back to his post on his own).
    const auto end = std::find_if(cartEnds_.begin(), cartEnds_.end(), [&](const CartEnd& e) { return e.farmId == d.from; });
    const auto* to = roadMarket(d.to);
    if (end == cartEnds_.end() || !to)
        return;
    std::vector<std::string> goods;
    for (const auto& [g, n] : d.manifest)
        goods.push_back(g);
    const auto missed = [&](const std::string& cause, const std::string& why) {
        // (Rolled, but nobody at the farm to take it, or no way through: it doesn't come, and the market hears why.)
        missedCart(d.to, goods, cause);
        recordEvent({"cart missed", d.id, d.to, end->cell, 0, 0, d.from, int(goods.size()), 0, why});
    };
    std::string driver;
    const Position* post = nullptr;
    std::uint64_t best = ~0ULL;
    for (const auto& p : society_.positions())
    {
        if (society_.farmOfPost(p) != int(end->farm))
            continue;
        const auto held = society_.state().careers.positions.find(p.id);
        if (held == society_.state().careers.positions.end() || held->second.holder.empty())
            continue;
        const auto& who = held->second.holder;
        const auto* e = entity(who);
        if (!e || e->dead || !e->npc || drivers_.count(who))
            continue;
        if (const auto l = lot(d.id + who, 1); l < best)
            best = l, driver = who, post = &p;
    }
    if (driver.empty() || !post)
        return missed("short-handed", "no hand to spare at " + end->name);
    Caravan c;
    c.id = "cv" + std::to_string(roads_.nextId++);
    c.kind = "cart";
    c.from = d.from;
    c.to = d.to;
    c.manifest = d.manifest;
    c.driver = driver;
    c.departure = d.id;
    c.guards = 0;
    c.startCell = cells_.count(post->work.cell) ? post->work.cell : end->cell;
    c.startX = post->work.x, c.startY = post->work.y;
    c.endCell = to->market, c.endX = to->marketX, c.endY = to->marketY;
    c.route = routeBetween(c.startCell, c.endCell);
    if (c.route.empty())
        return missed("road closed", "no way through to " + marketWords(d.to));
    c.departed = calendarDays_;
    c.due = calendarDays_ + DueDays;
    c.waitUntil = calendarDays_ + 1.0 / 24;
    for (const auto& [g, n] : c.manifest)
        c.sourceP[g] = 0;
    recordEvent({"cart departs", c.id, d.to, c.startCell, 0, 0, d.from, int(c.manifest.size()), 0, end->name});
    roads_.caravans.push_back(std::move(c));
    drivers_[driver] = roads_.caravans.back().id;
}

void World::stagedArrival(Caravan& c)
{
    // At the far end (8.6): the gap on its goods closed by a fifth (never up), the market's shelves of them refilled and the
    // back rooms filled, the keepers unloading; a cart's goods pulled halfway toward its harvest's baseline (only down), a
    // Marketday cart's stalls stocked a little over; a wagon's news of its source.
    const bool cart = c.kind == "cart";
    int harvest = 1;
    if (cart)
        if (const auto end = std::find_if(cartEnds_.begin(), cartEnds_.end(), [&](const CartEnd& e) { return e.farmId == c.from; });
            end != cartEnds_.end())
            harvest = harvestOf(end->area);
    const bool marketday = calendar::weekdayOf(calendarDays_) == calendar::Marketday;
    const double baseline = harvest == 2 ? -.15 : harvest == 0 ? .15 : 0;
    for (const auto& [g, n] : c.manifest)
    {
        const double P = society_.pressure(c.to, g);
        if (cart)
        {
            if (P > baseline + 1e-4)
                society_.setPressure(c.to, g, (P + baseline) / 2, "harvest");
        }
        else if (const auto s = c.sourceP.find(g); s != c.sourceP.end() && P > s->second + 1e-4)
            society_.setPressure(c.to, g, P - ArrivalNudge * (P - s->second), "caravan");
        society_.arrivalRefill(c.to, g, cart && marketday ? 1.5 : 1);
    }
    if (cart)
    {
        if (marketday)
            recordEvent({"cart stocks the stalls", c.id, c.to, c.cell, 0, 0, c.from, int(c.manifest.size()), 0, "Marketday"});
        return;
    }
    // The news it carries: the source market's top cause.
    auto& b = society_.stagedBoard();
    std::string words;
    double most = .05;
    if (const auto m = b.cell.find(c.from); m != b.cell.end())
        for (const auto& [g, cellP] : m->second)
            if (const double P = society_.pressure(c.from, g); std::abs(P) > most)
            {
                most = std::abs(P);
                const auto good = goodWords(g);
                words = good + (pluralGood(good) ? " are " : " is ") + (P < 0 ? "cheap in " : "dear in ") + marketWords(c.from);
            }
    if (words.empty())
        if (const int times = society_.marketTimes(c.from); times != 0)
            words = std::string("times are ") + (times <= -2 ? "hard" : times < 0 ? "lean" : times >= 2 ? "booming" : "good") + " in " +
                    marketWords(c.from);
    if (!words.empty())
        b.news[c.to] = {c.from, words, calendarDays_};
}

void World::roadsMorning(std::int64_t today)
{
    // The roads' day (8.1, 8.2, 8.6): overdue wagons posted and pressing their goods dearer at their end; the troubles of
    // the week and the harvests set each market's Times; the routes rolled two days ahead; the farm carts rolled.
    ensureRoadMarkets();
    auto& b = society_.stagedBoard();
    for (auto& c : roads_.caravans)
    {
        if (c.kind != "wagon" || c.status != "travelling" || c.due <= 0 || calendarDays_ <= c.due + LateAfterDays)
            continue;
        if (!c.overdue)
        {
            c.overdue = true;
            b.troubles.push_back({calendarDays_, c.to, "late"});
            // (With its cause where it met one; "No word of it yet" where nobody knows why.)
            recordEvent({"wagon overdue", c.id, c.to, c.cell, 0, 0, c.departure, 0, 0,
                         "Overdue: the wagon from " + marketWords(c.from) +
                             (c.lateCause.empty() ? std::string(". No word of it yet.") : ", held up by " + c.lateCause + ".")});
        }
        if (c.latePushed + 1e-9 < LateMost)
        {
            const double step = std::min(LateNudge, LateMost - c.latePushed);
            c.latePushed += step;
            for (const auto& [g, n] : c.manifest)
                society_.setPressure(c.to, g, society_.pressure(c.to, g) + step, "late");
        }
    }
    b.troubles.erase(std::remove_if(b.troubles.begin(), b.troubles.end(), [&](const market::Trouble& t) {
                         return calendarDays_ - t.day > 7 || !roadMarket(t.market);
                     }),
                     b.troubles.end());
    while (b.troubles.size() > market::TroublesKept)
        b.troubles.erase(b.troubles.begin());
    for (auto it = b.news.begin(); it != b.news.end();)
        it = calendarDays_ - it->second.day > 7 || !roadMarket(it->first) ? b.news.erase(it) : std::next(it);
    for (auto it = b.harvestOverride.begin(); it != b.harvestOverride.end();)
        it = calendarDays_ >= it->second.until ? b.harvestOverride.erase(it) : std::next(it);
    // Times (5.3): each market's harvests (good +15, poor -15, averaged over its carts' areas), raids on its wagons (-10
    // each, at most -30) and late wagons (-5 each, at most -15) in the last 7 days.
    std::map<std::string, int> shift;
    std::map<std::string, std::pair<int, int>> harvests;   // Market -> sum of (word - 1), carts.
    const auto& farms = society_.farms();
    for (const auto& end : cartEnds_)
    {
        if (end.farm >= farms.size() || farms[end.farm].heldKeepers + farms[end.farm].heldHands <= 0)
            continue;                               // (A farm nobody works sends no cart, and its harvest is nobody's talk.)
        auto& h = harvests[end.market];
        h.first += harvestOf(end.area) - 1;
        ++h.second;
    }
    std::map<std::string, int> harvestShift;       // (The harvest's part, for the Times' cause in words: Phase 7.)
    for (const auto& [m, h] : harvests)
        if (h.second > 0)
            shift[m] += harvestShift[m] = int(std::lround(15.0 * h.first / h.second));
    society_.setHarvestShift(std::move(harvestShift));
    std::map<std::string, int> raids, lates;
    for (const auto& t : b.troubles)
        ++(t.kind == "raid" ? raids : lates)[t.market];
    for (const auto& [m, n] : raids)
        shift[m] -= std::min(30, 10 * n);
    for (const auto& [m, n] : lates)
        shift[m] -= std::min(15, 5 * n);
    society_.setTimesShift(std::move(shift));
    // Departures for markets that are gone are dropped (a host's edit), the rest kept to their promise.
    b.committed.erase(std::remove_if(b.committed.begin(), b.committed.end(), [&](const market::Departure& d) {
                          return !roadMarket(d.to) || (d.kind == "wagon" ? !roadMarket(d.from) : !items::farmPeople().farm(d.from));
                      }),
                      b.committed.end());
    postDepartures(today);
    rollCarts(today);
}

void World::raidCargo(Caravan& c, BanditCamp& camp)
{
    // A raid on a staged wagon (8.1, 8.6): no coins into the camp; its manifest becomes goods at the camp (a goods faucet,
    // at most about 40p a camp, for whoever clears it), its goods dearer at its destination at once, a trouble for its
    // Times; its carter comes home robbed or hurt, never killed.
    const auto loot = "bandits:" + camp.id;
    society_.openAccount(loot);
    double worth = 0;
    if (const auto* held = society_.account(loot))
        for (const auto& [item, n] : held->stock)
            worth += double(n) * double(Society::catalogPrice(item));
    for (const auto& [g, n] : c.manifest)
    {
        const double each = std::max<double>(1, double(Society::catalogPrice(g)));
        const int room = int(std::floor((CampLootMost - worth) / each));
        if (const int pieces = std::min(n, room); pieces > 0 && society_.create(loot, g, pieces, "raided cargo"))
            worth += pieces * each;
        society_.setPressure(c.to, g, society_.pressure(c.to, g) + RaidNudge, "raid");
    }
    society_.stagedBoard().troubles.push_back({calendarDays_, c.to, "raid"});
    if (auto* driver = entity(c.driver); driver && !driver->dead)
    {
        if (unit(c.id, 11) < .5)
        {
            auto i = injury::given("bruised_ribs", 1, "", "bandits");
            i.id = injuryId() + driver->id;
            i.gotDay = calendarDays_;
            injury::give(driver->injuries, i);
            recordEvent({"carter hurt", c.driver, camp.id, c.cell, 0, 0, {}, 0, 0, "bruised in the raid"});
        }
        else
        {
            society_.stagedSet(c.driver, 0);
            recordEvent({"carter robbed", c.driver, camp.id, c.cell, 0, 0, {}, 0, 0, "his purse taken in the raid"});
        }
    }
}

void World::dropOrphanedRoads()
{
    // After a load (8.1, 16.5-6): a caravan whose ends are gone is removed, its letters reopened at their origin (or back to
    // the writer if that is gone too) and its escorts reopened (or their pay back); an older save's wagon (its `account` the
    // old economy's) comes home empty; contracts whose market is gone refund their posters.
    ensureRoadMarkets();
    const auto reopen = [&](const Caravan& c, bool keepLetters) {
        const std::string carrier = "caravan:" + c.id;
        for (auto& k : roads_.contracts)
        {
            if (k.status != "taken")
                continue;
            if (k.kind == "courier" && (k.taker == carrier || (!c.account.empty() && k.taker == c.account)))
            {
                if (keepLetters)                   // (Open again at its origin, for another courier.)
                    k.status = "open", k.taker.clear();
                else
                    settleContract(k, "expired", {});
            }
            else if (escortOn(k, c.from, c.to) && std::find(c.escorts.begin(), c.escorts.end(), k.taker) != c.escorts.end())
            {
                if (roadMarket(k.town) && (k.from.empty() || roadMarket(k.from)))
                    k.status = "open", k.taker.clear();
                else
                    settleContract(k, "expired", {});
            }
        }
    };
    for (auto& c : roads_.caravans)
    {
        const bool ends = c.kind == "cart" ? roadMarket(c.to) != nullptr : roadMarket(c.from) && roadMarket(c.to);
        if (!ends)
        {
            reopen(c, roadMarket(c.from) != nullptr);
            c.status = "arrived";                  // (Dropped below.)
            recordEvent({"caravan dropped", c.id, c.to, c.cell, 0, 0, c.departure, 0, 0, "its market is gone"});
            continue;
        }
        if (!c.account.empty())
        {
            // An older save's: its load was the old economy's; it comes home empty, its letters and escorts open again.
            reopen(c, true);
            c.letters.clear();
            c.escorts.clear();
            c.manifest.clear();
            c.account.clear();
            if (c.status == "travelling")
                c.status = "returning";
        }
        // Its ends' cells, from its markets where an older save has none.
        if (c.startCell.empty() || !cells_.count(c.startCell))
            if (const auto* home = roadMarket(c.kind == "cart" ? c.to : c.from))
                c.startCell = home->market, c.startX = home->marketX, c.startY = home->marketY;
        if (c.endCell.empty() || !cells_.count(c.endCell))
            if (const auto* there = roadMarket(c.to))
                c.endCell = there->market, c.endX = there->marketX, c.endY = there->marketY;
    }
    roads_.caravans.erase(std::remove_if(roads_.caravans.begin(), roads_.caravans.end(),
                                         [](const Caravan& c) { return c.status == "arrived"; }),
                          roads_.caravans.end());
    // An escort whose road starts at a market that is gone has no wagon to go with: its pay back to its poster.
    for (auto& k : roads_.contracts)
        if ((k.status == "open" || k.status == "taken") && k.kind == "escort" && !k.from.empty() && !roadMarket(k.from))
            settleContract(k, "expired", {});
    for (auto& k : roads_.contracts)
        if ((k.status == "open" || k.status == "taken") && (k.kind == "courier" || k.kind == "escort" || k.kind == "bounty") &&
            !k.town.empty() && !roadMarket(k.town) && !town(k.town) && k.taker.rfind("caravan:", 0) != 0)
        {
            // (A market, a law town or a town a contract names: a deleted one refunds its poster. A courier contract's town is
            // its letter's town, and a law town of the same name is the letter's still.)
            bool known = false;
            for (const auto& [id, c] : cells_)
                if (c.region == k.town)
                {
                    known = true;
                    break;
                }
            if (!known)
                settleContract(k, "expired", {});
        }
    // Departures for markets that are gone (a host's edit) dropped, with their notices; the rest keep their promise.
    auto& committed = society_.stagedBoard().committed;
    for (auto it = committed.begin(); it != committed.end();)
        if (!roadMarket(it->to) || (it->kind == "wagon" ? !roadMarket(it->from) : !items::farmPeople().farm(it->from)))
        {
            recordEvent({"wagon cancelled", it->id, it->to, {}, 0, 0, it->from, 0, 0, "its market is gone"});
            it = committed.erase(it);
        }
        else
            ++it;
    drivers_.clear();
    for (const auto& c : roads_.caravans)
        if (!c.driver.empty())
            drivers_[c.driver] = c.id;
}
} // namespace ratw
