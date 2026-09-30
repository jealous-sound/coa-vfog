#include "vf_density_variation.hlsli"
#include "vf_authored_noise.hlsli"

float4 cDensityProbeOrigin : register(c0);
float4 cDensityProbeStep : register(c1);

static const int kProbedNoiseLayer = 0;

float4 main(float2 pixel : VPOS) : COLOR0
{
    float3 position = cDensityProbeOrigin.xyz + float3(pixel * cDensityProbeStep.xy, 0);
    float value = DensityVariation(position);
    return float4(value * 0.5, LayerNoiseDensity(kProbedNoiseLayer, position),
                  LayerNoiseSample(kProbedNoiseLayer, position), 1);
}
