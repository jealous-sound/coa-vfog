#pragma once

#include "water_types.h"

#include <cstdint>

constexpr uint32_t kLiquidMaterialWater = 1;
constexpr uint32_t kLiquidSoundBankOcean = 1;
constexpr uint32_t kLiquidTypeFastWater = 9;
constexpr uint32_t kLiquidTypeWmoWaterInterior = 17;
constexpr const char* kRiverTextureFolder = "\\river\\";
constexpr const char* kOceanTextureFolder = "\\ocean\\";

inline char AsciiLower(char c)
{
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

inline bool StartsWithIgnoringCase(const char* text, const char* prefix)
{
    for (; *prefix; ++text, ++prefix)
        if (AsciiLower(*text) != AsciiLower(*prefix))
            return false;
    return true;
}

inline bool ContainsIgnoringCase(const char* text, const char* part)
{
    if (!text)
        return false;
    for (; *text; ++text)
        if (StartsWithIgnoringCase(text, part))
            return true;
    return false;
}

inline bool HasWaterSurfaceTexture(const char* texture0)
{
    return ContainsIgnoringCase(texture0, kRiverTextureFolder) || ContainsIgnoringCase(texture0, kOceanTextureFolder);
}

inline WaterClass ClassifyLiquid(uint32_t liquidTypeId, uint32_t soundBank, uint32_t materialId, const char* texture0)
{
    if (materialId != kLiquidMaterialWater || !HasWaterSurfaceTexture(texture0))
        return WaterClass::None;
    if (liquidTypeId == kLiquidTypeWmoWaterInterior)
        return WaterClass::Interior;
    if (soundBank == kLiquidSoundBankOcean)
        return WaterClass::Ocean;
    if (liquidTypeId == kLiquidTypeFastWater)
        return WaterClass::River;
    return WaterClass::Lake;
}

inline const char* WaterClassLabel(WaterClass waterClass)
{
    switch (waterClass)
    {
    case WaterClass::Lake:
        return "lake";
    case WaterClass::River:
        return "river";
    case WaterClass::Ocean:
        return "ocean";
    case WaterClass::Interior:
        return "interior";
    default:
        return "none";
    }
}
