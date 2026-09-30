#pragma once

#include "water_types.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

struct WaterMaskLevels
{
    WaterMask info;
    std::vector<std::vector<uint8_t>> levels;
};

struct WaterMaskView
{
    WaterMask info;
    const uint8_t* const* levels;
};

constexpr int kWaveFoamMaskSlots = 3;
constexpr uint32_t kPackedMaskOpaque = 0xFF000000u;
constexpr int kPackedMaskChannelShift[kWaveFoamMaskSlots] = {16, 8, 0};

struct WaterPackedMask
{
    uint32_t size = 0;
    std::vector<std::vector<uint32_t>> levels;
    bool present[kWaveFoamMaskSlots] = {};
};

WaterPackedMask PackWaveFoamMasks(const std::vector<WaterMaskLevels>& masks,
                                  const int32_t (&indices)[kWaveFoamMaskSlots]);

class WaterData
{
public:
    bool Load(const std::string& path);
    bool LoadFromMemory(const uint8_t* bytes, size_t size);
    bool Assign(const WaterPreset* presets, int presetCount, const WaterFftTile* tiles, int tileCount,
                const WaterMaskView* masks, int maskCount);
    void Clear();

    bool Loaded() const { return m_loaded; }
    uint32_t Revision() const { return m_revision; }
    const WaterPreset* Preset(WaterClass waterClass) const;
    const std::vector<WaterPreset>& Presets() const { return m_presets; }
    const std::vector<WaterFftTile>& Tiles() const { return m_tiles; }
    const std::vector<WaterMaskLevels>& Masks() const { return m_masks; }

private:
    bool Validate() const;

    bool m_loaded = false;
    uint32_t m_revision = 0;
    std::vector<WaterPreset> m_presets;
    std::vector<WaterFftTile> m_tiles;
    std::vector<WaterMaskLevels> m_masks;
};

WaterData& GlobalWaterData();
