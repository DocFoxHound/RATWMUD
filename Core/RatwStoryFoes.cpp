// A story's foes (Docs/Design/86, C10): foe.add, foe.leave, foe.outlaw and the fights with them. The rules of the fight
// itself are RatwBattle.cpp's; this file keeps the table (Data/Storylines/foes.json), makes and ends the foes, walks a
// hunting foe to the nearest wolf, and lets a beaten or timed-out foe go.
#include "RatwStoryFoes.h"

#include "RatwJsonDoc.h"
#include "RatwNames.h"
#include "RatwWorld.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>

namespace ratw
{
namespace foes
{
namespace
{
struct Table
{
    std::map<std::string, Strength> kinds;
    std::string followerName = "one of {name}'s band", followerLook = "Hard-faced and armed, at their leader's back.", tuned, error;
};

// Data/Storylines/foes.json: SIM_FOES (a trial table), RATW_DATA_DIR, the working directory or one above it, the source tree.
std::filesystem::path tablePath()
{
    namespace fs = std::filesystem;
    std::error_code ec;
    if (const char* trial = std::getenv("SIM_FOES"); trial && *trial)
        return trial;
    if (const char* dir = std::getenv("RATW_DATA_DIR"); dir && *dir)
        return fs::path(dir) / "Storylines" / "foes.json";
    for (auto at = fs::current_path(ec); !ec && !at.empty(); at = at.parent_path())
    {
        if (fs::exists(at / "Data" / "Storylines" / "foes.json", ec))
            return at / "Data" / "Storylines" / "foes.json";
        if (at == at.parent_path())
            break;
    }
#ifdef RATW_SOURCE_DIR
    return fs::path(RATW_SOURCE_DIR) / "Data" / "Storylines" / "foes.json";
#else
    return fs::path("Data") / "Storylines" / "foes.json";
#endif
}

Table build()
{
    Table t;
    // The starting table (engine spec C10, rule 4), kept when the file cannot be read.
    t.kinds = {{"tough", {60, 60, 20, 25}}, {"fierce", {70, 68, 24, 15}}, {"champion", {80, 75, 30, 0}}, {"follower", {52, 55, 16, 35}}};
    const auto path = tablePath();
    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        t.error = "cannot read " + path.string();
        return t;
    }
    std::stringstream text;
    text << in.rdbuf();
    json::Value doc;
    std::string problem;
    if (!json::parse(text.str(), doc, problem) || !doc.isObject())
    {
        t.error = path.string() + ": " + problem;
        return t;
    }
    for (auto& [kind, s] : t.kinds)
        if (const auto& j = doc[kind]; j.isObject())
            s = {std::clamp(j.number("skill", s.skill), 1.0, 150.0), std::clamp(j.number("strength", s.strength), 1.0, 150.0),
                 std::clamp(j.number("hp", s.hp), 4.0, 200.0), std::clamp(j.number("flee", s.flee), 0.0, 100.0)};
    t.followerName = doc.string("followerName", t.followerName);
    t.followerLook = doc.string("followerLook", t.followerLook);
    t.tuned = doc.string("tuned");
    return t;
}

const Table& table()
{
    static const Table t = build();
    return t;
}
} // namespace

const Strength* strength(const std::string& kind)
{
    const auto& k = table().kinds;
    const auto found = k.find(kind);
    return found == k.end() ? nullptr : &found->second;
}

bool leaderKind(const std::string& kind)
{
    return kind == "tough" || kind == "fierce" || kind == "champion";
}

std::string followerName(const std::string& leader)
{
    auto words = table().followerName;
    if (const auto at = words.find("{name}"); at != std::string::npos)
        words.replace(at, 6, leader);
    return words;
}

std::string followerLook() { return table().followerLook; }
std::string tuned() { return table().tuned; }
std::string loadError() { return table().error; }

double hurtScale(const std::string& kind)
{
    const auto* s = strength(kind);
    return s ? 20.0 / s->hp : 1.0;
}
} // namespace foes

