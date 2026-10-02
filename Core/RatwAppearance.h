#pragma once

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

struct Appearance
{
    std::string species = "timber", sex = "male", stature = "average", pattern = "saddle";
    int baseColor = 3, gradientColor = 1, markingColor = 5;
    double gradientAmount = .35, patternAmount = .55;
    // Phase 9 (all optional; empty means the palette above): free colours, eyes, build and layered markings.
    std::string coat, gradientTint, markingTint, eyes, build;
    std::vector<Marking> markings;
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
