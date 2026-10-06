// Not a test: what decides a fight (Docs/Design/44-levelling.md, 45-gift-balance.md), on the real fight rules
// (Core/RatwBattle.cpp, RatwMagic.cpp): a level gap (a level is +1.5 fighting skill, nothing else), striking first, gear,
// numbers, and Gifts. Each wolf closes on the nearest foe and bites (or cuts, with a sword) until one side is down; a
// wolf with a Gift uses it, smartly (a per-family plan) or naively (whatever is ready), hundreds of times over.
//
//   build-gifts/level_sim [fights] [suite]
//
// Suites: levels (the default: ungifted, doc 44), core (doc 45's yardsticks, bare and armed), gifted2 and quick2 (each
// family alone and in a pair), qvq (every Quickened matchup), wide (higher levels, mixed teams, three a side), worth
// (what a head start in health or the first blow is worth), trance (a Quickened wolf ten levels over one to five plain
// ones: doc 45's Trance targets), parties, ladder and crowd (one Quickened against one, two or three), gifts (the first
// broad sweep), naive, and the parts of gifts on their own (quickened, gifted, teams, gear, gaps, adjacent). One row: "duel <side> <side> [level] [gap]", a side being wolves joined by commas, each "plain" or a
// family, with "q" for Quickened, "+" for a sword and leather, "!" for naive play: "duel fireq+ plain+ 10".
// SIM_GAP sets how far apart the sides start (5 by default: there neither gains by waiting; 11 for the Trance's runs);
// SIM_TACTICS has every wolf play tactics (runs to close, focuses, flanks, spreads, steps back outnumbered, takes a
// moment to act); SIM_TRANCE (never, outnumbered, always) when a Quickened wolf goes into a Trance; SIM_SKIP leaves
// abilities unused; SIM_LEVEL one level for the trance suite; SIM_DETAILS lists each ability used; SIM_TRACE prints one
// fight's log. A wolf may carry its own level: "fireq@20".
#include "RatwBattle.h"
#include "RatwGifts.h"
#include "RatwInjury.h"
#include "RatwWorld.h"
#include "battle_play.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <mutex>
#include <thread>
#include <string>
#include <vector>

using namespace ratw;
namespace
{
struct Wolf
{
    int level = 1;
    bool sword = false, armour = false;     // A bit-sword; a leather kit (barding, gorget, cap, leg guards).
    std::string gift;                       // A family, or "" for none.
    bool quickened = false;
    bool naive = false;                     // Uses whatever of its Gift is ready, not a plan.
    double hurt = 0;                        // Starts the fight this hurt (a yardstick: what a head start in health is worth).
};

Wolf gifted(const std::string& family, int level = 1) { return {level, false, false, family, false}; }
Wolf quick(const std::string& family, int level = 1) { return {level, false, false, family, true}; }
Wolf armed(Wolf w) { w.sword = w.armour = true; return w; }
Wolf naive(Wolf w) { w.naive = true; return w; }

const std::vector<std::string> Families = {"fire", "earth", "water", "wind", "sound", "blinker", "gravity", "seer"};
thread_local std::map<std::string, Wolf> wolves;    // Who is who in the fight being played.
thread_local std::map<std::string, int> used;       // Each ability used (and each overreach), in this thread's fights.
std::map<std::string, int> usedAll;                 // ...over a row.
std::mutex usedLock;
bool tally = true;
bool trace = false;
bool tactics = false;                               // SIM_TACTICS: every wolf plays the fight's tactics (below).

std::string at(int x, int y) { return std::to_string(x) + "," + std::to_string(y); }
std::string at(const BattleFighter& o) { return at(o.x, o.y); }
int apart(const BattleFighter& a, const BattleFighter& b) { return test::apart(a.x, a.y, b.x, b.y); }

int standingOf(const Battle& b, int side);

// One wolf's Gift on its turn, by its plan. True if its action went on it.
struct Turn
{
    World& w;
    std::string id;
    const Battle* b = nullptr;
    const BattleFighter* me = nullptr;
    Entity* e = nullptr;
    std::vector<const BattleFighter*> foes, allies, down;   // (Allies: oneself not among them.)
    const BattleFighter* mark = nullptr;
    int d = 99;
    int reach = 1;                                  // Its blow's reach (a sword's is 2).
    mutable bool wanted = false;                    // A gathered Gift it would cast, but not safely yet.

