#include "vf_common.hlsli"

sampler2D sDepth : register(s0);

static const float kPackedDepthLevels = 16777215;
static const float kHighByteWeight = 65536;
static const float kMidByteWeight = 256;
static const float kByteMax = 255;

float4 PackedDepth(float normalisedDepth)
{
    float packed = floor(saturate(normalisedDepth) * kPackedDepthLevels + 0.5);
    float high = floor(packed / kHighByteWeight);
    float rest = packed - high * kHighByteWeight;
    float mid = floor(rest / kMidByteWeight);
    float low = rest - mid * kMidByteWeight;
    return float4(high, mid, low, kByteMax) / kByteMax;
}

float4 main(float2 copyTexel : VPOS) : COLOR0
{
    float2 pixel = ViewportOrigin() + copyTexel + 0.5;
    float viewZ = LinearDepth(SampleDepth(sDepth, pixel));
#if PACKED_DEPTH
    return PackedDepth(viewZ / MaxFogDistance());
#else
    return viewZ;
#endif
}
