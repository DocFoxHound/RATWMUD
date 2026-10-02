#pragma once
// Portraits players upload for their characters (Docs/Design/29-client-polish.md, phase 9).
//
// The browser decodes the picture, crops it square and scales it to 256×256; the server receives only raw RGBA pixels
// (in chunks, over the signed-in connection) and encodes the PNG itself, so no image parser runs on the server and
// nothing hidden in an uploaded file survives. A portrait is seen by its owner at once and by everyone else only once
// a Dungeon Master has approved it. Where the images live: a folder beside a world's save file, or game.artwork for a
// world from the database.
#include <memory>
#include <string>
#include <vector>

namespace ratw::art
{
constexpr int Side = 256;                                       // Every portrait is Side × Side.
constexpr std::size_t PixelBytes = std::size_t(Side) * Side * 4;
constexpr int UploadsPerDay = 10;                               // An account's uploads in a day.

struct Meta
{
    std::string id;                 // "art-<random>"
    std::string account, character;
    std::string status = "pending"; // "pending", "approved" or "rejected"
    std::string reason;             // Why it was rejected (or reported), for the owner and the DM.
    std::string sha;                // SHA-256 of the PNG, hex.
    double createdUnix = 0;
    bool reported = false;
};

// A PNG (8-bit RGBA, no metadata) of raw pixels; empty if zlib is missing or the size is wrong.
std::string encodePng(const std::vector<unsigned char>& rgba, int width, int height);

class Store
{
  public:
    virtual ~Store() = default;
    virtual bool put(const Meta& meta, const std::string& png, std::string& error) = 0;
    virtual bool image(const std::string& id, std::string& png) = 0;
    virtual bool update(const Meta& meta) = 0;              // Status, reason, reported.
    virtual std::vector<Meta> all() = 0;                    // Every portrait's record (not the images).
};
std::unique_ptr<Store> memoryStore();
// A folder (created if missing): <id>.png and index.json.
std::unique_ptr<Store> folderStore(const std::string& directory, std::string& error);
// game.artwork in the world's database (migration 0027).
std::unique_ptr<Store> databaseStore(const std::string& conninfo, const std::string& worldId, std::string& error);
} // namespace ratw::art
