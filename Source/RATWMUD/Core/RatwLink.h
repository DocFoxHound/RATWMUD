#pragma once
// The standalone server's wire (Docs/Design/26-living-npcs.md, Phase 6), shared by the server (Server/ratw_server.cpp)
// and the Unreal client's remote mode (Runtime/RatwRemoteLink.h). TCP, one frame after another:
//
//   u32 length (little-endian; of what follows), u8 kind, payload
//
// Client to server: Command (a command's JSON, as the Unreal client sends ServerCommand), Ack (f64 revision, u8 missing:
// see RatwSections.h). Server to client: Event, Snapshot (u32 raw length, then the JSON zlib-compressed, as Unreal's
// envelope), Motion (u32 raw length, then the binary frame of RatwMotionCore.h zlib-compressed).
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace ratw::link
{
constexpr std::uint32_t MaxFrame = 8u << 20;
constexpr std::uint32_t MaxRaw = 16u << 20;
constexpr std::uint32_t MaxCommand = 65536;
enum Kind : std::uint8_t
{
    Command = 1,
    Ack = 2,
    Event = 10,
    Snapshot = 11,
    Motion = 12,
};

inline void appendFrame(std::string& out, Kind kind, const void* payload, std::size_t length)
{
    const std::uint32_t size = std::uint32_t(length + 1);
    unsigned char head[5] = {std::uint8_t(size), std::uint8_t(size >> 8), std::uint8_t(size >> 16), std::uint8_t(size >> 24), kind};
    out.append(reinterpret_cast<const char*>(head), 5);
    out.append(static_cast<const char*>(payload), length);
}

// Takes one whole frame off the front of `buffer`; false if there isn't one yet. `bad` is set for a frame that is too
// large or empty (the connection should then be closed).
inline bool takeFrame(std::string& buffer, Kind& kind, std::string& payload, bool& bad)
{
    bad = false;
    if (buffer.size() < 4)
        return false;
    const auto* b = reinterpret_cast<const unsigned char*>(buffer.data());
    const std::uint32_t size = std::uint32_t(b[0]) | std::uint32_t(b[1]) << 8 | std::uint32_t(b[2]) << 16 | std::uint32_t(b[3]) << 24;
    if (size == 0 || size > MaxFrame)
    {
        bad = true;
        return false;
    }
    if (buffer.size() < 4 + std::size_t(size))
        return false;
    kind = Kind(b[4]);
    payload.assign(buffer, 5, size - 1);
    buffer.erase(0, 4 + std::size_t(size));
    return true;
}
} // namespace ratw::link
