#pragma once
// Motion frames, portable: Source/RATWMUD/Runtime/RatwMotion.h's frame (the poses an observer can see) and its binary
// form, byte for byte as Unreal's FArchive writes it (ratwmotion::Pack), so an Unreal client reads a standalone
// server's frames with the same ratwmotion::Unpack.
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
// Reads what pack() writes; a null value for anything malformed. (For tests; clients use ratwmotion::Unpack.)
json::Value unpack(const std::vector<std::uint8_t>& bytes);
} // namespace ratw::motion
