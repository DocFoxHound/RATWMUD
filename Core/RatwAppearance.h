#pragma once

#include <string>

namespace ratw
{
// Descriptive character-sheet choices only. Appearance never modifies combat,
// senses, movement, collisions, attributes, or earned social progression.
struct Appearance
{
    std::string species = "timber", sex = "male", stature = "average", pattern = "saddle";
    int baseColor = 3, gradientColor = 1, markingColor = 5;
    double gradientAmount = .35, patternAmount = .55;
};

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
