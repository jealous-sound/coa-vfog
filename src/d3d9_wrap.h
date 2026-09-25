#pragma once

#include "config.h"
#include "engine.h"

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
void SuppressDepthWrite(FogDevice* device, bool suppress);
bool RenderFog(FogDevice* device, const FrameInputs& in, const Config& cfg, const char** skipReason);
bool AdaptiveLightingHistory(FogDevice* device);
