#pragma once

#include <map>
#include <string>
#include <vector>

namespace ratw
{
// Descriptive character-sheet choices only. Appearance never modifies combat,
// senses, movement, collisions, attributes, or earned social progression.
// One marking laid over the coat (Docs/Design/29-client-polish.md, phase 9): a shape from the known set, its colour
// ("#rrggbb") and how strongly it shows.
struct Marking
{
    std::string mask, color;
    double opacity = 1;
};
constexpr std::size_t MaxMarkings = 6;
extern const char* const MarkingMasks[];       // "socks", "stockings", "blaze", ... (null-terminated)
bool knownMarking(const std::string& mask);

// The painted wolf (Docs/Design/81-painted-wolves.md): one of three paintings (the appearance's `build`) coloured with
// the player's fur. Three fur colours for the whole coat (the painting's dark, mid and light fur), an eye colour, and
// for any of the eight fur zones three colours of its own. Every colour is an id in the palette
// (Data/Wolves/palette.json), never a colour itself: nothing outside the palette can be kept.
struct FurCoat
{
    std::string dark, mid, light;                // Fur palette ids.
};
struct Fur
{
    FurCoat coat;                                // The whole wolf's. All empty: no painted look chosen (an older wolf).
    std::string eyes;                            // An eye palette id.
    std::map<std::string, FurCoat> zones;        // A zone's own colours, by zone id; a zone not here wears the coat.
    bool chosen() const { return !coat.dark.empty() || !coat.mid.empty() || !coat.light.empty() || !eyes.empty() || !zones.empty(); }
};
bool furColour(const std::string& id);           // In the palette's fur colours.
bool eyeColour(const std::string& id);           // In the palette's eye colours.
bool furZone(const std::string& id);             // "back", "sides", "chest", "legs", "tail", "tail_tip", "head", "muzzle".
const char* furColourName(const std::string& id);    // "" for one not in the palette.
int furColourRgb(const std::string& id);             // 0xRRGGBB; -1 for one not in the palette.
bool validFur(const Fur& fur);                   // Chosen, complete, and every id in the palette.

struct Appearance
{
    std::string species = "timber", sex = "male", stature = "average", pattern = "saddle";
    int baseColor = 3, gradientColor = 1, markingColor = 5;
    double gradientAmount = .35, patternAmount = .55;
    // Phase 9 (all optional; empty means the palette above): free colours, eyes, build and layered markings.
    std::string coat, gradientTint, markingTint, eyes, build;
    std::vector<Marking> markings;
    // The painted look (doc 81), when chosen: see Fur below. `build` picks the painting: lean, average or heavy.
    Fur fur;
};
bool hexColour(const std::string& s);             // "#rrggbb", lower case.

// Stable palette indices are persisted, rather than arbitrary client colors.
// The default coat is stone, silver, and charcoal, independent of chat color.
constexpr int CoatColorCount = 8;
const char* coatColorName(int index);
const char* coatColorHex(int index);
bool validAppearance(const Appearance& appearance);

enum class LifeStage { Young, Adolescent, Adult, Old };
LifeStage lifeStage(int age);
const char* lifeStageName(LifeStage stage);
// Provisional fictional-world portrait proportions, not biological claims or
// stat bonuses. Invalid appearance returns zero; ages use the shared stages.
double shoulderHeightCm(const Appearance& appearance, int age);
} // namespace ratw
