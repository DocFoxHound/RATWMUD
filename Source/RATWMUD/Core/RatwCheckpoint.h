#pragma once
// The checkpoint document, portable (Docs/Design/26-living-npcs.md, Phase 6): everything the game server saves, as
// one JSON document, the same one the Unreal runtime writes (RatwGameMode.cpp, BuildState and Load). The world's part
// (PersistedWorld) and the server's (who the characters are, conversations remembered, the social ledger, receipts)
// are read and written here; the accounts and the director's receipts travel as they are, for their own modules.
#include "RatwJsonDoc.h"
#include "RatwSocialCore.h"
#include "RatwWorld.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace ratw::checkpoint
{
struct ServerState
{
    json::Value accounts, director;               // Null when a save has none.
    std::uint64_t sequence = 1, revision = 0;
    std::map<std::string, Entity> characters;     // Every player character, online or not.
    std::map<std::string, std::string> companions;   // NPC -> the player it follows.
    MemoryStore memories;
    SocialLedger social;
    std::map<std::string, std::vector<std::string>> commandReceipts;
    std::map<std::string, std::map<std::string, std::string>> responseReceipts;
};

// The document for a save: `world` as World::save() gives it (its players are ignored: `characters` are saved), the
// NPCs to write (road folk left out), and the time the capture was taken.
json::Value encode(const PersistedWorld& world, const ServerState& server, const std::vector<Entity>& npcs, double time);

// Each NPC's running state for live.npc_state (a JSON array): where it is, how it is, its purse.
std::string npcStates(const PersistedWorld& world, const std::vector<Entity>& npcs);

// Reads a document. False, with the problem, if it can't be used (the checkpoint is then kept as it is and the server
// doesn't save over it); `world` is then ready for World::restore (which checks the rest), its players the characters.
bool decode(const json::Value& root, PersistedWorld& world, ServerState& server, std::string& problem);
} // namespace ratw::checkpoint
