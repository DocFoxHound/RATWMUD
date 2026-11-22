#include "RatwGame.h"
#include "RatwStoryRegion.h"

#include <algorithm>
#include <cmath>
#include <optional>

namespace ratw::game
{
namespace
{
int countField(const json::Value& p, const char* key, int upper)
{
    const auto& value = p[key];
    if (!value.isNumber())
        return 0;
    const double n = value.asNumber();
    return n >= 1 && n <= upper && std::floor(n) == n ? int(n) : 0;
}
} // namespace

Result Game::applyStoryArea(const std::string& kind, const std::string& run, const json::Value& payload)
{
    const auto* release = storylines_.find(run);
    const int visit = countField(payload, "visit", 1000000000);
    if (!visit || !release || release->kind != "world" || release->source != "storykeeper" ||
        !release->live() || release->sourceRef != std::to_string(visit))
        return {false, "This story visit is no longer active.", run};

    const auto area = story::Region::read(payload);
    const auto* cell = area ? world_.cell(area->cell) : nullptr;
    if (!cell || payload.string("region").empty() || area->x2 >= cell->width || area->y2 >= cell->height)
        return {false, "The story area is outside its cell.", run};

    const auto bound = release->regions.find(payload.string("region"));
    if (bound == release->regions.end() || bound->second.cell != area->cell ||
        bound->second.x1 != area->x1 || bound->second.y1 != area->y1 || bound->second.x2 != area->x2 || bound->second.y2 != area->y2)
        return {false, "The requested area differs from the published story.", run};

    if (kind == "story.area_spawn")
    {
        const auto result = world_.storyBandits(area->cell, area->x1, area->y1, area->x2, area->y2, countField(payload, "count", 6));
        if (result.ok)
            record(Roads);
        return result;
    }
    const bool damage = kind == "story.area_damage", item = kind == "story.area_item", buff = kind == "story.area_buff";
    const int points = countField(payload, "points", 99), quantity = countField(payload, "quantity", 100),
              hours = countField(payload, "hours", 168);
    const std::string good = payload.string("item");
    if (!(damage && points) && !(item && quantity && itemValid(good)) && !(buff && hours && payload.string("buff") == "fed"))
        return {false, "Unknown area effect or invalid amount.", run};

    std::vector<std::string> here, full;
    for (const auto& [id, participant] : release->participants)
    {
        const auto* e = world_.entity(id);
        if (!participant.active() || !e || e->npc || e->dead || e->offstage || e->downedLeft > 0)
            continue;
        const auto [x, y] = world_.eventTile(*e);
        if (!area->contains(e->cellId, x, y))
            continue;
        if (!damage && !cheatsOn() && hostsOwnWolf(id))
            continue;
        // A full pack misses this reward alone. Refusing the whole effect would pause the story for everyone, and any
        // one participant could keep it paused by standing in the area with a full pack.
        if (item && !world_.society().canCreate(id, good, quantity))
        {
            full.push_back(id);
            continue;
        }
        here.push_back(id);
    }
    // The server processes one action at a time. Check every pack before granting the first item.
    for (const auto& id : here)
    {
        auto& e = *world_.entity(id);
        if (item)
            world_.society().create(id, good, quantity, "dm grant: story area " + payload.string("region"));
        else if (damage)
            e.hurt = std::max(e.hurt, std::min(99., e.hurt + points));
        else
            e.fedUntil = std::max(e.fedUntil, world_.calendarDays() + hours / 24.);
    }
    // Even an empty area gets a receipt. A replay after a crash must not target a later arrival.
    recordCharacters(item ? Economy : Character, here);
    for (const auto& id : here)
        tellActedOn(id, item ? "You receive " + std::to_string(quantity) + " " + Society::itemName(good) + "."
                         : damage ? "You are hurt in " + payload.string("region") + "." : "You feel well fed.");
    for (const auto& id : full)
        tellActedOn(id, "You have no room to carry " + std::to_string(quantity) + " " + Society::itemName(good) + ".");
    return {true, "Area effect reached " + std::to_string(here.size()) + " participants" +
                      (full.empty() ? std::string(".") : "; " + std::to_string(full.size()) + " had no room."), run};
}
// A Dungeon Master's words to a world story's wolves (doc 85, Phase 2, story.tell): to all of them, one of them, or the ones
// standing in one of its regions. {visit, text, to: participants | wolf | region, player?, region?, title?}. A wolf in the
// world sees it as a banner and a line; one away is told when back (tellActedOn: on a community world, kept with the wolf).
// Never refused for whoever is away: a message must not pause the story.
Result Game::applyStoryTell(const std::string& run, const json::Value& payload)
{
    const auto* release = storylines_.find(run);
    const int visit = countField(payload, "visit", 1000000000);
    if (!visit || !release || release->kind != "world" || release->source != "storykeeper" || !release->live() ||
        release->sourceRef != std::to_string(visit))
        return {false, "This story visit is no longer active.", run};
    const auto text = payload.string("text"), to = payload.string("to");
    if (text.empty() || json::utf8Prefix(text, 300) != text || text.find_first_of("\r\n") != std::string::npos)
        return {false, "A story message is one line of up to 300 characters.", run};
    std::optional<story::Region> area;
    if (to == "region")
    {
        area = story::Region::read(payload["region"]);
        const auto bound = release->regions.find(payload["region"].string("id"));
        if (!area || bound == release->regions.end() || bound->second.cell != area->cell || bound->second.x1 != area->x1 ||
            bound->second.y1 != area->y1 || bound->second.x2 != area->x2 || bound->second.y2 != area->y2)
            return {false, "The region differs from the published story.", run};
    }
    else if (to == "wolf")
    {
        const auto found = release->participants.find(payload.string("player"));
        if (found == release->participants.end() || !found->second.active())
            return {false, "That wolf is not in this story.", run};
    }
    else if (to != "participants")
        return {false, "Tell the participants, a wolf or a region.", run};
    const auto title = payload.string("title", release->title);
    const auto words = (title.empty() ? std::string() : "\"" + title + "\": ") + text;
    int told = 0, away = 0;
    for (const auto& [id, participant] : release->participants)
    {
        if (!participant.active() || (to == "wolf" && id != payload.string("player")))
            continue;
        auto* c = clientOf(id);
        const auto* e = world_.entity(id);
        if (area)
        {
            if (!c || !e || e->npc || e->offstage)
                continue;
            const auto [x, y] = world_.eventTile(*e);
            if (!area->contains(e->cellId, x, y))
                continue;
        }
        if (c)
        {
            storylineToast(c, words);
            ++told;
        }
        else
        {
            tellActedOn(id, words);
            ++away;
        }
    }
    return {true, "Told " + std::to_string(told) + " in the world" + (away ? "; " + std::to_string(away) + " away, told when back." : "."), run};
}
} // namespace ratw::game
