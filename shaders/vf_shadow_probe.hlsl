#include "vf_integrate.hlsli"

float4 cShadowProbe : register(c79);

float4 main() : COLOR0
{
    float visibility = ScreenSpaceSunVisibility(cShadowProbe.xyz, length(cShadowProbe.xyz), cShadowProbe.w);
    return float4(visibility.xxx, 1);
}
