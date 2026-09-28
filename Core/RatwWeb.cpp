#include "RatwWeb.h"

#include "RatwSystemLibs.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace ratw::web
{
namespace
{
std::string lower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return s;
}
std::string trim(const std::string& s)
{
    const auto a = s.find_first_not_of(" \t");
    if (a == std::string::npos)
        return {};
    return s.substr(a, s.find_last_not_of(" \t") - a + 1);
}
// Whether a comma-separated header value lists `token` (case-insensitively).
bool lists(const std::string& value, const std::string& token)
{
    std::size_t start = 0;
    while (start <= value.size())
    {
        const auto end = std::min(value.find(',', start), value.size());
        if (lower(trim(value.substr(start, end - start))) == token)
            return true;
        start = end + 1;
    }
    return false;
}
const char* reason(int status)
{
    switch (status)
    {
    case 200: return "OK";
    case 400: return "Bad Request";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 426: return "Upgrade Required";
    default: return "Error";
    }
}
} // namespace

std::string Request::header(const std::string& name) const
{
    const auto found = headers.find(lower(name));
    return found == headers.end() ? std::string() : found->second;
}

int takeRequest(std::string& buffer, Request& out)
{
    const auto end = buffer.find("\r\n\r\n");
    if (end == std::string::npos)
        return buffer.size() > MaxRequestHead ? -1 : 0;
    if (end + 4 > MaxRequestHead)
        return -1;
    const std::string head = buffer.substr(0, end);
    buffer.erase(0, end + 4);
    out = Request{};
    std::size_t lineEnd = head.find("\r\n");
    const std::string line = head.substr(0, lineEnd);
    const auto a = line.find(' '), b = line.rfind(' ');
    if (a == std::string::npos || b == a)
        return -1;
    out.method = line.substr(0, a);
    std::string target = line.substr(a + 1, b - a - 1);
    out.version = line.substr(b + 1);
    if (out.version != "HTTP/1.1" && out.version != "HTTP/1.0")
        return -1;
    if (target.empty() || target[0] != '/')
        return -1;
    if (const auto q = target.find('?'); q != std::string::npos)
    {
        out.query = target.substr(q + 1);
        target.resize(q);
    }
    out.path = target;
    for (const char c : out.method + out.path)
        if (static_cast<unsigned char>(c) < 0x21 || c == 0x7f)
            return -1;
    while (lineEnd != std::string::npos)
    {
        const auto start = lineEnd + 2;
        lineEnd = head.find("\r\n", start);
        const std::string field = head.substr(start, lineEnd == std::string::npos ? std::string::npos : lineEnd - start);
        const auto colon = field.find(':');
        if (colon == std::string::npos || colon == 0)
            return -1;
        const std::string name = lower(field.substr(0, colon)), value = trim(field.substr(colon + 1));
        for (const char c : name)
            if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '-'))
                return -1;
        auto& slot = out.headers[name];
        slot = slot.empty() ? value : slot + ", " + value;
    }
    return 1;
}

bool upgradeRequested(const Request& r)
{
    if (r.method != "GET" || r.version != "HTTP/1.1" || !lists(r.header("upgrade"), "websocket") ||
        !lists(r.header("connection"), "upgrade") || r.header("sec-websocket-version") != "13")
        return false;
    // The key is 16 random bytes in base64: 22 characters and "==".
    const std::string key = r.header("sec-websocket-key");
    if (key.size() != 24 || key.compare(22, 2, "==") != 0)
        return false;
    return std::all_of(key.begin(), key.begin() + 22,
                       [](unsigned char c) { return std::isalnum(c) || c == '+' || c == '/'; });
}

bool originAllowed(const Request& r)
{
    const auto found = r.headers.find("origin");
    if (found == r.headers.end())
        return true;
    const std::string origin = lower(found->second), host = lower(r.header("host"));
    if (host.empty())
        return false;
    for (const char* scheme : {"http://", "https://"})
        if (origin == scheme + host)
            return true;
    return false;
}

std::string base64(const std::uint8_t* data, std::size_t length)
{
    static const char* digits = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((length + 2) / 3 * 4);
    for (std::size_t i = 0; i < length; i += 3)
    {
        const std::uint32_t n = std::uint32_t(data[i]) << 16 | (i + 1 < length ? std::uint32_t(data[i + 1]) << 8 : 0) |
                                (i + 2 < length ? std::uint32_t(data[i + 2]) : 0);
        out += digits[n >> 18 & 63];
        out += digits[n >> 12 & 63];
        out += i + 1 < length ? digits[n >> 6 & 63] : '=';
        out += i + 2 < length ? digits[n & 63] : '=';
    }
    return out;
}

