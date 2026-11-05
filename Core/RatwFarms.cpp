// The Country's food farms (Docs/Design/71-the-country-s-industries.md, 4, Phases A and B; the user, 2026-11-01: "Yes to
// all your suggestions"): modelled producers with virtual crews, before any NPC is placed. Each farm is a till of its own
// and its crew a household that shops in the city; an area switched on takes its goods from the Country, its farms
// selling them to the shops' store draws from their stock. Every coin moves by shift or sale but the founding (a farm's
// float and its crew's settled purse, made as the world's founding money was and counted as minted), so the money stays
// conserved; goods are grown on the farms (create) and leave by being sold, eaten at the Country's table or by the crews,
// spoiling, or being used at the farm (the basket).
#include "RatwItems.h"
#include "RatwSociety.h"

#include <algorithm>
#include <cctype>
#include <cmath>

namespace ratw
{
namespace
{
// A world's own setting where it has one (0 or more), else the next level's.
int orElse(int own, int otherwise)
{
    return own >= 0 ? own : otherwise;
}
} // namespace

void Society::buildFarms()
{
    // The farms as this world has them: country_farms.json's, with the world's own changes (EconomySpec::farms); a farm's
    // own setting, then its area's, then the defaults. What each carries (its fractions of a piece) is kept.
    const auto& defaults = items::farms();
    const auto& own = authored_.economy.farms;
    std::map<std::string, FarmNow> was;
    for (auto& f : farms_)
        was.emplace(f.def->id, std::move(f));
    farms_.clear();
    farmOf_.clear();
    areaOfGood_.clear();
    for (std::size_t a = 0; a < defaults.areas.size(); ++a)
    {
        const auto& area = defaults.areas[a];
        for (const auto& good : area.goods)
            areaOfGood_.emplace(good, a);
        const auto oa = own.areas.find(area.id);
        const EconomySpec::Farms::Area none{};
        const auto& ownArea = oa == own.areas.end() ? none : oa->second;
        const int areaYield = orElse(ownArea.yieldScale, area.yieldScale), areaCrew = orElse(ownArea.crewScale, area.crewScale);
        // The defaults' wages, then the area's (as the file has them, then the world's own).
        const int keeperOfArea = orElse(ownArea.keeper, orElse(area.keeper, orElse(own.keeper, defaults.keeper)));
        const int handOfArea = orElse(ownArea.hand, orElse(area.hand, orElse(own.hand, defaults.hand)));
        for (const auto& def : area.farms)
        {
            const auto of = own.farms.find(def.id);
            const EconomySpec::Farms::Farm noFarm{};
            const auto& ownFarm = of == own.farms.end() ? noFarm : of->second;
            auto found = was.find(def.id);
            FarmNow f = found == was.end() ? FarmNow{} : std::move(found->second);
            f.def = &def;
            f.area = a;
            f.works = area.works;                    // (A works pays rent, a food farm buys its basket: doc 72, Phase 0.)
            f.house = def.house.empty() ? area.house : def.house;   // (Its rent's House: a works', or a food farm's land rent.)
            f.till = farmTill(def.id);
            f.crew = farmCrew(def.id);
            f.yieldScale = areaYield / 100. * orElse(ownFarm.yieldScale, 100) / 100.;
            f.keepers = def.keepers;
            // Its hands times its area's crew scale and its own, rounded half up (doc 71, 4).
            f.hands = int(std::floor(def.hands * (areaCrew / 100.) * (orElse(ownFarm.crewScale, 100) / 100.) + .5));
            f.keeper = orElse(ownFarm.keeper, orElse(def.keeper, keeperOfArea));
            f.hand = orElse(ownFarm.hand, orElse(def.hand, handOfArea));
            // Its float: floatDays of its wages now, and the design's lean season's shortfall on top (what its reserve is
            // over floatDays of the design's wages: none has one at the defaults). Its crew's settled purse, by its size.
            const std::int64_t designWages = std::int64_t(def.keepers) * orElse(def.keeper, orElse(area.keeper, defaults.keeper)) +
                                             std::int64_t(def.hands) * orElse(def.hand, orElse(area.hand, defaults.hand));
            const auto lean = std::max<std::int64_t>(0, def.reserve - defaults.floatDays * designWages);
            f.reserve = defaults.floatDays * f.wages() + lean;
            const int designHeads = def.keepers + def.hands;
            f.purse = designHeads > 0 ? std::int64_t(std::llround(double(def.purse) * f.crewSize() / designHeads)) : 0;
            farmOf_[f.till] = farms_.size();
            farms_.push_back(std::move(f));
        }
    }
    // Each farm's work rooms (communities.json, doc 71 Phase C): a post there is the farm's. A works' (doc 72) is the cell
    // out of doors its centre is in: a post there is the works' by its labels (farmOfPost).
    farmWorkplace_.clear();
    for (const auto& place : items::farmPeople().farms)
        if (const auto at = std::find_if(farms_.begin(), farms_.end(), [&](const FarmNow& f) { return f.def->id == place.farm; });
            at != farms_.end())
        {
            at->outdoors = place.works;
            for (const auto& room : place.workplaces)
                farmWorkplace_.emplace(room.area, std::size_t(at - farms_.begin()));
        }
    countFarmPosts();
    refreshAreas(-1, false);                         // (Which are on as of the day it knows; none founded here.)
}

void Society::refreshAreas(std::int64_t day, bool found)
{
    // Today's areas (the schedule moves on with the day: an area's place in the order, `every` days apart from `start`;
    // an area's own switch over it), and, with `found`, the farms of an area on founded where they are not yet. Without a
    // day, the last the farms were tended (or the society's, once it knows the date).
    if (day >= 0)
        farmsDay_ = day;
    else
        day = farmsDay_ >= 0 ? farmsDay_ : seasonDay_;
    const auto& defaults = items::farms();
    const auto& own = authored_.economy.farms;
    areaOn_.assign(defaults.areas.size(), 0);
    const int start = own.start == -2 ? defaults.start : own.start;
    const int every = orElse(own.every, defaults.every);
    const auto& order = own.order.empty() ? defaults.order : own.order;
    for (std::size_t a = 0; a < defaults.areas.size(); ++a)
    {
        const auto& area = defaults.areas[a];
        const auto oa = own.areas.find(area.id);
        int on = oa != own.areas.end() && oa->second.on >= 0 ? oa->second.on : area.on;
        if (on < 0)
        {
            const auto at = std::find(order.begin(), order.end(), area.id);
            on = start >= 0 && day >= 0 && at != order.end() && day >= start + std::int64_t(at - order.begin()) * every;
        }
        areaOn_[a] = char(on > 0);
    }
    if (!found || roster_ != Roster::Authored)
        return;
    for (auto& f : farms_)
        if (farmAreaOn(f.area) && (!account(f.till) || !account(f.crew)))
            foundFarm(f);
}

void Society::foundFarm(FarmNow& f)
{
    // A farm's founding (doc 71, 3, "The money the switch holds"; the user, 2026-11-01): its till opened with its float and
    // its crew's household with its settled purse, made as the world's founding money was ("initial funding") and counted
    // as minted, so the money stays conserved; and its store a week of its yields (at most what a perishable good keeps).
    const bool fresh = !account(f.till);
    if ((!account(f.till) && !openAccount(f.till)) || (!account(f.crew) && !openAccount(f.crew)))
        return;
    if (!fresh)
        return;                                     // (Its crew's household alone was missing: a save's; nothing made.)
    const auto mint = [&](const std::string& id, std::int64_t coins, const char* kind) {
        if (coins <= 0)
            return;
        state_.accounts.at(id).cash += coins;
        state_.minted += coins;
        loopToday_.minted += coins;
        farmsMinted_ += coins;
        record(kind, "made", id, "", 0, coins);
    };
    mint(f.till, f.reserve, "a farm's founding money");
    mint(f.crew, f.purse, "a farm crew's founding purse");
    const auto& defaults = items::farms();
    for (const auto& [good, middle] : f.def->yield)
    {
        const auto* item = items::good(good);
        const double keeps = item ? item->keeps : 0;
        const double days = keeps > 0 ? std::min<double>(defaults.openingStockDays, keeps) : defaults.openingStockDays;
        const int n = int(std::floor(seasonalValue(middle, yearDay_) * f.yieldScale * days + 1e-9));
        for (int left = n; left > 0; left -= 99)
            if (!create(f.till, good, std::min(left, 99), "grown at " + f.def->id))
                break;
    }
}

void Society::tendFarms(std::int64_t day)
{
    // The farms' day (doc 71, 4, Phase B: "Production"), at the first loop hour of each day: the areas as of today; each
    // on farm grows its yields (the season's, blended, times its yield scale; fractions carried), the Country's own people
    // eat at its table (its `table`: no dependants until families are placed), and what is past keeping spoils (more than
    // its keeping days of the good's yield); and its crew eats a day's food from what its household holds.
    refreshAreas(day);
    if (roster_ != Roster::Authored)
        return;
    countFarmPosts();
    const double table = tableShare();
    for (auto& f : farms_)
    {
        if (!farmAreaOn(f.area) || !account(f.till))
            continue;
        auto& today = loopToday_.farms[f.def->id];
        const auto& name = f.def->id;
        // Its crew at work (doc 71, Phase C): a farm short of hands (a real post empty) yields in proportion.
        const double working = f.working();
        for (const auto& [good, middle] : f.def->yield)
        {
            double& carry = f.grown[good];
            carry += seasonalValue(middle, yearDay_) * f.yieldScale * working;
            const int n = int(std::floor(carry + 1e-9));
            if (n < 1)
                continue;
            carry -= n;
            int made = 0;
            for (int left = n; left > 0; left -= 99)
                if (const int lot = std::min(left, std::min(99, StockCap - stock(*account(f.till), good)));
                    lot > 0 && create(f.till, good, lot, "grown at " + name))
                    made += lot;
                else
                    break;
            today.grown += made;
            today.spoiled += n - made;              // (Its store full: what it couldn't keep.)
            auto& byGood = today.goods[good];
            byGood.grown += made;
            byGood.spoiled += n - made;
            if (n > made)
                loopToday_.goodsSpoiled[good] += n - made;
        }
        makeOfOwn(f, working);
        for (const auto& [good, middle] : f.def->table)
        {
            double& carry = f.eaten[good];
            carry += seasonalValue(middle, yearDay_) * table;
            const int n = int(std::floor(carry + 1e-9));
            if (n < 1)
                continue;
            carry -= n;
            const int got = consume(f.till, good, n, "eaten in the Country");
            today.eaten += got;
            today.tableShort += n - got;
            auto& byGood = today.goods[good];
            byGood.eaten += got;
            byGood.tableShort += n - got;
        }
        for (const auto& [good, middle] : f.def->yield)
        {
            const auto* item = items::good(good);
            if (!item || item->keeps <= 0)
                continue;
            const double mean = (middle[0] + middle[1] + middle[2] + middle[3]) / 4 * f.yieldScale;
            const int keep = std::max(1, int(std::ceil(item->keeps * mean - 1e-9)));
            // What it makes of what is past keeping first (its `makes`: the dairies' spare milk, mostly the morning after a
            // Restday, into cheese and butter, doc 72): one good made into another at the farm, nothing sold or made from
            // nothing; the rest spoils.
            if (const int have = stock(*account(f.till), good); have > keep)
                makeOfSpare(f, good, have - keep);
            if (const int over = stock(*account(f.till), good) - keep; over > 0)
            {
                const int gone = consume(f.till, good, over, "spoiled");
                today.spoiled += gone;
                today.goods[good].spoiled += gone;
                loopToday_.goodsSpoiled[good] += gone;
            }
        }
        // The crew eats (a household of its size: NourishADay a head), the soonest-spoiling food first; what it can't eat
        // for want of food is owed to tomorrow, a day's at most (it eats a piece over, rather than leave a want).
        if (const auto* larder = account(f.crew); larder && f.heads() > 0)
        {
            const double aDay = f.heads() * NourishADay;
            f.hunger = std::min(aDay, f.hunger) + aDay;
            while (f.hunger > 0)
            {
                const auto food = eatFirst(*account(f.crew));
                const int each = food.empty() ? 0 : nourishment(food);
                if (each < 1)
                    break;
                const int n = std::clamp(int(std::floor(f.hunger / each)), 1, stock(*account(f.crew), food));
                const int got = consume(f.crew, food, n, "eaten by a farm's crew");
                if (got < 1)
                    break;
                f.hunger -= double(got) * each;
            }
            if (f.hunger > 0)
                today.crewHungry += f.hunger;
        }
        // Every virtual post real now (doc 71, Phase C): what its crew's household still holds (its founding purse's
        // rest, its larder) is its keeper's household's, or a hand's where the keeper's post stands empty. Moved, not made.
        if (const auto* larder = account(f.crew); larder && f.heads() == 0 && (larder->cash > 0 || !larder->stock.empty()))
        {
            std::string heir;
            bool keeper = false;
            for (const auto& p : positions_)
                if (const auto held = state_.careers.positions.find(p.id);
                    held != state_.careers.positions.end() && !held->second.holder.empty() && !dead_.count(held->second.holder) &&
                    account(held->second.holder) && farmOfPost(p, &keeper) >= 0 && farms_[std::size_t(farmOfPost(p))].def == f.def &&
                    (heir.empty() || keeper))
                {
                    heir = held->second.holder;
                    if (keeper)
                        break;
                }
            if (!heir.empty())
            {
                if (larder->cash > 0)
                    shift(f.crew, heir, "", 0, larder->cash, "a farm crew's purse, to its people");
                const std::vector<std::pair<std::string, int>> goods(larder->stock.begin(), larder->stock.end());
                for (const auto& [item, n] : goods)
                    for (int left = n; left > 0; left -= 99)
                        if (!shift(f.crew, heir, item, std::min(left, 99), 0, "a farm crew's larder, to its people"))
                            break;
            }
        }
    }
}

int Society::makeOfSpare(FarmNow& f, const std::string& from, int spare)
{
    // A farm's `makes` of `spare` pieces of `from` (doc 72: the dairies' spare milk into cheese, then butter): each its
    // share of the spare (the last all that is left), whole pieces at `in` of `from` a piece, while its store holds under
    // its `store`. What it used of `from`; journalled "made into <good>" and "made at <farm>": the goods move from one
    // into the other, nothing sold, no money.
    const auto* till = f.def ? account(f.till) : nullptr;
    if (!till || spare <= 0)
        return 0;
    std::vector<const items::FarmDef::Make*> mine;
    for (const auto& m : f.def->makes)
        if (m.from == from)
            mine.push_back(&m);
    auto& today = loopToday_.farms[f.def->id];
    int left = std::min(spare, stock(*till, from)), used = 0;
    for (std::size_t i = 0; i < mine.size() && left > 0; ++i)
    {
        const auto& m = *mine[i];
        const int portion = i + 1 == mine.size() ? left : std::min(left, int(std::floor(double(spare) * m.share / 100. + 1e-9)));
        const int n = std::min(portion / m.in, std::max(0, std::min(m.store, StockCap) - stock(*till, m.good)));
        if (n < 1 || (!till->stock.count(m.good) && till->stock.size() >= MaxGoodsKinds))
            continue;
        if (consume(f.till, from, n * m.in, "made into " + m.good) != n * m.in || !create(f.till, m.good, n, "made at " + f.def->id))
            break;                                  // (Never: it holds them, and room for the made.)
        left -= n * m.in;
        used += n * m.in;
        today.grown += n;
        today.used += n * m.in;
        today.goods[m.good].grown += n;
        today.goods[from].used += n * m.in;
    }
    return used;
}

int Society::makeOfOwn(FarmNow& f, double working)
{
    // A make's own yield (`aDay`; doc 72, Phase 4's follow-up: the shops wanted some 60 butter and 80 cheese a day, the
    // spare made 2 and 0.5): its pieces of `from` a day, its season's blended as a yield is (times the yield scale and the
    // crew at work, fractions carried), made at once into whole pieces of `good` while its store has room. The milk is
    // counted grown and used, the made good grown: nothing sold, no money, never in the till as milk (the shops' milk is
    // the yield's alone). What the store can't take this morning isn't milked for it (nothing spoils, nothing carried).
    const auto* till = f.def ? account(f.till) : nullptr;
    if (!till)
        return 0;
    auto& today = loopToday_.farms[f.def->id];
    int made = 0;
    for (const auto& m : f.def->makes)
    {
        if (!m.dedicated())
            continue;
        double& carry = f.making[m.good];
        carry += seasonalValue(m.aDay, yearDay_) * f.yieldScale * working;
        const int milk = int(std::floor(carry + 1e-9)), whole = milk / m.in;
        const int room = std::max(0, std::min(m.store, StockCap) - stock(*till, m.good));
        const int n = std::min(whole, room);
        carry -= n < whole ? milk : n * m.in;      // (Its store full: the rest isn't milked for it.)
        if (n < 1 || (!till->stock.count(m.good) && till->stock.size() >= MaxGoodsKinds) ||
            !create(f.till, m.good, n, "made at " + f.def->id + " of its own " + m.from))
            continue;
        made += n;
        today.grown += n + n * m.in;
        today.used += n * m.in;
        today.goods[m.good].grown += n;
        today.goods[m.from].grown += n * m.in;
        today.goods[m.from].used += n * m.in;
    }
    return made;
}

void Society::payFarms()
{
    // The farms' wages at 18:00 (doc 71, 4; the user: fixed, a keeper 16p and a hand 10p, editable per farm and area), to
    // their crews' households, from all the till holds (its float is for them): what it can't pay goes to the treasury's
    // wage support with the city's short tills' (supportWages).
    // Its real posts (doc 71, Phase C) are paid as its virtual crew was, from the same till at the same wages: each held
    // post its holder, the rest its crew's household; short of the whole, each the same share of its wage.
    countFarmPosts();
    std::vector<std::vector<std::pair<std::string, int>>> real(farms_.size());
    for (const auto& p : positions_)
    {
        bool keeper = false;
        const int farm = farmOfPost(p, &keeper);
        if (farm < 0)
            continue;
        const auto held = state_.careers.positions.find(p.id);
        if (held == state_.careers.positions.end() || held->second.holder.empty() || dead_.count(held->second.holder) ||
            !account(held->second.holder))
            continue;
        const auto& f = farms_[std::size_t(farm)];
        real[std::size_t(farm)].push_back({held->second.holder, keeper ? f.keeper : f.hand});
    }
    for (std::size_t i = 0; i < farms_.size(); ++i)
    {
        const auto& f = farms_[i];
        if (!farmAreaOn(f.area) || !account(f.till) || !account(f.crew))
            continue;
        auto dues = real[i];
        if (const auto crew = f.virtualWages(); crew > 0)
            dues.push_back({f.crew, int(crew)});
        std::int64_t owed = 0;
        for (const auto& [to, wage] : dues)
            owed += wage;
        if (owed <= 0)
            continue;
        auto& today = loopToday_.farms[f.def->id];
        const auto can = cash(f.till);
        std::int64_t paid = 0;
        for (const auto& [to, wage] : dues)
            if (const std::int64_t coins = can >= owed ? wage : std::int64_t(wage) * can / owed;
                coins > 0 && shift(f.till, to, "", 0, coins, "wages"))
            {
                paid += coins;
                if (to != f.crew)
                    today.peoplePaid += coins;
            }
        today.wagesOwed += owed;
        today.wagesPaid += paid;
        if (paid < owed)
        {
            tillsShort_[f.till] = owed - paid;      // (For the treasury's wage support, with the city's short tills.)
            loopToday_.shortByTrade["farm"] += owed - paid;   // (The log's, by trade, as the support's: doc 73, round 2.)
        }
    }
    payRents();
}

void Society::payRents()
{
    // The estates' rent (Docs/Design/72-pulling-the-country-into-the-world.md, 3, option 1; the user, 2026-11-03: "each
    // works pays its own crew and sends its surplus to the noble House whose land it is on"): after tonight's wages, what
    // a works' till holds over its float (two days of its wages) goes to its House's estate. At first every House's
    // estate was the Country's one purse ("the Houses' estates and the sea trade"), booked by House in the reports; since
    // Phase 6 each House has its own (houseFund: its estate's purse, the Country's while the estates are one). A till
    // short of its wages keeps all it has (its rent is what is over).
    // The food farms' land rent (doc 72, open question 2, taken once country3 showed the Country short of its posts): a
    // farm that paid its crew in full tonight pays its rule's percent of its takings since the last night (landRentDue) to
    // the House whose land it is on, into the same purse, out of what is over its float: the share its basket would have
    // spent (farmsBuy keeps it back), never its wages or its float. Short of its wages, it pays none, and those takings'
    // rent is let go.
    const bool purse = account(WharfFund) != nullptr;
    const bool split = estatesOn();
    for (auto& f : farms_)
    {
        const auto due = f.rents() ? 0 : landRentDue(f);
        f.rentTakings = 0;
        if (!purse || !farmAreaOn(f.area) || !account(f.till))
            continue;
        const auto house = f.house.empty() ? std::string("none") : f.house;   // (An area with no House named.)
        const auto named = house == "the Crown" ? house : "House " + house;
        const auto to = houseFund(house);
        const auto* estate = split ? estateOfFund(to) : nullptr;
        auto& today = loopToday_.farms[f.def->id];
        const auto over = cash(f.till) - f.reserve;
        if (f.rents())
        {
            if (over <= 0 || cash(to) > MoneyCap - over)
                continue;
            if (shift(f.till, to, "", 0, over, "an estate's rent: " + named))
            {
                today.rent += over;
                loopToday_.rents[house] += over;
                if (estate)
                {
                    loopToday_.estates[estate->id].rents += over;
                    loopToday_.estates[estate->id].works += over;
                }
            }
            continue;
        }
        const auto rent = std::min(due, over);
        if (today.wagesPaid < today.wagesOwed)
            continue;
        if (rent > 0 && cash(to) <= MoneyCap - rent && shift(f.till, to, "", 0, rent, "a land rent: " + named))
        {
            today.rent += rent;
            loopToday_.landRents[house] += rent;
            if (estate)
            {
                loopToday_.estates[estate->id].rents += rent;
                loopToday_.estates[estate->id].land += rent;
            }
        }
        // Its surplus (the rule farmSurplus, after wharf7b): what it holds over its float and its design's basket a day
        // (which tomorrow's basket may spend) is its House's, as a works' surplus is; it paid its crew in full tonight.
        if (const auto most = farmBasketMost(f); most >= 0)
            if (const auto surplus = cash(f.till) - f.reserve - most;
                surplus > 0 && cash(to) <= MoneyCap - surplus && shift(f.till, to, "", 0, surplus, "a farm's surplus: " + named))
            {
                today.rent += surplus;
                today.surplus += surplus;
                loopToday_.landRents[house] += surplus;
                if (estate)
                {
                    loopToday_.estates[estate->id].rents += surplus;
                    loopToday_.estates[estate->id].land += surplus;
                    loopToday_.estates[estate->id].surplus += surplus;
                }
            }
    }
}

std::int64_t Society::farmBasketMost(const FarmNow& f) const
{
    // A food farm's basket a day at most (the rule farmSurplus): its design's (basketADay), whole pennies; -1: no bound
    // (the rule off, a works, a farm with no design basket).
    if (rule("farmSurplus") <= 0 || f.rents() || !f.def || f.def->basketADay <= 0)
        return -1;
    return std::int64_t(std::ceil(f.def->basketADay - 1e-9));
}

int Society::landRentPercent(const FarmNow& f) const
{
    if (f.works || !f.def || f.area >= items::farms().areas.size())
        return 0;
    // Its own rule first (a farm's `landRent`: the dairies' landRentDairy, doc 72, Phase 4), then its area's, then landRent.
    for (const auto* which : {&f.def->landRent, &items::farms().areas[f.area].id})
    {
        if (which->empty())
            continue;
        auto name = "landRent" + *which;
        name[8] = char(std::toupper(static_cast<unsigned char>(name[8])));
        if (items::ruleInfo(name))
            if (const int own = rule(name); own >= 0)
                return std::clamp(own, 0, 100);
    }
    return std::clamp(rule("landRent"), 0, 100);
}

std::int64_t Society::landRentDue(const FarmNow& f) const
{
    const int percent = landRentPercent(f);
    return percent > 0 && f.rentTakings > 0 ? f.rentTakings * percent / 100 : 0;
}

void Society::farmsBuy(int hod)
{
    // The farms' basket (doc 71, 2.6: what each takes in over its crew's wages, about half its takings): each shop hour,
    // what its till holds over its float and tonight's wages, spread over the hours left, on the city's goods a farm uses
    // (country_farms.json `basket`: carts, barrels and hoops, tools, rope and nets, building goods), from their makers'
    // open shops, as the Country buys; it pays the carriage out.
    const auto& goods = items::farms().basket;
    if (goods.empty() || std::none_of(farms_.begin(), farms_.end(), [&](const FarmNow& f) { return farmAreaOn(f.area) && account(f.till) && !f.rents(); }))
        return;
    std::vector<std::pair<const std::string*, double>> basket;
    for (const auto& [item, share] : goods)
        basket.push_back({&item, double(share)});
    const auto open = openShops();
    for (const auto& f : farms_)
    {
        if (!farmAreaOn(f.area) || !account(f.till) || f.rents())
            continue;                               // (A works pays its rent instead: payRents.)
        auto keep = f.reserve + (hod < PayHour ? f.wages() + landRentDue(f) : 0);   // (Its land rent kept back too.)
        auto& today = loopToday_.farms[f.def->id];
        // (Its design's basket a day at most, its carriage within it: the rest is its House's tonight, payRents.)
        if (const auto most = farmBasketMost(f); most >= 0)
            keep = std::max(keep, cash(f.till) - (most - today.basket - today.basketCarriage));
        const auto spare = cash(f.till) - keep;
        if (spare <= 0)
            continue;
        today.basket += basketBuys(f.till, basket, double(spare) / double(ShopShut - hod), keep, hod, open, "to a farm",
                                   "used at the farm", &today.basketCarriage, true, true);   // (The workshops first; the Wharf last.)
    }
}

int Society::farmAreaOf(const std::string& item) const
{
    if (areaOfGood_.empty())
        return -1;
    const auto found = areaOfGood_.find(items::baseOf(item));
    return found != areaOfGood_.end() && farmAreaOn(found->second) ? int(found->second) : -1;
}

int Society::farmsHold(const std::string& item) const
{
    const int area = farmAreaOf(item);
    if (area < 0)
        return 0;
    int held = 0;
    for (const auto& f : farms_)
        if (farmSells(f, std::size_t(area), item))
            if (const auto* till = account(f.till))
                held += stock(*till, item);
    return held;
}

bool Society::farmSells(const FarmNow& f, std::size_t area, const std::string& item) const
{
    // A good is one area's (the first that names it, country_farms.json); a farm of another area on may grow it too (doc
    // 72, the iron: the Frosthollow hearth's charcoal for the ironworks, the iron area's, sold beside the woods' own).
    return f.area == area || (f.def && farmAreaOn(f.area) && f.def->yield.count(items::baseOf(item)));
}

double Society::farmPriceFactor(const std::string& item) const
{
    const int area = farmAreaOf(item);
    if (area < 0)
        return 0;
    const int f = items::farms().areas[std::size_t(area)].priceFactor;
    const auto own = authored_.economy.farms.areas.find(items::farms().areas[std::size_t(area)].id);
    const int factor = own != authored_.economy.farms.areas.end() && own->second.priceFactor >= 0 ? own->second.priceFactor : f;
    return factor > 0 ? factor / 100. : 0;
}

double Society::wharfBasketShift() const
{
    // The share of the food areas' takings (the design's, country_farms.json) whose areas are on. (The works' areas are
    // not food: their goods were never in the Country's basket, and their rent keeps its purse; doc 72.)
    const auto& areas = items::farms().areas;
    double on = 0, all = 0;
    for (std::size_t a = 0; a < areas.size(); ++a)
    {
        if (areas[a].works)
            continue;
        all += areas[a].takings;
        if (farmAreaOn(a))
            on += areas[a].takings;
    }
    return all > 0 ? std::clamp(on / all, 0., 1.) : 0.;
}

double Society::tableShare() const
{
    // The farms' `table` feeds the Country's own people (doc 71: the Houses' estates' 24 and the 68 workers of its other
    // industries). The works' crews are those workers, placed (doc 72): with their area on they live in the realm and buy
    // their food in the shops like any household, so they eat at the table no more, or they would eat twice (in country2
    // the table took 8-12 oats, 8-13 milk and 10 salt a day, the fuel's 15 among its 92).
    const auto& d = items::farms();
    const int people = d.estatePeople + d.otherWorkers;
    if (people <= 0)
        return 0;
    int placed = 0;
    for (const auto& f : farms_)
        if (f.works && farmAreaOn(f.area))
            placed += f.crewSize();
    return std::clamp(double(people - placed) / people, 0., 1.);
}

bool Society::farmSwitchTill(const std::string& till) const
{
    // A till the switch moves orders from or to (doc 71, 2.6): a farm's, or a shop of a trade that loses the Country's.
    if (farmOf_.count(till))
        return true;
    const auto& trades = items::farms().switchTrades;
    if (trades.empty() || till.rfind("till:", 0) != 0)
        return false;
    const auto* p = position(till.substr(5));
    const auto* business = p ? items::businessFor(p->title) : nullptr;
    return business && std::find(trades.begin(), trades.end(), business->id) != trades.end();
}
} // namespace ratw
