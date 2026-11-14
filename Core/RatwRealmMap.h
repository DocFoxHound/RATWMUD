#pragma once
// The realm's map, whole (the user, 2026-11-14: "When clicking on the minimap and expanding the world map, I think we
// should just have the full worldmap and not just where a player has been/seen"): every outdoor cell's ground at the
// world's level, sampled down to at most `MaxSide` samples a side, the names of its regions, towns and districts, and
// its buildings (their footprints' middles, names and what they are, for the maps' icons).
//
// Made once per server from the world as built (a streamed world's cells read on a thread of its own: no tick waits on
// it), kept as the one JSON event it is sent as, and sent to a client that asks (`realmMap`): never in a snapshot. It
// is the map as authored: no wolves, no doors' state, nothing the world's own rules hide (doc 80's sneaking is about
// wolves; a building's lock is the door's, and the door is not on it). The minimap's memory of what a wolf has seen is
// unchanged: the snapshot's worldMap still carries it, for the minimap and the World Map's close detail.
#include <functional>
#include <string>
#include <vector>

namespace ratw::realm
{
constexpr int MaxSide = 1024;

// An outdoor cell: where it lies, what it is called, and its rows of glyphs if they are at hand (else `fetch` brings
// them, from the cell's file).
struct CellIn
{
    std::string id, name, region;
    int width = 0, height = 0;
    double x = 0, y = 0;
    std::vector<std::string> rows;
};
// A building: a door from an outdoor cell into an interior. `words` are what the place is called and what its workers
// do there (their work labels), for what it is (kindOf).
struct DoorIn
{
    std::string cell, interior, name, town, words;
    double x = 0, y = 0;                // In the outdoor cell, tiles.
    int innerWidth = 0, innerHeight = 0;
};
struct Input
{
    std::vector<CellIn> cells;
    std::vector<DoorIn> doors;
    std::string build;                  // Which build the map is of (the client keeps one per world).
};
// A cell's file, whole, for its rows; false if it can't be had.
using Fetch = std::function<bool(const std::string& cellId, std::string& body)>;

// The rows of a cell's file: the lines after "grid:".
std::vector<std::string> gridRows(const std::string& body);
// What a building is, for its icon, from its name and its workers' words: "inn", "bakery", "butcher", "smith",
// "tailor", "apothecary", "church", "watch", "bank", "stables", "wharf", "store", "market", "brewery", "workshop",
// "farm", "hall", "house", or "" (nothing to say).
std::string kindOf(const std::string& words);
// The middle of a building's footprint (world tiles of its cell), from its door: the block of walls and roofs the door
// opens into, as far as the interior's size allows; else a step in from the door by half the interior's depth.
void footprintMiddle(const std::vector<std::string>& rows, double doorX, double doorY, int innerWidth, int innerHeight,
                     double& outX, double& outY);
// The map as the event a client is sent: {"type":"realmMap", ...}. `stop` is asked between cells: true gives up ("").
std::string build(Input input, const Fetch& fetch, const std::function<bool()>& stop = {}, int maxSide = MaxSide);
} // namespace ratw::realm
