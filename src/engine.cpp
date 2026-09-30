#include "engine.h"

#include "log.h"
#include "water_classify.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
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

constexpr uintptr_t kLightSkyColors[kSkyColorCount] = {0x00D38BE0, 0x00D38BE4, 0x00D38BE8,
                                                       0x00D38BEC, 0x00D38BF0, 0x00D38BF4};
constexpr uintptr_t kLightRiverColors[kWaterColorPair] = {0x00D38C14, 0x00D38C18};
constexpr uintptr_t kLightOceanColors[kWaterColorPair] = {0x00D38C0C, 0x00D38C10};

constexpr uintptr_t kLiquidSettingsBankCount = 0x00D43B18;
constexpr uintptr_t kLiquidSettingsBankEntries = 0x00D43B1C;
constexpr uint32_t kMaxLiquidSettingsBankCount = 0x10000;
constexpr uintptr_t kLiquidSettingsTexture0 = 0x000;
constexpr size_t kLiquidSettingsTextureSlot = 0x80;
constexpr uintptr_t kLiquidTypeMaxId = 0x00AD4070;
constexpr uintptr_t kLiquidTypeMinId = 0x00AD4074;
constexpr uintptr_t kLiquidTypeRows = 0x00AD4084;
constexpr uintptr_t kLiquidTypeFlags = 0x08;
constexpr uintptr_t kLiquidTypeSoundBank = 0x0C;
constexpr uintptr_t kLiquidTypeMaterialId = 0x38;
constexpr uintptr_t kLiquidBucketStride = 0x10;
constexpr uintptr_t kLiquidBucketCount = 0x04;
constexpr uintptr_t kTransparentLiquidPass = 1;
constexpr int kClassifiedSettingsCacheSize = 32;
constexpr int kMaxLoggedLiquidTypes = 64;

constexpr size_t kMaxCodeBytes = 16;

struct CodeBytes
{
    const char* name;
    uintptr_t address;
    size_t size;
    unsigned char bytes[kMaxCodeBytes];
};

const CodeBytes kLightRecordStores[] = {
    {"light record copy destination", 0x007F3574, 5, {0xB9, 0xD4, 0x8B, 0xD3, 0x00}},
    {"sky top colour store", 0x007EC03C, 3, {0x89, 0x56, 0x0C}},
    {"sky middle colour store", 0x007EC051, 3, {0x89, 0x4E, 0x10}},
    {"sky band 1 colour store", 0x007EC063, 3, {0x89, 0x46, 0x14}},
    {"sky band 2 colour store", 0x007EC075, 3, {0x89, 0x56, 0x18}},
    {"sky smog colour store", 0x007EC087, 3, {0x89, 0x4E, 0x1C}},
    {"fog colour store", 0x007EC09C, 3, {0x89, 0x46, 0x20}},
    {"ocean close colour store", 0x007EC11D, 3, {0x89, 0x56, 0x38}},
    {"ocean far colour store", 0x007EC132, 3, {0x89, 0x4E, 0x3C}},
    {"river close colour store", 0x007EC144, 3, {0x89, 0x46, 0x40}},
    {"river far colour store", 0x007EC152, 3, {0x89, 0x56, 0x44}},
};

