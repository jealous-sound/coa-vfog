#include "engine_shadows.h"

#include "engine.h"
#include "log.h"

#include <cmath>
#include <cstdint>
#include <cstring>

namespace
{
constexpr uintptr_t kShadowQuality = 0x00D43154;
constexpr uintptr_t kShadowResourcesDirty = 0x00B1D51C;
constexpr uintptr_t kHardwareShadowComparison = 0x00D43014;
constexpr uintptr_t kShadowLightDirection = 0x00D43180;
constexpr uintptr_t kShadowDepthScale = 0x00D431C8;
constexpr uintptr_t kShadowViewMatrices = 0x00D43348;
constexpr uintptr_t kDynamicShadowTextures = 0x00D43250;
constexpr uintptr_t kEnvironmentShadowTextures = 0x00D43290;
constexpr uintptr_t kEnvironmentShadowTextureIndex = 0x00D432C8;
constexpr uintptr_t kEnvironmentShadowStride = 0x3C;
constexpr uintptr_t kTextureResourceFlags = 0x28;
constexpr uintptr_t kTextureResourceGxTexture = 0x44;
constexpr uintptr_t kTextureResourceCache = 0x5C;
constexpr uintptr_t kTextureCacheGxTexture = 0x18;
constexpr uintptr_t kGxTextureFormat = 0x24;
constexpr uintptr_t kGxTextureD3DTexture = 0x38;
constexpr uint32_t kGxShadowFloatFormat = 11;
constexpr uint32_t kGxShadowDepthFormat = 12;
constexpr float kShadowDepthRange = 4000.0f;

template <typename T>
T Read(uintptr_t address)
{
    T value;
    std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(value));
    return value;
}

template <size_t Size>
bool Matches(uintptr_t address, const unsigned char (&bytes)[Size])
{
    return std::memcmp(reinterpret_cast<const void*>(address), bytes, Size) == 0;
}

bool SupportedShadowContract()
{
    static constexpr unsigned char textureBind[] = {0x8B, 0x53, 0x38};
    static constexpr unsigned char resourceTexture[] = {0x8B, 0x46, 0x44};
    static constexpr unsigned char matrixUpload[] = {0x6A, 0x0C, 0x68, 0x48, 0x33, 0xD4, 0x00,
                                                      0x68, 0xE0, 0x00, 0x00, 0x00, 0x6A, 0x00};
    static constexpr unsigned char matrixStore[] = {0xD9, 0x1D, 0x48, 0x33, 0xD4, 0x00};
    static constexpr unsigned char depthScaleLoad[] = {0xD9, 0x05, 0x00, 0x01, 0xA4, 0x00};
    return engine::IsSupportedClient() && Matches(0x006A492E, textureBind) &&
           Matches(0x004B6D7A, resourceTexture) && Matches(0x0087450A, matrixUpload) &&
           Matches(0x0087526B, matrixStore) && Matches(0x007BC105, depthScaleLoad);
}

bool FiniteMatrix(const float* matrix)
{
    for (int i = 0; i < 12; ++i)
        if (!std::isfinite(matrix[i]))
            return false;
    for (int row = 0; row < 3; ++row)
    {
        const float* axis = matrix + row * 4;
        const float lengthSquared = axis[0] * axis[0] + axis[1] * axis[1] + axis[2] * axis[2];
        if (!(lengthSquared > 1e-12f && lengthSquared < 4.0f))
            return false;
    }
    return true;
}

uintptr_t GxTexture(uintptr_t resource)
{
    if (!resource)
        return 0;
    if ((Read<uint32_t>(resource + kTextureResourceFlags) & 4u) != 0)
    {
        const uintptr_t cache = Read<uintptr_t>(resource + kTextureResourceCache);
        if (cache)
            return Read<uintptr_t>(cache + kTextureCacheGxTexture);
    }
    return Read<uintptr_t>(resource + kTextureResourceGxTexture);
}

