#pragma once

#include <d3d9.h>

#include <cstddef>
#include <string>
#include <vector>

constexpr UINT kFogMaterialConstantCount = 3;
constexpr UINT kFogMaterialClipTexcoord = 7;

struct FogMaterialResources
{
    UINT sampler = 0;
    UINT constantBase = 0;
};

struct FogMaterialShaders
{
    std::vector<DWORD> vertex;
    std::vector<DWORD> pixel;
    FogMaterialResources resources;
};

bool InstrumentFogMaterialShaders(const DWORD* vertex, size_t vertexWords, const DWORD* pixel, size_t pixelWords,
                                  FogMaterialShaders& result, std::string& failure);
