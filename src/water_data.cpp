#include "water_data.h"

#include "log.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace
{
constexpr char kFileMagic[4] = {'V', 'W', 'D', '1'};
constexpr uint32_t kFormatVersion = 1;
constexpr uint32_t kMaxMaskSide = 8192;
constexpr long kMaxFileBytes = 64L * 1024 * 1024;
constexpr uint32_t kFloatExponentBits = 0x7F800000u;

struct Section
{
    uint32_t offset;
    uint32_t count;
    uint32_t recordSize;
};

struct Header
{
    char magic[4];
    uint32_t version;
    uint32_t fileSize;
    Section presets;
    Section tiles;
    Section masks;
    uint32_t mipDataOffset;
    uint32_t mipDataSize;
};

struct MaskRecord
{
    WaterMask info;
    uint32_t mipDataOffset;
    uint32_t mipDataSize;
};

static_assert(sizeof(Header) == 56 && sizeof(WaterPreset) == 204 && sizeof(WaterFftTile) == 44 &&
                  sizeof(MaskRecord) == 44,
              "records match the struct formats of tools/convert_forever_water.py");

struct ByteRange
{
    uint64_t begin;
    uint64_t end;
};

struct WaterFile
{
    std::vector<WaterPreset> presets;
    std::vector<WaterFftTile> tiles;
    std::vector<MaskRecord> masks;
    const uint8_t* mipData = nullptr;
};

uint32_t MipSize(uint32_t size, uint32_t level)
{
    uint32_t s = size >> level;
    return s ? s : 1;
}

uint32_t FullMipCount(uint32_t size)
{
    uint32_t count = 1;
    for (; size > 1; size >>= 1)
        ++count;
    return count;
}

bool MaskGeometryValid(const WaterMask& info)
{
    return info.size > 0 && info.size <= kMaxMaskSide && info.mipCount > 0 && info.mipCount <= FullMipCount(info.size);
}

uint64_t MipChainBytes(const WaterMask& info)
{
    uint64_t bytes = 0;
    for (uint32_t level = 0; level < info.mipCount; ++level)
    {
        const uint64_t side = MipSize(info.size, level);
        bytes += side * side;
    }
    return bytes;
}

bool IndexInRange(int32_t index, size_t count)
{
    return index == kWaterNoIndex || (index >= 0 && static_cast<size_t>(index) < count);
}

bool IsFinite(float value)
{
    uint32_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return (bits & kFloatExponentBits) != kFloatExponentBits;
}

template <size_t N>
bool AllFinite(const float (&values)[N])
{
    return std::all_of(values, values + N, IsFinite);
}

bool AllFinite(const WaterPreset& p)
{
    return AllFinite(p.absorption) && AllFinite(p.scatteringIntensities) && AllFinite(p.scatteringTop) &&
           AllFinite(p.scatteringBottom) && AllFinite(p.depthFadeFoam) && AllFinite(p.shoreFoam) &&
           AllFinite(p.waveFoam) && AllFinite(p.waveFoamScaling) && AllFinite(p.flow) && AllFinite(p.roughness);
}

bool AllFinite(const WaterFftTile& t)
{
    return IsFinite(t.size) && IsFinite(t.amplitude) && IsFinite(t.windMultiplier) && IsFinite(t.windAlignment) &&
           AllFinite(t.foam) && AllFinite(t.oxygen);
}

bool AllFinite(const WaterMask& m)
{
    return AllFinite(m.tintLow) && AllFinite(m.tintHigh);
}

ByteRange SectionRange(const Section& section)
{
    return {section.offset, section.offset + static_cast<uint64_t>(section.count) * section.recordSize};
}

bool RangesFitApart(std::vector<ByteRange> ranges, uint64_t limit)
{
    for (const ByteRange& range : ranges)
        if (range.end > limit)
            return false;
    ranges.erase(std::remove_if(ranges.begin(), ranges.end(), [](const ByteRange& r) { return r.begin == r.end; }),
                 ranges.end());
    std::sort(ranges.begin(), ranges.end(), [](const ByteRange& a, const ByteRange& b) { return a.begin < b.begin; });
    for (size_t i = 1; i < ranges.size(); ++i)
        if (ranges[i - 1].end > ranges[i].begin)
            return false;
    return true;
}

template <typename T>
std::vector<T> ReadRecords(const uint8_t* bytes, const Section& section)
{
    std::vector<T> records(section.count);
    if (section.count)
        std::memcpy(records.data(), bytes + section.offset, static_cast<size_t>(section.count) * sizeof(T));
    return records;
}

const char* ParseWaterFile(const uint8_t* bytes, size_t size, WaterFile& out)
{
    if (!bytes || size < sizeof(Header))
        return "shorter than its header";
    Header header;
    std::memcpy(&header, bytes, sizeof(header));
    if (std::memcmp(header.magic, kFileMagic, sizeof(kFileMagic)) != 0)
        return "not a water data file";
    if (header.version != kFormatVersion)
        return "unsupported format version";
    if (header.fileSize != size)
        return "truncated or padded";
    if (header.presets.recordSize != sizeof(WaterPreset) || header.tiles.recordSize != sizeof(WaterFftTile) ||
        header.masks.recordSize != sizeof(MaskRecord))
        return "record sizes differ from this build";
    if (header.tiles.count > static_cast<uint32_t>(kWaterMaxTiles))
        return "more FFT tiles than the simulation holds";
    const ByteRange mipData = {header.mipDataOffset, static_cast<uint64_t>(header.mipDataOffset) + header.mipDataSize};
    if (!RangesFitApart({{0, sizeof(Header)}, SectionRange(header.presets), SectionRange(header.tiles),
                         SectionRange(header.masks), mipData},
                        size))
        return "sections overlap or leave the file";
    out.presets = ReadRecords<WaterPreset>(bytes, header.presets);
    out.tiles = ReadRecords<WaterFftTile>(bytes, header.tiles);
    out.masks = ReadRecords<MaskRecord>(bytes, header.masks);
    std::vector<ByteRange> chains;
    for (const MaskRecord& mask : out.masks)
    {
        if (!MaskGeometryValid(mask.info) || mask.mipDataSize != MipChainBytes(mask.info))
            return "a mask's mip chain does not match its size";
        chains.push_back({mask.mipDataOffset, static_cast<uint64_t>(mask.mipDataOffset) + mask.mipDataSize});
    }
    if (!RangesFitApart(chains, header.mipDataSize))
        return "mask mip chains overlap or leave the mip data";
    out.mipData = bytes + header.mipDataOffset;
    return nullptr;
}

std::vector<const uint8_t*> MipLevels(const MaskRecord& mask, const uint8_t* mipData)
{
    std::vector<const uint8_t*> levels;
    const uint8_t* texels = mipData + mask.mipDataOffset;
    for (uint32_t level = 0; level < mask.info.mipCount; ++level)
    {
        levels.push_back(texels);
        const size_t side = MipSize(mask.info.size, level);
        texels += side * side;
    }
    return levels;
}

bool ReadWholeFile(std::FILE* f, std::vector<uint8_t>& bytes)
{
    if (std::fseek(f, 0, SEEK_END) != 0)
        return false;
    const long size = std::ftell(f);
    if (size < 0 || size > kMaxFileBytes || std::fseek(f, 0, SEEK_SET) != 0)
        return false;
    bytes.resize(static_cast<size_t>(size));
    return bytes.empty() || std::fread(bytes.data(), 1, bytes.size(), f) == bytes.size();
}
}

