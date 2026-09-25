#include "hooks.h"

#include "config.h"
#include "d3d9_wrap.h"
#include "engine.h"
#include "log.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
using GetProcAddressFn = FARPROC(WINAPI*)(HMODULE, LPCSTR);

constexpr unsigned char kCallRel32Opcode = 0xE8;
constexpr uintptr_t kCallRel32Size = 5;
constexpr float kOutOfRangeStockFogStart = 50000.0f;
constexpr DWORD kConfigReloadIntervalMs = 1000;
constexpr unsigned kMaxSkipsLogged = 50;
constexpr int kEasternKingdomsMap = 0;
constexpr int kKalimdorMap = 1;
constexpr int kOutlandMap = 530;
constexpr int kNorthrendMap = 571;

uintptr_t g_worldRenderTarget = engine::kWorldRenderTarget;
uintptr_t g_opaqueM2PassTarget = engine::kOpaqueM2PassTarget;
uintptr_t g_liquidSurfaceTarget = engine::kLiquidSurfaceTarget;
FogDevice* g_liquidDepthWriteDevice = nullptr;
uintptr_t g_screenEffectsTarget = engine::kScreenEffectsTarget;
bool g_failed = false;
bool g_renderedLastFrame = false;
bool g_renderedThisFrame = false;
bool g_opaqueFogRendered = false;
bool g_stockFogPushed = false;
engine::StockFog g_savedStockFog = {};
bool g_deviceChecked = false;
unsigned g_skipsLogged = 0;
DWORD g_lastReload = 0;
const char* g_lastSkip = "";

FARPROC WINAPI GetProcAddressFilter(HMODULE module, LPCSTR name)
{
    FARPROC proc = GetProcAddress(module, name);
    if (!proc || !name || IS_INTRESOURCE(name))
        return proc;
    if (std::strcmp(name, "Direct3DCreate9") == 0)
    {
        HMODULE pinned = nullptr;
        GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_PIN | GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                           reinterpret_cast<LPCSTR>(proc), &pinned);
        SetRealDirect3DCreate9(reinterpret_cast<Direct3DCreate9Fn>(proc));
        VF_LOG_INFO("Direct3DCreate9 resolved at %p; wrapping", reinterpret_cast<void*>(proc));
        return reinterpret_cast<FARPROC>(&WrappedDirect3DCreate9);
    }
    if (std::strcmp(name, "Direct3DCreate9Ex") == 0)
        VF_LOG_INFO("Direct3DCreate9Ex requested; the D3D9Ex path is not wrapped (fog unavailable with gxApi d3d9ex)");
    return proc;
}

FogDevice* GameFogDevice()
{
    void* game = engine::GameD3DDevice();
    FogDevice* device = WrapperOrLatestFogDevice(game);
    if (!g_deviceChecked && device)
    {
        g_deviceChecked = true;
        if (!IsWrapperOf(device, game))
            VF_LOG_ERROR("the client's D3D device %p is not a fog wrapper; using the latest fog device", game);
    }
    return device;
}

bool FogDrawsInPlaceOfStockFog()
{
    return !g_failed && g_renderedLastFrame && GlobalConfig().Get().stockFog == 1 && !engine::CameraInLiquid() &&
           GameFogDevice();
}

void PushStockFogOutOfRange()
{
    g_savedStockFog = engine::ReadStockFog();
    engine::StockFog outOfRange;
    for (int group = 0; group < engine::kDayNightFogGroupCount; ++group)
    {
        outOfRange.start[group] = kOutOfRangeStockFogStart;
        outOfRange.end[group] = kOutOfRangeStockFogStart * 2.0f;
    }
    engine::WriteStockFog(outOfRange);
    g_stockFogPushed = true;
}

void RestorePushedStockFog()
{
    if (!g_stockFogPushed)
        return;
    engine::WriteStockFog(g_savedStockFog);
    g_stockFogPushed = false;
}

void OnFrameBegin()
{
    g_renderedThisFrame = false;
    g_opaqueFogRendered = false;
    EndMaterialFog(GameFogDevice());
    if (FogDrawsInPlaceOfStockFog())
        PushStockFogOutOfRange();
}

void OnLiquidSurfaceBegin()
{
    if (g_failed || g_opaqueFogRendered || !g_renderedLastFrame || !GlobalConfig().Get().liquidDepth)
        return;
    g_liquidDepthWriteDevice = GameFogDevice();
    ForceDepthWrite(g_liquidDepthWriteDevice, true);
}

