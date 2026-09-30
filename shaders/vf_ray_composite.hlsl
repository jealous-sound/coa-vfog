#include "vf_composite.hlsli"

float4 main(float2 pixelIndex : VPOS) : COLOR0
{
    float2 viewportUv = (pixelIndex + 0.5 - ViewportOrigin()) / ViewportSize();
    float3 scene = tex2Dlod(sSceneBeforeFog, float4(viewportUv, 0, 0)).rgb;
    return float4(BlendGodRays(scene, DisplaySpaceGodRays(viewportUv)), 1);
}
