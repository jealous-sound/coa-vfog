#include "hooks.h"

#include "client_ripple_sprites.h"
#include "config.h"
#include "d3d9_wrap.h"
#include "engine.h"
#include "fog_model.h"
#include "log.h"
#include "water_classify.h"
#include "water_renderer.h"
#include "status_log.h"

#include <windows.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{
using GetProcAddressFn = FARPROC(WINAPI*)(HMODULE, LPCSTR);
using WaterMaterialRenderFn = void(__thiscall*)(void* material, void* environment, void* geometry, void* aux,
                                                const float* cameraPosition, const float* world, const float* bounds,
                                                void* liquidSettings);

constexpr unsigned char kCallRel32Opcode = 0xE8;
constexpr uintptr_t kCallRel32Size = 5;
constexpr float kOutOfRangeStockFogStart = 50000.0f;
constexpr DWORD kConfigReloadIntervalMs = 1000;
constexpr int kEasternKingdomsMap = 0;
constexpr int kKalimdorMap = 1;
constexpr int kOutlandMap = 530;
constexpr int kNorthrendMap = 571;
constexpr size_t kDrawnWaterClassesTextSize = 64;
constexpr unsigned kFlatWavesBit = 1u << 31;
constexpr const char* kCameraUnderLiquid = "camera under liquid";

uintptr_t g_worldRenderTarget = engine::kWorldRenderTarget;
uintptr_t g_opaqueM2PassTarget = engine::kOpaqueM2PassTarget;
uintptr_t g_liquidSurfaceTarget = engine::kLiquidSurfaceTarget;
FogDevice* g_liquidDepthWriteDevice = nullptr;
uintptr_t g_screenEffectsTarget = engine::kScreenEffectsTarget;
bool g_failed = false;
bool g_renderedLastFrame = false;
bool g_renderedThisFrame = false;
bool g_stockFogPushed = false;
engine::StockFog g_savedStockFog = {};
bool g_deviceChecked = false;
DWORD g_lastReload = 0;
const char* g_fogNotDrawnReason = "";
StatusLog g_fogLog;

uintptr_t g_waterPassTarget = engine::kWaterPassTarget;
bool g_waterHooksInstalled = false;
bool g_waterFailed = false;
bool g_waterFaultLogged = false;
bool g_waterPassBeginReusesArgumentSlot = false;
FogDevice* g_waterPassDevice = nullptr;
FogDevice* g_waterResourcesDevice = nullptr;
bool g_waterPassRanThisFrame = false;
bool g_waterShadedThisFrame = false;
bool g_waterRipplesAvailable = true;
unsigned g_waterClassesThisPass = 0;
WaterFrameStatus g_waterStatus = {false, "waiting for the world to render"};
StatusLog g_waterLog;
unsigned g_drawnWaterClassesTextMask = 0;
char g_drawnWaterClassesText[kDrawnWaterClassesTextSize] = "";

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
    if (FogDrawsInPlaceOfStockFog())
        PushStockFogOutOfRange();
}

