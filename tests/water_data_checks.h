#pragma once

namespace water_data_checks
{
constexpr float kValueTolerance = 1e-4f;
constexpr float kBoxFilterTolerance = 1.0f;
constexpr float kFoamHighMeanCoverage = 46.9f;
constexpr float kMeanCoverageTolerance = 0.1f;
constexpr float kMinimumTintRise = 0.1f;
constexpr float kGamma = 2.2f;
constexpr float kChannelMax = 255.0f;
constexpr uint32_t kMaskSide = 1024;
constexpr uint32_t kMaskLevels = 11;
constexpr uint32_t kFoamHigh = 7475833;
constexpr uint32_t kFoamMid = 7475835;
constexpr uint32_t kFoamLow = 7475834;
constexpr uint32_t kFoamRim = 7475836;
constexpr uint32_t kFoamDepth = 7660147;
constexpr uint32_t kFoamRiver = 7480794;
constexpr int kShoreSlot = static_cast<int>(WaterMaskSlot::ShoreFoam);
constexpr int kDepthSlot = static_cast<int>(WaterMaskSlot::DepthFoam);
constexpr uint32_t kSmallMaskSide = 4;
constexpr uint32_t kSmallMaskLevels = 3;

struct FileHeader
{
    char magic[4];
    uint32_t version;
    uint32_t fileSize;
    uint32_t presetOffset;
    uint32_t presetCount;
    uint32_t presetRecordSize;
    uint32_t tileOffset;
    uint32_t tileCount;
    uint32_t tileRecordSize;
    uint32_t maskOffset;
    uint32_t maskCount;
    uint32_t maskRecordSize;
    uint32_t mipDataOffset;
    uint32_t mipDataSize;
};

struct FileMask
{
    WaterMask info;
    uint32_t mipDataOffset;
    uint32_t mipDataSize;
};

static_assert(sizeof(FileHeader) == 56 && sizeof(FileMask) == 44, "layout of tools/convert_forever_water.py");

struct WaterFile
{
    FileHeader header;
    std::vector<WaterPreset> presets;
    std::vector<WaterFftTile> tiles;
    std::vector<FileMask> masks;
    std::vector<uint8_t> mipData;
};

std::vector<uint8_t> ReadBytes(const std::string& path)
{
    std::vector<uint8_t> bytes;
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f)
        return bytes;
    uint8_t chunk[65536];
    for (size_t n; (n = std::fread(chunk, 1, sizeof(chunk), f)) > 0;)
        bytes.insert(bytes.end(), chunk, chunk + n);
    std::fclose(f);
    return bytes;
}

template <typename T>
bool CopyRecords(const std::vector<uint8_t>& bytes, uint32_t offset, uint32_t count, std::vector<T>& out)
{
    if (offset + static_cast<uint64_t>(count) * sizeof(T) > bytes.size())
        return false;
    out.resize(count);
    if (count)
        std::memcpy(out.data(), bytes.data() + offset, count * sizeof(T));
    return true;
}

bool Split(const std::vector<uint8_t>& bytes, WaterFile& file)
{
    if (bytes.size() < sizeof(FileHeader))
        return false;
    std::memcpy(&file.header, bytes.data(), sizeof(FileHeader));
    const FileHeader& h = file.header;
    const uint64_t mipEnd = static_cast<uint64_t>(h.mipDataOffset) + h.mipDataSize;
    if (!CopyRecords(bytes, h.presetOffset, h.presetCount, file.presets) ||
        !CopyRecords(bytes, h.tileOffset, h.tileCount, file.tiles) ||
        !CopyRecords(bytes, h.maskOffset, h.maskCount, file.masks) || mipEnd > bytes.size())
        return false;
    file.mipData.assign(bytes.begin() + h.mipDataOffset, bytes.begin() + static_cast<size_t>(mipEnd));
    return true;
}

template <typename T>
void Append(std::vector<uint8_t>& bytes, const std::vector<T>& records)
{
    const uint8_t* first = reinterpret_cast<const uint8_t*>(records.data());
    bytes.insert(bytes.end(), first, first + records.size() * sizeof(T));
}

