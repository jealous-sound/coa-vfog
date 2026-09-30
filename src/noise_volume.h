#pragma once

#include <d3d9.h>

constexpr UINT kDensityNoiseSize = 32;
constexpr UINT kAuthoredNoiseSize = 64;

bool CreateDensityNoise(IDirect3DDevice9* device, IDirect3DVolumeTexture9** output);

void PrepareAuthoredNoise();
BYTE AuthoredNoiseTexel(UINT x, UINT y, UINT z);
bool CreateAuthoredNoise(IDirect3DDevice9* device, IDirect3DVolumeTexture9** output);
