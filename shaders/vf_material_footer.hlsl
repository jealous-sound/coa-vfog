float4 cAtlasDepth : register(c0);
float4 cAtlasGrid : register(c1);
float4 cMaterial : register(c2);

sampler2D sFogAtlas : register(s0);

float2 AtlasUv(float2 screenUv, float slice)
{
    float row = floor(slice * cAtlasDepth.w);
    float column = slice - row * cAtlasDepth.z;
    float2 tileSize = float2(cAtlasDepth.w, cAtlasGrid.x);
    float2 halfTexel = cAtlasGrid.yz;
    return float2(column, row) * tileSize + halfTexel + saturate(screenUv) * (tileSize - 2 * halfTexel);
}

float3 RollOffHighlights(float3 colour, float3 knee)
{
    float3 span = max(1 - knee, 1e-4);
    return colour <= knee ? colour : knee + span * (1 - exp(-(colour - knee) / span));
}

float3 MatchOpaqueComposite(float3 colour, float3 scene, float opacity)
{
    colour = RollOffHighlights(colour, max(0.8, scene));
    colour = pow(max(colour, 0), 1 / 2.2);
    [branch] if (cMaterial.w > 0)
    {
        float3 beforeGlow = 2 * colour / (sqrt(1 + 4 * cMaterial.w * colour) + 1);
        colour = lerp(colour, beforeGlow, opacity);
    }
    return colour;
}

float4 main(float4 source : COLOR0, float4 clipPosition : TEXCOORD7) : COLOR0
{
    float2 screenUv = clipPosition.xy / max(clipPosition.w, 1e-6) * float2(0.5, -0.5) + 0.5;
    float slice = sqrt(saturate(clipPosition.w * cAtlasDepth.x)) * cAtlasDepth.y;
    float lowerSlice = floor(slice);
    float4 lowerFog = tex2D(sFogAtlas, AtlasUv(screenUv, lowerSlice));
    float4 upperFog = tex2D(sFogAtlas, AtlasUv(screenUv, min(lowerSlice + 1, cAtlasDepth.y)));
    float4 fog = lerp(lowerFog, upperFog, slice - lowerSlice);
    float association = lerp(1, source.a, cMaterial.x);
    float3 colour = max(source.rgb / max(association, 1e-6), 0);
    float3 radiance = fog.rgb * cAtlasGrid.w;
    float transmittance = saturate(1 - fog.a);
    float3 output;
    [branch] if (cMaterial.z > 0.5 && cMaterial.z < 1.5)
    {
        float3 linearColour = pow(colour, 2.2);
        float3 fogged = linearColour * transmittance + radiance * (1 - cMaterial.y);
        output = pow(max(fogged, 0), 1 / 2.2);
        [branch] if (cMaterial.y < 0.5)
            output = MatchOpaqueComposite(fogged, linearColour, fog.a);
    }
    else
    {
        float3 fogColour = RollOffHighlights(radiance / max(fog.a, 1e-4), 0.8);
        [branch] if (cMaterial.z > 1.5)
            fogColour = pow(max(fogColour, 0), 1 / 2.2);
        output = colour * transmittance + fogColour * fog.a * (1 - cMaterial.y);
    }
    return float4(output * association, source.a);
}
