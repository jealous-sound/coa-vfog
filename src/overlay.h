#pragma once

#include <windows.h>
#include <d3d9.h>

void AttachOverlay(IDirect3DDevice9* device, HWND window);
void DetachOverlay(IDirect3DDevice9* device);
void ReleaseOverlayDeviceObjects(IDirect3DDevice9* device);
void DrawOverlay(IDirect3DDevice9* device);
bool OverlayVisible();
