#pragma once
// The game's messages (Docs/Design/27-browser-client.md): each WebSocket message (RatwWeb.h) is a kind, then a
// payload. Client to server: Command (a command's JSON), Ack (f64 revision, u8 missing: see RatwSections.h). Server to
// client: Event, Snapshot (u32 raw length, then the JSON zlib-compressed), Motion (u32 raw length, then the binary
// frame of RatwMotionCore.h zlib-compressed).
#include <cstdint>

namespace ratw::link
{
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
} // namespace ratw::link
