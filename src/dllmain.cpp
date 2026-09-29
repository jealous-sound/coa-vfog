#include "config.h"
#include "d3d9_wrap.h"
#include "engine.h"
#include "fog_data.h"
#include "hooks.h"
#include "log.h"
#include "overlay.h"
#include "water_data.h"
#include "water_renderer.h"

#include <windows.h>

#include <string>

namespace
{
std::string ModuleDirectory(HMODULE module)
{
    char path[MAX_PATH] = {};
    DWORD n = GetModuleFileNameA(module, path, MAX_PATH);
    std::string dir(path, n);
    size_t slash = dir.find_last_of("\\/");
    return slash == std::string::npos ? std::string() : dir.substr(0, slash + 1);
}

void Attach(HMODULE module)
{
    std::string dir = ModuleDirectory(module);
    LogOpen((dir + "CoAVolFog.log").c_str());
    GlobalConfig().Load(dir + "CoAVolFog.ini");
    const Config& cfg = GlobalConfig().Get();
    VF_LOG_INFO("CoAVolFog loaded from %s", dir.c_str());
    GlobalFogData().Load(dir + "fogdata.bin");

    if (!engine::IsSupportedClient())
    {
        VF_LOG_INFO("host is not the 3.3.5a (12340) client; engine hooks skipped");
        return;
    }
    if (!cfg.enable)
    {
        VF_LOG_INFO("Enable=0; the client runs unmodified");
        return;
    }
    if (!cfg.hooks)
    {
        VF_LOG_INFO("EngineHooks=0; the client runs unmodified");
        return;
    }
    const bool engineHooks = InstallEngineHooks();
    AllowFogOnNewDevices(engineHooks);
    if (engineHooks && InstallWaterHooks())
        GlobalWaterData().Load(dir + "waterdata.bin");
    InstallFarClipHooks();
}
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID)
{
    if (reason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(instance);
        Attach(instance);
    }
    return TRUE;
}

extern "C" int __cdecl vf_loader_anchor()
{
    return 1;
}

extern "C" IDirect3D9* __cdecl vf_test_wrap_direct3d9(Direct3DCreate9Fn realCreate, UINT sdkVersion)
{
    SetRealDirect3DCreate9(realCreate);
    AllowFogOnNewDevices(true);
    return WrappedDirect3DCreate9(sdkVersion);
}

extern "C" void __cdecl vf_test_set_config(const Config* cfg)
{
    GlobalConfig().Override(*cfg);
    LogSetLevel(cfg->logLevel);
}

extern "C" void __cdecl vf_test_get_config(Config* out)
{
    *out = GlobalConfig().Get();
}

extern "C" void __cdecl vf_test_force_depth_write(int force)
{
    ForceDepthWrite(LatestFogDevice(), force != 0);
}

extern "C" int __cdecl vf_test_render(const FrameInputs* in, const char** skipReason)
{
    return RenderFog(LatestFogDevice(), *in, GlobalConfig().Get(), skipReason) ? 1 : 0;
}

extern "C" void __cdecl vf_test_suppress_depth_write(int suppress)
{
    SuppressDepthWrite(LatestFogDevice(), suppress != 0);
}

extern "C" int __cdecl vf_test_adaptive_lighting_history()
{
    return AdaptiveLightingHistory(LatestFogDevice()) ? 1 : 0;
}

extern "C" int __cdecl vf_test_overlay_visible()
{
    return OverlayVisible() ? 1 : 0;
}

extern "C" void __cdecl vf_test_draw_overlay()
{
    DrawOverlay(RealDevice(LatestFogDevice()));
}

extern "C" int __cdecl vf_test_assign_water_data(const WaterPreset* presets, int presetCount, const WaterFftTile* tiles,
                                                 int tileCount, const WaterMaskView* masks, int maskCount)
{
    return GlobalWaterData().Assign(presets, presetCount, tiles, tileCount, masks, maskCount) ? 1 : 0;
}

extern "C" int __cdecl vf_test_load_water_data(const char* path)
{
    return GlobalWaterData().Load(path) ? 1 : 0;
}