void OnLiquidSurfaceEnd()
{
    ForceDepthWrite(g_liquidDepthWriteDevice, false);
    g_liquidDepthWriteDevice = nullptr;
}

void OnFrameEnd()
{
    EndMaterialFog(GameFogDevice());
    OnLiquidSurfaceEnd();
    RestorePushedStockFog();
    g_renderedLastFrame = g_renderedThisFrame;
}

void ReloadConfigAfterInterval()
{
    DWORD now = GetTickCount();
    if (now - g_lastReload > kConfigReloadIntervalMs)
    {
        g_lastReload = now;
        GlobalConfig().ReloadIfChanged();
    }
}

void UseClientFogRangeInsteadOfPushed(FrameInputs& in)
{
    in.fogStart = g_savedStockFog.start[engine::kFrameInputsFogGroup];
    in.fogEnd = g_savedStockFog.end[engine::kFrameInputsFogGroup];
}

bool RenderCurrentWorldFog(FogDevice* device, bool materialFog)
{
    if (!device || !engine::HasOpaqueState())
        return false;

    ReloadConfigAfterInterval();

    FrameInputs in = {};
    bool valid = engine::BuildFrameInputs(in);
    if (g_stockFogPushed)
        UseClientFogRangeInsteadOfPushed(in);
    Config cfg = GlobalConfig().Get();
    cfg.materialFog = cfg.materialFog && materialFog;
    const char* skip = "invalid frame inputs";
    bool rendered = false;
    if (valid && (!in.inLiquid || cfg.underwater))
        rendered = RenderFog(device, in, cfg, &skip);
    else if (valid)
        skip = "camera under liquid";
    g_renderedThisFrame = rendered;
    if (!rendered && skip != g_lastSkip && g_skipsLogged < kMaxSkipsLogged)
    {
        ++g_skipsLogged;
        VF_LOG_INFO("fog skipped: %s", skip);
    }
    g_lastSkip = rendered ? "" : skip;
    return rendered;
}

void OnOpaqueDone()
{
    FogDevice* device = g_failed ? nullptr : GameFogDevice();
    if (!device)
        return;
    engine::CaptureOpaqueState(RealDevice(device));
    ReloadConfigAfterInterval();
    const Config& cfg = GlobalConfig().Get();
    if (cfg.materialFog && cfg.debugView == 0 && cfg.stockFog == 1 && MaterialFogCompatible(device))
    {
        g_opaqueFogRendered = RenderCurrentWorldFog(device, true);
        if (g_opaqueFogRendered)
            BeginRenderedMaterialFog(device);
    }
}

void OnWorldDone()
{
    FogDevice* device = GameFogDevice();
    EndMaterialFog(device);
    if (!g_failed && !g_opaqueFogRendered)
        RenderCurrentWorldFog(device, false);
    engine::ClearOpaqueState();
}

int GuardFilter(unsigned code, const char* where)
{
    VF_LOG_ERROR("exception 0x%08X in %s; fog disabled for this session", code, where);
    return EXCEPTION_EXECUTE_HANDLER;
}

bool PatchCallSite(uintptr_t site, uintptr_t expectedTarget, const void* thunk)
{
    auto* bytes = reinterpret_cast<unsigned char*>(site);
    int32_t rel;
    std::memcpy(&rel, bytes + 1, sizeof(rel));
    if (bytes[0] != kCallRel32Opcode || site + kCallRel32Size + rel != expectedTarget)
    {
        VF_LOG_ERROR("call site 0x%08X does not match (E8 -> 0x%08X expected); hooks not installed",
                     static_cast<unsigned>(site), static_cast<unsigned>(expectedTarget));
        return false;
    }
    int32_t newRel = static_cast<int32_t>(reinterpret_cast<uintptr_t>(thunk) - (site + kCallRel32Size));
    DWORD old;
    if (!VirtualProtect(bytes + 1, sizeof(newRel), PAGE_EXECUTE_READWRITE, &old))
        return false;
    std::memcpy(bytes + 1, &newRel, sizeof(newRel));
    VirtualProtect(bytes + 1, sizeof(newRel), old, &old);
    FlushInstructionCache(GetCurrentProcess(), bytes, kCallRel32Size);
    return true;
}

