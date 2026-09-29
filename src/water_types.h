#pragma once

#include <cstdint>

enum class WaterClass : uint8_t
{
    None = 0,
    Lake = 1,
    River = 2,
    Ocean = 3,
    Interior = 4,
};

constexpr int kWaterClassCount = 5;
constexpr int kWaterPresetTiles = 4;
constexpr int kWaterMaxTiles = 8;
constexpr int kWaterMaskSlots = 6;
constexpr int32_t kWaterNoIndex = -1;

constexpr uint32_t kForeverGenericLake = 1240;
constexpr uint32_t kForeverGenericOcean = 1250;
constexpr uint32_t kForeverGenericRiver = 1288;
constexpr uint32_t kForeverWmoInterior = 1290;

enum class WaterMaskSlot : int
{
    HighFoam = 0,
    MidFoam = 1,
    LowFoam = 2,
    ShoreFoam = 3,
    DepthFoam = 4,
    RiverFoam = 5,
};

struct WaterFftTile
{
    uint32_t foreverTileId;
    float size;
    float amplitude;
    float windMultiplier;
    float windAlignment;
    float foam[3];
    float oxygen[3];
};

struct WaterPreset
{
    uint32_t foreverLiquidId;
    float absorption[4];
    float scatteringIntensities[4];
    float scatteringTop[4];
    float scatteringBottom[4];
    float depthFadeFoam[4];
    float shoreFoam[4];
    float waveFoam[4];
    float waveFoamScaling[4];
    float flow[4];
    float roughness[4];
    int32_t tiles[kWaterPresetTiles];
    int32_t masks[kWaterMaskSlots];
};

struct WaterMask
{
    uint32_t foreverFileDataId;
    uint32_t size;
    uint32_t mipCount;
    float tintLow[3];
    float tintHigh[3];
};

inline uint32_t ForeverLiquidOf(WaterClass waterClass)
{
    switch (waterClass)
    {
    case WaterClass::Lake:
        return kForeverGenericLake;
    case WaterClass::River:
        return kForeverGenericRiver;
    case WaterClass::Ocean:
        return kForeverGenericOcean;
    case WaterClass::Interior:
        return kForeverWmoInterior;
    default:
        return 0;
    }
}
