// Fights as players see and drive them (Docs/Design/33-combat.md; the rules are the world's, RatwBattle.cpp): the
// arena in a fighter's or watcher's snapshot, the red squares onlookers see, and the fight commands.
#include "RatwGame.h"

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
    return hurt < 25 ? "Scratched" : hurt < 50 ? "Wounded" : hurt < 75 ? "Badly hurt" : "Limping";
}
} // namespace

Value Game::battleView(const Battle& b, const std::string& viewer) const
{
    auto v = Value::object();
    v.add("id", b.id);
    v.add("over", b.over);
    v.add("banner", b.banner);
    v.add("pvp", b.pvp);
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
    v.add("turn", b.turn);
    if (const auto* t = world_.entity(b.turn))
        v.add("turnName", names::capitalised(labelFor(viewer, t->id)));
    v.add("turnLeft", b.turn.empty() ? 0.0 : std::max(0.0, b.deadline - world_.time()));
    v.add("moved", b.moved);
    v.add("acted", b.acted);
    v.add("round", b.turns);
    v.add("watching", double(b.observers.size()));
    const auto veiled = veilMap(viewer);           // Names this wolf doesn't know, as the fighters look (doc 32).
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
        o.add("name", names::capitalised(labelFor(viewer, f.id)));   // As this wolf knows them (doc 32).
        o.add("side", f.side);
        o.add("x", f.x);
        o.add("y", f.y);
        o.add("facing", f.facing);
        o.add("status", f.status);
        o.add("npc", e->npc);
        o.add("away", f.away);
        o.add("label", healthLabel(e->hurt, f.status == "downed", f.status == "dead"));
        o.add("health", std::round(100 - e->hurt));
        if (!e->mouth.empty())
            o.add("mouth", e->mouth);
        if (f.burning > 0)
            o.add("burning", f.burning);
        if (f.casting)
            o.add("casting", true);
        if (!b.truceBy.empty())
            o.add("truce", f.truce);
        if (f.id == viewer)
            o.add("stamina", std::round(e->stamina));
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
        o.add("meter", std::round(cast.meter));
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
    // The next six turns, as the meters stand (the Tactics turn list).
    struct Next
    {
        std::string id;
        double meter, gain;
        int order;
    };
    std::vector<Next> next;
    for (const auto& f : b.fighters)
        if (f.status == "fighting" || f.status == "downed")
            if (const auto* e = world_.entity(f.id))
                next.push_back({f.id, f.id == b.turn ? 0.0 : f.meter, battle::meterGain(effectiveDexterity(*e)), f.order});
    auto order = Value::array();
    if (!b.turn.empty())
        order.push(b.turn);
    for (int k = 0; k < 6 && !next.empty() && !b.over; ++k)
    {
        double need = 1e18;
        for (const auto& n : next)
            need = std::min(need, std::max(0.0, std::ceil((100 - n.meter) / n.gain)));
        for (auto& n : next)
            n.meter += n.gain * need;
        auto pick = next.begin();
        for (auto it = next.begin(); it != next.end(); ++it)
            if (it->meter > pick->meter + 1e-9 || (std::abs(it->meter - pick->meter) <= 1e-9 && it->order < pick->order))
                pick = it;
        order.push(pick->id);
        pick->meter = 0;
    }
    v.add("order", order);
    auto reach = Value::array();
    if (!observer && b.turn == viewer)
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
        for (int side = 0; side < 2; ++side)
            for (const auto& f : b.fighters)
                if (f.side == side && f.status != "fled")
                    if (const auto* e = world_.entity(f.id))
                    {
                        o.add(side == 0 ? "side0" : "side1", names::capitalised(labelFor(self.id, f.id)));
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
    else if (verb == "join")
        result = world_.joinBattle(id, j.string("battle"), int(j.number("side", -1)));
    else if (verb == "observe")
        result = world_.observeBattle(id, j.string("battle"));
    else if (verb == "leave")
        result = world_.leaveObserving(id);
    else if (verb == "bite" || verb == "tend" || verb == "flee" || verb == "struggle" || verb == "wait" || verb == "sword" ||
             verb == "roll" || verb == "hold" || verb == "stow" || verb == "pickup" || verb == "truce")
        result = world_.battleAct(id, verb, target);
    else
        return false;
    if (result.ok)
        updateMovementModes();
    return true;
}

} // namespace ratw::game
