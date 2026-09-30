float4 cGradedViewport : register(c0);
float4 cGrading : register(c1);

sampler2D sSceneBeforeGrading : register(s0);
sampler2D sGradingCurve : register(s1);

static const float kCurveEntries = 32;

float2 ViewportOrigin()
{
    return cGradedViewport.xy;
}

float2 InverseViewportSize()
{
    return cGradedViewport.zw;
}

float GradingStrength()
{
    return cGrading.x;
}

float CurveEntry(float index)
{
    return tex2Dlod(sGradingCurve, float4((index + 0.5) / kCurveEntries, 0.5, 0, 0)).r;
}

float GradedChannel(float value)
{
    float position = saturate(value) * (kCurveEntries - 1);
    float lower = floor(position);
    float upper = min(lower + 1, kCurveEntries - 1);
    return lerp(CurveEntry(lower), CurveEntry(upper), position - lower);
}

float3 Graded(float3 colour)
{
    return float3(GradedChannel(colour.r), GradedChannel(colour.g), GradedChannel(colour.b));
}

float4 main(float2 pixelIndex : VPOS) : COLOR0
{
    float2 viewportUv = (pixelIndex + 0.5 - ViewportOrigin()) * InverseViewportSize();
    float3 scene = tex2Dlod(sSceneBeforeGrading, float4(viewportUv, 0, 0)).rgb;
    return float4(lerp(scene, Graded(scene), GradingStrength()), 1);
}