bool SiteMatches(uintptr_t site, uintptr_t expectedTarget)
{
    auto* bytes = reinterpret_cast<const unsigned char*>(site);
    int32_t rel;
    std::memcpy(&rel, bytes + 1, sizeof(rel));
    return bytes[0] == kCallRel32Opcode && site + kCallRel32Size + rel == expectedTarget;
}

float g_loggedFarClip = -1.0f;
int g_loggedFarClipMap = -1;

float ClientFarClipClamp(float farClipSetting, int mapId)
{
    return reinterpret_cast<engine::FarClipClampFn>(engine::kFarClipClamp)(farClipSetting, mapId);
}

bool IsContinentCappedByExtensionsDll(int mapId)
{
    return mapId == kEasternKingdomsMap || mapId == kKalimdorMap || mapId == kOutlandMap || mapId == kNorthrendMap;
}

const Config& ReloadedConfig()
{
    GlobalConfig().ReloadIfChanged();
    return GlobalConfig().Get();
}

float LiftedFarClipClamp(float farClipSetting, int mapId)
{
    float result = ClientFarClipClamp(farClipSetting, mapId);
    const float farClipMax = ReloadedConfig().farClipMax;
    if (farClipMax > 0.0f && IsContinentCappedByExtensionsDll(mapId) && std::isfinite(farClipSetting))
        result = std::max(result,
                          std::clamp(farClipSetting, kEngineFarClipMin, std::min(farClipMax, kEngineFarClipMax)));
    if (result != g_loggedFarClip || mapId != g_loggedFarClipMap)
    {
        g_loggedFarClip = result;
        g_loggedFarClipMap = mapId;
        VF_LOG_INFO("far clip: map %d, farclip setting %.1f -> %.2f", mapId, farClipSetting, result);
    }
    return result;
}
}

extern "C" float __cdecl vf_far_clip_clamp(float farClipSetting, int mapId)
{
    __try
    {
        return LiftedFarClipClamp(farClipSetting, mapId);
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return ClientFarClipClamp(farClipSetting, mapId);
    }
}

extern "C" void __cdecl vf_on_frame_begin()
{
    __try
    {
        OnFrameBegin();
    }
    __except (GuardFilter(GetExceptionCode(), "frame begin hook"))
    {
        g_failed = true;
    }
}

extern "C" void __cdecl vf_on_frame_end()
{
    __try
    {
        OnFrameEnd();
    }
    __except (GuardFilter(GetExceptionCode(), "frame end hook"))
    {
        g_failed = true;
    }
}

extern "C" void __cdecl vf_on_liquid_begin()
{
    __try
    {
        OnLiquidSurfaceBegin();
    }
    __except (GuardFilter(GetExceptionCode(), "liquid begin hook"))
    {
        g_failed = true;
    }
}

extern "C" void __cdecl vf_on_liquid_end()
{
    __try
    {
        OnLiquidSurfaceEnd();
    }
    __except (GuardFilter(GetExceptionCode(), "liquid end hook"))
    {
        g_failed = true;
    }
}

extern "C" void __cdecl vf_on_opaque_done()
{
    __try
    {
        OnOpaqueDone();
    }
    __except (GuardFilter(GetExceptionCode(), "opaque hook"))
    {
        g_failed = true;
    }
}

extern "C" void __cdecl vf_on_world_done()
{
    __try
    {
        OnWorldDone();
    }
    __except (GuardFilter(GetExceptionCode(), "world hook"))
    {
        g_failed = true;
    }
}

__declspec(naked) static void WorldRenderThunk()
{
    __asm {
        push ecx
        call vf_on_frame_begin
        pop ecx
        call dword ptr [g_worldRenderTarget]
        pushad
        call vf_on_frame_end
        popad
        ret
    }
}

__declspec(naked) static void OpaqueM2PassThunk()
{
    __asm {
        push dword ptr [esp + 4]
        call dword ptr [g_opaqueM2PassTarget]
        pushad
        call vf_on_opaque_done
        popad
        ret 4
    }
}

__declspec(naked) static void LiquidSurfaceThunk()
{
    __asm {
        pushad
        call vf_on_liquid_begin
        popad
        call dword ptr [g_liquidSurfaceTarget]
        pushad
        call vf_on_liquid_end
        popad
        ret
    }
}

__declspec(naked) static void ScreenEffectsThunk()
{
    __asm {
        pushad
        call vf_on_world_done
        popad
        jmp dword ptr [g_screenEffectsTarget]
    }
}