bool WaterData::Load(const std::string& path)
{
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f)
    {
        Clear();
        VF_LOG_INFO("no Forever water data at %s; native water only", path.c_str());
        return false;
    }
    std::vector<uint8_t> bytes;
    const bool read = ReadWholeFile(f, bytes);
    std::fclose(f);
    if (read && LoadFromMemory(bytes.data(), bytes.size()))
        return true;
    Clear();
    VF_LOG_ERROR("Forever water data %s is invalid; native water only", path.c_str());
    return false;
}

bool WaterData::LoadFromMemory(const uint8_t* bytes, size_t size)
{
    WaterFile file;
    if (const char* problem = ParseWaterFile(bytes, size, file))
    {
        Clear();
        VF_LOG_ERROR("Forever water data rejected: %s", problem);
        return false;
    }
    std::vector<std::vector<const uint8_t*>> levels;
    std::vector<WaterMaskView> views;
    levels.reserve(file.masks.size());
    for (const MaskRecord& mask : file.masks)
    {
        levels.push_back(MipLevels(mask, file.mipData));
        views.push_back({mask.info, levels.back().data()});
    }
    if (!Assign(file.presets.data(), static_cast<int>(file.presets.size()), file.tiles.data(),
                static_cast<int>(file.tiles.size()), views.data(), static_cast<int>(views.size())))
    {
        VF_LOG_ERROR("Forever water data rejected: an index is out of range, a value is not finite or a tile is empty");
        return false;
    }
    VF_LOG_INFO("Forever water data: %u presets, %u FFT tiles, %u foam masks, %u bytes",
                static_cast<unsigned>(m_presets.size()), static_cast<unsigned>(m_tiles.size()),
                static_cast<unsigned>(m_masks.size()), static_cast<unsigned>(size));
    return true;
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
        if (!MaskGeometryValid(masks[m].info) || !masks[m].levels)
        {
            Clear();
            return false;
        }
        WaterMaskLevels levels;
        levels.info = masks[m].info;
        for (uint32_t level = 0; level < masks[m].info.mipCount; ++level)
        {
            const size_t side = MipSize(masks[m].info.size, level);
            const uint8_t* texels = masks[m].levels[level];
            if (!texels)
            {
                Clear();
                return false;
            }
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
        if (!AllFinite(preset))
            return false;
        for (int32_t tile : preset.tiles)
            if (!IndexInRange(tile, m_tiles.size()))
                return false;
        for (int32_t mask : preset.masks)
            if (!IndexInRange(mask, m_masks.size()))
                return false;
    }
    for (const WaterMaskLevels& mask : m_masks)
    {
        if (!MaskGeometryValid(mask.info) || !AllFinite(mask.info) || mask.levels.size() != mask.info.mipCount ||
            mask.levels.empty())
            return false;
        for (uint32_t level = 0; level < mask.info.mipCount; ++level)
        {
            const uint32_t side = MipSize(mask.info.size, level);
            if (mask.levels[level].size() != static_cast<size_t>(side) * side)
                return false;
        }
    }
    for (const WaterFftTile& tile : m_tiles)
        if (!AllFinite(tile) || !(tile.size > 0.0f))
            return false;
    return true;
}

WaterData& GlobalWaterData()
{
    static WaterData data;
    return data;
}
