#include "vf_common.hlsli"
#include "vf_density_variation.hlsli"
#include "vf_authored_noise.hlsli"

#ifndef STEPS
#define STEPS 24
#endif

#ifndef LOCAL_LIGHTS
#define LOCAL_LIGHTS 1
#endif

static const int kFogLayers = 4;
static const int kRegistersPerLayer = 6;
static const float kGoldenRatioFraction = 0.618034;
static const bool kMarchesLocalLights = LOCAL_LIGHTS;
static const bool kUnrollsLayers = !kMarchesLocalLights;

float4 cLight : register(c9);
float4 cMarch : register(c11);
float4 cLayers[kFogLayers * kRegistersPerLayer] : register(c12);

sampler2D sDepth : register(s0);

struct FogLayer
{
    float start;
    float density;
    float g;
    float isotropic;
    float3 emissive;
    float strength;
    float3 diffuse;
    float exponent;
    float upperHeight;
    float upperFalloff;
    float lowerHeight;
    float lowerFalloff;
    float3 shadowEmissive;
    float shadowDensity;
    float shadowed;
    float skyFalloff;
    float limit;
    float densityVariation;
};

FogLayer LoadConstantFogLayer(int index)
{
    int first = index * kRegistersPerLayer;
    FogLayer layer;
    layer.start = cLayers[first].x;
    layer.density = cLayers[first].y;
    layer.g = cLayers[first].z;
    layer.isotropic = cLayers[first].w;
    layer.emissive = cLayers[first + 1].rgb;
    layer.strength = cLayers[first + 1].w;
    layer.diffuse = cLayers[first + 2].rgb;
    layer.exponent = cLayers[first + 2].w;
    layer.upperHeight = cLayers[first + 3].x;
    layer.upperFalloff = cLayers[first + 3].y;
    layer.lowerHeight = cLayers[first + 3].z;
    layer.lowerFalloff = cLayers[first + 3].w;
    layer.shadowEmissive = cLayers[first + 4].rgb;
    layer.shadowDensity = cLayers[first + 4].w;
    layer.shadowed = cLayers[first + 5].x;
    layer.skyFalloff = cLayers[first + 5].y;
    layer.limit = cLayers[first + 5].z;
    layer.densityVariation = cLayers[first + 5].w;
    return layer;
}

FogLayer LoadFogLayer(int index)
{
    [branch] if (index == 0)
        return LoadConstantFogLayer(0);
    [branch] if (index == 1)
        return LoadConstantFogLayer(1);
    [branch] if (index == 2)
        return LoadConstantFogLayer(2);
    return LoadConstantFogLayer(3);
}

float3 DirectionToLightView()
{
    return cLight.xyz;
}

float LightAboveHorizon()
{
    return cLight.w;
}

bool JitterEnabled()
{
    return cMarch.x > 0;
}

float DistanceCurveRange()
{
    return cMarch.y;
}

float HorizonBlendStart()
{
    return cMarch.z;
}

float FarClip()
{
    return cMarch.w;
}

float PhaseHG(float g, float cosAngle)
{
    float r = (1 - g) / sqrt(max(1 + g * g - 2 * g * cosAngle, 1e-6));
    return r * r * r;
}

float DistanceCurve(FogLayer layer, float sampleDistance)
{
    return 1 + layer.strength * pow(saturate(max(sampleDistance - layer.start, 0) / DistanceCurveRange()) + 1e-6,
                                    layer.exponent);
}

float HeightProfile(FogLayer layer, float height)
{
    return saturate(exp((layer.upperHeight - height) * layer.upperFalloff)) *
           saturate(exp((height - layer.lowerHeight) * layer.lowerFalloff));
}

float HorizonShadow(FogLayer layer)
{
    return layer.shadowed * (1 - LightAboveHorizon());
}

float ShadowDensityScale(FogLayer layer)
{
    return lerp(1, layer.shadowDensity, HorizonShadow(layer));
}

float SkyDensityScale(FogLayer layer, float skyMask, float upward)
{
    return skyMask > 0 ? exp(-upward * layer.skyFalloff) : 1;
}

float LayerPhase(FogLayer layer, float cosAngle)
{
    return lerp(PhaseHG(layer.g, cosAngle), 1, layer.isotropic);
}

struct MarchRay
{
    float3 viewDirection;
    float3 directionWorld;
    float cameraHeight;
    float heightPerYard;
};

