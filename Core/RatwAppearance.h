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

// The layered wolf doll (Docs/Design/81-painted-wolves.md): one of six dolls (female/male x small/medium/large,
// driven by `sex` and `build`), coloured with the player's fur. A coat is either 5 fur colours (darkest, dark, mid,
// light, lightest — the new layered dolls) or 3 (dark, mid, light — legacy wolves), plus an eye colour, optional claw
// colour, optional per-layer overrides (any of the 45 fur/claw doll layers), and legacy zone overrides. Every colour
// is an id in the palette (Data/Wolves/palette.json), never a raw colour: nothing outside the palette can be kept.
struct FurCoat
{
    std::string dark, mid, light;                // 3-shade coat (legacy) or [0, 2, 4] summary of a 5-shade coat.
};
struct Fur
{
    FurCoat coat;                                // 3-shade coat (empty when no painted look chosen).
    std::vector<std::string> shades;             // 5-shade coat (darkest..lightest), when chosen; empty for 3-shade coats.
    std::string eyes;                            // An eye palette id.
    std::string claws;                           // Optional claw colour (a fur palette id).
    std::map<std::string, FurCoat> zones;        // Legacy 8-zone overrides (3 fur ids each).
    std::map<std::string, std::string> layers;   // Per-layer overrides (doll layer id -> fur palette id).
    bool chosen() const
    {
        return !coat.dark.empty() || !coat.mid.empty() || !coat.light.empty() || !shades.empty() || !eyes.empty() ||
               !claws.empty() || !zones.empty() || !layers.empty();
    }
};
bool furColour(const std::string& id);           // In the palette's fur colours.
bool eyeColour(const std::string& id);           // In the palette's eye colours.
bool furZone(const std::string& id);             // "back", "sides", "chest", "legs", "tail", "tail_tip", "head", "muzzle".
bool furLayer(const std::string& id);            // Any of the 45 customizable non-eye doll layers.
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
    // The painted look (doc 81), when chosen: see Fur below. `build` is the size it is painted as: small, medium or
    // large (the creator writes the stature that goes with it: short, average, tall). The names it held before the
    // sizes still read, and a wolf saved with one keeps it: lean (painted large), average (small), heavy (medium).
    Fur fur;
};

// A build an appearance may hold: a size, or one of the older names.
bool knownBuild(const std::string& build);
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
