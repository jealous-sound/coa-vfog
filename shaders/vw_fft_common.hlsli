static const float kWaterTwoPi = 6.28318548;

float2 ComplexMultiply(float2 a, float2 b)
{
    return float2(a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x);
}

float2 Conjugate(float2 value)
{
    return float2(value.x, -value.y);
}

float2 TimesMinusI(float2 value)
{
    return float2(value.y, -value.x);
}

float4 FetchTexel(sampler2D source, float2 uv)
{
    return tex2Dlod(source, float4(uv, 0, 0));
}
