// Taverns (Docs/Design/54-gathering-places.md, 1; Phase 1). A common room is an inn's or tavern's cell (where a post
// of the `inn` business works: Game::innCells_). Every 5 s the game tells the world which common rooms have players in
// them, which of those were active in the last 10 minutes (moved or said anything), and whether a performer is there;
// the world's rest reads it (World::restRate): 1.25 rest hours an hour, +10% for each other active player up to +30%,
// or +30% with a performer. Residents don't count: the room always has some. A full rest needs a bed the wolf has a
// right to (the user, 2026-10-08): its Chapter's own place now, its lodging and a paid inn bed when renting comes
// (Phase 3). Performing earns nothing (doc 48's principle 2); listeners may give and star.
#include "RatwGame.h"

#include <algorithm>
#include <cmath>

namespace ratw::game
{
using json::Value;

namespace
{
constexpr double ActiveSeconds = 600, RestSeconds = 600;
const char* const Instruments[] = {"mouth_pipe", "paw_drum", "hurdy_gurdy", "handbell"};
} // namespace

bool Game::hasBedRight(const std::string& id, const std::string& cell) const
{
    // Its Chapter's own rented place (doc 32); its lodging (a bed it rents, or a whole place: doc 54, 4).
    if (const auto* lease = estates_.lease(cell))
        if (const auto* mine = chapters_.of(id); mine && mine->id == lease->chapter)
            return true;
    if (const auto* l = lodgingOf(id); l && l->cell == cell)
    {
        if (l->x < 0)
            return true;                            // (A whole place: any bed in it.)
        if (const auto* e = world_.entity(id))
            return int(std::floor(e->position.x)) == l->x && int(std::floor(e->position.y)) == l->y;
    }
    return false;
}

bool Game::performCommand(Connection* c, const json::Value& j, Result& result)
{
    // {"verb": "start", "kind": "sing" | "tale" | an instrument carried} / "stop".
    const auto me = c->entityId;
    const auto* e = world_.entity(me);
    if (!e)
        return result = {false, "No such character.", {}}, true;
    if (j.string("verb") == "stop")
    {
        if (!performers_.erase(me))
            return result = {false, "You aren't performing.", {}}, true;
        performRestUntil_[me] = now() + RestSeconds / 3;
        return result = {true, "You finish, to whatever applause there is.", {}}, true;
    }
    refreshLetterPlaces();
    const auto* venue = wholeLodgingAt(e->cellId);
    if (!innCells_.count(e->cellId) && !(venue && venue->open))
        return result = {false, "Perform in an inn's or tavern's common room, or a place opened for the night.", {}}, true;
    if (const auto rest = performRestUntil_.find(me); rest != performRestUntil_.end() && rest->second > now())
        return result = {false, "Rest your voice a while first.", {}}, true;
    for (const auto& [who, p] : performers_)
        if (p.cell == e->cellId && who != me)
            return result = {false, "Someone is already performing here.", {}}, true;
    const auto kind = j.string("kind", "sing");
    if (kind != "sing" && kind != "tale")
    {
        const bool instrument = std::any_of(std::begin(Instruments), std::end(Instruments), [&](const char* i) { return kind == i; });
        const auto* purse = world_.society().account(me);
        if (!instrument || !purse || Society::stock(*purse, kind) <= 0)
            return result = {false, "Sing, tell a tale, or play an instrument you carry.", {}}, true;
    }
    performers_[me] = {e->cellId, kind, now(), now()};
    const auto words = kind == "sing" ? "begins to sing" : kind == "tale" ? "begins a tale" : "begins to play";
    for (auto* other : clients_)
        if (other && other->entityId != me)
            if (const auto* o = world_.entity(other->entityId); o && o->cellId == e->cellId)
                system(other, names::capitalised(labelFor(other->entityId, me)) + " " + words + ". The room settles to listen.");
    world_.recordEvent({"performance", me, {}, e->cellId, 0, 0, kind, 0, 0, {}});
    return result = {true, std::string("You ") + (kind == "sing" ? "begin to sing" : kind == "tale" ? "begin a tale" : "begin to play") +
                               ". Keep at it (say or do something every two minutes); those resting here rest better for it.",
                     {}},
           true;
}

void Game::tendTaverns(double dt)
{
    tavernsAccumulator_ += dt;
    if (tavernsAccumulator_ < 5)
        return;
    tavernsAccumulator_ = 0;
    // Performers: gone from the room, quiet two minutes, or at it half an hour.
    for (auto it = performers_.begin(); it != performers_.end();)
    {
        const auto* e = world_.entity(it->first);
        auto* c = clientOf(it->first);
        std::string why;
        if (!e || !c || e->cellId != it->second.cell)
            why = "You have left the room; the performance is over.";
        else if (now() - it->second.lastSaid > options_.performQuietSeconds)
            why = "Your performance trails off.";
        else if (now() - it->second.started > options_.performLongestSeconds)
            why = "You have performed a long while; rest your voice.";
        if (why.empty())
        {
            ++it;
            continue;
        }
        if (c)
            system(c, why);
        performRestUntil_[it->first] = now() + RestSeconds;
        it = performers_.erase(it);
    }
    // The common rooms with players in them: who is active, and a performer.
    std::map<std::string, World::CommonRoom> rooms;
    for (auto* c : clients_)
    {
        const auto* e = c ? world_.entity(c->entityId) : nullptr;
        const auto* venue = e ? wholeLodgingAt(e->cellId) : nullptr;
        if (!e || e->npc || (!innCells_.count(e->cellId) && !(venue && venue->open)))
            continue;                               // (A common room, or a place opened for a player-run night: doc 54, 4.)
        auto& room = rooms[e->cellId];
        if (const auto at = lastActiveReal_.find(e->id); at != lastActiveReal_.end() && now() - at->second <= ActiveSeconds)
            room.active.insert(e->id);
    }
    for (const auto& [who, p] : performers_)
        if (rooms.count(p.cell))
            rooms[p.cell].performer = true;
    world_.setCommonRooms(std::move(rooms));
}
} // namespace ratw::game
