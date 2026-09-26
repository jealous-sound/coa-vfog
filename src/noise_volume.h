#pragma once

#include <d3d9.h>

constexpr UINT kDensityNoiseSize = 32;

bool CreateDensityNoise(IDirect3DDevice9* device, IDirect3DVolumeTexture9** output);