float DensityVariationAlongRay(MarchRay ray, float distanceAlongRay)
{
    return DensityVariation(CameraPositionWorld() + ray.directionWorld * distanceAlongRay);
}

float WeightedVariation(float variation, float weight)
{
    return kSamplesAuthoredNoise ? lerp(1, variation, weight) : variation;
}

float DensityProfile(FogLayer layer, MarchRay ray, float distanceAlongRay, float variation)
{
    float density = DistanceCurve(layer, distanceAlongRay) *
                    HeightProfile(layer, ray.cameraHeight + ray.heightPerYard * distanceAlongRay);
    return layer.densityVariation > 0 ? density * WeightedVariation(variation, layer.densityVariation) : density;
}

#include "vf_local_lights.hlsli"

struct StepVariation
{
    float distance;
    float variation;
};

StepVariation StepVariationAt(MarchRay ray, float stepStart, float stepEnd, float jitter)
{
    StepVariation step;
    step.distance = stepStart + (stepEnd - stepStart) * jitter;
    step.variation = DensityVariationAlongRay(ray, step.distance);
    return step;
}

float LayerVariation(StepVariation step, MarchRay ray, float sampleDistance)
{
    [branch] if (kUnrollsLayers && sampleDistance == step.distance)
        return step.variation;
    return DensityVariationAlongRay(ray, sampleDistance);
}

void AccumulateLayer(FogLayer layer, int layerIndex, float noiseDensity, float cosToLight, float skyDensityScale,
                     float stepStart, float stepEnd, float jitter, MarchRay ray, StepVariation stepVariation,
                     inout float3 radiance, inout float opticalDepth)
{
    float layerStart = max(stepStart, layer.start);
    float layerLength = max(min(stepEnd, layer.limit) - layerStart, 0);
    [branch] if (layerLength <= 0 || layer.density <= 0)
        return;
    float shadow = HorizonShadow(layer);
    float shadowDensityScale = ShadowDensityScale(layer);
    float sampleDistance = layerStart + layerLength * jitter;
    float sampleHeight = ray.cameraHeight + ray.heightPerYard * sampleDistance;
    float distanceCurve = DistanceCurve(layer, sampleDistance);
    float heightProfile = HeightProfile(layer, sampleHeight);
    float variation = 1;
    [branch] if (layer.densityVariation > 0 && cDensityVariation.x > 0)
        variation = WeightedVariation(LayerVariation(stepVariation, ray, sampleDistance), layer.densityVariation);
    float directLight = 1 - shadow;
    float layerOpticalDepth = layer.density * skyDensityScale * layerLength * distanceCurve * heightProfile *
                              shadowDensityScale * variation * noiseDensity;
    float3 emissive = lerp(layer.emissive, layer.shadowEmissive, shadow);
    [flatten] if (kSamplesAuthoredNoise && layerIndex < kNoisyLayers)
        emissive = lerp(emissive, LayerNoiseFade(layerIndex), 1 - noiseDensity);
    float phase = LayerPhase(layer, cosToLight);
    radiance += (layer.diffuse * (directLight * phase) + emissive) * layerOpticalDepth;
    opticalDepth += layerOpticalDepth;
}

float3 StepSamplePosition(MarchRay ray, float stepStart, float stepEnd, float jitter)
{
    return CameraPositionWorld() + ray.directionWorld * (stepStart + (stepEnd - stepStart) * jitter);
}

void AccumulateLayers(float4 noiseDensities, float cosToLight, float skyMask, float upward, float stepStart,
                      float stepEnd, float jitter, MarchRay ray, inout float3 radiance, inout float opticalDepth)
{
    StepVariation stepVariation = StepVariationAt(ray, stepStart, stepEnd, jitter);
    [branch] if (kUnrollsLayers)
    {
        [unroll] for (int j = 0; j < kFogLayers; j++)
        {
            FogLayer layer = LoadConstantFogLayer(j);
            AccumulateLayer(layer, j, noiseDensities[j], cosToLight, SkyDensityScale(layer, skyMask, upward),
                            stepStart, stepEnd, jitter, ray, stepVariation, radiance, opticalDepth);
        }
    }
    else
    {
        [loop] for (int j = 0; j < kFogLayers; j++)
        {
            FogLayer layer = LoadFogLayer(j);
            float noiseDensity = kSamplesAuthoredNoise ? dot(noiseDensities, j == int4(0, 1, 2, 3) ? 1 : 0) : 1;
            AccumulateLayer(layer, j, noiseDensity, cosToLight, SkyDensityScale(layer, skyMask, upward), stepStart,
                            stepEnd, jitter, ray, stepVariation, radiance, opticalDepth);
        }
    }
}

