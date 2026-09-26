#pragma once

#include <d3d9.h>

constexpr int kWorldShadowMapCount = 4;

struct WorldShadowInputs
{
    IDirect3DTexture9* textures[kWorldShadowMapCount] = {};
    float viewToShadow[kWorldShadowMapCount][12] = {};
    float texelSize[kWorldShadowMapCount][2] = {};
    float toLight[3] = {};
    int count = 0;
    bool hardwareComparison = false;
};

namespace engine
{
bool AcquireWorldShadows(IDirect3DDevice9* device, WorldShadowInputs& out);
void ReleaseWorldShadows(WorldShadowInputs& in);
}
