#pragma once
// Building a wolf at creation (Docs/Design/49-characters-and-earned-gifts.md, Phase 4): the parts that read and write
// JSON, kept apart from RatwPractice.h so that headers everyone includes (RatwWorld.h) don't bring the JSON types in.
#include "RatwJsonDoc.h"
#include "RatwPractice.h"

#include <string>

namespace ratw::practice
{
// What the creator is sent (the lobby's `creation`): each attribute's grades, the budget and limits, the specialties,
// the presets and each Gift tier's cost, from Data/Progression/creation.json.
const json::Value& creationCatalog();
// A creator's `build` ({"grades": {"strength": "strong"...}, "specialty": "tracker"}) checked against creation.json:
// known attributes, grades and specialty, within the budget and limits, for a Gift tier's cost. False, with the reason.
bool checkBuild(const json::Value& build, const std::string& tier, Build& out, std::string& error);
} // namespace ratw::practice
