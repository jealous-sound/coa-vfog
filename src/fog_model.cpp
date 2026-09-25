#include "fog_model.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace
{
constexpr float kHazeDensity = 0.00035f;
constexpr float kHazeG = 0.75f;
constexpr float kHazeIsotropic = 0.3f;
constexpr float kHazeFalloff = 1.0f / 220.0f;
constexpr float kHazeSun = 1.3f;
constexpr float kHazeAmbient = 0.8f;
constexpr float kHazeShadowDensity = 0.75f;

constexpr float kGroundStart = 12.0f;
constexpr float kGroundOpticalDepth = 1.1f;
constexpr float kGroundG = 0.4f;
constexpr float kGroundIsotropic = 0.5f;
constexpr float kGroundFalloff = 1.0f / 18.0f;
constexpr float kGroundSun = 0.8f;
constexpr float kGroundAmbient = 0.75f;
constexpr float kGroundShadowDensity = 0.85f;

constexpr float kFarOpticalDepth = 3.0f;
constexpr float kFarRamp = 60.0f;
constexpr float kFarStartFraction = 0.25f;
constexpr float kFarG = 0.2f;
constexpr float kFarIsotropic = 0.7f;
constexpr float kFarSun = 0.45f;
constexpr float kFarAmbient = 0.6f;
constexpr float kFarSkyFalloff = 10.0f;
constexpr float kFarExponent = 2.0f;

constexpr float kClassicUnits = 0.01f;
constexpr float kFogRangeMargin = 300.0f;
constexpr uint32_t kFlagShadowed = 0x1;
constexpr uint32_t kFlagRelativeHeights = 0x2;

constexpr float kDerivedLayerLimit = 1500.0f;

constexpr float kMoonLight = 0.35f;
constexpr float kLightSettingHalfWidth = 0.02f;
constexpr float kMinClassicDirectLuminance = 1.0e-3f;
constexpr float kLuminanceWeights[3] = {0.2126f, 0.7152f, 0.0722f};
constexpr float kReferenceFarTarget = 200.0f;
constexpr float kNoLimit = 1.0e9f;

float SmoothStep(float e0, float e1, float x)
{
    float t = std::clamp((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

void Scale(const float* rgb, float s, float* out)
{
    out[0] = rgb[0] * s;
    out[1] = rgb[1] * s;
    out[2] = rgb[2] * s;
}

void Encode(float* rgb, bool linear)
{
    if (linear)
        for (int i = 0; i < 3; ++i)
            rgb[i] = std::pow(std::max(rgb[i], 0.0f), 2.2f);
}

float Luminance(const float* rgb)
{
    return rgb[0] * kLuminanceWeights[0] + rgb[1] * kLuminanceWeights[1] + rgb[2] * kLuminanceWeights[2];
}

float ReferenceZ(const FrameInputs& in)
{
    float dx = in.camTarget[0] - in.camPos[0];
    float dy = in.camTarget[1] - in.camPos[1];
    float dz = in.camTarget[2] - in.camPos[2];
    if (dx * dx + dy * dy + dz * dz > kReferenceFarTarget * kReferenceFarTarget)
        return in.camPos[2] - 2.0f;
    return std::min(in.camPos[2], in.camTarget[2]) - 1.0f;
}

void Unbounded(FogLayer& l)
{
    l.skyFalloff = 0.0f;
    l.endDistance = kNoLimit;
    l.densityVariation = 1.0f;
}

void DerivedLayers(const FrameInputs& in, const Config& cfg, const FogParams& p, float sunScatter, float fogDistance,
                   const float* fogColor, FogLayer* out)
{
    float foggyZone = std::clamp(700.0f / fogDistance, 0.6f, 2.5f);
    float shadowed = cfg.lightShafts ? 1.0f : 0.0f;

    FogLayer& haze = out[0];
    haze.start = 0.0f;
    haze.density = kHazeDensity * cfg.haze * cfg.density * foggyZone;
    haze.g = kHazeG;
    haze.isotropic = kHazeIsotropic;
    Scale(fogColor, kHazeAmbient * cfg.ambient, haze.emissive);
    Scale(p.lightColor, kHazeSun * sunScatter, haze.diffuse);
    haze.exponent = 1.0f;
    haze.upperHeight = p.referenceZ;
    haze.upperFalloff = kHazeFalloff;
    std::memcpy(haze.shadowEmissive, haze.emissive, sizeof(haze.emissive));
    haze.shadowDensity = kHazeShadowDensity;
    haze.shadowed = shadowed;
    Unbounded(haze);
    haze.endDistance = kDerivedLayerLimit;

    FogLayer& ground = out[1];
    ground.start = kGroundStart;
    ground.density = kGroundOpticalDepth * cfg.groundFog * cfg.density / std::max(fogDistance, 250.0f);
    ground.g = kGroundG;
    ground.isotropic = kGroundIsotropic;
    Scale(fogColor, kGroundAmbient * cfg.ambient, ground.emissive);
    Scale(p.lightColor, kGroundSun * sunScatter, ground.diffuse);
    ground.exponent = 1.0f;
    ground.upperHeight = p.referenceZ;
    ground.upperFalloff = kGroundFalloff;
    std::memcpy(ground.shadowEmissive, ground.emissive, sizeof(ground.emissive));
    ground.shadowDensity = kGroundShadowDensity;
    ground.shadowed = shadowed;
    Unbounded(ground);
    ground.endDistance = kDerivedLayerLimit;

    out[2] = FogLayer{};
    Unbounded(out[2]);
}

void AuthoredLayers(const AuthoredFog& fog, const Config& cfg, const FogParams& p, float sunScatter, FogLayer* out)
{
    for (int i = 0; i < kSceneLayers; ++i)
    {
        FogLayer& l = out[i];
        l = FogLayer{};
        Unbounded(l);
        if (i >= fog.layerCount)
            continue;
        const AuthoredLayer& a = fog.layers[i];
        const float heightBase = a.flags & kFlagRelativeHeights ? p.referenceZ : 0.0f;
        l.start = std::clamp(a.start, 0.0f, std::max(p.maxDistance - kFogRangeMargin, 0.0f));
        l.density = a.density * kClassicUnits * cfg.density;
        l.g = std::clamp(a.g, -0.99f, 0.99f);
        l.isotropic = 0.0f;
        Scale(a.emissive, cfg.ambient, l.emissive);
        l.strength = a.strength;
        Scale(a.diffuse, 1.0f, l.diffuse);
        l.exponent = a.exponent > 0.0f ? a.exponent : 1.0f;
        l.upperHeight = a.upperHeight + heightBase;
        l.upperFalloff = a.upperDensity > 0.0f ? a.upperDensity * kClassicUnits : 0.0f;
        l.lowerHeight = a.lowerHeight + heightBase;
        l.lowerFalloff = a.lowerDensity > 0.0f ? a.lowerDensity * kClassicUnits : 0.0f;
        Scale(a.shadowEmissive, cfg.ambient, l.shadowEmissive);
        l.shadowDensity = a.shadowMultiplier;
        l.shadowed = (a.flags & kFlagShadowed) && cfg.lightShafts ? 1.0f : 0.0f;
        Encode(l.emissive, p.linear);
        Encode(l.diffuse, p.linear);
        Encode(l.shadowEmissive, p.linear);
        Scale(l.diffuse, a.intensity * sunScatter, l.diffuse);
    }
}

float DistanceFogOpticalDepthPerDensity(float farStart, float farLimit, float range)
{
    const float spanFraction = (farLimit - farStart) / range;
    return range * (spanFraction + kFarRamp * spanFraction * spanFraction * spanFraction / 3.0f);
}

void DistanceLayer(const FrameInputs& in, const Config& cfg, const FogParams& p, float sunScatter,
                   const float* fogColor, FogLayer& out)
{
    out = FogLayer{};
    float farStart = std::min(std::max(in.fogStart, 0.0f), p.farLimit * kFarStartFraction);
    float opticalDepthPerDensity = DistanceFogOpticalDepthPerDensity(farStart, p.farLimit, p.maxDistance);
    out.start = farStart;
    out.density = cfg.stockFog == 1 && opticalDepthPerDensity > 0.0f
                      ? kFarOpticalDepth * cfg.farFog * cfg.density / opticalDepthPerDensity
                      : 0.0f;
    out.g = kFarG;
    out.isotropic = kFarIsotropic;
    out.strength = kFarRamp;
    out.exponent = kFarExponent;
    out.shadowDensity = 1.0f;
    out.skyFalloff = kFarSkyFalloff;
    out.endDistance = p.farLimit;
    Scale(fogColor, kFarAmbient * cfg.ambient, out.emissive);
    Scale(p.lightColor, kFarSun * sunScatter, out.diffuse);
    std::memcpy(out.shadowEmissive, out.emissive, sizeof(out.emissive));
}

float RampedPathLength(const FogLayer& l, float range)
{
    const float length = std::max(range - l.start, 0.0f);
    const float u = length / range;
    return length + l.strength * range * std::pow(u, l.exponent + 1.0f) / (l.exponent + 1.0f);
}

float HeightDensityFactor(const FogLayer& l, float z)
{
    return std::min(std::exp((l.upperHeight - z) * l.upperFalloff), 1.0f) *
           std::min(std::exp((z - l.lowerHeight) * l.lowerFalloff), 1.0f);
}

float ShadowDensityFactor(const FogLayer& l, float shadowedLayerLightScale)
{
    return l.shadowed > 0.0f ? l.shadowDensity + (1.0f - l.shadowDensity) * shadowedLayerLightScale : 1.0f;
}

float SceneLayersLevelRayOpticalDepth(const FogParams& p, float cameraZ)
{
    float opticalDepth = 0.0f;
    for (int i = 0; i < kSceneLayers; ++i)
    {
        const FogLayer& l = p.layers[i];
        const float heightFactor = HeightDensityFactor(l, cameraZ);
        const float shadowFactor = ShadowDensityFactor(l, p.shadowedLayerLightScale);
        opticalDepth += l.density * heightFactor * shadowFactor * RampedPathLength(l, p.maxDistance);
    }
    return opticalDepth;
}

float ClassicFogThinness(const FogParams& p, const AuthoredFog& fog, float cameraZ)
{
    const float coverageEdge = 1.0f - SmoothStep(kMinimumClassicCoverage, 1.0f, fog.coverage);
    const float levelRayOpticalDepth = SceneLayersLevelRayOpticalDepth(p, cameraZ);
    const float levelRayThinness = std::clamp(1.0f - levelRayOpticalDepth / kFarOpticalDepth, 0.0f, 1.0f);
    return std::max(coverageEdge, levelRayThinness);
}

const AuthoredLayer* MostForwardScatteringLayer(const AuthoredFog& fog)
{
    const AuthoredLayer* halo = nullptr;
    for (int i = 0; i < fog.layerCount; ++i)
        if (!halo || fog.layers[i].g > halo->g)
            halo = &fog.layers[i];
    return halo;
}

bool HaloHue(const AuthoredFog& fog, float* displayReferredRgb)
{
    const AuthoredLayer* halo = MostForwardScatteringLayer(fog);
    if (!halo)
        return false;
    const float* c = halo->diffuse;
    float peak = std::max(c[0], std::max(c[1], c[2]));
    if (peak < 1e-4f)
        return false;
    Scale(c, 1.0f / peak, displayReferredRgb);
    return true;
}

float WeatherStormWeight(const LightParamsSelection& selection)
{
    return selection.screenEffectSlot == kNoScreenEffectLightSlot ? std::clamp(selection.stormBlend, 0.0f, 1.0f)
                                                                   : 0.0f;
}

float InteriorWeight(const FrameInputs& in, const Config& cfg)
{
    return cfg.interiorAware && in.localLights.cameraInterior && std::isfinite(in.localLights.interiorBlend)
               ? std::clamp(in.localLights.interiorBlend, 0.0f, 1.0f)
               : 0.0f;
}

void ApplyInteriorFog(float weight, const Config& cfg, FogParams& fog)
{
    const float retainedDensity = std::isfinite(cfg.interiorDensity) ? std::clamp(cfg.interiorDensity, 0.0f, 1.0f)
                                                                   : 0.15f;
    const float densityScale = 1.0f + (retainedDensity - 1.0f) * weight;
    for (int i = 0; i < kSceneLayers; ++i)
        fog.layers[i].density *= densityScale;
    for (FogLayer& layer : fog.layers)
        Scale(layer.diffuse, 1.0f - weight, layer.diffuse);
    fog.lightVisibility *= 1.0f - weight;
}

float ClientToClassicDirectLight(const AuthoredFog& fog, const float* clientDirectLight, bool linear)
{
    if (!fog.hasClassicDirectLight)
        return 1.0f;
    float classic[3] = {fog.classicDirectLight[0], fog.classicDirectLight[1], fog.classicDirectLight[2]};
    Encode(classic, linear);
    const float classicLuminance = Luminance(classic);
    if (classicLuminance < kMinClassicDirectLuminance)
        return 1.0f;
    return std::clamp(Luminance(clientDirectLight) / classicLuminance, 0.0f, 1.0f);
}
}

void UnpackColor(uint32_t argb, float* rgb)
{
    rgb[0] = static_cast<float>((argb >> 16) & 0xFF) / 255.0f;
    rgb[1] = static_cast<float>((argb >> 8) & 0xFF) / 255.0f;
    rgb[2] = static_cast<float>(argb & 0xFF) / 255.0f;
}

FogParams BuildFogParams(const FrameInputs& in, const Config& cfg, const AuthoredFog* authored)
{
    FogParams p = {};
    p.linear = cfg.colorSpace == 1;
    p.authored = authored != nullptr;
    const float interiorWeight = InteriorWeight(in, cfg);
    float fogColor[3];
    UnpackColor(in.fogColor, fogColor);
    UnpackColor(in.directColor, p.lightColor);
    UnpackColor(in.lightIsMoon ? in.directColor : in.sunColor, p.rayColor);
    Encode(fogColor, p.linear);
    Encode(p.lightColor, p.linear);

    float elevation = SmoothStep(-0.03f, 0.10f, in.toLight[2]);
    p.lightVisibility = elevation * (in.lightIsMoon ? kMoonLight : 1.0f);
    p.lightAboveHorizon = SmoothStep(-kLightSettingHalfWidth, kLightSettingHalfWidth, in.toLight[2]);
    p.shadowedLayerLightScale = p.authored ? p.lightAboveHorizon : 1.0f;
    p.farClip = in.farClip;
    p.maxDistance = std::max(cfg.maxDistance, in.farClip);
    p.horizonStart = in.farClip * 0.85f;
    p.referenceZ = ReferenceZ(in);
    p.farLimit = std::clamp(in.fogEnd, 100.0f, in.farClip);

    float fogDistance = in.zoneFogDistance > 50.0f && in.zoneFogDistance < 20000.0f ? in.zoneFogDistance : in.fogEnd;
    fogDistance = std::clamp(fogDistance, 50.0f, 5000.0f);
    float elevationFadedScatter = p.lightVisibility * cfg.sunScatter;

    FogLayer& distanceFog = p.layers[kDistanceFogLayer];
    p.directLightMatch = 1.0f;
    if (p.authored)
    {
        const float stormLightMatch = ClientToClassicDirectLight(*authored, p.lightColor, p.linear);
        p.directLightMatch = 1.0f + (stormLightMatch - 1.0f) * WeatherStormWeight(in.lightParams);
        AuthoredLayers(*authored, cfg, p, cfg.sunScatter * p.directLightMatch, p.layers);
        HaloHue(*authored, p.rayColor);
        DistanceLayer(in, cfg, p, elevationFadedScatter, fogColor, distanceFog);
        const float thinness = ClassicFogThinness(p, *authored, in.camPos[2]);
        distanceFog.density *= thinness + (1.0f - thinness) * interiorWeight;
    }
    else
    {
        DerivedLayers(in, cfg, p, elevationFadedScatter, fogDistance, fogColor, p.layers);
        DistanceLayer(in, cfg, p, elevationFadedScatter, fogColor, distanceFog);
    }
    ApplyInteriorFog(interiorWeight, cfg, p);
    return p;
}

void Mul4x4(const float* a, const float* b, float* out)
{
    float r[16];
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            r[i * 4 + j] = a[i * 4 + 0] * b[0 * 4 + j] + a[i * 4 + 1] * b[1 * 4 + j] + a[i * 4 + 2] * b[2 * 4 + j] +
                           a[i * 4 + 3] * b[3 * 4 + j];
    std::memcpy(out, r, sizeof(r));
}

bool Invert4x4(const float* m, float* out)
{
    double inv[16];
    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] +
             m[13] * m[6] * m[11] - m[13] * m[7] * m[10];
    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] -
             m[12] * m[6] * m[11] + m[12] * m[7] * m[10];
    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] +
             m[12] * m[5] * m[11] - m[12] * m[7] * m[9];
    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] -
              m[12] * m[5] * m[10] + m[12] * m[6] * m[9];
    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] -
             m[13] * m[2] * m[11] + m[13] * m[3] * m[10];
    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] +
             m[12] * m[2] * m[11] - m[12] * m[3] * m[10];
    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] -
             m[12] * m[1] * m[11] + m[12] * m[3] * m[9];
    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] +
              m[12] * m[1] * m[10] - m[12] * m[2] * m[9];
    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] +
             m[13] * m[2] * m[7] - m[13] * m[3] * m[6];
    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] -
             m[12] * m[2] * m[7] + m[12] * m[3] * m[6];
    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] +
              m[12] * m[1] * m[7] - m[12] * m[3] * m[5];
    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] -
              m[12] * m[1] * m[6] + m[12] * m[2] * m[5];
    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] -
             m[9] * m[2] * m[7] + m[9] * m[3] * m[6];
    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] +
             m[8] * m[2] * m[7] - m[8] * m[3] * m[6];
    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] -
              m[8] * m[1] * m[7] + m[8] * m[3] * m[5];
    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] +
              m[8] * m[1] * m[6] - m[8] * m[2] * m[5];
    double det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];
    if (std::fabs(det) < 1e-12)
        return false;
    for (int i = 0; i < 16; ++i)
        out[i] = static_cast<float>(inv[i] / det);
    return true;
}

void TransformDirection(const float* v, const float* m, float* out)
{
    float r[3];
    for (int j = 0; j < 3; ++j)
        r[j] = v[0] * m[0 * 4 + j] + v[1] * m[1 * 4 + j] + v[2] * m[2 * 4 + j];
    std::memcpy(out, r, sizeof(r));
}
