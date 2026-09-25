#pragma once

#include <d3d9.h>

struct MaterialFogVolume
{
    IDirect3DTexture9* atlas = nullptr;
    IDirect3DSurface9* target = nullptr;
    IDirect3DSurface9* depth = nullptr;
    D3DVIEWPORT9 viewport = {};
    float maxDistance = 0.0f;
    float exposure = 1.0f;
    UINT slices = 0;
    UINT columns = 0;
    UINT rows = 0;
    UINT width = 0;
    UINT height = 0;
    bool linear = false;
    float glow = 0.0f;
    bool sceneCopy = true;
};
