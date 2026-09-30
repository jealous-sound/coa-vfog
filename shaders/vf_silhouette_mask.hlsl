#include "vf_sample_split.hlsli"

struct SilhouetteSplit
{
    float4 colour : COLOR0;
    float depth : DEPTH;
};

SilhouetteSplit main(float2 pixelIndex : VPOS)
{
    DepthNeighbourhood n = NeighbourhoodOf(pixelIndex + 0.5);
    clip(SilhouetteWithin(n) ? 1 : -1);
    SilhouetteSplit output;
    output.colour = 0;
    output.depth = SplitDepth(n);
    return output;
}