std::vector<uint8_t> Assemble(const WaterFile& file)
{
    FileHeader h = file.header;
    h.presetOffset = sizeof(FileHeader);
    h.presetCount = static_cast<uint32_t>(file.presets.size());
    h.tileOffset = h.presetOffset + h.presetCount * static_cast<uint32_t>(sizeof(WaterPreset));
    h.tileCount = static_cast<uint32_t>(file.tiles.size());
    h.maskOffset = h.tileOffset + h.tileCount * static_cast<uint32_t>(sizeof(WaterFftTile));
    h.maskCount = static_cast<uint32_t>(file.masks.size());
    h.mipDataOffset = h.maskOffset + h.maskCount * static_cast<uint32_t>(sizeof(FileMask));
    h.mipDataSize = static_cast<uint32_t>(file.mipData.size());
    h.fileSize = h.mipDataOffset + h.mipDataSize;
    std::vector<uint8_t> bytes(sizeof(h));
    std::memcpy(bytes.data(), &h, sizeof(h));
    Append(bytes, file.presets);
    Append(bytes, file.tiles);
    Append(bytes, file.masks);
    bytes.insert(bytes.end(), file.mipData.begin(), file.mipData.end());
    return bytes;
}

bool Rejected(const std::vector<uint8_t>& bytes)
{
    WaterData data;
    bool loaded = true;
    try
    {
        loaded = data.LoadFromMemory(bytes.data(), bytes.size());
    }
    catch (...)
    {
        return false;
    }
    return !loaded && !data.Loaded() && data.Presets().empty() && data.Tiles().empty() && data.Masks().empty();
}

void GrowTiles(WaterFile& file, size_t count)
{
    const WaterFftTile copy = file.tiles[0];
    file.tiles.resize(count, copy);
}

template <typename Edit>
void CheckRejectedEdit(const WaterFile& original, Edit edit, const char* label)
{
    WaterFile file = original;
    edit(file);
    Check(Rejected(Assemble(file)), label);
}

template <typename Edit>
void CheckRejectedHeader(const std::vector<uint8_t>& original, Edit edit, const char* label)
{
    std::vector<uint8_t> bytes = original;
    FileHeader header;
    std::memcpy(&header, bytes.data(), sizeof(header));
    edit(header);
    std::memcpy(bytes.data(), &header, sizeof(header));
    Check(Rejected(bytes), label);
}

bool Near(float value, float expected, float tolerance = kValueTolerance)
{
    return std::fabs(value - expected) <= tolerance * std::max(1.0f, std::fabs(expected));
}

bool Near4(const float* values, float x, float y, float z, float w)
{
    return Near(values[0], x) && Near(values[1], y) && Near(values[2], z) && Near(values[3], w);
}

float OpticalDepth(int channel)
{
    return -std::log(static_cast<float>(channel) / kChannelMax);
}

float Linearised(int channel)
{
    return std::pow(static_cast<float>(channel) / kChannelMax, kGamma);
}

std::vector<float> TileSizes(const WaterData& data, const WaterPreset& preset)
{
    std::vector<float> sizes;
    for (int32_t tile : preset.tiles)
        if (tile != kWaterNoIndex)
            sizes.push_back(data.Tiles()[tile].size);
    return sizes;
}

std::vector<uint32_t> MaskTextures(const WaterData& data, const WaterPreset& preset)
{
    std::vector<uint32_t> ids;
    for (int32_t mask : preset.masks)
        ids.push_back(mask == kWaterNoIndex ? 0 : data.Masks()[mask].info.foreverFileDataId);
    return ids;
}

const WaterFftTile* FindTile(const WaterData& data, uint32_t id)
{
    for (const WaterFftTile& tile : data.Tiles())
        if (tile.foreverTileId == id)
            return &tile;
    return nullptr;
}

