#include "RatwWorld.h"

#include "RatwInjury.h"
#include "RatwItems.h"
#include "RatwNames.h"

#include <algorithm>
#include <cctype>
#include <cmath>

// Contracts for goods (Docs/Design/35-items-crafting-industry.md, Part 7). A town's own buyers (the Town Works, the
// watch, the church; Society::townBuyers) ask for what their town's shops couldn't sell them, and the world posts each
// ask as a "procure" contract in that town: so many of a good, for a reward of the goods' price and a premium, held in
// escrow from the buyer's own funds. Anyone in town can hear of it from a merchant and take it on; the one who took it
// delivers what they carry of it, any quality, to any merchant of that town, a few at a time if they like, and is paid
// by the piece. The goods go to the buyer. A contract left undelivered lapses and the rest of the reward goes back.
namespace ratw
{
namespace
{
std::string lowerFirst(std::string s)
{
    if (!s.empty())
        s[0] = char(std::tolower(static_cast<unsigned char>(s[0])));
    return s;
}
// "upper_accord" -> "Upper Accord": a town's id as a name.
std::string placeTitle(std::string id)
{
    bool start = true;
    for (auto& ch : id)
    {
        if (ch == '_' || ch == '-')
            ch = ' ';
        else if (start)
            ch = char(std::toupper(static_cast<unsigned char>(ch)));
        start = ch == ' ';
    }
    return id;
}
} // namespace

void World::postProcurements()
{
    for (const auto& want : society_.takeProcurements())
    {
        // One standing ask at a time for each buyer and good.
        const bool asked = std::any_of(roads_.contracts.begin(), roads_.contracts.end(), [&](const Contract& k) {
            return k.kind == "procure" && k.poster == want.account && k.item == want.item && (k.status == "open" || k.status == "taken");
        });
        if (asked || want.quantity <= 0)
            continue;
        const auto* funds = society_.account(want.account);
        const auto reward = std::int64_t(std::ceil(want.price * want.quantity * items::contractPremium()));
        if (!funds || funds->cash < reward)
            continue;                                   // (It can't pay for it: it does without.)
        const auto* good = items::good(want.item);
        const std::string what = std::to_string(want.quantity) + " " + lowerFirst(good ? good->name : want.item);
        auto& c = postContract("procure", want.account, want.community, want.account, reward, 7,
                               names::capitalised(want.buyer) + " of " + placeTitle(want.community) + " wants " + what);
        c.item = want.item;
        c.quantity = want.quantity;
    }
}

std::string World::reckonNow()
{
    society_.reckon(std::int64_t(std::floor(calendarDays_)), true);
    std::string text = "The reckoning:";
    for (const auto& line : noteReckonings())
        text += "\n" + line;
    absorbJournal();
    return text;
}

bool World::churchCares(const std::string& player, const std::string& clergy) const
{
    // A poor wolf with wounds still healing, before a priest or chapel keeper (doc 42, Phase 5: care for the poor).
    const auto* p = entity(player);
    const auto* c = entity(clergy);
    const auto* post = society_.jobOf(clergy);
    const auto* purse = society_.account(player);
    if (!p || !c || p->npc || !c->npc || c->dead || !post || !Society::clergy(post->title) || !purse || purse->cash >= PoorPurse)
        return false;
    return std::any_of(p->injuries.begin(), p->injuries.end(), [](const Injury& i) { return i.kind == "acute" && i.restLeft > 1; });
}

Result World::churchCare(const std::string& player, const std::string& clergy)
{
    if (!churchCares(player, clergy))
        return {false, "The church tends those who can't pay a healer, and whose wounds are still healing.", clergy};
    auto* p = entity(player);
    const auto* c = entity(clergy);
    if (p->cellId != c->cellId || std::hypot(p->position.x - c->position.x, p->position.y - c->position.y) > 3)
        return {false, "Come closer.", clergy};
    const auto today = std::int64_t(std::floor(calendarDays_));
    if (const auto last = churchCared_.find(player); last != churchCared_.end() && last->second == today)
        return {false, c->name + " has tended you today already. Rest now.", clergy};
    const auto church = society_.churchOf(society_.treasuryOfResident(clergy));
    const auto* stock = society_.account(church);
    std::string bandage;
    for (const auto& kind : stock ? Society::kindsHeld(*stock, "bandages") : std::vector<std::string>{})
        bandage = kind;
    if (bandage.empty())
        return {false, c->name + " has no bandages left to spare; the church is waiting on the chandler and the apothecary.", clergy};
    society_.consume(church, bandage, 1, "the church's care");
    // A quarter of each wound's rest, never all of it: care speeds rest, it doesn't replace it (doc 38).
    for (auto& i : p->injuries)
        if (i.kind == "acute" && i.restLeft > 1)
            i.restLeft = std::max(1.0, i.restLeft - i.restFull / 4);
    churchCared_[player] = today;
    recordEvent({"church care", clergy, player, c->cellId, 0, 0, "bandages", 1, 0, "tended a poor wolf's wounds"});
    return {true, c->name + " cleans and binds your wounds, and asks nothing for it. Rest will do the rest, sooner now.", clergy};
}

std::vector<std::string> World::noteReckonings()
{
    std::vector<std::string> told;
    // The reckoning (doc 42; weekly): each town's tax and tithes, told once.
    for (const auto& r : society_.takeReckonings())
    {
        const auto town = r.treasury == "treasury" ? society_.capital() : r.treasury.substr(r.treasury.find(':') + 1);
        WorldEvent e;
        e.kind = "reckoning";
        e.actor = r.treasury;
        e.target = r.church;
        e.coins = r.tax;
        e.quantity = r.payers;
        e.detail = placeTitle(town.empty() ? r.treasury : town) + "'s reckoning: " + std::to_string(r.tax) + "p in tax from " +
                   std::to_string(r.payers) + " of " + std::to_string(r.residents) + " wolves, " + std::to_string(r.tithes) +
                   "p in tithes" + (r.toCapital ? ", " + std::to_string(r.toCapital) + "p sent to the capital" : std::string());
        told.push_back(e.detail);
        recordEvent(std::move(e));
    }
    // A town falling into disrepair, or mended again (doc 42, Phase 5).
    for (const auto& n : society_.takeTownNews())
    {
        WorldEvent e;
        e.kind = n.what == "mended" ? "town mended" : "town in disrepair";
        e.actor = n.community;
        e.detail = n.what == "mended" ? placeTitle(n.community) + "'s streets and roofs are mended again"
                                      : placeTitle(n.community) + " is falling into disrepair: cracked plaster, missing tiles";
        recordEvent(std::move(e));
    }
    // What the treasuries, churches and houses spent of their surplus today (the rule against hoarding).
    for (const auto& s : society_.takeSpendings())
    {
        WorldEvent e;
        e.kind = "surplus spent";
        e.actor = s.collector;
        e.coins = s.total;
        e.detail = placeTitle(s.community) + ", " + s.collector + ": " + s.detail;
        recordEvent(std::move(e));
    }
    return told;
}

// ------------------------------------------------------------------ Residents filling contracts for goods (doc 42, Phase 4)

namespace
{
constexpr double OpenToPlayersDays = .9;            // A contract for goods is the players' alone this long (about a day).
// (As RatwRoads.cpp's: a steady lot, and a distance.)
std::uint64_t lot(const std::string& a, std::int64_t b)
{
    std::uint64_t h = 1469598103934665603ULL ^ std::uint64_t(b) * 0x9E3779B97F4A7C15ULL;
    for (unsigned char c : a)
        h = (h ^ c) * 1099511628211ULL;
    return h ^ (h >> 29);
}
double apart(Vec2 a, Vec2 b) { return std::hypot(a.x - b.x, a.y - b.y); }
}

std::pair<std::string, int> World::sourceFor(const Contract& k) const
{
    // A shop (in any town; its own first) whose shelves have the goods to spare, at a price the reward covers.
    const int left = k.quantity - k.delivered;
    const auto* good = items::good(k.item);
    if (left <= 0 || !good || k.reward <= 0)
        return {};
    const std::int64_t price = std::max(1, good->price);
    if (price * left > k.reward)
        return {};
    std::string best;
    int bestSpare = 0;
    bool bestHome = false;
    for (const auto& r : society_.authored().residents)
    {
        // A shop with it to spare, or someone who brings it in from the land (a quarry's stone, a farm's grain).
        const auto* producer = items::producerFor(r.workLabel);
        if ((r.role != "merchant" && !producer) || !society_.resident(r.id) || r.id == k.poster)
            continue;
        const auto* e = entity(r.id);
        const auto* shelves = society_.account(society_.tillOf(r.id));
        if (!e || e->dead || !shelves)
            continue;
        const int spare = Society::stockAll(*shelves, k.item) - (r.role == "merchant" ? Society::GoodsKept : 0);
        const bool home = lawTown(r.home.cell) == k.town;
        if (spare > 0 && (home > bestHome || (home == bestHome && spare > bestSpare)))
            best = r.id, bestSpare = spare, bestHome = home;
    }
    return {best, std::min(bestSpare, left)};
}

std::int64_t World::provisionTraveller(const std::string& traveller, const std::string& payer, const std::string& town,
                                      const std::string& toCell, std::int64_t budget)
{
    const auto* who = entity(traveller);
    if (!who || budget <= 0)
        return 0;
    const auto route = routeBetween(who->cellId, toCell);
    const int meals = std::max(2, int(std::ceil(double(route.empty() ? 20 : route.size()) * 2 / 10)));
    std::int64_t spent = 0;
    int got = 0;
    for (const auto& p : society_.positions())
    {
        if (got >= meals)
            break;
        const auto* at = p.role == "merchant" ? townOf(p.work.cell) : nullptr;
        const auto held = society_.state().careers.positions.find(p.id);
        if (!at || at->id != town || held == society_.state().careers.positions.end() || held->second.holder.empty())
            continue;
        const auto& keeper = held->second.holder;
        const auto till = society_.tillOf(keeper);
        while (got < meals)
        {
            const auto* shelves = society_.account(till);
            const auto food = shelves && till != payer ? Society::bestFood(*shelves) : std::string();
            const auto* meal = food.empty() ? nullptr : items::good(food);
            const auto price = std::int64_t(std::ceil(std::max(1, meal ? meal->price : 1) * society_.priceFactor(keeper, items::baseOf(food))));
            if (food.empty() || spent + price > budget || !society_.sale(till, payer, food, 1, price, "provisions for the road") ||
                !society_.shift(payer, traveller, food, 1, 0, "provisions for the road"))
                break;
            spent += price;
            ++got;
        }
    }
    if (const std::int64_t money = std::min<std::int64_t>(budget - spent, std::int64_t(meals - got) * 4);
        money > 0 && society_.shift(payer, traveller, "", 0, money, "money for the road"))
        spent += money;
    return spent;
}

void World::provisionCarrier(Contract& k, int quantity)
{
    // Provisions for the road (the user, 2026-10-05): a carrier sent to another town for the goods is given food for
    // the way by whoever wants them, from its own purse (or, if it has too little, out of the reward held for the
    // contract, above the goods' own price).
    const auto* job = society_.jobOf(k.source);
    if (!job || residentTown(k.source) == k.town)
        return;
    const auto& at = job->role == "merchant" ? job->serve : job->work;
    const auto* poster = society_.account(k.poster);
    if (poster && poster->cash >= 20)
        provisionTraveller(k.taker, k.poster, k.town, at.cell, poster->cash / 4);
    else
    {
        const auto* good = items::good(k.item);
        const auto above = k.reward - std::int64_t(std::max(1, good ? good->price : 1)) * quantity;
        k.reward -= provisionTraveller(k.taker, "contract:" + k.id, k.town, at.cell, above);
    }
}

void World::residentsFillContracts(std::set<std::string>& busy)
{
    const auto today = std::int64_t(std::floor(calendarDays_));
    for (auto& k : roads_.contracts)
    {
        if (k.kind != "procure" || k.status != "open" || calendarDays_ - k.created < OpenToPlayersDays)
            continue;
        const auto [source, n] = sourceFor(k);
        if (source.empty() || n <= 0)
            continue;
        // Someone of the town that wants it, out of work, carries it: by lot.
        std::string chosen;
        std::uint64_t best = ~0ULL;
        for (const auto& [id, life] : society_.state().residents)
        {
            const auto* e = entity(id);
            // Someone out of work, labouring for the Town Works, working out of town, or whose work is carrying
            // (a porter, a messenger, a carter).
            const auto* job = society_.jobOf(id);
            const bool carries = job && job->role == "civilian" &&
                                 (job->title.find("carries") != std::string::npos || job->title.find("carrying") != std::string::npos ||
                                  job->title.find("hauling") != std::string::npos || job->title.find("messages") != std::string::npos ||
                                  job->title.find("loads") != std::string::npos);
            if (!e || e->dead || e->transient || e->age < 16 || e->age >= Society::RetireAge || busy.count(id) ||
                (job && !carries) || society_.apprenticedTo(id) || lawTown(life.homeCell) != k.town)
                continue;
            if (const auto drawn = lot(k.id + id, today); drawn < best)
                best = drawn, chosen = id;
        }
        if (chosen.empty())
            continue;
        k.status = "taken";
        k.taker = chosen;
        k.source = source;
        k.carried = 0;
        busy.insert(chosen);
        recordEvent({"contract taken", chosen, k.poster, entity(chosen)->cellId, 0, 0, k.item, n, k.reward, k.kind + ": " + k.detail});
        provisionCarrier(k, n);
    }
}

void World::tendContractCarriers()
{
    for (auto& k : roads_.contracts)
    {
        if (k.kind != "procure" || k.status != "taken" || k.source.empty())
            continue;
        const auto* carrier = entity(k.taker);
        const auto release = [&] {
            k.status = "open";
            k.taker.clear();
            k.source.clear();
            k.carried = 0;
        };
        if (!carrier || carrier->dead)
        {
            release();
            continue;
        }
        if (k.carried == 0)
        {
            // At the shop: the goods are handed over, on the contract's account (it is paid when they arrive).
            const auto* job = society_.jobOf(k.source);
            const auto at = job && job->role == "merchant" ? job->serve : job ? job->work : Spot{};
            if (!job || carrier->cellId != at.cell || apart(carrier->position, {at.x, at.y}) > 3)
                continue;
            const auto till = society_.tillOf(k.source);
            const bool shop = job->role == "merchant";
            int want = std::min(k.quantity - k.delivered, Society::stockAll(*society_.account(till), k.item) - (shop ? Society::GoodsKept : 0));
            const auto kinds = Society::kindsHeld(*society_.account(till), k.item);
            for (const auto& kind : kinds)
            {
                const int n = std::min(want, Society::stock(*society_.account(till), kind));
                if (n > 0 && society_.shift(till, k.taker, kind, n, 0, "goods for a contract"))
                    k.carried += n, want -= n;
                if (want <= 0)
                    break;
            }
            if (k.carried == 0)
                release();                          // Sold out since: someone else may find them.
            continue;
        }
        // At the market of the town that wants them: delivered, the shop paid its price and the carrier the rest.
        const auto* dest = town(k.town);
        if (!dest || carrier->cellId != dest->market || apart(carrier->position, {dest->marketX, dest->marketY}) > 3)
            continue;
        int given = 0;
        const auto* purse = society_.account(k.taker);
        for (const auto& kind : purse ? Society::kindsHeld(*purse, k.item) : std::vector<std::string>{})
        {
            const int n = std::min(Society::stock(*society_.account(k.taker), kind), k.carried - given);
            if (n > 0 && society_.shift(k.taker, k.poster, kind, n, 0, "contract delivery"))
                given += n;
            if (given >= k.carried)
                break;
        }
        const auto escrow = "contract:" + k.id;
        const int left = k.quantity - k.delivered;
        const auto pay = given >= left ? k.reward : k.reward * given / std::max(1, left);
        const auto* good = items::good(k.item);
        const auto price = std::min<std::int64_t>(pay, std::int64_t(std::max(1, good ? good->price : 1)) * given);
        if (price > 0 && society_.shift(escrow, society_.tillOf(k.source), "", 0, price, "sold on a contract"))
            k.reward -= price;
        if (pay - price > 0 && society_.shift(escrow, k.taker, "", 0, pay - price, "carrier's pay"))
            k.reward -= pay - price;
        k.delivered += given;
        if (k.delivered >= k.quantity)
        {
            society_.closeAccount(escrow);
            k.status = "done";
            k.source.clear();
            k.carried = 0;
            recordEvent({"contract done", k.taker, k.target, carrier->cellId, 0, 0, k.item, given, pay, k.kind + ": " + k.detail});
        }
        else
            release();                              // Some still wanted: open again.
    }
}

std::vector<const Contract*> World::deliverable(const std::string& player) const
{
    std::vector<const Contract*> out;
    const auto* p = entity(player);
    const auto* here = p ? townOf(p->cellId) : nullptr;
    const auto* purse = society_.account(player);
    if (!here || !purse)
        return out;
    for (const auto& c : roads_.contracts)
        if (c.kind == "procure" && c.status == "taken" && c.taker == player && c.town == here->id && Society::stockAll(*purse, c.item) > 0)
            out.push_back(&c);
    return out;
}

Result World::deliverContract(const std::string& player, const std::string& contractId)
{
    const auto* p = entity(player);
    if (!p || p->npc)
        return {false, "No such character.", {}};
    for (auto& c : roads_.contracts)
    {
        if (c.id != contractId)
            continue;
        if (c.kind != "procure" || c.status != "taken" || c.taker != player)
            return {false, "That isn't work you've taken on.", contractId};
        const auto* here = townOf(p->cellId);
        if (!here || here->id != c.town)
            return {false, "That is wanted in another town.", contractId};
        const auto* purse = society_.account(player);
        if (!purse)
            return {false, "You carry nothing.", contractId};
        // What they carry of it, any quality, plainest first, up to what is still wanted.
        int given = 0;
        for (const auto& kind : Society::kindsHeld(*purse, c.item))
        {
            const int n = std::min(Society::stock(*society_.account(player), kind), c.quantity - c.delivered - given);
            if (n > 0 && society_.shift(player, c.poster, kind, n, 0, "contract delivery"))
                given += n;
            if (c.delivered + given >= c.quantity)
                break;
        }
        if (given <= 0)
            return {false, "You carry none of what is wanted.", contractId};
        const auto escrow = "contract:" + c.id;
        const int left = c.quantity - c.delivered;
        const auto pay = given >= left ? c.reward : c.reward * given / left;
        c.delivered += given;
        if (pay > 0 && society_.shift(escrow, player, "", 0, pay, "contract reward"))
            c.reward -= pay;
        const auto* good = items::good(c.item);
        const std::string what = lowerFirst(good ? good->name : c.item);
        if (c.delivered >= c.quantity)
        {
            society_.closeAccount(escrow);
            c.status = "done";
            recordEvent({"contract done", player, c.target, p->cellId, 0, 0, c.item, c.quantity, pay, c.kind + ": " + c.detail});
            return {true, "You deliver the last " + std::to_string(given) + " " + what + ", and are paid " + std::to_string(pay) +
                              "p. The work is done.",
                    contractId};
        }
        recordEvent({"contract delivery", player, c.target, p->cellId, 0, 0, c.item, given, pay, c.kind + ": " + c.detail});
        return {true, "You deliver " + std::to_string(given) + " " + what + " and are paid " + std::to_string(pay) + "p; " +
                          std::to_string(c.quantity - c.delivered) + " more are wanted.",
                contractId};
    }
    return {false, "There is no such work.", contractId};
}
} // namespace ratw
