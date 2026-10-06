#include "RatwSociety.h"

#include "RatwItems.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>

// Crafting by shopkeepers (Docs/Design/35-items-crafting-industry.md, Phase 5; Data/Items/crafts.json).
// A maker at work makes a batch of whatever has run low on their shelves, from materials in their own stock; when the
// materials run short they buy more, at the catalog's price, from a shop in the same community that supplies them.
// Suppliers restock from the workshops that make their goods (a mill's flour) and from producers who bring goods in
// from the land (a farmer's wheat). What a community can't find at home is fetched from another, dearer.
// Nothing is made from nothing: goods appear only from producers' work and the starting materials (stockMaterials).
namespace ratw
{
int Society::stockMaterials(const std::string& id)
{
    const auto* r = spec(id);
    const auto* business = r && r->role == "merchant" ? items::businessFor(r->workLabel) : nullptr;
    const auto found = state_.accounts.find(tillOf(id));   // (A house's business: its till, doc 42.)
    if (!business || found == state_.accounts.end())
        return 0;
    auto& stockHeld = found->second.stock;
    int added = 0;
    const auto topUp = [&](const std::string& item, int level) {
        const int held = stock(found->second, item);
        if (held >= level || (!stockHeld.count(item) && stockHeld.size() >= MaxGoodsKinds))
            return;
        stockHeld[item] = std::min(level, StockCap);
        added += stockHeld[item] - held;
    };
    for (const auto* craft : items::craftsFor(business->id))
        for (const auto& [item, count] : craft->in)
            topUp(item, count * MaterialBatches);
    for (const auto& item : items::suppliesFor(business->id))
        topUp(item, SuppliesKept);
    if (added > 0)
        record("starting materials", "outside", found->first, "", 0, 0);
    return added;
}

std::string Society::communityOfResident(const std::string& id) const
{
    const auto* r = spec(id);
    if (!r)
        return {};
    // The day's plans know the communities; without them, the store a cell's merchants restock from. A producer who
    // works out in the country (a farmer in the fields) belongs to the town they live in.
    const auto of = [&](const std::string& cell) { return day_.communityOf ? day_.communityOf(cell) : storeFor(cell); };
    auto here = of(r->work.cell);
    return here.empty() ? of(r->home.cell) : here;
}

int Society::buyMaterials(const std::string& id, const std::string& workCell, const std::string& item, int wanted,
                          bool forSupplier)
{
    const auto* good = items::good(item);
    if (!good || wanted < 1)
        return 0;
    const auto till = tillOf(id);                   // The shop's money and shelves (its house's till, doc 42).
    if (suppliers_.empty())
        for (const auto& r : authored_.residents)
        {
            if (const auto* producer = items::producerFor(r.workLabel))
                for (const auto& [made, n] : producer->out)
                    suppliers_[made].push_back({r.id, 2});
            if (r.role != "merchant")
                continue;
            if (const auto* business = items::businessFor(r.workLabel))
            {
                for (const auto& supply : items::suppliesFor(business->id))
                    suppliers_[supply].push_back({r.id, 0});
                for (const auto* craft : items::craftsFor(business->id))
                    for (const auto& [made, n] : craft->out)
                        if (std::none_of(suppliers_[made].begin(), suppliers_[made].end(),
                                         [&](const Seller& s) { return s.id == r.id; }))
                            suppliers_[made].push_back({r.id, 1});
            }
        }
    const auto found = suppliers_.find(item);
    if (found == suppliers_.end())
        return 0;
    const auto of = [&](const std::string& cell) { return day_.communityOf ? day_.communityOf(cell) : storeFor(cell); };
    auto here = of(workCell);
    if (here.empty())
        here = communityOfResident(id);
    int bought = 0;
    // Close to home first, at the price; then anywhere, carted in (in a world of towns, only the caravans bring what a
    // town lacks: doc 42, Phase 7).
    for (const bool local : {true, false})
        for (int kind = forSupplier ? 1 : 0; kind <= 2 && bought < wanted && (local || !tradeByCaravan_); ++kind)
            for (const auto& seller : found->second)
            {
                if (seller.kind != kind || seller.id == id || (communityOfResident(seller.id) == here) != local)
                    continue;
                const auto from = account(tillOf(seller.id));
                const auto* buyer = account(till);
                if (!from || !buyer)
                    continue;
                // Any quality it has (doc 35, Part 4), at that quality's price. A workshop keeps a few common ones for
                // its own shelf; a supplier or producer sells all it has.
                for (const auto& sort : kindsHeld(*from, item))
                {
                    const auto* sortGood = items::good(sort);
                    const auto* buyerNow = account(till);
                    if (!sortGood || !buyerNow)
                        continue;
                    const int spare = stock(*from, sort) - (kind == 1 && sort == item ? GoodsKept : 0);
                    // At its price, cheaper from a seller with plenty, dearer from one running short (supplyFactor), and
                    // less still to a shop selling at a markdown (doc 42: what doesn't sell drives prices down the line).
                    const auto m = markdown_.count(till) ? markdown_.at(till) : 1.;
                    // (A producer holds little because it sells: only a glut makes its price, never "scarcity".)
                    const double supply = seller.kind == 2 ? std::min(1., supplyFactor(*from, sort)) : supplyFactor(*from, sort);
                    const std::int64_t price = std::max<std::int64_t>(
                        1, std::int64_t(std::ceil(sortGood->price * (local ? 1. : CartedIn) * supply * m)));
                    const int n = int(std::min<std::int64_t>({wanted - bought, spare, 99, spendable(till) / price}));
                    if (n > 0 && transfer(tillOf(seller.id), till, sort, n, price, local ? "materials bought" : "materials carted in"))
                        bought += n;
                    if (bought >= wanted)
                        return bought;
                }
            }
    return bought;
}

bool Society::produce(const std::string& id, double absoluteDay)
{
    const auto* r = spec(id);
    const auto* producer = r ? items::producerFor(r->workLabel) : nullptr;
    const auto till = tillOf(id);
    if (!producer || !account(till))
        return false;
    if (const auto next = produceNext_.find(id); next != produceNext_.end() && absoluteDay < next->second)
        return false;
    const bool inSeason = producer->seasons.empty() ||
                          std::find(producer->seasons.begin(), producer->seasons.end(), season_) != producer->seasons.end();
    if (!inSeason && producer->offSeason.empty())
        return false;                               // Nothing grows in the fields this season.
    const auto* job = jobOf(id);
    produceNext_[id] = absoluteDay + producer->seconds / 86400. * (1 - ImprovementPace * (job ? improvement(job->id) : 0));
    bool any = false;
    // Out of season, what the work brings in instead (threshing the barn's grain in winter, doc 42).
    // (TRIAL produce_more: works on until it holds twice as much, doc 42 "Pressure": the land was idle at 20.)
    const int kept = trial("produce_more") ? 2 * ProducerKept : ProducerKept;
    for (const auto& [item, count] : inSeason ? producer->out : producer->offSeason)
        if (const int held = stock(*account(till), item); held < kept)
            any = create(till, item, std::min(count, kept - held), "brought in") || any;
    return any;
}

void Society::craft(const std::string& id, const std::string& workCell, double absoluteDay)
{
    const auto* r = spec(id);
    const auto* business = r ? items::businessFor(r->workLabel) : nullptr;
    const auto till = tillOf(id);                   // The shop's shelves (its house's till, doc 42).
    if (!r || !account(till))
        return;
    if (const auto next = craftNext_.find(id); next != craftNext_.end() && absoluteDay < next->second)
        return;
    const auto crafts = business ? items::craftsFor(business->id) : std::vector<const items::Craft*>{};
    const auto& supplies = business ? items::suppliesFor(business->id) : std::vector<std::string>{};
    const auto supplied = [&](const std::string& item) { return std::find(supplies.begin(), supplies.end(), item) != supplies.end(); };
    // Preserving (salting, smoking: doc 42, "Pressure"): a shop that sells a fresh food it also preserves keeps GoodsKept
    // of it fresh on the counter, for hot meals and those who eat it the day they buy it; only what's over is cured.
    const auto fresh = [&](const items::Craft& k, const std::string& item) {
        const auto* in = items::good(item);
        const auto* out = items::good(k.out.front().first);
        if (!business || !in || !out || in->nourish <= 0 || in->keeps <= 0 || out->keeps <= in->keeps)
            return 0;
        const auto sold = items::goodsSold(*business, 1 << 20);
        return std::find(sold.begin(), sold.end(), item) != sold.end() ? GoodsKept : 0;
    };
    const auto has = [&](const items::Craft& k) {
        return std::all_of(k.in.begin(), k.in.end(),
                           [&](const auto& i) { return stockAll(*account(till), i.first) >= i.second + fresh(k, i.first); });
    };
    // A batch finished: its materials are used and its goods are on the shelf (if the materials are still there).
    if (const auto atWork = craftAtWork_.find(id); atWork != craftAtWork_.end())
    {
        const auto k = std::find_if(crafts.begin(), crafts.end(), [&](const auto* c) { return c->id == atWork->second; });
        craftAtWork_.erase(atWork);
        if (k != crafts.end() && has(**k))
        {
            // The materials, plain ones first (common, then crude, then the better), and how good they were on the whole.
            double qualities = 0;
            int used = 0;
            for (const auto& [item, count] : (*k)->in)
            {
                int left = count;
                for (const auto& sort : kindsHeld(*account(till), item))
                {
                    const int took = consume(till, sort, std::min(left, stock(*account(till), sort)), "used in crafting");
                    qualities += took * items::qualityOf(sort);
                    used += took;
                    left -= took;
                    if (left <= 0)
                        break;
                }
            }
            // The batch's quality (doc 35, Part 4): the maker's skill at their trade, the materials (a crude one drags
            // it down, a fine one lifts it) and the day's luck. Fine needs a skill of 60 (and is common only past 75), masterwork 85.
            const auto* job = jobOf(id);
            const double craft = job ? skill(id, job->id) : 30;
            const double materials = used > 0 ? (qualities / used - 1) * 15 : 0;
            const double luck = (double(std::hash<std::string>{}(id + "|" + std::to_string(std::int64_t(absoluteDay * 86400))) % 1000) / 1000 - .5) * 40;
            // A Gift lent to it (doc 43): the forge kept hot, the clay true, the flaws heard; once, for this batch.
            double lifted = 0;
            if (const auto lent = giftLift_.find(id); lent != giftLift_.end())
            {
                lifted = absoluteDay <= lent->second.second ? lent->second.first : 0;
                giftLift_.erase(lent);
            }
            const double score = craft + materials + luck + lifted;
            // Masterwork is rare even for a master: a good day, and a further one in seven besides.
            const bool inspired = std::hash<std::string>{}(id + "|muse|" + std::to_string(std::int64_t(absoluteDay * 86400))) % 7 == 0;
            const int quality = craft >= 85 && score >= 100 && inspired ? 3 : craft >= 60 && score >= 85 ? 2 : score < 20 ? 0 : 1;
            // A masterwork carries its maker's mark (doc 35, Part 4).
            for (const auto& [item, count] : (*k)->out)
                create(till, quality == 3 ? items::withMaker(items::withQuality(item, 3), id) : items::withQuality(item, quality), count, "crafted");
        }
    }
    // A supplier's shelves: what has run below a quarter of its store is bought back up from workshops and producers.
    for (const auto& item : supplies)
        if (const int held = stockAll(*account(till), item); held < SuppliesKept / 4)
            buyMaterials(id, workCell, item, SuppliesKept - held, true);
    // A shopkeeper who also brings goods in (a stables' stock-breeding) does so while at work.
    produce(id, absoluteDay);
    double wait = 1. / 96;                          // Nothing to make: look again in a quarter of an hour.
    // (TRIAL make_what_sells: what sells fastest first, so scarce materials (iron) go where they're wanted, not into
    // goods that sit on the shelf; otherwise in file order.)
    auto order = crafts;
    if (trial("make_what_sells"))
        if (const auto rates = sellRate_.find(till); rates != sellRate_.end())
            std::stable_sort(order.begin(), order.end(), [&](const items::Craft* a, const items::Craft* b) {
                const auto rate = [&](const items::Craft* k) {
                    const auto r = rates->second.find(k->out.front().first);
                    return r == rates->second.end() ? 0. : r->second;
                };
                return rate(a) > rate(b);
            });
    for (const auto* k : order)
    {
        // Low, judged by what the batch is for (its first good; a butcher's meat, not the hides that come with it):
        // under the few a shop keeps, or, for what other trades work with (a mill's flour), under half a supplier's store.
        // (What other trades work with and keeps (a mill's flour, not a baker's bread): a full store, so there is some to
        // spare for other towns' makers, doc 42, "Food between towns". Their standing orders take what is over half.)
        const auto& made = k->out.front().first;
        const auto* madeGood = items::good(made);
        const bool keeps = !madeGood || madeGood->keeps <= 0;
        // (TRIAL cure_more: a shop curing food (what keeps longer than what it's made from) keeps half a store of it,
        // not a few: more fresh meat and fish cured before it spoils, more salt and firewood bought.)
        const bool cures = trial("cure_more") && !k->in.empty() && madeGood && madeGood->nourish > 0 && [&] {
            const auto* in = items::good(k->in.front().first);
            return in && in->keeps > 0 && (madeGood->keeps <= 0 || madeGood->keeps > in->keeps);
        }();
        const bool low = stockAll(*account(till), made) < (items::traded(made) && keeps ? SuppliesKept
                                                           : supplied(made) || items::traded(made) || cures ? SuppliesKept / 2 : GoodsKept);
        if (!low)
            continue;
        // A seasonal good (grapes, apples) is laid in at the harvest, a store to last the year: SeasonalStore times as much.
        for (const auto& [item, count] : k->in)
        {
            const int store = count * MaterialBatches * (items::seasonal(item) ? SeasonalStore : 1) + fresh(*k, item);
            // (What it sells as a supplier too, a fishmonger's fish, it restocks as a supplier does, from the makers and
            // the land, never from the town's other suppliers: they'd only buy it back, round and round.)
            if (const int held = stockAll(*account(till), item); held < count * MaterialsLow || (store > count * MaterialBatches && held < store / 2))
                buyMaterials(id, workCell, item, store - held, supplied(item));
        }
        if (!has(*k))
            continue;                               // Short of materials, and nobody sells them: something else.
        craftAtWork_[id] = k->id;                   // A batch begun: done after its working time (quicker in improved
        wait = k->seconds / 86400.;                 // premises: RatwOddJobs.cpp).
        if (const auto* job = jobOf(id))
            wait *= 1 - ImprovementPace * improvement(job->id);
        break;
    }
    craftNext_[id] = absoluteDay + wait;
}

void Society::lendGift(const std::string& maker, double lift, double untilDay)
{
    auto& lent = giftLift_[maker];
    lent.first = std::min(30.0, (lent.second >= untilDay - 1 ? lent.first : 0) + lift);   // (Several Gifts help, to a point.)
    lent.second = untilDay;
}

double Society::giftLiftOf(const std::string& maker) const
{
    const auto lent = giftLift_.find(maker);
    return lent == giftLift_.end() ? 0 : lent->second.first;
}
} // namespace ratw
