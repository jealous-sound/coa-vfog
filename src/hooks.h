#pragma once

#include <cstdint>

struct FrameInputs;
struct WaterInputs;

bool InstallEngineHooks();

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

extern "C" void __cdecl vf_on_frame_end();
extern "C" void __cdecl vf_on_water_pass_begin(const void* liquidRenderer);
extern "C" void __cdecl vf_on_water_pass_end();

void RecordHookedFogFrame(bool rendered, bool cameraUnderLiquid, const char* skip);
void UseTestWaterClient(const FrameInputs& in, const WaterInputs& water);
const void* RetargetWaterPassThunk(uintptr_t target);
void ReuseWaterPassBeginArgumentSlot(bool reuse);
bool TagHookedWaterDraw(const void* liquidSettings);
void UntagHookedWaterDraw();
