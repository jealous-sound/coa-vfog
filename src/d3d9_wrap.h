#pragma once

#include "config.h"
#include "engine.h"
#include "fog_volume.h"

#include <d3d9.h>

using Direct3DCreate9Fn = IDirect3D9*(WINAPI*)(UINT);

void SetRealDirect3DCreate9(Direct3DCreate9Fn fn);
IDirect3D9* WINAPI WrappedDirect3DCreate9(UINT sdkVersion);

void AllowFogOnNewDevices(bool allowedOnNewDevices);

class FogDevice;
FogDevice* LatestFogDevice();
FogDevice* WrapperOrLatestFogDevice(void* gameDevice);
bool IsWrapperOf(FogDevice* device, void* gameDevice);
IDirect3DDevice9* RealDevice(FogDevice* device);
void ForceDepthWrite(FogDevice* device, bool force);
bool RenderFog(FogDevice* device, const FrameInputs& in, const Config& cfg, const char** skipReason);
bool BeginMaterialFog(FogDevice* device, const MaterialFogVolume& volume);
bool BeginRenderedMaterialFog(FogDevice* device);
bool BeginNativeGlare(FogDevice* device);
void EndNativeGlare(FogDevice* device);
bool MaterialFogCompatible(FogDevice* device);
bool AdaptiveLightingHistory(FogDevice* device);
void EndMaterialFog(FogDevice* device);
