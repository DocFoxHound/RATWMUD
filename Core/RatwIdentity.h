#pragma once
// A wolf's identity (Docs/Design/77-central-accounts-and-hosted-worlds.md, 4.1): the rules a new wolf's name, age and
// Gift are checked by, in one place. The game's creation (RatwGame.cpp, character_create) and ratw_identity (central's
// check, Server/ratw_identity.cpp) both use them, with wire::readAppearance for the look, practice::checkBuild for the
// build and people::applyFields for the roleplay profile, so central never keeps a copy of the rules in Python.
#include "RatwJsonDoc.h"

#include <string>

namespace ratw::identity
{
// The name as a wolf keeps it: trimmed, with a capital first (names::capitalised); empty if it isn't a 2-32 character
// display name (accounts::validDisplayName).
std::string name(const std::string& given);
// A new wolf's name is at most this many characters (the user, 2026-11-14): the creator's limit, kept by the game's
// creation and by central's check. A wolf made before keeps its longer name (name() above still reads it, for tickets).
constexpr int NewNameMost = 24;
// The name as a new wolf takes it: name(), and empty if it is longer than NewNameMost characters.
std::string newName(const std::string& given);
// The starting age at `key`: a whole number from 6 to 99, or -1 if it isn't one.
double age(const json::Value& o, const std::string& key = "age");
// The Gift as creation takes it (doc 43): absent is Normal; otherwise {"tier": "normal" | "gifted" | "quickened",
// "family": a playable family}, with a family exactly when the tier isn't Normal. False if it is anything else.
bool gift(const json::Value* given, std::string& tier, std::string& family);
// The family alone, as central keeps it (doc 77, Q12 (a): the tier is chosen at each world's first arrival): absent,
// null or "" for none, else a playable family. False if it is anything else.
bool family(const json::Value* given, std::string& family);
} // namespace ratw::identity
