float4 cDensityVariation : register(c78);
sampler3D sDensityNoise : register(s9);

float DensityVariation(float3 worldPosition)
{
    [branch] if (cDensityVariation.x <= 0)
        return 1;
    float3 uv = (worldPosition - float3(cDensityVariation.zw, 0)) * (cDensityVariation.y / 32) + 0.5 / 32;
    float noise = tex3Dlod(sDensityNoise, float4(uv, 0)).r * (4.0 / 3.0);
    uv = uv * 2 + float3(36.5, 16.5, 52.5) / 32;
    float centered = noise + tex3Dlod(sDensityNoise, float4(uv, 0)).r * (2.0 / 3.0) - 1;
    return 1 + saturate(cDensityVariation.x) * centered;
}
