#include "vf_common.hlsli"

row_major float4x4 cViewToPreviousClip : register(c9);
float4 cTemporal : register(c13);

sampler2D sCurrent : register(s0);
sampler2D sHistory : register(s1);
sampler2D sDepth : register(s2);
sampler2D sHistoryDepth : register(s3);

static const float kHistoryRelativeDepthTolerance = 0.02;
static const float kHistoryAbsoluteDepthTolerance = 0.5;

float HistoryWeight()
{
    return cTemporal.x;
}

bool HistoryInvalid()
{
    return cTemporal.y <= 0;
}

float LightingHistoryWeight(float3 current, float3 history)
{
    float weight = HistoryWeight();
    [branch] if (cTemporal.z > 0)
    {
        float3 difference = abs(current - history);
        float peakDifference = max(difference.r, max(difference.g, difference.b));
        float3 peak = max(current, history);
        float brightness = max(0.05, max(peak.r, max(peak.g, peak.b)));
        weight *= 1 - smoothstep(0.20, 0.40, peakDifference / brightness);
    }
    return weight;
}

void CurrentNeighbourhoodRange(float2 uv, float4 centre, out float4 lowest, out float4 highest)
{
    lowest = centre;
    highest = centre;
    static const float2 kNeighbourOffsets[8] = {
        float2(-1, -1), float2(0, -1), float2(1, -1), float2(-1, 0),
        float2(1, 0), float2(-1, 1), float2(0, 1), float2(1, 1)
    };
    [unroll] for (int k = 0; k < 8; k++)
    {
        float4 neighbour = tex2Dlod(sCurrent, float4(uv + kNeighbourOffsets[k] * LowResTexelSize(), 0, 0));
        lowest = min(lowest, neighbour);
        highest = max(highest, neighbour);
    }
}

bool MatchesSurface(float expectedDepth, float depthClass, float tapViewZ, float tapDepthClass)
{
    float tolerance = max(kHistoryAbsoluteDepthTolerance, expectedDepth * kHistoryRelativeDepthTolerance);
    return abs(tapDepthClass - depthClass) < 0.25 && (depthClass > 0 || abs(tapViewZ - expectedDepth) <= tolerance);
}

float DepthClass(float depth)
{
    return IsSky(depth) ? 1 : (BeyondFarClip(depth) ? 0.5 : 0);
}

struct History
{
    float4 value;
    bool found;
};

History NoHistory()
{
    History none;
    none.value = 0;
    none.found = false;
    return none;
}

History ValidatedHistory(float2 uv, float expectedDepth, float depthClass)
{
    float2 texel = uv * LowResSize() - 0.5;
    float2 baseTexel = floor(texel);
    float2 fraction = texel - baseTexel;
    static const float2 kOffsets[4] = {float2(0, 0), float2(1, 0), float2(0, 1), float2(1, 1)};
    float4 history = 0;
    float weightSum = 0;
    [unroll] for (int i = 0; i < 4; ++i)
    {
        float2 tap = baseTexel + kOffsets[i];
        float2 tapUv = LowResTexelToUv(clamp(tap, 0, LowResSize() - 1));
        float3 previousDepth = tex2Dlod(sHistoryDepth, float4(tapUv, 0, 0)).rgb;
        float previousViewZ = dot(previousDepth.rg, float2(65280, 255)) * (MaxFogDistance() / 65535);
        float valid = MatchesSurface(expectedDepth, depthClass, previousViewZ, previousDepth.b) ? 1 : 0;
        float2 bilinear = lerp(1 - fraction, fraction, kOffsets[i]);
        float weight = bilinear.x * bilinear.y * valid;
        history += tex2Dlod(sHistory, float4(tapUv, 0, 0)) * weight;
        weightSum += weight;
    }
    History validated;
    validated.found = weightSum > 1e-5;
    validated.value = history / max(weightSum, 1e-5);
    return validated;
}

History ReprojectedHistory(float2 pixel, float depth, float viewZ, float depthClass)
{
    [branch] if (HistoryInvalid())
        return NoHistory();
    float4 previousClip = mul(float4(ViewRayAtUnitDepth(pixel) * viewZ, IsSky(depth) ? 0 : 1),
                              cViewToPreviousClip);
    [branch] if (previousClip.w <= 1e-3)
        return NoHistory();
    float2 previousPixel = NdcToPixel(previousClip.xy / previousClip.w);
    float2 previousUv = LowResTexelToUv(FullPixelToLowResTexel(previousPixel));
    [branch] if (any(previousUv < 0) || any(previousUv > 1))
        return NoHistory();
    return ValidatedHistory(previousUv, previousClip.w, depthClass);
}

bool InsideLowResTarget(float2 lowResTexel)
{
    return all(lowResTexel >= 0) && all(lowResTexel < LowResSize());
}

float4 SameSurfaceCurrentAverage(float2 lowResTexel, float viewZ, float depthClass)
{
    float4 sum = 0;
    float weightSum = 0;
    [loop] for (int k = 0; k < 9; ++k)
    {
        float2 tap = lowResTexel + float2(k % 3, k / 3) - 1;
        float tapDepth = SampleDepth(sDepth, LowResTexelToFullPixel(tap));
        float weight = InsideLowResTarget(tap) &&
                       MatchesSurface(viewZ, depthClass, LinearDepth(tapDepth), DepthClass(tapDepth)) ? 1 : 0;
        sum += tex2Dlod(sCurrent, float4(LowResTexelToUv(tap), 0, 0)) * weight;
        weightSum += weight;
    }
    return sum / max(weightSum, 1);
}

float4 main(float2 lowResTexel : VPOS) : COLOR0
{
    float2 uv = LowResTexelToUv(lowResTexel);
    float2 pixel = LowResTexelToFullPixel(lowResTexel);
    float depth = SampleDepth(sDepth, pixel);
    float viewZ = LinearDepth(depth);
    float depthClass = DepthClass(depth);
    History reprojected = ReprojectedHistory(pixel, depth, viewZ, depthClass);
    [branch] if (!reprojected.found)
        return SameSurfaceCurrentAverage(lowResTexel, viewZ, depthClass);

    float4 current = tex2Dlod(sCurrent, float4(uv, 0, 0));
    float4 lowest;
    float4 highest;
    CurrentNeighbourhoodRange(uv, current, lowest, highest);
    float4 history = clamp(reprojected.value, lowest, highest);
    return lerp(current, history, LightingHistoryWeight(current.rgb, history.rgb));
}