    Turn(World& world, const std::string& who) : w(world), id(who) { look(); }
    void look()
    {
        b = w.battleOf(id);
        me = b ? b->fighter(id) : nullptr;
        e = w.entity(id);
        foes.clear();
        allies.clear();
        down.clear();
        mark = nullptr;
        d = 99;
        if (!me)
            return;
        reach = e->mouth == "sword" ? battle::SwordReach : 1;
        for (const auto& o : b->fighters)
        {
            if (o.id == id)
                continue;
            if (o.side != me->side && o.status == "fighting" && !o.unseen)
            {
                foes.push_back(&o);
                if (!mark || apart(*me, o) < d)
                {
                    mark = &o;
                    d = apart(*me, o);
                }
            }
            else if (o.side == me->side && o.status == "fighting")
                allies.push_back(&o);
            else if (o.side == me->side && o.status == "downed")
                down.push_back(&o);
        }
    }
    bool canStrikeHere() const
    {
        for (const auto* o : foes)
            if (apart(*me, *o) <= reach)
                return true;
        return false;
    }
    // Whether a blow can be struck this turn: a foe in reach now, or after a move.
    bool canStrike() const
    {
        for (const auto* o : foes)
            if (apart(*me, *o) <= reach)
                return true;
        if (me->moved)
            return false;
        for (const auto& [x, y] : w.battleReach(id))
            for (const auto* o : foes)
                if (test::apart(x, y, o->x, o->y) <= reach)
                    return true;
        return false;
    }
    bool live() const { return b && !b->over && me && me->acting && !me->acted && me->status == "fighting"; }
    // Ready, and (unless allowed) without overreaching: past its mana, or the same Gift two turns running.
    bool ok(const std::string& ability, double cost = -1, bool overreach = false) const
    {
        // SIM_SKIP: abilities left unused (comma-separated), to see what each is worth.
        static const std::string skip = std::getenv("SIM_SKIP") ? "," + std::string(std::getenv("SIM_SKIP")) + "," : "";
        if (!skip.empty() && skip.find("," + ability + ",") != std::string::npos)
            return false;
        bool ready = false;
        for (const auto& o : w.giftOptions(id))
            if (o.id == ability)
            {
                ready = o.ready;
                if (cost < 0)
                    cost = o.mana;
            }
        if (!ready)
            return false;
        if (!safe(ability))
        {
            wanted = true;                          // (Held back for a better moment.)
            return false;
        }
        // A Gift whose Tell draws blood (Fire's held breath, a Blinker's nosebleed) isn't paid for near the end of one's
        // strength.
        static const std::set<std::string> bloody = {"flamethrower", "heat_lance", "blastwave", "wall_of_fire", "blink_strike",
                                                     "chain_blink", "unmoor", "displace", "extract"};
        if (tactics && e->quickened && e->hurt > 65 && bloody.count(ability))
            return false;
        // The Gifted keep breath for their blows: help only with stamina to spare (a Tell costs some, doc 43).
        if (!e->quickened && e->stamina < 35)
            return false;
        // In a Trance, a Gift is worth pushing past its limits, when that costs no turn (Blinker and Gravity lose one) and the
        // wolf isn't Trance-fatigued (doc 45).
        if (e->quickened && me->magic.trance > 0 && me->magic.overreaches < 2 && e->gift != "blinker" && e->gift != "gravity" &&
            !injury::spent(e->injuries))
            overreach = true;                       // (Twice at most: each costs more than the last.)
        if (!overreach && e->quickened &&
            (e->mana + 1e-9 < cost || (me->magic.last == ability && me->magic.lastTurn == me->turnsTaken - 1)))
            return false;
        return true;
    }
    bool use(const std::string& ability, const std::string& target)
    {
        const bool reaction = gifts::ability(ability) && gifts::ability(ability)->kind == "reaction";
        const bool over = e->quickened && (e->mana + 1e-9 < (gifts::ability(ability) ? gifts::ability(ability)->mana : 0) ||
                                           (me->magic.last == ability && me->magic.lastTurn == me->turnsTaken - 1));
        const auto r = w.useGift(id, ability, target);
        if (r.ok && tally)
        {
            ++used[(e->quickened ? "Q " : "G ") + ability];
            if (over)
                ++used["  (overreach)"];
        }
        look();
        return r.ok && !reaction;
    }
    // A gathered Gift is safe to start when no foe can reach and strike before it goes off (its Tell broken by a hit):
    // none acting now, none whose bar fills in time. (A player reads the bars; doc 43's Tell.)
    bool safe(const std::string& ability) const
    {
        static const std::map<std::string, double> gather = {
            {"flamethrower", 3}, {"heat_lance", 2}, {"wall_of_fire", 2.5}, {"upheaval", 2}, {"hurl_stone", 3}, {"fissure", 2.5},
            {"stone_wall", 2}, {"wave", 3}, {"flood", 2.5}, {"pressure_drop", 2}, {"shatterhowl", 2}, {"well", 2.5}};
        // A held Gift (a Slam's lift, a Crush) breaks at a hit too: lift a foe that has spent its turn, with no other near.
        if (ability == "slam" || ability == "crush")
        {
            const double haste = w.meterHaste(*b);
            for (const auto* o : foes)
            {
                if (o->acting)
                    return false;
                const double rate = w.meterRate(*b, *o, haste);
                if (o != mark && apart(*me, *o) <= 6 && rate > 0 && (100 - o->meter) / rate < 8)
                    return false;                   // (Another foe whose turn comes before the drop.)
            }
            return true;
        }
        const auto g = gather.find(ability);
        if (g == gather.end())
            return true;
        // (How much room it leaves differs from wolf to wolf, fight to fight: players differ in the risks they take.)
        const double caution = double(std::hash<std::string>{}(id) % 1000) / 1000;
        const double takes = g->second / (1 + e->wisdom / 200) + .1 + 1.2 * caution;
        const double haste = w.meterHaste(*b);
        for (const auto* o : foes)
        {
            const int reach = w.entity(o->id)->mouth == "sword" ? battle::SwordReach : 1;
            if (apart(*me, *o) - reach > 8)
                continue;
            if (o->acting)
                return false;
            const double rate = w.meterRate(*b, *o, haste);
            if (rate > 0 && (100 - o->meter) / rate < takes)
                return false;
        }
        return true;
    }
    int near(const BattleFighter& to, int within, bool foe) const
    {
        int n = 0;
        for (const auto* o : foe ? foes : allies)
            n += apart(*o, to) <= within;
        return n;
    }
    // Whether an ally stands in a cone (or line) from me toward a tile.
    bool allyInCone(int tx, int ty, int range, double degrees) const
    {
        const double aim = std::atan2(double(ty - me->y), double(tx - me->x));
        for (const auto* o : allies)
        {
            const double dx = o->x - me->x, dy = o->y - me->y, far = std::hypot(dx, dy);
            if (far < .5 || far > range + .5)
                continue;
            double off = std::abs(std::atan2(dy, dx) - aim) * 180 / 3.14159265358979323846;
            if (off > 180)
                off = 360 - off;
            if (off <= degrees)
                return true;
        }
        return false;
    }
    // Foes within `within` already hurt this much or more (a Chain Blink can finish them).
    int finishable(int within, double hurt) const
    {
        int n = 0;
        for (const auto* o : foes)
            n += apart(*me, *o) <= within && w.entity(o->id)->hurt >= hurt;
        return n;
    }
    // With tactics, the Gift goes at the foe the side is on (the most hurt in reach), not just the nearest.
    void focus(const std::string& markId)
    {
        for (const auto* o : foes)
            if (o->id == markId)
            {
                mark = o;
                d = apart(*me, *o);
            }
    }
    // Whether one of the side is fighting this foe (beside it): then it isn't shoved or thrown off them.
    bool engaged(const BattleFighter& foe) const
    {
        for (const auto* a : allies)
            if (apart(*a, foe) <= (w.entity(a->id)->mouth == "sword" ? battle::SwordReach : 1))
                return true;
        return false;
    }
    // Whether the first wolf on a line Gift's path toward (tx, ty) is a friend (the rules' line: eight ways, out to range).
    bool allyFirst(int tx, int ty, int range) const
    {
        const double dx = tx - me->x, dy = ty - me->y, n = std::max(std::abs(dx), std::abs(dy));
        if (n < 1)
            return false;
        for (int i = 1; i <= range; ++i)
        {
            const int x = me->x + int(std::lround(dx / n * i)), y = me->y + int(std::lround(dy / n * i));
            for (const auto& o : b->fighters)
                if (o.id != id && o.status == "fighting" && o.x == x && o.y == y)
                    return o.side == me->side;
        }
        return false;
    }
    // The foe to aim a cone or wave at so it takes the most foes (and no friend): its tile, and how many it takes.
    std::pair<std::string, int> bestCone(int range, double half) const
    {
        std::pair<std::string, int> out{mark ? at(*mark) : std::string(), 0};
        for (const auto* aim : foes)
        {
            if (apart(*me, *aim) > range || allyInCone(aim->x, aim->y, range, half))
                continue;
            const double a = std::atan2(double(aim->y - me->y), double(aim->x - me->x));
            int n = 0;
            for (const auto* o : foes)
            {
                const double dx = o->x - me->x, dy = o->y - me->y, far = std::hypot(dx, dy);
                if (far < .5 || far > range + .5)
                    continue;
                double off = std::abs(std::atan2(dy, dx) - a) * 180 / 3.14159265358979323846;
                if (off > 180)
                    off = 360 - off;
                n += off <= half;
            }
            if (n > out.second)
                out = {at(*aim), n};
        }
        return out;
    }
    // The foe that matters most: the one hurting us most (nearest our most hurt ally), else the nearest.
    const BattleFighter* threat() const
    {
        const BattleFighter* best = mark;
        double worst = -1;
        for (const auto* o : foes)
            for (const auto* a : allies)
                if (apart(*o, *a) == 1)
                    if (const double h = w.entity(a->id)->hurt; h > worst)
                    {
                        worst = h;
                        best = o;
                    }
        return best;
    }
};

// Before moving: fight-long Gifts and reactions as the fight begins, then the casts made from where one stands.
bool smartBefore(Turn& t)
{
    const auto& g = t.e->gift;
    const bool q = t.e->quickened;
    if (!t.mark)
        return false;
    if (q && g == "earth" && !t.me->magic.has("stone_armor") && t.ok("stone_armor"))
        t.use("stone_armor", "");
    if (q && g == "water" && !t.me->magic.has("water_screen"))
        for (const auto* o : t.foes)
            if (const auto* oe = t.w.entity(o->id); oe->gift == "fire" && oe->quickened && t.ok("water_screen"))
            {
                t.use("water_screen", "");
                break;
            }
    if (!q && g == "blinker")
    {
        if (!t.me->magic.armed.count("slip") && t.ok("slip"))
            t.use("slip", "");
        if (!t.allies.empty() && !t.me->magic.armed.count("interpose") && t.ok("interpose"))
            t.use("interpose", "");
    }
    if (!t.live() || !q)
        return false;
    const auto& m = *t.mark;
    // With a side already fighting, it casts from afar only when it couldn't reach a foe this turn anyway.
    const int d = !t.allies.empty() && t.canStrike() && t.d > t.reach ? 99 : t.d;
    {
        if (g == "fire")
        {
            if (tactics)
            {
                const auto [aim, n] = t.bestCone(5, 33);
                if (n >= 1 && (d >= 2 || n >= 2) && t.ok("flamethrower") && t.use("flamethrower", aim))
                    return true;
            }
            else if (d >= 2 && d <= 5 && !t.allyInCone(m.x, m.y, 5, 30) && t.ok("flamethrower") && t.use("flamethrower", at(m)))
                return true;
            if (d >= 2 && d <= 4 && !t.allyFirst(m.x, m.y, 4) && t.ok("heat_lance") && t.use("heat_lance", at(m)))
                return true;
        }
        if (g == "earth")
        {
            if (d >= 2 && d <= 10 && t.ok("hurl_stone") && t.use("hurl_stone", m.id))
                return true;
            if (d >= 2 && d <= 6 && t.ok("upheaval") && t.use("upheaval", at(m)))
                return true;
        }
        if (g == "water")
        {
            if (tactics)
                if (const auto [aim, n] = t.bestCone(4, 25); n >= 2 && t.ok("wave") && t.use("wave", aim))
                    return true;                    // (A wave into the pack: down and soaked, ready for Freeze.)
            if (d >= 2 && d <= 5 && !t.allyFirst(m.x, m.y, 5) && !t.engaged(m) && t.ok("pressure_jet") && t.use("pressure_jet", at(m)))
                return true;
            // Freeze: only worth it on a foe that must step in to strike.
            const int theirs = t.w.entity(m.id)->mouth == "sword" ? battle::SwordReach : 1;
            if (d > theirs && (m.magic.has("soaked") || t.b->groundAt(m.x, m.y, "water")) && t.ok("freeze") && t.use("freeze", m.id))
                return true;
        }
        // (Against a Seer, Wind saves itself for the grit up close: doc 45's counter.)
        const bool seer = t.w.entity(m.id)->gift == "seer" && t.w.entity(m.id)->quickened && !m.magic.has("dusted");
        if (g == "wind" && !seer)
            if (d >= 2 && d <= 5 && !t.allyFirst(m.x, m.y, 5) && !t.engaged(m) && t.ok("battering_gust") && t.use("battering_gust", at(m)))
                return true;
        if (g == "sound")
        {
            if (d <= 5 && m.magic.has("stone_armor") && t.ok("resonance") && t.use("resonance", m.id))
                return true;                        // (Its stone cracks first.)
            if (tactics)
            {
                const auto [aim, n] = t.bestCone(5, 28);
                if (n >= 1 && (d >= 2 || n >= 2) && t.ok("shatterhowl") && t.use("shatterhowl", aim))
                    return true;
            }
            else if (d >= 2 && d <= 5 && !t.allyInCone(m.x, m.y, 5, 30) && t.ok("shatterhowl") && t.use("shatterhowl", at(m)))
                return true;
        }
        if (g == "blinker")
        {
            if (t.finishable(6, 60) >= 2 && t.ok("chain_blink") && t.use("chain_blink", m.id))
                return true;
            if (d >= 2 && d <= 6 && t.ok("blink_strike") && t.use("blink_strike", m.id))
                return true;
        }
        if (g == "gravity")
        {
            // A Blinker is crushed first: under its own weight it can't blink (doc 45).
            if (t.w.entity(m.id)->gift == "blinker" && !m.magic.has("crushed") && d <= 5 && t.ok("crush") && t.use("crush", m.id))
                return true;
            if (d >= 2 && d <= 5 && t.ok("slam") && t.use("slam", m.id))
                return true;
        }
        if (g == "seer")
        {
            const auto* doom = t.threat();
            // Doom Mark: worth the action when two or more of the side are on that foe.
            if (doom && !doom->magic.has("doomed") && t.near(*doom, 1, false) >= 2 && t.ok("doom_mark") && t.use("doom_mark", doom->id))
                return true;
        }
        return false;
    }
    return false;
}

// A Gifted wolf's help (doc 45: it takes the turn's move, or its action once it has moved). True if it helped.
bool giftedHelp(Turn& t, bool action = false)
{
    if (!t.live() && !(t.me && t.me->acting && !t.me->moved && t.me->status == "fighting" && !t.b->over))
        return false;
    const auto& g = t.e->gift;
    const auto* hit = t.threat();
    if (g == "gravity")
    {
        for (const auto* o : t.down)
            if (apart(*t.me, *o) <= 5 && t.ok("lift_up") && t.use("lift_up", o->id))
                return true;
        (void)hit;                                  // (Burden: not worth its breath and mana yet.)
    }
    if (g == "fire")
    {
        // With the action, only when the bleeding would put them down; with the move, any bleeding.
        const auto worth = [&](const BattleFighter& o) {
            return o.bleeding >= 1 && (!action || t.w.entity(o.id)->hurt + 2.0 * o.bleeding >= 95);
        };
        if (worth(*t.me) && t.ok("cauterize") && t.use("cauterize", t.id))
            return true;
        for (const auto* o : t.allies)
            if (worth(*o) && apart(*t.me, *o) <= 1 && t.ok("cauterize") && t.use("cauterize", o->id))
                return true;
        for (const auto* o : action ? std::vector<const BattleFighter*>{} : t.foes)   // Flare: at a foe about to act.
            if (apart(*t.me, *o) <= 1 && (o->acting || o->meter >= 25) && t.ok("flare") && t.use("flare", o->id))
                return true;
    }
    if (g == "water")
    {
        // (Splash: a small help, for the move only.)
        for (const auto* o : t.allies)
            if (o->burning > 0 && apart(*t.me, *o) <= 3 && t.ok("douse") && t.use("douse", o->id))
                return true;
        if (!action)
            if (const auto* o = t.threat(); o && apart(*t.me, *o) <= 3 && !o->magic.has("splashed") && t.ok("splash_eyes") &&
                                             t.use("splash_eyes", o->id))
                return true;
        for (const auto* o : action ? std::vector<const BattleFighter*>{} : t.foes)
            if (apart(*t.me, *o) <= 1 && !o->magic.has("splashed") && t.ok("splash_eyes") && t.use("splash_eyes", o->id))
                return true;
    }
    if (g == "wind" && !action)
        for (const auto* o : t.foes)
            if (!o->magic.has("dusted") && apart(*t.me, *o) <= 3 && t.ok("air_blast") && t.use("air_blast", o->id))
                return true;
    if (g == "sound" && !action && t.me->magic.channel != "steady_beat" && (t.allies.empty() || t.near(*t.me, 3, false) >= 1) &&
        t.ok("steady_beat") && t.use("steady_beat", ""))
        return true;                                // (Kept through blows; held while it stands its ground.)
    if (g == "seer")
    {
        // The one a foe is on: the most hurt of the side next to a foe (oneself too).
        const BattleFighter* warn = nullptr;
        double worst = -1;
        for (const auto* a : t.allies)
            if (t.near(*a, 1, true) >= 1 && !a->magic.has("forewarned") && apart(*t.me, *a) <= 6)
                if (const double h = t.w.entity(a->id)->hurt; h > worst)
                {
                    worst = h;
                    warn = a;
                }
        if (warn && !action && t.ok("forewarn") && t.use("forewarn", warn == t.me ? t.id : warn->id))
            return true;
    }
    if (g == "earth" && !action)
    {
        for (const auto* a : t.allies)
            if (apart(*t.me, *a) <= 1 && t.near(*a, 1, true) >= 1 && !a->magic.has("firm") && t.ok("firm_footing") &&
                t.use("firm_footing", a == t.me ? t.id : a->id))
                return true;
    }
    return false;
}

// Next to the mark (after moving): the casts made up close, instead of a blow.
bool smartAfter(Turn& t)
{
    if (!t.live() || !t.mark || t.d > 1 || !t.e->quickened)
        return false;
    const auto& g = t.e->gift;
    const auto& m = *t.mark;
    if (g == "fire" && t.near(*t.me, 1, false) == 0 && t.ok("blastwave") && t.use("blastwave", ""))
        return true;
    if (g == "earth" && t.ok("upheaval") && t.use("upheaval", at(m)))
        return true;
    if (g == "water" && !t.allyFirst(m.x, m.y, 5) && !t.engaged(m) && t.ok("pressure_jet") && t.use("pressure_jet", at(m)))
        return true;
    if (g == "wind")
    {
        if (!m.magic.has("dusted") && !t.engaged(m) && t.ok("whirlwind") && t.use("whirlwind", ""))
            return true;
        if (!t.allyFirst(m.x, m.y, 5) && !t.engaged(m) && t.ok("battering_gust") && t.use("battering_gust", at(m)))
            return true;
    }
    if (g == "sound")
    {
        const auto* me2 = t.w.entity(m.id);
        if ((me2->mouth == "sword" || !me2->worn.empty()) && t.ok("resonance") && t.use("resonance", m.id))
            return true;
        if (t.near(*t.me, 2, true) >= 2 && t.near(*t.me, 2, false) == 0 && t.ok("thunderclap") && t.use("thunderclap", ""))
            return true;
        if (!t.allyInCone(m.x, m.y, 5, 30) && t.ok("shatterhowl") && t.use("shatterhowl", at(m)))
            return true;
    }
    if (g == "blinker")
    {
        if (t.finishable(6, 60) >= 2 && t.ok("chain_blink") && t.use("chain_blink", m.id))
            return true;
        if ((!tactics || t.foes.size() == 1) && t.ok("blink_strike") && t.use("blink_strike", m.id))
            return true;                            // (Outnumbered, with tactics, a foe beside it is bitten: a blink costs blood.)
        // Unmoor: a turn lost to one foe, for no blow and a nosebleed. Only worth it one on one (with tactics: doc 45).
        if ((!tactics || t.foes.size() == 1) && t.ok("unmoor") && t.use("unmoor", m.id))
            return true;
    }
    if (g == "gravity")
    {
        if (t.ok("slam") && t.use("slam", m.id))
            return true;
        if (!t.engaged(m) && t.ok("hurl") && t.use("hurl", m.id))
            return true;
    }
    // Riposte: when this wolf is the one the foes are on (alone, or no friend beside the foe to draw its blow).
    if (g == "seer" && t.me->magic.channel != "riposte" && t.near(m, 1, false) == 0 && t.ok("riposte") && t.use("riposte", ""))
        return true;
    return false;
}

// Naive: whatever of the Gift is ready and has a target (never past its mana), else a blow.
bool naiveTurn(Turn& t)
{
    if (!t.mark)
        return false;
    for (const auto& o : t.w.giftOptions(t.id))
        if (o.kind == "fightlong" && o.ready && !t.me->magic.has(o.id) && t.e->mana >= o.mana)
            t.use(o.id, "");
        else if (o.kind == "reaction" && o.ready && !t.me->magic.armed.count(o.id))
            t.use(o.id, "");
    if (!t.live())
        return false;
    std::vector<std::pair<std::string, std::string>> can;
    const auto& m = *t.mark;
    for (const auto& o : t.w.giftOptions(t.id))
    {
        if (!o.ready || o.kind == "passive" || o.kind == "fightlong" || o.kind == "reaction" || t.e->mana < o.mana + o.perTurn ||
            (!t.me->magic.channel.empty() && t.me->magic.channel == o.id))
            continue;
        std::string target;
        if (o.target == "foe")
            target = t.d <= o.range ? m.id : "";
        else if (o.target == "self")
            target = t.d <= std::max(1, o.range) || o.range == 0 ? "-" : "";
        else if (o.target == "ally")
            target = t.allies.empty() ? (o.id == "cauterize" && t.me->bleeding ? t.id : "") : t.allies[0]->id;
        else if (o.target == "downed")
            target = t.down.empty() ? "" : t.down[0]->id;
        else if (o.target == "dir" || o.target == "tile" || o.target == "any")
            target = t.d <= std::max(1, o.range) ? at(m) : "";
        else if (o.target == "shape")
        {
            // A run of tiles between the two, up to three.
            int x = t.me->x, y = t.me->y;
            for (int i = 0; i < 3 && test::apart(x, y, m.x, m.y) > 1; ++i)
            {
                x += (m.x > x) - (m.x < x);
                y += (m.y > y) - (m.y < y);
                target += (target.empty() ? "" : ";") + at(x, y);
            }
        }
        else if (o.target == "foe+tile" && t.d <= 1)
            target = m.id + "@" + at(m.x + 2 * ((m.x >= t.me->x) ? 1 : -1), m.y);
        if (!target.empty())
            can.push_back({o.id, target == "-" ? "" : target});
    }
    if (can.empty())
        return false;
    const auto& pick = can[std::hash<std::string>{}(t.id + std::to_string(t.b->turns)) % can.size()];
    return t.use(pick.first, pick.second);
}

// ------------------------------------------------------------------ Tactics (SIM_TACTICS)
//
// What a sensible player does with the ground and the turn, on top of its Gift: runs to close a long gap and walks
// once engaged (breath for the blows); goes for a foe gathering a Gift, else the most hurt it can reach (the side
// focuses); strikes from the side or behind where it can; keeps apart from its friends when the foe has a Gift that
// hits a crowd; and, outnumbered, stands where the fewest foes can reach it, striking and then stepping back.

int reachOf(World& w, const std::string& id) { return w.entity(id)->mouth == "sword" ? battle::SwordReach : 1; }

// How far a fighter could come and strike on its next turn, at a run.
int threatRange(World& w, const BattleFighter& o)
{
    const auto* e = w.entity(o.id);
    int move = battle::moveRange(e->dexterity, e->hurt, 7);
    if (e->gift == "wind" && e->quickened)
        move *= 2;                                  // (Tailwind.)
    if (o.magic.has("crushed"))
        move = std::min(move, 1);
    if (o.magic.has("frozen") || o.magic.has("prone") || o.magic.has("held") || o.casting)
        move = 0;
    return move + reachOf(w, o.id);
}

// How many foes of `side` could strike (x, y) next turn.
int threatsAt(World& w, const Battle& b, int side, int x, int y)
{
    int n = 0;
    for (const auto& o : b.fighters)
        if (o.side != side && o.status == "fighting" && !o.unseen && test::apart(o.x, o.y, x, y) <= threatRange(w, o))
            ++n;
    return n;
}

int standingOf(const Battle& b, int side)
{
    int n = 0;
    for (const auto& o : b.fighters)
        n += o.side == side && o.status == "fighting";
    return n;
}

// The foe to go for: one gathering a Gift that can be reached and struck this turn; else the most hurt that can be;
// else the nearest.
std::string tacticalMark(World& w, const std::string& id)
{
    const auto* b = w.battleOf(id);
    const auto* me = b->fighter(id);
    const int reach = reachOf(w, id);
    std::vector<std::pair<int, int>> from{{me->x, me->y}};
    if (!me->moved)
        for (const auto& t : w.battleReach(id))
            from.push_back(t);
    const BattleFighter *nearest = nullptr, *pick = nullptr;
    double score = -1e9;
    for (const auto& o : b->fighters)
    {
        if (o.side == me->side || o.status != "fighting" || o.unseen)
            continue;
        if (!nearest || test::apart(me->x, me->y, o.x, o.y) < test::apart(me->x, me->y, nearest->x, nearest->y))
            nearest = &o;
        bool can = false;
        for (const auto& [x, y] : from)
            can = can || test::apart(x, y, o.x, o.y) <= reach;
        if (!can)
            continue;
        const double s = (o.casting ? 1000 : 0) + w.entity(o.id)->hurt - test::apart(me->x, me->y, o.x, o.y) * .1;
        if (s > score)
        {
            score = s;
            pick = &o;
        }
    }
    return pick ? pick->id : nearest ? nearest->id : std::string();
}

// Runs to close a gap, walks once a foe can be struck after a walk.
void tacticalPace(World& w, const std::string& id)
{
    // (Tried for real: at a walk, can a foe be struck from somewhere it can get to?)
    const auto* b = w.battleOf(id);
    const auto* me = b->fighter(id);
    const auto* e = w.entity(id);
    w.setPace(id, 0);
    std::vector<std::pair<int, int>> tiles = w.battleReach(id);
    tiles.push_back({me->x, me->y});
    bool near = false;
    for (const auto& [x, y] : tiles)
        for (const auto& o : b->fighters)
            near = near || (o.side != me->side && o.status == "fighting" && test::apart(x, y, o.x, o.y) <= reachOf(w, id));
    w.setPace(id, near || e->stamina < 30 ? 0 : 7);
}

// Where to go this turn: a tile to strike the mark from (the side or back best), or as near it as can be; apart from
// friends when the foe's Gift hits a crowd; outnumbered, where the fewest foes can reach. Moves there; true if it
// moved.
bool tacticalMove(World& w, const std::string& id, const std::string& markId, bool stepBack = false)
{
    const auto* b = w.battleOf(id);
    const auto* me = b->fighter(id);
    const auto* mark = b->fighter(markId);
    if (!mark || me->moved)
        return false;
    const int side = me->side, reach = reachOf(w, id);
    const int mine = standingOf(*b, side), theirs = standingOf(*b, 1 - side);
    const bool lone = mine == 1 && theirs >= 2, outnumbered = mine < theirs;
    bool crowd = false;                             // (A foe whose Gift hits several: keep apart.)
    for (const auto& o : b->fighters)
        crowd = crowd || (o.side != side && o.status == "fighting" && w.entity(o.id)->quickened);
    std::vector<std::pair<int, int>> tiles = w.battleReach(id);
    const std::pair<int, int> here{me->x, me->y};
    tiles.push_back(here);
    const int threatsHere = threatsAt(w, *b, side, me->x, me->y);
    std::pair<int, int> best = here;
    double top = -1e18;
    for (const auto& [x, y] : tiles)
    {
        const int d = test::apart(x, y, mark->x, mark->y);
        const int threats = threatsAt(w, *b, side, x, y);
        int friends = 0;
        for (const auto& o : b->fighters)
            friends += o.side == side && o.id != id && o.status == "fighting" && test::apart(o.x, o.y, x, y) <= 1;
        double s;
        if (stepBack)
            s = -threats * 100.0 + (std::pair<int, int>{x, y} == here ? 50 : 0);   // (After the blow: away from the most.)
        else
        {
            if (d <= reach)
            {
                const int gap = battle::octantGap(mark->facing, battle::octant(x - mark->x, y - mark->y));
                s = 1000 + 40.0 * (gap >= 3 ? 2 : gap == 2 ? 1 : 0);
            }
            else
                s = -10.0 * d;
            s -= threats * (lone ? 60.0 : outnumbered ? 25.0 : 3.0);
            if (crowd)
                s -= 15.0 * friends;
            if (std::pair<int, int>{x, y} == here)
                s += 15;                            // (Staying put keeps the bar's head start.)
        }
        if (s > top)
        {
            top = s;
            best = {x, y};
        }
    }
    if (stepBack && threatsAt(w, *b, side, best.first, best.second) >= threatsHere)
        return false;
    // Stuck (no way nearer, the fallen in the way): a step aside, to come round another way.
    if (!stepBack && best == here && test::apart(me->x, me->y, mark->x, mark->y) > reach && tiles.size() > 1)
        best = tiles[std::hash<std::string>{}(id + std::to_string(b->turns)) % (tiles.size() - 1)];
    if (best == here)
        return false;
    return w.battleMove(id, best.first, best.second).ok;
}

// Ends a wolf's turn facing the nearest foe still standing (turning is free: doc 33), as a player would.
void endTurn(World& w, const std::string& id)
{
    const auto* b = w.battleOf(id);
    if (!b || b->over || !test::acting(b, id))
        return;
    const auto* me = b->fighter(id);
    const BattleFighter* near = nullptr;
    for (const auto& o : b->fighters)
        if (o.side != me->side && o.status == "fighting" && !o.unseen &&
            (!near || test::apart(me->x, me->y, o.x, o.y) < test::apart(me->x, me->y, near->x, near->y)))
            near = &o;
    if (near && me->status == "fighting" && (near->x != me->x || near->y != me->y))
        w.battleFace(id, battle::octant(near->x - me->x, near->y - me->y));
    if ((b = w.battleOf(id)) && !b->over && test::acting(b, id))
        w.battleAct(id, "wait");
}

// A wolf's turn: its Gift if it has one, then close on the nearest foe still standing, and strike when in reach.
void play(World& w, const std::string& id)
{
    const auto* b = w.battleOf(id);
    if (!b || b->over || !test::acting(b, id))
        return;
    const auto* me = b->fighter(id);
    if (!me->walk.empty())
        return;
    if (me->status != "fighting")
    {
        endTurn(w, id);
        return;
    }
    const auto& wolf = wolves[id];
    const bool gift = !wolf.gift.empty();
    // A player takes a moment to act, not the same each turn (with tactics): it blurs the turns' rhythm, as play does.
    if (tactics && !me->moved && !me->acted && !me->casting)
    {
        const double think = .2 + 1.3 * double(std::hash<std::string>{}(id + "|" + std::to_string(me->turnsTaken)) % 1000) / 1000;
        if (w.time() - me->turnStarted < think)
            return;
    }
    if (tactics && !me->moved && !me->acted)
        tacticalPace(w, id);
    // A Quickened wolf goes into a Trance when the odds are against it (SIM_TRANCE: never, outnumbered, always).
    static const std::string tranceWhen = std::getenv("SIM_TRANCE") ? std::getenv("SIM_TRANCE") : "outnumbered";
    if (wolf.quickened && !wolf.naive && me->magic.trance <= 0 && tranceWhen != "never" &&
        (tranceWhen == "always" || standingOf(*b, 1 - me->side) > standingOf(*b, me->side)))
        w.enterTrance(id);

    if (gift && !wolf.quickened && !wolf.naive)
    {
        Turn t(w, id);
        if (!me->moved && !me->acted)
            smartBefore(t);                         // (Reactions armed.)
        // Help where it is free: next to a foe already, the help takes the move and the blow is still to come.
        t.look();
        if (t.me && !t.me->moved && t.canStrikeHere())
            giftedHelp(t);
        else if (t.me && !t.me->moved && !t.canStrike())
        {
            // Nothing to strike this turn: walk up first, then help with the action.
        }
        else if (t.me && !t.me->moved && !t.down.empty() && wolf.gift == "gravity")
            giftedHelp(t);                          // (Lifting the fallen comes before closing in.)
    }
    if (gift && !me->acted && !me->casting)
    {
        Turn t(w, id);
        if (tactics && wolf.quickened && !wolf.naive)
            t.focus(tacticalMark(w, id));
        const bool cast = wolf.naive ? naiveTurn(t) : smartBefore(t);
        // A Gifted wolf with a side still walks up to the fight after helping (a support Gift takes the action, not the
        // move), unless it holds a Gift that moving would break.
        const bool walkOn = cast && !wolf.quickened && !t.allies.empty() && (b = w.battleOf(id)) && b->fighter(id) &&
                            !b->fighter(id)->moved && b->fighter(id)->magic.channel.empty();
        // Waiting for its moment to gather: it holds its ground (and its bar's head start) rather than closing in.
        // (With a side, it doesn't hang back: its friends would fight outnumbered.)
        if (!walkOn && (cast || (!wolf.naive && t.wanted && t.allies.empty() && t.d > 1 + (w.entity(id)->mouth == "sword"))))
        {
            if ((b = w.battleOf(id)) && !b->over && test::acting(b, id))
                endTurn(w, id);
            return;
        }
        b = w.battleOf(id);
        me = b ? b->fighter(id) : nullptr;
        if (!b || b->over || !me || !me->acting)
            return;
    }
    const bool sword = w.entity(id)->mouth == "sword";
    const int reach = sword ? battle::SwordReach : 1;
    const BattleFighter* mark = nullptr;
    for (const auto& o : b->fighters)
        if (o.side != me->side && o.status == "fighting" && !o.unseen &&
            (!mark || test::apart(me->x, me->y, o.x, o.y) < test::apart(me->x, me->y, mark->x, mark->y)))
            mark = &o;
    if (!mark)
    {
        endTurn(w, id);
        return;
    }
    for (const auto& o : b->fighters)
        if (o.side != me->side && o.status == "fighting" && !o.unseen && o.casting)
        {
            bool reachable = test::apart(me->x, me->y, o.x, o.y) <= reach;
            if (!reachable && !me->moved)
                for (const auto& [x, y] : w.battleReach(id))
                    reachable = reachable || test::apart(x, y, o.x, o.y) <= reach;
            if (reachable)
            {
                mark = &o;
                break;
            }
        }
    std::string markId = mark->id;
    if (tactics)
        if (const auto pick = tacticalMark(w, id); !pick.empty())
        {
            markId = pick;
            mark = b->fighter(pick);
        }
    // Holding Steady Beat (Gifted Sound): stay still, and strike only what comes.
    bool still = me->magic.channel == "steady_beat";
    if (still)
    {
        bool side = false, alone = true, foeNear = false;
        for (const auto& o : b->fighters)
            if (o.side == me->side && o.id != id && o.status == "fighting")
            {
                alone = false;
                side = side || test::apart(o.x, o.y, me->x, me->y) <= 3;
            }
            else if (o.side != me->side && o.status == "fighting")
                foeNear = foeNear || test::apart(o.x, o.y, me->x, me->y) <= 4;
        if (((!alone && !side) || foeNear) && !(test::apart(me->x, me->y, mark->x, mark->y) <= (w.entity(id)->mouth == "sword" ? 2 : 1)))
        {
            w.letGo(id);
            still = false;
        }
    }
    if (tactics && test::apart(me->x, me->y, mark->x, mark->y) > reach && !me->moved && !me->casting && !still)
    {
        tacticalMove(w, id, markId);
        if ((b = w.battleOf(id)) && b->fighter(id) && !b->fighter(id)->walk.empty())
            return;
    }
    else if (test::apart(me->x, me->y, mark->x, mark->y) > reach && !me->moved && !me->casting && !still)
    {
        // The tile in reach nearest any foe (the nearest is often walled in by the fallen); else, stuck, a step aside.
        std::pair<int, int> to{me->x, me->y};
        int best = test::apart(me->x, me->y, mark->x, mark->y);
        const auto tiles = w.battleReach(id);
        for (const auto& [x, y] : tiles)
            for (const auto& o : b->fighters)
                if (o.side != me->side && o.status == "fighting" && !o.unseen && (!mark->casting || o.id == mark->id))
                    if (const int d = test::apart(x, y, o.x, o.y); d < best)
                    {
                        best = d;
                        to = {x, y};
                        markId = o.id;
                    }
        if (to == std::pair<int, int>{me->x, me->y} && !tiles.empty())
            to = tiles[std::hash<std::string>{}(id + std::to_string(b->turns)) % tiles.size()];
        if (to != std::pair<int, int>{me->x, me->y})
            w.battleMove(id, to.first, to.second);
        if ((b = w.battleOf(id)) && b->fighter(id) && !b->fighter(id)->walk.empty())
            return;
    }
    if (gift && !wolf.naive && (b = w.battleOf(id)) && !b->over && test::acting(b, id))
    {
        Turn t(w, id);
        if (tactics && wolf.quickened)
            t.focus(markId);
        if (!t.me->acted && !t.me->casting)
        {
            if (wolf.quickened)
                smartAfter(t);
            else if (!t.canStrikeHere())
                giftedHelp(t, true);
        }
    }
    if ((b = w.battleOf(id)) && !b->over && test::acting(b, id) && !b->fighter(id)->acted)
    {
        me = b->fighter(id);
        const auto* m = b->fighter(markId);
        if (m && m->status == "fighting" && test::apart(me->x, me->y, m->x, m->y) <= reach)
            w.battleAct(id, sword ? "sword" : "bite", markId);
    }
    if (tactics && (b = w.battleOf(id)) && !b->over && test::acting(b, id))
    {
        me = b->fighter(id);
        const auto* e = w.entity(id);
        const bool tailwind = e->gift == "wind" && e->quickened;
        if (me->acted && !me->moved && !me->casting && me->magic.channel.empty() &&
            (standingOf(*b, me->side) < standingOf(*b, 1 - me->side) || tailwind) && tacticalMove(w, id, markId, true))
            return;                                 // (Struck, and stepping back out of the most foes' reach.)
    }
    if ((b = w.battleOf(id)) && !b->over && test::acting(b, id))
        endTurn(w, id);
}

// What a fight came to.
struct Outcome
{
    int result = 0;                                 // 1 if A stands, -1 if B does, 0 if neither side went down.
    int turns = 0;
    double health = 0;                              // The winning side's health left, all told.
    int downedB = 0;                                // Side B's wolves down at the end.
    double hurtB = 0;                               // Side B's health lost, all told.
};

// One fight: side A against side B, `gap` tiles apart, the first of A (or of B) starting it and striking first.
Outcome fight(const std::vector<Wolf>& a, const std::vector<Wolf>& b, int trial, bool aFirst, int gap = 1)
{
    World w;
    for (const auto& e : w.entities())
        if (e.second.npc)
            w.entity(e.first)->leaderId = "test-frozen";
    std::map<std::string, int> level;
    w.levelOf = [&](const std::string& id) { const auto l = level.find(id); return l == level.end() ? 1 : l->second; };
    std::vector<std::string> ids[2];
    wolves.clear();
    const auto make = [&](const Wolf& wolf, int side, int i) {
        const std::string id = std::string(side ? "player-b" : "player-a") + std::to_string(i) + "-" + std::to_string(trial);
        auto& e = w.addPlayer(id, id);
        level[id] = wolf.level;
        if (wolf.sword)
            e.mouth = "sword";
        e.hurt = wolf.hurt;
        if (wolf.armour)
            for (const auto& [slot, item] : {std::pair<const char*, const char*>{"body", "leather_barding"}, {"throat", "leather_gorget"},
                                            {"head", "leather_cap"}, {"paws", "leg_guards"}})
                if (std::string(slot) != "paws" || wolf.gift != "earth" || wolf.quickened)
                    e.worn[slot] = item;            // (A Gifted Earth wolf braces bare paws: it goes without leg guards.)
        if (!wolf.gift.empty())
        {
            w.giveGift(id, wolf.gift, wolf.quickened);
            if (wolf.gift == "water")
            {
                w.society().openAccount(id);
                w.society().create(id, "water", 1, "a waterskin");   // (Water's Tell: water to hand.)
            }
        }
        wolves[id] = wolf;
        ids[side].push_back(id);
        return &e;
    };
    auto* lead = make(a[0], 0, 0);
    auto* foe = make(b[0], 1, 0);
    foe->cellId = lead->cellId;
    // Which side stands west of the other alternates too (the ground isn't the same both ways).
    foe->position = {lead->position.x + ((trial / 2) % 2 ? -1 : 1) * (gap + .2), lead->position.y};
    const std::string first = aFirst ? ids[0][0] : ids[1][0];
    const std::string second = aFirst ? ids[1][0] : ids[0][0];
    const std::string cellId = lead->cellId;
    w.attack(first, second, "death");
    w.answerChallenge(second, true);
    auto* fightP = const_cast<Battle*>(w.battleOf(first));
    if (!fightP)
        return {};
    // The rest of each side, beside (behind) their first (put in directly: a fight between players is one to one to begin).
    for (int side = 0; side < 2; ++side)
        for (std::size_t i = 1; i < (side ? b : a).size(); ++i)
        {
            auto* e = make((side ? b : a)[i], side, int(i));
            e->cellId = cellId;
            const auto* near = fightP->fighter(ids[side][0]);
            const auto* other = fightP->fighter(ids[1 - side][0]);
            const int back = near->x < other->x ? -1 : 1;  // (Away from the other side.)
            BattleFighter f;
            f.id = e->id;
            f.side = near->side;
            f.facing = near->facing;
            bool placed = false;
            for (int r = 1; r <= 3 && !placed; ++r)
                for (int dy = -r; dy <= r && !placed; ++dy)
                    for (int dx : {back * r, 0, -back * r})
                        if (!placed && !(dx == 0 && dy == 0) && w.arenaOpen(*fightP, near->x + dx, near->y + dy))
                        {
                            f.x = near->x + dx;
                            f.y = near->y + dy;
                            placed = true;
                        }
            fightP->fighters.push_back(f);
        }
    test::takeGround(w, first);
    const int firstSide = aFirst ? 0 : 1;
    std::uint32_t seen = 0;
    for (double t = 0; t < 1800; t += .1)
    {
        const auto* now = w.battleOf(first);
        if (!now || now->over)
            break;
        // Several bars full at once (the first turn): the side that started acts first.
        for (int s : {firstSide, 1 - firstSide})
            for (const auto& id : ids[s])
                play(w, id);
        if (trace)
            for (const auto& l : now->log)
                if (l.seq > seen)
                {
                    seen = l.seq;
                    std::printf("    %.1f %s\n", t, l.text.c_str());
                }
        if (trace && std::fmod(t, 30) < .05)
        {
            std::printf("t=%.0f turns=%d:", t, now->turns);
            for (const auto& f : now->fighters)
                std::printf(" [%s s%d (%d,%d) %s hp%.0f st%.0f m%.0f%s]", f.id.substr(7, 3).c_str(), f.side, f.x, f.y, f.status.c_str(),
                            100 - w.entity(f.id)->hurt, w.entity(f.id)->stamina, f.meter, f.acting ? " ACT" : "");
            std::printf("\n");
        }
        w.tick(.1);
    }
    Outcome out;
    double hp[2] = {0, 0};
    bool up[2] = {false, false};
    for (int side = 0; side < 2; ++side)
        for (const auto& id : ids[side])
            if (const auto* e = w.entity(id); e && e->hurt < 100)
            {
                up[side] = true;
                hp[side] += 100 - e->hurt;
            }
    out.result = up[0] == up[1] ? 0 : up[0] ? 1 : -1;
    for (const auto& id : ids[1])
        if (const auto* e = w.entity(id))
        {
            out.downedB += e->hurt >= 100 || e->downedLeft > 0;
            out.hurtB += std::min(100.0, e->hurt);
        }
    out.health = out.result > 0 ? hp[0] : out.result < 0 ? hp[1] : 0;
    for (const auto& id : ids[0])
        if (const auto* bb = w.battleOf(id))
            out.turns = bb->turns;
    return out;
}

struct Rate
{
    double win = 0, turns = 0, health = 0, downedB = 0, hurtB = 0;
    int undecided = 0;
};

// Side A's wins of the fights decided: starts alternated (`order` 0), or always A (1) or always B (-1).
Rate winRate(const std::vector<Wolf>& a, const std::vector<Wolf>& b, int fights, int order = 0, int gap = 1)
{
    // Fights are independent worlds: spread over the machine's threads.
    std::vector<Outcome> out(std::max(0, fights));
    const int threads = std::max(1, std::min(fights, int(std::thread::hardware_concurrency())));
    std::vector<std::thread> pool;
    for (int k = 0; k < threads; ++k)
        pool.emplace_back([&, k] {
            for (int i = k; i < fights; i += threads)
                out[i] = fight(a, b, i, order == 0 ? i % 2 == 0 : order > 0, gap);
            std::lock_guard<std::mutex> hold(usedLock);
            for (const auto& [ab, n] : used)
                usedAll[ab] += n;
            used.clear();
        });
    for (auto& t : pool)
        t.join();
    int wins = 0, decided = 0;
    Rate r;
    for (const auto& o : out)
    {
        decided += o.result != 0;
        wins += o.result > 0;
        r.undecided += o.result == 0;
        r.turns += o.turns;
        r.health += o.health;
        r.downedB += o.downedB;
        r.hurtB += o.hurtB;
    }
    r.downedB /= std::max(1, fights);
    r.hurtB /= std::max(1, fights);
    r.win = decided ? 100.0 * wins / decided : 0;
    r.turns /= std::max(1, fights);
    r.health /= std::max(1, decided);
    return r;
}

bool details = false;
void row(const std::string& what, const Rate& r)
{
    std::printf("  %-58s %5.1f%%   %4.0f turns  %3.0f left  (B lost %3.0f)%s\n", what.c_str(), r.win, r.turns, r.health, r.hurtB,
                r.undecided ? ("  (" + std::to_string(r.undecided) + " undecided)").c_str() : "");
    if (details && !usedAll.empty())
    {
        std::string s = "      used:";
        for (const auto& [k, v] : usedAll)
            s += " " + k + " " + std::to_string(v);
        std::printf("%s\n", s.c_str());
    }
    usedAll.clear();
    std::fflush(stdout);
}
void row(const std::string& what, double rate) { row(what, Rate{rate}); }

std::string label(const Wolf& w)
{
    std::string s = w.gift.empty() ? "plain" : (w.quickened ? "Q-" : "G-") + w.gift;
    if (w.naive)
        s += "(naive)";
    if (w.sword)
        s += "+gear";
    return s + " L" + std::to_string(w.level);
}
std::string label(const std::vector<Wolf>& side)
{
    std::string s;
    for (const auto& w : side)
        s += (s.empty() ? "" : " + ") + label(w);
    return s;
}
void vs(const std::vector<Wolf>& a, const std::vector<Wolf>& b, int n, int gap = 6, int order = 0)
{
    row(label(a) + " vs " + label(b), winRate(a, b, n, order, gap));
}

Wolf parseWolf(const std::string& given, int level)
{
    // "fireq@20": a level of its own.
    std::string s = given;
    if (const auto at = s.find('@'); at != std::string::npos)
    {
        level = std::atoi(s.c_str() + at + 1);
        s = s.substr(0, at);
    }
    if (s == "plain")
        return Wolf{level};
    std::string f = s;
    bool q = false, nv = false, g = false;
    while (!f.empty() && (f.back() == 'q' || f.back() == '!' || f.back() == '+'))
    {
        if (f.back() == 'q' && f != "q")
            q = true;
        if (f.back() == '!')
            nv = true;
        if (f.back() == '+')
            g = true;
        f.pop_back();
    }
    Wolf w{level, g, g, f == "plain" ? "" : f, q, nv};
    return w;
}

void levels(int n)
{
    const Wolf L1{1}, L5{5}, L10{10}, L15{15}, L20{20}, L25{25};
    std::printf("Ungifted wolves, to the ground; %d fights each, who starts alternated unless said. Wins for the first named.\n", n);
    std::printf("\nLevels (a level: +1.5 fighting skill)\n");
    row("L1 vs L1", winRate({L1}, {L1}, n));
    row("L5 vs L1", winRate({L5}, {L1}, n));
    row("L10 vs L1", winRate({L10}, {L1}, n));
    row("L25 vs L1", winRate({L25}, {L1}, n));
    row("L10 vs L5", winRate({L10}, {L5}, n));
    row("L25 vs L20", winRate({L25}, {L20}, n));
    row("L25 vs L15", winRate({L25}, {L15}, n));
    std::printf("\nStriking first (tactics)\n");
    row("L1 vs L1, L1 strikes first", winRate({L1}, {L1}, n, 1));
    row("L1 vs L5, L1 strikes first", winRate({L1}, {L5}, n, 1));
    row("L1 vs L10, L1 strikes first", winRate({L1}, {L10}, n, 1));
    row("L1 vs L25, L1 strikes first", winRate({L1}, {L25}, n, 1));
    std::printf("\nGear\n");
    row("L1 in a leather kit vs L25 bare", winRate({{1, false, true}}, {L25}, n));
    row("L1 with a sword vs L25 bare", winRate({{1, true, false}}, {L25}, n));
    row("L1 with a sword and leather vs L25 bare", winRate({{1, true, true}}, {L25}, n));
    row("L1 with a sword and leather vs L25 the same", winRate({{1, true, true}}, {{25, true, true}}, n));
    std::printf("\nNumbers\n");
    row("two L1 vs one L25", winRate({L1, L1}, {L25}, n));
    row("two L5 vs one L15", winRate({L5, L5}, {L15}, n));
    row("two L1 vs two L25", winRate({L1, L1}, {L25, L25}, n));
    row("three L1 vs two L25", winRate({L1, L1, L1}, {L25, L25}, n));
}
} // namespace