extern "C" int __cdecl vf_test_water_begin(const FrameInputs* in, const WaterInputs* water, const char** skipReason)
{
    return BeginWaterPass(LatestFogDevice(), *in, *water, GlobalConfig().Get(), skipReason) ? 1 : 0;
}

extern "C" void __cdecl vf_test_water_tag(int waterClass)
{
    TagWaterDraw(LatestFogDevice(), static_cast<WaterClass>(waterClass));
}

extern "C" void __cdecl vf_test_water_untag()
{
    UntagWaterDraw(LatestFogDevice());
}

extern "C" int __cdecl vf_test_water_end(const char** skipReason)
{
    const WaterPassEnd end = EndWaterPass(LatestFogDevice());
    if (skipReason)
        *skipReason = end.skipReason;
    return end.shaded ? 1 : 0;
}

extern "C" void __cdecl vf_test_set_water_seconds(double seconds)
{
    OverrideWaterSeconds(seconds);
}

extern "C" void __cdecl vf_test_disable_wave_simulation(int disabled)
{
    DisableWaveSimulation(disabled != 0);
}

extern "C" void __cdecl vf_test_force_packed_water_depth(int forced)
{
    ForcePackedWaterDepth(forced != 0);
}

extern "C" int __cdecl vf_test_transparent_liquids_queued(const void* liquidRenderer)
{
    return engine::TransparentLiquidsQueued(liquidRenderer) ? 1 : 0;
}

extern "C" void __cdecl vf_test_fail_water_in_window(int stage)
{
    InjectWaterFault(static_cast<WaterFaultStage>(stage));
}

extern "C" int __cdecl vf_test_water_data_loaded()
{
    return GlobalWaterData().Loaded() ? 1 : 0;
}

extern "C" unsigned __cdecl vf_test_water_resources_held()
{
    return HeldWaterResources(LatestFogDevice());
}

extern "C" int __cdecl vf_test_water_mask_pool()
{
    return WaterFoamMaskPool(LatestFogDevice());
}

extern "C" void __cdecl vf_test_force_water_summary()
{
    ForceWaterSummary();
}

extern "C" void __cdecl vf_test_water_abort()
{
    AbortWaterPass(LatestFogDevice());
}

extern "C" void __cdecl vf_test_use_water_hook_client(const FrameInputs* in, const WaterInputs* water)
{
    UseTestWaterClient(*in, *water);
}

extern "C" void __cdecl vf_test_hook_water_pass_begin(const void* liquidRenderer)
{
    vf_on_water_pass_begin(liquidRenderer);
}

extern "C" int __cdecl vf_test_hook_water_draw_tag(const void* liquidSettings)
{
    return TagHookedWaterDraw(liquidSettings) ? 1 : 0;
}

extern "C" void __cdecl vf_test_hook_water_draw_untag()
{
    UntagHookedWaterDraw();
}

extern "C" void __cdecl vf_test_hook_water_pass_end()
{
    vf_on_water_pass_end();
}

extern "C" void __cdecl vf_test_hook_frame_end()
{
    vf_on_frame_end();
}

extern "C" int __cdecl vf_test_water_armed()
{
    return WaterPassArmed(LatestFogDevice()) ? 1 : 0;
}

extern "C" int __cdecl vf_test_water_status(const char** reason)
{
    const WaterFrameStatus status = LastWaterFrameStatus();
    *reason = status.reason;
    return status.drawn ? 1 : 0;
}

extern "C" const void* __cdecl vf_test_water_pass_thunk(uintptr_t target)
{
    return RetargetWaterPassThunk(target);
}

extern "C" void __cdecl vf_test_water_pass_begin_reuses_argument_slot(int reuse)
{
    ReuseWaterPassBeginArgumentSlot(reuse != 0);
}

extern "C" void __cdecl vf_test_record_fog_frame(int rendered, int cameraUnderLiquid, const char* skip)
{
    RecordHookedFogFrame(rendered != 0, cameraUnderLiquid != 0, skip);
}
