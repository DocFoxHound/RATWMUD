#include "RatwAppearance.h"

#include <cmath>

namespace ratw
{
namespace
{
constexpr const char* CoatNames[] = {"ivory", "silver", "ash", "stone", "sable", "charcoal", "rust", "sand"};
constexpr const char* CoatColors[] = {"#E1D9C6", "#ADB3B2", "#777D7B", "#8E8271", "#65513F", "#303534", "#A26843", "#BEAA84"};
bool paletteIndex(int index) { return index >= 0 && index < CoatColorCount; }
} // namespace

const char* coatColorName(int index) { return paletteIndex(index) ? CoatNames[index] : ""; }
const char* coatColorHex(int index) { return paletteIndex(index) ? CoatColors[index] : ""; }

const char* const MarkingMasks[] = {"socks", "stockings", "blaze", "mask", "cape", "bib", "belly", "tail_tip", "ear_tips",
                                    "freckles", "brindle", "merle", "scar", "eye_patches", "saddle", nullptr};

bool knownMarking(const std::string& mask)
{
    for (auto* m = MarkingMasks; *m; ++m)
        if (mask == *m)
            return true;
    return false;
}

bool hexColour(const std::string& s)
{
    if (s.size() != 7 || s[0] != '#')
        return false;
    for (std::size_t i = 1; i < 7; ++i)
        if (!((s[i] >= '0' && s[i] <= '9') || (s[i] >= 'a' && s[i] <= 'f')))
            return false;
    return true;
}

namespace
{
struct PaletteEntry
{
    const char* id;
    const char* name;
    int rgb;
};
constexpr PaletteEntry FurPalette[] = {
#define RATW_FUR(id, name, rgb) {id, name, rgb},
#define RATW_EYE(id, name, rgb)
#define RATW_FUR_ZONE(id, name)
#include "RatwFurPalette.inc"
#undef RATW_FUR
#undef RATW_EYE
#undef RATW_FUR_ZONE
};
constexpr PaletteEntry EyePalette[] = {
#define RATW_FUR(id, name, rgb)
#define RATW_EYE(id, name, rgb) {id, name, rgb},
#define RATW_FUR_ZONE(id, name)
#include "RatwFurPalette.inc"
#undef RATW_FUR
#undef RATW_EYE
#undef RATW_FUR_ZONE
};
constexpr const char* FurZoneIds[] = {
#define RATW_FUR(id, name, rgb)
#define RATW_EYE(id, name, rgb)
#define RATW_FUR_ZONE(id, name) id,
#include "RatwFurPalette.inc"
#undef RATW_FUR
#undef RATW_EYE
#undef RATW_FUR_ZONE
};
const PaletteEntry* furEntry(const std::string& id)
{
    for (const auto& entry : FurPalette)
        if (id == entry.id)
            return &entry;
    return nullptr;
}
bool furCoat(const FurCoat& coat) { return furColour(coat.dark) && furColour(coat.mid) && furColour(coat.light); }
} // namespace

bool furColour(const std::string& id) { return furEntry(id) != nullptr; }
const char* furColourName(const std::string& id)
{
    const auto* entry = furEntry(id);
    return entry ? entry->name : "";
}
int furColourRgb(const std::string& id)
{
    const auto* entry = furEntry(id);
    return entry ? entry->rgb : -1;
}

bool eyeColour(const std::string& id)
{
    for (const auto& entry : EyePalette)
        if (id == entry.id)
            return true;
    return false;
}

bool furZone(const std::string& id)
{
    for (const auto* zone : FurZoneIds)
        if (id == zone)
            return true;
    return false;
}

bool validFur(const Fur& fur)
{
    if (!furCoat(fur.coat) || !eyeColour(fur.eyes))
        return false;
    for (const auto& [zone, coat] : fur.zones)
        if (!furZone(zone) || !furCoat(coat))
            return false;
    return true;
}

bool knownBuild(const std::string& build)
{
    for (const auto* known : {"small", "medium", "large", "lean", "average", "heavy"})
        if (build == known)
            return true;
    return false;
}

bool validAppearance(const Appearance& appearance)
{
    if (appearance.fur.chosen() && !validFur(appearance.fur))
        return false;
    for (const auto* colour : {&appearance.coat, &appearance.gradientTint, &appearance.markingTint, &appearance.eyes})
        if (!colour->empty() && !hexColour(*colour))
            return false;
    if (!appearance.build.empty() && !knownBuild(appearance.build))
        return false;
    if (appearance.markings.size() > MaxMarkings)
        return false;
    for (const auto& m : appearance.markings)
        if (!knownMarking(m.mask) || !hexColour(m.color) || !std::isfinite(m.opacity) || m.opacity < 0 || m.opacity > 1)
            return false;
    return (appearance.species == "timber" || appearance.species == "maned" || appearance.species == "arctic" ||
            appearance.species == "red" || appearance.species == "ethiopian") &&
           (appearance.sex == "female" || appearance.sex == "male") &&
           (appearance.stature == "short" || appearance.stature == "average" || appearance.stature == "tall") &&
           (appearance.pattern == "solid" || appearance.pattern == "saddle" || appearance.pattern == "mantle" ||
            appearance.pattern == "piebald") &&
           paletteIndex(appearance.baseColor) && paletteIndex(appearance.gradientColor) && paletteIndex(appearance.markingColor) &&
           std::isfinite(appearance.gradientAmount) && appearance.gradientAmount >= 0 && appearance.gradientAmount <= 1 &&
           std::isfinite(appearance.patternAmount) && appearance.patternAmount >= 0 && appearance.patternAmount <= 1;
}

LifeStage lifeStage(int age)
{
    return age <= 12 ? LifeStage::Young : age <= 17 ? LifeStage::Adolescent : age <= 64 ? LifeStage::Adult : LifeStage::Old;
}

const char* lifeStageName(LifeStage stage)
{
    switch (stage)
    {
    case LifeStage::Young: return "young";
    case LifeStage::Adolescent: return "adolescent";
    case LifeStage::Adult: return "adult";
    case LifeStage::Old: return "old";
    }
    return "";
}

double shoulderHeightCm(const Appearance& appearance, int age)
{
    if (!validAppearance(appearance)) return 0;
    // Design-only scale table for the fictional RATW lifespans and portraits.
    const double baseline = appearance.species == "maned" ? 90 : appearance.species == "arctic" ? 72 :
                            appearance.species == "red" ? 66 : appearance.species == "ethiopian" ? 60 : 76;
    const double stature = appearance.stature == "short" ? .88 : appearance.stature == "tall" ? 1.12 : 1;
    const auto stage = lifeStage(age);
    const double maturity = stage == LifeStage::Young ? .65 : stage == LifeStage::Adolescent ? .87 :
                            stage == LifeStage::Old ? .96 : 1;
    return baseline * stature * maturity;
}
} // namespace ratw
