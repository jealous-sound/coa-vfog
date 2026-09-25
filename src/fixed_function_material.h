#pragma once

#include <d3d9.h>

#include <string>
#include <vector>

constexpr UINT kFixedFunctionTextureStages = 8;
constexpr UINT kFixedFunctionVertexConstantCount = 40;
constexpr UINT kFixedFunctionPixelConstantCount = 9;

struct FixedFunctionTextureStage
{
    DWORD colourOperation = D3DTOP_DISABLE;
    DWORD alphaOperation = D3DTOP_DISABLE;
    DWORD colourArguments[3] = {};
    DWORD alphaArguments[3] = {};
    DWORD result = D3DTA_CURRENT;
    DWORD coordinateIndex = 0;
    DWORD transform = D3DTTFF_DISABLE;
    bool texture = false;
    bool coordinates = false;
};

struct FixedFunctionMaterialState
{
    std::vector<DWORD> key;
    std::vector<D3DVERTEXELEMENT9> declaration;
    FixedFunctionTextureStage stages[kFixedFunctionTextureStages];
    float vertexConstants[kFixedFunctionVertexConstantCount][4] = {};
    float pixelConstants[kFixedFunctionPixelConstantCount][4] = {};
    UINT vertexConstantCount = 0;
    UINT pixelConstantCount = 0;
    UINT stageCount = 0;
    bool vertex = false;
    bool pixel = false;
    bool diffuse = false;
    bool specular = false;
    bool addSpecular = false;
};

bool CaptureFixedFunctionMaterial(IDirect3DDevice9* device, bool vertex, bool pixel,
                                  FixedFunctionMaterialState& state, std::string& failure);
bool BuildFixedFunctionMaterialShaders(const FixedFunctionMaterialState& state, std::vector<DWORD>& vertex,
                                       std::vector<DWORD>& pixel, std::string& failure);
