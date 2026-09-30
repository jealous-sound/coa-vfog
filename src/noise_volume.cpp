#include "noise_volume.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace
{
constexpr uint32_t kAuthoredNoiseBaseCells = 4;
constexpr int kAuthoredNoiseDetailLayers = 3;
constexpr float kAuthoredNoiseDetailGain = 0.5f;
constexpr float kByteCentre = 127.5f;
constexpr int kEdgeGradientCount = 12;
constexpr float kEdgeGradients[kEdgeGradientCount][3] = {
    {1, 1, 0}, {-1, 1, 0}, {1, -1, 0}, {-1, -1, 0}, {1, 0, 1}, {-1, 0, 1},
    {1, 0, -1}, {-1, 0, -1}, {0, 1, 1}, {0, -1, 1}, {0, 1, -1}, {0, -1, -1},
};

BYTE DensityNoiseValue(UINT x, UINT y, UINT z)
{
    uint32_t value = x + kDensityNoiseSize * (y + kDensityNoiseSize * z);
    value = ((value ^ (value >> 7)) * 4051u + 12743u) & 0x7FFFu;
    value ^= value >> 9;
    value = (value * 109u + 583u) & 0x7FFFu;
    value ^= value >> 6;
    value = (value * 29317u + 9419u) & 0x7FFFu;
    value ^= value >> 7;
    return static_cast<BYTE>(value >> 7);
}

uint32_t LatticeHash(uint32_t x, uint32_t y, uint32_t z, uint32_t detailLayer)
{
    uint32_t h = (x * 0x8DA6B343u) ^ (y * 0xD8163841u) ^ (z * 0xCB1AB31Fu) ^ (detailLayer * 0x165667B1u);
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0x297A2D39u;
    h ^= h >> 15;
    return h;
}

float QuinticFade(float t)
{
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

float CornerGradientDot(uint32_t hash, float dx, float dy, float dz)
{
    const float* g = kEdgeGradients[hash % kEdgeGradientCount];
    return g[0] * dx + g[1] * dy + g[2] * dz;
}

float TileableGradientNoise(const float* position, uint32_t period, uint32_t detailLayer)
{
    uint32_t cell[3];
    float offset[3];
    float fade[3];
    for (int axis = 0; axis < 3; ++axis)
    {
        const float floored = std::floor(position[axis]);
        cell[axis] = static_cast<uint32_t>(floored) % period;
        offset[axis] = position[axis] - floored;
        fade[axis] = QuinticFade(offset[axis]);
    }
    float value = 0.0f;
    for (int corner = 0; corner < 8; ++corner)
    {
        const uint32_t step[3] = {corner & 1u, (corner >> 1) & 1u, (corner >> 2) & 1u};
        const uint32_t hash = LatticeHash((cell[0] + step[0]) % period, (cell[1] + step[1]) % period,
                                          (cell[2] + step[2]) % period, detailLayer);
        float weight = 1.0f;
        for (int axis = 0; axis < 3; ++axis)
            weight *= step[axis] ? fade[axis] : 1.0f - fade[axis];
        value += weight * CornerGradientDot(hash, offset[0] - step[0], offset[1] - step[1], offset[2] - step[2]);
    }
    return value;
}

float TileableGradientDetail(UINT x, UINT y, UINT z)
{
    const float tileFraction[3] = {(x + 0.5f) / kAuthoredNoiseSize, (y + 0.5f) / kAuthoredNoiseSize,
                                   (z + 0.5f) / kAuthoredNoiseSize};
    float value = 0.0f;
    float amplitude = 1.0f;
    for (int layer = 0; layer < kAuthoredNoiseDetailLayers; ++layer)
    {
        const uint32_t period = kAuthoredNoiseBaseCells << layer;
        const float position[3] = {tileFraction[0] * period, tileFraction[1] * period, tileFraction[2] * period};
        value += amplitude * TileableGradientNoise(position, period, static_cast<uint32_t>(layer));
        amplitude *= kAuthoredNoiseDetailGain;
    }
    return value;
}

std::vector<BYTE> QuantizedAroundMedian(const std::vector<float>& values)
{
    std::vector<float> sorted = values;
    std::nth_element(sorted.begin(), sorted.begin() + sorted.size() / 2, sorted.end());
    const float median = sorted[sorted.size() / 2];
    float extent = 0.0f;
    for (float value : values)
        extent = std::max(extent, std::fabs(value - median));
    std::vector<BYTE> bytes(values.size());
    for (size_t i = 0; i < values.size(); ++i)
    {
        const float centred = extent > 0.0f ? (values[i] - median) / extent : 0.0f;
        bytes[i] = static_cast<BYTE>(std::clamp(std::lround(kByteCentre + kByteCentre * centred), 0L, 255L));
    }
    return bytes;
}

const std::vector<BYTE>& AuthoredNoiseTexels()
{
    static const std::vector<BYTE> texels = [] {
        std::vector<float> values(kAuthoredNoiseSize * kAuthoredNoiseSize * kAuthoredNoiseSize);
        for (UINT z = 0; z < kAuthoredNoiseSize; ++z)
            for (UINT y = 0; y < kAuthoredNoiseSize; ++y)
                for (UINT x = 0; x < kAuthoredNoiseSize; ++x)
                    values[x + kAuthoredNoiseSize * (y + kAuthoredNoiseSize * z)] = TileableGradientDetail(x, y, z);
        return QuantizedAroundMedian(values);
    }();
    return texels;
}

bool CreateManagedVolume(IDirect3DDevice9* device, UINT size, D3DFORMAT format, IDirect3DVolumeTexture9** texture)
{
    return SUCCEEDED(device->CreateVolumeTexture(size, size, size, 1, 0, format, D3DPOOL_MANAGED, texture, nullptr));
}

template <typename Texel>
bool FillVolume(IDirect3DVolumeTexture9* texture, UINT size, Texel texel)
{
    D3DLOCKED_BOX locked = {};
    if (FAILED(texture->LockBox(0, &locked, nullptr, 0)))
        return false;
    for (UINT z = 0; z < size; ++z)
        for (UINT y = 0; y < size; ++y)
        {
            BYTE* row = static_cast<BYTE*>(locked.pBits) + z * locked.SlicePitch + y * locked.RowPitch;
            for (UINT x = 0; x < size; ++x)
                texel(row, x, y, z);
        }
    return SUCCEEDED(texture->UnlockBox(0));
}

void WriteGreyArgb(BYTE* row, UINT x, BYTE value)
{
    reinterpret_cast<DWORD*>(row)[x] = 0xFF000000u | (value * 0x00010101u);
}
}

bool CreateDensityNoise(IDirect3DDevice9* device, IDirect3DVolumeTexture9** output)
{
    if (!device || !output || *output)
        return false;
    IDirect3DVolumeTexture9* texture = nullptr;
    if (!CreateManagedVolume(device, kDensityNoiseSize, D3DFMT_A8R8G8B8, &texture))
        return false;
    if (!FillVolume(texture, kDensityNoiseSize,
                    [](BYTE* row, UINT x, UINT y, UINT z) { WriteGreyArgb(row, x, DensityNoiseValue(x, y, z)); }))
    {
        texture->Release();
        return false;
    }
    *output = texture;
    return true;
}

BYTE AuthoredNoiseTexel(UINT x, UINT y, UINT z)
{
    const UINT size = kAuthoredNoiseSize;
    return AuthoredNoiseTexels()[x % size + size * (y % size + size * (z % size))];
}

bool CreateAuthoredNoise(IDirect3DDevice9* device, IDirect3DVolumeTexture9** output)
{
    if (!device || !output || *output)
        return false;
    IDirect3DVolumeTexture9* texture = nullptr;
    bool filled = false;
    if (CreateManagedVolume(device, kAuthoredNoiseSize, D3DFMT_L8, &texture))
        filled = FillVolume(texture, kAuthoredNoiseSize,
                            [](BYTE* row, UINT x, UINT y, UINT z) { row[x] = AuthoredNoiseTexel(x, y, z); });
    else if (CreateManagedVolume(device, kAuthoredNoiseSize, D3DFMT_A8R8G8B8, &texture))
        filled = FillVolume(texture, kAuthoredNoiseSize, [](BYTE* row, UINT x, UINT y, UINT z) {
            WriteGreyArgb(row, x, AuthoredNoiseTexel(x, y, z));
        });
    if (!filled)
    {
        if (texture)
            texture->Release();
        return false;
    }
    *output = texture;
    return true;
}