const CodeBytes kWaterClientLayout[] = {
    {"settings bank count check", 0x008A28F8, 6, {0x3B, 0x3D, 0x18, 0x3B, 0xD4, 0x00}},
    {"settings bank array load", 0x008A2900, 5, {0xA1, 0x1C, 0x3B, 0xD4, 0x00}},
    {"settings bank entry load", 0x008A2952, 3, {0x8B, 0x04, 0xB8}},
    {"settings texture copy destination", 0x008A2809, 3, {0x89, 0x5D, 0xFC}},
    {"LiquidType texture source", 0x008A280C, 3, {0x8D, 0x47, 0x3C}},
    {"settings texture slot stride", 0x008A282E, 7, {0x81, 0x45, 0xFC, 0x80, 0x00, 0x00, 0x00}},
    {"LiquidType minimum id load", 0x00793DF7, 5, {0xA1, 0x74, 0x40, 0xAD, 0x00}},
    {"LiquidType maximum id check", 0x00793E00, 6, {0x3B, 0x35, 0x70, 0x40, 0xAD, 0x00}},
    {"LiquidType rows load", 0x00793E08, 6, {0x8B, 0x15, 0x84, 0x40, 0xAD, 0x00}},
    {"LiquidType row load", 0x00793E12, 3, {0x8B, 0x04, 0x8A}},
    {"LiquidType material id read", 0x008A1FE8, 3, {0x8B, 0x78, 0x38}},
    {"material render call with the settings argument", 0x008A22C7, 11,
     {0x8B, 0x46, 0x04, 0x8B, 0x0E, 0x8B, 0x11, 0x8B, 0x52, 0x08, 0x50}},
    {"liquid render pass return", 0x008A2376, 3, {0xC2, 0x08, 0x00}},
    {"liquid renderer load at the water pass", 0x00790A91, 6, {0x8B, 0x0D, 0x10, 0x86, 0xCD, 0x00}},
    {"transparent liquid pass index", 0x00790A9B, 2, {0x6A, 0x01}},
    {"liquid renderer kept in ebx", 0x008A224C, 2, {0x8B, 0xD9}},
    {"liquid bucket stride", 0x008A229C, 3, {0xC1, 0xE7, 0x04}},
    {"liquid bucket count load", 0x008A229F, 4, {0x8B, 0x4C, 0x1F, 0x04}},
    {"liquid draw loop skipped on an empty bucket", 0x008A22B2, 15,
     {0x8B, 0x47, 0x04, 0x83, 0xC4, 0x10, 0x33, 0xDB, 0x85, 0xC0, 0x89, 0x45, 0x0C, 0x76, 0x3A}},
    {"water material render return", 0x008A58FB, 3, {0xC2, 0x1C, 0x00}},
    {"water no-specular material render return", 0x008A5C6B, 3, {0xC2, 0x1C, 0x00}},
};

