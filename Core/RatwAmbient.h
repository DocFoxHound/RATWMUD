#pragma once
// The ambient director (Docs/Design/26-living-npcs.md, Phase 10): now and then, where a player can hear, two residents
// standing together talk about something worth voicing: a rumour one has heard and the other hasn't, news from one's
// life, an old grudge, the day. The world picks who and what (World::ambientPicks) and applies what the talk changes
// (World::ambientSpoken: gossip passed on is believed, rivals sour, friends warm); the game has it voiced, by the
// NPC Mind or authored lines, and heard.
#include <map>
#include <string>
#include <vector>

namespace ratw
{
struct AmbientTopic
{
    std::string kind;                   // "gossip", "news", "quarrel", "friends", "day".
    std::string subject, claim, source, incident;   // Gossip: what the teller believes, and whose word it is.
    double confidence = 0;
    std::vector<std::string> facts;     // Plain statements the words may draw on, and nothing else.
    double score = 0;
    // For the written scenes (RatwScenes.h, doc 30): the moment's conditions (band, place, item=herbs...) and blanks.
    std::map<std::string, std::string> tags, blanks;
};
struct AmbientPick
{
    std::string teller, listener, cell;  // `teller` opens (and, for gossip, is the one who knows).
    AmbientTopic topic;
};
// Something that happened in a resident's life lately, as they would tell it.
struct AmbientNews
{
    double day = 0;
    std::string kind, text;
};
// Something that happened in a town lately, as its people talk of it: a wedding, a death, a theft, a newcomer.
struct TownNews
{
    double day = 0;
    std::string kind;                   // A scene tag: marriage, death, apprentice, succession, theft, assault, arrest...
    std::string subject, other;         // Whom it concerns (IDs).
};
} // namespace ratw
