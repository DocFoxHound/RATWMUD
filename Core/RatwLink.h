#pragma once
// The game's messages (Docs/Design/27-browser-client.md): each WebSocket message (RatwWeb.h) is a kind, then a
// payload. Client to server: Command (a command's JSON), Ack (f64 revision, u8 missing: see RatwSections.h). Server to
// client: Event, Snapshot (u32 raw length, then the JSON zlib-compressed), Motion (u32 raw length, then the binary
// frame of RatwMotionCore.h zlib-compressed).
//
// Ping (client to server, 8 bytes of the client's choosing) is answered at once with Pong (the same 8 bytes, not
// compressed) by the host, not the game: the client's latency overlay (Docs/Design/31-responsiveness.md, Phase 1).
#include <cstdint>

namespace ratw::link
{
constexpr std::uint32_t MaxRaw = 16u << 20;
constexpr std::uint32_t MaxCommand = 65536;
constexpr std::uint32_t PingBytes = 8;
enum Kind : std::uint8_t
{
    Command = 1,
    Ack = 2,
    Ping = 3,
    Event = 10,
    Snapshot = 11,
    Motion = 12,
    Pong = 13,
};
} // namespace ratw::link
