float4 cWorldShadowControl : register(c36);
float4 cViewToShadow[12] : register(c37);
float4 cShadowTexels[4] : register(c49);

sampler2D sWorldShadow0 : register(s4);
sampler2D sWorldShadow1 : register(s5);
sampler2D sWorldShadow2 : register(s6);
sampler2D sWorldShadow3 : register(s7);

float SampleWorldShadow(sampler2D shadowMap, float2 uv, float receiverDepth)
{
    float sampleValue = tex2Dlod(shadowMap, float4(uv, receiverDepth, 0)).r;
    return cWorldShadowControl.y > 0 ? sampleValue : step(receiverDepth, sampleValue);
}

float2 FilterWorldShadow(sampler2D shadowMap, float3 shadowPosition, float2 texelSize)
{
    float edge = max(abs(shadowPosition.x), abs(shadowPosition.y));
    float coverage = saturate((0.99 - edge) / 0.29);
    coverage *= shadowPosition.z > 0 && shadowPosition.z < 1 ? 1 : 0;
    [branch] if (coverage <= 0)
        return float2(1, 0);
    float2 uv = shadowPosition.xy * 0.5 + 0.5;
    float2 offset = texelSize * 0.5;
    float visibility = SampleWorldShadow(shadowMap, uv + float2(-offset.x, -offset.y), shadowPosition.z);
    visibility += SampleWorldShadow(shadowMap, uv + float2(offset.x, -offset.y), shadowPosition.z);
    visibility += SampleWorldShadow(shadowMap, uv + float2(-offset.x, offset.y), shadowPosition.z);
    visibility += SampleWorldShadow(shadowMap, uv + offset, shadowPosition.z);
    return float2(visibility * 0.25, coverage);
}

float3 ShadowPosition(float3 viewPosition, int mapIndex)
{
    float4 position = float4(viewPosition, 1);
    return float3(dot(position, cViewToShadow[mapIndex * 3]),
                  dot(position, cViewToShadow[mapIndex * 3 + 1]),
                  dot(position, cViewToShadow[mapIndex * 3 + 2]));
}

float WorldSunVisibility(float3 viewPosition, float fallbackVisibility)
{
    float environment = fallbackVisibility;
    [branch] if (cWorldShadowControl.x > 1)
    {
        float2 farMap = FilterWorldShadow(sWorldShadow3, ShadowPosition(viewPosition, 3), cShadowTexels[3].xy);
        environment = lerp(environment, farMap.x, farMap.y);
        float2 middleMap = FilterWorldShadow(sWorldShadow2, ShadowPosition(viewPosition, 2), cShadowTexels[2].xy);
        environment = lerp(environment, middleMap.x, middleMap.y);
        float2 nearMap = FilterWorldShadow(sWorldShadow1, ShadowPosition(viewPosition, 1), cShadowTexels[1].xy);
        environment = lerp(environment, nearMap.x, nearMap.y);
    }
    float2 characters = FilterWorldShadow(sWorldShadow0, ShadowPosition(viewPosition, 0), cShadowTexels[0].xy);
    return min(environment, lerp(1, characters.x, characters.y));
}
