#include "engine.h"

#include "log.h"
#include "water_classify.h"

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
constexpr uintptr_t kDeathScreenEffect = 0x00B74368;
constexpr uintptr_t kScreenEffectEnableCVar = 0x4;
constexpr uintptr_t kCVarIntValue = 0x30;
constexpr uintptr_t kDayNightGlow = 0x00D38C2C;

constexpr uintptr_t kGlowEffectVtable = 0x00A941C8;
constexpr uintptr_t kGlowSetParamSlot = 0x00A941D8;
constexpr uintptr_t kGlowSetParam = 0x008BFDE0;
constexpr uintptr_t kGlowCompositePassVtable = 0x00A94294;
constexpr uintptr_t kGlowPassListCount = 0x4;
constexpr uintptr_t kGlowPassListData = 0x8;
constexpr uintptr_t kGlowPassListOffsets[kGlowPassLists] = {0x08, 0x1C};
constexpr uint32_t kGlowCompositePassIndex = 2;
constexpr uintptr_t kPassColourAlpha = 0x33;

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

constexpr size_t kMaxCodeBytes = 32;

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

constexpr CodeBytes kWorldRenderTailAfterFfxEnd = {
    "world render tail after FFX end", 0x004F9286, 11,
    {0xE8, 0x55, 0xE8, 0x24, 0x00, 0x5F, 0x5E, 0x8B, 0xE5, 0x5D, 0xC3}};

const CodeBytes kGlowPassColourLayout[] = {
    {"glow feed SetParam call", 0x004F883C, 8, {0x8B, 0x40, 0x10, 0x52, 0x6A, 0x03, 0xFF, 0xD0}},
    {"glow SetParam argument reads", 0x008BFDE0, 17,
     {0x55, 0x8B, 0xEC, 0x8B, 0x45, 0x0C, 0x8B, 0x10, 0x89, 0x51, 0x2C, 0x8A, 0x50, 0x04, 0x8A, 0x40, 0x08}},
    {"underwater list composite pass load", 0x008BFDF2, 6, {0x8B, 0x71, 0x24, 0x8B, 0x76, 0x08}},
    {"underwater composite colour store", 0x008BFE08, 3, {0x89, 0x7E, 0x30}},
    {"clear-view list composite pass load", 0x008BFE14, 6, {0x8B, 0x41, 0x10, 0x8B, 0x48, 0x08}},
    {"clear-view composite colour store", 0x008BFE21, 3, {0x89, 0x51, 0x30}},
    {"glow SetParam return", 0x008BFE26, 3, {0xC2, 0x08, 0x00}},
    {"glow effect vtable store", 0x008BFE98, 6, {0xC7, 0x03, 0xC8, 0x41, 0xA9, 0x00}},
    {"glow composite pass vtable store", 0x008C2206, 6, {0xC7, 0x06, 0x94, 0x42, 0xA9, 0x00}},
    {"glow composite colour argument", 0x008C28A1, 3, {0x83, 0xC6, 0x30}},
    kWorldRenderTailAfterFfxEnd,
};

const CodeBytes kGradingPlacement[] = {
    kWorldRenderTailAfterFfxEnd,
    {"list flag clear after FFX end", 0x00747AE0, 27,
     {0xA1, 0x68, 0x13, 0xCA, 0x00, 0x85, 0xC0, 0x74, 0x11, 0xB9, 0xFF, 0xEF, 0xFF, 0xFF,
      0x8B, 0xFF, 0x21, 0x48, 0x10, 0x8B, 0x40, 0x08, 0x85, 0xC0, 0x75, 0xF6, 0xC3}},
    {"name and icon draw after the world render", 0x004FB042, 5, {0xE8, 0xF9, 0xA0, 0x2E, 0x00}},
    {"FFX pass end default target", 0x008C15A7, 13,
     {0x8B, 0x01, 0x8B, 0x50, 0x5C, 0x6A, 0x00, 0x6A, 0x00, 0x6A, 0x00, 0xFF, 0xD2}},
};

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

bool GlowScreenEffectRunsUnsafe()
{
    const uintptr_t effect = Read<uintptr_t>(kCurrentScreenEffect);
    return CVarEnabled(Read<uintptr_t>(kFfxCVar)) && effect && effect == Read<uintptr_t>(kGlowScreenEffect) &&
           CVarEnabled(Read<uintptr_t>(effect + kScreenEffectEnableCVar));
}

