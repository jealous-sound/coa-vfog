#include "vf_integrate.hlsli"

float4 cAtlasLayout : register(c80);

float4 main(float2 texel : VPOS) : COLOR0
{
    float2 tileSize = cAtlasLayout.xy;
    float2 tile = floor(texel / tileSize);
    float slice = tile.y * cAtlasLayout.z + tile.x;
    [branch] if (slice <= 0 || slice >= cAtlasLayout.w)
        return 0;
    float2 local = texel - tile * tileSize;
    float2 pixel = ViewportOrigin() + local / max(tileSize - 1, 1) * ViewportSize();
    float previousFraction = (slice - 1) / (cAtlasLayout.w - 1);
    float fraction = slice / (cAtlasLayout.w - 1);
    return IntegrateFogAtPixel(pixel, 0.5, MaxFogDistance() * previousFraction * previousFraction,
                              MaxFogDistance() * fraction * fraction, true);
}