bool AcquireTexture(IDirect3DDevice9* device, uintptr_t resource, int index, WorldShadowInputs& out)
{
    const uintptr_t gxTexture = GxTexture(resource);
    const uint32_t expectedFormat = out.hardwareComparison ? kGxShadowDepthFormat : kGxShadowFloatFormat;
    if (!gxTexture || Read<uint32_t>(gxTexture + kGxTextureFormat) != expectedFormat)
        return false;
    auto* texture = Read<IDirect3DTexture9*>(gxTexture + kGxTextureD3DTexture);
    if (!texture || texture->GetType() != D3DRTYPE_TEXTURE)
        return false;
    D3DSURFACE_DESC desc = {};
    const D3DFORMAT expectedD3DFormat = out.hardwareComparison ? D3DFMT_D24X8 : D3DFMT_R32F;
    if (FAILED(texture->GetLevelDesc(0, &desc)) || desc.Format != expectedD3DFormat ||
        desc.Width < 256 || desc.Height < 256 || desc.Width > 4096 || desc.Height > 4096)
        return false;
    IDirect3DDevice9* owner = nullptr;
    if (FAILED(texture->GetDevice(&owner)) || !owner)
        return false;
    const bool sameDevice = owner == device;
    owner->Release();
    if (!sameDevice)
        return false;
    texture->AddRef();
    out.textures[index] = texture;
    out.texelSize[index][0] = 1.0f / static_cast<float>(desc.Width);
    out.texelSize[index][1] = 1.0f / static_cast<float>(desc.Height);
    return true;
}

bool AcquireWorldShadowsUnsafe(IDirect3DDevice9* device, WorldShadowInputs& out)
{
    if (!device || !SupportedShadowContract() || Read<uint32_t>(kShadowResourcesDirty) != 0)
        return false;
    const int quality = Read<int>(kShadowQuality);
    const uint32_t hardware = Read<uint32_t>(kHardwareShadowComparison);
    const float depthScale = Read<float>(kShadowDepthScale);
    if (quality < 1 || quality > 5 || hardware > 1 || !std::isfinite(depthScale) ||
        std::fabs(depthScale * kShadowDepthRange - 1.0f) > 1e-4f)
        return false;
    out.hardwareComparison = hardware != 0;
    out.count = quality > 2 ? kWorldShadowMapCount : 1;
    std::memcpy(out.viewToShadow, reinterpret_cast<const void*>(kShadowViewMatrices),
                sizeof(out.viewToShadow[0]) * out.count);
    std::memcpy(out.toLight, reinterpret_cast<const void*>(kShadowLightDirection), sizeof(out.toLight));
    float lightLengthSquared = 0.0f;
    for (float component : out.toLight)
    {
        if (!std::isfinite(component))
            return false;
        lightLengthSquared += component * component;
    }
    if (!(lightLengthSquared > 0.5f && lightLengthSquared < 1.5f))
        return false;
    const float inverseLightLength = -1.0f / std::sqrt(lightLengthSquared);
    for (float& component : out.toLight)
        component *= inverseLightLength;
    for (int i = 0; i < out.count; ++i)
    {
        if (!FiniteMatrix(out.viewToShadow[i]))
            return false;
        uintptr_t resourceAddress = kDynamicShadowTextures + hardware * sizeof(uintptr_t);
        if (i > 0)
        {
            const uintptr_t offset = (i - 1) * kEnvironmentShadowStride;
            const uint32_t buffer = Read<uint32_t>(kEnvironmentShadowTextureIndex + offset);
            if (buffer > 1)
                return false;
            resourceAddress = kEnvironmentShadowTextures + offset + buffer * sizeof(uintptr_t);
        }
        if (!AcquireTexture(device, Read<uintptr_t>(resourceAddress), i, out))
            return false;
    }
    return true;
}
}

namespace engine
{
bool AcquireWorldShadows(IDirect3DDevice9* device, WorldShadowInputs& out)
{
    ReleaseWorldShadows(out);
    bool acquired = false;
    __try
    {
        acquired = AcquireWorldShadowsUnsafe(device, out);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        acquired = false;
    }
    if (!acquired)
        ReleaseWorldShadows(out);
    static int lastLoggedMode = -1;
    const int mode = out.count | (out.hardwareComparison ? 8 : 0);
    if (mode != lastLoggedMode)
    {
        if (out.count > 0)
            VF_LOG_INFO("world shadows: %d native maps, %s, %u px, light (%.3f %.3f %.3f)", out.count,
                        out.hardwareComparison ? "D24X8 comparison" : "R32F depth",
                        static_cast<unsigned>(std::lround(1.0f / out.texelSize[0][0])),
                        out.toLight[0], out.toLight[1], out.toLight[2]);
        else
            VF_LOG_INFO("world shadows: native maps unavailable; screen-space visibility retained");
        lastLoggedMode = mode;
    }
    return acquired;
}

void ReleaseWorldShadows(WorldShadowInputs& in)
{
    for (IDirect3DTexture9* texture : in.textures)
        if (texture)
            texture->Release();
    in = {};
}
}
