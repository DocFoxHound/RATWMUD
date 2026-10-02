#pragma once
// Motion frames: the poses an observer can see, twenty times a second, and their binary form (the layout Unreal's
// FArchive wrote, with the observer's own movement after the poses: doc 31, Phase 3). The browser client reads them with Client/src/net/motion.ts.
#include "RatwJsonDoc.h"
#include "RatwWorld.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ratw::motion
{
json::Value frame(const World& world, const std::string& observer);
// The frame (with the host's stamps: motionSession, cellGeneration, revision) in binary.
std::vector<std::uint8_t> pack(const json::Value& frame);
// Reads what pack() writes; a null value for anything malformed. (For tests; the browser client has its own, Client/src/net/motion.ts.)
json::Value unpack(const std::vector<std::uint8_t>& bytes);
} // namespace ratw::motion
