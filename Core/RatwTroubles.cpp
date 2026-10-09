// Residents' troubles (Docs/Design/57-changing-the-world.md, 3): see RatwTroubles.h.
#include "RatwTroubles.h"

#include "RatwBonds.h"
#include "RatwSociety.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

namespace ratw::troubles
{
using json::Value;

namespace
{
std::filesystem::path troublesFile()
{
    // Data/Town: RATW_DATA_DIR, else the working directory or one above it, else the source tree.
    namespace fs = std::filesystem;
    std::error_code ec;
    if (const char* dir = std::getenv("RATW_DATA_DIR"); dir && *dir)
        return fs::path(dir) / "Town" / "troubles.json";
    for (auto at = fs::current_path(ec); !ec && !at.empty(); at = at.parent_path())
    {
        if (fs::exists(at / "Data" / "Town" / "troubles.json", ec))
            return at / "Data" / "Town" / "troubles.json";
        if (at == at.parent_path())
            break;
    }
#ifdef RATW_SOURCE_DIR
    return fs::path(RATW_SOURCE_DIR) / "Data" / "Town" / "troubles.json";
#else
    return fs::path("Data") / "Town" / "troubles.json";
#endif
}

std::vector<std::string> strings(const Value& list)
{
    std::vector<std::string> out;
    for (const auto& s : list.items())
        if (s.isString() && !s.asString().empty())
            out.push_back(s.asString());
    return out;
}

bool aliveIn(const std::string& id, const Reads& r)
{
    return r.society->resident(id) && (!r.alive || r.alive(id));
}
int ageOf(const std::string& id, const Reads& r)
{
    if (r.age)
        return r.age(id);
    const auto* spec = r.society->spec(id);
    return spec ? spec->age : 30;
}
std::string townOf(const std::string& id, const Reads& r)
{
    const auto* life = r.society->resident(id);
    if (!life)
        return {};
    return r.communityOf ? r.communityOf(life->homeCell) : life->homeCell;
}
} // namespace

const Kind* Rules::kind(const std::string& id) const
{
    for (const auto& k : kinds)
        if (k.id == id)
            return &k;
    return nullptr;
}

Rules parse(const std::string& text)
{
    Rules r;
    Value doc;
    std::string error;
    if (text.empty() || !json::parse(text, doc, error) || !doc.isObject())
        return r;
    r.trust = doc.object("speak").number("trust", r.trust);
    r.familiarity = doc.object("speak").number("familiarity", r.familiarity);
    r.refusal = strings(doc["refusal"]);
    const auto& limits = doc.object("limits");
    r.restDays = limits.number("restDays", r.restDays);
    r.perWolfDays = limits.number("perWolfDays", r.perWolfDays);
    r.peaceTrust = limits.number("peaceTrust", r.peaceTrust);
    r.peaceTiles = limits.number("peaceTiles", r.peaceTiles);
    r.employerTrust = limits.number("employerTrust", r.employerTrust);
    r.masterTrust = limits.number("masterTrust", r.masterTrust);
    r.notableDebt = std::int64_t(limits.number("notableDebt", double(r.notableDebt)));
    const auto& kinds = doc.object("kinds");
    for (const auto& id : strings(doc["order"]))
    {
        const auto* k = kinds.find(id);
        if (!k || !k->isObject())
            continue;
        Kind kind;
        kind.id = id;
        kind.on = k->boolean("on", true);
        kind.briefing = k->string("briefing");
        kind.unfinished = k->string("unfinished");
        kind.said = strings((*k)["said"]);
        kind.underDays = k->number("underDays", kind.underDays);
        kind.refillDays = std::max(kind.underDays, k->number("refillDays", kind.refillDays));
        kind.youngest = int(k->number("youngest", kind.youngest));
        kind.oldest = int(k->number("oldest", kind.oldest));
        kind.fee = std::max<std::int64_t>(0, std::int64_t(k->number("fee", double(kind.fee))));
        kind.notableDays = k->number("notableDays", kind.notableDays);
        kind.both = k->number("both", kind.both);
        kind.one = k->number("one", kind.one);
        r.kinds.push_back(std::move(kind));
    }
    return r;
}

const Rules& rules()
{
    static const Rules r = [] {
        std::ifstream in(troublesFile());
        std::stringstream text;
        text << in.rdbuf();
        if (!in)
            std::cerr << "[warn] RATW_TROUBLES no Data/Town/troubles.json: residents have no troubles\n";
        return parse(text.str());
    }();
    return r;
}

double foodDays(const std::vector<std::string>& household, const std::string& homeCell, const Reads& r, std::int64_t* held,
                double* dayCost)
{
    const auto& s = *r.society;
    const auto town = r.communityOf ? r.communityOf(homeCell) : homeCell;
    double cost = double(Society::FoodADay);
    for (const auto& t : s.orchestrator().last.towns)
        if (t.id == town && t.foodCost > 0)
            cost = t.foodCost;
    // Their purses, savings and the food they carry, and the larder's food, at the town's price.
    double money = 0;
    for (const auto& id : household)
    {
        if (const auto* a = s.account(id))
        {
            money += double(a->cash);
            for (const auto& [item, n] : a->stock)
                if (n > 0 && Society::edible(item))
                    money += n * s.townPrice(town, item);
        }
        if (const auto saved = s.state().orchestrator.deposits.find(id); saved != s.state().orchestrator.deposits.end())
            money += double(saved->second.second);
    }
    if (const auto* larder = s.account(Society::homeStore(homeCell, "larder")))
        for (const auto& [item, n] : larder->stock)
            if (n > 0 && Society::edible(item))
                money += n * s.townPrice(town, item);
    if (held)
        *held = std::int64_t(money);
    if (dayCost)
        *dayCost = cost * double(std::max<std::size_t>(1, household.size()));
    return money / (cost * double(std::max<std::size_t>(1, household.size())));
}

Trouble kindOf(const std::string& kindId, const std::string& resident, const Reads& r, const Rules& rules)
{
    Trouble t;
    const auto* k = rules.kind(kindId);
    if (!k || !k->on || !r.society || !aliveIn(resident, r) || (r.resting && r.resting(resident, kindId)))
        return t;
    const auto& s = *r.society;
    const auto give = [&](Trouble found) {
        found.kind = kindId;
        found.resident = resident;
        return found;
    };
    if (kindId == "debt")
    {
        // The keeper of a business its town's rescue fund has lent to (doc 46, Phase 6).
        if (!s.jobOf(resident))
            return t;
        const auto till = s.tillOf(resident);
        const auto loan = s.state().memory.loans.find(till);
        if (till == resident || loan == s.state().memory.loans.end() || loan->second.first <= 0)
            return t;
        Trouble found;
        found.other = till;
        found.coins = loan->second.first;
        return give(found);
    }
    if (kindId == "short")
    {
        // The head of a household (its eldest grown wolf) whose purses, savings and larder hold under underDays of plain
        // food for everyone at home, at the town's price. Not a barracks or a bunkhouse.
        const auto* life = s.resident(resident);
        if (life->homeCell.empty() || Society::communalHome(life->homeCell))
            return t;
        std::vector<std::string> home;
        for (const auto& [id, other] : s.state().residents)
            if (other.homeCell == life->homeCell && aliveIn(id, r))
                home.push_back(id);
        std::string head;
        for (const auto& id : home)
            if (ageOf(id, r) >= 16 && (head.empty() || ageOf(id, r) > ageOf(head, r)))
                head = id;
        if (head != resident)
            return t;
        double cost = 0;
        const double days = foodDays(home, life->homeCell, r, nullptr, &cost);
        if (days >= k->underDays)
            return t;
        Trouble found;
        found.days = days;
        found.coins = std::max<std::int64_t>(1, std::int64_t(std::ceil((k->refillDays - days) * cost)));
        std::stable_partition(home.begin(), home.end(), [&](const std::string& id) { return id == head; });
        found.household = std::move(home);
        return give(found);
    }
    if (kindId == "child")
    {
        // A parent of a youth of the town's age for it with neither a post nor a master.
        Trouble found;
        for (const auto& [child, parents] : s.state().careers.parents)
        {
            if (std::find(parents.begin(), parents.end(), resident) == parents.end() || !aliveIn(child, r))
                continue;
            const int age = ageOf(child, r);
            if (age < k->youngest || age > k->oldest || s.jobOf(child) || s.apprenticedTo(child))
                continue;
            if (found.other.empty() || age > found.age)
                found.other = child, found.age = age;
        }
        return found.other.empty() ? t : give(found);
    }
    if (kindId == "work")
    {
        // A grown wolf with no post and no master, who isn't the one keeping house at home.
        const int age = ageOf(resident, r);
        if (age < k->youngest || age > k->oldest || s.jobOf(resident) || s.apprenticedTo(resident))
            return t;
        for (const auto& [home, keeper] : s.state().memory.keeper)
            if (keeper == resident)
                return t;
        Trouble found;
        found.days = r.idleDays ? r.idleDays(resident) : 0;
        return give(found);
    }
    if (kindId == "feud")
    {
        // Bad blood with another resident of the same town: both ways at `both` or worse, or one way at `one`.
        if (!r.bonds)
            return t;
        const auto* mine = r.bonds->of(resident);
        if (!mine)
            return t;
        const auto town = townOf(resident, r);
        Trouble found;
        double worst = 0;
        for (const auto& [other, bond] : *mine)
        {
            if (bond.affinity > k->both || !aliveIn(other, r) || townOf(other, r) != town)
                continue;
            const auto* theirs = r.bonds->find(other, resident);
            const bool both = theirs && theirs->affinity <= k->both;
            if (!both && bond.affinity > k->one)
                continue;
            if (bond.affinity < worst)
                worst = bond.affinity, found.other = other;
        }
        return found.other.empty() ? t : give(found);
    }
    return t;
}

Trouble troubleOf(const std::string& resident, const Reads& r, const Rules& rules)
{
    for (const auto& k : rules.kinds)
        if (auto t = kindOf(k.id, resident, r, rules))
            return t;
    return {};
}

std::string fill(std::string line, const std::map<std::string, std::string>& blanks)
{
    for (const auto& [key, value] : blanks)
    {
        const auto blank = "{" + key + "}";
        for (auto at = line.find(blank); at != std::string::npos; at = line.find(blank, at + value.size()))
            line.replace(at, blank.size(), value);
    }
    return line;
}
} // namespace ratw::troubles
