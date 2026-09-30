#pragma once

#include <cstdint>

struct FrameInputs;
struct WaterInputs;

bool InstallEngineHooks();
void EnableForeverLookOnHookedClient();

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
extern "C" void __cdecl vf_on_opaque_done();
extern "C" void __cdecl vf_on_world_done();
extern "C" void __cdecl vf_on_water_pass_begin(const void* liquidRenderer);
extern "C" void __cdecl vf_on_water_pass_end();

void RecordHookedFogFrame(bool rendered, bool cameraUnderLiquid, const char* skip);
void UseTestWorldClient(const FrameInputs& in, bool glowScreenEffectRuns);
void SimulateFogHookFailure(bool failed);
void UseTestWaterClient(const FrameInputs& in, const WaterInputs& water);
unsigned TestWaterContactReads();
void RefuseTestWaterContacts(bool refused);
const void* RetargetWaterPassThunk(uintptr_t target);
void ReuseWaterPassBeginArgumentSlot(bool reuse);
bool TagHookedWaterDraw(const void* liquidSettings);
void UntagHookedWaterDraw();