const CodeBytes kTransparentFogClientLayout[] = {
    {"M2 fog exponent argument", 0x0081FCEE, 9, {0x8B, 0x46, 0x70, 0xD9, 0x80, 0xB4, 0x00, 0x00, 0x00}},
    {"M2 fog colour argument address", 0x0081FCF7, 4, {0x8D, 0x4D, 0xFC, 0x51}},
    {"M2 fog argument block", 0x0081FCFB, 7, {0x83, 0xEC, 0x0C, 0xD9, 0x5C, 0x24, 0x08}},
    {"M2 fog end argument", 0x0081FD02, 10, {0xD9, 0x80, 0xAC, 0x00, 0x00, 0x00, 0xD9, 0x5C, 0x24, 0x04}},
    {"M2 fog start argument", 0x0081FD0C, 9, {0xD9, 0x80, 0xA8, 0x00, 0x00, 0x00, 0xD9, 0x1C, 0x24}},
    {"M2 fog upload and argument pop", 0x0081FD1A, 10, {0x6A, 0x01, 0xE8, 0x6F, 0x36, 0x05, 0x00, 0x83, 0xC4, 0x14}},
    {"M2 fog setter prologue", 0x00873210, 13,
     {0x55, 0x8B, 0xEC, 0x83, 0xEC, 0x14, 0x83, 0x3D, 0x20, 0x30, 0xD4, 0x00, 0x00}},
    {"fog colour red byte read", 0x00873225, 7, {0x8B, 0x75, 0x14, 0x0F, 0xB6, 0x46, 0x02}},
    {"fog colour green byte read", 0x00873242, 4, {0x0F, 0xB6, 0x4E, 0x01}},
    {"fog colour blue byte read", 0x00873254, 3, {0x0F, 0xB6, 0x16}},
    {"fog start and end read", 0x00873263, 8, {0xD9, 0x45, 0x0C, 0xD9, 0x45, 0x08, 0xD8, 0xE9}},
    {"fog exponent read", 0x0087328C, 3, {0xD9, 0x45, 0x10}},
    {"blend mode fog table read", 0x0081FB7B, 7, {0x8B, 0x04, 0x8D, 0x90, 0x53, 0xA4, 0x00}},
    {"blend mode fog table", 0x00A45390, 16,
     {0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00}},
    {"blend mode fog table end", 0x00A453A0, 16,
     {0x02, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {"fog colour mode dispatch", 0x0081FBA3, 7, {0xFF, 0x24, 0x85, 0x7C, 0xFE, 0x81, 0x00}},
    {"fog colour mode jump table", 0x0081FE7C, 16,
     {0xAA, 0xFB, 0x81, 0x00, 0xAE, 0xFC, 0x81, 0x00, 0xC2, 0xFC, 0x81, 0x00, 0xD8, 0xFC, 0x81, 0x00}},
    {"zero fog colour alpha register", 0x0081FB20, 2, {0x33, 0xDB}},
    {"lighting fog colour alpha", 0x0081FC5D, 4, {0xC6, 0x45, 0xFB, 0xFF}},
    {"black fog colour alpha", 0x0081FCB7, 3, {0x88, 0x5D, 0xFB}},
    {"white fog colour alpha", 0x0081FCCD, 3, {0x88, 0x5D, 0xFB}},
    {"grey fog colour alpha", 0x0081FCE3, 3, {0x88, 0x5D, 0xFB}},
    {"glare call argument pop before", 0x004F9210, 3, {0x83, 0xC4, 0x14}},
    {"world text call after the glare", 0x004F9218, 5, {0xE8, 0x63, 0xC3, 0x2E, 0x00}},
    {"glare pass visibility test", 0x007F0877, 14,
     {0x74, 0x3C, 0xD9, 0x05, 0x48, 0x8B, 0xD3, 0x00, 0x51, 0xB9, 0xA8, 0x8E, 0xD3, 0x00}},
    {"glare pass moon tail jump", 0x007F08AB, 10, {0xB9, 0x58, 0x8F, 0xD3, 0x00, 0xE9, 0x4B, 0xBB, 0x1B, 0x00}},
    {"glare pass return", 0x007F08B5, 1, {0xC3}},
};

constexpr size_t kGlarePassEntrySize = 7;
constexpr unsigned char kGlarePassEntry[kGlarePassEntrySize] = {0x83, 0x3D, 0xCC, 0x8C, 0xD3, 0x00, 0x00};
constexpr unsigned char kJumpRel32Opcode = 0xE9;
constexpr size_t kJumpRel32Size = 5;
constexpr unsigned char kMoveEaxImm32Opcode = 0xB8;
constexpr unsigned char kJumpEax[] = {0xFF, 0xE0};

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

bool BuildFrameInputsUnsafe(FrameInputs& out, bool withPointLights)
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
    CaptureLocalLightInputs(out.camPos, withPointLights, out.localLights);

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

bool CodeBytesMatch(const CodeBytes& code)
{
    __try
    {
        return std::memcmp(reinterpret_cast<const void*>(code.address), code.bytes, code.size) == 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool CopyCode(uintptr_t address, unsigned char* out, size_t size)
{
    __try
    {
        std::memcpy(out, reinterpret_cast<const void*>(address), size);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

template <size_t N>
bool AllCodeBytesMatch(const CodeBytes (&codes)[N], const char* what)
{
    for (const CodeBytes& code : codes)
        if (!CodeBytesMatch(code))
        {
            VF_LOG_ERROR("%s: the client's %s at 0x%08X differs from the 12340 client", what, code.name,
                         static_cast<unsigned>(code.address));
            return false;
        }
    return true;
}

enum class LayoutCheck
{
    Unchecked,
    Matches,
    Differs,
};

LayoutCheck g_lightRecordLayout = LayoutCheck::Unchecked;

bool LightRecordLayoutMatches()
{
    if (g_lightRecordLayout == LayoutCheck::Unchecked)
        g_lightRecordLayout = AllCodeBytesMatch(kLightRecordStores, "water light colours") ? LayoutCheck::Matches
                                                                                             : LayoutCheck::Differs;
    return g_lightRecordLayout == LayoutCheck::Matches;
}

void ReadColors(const uintptr_t* addresses, uint32_t* out, int count)
{
    for (int i = 0; i < count; ++i)
        out[i] = Read<uint32_t>(addresses[i]);
}

bool ReadWaterColors(WaterInputs& out)
{
    __try
    {
        ReadColors(kLightSkyColors, out.skyColors, kSkyColorCount);
        ReadColors(kLightRiverColors, out.riverColors, kWaterColorPair);
        ReadColors(kLightOceanColors, out.oceanColors, kWaterColorPair);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

struct SettingsBank
{
    uintptr_t entries;
    uint32_t count;
};

struct LiquidDescription
{
    bool inBank;
    bool hasRow;
    uint32_t id;
    uint32_t flags;
    uint32_t soundBank;
    uint32_t materialId;
    char texture0[kLiquidSettingsTextureSlot];
};

struct ClassifiedSettings
{
    const void* settings;
    WaterClass waterClass;
};

struct ClassifiedSettingsCache
{
    SettingsBank bank;
    int count;
    int next;
    ClassifiedSettings entries[kClassifiedSettingsCacheSize];
};

ClassifiedSettingsCache g_classified = {};
uint32_t g_loggedLiquidTypes[kMaxLoggedLiquidTypes] = {};
int g_loggedLiquidTypeCount = 0;
bool g_loggedUnreadableSettings = false;

bool ReadSettingsBank(SettingsBank& bank)
{
    __try
    {
        bank.count = Read<uint32_t>(kLiquidSettingsBankCount);
        bank.entries = Read<uintptr_t>(kLiquidSettingsBankEntries);
        return bank.entries && bank.count <= kMaxLiquidSettingsBankCount;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool FindLiquidTypeId(const SettingsBank& bank, const void* settings, uint32_t& id)
{
    for (uint32_t i = 0; i < bank.count; ++i)
        if (Read<uintptr_t>(bank.entries + i * sizeof(uintptr_t)) == reinterpret_cast<uintptr_t>(settings))
        {
            id = i;
            return true;
        }
    return false;
}

uintptr_t LiquidTypeRow(uint32_t id)
{
    const int32_t minId = Read<int32_t>(kLiquidTypeMinId);
    const int32_t maxId = Read<int32_t>(kLiquidTypeMaxId);
    const uintptr_t rows = Read<uintptr_t>(kLiquidTypeRows);
    const int64_t signedId = id;
    if (!rows || signedId < minId || signedId > maxId)
        return 0;
    return Read<uintptr_t>(rows + static_cast<uintptr_t>(signedId - minId) * sizeof(uintptr_t));
}

void ReadTexture0(const void* settings, char* out)
{
    std::memcpy(out, static_cast<const char*>(settings) + kLiquidSettingsTexture0, kLiquidSettingsTextureSlot);
    if (!std::memchr(out, 0, kLiquidSettingsTextureSlot))
        out[0] = 0;
}

void DescribeLiquidUnsafe(const SettingsBank& bank, const void* settings, LiquidDescription& d)
{
    d.inBank = FindLiquidTypeId(bank, settings, d.id);
    if (!d.inBank)
        return;
    const uintptr_t row = LiquidTypeRow(d.id);
    d.hasRow = row != 0;
    if (d.hasRow)
    {
        d.flags = Read<uint32_t>(row + kLiquidTypeFlags);
        d.soundBank = Read<uint32_t>(row + kLiquidTypeSoundBank);
        d.materialId = Read<uint32_t>(row + kLiquidTypeMaterialId);
    }
    ReadTexture0(settings, d.texture0);
}

bool DescribeLiquid(const SettingsBank& bank, const void* settings, LiquidDescription& d)
{
    __try
    {
        DescribeLiquidUnsafe(bank, settings, d);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

WaterClass ClassOf(const LiquidDescription& d)
{
    if (!d.inBank || !d.hasRow)
        return WaterClass::None;
    return ClassifyLiquid(d.id, d.soundBank, d.materialId, d.texture0);
}

bool SameBank(const SettingsBank& a, const SettingsBank& b)
{
    return a.entries == b.entries && a.count == b.count;
}

void ForgetClassifiedSettings(const SettingsBank& bank)
{
    g_classified = {};
    g_classified.bank = bank;
}

const ClassifiedSettings* FindClassified(const void* settings)
{
    for (int i = 0; i < g_classified.count; ++i)
        if (g_classified.entries[i].settings == settings)
            return &g_classified.entries[i];
    return nullptr;
}

void RememberClassified(const void* settings, WaterClass waterClass)
{
    const int slot = g_classified.count < kClassifiedSettingsCacheSize ? g_classified.count++ : g_classified.next;
    g_classified.next = (slot + 1) % kClassifiedSettingsCacheSize;
    g_classified.entries[slot] = {settings, waterClass};
}

bool MarkLiquidTypeLogged(uint32_t id)
{
    for (int i = 0; i < g_loggedLiquidTypeCount; ++i)
        if (g_loggedLiquidTypes[i] == id)
            return false;
    if (g_loggedLiquidTypeCount < kMaxLoggedLiquidTypes)
        g_loggedLiquidTypes[g_loggedLiquidTypeCount++] = id;
    return true;
}

void LogNewLiquid(const void* settings, bool read, const LiquidDescription& d, WaterClass waterClass)
{
    if (!read || !d.inBank)
    {
        if (!g_loggedUnreadableSettings)
            VF_LOG_INFO("water: liquid settings %p are %s; left to the client", settings,
                        read ? "not in the client's settings bank" : "unreadable");
        g_loggedUnreadableSettings = true;
        return;
    }
    if (!MarkLiquidTypeLogged(d.id))
        return;
    if (!d.hasRow)
        VF_LOG_INFO("water: liquid type %u -> %s (no LiquidType row)", d.id, WaterClassLabel(waterClass));
    else
        VF_LOG_INFO("water: liquid type %u -> %s (sound bank %u, material %u, flags 0x%X, texture %s)", d.id,
                    WaterClassLabel(waterClass), d.soundBank, d.materialId, d.flags, d.texture0);
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

bool BuildFrameInputs(FrameInputs& out, bool withPointLights)
{
    if (!g_opaque.valid)
        return false;
    __try
    {
        return BuildFrameInputsUnsafe(out, withPointLights);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool BuildWaterInputs(WaterInputs& out)
{
    out = {};
    return LightRecordLayoutMatches() && ReadWaterColors(out);
}

bool TransparentLiquidsQueued(const void* liquidRenderer)
{
    if (!liquidRenderer)
        return false;
    __try
    {
        const uintptr_t bucket = reinterpret_cast<uintptr_t>(liquidRenderer) +
                                 kTransparentLiquidPass * kLiquidBucketStride;
        return Read<uint32_t>(bucket + kLiquidBucketCount) != 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return true;
    }
}

bool WaterClientLayoutMatches()
{
    return AllCodeBytesMatch(kWaterClientLayout, "water hooks");
}

bool TransparentFogClientLayoutMatches()
{
    return AllCodeBytesMatch(kTransparentFogClientLayout, "transparent fog hooks");
}

void DescribeGlarePassEntry(char* text, size_t size)
{
    unsigned char entry[kGlarePassEntrySize] = {};
    if (!CopyCode(kGlarePassTarget, entry, sizeof(entry)))
    {
        std::snprintf(text, size, "unreadable");
        return;
    }
    int32_t rel = 0;
    std::memcpy(&rel, entry + 1, sizeof(rel));
    uintptr_t target = 0;
    std::memcpy(&target, entry + 1, sizeof(target));
    if (std::memcmp(entry, kGlarePassEntry, sizeof(entry)) == 0)
        std::snprintf(text, size, "the 12340 code (not detoured)");
    else if (entry[0] == kJumpRel32Opcode)
        std::snprintf(text, size, "a jump to 0x%08X (detoured)",
                      static_cast<unsigned>(kGlarePassTarget + kJumpRel32Size + rel));
    else if (entry[0] == kMoveEaxImm32Opcode && std::memcmp(entry + 5, kJumpEax, sizeof(kJumpEax)) == 0)
        std::snprintf(text, size, "mov eax, 0x%08X; jmp eax (detoured)", static_cast<unsigned>(target));
    else
        std::snprintf(text, size, "%02X %02X %02X %02X %02X %02X %02X (unknown)", entry[0], entry[1], entry[2],
                      entry[3], entry[4], entry[5], entry[6]);
}

WaterClass ClassifyWaterSettings(const void* liquidSettings)
{
    SettingsBank bank = {};
    if (!liquidSettings || !ReadSettingsBank(bank))
        return WaterClass::None;
    if (!SameBank(bank, g_classified.bank))
        ForgetClassifiedSettings(bank);
    if (const ClassifiedSettings* known = FindClassified(liquidSettings))
        return known->waterClass;
    LiquidDescription description = {};
    const bool read = DescribeLiquid(bank, liquidSettings, description);
    const WaterClass waterClass = read ? ClassOf(description) : WaterClass::None;
    RememberClassified(liquidSettings, waterClass);
    LogNewLiquid(liquidSettings, read, description, waterClass);
    return waterClass;
}
}
