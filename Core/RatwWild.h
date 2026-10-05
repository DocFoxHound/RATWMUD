#pragma once
// The wild (Docs/Design/41-hunting-and-foraging.md): the animals a hunt can find and what a wolf can forage, from
// Data/Wild. Data only; the hunt and the foraging are World's (RatwHunt.cpp).
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace ratw::wild
{
struct Species
{
    std::string id, name, glyph, color, temper;     // temper: "flee", "cornered" or "fierce".
    std::map<std::string, double> habitats;         // Ground kind -> weight.
    double rarity = 1, health = 10, dex = 50, strength = 30, alert = 8, sight = 1, hearing = 1, smell = 1, bite = 0;
    std::vector<std::pair<std::string, int>> yield;
};

struct Population
{
    double expected = 3, capacity = 6, recoveryDays = 2, perPlayers = 50, most = 2, arrivalSeconds = 30;
    int atOnce = 6;
    double wildFrom = .5;                           // The share of an arena that must be natural ground to hunt in.
    int nearestStart = 10;                          // Animals begin at least this many tiles from a hunter.
};

struct ForageGood
{
    std::string item;
    double weight = 1;
    int count = 1;
    std::vector<int> seasons;                       // Empty: all year.
};

struct ForageGround
{
    std::string id, tiles, name;
    std::vector<ForageGood> goods;
};

struct Forage
{
    int patchTiles = 8, picks = 4;
    double regrowHours = 2, seconds = 4;
    std::vector<ForageGround> ground;
};

// Loaded once (from RATW_DATA_DIR, the working directory or above it, or the source tree); false, with the reason, if
// they can't be read.
bool load(std::string* error = nullptr);
const std::vector<Species>& species();
const Species* speciesById(const std::string& id);
const Population& population();
const Forage& forage();
// The kind of ground a map tile code is ("forest", "grass"...), or "" for none (walls, floors, roads).
const std::string& groundOf(char tile);
} // namespace ratw::wild
