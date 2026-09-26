#pragma once

#include "config.h"
#include "engine.h"
#include "fog_data.h"

struct FogLayer
{
    float start;
    float density;
    float g;
    float isotropic;
    float emissive[3];
    float strength;
    float diffuse[3];
    float exponent;
    float upperHeight;
    float upperFalloff;
    float lowerHeight;
    float lowerFalloff;
    float shadowEmissive[3];
    float shadowDensity;
    float shadowed;
    float skyFalloff;
    float endDistance;
    float densityVariation;
};

static_assert(sizeof(FogLayer) == 6 * sizeof(float[4]), "FogLayer uploads as a march layer's six float4 registers");

constexpr int kFogLayers = 4;
constexpr int kSceneLayers = 3;
constexpr int kDistanceFogLayer = kSceneLayers;
static_assert(kDistanceFogLayer + 1 == kFogLayers, "the distance fog follows the scene layers");

struct FogParams
{
    FogLayer layers[kFogLayers];
    float lightColor[3];
    float rayColor[3];
    float lightVisibility;
    float lightAboveHorizon;
    float shadowedLayerLightScale;
    float directLightMatch;
    float maxDistance;
    float horizonStart;
    float farClip;
    float referenceZ;
    float farLimit;
    bool linear;
    bool authored;
};

void UnpackColor(uint32_t argb, float* rgb);
FogParams BuildFogParams(const FrameInputs& in, const Config& cfg, const AuthoredFog* authored);

void Mul4x4(const float* a, const float* b, float* out);
bool Invert4x4(const float* m, float* out);
void TransformDirection(const float* v, const float* m, float* out);
