// Turn-based fights in arenas (RatwBattle.h; Docs/Design/33-combat.md). World members, kept here.
#include "RatwBattle.h"
#include "RatwItems.h"
#include "RatwWorld.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <deque>
#include <limits>

namespace ratw
{
namespace
{
constexpr double Pi = 3.14159265358979323846;
const std::string GroundAccount = "ground:lost";    // Whatever lies on the ground is held here (goods only).

std::uint64_t roll(const std::string& a, std::int64_t b)
{
    return std::hash<std::string>{}(a) * 1099511628211ULL + std::uint64_t(b) * 2654435761ULL;
}
double chance(const std::string& a, std::int64_t b)
{
    return double(roll(a, b) % 10000) / 10000.0;
}
int tilesApart(int ax, int ay, int bx, int by)
{
    return std::max(std::abs(ax - bx), std::abs(ay - by));
}

// Where a blow from `f` lands on `t` (a hit zone rolled by the side it comes at), and what gets through the armour
// there: the damage, and the words for the log (" on the throat, the steel gorget taking 4").
struct Landed
{
    double damage = 0;
    std::string words;
    std::string zone;                   // Where it fell ("head", "throat", "body", "legs"): the armour there wears.
};
Landed land(const BattleFighter& f, const BattleFighter& t, const Entity& d, double damage, const std::string& type, std::int64_t key);

std::string whole(double n)
{
    return std::to_string(std::max(1L, std::lround(n)));
}
bool takesTurns(const BattleFighter& f)
{
    return f.status == "fighting" || f.status == "downed";
}
} // namespace

namespace battle
{
const std::vector<HitZone>& hitZones(int quarter)
{
    // Placeholder weights (doc 35, Part 8). Head on: the face, the throat, a shoulder, a foreleg; from the side: the
    // flank most; from behind: the back, the haunch, a hind leg, rarely the scruff.
    static const std::vector<HitZone> front{{"head", "the face", 30}, {"throat", "the throat", 30}, {"body", "the shoulder", 25}, {"legs", "a foreleg", 15}};
    static const std::vector<HitZone> side{{"head", "the head", 15}, {"throat", "the neck", 15}, {"body", "the flank", 45}, {"legs", "a leg", 25}};
    static const std::vector<HitZone> back{{"throat", "the scruff", 10}, {"body", "the back", 30}, {"body", "the haunch", 20}, {"legs", "a hind leg", 40}};
    return quarter >= 2 ? back : quarter == 1 ? side : front;
}

std::string armourZone(const std::string& catalogSlot)
{
    return catalogSlot == "paws" ? "legs" : catalogSlot == "throat" || catalogSlot == "head" || catalogSlot == "body" ? catalogSlot : std::string();
}

namespace
{
const items::Item* armourOn(const Entity& e, const std::string& zone)
{
    const items::Item* best = nullptr;
    for (const auto& [slot, id] : e.worn)
        if (const auto* item = items::wearable(id); item && item->protect > 0 && armourZone(item->slot) == zone && (!best || item->protect > best->protect))
            best = item;
    return best;
}
} // namespace

int armourAt(const Entity& e, const std::string& zone, const std::string& type)
{
    const auto* item = armourOn(e, zone);
    return !item ? 0 : item->protect + (type == "cut" ? item->vsCut : type == "thrust" ? item->vsThrust : type == "blunt" ? item->vsBlunt : 0);
}

std::string armourPieceAt(const Entity& e, const std::string& zone)
{
    const auto* item = armourOn(e, zone);
    return item ? item->name : std::string();
}

int armourDex(const Entity& e)
{
    int dex = 0;
    for (const auto& [slot, id] : e.worn)
        if (const auto* item = items::wearable(id))
            dex += std::min(0, item->dex);
    return dex;
}

double throughArmour(const Entity& target, const std::string& zone, double damage, const std::string& type, int pierce)
{
    const int armour = std::max(0, armourAt(target, zone, type) - pierce);
    return armour <= 0 ? damage : std::max(damage * ArmourFloor, damage - armour);
}

double expectedThrough(const Entity& target, int quarter, double damage, const std::string& type, int pierce)
{
    double sum = 0, weights = 0;
    for (const auto& z : hitZones(quarter))
    {
        sum += z.weight * throughArmour(target, z.zone, damage, type, pierce);
        weights += z.weight;
    }
    return weights > 0 ? sum / weights : damage;
}

int moveRange(double dexterity, double hurt, int pace)
{
    return std::max(1, int(std::floor((3 + dexterity / 25) * injuryFactor(hurt) * paceFactor(pace))));
}

double staminaPerTurn(double hurt, double strength)
{
    return (4 + strength / 10) * (hurt < 25 ? 1 : hurt < 50 ? .75 : .5);
}

int octant(double dx, double dy)
{
    if (dx == 0 && dy == 0)
        return 0;
    return (int(std::lround(std::atan2(dy, dx) / (Pi / 4))) % 8 + 8) % 8;
}

Temperament temperament(const std::string& role, bool bandit, int age, bool npc)
{
    Temperament t;
    if (!npc)
        return t;                                   // Players choose for themselves.
    if (bandit)
        t = {55, "aggressive", 30};
    else if (role == "guard")
        t = {70, "aggressive", 10};
    else if (role == "forager" || role == "gatherer" || role == "carter" || role == "water carrier")
        t = {40, "cautious", 50};
    else if (role == "cook")
        t = {25, "timid", 70};
    else
        t = {30, "timid", 60};                      // Farmers, merchants, residents.
    if (age < YoungestFighter)
    {
        t.kind = "timid";
        t.fleeBelow = 101;                          // The young never fight: they run.
    }
    else if (age < 18)
        t.skill -= 10;
    else if (age >= 65)
    {
        t.skill -= 15;
        t.fleeBelow += 10;
    }
    return t;
}
} // namespace battle

namespace
{
Landed land(const BattleFighter& f, const BattleFighter& t, const Entity& d, double damage, const std::string& type, std::int64_t key)
{
    const auto& zones = battle::hitZones(battle::quarterOf(battle::octantGap(t.facing, battle::octant(f.x - t.x, f.y - t.y))));
    double total = 0;
    for (const auto& z : zones)
        total += z.weight;
    double roll = chance(f.id + "|zone|" + t.id, key) * total;
    const battle::HitZone* hit = &zones.back();
    for (const auto& z : zones)
        if ((roll -= z.weight) < 0)
        {
            hit = &z;
            break;
        }
    Landed out;
    out.zone = hit->zone;
    out.damage = battle::throughArmour(d, hit->zone, damage, type);
    out.words = std::string(" on ") + hit->part;
    if (const long taken = std::lround(damage) - std::lround(out.damage); taken >= 1)
    {
        auto piece = battle::armourPieceAt(d, hit->zone);
        if (!piece.empty())
            piece[0] = char(std::tolower(static_cast<unsigned char>(piece[0])));
        out.words += ", the " + piece + " taking " + std::to_string(taken);
    }
    return out;
}
} // namespace

// ------------------------------------------------------------------ Looking things up

const Battle* World::battle(const std::string& battleId) const
{
    for (const auto& b : battles_)
        if (b.id == battleId)
            return &b;
    return nullptr;
}

Battle* World::battleById(const std::string& battleId)
{
    return const_cast<Battle*>(static_cast<const World&>(*this).battle(battleId));
}

const Battle* World::battleOf(const std::string& id) const
{
    for (const auto& b : battles_)
        if (const auto* f = b.fighter(id); f && f->status != "fled")
            return &b;
    return nullptr;
}

Battle* World::battleFor(const std::string& id)
{
    return const_cast<Battle*>(battleOf(id));
}

const Battle* World::watching(const std::string& id) const
{
    for (const auto& b : battles_)
        if (b.observers.count(id))
            return &b;
    return nullptr;
}

const Challenge* World::challengeTo(const std::string& player) const
{
    for (const auto& c : challenges_)
        if (c.to == player)
            return &c;
    return nullptr;
}

bool World::downed(const std::string& id) const
{
    const auto* e = entity(id);
    return e && !e->dead && e->downedLeft > 0;
}

bool World::recoveryAvailable(const Entity& e) const
{
    return e.recoveryUsed < std::floor(calendarDays_);
}

battle::Temperament World::temperamentOf(const Entity& e) const
{
    const auto* job = society_.jobOf(e.id);
    const auto folk = folk_.find(e.id);
    // A Dev Console fight's allies stand and fight like the watch.
    const std::string role = folk != folk_.end() && folk->second.kind == "ally" ? "guard" : job ? job->role : std::string();
    auto t = battle::temperament(role, folk != folk_.end() && folk->second.kind == "bandit", e.age, e.npc);
    if (folk != folk_.end() && folk->second.skill >= 0)
        t.skill = folk->second.skill;
    if (!e.npc)
        t.skill = e.fightingSkill;                  // A player's own, grown by fighting.
    return t;
}

// ------------------------------------------------------------------ Starting and joining

Result World::attack(const std::string& attacker, const std::string& target, const std::string& terms)
{
    const auto* a = entity(attacker);
    const auto* t = entity(target);
    if (const auto why = tooLoadedToFight(attacker); !why.empty())
        return {false, why, target};                // (Carrying too much, doc 35.)
    if (a && t && !a->npc && !t->npc)
        return challenge(attacker, target, terms);  // Between players: only with the other's yes.
    return startBattle(attacker, target, false);
}

Result World::testFight(const std::string& player)
{
    auto* p = entity(player);
    if (!p || p->npc || p->dead)
        return {false, "No such character.", {}};
    if (p->downedLeft > 0)
        return {false, "You are down.", {}};
    if (inBattle(player))
        return {false, "You are already in a fight.", {}};
    if (custodyOf(player))
        return {false, "You are held in the gaol.", {}};
    if (!ensureLoaded(p->cellId).ok || !cell(p->cellId))
        return {false, "This place isn't loaded.", {}};
    // The bandit starts beside them (a fight begins between two within reach), then goes to the far side.
    Vec2 beside{-1, -1};
    for (int ring = 1; ring <= 2 && beside.x < 0; ++ring)
        for (int dy = -ring; dy <= ring && beside.x < 0; ++dy)
            for (int dx = -ring; dx <= ring && beside.x < 0; ++dx)
            {
                const Vec2 at{std::floor(p->position.x) + dx + .5, std::floor(p->position.y) + dy + .5};
                if (std::max(std::abs(dx), std::abs(dy)) == ring && standable(p->cellId, at))
                    beside = at;
            }
    if (beside.x < 0)
        return {false, "There is no open ground around you for a fight.", {}};
    int n = 0;
    for (const auto& c : roads_.camps)
        n = std::max(n, testCamp(c.id) ? std::atoi(c.id.c_str() + 12) : 0);
    BanditCamp camp{"camp_dmtest_" + std::to_string(n + 1), p->cellId, 1, 0, calendarDays_, true};
    camp.x = beside.x;
    camp.y = beside.y;
    roads_.camps.push_back(camp);
    const auto id = "road:" + camp.id + ":0";
    auto& bandit = addRoadFolk(id, "a ragged bandit", "Thin, nervous and new to the road, with a rusted blade and more hunger than sense.",
                               p->cellId, beside, "bandit", camp.id);
    bandit.offstage = false;
    bandit.age = 20;
    folk_[id].hp = 10;
    folk_[id].skill = 30;
    const auto dropCamp = [&] {
        removeRoadFolk(id);
        roads_.camps.erase(std::remove_if(roads_.camps.begin(), roads_.camps.end(), [&](const BanditCamp& c) { return c.id == camp.id; }),
                           roads_.camps.end());
    };
    const auto started = startBattle(id, player, false);
    auto* b = started.ok ? battleFor(player) : nullptr;
    auto* mine = b ? b->fighter(player) : nullptr;
    auto* theirs = b ? b->fighter(id) : nullptr;
    if (!b || !mine || !theirs)
    {
        if (b)
            b->fighters.erase(std::remove_if(b->fighters.begin(), b->fighters.end(), [&](const BattleFighter& f) { return f.id == id; }),
                              b->fighters.end());
        dropCamp();
        return started.ok ? Result{false, "The fight could not be set up.", {}} : started;
    }
    // The far side: the farthest tile the player could walk to whose straight line to them is open all the way (no
    // corner cut), so the bandit, stepping toward them, has a clear way and has to close the distance.
    const auto clearLine = [&](int x, int y) {
        const int dx = std::abs(mine->x - x), dy = std::abs(mine->y - y), sx = mine->x > x ? 1 : -1, sy = mine->y > y ? 1 : -1;
        int err = dx - dy;
        while (x != mine->x || y != mine->y)
        {
            const int e2 = 2 * err, px = x, py = y;
            if (e2 > -dy)
                err -= dy, x += sx;
            if (e2 < dx)
                err += dx, y += sy;
            if (x == mine->x && y == mine->y)
                break;
            if (!arenaOpen(*b, x, y, theirs->id) || (x != px && y != py && (!arenaOpen(*b, x, py, theirs->id) || !arenaOpen(*b, px, y, theirs->id))))
                return false;
        }
        return true;
    };
    BattleFighter probe = *mine;
    std::pair<int, int> far{theirs->x, theirs->y};
    int best = tilesApart(mine->x, mine->y, far.first, far.second);
    for (const auto& [x, y] : reachFrom(*b, probe, b->w + b->h))
        if (const int d = tilesApart(mine->x, mine->y, x, y); d > best && clearLine(x, y))
        {
            best = d;
            far = {x, y};
        }
    theirs->x = far.first;
    theirs->y = far.second;
    theirs->lineupX = far.first + .5;
    theirs->lineupY = far.second + .5;
    theirs->facing = battle::octant(mine->x - theirs->x, mine->y - theirs->y);
    mine->facing = battle::octant(theirs->x - mine->x, theirs->y - mine->y);
    if (auto* e = entity(id))
    {
        e->position = {far.first + .5, far.second + .5};
        e->facing = theirs->facing * Pi / 4;
        e->turnTarget = e->facing;
    }
    for (auto& c : roads_.camps)
        if (c.id == camp.id)
            c.x = far.first + .5, c.y = far.second + .5;
    return {true, "A ragged bandit comes at you from across the ground, " + std::to_string(best) + " strides off. A fight!", id};
}

Result World::testFightTeam(const std::string& player)
{
    // The one-bandit test fight first: its arena, its camp and its leader across the ground.
    auto started = testFight(player);
    if (!started.ok)
        return started;
    auto* b = battleFor(player);
    auto* mine = b ? b->fighter(player) : nullptr;
    auto* leader = b ? b->fighter(started.targetId) : nullptr;
    const auto folk = folk_.find(started.targetId);
    if (!b || !mine || !leader || folk == folk_.end())
        return {false, "The fight could not be set up.", {}};
    const std::string campId = folk->second.of;
    const int ourSide = mine->side, theirSide = leader->side;
    std::uint64_t seed = roll(player + "|" + campId, std::int64_t(time_ * 1000));
    const auto pick = [&](std::size_t n) {
        seed = seed * 6364136223846793005ULL + 1442695040888963407ULL;
        return std::size_t((seed >> 33) % n);
    };
    // The nearest open tile to (x, y) in the arena, for one joining beside a friend.
    const auto openNear = [&](int x, int y, const std::string& id) -> std::pair<int, int> {
        for (int ring = 1; ring <= 6; ++ring)
            for (int dy = -ring; dy <= ring; ++dy)
                for (int dx = -ring; dx <= ring; ++dx)
                    if (std::max(std::abs(dx), std::abs(dy)) == ring && arenaOpen(*b, x + dx, y + dy, id))
                        return {x + dx, y + dy};
        return {-1, -1};
    };
    const auto join = [&](const std::string& id, int side, int nearX, int nearY) -> bool {
        const auto [x, y] = openNear(nearX, nearY, id);
        if (x < 0)
        {
            removeRoadFolk(id);
            return false;
        }
        auto* e = entity(id);
        e->position = {x + .5, y + .5};
        e->offstage = false;
        enterBattle(*b, id, side, true);
        auto& f = b->fighters.back();
        f.x = x;
        f.y = y;
        f.lineupX = x + .5;
        f.lineupY = y + .5;
        stop(id);
        return true;
    };
    const auto looks = [&](Entity& e) {
        static const char* species[] = {"timber", "timber", "arctic", "red", "maned", "ethiopian"};
        e.appearance.species = species[pick(6)];
        e.appearance.sex = pick(2) ? "female" : "male";
        e.appearance.stature = pick(3) == 0 ? "tall" : "average";
        e.appearance.baseColor = int(pick(CoatColorCount));
        e.appearance.markingColor = int(pick(CoatColorCount));
        e.appearance.gradientColor = int(pick(CoatColorCount));
    };
    // Two more of the band, beside their leader: as weak as the first.
    int bandits = 1;
    for (int i = 1; i <= 2; ++i)
    {
        const auto id = "road:" + campId + ":" + std::to_string(i);
        auto& e = addRoadFolk(id, i == 1 ? "a scrawny bandit" : "a limping bandit",
                              i == 1 ? "All ribs and bluster, with a stolen knife held wrong." : "Favouring one leg, and hoping the others do the work.",
                              b->cellId, Vec2{leader->x + .5, leader->y + .5}, "bandit", campId);
        e.age = 19 + int(pick(15));
        e.strength = 35;
        e.dexterity = 40;
        looks(e);
        folk_[id].hp = 10;
        folk_[id].skill = 25;
        bandits += join(id, theirSide, leader->x, leader->y);
    }
    // Two passers-by who take the player's side: names and looks made up for the fight.
    static const char* given[] = {"Bram", "Tamsin", "Rook", "Ysolde", "Hale", "Wren", "Corin", "Maren", "Oswin", "Sable", "Fenna", "Garrick"};
    static const char* family[] = {"Ashcoat", "Thornridge", "Stonefell", "Larchwood", "Greymantle", "Copperfen", "Redbrook", "Holloway"};
    std::vector<std::string> allies;
    for (int i = 0; i < 2; ++i)
    {
        const auto id = "road:" + campId + ":ally" + std::to_string(i);
        const std::string name = std::string(given[pick(12)]) + " " + family[pick(8)];
        auto& e = addRoadFolk(id, name, "A passer-by who saw the bandits come at you, and chose your side.", b->cellId,
                              Vec2{mine->x + .5, mine->y + .5}, "ally", campId);
        e.age = 22 + int(pick(30));
        e.strength = 55;
        e.dexterity = 55;
        looks(e);
        folk_[id].skill = 55;
        if (join(id, ourSide, mine->x, mine->y))
            allies.push_back(name);
    }
    fitArena(*b);
    for (auto& f : b->fighters)
    {
        // Each faces the nearest of the other side.
        int best = std::numeric_limits<int>::max();
        for (const auto& o : b->fighters)
            if (o.side != f.side && tilesApart(f.x, f.y, o.x, o.y) < best)
            {
                best = tilesApart(f.x, f.y, o.x, o.y);
                f.facing = battle::octant(o.x - f.x, o.y - f.y);
            }
    }
    std::string with = allies.empty() ? "alone" : allies.size() == 1 ? "with " + allies[0] : "with " + allies[0] + " and " + allies[1];
    fightLine(*b, {}, {}, "join", std::to_string(bandits) + " bandits against you, " + with + " at your side.");
    return {true, std::to_string(bandits) + " weak bandits come at you, and you stand " + with + ". A fight!", started.targetId};
}

Result World::endFightInDraw(const std::string& player)
{
    auto* b = battleFor(player);
    if (!b)
        return {false, "You aren't in a fight.", {}};
    if (b->over)
        return {false, "The fight is already over.", {}};
    b->over = true;
    b->overAt = time_;
    b->truced = true;                               // As after a truce: nobody stays hostile.
    for (auto& f : b->fighters)
        f.acting = false;
    b->banner = "The fight is called a draw";
    fightLine(*b, {}, {}, "over", b->banner + ".");
    return {true, "You call the fight a draw.", {}};
}

Result World::startBattle(const std::string& attacker, const std::string& target, bool pvp)
{
    auto* a = entity(attacker);
    auto* t = entity(target);
    if (!a || a->dead)
        return {false, "No such character.", {}};
    if (a->downedLeft > 0)
        return {false, "You are down.", {}};
    if (custodyOf(attacker))
        return {false, "You are held in the gaol.", {}};
    if (!t || t->id == attacker || (t->transient && !campOf(target)))
        return {false, "There is no call to fight them.", target};
    if (t->dead || t->downedLeft > 0)
        return {false, t->name + " is already down.", target};
    if (a->age < battle::YoungestFighter)
        return {false, "You are too young to fight.", target};
    if (t->age < battle::YoungestFighter)
        return {false, "You can't bring yourself to fight one so young.", target};
    if (a->cellId != t->cellId || std::hypot(a->position.x - t->position.x, a->position.y - t->position.y) > battle::StartReach)
        return {false, "Get closer first.", target};
    if (const auto settle = settleUntil_.find(attacker); settle != settleUntil_.end() && time_ < settle->second)
        return {false, "You are still finding your feet after that fight.", target};
    for (const auto& b : battles_)
        if (!b.over && b.fled.count(attacker) && b.fighter(target) && b.fighter(target)->status != "fled")
            return {false, "You fled from that fight; you can't take it up again until it is over.", target};
    if (inBattle(attacker))
        return {false, "You are already in a fight.", target};
    if (auto* existing = battleFor(target))
    {
        const auto* f = existing->fighter(target);
        return joinBattle(attacker, existing->id, 1 - f->side);   // Into the fight they're in, against them.
    }
    leaveObserving(attacker);
    Battle b;
    b.id = "fight-" + std::to_string(++nextBattle_);
    b.cellId = a->cellId;
    b.pvp = pvp;
    // A bandit set on (or setting on): the camp's fight. A resident set on: an assault, a crime (RatwCrime.cpp).
    if (auto* camp = campOf(target) ? campOf(target) : campOf(attacker))
    {
        b.camp = camp->id;
        const auto& player = campOf(target) ? attacker : target;
        auto enc = std::find_if(encounters_.begin(), encounters_.end(), [&](const Encounter& e) { return e.camp == camp->id; });
        if (enc == encounters_.end())
            encounters_.push_back({camp->id, player, 0, time_, true});
        else
            enc->fighting = true;
    }
    else if (t->npc && !a->npc)
    {
        auto& inc = openIncident("assault", attacker, target);
        recordEvent({"assault", attacker, target, inc.cell, 0, 0, {}, 0, 0, inc.id});
        bonds_.change(target, attacker, {-20, -20, 2, 15, -5}, calendarDays_);
        witness(inc, 0);
        b.incident = inc.id;
    }
    enterBattle(b, attacker, 0, true);
    enterBattle(b, target, 1, true);
    // Their own come with them: the rest of a bandit band, companions who follow either of them; and a guard on duty
    // who sees a resident set on comes in against the one who started it.
    for (const Entity* other : entitiesIn(b.cellId))
    {
        if (!other || other->cellId != b.cellId || other->dead || other->downedLeft > 0 || b.fighter(other->id) ||
            inBattle(other->id) || other->age < battle::YoungestFighter)
            continue;
        int side = -1;
        if (!b.camp.empty())
            if (const auto* camp = campOf(other->id); camp && camp->id == b.camp)
                side = campOf(attacker) ? 0 : 1;
        if (side < 0 && other->npc && !other->leaderId.empty())
            side = other->leaderId == attacker ? 0 : other->leaderId == target ? 1 : -1;
        if (side < 0 && !b.incident.empty() && other->npc && guardOnDuty(other->id) && visionClarity(other->id, attacker) > 0)
            side = 1;
        if (side >= 0)
            enterBattle(b, other->id, side, false);
    }
    fitArena(b);
    for (auto& f : b.fighters)
    {
        // Each to the nearest open tile to where they stood.
        const auto* e = entity(f.id);
        const int sx = std::clamp(int(std::floor(e->position.x)), b.x0, b.x0 + b.w - 1);
        const int sy = std::clamp(int(std::floor(e->position.y)), b.y0, b.y0 + b.h - 1);
        f.x = sx;
        f.y = sy;
        if (!arenaOpen(b, sx, sy, f.id))
        {
            BattleFighter probe = f;
            for (const auto& [x, y] : reachFrom(b, probe, b.w + b.h))
            {
                f.x = x;
                f.y = y;
                break;
            }
        }
    }
    lineUp(b);
    for (auto& f : b.fighters)
    {
        // Each begins facing the nearest of the other side (after that a player turns only when they choose).
        int best = std::numeric_limits<int>::max();
        for (const auto& o : b.fighters)
            if (o.side != f.side && tilesApart(f.x, f.y, o.x, o.y) < best)
            {
                best = tilesApart(f.x, f.y, o.x, o.y);
                f.facing = battle::octant(o.x - f.x, o.y - f.y);
            }
    }
    for (const auto& f : b.fighters)
    {
        stop(f.id);
        setClientWalks(f.id, false);
        if (const auto* e = entity(f.id); e && !e->npc && f.id != attacker && !(pvp && f.id == target))
            notice(f.id, a->name + " comes at " + (f.id == target ? std::string("you") : t->name) + ". You are in a fight!");
    }
    b.opening = attacker;
    recordEvent({"fight", attacker, target, b.cellId, 0, 0, {}, 0, 0, b.id});
    fightLine(b, attacker, target, "start", a->name + " goes for " + t->name + ".");
    battles_.push_back(std::move(b));
    return {true, "You go for " + t->name + ". A fight!", target};
}

void World::enterBattle(Battle& b, const std::string& id, int side, bool full)
{
    const auto* e = entity(id);
    BattleFighter f;
    f.id = id;
    f.side = side;
    f.meter = full ? 100 : 0;
    f.readyAt = full ? time_ : -1;
    f.order = b.nextOrder++;
    f.lineupX = e ? e->position.x : 0;
    f.lineupY = e ? e->position.y : 0;
    f.facing = e ? battle::octant(std::cos(e->facing), std::sin(e->facing)) : 0;
    f.x = e ? int(std::floor(e->position.x)) : 0;
    f.y = e ? int(std::floor(e->position.y)) : 0;
    b.fighters.push_back(f);
    strainOnEntering(id);                           // Fighting on an unhealed injury sets it back (doc 38).
}

void World::fitArena(Battle& b)
{
    const auto* c = cell(b.cellId);
    if (!c)
        return;
    const int count = int(b.fighters.size());
    const int grow = std::max(0, count - battle::GrowthFrom);
    const int w = std::min(c->width, battle::ArenaWidth + grow), h = std::min(c->height, battle::ArenaHeight + grow);
    double cx = 0, cy = 0;
    if (b.w > 0)
    {
        cx = b.x0 + b.w / 2.0;
        cy = b.y0 + b.h / 2.0;
    }
    else if (count > 0)
    {
        for (const auto& f : b.fighters)
            if (const auto* e = entity(f.id))
            {
                cx += e->position.x / count;
                cy += e->position.y / count;
            }
    }
    int x0 = std::clamp(int(std::lround(cx - w / 2.0)), 0, c->width - w);
    int y0 = std::clamp(int(std::lround(cy - h / 2.0)), 0, c->height - h);
    // Everyone already in it stays in it.
    for (const auto& f : b.fighters)
    {
        if (b.w <= 0)
            break;
        x0 = std::min(x0, f.x);
        y0 = std::min(y0, f.y);
        x0 = std::max(x0, f.x - w + 1);
        y0 = std::max(y0, f.y - h + 1);
    }
    b.x0 = std::clamp(x0, 0, c->width - w);
    b.y0 = std::clamp(y0, 0, c->height - h);
    b.w = w;
    b.h = h;
}

bool World::arenaOpen(const Battle& b, int x, int y, const std::string& except) const
{
    if (!b.inArena(x, y) || !standable(b.cellId, {x + .5, y + .5}))
        return false;
    for (const auto& f : b.fighters)
        if (f.id != except && f.status != "fled" &&
            ((f.x == x && f.y == y) || (!f.walk.empty() && f.walk.back() == std::pair<int, int>{x, y})))
            return false;                           // (Where someone is walking to is theirs.)
    return true;
}

std::vector<std::pair<int, int>> World::walkTo(const Battle& b, const BattleFighter& f, int tx, int ty) const
{
    // Down the steps stepsTo counts, one tile at a time, the way it walks: eight ways, never cutting a corner.
    std::vector<std::pair<int, int>> out;
    const auto steps = stepsTo(b, tx, ty, f.id);
    const auto at = [&](int x, int y) { return b.inArena(x, y) ? steps[std::size_t((y - b.y0) * b.w + (x - b.x0))] : -1; };
    const auto open = [&](int x, int y) { return b.inArena(x, y) && arenaOpen(b, x, y, f.id); };
    int x = f.x, y = f.y;
    for (int left = at(x, y); left > 0;)
    {
        bool stepped = false;
        // Straight toward the goal first, then the others.
        const int gx = (tx > x) - (tx < x), gy = (ty > y) - (ty < y);
        const std::pair<int, int> order[] = {{gx, gy}, {gx, 0}, {0, gy}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}, {1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
        for (const auto& [dx, dy] : order)
        {
            const int nx = x + dx, ny = y + dy;
            if ((dx == 0 && dy == 0) || at(nx, ny) != left - 1 || !stepBetween(b.cellId, x, y, nx, ny))
                continue;
            if (dx != 0 && dy != 0 && (!open(x + dx, y) || !open(x, y + dy)) && !(nx == tx && ny == ty))
                continue;
            out.push_back({nx, ny});
            x = nx;
            y = ny;
            --left;
            stepped = true;
            break;
        }
        if (!stepped)
            return {};
    }
    return out;
}

double World::stepSeconds(const BattleFighter& f) const
{
    const auto* e = entity(f.id);
    if (f.status == "downed")
        return battle::CrawlStepSeconds;
    const double pace = e ? fightPace(*e) / 10.0 : .5;
    return (battle::StepSeconds + (battle::SprintStepSeconds - battle::StepSeconds) * pace) /
           std::max(.4, battle::injuryFactor(e ? e->hurt : 0));
}

int World::fightPace(const Entity& e) const
{
    return e.exhausted ? 0 : e.npc ? battle::NpcPace : e.pace;
}

void World::walkFighters(Battle& b)
{
    // Moves are walked: a tile at a time, facing the way of each step unless turned by hand since (doc 33).
    for (auto& f : b.fighters)
    {
        if (f.walk.empty())
            continue;
        if (b.over || (f.status != "fighting" && f.status != "downed"))
        {
            f.walk.clear();
            continue;
        }
        while (!f.walk.empty() && time_ >= f.stepAt)
        {
            const auto [nx, ny] = f.walk.front();
            bool blocked = false;
            for (const auto& o : b.fighters)
                if (o.id != f.id && o.status != "fled" && o.x == nx && o.y == ny)
                    blocked = true;                 // (Someone stepped into the way: it stops short.)
            if (blocked)
            {
                f.walk.clear();
                break;
            }
            if (!f.turned)
                f.facing = battle::octant(nx - f.x, ny - f.y);
            f.x = nx;
            f.y = ny;
            f.walk.erase(f.walk.begin());
            f.stepAt += stepSeconds(f);
        }
    }
}

std::vector<std::pair<int, int>> World::reachFrom(const Battle& b, const BattleFighter& f, int range) const
{
    // Steps of one tile in eight directions, never cutting a corner, around anyone standing (or lying) in the way.
    std::vector<std::pair<int, int>> out;
    if (b.w <= 0 || b.h <= 0)
        return out;
    std::vector<int> steps(std::size_t(b.w * b.h), -1);
    const auto at = [&](int x, int y) -> int& { return steps[std::size_t((y - b.y0) * b.w + (x - b.x0))]; };
    std::deque<std::pair<int, int>> open;
    if (!b.inArena(f.x, f.y))
        return out;
    at(f.x, f.y) = 0;
    open.push_back({f.x, f.y});
    while (!open.empty())
    {
        const auto [x, y] = open.front();
        open.pop_front();
        const int here = at(x, y);
        if (here >= range)
            continue;
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
            {
                const int nx = x + dx, ny = y + dy;
                if ((dx == 0 && dy == 0) || !b.inArena(nx, ny) || at(nx, ny) >= 0 || !arenaOpen(b, nx, ny, f.id) ||
                    !stepBetween(b.cellId, x, y, nx, ny))
                    continue;
                if (dx != 0 && dy != 0 && (!arenaOpen(b, x + dx, y, f.id) || !arenaOpen(b, x, y + dy, f.id)))
                    continue;
                at(nx, ny) = here + 1;
                out.push_back({nx, ny});
                open.push_back({nx, ny});
            }
    }
    return out;
}

std::vector<int> World::stepsTo(const Battle& b, int tx, int ty, const std::string& mover) const
{
    std::vector<int> steps(std::size_t(std::max(0, b.w * b.h)), -1);
    if (!b.inArena(tx, ty))
        return steps;
    const auto at = [&](int x, int y) -> int& { return steps[std::size_t((y - b.y0) * b.w + (x - b.x0))]; };
    // Open to walk through: free ground, or the mover's own tile; the goal itself is where the walk ends.
    const auto open = [&](int x, int y) { return b.inArena(x, y) && arenaOpen(b, x, y, mover); };
    std::deque<std::pair<int, int>> queue{{tx, ty}};
    at(tx, ty) = 0;
    while (!queue.empty())
    {
        const auto [x, y] = queue.front();
        queue.pop_front();
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
            {
                const int nx = x + dx, ny = y + dy;
                if ((dx == 0 && dy == 0) || !open(nx, ny) || at(nx, ny) >= 0 || !stepBetween(b.cellId, nx, ny, x, y))
                    continue;
                if (dx != 0 && dy != 0 && (!open(x + dx, y) || !open(x, y + dy)) && !(x == tx && y == ty))
                    continue;
                at(nx, ny) = at(x, y) + 1;
                queue.push_back({nx, ny});
            }
    }
    return steps;
}

void World::lineUp(Battle& b)
{
    // In the world the fighters stand frozen in two facing lines, one a side, where the fight began: if there is
    // room. Where there isn't, each stays where they stood, turned toward the other side.
    double mx = 0, my = 0;
    int n = 0;
    for (const auto& f : b.fighters)
        if (f.status != "fled")
        {
            mx += f.lineupX;
            my += f.lineupY;
            ++n;
        }
    if (n == 0)
        return;
    mx /= n;
    my /= n;
    std::vector<std::pair<const BattleFighter*, Vec2>> spots;
    bool fits = true;
    for (int side = 0; side < 2 && fits; ++side)
    {
        std::vector<const BattleFighter*> row;
        for (const auto& f : b.fighters)
            if (f.side == side && f.status != "fled")
                row.push_back(&f);
        for (std::size_t i = 0; i < row.size(); ++i)
        {
            const Vec2 p{mx + (double(i) - (double(row.size()) - 1) / 2), my + (side == 0 ? -1 : 1)};
            if (!standable(b.cellId, p))
            {
                fits = false;
                break;
            }
            spots.push_back({row[i], p});
        }
    }
    for (auto& f : b.fighters)
    {
        auto* e = entity(f.id);
        if (!e || f.status == "fled")
            continue;
        if (fits)
            for (const auto& [who, p] : spots)
                if (who == &f)
                {
                    e->position = p;
                    f.lineupX = p.x;
                    f.lineupY = p.y;
                }
        e->facing = f.side == 0 ? Pi / 2 : -Pi / 2;
        e->turnTarget = e->facing;
        e->velocity = {};
    }
}

Result World::joinBattle(const std::string& id, const std::string& battleId, int side)
{
    auto* b = battleById(battleId);
    auto* e = entity(id);
    if (!e || e->dead)
        return {false, "No such character.", {}};
    if (!b || b->over)
        return {false, "That fight is over.", {}};
    if (b->hunt && side != 0)
        return {false, "Join the hunters, not the game.", {}};
    if (const auto why = huntJoinRefusal(*b, id); !why.empty())
        return {false, why, {}};                    // Only a hunter's party or Chapter (doc 41).
    if (const auto why = tooLoadedToFight(id); !why.empty())
        return {false, why, {}};
    if (side != 0 && side != 1)
        return {false, "Choose a side.", {}};
    if (e->downedLeft > 0)
        return {false, "You are down.", {}};
    if (e->age < battle::YoungestFighter)
        return {false, "You are too young to fight.", {}};
    if (b->fighter(id) && b->fighter(id)->status != "fled")
        return {false, "You are already in that fight.", {}};
    if (b->fled.count(id))
        return {false, "You fled from that fight. You may only watch it now.", {}};
    if (b->observed.count(id))
        return {false, "You have watched this fight. You may only watch it now.", {}};
    if (inBattle(id))
        return {false, "You are already in a fight.", {}};
    if (e->cellId != b->cellId)
        return {false, "That fight is somewhere else.", {}};
    if (custodyOf(id))
        return {false, "You are held in the gaol.", {}};
    enterBattle(*b, id, side, false);
    fitArena(*b);
    auto& f = b->fighters.back();
    // In at the arena's edge nearest where they stood.
    const int sx = std::clamp(int(std::floor(e->position.x)), b->x0, b->x0 + b->w - 1);
    const int sy = std::clamp(int(std::floor(e->position.y)), b->y0, b->y0 + b->h - 1);
    int best = std::numeric_limits<int>::max();
    for (int y = b->y0; y < b->y0 + b->h; ++y)
        for (int x = b->x0; x < b->x0 + b->w; ++x)
            if (b->onEdge(x, y) && arenaOpen(*b, x, y, id))
                if (const int d = std::abs(x - sx) + std::abs(y - sy); d < best)
                {
                    best = d;
                    f.x = x;
                    f.y = y;
                }
    if (best == std::numeric_limits<int>::max())
    {
        b->fighters.pop_back();
        return {false, "There is no room to get into that fight.", {}};
    }
    {
        int nearest = std::numeric_limits<int>::max();
        for (const auto& o : b->fighters)
            if (o.side != side && o.status == "fighting" && tilesApart(f.x, f.y, o.x, o.y) < nearest)
            {
                nearest = tilesApart(f.x, f.y, o.x, o.y);
                f.facing = battle::octant(o.x - f.x, o.y - f.y);
            }
    }
    stop(id);
    setClientWalks(id, false);
    lineUp(*b);
    std::string sideOf;
    for (const auto& other : b->fighters)
        if (other.side == side && other.id != id)
            if (const auto* o = entity(other.id))
            {
                sideOf = o->name;
                break;
            }
    fightLine(*b, id, {}, "join", e->name + " joins the fight" + (sideOf.empty() ? "." : " on " + sideOf + "'s side."));
    for (const auto& other : b->fighters)
        if (const auto* o = entity(other.id); o && !o->npc && other.id != id && other.status != "fled")
            notice(other.id, e->name + " joins the fight.");
    return {true, "You join the fight.", {}};
}

Result World::observeBattle(const std::string& id, const std::string& battleId)
{
    auto* b = battleById(battleId);
    const auto* e = entity(id);
    if (!e || e->npc)
        return {false, "No such character.", {}};
    if (!b || b->over)
        return {false, "That fight is over.", {}};
    if (inBattle(id))
        return {false, "You are in that fight.", {}};
    if (e->cellId != b->cellId)
        return {false, "That fight is somewhere else.", {}};
    leaveObserving(id);
    b->observers.insert(id);
    b->observed.insert(id);
    return {true, "You watch the fight.", {}};
}

Result World::leaveObserving(const std::string& id)
{
    bool was = false;
    for (auto& b : battles_)
        was = b.observers.erase(id) > 0 || was;
    return {was, was ? "You stop watching." : "You aren't watching a fight.", {}};
}

// ------------------------------------------------------------------ Challenges between players

namespace
{
std::string termsWords(const std::string& terms)
{
    return terms == "blood" ? "to first blood" : terms == "death" ? "until one goes down" : "until one yields";
}
} // namespace

Result World::challenge(const std::string& from, const std::string& to, const std::string& asked)
{
    const std::string terms = asked == "blood" || asked == "death" ? asked : "yield";
    const auto* a = entity(from);
    const auto* t = entity(to);
    if (!a || !t || a->dead || t->dead)
        return {false, "No such character.", {}};
    if (a->downedLeft > 0)
        return {false, "You are down.", {}};
    if (const auto why = tooLoadedToFight(from); !why.empty())
        return {false, why, to};
    if (t->downedLeft > 0)
        return {false, t->name + " is already down.", to};
    if (a->age < battle::YoungestFighter || t->age < battle::YoungestFighter)
        return {false, "Not with one so young.", to};
    if (a->cellId != t->cellId || std::hypot(a->position.x - t->position.x, a->position.y - t->position.y) > battle::StartReach)
        return {false, "Get closer first.", to};
    if (inBattle(from))
        return {false, "You are already in a fight.", to};
    if (const auto settle = settleUntil_.find(from); settle != settleUntil_.end() && time_ < settle->second)
        return {false, "You are still finding your feet after that fight.", to};
    for (const auto& b : battles_)
        if (!b.over && b.fled.count(from) && b.fighter(to) && b.fighter(to)->status != "fled")
            return {false, "You fled from that fight; you can't take it up again until it is over.", to};
    if (const auto* b = battleOf(to))
    {
        const auto* f = b->fighter(to);
        return joinBattle(from, b->id, 1 - f->side);   // A fight already going: anyone may join it.
    }
    challenges_.erase(std::remove_if(challenges_.begin(), challenges_.end(),
                                     [&](const Challenge& c) { return c.from == from || c.to == to; }),
                      challenges_.end());
    challenges_.push_back({from, to, time_ + battle::ChallengeSeconds, terms});
    notice(to, a->name + " challenges you to a fight " + termsWords(terms) + ". Accept or decline (30 seconds).");
    return {true, "You challenge " + t->name + " to a fight " + termsWords(terms) + ". They have 30 seconds to answer.", to};
}

Result World::answerChallenge(const std::string& player, bool accept)
{
    auto it = std::find_if(challenges_.begin(), challenges_.end(), [&](const Challenge& c) { return c.to == player; });
    if (it == challenges_.end())
        return {false, "Nobody has challenged you.", {}};
    const auto from = it->from;
    const auto terms = it->terms;
    challenges_.erase(it);
    const auto* p = entity(player);
    if (!accept)
    {
        notice(from, (p ? p->name : std::string("They")) + " declines your challenge.");
        return {true, "You decline.", from};
    }
    if (const auto why = tooLoadedToFight(player); !why.empty())
    {
        notice(from, (p ? p->name : std::string("They")) + " is carrying too much to fight.");
        return {false, why, from};
    }
    auto r = startBattle(from, player, true);
    if (r.ok)
    {
        if (auto* b = battleFor(from))
            b->terms = terms;
        notice(from, (p ? p->name : std::string("They")) + " accepts your challenge. A fight " + termsWords(terms) + "!");
    }
    return {r.ok, r.ok ? "You accept. A fight!" : r.message, from};
}

// ------------------------------------------------------------------ Turns

void World::fightLine(Battle& b, const std::string& actor, const std::string& target, const std::string& kind, std::string text)
{
    b.log.push_back({++b.seq, time_, actor, target, kind, std::move(text), {}});
    if (b.log.size() > battle::BattleLogKept)
        b.log.erase(b.log.begin());
}

void World::beginTurn(Battle& b, BattleFighter& f)
{
    auto* e = entity(f.id);
    f.acting = true;
    f.turnStarted = time_;
    f.deadline = time_ + battle::TurnSeconds;
    f.moved = f.acted = f.extended = f.faced = false;
    f.guarding = f.drew = false;                    // (On guard until now.)
    f.partsUsed = 0;
    f.partsAt = time_;
    f.weight = 0;
    ++f.turnsTaken;
    ++b.turns;
    if (f.status == "downed")
    {
        if (f.struggling)
        {
            f.struggling = false;
            standUp(*e, battle::StruggleUpHealth);
            e->recoveryUsed = std::floor(calendarDays_);
            f.status = "fighting";
            fightLine(b, f.id, {}, "rise", e->name + " struggles back to their feet.");
        }
    }
    f.staggered = 0;
    const bool rested = f.resting;
    f.resting = false;
    if (f.status == "fighting")
    {
        e->stamina = std::min(100.0, e->stamina + battle::staminaPerTurn(e->hurt, e->strength) * (rested ? battle::RestFactor : 1) *
                                             injury::effects(e->injuries).recovery);   // (Hurt ribs or burns, doc 38.)
        if (e->exhausted && e->stamina >= 20)
            e->exhausted = false;
        if (!e->gift.empty())
            e->mana = std::min(battle::manaMax(e->wisdom, true), e->mana + battle::ManaPerTurn);
        if (f.burning > 0)
        {
            --f.burning;
            const double burn = battle::BurnDamage + injury::effects(e->injuries).fireExtra;   // (Burned before, doc 38.)
            fightLine(b, f.id, {}, "burn", e->name + " burns (" + whole(burn) + ").");
            hurtFighter(b, f, burn, battle::DownedFire, {}, true);
            if (f.status != "fighting")
            {
                f.acting = false;
                checkOver(b);
                return;
            }
        }
        if (f.bleeding > 0)
        {
            --f.bleeding;
            fightLine(b, f.id, {}, "bleed", e->name + " bleeds (" + whole(battle::BleedDamage) + ").");
            hurtFighter(b, f, battle::BleedDamage, battle::DownedBite, {}, false);
            if (f.status != "fighting")
            {
                f.acting = false;
                checkOver(b);
                return;
            }
        }
    }
    // Smoke clears after its rounds.
    b.smoke.erase(std::remove_if(b.smoke.begin(), b.smoke.end(), [&](const auto& s) { return b.turns >= s.second; }), b.smoke.end());
}

void World::endTurn(Battle& b, BattleFighter& f)
{
    if (f.id == b.opening)
        b.opening.clear();
    f.meter = (f.moved ? 0 : 20) + (f.acted ? 0 : 20) - f.weight;   // Its bar starts again (a head start if it held back).
    if (f.staggered == 1)
    {
        f.meter -= battle::StaggerSetback;          // Staggered in its own turn: the next bar starts lower (doc 38).
        f.staggered = 2;
    }
    f.readyAt = -1;
    f.acting = false;
    checkOver(b);
}

Result World::battleMove(const std::string& id, int x, int y)
{
    auto* b = battleFor(id);
    if (!b)
        return {false, "You are not in a fight.", {}};
    if (b->over)
        return {false, "The fight is over.", {}};
    auto& f = *b->fighter(id);
    f.away = false;                                 // Any try brings an away player back.
    f.timeouts = 0;
    if (!f.acting)
        return {false, "It isn't your turn.", {}};
    if (f.moved)
        return {false, "You have already moved this turn.", {}};
    if (f.status == "downed" && f.struggling)
        return {false, "You are trying to get up.", {}};
    if (f.casting)
        return {false, "You can't move while you gather the fire.", {}};
    const auto reach = battleReach(id);
    if (std::find(reach.begin(), reach.end(), std::pair<int, int>{x, y}) == reach.end())
        return {false, "You can't get there this turn.", {}};
    auto walk = walkTo(*b, f, x, y);
    if (walk.empty())
        return {false, "You can't get there this turn.", {}};
    if (auto* mover = entity(id))
    {
        // Running costs breath for every tile (doc 33); out of it, the wolf is exhausted.
        mover->stamina = std::max(0.0, mover->stamina - double(walk.size()) * battle::tileStamina(fightPace(*mover)));
        if (mover->stamina <= 0)
            mover->exhausted = true;
    }
    f.walk = std::move(walk);                       // Walked a tile at a time (walkFighters), facing the way it goes.
    f.stepAt = time_ + stepSeconds(f);
    f.turned = f.faced;                             // (Faced by hand first this turn: the walk keeps that facing.)
    f.moved = true;
    if (const auto* mover = entity(id); mover && mover->npc && (f.acted || f.status == "downed"))
        endTurn(*b, f);                             // (A player's turn ends at its time or End turn: doc 33.)
    return {true, {}, {}};
}

std::vector<std::pair<int, int>> World::battleReach(const std::string& id) const
{
    const auto* b = battleOf(id);
    if (!b || b->over)
        return {};
    const auto* f = b->fighter(id);
    const auto* e = entity(id);
    if (!f || !e || !f->acting || f->moved)
        return {};
    if (f->status == "downed")
        return f->struggling ? std::vector<std::pair<int, int>>{} : reachFrom(*b, *f, 1);   // A crawl.
    if (f->status != "fighting")
        return {};
    return reachWith(*b, *f, *e, e->stamina, f->drew ? 1 : 0);    // (A sword taken up first costs a tile.)
}

std::vector<std::pair<int, int>> World::reachWith(const Battle& b, const BattleFighter& f, const Entity& e, double stamina, int less) const
{
    // As far as their pace takes them, and their stamina pays for (doc 33); walking is free.
    const int pace = fightPace(e);
    int range = battle::moveRange(effectiveDexterity(e), e.hurt, pace) - injury::effects(e.injuries).arenaMove;   // (A hurt leg, doc 38.)
    const int walking = battle::moveRange(effectiveDexterity(e), e.hurt, 0);
    while (range > walking && range * battle::tileStamina(pace) > stamina)
        --range;
    range -= less;
    if (range <= 0)
        return {};
    return reachFrom(b, f, range);
}

// ------------------------------------------------------------------ Planning ahead (doc 37, phase 5)

std::vector<std::pair<int, int>> World::planReach(const std::string& id) const
{
    const auto* b = battleOf(id);
    const auto* f = b ? b->fighter(id) : nullptr;
    const auto* e = entity(id);
    if (!b || b->over || !f || !e || f->acting || f->status != "fighting" || f->casting)
        return {};
    // From where it stands, with the stamina it will have when the turn comes.
    const double stamina = std::min(100.0, e->stamina + battle::staminaPerTurn(e->hurt, e->strength) * (f->resting ? battle::RestFactor : 1));
    return reachWith(*b, *f, *e, stamina);
}

namespace
{
// Whether one may plan now, and why not.
std::string cantPlan(const Battle* b, const BattleFighter* f)
{
    if (!b || !f)
        return "You are not in a fight.";
    if (b->over)
        return "The fight is over.";
    if (f->acting)
        return "It's your turn: do it now.";
    if (f->status != "fighting")
        return "You can't plan from where you lie.";
    if (f->casting)
        return "You are gathering the fire.";
    return {};
}
} // namespace

Result World::planMove(const std::string& id, int x, int y)
{
    auto* b = battleFor(id);
    auto* f = b ? b->fighter(id) : nullptr;
    if (const auto why = cantPlan(b, f); !why.empty())
        return {false, why, {}};
    f->away = false;                                // Any try brings an away player back.
    f->timeouts = 0;
    if (x == f->x && y == f->y)
    {
        f->plan.move = false;                       // Staying put.
        return {true, {}, {}};
    }
    const auto reach = planReach(id);
    if (std::find(reach.begin(), reach.end(), std::pair<int, int>{x, y}) == reach.end())
        return {false, "Your next turn can't take you there.", {}};
    if (f->plan.act == "rest")
        f->plan.act.clear();                        // (Resting is a turn without a move.)
    f->plan.move = true;
    f->plan.x = x;
    f->plan.y = y;
    return {true, {}, {}};
}

Result World::planAct(const std::string& id, const std::string& action, const std::string& target)
{
    auto* b = battleFor(id);
    auto* f = b ? b->fighter(id) : nullptr;
    if (const auto why = cantPlan(b, f); !why.empty())
        return {false, why, {}};
    const auto* e = entity(id);
    f->away = false;
    f->timeouts = 0;
    if (action == "bite" || action == "sword")
    {
        const auto* t = b->fighter(target);
        if (!t || t->side == f->side || t->status != "fighting")
            return {false, "Strike whom?", target};
    }
    else if (action == "shove")
    {
        const auto* t = b->fighter(target);
        if (!t || t->id == f->id || t->status != "fighting")
            return {false, "Shove whom?", target};
    }
    else if (action == "tend")
    {
        const auto* t = b->fighter(target);
        if (!t || t->side != f->side || t->status != "downed")
            return {false, "Tend whom? Only someone on your side who is down.", target};
    }
    else if (action == "flame")
    {
        if (!e || e->gift != "fire")
            return {false, "You have no Gift of fire.", {}};
        if (target.find(',') == std::string::npos)
            return {false, "Aim it: which way?", {}};
    }
    else if (action == "roll")
    {
        if (f->burning <= 0)
            return {false, "You aren't burning.", {}};
    }
    else if (action == "rest")
        f->plan.move = false;                       // (A turn without a move.)
    else if (action != "hold" && action != "stow" && action != "pickup" && action != "flee" && action != "guard")
        return {false, "You can't plan that.", {}};
    f->plan.act = action;
    f->plan.target = target;
    return {true, {}, target};
}

Result World::unplan(const std::string& id, const std::string& part)
{
    auto* b = battleFor(id);
    auto* f = b ? b->fighter(id) : nullptr;
    if (!b || !f)
        return {false, "You are not in a fight.", {}};
    if (part == "move")
        f->plan.move = false;
    else if (part == "act")
    {
        f->plan.act.clear();
        f->plan.target.clear();
    }
    else if (!f->plan.begun)
        f->plan = {};
    return {true, {}, {}};
}

void World::playPlan(Battle& b, BattleFighter& f)
{
    // The turn has come: the planned move first (or, for a blow at a foe out of reach, a step in as a click on them
    // takes), then the action once the walk is done. What can't be done now is said, and left for the player to do.
    const std::string id = f.id;
    auto& plan = f.plan;
    if (f.status != "fighting")
    {
        plan = {};
        return;
    }
    const auto* e = entity(id);
    std::string act = plan.act;
    if ((act == "bite" || act == "sword") && e)
        act = e->mouth == "sword" ? "sword" : "bite";   // Whatever is in the jaws when the moment comes.
    if (!plan.begun)
    {
        plan.begun = true;
        if (act == "rest")
        {
            plan = {};
            if (const auto r = battleAct(id, "rest"); !r.ok)
                notice(id, "Your plan: " + r.message);
            return;
        }
        int tx = f.x, ty = f.y;
        if (plan.move)
        {
            tx = plan.x;
            ty = plan.y;
        }
        else if (act == "bite" || act == "sword" || act == "shove")
        {
            const auto* t = b.fighter(plan.target);
            const int range = act == "sword" ? battle::SwordReach : 1;
            if (t && t->status == "fighting" && tilesApart(f.x, f.y, t->x, t->y) > range)
                for (const auto& [x, y] : battleReach(id))
                {
                    const int d = tilesApart(x, y, t->x, t->y), best = tilesApart(tx, ty, t->x, t->y);
                    if (d < best || (d == best && tilesApart(x, y, f.x, f.y) < tilesApart(tx, ty, f.x, f.y)))
                    {
                        tx = x;
                        ty = y;
                    }
                }
        }
        if (tx != f.x || ty != f.y)
            if (const auto r = battleMove(id, tx, ty); !r.ok)
                notice(id, "Your plan: " + r.message);
    }
    if (!f.walk.empty())
        return;                                     // (The action when it gets there.)
    const std::string target = plan.target;
    plan = {};
    if (act.empty())
        return;
    if (const auto r = battleAct(id, act, target); !r.ok && !r.message.empty())
        notice(id, "Your plan: " + r.message);
}

double World::meterHaste(const Battle& b) const
{
    // No dead air (doc 37): while no player is taking a turn and no fire is gathering, the bars fill faster.
    if (b.over || !b.casts.empty())
        return 1;
    for (const auto& f : b.fighters)
        if (f.acting && !f.away)
            if (const auto* e = entity(f.id); e && !e->npc)
                return 1;
    return battle::Haste;
}

Result World::battleAct(const std::string& id, const std::string& action, const std::string& target)
{
    auto* b = battleFor(id);
    if (!b)
    {
        if (action == "struggle")
            return struggleUp(id);
        if (action == "tend")
            return tendWounds(id, target);
        return {false, "You are not in a fight.", {}};
    }
    if (b->over)
        return {false, "The fight is over.", {}};
    auto& f = *b->fighter(id);
    auto* e = entity(id);
    const bool wasAway = f.away;
    f.away = false;                                 // Any try brings an away player back.
    f.timeouts = 0;
    if (action == "back")
        return {true, wasAway ? "You're back in the fight: your next turn is yours." : std::string(), {}};
    if (action == "yield")
        return offerYield(id);                      // At any time, one's turn or not.
    if (!f.acting)
        return {false, "It isn't your turn.", {}};
    Result r{true, {}, target};
    if (action == "wait")
    {
        fightLine(*b, id, {}, "wait", e->name + (f.moved || f.acted ? " ends their turn." : " waits."));
        endTurn(*b, f);
        return {true, "You wait.", {}};
    }
    if (f.status == "downed")
    {
        if (action != "struggle")
            return {false, "You are down. You can crawl, struggle up, or wait.", {}};
        if (!recoveryAvailable(*e))
            return {false, "You have no strength left to rise. Only someone tending your wounds can get you up.", {}};
        f.struggling = true;
        fightLine(*b, id, {}, "struggle", e->name + " tries to struggle up.");
        f.acted = true;
        endTurn(*b, f);
        return {true, "You try to struggle up. If nothing hits you, you'll be on your feet next turn.", {}};
    }
    if (f.status != "fighting")
        return {false, "You can't act now.", {}};
    if (f.casting)
        return {false, "You are gathering the fire: you can only wait.", {}};
    if (action == "truce")
        return offerTruce(id);
    if (action == "hold" || action == "stow")
    {
        // Taking up or putting away a sword is part of the move, not the action (doc 37): once a turn; before the
        // move it costs a tile of it.
        if (f.drew)
            return {false, "You have already reached for your sword this turn.", {}};
        r = action == "hold" ? holdItem(id, "sword") : stowItem(id);
        if (!r.ok)
            return r;
        f.drew = true;
        fightLine(*b, id, {}, action, e->name + (action == "hold" ? " takes up a sword in their jaws." : " puts their sword away."));
        return r;
    }
    if (f.acted)
        return {false, "You have already acted this turn.", {}};
    if (action == "bite")
        r = bite(*b, f, target);
    else if (action == "sword")
        r = swordStrike(*b, f, target);
    else if (action == "flame")
    {
        const auto comma = target.find(',');
        if (comma == std::string::npos)
            return {false, "Aim it: which way?", {}};
        r = castFlame(*b, f, std::atoi(target.c_str()), std::atoi(target.c_str() + comma + 1));
    }
    else if (action == "rest")
    {
        // Catching one's breath: a turn without moving or acting, for twice the stamina back at the next (doc 33).
        // Each turn anew.
        if (f.status != "fighting")
            return {false, "You can't rest now.", {}};
        if (f.moved || f.acted)
            return {false, "You have already moved or acted this turn: rest instead, before either.", {}};
        if (f.casting)
            return {false, "You are gathering the fire.", {}};
        f.moved = f.acted = true;
        f.resting = true;
        fightLine(*b, id, {}, "rest", e->name + " catches their breath.");
        return {true, "You catch your breath: no moving or acting this turn, and twice the stamina back at your next.", {}};
    }
    else if (action == "roll")
    {
        if (f.burning <= 0)
            return {false, "You aren't burning.", {}};
        f.burning = 0;
        f.acted = true;
        fightLine(*b, id, {}, "roll", e->name + " rolls and puts the flames out.");
        r = {true, "You roll, and the flames go out.", {}};
    }
    else if (action == "shove")
        r = shove(*b, f, target);
    else if (action == "guard")
    {
        // No blow: harder to hit, and turning to meet one, until its next turn (doc 37).
        f.guarding = true;
        f.acted = true;
        fightLine(*b, id, {}, "guard", e->name + " stands on guard.");
        r = {true, "You stand on guard: harder to hit, and you turn to meet any blow, until your next turn.", {}};
    }
    else if (action == "pickup")
    {
        auto drop = std::find_if(b->drops.begin(), b->drops.end(), [&](const BattleDrop& d) { return tilesApart(f.x, f.y, d.x, d.y) <= 1; });
        if (drop == b->drops.end())
            return {false, "There is nothing next to you to pick up.", {}};
        if (!e->mouth.empty())
            return {false, "Your mouth is full.", {}};
        society_.openAccount(GroundAccount);
        if (!society_.shift(GroundAccount, id, drop->item, 1, 0, "picked up in a fight"))
            return {false, "You can't take it.", {}};
        e->mouth = drop->item;
        b->drops.erase(drop);
        f.acted = true;
        fightLine(*b, id, {}, "pickup", e->name + " snatches up the sword.");
        r = {true, "You snatch up the sword.", {}};
    }
    else if (action == "tend")
    {
        auto* t = b->fighter(target);
        auto* te = entity(target);
        if (!t || !te || t->side != f.side || t->status != "downed")
            return {false, "Tend whom? Only someone on your side who is down.", target};
        if (tilesApart(f.x, f.y, t->x, t->y) != 1)
            return {false, "Get next to them first.", target};
        if (e->stamina < battle::TendStamina)
            return {false, "You are too spent to tend them.", target};
        e->stamina -= battle::TendStamina;
        standUp(*te, battle::TendedHealth);
        t->status = "fighting";
        t->struggling = false;
        if (e->npc)
            f.facing = battle::octant(t->x - f.x, t->y - f.y);
        f.acted = true;
        fightLine(*b, id, target, "tend", e->name + " tends " + te->name + "'s wounds; " + te->name + " is back on their feet.");
        r = {true, "You get " + te->name + " back on their feet.", target};
    }
    else if (action == "flee")
    {
        if (!b->onEdge(f.x, f.y))
        {
            // Which way out, and how far: the nearest of the arena's edge rows (lit on the map).
            const int west = f.x - (b->x0 + 1), east = b->x0 + b->w - 2 - f.x, north = f.y - (b->y0 + 1), south = b->y0 + b->h - 2 - f.y;
            const int nearest = std::min({west, east, north, south});
            const char* way = nearest == north ? "north" : nearest == south ? "south" : nearest == west ? "west" : "east";
            return {false, "You can only flee from the arena's edge: the nearest is " + std::to_string(nearest) +
                               (nearest == 1 ? " tile" : " tiles") + " to the " + way + ".", {}};
        }
        double best = -1;
        for (const auto& o : b->fighters)
            if (o.side != f.side && o.status == "fighting" && tilesApart(f.x, f.y, o.x, o.y) == 1)
                if (const auto* oe = entity(o.id))
                    best = std::max(best, effectiveDexterity(*oe));
        f.acted = true;
        if (best >= 0)
        {
            const double odds = std::clamp(.5 + (effectiveDexterity(*e) - best) / 100, .2, .9);
            if (chance(id + "flee", std::int64_t(b->seq) * 31 + b->turns) >= odds)
            {
                fightLine(*b, id, {}, "flee", e->name + " tries to break away, but is cut off.");
                if (e->npc)
                    endTurn(*b, f);
                return {true, "You try to break away, but they cut you off.", {}};
            }
        }
        fightLine(*b, id, {}, "flee", e->name + " breaks away and flees the fight.");
        leaveArena(*b, f, true);
        f.acting = false;
        checkOver(*b);
        return {true, "You break away and flee the fight.", {}};
    }
    else
        return {false, "You can't do that in a fight.", {}};
    if (r.ok && f.moved && f.acted && !b->over && e->npc)
        endTurn(*b, f);
    else
        checkOver(*b);
    return r;
}

double World::strikeChance(const BattleFighter& f, const BattleFighter& t) const
{
    const auto* e = entity(f.id);
    const auto* d = entity(t.id);
    if (!e || !d)
        return 0;
    // From the side or behind is easier: how far the defender faces from where the blow comes.
    // One on guard turns to meet it, and is harder to hit (doc 37).
    const int gap = battle::octantGap(t.facing, battle::octant(f.x - t.x, f.y - t.y));
    const double angle = t.guarding ? -battle::GuardDodge : gap >= 3 ? .2 : gap == 2 ? .1 : 0;
    return std::clamp(.75 + (effectiveDexterity(*e) - effectiveDexterity(*d)) * .005 +
                          (temperamentOf(*e).skill - temperamentOf(*d).skill) * .003 + angle,
                      .2, .95);
}

Result World::bite(Battle& b, BattleFighter& f, const std::string& target)
{
    auto* t = b.fighter(target);
    auto* e = entity(f.id);
    auto* d = entity(target);
    if (!t || !d || t->side == f.side || t->status == "fled" || t->status == "dead")
        return {false, "Bite whom?", target};
    if (t->status == "downed")
        return {false, d->name + " is already down.", target};
    if (!e->mouth.empty())
        return {false, "You can't bite with a " + e->mouth + " in your mouth.", target};
    if (tilesApart(f.x, f.y, t->x, t->y) != 1)
        return {false, "Get next to them first.", target};
    const double biteCost = battle::BiteStamina + injury::effects(e->injuries).attackStamina;   // (Hurt ribs, doc 38.)
    if (e->exhausted || e->stamina < biteCost)
        return {false, "You are too winded to bite.", target};
    e->stamina -= biteCost;
    if (e->stamina <= 0)
    {
        e->stamina = 0;
        e->exhausted = true;
    }
    if (e->npc)
        f.facing = battle::octant(t->x - f.x, t->y - f.y);
    if (t->guarding)
        t->facing = battle::octant(f.x - t->x, f.y - t->y);     // On guard: it turns to meet the blow.
    f.acted = true;
    const double hit = strikeChance(f, *t);
    const auto key = std::int64_t(b.seq) * 7919 + b.turns;
    const double r = chance(f.id + "|" + target, key);
    if (r >= hit)
    {
        fightLine(b, f.id, target, "miss", e->name + " snaps at " + d->name + " and misses.");
        return {true, "You snap at " + d->name + " and miss.", target};
    }
    const bool graze = r >= hit - .1;
    double damage = battle::BiteDamage * (.6 + e->strength / 125) * (.85 + .3 * chance(target + "|" + f.id, key));
    damage = std::max(1.0, damage - injury::effects(e->injuries).biteLess);   // (A hurt neck or shoulder, doc 38.)
    if (graze)
        damage /= 2;
    const auto landed = land(f, *t, *d, damage, "thrust", key);   // Teeth: a thrust, no pierce (doc 35, Part 8).
    wearArmourAt(*d, landed.zone, damage - landed.damage);           // The armour there takes the wear (RatwDurability.cpp).
    damage = landed.damage;
    const std::string how = graze ? " grazes " : " bites ";
    fightLine(b, f.id, target, graze ? "graze" : "hit", e->name + how + d->name + landed.words + " (" + whole(damage) + ").");
    growSkill(*e, battle::SkillPerHit);
    hurtFighter(b, *t, damage, battle::DownedBite, f.id, true);
    if (t->status == "downed")
        return {true, "You bite " + d->name + ", and they go down.", target};
    return {true, std::string(graze ? "You graze " : "You bite ") + d->name + ".", target};
}

void World::downFighter(Battle& b, BattleFighter& f, double overkill, double base, const std::string& by)
{
    auto* e = entity(f.id);
    if (!e)
        return;
    if (huntKill(b, f, by))
        return;                                     // An animal in a hunt dies, and is taken (doc 41).
    if (e->npc)
        e->downedLeft = std::max(base * battle::DownedMinimum, base - overkill * battle::OverkillSeconds);
    else
    {
        // A player lies down for a while, never dies (doc 38): the longer for each downing since a full rest.
        ++e->downsSinceRest;
        const double getUp = base == battle::DownedBlunt ? battle::GetUpBlunt : base == battle::DownedFire ? battle::GetUpFire
                                                                                                          : battle::GetUpBite;
        e->downedLeft = std::min(battle::GetUpLongest,
                                 getUp * battle::getUpStretch(e->downsSinceRest) + overkill * battle::GetUpOverkillSeconds);
        injureOnDown(b, f, overkill, base, by);     // And an injury that outlasts the fight (doc 38).
    }
    e->restRun = 0;
    e->state = "downed";
    e->posture = "lying";
    e->postureTarget.clear();
    e->postureRemaining = 0;
    f.status = "downed";
    f.struggling = false;
    f.burning = 0;
    f.bleeding = 0;
    f.staggered = 0;
    f.walk.clear();
    fightLine(b, f.id, by, "down", e->name + " goes down.");
    if (f.casting)
    {
        f.casting = false;
        b.casts.erase(std::remove_if(b.casts.begin(), b.casts.end(), [&](const BattleCast& c) { return c.caster == f.id; }), b.casts.end());
    }
    if (!e->mouth.empty())
        dropItem(b, f);
    recordEvent({"downed", by, f.id, b.cellId, 0, 0, {}, 0, 0, b.id});
    if (auto* camp = campOf(f.id))
    {
        const auto folk = folk_.find(f.id);
        camp->strength = std::max(0.0, camp->strength - (folk != folk_.end() ? folk->second.share : 1));
        recordEvent({"bandit falls", by, camp->id, b.cellId, 0, 0, {}, 0, 0, e->name});
        // Their leader down, the rest break and run.
        if (f.id.size() >= 2 && f.id.compare(f.id.size() - 2, 2, ":0") == 0)
            for (auto& o : b.fighters)
                if (o.id != f.id && o.status == "fighting" && campOf(o.id) == camp)
                {
                    if (const auto* oe = entity(o.id))
                        fightLine(b, o.id, {}, "flee", oe->name + " breaks and runs.");
                    leaveArena(b, o, true);
                }
    }
    if (!b.incident.empty())
        if (auto* inc = incident(b.incident))
            witness(*inc, 0);                       // Those who came running see the end of it.
}

void World::checkOver(Battle& b)
{
    if (b.over)
        return;
    const int a = b.standing(0), c = b.standing(1);
    bool truce = !b.truceBy.empty();
    for (const auto& f : b.fighters)
        truce = truce && (f.status != "fighting" || f.truce);
    if (truce && a > 0 && c > 0)
    {
        b.over = true;
        b.overAt = time_;
        for (auto& o : b.fighters)
            o.acting = false;
        b.banner = "The fight ends in a truce";
        b.truced = true;
        fightLine(b, {}, {}, "over", b.banner + ".");
        return;
    }
    if (a > 0 && c > 0)
        return;
    b.over = true;
    b.overAt = time_;
    for (auto& o : b.fighters)
        o.acting = false;
    std::string winner;
    for (const auto& f : b.fighters)
        if (f.status == "fighting" && (a > 0 ? f.side == 0 : f.side == 1))
            if (const auto* e = entity(f.id))
            {
                winner = e->name;
                break;
            }
    // How it ended, in a few words: first blood, a yield, or the fight over.
    const int losing = a > 0 ? 1 : 0;
    bool yielded = false, allYielded = true;
    std::string loser;
    for (const auto& f : b.fighters)
        if (f.side == losing && f.status != "fled")
        {
            yielded = yielded || f.status == "yielded";
            allYielded = allYielded && f.status == "yielded";
            if (const auto* e = entity(f.id); e && loser.empty())
                loser = e->name;
        }
    const std::string how = yielded && allYielded ? (b.terms == "blood" ? "First blood" : loser + " yields") : "The fight is over";
    b.banner = winner.empty() ? how : how + " · " + winner + "'s side stands";
    fightLine(b, {}, {}, "over", b.banner + ".");
    huntBanner(b);                                  // A hunt says how it went (doc 41).
}

void World::standUp(Entity& e, double health)
{
    e.hurt = std::clamp(100 - health, 0.0, 99.0);
    e.downedLeft = 0;
    e.struggleUntil = 0;
    if (e.state == "downed")
        e.state.clear();
    e.posture = "standing";
    e.postureTarget.clear();
    e.postureRemaining = 0;
}

void World::leaveArena(Battle& b, BattleFighter& f, bool fleeing)
{
    // Back into the world at the tile they left from, or the nearest open spot to it within three tiles.
    auto* e = entity(f.id);
    if (fleeing)
    {
        f.status = "fled";
        b.fled.insert(f.id);
    }
    if (!e)
        return;
    const auto freeAt = [&](Vec2 p) {
        if (!standable(b.cellId, p))
            return false;
        for (const Entity* o : entitiesIn(b.cellId))
            if (o && o->id != e->id && o->cellId == b.cellId && !o->offstage &&
                std::hypot(o->position.x - p.x, o->position.y - p.y) < .6)
                return false;
        return true;
    };
    Vec2 spot{f.x + .5, f.y + .5};
    bool found = freeAt(spot);
    for (int ring = 1; ring <= 3 && !found; ++ring)
        for (int dy = -ring; dy <= ring && !found; ++dy)
            for (int dx = -ring; dx <= ring && !found; ++dx)
                if (std::max(std::abs(dx), std::abs(dy)) == ring && freeAt({f.x + dx + .5, f.y + dy + .5}))
                {
                    spot = {f.x + dx + .5, f.y + dy + .5};
                    found = true;
                }
    if (!found)
        spot = {f.lineupX, f.lineupY};
    e->position = spot;
    e->facing = f.facing * Pi / 4;
    e->turnTarget = e->facing;
    e->velocity = {};
    e->transitioned = true;                         // The page lets go of any held movement.
    settleUntil_[f.id] = time_ + battle::SettleSeconds;
    if (!e->npc && fleeing)
        notice(f.id, "You break away from the fight.");
}

void World::finishBattle(Battle& b)
{
    injureAtEnd(b);                                 // Limping at the end may leave an injury (doc 38).
    // Everyone fades back into the world where they stood in the arena.
    std::vector<std::string> players, bandits;
    for (auto& f : b.fighters)
    {
        if (f.status == "fled")
            continue;
        leaveArena(b, f, false);
        if (const auto* e = entity(f.id))
        {
            if (!e->npc)
            {
                players.push_back(f.id);
                notice(f.id, b.banner + ".");
            }
            if (campOf(f.id))
                bandits.push_back(f.id);
        }
    }
    // What was dropped in the arena lies where it fell; those who stood to the end learned something.
    for (const auto& d : b.drops)
        ground_.push_back({"ground-" + std::to_string(++nextGround_), b.cellId, d.item, d.x + .5, d.y + .5});
    b.drops.clear();
    for (const auto& f : b.fighters)
        if (f.status == "fighting")
            if (auto* e = entity(f.id))
                growSkill(*e, battle::SkillPerFight);
    recordEvent({"fight ends", {}, {}, b.cellId, 0, 0, {}, 0, 0, b.id});
    endHunt(b);                                     // A hunt's animals go with it (doc 41).
    if (b.camp.empty())
        return;
    if (testCamp(b.camp))
    {
        // A Dev Console fight: its bandits, any allies made up for it, and its camp go with it; nobody is robbed, and no
        // camp is cleared.
        std::vector<std::string> ofCamp = bandits;
        for (const auto& [id, folk] : folk_)
            if (folk.of == b.camp)
                ofCamp.push_back(id);
        for (const auto& id : ofCamp)
            removeRoadFolk(id);
        endEncounter(b.camp, 0);
        roads_.camps.erase(std::remove_if(roads_.camps.begin(), roads_.camps.end(), [&](const BanditCamp& c) { return c.id == b.camp; }),
                           roads_.camps.end());
        return;
    }
    auto* camp = [&]() -> BanditCamp* {
        for (const auto& id : bandits)
            if (auto* c = campOf(id))
                return c;
        return nullptr;
    }();
    bool banditStands = false, travellerStands = false;
    std::string traveller;
    for (const auto& f : b.fighters)
    {
        const bool bandit = std::find(bandits.begin(), bandits.end(), f.id) != bandits.end();
        if (!bandit && traveller.empty() && std::find(players.begin(), players.end(), f.id) != players.end())
            traveller = f.id;
        if (f.status == "fighting")
            (bandit ? banditStands : travellerStands) = true;
    }
    if (!camp)
        return;
    if (!banditStands && !traveller.empty())
        clearCamp(*camp, traveller);
    else if (!travellerStands)
    {
        for (const auto& id : players)
            if (auto* p = entity(id); p && !p->dead && p->downedLeft > 0)
            {
                beaten(*p, *camp);
                p->posture = "lying";
                p->state = "downed";
            }
        endEncounter(camp->id, calendar::SecondsPerDay / 24);
    }
    else
        endEncounter(camp->id, calendar::SecondsPerDay / 24);
}

void World::npcTurn(Battle& b, BattleFighter& f)
{
    auto* e = entity(f.id);
    if (!e)
    {
        endTurn(b, f);
        return;
    }
    if (animalTurn(b, f))
        return;                                     // An animal in a hunt (doc 41, RatwHunt.cpp).
    if (f.status == "downed")
    {
        if (!f.struggling && recoveryAvailable(*e))
            battleAct(f.id, "struggle");
        else
            battleAct(f.id, "wait");
        return;
    }
    const auto temper = temperamentOf(*e);
    const double health = 100 - e->hurt;
    if (f.casting)
    {
        battleAct(f.id, "wait");
        return;
    }
    // A truce on the table: the timid and the hurt take it; the aggressive won't.
    if (!b.truceBy.empty() && !f.truce)
        answerTruce(f.id, temper.kind != "aggressive" || health < temper.fleeBelow * 2);
    if (b.over || !f.acting)
        return;
    if (f.burning > 0)
    {
        battleAct(f.id, "roll");
        if (f.acting)
            battleAct(f.id, "wait");
        return;
    }
    std::vector<const BattleFighter*> enemies, downedAllies;
    for (const auto& o : b.fighters)
    {
        if (o.side != f.side && o.status == "fighting")
            enemies.push_back(&o);
        if (o.side == f.side && o.id != f.id && o.status == "downed")
            downedAllies.push_back(&o);
    }
    if (enemies.empty())
    {
        battleAct(f.id, "wait");
        return;
    }
    const auto nearest = [&](int x, int y) {
        int best = std::numeric_limits<int>::max();
        for (const auto* o : enemies)
            best = std::min(best, tilesApart(x, y, o->x, o->y));
        return best;
    };
    bool allyClose = false;
    for (const auto& o : b.fighters)
        if (o.side == f.side && o.id != f.id && o.status == "fighting" && tilesApart(f.x, f.y, o.x, o.y) <= 3)
            allyClose = true;
    const bool running = health < temper.fleeBelow || f.scared || (temper.kind == "timid" && e->hurt > 0 && !allyClose);
    if (running)
    {
        if (!b.onEdge(f.x, f.y))
        {
            // To the edge, as far from them as it can.
            const auto reach = battleReach(f.id);
            std::pair<int, int> best{f.x, f.y};
            int bestScore = std::numeric_limits<int>::min();
            for (const auto& [x, y] : reach)
            {
                const int edge = std::min({x - b.x0, y - b.y0, b.x0 + b.w - 1 - x, b.y0 + b.h - 1 - y});
                const int score = -edge * 4 + nearest(x, y);
                if (score > bestScore)
                {
                    bestScore = score;
                    best = {x, y};
                }
            }
            if (best != std::pair<int, int>{f.x, f.y})
                battleMove(f.id, best.first, best.second);
            if (!f.walk.empty())
                return;                             // (The rest when it gets there.)
        }
        if (!b.over && f.acting)
        {
            if (b.onEdge(f.x, f.y))
                battleAct(f.id, "flee");
            else
                battleAct(f.id, "wait");
        }
        return;
    }
    // A cautious one tends a fallen friend next to it first.
    if (temper.kind == "cautious" && e->stamina >= battle::TendStamina)
        for (const auto* o : downedAllies)
            if (tilesApart(f.x, f.y, o->x, o->y) == 1)
            {
                battleAct(f.id, "tend", o->id);
                if (f.acting)
                    battleAct(f.id, "wait");
                return;
            }
    // Whom to go for: the nearest (an aggressive one, the most hurt of the nearest).
    const BattleFighter* mark = nullptr;
    double markScore = std::numeric_limits<double>::max();
    for (const auto* o : enemies)
    {
        const auto* oe = entity(o->id);
        const double score = tilesApart(f.x, f.y, o->x, o->y) * 10 - (temper.kind == "aggressive" && oe ? oe->hurt / 10 : 0);
        if (score < markScore)
        {
            markScore = score;
            mark = o;
        }
    }
    const bool closeIn = temper.kind == "aggressive" || (temper.kind == "cautious" && tilesApart(f.x, f.y, mark->x, mark->y) <= 3);
    if (tilesApart(f.x, f.y, mark->x, mark->y) > 1 && closeIn)
    {
        // Next to it, from its side or back where it can; getting there by the way there is, around walls and
        // tables, not as the crow flies (where there is no way, as near as it can get).
        const auto steps = stepsTo(b, mark->x, mark->y, f.id);
        const auto walk = [&](int x, int y) {
            const int s = b.inArena(x, y) ? steps[std::size_t((y - b.y0) * b.w + (x - b.x0))] : -1;
            return s >= 0 ? s : 1000 + tilesApart(x, y, mark->x, mark->y);
        };
        std::pair<int, int> best{f.x, f.y};
        double bestScore = walk(f.x, f.y) * 10.0;
        for (const auto& [x, y] : battleReach(f.id))
        {
            const int d = tilesApart(x, y, mark->x, mark->y);
            const int gap = battle::octantGap(mark->facing, battle::octant(x - mark->x, y - mark->y));
            const double score = walk(x, y) * 10 - (d == 1 && temper.kind == "aggressive" ? gap : 0);
            if (score < bestScore)
            {
                bestScore = score;
                best = {x, y};
            }
        }
        if (best != std::pair<int, int>{f.x, f.y})
            battleMove(f.id, best.first, best.second);
        if (!f.walk.empty())
            return;                                 // (The rest when it gets there.)
    }
    else if (temper.kind == "timid" && tilesApart(f.x, f.y, mark->x, mark->y) > 1)
    {
        // Keep its distance.
        std::pair<int, int> best{f.x, f.y};
        int bestScore = nearest(f.x, f.y);
        for (const auto& [x, y] : battleReach(f.id))
            if (nearest(x, y) > bestScore)
            {
                bestScore = nearest(x, y);
                best = {x, y};
            }
        if (best != std::pair<int, int>{f.x, f.y})
            battleMove(f.id, best.first, best.second);
        if (!f.walk.empty())
            return;                                 // (The rest when it gets there.)
    }
    if (b.over || !f.acting)
        return;
    if (e->mouth == "sword" && tilesApart(f.x, f.y, mark->x, mark->y) <= battle::SwordReach && !e->exhausted &&
        e->stamina >= battle::SwordStamina)
        battleAct(f.id, "sword", mark->id);
    else if (e->mouth.empty() && tilesApart(f.x, f.y, mark->x, mark->y) == 1 && !e->exhausted && e->stamina >= battle::BiteStamina)
        battleAct(f.id, "bite", mark->id);
    // A foe at its throat and no blow to give (winded, or the wrong thing in its jaws): all but the aggressive stand on
    // guard (doc 37).
    if (!b.over && f.acting && !f.acted && temper.kind != "aggressive" && tilesApart(f.x, f.y, mark->x, mark->y) == 1)
        battleAct(f.id, "guard");
    if (!b.over && f.acting)
        battleAct(f.id, "wait");
}

void World::tendBattles(double dt)
{
    // Challenges nobody answered.
    for (auto it = challenges_.begin(); it != challenges_.end();)
        if (time_ >= it->until)
        {
            if (const auto* t = entity(it->to))
                notice(it->from, t->name + " does not answer your challenge.");
            it = challenges_.erase(it);
        }
        else
            ++it;
    for (auto& b : battles_)
    {
        // Watchers who went elsewhere stop watching.
        for (auto it = b.observers.begin(); it != b.observers.end();)
        {
            const auto* o = entity(*it);
            it = !o || o->cellId != b.cellId ? b.observers.erase(it) : std::next(it);
        }
        // Someone who left the world (a player gone offline) is out of the fight.
        for (auto& f : b.fighters)
            if (f.status != "fled" && f.status != "dead" && !entity(f.id))
            {
                f.status = "fled";
                b.fled.insert(f.id);
                f.acting = false;
            }
        if (!b.over)
            checkOver(b);
        if (!b.over && time_ - b.lookedAround >= .5)
        {
            b.lookedAround = time_;
            tendFightSurroundings(b);
        }
        if (b.over)
        {
            if (time_ >= b.overAt + battle::BannerSeconds + battle::FadeSeconds)
                finishBattle(b);
            continue;
        }
        // An offer to yield nobody answered is a no.
        if (!b.yieldBy.empty() && time_ >= b.yieldUntil)
        {
            b.yieldBy.clear();
            fightLine(b, {}, {}, "refuse", "No one answers the offer to yield. The fight goes on.");
        }
        // Every player still standing in it away, and no NPC to fight on: after a while the fight lapses.
        {
            bool players = false, allAway = true;
            for (const auto& f : b.fighters)
                if (f.status == "fighting")
                    if (const auto* e = entity(f.id))
                    {
                        players = players || !e->npc;
                        allAway = allAway && !e->npc && f.away;
                    }
            if (!players || !allAway)
                b.allAwaySince = -1;
            else if (b.allAwaySince < 0)
                b.allAwaySince = time_;
            else if (time_ - b.allAwaySince >= battle::LapseSeconds)
            {
                b.over = true;
                b.overAt = time_;
                b.truced = true;
                for (auto& o : b.fighters)
                    o.acting = false;
                b.banner = "The fight lapses: no one is left fighting it";
                fightLine(b, {}, {}, "over", b.banner + ".");
                continue;
            }
        }
        walkFighters(b);
        const double haste = meterHaste(b);
        // Real time (doc 33's initiative, as a bar): every bar fills but those taking a turn, and the Downed bleed. A
        // full bar is a turn at once: several fighters may be acting together, each on their own clock.
        for (auto& f : b.fighters)
        {
            auto* e = entity(f.id);
            if (!e || !takesTurns(f))
                continue;
            if (f.status == "downed" && !e->lingering)
            {
                e->downedLeft -= dt;
                if (e->downedLeft <= 0 && !e->npc)
                {
                    // A player's time down is up: they get back up, sore (doc 38).
                    standUp(*e, battle::GetUpHealth);
                    f.status = "fighting";
                    f.struggling = false;
                    fightLine(b, f.id, {}, "rise", e->name + " gets back up.");
                }
                else if (e->downedLeft <= 0)
                {
                    e->downedLeft = 0;
                    f.status = "dead";
                    f.meter = 0;
                    f.acting = false;
                    setDead(f.id, true);
                    fightLine(b, f.id, {}, "death", e->name + " dies of their wounds.");
                    continue;
                }
            }
            if (f.acting)
                continue;
            f.meter = std::min(100.0, f.meter + battle::meterGain(effectiveDexterity(*e) + battle::armourDex(*e)) * battle::MeterPerSecond * haste * dt *
                                          injury::effects(e->injuries).initiative);   // (Knocked senseless, doc 38.)
            if (f.meter >= 100)
            {
                f.readyAt = time_;
                beginTurn(b, f);
            }
        }
        // Spells go off when their time is up, whoever is acting then (the tell is a countdown everyone sees).
        for (std::size_t i = 0; i < b.casts.size();)
            if (time_ >= b.casts[i].firesAt)
            {
                const auto cast = b.casts[i];
                b.casts.erase(b.casts.begin() + std::ptrdiff_t(i));
                resolveCast(b, cast);
            }
            else
                ++i;
        checkOver(b);
        if (b.over)
            continue;
        for (auto& f : b.fighters)
        {
            if (!f.acting || b.over)
                continue;
            auto* e = entity(f.id);
            if (!e || !takesTurns(f))
            {
                f.acting = false;
                continue;
            }
            if (e->npc)
            {
                // The one who started it has the first blow: the others wait until that first turn is done.
                if (!b.opening.empty() && f.id != b.opening)
                {
                    if (const auto* o = b.fighter(b.opening); o && o->acting && o->status == "fighting")
                    {
                        f.turnStarted = time_;
                        continue;
                    }
                    b.opening.clear();
                }
                if (!f.walk.empty())
                    continue;                       // Walking there first; then the rest of its turn.
                if (time_ >= f.turnStarted + battle::NpcPause)
                {
                    npcTurn(b, f);
                    if (f.acting && f.walk.empty())
                        endTurn(b, f);
                }
                continue;
            }
            if (f.away)
            {
                endTurn(b, f);
                continue;
            }
            // A plan made while the bar filled plays out (doc 37); the rest of the turn is still theirs.
            if (!f.plan.empty() && time_ >= f.turnStarted + battle::PlanBeat)
            {
                playPlan(b, f);
                if (b.over || !f.acting)
                    continue;
            }
            // Move, action and facing all used: the turn ends by itself, a moment after the last (doc 33).
            if (const int used = int(f.moved) + int(f.acted) + int(f.faced); used != f.partsUsed)
            {
                f.partsUsed = used;
                f.partsAt = std::max(f.partsAt, time_);
            }
            if (f.partsUsed == 3 && time_ >= f.partsAt + battle::PartsGrace)
            {
                endTurn(b, f);
                continue;
            }
            if (time_ < f.deadline)
                continue;
            if (e->typing && !f.extended)
            {
                f.deadline += battle::TypingExtra;
                f.extended = true;
                continue;
            }
            if (f.moved || f.acted)
                f.timeouts = 0;                     // (A turn used, left to run out: not a turn let pass.)
            else
            {
                if (++f.timeouts >= battle::AwayAfter)
                    f.away = true;
                fightLine(b, f.id, {}, "timeout", e->name + " lets the moment pass.");
            }
            endTurn(b, f);
        }
    }
    for (const auto& b : battles_)
        if (b.over && time_ >= b.overAt + battle::BannerSeconds + battle::FadeSeconds)
            heardFights_.erase(b.id);
    battles_.erase(std::remove_if(battles_.begin(), battles_.end(),
                                  [&](const Battle& b) { return b.over && time_ >= b.overAt + battle::BannerSeconds + battle::FadeSeconds; }),
                   battles_.end());
}

// ------------------------------------------------------------------ Downed, out of a fight

Result World::struggleUp(const std::string& id)
{
    auto* e = entity(id);
    if (!e || e->dead)
        return {false, "No such character.", {}};
    if (e->downedLeft <= 0)
        return {false, "You are not down.", {}};
    if (inBattle(id))
        return battleAct(id, "struggle");
    if (e->struggleUntil > 0)
        return {false, "You are already trying to get up.", {}};
    if (!recoveryAvailable(*e))
        return {false, "You have no strength left to rise. Only someone tending your wounds can get you up.", {}};
    e->struggleUntil = time_ + battle::StruggleSeconds;
    return {true, "You struggle to get up. It will take a little while.", {}};
}

Result World::tendWounds(const std::string& id, const std::string& target)
{
    auto* e = entity(id);
    auto* t = entity(target);
    if (!e || e->dead)
        return {false, "No such character.", {}};
    if (e->downedLeft > 0)
        return {false, "You are down yourself.", {}};
    if (!t || t->dead || t->downedLeft <= 0)
        return {false, "They don't need tending.", target};
    if (inBattle(target))
        return {false, "They are in a fight: join it to reach them.", target};
    if (t->cellId != e->cellId || std::hypot(t->position.x - e->position.x, t->position.y - e->position.y) > 1.8)
        return {false, "Get next to them first.", target};
    if (e->stamina < battle::TendStamina)
        return {false, "You are too spent to tend them.", target};
    e->tending = target;
    e->tendUntil = time_ + battle::TendSeconds;
    stop(id);
    if (!t->npc)
        notice(target, e->name + " kneels by you and tends your wounds.");
    return {true, "You tend " + t->name + "'s wounds.", target};
}

void World::tendDowned(double dt)
{
    for (auto& [id, e] : entities_)
    {
        if (e.dead || e.lingering)
            continue;                               // (A player gone from the world: their timer waits.)
        if (!e.gift.empty() && e.mana < battle::manaMax(e.wisdom, true) && (battles_.empty() || !inBattle(id)))
            e.mana = std::min(battle::manaMax(e.wisdom, true), e.mana + battle::ManaPerSecond * dt);
        // Tending someone, out of a fight: done after a while, if they're both still there.
        if (e.tendUntil > 0 && time_ >= e.tendUntil)
        {
            e.tendUntil = 0;
            auto* t = entity(e.tending);
            if (t && !t->dead && t->downedLeft > 0 && !inBattle(t->id) && t->cellId == e.cellId &&
                std::hypot(t->position.x - e.position.x, t->position.y - e.position.y) <= 2.2 && e.stamina >= battle::TendStamina)
            {
                e.stamina -= battle::TendStamina;
                standUp(*t, battle::TendedHealth);
                if (!t->npc)
                    notice(t->id, e.name + " gets you back on your feet.");
                if (!e.npc)
                    notice(id, t->name + " is back on their feet.");
                recordEvent({"tended", id, t->id, e.cellId, 0, 0, {}, 0, 0, {}});
            }
            e.tending.clear();
        }
        if (e.downedLeft <= 0 || inBattle(id))
            continue;
        if (e.struggleUntil > 0 && time_ >= e.struggleUntil)
        {
            standUp(e, battle::StruggleUpHealth);
            e.recoveryUsed = std::floor(calendarDays_);
            if (!e.npc)
                notice(id, "You struggle back to your feet.");
            continue;
        }
        // A resident or a bandit left lying gets up if it can.
        if (e.npc && e.struggleUntil <= 0 && recoveryAvailable(e))
            e.struggleUntil = time_ + battle::StruggleSeconds;
        e.downedLeft -= dt;
        if (e.downedLeft <= 0 && !e.npc)
        {
            standUp(e, battle::GetUpHealth);
            notice(id, "You get back on your feet, sore all over.");
        }
        else if (e.downedLeft <= 0)
        {
            e.downedLeft = 0;
            setDead(id, true);
        }
    }
}

void World::restPlayers(double dt)
{
    // Rest (doc 38): lying or sitting still, out of a fight and not down, without a break, is a partial rest; six hours
    // of it lying in a bed is a full rest.
    for (auto& [id, e] : entities_)
    {
        if (e.npc || e.dead || e.lingering)
            continue;
        if (e.fullRestDay < 0)
            e.fullRestDay = calendarDays_;          // (Characters from before rest was kept: counted from now.)
        const bool still = (e.posture == "lying" || e.posture == "sitting") && e.path.empty() &&
                           std::hypot(e.velocity.x, e.velocity.y) < .05;
        if (!e.injuries.empty())
        {
            // Rest heals injuries (doc 38): lying in a bed fastest, still anywhere half that, up and about a little.
            const bool fighting = e.downedLeft > 0 || (!battles_.empty() && inBattle(id));
            const bool sprinting = effectivePace(e) >= 9 && std::hypot(e.velocity.x, e.velocity.y) > .05;
            healInjuries(e, dt / battle::RestHourSeconds * (fighting || sprinting ? 0 : still ? (inBed(e) ? 1.5 : .75) : .25));
        }
        if (!still || e.downedLeft > 0 || (!battles_.empty() && inBattle(id)))
        {
            e.restRun = e.bedRun = 0;
            continue;
        }
        const double hours = dt / battle::RestHourSeconds;
        e.restRun += hours;
        if (!inBed(e))
        {
            e.bedRun = 0;
            continue;
        }
        const double before = e.bedRun;
        e.bedRun += hours;
        if (before < battle::FullRestHours && e.bedRun >= battle::FullRestHours)
        {
            fullRest(e);
            notice(id, "You have slept well. Your strength is coming back.");
        }
    }
}

bool World::inBed(const Entity& e) const
{
    if (e.posture != "lying")
        return false;
    const auto* c = cell(e.cellId);
    const auto* t = c && c->loaded ? c->tile(int(std::floor(e.position.x)), int(std::floor(e.position.y))) : nullptr;
    return t && !t->solid && (t->glyph == 'b' || t->glyph == 'z');
}

void World::fullRest(Entity& e)
{
    e.downsSinceRest = 0;
    e.recoveryUsed = -1;
    e.fullRestDay = calendarDays_;
}

void World::returnFromAway(Entity& e)
{
    if (e.awaySince < 0)
        return;
    const double away = std::max(0.0, (calendarDays_ - e.awaySince) * calendar::SecondsPerDay);
    const bool bed = e.awayInBed;
    e.awaySince = -1;
    e.awayInBed = false;
    // Down when they left: the time away counts it down, and none of it is rest.
    if (e.downedLeft > 0 && !e.dead)
    {
        e.downedLeft -= away;
        if (e.downedLeft > 0)
            return;
        standUp(e, battle::GetUpHealth);
        e.restRun = e.bedRun = 0;
        return;
    }
    healInjuries(e, away / battle::RestHourSeconds * (bed ? 1.5 : .75));   // Time away heals as rest (doc 38).
    const double hours = away / battle::RestHourSeconds * battle::AwayRestRate;
    e.restRun += hours;
    if (!bed)
        return;
    const double before = e.bedRun;
    e.bedRun += hours;
    if (before < battle::FullRestHours && e.bedRun >= battle::FullRestHours)
        fullRest(e);
}

// ------------------------------------------------------------------ Facing, truces

Result World::battleFace(const std::string& id, int dir)
{
    auto* b = battleFor(id);
    if (!b)
        return {false, "You are not in a fight.", {}};
    if (b->over)
        return {false, "The fight is over.", {}};
    auto& f = *b->fighter(id);
    f.away = false;
    f.timeouts = 0;
    if (!f.acting)
        return {false, "You can turn only on your turn.", {}};
    if (f.status != "fighting")
        return {false, "You can't turn now.", {}};
    if (dir < 0 || dir > 7)
        return {false, "Face which way?", {}};
    f.facing = dir;                                 // Free: it costs neither the move nor the action.
    f.turned = true;                                // (Kept, over the way a walk under way would turn it.)
    f.faced = true;                                 // A turn's third part (doc 33); turning again puts its end off.
    f.partsAt = time_;
    return {true, {}, {}};
}

Result World::offerTruce(const std::string& id)
{
    auto* b = battleFor(id);
    if (!b || b->over)
        return {false, "You are not in a fight.", {}};
    auto& f = *b->fighter(id);
    if (!f.acting || f.status != "fighting")
        return {false, "You can offer a truce only on your turn.", {}};
    if (!b->truceBy.empty())
        return answerTruce(id, true);
    if (f.acted)
        return {false, "You have already acted this turn.", {}};
    b->truceBy = id;
    for (auto& o : b->fighters)
        o.truce = o.id == id;
    f.acted = true;
    const auto* e = entity(id);
    fightLine(*b, id, {}, "truce", e->name + " offers a truce.");
    for (const auto& o : b->fighters)
        if (const auto* oe = entity(o.id); oe && !oe->npc && o.id != id && o.status == "fighting")
            notice(o.id, e->name + " offers a truce. Agree or refuse.");
    checkOver(*b);
    if (!b->over && f.acting && f.moved && e->npc)
        endTurn(*b, f);
    return {true, "You offer a truce.", {}};
}

Result World::answerTruce(const std::string& id, bool agree)
{
    auto* b = battleFor(id);
    if (!b || b->over || b->truceBy.empty())
        return {false, "No truce is on offer.", {}};
    auto& f = *b->fighter(id);
    if (f.status != "fighting")
        return {false, "Only those still standing decide.", {}};
    const auto* e = entity(id);
    if (!agree)
    {
        b->truceBy.clear();
        for (auto& o : b->fighters)
            o.truce = false;
        fightLine(*b, id, {}, "truce", e->name + " refuses the truce.");
        return {true, "You refuse the truce.", {}};
    }
    f.truce = true;
    fightLine(*b, id, {}, "truce", e->name + " agrees to the truce.");
    checkOver(*b);
    return {true, "You agree to the truce.", {}};
}

// ------------------------------------------------------------------ The mouth slot, and things on the ground

Result World::holdItem(const std::string& id, const std::string& item)
{
    auto* e = entity(id);
    if (!e || e->dead)
        return {false, "No such character.", {}};
    if (items::baseOf(item) != "sword")
        return {false, "You can't hold that in your mouth.", {}};
    if (e->downedLeft > 0)
        return {false, "You are down.", {}};
    if (!e->mouth.empty())
        return {false, "Your mouth is already full.", {}};
    const auto* purse = society_.account(id);
    if (!purse)
        return {false, "You have no sword.", {}};
    // Any sword (doc 35, Part 4): the one named, else the best one has.
    std::string kind = item != "sword" && Society::stock(*purse, item) > 0 ? item : std::string();
    if (kind.empty())
        for (const auto& [held, n] : purse->stock)
            if (n > 0 && items::baseOf(held) == "sword" && (kind.empty() || items::qualityOf(held) > items::qualityOf(kind)))
                kind = held;
    if (kind.empty())
        return {false, "You have no sword.", {}};
    e->mouth = "sword";
    e->swordKind = kind == "sword" ? std::string() : kind;
    const auto* good = items::good(kind);
    std::string name = good && kind != "sword" ? good->name : std::string("sword");
    if (!name.empty())
        name[0] = char(std::tolower(static_cast<unsigned char>(name[0])));
    return {true, "You take the " + name + " in your jaws.", {}};
}

Result World::stowItem(const std::string& id)
{
    auto* e = entity(id);
    if (!e || e->mouth.empty())
        return {false, "Your mouth is empty.", {}};
    const auto item = e->mouth;
    e->mouth.clear();
    e->swordKind.clear();
    return {true, "You put the " + item + " away.", {}};
}

Result World::takeItem(const std::string& id, const std::string& groundId)
{
    auto* e = entity(id);
    auto it = std::find_if(ground_.begin(), ground_.end(), [&](const GroundItem& g) { return g.id == groundId; });
    if (!e || e->dead || e->downedLeft > 0)
        return {false, "You can't.", {}};
    if (it == ground_.end())
        return {false, "It's gone.", {}};
    if (it->cellId != e->cellId || std::hypot(it->x - e->position.x, it->y - e->position.y) > 1.8)
        return {false, "Get closer first.", {}};
    society_.openAccount(GroundAccount);
    if (!society_.shift(GroundAccount, id, it->item, 1, 0, "picked up"))
        return {false, "You can't carry it.", {}};
    const auto item = it->item;
    ground_.erase(it);
    return {true, "You pick up the " + item + ".", {}};
}

void World::dropItem(Battle& b, BattleFighter& f)
{
    auto* e = entity(f.id);
    if (!e || e->mouth.empty())
        return;
    const auto item = e->mouth == "sword" ? swordHeld(*e) : e->mouth;   // (That very sword, of its kind.)
    society_.openAccount(GroundAccount);
    if (!society_.shift(f.id, GroundAccount, item, 1, 0, "knocked loose in a fight"))
        return;
    e->mouth.clear();
    e->swordKind.clear();
    b.drops.push_back({f.x, f.y, item, f.id});
    fightLine(b, f.id, {}, "drop", "The " + item + " is knocked from " + e->name + "'s jaws.");
}

Result World::giveGift(const std::string& id, const std::string& gift, bool quickened)
{
    auto* e = entity(id);
    if (!e)
        return {false, "No such character.", {}};
    if (!gift.empty() && gift != "fire")
        return {false, "The only Gift known to the game is fire.", {}};
    e->gift = gift;
    e->quickened = !gift.empty() && quickened;
    e->mana = battle::manaMax(e->wisdom, !gift.empty());
    return {true, gift.empty() ? e->name + " has no Gift." : e->name + (quickened ? " is Quickened: fire." : " is Gifted: fire."), id};
}

void World::growSkill(Entity& e, double amount)
{
    if (!e.npc)
        e.fightingSkill = std::min(100.0, e.fightingSkill + amount * (1 - e.fightingSkill / 120));
}

// ------------------------------------------------------------------ Hurting

void World::hurtFighter(Battle& b, BattleFighter& t, double damage, double downedBase, const std::string& by, bool interrupt)
{
    auto* d = entity(t.id);
    if (!d || t.status != "fighting")
        return;
    damage = huntBlow(b, t, damage, downedBase, by);   // An animal (doc 41): its own health, and the blow remembered.
    d->hurt += damage;
    injureOnBlow(b, t, damage, downedBase, by);     // A heavy blow can leave an injury that outlasts the fight (doc 38).
    if (d->npc)
        stop(t.id);
    // Combat injuries (doc 38): a hard bite or cut bleeds; a very hard blow staggers.
    if (!by.empty() && downedBase == battle::DownedBite && damage >= battle::BleedFrom && d->hurt < 100)
    {
        if (t.bleeding <= 0)
            fightLine(b, t.id, {}, "bleeding", d->name + " is bleeding.");
        t.bleeding = battle::BleedTurns;
    }
    if (!by.empty() && damage >= battle::StaggerFrom && d->hurt < 100 && t.staggered == 0)
    {
        fightLine(b, t.id, {}, "stagger", d->name + " staggers.");
        if (t.acting)
            t.staggered = 1;
        else
        {
            t.meter = std::max(0.0, t.meter - battle::StaggerSetback);
            t.staggered = 2;
        }
    }
    if (!by.empty() && !b.truceBy.empty())
    {
        b.truceBy.clear();
        for (auto& o : b.fighters)
            o.truce = false;
        fightLine(b, by, {}, "truce", "The truce is off.");
    }
    if (interrupt && t.casting)
    {
        // Hit while gathering the fire: it breaks off, and half the mana is lost.
        for (auto it = b.casts.begin(); it != b.casts.end(); ++it)
            if (it->caster == t.id)
            {
                d->mana = std::min(battle::manaMax(d->wisdom, !d->gift.empty()), d->mana + it->mana / 2);
                b.casts.erase(it);
                break;
            }
        t.casting = false;
        fightLine(b, t.id, {}, "break", d->name + "'s fire breaks off.");
    }
    // A hard blow can knock a sword from the jaws.
    if (damage >= battle::KnockLooseFrom && d->mouth == "sword" &&
        chance(t.id + "|loose", std::int64_t(b.seq) * 13 + b.turns) < std::max(0.0, .2 - d->strength / 1000))
        dropItem(b, t);
    // On a duel's terms (doc 37): the first wound ends a fight to first blood; to yield, who would go down yields.
    if (b.terms != "death" && (d->hurt >= 100 || (b.terms == "blood" && damage > 0 && !by.empty())))
    {
        d->hurt = std::min(d->hurt, 99.0);
        yieldFighter(b, t, by, b.terms == "blood" && d->hurt < 99 ? d->name + " is blooded, and yields." : d->name + " can fight no more, and yields.");
        return;
    }
    if (d->hurt >= 100)
    {
        const double overkill = d->hurt - 100;
        d->hurt = 100;
        downFighter(b, t, overkill, downedBase, by);
    }
}

void World::yieldFighter(Battle& b, BattleFighter& f, const std::string& to, const std::string& line)
{
    // Out of the fight, on their feet: they stay where they are until it ends, and walk away from it.
    f.status = "yielded";
    f.acting = false;
    f.burning = 0;
    if (f.casting)
    {
        f.casting = false;
        b.casts.erase(std::remove_if(b.casts.begin(), b.casts.end(), [&](const BattleCast& c) { return c.caster == f.id; }), b.casts.end());
    }
    if (b.yieldBy == f.id)
        b.yieldBy.clear();
    fightLine(b, f.id, to, "yield", line);
    checkOver(b);
}

Result World::offerYield(const std::string& id)
{
    auto* b = battleFor(id);
    if (!b)
        return {false, "You are not in a fight.", {}};
    if (b->over)
        return {false, "The fight is over.", {}};
    auto& f = *b->fighter(id);
    f.away = false;
    f.timeouts = 0;
    if (f.status != "fighting")
        return {false, "You can't yield now.", {}};
    if (!b->yieldBy.empty())
        return {false, "Someone has already offered to yield.", {}};
    const auto* e = entity(id);
    // Is there anyone on the other side to answer? If only NPCs stand there, they accept.
    bool asked = false;
    for (const auto& o : b->fighters)
        if (o.side != f.side && o.status == "fighting")
            if (const auto* oe = entity(o.id); oe && !oe->npc)
                asked = true;
    if (!asked)
    {
        yieldFighter(*b, f, {}, e->name + " yields, and is let be.");
        return {true, "You yield. They let you be.", {}};
    }
    b->yieldBy = id;
    b->yieldUntil = time_ + battle::YieldSeconds;
    fightLine(*b, id, {}, "yield", e->name + " offers to yield.");
    return {true, "You offer to yield.", {}};
}

Result World::answerYield(const std::string& id, bool accept)
{
    auto* b = battleFor(id);
    if (!b || b->over || b->yieldBy.empty())
        return {false, "No one is offering to yield.", {}};
    auto* f = b->fighter(id);
    auto* y = b->fighter(b->yieldBy);
    const auto* e = entity(id);
    if (!f || !y || !e || f->side == y->side || f->status != "fighting")
        return {false, "It isn't yours to answer.", {}};
    f->away = false;
    f->timeouts = 0;
    const auto* ye = entity(y->id);
    if (!accept)
    {
        b->yieldBy.clear();
        fightLine(*b, id, y->id, "refuse", e->name + " will not take " + (ye ? ye->name : std::string("their")) + "'s yield. The fight goes on.");
        return {true, "You fight on.", {}};
    }
    yieldFighter(*b, *y, id, (ye ? ye->name : std::string("They")) + " yields, and " + e->name + " lets them be.");
    return {true, "You let them be.", {}};
}

Result World::swordStrike(Battle& b, BattleFighter& f, const std::string& target)
{
    auto* t = b.fighter(target);
    auto* e = entity(f.id);
    auto* d = entity(target);
    if (e->mouth != "sword")
        return {false, "You have no sword in your jaws.", target};
    if (!t || !d || t->side == f.side || t->status != "fighting")
        return {false, t && t->status == "downed" ? d->name + " is already down." : "Strike whom?", target};
    const int apart = tilesApart(f.x, f.y, t->x, t->y);
    if (apart > battle::SwordReach)
        return {false, "They are out of your reach.", target};
    if (apart == 2)
    {
        // The blade's reach needs the tile between them clear of walls.
        const int mx = f.x + (t->x - f.x) / 2, my = f.y + (t->y - f.y) / 2;
        if (!standable(b.cellId, {mx + .5, my + .5}))
            return {false, "Something is in the way.", target};
    }
    const double swordCost = battle::SwordStamina + injury::effects(e->injuries).attackStamina;   // (Hurt ribs, doc 38.)
    if (e->exhausted || e->stamina < swordCost)
        return {false, "You are too winded to swing.", target};
    e->stamina -= swordCost;
    if (e->stamina <= 0)
    {
        e->stamina = 0;
        e->exhausted = true;
    }
    if (e->npc)
        f.facing = battle::octant(t->x - f.x, t->y - f.y);
    f.acted = true;
    f.weight = std::max(f.weight, battle::SwordWeight);
    if (t->guarding)
        t->facing = battle::octant(f.x - t->x, f.y - t->y);
    const double hit = strikeChance(f, *t);
    const auto key = std::int64_t(b.seq) * 7919 + b.turns;
    const double r = chance(f.id + "|sword|" + target, key);
    // The sword's kind (doc 35, Part 4): a finer blade cuts deeper; and every swing wears it (RatwDurability.cpp).
    const auto blade = swordHeld(*e);
    const double edge = items::qualityDamage(items::qualityOf(blade));
    wearGear(*e, blade, 1);
    if (r >= hit)
    {
        fightLine(b, f.id, target, "miss", e->name + " swings at " + d->name + " and misses.");
        return {true, "You swing at " + d->name + " and miss.", target};
    }
    const bool graze = r >= hit - .1;
    double damage = battle::SwordDamage * edge * (.6 + e->strength / 125) * (.85 + .3 * chance(target + "|sword|" + f.id, key));
    damage = std::max(1.0, damage - injury::effects(e->injuries).swordLess);   // (A hurt neck or shoulder, doc 38.)
    if (graze)
        damage /= 2;
    const auto landed = land(f, *t, *d, damage, "cut", key);      // The bit-sword: a cut, no pierce (doc 35, 2.1).
    wearArmourAt(*d, landed.zone, damage - landed.damage);
    damage = landed.damage;
    fightLine(b, f.id, target, graze ? "graze" : "slash", e->name + (graze ? " nicks " : " cuts ") + d->name + landed.words + " (" + whole(damage) + ").");
    growSkill(*e, battle::SkillPerHit);
    hurtFighter(b, *t, damage, battle::DownedBite, f.id, true);
    if (t->status == "downed")
        return {true, "Your blade takes " + d->name + " down.", target};
    return {true, std::string(graze ? "You nick " : "You cut ") + d->name + ".", target};
}

Result World::shove(Battle& b, BattleFighter& f, const std::string& target)
{
    // A push (doc 37): an adjacent wolf a tile straight back, out of a doorway or toward the edge, strength against
    // strength (less against one on guard). Nowhere for them to go, and it is only a shove.
    auto* t = b.fighter(target);
    auto* e = entity(f.id);
    auto* d = entity(target);
    if (!t || !d || t->id == f.id || t->status != "fighting")
        return {false, "Shove whom?", target};
    if (tilesApart(f.x, f.y, t->x, t->y) != 1)
        return {false, "Get next to them first.", target};
    if (e->exhausted || e->stamina < battle::ShoveStamina)
        return {false, "You are too winded to shove.", target};
    e->stamina -= battle::ShoveStamina;
    if (e->stamina <= 0)
    {
        e->stamina = 0;
        e->exhausted = true;
    }
    if (e->npc)
        f.facing = battle::octant(t->x - f.x, t->y - f.y);
    f.acted = true;
    const int nx = t->x + (t->x - f.x), ny = t->y + (t->y - f.y);
    if (!b.inArena(nx, ny) || !arenaOpen(b, nx, ny, t->id) || !stepBetween(b.cellId, t->x, t->y, nx, ny))
    {
        fightLine(b, f.id, target, "shove", e->name + " shoves " + d->name + ", but there is nowhere for them to go.");
        return {true, "You shove " + d->name + ", but there is nowhere for them to go.", target};
    }
    const double odds = std::clamp(battle::ShoveOdds + (e->strength - d->strength) / 100 - (t->guarding ? battle::GuardDodge : 0), .2, .9);
    if (chance(f.id + "|shove|" + target, std::int64_t(b.seq) * 7919 + b.turns) >= odds)
    {
        fightLine(b, f.id, target, "shove", e->name + " shoves at " + d->name + ", but " + d->name + " holds their ground.");
        return {true, d->name + " holds their ground.", target};
    }
    t->x = nx;
    t->y = ny;
    t->walk.clear();                                // (A walk under way is stopped where it stands.)
    fightLine(b, f.id, target, "shove", e->name + " shoves " + d->name + " back.");
    return {true, "You shove " + d->name + " back.", target};
}

Result World::castFlame(Battle& b, BattleFighter& f, int x, int y)
{
    auto* e = entity(f.id);
    if (e->gift != "fire")
        return {false, "You have no Gift of fire.", {}};
    const auto& spell = e->quickened ? battle::QuickenedFlame : battle::GiftedFlame;
    if (x == f.x && y == f.y)
        return {false, "Aim it: which way?", {}};
    if (e->exhausted || e->stamina < spell.stamina)
        return {false, "You haven't the breath for it.", {}};
    BattleCast cast;
    cast.caster = f.id;
    cast.spell = "flame";
    cast.dir = battle::octant(x - f.x, y - f.y);
    cast.quickened = e->quickened;
    cast.castAt = time_;
    cast.firesAt = time_ + spell.charge / (1 + e->wisdom / 200);   // A hard countdown: bars that fill in time may dodge.
    const double aim = std::atan2(double(y - f.y), double(x - f.x));
    for (int ty = b.y0; ty < b.y0 + b.h; ++ty)
        for (int tx = b.x0; tx < b.x0 + b.w; ++tx)
        {
            const double dx = tx - f.x, dy = ty - f.y, far = std::hypot(dx, dy);
            if (far < .5 || far > spell.length + .5)
                continue;
            double off = std::abs(std::atan2(dy, dx) - aim) * 180 / Pi;
            if (off > 180)
                off = 360 - off;
            if (off <= spell.halfAngle + 1e-6 && standable(b.cellId, {tx + .5, ty + .5}))
                cast.tiles.push_back({tx, ty});
        }
    // Mana, breath, and a singed muzzle. At no mana it still comes, at double the burn: a last resort.
    const bool desperate = e->mana < spell.mana;
    cast.mana = desperate ? e->mana : spell.mana;
    e->mana -= cast.mana;
    e->stamina -= spell.stamina;
    if (e->npc)
        f.facing = cast.dir;
    f.acted = true;
    f.weight = std::max(f.weight, spell.weight);
    fightLine(b, f.id, {}, "charge", e->name + " draws a deep breath; heat shimmers at their jaw.");
    b.log.back().tiles = cast.tiles;
    hurtFighter(b, f, spell.self * (desperate ? 2 : 1), battle::DownedFire, {}, false);
    if (f.status != "fighting")
        return {true, "The fire turns on you.", {}};
    f.casting = true;
    b.casts.push_back(cast);
    f.moved = true;                                 // (No moving while it gathers.)
    if (e->npc)
        endTurn(b, f);                              // (A player may still turn, or talk, till their time or End turn.)
    return {true, "You gather the fire.", {}};
}

void World::resolveCast(Battle& b, const BattleCast& cast)
{
    auto* cf = b.fighter(cast.caster);
    auto* ce = entity(cast.caster);
    if (!cf || !ce || cf->status != "fighting")
        return;
    cf->casting = false;
    const auto& spell = cast.quickened ? battle::QuickenedFlame : battle::GiftedFlame;
    fightLine(b, cast.caster, {}, "flame", ce->name + (cast.quickened ? " looses a roaring blaze!" : " breathes a gout of fire!"));
    b.log.back().tiles = cast.tiles;
    int alive = 0;
    for (const auto& o : b.fighters)
        alive += o.status == "fighting" || o.status == "downed";
    for (const auto& t : cast.tiles)
        b.smoke.push_back({t, b.turns + battle::SmokeRounds * std::max(1, alive)});
    const auto key = std::int64_t(b.seq) * 104729 + b.turns;
    for (auto& t : b.fighters)
    {
        if (t.id == cast.caster || t.status != "fighting" ||
            std::find(cast.tiles.begin(), cast.tiles.end(), std::pair<int, int>{t.x, t.y}) == cast.tiles.end())
            continue;
        auto* d = entity(t.id);
        if (!d)
            continue;
        const auto here = environmentAt(b.cellId, {t.x + .5, t.y + .5});
        const bool heavyRain = (here.weather == Weather::Rain || here.weather == Weather::Storm) && here.intensity >= .5;
        double damage = spell.damage * (.5 + ce->wisdom / 100) * (.85 + .3 * chance(t.id + "|fire", key)) * (heavyRain ? battle::RainFactor : 1) +
                        injury::effects(d->injuries).fireExtra;   // (Burned before, doc 38.)
        const auto* tile = cell(b.cellId) ? cell(b.cellId)->tile(t.x, t.y) : nullptr;
        const bool water = tile && tile->terrain == Terrain::Water;
        fightLine(b, cast.caster, t.id, "burnt", d->name + " is caught in the fire (" + whole(damage) + ").");
        growSkill(*ce, battle::SkillPerHit);
        hurtFighter(b, t, damage, battle::DownedFire, cast.caster, true);
        if (t.status != "fighting")
            continue;
        if (!water)
            t.burning = battle::BurnTurns;
        if (d->npc && 100 - d->hurt < 50)
            t.scared = true;                        // Fear: it runs.
    }
}

// ------------------------------------------------------------------ The world around a fight

void World::linger(const std::string& id)
{
    auto* e = entity(id);
    auto* b = battleFor(id);
    if (!e || !b)
        return;
    e->lingering = true;
    e->typing = false;
    if (auto* f = b->fighter(id))
        f->away = true;
}

void World::stopLingering(const std::string& id)
{
    if (auto* e = entity(id))
        e->lingering = false;
    if (auto* b = battleFor(id))
        if (auto* f = b->fighter(id))
        {
            f->away = false;
            f->timeouts = 0;
        }
}

void World::tendFightSurroundings(Battle& b)
{
    // Those nearby who can only hear the fight are told so, once.
    std::set<std::string> fighters;
    for (const auto& f : b.fighters)
        if (f.status != "fled")
            fighters.insert(f.id);
    auto& told = heardFights_[b.id];
    for (const Entity* o : entitiesIn(b.cellId))
    {
        if (!o || o->npc || o->cellId != b.cellId || fighters.count(o->id) || told.count(o->id) || b.observers.count(o->id))
            continue;
        bool sees = false, hears = false;
        for (const auto& id : fighters)
        {
            const auto* fe = entity(id);
            if (!fe || std::hypot(fe->position.x - o->position.x, fe->position.y - o->position.y) > battle::NoiseReach)
                continue;
            sees = sees || visionClarity(o->id, id) > 0;
            hears = hears || hearingClarity(o->id, id, Voice::Yell) > 0;
        }
        if (sees || !hears)
            continue;
        told.insert(o->id);
        notice(o->id, "You hear the sounds of a fight nearby: snarls, a yelp, scrabbling claws.");
    }
}

} // namespace ratw
