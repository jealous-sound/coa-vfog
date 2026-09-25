#pragma once

#include <windows.h>
#include <d3d9.h>

#include "fog_data.h"
#include "engine_lights.h"

#include <cstdint>

struct FrameInputs
{
    float cameraRelativeView[16];
    float glProjection[16];
    float camPos[3];
    float camTarget[3];
    D3DVIEWPORT9 viewport;
    float dayFraction;
    float toLight[3];
    bool lightIsMoon;
    uint32_t fogColor;
    uint32_t sunColor;
    uint32_t directColor;
    uint32_t ambientColor;
    float fogStart;
    float fogEnd;
    float zoneFogDistance;
    float farClip;
    float clientGlowAmount;
    bool inLiquid;
    int mapId;
    LightParamsSelection lightParams;
    LocalLightInputs localLights;
};

namespace engine
{
constexpr uint32_t kClientTimestamp = 0x4C2452FE;

constexpr uintptr_t kGetProcAddressSlot = 0x00B2ED98;
constexpr uintptr_t kGetProcAddressThunk = 0x0041C654;

constexpr uintptr_t kWorldRenderSite = 0x004FB03D;
constexpr uintptr_t kWorldRenderTarget = 0x004F8EA0;
constexpr uintptr_t kOpaqueM2PassSite = 0x004F911D;
constexpr uintptr_t kOpaqueM2PassTarget = 0x00823CB0;
constexpr uintptr_t kLiquidSurfaceSite = 0x004F9170;
constexpr uintptr_t kLiquidSurfaceTarget = 0x0077F020;
constexpr uintptr_t kScreenEffectsSite = 0x004F9281;
constexpr uintptr_t kScreenEffectsTarget = 0x008C1010;

using FarClipClampFn = float(__cdecl*)(float farClipSetting, int mapId);
constexpr uintptr_t kFarClipClamp = 0x00780770;
constexpr uintptr_t kFarClipCVarSetSite = 0x00780810;
constexpr uintptr_t kFarClipMapLoadSite = 0x00781444;

bool IsSupportedClient();
void* GameD3DDevice();
bool CameraInLiquid();

constexpr int kDayNightFogGroupCount = 2;
constexpr int kFrameInputsFogGroup = 1;

struct StockFog
{
    float start[kDayNightFogGroupCount];
    float end[kDayNightFogGroupCount];
};
StockFog ReadStockFog();
void WriteStockFog(const StockFog& fog);

void CaptureOpaqueState(IDirect3DDevice9* device);
bool HasOpaqueState();
void ClearOpaqueState();

bool BuildFrameInputs(FrameInputs& out);
}
