#include "vf_integrate.hlsli"

float4 main(float2 lowResTexel : VPOS) : COLOR0
{
    return IntegrateFogAtPixel(LowResTexelToFullPixel(lowResTexel), StepJitter(lowResTexel), 0, 0, false);
}
