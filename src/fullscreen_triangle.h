#pragma once

#include <d3d9.h>

inline constexpr float kFullscreenTriangleLow = -1.5f;
inline constexpr float kFullscreenTriangleHigh = 4.5f;
inline constexpr float kFullscreenTriangle[3][4] = {
    {kFullscreenTriangleLow, kFullscreenTriangleLow, 0.0f, 1.0f},
    {kFullscreenTriangleLow, kFullscreenTriangleHigh, 0.0f, 1.0f},
    {kFullscreenTriangleHigh, kFullscreenTriangleLow, 0.0f, 1.0f},
};

inline HRESULT DrawFullscreenTriangle(IDirect3DDevice9* dev)
{
    return dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, kFullscreenTriangle, sizeof(kFullscreenTriangle[0]));
}