bool TileMatches(const WaterFftTile* tile, float size, float amplitude, float windMultiplier, float windAlignment,
                 const float (&foam)[3], const float (&oxygen)[3])
{
    if (!tile)
        return false;
    bool ok = Near(tile->size, size) && Near(tile->amplitude, amplitude) &&
              Near(tile->windMultiplier, windMultiplier) && Near(tile->windAlignment, windAlignment);
    for (int i = 0; i < 3; ++i)
        ok = ok && Near(tile->foam[i], foam[i]) && Near(tile->oxygen[i], oxygen[i]);
    return ok;
}

bool MipChainIsFull(const WaterMaskLevels& mask)
{
    if (mask.info.size != kMaskSide || mask.info.mipCount != kMaskLevels || mask.levels.size() != kMaskLevels)
        return false;
    for (uint32_t level = 0; level < kMaskLevels; ++level)
    {
        const size_t side = std::max<size_t>(kMaskSide >> level, 1);
        if (mask.levels[level].size() != side * side)
            return false;
    }
    return true;
}

float MeanCoverage(const std::vector<uint8_t>& texels)
{
    double sum = 0.0;
    for (uint8_t t : texels)
        sum += t;
    return texels.empty() ? 0.0f : static_cast<float>(sum / texels.size());
}

bool SecondLevelIsBoxFiltered(const WaterMaskLevels& mask)
{
    const size_t side = mask.info.size;
    const size_t half = side / 2;
    const std::vector<uint8_t>& top = mask.levels[0];
    const std::vector<uint8_t>& next = mask.levels[1];
    for (size_t y = 0; y < half; ++y)
        for (size_t x = 0; x < half; ++x)
        {
            const size_t t = 2 * y * side + 2 * x;
            const float box = (top[t] + top[t + 1] + top[t + side] + top[t + side + 1]) * 0.25f;
            if (std::fabs(box - next[y * half + x]) > kBoxFilterTolerance)
                return false;
        }
    return std::fabs(MeanCoverage(top) - mask.levels.back()[0]) <= kBoxFilterTolerance;
}

bool TintsBrightenWithCoverage(const WaterMask& info)
{
    for (int c = 0; c < 3; ++c)
        if (!(info.tintLow[c] >= 0.0f && info.tintHigh[c] <= 1.0f &&
              info.tintHigh[c] - info.tintLow[c] >= kMinimumTintRise))
            return false;
    return true;
}

