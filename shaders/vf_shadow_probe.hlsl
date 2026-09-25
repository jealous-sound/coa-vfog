#include "vf_integrate.hlsli"

float4 cShadowProbe : register(c79);

float4 main() : COLOR0
{
    float2 pixel = ViewToPixel(cShadowProbe.xyz);
    float depth = SampleDepth(sDepth, pixel);
    float surfaceMask = !IsSky(depth) && cShadowProbe.z <= LinearDepth(depth) ? 1 : 0;
    float visibility = ScreenSpaceSunVisibility(cShadowProbe.xyz, length(cShadowProbe.xyz), cShadowProbe.w,
                                                surfaceMask);
    return float4(visibility.xxx, 1);
}
