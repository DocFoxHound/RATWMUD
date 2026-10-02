// Uploaded portraits (RatwArtwork.h; Docs/Design/29-client-polish.md, phase 9): receiving one in chunks, encoding it,
// who may see it, reports, and the Dungeon Master's decision.
#include "RatwGame.h"
#include "RatwSystemLibs.h"
#include "RatwWire.h"

#include <algorithm>
#include <ctime>

namespace ratw::game
{
namespace
{
constexpr int MaxParts = 8;
constexpr double UploadSeconds = 120;               // An upload not finished in this long is let go.

bool fromBase64(const std::string& in, std::vector<unsigned char>& out, std::size_t limit)
{
    std::uint32_t acc = 0;
    int bits = 0;
    for (char c : in)
    {
        if (c == '=')
            break;
        const int v = c >= 'A' && c <= 'Z' ? c - 'A' : c >= 'a' && c <= 'z' ? c - 'a' + 26 : c >= '0' && c <= '9' ? c - '0' + 52
                      : c == '+' ? 62 : c == '/' ? 63 : -1;
        if (v < 0)
            return false;
        acc = (acc << 6) | std::uint32_t(v);
        bits += 6;
        if (bits >= 8)
        {
            bits -= 8;
            out.push_back((acc >> bits) & 0xFF);
            if (out.size() > limit)
                return false;
        }
    }
    return true;
}

std::string toBase64(const std::string& in)
{
    static const char* digits = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    for (std::size_t i = 0; i < in.size(); i += 3)
    {
        const std::uint32_t n = (std::uint8_t(in[i]) << 16) | (i + 1 < in.size() ? std::uint8_t(in[i + 1]) << 8 : 0) |
                                (i + 2 < in.size() ? std::uint8_t(in[i + 2]) : 0);
        out += digits[(n >> 18) & 63];
        out += digits[(n >> 12) & 63];
        out += i + 1 < in.size() ? digits[(n >> 6) & 63] : '=';
        out += i + 2 < in.size() ? digits[n & 63] : '=';
    }
    return out;
}
} // namespace

std::string Game::artworkOwner(const Connection* c) const
{
    return !c->accountUsername.empty() ? c->accountUsername : !c->entityId.empty() ? "dev:" + c->entityId : std::string();
}

bool Game::ownsCharacter(const Connection* c, const std::string& character) const
{
    if (!c->accountUsername.empty())
        return accounts_.owns(c->accountUsername, character);
    return !c->entityId.empty() && c->entityId == character;     // A development identity owns only itself.
}

const art::Meta* Game::portraitOf(const std::string& character) const
{
    // The newest portrait of a character that hasn't been rejected.
    const art::Meta* best = nullptr;
    for (const auto& [id, m] : artworkMeta_)
        if (m.character == character && m.status != "rejected" && (!best || m.createdUnix > best->createdUnix))
            best = &m;
    return best;
}

std::string Game::visiblePortrait(const std::string& character, const std::string& viewer) const
{
    const auto* m = portraitOf(character);
    if (!m)
        return {};
    // Approved: everyone. Pending: only its owner (their own character).
    return m->status == "approved" || character == viewer ? m->id : std::string();
}

bool Game::artworkCommand(Connection* c, const json::Value& j, const std::string& type)
{
    if (type != "artwork_upload" && type != "artwork_get" && type != "artwork_report")
        return false;
    const auto reply = [&](json::Value e) {
        e.add("commandId", j.string("commandId"));
        send(c, e);
    };
    const auto refuse = [&](const std::string& why) {
        auto e = json::Value::object();
        e.add("type", "artworkError");
        e.add("text", why);
        reply(e);
        return true;
    };
    const auto owner = artworkOwner(c);
    if (owner.empty())
        return refuse("Sign in first.");
    if (!artwork_)
        return refuse("Portraits can't be uploaded on this server.");
    if (type == "artwork_get")
    {
        const auto id = j.string("id");
        const auto found = artworkMeta_.find(id);
        auto e = json::Value::object();
        e.add("type", "artwork");
        e.add("id", id);
        std::string png;
        const bool mine = found != artworkMeta_.end() && found->second.account == owner;
        if (found == artworkMeta_.end() || (found->second.status != "approved" && !mine) || found->second.status == "rejected" ||
            !artwork_->image(id, png))
            e.add("denied", true);
        else
        {
            e.add("status", found->second.status);
            e.add("png", toBase64(png));
        }
        reply(e);
        return true;
    }
    if (type == "artwork_report")
    {
        const auto found = artworkMeta_.find(j.string("id"));
        if (found == artworkMeta_.end() || found->second.status != "approved")
            return refuse("There is no such portrait to report.");
        auto& m = found->second;
        m.status = "pending";               // Back to the DM, hidden from others until they decide again.
        m.reported = true;
        m.reason = mind::left("Reported: " + mind::trim(j.string("reason")), 400);
        artwork_->update(m);
        auto e = json::Value::object();
        e.add("type", "artworkReported");
        e.add("id", m.id);
        reply(e);
        return true;
    }
    // artwork_upload: {characterId, uploadId, part, parts, data}: raw 256×256 RGBA pixels in base64, in up to eight parts.
    const auto character = j.string("characterId");
    if (!ownsCharacter(c, character))
        return refuse("That character isn't yours.");
    const double part = wire::strictNumber(j, "part", -1), parts = wire::strictNumber(j, "parts", -1);
    const auto uploadId = j.string("uploadId");
    if (uploadId.empty() || uploadId.size() > 64 || parts < 1 || parts > MaxParts || part < 0 || part >= parts ||
        part != std::floor(part) || parts != std::floor(parts))
        return refuse("That upload is malformed.");
    const double now = world_.time();
    for (auto it = uploads_.begin(); it != uploads_.end();)
        it = now - it->second.startedAt > UploadSeconds ? uploads_.erase(it) : std::next(it);
    auto& u = uploads_[c->id];
    if (u.id != uploadId || u.character != character || part == 0)
    {
        if (part != 0)
            return refuse("That upload has to start from its first part.");
        // An account's uploads in a day are limited.
        int today = 0;
        const double unix = double(std::time(nullptr));
        for (const auto& [id, m] : artworkMeta_)
            today += m.account == owner && unix - m.createdUnix < 86400;
        if (today >= art::UploadsPerDay)
        {
            uploads_.erase(c->id);
            return refuse("That's enough portraits for one day. Try again tomorrow.");
        }
        u = {uploadId, character, {}, int(parts), 0, now};
    }
    if (int(part) != u.received || int(parts) != u.parts)
        return refuse("That upload's parts came out of order.");
    if (!fromBase64(j.string("data"), u.data, art::PixelBytes))
    {
        uploads_.erase(c->id);
        return refuse("That upload is too large or malformed.");
    }
    ++u.received;
    if (u.received < u.parts)
    {
        auto e = json::Value::object();
        e.add("type", "artworkPart");
        e.add("uploadId", uploadId);
        e.add("received", u.received);
        reply(e);
        return true;
    }
    if (u.data.size() != art::PixelBytes)
    {
        uploads_.erase(c->id);
        return refuse("A portrait is 256 by 256 pixels.");
    }
    // Encoded here, from the pixels alone: nothing from the uploaded file survives.
    const auto png = art::encodePng(u.data, art::Side, art::Side);
    uploads_.erase(c->id);
    std::uint8_t digest[32];
    if (png.empty() || !sys::sha256(png, digest))
        return refuse("The portrait couldn't be encoded on this server.");
    art::Meta m;
    m.id = "art-" + guid().substr(0, 24);
    m.account = owner;
    m.character = character;
    m.sha = sys::hex(digest, 32);
    m.createdUnix = double(std::time(nullptr));
    std::string problem;
    if (!artwork_->put(m, png, problem))
        return refuse("The portrait couldn't be stored: " + problem);
    // One portrait at a time: an older one still waiting for the DM gives way to the new one.
    for (auto& [id, other] : artworkMeta_)
        if (other.character == character && other.status == "pending")
        {
            other.status = "rejected";
            other.reason = "Replaced by a newer upload.";
            artwork_->update(other);
        }
    artworkMeta_[m.id] = m;
    note("info", "RATW_ARTWORK uploaded " + m.id + " for " + character);
    auto e = json::Value::object();
    e.add("type", "artworkUploaded");
    e.add("id", m.id);
    e.add("characterId", character);
    e.add("status", m.status);
    e.add("text", "Your portrait is saved. You see it now; others will once a Dungeon Master has approved it.");
    reply(e);
    if (c->entityId.empty())
        lobby(c, true, "Portrait saved. Others will see it once it has been approved.");
    return true;
}

Result Game::reviewArtwork(const std::string& id, const std::string& decision, const std::string& reason)
{
    const auto found = artworkMeta_.find(id);
    if (found == artworkMeta_.end())
        return {false, "No such portrait.", {}};
    if (decision != "approve" && decision != "reject")
        return {false, "A decision is approve or reject.", {}};
    auto& m = found->second;
    m.status = decision == "approve" ? "approved" : "rejected";
    m.reason = mind::left(reason, 400);
    m.reported = false;
    if (!artwork_ || !artwork_->update(m))
        return {false, "The decision couldn't be stored.", {}};
    // The owner hears, if they're here.
    for (auto* c : clients_)
        if (artworkOwner(c) == m.account)
            system(c, decision == "approve" ? "Your portrait has been approved; others can see it now."
                                            : "Your portrait was not approved" + (reason.empty() ? std::string(".") : ": " + reason));
    return {true, "Portrait " + m.status + ".", {}};
}
} // namespace ratw::game
