#pragma once
// The Gift families (Docs/Design/43-gifts.md; Data/Gifts/families.json): which families exist, which a player may
// have, the creator's description of each, and each ability's name, kind and mana. What the abilities do is in
// RatwMagic.cpp.
#include "RatwJsonDoc.h"

#include <string>
#include <vector>

namespace ratw::gifts
{
// Loads the catalog once (RATW_DATA_DIR, the working directory or above it, or the source tree); false, with the
// reason, if it can't.
bool load(std::string* error = nullptr);
// A family in the catalog ("fire", "death_walker"...).
bool known(const std::string& family);
// A family a player may have: known and not NPC-only (Death Walkers are NPCs only).
bool playable(const std::string& family);
// The family's name ("Fire"), or the id if unknown.
std::string name(const std::string& family);
// What the creator shows (doc 43, "Choosing a Gift"): the tiers and the playable families, each with its best-for
// line, Tell, Cost, Limit and abilities per tier.
const json::Value& creatorCatalog();
// An ability, as the catalog has it: its family and tier, kind ("work", "instant", "gathered", "channelled",
// "reaction", "fightlong", "passive", "shape", "twoturn"), mana (up front, while held, a painted tile) and summary.
struct Ability
{
    std::string id, name, family, kind, summary;
    bool quickened = false, work = false;
    double mana = 0, perTurn = 0, perTile = 0;
};
const Ability* ability(const std::string& id);
// A family's abilities at a tier, in catalog order.
std::vector<const Ability*> abilities(const std::string& family, bool quickened);
// The Flamethrower is a Quickened Fire wolf's (doc 43): never a Gifted one's.
inline bool hasFlame(const std::string& gift, bool quickened) { return gift == "fire" && quickened; }
}
