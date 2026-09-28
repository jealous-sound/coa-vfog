#include "water_data.h"

#include "log.h"

namespace
{
uint32_t MipSize(uint32_t size, uint32_t level)
{
    uint32_t s = size >> level;
    return s ? s : 1;
}

bool IndexInRange(int32_t index, size_t count)
{
    return index == kWaterNoIndex || (index >= 0 && static_cast<size_t>(index) < count);
}
}

bool WaterData::Load(const std::string& path)
{
    VF_LOG_ERROR("water data %s not loaded: the reader is not implemented", path.c_str());
    Clear();
    return false;
}

bool WaterData::LoadFromMemory(const uint8_t*, size_t)
{
    Clear();
    return false;
}

bool WaterData::Assign(const WaterPreset* presets, int presetCount, const WaterFftTile* tiles, int tileCount,
                       const WaterMaskView* masks, int maskCount)
{
    Clear();
    if (presetCount < 0 || tileCount < 0 || maskCount < 0 || tileCount > kWaterMaxTiles)
        return false;
    m_presets.assign(presets, presets + presetCount);
    m_tiles.assign(tiles, tiles + tileCount);
    for (int m = 0; m < maskCount; ++m)
    {
        WaterMaskLevels levels;
        levels.info = masks[m].info;
        for (uint32_t level = 0; level < masks[m].info.mipCount; ++level)
        {
            const uint32_t side = MipSize(masks[m].info.size, level);
            const uint8_t* texels = masks[m].levels[level];
            levels.levels.emplace_back(texels, texels + side * side);
        }
        m_masks.push_back(std::move(levels));
    }
    if (!Validate())
    {
        Clear();
        return false;
    }
    m_loaded = true;
    ++m_revision;
    return true;
}

void WaterData::Clear()
{
    m_loaded = false;
    m_presets.clear();
    m_tiles.clear();
    m_masks.clear();
}

const WaterPreset* WaterData::Preset(WaterClass waterClass) const
{
    const uint32_t id = ForeverLiquidOf(waterClass);
    for (const WaterPreset& preset : m_presets)
        if (id && preset.foreverLiquidId == id)
            return &preset;
    return nullptr;
}

bool WaterData::Validate() const
{
    for (const WaterPreset& preset : m_presets)
    {
        for (int32_t tile : preset.tiles)
            if (!IndexInRange(tile, m_tiles.size()))
                return false;
        for (int32_t mask : preset.masks)
            if (!IndexInRange(mask, m_masks.size()))
                return false;
    }
    for (const WaterMaskLevels& mask : m_masks)
    {
        if (!mask.info.size || mask.levels.size() != mask.info.mipCount || mask.levels.empty())
            return false;
        for (uint32_t level = 0; level < mask.info.mipCount; ++level)
        {
            const uint32_t side = MipSize(mask.info.size, level);
            if (mask.levels[level].size() != static_cast<size_t>(side) * side)
                return false;
        }
    }
    for (const WaterFftTile& tile : m_tiles)
        if (!(tile.size > 0.0f))
            return false;
    return true;
}

WaterData& GlobalWaterData()
{
    static WaterData data;
    return data;
}
