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

constexpr float kMoonLightScale = 0.35f;

constexpr int kFogLayers = 4;
constexpr int kSceneLayers = 3;
constexpr int kDistanceFogLayer = kSceneLayers;
static_assert(kDistanceFogLayer + 1 == kFogLayers, "the distance fog follows the scene layers");

constexpr float kNoiseCurveContrast = 20.0f;
constexpr float kNoiseCurvePivot = 0.5f;
constexpr float kNoiseCurveMean = 0.5f;

struct LayerNoise
{
    float alpha;
    float octaveWeight[kAuthoredNoiseOctaves];
    float inverseTileYards[kAuthoredNoiseOctaves];
    float velocity[kAuthoredNoiseOctaves][3];
    float fade[3];
};

struct LayerNoiseRegisters
{
    float octaveOffsetAndInverseTile[kAuthoredNoiseOctaves][4];
    float fadeAndAlpha[4];
    float octaveWeights[4];
};

static_assert(sizeof(LayerNoiseRegisters) == 4 * sizeof(float[4]),
              "LayerNoiseRegisters uploads as a scene layer's four float4 noise registers");

struct FogParams
{
    FogLayer layers[kFogLayers];
    LayerNoise noise[kSceneLayers];
    float lightColor[3];
    float rayColor[3];
    float lightVisibility;
    float lightAboveHorizon;
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
bool AnyLayerNoise(const FogParams& fog);
float EnergyNormalisedPhaseScale(float g);

float NoiseDensityCurve(float x);
float MeanNoiseDensity(const LayerNoise& noise);
void MeanNoiseEmissive(const LayerNoise& noise, const float* emissive, float* out);
FogLayer MeanNoiseLayer(const FogLayer& layer, const LayerNoise& noise, float lightAboveHorizon);
FogParams WithMeanNoise(const FogParams& fog);

class AuthoredNoiseScroll
{
public:
    void Advance(const FogParams& fog, const float* camera, double seconds);
    void Registers(const FogParams& fog, LayerNoiseRegisters* out) const;

private:
    double m_phaseInTiles[kSceneLayers][kAuthoredNoiseOctaves][3] = {};
    double m_inverseTileYards[kSceneLayers][kAuthoredNoiseOctaves] = {};
};

void Mul4x4(const float* a, const float* b, float* out);
bool Invert4x4(const float* m, float* out);
void TransformDirection(const float* v, const float* m, float* out);