namespace
{
constexpr int MostBand = 8;
constexpr double HuntRange = 12, WaitRange = 6, TendEvery = .5;   // (Tiles; seconds.)

bool foeWord(const std::string& id)
{
    return id.size() >= 3 && id.size() <= 40 && ((id[0] >= 'a' && id[0] <= 'z') || (id[0] >= '0' && id[0] <= '9')) &&
           std::all_of(id.begin(), id.end(), [](char c) { return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-'; });
}

double apart(Vec2 a, Vec2 b) { return std::hypot(a.x - b.x, a.y - b.y); }

std::string folkId(const std::string& camp, int i) { return "road:" + camp + ":" + std::to_string(i); }
} // namespace

const World::StoryFoe* World::storyFoe(const std::string& camp) const
{
    const auto found = storyFoes_.find(camp);
    return found == storyFoes_.end() ? nullptr : &found->second;
}

const World::StoryFoe* World::storyFoeOf(const std::string& id) const
{
    if (storyFoes_.empty())
        return nullptr;
    if (const auto f = folk_.find(id); f != folk_.end())
        return f->second.kind == "foe" ? storyFoe(f->second.of) : nullptr;
    for (const auto& [camp, foe] : storyFoes_)
        if (!foe.spec.npc.empty() && foe.spec.npc == id)
            return &foe;
    return nullptr;
}

bool World::foeLeader(const std::string& id) const
{
    const auto* foe = storyFoeOf(id);
    return foe && (foe->spec.npc.empty() ? id == folkId(foe->spec.camp, 0) : id == foe->spec.npc);
}

bool World::foeInFight(const std::string& camp) const
{
    const auto* foe = storyFoe(camp);
    if (!foe)
        return false;
    for (const auto& b : battles_)                   // (Over or not: a fight is let go of only once it is finished.)
            for (const auto& f : b.fighters)
                if ((f.id == foe->spec.npc && !foe->spec.npc.empty()) || f.id.rfind("road:" + camp + ":", 0) == 0)
                    return true;
    return false;
}

Result World::addStoryFoe(const StoryFoeSpec& given)
{
    auto spec = given;
    if (!storyFoeCamp(spec.camp) || !foeWord(spec.foe))
        return {false, "A foe's id is 3 to 40 small letters, digits, - and _.", spec.foe};
    if (storyFoes_.count(spec.camp) || entity(folkId(spec.camp, 0)))
        return {false, "That foe is already here.", spec.foe};
    if (spec.name.empty() || spec.name.size() > 60 || spec.name.find_first_of("\r\n") != std::string::npos)
        return {false, "Give the foe a name (up to 60 letters).", spec.foe};
    if (!foes::strength(spec.tier))   // (The story's are tough, fierce or champion; foe_sim may set a follower alone.)
        return {false, "A foe is tough, fierce or champion.", spec.foe};
    if (spec.stance != "waits" && spec.stance != "hunts")
        return {false, "A foe waits or hunts.", spec.foe};
    if (spec.band < 0 || spec.band > MostBand)
        return {false, "A foe has 0 to 8 followers.", spec.foe};
    if (!std::isfinite(spec.until) || spec.until <= time_)
        return {false, "A foe stays a minute to a day.", spec.foe};
    if (!ensureLoaded(spec.cell).ok || !cell(spec.cell))
        return {false, "No such place.", spec.foe};
    const auto* place = cell(spec.cell);
    if (!std::isfinite(spec.x) || !std::isfinite(spec.y) || spec.x < 0 || spec.y < 0 || spec.x >= place->width || spec.y >= place->height)
        return {false, "That isn't a tile of " + place->name + ".", spec.foe};
    const Vec2 at{std::floor(spec.x) + .5, std::floor(spec.y) + .5};
    if (!standable(spec.cell, at))
        return {false, "No one can stand there.", spec.foe};
    spec.x = at.x;
    spec.y = at.y;
    BanditCamp camp{spec.camp, spec.cell, 3.0 * (spec.band + 1), 0, -100, true};
    camp.x = at.x;
    camp.y = at.y;
    roads_.camps.push_back(camp);
    StoryFoe foe;
    foe.spec = spec;
    storyFoes_[spec.camp] = foe;
    foesEnded_.erase(spec.camp);
    // The leader, then the band around them (each on the nearest open tile, never in a doorway).
    const auto* lead = foes::strength(spec.tier);
    const auto* follow = foes::strength("follower");
    for (int i = 0; i <= spec.band; ++i)
    {
        Vec2 spot = at;
        if (i > 0)
            for (int ring = 1; ring <= 3; ++ring)
            {
                bool found = false;
                for (int k = 0; k < 8 && !found; ++k)
                {
                    const double a = (i * 3 + k) * 3.14159265358979323846 / 4;
                    const Vec2 p{std::floor(at.x + std::cos(a) * ring) + .5, std::floor(at.y + std::sin(a) * ring) + .5};
                    if (standable(spec.cell, p) && !nearPortal(spec.cell, p, 1))
                    {
                        spot = p;
                        found = true;
                    }
                }
                if (found)
                    break;
            }
        const auto id = folkId(spec.camp, i);
        auto& e = addRoadFolk(id, i == 0 ? spec.name : foes::followerName(spec.name),
                              i == 0 ? (spec.description.empty() ? std::string("A hard stranger, watching the wolves.") : spec.description)
                                     : foes::followerLook(),
                              spec.cell, spot, "foe", spec.camp);
        e.offstage = false;
        e.activity = spec.stance == "hunts" ? "hunting" : "waiting";
        if (i == 0 && spec.hasLook)
            e.appearance = spec.look;
        const auto* s = i == 0 ? lead : follow;
        e.strength = s->strength;
        folk_[id].skill = s->skill;
    }
    entitiesChanged();
    index_.dirty = true;
    return {true, spec.name + (spec.band ? " and " + std::to_string(spec.band) + (spec.band == 1 ? " follower" : " followers") : std::string()) +
                      " come to " + place->name + ".",
            spec.camp};
}

Result World::outlawResident(const StoryFoeSpec& given)
{
    auto spec = given;
    auto* e = entity(spec.npc);
    if (!e || !e->npc || e->transient)
        return {false, "No such resident.", spec.npc};
    if (!storyFoeCamp(spec.camp))
        return {false, "Invalid foe.", spec.npc};
    if (isProtected(spec.npc))
        return {false, "That resident is protected: no story makes them a foe.", spec.npc};
    if (e->dead)
        return {false, "That resident has died.", spec.npc};
    if (e->downedLeft > 0)
        return {false, "That resident is down.", spec.npc};
    if (inBattle(spec.npc))
        return {false, "That resident is in a fight.", spec.npc};
    if (!e->leaderId.empty() || !cell(e->cellId))
        return {false, "That resident is away.", spec.npc};      // (With a party, or nowhere: offstage is no matter.)
    if (storyFoeOf(spec.npc) || storyFoes_.count(spec.camp))
        return {false, "That resident is a foe already.", spec.npc};
    if (!foes::leaderKind(spec.tier))
        return {false, "A foe is tough, fierce or champion.", spec.npc};
    if (spec.band < 0 || spec.band > 6)
        return {false, "A resident made a foe has 0 to 6 followers.", spec.npc};
    if (!std::isfinite(spec.until) || spec.until <= time_)
        return {false, "A resident is a foe for a minute to twelve hours.", spec.npc};
    spec.name = e->name;
    spec.cell = e->cellId;
    spec.x = e->position.x;
    spec.y = e->position.y;
    spec.stance = "waits";
    BanditCamp camp{spec.camp, spec.cell, 3.0 * (spec.band + 1), 0, -100, true};
    camp.x = spec.x;
    camp.y = spec.y;
    roads_.camps.push_back(camp);
    StoryFoe foe;
    foe.spec = spec;
    storyFoes_[spec.camp] = foe;
    foesEnded_.erase(spec.camp);
    const auto* follow = foes::strength("follower");
    for (int i = 1; i <= spec.band; ++i)
    {
        Vec2 spot = e->position;
        for (int ring = 1; ring <= 3; ++ring)
        {
            bool found = false;
            for (int k = 0; k < 8 && !found; ++k)
            {
                const double a = (i * 3 + k) * 3.14159265358979323846 / 4;
                const Vec2 p{std::floor(e->position.x + std::cos(a) * ring) + .5, std::floor(e->position.y + std::sin(a) * ring) + .5};
                if (standable(spec.cell, p) && !nearPortal(spec.cell, p, 1))
                {
                    spot = p;
                    found = true;
                }
            }
            if (found)
                break;
        }
        const auto id = folkId(spec.camp, i);
        auto& f = addRoadFolk(id, foes::followerName(spec.name), foes::followerLook(), spec.cell, spot, "foe", spec.camp);
        f.offstage = false;
        f.strength = follow->strength;
        folk_[id].skill = follow->skill;
        e = entity(spec.npc);                       // (The map may have moved it.)
    }
    entitiesChanged();
    index_.dirty = true;
    return {true, names::capitalised(spec.name) + " turns on the wolves" +
                      (spec.band ? " with " + std::to_string(spec.band) + (spec.band == 1 ? " follower" : " followers") : std::string()) + ".",
            spec.npc};
}

void World::dropStoryFoe(const std::string& camp, const std::string& how)
{
    std::vector<std::string> mine;
    for (const auto& [id, f] : folk_)
        if (f.of == camp && f.kind == "foe")
            mine.push_back(id);
    for (const auto& id : mine)
        removeRoadFolk(id);
    endEncounter(camp, 0);
    roads_.camps.erase(std::remove_if(roads_.camps.begin(), roads_.camps.end(), [&](const BanditCamp& c) { return c.id == camp; }),
                       roads_.camps.end());
    storyFoes_.erase(camp);
    foesEnded_[camp] = how;
    entitiesChanged();
}

bool World::endStoryFoe(const std::string& camp, const std::string& why, bool evenInAFight)
{
    const auto* foe = storyFoe(camp);
    if (!foe || (!evenInAFight && foeInFight(camp)))
        return false;
    const std::string how = foe->beaten ? "beaten" : why;
    if (!why.empty())
    {
        const auto* lead = entity(foe->spec.npc.empty() ? folkId(camp, 0) : foe->spec.npc);
        WorldEvent ev{"foe fled", {}, foe->spec.npc.empty() ? foe->spec.foe : foe->spec.npc, lead ? lead->cellId : foe->spec.cell};
        ev.item = why;
        ev.detail = foe->spec.name;
        if (lead)
        {
            ev.x = int(std::floor(lead->position.x));
            ev.y = int(std::floor(lead->position.y));
        }
        recordEvent(ev);
    }
    dropStoryFoe(camp, how);
    return true;
}

void World::foeFightEnds(Battle& b)
{
    // A fight with a story's foe is over: who fought it remembered; the downed of the band carried off (never left to die
    // in the road); a resident made a foe and beaten got up again where they lie (or, the story saying so, dies).
    const auto* found = storyFoe(b.camp);
    if (!found)
        return;
    auto& foe = storyFoes_[b.camp];
    for (const auto& f : b.fighters)
        if (const auto* e = entity(f.id); e && !e->npc && !storyFoeOf(f.id))
            foe.fought.insert(f.id);
    for (const auto& f : b.fighters)
    {
        if (!storyFoeOf(f.id) || storyFoeOf(f.id) != &foe)
            continue;
        auto* e = entity(f.id);
        if (!e)
            continue;
        if (f.id == foe.spec.npc)
        {
            if (e->downedLeft > 0 || f.status == "downed")
            {
                if (foe.spec.lethal)
                    setDead(f.id, true);
                else
                    standUp(*e, 25);
            }
            continue;
        }
        if (e->downedLeft > 0 || f.status == "downed" || f.status == "dead" || foe.beaten || foe.fled)
            removeRoadFolk(f.id);
    }
}

void World::tendStoryFoes()
{
    if (storyFoes_.empty() || time_ - foesTendedAt_ < TendEvery)
        return;
    foesTendedAt_ = time_;
    std::vector<std::pair<std::string, std::string>> ending;   // (camp, why)
    for (auto& [camp, foe] : storyFoes_)
    {
        const bool fighting = foeInFight(camp);
        const auto& s = foe.spec;
        Entity* lead = entity(s.npc.empty() ? folkId(camp, 0) : s.npc);
        if (fighting)
            continue;
        if (foe.beaten || !lead || lead->dead)
        {
            ending.push_back({camp, {}});
            continue;
        }
        if (foe.fled)
        {
            ending.push_back({camp, "fled"});
            continue;
        }
        if (time_ >= s.until)
        {
            ending.push_back({camp, "time"});
            continue;
        }
        // The band keeps by its leader; a hunting foe goes for the nearest wolf within 12 tiles, a waiting one for a wolf
        // who comes within 6 of where it stands. Never one down, in a fight, or just out of one.
        const Vec2 post = s.stance == "hunts" || !s.npc.empty() ? lead->position : Vec2{s.x, s.y};
        const double range = s.stance == "hunts" ? HuntRange : WaitRange;
        Entity* mark = nullptr;
        double best = range;
        if (s.npc.empty())
            for (const Entity* other : entitiesIn(lead->cellId))
            {
                if (!other || other->npc || other->dead || other->downedLeft > 0 || other->cellId != lead->cellId || inBattle(other->id) ||
                    other->age < battle::YoungestFighter)
                    continue;
                if (const auto settle = settleUntil_.find(other->id); settle != settleUntil_.end() && time_ < settle->second)
                    continue;
                if (const double d = apart(other->position, post); d <= best)
                {
                    best = d;
                    mark = entity(other->id);
                }
            }
        if (mark && apart(lead->position, mark->position) <= battle::StartReach)
        {
            // (Within a fight's reach: it begins, the band and the Watch on duty brought in by startBattle.)
            stop(lead->id);
            if (startBattle(lead->id, mark->id, false).ok)
                continue;
        }
        if (s.npc.empty() && apart(lead->position, {s.x, s.y}) > 2 && lead->path.empty() && s.stance == "waits")
        {
            auto& f = folk_[lead->id];
            if (time_ >= f.nextPath)
            {
                f.nextPath = time_ + 3;
                moveTo(lead->id, s.x, s.y);         // (Back to the ground it holds.)
            }
        }
        for (int i = 1; i <= s.band; ++i)
            if (auto* b = entity(folkId(camp, i)); b && !b->dead && b->downedLeft <= 0 && b->cellId == lead->cellId &&
                                                   apart(b->position, lead->position) > 2.5)
            {
                auto& f = folk_[b->id];
                if (time_ >= f.nextPath)
                {
                    f.nextPath = time_ + 1.5;
                    moveTo(b->id, lead->position.x, lead->position.y);
                }
            }
    }
    for (const auto& [camp, why] : ending)
        endStoryFoe(camp, why);
}
} // namespace ratw