void CheckPresets(const WaterData& data)
{
    std::vector<uint32_t> liquids;
    for (const WaterPreset& preset : data.Presets())
        liquids.push_back(preset.foreverLiquidId);
    Check(liquids == std::vector<uint32_t>{kForeverGenericLake, kForeverGenericRiver, kForeverGenericOcean,
                                           kForeverWmoInterior},
          "water data holds the lake, river, ocean and interior presets in class order");
    bool resolved = !data.Preset(WaterClass::None);
    for (WaterClass waterClass : {WaterClass::Lake, WaterClass::River, WaterClass::Ocean, WaterClass::Interior})
    {
        const WaterPreset* preset = data.Preset(waterClass);
        resolved = resolved && preset && preset->foreverLiquidId == ForeverLiquidOf(waterClass);
    }
    Check(resolved, "every water class resolves its Forever preset and None resolves nothing");
    const WaterPreset* lake = data.Preset(WaterClass::Lake);
    const WaterPreset* river = data.Preset(WaterClass::River);
    const WaterPreset* ocean = data.Preset(WaterClass::Ocean);
    const WaterPreset* interior = data.Preset(WaterClass::Interior);
    if (!lake || !river || !ocean || !interior)
        return;

    const std::vector<float> lakeSizes = {32.0f, 64.0f, 16.0f};
    const std::vector<float> oceanSizes = {128.0f, 256.0f, 512.0f, 32.0f};
    const std::vector<float> interiorSizes = {32.0f, 64.0f};
    Check(TileSizes(data, *lake) == lakeSizes && TileSizes(data, *river) == lakeSizes &&
              lake->tiles[3] == kWaterNoIndex,
          "lake and river FFT tiles are 32, 64 and 16 yards with the fourth slot unused");
    Check(TileSizes(data, *ocean) == oceanSizes, "ocean FFT tiles are 128, 256, 512 and 32 yards");
    Check(TileSizes(data, *interior) == interiorSizes, "interior FFT tiles are 32 and 64 yards");

    const std::vector<uint32_t> lakeMasks = {kFoamHigh, kFoamMid, kFoamLow, kFoamRim, kFoamDepth, kFoamRiver};
    const std::vector<uint32_t> oceanMasks = {kFoamHigh, kFoamMid, kFoamLow, kFoamDepth, kFoamRim, kFoamRiver};
    Check(MaskTextures(data, *lake) == lakeMasks && MaskTextures(data, *river) == lakeMasks &&
              MaskTextures(data, *interior) == lakeMasks,
          "lake, river and interior foam slots follow the texture OrderIndex");
    Check(MaskTextures(data, *ocean) == oceanMasks && ocean->masks[kShoreSlot] == lake->masks[kDepthSlot] &&
              ocean->masks[kDepthSlot] == lake->masks[kShoreSlot],
          "the ocean swaps the shore and depth-fade foam masks");

    Check(Near4(lake->absorption, OpticalDepth(0x5D), OpticalDepth(0xB6), OpticalDepth(0xD4), 0.3f) &&
              Near4(ocean->absorption, OpticalDepth(0xB3), OpticalDepth(0xDB), OpticalDepth(0xC6), 0.25f) &&
              Near4(interior->absorption, OpticalDepth(0xCC), OpticalDepth(0xDB), OpticalDepth(0xB3), 0.5f),
          "absorption is -ln of Color[0] with Float[0] as its density");
    Check(Near4(lake->scatteringTop, Linearised(0x87), Linearised(0xBE), Linearised(0xFF), 4.0f) &&
              Near4(lake->scatteringBottom, Linearised(0x2A), Linearised(0x85), Linearised(0xC1), 0.5f) &&
              Near4(ocean->scatteringTop, Linearised(0x52), Linearised(0xC1), Linearised(0xC8), 6.0f) &&
              Near4(ocean->scatteringBottom, Linearised(0x33), Linearised(0x6D), Linearised(0x7C), 0.75f),
          "scattering colours are linearised Color[1] and Color[2] with Float[2] and the phase g Float[1]");
    Check(Near4(lake->scatteringIntensities, 20.0f, 0.5f, 1.0f, 80.0f) &&
              Near4(ocean->scatteringIntensities, 4.0f, 6.0f, 2.0f, 200.0f) &&
              Near4(lake->depthFadeFoam, 0.826f, 26.0f, 1.0f, 0.01f) &&
              Near4(ocean->depthFadeFoam, 0.639f, 22.0f, 15.0f, 0.003f) &&
              Near4(river->shoreFoam, 0.0f, 36.0f, 0.444f, 0.015f) &&
              Near4(ocean->waveFoam, 0.9f, -0.02f, 0.0f, 0.0f) &&
              Near4(ocean->waveFoamScaling, 5.0f, 12.0f, 20.0f, 0.0f) &&
              Near4(lake->flow, 1.1f, 0.03f, 0.001f, -0.146f) && Near4(interior->flow, 1.1f, 0.03f, 0.01f, -0.146f) &&
              Near4(lake->roughness, 0.03f, 0.013f, 1.0f, 0.0f),
          "intensities, foam, flow and roughness follow the H2a Float[] mapping, "
          "with the sun roughness from Float[27]");
}