namespace
{
struct CallSite
{
    uintptr_t site;
    uintptr_t originalTarget;
    const void* thunk;
};
}

bool InstallEngineHooks()
{
    auto* slot = reinterpret_cast<GetProcAddressFn*>(engine::kGetProcAddressSlot);
    uintptr_t current = reinterpret_cast<uintptr_t>(*slot);
    if (current != engine::kGetProcAddressThunk)
    {
        VF_LOG_ERROR("GetProcAddress slot holds 0x%08X (expected the loader thunk 0x%08X); hooks not installed",
                     static_cast<unsigned>(current), static_cast<unsigned>(engine::kGetProcAddressThunk));
        return false;
    }
    const CallSite sites[] = {
        {engine::kWorldRenderSite, engine::kWorldRenderTarget, &WorldRenderThunk},
        {engine::kOpaqueM2PassSite, engine::kOpaqueM2PassTarget, &OpaqueM2PassThunk},
        {engine::kLiquidSurfaceSite, engine::kLiquidSurfaceTarget, &LiquidSurfaceThunk},
        {engine::kScreenEffectsSite, engine::kScreenEffectsTarget, &ScreenEffectsThunk},
    };
    for (const CallSite& s : sites)
        if (!SiteMatches(s.site, s.originalTarget))
        {
            VF_LOG_ERROR("world render call site 0x%08X differs from the 12340 client; hooks not installed",
                         static_cast<unsigned>(s.site));
            return false;
        }
    int patched = 0;
    for (const CallSite& s : sites)
    {
        if (!PatchCallSite(s.site, s.originalTarget, s.thunk))
        {
            while (patched-- > 0)
                PatchCallSite(sites[patched].site, reinterpret_cast<uintptr_t>(sites[patched].thunk),
                              reinterpret_cast<const void*>(sites[patched].originalTarget));
            return false;
        }
        ++patched;
    }
    *slot = &GetProcAddressFilter;
    VF_LOG_INFO("engine hooks installed: GetProcAddress filter, world render 0x%08X, opaque 0x%08X, liquid 0x%08X, "
                "world done 0x%08X",
                static_cast<unsigned>(engine::kWorldRenderSite), static_cast<unsigned>(engine::kOpaqueM2PassSite),
                static_cast<unsigned>(engine::kLiquidSurfaceSite), static_cast<unsigned>(engine::kScreenEffectsSite));
    return true;
}

FogFrameStatus LastFogFrameStatus()
{
    if (g_failed)
        return {false, "stopped after an exception, see CoAVolFog.log"};
    if (g_renderedLastFrame)
    {
        if (GlobalConfig().Get().materialFog && !MaterialFogCompatible(GameFogDevice()))
            return {true, "Material fog compatibility fallback; see CoAVolFog.log"};
        return {true, ""};
    }
    return {false, *g_lastSkip ? g_lastSkip : "waiting for the world to render"};
}

void InstallFarClipHooks()
{
    if (GlobalConfig().Get().farClipMax <= kFarClipMaxKeepsClientCap)
    {
        VF_LOG_INFO("far clip hooks not installed (FarClipMax=0)");
        return;
    }
    const uintptr_t sites[] = {engine::kFarClipCVarSetSite, engine::kFarClipMapLoadSite};
    for (uintptr_t site : sites)
        if (!SiteMatches(site, engine::kFarClipClamp))
        {
            VF_LOG_ERROR("far clip call site 0x%08X differs from the 12340 client; far clip left alone",
                         static_cast<unsigned>(site));
            return;
        }
    if (!PatchCallSite(sites[0], engine::kFarClipClamp, reinterpret_cast<const void*>(&vf_far_clip_clamp)))
        return;
    if (!PatchCallSite(sites[1], engine::kFarClipClamp, reinterpret_cast<const void*>(&vf_far_clip_clamp)))
    {
        PatchCallSite(sites[0], reinterpret_cast<uintptr_t>(&vf_far_clip_clamp),
                      reinterpret_cast<const void*>(engine::kFarClipClamp));
        return;
    }
    VF_LOG_INFO("far clip hooks installed at 0x%08X and 0x%08X (FarClipMax %.0f)", static_cast<unsigned>(sites[0]),
                static_cast<unsigned>(sites[1]), GlobalConfig().Get().farClipMax);
}