float GlowScreenEffectAmount()
{
    const float glow = Read<float>(kDayNightGlow);
    if (!GlowScreenEffectRunsUnsafe() || !std::isfinite(glow))
        return 0.0f;
    return std::clamp(glow, 0.0f, 1.0f);
}

bool FindGlowCompositePassesUnsafe(const ScreenEffects& effects, GlowCompositePasses& out)
{
    if (!effects.glow || effects.current != effects.glow || Read<uintptr_t>(effects.glow) != kGlowEffectVtable)
        return false;
    for (int list = 0; list < kGlowPassLists; ++list)
    {
        const uintptr_t passes = effects.glow + kGlowPassListOffsets[list];
        const uintptr_t data = Read<uintptr_t>(passes + kGlowPassListData);
        if (Read<uint32_t>(passes + kGlowPassListCount) <= kGlowCompositePassIndex || !data)
            return false;
        const uintptr_t pass = Read<uintptr_t>(data + kGlowCompositePassIndex * sizeof(uintptr_t));
        if (!pass || Read<uintptr_t>(pass) != kGlowCompositePassVtable)
            return false;
        out.pass[list] = pass;
    }
    return true;
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

bool ReadClassicLightInputsUnsafe(ClassicLightInputs& out)
{
    out.mapId = Read<int32_t>(kCurrentMap);
    ReadFloats(kCameraPosition, out.camPos, 3);
    out.dayFraction = Read<float>(kDayFraction);
    out.lightParams = ReadLightParamsSelection();
    return Finite(out.camPos, 3) && std::isfinite(out.dayFraction);
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

bool SlotHolds(uintptr_t slot, uintptr_t expected)
{
    __try
    {
        return Read<uintptr_t>(slot) == expected;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

class GuardRangeList
{
public:
    GuardRangeList(CodeRange* out, int capacity) : m_out(out), m_capacity(capacity) {}

    template <size_t N>
    void Add(const CodeBytes (&codes)[N])
    {
        for (const CodeBytes& code : codes)
            Add(code.address, code.size);
    }

    void Add(uintptr_t address, size_t size)
    {
        if (m_count < m_capacity)
            m_out[m_count] = {address, size};
        ++m_count;
    }

    int Count() const { return m_count; }

private:
    CodeRange* m_out;
    int m_capacity;
    int m_count = 0;
};

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

ScreenEffects ReadScreenEffects()
{
    ScreenEffects effects;
    __try
    {
        effects.current = Read<uintptr_t>(kCurrentScreenEffect);
        effects.glow = Read<uintptr_t>(kGlowScreenEffect);
        effects.death = Read<uintptr_t>(kDeathScreenEffect);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        effects.current = effects.glow = effects.death = 0;
    }
    return effects;
}

bool GlowScreenEffectRuns()
{
    __try
    {
        return GlowScreenEffectRunsUnsafe();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool GlowPassColourLayoutMatches()
{
    if (!AllCodeBytesMatch(kGlowPassColourLayout, "Forever glow unavailable"))
        return false;
    if (SlotHolds(kGlowSetParamSlot, kGlowSetParam))
        return true;
    VF_LOG_ERROR("Forever glow unavailable: the glow effect's SetParam slot 0x%08X differs from the 12340 client",
                 static_cast<unsigned>(kGlowSetParamSlot));
    return false;
}

bool GradingPlacementMatches()
{
    return AllCodeBytesMatch(kGradingPlacement, "colour grading unavailable");
}

int ForeverLookGuardRanges(CodeRange* out, int capacity)
{
    GuardRangeList ranges(out, capacity);
    ranges.Add(kGlowPassColourLayout);
    ranges.Add(kGlowSetParamSlot, sizeof(uintptr_t));
    ranges.Add(kGradingPlacement);
    return ranges.Count();
}

bool FindGlowCompositePasses(const ScreenEffects& effects, GlowCompositePasses& out)
{
    __try
    {
        return FindGlowCompositePassesUnsafe(effects, out);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool ReadGlowByte(uintptr_t pass, uint8_t& value)
{
    __try
    {
        value = Read<uint8_t>(pass + kPassColourAlpha);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool WriteGlowByte(uintptr_t pass, uint8_t value)
{
    __try
    {
        *reinterpret_cast<volatile uint8_t*>(pass + kPassColourAlpha) = value;
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
}

bool ReadClassicLightInputs(ClassicLightInputs& out)
{
    __try
    {
        return ReadClassicLightInputsUnsafe(out);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return false;
    }
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

bool OpaqueViewport(D3DVIEWPORT9& out)
{
    if (!g_opaque.valid)
        return false;
    out = g_opaque.viewport;
    return true;
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
