// Crime and law (RatwCrime.h; Docs/Design/26-living-npcs.md, Phase 7). World members, kept here.
#include "RatwCrime.h"
#include "RatwWorld.h"
#include "RatwItems.h"

#include <algorithm>
#include <cmath>
#include <regex>

namespace ratw
{
namespace
{
// Placeholder numbers, to be tuned with play.
constexpr double StealReach = 1.6, StealEvery = 4;          // Tiles; seconds between attempts.
constexpr double Reach = 1.6, Swing = 1.2;                   // As for bandits (RatwRoads.cpp).
constexpr double ReportReach = 6;                            // A witness tells a guard on duty this close.
constexpr double Evidence = 1.0;                             // What the Watch needs to want someone.
constexpr double ColdDays = 7;                               // An unsolved incident is closed after this long.
constexpr double ConfrontSeconds = 30, WalkAway = 8;         // To pay a guard; how far is walking off.
constexpr double PlayerHours = 2, ResidentHours = 4;         // In the gaol (game hours).
constexpr double DownAt = 100, UpBelow = 50, HealPerHour = 50;
constexpr std::size_t IncidentsKept = 400;
constexpr double WitnessDays = 7;                    // A witness tells the watch within a week, or not at all.

std::uint64_t roll(const std::string& a, std::int64_t b)
{
    return std::hash<std::string>{}(a) * 1099511628211ULL + std::uint64_t(b) * 2654435761ULL;
}
double chance(const std::string& a, std::int64_t b)
{
    return double(roll(a, b) % 10000) / 10000.0;
}
double between(Vec2 a, Vec2 b)
{
    return std::hypot(a.x - b.x, a.y - b.y);
}
std::string pennies(std::int64_t n)
{
    return std::to_string(n) + (n == 1 ? " penny" : " pennies");
}
// What a thing is worth to the Watch, for fines and for restitution in coin when the goods are gone (doc 79, 7.8 and
// 10.1): its quoted price where the one robbed buys (its household's market, or its shop's), the Board's pressure in it.
std::int64_t worth(const Society& s, const std::string& victim, const std::string& item, int quantity)
{
    if (item.empty() || quantity <= 0)
        return 0;
    std::string market;
    if (const auto* st = s.statusOf(victim))
        market = st->market;
    else if (const auto* job = s.jobOf(victim); job && job->role == "merchant")
        market = s.marketOf(s.tillOf(victim));
    return s.marketLot(market, item, quantity);
}
std::string charge(const std::string& kind)
{
    return kind == "assault" ? "assault" : kind == "attempted theft" ? "attempted theft" : "theft";
}
} // namespace

const Warrant* World::warrantFor(const std::string& person) const
{
    for (const auto& w : crime_.warrants)
        if (w.person == person)
            return &w;
    return nullptr;
}

const Custody* World::custodyOf(const std::string& person) const
{
    for (const auto& c : crime_.custody)
        if (c.person == person)
            return &c;
    return nullptr;
}

bool World::guardOnDuty(const std::string& id) const
{
    const auto* e = entity(id);
    const auto* life = society_.resident(id);
    const auto* job = society_.jobOf(id);
    return e && !e->dead && e->hurt < UpBelow && life && job && job->role == "guard" &&
           (life->task == "patrol" || life->task == "watch") && !custodyOf(id);
}

std::string World::lawTown(const std::string& cellId) const
{
    // A world of one town may have no Town records (Greyfen): its region is the town, and its guards the Watch.
    if (const auto* t = townOf(cellId))
        return t->id;
    if (!towns_.empty())
        return {};
    const auto* c = cell(cellId);
    return c ? c->region : std::string();
}

std::vector<std::string> World::guardsOf(const std::string& townId) const
{
    if (townId.empty())
        return {};
    std::vector<std::string> out;
    for (const auto& p : society_.positions())
    {
        if (p.role != "guard")
            continue;
        const auto& holder = society_.state().careers.positions.at(p.id).holder;
        if (holder.empty())
            continue;
        if (lawTown(p.work.cell) == townId)
            out.push_back(holder);
    }
    return out;
}

std::int64_t World::owedBy(const Warrant& w) const
{
    std::int64_t total = w.fine;
    for (const auto& r : w.restitution)
        total += r.coins + worth(society_, r.to, r.item, r.quantity);
    return total;
}

Incident& World::openIncident(const std::string& kind, const std::string& offender, const std::string& victim)
{
    const auto* e = entity(offender);
    Incident inc;
    inc.id = "inc-" + std::to_string(crime_.nextIncident++);
    inc.kind = kind;
    inc.offender = offender;
    inc.victim = victim;
    inc.cell = e ? e->cellId : "";
    inc.town = lawTown(inc.cell);
    inc.time = time_;
    inc.day = calendarDays_;
    crime_.incidents.push_back(std::move(inc));
    if (crime_.incidents.size() > IncidentsKept)
    {
        // The oldest closed ones go first; an open one is kept while there is room.
        const auto closed = std::find_if(crime_.incidents.begin(), crime_.incidents.end(),
                                         [](const Incident& i) { return i.status != "open"; });
        crime_.incidents.erase(closed != crime_.incidents.end() ? closed : crime_.incidents.begin());
    }
    return crime_.incidents.back();
}

Incident* World::incident(const std::string& id)
{
    for (auto& i : crime_.incidents)
        if (i.id == id)
            return &i;
    return nullptr;
}

void World::witness(Incident& inc, double sleight)
{
    const auto* off = entity(inc.offender);
    const auto* victim = entity(inc.victim);
    if (!off)
        return;
    const std::string victimName = victim ? victim->name : "someone";
    const std::string claim = inc.kind == "assault" ? "attacked " + victimName
                            : inc.kind == "attempted theft" ? "tried to steal from " + victimName : "stole from " + victimName;
    // (What a player is told they see happen: "You see Piet steal from Imogen.", never "You see Piet stole from".)
    const std::string sight = inc.kind == "assault" ? "attack " + victimName
                            : inc.kind == "attempted theft" ? "try to steal from " + victimName : "steal from " + victimName;
    const std::string unseen = inc.kind == "assault" ? "was attacked by someone they couldn't make out"
                                                     : "was robbed by someone they couldn't make out";
    for (const auto& [id, e] : entities_)
    {
        if (id == inc.offender || e.dead || e.transient || e.cellId != inc.cell || e.hurt >= DownAt)
            continue;
        if (std::any_of(inc.witnesses.begin(), inc.witnesses.end(), [&](const Witness& w) { return w.id == id; }))
            continue;
        double clarity = 0;
        bool identified = false;
        if (!off->offstage && !e.offstage && e.npc && !off->npc)
        {
            // A resident names a player only if it noticed them (doc 40): alert, it saw who; only suspicious, it saw
            // someone it couldn't make out; unaware, at most it heard the struggle.
            const double aware = senseInWorld(id, inc.offender);
            identified = aware >= battle::AwareAlert;
            clarity = identified ? std::max(.6, visionClarity(id, inc.offender)) : aware >= battle::AwareSuspicious ? .5 : 0;
            if (clarity <= 0 && inc.kind == "assault")
                clarity = .5 * hearingClarity(id, inc.victim, Voice::Yell);   // The sounds of a struggle.
        }
        else if (!off->offstage && !e.offstage)
        {
            clarity = visionClarity(id, inc.offender);
            identified = clarity > 0;
            if (!identified && inc.kind == "assault")
                clarity = .5 * hearingClarity(id, inc.victim, Voice::Yell);   // The sounds of a struggle.
        }
        else if (const double d = between(e.position, off->position); d < 8)
        {
            clarity = 1 - d / 8;                    // Offstage there is no line of sight to trace: nearness will do.
            identified = true;
        }
        if (clarity <= .05)
            continue;
        // A deft hand goes unnoticed by most; the one robbed may feel it all the same.
        const double noticed = sleight > 0 ? clarity * (1 - sleight) * (id == inc.victim ? 1.5 : 1) : 1;
        if (chance(id + inc.id, 1) >= noticed)
            continue;
        inc.witnesses.push_back({id, identified, clarity, guardOnDuty(id)});
        if (!e.npc)
        {
            // (Food taken from a till moved its shelves, journalled whole with the next record: so the notice is a move, and
            // the host journals before it tells it, as for a raid; doc 79, Phase 6 merged. A pocket's coins are staged
            // purses, not journalled on a tick. Either way the incident, with this player among its witnesses, is a crime
            // the host journals before telling: a crash never takes back what a player was told they saw, which they may
            // go and report to the Watch; Phase 6 merged, checked.)
            notice(id, identified ? "You see " + off->name + " " + sight + "."
                                  : inc.kind == "assault" ? "You hear a struggle nearby." : "You glimpse someone robbing " + victimName + ".",
                   inc.kind == "theft" && !inc.from.empty(), true);
            continue;
        }
        if (identified)
        {
            believe(id, inc.offender, claim, "saw it", .6 + .4 * clarity, inc.id);
            bonds_.change(id, inc.offender, {-3, -5, 0, inc.kind == "assault" ? 4.0 : 1.0, -2}, calendarDays_);
        }
        else if (victim)
            believe(id, inc.victim, unseen, "saw it", .5 + .3 * clarity, inc.id);
    }
    weigh(inc);
}

void World::weigh(Incident& inc)
{
    if (inc.status != "open" || inc.town.empty())
        return;
    const auto guards = guardsOf(inc.town);
    if (guards.empty())
        return;                                     // No watch here: nobody to tell, nobody to act.
    double evidence = 0, hearsay = 0;
    for (const auto& w : inc.witnesses)
        if (w.reported && w.identified)
        {
            const bool guard = std::find(guards.begin(), guards.end(), w.id) != guards.end();
            evidence += w.clarity * (guard ? 2.0 : w.id == inc.victim ? 1.0 : .6);
        }
    // What the guards have heard said of it, second hand, counts for a little.
    for (const auto& g : guards)
        if (const auto* heard = beliefsOf(g))
            for (const auto& b : *heard)
                if (b.incident == inc.id && b.subject == inc.offender && b.source != "saw it")
                    hearsay += .25 * b.confidence;
    evidence += std::min(hearsay, .5);
    if (evidence < Evidence)
        return;
    inc.status = "charged";
    Warrant* w = nullptr;
    for (auto& existing : crime_.warrants)
        if (existing.person == inc.offender && existing.town == inc.town)
            w = &existing;
    if (!w)
    {
        crime_.warrants.push_back({inc.offender, inc.town, {}, {}, 0, calendarDays_});
        w = &crime_.warrants.back();
    }
    w->incidents.push_back(inc.id);
    if (inc.kind == "theft" && (inc.coins > 0 || inc.quantity > 0))
        w->restitution.push_back({inc.victim, inc.item, inc.quantity, inc.coins, inc.from});
    // (The fine: 8p for an assault, 3p for a theft or an attempt; and, for a theft, what was taken.)
    w->fine += (inc.kind == "assault" ? 8 : 3) +
               (inc.kind == "assault" || inc.kind == "attempted theft" ? 0 : inc.coins + worth(society_, inc.victim, inc.item, inc.quantity));
    const auto* off = entity(inc.offender);
    recordEvent({"warrant", inc.offender, inc.victim, inc.cell, 0, 0, inc.item, inc.quantity, inc.coins,
                 "wanted by the watch of " + inc.town + " for " + charge(inc.kind) + " (" + inc.id + ")"});
    for (const auto& g : guards)
        believe(g, inc.offender, "is wanted for " + charge(inc.kind), "the watch", 1, inc.id);
    if (off && !off->npc)
        notice(inc.offender, "Word has reached the watch. You are wanted for " + charge(inc.kind) + ".", false, true);   // (A crime's.)
}

void World::reportTo(Incident& inc, Witness& w, const std::string& guard)
{
    w.reported = true;
    recordEvent({"reported", w.id, guard, inc.cell, 0, 0, {}, 0, 0, charge(inc.kind) + " (" + inc.id + ")"});
    if (w.identified)
    {
        const auto* victim = entity(inc.victim);
        believe(guard, inc.offender, (inc.kind == "assault" ? "attacked " : "stole from ") + (victim ? victim->name : "someone"),
                w.id, .5 + .4 * w.clarity, inc.id);
    }
    weigh(inc);
}

bool World::willReport(const Witness& w, const Incident& inc) const
{
    const auto* e = entity(w.id);
    if (!e || e->dead || !e->npc)
        return false;
    // Fear keeps the mouth shut; so does fondness for whoever did it.
    if (const auto* b = bonds_.find(w.id, inc.offender))
        return w.id == inc.victim ? b->fear < 60 : b->fear < 50 && b->affinity < 40;
    return true;
}


Result World::steal(const std::string& thief, const std::string& victimId)
{
    auto* t = entity(thief);
    if (!t || t->dead)
        return {false, "No such character.", {}};
    if (custodyOf(thief))
        return {false, "You are held in the gaol.", {}};
    auto* v = entity(victimId);
    if (!v || v->transient || v->id == thief)
        return {false, "There is nothing of theirs to take.", victimId};
    if (!t->npc && !v->npc)
        return {false, "You can't steal from another player.", victimId};
    if (v->dead)
        return {false, v->name + " is beyond caring.", victimId};
    if (t->cellId != v->cellId || between(t->position, v->position) > StealReach)
        return {false, "Get closer first.", victimId};
    if (const auto ready = stealReady_.find(thief); ready != stealReady_.end() && time_ < ready->second)
        return {false, "Not so soon: they'd notice.", victimId};
    stealReady_[thief] = time_ + StealEvery;
    const auto* purse = society_.account(victimId);
    if (!purse)
        return {false, "They carry nothing worth taking.", victimId};
    const auto at = std::int64_t(time_ * 1000);
    // A deft, quiet paw against a wary eye.
    // How watchful: a resident by what it has noticed of the thief, checked now (doc 40); a player by sight.
    const double watchful = v->hurt >= DownAt ? 0 : t->offstage || v->offstage ? .3
                          : v->npc && !t->npc ? std::min(1.0, senseInWorld(victimId, thief)) : visionClarity(victimId, thief);
    const double odds = std::clamp(.35 + (effectiveDexterity(*t) - 50) / 200 + t->sneakSkill / 300 +
                                       (t->posture == "crouching" ? .1 : 0) - .3 * watchful, .05, .9);
    const bool success = chance(thief + victimId, at) < odds;
    std::string item;
    int quantity = 0;
    std::int64_t coins = 0;
    std::string tookFrom;                   // (A till the goods came from: its shop's, not the keeper's own.)
    if (success)
    {
        // Now and then a masterwork, marked with its maker's scent (doc 35): a prize that can give a thief away.
        std::string prize;
        for (const auto& [held, n] : purse->stock)
            if (n > 0 && !items::makerOf(held).empty() && World::wornCount(*v, held) < n)
                prize = held;
        const bool shielded = !t->npc && isProtected(victimId);   // (Never coin from a protected resident: doc 57, 6.)
        // What a pocket holds goes with its band (doc 79, 6.5 and 10.1: a destitute one 0-1p, a rich one 5-25p); one in
        // no band, 1-5p as ever.
        std::int64_t take = 1 + std::int64_t(roll(thief, at) % 5);
        if (const auto* band = v->npc ? society_.statusOf(victimId) : nullptr)
        {
            const auto& row = society_.statusTable().rows[std::size_t(std::clamp(band->band, 0, status::Bands - 1))];
            take = row.pocketLow + std::int64_t(roll(thief, at) % std::uint64_t(std::max<std::int64_t>(1, row.pocketHigh - row.pocketLow + 1)));
        }
        // A hungry thief at a food shop takes food from its till's stock (doc 79, 10.1), not the keeper's own purse.
        std::string till;
        // (Only from a shop being kept: its keeper trading there, not one gone home with its till's goods behind it.)
        const auto* keeping = v->npc ? society_.resident(victimId) : nullptr;
        if (t->npc && v->npc && !shielded && keeping && keeping->task == "trade")
            if (const auto motive = markMotive_.find(thief); motive != markMotive_.end() && motive->second == "food")
                if (const auto own = society_.tillOf(victimId); own != victimId)
                    if (const auto* shelf = society_.account(own))
                    {
                        std::string food;
                        for (const auto& [held, n] : shelf->stock)
                            if (n > 0 && Society::edible(held) && (food.empty() || Society::catalogPrice(held) < Society::catalogPrice(food)))
                                food = held;
                        if (!food.empty())
                            till = own, item = food,
                            quantity = std::min(Society::stock(*shelf, food), 1 + int(roll(thief + "|food", at) % 2));
                    }
        if (till.empty())
        {
            if (!prize.empty() && !shielded && roll(thief + "|prize", at) % 4 == 0)
                item = prize, quantity = 1;
            else if (purse->cash > 0 && !shielded && take > 0)
                coins = std::min<std::int64_t>(purse->cash, take);
            else if (Society::stock(*purse, "meal") > 0)
                item = "meal", quantity = 1;
            else if (Society::stock(*purse, "herbs") > 0)
                item = "herbs", quantity = 1;
            else
                return {true, "You find nothing in " + v->name + "'s pack worth the risk.", victimId};
        }
        // The staged economy (doc 79, 6.5 and 9.3): a second theft in a victim's lean days finds nothing, and a player's
        // take from a staged purse is held to the day's theft cap (coins and goods together, by account holder).
        if (v->npc)
        {
            const std::int64_t room = t->npc ? -1 : society_.faucetRoom(thief, "theftADay");
            if (coins > 0 && room >= 0)
                coins = std::min(coins, room);
            if ((till.empty() && society_.leanNow(victimId)) || (coins <= 0 && item.empty()) ||
                (!item.empty() && room >= 0 && Society::catalogPrice(item) * quantity > room))
                return {true, "You find nothing in " + v->name + "'s pack worth the risk.", victimId};
        }
        if (!society_.shift(till.empty() ? victimId : till, thief, item, quantity, coins, "stolen"))
            return {false, "Your paw comes away empty.", victimId};
        if (!till.empty())
            society_.noteTillMoved(till);       // (Its shelves moved: journalled with the next record.)
        tookFrom = till;
    }
    auto& inc = openIncident(success ? "theft" : "attempted theft", thief, victimId);
    inc.from = tookFrom;
    inc.item = item;
    inc.quantity = quantity;
    inc.coins = coins;
    recordEvent({inc.kind, thief, victimId, inc.cell, 0, 0, item, quantity, coins, inc.id});
    if (!success)
        bonds_.change(victimId, thief, {-8, -15, 1, 2, -3}, calendarDays_);
    // A clumsy try is plain to anyone looking; a clean one only to the sharp-eyed.
    const double sleight = success ? std::clamp(.3 + t->sneakSkill / 200 + effectiveDexterity(*t) / 400, 0.0, .85) : 0.0;
    witness(inc, sleight);
    const bool caught = std::any_of(inc.witnesses.begin(), inc.witnesses.end(), [&](const Witness& w) { return w.id == victimId; });
    if (!success)
        return {true, v->name + " catches you at " + (v->npc ? "their" : "your") + " purse!", victimId};
    const auto* prized = items::good(item);
    const std::string took = coins > 0 ? pennies(coins) : item == "meal" ? "a meal" : item == "herbs" ? "a bundle of herbs"
                           : quantity > 1 ? std::to_string(quantity) + " of " + std::string(prized ? prized->name : "something")
                                          : "a " + std::string(prized ? prized->name : "thing");
    return {true, "You lift " + took + " from " + v->name + (caught ? ", and they feel it." : " unnoticed."), victimId};
}

Result World::report(const std::string& player, const std::string& guard)
{
    const auto* p = entity(player);
    const auto* g = entity(guard);
    const auto* job = society_.jobOf(guard);
    if (!p || !g || !job || job->role != "guard" || g->dead)
        return {false, "They are not of the watch.", guard};
    if (p->cellId != g->cellId || between(p->position, g->position) > 3)
        return {false, "Get closer first.", guard};
    int told = 0;
    for (auto& inc : crime_.incidents)
        for (auto& w : inc.witnesses)
            if (w.id == player && !w.reported && inc.offender != player)
            {
                reportTo(inc, w, guard);
                ++told;
            }
    if (!told)
        return {true, g->name + " listens, but you have nothing new to tell the watch.", guard};
    return {true, "You tell " + g->name + " what you saw. They note it down.", guard};
}

Result World::payFine(const std::string& person, const std::string& guard)
{
    const auto* p = entity(person);
    const auto* g = entity(guard);
    const auto* job = society_.jobOf(guard);
    if (!p || !g || !job || job->role != "guard")
        return {false, "They are not of the watch.", guard};
    if (p->cellId != g->cellId || between(p->position, g->position) > 3)
        return {false, "Get closer first.", guard};
    const auto here = lawTown(g->cellId);
    auto it = std::find_if(crime_.warrants.begin(), crime_.warrants.end(),
                           [&](const Warrant& w) { return w.person == person && !here.empty() && w.town == here; });
    if (it == crime_.warrants.end())
        return {false, "The watch wants nothing of you.", guard};
    const auto* purse = society_.account(person);
    if (!purse || purse->cash < owedBy(*it))
        return {false, "You can't pay " + pennies(owedBy(*it)) + ".", guard};
    settleWarrant(*it, guard);
    crime_.warrants.erase(it);
    confrontations_.erase(person);
    for (auto pursuit = pursuits_.begin(); pursuit != pursuits_.end();)
        pursuit = pursuit->second == person ? pursuits_.erase(pursuit) : std::next(pursuit);
    return {true, "You pay what the watch asks. " + g->name + " lets you go.", guard};
}

void World::settleWarrant(const Warrant& w, const std::string& guard)
{
    // What was taken goes back (the goods if they're still in hand, else their worth); the fine to the treasury.
    std::int64_t paid = 0;
    for (const auto& r : w.restitution)
    {
        const auto* purse = society_.account(w.person);
        if (!r.item.empty() && purse && Society::stock(*purse, r.item) >= r.quantity)
        {
            // (Goods taken from a shop's till go back on its shelf, not into its keeper's own wallet: doc 79, 10.1.)
            const bool shelf = !r.till.empty() && society_.account(r.till);
            society_.shift(w.person, shelf ? r.till : r.to, r.item, r.quantity, 0, "restitution");
            if (shelf)
                society_.noteTillMoved(r.till);
        }
        else if (const auto value = worth(society_, r.to, r.item, r.quantity); value > 0)
            paid += society_.shift(w.person, r.to, "", 0, value, "restitution") ? value : 0;
        if (r.coins > 0)
            paid += society_.shift(w.person, r.to, "", 0, r.coins, "restitution") ? r.coins : 0;
    }
    const auto* purse = society_.account(w.person);
    const std::int64_t fine = std::min<std::int64_t>(w.fine, purse ? purse->cash : 0);
    if (fine > 0)
        society_.shift(w.person, "treasury", "", 0, fine, "fine");
    for (const auto& id : w.incidents)
        if (auto* inc = incident(id))
            inc->status = "settled";
    recordEvent({"fine paid", w.person, guard, entity(w.person) ? entity(w.person)->cellId : "", 0, 0, {}, 0, paid + fine,
                 "to the watch of " + w.town});
}

bool World::gaolSpot(const std::string& townId, std::string& cellId, double& x, double& y)
{
    // A cell named for holding people, else wherever the town's guards keep their watch.
    static const std::regex held("gaol|jail|lock-?up|cells|watch ?house|guard ?house|barracks", std::regex::icase);
    std::string fallback;
    for (const auto& [id, c] : cells_)
    {
        if (lawTown(id) != townId)
            continue;
        if (std::regex_search(c.name, held))
        {
            cellId = id;
            break;
        }
    }
    if (cellId.empty())
        for (const auto& p : society_.positions())
            if (p.role == "guard")
                if (lawTown(p.work.cell) == townId)
                {
                    cellId = p.work.cell;
                    x = p.work.x;
                    y = p.work.y;
                    break;
                }
    if (cellId.empty())
        return false;
    const auto* c = cell(cellId);
    if (x <= 0 && c)
    {
        x = c->width / 2.0;
        y = c->height / 2.0;
    }
    return freeSpotNear(cellId, x, y) || c != nullptr;
}

void World::takeIntoCustody(const std::string& person, const std::string& townId, const std::string& guard)
{
    auto* p = entity(person);
    auto it = std::find_if(crime_.warrants.begin(), crime_.warrants.end(),
                           [&](const Warrant& w) { return w.person == person && w.town == townId; });
    if (!p || it == crime_.warrants.end())
        return;
    // Whatever they carry goes to making it good; the rest they serve in hours.
    settleWarrant(*it, guard);
    crime_.warrants.erase(it);
    confrontations_.erase(person);
    for (auto pursuit = pursuits_.begin(); pursuit != pursuits_.end();)
        pursuit = pursuit->second == person ? pursuits_.erase(pursuit) : std::next(pursuit);
    Custody c;
    c.person = person;
    c.town = townId;
    if (!gaolSpot(townId, c.cell, c.x, c.y))
        return;
    c.until = calendarDays_ + (p->npc ? ResidentHours : PlayerHours) / 24;
    stop(person);
    // The watch takes in a prisoner, not a corpse: whoever is down has their wounds bound first (it doesn't spend
    // their own once-a-day recovery).
    const bool bound = p->downedLeft > 0;
    if (bound)
        standUp(*p, battle::TendedHealth);
    if (!p->npc)
    {
        p->stamina = 0;
        p->exhausted = true;
    }
    if (ensureLoaded(c.cell).ok)
    {
        p->cellId = c.cell;
        p->position = {c.x, c.y};
        p->velocity = {};
        p->path.clear();
        p->input = {};
        p->transitioned = true;
        if (!p->npc)
            observe(person);
    }
    recordEvent({"arrest", guard, person, c.cell, 0, 0, {}, 0, 0, "held by the watch of " + townId});
    crime_.custody.push_back(std::move(c));
    // (The fine and what is made good came out of a player's own coins, and the warrant became custody: so the notice is a
    // move and a crime, journalled before it is told. Taken on a tick, a crash before the next record once left the player
    // their coins and the warrant open, out of the gaol they had been told they were held in; Phase 6 merged, checked.)
    if (!p->npc)
        notice(person, std::string(bound ? "The watch binds your wounds and carries you to the gaol."
                                         : "The watch drags you to the gaol.") +
                           " You will be held there for " + std::to_string(int(PlayerHours)) + " game hours.",
               true, true);
}

void World::confront(const std::string& guard, const std::string& person)
{
    const auto* g = entity(guard);
    const auto* p = entity(person);
    const auto* w = warrantFor(person);
    if (!g || !p || !w)
        return;
    if (p->npc)
    {
        // A resident pays if they can, else goes quietly to the gaol.
        const auto* purse = society_.account(person);
        if (purse && purse->cash >= owedBy(*w))
        {
            const auto it = std::find_if(crime_.warrants.begin(), crime_.warrants.end(), [&](const Warrant& x) { return &x == w; });
            settleWarrant(*it, guard);
            crime_.warrants.erase(it);
            pursuits_.erase(guard);
        }
        else
            takeIntoCustody(person, w->town, guard);
        return;
    }
    if (confrontations_.count(person))
        return;
    if (p->downedLeft > 0)
    {
        takeIntoCustody(person, w->town, guard);    // No use asking the fallen to pay: they are carried in.
        return;
    }
    std::string charges;
    for (const auto& id : w->incidents)
        if (const auto* inc = incident(id))
            charges += (charges.empty() ? "" : " and ") + charge(inc->kind);
    confrontations_[person] = {guard, time_ + ConfrontSeconds};
    recordEvent({"stopped by the watch", guard, person, p->cellId, 0, 0, {}, 0, 0, charges});
    notice(person, g->name + " blocks your way: \"You're wanted by the watch for " + charges + ". Pay " +
                       pennies(owedBy(*w)) + ", what's owed and the fine, or you'll come with me.\"");
}

bool World::crimeErrand(const std::string& resident, std::string& task, std::string& reason, std::string& goalCell, Vec2& goal) const
{
    if (const auto* c = custodyOf(resident))
    {
        task = "held in the gaol";
        reason = "until the watch lets them go";
        goalCell = c->cell;
        goal = {c->x, c->y};
        return true;
    }
    if (const auto pursuit = pursuits_.find(resident); pursuit != pursuits_.end())
        if (const auto* p = entity(pursuit->second))
        {
            task = "stopping " + p->name;
            reason = "wanted by the watch";
            goalCell = p->cellId;
            goal = p->position;
            return true;
        }
    if (const auto look = lookings_.find(resident); look != lookings_.end())
    {
        // Something half heard or glimpsed, a sneak creeping about (doc 40): a guard goes to look; anyone else backs off.
        task = look->second.task;
        reason = look->second.reason;
        goalCell = look->second.cell;
        goal = look->second.at;
        return true;
    }
    if (const auto mark = marks_.find(resident); mark != marks_.end())
        if (const auto* m = entity(mark->second))
        {
            task = "looking for a chance";
            const auto motive = markMotive_.find(resident);
            reason = motive != markMotive_.end() && motive->second == "pocket" ? "short of coin, and someone's purse looks heavy"
                                                                              : "hungry, and no coin for a meal";
            goalCell = m->cellId;
            goal = m->position;
            return true;
        }
    return false;
}

void World::tendCrime()
{
    const auto today = std::int64_t(std::floor(calendarDays_));
    if (crime_.day < 0)
        crime_.day = today;
    if (today != crime_.day)
    {
        crime_.day = today;
        crimeDaily();
    }
    if (const auto hour = std::int64_t(std::floor(calendarDays_ * 24)); hour != crimeHour_)
    {
        crimeHour_ = hour;
        chooseMarks();
    }
    // The beaten get up again in time.
    for (auto& [id, e] : entities_.inOrder())
        if (e.hurt > 0 && !e.dead && e.downedLeft <= 0 && !inBattle(id))  // Not while down, nor in a fight.
        {
            e.hurt = std::max(0.0, e.hurt - HealPerHour * .5 / (calendar::SecondsPerDay / 24));
            if (e.state == "beaten down" && e.hurt < UpBelow)
            {
                e.state.clear();
                if (e.npc)
                    setPosture(id, "standing");
            }
        }
    // Out of the gaol when the time is served.
    for (auto it = crime_.custody.begin(); it != crime_.custody.end();)
    {
        if (calendarDays_ < it->until)
        {
            ++it;
            continue;
        }
        recordEvent({"released", it->person, {}, it->cell, 0, 0, {}, 0, 0, "by the watch of " + it->town});
        if (const auto* p = entity(it->person); p && !p->npc)
            notice(it->person, "The watch lets you go. You're free to leave the gaol.", false, true);
        it = crime_.custody.erase(it);
    }
    // Witnesses tell a guard on duty they meet.
    std::vector<std::string> onDuty;
    for (const auto& [id, e] : entities_.inOrder())
        if (e.npc && !e.offstage && guardOnDuty(id))
            onDuty.push_back(id);
    // (Not a crime more than WitnessDays old: one who kept quiet that long keeps quiet. Without this every tick asked every
    // silent witness of the 400 incidents kept, and the cost grew all year: Docs/Design/31-responsiveness.md.)
    for (auto& inc : crime_.incidents)
        for (auto& w : inc.witnesses)
        {
            if (w.reported || calendarDays_ - inc.day > WitnessDays || !willReport(w, inc))
                continue;
            const auto* e = entity(w.id);
            if (!e || e->offstage)
                continue;
            for (const auto& g : onDuty)
                if (const auto* ge = entity(g); ge && ge->cellId == e->cellId && between(ge->position, e->position) <= ReportReach)
                {
                    reportTo(inc, w, g);
                    break;
                }
        }
    // Guards on duty who see someone wanted go after them.
    for (const auto& w : crime_.warrants)
    {
        const auto* p = entity(w.person);
        if (!p || p->dead || p->offstage || custodyOf(w.person) || inBattle(w.person))
            continue;                               // (Not in the middle of a fight: the watch waits for its end.)
        const bool chased = std::any_of(pursuits_.begin(), pursuits_.end(), [&](const auto& x) { return x.second == w.person; });
        if (chased)
            continue;
        for (const auto& g : onDuty)
        {
            const auto* ge = entity(g);
            // Seen, for a resident; noticed (doc 40: a sneak can slip past), for a player.
            const bool seen = p->npc ? visionClarity(g, w.person) > 0 : residentAwareness(g, w.person) >= battle::AwareAlert;
            if (lawTown(ge->cellId) == w.town && !pursuits_.count(g) && ge->cellId == p->cellId && seen)
            {
                pursuits_[g] = w.person;
                break;
            }
        }
    }
    for (auto it = pursuits_.begin(); it != pursuits_.end();)
    {
        const auto* g = entity(it->first);
        const auto* p = entity(it->second);
        if (!g || !p || !warrantFor(it->second) || g->cellId != p->cellId || !guardOnDuty(it->first))
        {
            it = pursuits_.erase(it);
            continue;
        }
        if (between(g->position, p->position) <= 1.8 && !inBattle(it->second))
        {
            // Confronting a resident ends this pursuit, and perhaps others after it (takeIntoCustody): go on from
            // this guard's place in the map, not from an iterator that may be gone.
            const std::string guard = it->first, person = it->second;
            confront(guard, person);
            it = pursuits_.upper_bound(guard);
            continue;
        }
        ++it;
    }
    // A player told to pay who neither pays nor stays is taken.
    for (auto it = confrontations_.begin(); it != confrontations_.end();)
    {
        const auto person = it->first;
        const auto* g = entity(it->second.guard);
        const auto* p = entity(person);
        const auto* w = warrantFor(person);
        if (!g || !p || !w)
        {
            it = confrontations_.erase(it);
            continue;
        }
        const bool gone = g->cellId != p->cellId || between(g->position, p->position) > WalkAway;
        if ((time_ < it->second.deadline && !gone) || inBattle(person))
        {
            ++it;
            continue;
        }
        const auto guard = it->second.guard, townId = w->town;
        ++it;
        takeIntoCustody(person, townId, guard);     // (Erases the confrontation.)
    }
    // The hungry who have made up their minds wait for their chance, when no guard is looking.
    for (auto it = marks_.begin(); it != marks_.end();)
    {
        const auto thief = it->first, mark = it->second;
        const auto* t = entity(thief);
        const auto* m = entity(mark);
        if (!t || !m || t->dead || m->dead || custodyOf(thief))
        {
            it = marks_.erase(it);
            continue;
        }
        ++it;
        if (t->cellId != m->cellId || between(t->position, m->position) > StealReach)
            continue;
        const bool watched = std::any_of(onDuty.begin(), onDuty.end(), [&](const std::string& g) { return visionClarity(g, thief) > 0; });
        if (watched)
            continue;
        steal(thief, mark);
        tried_[thief] = today;
        marks_.erase(thief);
        markMotive_.erase(thief);
    }
}

void World::chooseMarks()
{
    // The day's thieves (doc 79, 10.1): an expected count per market, each thief at its hour going for a target that fits
    // its want, kept on it while the mark stays good (till it tries, or dusk). Residents in no band (a world without the
    // staged households: the Greyfen roster) keep the old rule below.
    const auto today = std::int64_t(std::floor(calendarDays_));
    const int hourOfDay = int((calendarDays_ - std::floor(calendarDays_)) * 24);
    const auto tried = [&](const std::string& id) {
        if (const auto last = tried_.find(id); last != tried_.end() && last->second >= today)
            return true;
        // (After a restart the day's incidents say who has tried: the same thieves never try twice in a day.)
        return std::any_of(crime_.incidents.begin(), crime_.incidents.end(), [&](const Incident& i) {
            return i.offender == id && std::int64_t(std::floor(i.day)) == today && (i.kind == "theft" || i.kind == "attempted theft");
        });
    };
    std::map<std::string, std::string> kept;
    std::map<std::string, std::string> motives;
    const auto& plan = theftPlan();
    if (!plan.thieves.empty() && hourOfDay < 21)
    {
        // Who crowds where (a pickpocket works a crowd), counted once.
        std::map<std::string, int> crowd;
        for (const auto& [id, e] : entities_)
            if (!e.dead && !e.transient)
                ++crowd[e.cellId];
        for (const auto& t : plan.thieves)
        {
            const auto* e = entity(t.id);
            const auto* life = society_.resident(t.id);
            const auto* mine = society_.statusOf(t.id);
            // (A household lifted by a player's help since the day's thieves were drawn steals no more: 6.7.)
            if (t.hour > hourOfDay || !e || e->dead || !life || !mine || custodyOf(t.id) || tried(t.id) || society_.liftedNow(t.id))
                continue;
            // Hungry (10.1): hunger 55 or more, and less than a day's food in hand (the two days a household keeps run
            // down by a short day, and nothing on a bare one): it goes for a food stall's till.
            const auto* wallet = society_.account(t.id);
            double inHand = 0;
            if (wallet)
                for (const auto& [item, n] : wallet->stock)
                    if (n > 0 && Society::edible(item))
                        inHand += double(std::max(0, Society::nourishment(item))) * n;
            const bool hungry = life->hunger >= 55 && inHand < Society::NourishADay;
            const bool shopHours = hourOfDay >= Society::ShopOpen && hourOfDay < Society::ShopShut;
            // (Counted for world_check --stage: each thief once a day for each.)
            if (thiefFlagsDay_ != today)
                thiefFlagsDay_ = today, thiefFlags_.clear();
            const auto counted = [&](int bit, long& total) {
                if (auto& f = thiefFlags_[t.id]; !(f & bit))
                    f |= bit, ++total;
            };
            counted(1, thiefTally_.considered);
            if (life->hunger >= 55)
                counted(2, thiefTally_.hungerHigh);
            if (hungry)
                counted(4, thiefTally_.hungry);
            if (hungry && !shopHours)
                counted(8, thiefTally_.hungryShut);
            // A keeper at its open shop, the till's food on the shelf before it: never one gone home or to the tavern.
            const auto trading = [&](const std::string& holder) {
                const auto* m = entity(holder);
                const auto* keeping = society_.resident(holder);
                const auto* job = society_.jobOf(holder);
                if (!m || m->dead || !m->npc || !keeping || keeping->task != "trade" || !job)
                    return false;
                return m->cellId == job->work.cell || (!job->serve.cell.empty() && m->cellId == job->serve.cell) || society_.atStall(holder);
            };
            // Still on a good mark: kept (a till's keeper only while it keeps its shop).
            if (const auto had = marks_.find(t.id); had != marks_.end())
                if (const auto* m = entity(had->second); m && !m->dead && markMotive_.count(t.id) &&
                                                         (markMotive_.at(t.id) != "food" || trading(had->second)))
                {
                    kept[t.id] = had->second;
                    motives[t.id] = markMotive_.at(t.id);
                    continue;
                }
            std::vector<std::string> marks;
            if (hungry && shopHours)
                for (const auto& p : society_.positions())
                {
                    if (p.role != "merchant")
                        continue;
                    const auto& holder = society_.state().careers.positions.at(p.id).holder;
                    const auto* stock = society_.account("till:" + p.id);
                    if (holder == t.id || !stock || society_.marketOf("till:" + p.id) != t.market || !trading(holder))
                        continue;
                    if (std::any_of(stock->stock.begin(), stock->stock.end(), [](const auto& g) { return g.second > 0 && Society::edible(g.first); }))
                        marks.push_back(holder);
                }
            if (hungry && shopHours && marks.empty())
                counted(16, thiefTally_.hungryNoShop);
            if (!marks.empty())
            {
                kept[t.id] = marks[std::size_t(roll(t.id + "|food mark", today) % marks.size())];
                motives[t.id] = "food";
                counted(32, thiefTally_.foodMarks);
                continue;
            }
            // Not hungry (or no food shop): the pocket of someone richer, where people crowd, never a guard, never kin.
            std::string best;
            int bestCrowd = -1;
            std::uint64_t bestRoll = 0;
            for (const auto& [id, other] : society_.state().residents)
            {
                const auto* o = society_.statusOf(id);
                const auto* oe = entity(id);
                const auto* job = society_.jobOf(id);
                const auto* spec = society_.spec(id);
                if (!o || o->band <= mine->band || o->market != t.market || o->household == mine->household || !oe || oe->dead ||
                    oe->transient || (job && job->role == "guard") || (spec && spec->ofTheWatch()) || custodyOf(id))
                    continue;
                const int n = crowd[oe->cellId];
                const auto r = roll(t.id + "|" + id, today);
                if (n > bestCrowd || (n == bestCrowd && r < bestRoll))
                    best = id, bestCrowd = n, bestRoll = r;
            }
            if (!best.empty())
            {
                kept[t.id] = best;
                motives[t.id] = "pocket";
                counted(64, thiefTally_.pocketMarks);
            }
        }
    }
    marks_ = std::move(kept);
    markMotive_ = std::move(motives);
    // The old rule, for residents in no band: the hungry and penniless, some of them, make up their minds to take a meal
    // (at most once a day).
    const std::int64_t meal = 6;                    // (Penniless: short of a meal's price.)
    for (const auto& [id, life] : society_.state().residents)
    {
        if (society_.statusOf(id))
            continue;
        const auto* e = entity(id);
        const auto* purse = society_.account(id);
        const auto* job = society_.jobOf(id);
        if (!e || e->dead || !purse || (job && job->role == "guard") || custodyOf(id) || roll(id, 7) % 10 >= 4)
            continue;
        if (const auto last = tried_.find(id); last != tried_.end() && last->second >= today)
            continue;
        if (life.hunger < 70 || Society::stock(*purse, "meal") > 0 || purse->cash >= meal)
            continue;
        const auto here = lawTown(e->cellId);
        for (const auto& p : society_.positions())
        {
            if (p.role != "merchant")
                continue;
            const auto& holder = society_.state().careers.positions.at(p.id).holder;
            const auto* m = entity(holder);
            const auto* stock = society_.account(society_.tillOf(holder));   // (A shop with food: its till.)
            if (m && holder != id && !m->dead && stock && Society::stock(*stock, "meal") > 0 && !here.empty() &&
                lawTown(m->cellId) == here)
            {
                marks_[id] = holder;
                break;
            }
        }
    }
}

void World::crimeDaily()
{
    const auto today = std::int64_t(std::floor(calendarDays_));
    for (auto& inc : crime_.incidents)
    {
        if (inc.status != "open")
            continue;
        // The robbed find their purse lighter by nightfall, whoever did it.
        if (inc.kind == "theft" && !std::any_of(inc.witnesses.begin(), inc.witnesses.end(), [&](const Witness& w) { return w.id == inc.victim; }))
            if (const auto* v = entity(inc.victim); v && v->npc && !v->dead)
            {
                inc.witnesses.push_back({inc.victim, false, 1, false});
            }
        // Offstage, witnesses find their way to the watch within the day.
        if (!guardsOf(inc.town).empty())
        {
            const auto guards = guardsOf(inc.town);
            for (auto& w : inc.witnesses)
                if (!w.reported && willReport(w, inc) && !guards.empty())
                    reportTo(inc, w, guards[std::size_t(roll(w.id, today) % guards.size())]);
        }
        if (inc.status == "open" && calendarDays_ - inc.day > ColdDays)
            inc.status = "cold";
    }
    // Residents wanted where no guard has set eyes on them are found in time.
    std::vector<std::pair<std::string, std::string>> found;
    for (const auto& w : crime_.warrants)
        if (const auto* p = entity(w.person); p && p->npc && p->offstage && !p->dead)
            if (const auto n = double(guardsOf(w.town).size()); n > 0 && chance(w.person, today) < n / (n + 3))
            {
                const auto guards = guardsOf(w.town);
                if (!guards.empty())
                    found.push_back({guards.front(), w.person});
            }
    for (const auto& [guard, person] : found)
        confront(guard, person);
    gossip();
}
} // namespace ratw
