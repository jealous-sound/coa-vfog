#include "vf_common.hlsli"

#ifndef STEPS
#define STEPS 24
#endif

static const int kShadowSteps = 6;
static const int kFogLayers = 4;
static const int kRegistersPerLayer = 6;
static const float kShadowMinViewZ = 0.3;
static const float kShadowDepthBias = 0.3;
static const float kGoldenRatioFraction = 0.618034;

float4 cLight : register(c9);
float4 cShadowMarch : register(c10);
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
};

FogLayer LoadFogLayer(int index)
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
    return layer;
}

float3 DirectionToLightView()
{
    return cLight.xyz;
}

float LightAboveHorizon()
{
    return cLight.w;
}

float ShadowMinStep()
{
    return cShadowMarch.x;
}

float ShadowStepPerYard()
{
    return cShadowMarch.y;
}

bool ShadowsEnabled()
{
    return cShadowMarch.z > 0;
}

float ShadowThicknessInSteps()
{
    return cShadowMarch.w;
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

bool SceneOccludes(float sceneDepth, float sampleViewZ, float occluderThickness)
{
    float sceneViewZ = LinearDepth(sceneDepth);
    return !BeyondFarClip(sceneDepth) && sceneViewZ < sampleViewZ - kShadowDepthBias &&
           sceneViewZ > sampleViewZ - occluderThickness;
}

float ScreenSpaceSunVisibility(float3 viewPosition, float sampleDistance, float jitter)
{
    float stepLength = max(ShadowMinStep(), sampleDistance * ShadowStepPerYard());
    float occluderThickness = stepLength * ShadowThicknessInSteps();
    float3 stepToLight = DirectionToLightView() * stepLength;
    float3 samplePosition = viewPosition + stepToLight * jitter;
    float visibility = 1;
    [loop] for (int k = 0; k < kShadowSteps; k++)
    {
        samplePosition += stepToLight;
        [branch] if (samplePosition.z < kShadowMinViewZ)
            break;
        float2 pixel = ViewToPixel(samplePosition);
        [branch] if (OutsideViewport(pixel))
            break;
        [branch] if (SceneOccludes(SampleDepth(sDepth, pixel), samplePosition.z, occluderThickness))
        {
            visibility = 0;
            break;
        }
    }
    return visibility;
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

void AccumulateLayer(FogLayer layer, float phase, float skyDensityScale, float stepStart, float stepEnd,
                     float jitter, float cameraHeight, float heightPerYard, float sunVisibility,
                     inout float3 radiance, inout float opticalDepth)
{
    float layerStart = max(stepStart, layer.start);
    float layerLength = max(min(stepEnd, layer.limit) - layerStart, 0);
    float sampleDistance = layerStart + layerLength * jitter;
    float sampleHeight = cameraHeight + heightPerYard * sampleDistance;
    float distanceCurve = DistanceCurve(layer, sampleDistance);
    float heightProfile = HeightProfile(layer, sampleHeight);
    float directLight = lerp(1, sunVisibility, layer.shadowed);
    float shadowDensityScale = lerp(1, lerp(layer.shadowDensity, 1, sunVisibility), layer.shadowed);
    float layerOpticalDepth = layer.density * skyDensityScale * layerLength * distanceCurve * heightProfile *
                              shadowDensityScale;
    float3 emissive = lerp(layer.emissive, lerp(layer.shadowEmissive, layer.emissive, sunVisibility), layer.shadowed);
    radiance += (layer.diffuse * (directLight * phase) + emissive) * layerOpticalDepth;
    opticalDepth += layerOpticalDepth;
}

float StepJitter(float2 lowResTexel)
{
    return JitterEnabled() ? frac(InterleavedGradientNoise(lowResTexel) + FrameIndex() * kGoldenRatioFraction) : 0.5;
}

float4 main(float2 lowResTexel : VPOS) : COLOR0
{
    float2 pixel = LowResTexelToFullPixel(lowResTexel);
    float3 viewRay = ViewRayAtUnitDepth(pixel);
    float distancePerViewZ = length(viewRay);
    float3 viewDirection = viewRay / distancePerViewZ;
    float depth = SampleDepth(sDepth, pixel);
    float skyMask = IsSky(depth) ? 1 : 0;
    float viewZ = LinearDepth(depth);
    float horizonBlend = max(BeyondFarClip(depth) ? 1 : 0, smoothstep(HorizonBlendStart(), FarClip(), viewZ));
    viewZ = lerp(viewZ, MaxFogDistance(), horizonBlend);
    float marchLength = min(viewZ * distancePerViewZ, MaxFogDistance());

    float jitter = StepJitter(lowResTexel);
    float3 cameraWorld = CameraPositionWorld();
    float3 directionWorld = ViewToWorldDirection(viewDirection);
    float cosToLight = dot(DirectionToLightView(), viewDirection);
    float upward = max(directionWorld.z, 0);
    float riseLevelledAtHorizon = lerp(directionWorld.z, upward, horizonBlend);

    float phase[kFogLayers];
    float skyDensityScale[kFogLayers];
    [unroll] for (int i = 0; i < kFogLayers; i++)
    {
        FogLayer layer = LoadFogLayer(i);
        phase[i] = lerp(PhaseHG(layer.g, cosToLight), 1, layer.isotropic);
        skyDensityScale[i] = skyMask > 0 ? exp(-upward * layer.skyFalloff) : 1;
    }

    float3 inScatteredRadiance = 0;
    float transmittance = 1;
    const float stepFraction = 1.0 / STEPS;
    [loop] for (int s = 0; s < STEPS; s++)
    {
        float startFraction = s * stepFraction;
        float endFraction = startFraction + stepFraction;
        float stepStart = marchLength * startFraction * startFraction;
        float stepEnd = marchLength * endFraction * endFraction;
        float sampleDistance = lerp(stepStart, stepEnd, jitter);
        float sunVisibility = LightAboveHorizon();
        [branch] if (ShadowsEnabled())
            sunVisibility *= ScreenSpaceSunVisibility(viewDirection * sampleDistance, sampleDistance, jitter);
        float3 stepRadiance = 0;
        float stepOpticalDepth = 0;
        [unroll] for (int j = 0; j < kFogLayers; j++)
            AccumulateLayer(LoadFogLayer(j), phase[j], skyDensityScale[j], stepStart, stepEnd, jitter, cameraWorld.z,
                            riseLevelledAtHorizon, sunVisibility, stepRadiance, stepOpticalDepth);
        [branch] if (stepOpticalDepth > 1e-6)
        {
            float stepTransmittance = exp(-stepOpticalDepth);
            inScatteredRadiance += transmittance * stepRadiance * ((1 - stepTransmittance) / stepOpticalDepth);
            transmittance *= stepTransmittance;
        }
    }
    return float4(inScatteredRadiance, 1 - transmittance);
}
