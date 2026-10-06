// Gifts in a fight (Docs/Design/43-gifts.md): every family's fight abilities, Gifted and Quickened, as World members.
// The fight's own rules (turns, blows, fire) are in RatwBattle.cpp; it calls in here at a turn's start, a step, a blow,
// a hit and a cast going off. What each ability is called, its kind and its mana are in Data/Gifts/families.json; how
// it is aimed, how far it reaches and what it does are here. Every number is a placeholder for play-testing.
#include "RatwBattle.h"
#include "RatwGifts.h"
#include "RatwInjury.h"
#include "RatwItems.h"
#include "RatwSociety.h"
#include "RatwWorld.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <sstream>

namespace ratw
{
namespace
{
std::uint64_t roll(const std::string& a, std::int64_t b)
{
    // Mixed through (splitmix64's finish): a plain sum moved each roll only a little when the fight's sequence grew by
    // the same steps each round, so a wolf rolled the same zone and nearly the same odds round after round (doc 45).
    std::uint64_t x = std::hash<std::string>{}(a) ^ (std::uint64_t(b) * 0x9e3779b97f4a7c15ULL);
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    return x ^ (x >> 31);
}
double chance(const std::string& a, std::int64_t b)
{
    return double(roll(a, b) % 10000) / 10000.0;
}
int apart(int ax, int ay, int bx, int by)
{
    return std::max(std::abs(ax - bx), std::abs(ay - by));
}
std::string whole(double n)
{
    return std::to_string(std::max(1L, std::lround(n)));
}
bool standing(const BattleFighter& f)
{
    return f.status == "fighting";
}

// How an ability is aimed and what it takes (doc 43). `target`: "self" (none), "foe", "ally" (oneself too), "downed"
// (one's own side, down), "any" (a fighter or a tile), "tile", "dir" (a way, toward a tile), "shape" (painted tiles),
// "foe+tile" (a foe, then where it goes). `range` in tiles (1: next to it); `tiles`: a shape's most, an area's size.
// `gather`: seconds before wisdom (÷ (1 + WIS/200)), 0 at once. `stamina`, `self` (hurt): what the Tell costs.
struct Rule
{
    const char* id;
    const char* target;
    int range;
    int tiles;
    double gather;
    double stamina;
    double self;
    double weight;
};

constexpr Rule Rules[] = {
    // Fire, Gifted: no damage, by touch.
    {"cauterize", "ally", 1, 0, 0, 3, 1, 0},
    {"flare", "foe", 2, 0, 0, 3, 0, 0},              // (Two tiles: a blade's reach, doc 47.)
    {"smother_to_smoke", "tile", 5, 0, 0, 6, 1, 0},
    {"warm_through", "ally", 1, 0, 0, 6, 1, 0},
    {"heat_sense", "self", 0, 6, 0, 4, 0, 0},
    // Fire, Quickened.
    {"flamethrower", "dir", 5, 0, 3, 20, 5, 10},
    {"heat_lance", "dir", 4, 0, 2, 12, 3, 10},
    {"blastwave", "self", 1, 0, 0, 14, 3, 10},
    {"wall_of_fire", "shape", 10, 10, 2.5, 8, 3, 10},
    // Earth, Gifted.
    {"loosen_ground", "tile", 5, 0, 0, 6, 0, 0},
    {"firm_footing", "ally", 1, 0, 0, 6, 0, 0},
    {"feel_footfalls", "self", 0, 6, 0, 4, 0, 0},
    // Earth, Quickened.
    {"upheaval", "tile", 6, 0, 2, 12, 0, 10},
    {"hurl_stone", "foe", 10, 0, 3, 14, 0, 10},
    {"fissure", "shape", 10, 10, 2.5, 10, 0, 10},
    {"stone_wall", "shape", 10, 5, 2, 10, 0, 10},
    {"stone_armor", "self", 0, 0, 0, 0, 0, 0},
    // Water, Gifted.
    {"douse", "any", 3, 0, 0, 2, 0, 0},
    {"slick", "tile", 5, 0, 0, 2, 0, 0},
    {"splash_eyes", "foe", 3, 0, 0, 2, 0, 0},
    {"wash_out", "ally", 1, 0, 0, 2, 0, 0},
    // Water, Quickened.
    {"pressure_jet", "dir", 5, 0, 0, 4, 0, 10},
    {"wave", "dir", 4, 3, 3, 8, 0, 10},
    {"freeze", "foe", 6, 0, 0, 4, 0, 0},
    {"flood", "tile", 6, 3, 2.5, 6, 0, 10},
    {"water_screen", "self", 0, 0, 0, 0, 0, 0},
    // Wind, Gifted.
    {"turn_the_wind", "self", 0, 0, 0, 4, 0, 0},
    {"clear_the_air", "tile", 5, 3, 0, 6, 0, 0},
    {"back_breeze", "ally", 3, 0, 0, 6, 0, 0},
    {"air_blast", "foe", 3, 0, 0, 6, 0, 0},
    // Wind, Quickened.
    {"battering_gust", "dir", 5, 0, 0, 5, 0, 10},
    {"pressure_drop", "tile", 6, 2, 2, 10, 0, 10},
    {"steal_breath", "foe", 4, 0, 0, 8, 0, 0},
    {"whirlwind", "self", 1, 0, 0, 6, 0, 10},
    // Sound, Gifted.
    {"hush", "self", 0, 2, 0, 3, 0, 0},
    {"throw_voice", "tile", 8, 6, 0, 3, 0, 0},
    {"steady_beat", "self", 0, 3, 0, 3, 0, 0},
    {"whisper_thread", "ally", 99, 0, 0, 0, 0, 0},
    // Sound, Quickened.
    {"shatterhowl", "dir", 5, 0, 2, 6, 0, 10},
    {"resonance", "foe", 5, 0, 0, 4, 0, 0},
    {"thunderclap", "self", 2, 0, 0, 6, 0, 10},
    {"dread_note", "self", 4, 0, 0, 4, 0, 0},
    // Blinker, Gifted.
    {"slip", "self", 0, 0, 0, 0, 0, 0},
    {"interpose", "self", 0, 0, 0, 0, 0, 0},
    {"blink", "tile", 3, 0, 0, 0, 0, 0},
    // Blinker, Quickened.
    {"blink_strike", "foe", 6, 0, 0, 0, 2, 0},
    {"chain_blink", "foe", 6, 3, 0, 0, 4, 10},
    {"displace", "foe+tile", 1, 4, 0, 0, 2, 0},
    {"unmoor", "foe", 1, 0, 0, 0, 3, 0},
    {"extract", "ally", 4, 0, 0, 0, 2, 0},
    // Gravity, Gifted.
    {"lighten", "ally", 4, 0, 0, 2, 0, 0},
    {"burden", "foe", 4, 0, 0, 2, 0, 0},
    {"anchor", "ally", 4, 0, 0, 2, 0, 0},
    {"lift_up", "downed", 5, 0, 0, 6, 0, 0},
    // Gravity, Quickened.
    {"crush", "foe", 5, 0, 0, 6, 0, 0},
    {"slam", "foe", 5, 0, 0, 8, 0, 10},
    {"well", "tile", 6, 3, 2.5, 8, 0, 10},
    {"hurl", "foe", 4, 4, 0, 8, 0, 0},
    {"weightless", "self", 5, 0, 0, 6, 0, 0},
    // Seer, Gifted.
    {"read_the_line", "foe", 99, 0, 0, 3, 0, 0},
    {"forewarn", "ally", 6, 0, 0, 3, 0, 0},
    {"glimpse_the_order", "self", 0, 0, 0, 3, 0, 0},
    // Seer, Quickened.
    {"seen_opening", "self", 0, 0, 0, 3, 0, 0},
    {"riposte", "self", 0, 0, 0, 3, 0, 0},
    {"doom_mark", "foe", 99, 0, 0, 4, 0, 0},
    {"shared_sight", "self", 0, 3, 0, 3, 0, 0},
};

const Rule* ruleOf(const std::string& id)
{
    for (const auto& r : Rules)
        if (id == r.id)
            return &r;
    return nullptr;
}

// The effects a fighter can carry, for its card: a name and what it does.
struct Fx
{
    const char* id;
    const char* name;
    const char* does;
};
constexpr Fx Effects[] = {
    {"dizzy", "Dizzy", "After a blink: 15% worse at hitting and at dodging."},
    {"blinked", "Blink-dazed", "After a quick blink out of the way: 10% worse at hitting."},
    {"nausea", "Nauseous", "Blinking far: 10% worse at hitting."},
    {"double_vision", "Double vision", "After a glimpse ahead: 10% worse at hitting."},
    {"heavy", "Heavy", "After moving weight: a tile less to move."},
    {"thirsty", "Thirsty", "After drawing water: a quarter less stamina back a turn."},
    {"dehydrated", "Dehydrated", "After moving much water: no stamina back a turn."},
    {"hoarse", "Hoarse", "After a hum: can't whisper along a thread."},
    {"breathless", "Breathless", "Its breath stolen: loses stamina, and can't use a Gift that needs breath."},
    {"splashed", "Splashed", "Water in its eyes: its next blow is 20% less likely."},
    {"forewarned", "Forewarned", "Knows what's coming: the next blow at it is 25% less likely."},
    {"dusted", "Grit in its eyes", "Sees half as far, and 10% worse at hitting, for the fight."},
    {"soaked", "Soaked", "Drenched: can't catch fire, but can be frozen anywhere."},
    {"frozen", "Frozen", "Frozen in place: no move on its next turn."},
    {"pinned", "Held fast", "Frozen fast this turn: it can't move or blink."},
    {"prone", "Knocked down", "On the ground: no move on its next turn."},
    {"deafened", "Deafened", "Hears nothing."},
    {"dread", "Dread", "A low note's fear: 15% worse at hitting."},
    {"unmoored", "Unmoored", "Its mind pulled loose: loses its next turn."},
    {"disoriented", "Disoriented", "15% worse at hitting, and can't plan ahead."},
    {"dissociated", "Dissociated", "Blinked too far: loses its next turn."},
    {"blackout", "Blacked out", "Too much weight: loses its next turn."},
    {"dazed", "Dazed", "Just lifted to its feet: loses its next turn."},
    {"cracked", "Cracked pads", "Too much stone: a tile less to move."},
    {"held", "Held aloft", "Lifted off the ground: loses its turns until dropped."},
    {"crushed", "Crushed", "Terribly heavy: hurt each turn, a tile at most to move."},
    {"burdened", "Burdened", "Everything heavier: a tile less, and more stamina for each tile and blow."},
    {"lightened", "Lightened", "Its load lighter: a tile more, half the stamina a tile."},
    {"anchored", "Anchored", "Can't be shoved, thrown or knocked down."},
    {"firm", "Firm footing", "On packed ground: can't be shoved, harder to hit, and guards better."},
    {"breeze", "Back breeze", "Its next move goes a tile further, for no stamina."},
    {"weightless", "Weightless", "This turn: 3 tiles further, for no stamina."},
    {"seen_opening", "Seen opening", "Its next blow can't miss, and finds the least armoured spot."},
    {"riposte", "Riposte", "The next blow at it misses, and it strikes back."},
    {"doomed", "Doomed", "Can't dodge: everyone hits it more easily."},
    {"warmed", "Warmed through", "Can't be soaked or frozen this fight."},
    {"seared", "Seared", "Its wounds seared shut: it doesn't bleed."},
    {"washed", "Washed", "No scent to catch."},
    {"stone_armor", "Stone armour", "Heavy stone on every side: no blow lands harder from the side or behind, and every blow does less. A little slower."},
    {"water_screen", "Water screen", "No blow lands harder from the side or behind; fire does a quarter."},
    {"read", "Read", "Its next move is known to the other side."},
    {"trance", "Trance", "Its Gift rises to the odds: mana each turn, a quicker bar, a second wind, blows warded off. Fatigue after."},
    {"warded", "Ward", "A Gift held on it."},
};

const Fx* fxOf(const std::string& id)
{
    for (const auto& f : Effects)
        if (id == f.id)
            return &f;
    return nullptr;
}

// A Trance (doc 45): a Quickened wolf may go into one at any time in a fight, for nothing. Its level is how far its side is
// outnumbered, foes standing for each of its side standing (TranceFloor at least, TranceCap at most), taken again at each
// of its turns: it rises as the fight turns against the side and falls as the foes do. At each of its turns it gives mana
// (never past the pool). After the fight: Trance fatigue, by the highest level reached and the overreaches, until a full
// rest.
constexpr double TranceFloor = 2, TranceCap = 5, TranceMana = 10;
// A depth of 1 (doc 45's table) fills the bar TranceHaste faster, gives TranceBreath more stamina back a turn
// (RatwBattle.h), and takes TranceWard off every blow at the wolf (TranceWardMost at most, unless the family's says
// otherwise).
constexpr double TranceHaste = .5, TranceWard = .2, TranceWardMost = .85;
// An overreach costs this much health more than the last, in a fight (doc 45).
constexpr double OverreachHurt = 3;

// How deep a Trance runs (doc 45), by family (the user: "not the same for every kind"): its degree at 3:1 and at 5:1, half
// the 3:1 at 2:1, and in between along the way. A degree of 1 gives the bar TranceHaste faster, blows and Gifts
// TranceMight harder, and blows at it TranceWard less (half at most).
struct TranceDepth
{
    double at3, at5, at2 = -1, at4 = -1;            // (at2 below 0: half the 3:1; at4 below 0: halfway from 3:1 to 5:1.)
    double wardMost = TranceWardMost;               // The most its Ward takes off a blow.
};
TranceDepth tranceDepth(const std::string& family)
{
    // (Tuned in Tests/level_sim.cpp, suite "trance": a Quickened wolf ten levels over plain ones wins about half its
    // fights three to one and a quarter five to one, the Trance falling with the odds as foes fall; re-tuned for doc 47,
    // once the simulator stood both sides on fair ground. A Blinker's is as deep at two to one as at three, and at four
    // as at five, and its Ward nearly whole: alone, it has nothing for a crowd but its blinks.)
    static const std::map<std::string, TranceDepth> by = {
        {"fire", {3.88, 3.90}},  {"earth", {3.35, 7.67}}, {"water", {3.47, 3.61}},                 {"wind", {2.50, 4.23}},
        {"sound", {2.15, 2.47}}, {"seer", {2.40, 9.63}},  {"blinker", {2.73, 12, 2.73, 12, .97}}, {"gravity", {3.39, 7.25, -1, 3.95}}};
    const auto k = by.find(family);
    TranceDepth d = k == by.end() ? TranceDepth{1, 1} : k->second;
    return d;
}

// Slip and Interpose share one rest, in the wolf's own turns (doc 45).
constexpr int BlinkRest = 8;
// Flare's knock to a foe's bar (doc 45).
constexpr double FlareKnock = 20;
// Effects that last only until the wolf's next turn has begun: kept for 2 of its turn starts.
constexpr int NextTurn = 2;
// Rounds of the fight (each turn taken by anyone) for "N turns" on the ground, as smoke counts them.
int roundsFor(const Battle& b, int turns)
{
    int alive = 0;
    for (const auto& o : b.fighters)
        alive += o.status == "fighting" || o.status == "downed";
    return b.turns + turns * std::max(1, alive);
}

std::pair<int, int> parseTile(const std::string& s, bool& ok)
{
    const auto comma = s.find(',');
    ok = comma != std::string::npos && comma > 0 && comma + 1 < s.size();
    if (!ok)
        return {0, 0};
    char* end = nullptr;
    const long x = std::strtol(s.c_str(), &end, 10);
    ok = end == s.c_str() + comma;
    const long y = std::strtol(s.c_str() + comma + 1, &end, 10);
    ok = ok && *end == 0;
    return {int(x), int(y)};
}

// A way from (x0, y0) toward (x1, y1): the line of tiles out to `length`, eight ways (a Bresenham walk past the target).
std::vector<std::pair<int, int>> lineOut(int x0, int y0, int x1, int y1, int length)
{
    std::vector<std::pair<int, int>> out;
    const double dx = x1 - x0, dy = y1 - y0, n = std::max(std::abs(dx), std::abs(dy));
    if (n < 1)
        return out;
    for (int i = 1; i <= length; ++i)
        out.push_back({x0 + int(std::lround(dx / n * i)), y0 + int(std::lround(dy / n * i))});
    return out;
}

bool metalPiece(const std::string& name)
{
    std::string n = name;
    std::transform(n.begin(), n.end(), n.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    for (const char* m : {"steel", "iron", "mail", "bronze", "chain", "splint", "plate", "brass", "gorget"})
        if (n.find(m) != std::string::npos)
            return true;
    return false;
}
} // namespace

// ------------------------------------------------------------------ Who has what

bool World::unflankable(const BattleFighter& f) const
{
    const auto* e = entity(f.id);
    if (!e)
        return false;
    // (A Seer with grit in its eyes doesn't see the blow from the side coming: doc 45.)
    return (e->quickened && (e->gift == "fire" || (e->gift == "seer" && !f.magic.has("dusted")))) || f.magic.has("stone_armor") ||
           f.magic.has("water_screen");
}

std::string World::giftWhyNot(const Battle& b, const BattleFighter& f, const std::string& ability) const
{
    const auto* e = entity(f.id);
    if (ability == "trance")
    {
        // A Trance (doc 45): any Quickened wolf, at any time in a fight, once.
        if (!e || !e->quickened || e->gift.empty())
            return "Only the Quickened go into a Trance.";
        if (f.status != "fighting")
            return "You can't, from where you lie.";
        if (f.magic.trance > 0)
            return "You are in a Trance.";
        return {};
    }
    const auto* a = gifts::ability(ability);
    const auto* rule = ruleOf(ability);
    if (!e || !a || a->family != e->gift || a->quickened != e->quickened || a->work)
        return "That isn't your Gift.";
    if (a->kind == "passive")
        return "It is always with you.";
    if (!rule)
        return "That isn't your Gift.";
    if (b.placing() && a->kind != "fightlong" && a->kind != "reaction")
        return "Not until the fight begins.";
    if (f.status != "fighting")
        return "You can't, from where you lie.";
    if (a->kind == "fightlong")
    {
        if (f.magic.has(ability))
            return "It is on you for the fight.";
        if (!b.placing() && f.turnsTaken > 1)
            return "Only as the fight begins: before or on your first turn.";
    }
    else if (a->kind == "reaction")
        return {};                                  // (Armed at any time: it fires by itself.)
    else
    {
        if (!f.acting)
            return "It isn't your turn.";
        // A Gifted wolf's help takes the turn's move, or its action once it has moved (doc 45); a Quickened Gift is the
        // action.
        if (e->quickened && f.acted)
            return "You have already acted this turn.";
        if (!e->quickened && f.acted && f.moved)
            return "You have already moved and acted this turn.";
        if (f.casting)
            return "You are gathering a Gift already.";
        if (a->kind == "channelled" && f.magic.channel == ability)
            return "You are holding it now.";
    }
    if (const auto cd = f.magic.cooldown.find(ability); cd != f.magic.cooldown.end() && cd->second > 0)
        return "Resting: " + std::to_string(cd->second) + (cd->second == 1 ? " more turn." : " more turns.");
    // The Tell (doc 43): what the family needs to call on its Gift.
    const auto& fam = e->gift;
    const bool breath = fam == "fire" || fam == "wind" || fam == "sound";
    if (breath && f.magic.has("breathless"))
        return "Your breath is stolen: you can't.";
    // A Blinker pinned (held aloft, crushed under its weight, frozen fast) can't blink: the Gifted's hop needs its feet
    // free too, and the Quickened's blink, with no Tell, still needs the body loose (doc 45).
    const bool pinned = f.magic.has("held") || f.magic.has("crushed") || f.magic.has("frozen") || f.magic.has("pinned");
    if (fam == "blinker" && e->quickened && pinned && (ability == "blink_strike" || ability == "chain_blink" || ability == "extract"))
        return "Pinned where you are: you can't blink.";
    if (fam == "blinker" && !e->quickened && (f.magic.has("prone") || pinned))
        return "You can't hop: pinned where you are.";
    if (fam == "gravity" && (f.magic.has("held") || f.magic.has("frozen")))
        return "You can't tap your feet.";
    if (fam == "earth")
    {
        if (!e->quickened && e->worn.count("paws"))
            return "Your paws are covered: an Earth wolf braces bare paws.";
        // Wood underfoot (a bridge or pier, a carpet, straw: Data/Terrain) has no earth to brace on.
        const auto* c = cell(b.cellId);
        const auto* t = c ? c->tile(f.x, f.y) : nullptr;
        if (t && (t->glyph == '8' || t->glyph == 'R' || t->glyph == 'z'))
            return "Wood underfoot: there's no earth to brace on.";
    }
    if (fam == "water")
    {
        const int reach = e->quickened ? 4 : 2;
        bool near = false;
        const auto* c = cell(b.cellId);
        for (int dy = -reach; dy <= reach && !near; ++dy)
            for (int dx = -reach; dx <= reach && !near; ++dx)
                if (const auto* t = c ? c->tile(f.x + dx, f.y + dy) : nullptr; t && t->terrain == Terrain::Water)
                    near = true;
        for (const auto& g : b.ground)
            near = near || (g.kind == "water" && apart(g.x, g.y, f.x, f.y) <= reach);
        const auto env = environmentAt(b.cellId, {f.x + .5, f.y + .5});
        near = near || ((env.weather == Weather::Rain || env.weather == Weather::Storm || env.weather == Weather::Snow) && env.intensity > .1) ||
               f.magic.has("soaked");
        if (const auto* purse = society_.account(f.id); purse && Society::stockAll(*purse, "water") > 0)
            near = true;                            // An open waterskin (a skinful of water, from a well or fountain).
        if (!near)
            return e->quickened ? "No water within 4 tiles, rain, or a skin of water." : "No water within 2 tiles, rain, or a skin of water.";
    }
    if (ability == "whisper_thread" && f.magic.has("hoarse"))
        return "Your voice is hoarse.";
    if (e->quickened && injury::spent(e->injuries) &&
        (e->mana + 1e-9 < a->mana + a->perTurn || (f.magic.last == ability && f.magic.lastTurn == f.turnsTaken - 1)))
        return "Trance fatigue: rest fully before you push your Gift past its limits again.";
    if (ability == "riposte" && f.magic.has("dusted"))
        return "Grit in your eyes: you can't see it coming.";
    if (ability == "stone_armor" && !e->quickened)
        return "That isn't your Gift.";
    // Mana: the Gifted need it all; the Quickened may overreach (doc 43).
    if (!e->quickened && e->mana + 1e-9 < a->mana + a->perTurn)
        return "Too little mana.";
    if (rule->stamina > 0 && (e->exhausted || e->stamina < rule->stamina) && a->kind != "fightlong" && a->kind != "reaction")
        return "You haven't the breath for it.";
    return {};
}

std::vector<World::GiftOption> World::giftOptions(const std::string& id) const
{
    std::vector<GiftOption> out;
    const auto* b = battleOf(id);
    const auto* e = entity(id);
    const auto* f = b ? b->fighter(id) : nullptr;
    if (!b || !e || !f || e->gift.empty())
        return out;
    for (const auto* a : gifts::abilities(e->gift, e->quickened))
    {
        const auto* rule = ruleOf(a->id);
        if (a->work || (!rule && a->kind != "passive"))
            continue;
        GiftOption o;
        o.id = a->id;
        o.name = a->name;
        o.kind = a->kind;
        o.target = a->kind == "passive" ? "passive" : rule->target;
        o.range = rule ? rule->range : 0;
        o.tiles = rule ? rule->tiles : 0;
        o.mana = a->mana;
        o.perTurn = a->perTurn;
        o.perTile = a->perTile;
        if (const auto cd = f->magic.cooldown.find(a->id); cd != f->magic.cooldown.end())
            o.cooldown = cd->second;
        o.on = f->magic.channel == a->id || f->magic.armed.count(a->id) || f->magic.has(a->id) || a->kind == "passive";
        o.why = giftWhyNot(*b, *f, a->id);
        o.ready = o.why.empty();
        out.push_back(std::move(o));
    }
    if (e->quickened)
    {
        GiftOption o;                               // (A Trance, doc 45: not one family's, every Quickened wolf's.)
        o.id = "trance";
        o.name = "Trance";
        o.kind = "fightlong";
        o.target = "self";
        o.on = f->magic.trance > 0;
        o.why = giftWhyNot(*b, *f, "trance");
        o.ready = o.why.empty();
        out.push_back(std::move(o));
    }
    return out;
}

std::vector<World::GiftEffect> World::giftEffects(const Battle&, const BattleFighter& f) const
{
    std::vector<GiftEffect> out;
    for (const auto& [id, turns] : f.magic.fx)
        if (const auto* fx = fxOf(id))
            out.push_back({id, fx->name, fx->does, turns < 0 ? -1 : std::max(0, turns - 1)});
    if (!f.magic.channel.empty())
        if (const auto* a = gifts::ability(f.magic.channel))
            out.push_back({"channel", "Holding " + a->name, a->summary, -1});
    for (const auto& r : f.magic.armed)
        if (const auto* a = gifts::ability(r))
            out.push_back({"armed", a->name + " ready", a->summary, -1});
    return out;
}

// ------------------------------------------------------------------ Hooks from the fight's own rules

double World::meterRate(const Battle& b, const BattleFighter& f, double haste) const
{
    const auto* e = entity(f.id);
    if (!e)
        return 0;
    double rate = battle::meterGain(effectiveDexterity(*e) + battle::armourDex(*e)) * battle::MeterPerSecond * haste * injury::effects(e->injuries).initiative;
    if (f.magic.has("stone_armor"))
        rate *= .95;                                // (Doc 45: a sixth slower cost more than the stone gave.)
    rate *= 1 + tranceGain(f, TranceHaste);         // (A Trance: doc 45.)
    for (const auto& o : b.fighters)
        if (o.side == f.side && o.id != f.id && standing(o) && o.magic.channel == "steady_beat" && apart(o.x, o.y, f.x, f.y) <= 3)
        {
            rate *= 1.3;                            // Steady Beat (doc 43): the rest of the side's bars fill faster (doc 45).
            break;
        }
    return rate;
}

bool World::magicBlocks(const Battle& b, int x, int y) const
{
    for (const auto& g : b.ground)
        if (g.x == x && g.y == y && (g.kind == "wall" || g.kind == "fissure"))
            return true;
    return false;
}

int World::magicRange(const Battle& b, const BattleFighter& f, int range) const
{
    const auto* e = entity(f.id);
    const auto& m = f.magic;
    if (e && e->gift == "wind" && e->quickened)
        range *= 2;                                 // Tailwind: twice as far as any other wolf.
    range += (m.has("lightened") ? 1 : 0) + (m.has("breeze") ? 1 : 0) + (m.has("weightless") ? 3 : 0) -
             (m.has("heavy") ? 1 : 0) - (m.has("cracked") ? 1 : 0) - (m.has("burdened") ? 1 : 0);
    if (m.has("crushed"))
        range = std::min(range, 1);
    if (const auto* w = b.groundAt(f.x, f.y, "water"); w && w->owner != f.id)
        range /= 2;                                 // Wading (a Flood or a Wave's water): half as far.
    return std::max(0, range);
}

double World::magicTileStamina(const Battle&, const BattleFighter& f, double perTile) const
{
    const auto& m = f.magic;
    if (m.has("weightless") || m.has("breeze"))
        return 0;
    if (m.has("lightened"))
        perTile *= .5;
    if (m.has("burdened"))
        perTile += .6;
    return perTile;
}

void World::magicSenses(const Battle& b, const BattleFighter& o, const BattleFighter& t, battle::Senses& s) const
{
    const auto* te = entity(t.id);
    if (o.magic.has("dusted"))
        s.sight *= .5;
    if (o.magic.has("deafened"))
        s.noise = 0;
    if (t.magic.has("washed"))
        s.scent = 0;
    if (te && te->gift == "sound" && te->quickened)
    {
        s.scent = 0;                                // Battle Sense: the rogue.
        s.noise *= .2;
    }
    for (const auto& a : b.fighters)
    {
        if (a.side != t.side || !standing(a) || a.magic.channel.empty())
            continue;
        if (a.magic.channel == "turn_the_wind")
            s.scent = 0;                            // The side's scent kept from enemy noses.
        if (a.magic.channel == "hush" && apart(a.x, a.y, t.x, t.y) <= 2)
            s.noise = 0;
    }
}

double World::magicStrikeChance(const BattleFighter& f, const BattleFighter& t, double c) const
{
    const auto& a = f.magic;
    const auto& d = t.magic;
    if (a.has("seen_opening"))
        return 1;                                   // Seen Opening: it can't miss.
    c -= (a.has("splashed") ? .13 : 0) + (a.has("dusted") ? .1 : 0) + (a.has("dread") ? .15 : 0) + (a.has("dizzy") ? .15 : 0) + (a.has("blinked") ? .1 : 0) +
         (a.has("nausea") ? .1 : 0) + (a.has("double_vision") ? .1 : 0) + (a.has("disoriented") ? .15 : 0);
    c += (d.has("dizzy") ? .15 : 0) - (d.has("forewarned") ? .3 : 0) - (d.has("firm") ? (t.guarding ? .16 : .08) : 0);
    if (d.has("doomed"))
        return std::max(.95, std::clamp(c + .15, .2, .95));   // Doom Mark: it can't dodge.
    return std::clamp(c, .2, .95);
}

double World::magicDamage(const Battle&, const BattleFighter& t, double damage, bool fire) const
{
    const auto* e = entity(t.id);
    if (fire && t.magic.has("water_screen"))
        damage *= .25;
    if (!fire && t.magic.has("stone_armor"))
        damage *= .88;                              // (Doc 45: a share of every blow, a sword's as much as a bite's.)
    if (e && e->gift == "seer" && e->quickened && !t.magic.has("dusted"))
        damage *= .97;                              // Critical Sight (not through grit: doc 45).
    if (const auto* te = entity(t.id); te && t.magic.trance > 0)
        damage *= 1 - std::min(tranceDepth(te->gift).wardMost, tranceGain(t, TranceWard));   // (A Trance: doc 45.)
    return damage;
}

void World::magicHurt(Battle& b, BattleFighter& t)
{
    // A hit breaks a Gift held (doc 43): the channel ends; a wolf lifted for a Slam drops short. (A Steady Beat is kept
    // through blows: doc 45.)
    if (t.magic.channel.empty() || t.magic.channel == "steady_beat")
        return;
    const auto held = t.magic.channel, on = t.magic.channelOn;
    t.magic.channel.clear();
    t.magic.channelOn.clear();
    const auto* e = entity(t.id);
    for (auto& o : b.fighters)
        for (auto it = o.magic.by.begin(); it != o.magic.by.end();)
            if (it->second == t.id)
            {
                o.magic.fx.erase(it->first);
                it = o.magic.by.erase(it);
            }
            else
                ++it;
    t.magic.fx.erase("riposte");
    if (held == "slam")
        if (auto* d = b.fighter(on); d && d->status == "fighting")
        {
            auto* de = entity(d->id);
            const double dmg = 12 * (.5 + (e ? e->wisdom : 30) / 100);
            fightLine(b, t.id, d->id, "gift", (de ? de->name : std::string("They")) + " drops short as the hold breaks (" + whole(dmg) + ").");
            b.log.back().tiles = {{d->x, d->y}};
            hurtFighter(b, *d, dmg, battle::DownedBlunt, t.id, true);
            return;
        }
    if (e)
        fightLine(b, t.id, {}, "break", e->name + "'s hold on the Gift breaks.");
}

void World::magicStep(Battle& b, BattleFighter& f)
{
    // A step onto ground a Gift has changed (doc 43).
    auto* e = entity(f.id);
    if (!e)
        return;
    f.magic.movedTurn = b.turns;
    if (f.magic.has("firm") && (f.x != f.magic.firmX || f.y != f.magic.firmY))
        f.magic.fx.erase("firm");
    const auto key = std::int64_t(b.seq) * 31 + f.x * 977 + f.y;
    if (b.groundAt(f.x, f.y, "fire") && f.burning <= 0 && !f.magic.has("soaked") && !b.groundAt(f.x, f.y, "water"))
    {
        f.burning = battle::BurnTurns;
        fightLine(b, f.id, {}, "burning", e->name + " walks into the fire, and is burning.");
    }
    if (b.groundAt(f.x, f.y, "loose"))
    {
        e->stamina = std::max(0.0, e->stamina - 2);
        if (chance(f.id + "|loose", key) < .3)
        {
            f.walk.clear();
            fightLine(b, f.id, {}, "stumble", e->name + " stumbles in the loose ground.");
        }
    }
    if (b.groundAt(f.x, f.y, "slick") && !f.magic.steady())
    {
        const auto* c = cell(b.cellId);
        const auto* t = c ? c->tile(f.x, f.y) : nullptr;
        const bool hard = t && (t->terrain == Terrain::Floor || t->terrain == Terrain::Stairs);
        if (chance(f.id + "|slick", key) < (hard ? .35 : .15))
        {
            f.walk.clear();
            fightLine(b, f.id, {}, "stumble", e->name + " slips on the wet ground.");
        }
    }
}

// ------------------------------------------------------------------ A turn begins

bool World::magicTurnStart(Battle& b, BattleFighter& f)
{
    auto* e = entity(f.id);
    if (!e)
        return true;
    auto& m = f.magic;
    m.reacted = false;
    // A Trance (doc 45): its level rises with the odds, and it gives mana.
    if (m.trance > 0)
    {
        m.trance = tranceLevel(b, f);               // (With the odds as they are now, up or down: doc 45.)
        m.tranceTop = std::max(m.tranceTop, m.trance);
        // (Mana by the odds alone, not the family's depth: tied to the depth, a Gift's cost made a cliff of it.)
        e->mana = std::min(manaPool(*e), e->mana + TranceMana * (m.trance - 1));
    }
    // Thirst (Water's Cost): stamina comes back slower, or not at all.
    if (m.has("thirsty") || m.has("dehydrated"))
        e->stamina = std::max(0.0, e->stamina - battle::staminaPerTurn(e->hurt, e->strength) * (m.has("dehydrated") ? 1 : .25));   // (Thirst: a quarter, doc 45.)
    // A turn lost: unmoored, blinked or weighed too far, or held aloft.
    for (const char* lost : {"held", "unmoored", "dissociated", "blackout", "dazed"})
        if (m.has(lost))
        {
            const std::string why = std::string(lost) == "held"        ? " hangs in the air, helpless."
                                    : std::string(lost) == "unmoored"  ? " stands empty-eyed, somewhere else."
                                    : std::string(lost) == "blackout"  ? " reels, blacked out."
                                    : std::string(lost) == "dazed"     ? " finds their feet, dazed."
                                                                       : " sways, lost in themselves.";
            if (std::string(lost) != "held")
                m.fx.erase(lost);
            if (std::string(lost) == "unmoored")
                m.fx["disoriented"] = NextTurn + 1;
            fightLine(b, f.id, {}, "lost", e->name + why);
            endTurn(b, f);
            return false;
        }
    // No move this turn: frozen, or knocked down.
    for (const char* still : {"frozen", "prone"})
        if (m.has(still))
        {
            m.fx.erase(still);
            f.moved = true;
            if (std::string(still) == "frozen")
                m.fx["pinned"] = 2;                 // (Held fast in the ice for this turn: no blinking out, doc 45.)
            fightLine(b, f.id, {}, "still", e->name + (std::string(still) == "frozen" ? " is frozen fast." : " scrambles up off the ground."));
        }
    // Effects and rests run down (counted at the wolf's own turns).
    for (auto it = m.fx.begin(); it != m.fx.end();)
        if (it->second > 0 && --it->second == 0)
        {
            m.by.erase(it->first);
            it = m.fx.erase(it);
        }
        else
            ++it;
    for (auto it = m.cooldown.begin(); it != m.cooldown.end();)
        it = --it->second <= 0 ? m.cooldown.erase(it) : std::next(it);
    // A held Gift: its mana for another turn, and what it does each turn.
    if (!m.channel.empty())
    {
        const auto* a = gifts::ability(m.channel);
        const double upkeep = a ? a->perTurn : 0;
        auto* d = b.fighter(m.channelOn);
        if (e->mana + 1e-9 < upkeep || (!m.channelOn.empty() && m.channel != "slam" && (!d || d->status != "fighting")))
        {
            fightLine(b, f.id, {}, "break", e->name + " lets the Gift go.");
            letGo(f.id);
        }
        else
        {
            e->mana -= upkeep;
            const auto power = .5 + e->wisdom / 100;
            if (m.channel == "slam" && d)
            {
                // The drop (doc 43): the second turn of a Slam.
                auto* de = entity(d->id);
                const double dmg = battle::throughArmour(*de, "body", 12 * power, "blunt");
                m.channel.clear();
                m.channelOn.clear();
                d->magic.fx.erase("held");
                d->magic.by.erase("held");
                if (!d->magic.steady())
                    d->magic.fx["prone"] = NextTurn;
                fightLine(b, f.id, d->id, "gift", e->name + " slams " + de->name + " down (" + whole(dmg) + ").");
                b.log.back().tiles = {{d->x, d->y}};
                hurtFighter(b, *d, dmg, battle::DownedBlunt, f.id, true);
            }
            else if (m.channel == "crush" && d)
            {
                auto* de = entity(d->id);
                int pieces = 0;
                for (const char* zone : {"head", "throat", "body", "legs"})
                    pieces += !battle::armourPieceAt(*de, zone).empty();
                const double dmg = (8 + 2 * pieces) * power;
                fightLine(b, f.id, d->id, "gift", de->name + " is crushed under their own weight (" + whole(dmg) + ").");
                b.log.back().tiles = {{d->x, d->y}};
                hurtFighter(b, *d, dmg, battle::DownedBlunt, f.id, false);
            }
            else if (m.channel == "steal_breath" && d)
            {
                auto* de = entity(d->id);
                de->stamina = std::max(0.0, de->stamina - 15);
                if (de->stamina <= 0)
                    de->exhausted = true;
                fightLine(b, f.id, d->id, "gift", de->name + " gasps for air.");
                b.log.back().tiles = {{d->x, d->y}};
            }
            else if (m.channel == "shared_sight")
            {
                for (auto& o : b.fighters)
                    if (o.side == f.side && o.id != f.id && standing(o) && apart(o.x, o.y, f.x, f.y) <= 3)
                        o.magic.fx["forewarned"] = NextTurn;
            }
            else if (m.channel == "heat_sense" || m.channel == "feel_footfalls")
                for (auto& o : b.fighters)
                {
                    if (o.side == f.side || !standing(o) || apart(o.x, o.y, f.x, f.y) > 6)
                        continue;
                    if (m.channel == "feel_footfalls" && o.magic.movedTurn < b.turns - int(b.fighters.size()) * 2)
                        continue;           // (Only those who have moved since about this wolf's last turn.)
                    if (o.unseen)
                        revealFighter(b, o, entity(o.id)->name + (m.channel == "heat_sense" ? "'s warmth gives them away." : "'s footsteps give them away."));
                    for (const auto& ally : b.fighters)
                        if (ally.side == f.side)
                        {
                            b.aware[{ally.id, o.id}] = battle::AwareKept;
                            b.seenAt[{ally.id, o.id}] = {o.x, o.y, time_};
                        }
                }
        }
    }
    // Quickened Fire's Heat Sense: hidden wolves within 12 tiles.
    if (e->gift == "fire" && e->quickened)
        for (auto& o : b.fighters)
            if (o.side != f.side && standing(o) && apart(o.x, o.y, f.x, f.y) <= 12)
            {
                if (o.unseen)
                    revealFighter(b, o, entity(o.id)->name + "'s warmth gives them away.");
                b.aware[{f.id, o.id}] = battle::AwareKept;
                b.seenAt[{f.id, o.id}] = {o.x, o.y, time_};
            }
    // The ground: standing in fire burns; a gravity well pulls.
    if (b.groundAt(f.x, f.y, "fire") && f.burning <= 0 && !m.has("soaked") && !b.groundAt(f.x, f.y, "water"))
        f.burning = battle::BurnTurns;
    for (const auto& g : b.ground)
        if (g.kind == "well" && apart(g.x, g.y, f.x, f.y) <= 3 && apart(g.x, g.y, f.x, f.y) > 0 && !m.steady())
        {
            const int nx = f.x + (g.x > f.x) - (g.x < f.x), ny = f.y + (g.y > f.y) - (g.y < f.y);
            if (arenaOpen(b, nx, ny, f.id) && !magicBlocks(b, nx, ny))
            {
                f.x = nx;
                f.y = ny;
                fightLine(b, f.id, {}, "pulled", e->name + " is dragged toward the well.");
                b.log.back().tiles = {{g.x, g.y}};
            }
            break;
        }
    return f.status == "fighting" || f.status == "downed";
}

// ------------------------------------------------------------------ Blows

bool World::magicBlow(Battle& b, BattleFighter& f, BattleFighter*& t, const std::string& weapon, Result& out)
{
    auto* e = entity(f.id);
    auto* d = entity(t->id);
    if (!e || !d)
        return false;
    const auto key = std::int64_t(b.seq) * 7919 + b.turns;
    // Riposte (Quickened Seer): the blow misses, and it strikes back.
    if (t->magic.has("riposte") && !t->magic.has("dusted"))   // (Grit in its eyes: it can't see it coming, doc 45.)
    {
        t->magic.fx.erase("riposte");
        if (t->magic.channel == "riposte")
            t->magic.channel.clear();
        // With what is in its jaws, through the attacker's armour, head on (doc 47).
        const auto* blade = bladeHeld(*d);
        const double raw = (blade ? blade->weapon.damage * items::qualityDamage(items::qualityOf(swordHeld(*d))) : battle::BiteDamage) *
                           (.6 + d->strength / 125) * (.85 + .3 * chance(t->id + "|riposte", key));
        const double back = battle::expectedThrough(*e, 0, raw, blade ? blade->weapon.type : "thrust", blade ? blade->weapon.pierce : 0);
        fightLine(b, t->id, f.id, "riposte", d->name + " saw it coming: the blow misses, and " + d->name + " strikes back (" + whole(back) + ").");
        b.log.back().tiles = {{f.x, f.y}};
        hurtFighter(b, f, magicDamage(b, f, back, false), battle::DownedBite, t->id, true);
        out = {true, d->name + " saw it coming, and strikes back.", t->id};
        return true;
    }
    // Interpose (Gifted Blinker): an ally beside the one struck blinks in front and takes it on guard.
    for (auto& o : b.fighters)
    {
        if (o.id == t->id || o.side != t->side || !standing(o) || !o.magic.armed.count("interpose") || o.magic.reacted ||
            o.magic.cooldown.count("interpose") || apart(o.x, o.y, t->x, t->y) != 1)
            continue;
        auto* oe = entity(o.id);
        const auto* a = gifts::ability("interpose");
        if (!oe || !a || oe->mana < a->mana || o.magic.has("prone") || o.magic.has("frozen") || o.magic.has("held"))
            continue;
        oe->mana -= a->mana;
        o.magic.reacted = true;
        o.magic.cooldown["interpose"] = o.magic.cooldown["slip"] = BlinkRest;   // (One reflex blink between them, doc 45.)
        o.magic.fx["blinked"] = NextTurn;           // (Doc 45: a reaction's blink leaves it a little off, not dizzy.)
        const std::pair<int, int> was{o.x, o.y};
        o.x = t->x;
        o.y = t->y;
        t->x = was.first;
        t->y = was.second;
        o.facing = battle::octant(f.x - o.x, f.y - o.y);
        fightLine(b, o.id, t->id, "blink", oe->name + " blinks in front of " + d->name + " and takes the blow.");
        b.log.back().tiles = {was, {o.x, o.y}};
        t = &o;
        return false;
    }
    // Slip (Gifted Blinker): a tile back, and the blow misses, if that takes it out of the weapon's reach (doc 45: a
    // sword's longer reach follows a short hop).
    if (t->magic.armed.count("slip") && !t->magic.reacted && !t->magic.cooldown.count("slip") && !t->magic.has("prone") &&
        !t->magic.has("frozen") && !t->magic.has("held") && !t->magic.has("crushed"))
        if (const auto* a = gifts::ability("slip"); a && d->mana >= a->mana)
        {
            const int dx = (t->x > f.x) - (t->x < f.x), dy = (t->y > f.y) - (t->y < f.y);
            const int reach = weapon == "blade" ? battle::SwordReach : 1;
            std::pair<int, int> to{-1, -1};
            if (apart(t->x + dx, t->y + dy, f.x, f.y) > reach && arenaOpen(b, t->x + dx, t->y + dy, t->id) &&
                !magicBlocks(b, t->x + dx, t->y + dy))
                to = {t->x + dx, t->y + dy};
            for (int oy = -1; oy <= 1 && to.first < 0; ++oy)
                for (int ox = -1; ox <= 1 && to.first < 0; ++ox)
                    if (apart(t->x + ox, t->y + oy, f.x, f.y) > reach && arenaOpen(b, t->x + ox, t->y + oy, t->id) &&
                        !magicBlocks(b, t->x + ox, t->y + oy))
                        to = {t->x + ox, t->y + oy};
            if (to.first >= 0)
            {
                d->mana -= a->mana;
                t->magic.reacted = true;
                t->magic.cooldown["slip"] = t->magic.cooldown["interpose"] = BlinkRest;
                t->magic.fx["blinked"] = NextTurn;
                const std::pair<int, int> was{t->x, t->y};
                t->x = to.first;
                t->y = to.second;
                t->walk.clear();
                fightLine(b, t->id, f.id, "blink", d->name + " blinks a step away, and " + e->name + "'s " + weapon + " finds air.");
                b.log.back().tiles = {was, to};
                out = {true, d->name + " blinks out of the way.", t->id};
                return true;
            }
        }
    // Seen Opening: it can't miss, and goes for the least armoured spot this side allows.
    if (f.magic.has("seen_opening"))
    {
        const int quarter = battle::quarterOf(battle::octantGap(t->facing, battle::octant(f.x - t->x, f.y - t->y)));
        std::string best;
        double least = std::numeric_limits<double>::max();
        for (const auto& z : battle::hitZones(quarter))
            if (const double armour = battle::armourAt(*d, z.zone, weapon == "blade" ? "cut" : "thrust"); armour < least)
            {
                least = armour;
                best = z.zone;
            }
        f.magic.by["aim_was"] = f.aim;
        f.aim = best;
    }
    return false;
}

void World::magicAfterBlow(Battle&, BattleFighter& f, BattleFighter& t)
{
    f.magic.fx.erase("splashed");
    t.magic.fx.erase("forewarned");
    if (f.magic.has("seen_opening"))
    {
        f.magic.fx.erase("seen_opening");
        f.aim = f.magic.by["aim_was"];
        f.magic.by.erase("aim_was");
    }
}

bool World::throwFighter(Battle& b, BattleFighter& t, int dx, int dy, int tiles, const std::string& by, double crash)
{
    // Thrown (doc 43: a gust, a hurl, a jet, a wave, a blast): along (dx, dy) a tile at a time until it would hit
    // something, which hurts. An anchored wolf stays put.
    auto* d = entity(t.id);
    if (!d || t.status != "fighting" || t.magic.steady() || (dx == 0 && dy == 0))
        return false;
    int moved = 0;
    bool hit = false;
    for (; moved < tiles; ++moved)
    {
        const int nx = t.x + dx, ny = t.y + dy;
        if (!b.inArena(nx, ny) || !arenaOpen(b, nx, ny, t.id) || magicBlocks(b, nx, ny) || !stepBetween(b.cellId, t.x, t.y, nx, ny))
        {
            hit = true;
            break;
        }
        t.x = nx;
        t.y = ny;
    }
    t.walk.clear();
    if (hit && crash > 0)
    {
        fightLine(b, by, t.id, "crash", d->name + " slams into something (" + whole(crash) + ").");
        b.log.back().tiles = {{t.x, t.y}};
        hurtFighter(b, t, crash, battle::DownedBlunt, by, true);
    }
    if (moved > 0)
        magicStep(b, t);
    return moved > 0;
}

void World::overreach(Battle& b, BattleFighter& f, const std::string& family)
{
    // Overreach (doc 43): a Quickened Gift pushed past its mana, or used two turns running, costs far more.
    auto* e = entity(f.id);
    if (!e)
        return;
    // Each overreach in a fight costs more than the last (doc 45): its health, 3 more each time.
    ++f.magic.overreaches;
    hurtFighter(b, f, OverreachHurt * f.magic.overreaches, battle::DownedBlunt, {}, false);
    if (f.status != "fighting")
        return;
    std::string what;
    if (family == "fire" || family == "water")
    {
        what = family == "fire" ? " overheats" : " is wrung dry";
        hurtFighter(b, f, 6, battle::DownedFire, {}, false);
    }
    else if (family == "earth")
    {
        what = "'s pads crack";
        f.magic.fx["cracked"] = 4;
        hurtFighter(b, f, 3, battle::DownedBlunt, {}, false);
    }
    else if (family == "wind")
    {
        what = " reels, dizzy";
        f.magic.fx["dizzy"] = NextTurn + 1;
    }
    else if (family == "sound")
    {
        what = "'s own ears ring and bleed";
        e->earHealth = std::max(0.2, e->earHealth - .05);
        f.magic.fx["deafened"] = NextTurn + 1;
    }
    else if (family == "blinker")
    {
        what = " comes loose from themselves";
        f.magic.fx["dissociated"] = 1;
    }
    else if (family == "gravity")
    {
        what = " blacks out under the weight";
        f.magic.fx["blackout"] = 1;
    }
    else if (family == "seer")
    {
        what = " loses track of now";
        f.magic.fx["disoriented"] = NextTurn + 1;
    }
    if (!what.empty())
        fightLine(b, f.id, {}, "overreach", e->name + what + ": the Gift pushed too far.");
}

void World::wardensSee(Battle& b, const BattleFighter& caster)
{
    // Quickened magic others see raises Warden attention (doc 43), for the Wardens of a later doc.
    auto* e = entity(caster.id);
    if (!e || !e->quickened)
        return;
    int seen = int(b.observers.size());
    for (const auto& o : b.fighters)
        if (o.id != caster.id && (o.status == "fighting" || o.status == "downed"))
            seen += 1;
    for (const auto* o : entitiesIn(b.cellId))
        if (o && o->npc && !b.fighter(o->id) && std::hypot(o->position.x - e->position.x, o->position.y - e->position.y) <= battle::NoiseReach &&
            visionClarity(o->id, caster.id) > 0)
            ++seen;
    e->wardenAttention += std::min(5, seen);
}

// ------------------------------------------------------------------ Using a Gift

Result World::armReaction(const std::string& id, const std::string& ability, bool on)
{
    auto* b = battleFor(id);
    auto* f = b ? b->fighter(id) : nullptr;
    if (!b || !f)
        return {false, "You are not in a fight.", {}};
    const auto* a = gifts::ability(ability);
    const auto* e = entity(id);
    if (!a || !e || a->kind != "reaction" || a->family != e->gift || a->quickened != e->quickened)
        return {false, "That isn't one of your Gift's reactions.", {}};
    if (on)
        f->magic.armed.insert(ability);
    else
        f->magic.armed.erase(ability);
    return {true, on ? a->name + " is ready: it comes by itself, then rests three turns." : a->name + " is off.", {}};
}

Result World::letGo(const std::string& id)
{
    auto* b = battleFor(id);
    auto* f = b ? b->fighter(id) : nullptr;
    if (!b || !f || f->magic.channel.empty())
        return {false, "You hold no Gift.", {}};
    const auto held = f->magic.channel;
    f->magic.channel.clear();
    f->magic.channelOn.clear();
    f->magic.fx.erase("riposte");
    for (auto& o : b->fighters)
        for (auto it = o.magic.by.begin(); it != o.magic.by.end();)
            if (it->second == id && it->first != "aim_was")
            {
                o.magic.fx.erase(it->first);
                it = o.magic.by.erase(it);
            }
            else
                ++it;
    return {true, "You let it go.", {}};
}

Result World::useGift(const std::string& id, const std::string& ability, const std::string& target)
{
    if (ability == "trance")
        return enterTrance(id);
    auto* b = battleFor(id);
    if (!b)
        return {false, "You are not in a fight.", {}};
    if (b->over)
        return {false, "The fight is over.", {}};
    auto& f = *b->fighter(id);
    auto* e = entity(id);
    f.away = false;
    f.timeouts = 0;
    const auto* a = gifts::ability(ability);
    const auto* rule = ruleOf(ability);
    if (const auto why = giftWhyNot(*b, f, ability); !why.empty())
        return {false, why, {}};
    if (ability == "flamethrower")
    {
        bool ok = false;
        const auto [x, y] = parseTile(target, ok);
        if (!ok)
            return {false, "Aim it: which way?", {}};
        if (f.unseen)
            revealFighter(*b, f, e->name + " comes out of hiding.");
        auto r = castFlame(*b, f, x, y);
        if (r.ok)
            wardensSee(*b, f);
        return r;
    }
    const double power = .5 + e->wisdom / 100;
    const auto key = std::int64_t(b->seq) * 104729 + b->turns + std::int64_t(std::hash<std::string>{}(ability) % 997);
    const auto rnd = [&](const std::string& salt) { return .85 + .3 * chance(id + "|" + ability + "|" + salt, key); };
    // What it is aimed at.
    BattleFighter* t = nullptr;
    std::pair<int, int> tile{f.x, f.y};
    std::vector<std::pair<int, int>> shape;
    std::string text;                               // Whisper Thread's words.
    const std::string kind = rule->target;
    if (kind == "foe" || kind == "ally" || kind == "downed" || kind == "foe+tile" || (kind == "any" && target.find(',') == std::string::npos))
    {
        std::string who = target;
        if (const auto at = who.find('@'); at != std::string::npos)
        {
            bool ok = false;
            tile = parseTile(who.substr(at + 1), ok);
            if (!ok)
                return {false, "Where to?", {}};
            who = who.substr(0, at);
        }
        else if (kind == "foe+tile")
            return {false, "Where to? Choose a tile for them.", {}};
        if (const auto bar = who.find('|'); bar != std::string::npos)
        {
            text = who.substr(bar + 1, 400);
            who = who.substr(0, bar);
        }
        t = who.empty() && kind == "ally" ? &f : b->fighter(who);
        if (!t)
            return {false, "At whom?", target};
        const bool foe = t->side != f.side;
        if ((kind == "foe" || kind == "foe+tile") && (!foe || !standing(*t)))
            return {false, "Choose a foe still standing.", target};
        if (kind == "ally" && (foe || !standing(*t)))
            return {false, "Choose someone on your side.", target};
        if (kind == "downed" && (foe || t->status != "downed"))
            return {false, "Choose someone on your side who is down.", target};
        if (kind == "any" && (foe || !standing(*t)))
            return {false, "Choose someone on your side, or a tile.", target};
        if (t->unseen && foe)
            return {false, "At whom?", target};
        if (apart(f.x, f.y, t->x, t->y) > std::max(1, rule->range))
            return {false, rule->range <= 1 ? "Get next to them first." : "Too far: " + std::to_string(rule->range) + " tiles at most.", target};
    }
    else if (kind == "tile" || kind == "dir" || kind == "any")
    {
        bool ok = false;
        tile = parseTile(target, ok);
        if (!ok || !b->inArena(tile.first, tile.second))
            return {false, kind == "dir" ? "Aim it: which way?" : "Where?", {}};
        if (kind == "dir" && tile == std::pair<int, int>{f.x, f.y})
            return {false, "Aim it: which way?", {}};
        if (kind != "dir" && apart(f.x, f.y, tile.first, tile.second) > rule->range)
            return {false, "Too far: " + std::to_string(rule->range) + " tiles at most.", {}};
    }
    else if (kind == "shape")
    {
        std::stringstream in(target);
        std::string part;
        while (std::getline(in, part, ';'))
        {
            bool ok = false;
            const auto p = parseTile(part, ok);
            if (!ok || !b->inArena(p.first, p.second) || !standable(b->cellId, {p.first + .5, p.second + .5}) ||
                apart(f.x, f.y, p.first, p.second) > rule->range || std::find(shape.begin(), shape.end(), p) != shape.end())
                return {false, "Paint tiles of open ground within " + std::to_string(rule->range) + " tiles of you.", {}};
            if (!shape.empty() && std::none_of(shape.begin(), shape.end(), [&](const auto& q) { return apart(q.first, q.second, p.first, p.second) == 1; }))
                return {false, "Paint it as one connected run of tiles.", {}};
            shape.push_back(p);
        }
        if (shape.empty())
            return {false, "Paint where it goes.", {}};
        if (int(shape.size()) > rule->tiles)
            return {false, std::to_string(rule->tiles) + " tiles at most.", {}};
    }
    if ((ability == "forewarn" || ability == "firm_footing") && t == &f)
        return {false, "That is for someone else on your side.", {}};   // (Doc 45: a Gifted wolf's help is for others.)
    if (ability == "lift_up" && t && t->magic.has("lifted"))
        return {false, entity(t->id)->name + " has been lifted once this fight already.", t->id};   // (Doc 45: once each.)
    // What it costs: mana (the Quickened may overreach), the Tell's breath and hurt, and the family's Cost.
    double mana = a->mana + a->perTile * double(shape.size());
    const double stamina = rule->stamina + (ability == "wall_of_fire" ? 1 : ability == "fissure" ? 1.5 : ability == "stone_wall" ? 2 : 0) * double(shape.size());
    bool over = e->quickened && (e->mana + 1e-9 < mana || (f.magic.last == ability && f.magic.lastTurn == f.turnsTaken - 1));
    if (over && injury::spent(e->injuries))
        return {false, "Trance fatigue: rest fully before you push your Gift past its limits again.", {}};
    if (stamina > 0 && e->stamina < stamina && a->kind != "fightlong")
        return {false, "You haven't the breath for it.", {}};
    const auto pay = [&] {
        e->mana = std::max(0.0, e->mana - mana);
        e->stamina = std::max(0.0, e->stamina - stamina);
        if (e->stamina <= 0)
            e->exhausted = true;
        if (rule->self > 0)
            hurtFighter(*b, f, rule->self, battle::DownedFire, {}, false);
        const auto& fam = e->gift;
        if (fam == "water")
            f.magic.fx["thirsty"] = NextTurn;           // (Doc 45: dehydration left a Quickened wolf no breath to swing.)
        else if (fam == "sound" && !e->quickened)
            f.magic.fx["hoarse"] = NextTurn;
        else if (fam == "blinker")
            f.magic.fx[e->quickened ? "nausea" : "dizzy"] = NextTurn;
        else if (fam == "gravity")
        {
            f.magic.fx["heavy"] = NextTurn;
            if (!e->quickened && f.unseen)
                revealFighter(*b, f, "Tapping feet give " + e->name + " away.");   // The Tell is heard.
        }
        else if (fam == "seer")
            f.magic.fx["double_vision"] = NextTurn;
        if (over)
            overreach(*b, f, fam);
        f.magic.last = ability;
        f.magic.lastTurn = f.turnsTaken;
        wardensSee(*b, f);
    };
    const auto act = [&](double weight) {
        if (!e->quickened && !f.moved && a->kind != "twoturn")
            f.moved = f.magic.helped = true;        // (A Gifted wolf's help: the move while it's there, doc 45.)
        else
            f.acted = true;
        f.weight = std::max(f.weight, weight);
    };
    const auto line = [&](const std::string& lineKind, const std::string& to, const std::string& words, std::vector<std::pair<int, int>> tiles) {
        fightLine(*b, id, to, lineKind, words);
        b->log.back().tiles = std::move(tiles);
    };
    const auto name = [&](const BattleFighter* o) { return o ? entity(o->id)->name : std::string("someone"); };
    // Fight-long abilities (Stone Armor, Water Screen): taken on at the start, no action spent.
    if (a->kind == "fightlong")
    {
        if (!e->quickened && e->mana < mana)
            return {false, "Too little mana.", {}};
        pay();
        f.magic.fx[ability] = -1;
        line("gift", {}, e->name + (ability == "stone_armor" ? " draws the stone up over their body." : " wraps themselves in a spinning screen of water."), {{f.x, f.y}});
        return {true, a->name + ": on you for the whole fight.", {}};
    }
    if (a->kind == "reaction")
        return armReaction(id, ability, !f.magic.armed.count(ability));
    if (f.unseen && (kind == "foe" || ability == "blastwave" || ability == "thunderclap" || ability == "whirlwind"))
        revealFighter(*b, f, e->name + " comes out of hiding.");
    if (e->npc && t && t != &f)
        f.facing = battle::octant(t->x - f.x, t->y - f.y);
    // Channelled abilities: held from now, a Gift a turn's mana, until let go, moved from or hit.
    if (a->kind == "channelled" || ability == "slam")
    {
        if (!f.magic.channel.empty())
            letGo(id);
        pay();
        f.magic.channel = ability;
        f.magic.channelOn = t && t != &f ? t->id : std::string();
        const std::map<std::string, std::string> marks = {{"lighten", "lightened"}, {"burden", "burdened"}, {"anchor", "anchored"},
                                                          {"crush", "crushed"}, {"steal_breath", "breathless"}, {"slam", "held"}};
        if (const auto mark = marks.find(ability); mark != marks.end() && t)
        {
            t->magic.fx[mark->second] = -1;
            t->magic.by[mark->second] = id;
        }
        if (ability == "riposte")
            f.magic.fx["riposte"] = -1;
        if (ability == "steal_breath" && t && t->casting)
            hurtFighter(*b, *t, 0, battle::DownedBlunt, id, true);   // (Breath stolen mid-gather: the Gift breaks off.)
        if (ability == "slam" && t)
        {
            t->walk.clear();
            line("gift", t->id, e->name + " lifts " + name(t) + " off the ground.", {{t->x, t->y}});
        }
        else
            line("gift", t && t != &f ? t->id : std::string(), e->name + " " + (t && t != &f ? "holds " + a->name + " on " + name(t) : "holds " + a->name) + ".",
                 {{(t ? t->x : f.x), (t ? t->y : f.y)}});
        act(rule->weight);
        return {true, "You hold " + a->name + ". It costs " + whole(a->perTurn) + " mana a turn; moving or being hit breaks it.", {}};
    }
    // Gathered (and painted): a countdown everyone sees, on tiles locked now (doc 43, as the Flamethrower).
    if (rule->gather > 0)
    {
        BattleCast cast;
        cast.caster = id;
        cast.spell = ability;
        cast.quickened = e->quickened;
        cast.x = tile.first;
        cast.y = tile.second;
        cast.target = t ? t->id : std::string();
        cast.castAt = time_;
        cast.firesAt = time_ + rule->gather / (1 + e->wisdom / 200);
        cast.dir = battle::octant(tile.first - f.x, tile.second - f.y);
        if (ability == "heat_lance")
            cast.tiles = lineOut(f.x, f.y, tile.first, tile.second, rule->range);
        else if (ability == "shatterhowl")
        {
            const double aim = std::atan2(double(tile.second - f.y), double(tile.first - f.x));
            for (int ty = b->y0; ty < b->y0 + b->h; ++ty)
                for (int tx = b->x0; tx < b->x0 + b->w; ++tx)
                {
                    const double dx = tx - f.x, dy = ty - f.y, far = std::hypot(dx, dy);
                    if (far < .5 || far > rule->range + .5)
                        continue;
                    double off = std::abs(std::atan2(dy, dx) - aim) * 180 / 3.14159265358979323846;
                    if (off > 180)
                        off = 360 - off;
                    if (off <= 30 && standable(b->cellId, {tx + .5, ty + .5}))
                        cast.tiles.push_back({tx, ty});
                }
        }
        else if (ability == "wave")
        {
            const int dx = (tile.first > f.x) - (tile.first < f.x), dy = (tile.second > f.y) - (tile.second < f.y);
            const int px = -dy, py = dx;            // Across the way it rolls.
            for (int i = 1; i <= rule->range; ++i)
                for (int w = -1; w <= 1; ++w)
                {
                    const int x = f.x + dx * i + px * w, y = f.y + dy * i + py * w;
                    if (b->inArena(x, y) && standable(b->cellId, {x + .5, y + .5}))
                        cast.tiles.push_back({x, y});
                }
        }
        else if (ability == "flood" || ability == "pressure_drop" || ability == "well")
        {
            const int r = ability == "flood" ? 1 : ability == "well" ? 0 : 0;
            const int extra = ability == "pressure_drop" ? 1 : 0;
            for (int y = tile.second - r; y <= tile.second + r + extra; ++y)
                for (int x = tile.first - r; x <= tile.first + r + extra; ++x)
                    if (b->inArena(x, y) && standable(b->cellId, {x + .5, y + .5}))
                        cast.tiles.push_back({x, y});
            if (ability == "well")
                for (int y = tile.second - 3; y <= tile.second + 3; ++y)
                    for (int x = tile.first - 3; x <= tile.first + 3; ++x)
                        if ((x != tile.first || y != tile.second) && b->inArena(x, y) && standable(b->cellId, {x + .5, y + .5}))
                            cast.tiles.push_back({x, y});
        }
        else if (!shape.empty())
            cast.tiles = shape;
        else
            cast.tiles = {tile};
        if (ability == "hurl_stone" && t)
            cast.tiles = {{t->x, t->y}};
        cast.mana = std::min(e->mana, mana);
        pay();
        if (f.status != "fighting")
            return {true, "The Gift turns on you.", {}};
        f.casting = true;
        f.moved = true;                             // (No moving while it gathers.)
        if (e->npc)
            f.facing = cast.dir;
        act(rule->weight);
        b->casts.push_back(cast);
        const std::map<std::string, std::string> tell = {
            {"heat_lance", " holds a breath; the air shimmers in a line."}, {"wall_of_fire", " holds a breath; heat crawls along the ground."},
            {"upheaval", " braces; the ground shudders."}, {"hurl_stone", " braces and tears at the ground."},
            {"fissure", " braces; a crack runs through the earth."}, {"stone_wall", " braces; stone stirs underfoot."},
            {"wave", " draws the water up into a wall."}, {"flood", " calls the water."}, {"pressure_drop", " breathes in, and in."},
            {"shatterhowl", " fills their chest for a howl."}, {"well", " stares, still; the air grows heavy."}};
        const auto said = tell.find(ability);
        line("charge", cast.target, e->name + (said != tell.end() ? said->second : " gathers their Gift."), cast.tiles);
        if (e->npc)
            endTurn(*b, f);
        return {true, "You gather " + a->name + ".", {}};
    }
    // At once.
    pay();
    if (f.status != "fighting")
        return {true, "The Gift turns on you.", {}};
    std::string said = "You use " + a->name + ".";
    const auto tileOf = [&](const BattleFighter* o) { return std::pair<int, int>{o->x, o->y}; };
    if (ability == "cauterize")
    {
        t->bleeding = 0;
        t->magic.fx["seared"] = 4;                  // (Doc 45: seared shut, it can't bleed again for 3 of its turns.)
        line("gift", t->id, e->name + " sears " + name(t) + "'s wound shut.", {tileOf(t)});
    }
    else if (ability == "flare")
    {
        // The flash knocks its bar back (doc 45), mid-turn too: its next bar starts that much lower.
        knockBar(*t, FlareKnock, true);
        line("gift", t->id, "A flash of flame in " + name(t) + "'s face.", {tileOf(t)});
    }
    else if (ability == "smother_to_smoke")
    {
        const auto* c = cell(b->cellId);
        const auto* g = c ? c->tile(tile.first, tile.second) : nullptr;
        bool fuel = b->groundAt(tile.first, tile.second, "fire") || (g && (g->terrain == Terrain::Grass || g->glyph == '"' || g->glyph == '&' ||
                                                                         g->glyph == 'E' || g->glyph == 'B' || g->glyph == '5'));
        for (const auto& o : b->fighters)
            fuel = fuel || (o.burning > 0 && o.x == tile.first && o.y == tile.second);
        if (!fuel)
            return {false, "There's nothing there to smoke: fire, grass or brush.", {}};
        std::vector<std::pair<int, int>> tiles;
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
                if (b->inArena(tile.first + dx, tile.second + dy) && standable(b->cellId, {tile.first + dx + .5, tile.second + dy + .5}))
                {
                    b->smoke.push_back({{tile.first + dx, tile.second + dy}, roundsFor(*b, 2)});
                    tiles.push_back({tile.first + dx, tile.second + dy});
                }
        line("gift", {}, e->name + " smothers it into a thick smoke.", tiles);
    }
    else if (ability == "warm_through")
    {
        t->magic.fx.erase("soaked");
        t->magic.fx.erase("frozen");
        t->magic.fx["warmed"] = -1;
        line("gift", t->id, e->name + " warms " + name(t) + " through.", {tileOf(t)});
    }
    else if (ability == "blastwave")
    {
        std::vector<std::pair<int, int>> tiles;
        for (auto& o : b->fighters)
            if (o.id != id && standing(o) && apart(o.x, o.y, f.x, f.y) == 1)
            {
                const double dmg = magicDamage(*b, o, 16 * power * rnd(o.id), true);
                tiles.push_back(tileOf(&o));
                line("gift", o.id, name(&o) + " is blasted by the heat (" + whole(dmg) + ").", {tileOf(&o)});
                hurtFighter(*b, o, dmg, battle::DownedFire, id, true);
                throwFighter(*b, o, (o.x > f.x) - (o.x < f.x), (o.y > f.y) - (o.y < f.y), 1, id, 0);
            }
        line("gift", {}, e->name + " bursts with heat.", tiles.empty() ? std::vector<std::pair<int, int>>{{f.x, f.y}} : tiles);
    }
    else if (ability == "loosen_ground" || ability == "slick")
    {
        b->ground.push_back({tile.first, tile.second, ability == "slick" ? "slick" : "loose", id, roundsFor(*b, ability == "slick" ? 4 : 6), 0});
        line("gift", {}, ability == "slick" ? e->name + " wets the ground." : "The ground there turns soft.", {tile});
    }
    else if (ability == "firm_footing")
    {
        t->magic.fx["firm"] = -1;
        t->magic.firmX = t->x;
        t->magic.firmY = t->y;
        line("gift", t->id, "The ground packs hard under " + name(t) + ".", {tileOf(t)});
    }
    else if (ability == "douse")
    {
        std::vector<std::pair<int, int>> tiles;
        if (t)
        {
            t->burning = 0;
            tiles.push_back(tileOf(t));
            line("gift", t->id, e->name + " douses " + name(t) + ".", tiles);
        }
        else
        {
            b->ground.erase(std::remove_if(b->ground.begin(), b->ground.end(),
                                           [&](const BattleGround& g) { return g.kind == "fire" && apart(g.x, g.y, tile.first, tile.second) <= 1; }),
                            b->ground.end());
            for (auto& o : b->fighters)
                if (apart(o.x, o.y, tile.first, tile.second) <= 1)
                    o.burning = 0;
            line("gift", {}, e->name + " douses the flames.", {tile});
        }
    }
    else if (ability == "splash_eyes")
    {
        t->magic.fx["splashed"] = NextTurn;
        line("gift", t->id, "Water in " + name(t) + "'s eyes.", {tileOf(t)});
    }
    else if (ability == "wash_out")
    {
        t->magic.fx["washed"] = 4;
        t->magic.fx.erase("dusted");
        t->magic.fx.erase("splashed");
        line("gift", t->id, e->name + " washes " + name(t) + " clean.", {tileOf(t)});
    }
    else if (ability == "pressure_jet" || ability == "battering_gust")
    {
        const bool jet = ability == "pressure_jet";
        const auto path = lineOut(f.x, f.y, tile.first, tile.second, rule->range);
        std::vector<std::pair<int, int>> tiles;
        BattleFighter* first = nullptr;
        for (const auto& p : path)
        {
            if (!b->inArena(p.first, p.second) || !standable(b->cellId, {p.first + .5, p.second + .5}) || magicBlocks(*b, p.first, p.second))
                break;
            tiles.push_back(p);
            for (auto& o : b->fighters)
                if (o.id != id && standing(o) && o.x == p.first && o.y == p.second)
                    first = &o;
            if (first)
                break;
        }
        line("gift", first ? first->id : std::string(), e->name + (jet ? " looses a jet of water." : " looses a battering gust."), tiles);
        if (first)
        {
            auto* de = entity(first->id);
            const double raw = (jet ? 12 : 15) * power * rnd(first->id);
            // (A jet is water's weight, a gust the wind's: armour takes it; stone half the rest of a gust: doc 45. Doc 47: once
            // armour took only half of a gust, when a sword cut deeper than any blade does now.)
            const double through = battle::throughArmour(*de, "body", raw, "blunt");
            const double dmg = magicDamage(*b, *first, jet ? through : through * (first->magic.has("stone_armor") ? .5 : 1), false);
            line("gift", first->id, de->name + (jet ? " is struck by the water (" : " is hurled back by the wind (") + whole(dmg) + ").", {tileOf(first)});
            hurtFighter(*b, *first, dmg, battle::DownedBlunt, id, true);
            const int dx = (tile.first > f.x) - (tile.first < f.x), dy = (tile.second > f.y) - (tile.second < f.y);
            throwFighter(*b, *first, dx, dy, jet ? 1 : 3, id, jet ? 0 : 12 * power);
            if (jet)
            {
                first->burning = 0;
                first->magic.fx["splashed"] = NextTurn;   // (Water in the eyes, doc 45: its next blow less likely.)
                if (!first->magic.has("warmed"))
                    first->magic.fx["soaked"] = 4;
            }
            else if (!first->magic.steady())
                first->magic.fx["prone"] = NextTurn;
        }
    }
    else if (ability == "freeze")
    {
        const auto* c = cell(b->cellId);
        const auto* g = c ? c->tile(t->x, t->y) : nullptr;
        const bool wet = b->groundAt(t->x, t->y, "water") || (g && g->terrain == Terrain::Water) || t->magic.has("soaked");
        if (!wet)
            return {false, name(t) + " isn't standing in water.", t->id};
        if (t->magic.has("warmed"))
        {
            line("gift", t->id, "The ice can't take hold of " + name(t) + ".", {tileOf(t)});
            said = "They are warmed through: the ice won't hold.";
        }
        else
        {
            t->magic.fx["frozen"] = NextTurn;
            line("gift", t->id, "Ice locks " + name(t) + " where they stand.", {tileOf(t)});
        }
    }
    else if (ability == "clear_the_air")
    {
        std::vector<std::pair<int, int>> tiles;
        b->smoke.erase(std::remove_if(b->smoke.begin(), b->smoke.end(), [&](const auto& s) { return apart(s.first.first, s.first.second, tile.first, tile.second) <= 1; }),
                       b->smoke.end());
        for (auto& o : b->fighters)
            if (o.side == f.side && apart(o.x, o.y, tile.first, tile.second) <= 1)
                o.magic.fx.erase("dusted");
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
                tiles.push_back({tile.first + dx, tile.second + dy});
        line("gift", {}, e->name + " blows the air clear.", tiles);
    }
    else if (ability == "back_breeze")
    {
        t->magic.fx["breeze"] = NextTurn;
        line("gift", t->id, "A breeze at " + name(t) + "'s back.", {tileOf(t)});
    }
    else if (ability == "air_blast")
    {
        t->magic.fx["dusted"] = 3;
        line("gift", t->id, e->name + " blasts grit into " + name(t) + "'s eyes.", {tileOf(t)});
    }
    else if (ability == "whirlwind")
    {
        std::vector<std::pair<int, int>> tiles;
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
                if (dx || dy)
                    tiles.push_back({f.x + dx, f.y + dy});
        line("gift", {}, e->name + " spins up a whirlwind.", tiles);
        for (auto& o : b->fighters)
            if (o.side != f.side && standing(o) && apart(o.x, o.y, f.x, f.y) == 1)
            {
                o.magic.fx["dusted"] = 4;               // (Doc 45: for 3 of its turns, not the fight.)
                throwFighter(*b, o, (o.x > f.x) - (o.x < f.x), (o.y > f.y) - (o.y < f.y), 2, id, 6 * power);
            }
    }
    else if (ability == "throw_voice")
    {
        int turned = 0;
        for (auto& o : b->fighters)
            if (o.side != f.side && standing(o) && apart(o.x, o.y, tile.first, tile.second) <= rule->tiles)
                if (const auto* oe = entity(o.id); oe && oe->npc && !o.magic.has("deafened"))
                {
                    o.facing = battle::octant(tile.first - o.x, tile.second - o.y);
                    ++turned;
                }
        line("gift", {}, "A noise from over there.", {tile});
        said = turned ? "The noise turns " + std::to_string(turned) + (turned == 1 ? " head." : " heads.") : "No one turns.";
    }
    else if (ability == "whisper_thread")
    {
        if (text.empty())
            return {false, "Say what?", {}};
        notice(t->id, e->name + ", along a thread no one else hears: " + text);
        said = "You whisper to " + name(t) + " along the thread.";
        return {true, said, t->id};             // (Free: no action spent.)
    }
    else if (ability == "resonance")
    {
        auto* de = entity(t->id);
        std::vector<std::pair<int, int>> tiles{tileOf(t)};
        if (de->mouth == "sword")
        {
            wearGear(*de, swordHeld(*de), 60);
            if (chance(t->id + "|ring", key) < .5)
            {
                dropItem(*b, *t);
                line("gift", t->id, name(t) + "'s sword rings and leaps from their jaws.", tiles);
            }
        }
        for (const char* zone : {"head", "throat", "body", "legs"})
            if (!battle::armourPieceAt(*de, zone).empty())
                wearArmourAt(*de, zone, 15);
        // Stone rings until it shatters (doc 45: Sound answers Earth): the Stone Armor falls away, in shards.
        const bool stone = t->magic.has("stone_armor");
        if (stone)
        {
            t->magic.fx.erase("stone_armor");
            line("gift", t->id, name(t) + "'s stone armour rings, cracks and falls away.", tiles);
        }
        const double dmg = magicDamage(*b, *t, (stone ? 20 : 10) * power * rnd(t->id), false);
        line("gift", t->id, e->name + "'s note makes " + name(t) + "'s steel ring until it cracks (" + whole(dmg) + ").", tiles);
        hurtFighter(*b, *t, dmg, battle::DownedBlunt, id, true);
    }
    else if (ability == "thunderclap")
    {
        std::vector<std::pair<int, int>> tiles;
        for (int dy = -2; dy <= 2; ++dy)
            for (int dx = -2; dx <= 2; ++dx)
                tiles.push_back({f.x + dx, f.y + dy});
        line("gift", {}, "A thunderclap!", tiles);
        for (auto& o : b->fighters)
            if (standing(o) && apart(o.x, o.y, f.x, f.y) <= 2)
            {
                o.magic.fx["deafened"] = NextTurn + 1;
                if (o.id == id)
                    continue;
                knockBar(o, 40);
                const double dmg = 10 * power;
                hurtFighter(*b, o, dmg, battle::DownedBlunt, id, true);
            }
    }
    else if (ability == "dread_note")
    {
        std::vector<std::pair<int, int>> tiles;
        for (auto& o : b->fighters)
            if (o.side != f.side && standing(o) && apart(o.x, o.y, f.x, f.y) <= rule->range)
            {
                const auto* oe = entity(o.id);
                tiles.push_back(tileOf(&o));
                if (oe && oe->npc && 100 - oe->hurt < 50)
                    o.scared = true;
                o.magic.fx["dread"] = NextTurn + 1;
            }
        line("gift", {}, e->name + " sounds a low note that trembles in the chest.", tiles);
    }
    else if (ability == "blink")
    {
        if (!arenaOpen(*b, tile.first, tile.second, id) || magicBlocks(*b, tile.first, tile.second))
            return {false, "Nowhere to land there.", {}};
        const std::pair<int, int> was{f.x, f.y};
        f.x = tile.first;
        f.y = tile.second;
        f.walk.clear();
        line("blink", {}, e->name + " blinks.", {was, tile});
        magicStep(*b, f);
    }
    else if (ability == "blink_strike" || ability == "chain_blink")
    {
        // The nausea comes after the blink, not with it (doc 45): the strike itself lands clean.
        const int sick = f.magic.fx.count("nausea") ? f.magic.fx["nausea"] : 0;
        f.magic.fx.erase("nausea");
        std::vector<BattleFighter*> marks{t};
        if (ability == "chain_blink")
        {
            std::vector<BattleFighter*> more;
            for (auto& o : b->fighters)
                if (&o != t && o.side != f.side && standing(o) && !o.unseen && apart(o.x, o.y, f.x, f.y) <= rule->range)
                    more.push_back(&o);
            std::sort(more.begin(), more.end(), [&](auto* x, auto* y) { return apart(x->x, x->y, f.x, f.y) < apart(y->x, y->y, f.x, f.y); });
            for (auto* o : more)
                if (marks.size() < 3)
                    marks.push_back(o);
            f.magic.fx["chain"] = 1;
        }

        for (auto* m : marks)
        {
            if (!standing(*m) || f.status != "fighting")
                continue;
            // Behind it, where it can: the tile its back is to, else whichever is most behind.
            const double ang = m->facing * 3.14159265358979323846 / 4;
            std::pair<int, int> spot{-1, -1};
            int best = -1;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx)
                {
                    const int x = m->x + dx, y = m->y + dy;
                    if ((!dx && !dy) || !arenaOpen(*b, x, y, id) || magicBlocks(*b, x, y))
                        continue;
                    const int gap = battle::octantGap(m->facing, battle::octant(dx, dy));
                    if (gap > best)
                    {
                        best = gap;
                        spot = {x, y};
                    }
                }
            (void)ang;
            if (spot.first < 0)
                continue;
            const std::pair<int, int> was{f.x, f.y};
            f.x = spot.first;
            f.y = spot.second;
            f.walk.clear();
            f.facing = battle::octant(m->x - f.x, m->y - f.y);
            line("blink", m->id, e->name + " blinks behind " + name(m) + ".", {was, spot});
            f.acted = false;
            // Behind it, the blow finds the gap: the least armoured spot there (doc 47), with no aim's cost; where every spot
            // is armoured alike (or bare), wherever it falls.
            const auto aimWas = f.aim;
            if (const auto* me = entity(m->id))
            {
                const auto* blade = bladeHeld(*e);
                double least = std::numeric_limits<double>::max(), most = 0;
                std::string gap;
                for (const auto& z : battle::hitZones(battle::quarterOf(battle::octantGap(m->facing, battle::octant(f.x - m->x, f.y - m->y)))))
                {
                    const double armour = battle::armourAt(*me, z.zone, blade ? blade->weapon.type : "thrust");
                    most = std::max(most, armour);
                    if (armour < least)
                    {
                        least = armour;
                        gap = z.zone;
                    }
                }
                if (least < most)
                    f.aim = gap;
            }
            f.magic.fx["gap"] = 1;
            if (e->mouth == "sword")
                swordStrike(*b, f, m->id);
            else
                bite(*b, f, m->id);
            f.magic.fx.erase("gap");
            f.aim = aimWas;
        }
        f.magic.fx.erase("chain");
        if (sick)
            f.magic.fx["nausea"] = sick;
    }
    else if (ability == "displace")
    {
        if (apart(t->x, t->y, tile.first, tile.second) > rule->tiles || !arenaOpen(*b, tile.first, tile.second, t->id) ||
            magicBlocks(*b, tile.first, tile.second))
            return {false, "Send them to open ground within " + std::to_string(rule->tiles) + " tiles of them.", t->id};
        if (t->magic.steady())
            return {false, name(t) + " won't budge: anchored, or under stone.", t->id};
        const std::pair<int, int> was{t->x, t->y};
        t->x = tile.first;
        t->y = tile.second;
        t->walk.clear();
        line("blink", t->id, e->name + " touches " + name(t) + ", and they are elsewhere.", {was, tile});
        magicStep(*b, *t);
    }
    else if (ability == "unmoor")
    {
        t->magic.fx["unmoored"] = 1;
        line("gift", t->id, e->name + " pulls " + name(t) + "'s mind loose from their body.", {tileOf(t)});
    }
    else if (ability == "extract")
    {
        if (t == &f)
            return {false, "Choose someone else on your side.", {}};
        std::pair<int, int> spot{-1, -1};
        int safest = -1;
        for (int dy = -rule->range; dy <= rule->range; ++dy)
            for (int dx = -rule->range; dx <= rule->range; ++dx)
            {
                const int x = t->x + dx, y = t->y + dy;
                if (!arenaOpen(*b, x, y, t->id) || magicBlocks(*b, x, y))
                    continue;
                int nearest = 1000;
                for (const auto& o : b->fighters)
                    if (o.side != f.side && standing(o))
                        nearest = std::min(nearest, apart(o.x, o.y, x, y));
                if (nearest > safest)
                {
                    safest = nearest;
                    spot = {x, y};
                }
            }
        if (spot.first < 0)
            return {false, "Nowhere safer for them.", t->id};
        const std::pair<int, int> was{t->x, t->y};
        t->x = spot.first;
        t->y = spot.second;
        t->walk.clear();
        line("blink", t->id, e->name + " blinks " + name(t) + " out of harm's way.", {was, spot});
    }
    else if (ability == "lift_up")
    {
        auto* te = entity(t->id);
        standUp(*te, battle::LiftedHealth);         // (Doc 45: on its feet, barely, and dazed for its next turn.)
        t->magic.fx["lifted"] = -1;
        t->magic.fx["dazed"] = 1;
        t->status = "fighting";
        t->struggling = false;
        line("rise", t->id, e->name + " lifts " + te->name + " to their feet from afar.", {tileOf(t)});
    }
    else if (ability == "hurl")
    {
        if (t->magic.steady())
            return {false, name(t) + " won't budge: anchored, or under stone.", t->id};
        const int dx = (t->x > f.x) - (t->x < f.x), dy = (t->y > f.y) - (t->y < f.y);
        const std::pair<int, int> was{t->x, t->y};
        throwFighter(*b, *t, dx || dy ? dx : 1, dy, rule->tiles, id, 15 * power);
        t->magic.fx["prone"] = NextTurn;
        line("gift", t->id, e->name + " makes " + name(t) + " weightless and hurls them.", {was, tileOf(t)});
    }
    else if (ability == "weightless")
    {
        std::vector<std::pair<int, int>> tiles;
        for (auto& o : b->fighters)
            if (o.side == f.side && standing(o) && apart(o.x, o.y, f.x, f.y) <= rule->range)
            {
                o.magic.fx["weightless"] = NextTurn;
                tiles.push_back(tileOf(&o));
            }
        line("gift", {}, e->name + " lets the weight go out of their side.", tiles);
    }
    else if (ability == "read_the_line")
    {
        t->magic.fx["read"] = NextTurn;
        t->magic.by["read"] = std::to_string(f.side);
        line("gift", t->id, e->name + " reads where " + name(t) + " will go.", {tileOf(t)});
    }
    else if (ability == "forewarn")
    {
        t->magic.fx["forewarned"] = NextTurn;
        line("gift", t->id, e->name + " calls a warning to " + name(t) + ".", {tileOf(t)});
    }
    else if (ability == "glimpse_the_order")
    {
        std::vector<std::pair<double, std::string>> order;
        for (const auto& o : b->fighters)
            if (standing(o) && !(o.unseen && o.side != f.side))
            {
                const double rate = meterRate(*b, o, meterHaste(*b));
                order.push_back({o.acting ? 0 : rate > 0 ? (100 - o.meter) / rate : 1e9, entity(o.id)->name});
            }
        std::sort(order.begin(), order.end());
        std::string words = "The order to come:";
        for (std::size_t i = 0; i < order.size() && i < 8; ++i)
            words += (i ? ", " : " ") + order[i].second + (order[i].first <= 0 ? " (now)" : " (" + whole(order[i].first) + " s)");
        notice(id, words + ".");
        line("gift", {}, e->name + "'s eyes go glassy.", {{f.x, f.y}});
        said = words + ".";
    }
    else if (ability == "seen_opening")
    {
        f.magic.fx["seen_opening"] = NextTurn + 1;
        line("gift", {}, e->name + " sees the opening.", {{f.x, f.y}});
    }
    else if (ability == "doom_mark")
    {
        t->magic.fx["doomed"] = 4;
        line("gift", t->id, e->name + " sees " + name(t) + "'s death.", {tileOf(t)});
    }
    else
        return {false, "That Gift isn't ready yet.", {}};
    act(rule->weight);
    if (e->npc && f.moved && (e->quickened || f.acted) && !b->over && f.acting)
        endTurn(*b, f);
    else
        checkOver(*b);
    return {true, said, t ? t->id : std::string()};
}

void World::magicResolve(Battle& b, const BattleCast& cast)
{
    auto* cf = b.fighter(cast.caster);
    auto* ce = entity(cast.caster);
    if (!cf || !ce || cf->status != "fighting")
        return;
    cf->casting = false;
    const double power = .5 + ce->wisdom / 100;
    const auto key = std::int64_t(b.seq) * 104729 + b.turns;
    const auto rnd = [&](const std::string& salt) { return .85 + .3 * chance(cast.caster + "|" + cast.spell + "|" + salt, key); };
    const auto inside = [&](const BattleFighter& o) {
        return std::find(cast.tiles.begin(), cast.tiles.end(), std::pair<int, int>{o.x, o.y}) != cast.tiles.end();
    };
    const auto line = [&](const std::string& kind, const std::string& to, const std::string& words, std::vector<std::pair<int, int>> tiles) {
        fightLine(b, cast.caster, to, kind, words);
        b.log.back().tiles = std::move(tiles);
    };
    const auto zoneHit = [&](BattleFighter& o, double raw, const std::string& type) {
        auto* d = entity(o.id);
        const int quarter = battle::quarterOf(battle::octantGap(o.facing, battle::octant(cf->x - o.x, cf->y - o.y)));
        const auto& zones = battle::hitZones(unflankable(o) ? 0 : quarter);
        double total = 0;
        for (const auto& z : zones)
            total += z.weight;
        double r = chance(o.id + "|zone|" + cast.spell, key) * total;
        const battle::HitZone* hit = &zones.back();
        for (const auto& z : zones)
            if ((r -= z.weight) < 0)
            {
                hit = &z;
                break;
            }
        const double through = battle::throughArmour(*d, hit->zone, raw, type);
        wearArmourAt(*d, hit->zone, raw - through);
        return std::pair<double, std::string>{magicDamage(b, o, through, false), hit->zone};
    };
    const auto& s = cast.spell;
    if (s == "heat_lance")
    {
        std::vector<std::pair<int, int>> path;
        BattleFighter* first = nullptr;
        for (const auto& p : cast.tiles)
        {
            if (!b.inArena(p.first, p.second) || !standable(b.cellId, {p.first + .5, p.second + .5}) || magicBlocks(b, p.first, p.second))
                break;
            path.push_back(p);
            for (auto& o : b.fighters)
                if (o.id != cast.caster && o.status == "fighting" && o.x == p.first && o.y == p.second)
                    first = &o;
            if (first)
                break;
        }
        line("flame", first ? first->id : std::string(), ce->name + " looses a lance of heat!", path);
        if (first)
        {
            auto* d = entity(first->id);
            const auto [dmg0, zone] = zoneHit(*first, 24 * power * rnd(first->id), "thrust");
            const double dmg = magicDamage(b, *first, dmg0, true);
            const bool metal = metalPiece(battle::armourPieceAt(*d, zone));
            line("burnt", first->id, d->name + " is pierced by the heat on the " + zone + " (" + whole(dmg) + ").", {{first->x, first->y}});
            hurtFighter(b, *first, dmg, battle::DownedFire, cast.caster, true);
            if (metal && first->status == "fighting" && !first->magic.has("soaked"))
            {
                first->burning = battle::BurnTurns;
                fightLine(b, first->id, {}, "burning", d->name + "'s armour glows: they are burning.");
            }
        }
    }
    else if (s == "wall_of_fire")
    {
        for (const auto& p : cast.tiles)
            if (!b.groundAt(p.first, p.second, "water"))
                b.ground.push_back({p.first, p.second, "fire", cast.caster, roundsFor(b, 3), 0});
        line("flame", {}, ce->name + " lays down a wall of fire!", cast.tiles);
        for (auto& o : b.fighters)
            if (o.status == "fighting" && inside(o) && !o.magic.has("soaked"))
                o.burning = battle::BurnTurns;
    }
    else if (s == "upheaval" || s == "hurl_stone")
    {
        line("gift", cast.target, s == "upheaval" ? "The ground erupts!" : ce->name + " hurls a stone!", cast.tiles);
        for (auto& o : b.fighters)
            if (o.id != cast.caster && o.status == "fighting" && inside(o))
            {
                const auto [dmg, zone] = zoneHit(o, (s == "upheaval" ? 18 : 17) * power * rnd(o.id), "blunt");
                line("hit", o.id, entity(o.id)->name + (s == "upheaval" ? " is thrown by the ground on the " : " is struck by the stone on the ") + zone +
                                        " (" + whole(dmg) + ").", {{o.x, o.y}});
                hurtFighter(b, o, dmg, battle::DownedBlunt, cast.caster, true);
                if (s == "upheaval" && !o.magic.steady())
                    o.magic.fx["prone"] = NextTurn;
            }
        if (s == "hurl_stone" && !cast.target.empty())
            if (const auto* o = b.fighter(cast.target); o && !inside(*o))
                line("miss", cast.target, "The stone smashes where " + entity(o->id)->name + " stood.", cast.tiles);
        if (s == "upheaval")
            b.ground.push_back({cast.x, cast.y, "loose", cast.caster, roundsFor(b, 4), 0});
    }
    else if (s == "fissure")
    {
        line("gift", {}, "The earth splits open!", cast.tiles);
        for (auto& o : b.fighters)
            if (o.id != cast.caster && o.status == "fighting" && inside(o))
            {
                const double dmg = magicDamage(b, o, 15 * power * rnd(o.id), false);
                line("hit", o.id, entity(o.id)->name + " falls into the fissure (" + whole(dmg) + ").", {{o.x, o.y}});
                hurtFighter(b, o, dmg, battle::DownedBlunt, cast.caster, true);
                if (!o.magic.steady())
                    o.magic.fx["prone"] = NextTurn;
            }
        for (const auto& p : cast.tiles)
            b.ground.push_back({p.first, p.second, "fissure", cast.caster, roundsFor(b, 2), 0});
    }
    else if (s == "stone_wall")
    {
        std::vector<std::pair<int, int>> raised;
        for (const auto& p : cast.tiles)
            if (arenaOpen(b, p.first, p.second) && !magicBlocks(b, p.first, p.second))
            {
                b.ground.push_back({p.first, p.second, "wall", cast.caster, roundsFor(b, 8), 30});
                raised.push_back(p);
            }
        line("gift", {}, "A wall of stone heaves up!", raised);
    }
    else if (s == "wave" || s == "flood")
    {
        line("gift", {}, s == "wave" ? ce->name + " sends a wave crashing!" : "Water floods the ground!", cast.tiles);
        const int dx = (cast.x > cf->x) - (cast.x < cf->x), dy = (cast.y > cf->y) - (cast.y < cf->y);
        for (auto& o : b.fighters)
            if (o.id != cast.caster && o.status == "fighting" && inside(o))
            {
                o.burning = 0;
                if (!o.magic.has("warmed"))
                    o.magic.fx["soaked"] = 4;
                if (s == "wave")
                {
                    const double dmg = magicDamage(b, o, 15 * power * rnd(o.id), false);
                    line("hit", o.id, entity(o.id)->name + " is swept off their feet (" + whole(dmg) + ").", {{o.x, o.y}});
                    hurtFighter(b, o, dmg, battle::DownedBlunt, cast.caster, true);
                    throwFighter(b, o, dx, dy, 2, cast.caster, 0);
                    if (!o.magic.steady())
                        o.magic.fx["prone"] = NextTurn;
                }
            }
        b.ground.erase(std::remove_if(b.ground.begin(), b.ground.end(),
                                      [&](const BattleGround& g) {
                                          return g.kind == "fire" && std::find(cast.tiles.begin(), cast.tiles.end(), std::pair<int, int>{g.x, g.y}) != cast.tiles.end();
                                      }),
                       b.ground.end());
        for (const auto& p : cast.tiles)
            b.ground.push_back({p.first, p.second, "water", cast.caster, roundsFor(b, s == "wave" ? 4 : 5), 0});
    }
    else if (s == "pressure_drop")
    {
        line("gift", {}, "The air drops away!", cast.tiles);
        for (auto& o : b.fighters)
            if (o.id != cast.caster && o.status == "fighting" && inside(o))
            {
                auto* d = entity(o.id);
                d->earHealth = std::max(.2, d->earHealth - .1);
                o.magic.fx["deafened"] = NextTurn + 1;
                knockBar(o, 50);
                const double dmg = magicDamage(b, o, 12 * power * rnd(o.id), false);
                line("hit", o.id, d->name + "'s ears pop and bleed (" + whole(dmg) + ").", {{o.x, o.y}});
                hurtFighter(b, o, dmg, battle::DownedBlunt, cast.caster, true);
            }
    }
    else if (s == "shatterhowl")
    {
        line("gift", {}, ce->name + " howls a howl that shatters!", cast.tiles);
        for (auto& o : b.fighters)
            if (o.id != cast.caster && o.status == "fighting" && inside(o))
            {
                o.magic.fx["deafened"] = NextTurn + 1;
                knockBar(o, 40);
                const double dmg = magicDamage(b, o, 24 * power * rnd(o.id), false);
                line("hit", o.id, entity(o.id)->name + " is battered by the howl (" + whole(dmg) + ").", {{o.x, o.y}});
                hurtFighter(b, o, dmg, battle::DownedBlunt, cast.caster, true);
            }
    }
    else if (s == "well")
    {
        b.ground.push_back({cast.x, cast.y, "well", cast.caster, roundsFor(b, 3), 0});
        line("gift", {}, "The ground there grows terribly heavy: a well!", {{cast.x, cast.y}});
    }
}

void World::magicFightStart(Battle& b)
{
    // As the fight proper begins (doc 43): a Seer's side is never surprised (and one sneaker is shown); a Quickened Fire
    // wolf feels the warmth of anyone hidden within 12 tiles.
    for (auto& f : b.fighters)
    {
        const auto* e = entity(f.id);
        if (!e || f.status != "fighting")
            continue;
        if (e->gift == "seer" && !e->quickened)
            for (auto& o : b.fighters)
                if (o.side != f.side && o.unseen)
                {
                    revealFighter(b, o, e->name + " saw " + entity(o.id)->name + " coming.");
                    break;
                }
        if (e->gift == "fire" && e->quickened)
            for (auto& o : b.fighters)
                if (o.side != f.side && o.unseen && apart(o.x, o.y, f.x, f.y) <= 12)
                    revealFighter(b, o, entity(o.id)->name + "'s warmth gives them away.");
    }
}

void World::magicFightEnd(Battle& b)
{
    // After a Trance (doc 45): fatigue, by how deep it went (and two overreaches in it, a degree more), until a full rest.
    for (const auto& f : b.fighters)
    {
        auto* e = entity(f.id);
        if (!e || !e->quickened || f.magic.trance <= 0 || e->dead)
            continue;
        const double top = std::max(f.magic.trance, f.magic.tranceTop);
        const int severity = std::min(3, (top >= 4.5 ? 3 : top >= 3 ? 2 : 1) + f.magic.overreaches / 2);
        auto i = injury::given("trance_fatigue", severity, "", "a Trance");
        i.id = injuryId() + e->id;
        i.gotDay = calendarDays_;
        const auto& now = injury::addAcute(e->injuries, i);
        e->mana = std::min(e->mana, manaPool(*e));
        if (!e->npc)
            notice(f.id, "The Trance leaves you: " + injury::describe(now) + ". A full rest will take it away.");
    }
}

double World::tranceGain(const BattleFighter& f, double perDegree) const
{
    const auto* e = entity(f.id);
    if (f.magic.trance <= 0 || !e)
        return 0;
    const auto d = tranceDepth(e->gift);
    const double t = f.magic.trance;
    const double at2 = d.at2 >= 0 ? d.at2 : d.at3 / 2, at4 = d.at4 >= 0 ? d.at4 : (d.at3 + d.at5) / 2;
    const double degree = t <= 2   ? at2 * (t - 1)
                          : t <= 3 ? at2 + (d.at3 - at2) * (t - 2)
                          : t <= 4 ? d.at3 + (at4 - d.at3) * (t - 3)
                                   : at4 + (d.at5 - at4) * (t - 4);
    return perDegree * degree;
}

bool World::knockBar(BattleFighter& t, double amount, bool weight)
{
    // A Gift knocking a fighter's bar back (doc 45): once between its own turns, however many come at it; mid-turn, its
    // next bar starts lower.
    if (t.magic.has("shaken"))
        return false;
    if (t.acting)
    {
        if (weight)
            t.weight += amount;
        else
            t.staggered = 1;
    }
    else
        t.meter = std::max(0.0, t.meter - amount);
    t.magic.fx["shaken"] = 1;
    return true;
}

double World::tranceLevel(const Battle& b, const BattleFighter& f) const
{
    // Foes standing for each of the side standing (observers and the fallen don't count), 2 to 5.
    const double mine = std::max(1, b.standing(f.side)), theirs = b.standing(1 - f.side);
    return std::clamp(theirs / mine, TranceFloor, TranceCap);
}

double World::manaPool(const Entity& e) const
{
    return battle::manaMax(e.wisdom, !e.gift.empty()) * injury::effects(e.injuries).mana;   // (Less, Trance-fatigued: doc 45.)
}

Result World::enterTrance(const std::string& id)
{
    auto* b = battleFor(id);
    auto* f = b ? b->fighter(id) : nullptr;
    const auto* e = entity(id);
    if (!b || !f || !e)
        return {false, "You are not in a fight.", {}};
    if (const auto why = giftWhyNot(*b, *f, "trance"); !why.empty())
        return {false, why, {}};
    f->magic.trance = f->magic.tranceTop = tranceLevel(*b, *f);
    f->magic.fx["trance"] = -1;
    fightLine(*b, id, {}, "gift", e->name + "'s eyes go far away: a Trance.");
    return {true, "You go into a Trance. It will cost you after the fight, until a full rest.", {}};
}

bool World::npcGift(Battle& b, BattleFighter& f, const BattleFighter& mark)
{
    // NPCs with a Gift use it (doc 43, phase 7), simply: the Quickened strike with theirs when the mark is in reach and
    // the mana is there; the Gifted help.
    const auto* e = entity(f.id);
    if (!e || e->gift.empty() || f.acted || f.casting)
        return false;
    const int d = apart(f.x, f.y, mark.x, mark.y);
    const std::string at = std::to_string(mark.x) + "," + std::to_string(mark.y);
    if (e->quickened && f.magic.trance <= 0 && b.standing(1 - f.side) > b.standing(f.side))
        enterTrance(f.id);                          // (A Quickened NPC outnumbered goes into a Trance: doc 45. It costs no action.)
    const auto tryUse = [&](const std::string& ability, const std::string& target) {
        if (!giftWhyNot(b, f, ability).empty())
            return false;
        const auto* a = gifts::ability(ability);
        if (a && e->mana < a->mana)
            return false;                           // (NPCs don't overreach.)
        return useGift(f.id, ability, target).ok;
    };
    if (e->quickened)
    {
        const std::string& g = e->gift;
        if (g == "fire")
            return (d <= 4 && tryUse("heat_lance", at)) || (d <= 5 && tryUse("flamethrower", at)) || (d == 1 && tryUse("blastwave", ""));
        if (g == "earth")
            return (d <= 10 && d > 1 && tryUse("hurl_stone", mark.id)) || (d <= 6 && tryUse("upheaval", at));
        if (g == "water")
            return tryUse("freeze", mark.id) || (d <= 5 && tryUse("pressure_jet", at));
        if (g == "wind")
            return (d <= 5 && tryUse("battering_gust", at)) || (d == 1 && tryUse("whirlwind", ""));
        if (g == "sound")
            return (d <= 5 && tryUse("shatterhowl", at)) || (d <= 4 && tryUse("dread_note", ""));
        if (g == "blinker")
            return d <= 6 && d > 1 && tryUse("blink_strike", mark.id);
        if (g == "gravity")
            return (d <= 4 && tryUse("hurl", mark.id)) || (d <= 5 && tryUse("slam", mark.id));
        if (g == "seer")
            return d == 1 && tryUse("seen_opening", "");
        return false;
    }
    const std::string& g = e->gift;
    for (const auto& o : b.fighters)
        if (o.side == f.side && o.id != f.id && apart(o.x, o.y, f.x, f.y) <= 1)
        {
            if (g == "fire" && o.bleeding > 0 && tryUse("cauterize", o.id))
                return true;
            if (g == "water" && o.burning > 0 && tryUse("douse", o.id))
                return true;
        }
    for (const auto& o : b.fighters)
        if (g == "gravity" && o.side == f.side && o.status == "downed" && tryUse("lift_up", o.id))
            return true;
    if (g == "blinker" && !f.magic.armed.count("slip"))
        armReaction(f.id, "slip", true);
    if (d == 1)
        return (g == "fire" && tryUse("flare", mark.id)) || (g == "water" && tryUse("splash_eyes", mark.id));
    if (d <= 3 && g == "wind" && !mark.magic.has("dusted"))
        return tryUse("air_blast", mark.id);
    return false;
}

// ------------------------------------------------------------------ Gifts at work (doc 43)

namespace
{
// The workshops a work Gift helps, and how much it lifts their next batch toward a better quality (RatwCrafting.cpp).
struct Lend
{
    const char* ability;
    std::vector<std::string> trades;
    double lift;
    const char* words;
};
const std::vector<Lend>& lends()
{
    static const std::vector<Lend> out = {
        {"forge_heat", {"smithy", "ironworks", "foundry", "armory", "tinker", "jeweler", "bakery", "pottery", "brickworks", "glassworks", "smokehouse", "inn", "charcoal"},
         15, "keeps the fire at working heat"},
        {"kindle", {"bakery", "inn", "smokehouse", "charcoal", "brewery"}, 8, "gets the fire going"},
        {"clay_hand", {"pottery", "brickworks", "mason"}, 15, "works the clay and stone true"},
        {"stone_sense", {"mason", "ironworks", "foundry"}, 10, "finds the flaws in the stone before they're cut"},
        {"draw_water", {"tannery", "dyeworks", "brewery", "papermill", "saltworks", "dairy"}, 12, "draws the water clean and where it's wanted"},
        {"winnow_and_dry", {"mill", "bakery", "tannery", "herbalist", "apothecary", "weaver", "dyeworks", "perfumer"}, 12, "winnows and dries the work"},
        {"bellows", {"smithy", "ironworks", "foundry", "armory", "glassworks", "jeweler", "tinker"}, 12, "breathes the forge hotter"},
        {"ring_true", {"smithy", "armory", "jeweler", "tinker", "mason", "glassworks"}, 12, "hears the flaws in the metal"},
        {"settle", {"smithy", "armory", "tinker", "saddlery", "tannery"}, 12, "settles the weight of the work"},
    };
    return out;
}
} // namespace

Result World::useWorkGift(const std::string& id, const std::string& ability, const std::string& target)
{
    auto* e = entity(id);
    const auto* a = gifts::ability(ability);
    if (!e || e->npc || e->dead)
        return {false, "No such character.", {}};
    if (!a || a->family != e->gift || a->quickened != e->quickened || !a->work)
        return {false, "That isn't one of your Gift's ways of working.", {}};
    if (a->kind == "passive")
        return {false, a->name + " is always with you.", {}};
    if (inBattle(id))
        return {false, "Not in a fight.", {}};
    if (e->downedLeft > 0)
        return {false, "You are down.", {}};
    if (e->mana + 1e-9 < a->mana)
        return {false, "Too little mana (" + whole(a->mana) + " needed).", {}};
    const std::string key = id + "|" + ability;
    const bool lend = std::any_of(lends().begin(), lends().end(), [&](const Lend& l) { return ability == l.ability; });
    const double rest = lend ? 600 : 60;            // (A workshop once in ten minutes; the rest once a minute.)
    if (const auto at = workGiftAt_.find(key); at != workGiftAt_.end() && time_ - at->second < rest)
        return {false, a->name + " needs a moment more (" + whole(rest - (time_ - at->second)) + " s).", {}};
    const auto spend = [&] {
        e->mana -= a->mana;
        workGiftAt_[key] = time_;
    };
    if (lend)
    {
        // Lent to a workshop near by: its next batch is the better for it, and it pays a little for the help.
        const auto& l = *std::find_if(lends().begin(), lends().end(), [&](const Lend& x) { return ability == x.ability; });
        const Entity* maker = nullptr;
        double best = 4.5;
        for (const auto* o : entitiesIn(e->cellId))
        {
            if (!o || !o->npc || o->dead || (!target.empty() && o->id != target))
                continue;
            const auto* r = society_.spec(o->id);
            const auto* business = r ? items::businessFor(r->workLabel) : nullptr;
            const double d = std::hypot(o->position.x - e->position.x, o->position.y - e->position.y);
            if (business && std::find(l.trades.begin(), l.trades.end(), business->id) != l.trades.end() && d < best)
            {
                best = d;
                maker = o;
            }
        }
        if (!maker)
            return {false, "No workshop your " + a->name + " would help is close by.", {}};
        spend();
        society_.lendGift(maker->id, l.lift, calendarDays_ + 1);
        award(id, "work", "lend:" + maker->id + ":" + std::to_string(std::int64_t(calendarDays_)));   // (Doc 44.)
        const auto till = society_.tillOf(maker->id);
        const auto* purse = society_.account(till);
        const bool paid = purse && purse->cash >= 4 && society_.shift(till, id, "", 0, 4, "a Gift's help");
        return {true, "You lend " + maker->name + " your Gift: it " + l.words + ". Their next batch will be the better for it" +
                          (paid ? ", and they pay you 4p." : "."),
                maker->id};
    }
    if (ability == "mend")
    {
        Entity* who = e;
        if (!target.empty())
        {
            who = entity(target);
            if (!who || who->cellId != e->cellId || std::hypot(who->position.x - e->position.x, who->position.y - e->position.y) > 2.5)
                return {false, "Get close to them first.", {}};
        }
        const std::int64_t day = std::int64_t(std::floor(calendarDays_));
        for (auto& i : who->injuries)
            if (i.kind == "acute" && i.restLeft > 0 && (!mendedDay_.count(i.id) || std::int64_t(mendedDay_[i.id]) < day))
            {
                spend();
                i.restLeft -= i.restLeft / 3;       // Healing half again as fast, from here.
                mendedDay_[i.id] = double(day);
                std::string what = i.type;
                std::replace(what.begin(), what.end(), '_', ' ');
                return {true, std::string(who == e ? "You tend your own " : "You tend " + who->name + "'s ") + what +
                                  " with clean water: it will heal the sooner.", who->id};
            }
        return {false, who == e ? "You have no injury to mend today." : who->name + " has no injury to mend today.", {}};
    }
    if (ability == "shortcut")
    {
        // Three body lengths the way it faces, over whatever is between, if there is ground to land on (not out of
        // the place: a blink stays in it).
        const double ang = e->facing;
        for (int reach = 3; reach >= 2; --reach)
        {
            const Vec2 to{e->position.x + std::cos(ang) * reach, e->position.y + std::sin(ang) * reach};
            if (standable(e->cellId, to))
            {
                spend();
                e->position = to;
                return {true, "You hop, and blink across.", {}};
            }
        }
        return {false, "There's nowhere to land that way.", {}};
    }
    if (ability == "lighten_load")
    {
        spend();
        e->lightLoadUntil = time_ + 600;
        refreshLoad(*e);
        return {true, "Your load grows light: you can carry half again as much for a while.", {}};
    }
    if (ability == "carry")
    {
        spend();
        e->carryVoiceUntil = time_ + 120;
        return {true, "For a little while, your voice carries as far as a shout.", {}};
    }
    if (ability == "dowse" || ability == "echo")
    {
        const auto* c = cell(e->cellId);
        if (!c)
            return {false, "Nothing here to read.", {}};
        spend();
        if (ability == "dowse")
        {
            double best = 1e9, bx = 0, by = 0;
            for (int y = 0; y < c->height; ++y)
                for (int x = 0; x < c->width; ++x)
                    if (const auto* t = c->tile(x, y); t && t->terrain == Terrain::Water)
                        if (const double d = std::hypot(x + .5 - e->position.x, y + .5 - e->position.y); d < best)
                        {
                            best = d;
                            bx = x + .5;
                            by = y + .5;
                        }
            if (best > 1e8)
                return {true, "You feel for water: there is none under this ground.", {}};
            static const char* ways[] = {"east", "south-east", "south", "south-west", "west", "north-west", "north", "north-east"};
            return {true, "You feel water about " + whole(best) + " paces to the " + ways[battle::octant(bx - e->position.x, by - e->position.y)] + ".", {}};
        }
        int open = 0, ways = 0;
        for (int y = 0; y < c->height; ++y)
            for (int x = 0; x < c->width; ++x)
                if (const auto* t = c->tile(x, y); t && !t->solid)
                {
                    ++open;
                    ways += t->glyph == '+' || t->glyph == 'G';
                }
        return {true, "Your hum comes back: about " + std::to_string(c->width) + " by " + std::to_string(c->height) + " paces, " +
                          std::to_string(open) + " of it open ground, " + std::to_string(ways) + (ways == 1 ? " doorway." : " doorways."),
                {}};
    }
    return {false, a->name + " is always with you.", {}};
}

void World::tendGiftSenses()
{
    // Danger Sense (a Gifted Seer, doc 43): a bandit lying in wait near by is felt before it is seen.
    if (time_ - giftSensesAt_ < 1)
        return;
    giftSensesAt_ = time_;
    for (const auto& [id, folk] : folk_)
    {
        if (folk.kind != "bandit")
            continue;
        const auto* bandit = entity(id);
        if (!bandit || bandit->dead || inBattle(id))
            continue;
        for (const auto* o : entitiesIn(bandit->cellId))
        {
            if (!o || o->npc || o->gift != "seer" || o->quickened || inBattle(o->id) || dangerTold_.count({o->id, id}))
                continue;
            const double d = std::hypot(bandit->position.x - o->position.x, bandit->position.y - o->position.y);
            if (d > 24)
                continue;
            dangerTold_.insert({o->id, id});
            static const char* ways[] = {"east", "south-east", "south", "south-west", "west", "north-west", "north", "north-east"};
            notice(o->id, "Danger Sense: someone waits for you, about " + whole(d) + " paces to the " +
                              ways[battle::octant(bandit->position.x - o->position.x, bandit->position.y - o->position.y)] + ".");
        }
    }
}
} // namespace ratw