float LayerMarchEnd(FogLayer layer)
{
    return layer.density > 0 ? layer.limit : 0;
}

float LayersMarchEnd()
{
    return max(max(LayerMarchEnd(LoadConstantFogLayer(0)), LayerMarchEnd(LoadConstantFogLayer(1))),
               max(LayerMarchEnd(LoadConstantFogLayer(2)), LayerMarchEnd(LoadConstantFogLayer(3))));
}

float StepJitter(float2 lowResTexel)
{
    return JitterEnabled() ? frac(InterleavedGradientNoise(lowResTexel) + FrameIndex() * kGoldenRatioFraction) : 0.5;
}

float4 IntegrateFogAtDepth(float2 pixel, float depth, float jitter)
{
    float3 viewRay = ViewRayAtUnitDepth(pixel);
    float distancePerViewZ = length(viewRay);
    float3 viewDirection = viewRay / distancePerViewZ;
    float skyMask = IsSky(depth) ? 1 : 0;
    float viewZ = LinearDepth(depth);
    float horizonBlend = max(BeyondFarClip(depth) ? 1 : 0, smoothstep(HorizonBlendStart(), FarClip(), viewZ));
    viewZ = lerp(viewZ, MaxFogDistance(), horizonBlend);
    float marchLength = min(viewZ * distancePerViewZ, MaxFogDistance());

    float3 cameraWorld = CameraPositionWorld();
    float3 directionWorld = ViewToWorldDirection(viewDirection);
    float cosToLight = dot(DirectionToLightView(), viewDirection);
    float upward = max(directionWorld.z, 0);
    float riseLevelledAtHorizon = lerp(directionWorld.z, upward, horizonBlend);
    MarchRay ray = {viewDirection, directionWorld, cameraWorld.z, riseLevelledAtHorizon};
    ChordCoverage lightCoverage = {0, false, 0, LocalLightCount()};
    float4 lightDensityScales = LayerLightDensityScales(skyMask, upward);

    float layersEnd = LayersMarchEnd();
    float3 inScatteredRadiance = 0;
    float transmittance = 1;
    const float stepFraction = 1.0 / STEPS;
    [loop] for (int s = 0; s < STEPS; s++)
    {
        float startFraction = s * stepFraction;
        float endFraction = startFraction + stepFraction;
        float stepStart = marchLength * startFraction * startFraction;
        [branch] if (kUnrollsLayers && stepStart >= layersEnd)
            break;
        float stepEnd = marchLength * endFraction * endFraction;
        float3 stepRadiance = 0;
        float stepOpticalDepth = 0;
        float4 noiseDensities = LayerNoiseDensities(StepSamplePosition(ray, stepStart, stepEnd, jitter));
        AccumulateLayers(noiseDensities, cosToLight, skyMask, upward, stepStart, stepEnd, jitter, ray, stepRadiance,
                         stepOpticalDepth);
        [branch] if (kMarchesLocalLights && lightCoverage.nextEndpoint <= stepStart)
            lightCoverage = ChordCoverageFrom(lightCoverage, viewDirection, stepStart, marchLength);
        [branch] if (kMarchesLocalLights && StepMeetsLocalLights(lightCoverage, stepEnd))
            stepRadiance += LocalLightScattering(lightCoverage, lightDensityScales, ray, stepStart, stepEnd,
                                                 noiseDensities);
        [branch] if (stepOpticalDepth > 0)
        {
            float stepOpacity = stepOpticalDepth < 1e-3
                ? stepOpticalDepth * (1 - 0.5 * stepOpticalDepth + stepOpticalDepth * stepOpticalDepth / 6)
                : 1 - exp(-stepOpticalDepth);
            inScatteredRadiance += transmittance * stepRadiance * (stepOpacity / stepOpticalDepth);
            transmittance *= 1 - stepOpacity;
        }
    }
    return float4(clamp(inScatteredRadiance, 0, 65504), 1 - transmittance);
}

float4 IntegrateFogAtPixel(float2 pixel, float jitter)
{
    return IntegrateFogAtDepth(pixel, SampleDepth(sDepth, pixel), jitter);
}
