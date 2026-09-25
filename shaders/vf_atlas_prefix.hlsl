float4 cAtlasLayout : register(c80);
sampler2D sIntervals : register(s0);

float4 main(float2 texel : VPOS) : COLOR0
{
    float2 tileSize = cAtlasLayout.xy;
    float2 tile = floor(texel / tileSize);
    float slice = tile.y * cAtlasLayout.z + tile.x;
    [branch] if (slice <= 0 || slice >= cAtlasLayout.w)
        return 0;
    float2 local = texel - tile * tileSize;
    float2 atlasSize = tileSize * float2(cAtlasLayout.z, ceil(cAtlasLayout.w / cAtlasLayout.z));
    float3 radiance = 0;
    float transmittance = 1;
    [loop] for (int integrated = 1; integrated <= slice; ++integrated)
    {
        float row = floor(integrated / cAtlasLayout.z);
        float column = integrated - row * cAtlasLayout.z;
        float2 uv = (float2(column, row) * tileSize + local + 0.5) / atlasSize;
        float4 segment = tex2Dlod(sIntervals, float4(uv, 0, 0));
        radiance += transmittance * segment.rgb;
        transmittance *= saturate(1 - segment.a);
    }
    return float4(clamp(radiance, 0, 65504), saturate(1 - transmittance));
}
