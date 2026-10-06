// The economy orchestrator (Docs/Design/46-economy-orchestrator.md): the day's plan, its saved state, and its thread.
#include "RatwOrchestratorJson.h"

#include <algorithm>
#include <cmath>
#include <set>

namespace ratw::orchestra
{
using json::Value;

const char* kindName(HolderKind kind)
{
    switch (kind)
    {
    case HolderKind::Till: return "till";
    case HolderKind::Keeper: return "keeper";
    case HolderKind::Producer: return "producer";
    case HolderKind::House: return "house";
    case HolderKind::Church: return "church";
    case HolderKind::Treasury: return "treasury";
    case HolderKind::Capital: return "capital";
    case HolderKind::Buyer: return "buyer";
    }
    return "till";
}
namespace
{
HolderKind kindNamed(const std::string& name)
{
    for (const auto k : {HolderKind::Till, HolderKind::Keeper, HolderKind::Producer, HolderKind::House, HolderKind::Church,
                         HolderKind::Treasury, HolderKind::Capital, HolderKind::Buyer})
        if (name == kindName(k))
            return k;
    return HolderKind::Till;
}
} // namespace

const std::vector<std::string>& channelNames()
{
    static const std::vector<std::string> names = {"works", "hires", "commissions", "food", "price support",
                                                   "wage support", "trade", "rescue", "opening"};
    return names;
}

bool channelLive(const std::string& channel)
{
    static const std::vector<std::string> live = {"works", "hires", "commissions", "food", "trade", "wage support", "price support", "rescue"};
    return std::find(live.begin(), live.end(), channel) != live.end();
}

const std::vector<std::string>& wageKinds()
{
    static const std::vector<std::string> kinds = {"help", "guard", "labour", "clergy", "keeper", "hand", "odd job"};
    return kinds;
}

bool validSteer(const Steer& s, std::string& problem)
{
    const auto within = [&](double lo, double hi) {
        if (!std::isfinite(s.strength) || s.strength < lo || s.strength > hi)
        {
            problem = "A " + s.kind + " steer's strength is " + std::to_string(lo).substr(0, 3) + " to " +
                      std::to_string(hi).substr(0, 3) + ".";
            return false;
        }
        return true;
    };
    if (s.until <= s.from || s.until - s.from > 56)
        return problem = "A steer lasts 1 to 56 days.", false;
    if (s.note.size() > 200)
        return problem = "A steer's note is at most 200 characters.", false;
    if (s.kind == "pressure")
        return within(.5, 3);
    if (s.kind == "town")
        return s.target.empty() ? (problem = "Which town?", false) : within(0, 3);
    if (s.kind == "holder")
    {
        if (s.target.empty() || s.target.size() > 120)
            return problem = "Which holder?", false;
        if (s.strength == 0)
            return true;                             // (Spared.)
        return within(1.5, 4);
    }
    if (s.kind == "channel")
    {
        const auto& names = channelNames();
        if (std::find(names.begin(), names.end(), s.target) == names.end())
            return problem = "No such channel.", false;
        return within(0, 3);
    }
    if (s.kind == "price")
        return s.target.empty() || s.item.empty() || s.item.size() > 60 ? (problem = "A price steer needs a town (or *) and a good.", false)
                                                                         : within(.5, 3);
    problem = "No such steer.";
    return false;
}

bool readDials(const Value& doc, Dials& d, std::string& problem)
{
    if (!doc.isObject())
        return problem = "The orchestrator's dials are not a JSON object.", false;
    const auto num = [&](const char* key, double& into) {
        if (const auto* v = doc.find(key); v && v->isNumber() && std::isfinite(v->asNumber()))
            into = v->asNumber();
    };
    if (const auto* m = doc.find("mode"); m && m->isString())
    {
        if (m->asString() != "shadow" && m->asString() != "on" && m->asString() != "off")
            return problem = "The orchestrator's mode is shadow, on or off.", false;
        d.mode = m->asString();
    }
    num("nourishADay", d.nourishADay);
    num("needDays", d.needDays);
    num("comfortable", d.comfortable);
    num("cap", d.cap);
    num("overShare", d.overShare);
    num("overShareAtCap", d.overShareAtCap);
    num("capShareOfLand", d.capShareOfLand);
    num("gainShare", d.gainShare);
    num("shareSlip", d.shareSlip);
    num("autoRaise", d.autoRaise);
    num("autoEase", d.autoEase);
    num("autoMost", d.autoMost);
    num("floorRaise", d.floorRaise);
    num("floorMost", d.floorMost);
    num("distressComfortable", d.distressComfortable);
    num("distressSpendBoost", d.distressSpendBoost);
    num("landShare", d.landShare);
    num("channelMostShare", d.channelMostShare);
    num("wageFloorOverFood", d.wageFloorOverFood);
    num("lodgingADay", d.lodgingADay);
    num("wageRaise", d.wageRaise);
    num("wageEase", d.wageEase);
    num("wageMost", d.wageMost);
    for (const auto& [kind, v] : doc.object("wages").fields())
        if (v.isNumber() && v.asNumber() > 0 && v.asNumber() < 1e4)
            d.wageStart[kind] = v.asNumber();
    num("stapleIncomeShare", d.stapleIncomeShare);
    num("priceMove", d.priceMove);
    num("priceLow", d.priceLow);
    num("priceHigh", d.priceHigh);
    num("marginStart", d.marginStart);
    num("marginLow", d.marginLow);
    num("marginHigh", d.marginHigh);
    num("marginMove", d.marginMove);
    num("smoothDays", d.smoothDays);
    const auto& w = doc.object("weights");
    if (w.isObject())
    {
        const auto wn = [&](const char* key, double& into) {
            if (const auto* v = w.find(key); v && v->isNumber() && v->asNumber() >= 0)
                into = v->asNumber();
        };
        wn("hungry", d.wHungry);
        wn("starving", d.wStarving);
        wn("short", d.wShort);
        wn("poor", d.wPoor);
        wn("idle", d.wIdle);
        wn("shelves", d.wShelves);
        wn("trade", d.wTrade);
        wn("drain", d.wDrain);
    }
    const auto& f = doc.object("floors");
    if (f.isObject())
    {
        const auto fn = [&](const char* key, double& into) {
            if (const auto* v = f.find(key); v && v->isNumber() && v->asNumber() >= 0)
                into = v->asNumber();
        };
        fn("townHead", d.floorTownHead);
        fn("churchHead", d.floorChurchHead);
        fn("house", d.floorHouse);
        fn("till", d.floorTill);
        fn("keeper", d.floorKeeper);
        fn("producer", d.floorProducer);
        fn("buyer", d.floorBuyer);
    }
    if (d.needDays <= 0 || d.comfortable < 1 || d.cap <= d.comfortable || d.distressComfortable < 1 || d.smoothDays < 1 ||
        d.priceLow <= 0 || d.priceHigh < d.priceLow || d.marginLow <= 0 || d.marginHigh < d.marginLow || d.nourishADay <= 0)
        return problem = "The orchestrator's dials are out of range.", false;
    return true;
}

bool readDialsText(const std::string& text, Dials& dials, std::string& problem)
{
    Value doc;
    return json::parse(text, doc, problem) && readDials(doc, dials, problem);
}

namespace
{
double clamp01(double x) { return std::clamp(x, 0.0, 1.0); }

// Channels that suit each kind of distress (doc 46, Part 7), before the steers and the cap on any one channel.
std::map<std::string, double> fitFor(const std::string& kind)
{
    // (Work that pays wolves at once, the works and hires, first; goods ordered from its makers and farms, which pay
    // wolves only through their tills, after.)
    if (kind == "empty shelves")
        return {{"food", 3}, {"works", 1}, {"trade", 1}};
    if (kind == "empty purses")
        return {{"works", 3}, {"hires", 2}, {"commissions", 1}, {"price support", 1}};
    if (kind == "no work")
        return {{"works", 3}, {"hires", 3}, {"opening", 1}};
    if (kind == "failing trade")
        return {{"hires", 2}, {"commissions", 2}, {"trade", 1}, {"wage support", 1}, {"rescue", 1}};
    if (kind == "draining")
        return {{"trade", 2}, {"works", 1}, {"commissions", 1}};
    return {{"works", 2}, {"hires", 2}, {"commissions", 1}, {"trade", 1}};   // (A town that is well: ordinary demand.)
}

// Shares `amount` out by `weights` (each at most `most` of it, the rest going to the others), in whole pennies, the
// remainder to the heaviest. The order of the map keeps it deterministic.
std::map<std::string, std::int64_t> share(std::int64_t amount, std::map<std::string, double> weights, double most)
{
    std::map<std::string, std::int64_t> out;
    double total = 0;
    for (auto it = weights.begin(); it != weights.end();)
        if (it->second <= 0)
            it = weights.erase(it);
        else
            total += (it++)->second;
    if (amount <= 0 || total <= 0)
        return out;
    std::map<std::string, double> part;
    for (const auto& [k, w] : weights)
        part[k] = w / total;
    // No channel over `most`, while there are others to take the rest.
    if (weights.size() > 1 && most > 0 && most < 1)
        for (int round = 0; round < 8; ++round)
        {
            double over = 0, under = 0;
            for (auto& [k, p] : part)
                if (p > most + 1e-12)
                    over += p - most, p = most;
            if (over <= 1e-12)
                break;
            for (const auto& [k, p] : part)
                if (p < most - 1e-12)
                    under += p;
            if (under <= 0)
                break;
            for (auto& [k, p] : part)
                if (p < most - 1e-12)
                    p += over * p / under;
        }
    std::int64_t given = 0;
    std::string heaviest;
    double best = -1;
    for (const auto& [k, p] : part)
    {
        const auto n = std::int64_t(std::floor(double(amount) * p));
        out[k] = n;
        given += n;
        if (p > best)
            best = p, heaviest = k;
    }
    if (!heaviest.empty())
        out[heaviest] += amount - given;
    for (auto it = out.begin(); it != out.end();)
        it = it->second <= 0 ? out.erase(it) : std::next(it);
    return out;
}
} // namespace

Brief plan(const Snapshot& s, Memory& memory)
{
    const auto& d = s.dials;
    Brief brief;
    brief.day = s.day;
    brief.mode = d.mode;
    brief.moneySupply = s.moneySupply;

    // The steers in force today.
    double pressure = 1;
    std::map<std::string, double> townWeight, holderFactor, channelWeight;
    std::map<std::string, double> priceShock;        // "town|item" ("*|item": every town).
    for (const auto& st : s.steers)
    {
        if (st.from > s.day || st.until <= s.day)
            continue;
        brief.steers.push_back(st);
        if (st.kind == "pressure")
            pressure *= st.strength;
        else if (st.kind == "town")
            townWeight[st.target] = st.strength;
        else if (st.kind == "holder")
            holderFactor[st.target] = st.strength;
        else if (st.kind == "channel")
            channelWeight[st.target] = st.strength;
        else if (st.kind == "price")
            priceShock[st.target + "|" + st.item] = st.strength;
    }
    pressure = std::clamp(pressure, .5, 3.);

    // --- Reading the land (Part 2) -----------------------------------------------------------------------------------
    std::map<std::string, TownReading> towns;
    std::map<std::string, std::vector<const ResidentSnap*>> people;
    for (const auto& r : s.residents)
        if (!r.town.empty())
            people[r.town].push_back(&r);
    // Food cost: the cheapest food a town's shops have in, a day of it; a town with none pays the land's cheapest, carted in.
    std::map<std::string, double> cheapest;           // Town -> pennies a nourishment.
    double landCheapest = 0;
    std::map<std::string, double> shopFood;           // Town -> nourishment on its shops' shelves.
    for (const auto& g : s.goods)
        if (g.nourish > 0 && g.stock > 0 && g.price > 0)
        {
            const double per = g.price / g.nourish;
            auto& c = cheapest[g.town];
            if (c <= 0 || per < c)
                c = per;
            if (landCheapest <= 0 || per < landCheapest)
                landCheapest = per;
            shopFood[g.town] += double(g.stock) * g.nourish;
        }
    std::map<std::string, std::pair<std::int64_t, std::int64_t>> takings;   // Town -> takings, running.
    for (const auto& sh : s.shops)
    {
        auto& t = takings[sh.town];
        t.first += sh.takings;
        t.second += sh.running;
    }
    std::map<std::string, const TownSnap*> townSnaps;
    for (const auto& t : s.towns)
        townSnaps[t.id] = &t;
    std::map<std::string, std::pair<int, int>> homesShort;   // Town -> short households, households.
    for (const auto& [id, residents] : people)
    {
        auto& t = towns[id];
        t.id = id;
        t.people = int(residents.size());
        const auto c = cheapest.find(id);
        t.foodCost = (c != cheapest.end() ? c->second : landCheapest > 0 ? landCheapest * 1.5 : 5. / d.nourishADay) * d.nourishADay;
    }
    for (const auto& h : s.homes)
    {
        auto it = towns.find(h.town);
        if (it == towns.end() || h.members <= 0)
            continue;
        const double days = double(h.nourishment) / (d.nourishADay * h.members) + double(h.cash) / (it->second.foodCost * h.members);
        auto& hs = homesShort[h.town];
        hs.first += days < 7;
        ++hs.second;
    }
    // A town's poorest earners' day (the lowest quarter of those who earned this week), for the staple ceiling.
    std::map<std::string, double> lowEarner;
    std::map<std::string, double> idleShares;         // Town -> the share of those able to work who earned nothing lately.
    for (auto& [id, t] : towns)
    {
        const auto& residents = people[id];
        int hungry = 0, starving = 0, poor = 0, grown = 0, workers = 0, idle = 0;
        std::vector<double> earnings;
        for (const auto* r : residents)
        {
            hungry += r->hunger >= 70;
            starving += r->hunger >= 90;
            if (r->age >= 16)
            {
                ++grown;
                poor += double(r->cash) < 7 * t.foodCost;
            }
            if (r->age >= 16 && r->age < 65 && !r->dependent)
            {
                ++workers;
                idle += r->earned3 <= 0;
                if (r->earned7 > 0)
                    earnings.push_back(double(r->earned7) / 7);
            }
        }
        std::sort(earnings.begin(), earnings.end());
        lowEarner[id] = earnings.empty() ? t.foodCost : earnings[earnings.size() / 4];
        const double n = std::max(1, t.people);
        t.hungry = hungry / n;
        t.starving = starving / n;
        t.poor = grown ? double(poor) / grown : 0;
        t.idle = s.counted >= 3 ? idle : -1;        // (-1: not known yet.)
        const auto hs = homesShort[id];
        t.short_ = hs.second ? double(hs.first) / hs.second : 0;
        t.shopFoodDays = shopFood[id] / (d.nourishADay * n);
        const auto tk = takings[id];
        t.takingsRatio = tk.second > 0 ? double(tk.first) / double(tk.second) : 1;
        if (const auto ts = townSnaps.find(id); ts != townSnaps.end())
            t.netInflow = ts->second->inflow - ts->second->outflow;
        // (Draining is judged on a week's average: a reckoning's day, or a caravan's, is no trend.)
        const auto knownNet = memory.net.find(id);
        const double net = knownNet == memory.net.end() ? double(t.netInflow) : knownNet->second + (double(t.netInflow) - knownNet->second) / 7;
        memory.net[id] = net;
        t.wageFloor = std::int64_t(std::ceil(t.foodCost * d.wageFloorOverFood * std::max(1.0, memory.floorLift) + d.lodgingADay));
        idleShares[id] = workers && s.counted >= 3 ? double(idle) / workers : 0;

        // Distress: each sensor as a part of a crisis (0..1), weighed, and saturating (1 - e^-sum).
        const double cHungry = clamp01(t.hungry / .2), cStarving = clamp01(t.starving / .05), cShort = clamp01(t.short_ / .5),
                     cPoor = clamp01(t.poor / .5), cIdle = clamp01(workers && s.counted >= 3 ? double(idle) / workers / .5 : 0),
                     cShelves = clamp01((2 - t.shopFoodDays) / 2), cTrade = clamp01(1 - t.takingsRatio),
                     cDrain = clamp01(-net / (n * t.foodCost));
        const double sum = d.wHungry * cHungry + d.wStarving * cStarving + d.wShort * cShort + d.wPoor * cPoor + d.wIdle * cIdle +
                           d.wShelves * cShelves * (cHungry + cShort > 0 ? 1 : .1) + d.wTrade * cTrade + d.wDrain * cDrain;
        double raw = 1 - std::exp(-sum);
        if (const auto w = townWeight.find(id); w != townWeight.end())
            raw = clamp01(raw * w->second);
        t.raw = raw;
        const auto known = memory.distress.find(id);
        t.distress = memory.day < 0 || known == memory.distress.end() ? raw : known->second + (raw - known->second) / d.smoothDays;
        memory.distress[id] = t.distress;
        auto& week = memory.week[id];
        week.first += raw;
        ++week.second;
        // Its kind: what its trouble mostly is.
        const double need = cHungry + cStarving + cShort;
        const std::pair<const char*, double> kinds[] = {{"empty shelves", (need + .25 * cPoor) * cShelves},
                                                        {"empty purses", (need + cPoor) * (1 - cShelves)},
                                                        {"no work", cIdle},
                                                        {"failing trade", cTrade},
                                                        {"draining", cDrain}};
        double best = 0;
        t.kind.clear();
        if (t.distress >= .1)
            for (const auto& [name, score] : kinds)
                if (score > best + 1e-9)
                    best = score, t.kind = name;
        for (const auto& [name, score] : kinds)
            memory.weekKinds[id + "|" + name] += score;
        // On a decision's day: the week's distress, and the kind of trouble it mostly had.
        if (s.decide)
        {
            t.week = week.first / std::max(1, week.second);
            best = 0;
            t.kind.clear();
            if (t.week >= .1)
                for (const auto& [name, score] : kinds)
                    if (const double w = memory.weekKinds[id + "|" + name]; w > best + 1e-9)
                        best = w, t.kind = name;
        }
    }
    double landPeople = 0, landWeighted = 0;
    for (const auto& [id, t] : towns)
        landPeople += t.people, landWeighted += t.distress * t.people;
    brief.landDistress = landPeople > 0 ? landWeighted / landPeople : 0;
    // A decision weighs the week: each town's distress its week's mean.
    double D = brief.landDistress;
    if (s.decide)
    {
        double weekWeighted = 0;
        for (const auto& [id, t] : towns)
            weekWeighted += t.week * t.people;
        D = landPeople > 0 ? weekWeighted / landPeople : 0;
        brief.landDistress = D;
        brief.decided = true;
    }

    // --- Its own pressure: from how the residents' share of the land's money moved over the week ----------------------------
    {
        double held = 0;
        for (const auto& r : s.residents)
            held += double(std::max<std::int64_t>(0, r.cash));
        brief.residentShare = s.moneySupply > 0 ? held / double(s.moneySupply) : 0;
        if (s.decide)
        {
            if (memory.residentShare >= 0 && brief.residentShare < memory.residentShare - d.shareSlip)
                memory.autoPressure = std::min(d.autoMost, memory.autoPressure * d.autoRaise);
            else if (memory.residentShare >= 0 && brief.residentShare > memory.residentShare + d.shareSlip)
                memory.autoPressure = std::max(1.0, memory.autoPressure * d.autoEase);
            memory.residentShare = brief.residentShare;
        }
        brief.autoPressure = memory.autoPressure;
        pressure = std::clamp(pressure * memory.autoPressure, .5, 3.);
        // The poorer half's share of the residents' money (Phase 6), by household: the households holding least a head,
        // a half of everyone living in them (a child's own purse is small; its household's is what feeds it). Falling, the
        // living floor rises (floorLift); rising, it eases back. (Its first measure only notes it.)
        std::vector<std::pair<double, const HomeSnap*>> homes;
        double people = 0, homesHeld = 0;
        for (const auto& h : s.homes)
            if (h.members > 0)
            {
                homes.push_back({double(std::max<std::int64_t>(0, h.cash)) / h.members, &h});
                people += h.members;
                homesHeld += double(std::max<std::int64_t>(0, h.cash));
            }
        std::sort(homes.begin(), homes.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
        double bottom = 0, counted = 0;
        for (const auto& [perHead, h] : homes)
        {
            if (counted >= people / 2)
                break;
            bottom += double(std::max<std::int64_t>(0, h->cash));
            counted += h->members;
        }
        brief.bottomShare = homesHeld > 0 ? bottom / homesHeld : 0;
        if (s.decide)
        {
            if (memory.bottomShare >= 0 && brief.bottomShare < memory.bottomShare - d.shareSlip)
                memory.floorLift = std::min(d.floorMost, memory.floorLift * d.floorRaise);
            else if (memory.bottomShare >= 0 && brief.bottomShare > memory.bottomShare + d.shareSlip)
                memory.floorLift = std::max(1.0, memory.floorLift * d.autoEase);
            memory.bottomShare = brief.bottomShare;
        }
        brief.floorLift = memory.floorLift;
        for (auto& [id, t] : towns)
            t.wageFloor = std::int64_t(std::ceil(t.foodCost * d.wageFloorOverFood * memory.floorLift + d.lodgingADay));
    }

    // --- Bands (Part 6) ------------------------------------------------------------------------------------------------
    const double top = std::max(1.0, (d.comfortable - (d.comfortable - d.distressComfortable) * D) / std::sqrt(pressure));
    const double capNeeds = std::max(top + .5, d.cap / std::sqrt(pressure));
    const double landCap = d.capShareOfLand * double(s.moneySupply);
    std::map<std::string, std::int64_t> localSpend;  // Holder -> what goes to its own town.
    for (const auto& h : s.holders)
    {
        if (s.counted < 1)
        {
            // Nothing counted yet: no band until a day's spending is known.
            HolderBand b;
            b.id = h.id, b.kind = h.kind, b.town = h.town, b.cash = h.cash, b.need = h.floor, b.band = "warming";
            brief.holders.push_back(b);
            continue;
        }
        const auto seen = memory.spent.find(h.id);
        const double avg = seen == memory.spent.end() ? double(h.spent) : seen->second + (double(h.spent) - seen->second) / 7;
        memory.spent[h.id] = avg;
        HolderBand b;
        b.id = h.id;
        b.kind = h.kind;
        b.town = h.town;
        b.cash = h.cash;
        b.need = std::max<std::int64_t>(h.floor, std::int64_t(std::ceil(d.needDays * avg)));
        const auto f = holderFactor.find(h.id);
        if (f != holderFactor.end() && f->second == 0)
        {
            b.band = "spared";
            brief.holders.push_back(b);
            continue;
        }
        const bool squeezed = f != holderFactor.end();
        const double eff = double(h.cash) * (squeezed ? f->second : 1);
        const double topLine = top * double(b.need);
        const double capLine = std::min(capNeeds * double(b.need), std::max(landCap, topLine + double(b.need) * .5));
        double spend = 0;
        if (eff < double(b.need))
            b.band = "lean";
        else if (eff <= topLine)
            b.band = "comfortable";
        else
        {
            const double boost = (1 + (d.distressSpendBoost - 1) * D) * pressure;
            if (eff <= capLine)
            {
                b.band = "over";
                const double into = capLine > topLine ? (eff - topLine) / (capLine - topLine) : 1;
                spend = std::min(1.0, (d.overShare + (d.overShareAtCap - d.overShare) * into) * boost) * (eff - topLine);
            }
            else
            {
                b.band = "cap";
                spend = (eff - capLine) + std::min(1.0, d.overShareAtCap * boost) * (capLine - topLine);
            }
        }
        // Never below its need (squeezed), or below the band's top (not).
        double keep = squeezed ? double(b.need) : topLine;
        // Growth: what it gained over the week, a share of it back out (whatever its band), keeping its floor (a till's
        // float, a town's or church's few pennies a head): a business whose takings outrun its costs is pooling, however
        // much of its need is turnover.
        const auto start = memory.weekStart.find(h.id);
        if (s.decide && start != memory.weekStart.end())
            b.gain = h.cash - start->second;
        if (s.decide && start != memory.weekStart.end() && h.cash > h.floor)
            {
                const double grown = double(std::max<std::int64_t>(0, b.gain)) * std::min(1.0, d.gainShare * (1 + D) * pressure);
                if (grown > spend)
                {
                    spend = grown;
                    keep = double(std::max<std::int64_t>(h.floor, 0));
                    if (b.band == "comfortable" || b.band == "lean")
                        b.band = "growing";
                }
            }
        // Only a decision sends money out: on other days the band is a measure.
        if (s.decide)
            b.toSpend = std::max<std::int64_t>(0, std::min<std::int64_t>(std::int64_t(std::floor(spend)), h.cash - std::int64_t(std::ceil(keep))));
        brief.holders.push_back(b);
        brief.pot += b.toSpend;
    }
    for (const auto& b : brief.holders)
        ++brief.bands[b.band];
    // The week starts again after a decision: from what each holder will hold once it has spent.
    if (s.decide)
        for (const auto& b : brief.holders)
            memory.weekStart[b.id] = b.cash - b.toSpend;
    for (const auto& h : s.holders)
        if (!memory.weekStart.count(h.id))
            memory.weekStart[h.id] = h.cash;     // (First seen: its week starts now.)
    {
        std::set<std::string> present;
        for (const auto& h : s.holders)
            present.insert(h.id);
        for (auto it = memory.spent.begin(); it != memory.spent.end();)
            it = present.count(it->first) ? std::next(it) : memory.spent.erase(it);
        for (auto it = memory.weekStart.begin(); it != memory.weekStart.end();)
            it = present.count(it->first) ? std::next(it) : memory.weekStart.erase(it);
    }

    // --- Prices and the margin (Part 4): decided once a week; a good first seen is priced at once ------------------------
    for (const auto& g : s.goods)
    {
        const auto key = g.town + "|" + g.item;
        auto prev = memory.price.find(key);
        if (g.catalog <= 0 || (!s.decide && prev != memory.price.end()))
            continue;
        // Its shelves against what its shops mean to keep: a fifth dearer when they are bare, a fifth cheaper with three
        // times as much; and dearer again when what is there would sell in under two days (sales missed counting).
        const double r = double(g.stock) / std::max(1, g.kept);
        const double shelf = r <= .5 ? 1.2 : r >= 3 ? .8 : 1.2 - .4 * (r - .5) / 2.5;
        const double days = double(g.stock) / std::max(.1, g.rate);
        const double scarce = days < 2 ? 1 + .15 * (2 - days) / 2 : 1;
        double target = std::clamp(shelf * scarce, d.priceLow, d.priceHigh) * double(g.catalog);
        for (const auto* key : {&g.town, static_cast<const std::string*>(nullptr)})
        {
            const auto shock = priceShock.find((key ? *key : std::string("*")) + "|" + g.item);
            if (shock != priceShock.end())
                target *= shock->second;
        }
        const double natural = target;
        if (g.staple && g.nourish > 0)
        {
            // A day's plain food at most a share of what the town's poorest earners make in a day.
            const double ceiling = d.stapleIncomeShare * lowEarner[g.town] * g.nourish / d.nourishADay;
            if (ceiling > 0)
                target = std::min(target, std::max(ceiling, d.priceLow * double(g.catalog)));
        }
        // A week's move at most; a good first seen goes straight to its price.
        const double from = prev != memory.price.end() ? prev->second : target;
        const double would = std::clamp(target, from * (1 - d.priceMove), from * (1 + d.priceMove));
        memory.price[key] = would;
        // Price support (Phase 6): a staple held under its price for the poor; its shops are paid the gap on what they sell.
        if (natural - would > .05 && g.staple)
            memory.support[key] = natural - would;
        else
            memory.support.erase(key);
        brief.prices.push_back({g.town, g.item, g.catalog, g.price, would, memory.support.count(key) ? memory.support[key] : 0});
    }
    // --- Where the pot goes (Part 7) -----------------------------------------------------------------------------------
    // Up to half of a town holder's spending stays in its own town (money stays local where it can): half when the land is
    // well or its town hurts as much as the land, less as its town is better off. The rest, and everything the land-wide
    // holders (the capital, the church) spend, is shared by distress and people.
    std::map<std::string, double> byDistress, byPeople;
    for (const auto& [id, t] : towns)
        byDistress[id] = std::max(0.0, s.decide ? t.week : t.distress) * t.people, byPeople[id] = t.people;
    double distressTotal = 0;
    for (const auto& [id, w] : byDistress)
        distressTotal += w;
    std::map<std::string, std::map<std::string, double>> fits;   // Town -> channel -> weight.
    for (const auto& [id, t] : towns)
    {
        auto f = fitFor(t.kind);
        for (auto& [c, w] : f)
            if (const auto cw = channelWeight.find(c); cw != channelWeight.end())
                w *= cw->second;
        // A channel favoured by a steer but not in the kind's list comes in at its strength.
        for (const auto& [c, w] : channelWeight)
            if (!f.count(c) && w > 1)
                f[c] = w - 1;
        // Only channels that are built spend, and the needs (wage and price support, rescue) are met first, by need, not
        // here; with none left (all closed by steers), ordinary demand: commissions.
        for (auto it = f.begin(); it != f.end();)
            it = channelLive(it->first) && it->second > 0 && it->first != "wage support" && it->first != "price support" &&
                         it->first != "rescue"
                     ? std::next(it)
                     : f.erase(it);
        if (f.empty())
            f["commissions"] = 1;
        // Town works pay only as many hands as there are idle: two days' work for each, at the living wage.
        fits[id] = f;
    }
    std::map<std::string, std::int64_t> townShare;
    const auto send = [&](const std::string& from, const std::string& town, std::int64_t coins) {
        if (coins <= 0 || !towns.count(town))
            return;
        townShare[town] += coins;
        for (const auto& [channel, n] : share(coins, fits[town], d.channelMostShare))
        {
            brief.orders.push_back({from, town, channel, n});
            brief.channels[channel] += n;
        }
    };
    std::int64_t pooled = 0;
    for (const auto& b : brief.holders)
    {
        if (b.toSpend <= 0)
            continue;
        const bool local = !b.town.empty() && towns.count(b.town) && b.kind != HolderKind::Capital && b.kind != HolderKind::Church;
        double keep = 0;
        if (local)
        {
            const auto& t = towns.at(b.town);
            const double own = s.decide ? t.week : t.distress;
            keep = D < .1 ? .5 : .5 * std::clamp(own / D, 0.0, 1.0);
        }
        const auto here = std::int64_t(std::floor(double(b.toSpend) * keep));
        send(b.id, b.town, here);
        pooled += b.toSpend - here;
    }
    // The needs first (Phase 6), from what is pooled, half of it at most: wages its payers can't pay, the staples' price
    // support, and the rescue of failing businesses; each town's less what its fund still holds.
    {
        const auto fundHeld = [&](const std::string& town, const std::string& channel) -> std::int64_t {
            const auto t = townSnaps.find(town);
            if (t == townSnaps.end())
                return 0;
            const auto f = t->second->funds.find(channel);
            return f == t->second->funds.end() ? 0 : f->second;
        };
        std::vector<std::tuple<std::string, std::string, double>> needs;   // Town, channel, coins.
        double wanted = 0;
        for (const auto& [id, t] : towns)
        {
            const auto snap = townSnaps.find(id);
            if (snap == townSnaps.end())
                continue;
            const auto& ts = *snap->second;
            const auto labour = memory.wage.count(id + "|labour") ? memory.wage.at(id + "|labour") : 12;
            double support = 0;
            for (const auto& g : s.goods)
                if (g.town == id)
                    if (const auto gap = memory.support.find(id + "|" + g.item); gap != memory.support.end())
                        support += gap->second * g.rate * 7;
            const std::pair<const char*, double> want[] = {{"wage support", double(ts.unpaid + ts.supported) * labour * 7},
                                                           {"price support", support},
                                                           {"rescue", double(ts.rescueNeed)}};
            for (const auto& [channel, coins] : want)
            {
                double n = coins - double(fundHeld(id, channel));
                if (const auto w = channelWeight.find(channel); w != channelWeight.end())
                    n *= w->second;
                if (n >= 1)
                    needs.emplace_back(id, channel, n), wanted += n;
            }
        }
        const double scale = wanted > 0 ? std::min(1.0, double(pooled) / 2 / wanted) : 0;
        for (const auto& [town, channel, coins] : needs)
            if (const auto n = std::int64_t(std::floor(coins * scale)); n > 0)
            {
                brief.orders.push_back({"land", town, channel, n});
                brief.channels[channel] += n;
                townShare[town] += n;
                pooled -= n;
            }
    }
    if (pooled > 0)
    {
        // The land's share by people alone, the rest by distress (by people too, when no town is in distress).
        const auto land = distressTotal > 0 ? std::int64_t(std::floor(double(pooled) * d.landShare)) : pooled;
        for (const auto& [town, n] : share(land, byPeople, 1))
            send("land", town, n);
        if (pooled - land > 0)
            for (const auto& [town, n] : share(pooled - land, byDistress, 1))
                send("land", town, n);
    }
    // The orders, one per giver, town and channel.
    {
        std::map<std::tuple<std::string, std::string, std::string>, std::int64_t> merged;
        for (const auto& o : brief.orders)
            merged[{o.from, o.town, o.channel}] += o.coins;
        brief.orders.clear();
        for (const auto& [k, n] : merged)
            brief.orders.push_back({std::get<0>(k), std::get<1>(k), std::get<2>(k), n});
    }
    for (auto& [id, t] : towns)
        t.share = townShare[id];

    // --- The wage table (Part 5): seeded at once, moved at the week's decision ----------------------------------------------
    // Its payers' books (Phase 5): what each kind's payers gained or lost over the week, against what they hold. Help and a
    // keeper's wage: the town's shops' tills; the watch and the town's labour: its treasury; hands: its farms;
    // the clergy: the land's church.
    std::map<std::string, std::map<std::string, std::pair<double, double>>> books;   // Town -> kind -> gain, held.
    std::pair<double, double> church{0, 0};
    for (const auto& b : brief.holders)
    {
        const auto add = [&](const std::string& kind) {
            auto& g = books[b.town][kind];
            g.first += double(b.gain);
            g.second += double(b.cash);
        };
        if (b.kind == HolderKind::Till || b.kind == HolderKind::Keeper)
            add("help"), add("keeper");
        else if (b.kind == HolderKind::Treasury || b.kind == HolderKind::Capital)
            add("labour"), add("guard");             // (Not its buyers: what they gain is the treasury's funding.)
        else if (b.kind == HolderKind::Producer)
            add("hand");
        else if (b.kind == HolderKind::Church)
            church.first += double(b.gain), church.second += double(b.cash);
    }
    for (const auto& [id, t] : towns)
    {
        const auto snap = townSnaps.find(id);
        const bool holdUp = t.distress >= .3 && (t.kind == "no work" || t.kind == "empty purses");
        for (const auto& kind : wageKinds())
        {
            const double start = d.wageStart.count(kind) ? d.wageStart.at(kind) : 8;
            const double floor = double(t.wageFloor) * (kind == "odd job" ? .25 : 1);
            const auto key = id + "|" + kind;
            auto known = memory.wage.find(key);
            double w = known != memory.wage.end() ? known->second : std::max(start, floor);
            if (s.decide && known != memory.wage.end())
            {
                const int begging = snap != townSnaps.end() && snap->second->unfilled.count(kind) ? snap->second->unfilled.at(kind) : 0;
                // Its payers gaining (more than a fiftieth of what they hold in the week) pay more: what pools in their
                // tills goes to those who work for them. Draining, they pay less, as far as the floor.
                const auto paid = kind == "clergy" ? church : books.count(id) && books[id].count(kind) ? books[id][kind] : std::pair<double, double>{0, 0};
                const double trend = paid.second > 0 ? paid.first / paid.second : 0;
                // (In step with the books: payers that gained a tenth of what they hold raise pay a fifth, up to a quarter.)
                if (trend > .02)
                    w *= 1 + std::min(.25, std::max(d.wageRaise, 2 * trend));
                else if (begging > 0)
                    w *= 1 + d.wageRaise;
                else if (trend < -.02 || (idleShares[id] > .1 && !holdUp))
                    w *= 1 - d.wageEase;
            }
            w = std::clamp(w, floor, std::max(floor, d.wageMost * start));
            memory.wage[key] = w;
            brief.wages[id][kind] = w;
        }
    }
    // The margin: between the shops' tills and the farms, toward whichever is short.
    {
        double shops = 0, farms = 0;
        for (const auto& b : brief.holders)
            if (b.kind == HolderKind::Till || b.kind == HolderKind::Keeper)
                shops += double(b.toSpend) - (b.band == "lean" ? double(b.need - b.cash) : 0);
            else if (b.kind == HolderKind::Producer)
                farms += double(b.toSpend) - (b.band == "lean" ? double(b.need - b.cash) : 0);
        if (memory.margin < 0)
            memory.margin = d.marginStart;
        const double signal = (shops - farms) / (std::abs(shops) + std::abs(farms) + 1);
        if (s.decide)
            memory.margin = std::clamp(memory.margin + d.marginMove * signal, d.marginLow, d.marginHigh);
        brief.margin = memory.margin;
    }

    // The residents' median purse and Gini, for the Dungeon Master.
    {
        std::vector<std::int64_t> purses;
        for (const auto& r : s.residents)
            purses.push_back(std::max<std::int64_t>(0, r.cash));
        std::sort(purses.begin(), purses.end());
        if (!purses.empty())
        {
            brief.median = purses[purses.size() / 2];
            double total = 0, weighted = 0;
            for (std::size_t i = 0; i < purses.size(); ++i)
                total += double(purses[i]), weighted += double(i + 1) * double(purses[i]);
            const double n = double(purses.size());
            brief.gini = total > 0 ? (2 * weighted) / (n * total) - (n + 1) / n : 0;
        }
    }
    for (auto& [id, t] : towns)
        brief.towns.push_back(t);
    std::sort(brief.towns.begin(), brief.towns.end(), [](const TownReading& a, const TownReading& b) {
        return a.distress != b.distress ? a.distress > b.distress : a.id < b.id;
    });
    memory.day = s.day;
    if (s.decide)
    {
        memory.week.clear();
        memory.weekKinds.clear();
        memory.decided = s.day;
    }
    return brief;
}

// --- Saving ------------------------------------------------------------------------------------------------------------
namespace
{
Value steerJson(const Steer& s)
{
    auto o = Value::object();
    o.add("id", s.id);
    o.add("kind", s.kind);
    o.add("target", s.target);
    o.add("item", s.item);
    o.add("strength", s.strength);
    o.add("from", double(s.from));
    o.add("until", double(s.until));
    o.add("note", s.note);
    o.add("by", s.by);
    return o;
}
Steer readSteer(const Value& v)
{
    Steer s;
    s.id = v.string("id");
    s.kind = v.string("kind");
    s.target = v.string("target");
    s.item = v.string("item");
    s.strength = v.number("strength", 1);
    s.from = std::int64_t(v.number("from"));
    s.until = std::int64_t(v.number("until"));
    s.note = v.string("note");
    s.by = v.string("by");
    return s;
}
double round3(double x) { return std::round(x * 1000) / 1000; }
} // namespace

Value briefJson(const Brief& b, bool full)
{
    auto o = Value::object();
    o.add("day", double(b.day));
    o.add("mode", b.mode);
    o.add("decided", b.decided);
    o.add("landDistress", round3(b.landDistress));
    o.add("moneySupply", double(b.moneySupply));
    o.add("pot", double(b.pot));
    o.add("margin", round3(b.margin));
    o.add("median", double(b.median));
    o.add("gini", round3(b.gini));
    o.add("residentShare", round3(b.residentShare));
    o.add("autoPressure", round3(b.autoPressure));
    o.add("bottomShare", round3(b.bottomShare));
    o.add("floorLift", round3(b.floorLift));
    auto towns = Value::array();
    for (const auto& t : b.towns)
    {
        auto x = Value::object();
        x.add("id", t.id);
        x.add("people", t.people);
        x.add("distress", round3(t.distress));
        x.add("raw", round3(t.raw));
        if (t.week >= 0)
            x.add("week", round3(t.week));
        x.add("kind", t.kind);
        x.add("foodCost", round3(t.foodCost));
        x.add("hungry", round3(t.hungry));
        x.add("starving", round3(t.starving));
        x.add("short", round3(t.short_));
        x.add("poor", round3(t.poor));
        x.add("idle", t.idle);
        x.add("shopFoodDays", round3(t.shopFoodDays));
        x.add("takingsRatio", round3(t.takingsRatio));
        x.add("netInflow", double(t.netInflow));
        x.add("wageFloor", double(t.wageFloor));
        x.add("share", double(t.share));
        if (const auto w = b.wages.find(t.id); w != b.wages.end())
        {
            auto table = Value::object();
            for (const auto& [kind, pay] : w->second)
                table.add(kind, round3(pay));
            x.add("wages", table);
        }
        towns.push(x);
    }
    o.add("towns", towns);
    auto holders = Value::array();
    for (const auto& h : b.holders)
    {
        if (!full && h.band != "over" && h.band != "cap" && h.band != "spared" && h.band != "growing")
            continue;
        auto x = Value::object();
        x.add("id", h.id);
        x.add("kind", kindName(h.kind));
        x.add("town", h.town);
        x.add("cash", double(h.cash));
        x.add("need", double(h.need));
        x.add("band", h.band);
        x.add("toSpend", double(h.toSpend));
        if (h.gain != 0)
            x.add("gain", double(h.gain));
        holders.push(x);
    }
    o.add("holders", holders);
    auto bands = Value::object();
    for (const auto& [band, n] : b.bands)
        bands.add(band, n);
    o.add("bands", bands);
    auto orders = Value::array();
    for (const auto& r : b.orders)
    {
        auto x = Value::object();
        x.add("from", r.from);
        x.add("town", r.town);
        x.add("channel", r.channel);
        x.add("coins", double(r.coins));
        orders.push(x);
    }
    o.add("orders", orders);
    auto channels = Value::object();
    for (const auto& [c, n] : b.channels)
        channels.add(c, double(n));
    o.add("channels", channels);
    // The prices: every one (full), or those furthest from the catalogue.
    std::vector<const PriceSet*> prices;
    for (const auto& p : b.prices)
        prices.push_back(&p);
    if (!full)
    {
        const auto off = [](const PriceSet* p) {
            const double c = double(std::max<std::int64_t>(1, p->catalog));
            return std::max(std::abs(p->now / c - 1), std::abs(p->would / c - 1));
        };
        std::stable_sort(prices.begin(), prices.end(), [&](const PriceSet* a, const PriceSet* c) { return off(a) > off(c); });
        if (prices.size() > 30)
            prices.resize(30);
    }
    auto priceList = Value::array();
    for (const auto* p : prices)
    {
        auto x = Value::object();
        x.add("town", p->town);
        x.add("item", p->item);
        x.add("catalog", double(p->catalog));
        x.add("now", round3(p->now));
        x.add("would", round3(p->would));
        if (p->support > 0)
            x.add("support", round3(p->support));
        priceList.push(x);
    }
    o.add("prices", priceList);
    return o;
}

namespace
{
Brief readBrief(const Value& o)
{
    Brief b;
    if (!o.isObject())
        return b;
    b.day = std::int64_t(o.number("day", -1));
    b.mode = o.string("mode");
    b.decided = o.boolean("decided");
    b.landDistress = o.number("landDistress");
    b.moneySupply = std::int64_t(o.number("moneySupply"));
    b.pot = std::int64_t(o.number("pot"));
    b.margin = o.number("margin");
    b.median = std::int64_t(o.number("median"));
    b.gini = o.number("gini");
    b.residentShare = o.number("residentShare");
    b.autoPressure = o.number("autoPressure", 1);
    b.bottomShare = o.number("bottomShare");
    b.floorLift = o.number("floorLift", 1);
    for (const auto& x : o.array("towns"))
    {
        TownReading t;
        t.id = x.string("id");
        t.people = int(x.number("people"));
        t.distress = x.number("distress");
        t.raw = x.number("raw");
        t.week = x.number("week", -1);
        t.kind = x.string("kind");
        t.foodCost = x.number("foodCost");
        t.hungry = x.number("hungry");
        t.starving = x.number("starving");
        t.short_ = x.number("short");
        t.poor = x.number("poor");
        t.idle = int(x.number("idle"));
        t.shopFoodDays = x.number("shopFoodDays");
        t.takingsRatio = x.number("takingsRatio");
        t.netInflow = std::int64_t(x.number("netInflow"));
        t.wageFloor = std::int64_t(x.number("wageFloor"));
        t.share = std::int64_t(x.number("share"));
        for (const auto& [kind, pay] : x.object("wages").fields())
            if (pay.isNumber())
                b.wages[t.id][kind] = pay.asNumber();
        b.towns.push_back(t);
    }
    for (const auto& x : o.array("holders"))
    {
        HolderBand h;
        h.id = x.string("id");
        h.kind = kindNamed(x.string("kind"));
        h.town = x.string("town");
        h.cash = std::int64_t(x.number("cash"));
        h.need = std::int64_t(x.number("need"));
        h.band = x.string("band");
        h.toSpend = std::int64_t(x.number("toSpend"));
        h.gain = std::int64_t(x.number("gain"));
        b.holders.push_back(h);
    }
    for (const auto& [band, n] : o.object("bands").fields())
        b.bands[band] = int(n.asNumber());
    for (const auto& x : o.array("orders"))
        b.orders.push_back({x.string("from"), x.string("town"), x.string("channel"), std::int64_t(x.number("coins"))});
    for (const auto& [c, n] : o.object("channels").fields())
        b.channels[c] = std::int64_t(n.asNumber());
    for (const auto& x : o.array("prices"))
        b.prices.push_back({x.string("town"), x.string("item"), std::int64_t(x.number("catalog")), x.number("now"), x.number("would"), x.number("support")});
    return b;
}
} // namespace

Value stateJson(const State& st)
{
    auto o = Value::object();
    o.add("mode", st.last.mode.empty() ? std::string("shadow") : st.last.mode);
    o.add("day", double(st.memory.day));
    o.add("nextSteer", double(st.nextSteer));
    auto steers = Value::array();
    for (const auto& s : st.steers)
        steers.push(steerJson(s));
    o.add("steers", steers);
    auto memory = Value::object(), spent = Value::object(), distress = Value::object(), price = Value::object();
    for (const auto& [k, v] : st.memory.spent)
        spent.add(k, round3(v));
    for (const auto& [k, v] : st.memory.distress)
        distress.add(k, v);
    for (const auto& [k, v] : st.memory.price)
        price.add(k, v);
    auto net = Value::object(), wage = Value::object();
    for (const auto& [k, v] : st.memory.net)
        net.add(k, v);
    for (const auto& [k, v] : st.memory.wage)
        wage.add(k, v);
    memory.add("net", net);
    memory.add("wage", wage);
    auto weekStart = Value::object();
    for (const auto& [k, v] : st.memory.weekStart)
        weekStart.add(k, double(v));
    memory.add("weekStart", weekStart);
    memory.add("spent", spent);
    memory.add("distress", distress);
    memory.add("price", price);
    auto week = Value::object(), weekKinds = Value::object();
    for (const auto& [k, v] : st.memory.week)
    {
        auto pair = Value::array();
        pair.push(v.first);
        pair.push(v.second);
        week.add(k, pair);
    }
    for (const auto& [k, v] : st.memory.weekKinds)
        weekKinds.add(k, v);
    memory.add("week", week);
    memory.add("weekKinds", weekKinds);
    memory.add("margin", st.memory.margin);
    memory.add("day", double(st.memory.day));
    memory.add("decided", double(st.memory.decided));
    memory.add("residentShare", st.memory.residentShare);
    memory.add("autoPressure", st.memory.autoPressure);
    memory.add("bottomShare", st.memory.bottomShare);
    memory.add("floorLift", st.memory.floorLift);
    auto support = Value::object();
    for (const auto& [k, v] : st.memory.support)
        support.add(k, v);
    memory.add("support", support);
    o.add("memory", memory);
    o.add("brief", briefJson(st.last, false));
    o.add("decision", briefJson(st.decision, false));
    return o;
}

bool readSteerScript(const std::string& text, std::vector<ScriptedSteer>& out, std::string& problem)
{
    Value doc;
    if (!json::parse(text, doc, problem))
        return false;
    if (!doc.isArray())
        return problem = "A steer script is a JSON array.", false;
    for (const auto& v : doc.items())
    {
        ScriptedSteer s;
        s.day = std::int64_t(v.number("day"));
        s.days = int(v.number("days", 7));
        s.steer.kind = v.string("kind");
        s.steer.target = v.string("target");
        s.steer.item = v.string("item");
        s.steer.strength = v.number("strength", 1);
        s.steer.note = v.string("note");
        s.steer.by = "script";
        out.push_back(s);
    }
    return true;
}

std::string briefText(const Brief& brief, bool full)
{
    return json::dump(briefJson(brief, full));
}

State readState(const Value& o)
{
    State st;
    if (!o.isObject())
        return st;
    st.nextSteer = std::max<std::int64_t>(1, std::int64_t(o.number("nextSteer", 1)));
    for (const auto& v : o.array("steers"))
    {
        auto s = readSteer(v);
        std::string problem;
        if (!s.id.empty() && validSteer(s, problem))
            st.steers.push_back(s);
    }
    const auto& m = o.object("memory");
    for (const auto& [k, v] : m.object("spent").fields())
        if (v.isNumber() && v.asNumber() >= 0 && v.asNumber() < 1e12)
            st.memory.spent[k] = v.asNumber();
    for (const auto& [k, v] : m.object("distress").fields())
        if (v.isNumber() && v.asNumber() >= 0 && v.asNumber() <= 1)
            st.memory.distress[k] = v.asNumber();
    for (const auto& [k, v] : m.object("price").fields())
        if (v.isNumber() && v.asNumber() > 0 && v.asNumber() < 1e9)
            st.memory.price[k] = v.asNumber();
    for (const auto& [k, v] : m.object("weekStart").fields())
        if (v.isNumber() && std::abs(v.asNumber()) < 1e12)
            st.memory.weekStart[k] = std::int64_t(v.asNumber());
    for (const auto& [k, v] : m.object("wage").fields())
        if (v.isNumber() && v.asNumber() > 0 && v.asNumber() < 1e6)
            st.memory.wage[k] = v.asNumber();
    for (const auto& [k, v] : m.object("net").fields())
        if (v.isNumber() && std::abs(v.asNumber()) < 1e12)
            st.memory.net[k] = v.asNumber();
    for (const auto& [k, v] : m.object("week").fields())
        if (v.isArray() && v.items().size() == 2 && v.items()[0].isNumber() && v.items()[1].isNumber())
            st.memory.week[k] = {v.items()[0].asNumber(), int(v.items()[1].asNumber())};
    for (const auto& [k, v] : m.object("weekKinds").fields())
        if (v.isNumber())
            st.memory.weekKinds[k] = v.asNumber();
    st.memory.margin = m.number("margin", -1);
    st.memory.day = std::int64_t(m.number("day", -1));
    st.memory.decided = std::int64_t(m.number("decided", -1));
    st.memory.residentShare = m.number("residentShare", -1);
    st.memory.autoPressure = std::clamp(m.number("autoPressure", 1), 1.0, 10.0);
    st.memory.bottomShare = m.number("bottomShare", -1);
    st.memory.floorLift = std::clamp(m.number("floorLift", 1), 1.0, 10.0);
    for (const auto& [k, v] : m.object("support").fields())
        if (v.isNumber() && v.asNumber() > 0 && v.asNumber() < 1e6)
            st.memory.support[k] = v.asNumber();
    st.last = readBrief(o.object("brief"));
    st.decision = readBrief(o.object("decision"));
    return st;
}

// --- The thread ----------------------------------------------------------------------------------------------------------
Runner::Runner(bool threaded)
{
    if (threaded)
        thread_ = std::thread([this] { work(); });
}
Runner::~Runner()
{
    {
        std::lock_guard<std::mutex> guard(lock_);
        stopping_ = true;
    }
    wake_.notify_all();
    if (thread_.joinable())
        thread_.join();
}
void Runner::submit(Snapshot snapshot, Memory memory)
{
    {
        std::unique_lock<std::mutex> guard(lock_);
        done_.wait(guard, [&] { return !busy_; });   // (One plan at a time: an earlier one is finished first.)
        pendingDay_ = snapshot.day;
        snapshot_ = std::move(snapshot);
        memory_ = std::move(memory);
        result_.reset();
        busy_ = true;
    }
    wake_.notify_all();
}
bool Runner::pending() const
{
    std::lock_guard<std::mutex> guard(lock_);
    return pendingDay_ >= 0;
}
std::int64_t Runner::pendingDay() const
{
    std::lock_guard<std::mutex> guard(lock_);
    return pendingDay_;
}
std::pair<Brief, Memory> Runner::take()
{
    std::unique_lock<std::mutex> guard(lock_);
    if (!thread_.joinable() && snapshot_)
    {
        // No thread: plan here, now.
        auto memory = std::move(memory_);
        auto brief = plan(*snapshot_, memory);
        snapshot_.reset();
        result_ = std::make_pair(std::move(brief), std::move(memory));
        busy_ = false;
    }
    done_.wait(guard, [&] { return !busy_; });
    auto out = result_ ? std::move(*result_) : std::make_pair(Brief{}, Memory{});
    result_.reset();
    pendingDay_ = -1;
    return out;
}
void Runner::work()
{
    for (;;)
    {
        std::optional<Snapshot> snapshot;
        Memory memory;
        {
            std::unique_lock<std::mutex> guard(lock_);
            wake_.wait(guard, [&] { return stopping_ || snapshot_.has_value(); });
            if (stopping_)
                return;
            snapshot = std::move(snapshot_);
            snapshot_.reset();
            memory = std::move(memory_);
        }
        auto brief = plan(*snapshot, memory);
        {
            std::lock_guard<std::mutex> guard(lock_);
            result_ = std::make_pair(std::move(brief), std::move(memory));
            busy_ = false;
        }
        done_.notify_all();
    }
}
} // namespace ratw::orchestra
