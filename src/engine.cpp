#include "engine.h"

#include "log.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace engine
{
namespace
{
constexpr uintptr_t kClientImageBase = 0x00400000;

constexpr uintptr_t kGxDevice = 0x00C5DF88;
constexpr uintptr_t kGxD3DDevice = 0x397C;
constexpr uintptr_t kGxProjection = 0xF88;
constexpr uintptr_t kGxViewportDepthRange = 0xF80;
constexpr uintptr_t kGxViewIndex = 0x1AF8;
constexpr uintptr_t kGxViewBase = 0x1B00;
constexpr uintptr_t kGxViewStride = 64;
constexpr uint32_t kGxViewStackDepth = 64;
constexpr float kMinWorldDepthRangeSpan = 0.01f;

constexpr uintptr_t kFfxCVar = 0x00D45774;
constexpr uintptr_t kCurrentScreenEffect = 0x00D45780;
constexpr uintptr_t kGlowScreenEffect = 0x00B74364;
constexpr uintptr_t kScreenEffectEnableCVar = 0x4;
constexpr uintptr_t kCVarIntValue = 0x30;
constexpr uintptr_t kDayNightGlow = 0x00D38C2C;

constexpr uintptr_t kViewGlobal = 0x00ADF5E8;
constexpr uintptr_t kProjectionGlobal = 0x00ADF628;
constexpr uintptr_t kCameraPosition = 0x00CD8F5C;
constexpr uintptr_t kCameraTarget = 0x00CD8F68;
constexpr uintptr_t kCameraInLiquid = 0x00CD8794;
constexpr uintptr_t kCurrentMap = 0x00AB63BC;
constexpr uintptr_t kWorldFrame = 0x00B7436C;
constexpr uintptr_t kWorldFrameFarClip = 0xB14;

constexpr uintptr_t kFogGroupStart[kDayNightFogGroupCount] = {0x00D38B90, 0x00D38BA4};
constexpr uintptr_t kFogGroupEnd[kDayNightFogGroupCount] = {0x00D38B94, 0x00D38BA8};

constexpr uintptr_t kDayFraction = 0x00D38B04;
constexpr uintptr_t kSkyCenter = 0x00D38B18;
constexpr uintptr_t kFogColor = 0x00D38BA0;
constexpr uintptr_t kAmbientColor = 0x00D38BD4;
constexpr uintptr_t kDirectColor = 0x00D38BD8;
constexpr uintptr_t kSunColor = 0x00D38BF8;
constexpr uintptr_t kZoneFogDistance = 0x00D38C1C;
constexpr uintptr_t kSunPosition = 0x00D38E28;
constexpr uintptr_t kMoonPosition = 0x00D38E48;
constexpr uintptr_t kSunDayEnd = 0x00A41CA0;
constexpr uintptr_t kSunDayStart = 0x00A41CA4;
constexpr uintptr_t kDayNightScreenEffectLightSlot = 0x00D38B58;
constexpr uintptr_t kDayNightStormBlend = 0x00D38B88;

struct OpaqueState
{
    bool valid;
    D3DVIEWPORT9 viewport;
    float cameraRelativeView[16];
    float glProjection[16];
};

OpaqueState g_opaque = {};

template <typename T>
T Read(uintptr_t address)
{
    T value;
    std::memcpy(&value, reinterpret_cast<const void*>(address), sizeof(T));
    return value;
}

void ReadFloats(uintptr_t address, float* out, int count)
{
    std::memcpy(out, reinterpret_cast<const void*>(address), sizeof(float) * count);
}

bool Finite(const float* v, int count)
{
    for (int i = 0; i < count; ++i)
        if (!std::isfinite(v[i]))
            return false;
    return true;
}

bool IsPerspective(const float* p)
{
    return Finite(p, 16) && std::fabs(p[11] - 1.0f) < 1e-3f && std::fabs(p[15]) < 1e-3f &&
           std::fabs(p[0]) > 1e-4f && std::fabs(p[5]) > 1e-4f;
}

bool ReadGxMatrices(float* view, float* proj)
{
    uintptr_t gx = Read<uintptr_t>(kGxDevice);
    if (!gx)
        return false;
    uint32_t index = Read<uint32_t>(gx + kGxViewIndex);
    if (index >= kGxViewStackDepth)
        return false;
    ReadFloats(gx + kGxViewBase + index * kGxViewStride, view, 16);
    ReadFloats(gx + kGxProjection, proj, 16);
    return Finite(view, 16) && IsPerspective(proj);
}

bool IsWorldDepthRange(const float* minMaxZ)
{
    return Finite(minMaxZ, 2) && minMaxZ[0] >= 0.0f && minMaxZ[1] <= 1.0f &&
           minMaxZ[1] - minMaxZ[0] > kMinWorldDepthRangeSpan;
}

void ApplyPendingGxViewportDepthRange(D3DVIEWPORT9& viewport)
{
    uintptr_t gx = Read<uintptr_t>(kGxDevice);
    if (!gx)
        return;
    float minMaxZ[2];
    ReadFloats(gx + kGxViewportDepthRange, minMaxZ, 2);
    if (IsWorldDepthRange(minMaxZ))
    {
        viewport.MinZ = minMaxZ[0];
        viewport.MaxZ = minMaxZ[1];
    }
}

bool CaptureOpaqueStateUnsafe(IDirect3DDevice9* device, OpaqueState& state)
{
    if (FAILED(device->GetViewport(&state.viewport)))
        return false;
    ApplyPendingGxViewportDepthRange(state.viewport);
    if (!ReadGxMatrices(state.cameraRelativeView, state.glProjection))
    {
        ReadFloats(kViewGlobal, state.cameraRelativeView, 16);
        ReadFloats(kProjectionGlobal, state.glProjection, 16);
    }
    return state.viewport.Width > 0 && state.viewport.Height > 0 && IsPerspective(state.glProjection) &&
           Finite(state.cameraRelativeView, 16);
}

void Normalize(float* v)
{
    float len = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (len < 1e-6f)
    {
        v[0] = 0.0f;
        v[1] = 0.0f;
        v[2] = 1.0f;
        return;
    }
    v[0] /= len;
    v[1] /= len;
    v[2] /= len;
}

bool CVarEnabled(uintptr_t cvar)
{
    return cvar && Read<int32_t>(cvar + kCVarIntValue) != 0;
}

float GlowScreenEffectAmount()
{
    uintptr_t effect = Read<uintptr_t>(kCurrentScreenEffect);
    if (!CVarEnabled(Read<uintptr_t>(kFfxCVar)) || !effect || effect != Read<uintptr_t>(kGlowScreenEffect))
        return 0.0f;
    float glow = Read<float>(kDayNightGlow);
    if (!CVarEnabled(Read<uintptr_t>(effect + kScreenEffectEnableCVar)) || !std::isfinite(glow))
        return 0.0f;
    return std::clamp(glow, 0.0f, 1.0f);
}

LightParamsSelection ReadLightParamsSelection()
{
    LightParamsSelection selection;
    const float storm = Read<float>(kDayNightStormBlend);
    selection.stormBlend = std::isfinite(storm) ? std::clamp(storm, 0.0f, 1.0f) : 0.0f;
    const int32_t slot = Read<int32_t>(kDayNightScreenEffectLightSlot);
    if (slot >= 0 && slot < FogData::kLightParamsSlots)
        selection.screenEffectSlot = slot;
    return selection;
}

bool BuildFrameInputsUnsafe(FrameInputs& out)
{
    std::memcpy(out.cameraRelativeView, g_opaque.cameraRelativeView, sizeof(out.cameraRelativeView));
    std::memcpy(out.glProjection, g_opaque.glProjection, sizeof(out.glProjection));
    out.viewport = g_opaque.viewport;
    ReadFloats(kCameraPosition, out.camPos, 3);
    ReadFloats(kCameraTarget, out.camTarget, 3);

    out.dayFraction = Read<float>(kDayFraction);
    float dayStart = Read<float>(kSunDayStart);
    float dayEnd = Read<float>(kSunDayEnd);
    out.lightIsMoon = !(out.dayFraction >= dayStart && out.dayFraction <= dayEnd);
    float center[3];
    float body[3];
    ReadFloats(kSkyCenter, center, 3);
    ReadFloats(out.lightIsMoon ? kMoonPosition : kSunPosition, body, 3);
    out.toLight[0] = body[0] - center[0];
    out.toLight[1] = body[1] - center[1];
    out.toLight[2] = body[2] - center[2];
    Normalize(out.toLight);

    out.fogColor = Read<uint32_t>(kFogColor);
    out.fogStart = Read<float>(kFogGroupStart[kFrameInputsFogGroup]);
    out.fogEnd = Read<float>(kFogGroupEnd[kFrameInputsFogGroup]);
    out.sunColor = Read<uint32_t>(kSunColor);
    out.directColor = Read<uint32_t>(kDirectColor);
    out.ambientColor = Read<uint32_t>(kAmbientColor);
    out.inLiquid = CameraInLiquid();
    out.mapId = Read<int32_t>(kCurrentMap);
    out.lightParams = ReadLightParamsSelection();
    CaptureLocalLightInputs(out.camPos, out.localLights);

    out.zoneFogDistance = Read<float>(kZoneFogDistance);
    out.clientGlowAmount = out.inLiquid ? 0.0f : GlowScreenEffectAmount();
    out.farClip = out.glProjection[14] / (1.0f - out.glProjection[10]);
    if (!(out.farClip > 10.0f && out.farClip < 100000.0f))
    {
        uintptr_t worldFrame = Read<uintptr_t>(kWorldFrame);
        out.farClip = worldFrame ? Read<float>(worldFrame + kWorldFrameFarClip) : 0.0f;
    }
    if (!(out.farClip > 10.0f && out.farClip < 100000.0f))
        out.farClip = 1000.0f;

    return Finite(out.camPos, 3) && Finite(out.toLight, 3) && std::isfinite(out.fogEnd) &&
           std::isfinite(out.fogStart);
}
}

bool IsSupportedClient()
{
    auto* base = reinterpret_cast<const unsigned char*>(GetModuleHandleA(nullptr));
    auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    return nt->FileHeader.TimeDateStamp == kClientTimestamp && nt->OptionalHeader.ImageBase == kClientImageBase;
}

void* GameD3DDevice()
{
    __try
    {
        uintptr_t gx = Read<uintptr_t>(kGxDevice);
        return gx ? Read<void*>(gx + kGxD3DDevice) : nullptr;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return nullptr;
    }
}

bool CameraInLiquid()
{
    return Read<uint32_t>(kCameraInLiquid) != 0;
}

StockFog ReadStockFog()
{
    StockFog fog;
    for (int group = 0; group < kDayNightFogGroupCount; ++group)
    {
        fog.start[group] = Read<float>(kFogGroupStart[group]);
        fog.end[group] = Read<float>(kFogGroupEnd[group]);
    }
    return fog;
}

void WriteStockFog(const StockFog& fog)
{
    for (int group = 0; group < kDayNightFogGroupCount; ++group)
    {
        *reinterpret_cast<float*>(kFogGroupStart[group]) = fog.start[group];
        *reinterpret_cast<float*>(kFogGroupEnd[group]) = fog.end[group];
    }
}

void CaptureOpaqueState(IDirect3DDevice9* device)
{
    OpaqueState state = {};
    bool ok = false;
    __try
    {
        ok = CaptureOpaqueStateUnsafe(device, state);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        ok = false;
    }
    state.valid = ok;
    g_opaque = state;
}

bool HasOpaqueState()
{
    return g_opaque.valid;
}

void ClearOpaqueState()
{
    g_opaque.valid = false;
}

bool BuildFrameInputs(FrameInputs& out)
{
    if (!g_opaque.valid)
        return false;
    __try
    {
        return BuildFrameInputsUnsafe(out);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}
}