int main(int argc, char** argv)
{
    const int n = argc > 1 ? std::atoi(argv[1]) : 300;
    const std::string suite = argc > 2 ? argv[2] : "levels";
    details = std::getenv("SIM_DETAILS") != nullptr;
    trace = std::getenv("SIM_TRACE") != nullptr;
    tactics = std::getenv("SIM_TACTICS") != nullptr;
    const int gap = std::getenv("SIM_GAP") ? std::atoi(std::getenv("SIM_GAP")) : 5;   // (5: neither side gains by waiting.)
    const auto has = [&](const char* s) { return suite == s || suite == "all"; };
    if (suite == "duel" || suite == "row")
    {
        // level_sim N duel fireq plain [level] [gap]; "a,b" for a side of several.
        const int level = argc > 5 ? std::atoi(argv[5]) : 1;
        const int g = argc > 6 ? std::atoi(argv[6]) : gap;
        const auto side = [&](const std::string& s) {
            std::vector<Wolf> out;
            std::size_t from = 0;
            while (from <= s.size())
            {
                const auto comma = s.find(',', from);
                out.push_back(parseWolf(s.substr(from, comma == std::string::npos ? std::string::npos : comma - from), level));
                if (comma == std::string::npos)
                    break;
                from = comma + 1;
            }
            return out;
        };
        details = true;
        vs(side(argv[3]), side(argv[4]), n, g);
        return 0;
    }
    if (has("levels"))
        levels(n);
    if (suite == "worth")
    {
        std::printf("What a head start in health is worth, L10, %d tiles apart\n", gap);
        for (int g : {2, 3, 4, 5})
        {
            row("bare, striking first, " + std::to_string(g) + " apart", winRate({Wolf{10}}, {Wolf{10}}, n, 1, g));
            row("armed, striking first, " + std::to_string(g) + " apart", winRate({armed(Wolf{10})}, {armed(Wolf{10})}, n, 1, g));
        }
        row("bare, striking first", winRate({Wolf{10}}, {Wolf{10}}, n, 1, gap));
        row("armed, striking first", winRate({armed(Wolf{10})}, {armed(Wolf{10})}, n, 1, gap));
        row("bare, striking first, side by side", winRate({Wolf{10}}, {Wolf{10}}, n, 1, 1));
        row("armed, striking first, side by side", winRate({armed(Wolf{10})}, {armed(Wolf{10})}, n, 1, 1));
        for (double h : {5.0, 10.0, 15.0, 20.0, 25.0, 30.0, 40.0})
        {
            Wolf hurtBare{10}, hurtArmed = armed(Wolf{10});
            hurtBare.hurt = hurtArmed.hurt = h;
            row("bare, the foe down " + std::to_string(int(h)), winRate({Wolf{10}}, {hurtBare}, n, 0, gap));
            row("armed, the foe down " + std::to_string(int(h)), winRate({armed(Wolf{10})}, {hurtArmed}, n, 0, gap));
        }
    }
    if (has("gifts") || has("quickened"))
    {
        std::printf("\nQuickened (smart) vs a plain wolf of the same level, %d tiles apart; %d fights\n", gap, n);
        for (int level : {10})
            for (const auto& f : Families)
                vs({quick(f, level)}, {Wolf{level}}, n, gap);
        std::printf("\nQuickened vs Quickened (smart, L10)\n");
        for (std::size_t i = 0; i < Families.size(); ++i)
            for (std::size_t j = i + 1; j < Families.size(); ++j)
                vs({quick(Families[i], 10)}, {quick(Families[j], 10)}, n, gap);
    }
    if (suite == "core")
    {
        // The yardsticks (doc 45), bare and armed: each Quickened against a plain equal; each Gifted alone and in a pair.
        std::printf("Core, L10, %d tiles apart, %d fights. Targets: Quickened 70-80%%; Gifted alone 50-55%%, in a pair 55-65%% "
                    "(plain pair %s)\n", gap, n, "~50%");
        for (const auto& f : Families)
        {
            vs({quick(f, 10)}, {Wolf{10}}, n, gap);
            vs({armed(quick(f, 10))}, {armed(Wolf{10})}, n, gap);
        }
        for (const auto& f : Families)
        {
            vs({gifted(f, 10)}, {Wolf{10}}, n, gap);
            vs({armed(gifted(f, 10))}, {armed(Wolf{10})}, n, gap);
            vs({Wolf{10}, gifted(f, 10)}, {Wolf{10}, Wolf{10}}, n, gap);
            vs({armed(Wolf{10}), armed(gifted(f, 10))}, {armed(Wolf{10}), armed(Wolf{10})}, n, gap);
        }
        return 0;
    }
    if (suite == "ladder")
    {
        // A Quickened wolf ten levels over plain ones, levels 1 to 15: one against one, then one against three.
        for (int foes : {1, 3})
        {
            std::printf("\nA Quickened wolf at level N+10 against %s at level N: the Quickened wolf's wins (%d fights, %d tiles apart, bare)\n",
                        foes == 1 ? "one plain wolf" : "three plain wolves", n, gap);
            std::printf("  %-4s %8s", "N", "plain");
            for (const auto& f : Families)
                std::printf(" %8s", f.c_str());
            std::printf("\n");
            for (int level = 1; level <= 15; ++level)
            {
                const std::vector<Wolf> them(std::size_t(foes), Wolf{level});
                std::printf("  %-4d %7.1f%%", level, winRate({Wolf{level + 10}}, them, n, 0, gap).win);
                std::fflush(stdout);
                for (const auto& f : Families)
                {
                    std::printf(" %7.1f%%", winRate({quick(f, level + 10)}, them, n, 0, gap).win);
                    std::fflush(stdout);
                }
                std::printf("\n");
            }
        }
        return 0;
    }
    if (suite == "trance")
    {
        // A Quickened wolf ten levels over plain wolves: one, three and five of them (doc 45's Trance targets: 3v1 50%, 5v1 25%).
        std::printf("A Quickened wolf at level N+10 against plain wolves at level N (%d fights, %d tiles apart, bare; Trance %s)\n", n, gap,
                    std::getenv("SIM_TRANCE") ? std::getenv("SIM_TRANCE") : "outnumbered");
        std::printf("  %-12s", "");
        for (const auto& f : Families)
            std::printf(" %7s", f.substr(0, 7).c_str());
        std::printf("    mean\n");
        std::vector<int> levels = {1, 5, 10, 15};
        if (const char* only = std::getenv("SIM_LEVEL"))
            levels = {std::atoi(only)};
        for (int foes : {1, 2, 3, 4, 5})
            for (int level : levels)
            {
                std::printf("  %dv1, N=%-4d", foes, level);
                double sum = 0;
                for (const auto& f : Families)
                {
                    const double r = winRate({quick(f, level + 10)}, std::vector<Wolf>(std::size_t(foes), Wolf{level}), n, 0, gap).win;
                    sum += r;
                    std::printf(" %6.0f%%", r);
                    std::fflush(stdout);
                }
                std::printf("  %5.1f%%\n", sum / Families.size());
            }
        return 0;
    }
    if (suite == "parties")
    {
        // A Quickened wolf in a party against more: its Trance by the odds (doc 45). L20 Quickened, L10 plain wolves.
        std::printf("Parties: a Quickened L20 (Trance when outnumbered) and plain L10 friends against plain L10s (%d fights)\n", n);
        std::printf("  %-22s", "");
        for (const auto& f : Families)
            std::printf(" %7s", f.substr(0, 7).c_str());
        std::printf("    mean\n");
        for (const auto& [mine, theirs] : std::vector<std::pair<int, int>>{{2, 4}, {2, 6}, {3, 6}, {2, 2}})
        {
            std::printf("  %d (Q + %d) vs %d%-9s", mine, mine - 1, theirs, "");
            double sum = 0;
            for (const auto& f : Families)
            {
                std::vector<Wolf> side{quick(f, 20)};
                for (int i = 1; i < mine; ++i)
                    side.push_back(Wolf{10});
                const double r = winRate(side, std::vector<Wolf>(std::size_t(theirs), Wolf{10}), n, 0, gap).win;
                sum += r;
                std::printf(" %6.0f%%", r);
                std::fflush(stdout);
            }
            std::printf("  %5.1f%%\n", sum / Families.size());
        }
        return 0;
    }
    if (suite == "crowd")
    {
        // A Quickened wolf ten levels over two or three plain ones: wins, foes it puts down, and the damage it deals.
        for (int foes : {2, 3})
        {
            std::printf("\nA Quickened wolf at level N+10 against %d plain wolves at level N (%d fights, %d tiles apart, bare): "
                        "wins / foes downed / damage dealt\n", foes, n, gap);
            std::printf("  %-4s %16s", "N", "plain");
            for (const auto& f : Families)
                std::printf(" %16s", f.c_str());
            std::printf("\n");
            for (int level : {1, 5, 10, 15})
            {
                const std::vector<Wolf> them(std::size_t(foes), Wolf{level});
                const auto cell = [&](const Rate& r) { std::printf(" %4.0f%% %4.1f %5.0f", r.win, r.downedB, r.hurtB); std::fflush(stdout); };
                std::printf("  %-4d", level);
                cell(winRate({Wolf{level + 10}}, them, n, 0, gap));
                for (const auto& f : Families)
                    cell(winRate({quick(f, level + 10)}, them, n, 0, gap));
                std::printf("\n");
            }
        }
        return 0;
    }
    if (suite == "gifted2")
    {
        std::printf("Gifted, L10, %d tiles apart, %d fights: alone and as a partner, bare and armed\n", gap, n);
        row("plain pair (bare)", winRate({Wolf{10}, Wolf{10}}, {Wolf{10}, Wolf{10}}, n, 0, gap));
        row("plain pair (armed)", winRate({armed(Wolf{10}), armed(Wolf{10})}, {armed(Wolf{10}), armed(Wolf{10})}, n, 0, gap));
        for (const auto& f : Families)
        {
            vs({gifted(f, 10)}, {Wolf{10}}, n, gap);
            vs({armed(gifted(f, 10))}, {armed(Wolf{10})}, n, gap);
            vs({Wolf{10}, gifted(f, 10)}, {Wolf{10}, Wolf{10}}, n, gap);
            vs({armed(Wolf{10}), armed(gifted(f, 10))}, {armed(Wolf{10}), armed(Wolf{10})}, n, gap);
        }
        return 0;
    }
    if (suite == "quick2")
    {
        std::printf("Quickened, L10, %d tiles apart, %d fights: alone and as a partner, bare and armed\n", gap, n);
        for (const auto& f : Families)
        {
            vs({quick(f, 10)}, {Wolf{10}}, n, gap);
            vs({armed(quick(f, 10))}, {armed(Wolf{10})}, n, gap);
            vs({Wolf{10}, quick(f, 10)}, {Wolf{10}, Wolf{10}}, n, gap);
        }
        return 0;
    }
    if (suite == "qvq")
    {
        std::printf("Quickened vs Quickened, L10, %d tiles apart, %d fights (bare, then armed)\n", gap, n);
        for (bool gear : {false, true})
            for (std::size_t i = 0; i < Families.size(); ++i)
                for (std::size_t j = i + 1; j < Families.size(); ++j)
                {
                    Wolf a = quick(Families[i], 10), b = quick(Families[j], 10);
                    vs({gear ? armed(a) : a}, {gear ? armed(b) : b}, n, gap);
                }
        return 0;
    }
    if (suite == "wide")
    {
        std::printf("Wide check, %d tiles apart, %d fights\n", gap, n);
        std::printf("\nQuickened vs Quickened, L10 (bare)\n");
        for (std::size_t i = 0; i < Families.size(); ++i)
            for (std::size_t j = i + 1; j < Families.size(); ++j)
                vs({quick(Families[i], 10)}, {quick(Families[j], 10)}, n, gap);
        std::printf("\nA Quickened wolf against plain ones of higher level\n");
        for (const auto& f : Families)
        {
            vs({quick(f, 1)}, {Wolf{10}}, n, gap);
            vs({quick(f, 1)}, {Wolf{25}}, n, gap);
            vs({quick(f, 10)}, {Wolf{10}, Wolf{10}}, n, gap);
        }
        std::printf("\nGifted against Quickened, and mixed pairs (L10)\n");
        for (const auto& f : Families)
        {
            vs({gifted(f, 10)}, {quick(f, 10)}, n, gap);
            vs({quick("fire", 10), gifted(f, 10)}, {quick("fire", 10), Wolf{10}}, n, gap);
            vs({Wolf{10}, gifted(f, 10)}, {quick(f, 10), Wolf{10}}, n, gap);
        }
        std::printf("\nThree a side (L10)\n");
        const Wolf P{10};
        vs({P, P, P}, {P, P, P}, n, gap);
        for (const auto& f : Families)
            vs({P, P, gifted(f, 10)}, {P, P, P}, n, gap);
        for (const auto& f : Families)
            vs({P, P, quick(f, 10)}, {P, P, P}, n, gap);
        vs({P, gifted("gravity", 10), gifted("seer", 10)}, {P, P, P}, n, gap);
        vs({quick("fire", 10), gifted("water", 10), gifted("gravity", 10)}, {quick("earth", 10), quick("blinker", 10), P}, n, gap);
        return 0;
    }
    if (has("gifts") || has("adjacent"))
    {
        std::printf("\nStarting side by side (1 tile apart), L10\n");
        for (const auto& f : Families)
            vs({quick(f, 10)}, {Wolf{10}}, n, 1);
        for (const auto& f : Families)
            vs({gifted(f, 10)}, {Wolf{10}}, n, 1);
    }
    if (has("gifts") || has("gifted"))
    {
        std::printf("\nGifted (smart) vs a plain wolf of the same level, %d tiles apart\n", gap);
        for (int level : {10})
            for (const auto& f : Families)
                vs({gifted(f, level)}, {Wolf{level}}, n, gap);
    }
    if (has("gifts") || has("teams"))
    {
        std::printf("\nTeams (L10)\n");
        const Wolf P{10};
        vs({P, P}, {P, P}, n, gap);
        for (const auto& f : Families)
            vs({P, gifted(f, 10)}, {P, P}, n, gap);
        for (const auto& f : Families)
            vs({P, quick(f, 10)}, {P, P}, n, gap);
        for (const auto& f : Families)
            vs({P, P, gifted(f, 10)}, {P, P, P}, n, gap);
        for (const auto& f : Families)
            vs({quick("fire", 10), gifted(f, 10)}, {quick("fire", 10), P}, n, gap);
    }
    if (has("gifts") || has("gear"))
    {
        std::printf("\nWith gear on both sides (sword and leather), L10\n");
        for (const auto& f : Families)
            vs({armed(quick(f, 10))}, {armed(Wolf{10})}, n, gap);
        for (const auto& f : Families)
            vs({armed(gifted(f, 10))}, {armed(Wolf{10})}, n, gap);
    }
    if (has("gifts") || has("gaps"))
    {
        std::printf("\nLevel gaps: a Quickened L1 against a plain L10 and L25\n");
        for (const auto& f : Families)
        {
            vs({quick(f, 1)}, {Wolf{10}}, n, gap);
            vs({quick(f, 1)}, {Wolf{25}}, n, gap);
        }
    }
    if (has("gifts") || has("naive"))
    {
        std::printf("\nNaive (whatever is ready) vs a plain wolf, L10\n");
        for (const auto& f : Families)
            vs({naive(quick(f, 10))}, {Wolf{10}}, n, gap);
        for (const auto& f : Families)
            vs({naive(gifted(f, 10))}, {Wolf{10}}, n, gap);
    }
    return 0;
}
