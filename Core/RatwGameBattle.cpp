// Fights as players see and drive them (Docs/Design/33-combat.md; the rules are the world's, RatwBattle.cpp): the
// arena in a fighter's or watcher's snapshot, the red squares onlookers see, and the fight commands.
#include "RatwGame.h"
#include "RatwWire.h"

#include <algorithm>
#include <cmath>

namespace ratw::game
{
using json::Value;

namespace
{
std::string healthLabel(double hurt, bool downed, bool dead)
{
    if (dead)
        return "Dead";
    if (downed)
        return "Downed";
    return hurt < 1 ? "Unhurt" : hurt < 25 ? "Scratched" : hurt < 50 ? "Wounded" : hurt < 75 ? "Badly hurt" : "Limping";
}
} // namespace

Value Game::battleView(const Battle& b, const std::string& viewer) const
{
    auto v = Value::object();
    v.add("id", b.id);
    v.add("over", b.over);
    v.add("banner", names::veil(b.banner, veilMap(viewer)));     // (Names as this wolf knows them.)
    v.add("pvp", b.pvp);
    v.add("terms", b.terms);                        // "blood", "yield" or "death" (doc 37).
    v.add("crime", !b.incident.empty());            // A resident set on: the watch will hear of it.
    v.add("yieldBy", b.yieldBy);
    auto arena = Value::object();
    arena.add("x", b.x0);
    arena.add("y", b.y0);
    arena.add("w", b.w);
    arena.add("h", b.h);
    v.add("arena", arena);
    // The arena's ground, all of it: a fighter sees the whole of the place they fight in.
    auto rows = Value::array();
    if (const auto* c = world_.cell(b.cellId); c && c->loaded)
        for (int y = b.y0; y < b.y0 + b.h; ++y)
        {
            std::string row;
            row.reserve(std::size_t(b.w));
            for (int x = b.x0; x < b.x0 + b.w; ++x)
            {
                const auto* t = c->tile(x, y);
                row += t ? t->glyph : ' ';
            }
            rows.push(row);
        }
    v.add("rows", rows);
    const auto* mine = b.fighter(viewer);
    const bool observer = !mine || mine->status == "fled";
    auto you = Value::object();
    you.add("observer", observer);
    if (!observer)
    {
        you.add("side", mine->side);
        you.add("status", mine->status);
        you.add("struggling", mine->struggling);
        you.add("burning", mine->burning);
        you.add("casting", mine->casting);
        you.add("truce", mine->truce);
        if (const auto* e = world_.entity(viewer))
        {
            you.add("canStruggle", mine->status == "downed" && !mine->struggling && world_.recoveryAvailable(*e));
            // Pace and stamina (doc 33): how far a move goes at this pace, what a tile costs, what comes back next turn.
            const int pace = world_.fightPace(*e);
            you.add("pace", pace);
            you.add("moveRange", battle::moveRange(effectiveDexterity(*e), e->hurt, pace));
            you.add("tileStamina", battle::tileStamina(pace));
            you.add("resting", mine->resting);
            you.add("stats", [&] {
                auto st = Value::object();
                st.add("dex", std::round(effectiveDexterity(*e)));
                st.add("baseDex", std::round(e->dexterity));
                st.add("str", std::round(e->strength));
                st.add("wis", std::round(e->wisdom));
                return st;
            }());
            you.add("mouth", e->mouth);
            if (const auto* purse = world_.society().account(viewer))
                you.add("swords", Society::stock(*purse, "sword"));
            if (!e->gift.empty())
            {
                you.add("gift", e->gift);
                you.add("quickened", e->quickened);
                you.add("mana", std::floor(e->mana));
                const auto& spell = e->quickened ? battle::QuickenedFlame : battle::GiftedFlame;
                you.add("flameLength", spell.length);
                you.add("flameAngle", spell.halfAngle);
                you.add("flameMana", spell.mana);
            }
        }
    }
    v.add("you", you);
    // This wolf's own turn (several fighters may be taking theirs at once: doc 33).
    const bool acting = !observer && mine->acting;
    v.add("turn", acting ? viewer : std::string());
    v.add("turnLeft", acting ? std::max(0.0, mine->deadline - world_.time()) : 0.0);
    v.add("moved", acting && mine->moved);
    v.add("acted", acting && mine->acted);
    v.add("round", b.turns);
    v.add("watching", double(b.observers.size()));
    const auto veiled = veilMap(viewer);           // Names this wolf doesn't know, as the fighters look (doc 32).
    const auto called = strangerNames(viewer);
    auto fighters = Value::array();
    for (const auto& f : b.fighters)
    {
        if (f.status == "fled")
            continue;
        const auto* e = world_.entity(f.id);
        if (!e)
            continue;
        auto o = Value::object();
        o.add("id", f.id);
        const auto stranger = called.find(f.id);           // As this wolf knows them (doc 32).
        o.add("name", names::capitalised(stranger != called.end() ? stranger->second : labelFor(viewer, f.id)));
        o.add("side", f.side);
        o.add("x", f.x);
        o.add("y", f.y);
        o.add("facing", f.facing);
        o.add("status", f.status);
        o.add("npc", e->npc);
        // The initiative bar: how full, how fast it fills (a second), and whether it is full and waiting its turn.
        o.add("meter", std::round(std::max(0.0, f.meter) * 10) / 10);
        o.add("rate", f.acting || f.meter >= 100 || (f.status != "fighting" && f.status != "downed")
                          ? 0.0
                          : battle::meterGain(effectiveDexterity(*e)) * battle::MeterPerSecond);
        o.add("acting", f.acting);
        if (f.acting)
            o.add("turnLeft", std::max(0.0, f.deadline - world_.time()));
        o.add("away", f.away);
        // What drives the bars, for their tooltips: stamina back a turn (doubled resting), the bar's fill time.
        o.add("regen", std::round(battle::staminaPerTurn(e->hurt, e->strength) * (f.resting ? battle::RestFactor : 1) * 10) / 10);
        o.add("fillSeconds", std::round(100 / (battle::meterGain(effectiveDexterity(*e)) * battle::MeterPerSecond)));
        if (f.resting)
            o.add("resting", true);
        // Injuries, named (doc 38): what is wrong with them, and what it does. Shown on their card, never drawn on them.
        auto injuries = Value::array();
        bool injured = false;
        const auto whole = [](double n) { return std::to_string(int(std::lround(n))); };
        const auto injury = [&](const std::string& kind, const std::string& name, const std::string& does) {
            injured = true;
            auto i = Value::object();
            i.add("kind", kind);
            i.add("name", name);
            i.add("does", does);
            injuries.push(i);
        };
        if (f.status == "downed")
            injury("down", "Down", "Can't fight; crawls a tile. Up when the time runs out, or tended.");
        if (f.burning > 0)
            injury("burning", "Burning", whole(battle::BurnDamage) + " damage at the start of each turn, " + std::to_string(f.burning) +
                                             " more. Roll to put it out.");
        if (f.bleeding > 0)
            injury("bleeding", "Bleeding", whole(battle::BleedDamage) + " damage at the start of each turn, " + std::to_string(f.bleeding) +
                                               " more.");
        if (f.staggered > 0)
            injury("staggered", "Staggered", "A hard blow: their bar set back " + whole(battle::StaggerSetback) + ".");
        if (e->exhausted && f.status == "fighting")
            injury("winded", "Winded", "Out of breath: can only walk, and can't bite or swing, until stamina is back to 20.");
        if (e->hurt >= 75 && f.status == "fighting")
            injury("limping", "Limping", "Badly hurt: moves less, and gets less stamina back a turn.");
        else if (e->hurt >= 50 && f.status == "fighting")
            injury("hurt", "Badly hurt", "Moves less, and gets half the stamina back a turn.");
        else if (e->hurt >= 25 && f.status == "fighting")
            injury("wounded", "Wounded", "Moves a little less; three quarters of the stamina back a turn.");
        if (injured)
            o.add("injuries", injuries);
        o.add("label", f.status == "yielded" ? std::string("Yielded") : healthLabel(e->hurt, f.status == "downed", f.status == "dead"));
        o.add("health", std::round(100 - e->hurt));
        if (!e->mouth.empty())
            o.add("mouth", e->mouth);
        if (f.burning > 0)
            o.add("burning", f.burning);
        if (f.casting)
            o.add("casting", true);
        if (!b.truceBy.empty())
            o.add("truce", f.truce);
        // How they look, for the fight screen's portraits (doc 37); everyone's breath and mana, on their cards.
        o.add("appearance", wire::appearance(e->appearance));
        o.add("lifeStage", lifeStageName(lifeStage(e->age)));
        {
            o.add("stamina", std::round(e->stamina));
            if (!e->gift.empty())
            {
                o.add("mana", std::floor(e->mana));
                o.add("manaMax", std::floor(battle::manaMax(e->wisdom, true)));
            }
        }
        // A foe, as this wolf would strike them from where it stands now: the chance, the blow, and whether in reach.
        if (!observer && mine->status == "fighting" && f.side != mine->side && f.status == "fighting")
            if (const auto* me = world_.entity(viewer))
            {
                const bool sword = me->mouth == "sword";
                auto odds = Value::object();
                odds.add("hit", std::round(world_.strikeChance(*mine, f) * 100));
                // Head on (no side or back to it): the page adds the bonus for a strike from any other tile.
                auto headOn = f;
                headOn.facing = battle::octant(mine->x - f.x, mine->y - f.y);
                odds.add("base", std::round(world_.strikeChance(*mine, headOn) * 100));
                odds.add("damage", std::round((sword ? battle::SwordDamage : battle::BiteDamage) * (.6 + me->strength / 125)));
                odds.add("reach", std::max(std::abs(f.x - mine->x), std::abs(f.y - mine->y)) <= (sword ? battle::SwordReach : 1));
                o.add("odds", odds);
            }
        if (f.status == "downed" && mine && mine->side == f.side)
            o.add("downedLeft", std::round(e->downedLeft));   // Their own side sees how long they have.
        fighters.push(o);
    }
    v.add("fighters", fighters);
    v.add("truceBy", b.truceBy);
    const auto tileList = [](const std::vector<std::pair<int, int>>& tiles) {
        auto list = Value::array();
        for (const auto& [x, y] : tiles)
        {
            auto p = Value::array();
            p.push(x);
            p.push(y);
            list.push(p);
        }
        return list;
    };
    // Spells gathering (their cones, locked), things on the arena's floor, smoke.
    auto casts = Value::array();
    for (const auto& cast : b.casts)
    {
        auto o = Value::object();
        o.add("caster", cast.caster);
        o.add("left", std::max(0.0, cast.firesAt - world_.time()));      // The countdown everyone sees.
        o.add("of", cast.firesAt - cast.castAt);
        o.add("quickened", cast.quickened);
        o.add("tiles", tileList(cast.tiles));
        casts.push(o);
    }
    v.add("casts", casts);
    auto drops = Value::array();
    for (const auto& d : b.drops)
    {
        auto o = Value::object();
        o.add("x", d.x);
        o.add("y", d.y);
        o.add("item", d.item);
        drops.push(o);
    }
    v.add("drops", drops);
    std::vector<std::pair<int, int>> smoke;
    for (const auto& s : b.smoke)
        smoke.push_back(s.first);
    v.add("smoke", tileList(smoke));
    auto reach = Value::array();
    if (acting)
        for (const auto& [x, y] : world_.battleReach(viewer))
        {
            auto p = Value::array();
            p.push(x);
            p.push(y);
            reach.push(p);
        }
    v.add("reach", reach);
    auto lines = Value::array();
    const std::size_t from = b.log.size() > 24 ? b.log.size() - 24 : 0;
    for (std::size_t i = from; i < b.log.size(); ++i)
    {
        auto line = Value::object();
        line.add("seq", double(b.log[i].seq));
        line.add("kind", b.log[i].kind);
        line.add("actor", b.log[i].actor);
        line.add("target", b.log[i].target);
        line.add("text", names::veil(b.log[i].text, veiled));
        if (!b.log[i].tiles.empty())
            line.add("tiles", tileList(b.log[i].tiles));
        lines.push(line);
    }
    v.add("log", lines);
    return v;
}

Value Game::fightsInView(const Entity& self) const
{
    // The red squares an onlooker sees: each fight in this cell they can see a fighter of, drawn around its lineup.
    auto out = Value::array();
    std::map<std::string, std::string> called;
    bool named = false;
    for (const auto& b : world_.battles())
    {
        if (b.cellId != self.cellId || b.fighter(self.id))
            continue;
        double x0 = 1e18, y0 = 1e18, x1 = -1e18, y1 = -1e18;
        bool seen = false;
        int sides[2] = {0, 0};
        auto lineup = Value::array();
        for (const auto& f : b.fighters)
        {
            if (f.status == "fled")
                continue;
            const auto* e = world_.entity(f.id);
            if (!e)
                continue;
            x0 = std::min(x0, e->position.x);
            y0 = std::min(y0, e->position.y);
            x1 = std::max(x1, e->position.x);
            y1 = std::max(y1, e->position.y);
            sides[f.side] += f.status == "fighting";
            seen = seen || world_.visionClarity(self, *e, world_.sightRange(self)) > 0;
        }
        if (!seen)
            continue;
        auto o = Value::object();
        o.add("id", b.id);
        o.add("x0", std::floor(x0) - 1);
        o.add("y0", std::floor(y0) - 1);
        o.add("x1", std::floor(x1) + 2);
        o.add("y1", std::floor(y1) + 2);
        o.add("standing0", sides[0]);
        o.add("standing1", sides[1]);
        o.add("round", b.turns);
        o.add("over", b.over);
        const bool locked = b.fled.count(self.id) || b.observed.count(self.id);
        o.add("canJoin", !b.over && !locked && !self.dead && self.downedLeft <= 0 && self.age >= battle::YoungestFighter &&
                             !world_.inBattle(self.id));
        o.add("canObserve", !b.over && !world_.inBattle(self.id));
        o.add("watching", b.observers.count(self.id) > 0);
        // For the story's one entry per fight (doc 18): how much has happened, and the latest of it.
        o.add("actions", double(b.seq));
        if (!b.log.empty())
            o.add("latest", veilFor(self.id, b.log.back().text));
        // A name on each side, for "join X's side".
        if (!named)
        {
            called = strangerNames(self.id);
            named = true;
        }
        for (int side = 0; side < 2; ++side)
            for (const auto& f : b.fighters)
                if (f.side == side && f.status != "fled")
                    if (const auto* e = world_.entity(f.id))
                    {
                    {
                        const auto stranger = called.find(f.id);
                        o.add(side == 0 ? "side0" : "side1",
                              names::capitalised(stranger != called.end() ? stranger->second : labelFor(self.id, f.id)));
                    }
                        break;
                    }
        out.push(o);
    }
    return out;
}

bool Game::battleCommand(Connection* c, const Value& j, Result& result)
{
    // {"type":"battle","verb":"move","x":..,"y":..} / "bite" "tend" "flee" "struggle" "wait" (+ "target") /
    // "join" (+ "battle", "side") / "observe" (+ "battle") / "leave" (stop watching).
    const auto& id = c->entityId;
    const std::string verb = j.string("verb"), target = j.string("target");
    if (verb == "move")
    {
        const double x = j.number("x", -1), y = j.number("y", -1);
        if (!std::isfinite(x) || !std::isfinite(y))
            result = {false, "Move where?", {}};
        else
            result = world_.battleMove(id, int(std::floor(x)), int(std::floor(y)));
    }
    else if (verb == "face")
    {
        const double dir = j.number("dir", -1);
        result = world_.battleFace(id, std::isfinite(dir) ? int(dir) : -1);
    }
    else if (verb == "flame")
    {
        const double x = j.number("x", -1), y = j.number("y", -1);
        result = std::isfinite(x) && std::isfinite(y)
                     ? world_.battleAct(id, "flame", std::to_string(int(std::floor(x))) + "," + std::to_string(int(std::floor(y))))
                     : Result{false, "Aim it: which way?", {}};
    }
    else if (verb == "agree" || verb == "refuse")
        result = world_.answerTruce(id, verb == "agree");
    else if (verb == "spare" || verb == "press")
        result = world_.answerYield(id, verb == "spare");   // A yield accepted (spare them), or not (press on).
    else if (verb == "join")
        result = world_.joinBattle(id, j.string("battle"), int(j.number("side", -1)));
    else if (verb == "observe")
        result = world_.observeBattle(id, j.string("battle"));
    else if (verb == "leave")
        result = world_.leaveObserving(id);
    else if (verb == "bite" || verb == "tend" || verb == "flee" || verb == "struggle" || verb == "wait" || verb == "sword" ||
             verb == "roll" || verb == "rest" || verb == "hold" || verb == "stow" || verb == "pickup" || verb == "truce" ||
             verb == "back" || verb == "yield")
        result = world_.battleAct(id, verb, target);
    else
        return false;
    if (result.ok)
        updateMovementModes();
    return true;
}

} // namespace ratw::game
