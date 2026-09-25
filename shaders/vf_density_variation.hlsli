float4 cDensityVariation : register(c78);
sampler3D sDensityNoise : register(s9);

float DensityVariation(float3 worldPosition)
{
    [branch] if (cDensityVariation.x <= 0)
        return 1;
    float3 position = (worldPosition - float3(cDensityVariation.zw, 0)) * cDensityVariation.y;
    float first = tex3Dlod(sDensityNoise, float4((position + 0.5) / 32, 0)).r;
    float second = tex3Dlod(sDensityNoise, float4((position * 2 + float3(37.5, 17.5, 53.5)) / 32, 0)).r;
    float centered = (first * (2.0 / 3.0) + second * (1.0 / 3.0)) * 2 - 1;
    return 1 + saturate(cDensityVariation.x) * centered;
}
