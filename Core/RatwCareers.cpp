// Careers: positions, skill, apprentices, estates and succession (Docs/Design/26-living-npcs.md, Phase 4).
#include "RatwSociety.h"
#include "RatwItems.h"

#include <algorithm>
#include <cmath>

namespace ratw
{
namespace
{
std::string surname(const std::string& name)
{
    const auto space = name.find_last_of(' ');
    return space == std::string::npos ? std::string() : name.substr(space + 1);
}
std::string skillKey(const std::string& resident, const std::string& position)
{
    return resident + "|" + position;
}
// Placeholder names for those born here and those who come from elsewhere, until naming is designed.
const char* const FirstNames[] = {"Alder", "Briar", "Cinder", "Dusk", "Ember", "Fennel", "Gale", "Hazel", "Ivy", "Juniper",
                                  "Kestrel", "Linden", "Moss", "Nettle", "Oriel", "Pike", "Quill", "Rowan", "Sorrel",
                                  "Thorn", "Umber", "Vesper", "Willow", "Yarrow", "Wren", "Tansy", "Bramble", "Sedge"};
const char* const FarSurnames[] = {"Farwander", "Roadsend", "Greymantle", "Longpath", "Stillwater", "Ashford", "Marrow",
                                   "Coldbrook", "Windrow", "Thistledown"};
std::uint64_t mix(const std::string& a, std::int64_t b)
{
    return std::hash<std::string>{}(a) * 1099511628211ULL + std::uint64_t(b) * 2654435761ULL;
}
std::string lower(std::string s)
{
    for (auto& c : s)
        c = char(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
// How much a position is a step up: keeping a shop, then the watch, then paid work, then unpaid.
int rank(const Position* p)
{
    return !p ? -1 : p->role == "merchant" ? 3 : p->role == "guard" ? 2 : p->paid ? 1 : 0;
}
// Masters take on an apprentice now and then, not the first day they could: about once a week.
bool thisWeek(const std::string& id, std::int64_t day)
{
    return (std::hash<std::string>{}(id) + std::uint64_t(day)) % 7 == 0;
}
} // namespace

bool facilityAccount(const std::string& id)
{
    for (const char* prefix : {"stores:", "caravan:", "bandits:", "contract:", "ground:", "chapter:", "home:", "town:", "house:", "till:"})   // (chapter: doc 32's treasuries; home: doc 36; town: doc 35's Town Works, watch, church)
        if (id.rfind(prefix, 0) == 0)
            return id.size() <= 80 && id.size() > std::string(prefix).size();
    return false;
}

bool Society::bequeath(const std::string& from, const std::string& to, const std::string& item, int quantity,
                       std::int64_t coins)
{
    return shift(from, to, item, quantity, coins, "inheritance");
}

bool Society::openAccount(const std::string& id)
{
    if (!facilityAccount(id) || state_.accounts.count(id) || state_.accounts.size() >= MaxAccounts)
        return false;
    state_.accounts[id] = {};
    return true;
}

bool Society::closeAccount(const std::string& id)
{
    const auto found = state_.accounts.find(id);
    if (!facilityAccount(id) || found == state_.accounts.end() || found->second.cash != 0)
        return false;
    for (const auto& [item, count] : found->second.stock)
        if (count != 0)
            return false;
    state_.accounts.erase(found);
    return true;
}

double Society::priceFactor(const std::string& merchant, const std::string& item) const
{
    const auto* job = jobOf(merchant);
    const auto store = priceFactors_.find(storeFor(job ? job->work.cell : std::string()));
    if (store == priceFactors_.end())
        return 1;
    auto found = store->second.find(item);
    if (found == store->second.end())
        found = store->second.find(items::baseOf(item));   // (A fine hide is dear where hides are scarce.)
    return found == store->second.end() || !std::isfinite(found->second) ? 1 : std::clamp(found->second, .5, 2.0);
}

const std::string& Society::storeFor(const std::string& cell) const
{
    static const std::string treasury = "treasury";
    const auto found = storeForCell_.find(cell);
    return found == storeForCell_.end() || !state_.accounts.count(found->second) ? treasury : found->second;
}

int Society::consume(const std::string& account, const std::string& item, int quantity, const std::string& kind)
{
    const auto found = state_.accounts.find(account);
    if (found == state_.accounts.end() || !itemValid(item) || quantity <= 0)
        return 0;
    const int used = std::min(quantity, stock(found->second, item));
    if (used <= 0)
        return 0;
    found->second.stock[item] -= used;
    record(kind, account, "consumed", item, std::min(used, 99), 0);
    return used;
}

bool Society::shift(const std::string& from, const std::string& to, const std::string& item, int quantity,
                    std::int64_t coins, const std::string& kind)
{
    // Existing money and goods only; what the receiver can't hold stays where it was.
    const auto a = state_.accounts.find(from), b = state_.accounts.find(to);
    if (a == state_.accounts.end() || b == state_.accounts.end() || from == to || coins < 0 || quantity < 0 ||
        a->second.cash < coins || (!item.empty() && stock(a->second, item) < quantity))
        return false;
    coins = std::min(coins, MoneyCap - b->second.cash);
    if (!item.empty())
        quantity = std::min(quantity, StockCap - stock(b->second, item));
    if (coins <= 0 && quantity <= 0)
        return false;
    a->second.cash -= std::max<std::int64_t>(0, coins);
    b->second.cash += std::max<std::int64_t>(0, coins);
    if (!item.empty() && quantity > 0)
    {
        a->second.stock[item] -= quantity;
        b->second.stock[item] += quantity;
    }
    record(kind, from, to, item, std::min(quantity, 99), std::max<std::int64_t>(0, coins));
    return true;
}

const char* skillFamily(const std::string& title)
{
    // A first sorting of the world's job titles by keyword; a proper list of skills will replace it.
    static const std::vector<std::pair<const char*, std::vector<const char*>>> Families{
        {"watch", {"watch", "guard", "patrol", "gate", "wall", "captain", "sentry"}},
        {"trade", {"keeping", "shop", "stall", "inn", "tavern", "market", "trader", "selling", "pawn", "ledger", "merchant"}},
        {"craft", {"smith", "forge", "glass", "weav", "sew", "tann", "carv", "joiner", "shipwright", "rope", "cooper",
                   "potter", "brew", "bak", "cook", "kiln", "mason"}},
        {"labour", {"saw", "quarry", "log", "haul", "unload", "dig", "rak", "burn", "fish", "farm", "herd", "gather",
                    "forag", "barge", "dock", "carry", "labour"}},
        {"service", {"serv", "wash", "clean", "steward", "house", "tend", "nurse", "heal", "scribe", "clerk", "tall"}},
        {"travel", {"road", "travel", "carter", "courier", "rider", "caravan", "scout"}}};
    const auto t = lower(title);
    for (const auto& [family, words] : Families)
        for (const char* word : words)
            if (t.find(word) != std::string::npos)
                return family;
    return "general";
}

double workPace(double skill)
{
    return std::clamp(1.25 - (std::isfinite(skill) ? skill : 0) / 200, .75, 1.25);
}

double Society::familySkill(const std::string& resident, const std::string& family) const
{
    double best = 0;
    const auto prefix = resident + "|";
    for (auto it = state_.careers.skill.lower_bound(prefix); it != state_.careers.skill.end() && it->first.rfind(prefix, 0) == 0; ++it)
        if (const auto* p = position(it->first.substr(prefix.size())); p && family == skillFamily(p->title))
            best = std::max(best, it->second);
    return best;
}

bool Society::marry(const std::string& a, const std::string& b)
{
    auto& spouses = state_.careers.spouses;
    if (a == b || !state_.residents.count(a) || !state_.residents.count(b) || spouses.count(a) || spouses.count(b))
        return false;
    spouses[a] = b;
    spouses[b] = a;
    return true;
}
const std::string* Society::spouse(const std::string& resident) const
{
    const auto found = state_.careers.spouses.find(resident);
    return found == state_.careers.spouses.end() ? nullptr : &found->second;
}

std::vector<ResidentRequest> Society::takeRequests()
{
    std::vector<ResidentRequest> out;
    out.swap(requests_);
    return out;
}

CareerNote Society::welcome(const ResidentRequest& request, const std::string& id)
{
    auto& c = state_.careers;
    if (!state_.residents.count(id))
        return {"refused", id, {}, "No such resident to welcome."};
    if (request.kind == "birth")
    {
        c.parents[id] = request.parents;
        // A child's first pennies (starting money, 2026-10-05), from the better-off parent: given, never made.
        const std::string* giver = nullptr;
        for (const auto& parent : request.parents)
            if (const auto* purse = account(parent); purse && (!giver || purse->cash > account(*giver)->cash))
                giver = &parent;
        if (giver)
            shift(*giver, id, "", 0, std::min<std::int64_t>(ChildPurse, account(*giver)->cash), "a child's first pennies");
        forgetCareers();
        return {"birth", id, request.parents.empty() ? std::string() : request.parents[0],
                request.parents.size() > 1 ? "child of " + request.parents[0] + " and " + request.parents[1] : std::string()};
    }
    auto found = c.positions.find(request.positionId);
    const auto* p = position(request.positionId);
    if (found != c.positions.end())
        found->second.newcomerAsked = false;
    if (!p || found == c.positions.end() || !found->second.holder.empty())
        return {"newcomer", id, {}, "came to town, but the post was taken; looking for work"};
    found->second.holder = id;
    found->second.vacantSince = -1;
    auto& known = c.skill[skillKey(id, p->id)];
    known = std::max(known, 35.0);                     // A stranger who has done this work elsewhere.
    forgetCareers();
    return {"newcomer", id, found->second.lastHolder, "came from outside to take up " + p->title};
}

void Society::buildPositions()
{
    employers_.clear();
    houses_.clear();
    housesKnown_ = false;
    positions_.clear();
    positionIndex_.clear();
    for (const auto& r : authored_.residents)
    {
        if (r.workLabel == "-")
            continue;                                  // Born here or come for another's post: no job of their own.
        Position p;
        p.id = "job:" + r.id;
        p.founder = r.id;
        p.title = r.workLabel;
        p.role = r.role;
        p.work = r.work;
        p.serve = r.serve;
        p.startHour = r.startHour;
        p.endHour = r.endHour;
        p.route = r.route;
        p.paid = r.paid;
        positionIndex_[p.id] = positions_.size();
        positions_.push_back(std::move(p));
    }
    forgetCareers();
}

void Society::defaultCareers()
{
    // Every founder holds the job they were written with, as skilled as their years suggest.
    state_.careers = {};
    for (const auto& p : positions_)
    {
        state_.careers.positions[p.id].holder = p.founder;
        const auto* r = spec(p.founder);
        state_.careers.skill[skillKey(p.founder, p.id)] = std::clamp(30.0 + (r ? r->age : 20), 40.0, 90.0);
    }
    forgetCareers();
}

void Society::reconcileCareers()
{
    // A saved career for a position or resident the world no longer has is let go; a new position is its founder's.
    auto& c = state_.careers;
    for (auto it = c.positions.begin(); it != c.positions.end();)
        it = positionIndex_.count(it->first) ? std::next(it) : c.positions.erase(it);
    const auto known = [&](const std::string& id) { return id.empty() || state_.residents.count(id) || playerAccountId(id); };
    for (auto& [id, ps] : c.positions)
    {
        if (!known(ps.holder))
            ps.holder.clear();
        if (!known(ps.apprentice))
            ps.apprentice.clear();
        if (!known(ps.lastHolder))
            ps.lastHolder.clear();
        if (ps.holder.empty() && ps.vacantSince < 0)
            ps.vacantSince = 0;
    }
    for (const auto& p : positions_)
        if (!c.positions.count(p.id))
        {
            c.positions[p.id].holder = p.founder;
            const auto* r = spec(p.founder);
            c.skill.emplace(skillKey(p.founder, p.id), std::clamp(30.0 + (r ? r->age : 20), 40.0, 90.0));
        }
    for (auto it = c.skill.begin(); it != c.skill.end();)
        it = std::isfinite(it->second) && it->second >= 0 && it->second <= 100 ? std::next(it) : c.skill.erase(it);
    for (auto it = c.mourning.begin(); it != c.mourning.end();)
        it = state_.residents.count(it->first) && std::isfinite(it->second.until) ? std::next(it) : c.mourning.erase(it);
    for (auto it = c.estates.begin(); it != c.estates.end();)
        it = state_.accounts.count(it->first) && std::isfinite(it->second) ? std::next(it) : c.estates.erase(it);
    forgetCareers();
}

void Society::indexCareers() const
{
    if (careersIndexed_)
        return;
    heldBy_.clear();
    learning_.clear();
    for (const auto& [id, ps] : state_.careers.positions)
    {
        if (!ps.holder.empty())
            heldBy_[ps.holder] = id;
        if (!ps.apprentice.empty())
            learning_[ps.apprentice] = id;
    }
    careersIndexed_ = true;
}

const std::vector<Position>& Society::positions() const
{
    return positions_;
}
const Position* Society::position(const std::string& id) const
{
    const auto found = positionIndex_.find(id);
    return found == positionIndex_.end() ? nullptr : &positions_[found->second];
}
const Position* Society::jobOf(const std::string& resident) const
{
    indexCareers();
    const auto found = heldBy_.find(resident);
    return found == heldBy_.end() ? nullptr : position(found->second);
}
const Position* Society::apprenticedTo(const std::string& resident) const
{
    indexCareers();
    const auto found = learning_.find(resident);
    return found == learning_.end() ? nullptr : position(found->second);
}
double Society::skill(const std::string& resident, const std::string& positionId) const
{
    const auto found = state_.careers.skill.find(skillKey(resident, positionId));
    return found == state_.careers.skill.end() ? 0 : found->second;
}
void Society::practise(const std::string& resident, const std::string& positionId, double rate)
{
    auto& s = state_.careers.skill[skillKey(resident, positionId)];
    s = std::min(100.0, s + rate * (1 - s / 100));
}
bool Society::household(const std::string& a, const std::string& b) const
{
    const auto* x = resident(a);
    const auto* y = resident(b);
    return a != b && x && y && !x->homeCell.empty() && x->homeCell == y->homeCell;
}
bool Society::family(const std::string& a, const std::string& b) const
{
    if (a == b)
        return false;
    const auto& c = state_.careers;
    if (const auto s = c.spouses.find(a); s != c.spouses.end() && s->second == b)
        return true;
    const auto childOf = [&](const std::string& child, const std::string& parent) {
        const auto p = c.parents.find(child);
        return p != c.parents.end() && std::find(p->second.begin(), p->second.end(), parent) != p->second.end();
    };
    if (childOf(a, b) || childOf(b, a))
        return true;
    const auto* x = spec(a);
    const auto* y = spec(b);
    return x && y && household(a, b) && !surname(x->name).empty() && surname(x->name) == surname(y->name);
}
const Mourning* Society::mourning(const std::string& resident) const
{
    const auto found = state_.careers.mourning.find(resident);
    return found == state_.careers.mourning.end() ? nullptr : &found->second;
}
void Society::mourn(const std::string& resident, const std::string& whom, double until)
{
    if (!state_.residents.count(resident) || resident == whom || !std::isfinite(until))
        return;
    auto& m = state_.careers.mourning[resident];
    if (m.until < until)
        m = {until, whom};
}

std::vector<CareerNote> Society::died(const std::string& resident, double day)
{
    std::vector<CareerNote> notes;
    auto& c = state_.careers;
    for (auto& [id, ps] : c.positions)
    {
        if (ps.holder == resident)
        {
            ps.holder.clear();
            ps.lastHolder = resident;
            ps.vacantSince = day;
            notes.push_back({"vacancy", resident, {}, position(id) ? position(id)->title : id});
        }
        if (ps.apprentice == resident)
            ps.apprentice.clear();
    }
    if (state_.accounts.count(resident) && !playerAccountId(resident))
        c.estates[resident] = day;
    forgetCareers();
    return notes;
}

std::vector<CareerNote> Society::revived(const std::string& resident)
{
    std::vector<CareerNote> notes;
    auto& c = state_.careers;
    c.estates.erase(resident);
    if (!jobOf(resident))
        for (auto& [id, ps] : c.positions)
            if (ps.holder.empty() && ps.lastHolder == resident)
            {
                ps.holder = resident;
                ps.vacantSince = -1;
                notes.push_back({"returned to work", resident, {}, position(id) ? position(id)->title : id});
                break;
            }
    forgetCareers();
    return notes;
}

std::vector<CareerNote> Society::tendCareers(double now, const CareerWorld& world)
{
    std::vector<CareerNote> notes;
    auto& c = state_.careers;
    const auto today = std::int64_t(std::floor(now));
    if (today == c.day || roster_ != Roster::Authored)
        return notes;
    c.day = today;
    const auto alive = [&](const std::string& id) { return !id.empty() && world.alive(id); };

    // Mourning passes.
    for (auto it = c.mourning.begin(); it != c.mourning.end();)
        it = it->second.until <= now ? c.mourning.erase(it) : std::next(it);

    // A day after a death the estate is settled: shared among the family, or to the town if there is none.
    for (auto it = c.estates.begin(); it != c.estates.end();)
    {
        const auto dead = it->first;
        if (alive(dead))
        {
            it = c.estates.erase(it);
            continue;
        }
        if (now - it->second < 1)
        {
            ++it;
            continue;
        }
        std::vector<std::string> heirs;
        for (const auto& [id, life] : state_.residents)
            if (family(dead, id) && alive(id))
                heirs.push_back(id);
        auto& estate = state_.accounts.at(dead);
        if (heirs.empty())
            heirs.push_back("treasury");
        // Shares of the estate as it stood, not of what is left after each heir.
        const auto shares = std::int64_t(heirs.size());
        const std::int64_t cash = estate.cash;
        for (std::size_t h = 0; h < heirs.size(); ++h)
        {
            const std::int64_t coins = cash / shares + (h == 0 ? cash % shares : 0);
            if (coins > 0)
                bequeath(dead, heirs[h], "", 0, coins);
        }
        for (const std::string item : {"herbs", "meal"})
        {
            const int total = stock(estate, item);      // Likewise taken once, before sharing.
            for (std::size_t h = 0; h < heirs.size() && total > 0; ++h)
            {
                const int quantity = total / int(heirs.size()) + (h == 0 ? total % int(heirs.size()) : 0);
                if (quantity > 0)
                    bequeath(dead, heirs[h], item, quantity, 0);
            }
        }
        notes.push_back({"estate settled", dead, heirs.size() == 1 ? heirs[0] : std::string(),
                         heirs[0] == "treasury" ? "no family: to the town" : "shared among " + std::to_string(heirs.size()) + " of the family"});
        it = c.estates.erase(it);
    }

    // Empty positions are filled, not all at once: an apprentice after two days, family after three, anyone local
    // who is out of work after five. Whoever moves up leaves their own job empty in turn.
    for (auto& p : positions_)
    {
        auto& ps = c.positions[p.id];
        if (!ps.holder.empty() || ps.vacantSince < 0)
            continue;
        const double waited = now - ps.vacantSince;
        std::string chosen, how;
        if (waited >= 2 && alive(ps.apprentice) && skill(ps.apprentice, p.id) >= 30)
            chosen = ps.apprentice, how = "the apprentice steps up";
        if (chosen.empty() && waited >= 3 && !ps.lastHolder.empty())
        {
            double best = -1;
            // Family carry on a family's trade when it is a step up for them (or they have no work): the son takes
            // over the stall, but the stallkeeper doesn't leave it to fetch errands.
            for (const auto& [id, life] : state_.residents)
                if (family(ps.lastHolder, id) && alive(id) && world.age(id) >= 16 && rank(jobOf(id)) < rank(&p))
                {
                    const double score = skill(id, p.id) * 2 + world.regard(id, ps.lastHolder) + world.age(id) / 10.0;
                    if (score > best)
                        best = score, chosen = id;
                }
            if (!chosen.empty())
                how = "the family carries it on";
        }
        if (chosen.empty() && waited >= 5)
        {
            double best = -1;
            for (const auto& [id, life] : state_.residents)
                if (!jobOf(id) && !apprenticedTo(id) && alive(id) && world.age(id) >= 16 &&
                    (!world.near || world.near(p.work.cell, life.homeCell)))
                {
                    const double score = skill(id, p.id) * 2 + (ps.lastHolder.empty() ? 0 : world.regard(ps.lastHolder, id));
                    if (score > best)
                        best = score, chosen = id;
                }
            if (!chosen.empty())
                how = "someone local takes it on";
        }
        if (chosen.empty())
            continue;
        for (auto& [otherId, other] : c.positions)
        {
            if (other.holder == chosen)
            {
                other.holder.clear();
                other.lastHolder = chosen;
                other.vacantSince = now;
                notes.push_back({"vacancy", chosen, {}, position(otherId) ? position(otherId)->title : otherId});
            }
            if (other.apprentice == chosen)
                other.apprentice.clear();
        }
        ps.holder = chosen;
        ps.vacantSince = -1;
        auto& known = c.skill[skillKey(chosen, p.id)];
        known = std::max({known, 10.0, .5 * familySkill(chosen, skillFamily(p.title))});   // Some of it carries over.
        notes.push_back({"succession", chosen, ps.lastHolder, p.title + ": " + how});
        forgetCareers();
    }

    // Apprenticeships: finished at skill 70; taken on by an older or accomplished master, now and then.
    for (auto& p : positions_)
    {
        auto& ps = c.positions[p.id];
        if (!ps.apprentice.empty() && (!alive(ps.apprentice) || ps.apprentice == ps.holder))
            ps.apprentice.clear();
        if (!ps.apprentice.empty() && skill(ps.apprentice, p.id) >= 70)
        {
            notes.push_back({"apprenticeship completed", ps.apprentice, ps.holder, p.title});
            ps.apprentice.clear();
        }
        if (!ps.apprentice.empty() || !alive(ps.holder) || p.role == "guard" || !thisWeek(p.id, today) ||
            (world.age(ps.holder) < 35 && skill(ps.holder, p.id) < 70))
            continue;
        std::string chosen;
        int bestTier = -1, bestAge = 1000;
        indexCareers();
        for (const auto& [id, life] : state_.residents)
        {
            const int age = world.age(id);
            if (id == ps.holder || !alive(id) || age < 12 || age > 25 || apprenticedTo(id))
                continue;
            const auto* own = jobOf(id);
            const bool alongside = own && own->work.cell == p.work.cell;
            if (own && !alongside)
                continue;                             // Busy with work elsewhere.
            const int tier = family(ps.holder, id) ? 3 : household(ps.holder, id) ? 2
                             : alongside && world.regard(ps.holder, id) >= 30 ? 1
                             : !own && (!world.near || world.near(p.work.cell, life.homeCell)) ? 0 : -1;
            if (tier > bestTier || (tier == bestTier && tier >= 0 && age < bestAge))
                bestTier = tier, bestAge = age, chosen = id;
        }
        if (bestTier < 0)
            continue;
        ps.apprentice = chosen;
        notes.push_back({"apprenticeship", chosen, ps.holder, p.title});
        forgetCareers();
    }

    // A post nobody here has taken in eight days: a stranger is sent for, with the last holder's kind of manner.
    for (auto& p : positions_)
    {
        auto& ps = c.positions[p.id];
        if (!ps.holder.empty() || ps.vacantSince < 0 || ps.newcomerAsked || now - ps.vacantSince < 8)
            continue;
        std::string model = spec(ps.lastHolder) ? ps.lastHolder : p.founder;
        if (!spec(model))
            continue;
        const auto* was = resident(model);
        ResidentRequest r;
        r.kind = "newcomer";
        r.templateId = model;
        const auto pick = mix(p.id, today);
        r.name = std::string(FirstNames[pick % std::size(FirstNames)]) + " " + FarSurnames[(pick / 31) % std::size(FarSurnames)];
        r.age = 22 + int(pick % 20);
        // They lodge where the last holder lived if nobody of that house is left; otherwise near the work.
        bool houseEmpty = true;
        for (const auto& [id, life] : state_.residents)
            if (id != ps.lastHolder && household(id, ps.lastHolder) && alive(id))
                houseEmpty = false;
        r.home = houseEmpty && was ? Spot{was->homeCell, was->homeX, was->homeY} : p.work;
        r.positionId = p.id;
        ps.newcomerAsked = true;
        requests_.push_back(r);
        notes.push_back({"newcomer sent for", {}, ps.lastHolder, p.title});
    }

    // Children, rarely: a married couple at home together, the mother of an age to bear, a year or so apart, no more
    // than three. About one a season for those who can.
    for (const auto& [a, b] : c.spouses)
    {
        if (!(a < b) || !alive(a) || !alive(b) || !household(a, b))
            continue;
        const auto* x = spec(a);
        const auto* y = spec(b);
        const auto* mother = x && x->appearance.sex == "female" && world.age(a) >= 18 && world.age(a) <= 45 ? x
                             : y && y->appearance.sex == "female" && world.age(b) >= 18 && world.age(b) <= 45 ? y : nullptr;
        if (!mother)
            continue;
        const auto key = a + "|" + b;
        const int children = c.births.count(key) ? c.births.at(key) : 0;
        const auto last = c.lastBirth.find(key);
        if (children >= 3 || (last != c.lastBirth.end() && now - last->second < 300) || mix(key, today) % 90 != 0)
            continue;
        const auto* home = resident(mother->id);
        ResidentRequest r;
        r.kind = "birth";
        r.templateId = mother->id;
        const auto family = surname(mother->name);
        r.name = std::string(FirstNames[mix(key, today + 7) % std::size(FirstNames)]) + (family.empty() ? "" : " " + family);
        r.age = 0;
        r.home = home ? Spot{home->homeCell, home->homeX, home->homeY} : mother->home;
        r.parents = {a, b};
        c.lastBirth[key] = now;
        ++c.births[key];
        requests_.push_back(r);
    }
    return notes;
}

CareerNote Society::apprentice(const std::string& player, const std::string& positionId, const CareerWorld& world, double)
{
    const auto* p = position(positionId);
    auto found = state_.careers.positions.find(positionId);
    if (!p || found == state_.careers.positions.end() || !playerAccountId(player))
        return {"refused", player, {}, "There is no such trade to learn."};
    auto& ps = found->second;
    if (ps.holder.empty() || !world.alive(ps.holder))
        return {"refused", player, {}, "Nobody holds that position to teach it."};
    if (!ps.apprentice.empty())
        return {"refused", player, ps.holder, "They already have an apprentice."};
    if (apprenticedTo(player))
        return {"refused", player, ps.holder, "You are already apprenticed."};
    // A master takes on someone they know and trust (Bonds: regard of 30 or more).
    if (world.regard(ps.holder, player) < 30)
        return {"refused", player, ps.holder, "They don't know you well enough to take you on."};
    ps.apprentice = player;
    forgetCareers();
    return {"apprenticeship", player, ps.holder, p->title};
}
} // namespace ratw