void CheckTilesAndMasks(const WaterData& data)
{
    std::vector<uint32_t> tileIds;
    for (const WaterFftTile& tile : data.Tiles())
        tileIds.push_back(tile.foreverTileId);
    Check(tileIds == std::vector<uint32_t>{20, 24, 122, 44, 45, 46, 96},
          "the FFT tiles are the seven the presets use, in first-use order");
    Check(TileMatches(FindTile(data, 20), 32.0f, 7689.0f, 3.0f, 4.0f, {-1.0f, 0.0f, 1.0f}, {-1.0f, 0.0f, 1.0f}) &&
              TileMatches(FindTile(data, 24), 64.0f, 50000.0f, 3.0f, 5.0f, {-0.16f, 12.0f, 3.0f},
                          {-0.16f, 2.0f, 2.0f}) &&
              TileMatches(FindTile(data, 46), 512.0f, 16000.0f, 1.8f, 6.0f, {-0.2f, 20.0f, 1.5f},
                          {-0.35f, 200.0f, 0.7f}) &&
              TileMatches(FindTile(data, 96), 32.0f, 200000.0f, 4.0f, 6.0f, {-1.0f, 0.0f, 1.0f}, {-1.0f, 0.0f, 1.0f}),
          "tile size, amplitude, wind multiplier F3, wind alignment F2, foam and oxygen follow FFTTile");

    const std::vector<WaterMaskLevels>& masks = data.Masks();
    std::vector<uint32_t> maskIds;
    bool full = masks.size() == 6;
    bool tinted = full;
    for (const WaterMaskLevels& mask : masks)
    {
        maskIds.push_back(mask.info.foreverFileDataId);
        full = full && MipChainIsFull(mask);
        tinted = tinted && TintsBrightenWithCoverage(mask.info);
    }
    Check(maskIds == std::vector<uint32_t>{kFoamHigh, kFoamMid, kFoamLow, kFoamRim, kFoamDepth, kFoamRiver},
          "the foam masks are the six Elwynn textures, in first-use order");
    Check(full, "every foam mask is 1024 texels with 11 mip levels down to 1x1");
    Check(tinted, "foam mask tints are linear colours in [0, 1] that brighten with coverage");
    if (!full)
        return;
    Check(std::fabs(MeanCoverage(masks[0].levels[0]) - kFoamHighMeanCoverage) <= kMeanCoverageTolerance,
          "foam_high coverage is the BLP alpha channel");
    bool boxFiltered = true;
    for (const WaterMaskLevels& mask : masks)
        boxFiltered = boxFiltered && SecondLevelIsBoxFiltered(mask);
    Check(boxFiltered, "each stored mip level is the 2x2 box filter of the level above, down to 1x1");
}

