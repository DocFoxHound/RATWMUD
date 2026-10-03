#pragma once
// The game's messages (Docs/Design/27-browser-client.md): each WebSocket message (RatwWeb.h) is a kind, then a
// payload. Client to server: Command (a command's JSON), Ack (f64 revision, u8 missing: see RatwSections.h). Server to
// client: Event, Snapshot (u32 raw length, then the JSON zlib-compressed), Motion (u32 raw length, then the binary
// frame of RatwMotionCore.h zlib-compressed).
//
// A server message's raw length may carry Stored (its top bit): the payload is then the raw bytes themselves, not
// compressed. Small messages (most motion frames) go that way, and the rest at zlib's fastest level: compression
// was an eighth of the server's work (Docs/Design/31-responsiveness.md, Phase 4).
//
// Pose (client to server, PoseBytes): a client walking its own wolf says where it is (doc 31, Phase 3), twenty times a
// second, so in a few fixed bytes rather than a JSON command: u32 seq, f32 x, f32 y, f32 facing, i8 heading x, i8
// heading y (each -1, 0 or 1).
//
// Ping (client to server, 8 bytes of the client's choosing) is answered at once with Pong (the same 8 bytes, not
// compressed) by the host, not the game: the client's latency overlay (Docs/Design/31-responsiveness.md, Phase 1).
#include "RatwSystemLibs.h"

#include <cstdint>
#include <string>
#include <vector>

namespace ratw::link
{
constexpr std::uint32_t MaxRaw = 16u << 20;
constexpr std::uint32_t MaxCommand = 65536;
constexpr std::uint32_t PingBytes = 8, PoseBytes = 18;
enum Kind : std::uint8_t
{
    Command = 1,
    Ack = 2,
    Ping = 3,
    Pose = 4,
    Event = 10,
    Snapshot = 11,
    Motion = 12,
    Pong = 13,
};
constexpr std::uint32_t Stored = 0x80000000u;
constexpr std::size_t StoreBelow = 1024;          // Bytes: smaller messages aren't worth compressing.
constexpr int WireLevel = 1;

// A server message as it goes on the wire: the kind, the raw length (with Stored), then the payload. False if zlib
// failed.
inline bool encode(Kind kind, const std::string& raw, std::string& out)
{
    const bool store = raw.size() < StoreBelow;
    std::vector<std::uint8_t> packed;
    if (!store && !sys::compress(reinterpret_cast<const std::uint8_t*>(raw.data()), raw.size(), packed, WireLevel))
        return false;
    const std::uint32_t length = std::uint32_t(raw.size()) | (store ? Stored : 0);
    out.assign(1, char(kind));
    for (int i = 0; i < 4; ++i)
        out += char(length >> (8 * i));
    if (store)
        out += raw;
    else
        out.append(reinterpret_cast<const char*>(packed.data()), packed.size());
    return true;
}
} // namespace ratw::link
