// The gathering howl and chorus (Docs/Design/51-scenes-and-stars.md, Phase 6; doc 48 §3.5). A wolf howls to say "come
// and find me": players far off hear it as a direction and a distance, never a place or a name; others near may join
// it, and a chorus carries further; residents near turn toward it, and a few say something; the town remembers it a
// while, and tells of it when asked. The numbers and residents' lines are in Data/Social/social.json.
#include "RatwGame.h"
#include "RatwNames.h"

#include <algorithm>
#include <cmath>

namespace ratw::game
{
using json::Value;

namespace
{
constexpr double Pi = 3.14159265358979323846;
const char* const Compass[] = {"north", "north-east", "east", "south-east", "south", "south-west", "west", "north-west"};

std::uint32_t mix(const std::string& text)
{
    std::uint32_t h = 2166136261u;
    for (const unsigned char ch : text)
        h = (h ^ ch) * 16777619u;
    return h;
}
} // namespace

double Game::howlCooldownLeft(const std::string& id) const
{
    const auto it = howledAt_.find(id);
    return it == howledAt_.end() ? 0 : std::max(0.0, stars::rules().howl.cooldown - (world_.time() - it->second));
}

Result Game::howl(const std::string& id)
{
    const auto& r = stars::rules().howl;
    auto* e = world_.entity(id);
    if (!e || e->npc)
        return {false, "There is no one to howl.", {}};
    const auto* place = world_.cell(e->cellId);
    if (const auto* fight = world_.battleOf(id); fight && !fight->over)
        return {false, "Not in a fight.", {}};
    if (e->dead || e->downedLeft > 0)
        return {false, "You can't howl, down as you are.", {}};
    if (world_.custodyOf(id))
        return {false, "Not while the watch holds you.", {}};
    if (!e->mouth.empty())
        return {false, "Not with something in your mouth.", {}};
    if (const double left = howlCooldownLeft(id); left > 0)
        return {false, "Your voice needs a rest: another " + std::to_string(int(std::ceil(left / 60))) + " min.", {}};
    if (!place)
        return {false, "There is nowhere to howl from.", {}};
    const double t = world_.time();
    const double wx = place->worldX + e->position.x, wy = place->worldY + e->position.y;
    howledAt_[id] = t;
    // A chorus near, still open: this howl joins it, and keeps it open a little longer (to twenty seconds at most).
    Chorus* chorus = nullptr;
    for (auto& ch : choruses_)
        if (t < ch.closes && std::hypot(ch.wx - wx, ch.wy - wy) <= r.chorusRange &&
            std::find(ch.howlers.begin(), ch.howlers.end(), id) == ch.howlers.end())
            chorus = &ch;
    const bool joined = chorus != nullptr;
    if (chorus)
    {
        chorus->howlers.push_back(id);
        chorus->closes = std::min(chorus->started + r.chorusMost, chorus->closes + r.chorusExtend);
    }
    else
    {
        choruses_.push_back({"howl-" + guid().substr(0, 12), e->cellId, wx, wy, place->worldZ, t, t + r.chorusWindow, {id},
                             !place->outdoors, {}});
        chorus = &choruses_.back();
    }
    // A real sound: a wolf sneaking about gives itself away to residents near enough to hear it, as a yell does.
    for (const auto* other : world_.entitiesIn(e->cellId))
        if (other->npc && !other->dead && std::hypot(other->position.x - e->position.x, other->position.y - e->position.y) <= r.sneakRange)
            world_.heardVoice(other->id, id);
    sendHowl(*chorus);
    if (!joined)
        residentsHear(*chorus);
    saveSoon();
    return {true, joined ? "You join the howl." : "You howl.", {}};
}

void Game::sendHowl(Chorus& ch)
{
    // Each player who can hear it: a direction (jittered, so a hiding wolf can't be found by it) and a distance in words,
    // never a place or a name; one mark for the chorus, its count updated as wolves join.
    const auto& r = stars::rules().howl;
    const auto& first = ch.howlers.front();
    const double carry = std::min(r.chorusCarryMost, 1 + r.chorusCarry * double(ch.howlers.size() - 1));
    std::string status;
    if (const auto p = profiles_.find(first); p != profiles_.end())
        status = p->second.status == "lfs" ? "Looking for a scene" : p->second.status == "ooc" ? "Out of character" : "";
    for (auto* c : clients_)
    {
        const auto& listener = c->entityId;
        if (listener.empty() || std::find(ch.howlers.begin(), ch.howlers.end(), listener) != ch.howlers.end())
            continue;
        if (std::any_of(ch.howlers.begin(), ch.howlers.end(), [&](const std::string& h) { return hides(listener, h); }))
            continue;                               // (Muted or blocked by this listener: never heard, doc 50.)
        const auto* me = world_.entity(listener);
        const auto* where = me ? world_.cell(me->cellId) : nullptr;
        if (!me || !where || std::abs(where->worldZ - ch.wz) > r.heightLimit)
            continue;
        const double lx = where->worldX + me->position.x, ly = where->worldY + me->position.y;
        const double apart = std::hypot(ch.wx - lx, ch.wy - ly);
        double range = r.range * carry * world_.hearingSensitivity(*me) * world_.environmentAt(where->id, me->position).hearing;
        if (ch.indoors)
            range *= r.indoorsHowler;
        if (!where->outdoors)
            range *= r.indoorsListener;
        if (apart > range)
            continue;
        // North is up (y down): 0° north, clockwise.
        const double jitter = (double(mix(ch.id + "|" + listener) % 2001) / 1000.0 - 1.0) * r.jitter;
        double bearing = std::atan2(ch.wx - lx, -(ch.wy - ly)) * 180 / Pi + jitter;
        bearing = std::fmod(bearing + 360, 360);
        const auto band = apart < range / 3 ? "near" : apart < range * 2 / 3 ? "far" : "very far";
        const bool canJoin = apart <= r.chorusRange && howlCooldownLeft(listener) <= 0 && world_.time() < ch.closes;
        auto e = Value::object();
        e.add("type", "howl");
        e.add("id", ch.id);
        e.add("bearing", std::round(bearing));
        e.add("band", band);
        if (!status.empty())
            e.add("status", status);
        e.add("wolves", int(ch.howlers.size()));
        e.add("canJoin", canJoin);
        e.add("until", r.markSeconds);
        send(c, e);
        const auto toward = Compass[int(std::floor((bearing + 22.5) / 45)) % 8];
        const std::string distance = std::string(band) == "near" ? "near by" : band == std::string("far") ? "far off" : "very far off";
        if (ch.heard.insert(listener).second)
            system(c, std::string("A howl rises to the ") + toward + ", " + distance + "." + (canJoin ? " You could join it." : ""));
        else
            system(c, std::string("More wolves join the howl to the ") + toward + ": " + std::to_string(ch.howlers.size()) + " now.");
    }
}

void Game::residentsHear(const Chorus& ch)
{
    // Residents near turn toward it (a few), and one or two say something: curious by day, grumbling at night in town.
    const auto& r = stars::rules().howl;
    struct Near
    {
        Entity* e;
        double apart;
        std::string cell;
    };
    std::vector<Near> near;
    for (const auto& [cid, cell] : world_.cells())
    {
        if (std::abs(cell.worldX - ch.wx) > r.residentRange + 1024 || std::abs(cell.worldY - ch.wy) > r.residentRange + 1024)
            continue;
        for (const auto* other : world_.entitiesIn(cid))
            if (other->npc && !other->dead && !other->transient && !world_.battleOf(other->id) && !pendingNpc_.count(other->id))
                if (const double apart = std::hypot(cell.worldX + other->position.x - ch.wx, cell.worldY + other->position.y - ch.wy);
                    apart <= r.residentRange)
                    near.push_back({world_.entity(other->id), apart, cid});
    }
    std::sort(near.begin(), near.end(), [](const Near& a, const Near& b) { return a.apart < b.apart; });
    int turned = 0, spoke = 0;
    for (const auto& n : near)
    {
        if (!n.e || turned >= r.residentsTurn)
            break;
        const auto* cell = world_.cell(n.cell);
        n.e->turnTarget = std::atan2(ch.wy - (cell->worldY + n.e->position.y), ch.wx - (cell->worldX + n.e->position.x));
        n.e->turning = true;
        ++turned;
        if (spoke >= r.residentsSpeak)
            continue;
        const bool night = world_.environmentAt(n.cell, n.e->position).phase == "night";
        const bool town = !world_.communityOf(n.cell).empty();
        const auto lines = r.lines.find(night ? (town ? "nightTown" : "night") : "day");
        if (lines == r.lines.end() || lines->second.empty())
            continue;
        ParsedPost post;
        post.ok = true;
        post.speech = true;
        post.segments.push_back({"speech", lines->second[mix(ch.id + n.e->id) % lines->second.size()]});
        publish(n.e->id, post, Voice::Speak, {}, {});
        npcLastSpeech_[n.e->id] = world_.time();
        ++spoke;
    }
}

void Game::tendHowls()
{
    // Choruses closing: those who howled together grow closer (once a pair a game day), it becomes a moment in any scene
    // they share, and the town nearest remembers it for a while (residents tell of it when asked).
    const auto& r = stars::rules().howl;
    const double t = world_.time();
    for (auto it = choruses_.begin(); it != choruses_.end();)
    {
        if (t < it->closes)
        {
            ++it;
            continue;
        }
        const auto& ch = *it;
        const auto day = std::floor(world_.calendarDays());
        for (std::size_t i = 0; i < ch.howlers.size(); ++i)
            for (std::size_t j = i + 1; j < ch.howlers.size(); ++j)
            {
                const auto& a = ch.howlers[i];
                const auto& b = ch.howlers[j];
                const auto key = a < b ? a + "|" + b : b + "|" + a;
                if (const auto last = chorusBondDay_.find(key); last == chorusBondDay_.end() || last->second != day)
                {
                    chorusBondDay_[key] = day;
                    world_.bonds().change(a, b, {1, 0, 1, 0, 0}, world_.calendarDays());
                    world_.bonds().change(b, a, {1, 0, 1, 0, 0}, world_.calendarDays());
                }
            }
        if (ch.howlers.size() > 1)
        {
            std::string together;
            for (const auto& h : ch.howlers)
                together += (together.empty() ? "" : ",") + h;
            std::set<std::string> scenes;
            for (const auto& h : ch.howlers)
                for (const auto& sid : social_.scenesOf(h))
                    scenes.insert(sid);
            for (const auto& sid : scenes)
                social_.moment(sid, {"chorus", ch.howlers.front(), {}, together, now()});
        }
        if (const auto* cell = world_.cell(ch.cell))
            if (const auto town = world_.communityOf(ch.cell); !town.empty())
                recentHowls_[town] = {t, std::string(ch.howlers.size() > 1 ? std::to_string(ch.howlers.size()) + " wolves howled together"
                                                                            : "a wolf howled") +
                                             " near " + cell->name};
        it = choruses_.erase(it);
        saveSoon();
    }
    for (auto it = recentHowls_.begin(); it != recentHowls_.end();)
        it = t - it->second.first > r.rememberedSeconds ? recentHowls_.erase(it) : std::next(it);
}
} // namespace ratw::game