void CheckMalformedFiles(const std::vector<uint8_t>& original)
{
    WaterFile file = {};
    Check(Split(original, file) && Assemble(file) == original,
          "reassembling the parsed water data reproduces the converter's bytes");
    WaterFile eightTiles = file;
    GrowTiles(eightTiles, kWaterMaxTiles);
    WaterData eight;
    const std::vector<uint8_t> eightBytes = Assemble(eightTiles);
    Check(eight.LoadFromMemory(eightBytes.data(), eightBytes.size()) &&
              eight.Tiles().size() == static_cast<size_t>(kWaterMaxTiles),
          "a reassembled file with eight FFT tiles loads");

    std::vector<uint8_t> bytes(original.begin(), original.end() - 1);
    Check(Rejected(bytes), "water data rejects a file truncated by one byte");
    bytes.assign(original.begin(), original.begin() + sizeof(FileHeader));
    Check(Rejected(bytes), "water data rejects a file cut after its header");
    bytes = original;
    bytes.push_back(0);
    Check(Rejected(bytes), "water data rejects trailing bytes");
    Check(Rejected({}) && !WaterData().LoadFromMemory(nullptr, 0), "water data rejects an empty buffer");

    CheckRejectedEdit(file, [](WaterFile& f) { f.header.magic[0] = 'X'; }, "water data rejects a bad magic");
    CheckRejectedEdit(file, [](WaterFile& f) { ++f.header.version; }, "water data rejects a newer format version");
    CheckRejectedEdit(file, [](WaterFile& f) { f.header.presetRecordSize -= 4; },
                      "water data rejects a record size that differs from the build");
    CheckRejectedEdit(file, [](WaterFile& f) { GrowTiles(f, kWaterMaxTiles + 1); },
                      "water data rejects more FFT tiles than the simulation holds");
    CheckRejectedEdit(file, [](WaterFile& f) { f.presets[0].tiles[3] = static_cast<int32_t>(f.tiles.size()); },
                      "water data rejects a preset tile index past the tiles");
    CheckRejectedEdit(file, [](WaterFile& f) { f.presets[2].masks[5] = static_cast<int32_t>(f.masks.size()); },
                      "water data rejects a preset mask index past the masks");
    CheckRejectedEdit(file, [](WaterFile& f) { f.presets[1].tiles[0] = kWaterNoIndex - 1; },
                      "water data rejects a negative preset index other than none");
    CheckRejectedEdit(file, [](WaterFile& f) { f.masks[0].info.size /= 2; },
                      "water data rejects a mask whose mip chain does not match its size");
    CheckRejectedEdit(file, [](WaterFile& f) { ++f.masks[3].info.mipCount; },
                      "water data rejects a mip chain that continues past 1x1");
    CheckRejectedEdit(file, [](WaterFile& f) { --f.masks[3].info.mipCount; },
                      "water data rejects a mip byte count that disagrees with the mip count");
    CheckRejectedEdit(file, [](WaterFile& f) { f.masks.back().mipDataOffset += 1; },
                      "water data rejects a mip chain past the end of the mip data");
    CheckRejectedEdit(file, [](WaterFile& f) { f.masks[1].mipDataOffset = f.masks[0].mipDataOffset; },
                      "water data rejects masks that share mip bytes");
    CheckRejectedEdit(file, [](WaterFile& f) { f.presets[0].absorption[1] = std::numeric_limits<float>::quiet_NaN(); },
                      "water data rejects a NaN preset field");
    CheckRejectedEdit(file, [](WaterFile& f) { f.tiles[4].amplitude = std::numeric_limits<float>::infinity(); },
                      "water data rejects an infinite tile field");
    CheckRejectedEdit(file, [](WaterFile& f) { f.masks[2].info.tintHigh[1] = std::numeric_limits<float>::quiet_NaN(); },
                      "water data rejects a NaN mask tint");
    CheckRejectedEdit(file, [](WaterFile& f) { f.tiles[2].size = 0.0f; }, "water data rejects a zero-sized FFT tile");

    CheckRejectedHeader(original, [](FileHeader& h) { h.tileOffset = h.presetOffset; },
                        "water data rejects overlapping sections");
    CheckRejectedHeader(original, [](FileHeader& h) { h.maskCount += 1; },
                        "water data rejects a record section that runs into the mip data");
    CheckRejectedHeader(original, [](FileHeader& h) { h.mipDataSize += 1; },
                        "water data rejects mip data that leaves the file");
    CheckRejectedHeader(original, [](FileHeader& h) { h.presetCount = UINT32_MAX; },
                        "water data rejects impossible counts before allocating");
}

WaterMaskView SmallMask(const uint8_t* const* levels, uint32_t mipCount)
{
    WaterMask info = {};
    info.foreverFileDataId = kFoamHigh;
    info.size = kSmallMaskSide;
    info.mipCount = mipCount;
    return {info, levels};
}

