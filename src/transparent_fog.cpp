#include "transparent_fog.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace
{
constexpr float kFitNdcColumns[] = {-0.8f, -0.4f, 0.0f, 0.4f, 0.8f};
constexpr float kFitNdcRows[] = {-0.6f, 0.0f, 0.6f};
constexpr int kFitRays = static_cast<int>(std::size(kFitNdcColumns) * std::size(kFitNdcRows));
constexpr float kCentreWeightFalloff = 1.5f;
constexpr int kFitDepthSteps = 25;
constexpr int kProfileSteps = 32;
constexpr float kMaxFitRise = 0.26f;
constexpr float kThinNearFogTransmittance = 0.9f;
constexpr float kFogFreeTransmittance = 0.995f;
constexpr float kExtendedFitTransmittance = 0.5f;
constexpr double kMinFogSlope = -1.0e-6;
constexpr float kClampedTransmittance = 0.02f;
constexpr float kMinFogOpacity = 1.0e-4f;
constexpr float kTaylorOpticalDepth = 1.0e-3f;
constexpr float kHighlightKnee = 0.8f;
constexpr float kMinHighlightSpan = 1.0e-4f;
constexpr float kDisplayGamma = 2.2f;
constexpr float kMinPhaseDenominator = 1.0e-6f;
constexpr float kDistanceCurveBias = 1.0e-6f;
constexpr double kMinLineDeterminant = 1.0e-12;
constexpr float kChannelLevels = 255.0f;

struct FitRay
{
    float world[3];
    float distancePerViewDepth;
    float weight;
};

struct DepthSample
{
    float transmittance;
    float inScatter[3];
};

using RaySamples = DepthSample[kFitDepthSteps + 1];

struct TransmittanceProfile
{
    float stepDepth;
    float transmittance[kProfileSteps + 1];
};

struct WeightedLine
{
    double weight = 0.0;
    double x = 0.0;
    double y = 0.0;
    double xx = 0.0;
    double xy = 0.0;

    void Add(double px, double py, double w)
    {
        weight += w;
        x += w * px;
        y += w * py;
        xx += w * px * px;
        xy += w * px * py;
    }

    bool Solve(double& slope, double& intercept) const
    {
        const double determinant = weight * xx - x * x;
        if (weight <= 0.0 || std::fabs(determinant) < kMinLineDeterminant)
            return false;
        slope = (weight * xy - x * y) / determinant;
        intercept = (y - slope * x) / weight;
        return true;
    }
};

float Dot3(const float* a, const float* b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

void Normalize3(float* v)
{
    const float length = std::sqrt(Dot3(v, v));
    if (length <= 0.0f)
        return;
    for (int i = 0; i < 3; ++i)
        v[i] /= length;
}

float SampleDepth(int step, float stepDepth)
{
    return static_cast<float>(step) * stepDepth;
}

float SmoothStep(float edge0, float edge1, float x)
{
    const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

float PhaseHG(float g, float cosAngle)
{
    const float r = (1.0f - g) / std::sqrt(std::max(1.0f + g * g - 2.0f * g * cosAngle, kMinPhaseDenominator));
    return r * r * r;
}

float LayerPhase(const FogLayer& layer, float cosAngle)
{
    const float hg = PhaseHG(layer.g, cosAngle);
    return hg + (1.0f - hg) * layer.isotropic;
}

float DistanceCurve(const FogLayer& layer, float distance, float range)
{
    const float t = std::clamp(std::max(distance - layer.start, 0.0f) / range, 0.0f, 1.0f);
    return 1.0f + layer.strength * std::pow(t + kDistanceCurveBias, layer.exponent);
}

float HeightProfile(const FogLayer& layer, float height)
{
    return std::min(std::exp((layer.upperHeight - height) * layer.upperFalloff), 1.0f) *
           std::min(std::exp((height - layer.lowerHeight) * layer.lowerFalloff), 1.0f);
}

struct RayLight
{
    float cosToLight;
    float lightAboveHorizon;
    float cameraHeight;
    float rise;
    float range;
};

void AccumulateLayer(const FogLayer& layer, const RayLight& ray, float stepStart, float stepEnd, float* radiance,
                     float& opticalDepth)
{
    const float layerStart = std::max(stepStart, layer.start);
    const float layerLength = std::max(std::min(stepEnd, layer.endDistance) - layerStart, 0.0f);
    if (layerLength <= 0.0f || layer.density <= 0.0f)
        return;
    const float shadow = layer.shadowed * (1.0f - ray.lightAboveHorizon);
    const float shadowDensity = 1.0f + (layer.shadowDensity - 1.0f) * shadow;
    const float sampleDistance = layerStart + layerLength * 0.5f;
    const float layerOpticalDepth = layer.density * layerLength * DistanceCurve(layer, sampleDistance, ray.range) *
                                    HeightProfile(layer, ray.cameraHeight + ray.rise * sampleDistance) *
                                    shadowDensity;
    const float directPhase = (1.0f - shadow) * LayerPhase(layer, ray.cosToLight);
    for (int c = 0; c < 3; ++c)
    {
        const float emissive = layer.emissive[c] + (layer.shadowEmissive[c] - layer.emissive[c]) * shadow;
        radiance[c] += (layer.diffuse[c] * directPhase + emissive) * layerOpticalDepth;
    }
    opticalDepth += layerOpticalDepth;
}

float StepOpacity(float opticalDepth)
{
    return opticalDepth < kTaylorOpticalDepth
               ? opticalDepth * (1.0f - 0.5f * opticalDepth + opticalDepth * opticalDepth / 6.0f)
               : 1.0f - std::exp(-opticalDepth);
}

void IntegrateRay(const FogParams& fog, const FitRay& fitRay, const FrameInputs& in, float stepDepth, int steps,
                  DepthSample* samples)
{
    const RayLight ray = {Dot3(in.toLight, fitRay.world), fog.lightAboveHorizon, in.camPos[2],
                          std::clamp(fitRay.world[2], -kMaxFitRise, kMaxFitRise), fog.maxDistance};
    float transmittance = 1.0f;
    float inScatter[3] = {};
    samples[0] = {1.0f, {0.0f, 0.0f, 0.0f}};
    for (int step = 0; step < steps; ++step)
    {
        const float stepStart = SampleDepth(step, stepDepth) * fitRay.distancePerViewDepth;
        const float stepEnd = SampleDepth(step + 1, stepDepth) * fitRay.distancePerViewDepth;
        float radiance[3] = {};
        float opticalDepth = 0.0f;
        for (const FogLayer& layer : fog.layers)
            AccumulateLayer(layer, ray, stepStart, stepEnd, radiance, opticalDepth);
        if (opticalDepth > 0.0f)
        {
            const float opacity = StepOpacity(opticalDepth);
            for (int c = 0; c < 3; ++c)
                inScatter[c] += transmittance * radiance[c] * (opacity / opticalDepth);
            transmittance *= 1.0f - opacity;
        }
        samples[step + 1] = {transmittance, {inScatter[0], inScatter[1], inScatter[2]}};
    }
}

int BuildFitRays(const FrameInputs& in, FitRay* rays)
{
    float viewToWorld[16];
    if (!Invert4x4(in.cameraRelativeView, viewToWorld))
        return 0;
    const float* proj = in.glProjection;
    int count = 0;
    for (float row : kFitNdcRows)
        for (float column : kFitNdcColumns)
        {
            float view[3] = {(column - proj[8]) / proj[0], (row - proj[9]) / proj[5], 1.0f};
            const float distancePerViewDepth = std::sqrt(Dot3(view, view));
            Normalize3(view);
            FitRay& ray = rays[count++];
            TransformDirection(view, viewToWorld, ray.world);
            Normalize3(ray.world);
            ray.distancePerViewDepth = distancePerViewDepth;
            ray.weight = std::exp(-kCentreWeightFalloff * (column * column + row * row));
        }
    return count;
}

void IntegrateRays(const FogParams& fog, const FitRay* rays, int rayCount, const FrameInputs& in, float stepDepth,
                   RaySamples* samples)
{
    for (int r = 0; r < rayCount; ++r)
        IntegrateRay(fog, rays[r], in, stepDepth, kFitDepthSteps, samples[r]);
}

bool Clamped(double slope, double intercept, float depth, float transmittance)
{
    return slope * depth + intercept <= 0.0 && transmittance < kClampedTransmittance;
}

bool FitTransmittanceLine(const FitRay* rays, const RaySamples* samples, int rayCount, float stepDepth,
                          double& slope, double& intercept)
{
    WeightedLine all;
    for (int r = 0; r < rayCount; ++r)
        for (int step = 0; step <= kFitDepthSteps; ++step)
            all.Add(SampleDepth(step, stepDepth), samples[r][step].transmittance, rays[r].weight);
    if (!all.Solve(slope, intercept))
        return false;
    WeightedLine unclamped;
    for (int r = 0; r < rayCount; ++r)
        for (int step = 0; step <= kFitDepthSteps; ++step)
        {
            const float depth = SampleDepth(step, stepDepth);
            if (!Clamped(slope, intercept, depth, samples[r][step].transmittance))
                unclamped.Add(depth, samples[r][step].transmittance, rays[r].weight);
        }
    double refinedSlope = 0.0;
    double refinedIntercept = 0.0;
    if (unclamped.Solve(refinedSlope, refinedIntercept))
    {
        slope = refinedSlope;
        intercept = refinedIntercept;
    }
    return true;
}

float WindowEndTransmittance(const FitRay* rays, const RaySamples* samples, int rayCount)
{
    float weighted = 0.0f;
    float weights = 0.0f;
    for (int r = 0; r < rayCount; ++r)
    {
        weighted += rays[r].weight * samples[r][kFitDepthSteps].transmittance;
        weights += rays[r].weight;
    }
    return weights > 0.0f ? weighted / weights : 1.0f;
}

const FitRay& ViewAxisRay(const FitRay* rays, int rayCount)
{
    return *std::max_element(rays, rays + rayCount,
                             [](const FitRay& a, const FitRay& b) { return a.weight < b.weight; });
}

TransmittanceProfile ProfileToTheFarClip(const FogParams& fog, const FitRay& ray, const FrameInputs& in)
{
    TransmittanceProfile profile = {};
    profile.stepDepth = std::max(kStockFogFitDepth, fog.farClip) / kProfileSteps;
    DepthSample samples[kProfileSteps + 1];
    IntegrateRay(fog, ray, in, profile.stepDepth, kProfileSteps, samples);
    for (int step = 0; step <= kProfileSteps; ++step)
        profile.transmittance[step] = samples[step].transmittance;
    return profile;
}

float DepthWhereTransmittanceFalls(const TransmittanceProfile& profile, float level)
{
    for (int step = 1; step <= kProfileSteps; ++step)
    {
        const float before = profile.transmittance[step - 1];
        const float after = profile.transmittance[step];
        if (after > level)
            continue;
        const float along = before > after ? (before - level) / (before - after) : 0.0f;
        return SampleDepth(step - 1, profile.stepDepth) + along * profile.stepDepth;
    }
    return SampleDepth(kProfileSteps, profile.stepDepth);
}

float ExtendedWindowDepth(const TransmittanceProfile& profile, float nearTransmittance)
{
    const float reach = SmoothStep(kThinNearFogTransmittance, kFogFreeTransmittance, nearTransmittance);
    const float extended =
        std::max(DepthWhereTransmittanceFalls(profile, kExtendedFitTransmittance), kStockFogFitDepth);
    return kStockFogFitDepth + reach * (extended - kStockFogFitDepth);
}

float RollOffHighlight(float colour)
{
    const float span = std::max(1.0f - kHighlightKnee, kMinHighlightSpan);
    if (colour <= kHighlightKnee)
        return colour;
    return kHighlightKnee + span * (1.0f - std::exp(-(colour - kHighlightKnee) / span));
}

float BeforeClientGlow(float onScreen, float glow)
{
    return 2.0f * onScreen / (1.0f + std::sqrt(1.0f + 4.0f * glow * onScreen));
}

uint32_t PackedChannel(float value, int shift)
{
    return static_cast<uint32_t>(std::lround(std::clamp(value, 0.0f, 1.0f) * kChannelLevels)) << shift;
}

uint32_t FittedFogColour(const FitRay* rays, const RaySamples* samples, int rayCount, bool linear,
                         const StockFogFitLight& light)
{
    double scattered[3] = {};
    double opacity = 0.0;
    for (int r = 0; r < rayCount; ++r)
        for (int step = 1; step <= kFitDepthSteps; ++step)
        {
            const DepthSample& sample = samples[r][step];
            const float sampleOpacity = 1.0f - sample.transmittance;
            if (sampleOpacity <= kMinFogOpacity)
                continue;
            for (int c = 0; c < 3; ++c)
                scattered[c] += rays[r].weight * sample.inScatter[c];
            opacity += rays[r].weight * sampleOpacity;
        }
    float colour[3] = {};
    for (int c = 0; c < 3; ++c)
    {
        const float unpremultiplied =
            opacity > 0.0 ? static_cast<float>(light.exposure * scattered[c] / opacity) : 0.0f;
        colour[c] = RollOffHighlight(std::max(unpremultiplied, 0.0f));
        if (linear)
            colour[c] = BeforeClientGlow(std::pow(colour[c], 1.0f / kDisplayGamma), light.glowToCompensate);
    }
    return kLightingFogColourAlpha | PackedChannel(colour[0], 16) | PackedChannel(colour[1], 8) |
           PackedChannel(colour[2], 0);
}
}

StockFogFit FitStockFog(const FogParams& drawn, const FrameInputs& in, const StockFogFitLight& light)
{
    const FogParams fog = WithMeanNoise(drawn);
    FitRay rays[kFitRays];
    const int rayCount = BuildFitRays(in, rays);
    if (rayCount == 0)
        return {};
    RaySamples samples[kFitRays];
    float stepDepth = kStockFogFitDepth / kFitDepthSteps;
    IntegrateRays(fog, rays, rayCount, in, stepDepth, samples);
    const float nearTransmittance = WindowEndTransmittance(rays, samples, rayCount);
    if (nearTransmittance > kThinNearFogTransmittance)
    {
        const TransmittanceProfile profile = ProfileToTheFarClip(fog, ViewAxisRay(rays, rayCount), in);
        if (profile.transmittance[kProfileSteps] > kFogFreeTransmittance)
            return {};
        stepDepth = ExtendedWindowDepth(profile, nearTransmittance) / kFitDepthSteps;
        IntegrateRays(fog, rays, rayCount, in, stepDepth, samples);
    }
    double slope = 0.0;
    double intercept = 0.0;
    if (!FitTransmittanceLine(rays, samples, rayCount, stepDepth, slope, intercept) || slope > kMinFogSlope)
        return {};
    StockFogFit fit;
    fit.fogs = true;
    fit.start = static_cast<float>((1.0 - intercept) / slope);
    fit.end = static_cast<float>(-intercept / slope);
    fit.colour = FittedFogColour(rays, samples, rayCount, fog.linear, light);
    return fit;
}

bool UsesLightingFogColour(uint32_t colour)
{
    return (colour & kFogColourAlphaMask) == kLightingFogColourAlpha;
}

void ApplyStockFogFit(const StockFogFit& fit, M2BatchFogArgs& args)
{
    if (!fit.fogs)
        return;
    args.start = fit.start;
    args.end = fit.end;
    args.exponent = kLinearStockFogExponent;
    if (args.colour && UsesLightingFogColour(*args.colour))
        args.colour = &fit.colour;
}
