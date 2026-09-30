#include "vf_sample_split.hlsli"

float4 cSampleSide : register(c99);

bool DrawsFarSamples()
{
    return cSampleSide.x > 0.5;
}

bool OwnSideAlreadyDrawn()
{
    return cSampleSide.y > 0.5;
}

float4 main(float2 pixelIndex : VPOS) : COLOR0
{
    float2 pixel = pixelIndex + 0.5;
    DepthNeighbourhood n = NeighbourhoodOf(pixel);
    const bool farSamples = DrawsFarSamples();
    const bool ownSide = (n.own > SplitDepth(n)) == farSamples;
    [branch] if (ownSide && OwnSideAlreadyDrawn())
    {
        clip(-1);
        return 0;
    }
    return CompositeAtDepth(pixel, ownSide ? n.own : (farSamples ? n.farthest : n.nearest));
}
