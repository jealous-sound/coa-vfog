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
    GlobalWaterData().Load(dir + "waterdata.bin");

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
    if (engineHooks)
        InstallWaterHooks();
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

extern "C" void __cdecl vf_test_water_end()
{
    EndWaterPass(LatestFogDevice());
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
