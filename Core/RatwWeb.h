#pragma once
// The server's web side (Docs/Design/27-browser-client.md): enough HTTP/1.1 to serve the browser client's files, and
// WebSockets (RFC 6455) for the game itself. No sockets here: the server reads and writes the bytes; this parses and
// builds them, so every rule can be tested on its own.
//
// Each WebSocket message is one frame of the game's wire (RatwLink.h), without its length: a kind byte, then the
// payload. Client to server: Command, Ack. Server to client: Event, Snapshot, Motion (zlib-compressed as on the link).
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>

namespace ratw::web
{
constexpr std::size_t MaxRequestHead = 16384;          // An HTTP request's head, request line and headers together.
constexpr const char* GamePath = "/ws";

struct Request
{
    std::string method, path, query, version;
    std::map<std::string, std::string> headers;        // Names in lower case; a repeated header's values joined by ", ".
    std::string header(const std::string& name) const;
};

// Takes one request head (up to its blank line) off the front of `buffer`: 1 when one was taken, 0 when more bytes are
// needed, -1 when it is malformed or too large (answer 400 and close).
int takeRequest(std::string& buffer, Request& out);

// Whether the request asks to become a WebSocket (a GET with Upgrade: websocket, Connection: upgrade, version 13 and
// a well-formed key).
bool upgradeRequested(const Request& r);
// A page on another site must not open the game in a player's browser: a request with an Origin is accepted only when
// the Origin names the same host and port the request was sent to. A request without one (a program, not a page) is
// accepted.
bool originAllowed(const Request& r);
std::string base64(const std::uint8_t* data, std::size_t length);
// The Sec-WebSocket-Accept value for a client's key. Empty if SHA-1 is unavailable.
std::string acceptKey(const std::string& clientKey);
std::string handshake(const std::string& accept);
// A whole response with a body (Connection: close). `extra` is added to the headers as it is (each line ending "\r\n").
std::string response(int status, const std::string& type, const std::string& body, const std::string& extra = {});

// Server to client: one unmasked frame holding a whole message.
enum Opcode : std::uint8_t
{
    Continuation = 0,
    Text = 1,
    Binary = 2,
    Close = 8,
    Ping = 9,
    Pong = 10,
};
void appendFrame(std::string& out, Opcode op, const void* data, std::size_t length);

// Client to server: frames are masked and may be fragmented. The reader reassembles messages; control frames (close,
// ping, pong) come out as they arrive, even between fragments.
struct Reader
{
    std::string partial;
    std::uint8_t partialOp = 0;
    bool fragmented = false;
};
// 1 when a message was taken (its opcode and payload), 0 when more bytes are needed, -1 for a protocol violation or a
// message larger than `maxMessage` (close the connection).
int takeMessage(std::string& buffer, Reader& reader, Opcode& op, std::string& payload, std::size_t maxMessage);

// A request path as a file under the client's folder: "/" is "index.html". False for anything that could leave the
// folder or name something odd (only letters, digits, '.', '_', '-' and '/' between names; no hidden names).
bool filePath(const std::string& requestPath, std::string& relative);
std::string contentType(const std::string& relative);
} // namespace ratw::web
