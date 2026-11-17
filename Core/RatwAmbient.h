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
    // The talk library's facts for it (doc 62): pair.band, the moment's (here.place...) and the topic's own
    // (topic.item=herbs, topic.price=3 pennies...).
    std::map<std::string, std::string> talk;
    std::string as;                     // A deed told by the doer's name: the name (doc 56).
    // What the teller says of it, in their own words (topic.said; doc 84 §7.7, defect 9): a claim from its point of view
    // in claims.json, one's own news. Empty for a topic with nothing to tell.
    std::string said;
    // A claim passed on when it is spoken (gossip, a deed, a camp the carters spoke of): believed by the listener, with its
    // kind, its second party and its place (doc 84 §7.7).
    bool passes = false;
    std::string claimKind, object, place;
    // Whether the teller saw it with their own eyes ("yes"), only heard it ("no"), or it isn't a thing one sees (""): the
    // scorecard's T3 counts an eyewitness's words said without it (doc 84 §15.3).
    std::string seen;
    // What it is about, so the director knows it again (doc 84 §8.5, Phase 5): a claim's subject and words, a wagon, a
    // ship, a good's price, an incident; an everyday topic's own name. The ledger's pair notes and a place's day keep it
    // (novelty, saturation, the told set).
    std::string key;
};
struct AmbientPick
{
    std::string teller, listener, cell;  // `teller` opens (and, for gossip, is the one who knows).
    std::string hearer;                  // The player it was picked for (doc 79, 11: the prices are that player's list's).
    AmbientTopic topic;
};
// Something that happened in a resident's life lately, as they would tell it.
struct AmbientNews
{
    double day = 0;
    std::string kind, text;
    std::string about;                  // The other one it concerns (an ID): never told it as news (a spouse, a thief).
};
// Something that happened in a town lately, as its people talk of it: a wedding, a death, a theft, a newcomer.
struct TownNews
{
    double day = 0;
    std::string kind;                   // A scene tag: marriage, death, apprentice, succession, theft, assault, arrest...
    std::string subject, other;         // Whom it concerns (IDs).
};
} // namespace ratw
