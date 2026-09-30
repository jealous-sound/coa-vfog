#include "vf_composite.hlsli"

float4 main(float2 pixelIndex : VPOS) : COLOR0
{
    float2 pixel = pixelIndex + 0.5;
    return CompositeAtDepth(pixel, SampleDepth(sDepth, pixel));
}
