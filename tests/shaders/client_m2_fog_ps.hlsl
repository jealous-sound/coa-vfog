float4 cFogColour : register(c2);

float4 main(float4 colour : COLOR0, float fog : FOG) : COLOR0
{
    return float4(lerp(cFogColour.rgb, colour.rgb, fog), colour.a);
}