std::string acceptKey(const std::string& clientKey)
{
    std::uint8_t digest[20];
    if (!sys::sha1(clientKey + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11", digest))
        return {};
    return base64(digest, sizeof digest);
}

std::string handshake(const std::string& accept)
{
    return "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: " + accept +
           "\r\n\r\n";
}

std::string response(int status, const std::string& type, const std::string& body, const std::string& extra)
{
    return "HTTP/1.1 " + std::to_string(status) + " " + reason(status) + "\r\nContent-Type: " + type +
           "\r\nContent-Length: " + std::to_string(body.size()) +
           "\r\nX-Content-Type-Options: nosniff\r\nReferrer-Policy: no-referrer\r\nConnection: close\r\n" + extra + "\r\n" + body;
}

void appendFrame(std::string& out, Opcode op, const void* data, std::size_t length)
{
    out += char(0x80 | op);
    if (length < 126)
        out += char(length);
    else if (length <= 0xffff)
    {
        out += char(126);
        out += char(length >> 8);
        out += char(length);
    }
    else
    {
        out += char(127);
        for (int shift = 56; shift >= 0; shift -= 8)
            out += char(std::uint64_t(length) >> shift);
    }
    out.append(static_cast<const char*>(data), length);
}

int takeMessage(std::string& buffer, Reader& reader, Opcode& op, std::string& payload, std::size_t maxMessage)
{
    for (;;)
    {
        if (buffer.size() < 2)
            return 0;
        const auto* b = reinterpret_cast<const unsigned char*>(buffer.data());
        const bool fin = b[0] & 0x80;
        const std::uint8_t opcode = b[0] & 0x0f;
        if (b[0] & 0x70)
            return -1;                                   // No extensions were agreed.
        if (!(b[1] & 0x80))
            return -1;                                   // A client's frames are always masked.
        std::uint64_t length = b[1] & 0x7f;
        std::size_t at = 2;
        if (length == 126)
        {
            if (buffer.size() < 4)
                return 0;
            length = std::uint64_t(b[2]) << 8 | b[3];
            at = 4;
        }
        else if (length == 127)
        {
            if (buffer.size() < 10)
                return 0;
            length = 0;
            for (int i = 0; i < 8; ++i)
                length = length << 8 | b[2 + i];
            at = 10;
        }
        const bool control = opcode & 0x08;
        if (control && (!fin || length > 125))
            return -1;
        if (!control && opcode != Continuation && opcode != Text && opcode != Binary)
            return -1;
        if (control && opcode != Close && opcode != Ping && opcode != Pong)
            return -1;
        if (length > maxMessage || (!control && reader.partial.size() + length > maxMessage))
            return -1;
        if (buffer.size() < at + 4 + length)
            return 0;
        const unsigned char* mask = b + at;
        std::string data(length, '\0');
        for (std::uint64_t i = 0; i < length; ++i)
            data[i] = char(b[at + 4 + i] ^ mask[i % 4]);
        buffer.erase(0, at + 4 + std::size_t(length));
        if (control)
        {
            op = Opcode(opcode);
            payload = std::move(data);
            return 1;
        }
        if (opcode == Continuation)
        {
            if (!reader.fragmented)
                return -1;
            reader.partial += data;
        }
        else
        {
            if (reader.fragmented)
                return -1;                               // A new message before the last one finished.
            reader.partial = std::move(data);
            reader.partialOp = opcode;
        }
        reader.fragmented = !fin;
        if (fin)
        {
            op = Opcode(reader.partialOp);
            payload = std::move(reader.partial);
            reader.partial.clear();
            return 1;
        }
    }
}

bool filePath(const std::string& requestPath, std::string& relative)
{
    if (requestPath.empty() || requestPath[0] != '/' || requestPath.size() > 256)
        return false;
    relative = requestPath == "/" ? "index.html" : requestPath.substr(1);
    if (relative.empty() || relative.back() == '/')
        return false;
    std::size_t start = 0;
    while (start < relative.size())
    {
        const auto end = std::min(relative.find('/', start), relative.size());
        const std::string name = relative.substr(start, end - start);
        if (name.empty() || name[0] == '.')
            return false;                                // No "..", no hidden files, no empty names.
        for (const char c : name)
            if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '_' || c == '-'))
                return false;
        start = end + 1;
    }
    return true;
}

std::string contentType(const std::string& relative)
{
    const auto dot = relative.rfind('.');
    const std::string ext = dot == std::string::npos ? std::string() : lower(relative.substr(dot + 1));
    if (ext == "html") return "text/html; charset=utf-8";
    if (ext == "js" || ext == "mjs") return "text/javascript; charset=utf-8";
    if (ext == "css") return "text/css; charset=utf-8";
    if (ext == "json") return "application/json";
    if (ext == "png") return "image/png";
    if (ext == "svg") return "image/svg+xml";
    if (ext == "ico") return "image/x-icon";
    if (ext == "ttf") return "font/ttf";
    if (ext == "woff2") return "font/woff2";
    if (ext == "txt") return "text/plain; charset=utf-8";
    if (ext == "map") return "application/json";
    return "application/octet-stream";
}
} // namespace ratw::web
