#include "noise_volume.h"

#include <cstdint>

namespace
{
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
}

bool CreateDensityNoise(IDirect3DDevice9* device, IDirect3DVolumeTexture9** output)
{
    if (!device || !output || *output)
        return false;
    IDirect3DVolumeTexture9* texture = nullptr;
    if (FAILED(device->CreateVolumeTexture(kDensityNoiseSize, kDensityNoiseSize, kDensityNoiseSize, 1, 0,
                                            D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture, nullptr)))
        return false;
    D3DLOCKED_BOX locked = {};
    if (FAILED(texture->LockBox(0, &locked, nullptr, 0)))
    {
        texture->Release();
        return false;
    }
    for (UINT z = 0; z < kDensityNoiseSize; ++z)
    {
        for (UINT y = 0; y < kDensityNoiseSize; ++y)
        {
            auto* row = reinterpret_cast<DWORD*>(static_cast<BYTE*>(locked.pBits) +
                                                 z * locked.SlicePitch + y * locked.RowPitch);
            for (UINT x = 0; x < kDensityNoiseSize; ++x)
                row[x] = 0xFF000000u | (DensityNoiseValue(x, y, z) * 0x00010101u);
        }
    }
    if (FAILED(texture->UnlockBox(0)))
    {
        texture->Release();
        return false;
    }
    *output = texture;
    return true;
}
