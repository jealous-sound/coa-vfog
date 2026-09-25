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

float4 ValidatedHistory(float2 uv, float expectedDepth, float depthClass, float4 current)
{
    float2 texel = uv * LowResSize() - 0.5;
    float2 baseTexel = floor(texel);
    float2 fraction = texel - baseTexel;
    static const float2 kOffsets[4] = {float2(0, 0), float2(1, 0), float2(0, 1), float2(1, 1)};
    float4 history = 0;
    float weightSum = 0;
    float tolerance = max(kHistoryAbsoluteDepthTolerance, expectedDepth * kHistoryRelativeDepthTolerance);
    [unroll] for (int i = 0; i < 4; ++i)
    {
        float2 tap = baseTexel + kOffsets[i];
        float2 tapUv = LowResTexelToUv(clamp(tap, 0, LowResSize() - 1));
        float3 previousDepth = tex2Dlod(sHistoryDepth, float4(tapUv, 0, 0)).rgb;
        float previousViewZ = dot(previousDepth.rg, float2(65280, 255)) * (MaxFogDistance() / 65535);
        float valid = abs(previousDepth.b - depthClass) < 0.25 &&
                      (depthClass > 0 || abs(previousViewZ - expectedDepth) <= tolerance) ? 1 : 0;
        float2 bilinear = lerp(1 - fraction, fraction, kOffsets[i]);
        float weight = bilinear.x * bilinear.y * valid;
        history += tex2Dlod(sHistory, float4(tapUv, 0, 0)) * weight;
        weightSum += weight;
    }
    return weightSum > 1e-5 ? history / weightSum : current;
}

float4 main(float2 lowResTexel : VPOS) : COLOR0
{
    float2 uv = LowResTexelToUv(lowResTexel);
    float4 current = tex2Dlod(sCurrent, float4(uv, 0, 0));
    [branch] if (HistoryInvalid())
        return current;

    float4 lowest;
    float4 highest;
    CurrentNeighbourhoodRange(uv, current, lowest, highest);

    float2 pixel = LowResTexelToFullPixel(lowResTexel);
    float depth = SampleDepth(sDepth, pixel);
    float viewZ = LinearDepth(depth);
    float depthClass = IsSky(depth) ? 1 : (BeyondFarClip(depth) ? 0.5 : 0);
    float4 previousClip = mul(float4(ViewRayAtUnitDepth(pixel) * viewZ, IsSky(depth) ? 0 : 1),
                              cViewToPreviousClip);
    [branch] if (previousClip.w <= 1e-3)
        return current;
    float2 previousPixel = NdcToPixel(previousClip.xy / previousClip.w);
    float2 previousUv = LowResTexelToUv(FullPixelToLowResTexel(previousPixel));
    [branch] if (any(previousUv < 0) || any(previousUv > 1))
        return current;
    float4 history = clamp(ValidatedHistory(previousUv, previousClip.w, depthClass, current), lowest, highest);
    return lerp(current, history, HistoryWeight());
}