void OnLiquidSurfaceBegin()
{
    if (g_failed || !g_renderedLastFrame || !GlobalConfig().Get().liquidDepth)
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

struct WaterClient
{
    FogDevice* (*device)();
    bool (*frameInputs)(FrameInputs& in);
    bool (*waterInputs)(WaterInputs& water);
    void (*contacts)(const FrameInputs& in, WaterContactFrame& out);
    bool (*contactsSupported)();
    WaterClass (*classify)(const void* liquidSettings);
};

bool GameWaterFrameInputs(FrameInputs& in)
{
    if (!engine::BuildFrameInputs(in, false, {}))
        return false;
    if (g_stockFogPushed)
        UseClientFogRangeInsteadOfPushed(in);
    return true;
}

bool GameWaterInputs(WaterInputs& water)
{
    if (!engine::BuildWaterInputs(water))
        return false;
    water.stockFogApplies = !g_stockFogPushed;
    return true;
}

void GameWaterContacts(const FrameInputs& in, WaterContactFrame& out)
{
    engine::CaptureWaterContacts(in.camTarget, out);
}

const WaterClient kGameWaterClient = {&GameFogDevice,
                                      &GameWaterFrameInputs,
                                      &GameWaterInputs,
                                      &GameWaterContacts,
                                      &engine::WaterContactsSupported,
                                      &engine::ClassifyWaterSettings};

FrameInputs g_testWaterFrame = {};
WaterInputs g_testWaterInputs = {};

bool TestWaterFrameInputs(FrameInputs& in)
{
    in = g_testWaterFrame;
    return true;
}

bool TestWaterInputs(WaterInputs& water)
{
    water = g_testWaterInputs;
    return true;
}

unsigned g_testWaterContactReads = 0;
bool g_testWaterContactsRefused = false;

void TestWaterContacts(const FrameInputs&, WaterContactFrame& out)
{
    out = g_testWaterInputs.contacts;
    ++g_testWaterContactReads;
}

bool TestWaterContactsSupported()
{
    return !g_testWaterContactsRefused;
}

WaterClass TestWaterClass(const void* liquidSettings)
{
    return liquidSettings ? static_cast<WaterClass>(*static_cast<const int*>(liquidSettings)) : WaterClass::None;
}

const WaterClient kTestWaterClient = {&LatestFogDevice,   &TestWaterFrameInputs,       &TestWaterInputs,
                                      &TestWaterContacts, &TestWaterContactsSupported, &TestWaterClass};
const WaterClient* g_waterClient = &kGameWaterClient;

void RecordFogFrame(bool rendered, bool cameraUnderLiquid, const char* skip)
{
    if (rendered)
    {
        g_fogNotDrawnReason = "";
        g_fogLog.Drawn();
        return;
    }
    if (cameraUnderLiquid)
    {
        g_fogNotDrawnReason = kCameraUnderLiquid;
        const StatusLogLine line = g_fogLog.Idle(kCameraUnderLiquid);
        if (line.write)
            LogWrite(line.level, "fog idle: %s%s", kCameraUnderLiquid,
                     line.level == LogLevel::Info ? " (repeats are logged at LogLevel 2)" : "");
        return;
    }
    g_fogNotDrawnReason = skip;
    const StatusLogLine line = g_fogLog.Skip(skip);
    if (line.write)
        LogWrite(line.level, "fog skipped: %s", skip);
}

bool RenderCurrentWorldFog(FogDevice* device)
{
    if (!device || !engine::HasOpaqueState())
        return false;

    ReloadConfigAfterInterval();

    const Config& cfg = GlobalConfig().Get();
    FrameInputs in = {};
    bool valid = engine::BuildFrameInputs(in, cfg.localLights, LocalLightUpload(cfg));
    if (g_stockFogPushed)
        UseClientFogRangeInsteadOfPushed(in);
    const char* skip = "invalid frame inputs";
    const bool cameraUnderLiquid = valid && in.inLiquid && !cfg.underwater;
    const bool rendered = valid && !cameraUnderLiquid && RenderFog(device, in, cfg, &skip);
    g_renderedThisFrame = rendered;
    RecordFogFrame(rendered, cameraUnderLiquid, skip);
    return rendered;
}

void OnOpaqueDone()
{
    FogDevice* device = g_failed ? nullptr : GameFogDevice();
    if (!device)
        return;
    engine::CaptureOpaqueState(RealDevice(device));
}

void OnWorldDone()
{
    if (!g_failed)
        RenderCurrentWorldFog(GameFogDevice());
    engine::ClearOpaqueState();
}

int GuardFilter(unsigned code, const char* where)
{
    VF_LOG_ERROR("exception 0x%08X in %s; fog disabled for this session", code, where);
    return EXCEPTION_EXECUTE_HANDLER;
}

void RecordWaterSkip(const char* reason)
{
    g_waterStatus = {false, reason};
    const StatusLogLine line = g_waterLog.Skip(reason);
    if (line.write)
        LogWrite(line.level, "water skipped: %s", reason);
}

void RecordWaterIdle(const char* state)
{
    g_waterStatus = {false, state};
    const StatusLogLine line = g_waterLog.Idle(state);
    if (line.write)
        LogWrite(line.level, "water idle: %s%s", state,
                 line.level == LogLevel::Info ? " (repeats are logged at LogLevel 2)" : "");
}

unsigned WaterClassBit(WaterClass waterClass)
{
    return 1u << static_cast<unsigned>(waterClass);
}

const char* DrawnWaterClassesText(unsigned classMask, bool flatWaves)
{
    const unsigned key = classMask | (flatWaves ? kFlatWavesBit : 0u);
    if (key == g_drawnWaterClassesTextMask)
        return g_drawnWaterClassesText;
    g_drawnWaterClassesTextMask = key;
    g_drawnWaterClassesText[0] = 0;
    for (WaterClass waterClass : {WaterClass::Lake, WaterClass::River, WaterClass::Ocean, WaterClass::Interior})
    {
        if (!(classMask & WaterClassBit(waterClass)))
            continue;
        const size_t used = std::strlen(g_drawnWaterClassesText);
        std::snprintf(g_drawnWaterClassesText + used, sizeof(g_drawnWaterClassesText) - used, "%s%s",
                      used ? ", " : "", WaterClassLabel(waterClass));
    }
    if (flatWaves)
    {
        const size_t used = std::strlen(g_drawnWaterClassesText);
        std::snprintf(g_drawnWaterClassesText + used, sizeof(g_drawnWaterClassesText) - used, "; flat waves");
    }
    return g_drawnWaterClassesText;
}

void RecordWaterDrawn(unsigned classMask, bool flatWaves)
{
    g_waterStatus = {true, DrawnWaterClassesText(classMask, flatWaves)};
    g_waterLog.Drawn();
}

enum class WaterArming
{
    Armed,
    Idle,
    Failed,
};

WaterArming ArmWaterPass(FogDevice* device, const Config& cfg, const char** reason)
{
    FrameInputs in = {};
    if (!g_waterClient->frameInputs(in))
    {
        *reason = "invalid frame inputs";
        return WaterArming::Failed;
    }
    if (in.inLiquid)
    {
        *reason = "camera under water";
        return WaterArming::Idle;
    }
    WaterInputs water = {};
    if (!g_waterClient->waterInputs(water))
    {
        *reason = "the client's water colours are unavailable";
        return WaterArming::Failed;
    }
    water.contacts = {};
    if (cfg.waterRipples > 0.0f)
        g_waterClient->contacts(in, water.contacts);
    const char* skip = "";
    if (BeginWaterPass(device, in, water, cfg, &skip))
        return WaterArming::Armed;
    *reason = skip && *skip ? skip : "the water pass could not start";
    return WaterArming::Failed;
}

void OnWaterPassBegin(const void* liquidRenderer)
{
    g_waterPassRanThisFrame = true;
    g_waterPassDevice = nullptr;
    g_waterClassesThisPass = 0;
    const Config& cfg = GlobalConfig().Get();
    if (g_waterFailed || !cfg.water)
        return;
    if (g_failed)
    {
        RecordWaterSkip("the fog stopped after an exception");
        return;
    }
    if (!engine::TransparentLiquidsQueued(liquidRenderer))
    {
        RecordWaterIdle("no water in view");
        return;
    }
    FogDevice* device = g_waterClient->device();
    if (!device)
    {
        RecordWaterSkip("no fog device");
        return;
    }
    const char* reason = "the water pass could not start";
    g_waterPassDevice = device;
    g_waterResourcesDevice = device;
    const WaterArming arming = ArmWaterPass(device, cfg, &reason);
    if (arming == WaterArming::Armed)
        return;
    g_waterPassDevice = nullptr;
    if (arming == WaterArming::Idle)
        RecordWaterIdle(reason);
    else
        RecordWaterSkip(reason);
}

void OnWaterPassEnd()
{
    if (!g_waterPassDevice)
        return;
    const WaterPassEnd end = EndWaterPass(g_waterPassDevice);
    g_waterPassDevice = nullptr;
    g_waterShadedThisFrame = g_waterShadedThisFrame || end.shaded;
    if (end.shaded)
    {
        g_waterRipplesAvailable = end.ripplesAvailable;
        RecordWaterDrawn(end.shadedClasses, end.flatWaves);
    }
    else if (!g_waterClassesThisPass)
        RecordWaterIdle("no water in view");
    else
        RecordWaterSkip(end.skipReason && *end.skipReason ? end.skipReason : "the water pass shaded nothing");
}

void TagWaterDrawUnsafe(const void* liquidSettings)
{
    const WaterClass waterClass = g_waterClient->classify(liquidSettings);
    TagWaterDraw(g_waterPassDevice, waterClass);
    if (waterClass != WaterClass::None)
        g_waterClassesThisPass |= WaterClassBit(waterClass);
}

void AbortArmedWaterPass()
{
    FogDevice* device = g_waterPassDevice;
    g_waterPassDevice = nullptr;
    if (device)
        AbortWaterPass(device);
}

void ReleaseWaterWhenOff()
{
    if (!g_waterResourcesDevice || (!g_waterFailed && GlobalConfig().Get().water))
        return;
    FogDevice* device = g_waterResourcesDevice;
    g_waterResourcesDevice = nullptr;
    if (device == g_waterClient->device())
        ReleaseWaterResources(device);
}

bool RingsReplaceClientSprites(const Config& cfg)
{
    return !g_waterFailed && cfg.water && cfg.waterRipples > 0.0f && !cfg.waterClientSplashes &&
           g_waterRipplesAvailable && g_waterClient->contactsSupported();
}

void UpdateClientRippleSprites()
{
    const bool allowed = RingsReplaceClientSprites(GlobalConfig().Get());
    GlobalClientRippleSprites().Update(allowed, g_waterShadedThisFrame, WaterClockSeconds());
    g_waterShadedThisFrame = false;
}

void EndWaterFrame()
{
    if (!g_waterHooksInstalled)
        return;
    if (g_waterPassDevice)
    {
        AbortArmedWaterPass();
        RecordWaterSkip("the water pass did not finish");
    }
    else if (!g_waterPassRanThisFrame)
        RecordWaterIdle("no liquid pass in this frame");
    g_waterPassRanThisFrame = false;
    UpdateClientRippleSprites();
    ReleaseWaterWhenOff();
}

int WaterGuardFilter(unsigned code, const char* where)
{
    if (!g_waterFaultLogged)
        VF_LOG_ERROR("exception 0x%08X in %s; water disabled for this session, the fog continues", code, where);
    g_waterFaultLogged = true;
    return EXCEPTION_EXECUTE_HANDLER;
}

void FailWater()
{
    g_waterFailed = true;
    GlobalClientRippleSprites().Restore();
    __try
    {
        AbortArmedWaterPass();
    }
    __except (WaterGuardFilter(GetExceptionCode(), "water pass abort"))
    {
        g_waterPassDevice = nullptr;
    }
}

bool TagArmedWaterDraw(const void* liquidSettings)
{
    if (!g_waterPassDevice)
        return false;
    __try
    {
        TagWaterDrawUnsafe(liquidSettings);
        return true;
    }
    __except (WaterGuardFilter(GetExceptionCode(), "water draw classification"))
    {
        FailWater();
        return false;
    }
}

void UntagArmedWaterDraw()
{
    if (!g_waterPassDevice)
        return;
    __try
    {
        UntagWaterDraw(g_waterPassDevice);
    }
    __except (WaterGuardFilter(GetExceptionCode(), "water draw untag"))
    {
        FailWater();
    }
}

void EndWaterFrameGuarded()
{
    __try
    {
        EndWaterFrame();
    }
    __except (WaterGuardFilter(GetExceptionCode(), "water frame end"))
    {
        FailWater();
    }
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
    EndWaterFrameGuarded();
    __try
    {
        OnFrameEnd();
    }
    __except (GuardFilter(GetExceptionCode(), "frame end hook"))
    {
        g_failed = true;
    }
}

extern "C" void __cdecl vf_on_water_pass_begin(const void* liquidRenderer)
{
    __try
    {
        OnWaterPassBegin(liquidRenderer);
    }
    __except (WaterGuardFilter(GetExceptionCode(), "water pass begin hook"))
    {
        FailWater();
    }
    if (g_waterPassBeginReusesArgumentSlot)
        *static_cast<const void* volatile*>(&liquidRenderer) = nullptr;
}

extern "C" void __cdecl vf_on_water_pass_end()
{
    __try
    {
        OnWaterPassEnd();
    }
    __except (WaterGuardFilter(GetExceptionCode(), "water pass end hook"))
    {
        FailWater();
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

__declspec(naked) static void WaterPassThunk()
{
    __asm {
        push ecx
        push ecx
        call vf_on_water_pass_begin
        add esp, 4
        pop ecx
        push dword ptr [esp + 8]
        push dword ptr [esp + 8]
        call dword ptr [g_waterPassTarget]
        pushad
        call vf_on_water_pass_end
        popad
        ret 8
    }
}

const void* RetargetWaterPassThunk(uintptr_t target)
{
    g_waterPassTarget = target;
    return reinterpret_cast<const void*>(&WaterPassThunk);
}

void ReuseWaterPassBeginArgumentSlot(bool reuse)
{
    g_waterPassBeginReusesArgumentSlot = reuse;
}

template <uintptr_t Render>
static void __fastcall WaterMaterialRenderHook(void* material, void*, void* environment, void* geometry, void* aux,
                                               const float* cameraPosition, const float* world, const float* bounds,
                                               void* liquidSettings)
{
    const bool tagged = TagArmedWaterDraw(liquidSettings);
    __try
    {
        reinterpret_cast<WaterMaterialRenderFn>(Render)(material, environment, geometry, aux, cameraPosition, world,
                                                        bounds, liquidSettings);
    }
    __finally
    {
        if (tagged)
            UntagArmedWaterDraw();
    }
}

static void __cdecl WorldTextDrawThunk(void* batch)
{
    FogDevice* device = nullptr;
    __try
    {
        if (!g_failed && GlobalConfig().Get().enable)
            device = GameFogDevice();
        SuppressDepthWrite(device, true);
    }
    __except (GuardFilter(GetExceptionCode(), "world text depth hook"))
    {
        g_failed = true;
    }
    __try
    {
        reinterpret_cast<void(__cdecl*)(void*)>(engine::kWorldTextDrawTarget)(batch);
    }
    __finally
    {
        SuppressDepthWrite(device, false);
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

struct PointerSlot
{
    uintptr_t slot;
    uintptr_t original;
    const void* hook;
};

uintptr_t SlotValue(uintptr_t slot)
{
    return *reinterpret_cast<const uintptr_t*>(slot);
}

bool ExchangeSlot(uintptr_t slot, uintptr_t expected, uintptr_t replacement)
{
    DWORD old;
    if (!VirtualProtect(reinterpret_cast<void*>(slot), sizeof(uintptr_t), PAGE_READWRITE, &old))
        return false;
    const LONG previous = InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(slot),
                                                     static_cast<LONG>(replacement), static_cast<LONG>(expected));
    VirtualProtect(reinterpret_cast<void*>(slot), sizeof(uintptr_t), old, &old);
    return static_cast<uintptr_t>(previous) == expected;
}

void RestoreSlots(const PointerSlot* slots, int count)
{
    for (int i = 0; i < count; ++i)
        ExchangeSlot(slots[i].slot, reinterpret_cast<uintptr_t>(slots[i].hook), slots[i].original);
}

template <size_t N>
bool SlotsHoldTheClientRenders(const PointerSlot (&slots)[N])
{
    for (const PointerSlot& s : slots)
        if (SlotValue(s.slot) != s.original)
        {
            VF_LOG_ERROR("water material slot 0x%08X holds 0x%08X (expected 0x%08X); water hooks not installed",
                         static_cast<unsigned>(s.slot), static_cast<unsigned>(SlotValue(s.slot)),
                         static_cast<unsigned>(s.original));
            return false;
        }
    return true;
}

template <size_t N>
bool ReplaceSlots(const PointerSlot (&slots)[N])
{
    int replaced = 0;
    for (const PointerSlot& s : slots)
    {
        if (!ExchangeSlot(s.slot, s.original, reinterpret_cast<uintptr_t>(s.hook)))
        {
            RestoreSlots(slots, replaced);
            VF_LOG_ERROR("water material slot 0x%08X could not be replaced; water hooks not installed",
                         static_cast<unsigned>(s.slot));
            return false;
        }
        ++replaced;
    }
    return true;
}
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
        {engine::kWorldTextDrawSite, engine::kWorldTextDrawTarget, &WorldTextDrawThunk},
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
                "world done 0x%08X, world text 0x%08X",
                static_cast<unsigned>(engine::kWorldRenderSite), static_cast<unsigned>(engine::kOpaqueM2PassSite),
                static_cast<unsigned>(engine::kLiquidSurfaceSite),
                static_cast<unsigned>(engine::kScreenEffectsSite), static_cast<unsigned>(engine::kWorldTextDrawSite));
    return true;
}

FogFrameStatus LastFogFrameStatus()
{
    if (g_failed)
        return {false, "stopped after an exception, see CoAVolFog.log"};
    if (g_renderedLastFrame)
        return {true, ""};
    return {false, *g_fogNotDrawnReason ? g_fogNotDrawnReason : "waiting for the world to render"};
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

bool InstallWaterHooks()
{
    const PointerSlot slots[] = {
        {engine::kWaterMaterialRenderSlot, engine::kWaterMaterialRender,
         reinterpret_cast<const void*>(&WaterMaterialRenderHook<engine::kWaterMaterialRender>)},
        {engine::kWaterNoSpecMaterialRenderSlot, engine::kWaterNoSpecMaterialRender,
         reinterpret_cast<const void*>(&WaterMaterialRenderHook<engine::kWaterNoSpecMaterialRender>)},
    };
    if (!engine::WaterClientLayoutMatches())
    {
        VF_LOG_ERROR("water hooks not installed: the client's liquid code differs from the 12340 client");
        return false;
    }
    if (!SiteMatches(engine::kWaterPassSite, engine::kWaterPassTarget))
    {
        VF_LOG_ERROR("water pass call site 0x%08X differs from the 12340 client; water hooks not installed",
                     static_cast<unsigned>(engine::kWaterPassSite));
        return false;
    }
    if (!SlotsHoldTheClientRenders(slots) || !ReplaceSlots(slots))
        return false;
    if (!PatchCallSite(engine::kWaterPassSite, engine::kWaterPassTarget,
                       reinterpret_cast<const void*>(&WaterPassThunk)))
    {
        RestoreSlots(slots, static_cast<int>(sizeof(slots) / sizeof(slots[0])));
        VF_LOG_ERROR("water pass call site 0x%08X could not be patched; water hooks not installed",
                     static_cast<unsigned>(engine::kWaterPassSite));
        return false;
    }
    g_waterHooksInstalled = true;
    VF_LOG_INFO("water hooks installed: water pass 0x%08X, material render slots 0x%08X and 0x%08X",
                static_cast<unsigned>(engine::kWaterPassSite), static_cast<unsigned>(engine::kWaterMaterialRenderSlot),
                static_cast<unsigned>(engine::kWaterNoSpecMaterialRenderSlot));
    GlobalClientRippleSprites().Bind(reinterpret_cast<volatile int32_t*>(engine::kWaterRipplesCommandValue),
                                     ClientCodeView());
    return true;
}

void RecordHookedFogFrame(bool rendered, bool cameraUnderLiquid, const char* skip)
{
    RecordFogFrame(rendered, cameraUnderLiquid, skip);
}

void UseTestWaterClient(const FrameInputs& in, const WaterInputs& water)
{
    g_testWaterFrame = in;
    g_testWaterInputs = water;
    g_waterClient = &kTestWaterClient;
    g_waterHooksInstalled = true;
}

unsigned TestWaterContactReads()
{
    return g_testWaterContactReads;
}

void RefuseTestWaterContacts(bool refused)
{
    g_testWaterContactsRefused = refused;
}

bool TagHookedWaterDraw(const void* liquidSettings)
{
    return TagArmedWaterDraw(liquidSettings);
}

void UntagHookedWaterDraw()
{
    UntagArmedWaterDraw();
}

WaterFrameStatus LastWaterFrameStatus()
{
    if (!g_waterHooksInstalled)
        return {false, "hooks not installed"};
    if (g_waterFailed)
        return {false, "stopped after an exception, see CoAVolFog.log"};
    if (!GlobalConfig().Get().water)
        return {false, "off"};
    return g_waterStatus;
}