void CheckAssignRejectsInconsistentData()
{
    const uint8_t texels[kSmallMaskSide * kSmallMaskSide] = {};
    const uint8_t* levels[kSmallMaskLevels + 1] = {texels, texels, texels, texels};
    WaterPreset preset = {};
    preset.foreverLiquidId = kForeverGenericLake;
    std::fill(std::begin(preset.tiles), std::end(preset.tiles), kWaterNoIndex);
    std::fill(std::begin(preset.masks), std::end(preset.masks), kWaterNoIndex);
    preset.tiles[0] = 0;
    preset.masks[0] = 0;
    WaterFftTile tile = {};
    tile.foreverTileId = 20;
    tile.size = 32.0f;
    const WaterMaskView mask = SmallMask(levels, kSmallMaskLevels);

    WaterData data;
    const uint32_t before = data.Revision();
    Check(data.Assign(&preset, 1, &tile, 1, &mask, 1) && data.Loaded() && data.Revision() == before + 1 &&
              data.Preset(WaterClass::Lake) && data.Masks()[0].levels.size() == kSmallMaskLevels,
          "Assign accepts a consistent preset, tile and mask and advances the revision");

    WaterPreset badPreset = preset;
    badPreset.tiles[1] = 1;
    Check(!data.Assign(&badPreset, 1, &tile, 1, &mask, 1) && !data.Loaded() && data.Presets().empty() &&
              data.Revision() == before + 1,
          "Assign rejects a preset tile index past the tiles and clears the data");
    badPreset = preset;
    badPreset.masks[4] = 1;
    Check(!data.Assign(&badPreset, 1, &tile, 1, &mask, 1), "Assign rejects a preset mask index past the masks");
    const WaterMaskView deepMask = SmallMask(levels, kSmallMaskLevels + 1);
    Check(!data.Assign(&preset, 1, &tile, 1, &deepMask, 1), "Assign rejects a mip count past 1x1");
    const uint8_t* holedLevels[kSmallMaskLevels] = {texels, nullptr, texels};
    const WaterMaskView holedMask = SmallMask(holedLevels, kSmallMaskLevels);
    Check(!data.Assign(&preset, 1, &tile, 1, &holedMask, 1), "Assign rejects a missing mip level");
    std::vector<WaterFftTile> tooMany(kWaterMaxTiles + 1, tile);
    Check(!data.Assign(&preset, 1, tooMany.data(), static_cast<int>(tooMany.size()), &mask, 1),
          "Assign rejects more FFT tiles than the simulation holds");
    WaterFftTile badTile = tile;
    badTile.windAlignment = std::numeric_limits<float>::quiet_NaN();
    Check(!data.Assign(&preset, 1, &badTile, 1, &mask, 1), "Assign rejects a NaN tile field");
    badTile = tile;
    badTile.size = 0.0f;
    Check(!data.Assign(&preset, 1, &badTile, 1, &mask, 1), "Assign rejects a zero-sized tile");
}

void CheckDllRejectsNonFiniteFile(const std::wstring& outDir, const std::vector<uint8_t>& original,
                                  const std::string& waterDataPath)
{
    WaterFile file = {};
    if (!Split(original, file))
        return;
    file.presets[3].roughness[0] = std::numeric_limits<float>::quiet_NaN();
    const std::vector<uint8_t> bytes = Assemble(file);
    const std::wstring fixture = FullPath(outDir + L"\\invalid-waterdata.bin");
    std::FILE* f = _wfopen(fixture.c_str(), L"wb");
    bool written = false;
    if (f)
    {
        written = std::fwrite(bytes.data(), 1, bytes.size(), f) == bytes.size();
        written = std::fclose(f) == 0 && written;
    }
    Check(written && vf_test_load_water_data(NarrowPath(fixture).c_str()) == 0,
          "the fast-math DLL build rejects a NaN water data file");
    Check(vf_test_load_water_data(waterDataPath.c_str()) == 1, "the DLL loads the converted water data");
    DeleteFileW(fixture.c_str());
}

void CheckWaterData(const std::wstring& outDir, const std::string& waterDataPath)
{
    WaterData missing;
    Check(!missing.Load(NarrowPath(FullPath(outDir + L"\\missing-waterdata.bin"))) && !missing.Loaded(),
          "a missing water data file is not loaded");
    WaterData data;
    Check(data.Load(waterDataPath) && data.Loaded(), "Forever water data loads from data/waterdata.bin");
    if (!data.Loaded())
        return;
    CheckPresets(data);
    CheckTilesAndMasks(data);

    const std::vector<uint8_t> original = ReadBytes(waterDataPath);
    Check(original.size() > sizeof(FileHeader), "water data bytes are available for malformed-file checks");
    if (original.size() <= sizeof(FileHeader))
        return;
    const uint32_t revision = data.Revision();
    Check(data.LoadFromMemory(original.data(), original.size()) && data.Revision() == revision + 1,
          "reloading the water data advances the revision");
    Check(!data.LoadFromMemory(original.data(), original.size() - 1) && !data.Loaded() && data.Presets().empty() &&
              data.Masks().empty() && data.Revision() == revision + 1,
          "a rejected reload clears the water data and keeps the revision");
    CheckMalformedFiles(original);
    CheckAssignRejectsInconsistentData();
    CheckDllRejectsNonFiniteFile(outDir, original, waterDataPath);
}
}
