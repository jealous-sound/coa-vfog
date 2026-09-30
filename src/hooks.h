#pragma once

#include <cstdint>

struct FrameInputs;
struct WaterInputs;
struct M2BatchFogArgs;

namespace engine
{
struct StockFog;
}

bool InstallEngineHooks();
bool InstallTransparentFogHooks();

void InstallFarClipHooks();

struct FogFrameStatus
{
    bool drawn;
    const char* reason;
};

FogFrameStatus LastFogFrameStatus();

bool InstallWaterHooks();

struct WaterFrameStatus
{
    bool drawn;
    const char* reason;
};

WaterFrameStatus LastWaterFrameStatus();

extern "C" void __cdecl vf_on_frame_begin();
extern "C" void __cdecl vf_on_frame_end();
extern "C" void __cdecl vf_on_liquid_end();
extern "C" void __cdecl vf_on_transparents_begin();
extern "C" void __cdecl vf_on_world_done();
extern "C" void __cdecl vf_on_m2_batch_fog(M2BatchFogArgs* args);
extern "C" void __cdecl vf_on_water_pass_begin(const void* liquidRenderer);
extern "C" void __cdecl vf_on_water_pass_end();

void RecordHookedFogFrame(bool rendered, bool cameraUnderLiquid, const char* skip);
void UseTestFogClient(const FrameInputs& in);
engine::StockFog TestClientStockFog();
void SetTestClientStockFog(const engine::StockFog& fog);
const void* RetargetM2BatchFogThunk(uintptr_t target);
const void* RetargetGlarePassThunk(uintptr_t target);
void UseTestWaterClient(const FrameInputs& in, const WaterInputs& water);
const void* RetargetWaterPassThunk(uintptr_t target);
void ReuseWaterPassBeginArgumentSlot(bool reuse);
bool TagHookedWaterDraw(const void* liquidSettings);
void UntagHookedWaterDraw();
