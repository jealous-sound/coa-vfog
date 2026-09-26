#include "vf_common.hlsli"

sampler2D sDepth : register(s0);

float4 main(float2 lowResTexel : VPOS) : COLOR0
{
    float depth = SampleDepth(sDepth, LowResTexelToFullPixel(lowResTexel));
    float packed = floor(saturate(LinearDepth(depth) / MaxFogDistance()) * 65535 + 0.5);
    float highByte = floor(packed / 256);
    float lowByte = packed - highByte * 256;
    float depthClass = IsSky(depth) ? 1 : (BeyondFarClip(depth) ? 0.5 : 0);
    return float4(highByte / 255, lowByte / 255, depthClass, 1);
}
