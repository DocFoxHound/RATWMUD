#pragma once
// What this build is (Docs/Design/77-central-accounts-and-hosted-worlds.md, 5.1), made by CMake at configure time
// (Core/RatwBuildInfo.cpp.in): the protocol a client must speak, the client-data hash of what the game page bakes in
// (the terrain catalogue, the walking rules, the packed formats: tools/client_data_hash.py makes the same one), and the
// commit it was built from.
namespace ratw::build
{
// The game's protocol with its clients: up by one whenever a message changes so that an older client can't play.
constexpr int Protocol = 1;
const char* clientDataHash();
const char* id();
} // namespace ratw::build
