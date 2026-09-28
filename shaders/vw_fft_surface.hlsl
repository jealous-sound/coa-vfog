#include "vw_fft_common.hlsli"

float4 cGrid : register(c0);
float4 cStencil : register(c1);
float4 cFoam : register(c2);
float4 cOxygen : register(c3);

sampler2D sDisplacement : register(s0);
sampler2D sPreviousFoam : register(s1);

static const float kSmallestNormalHalf = 6.10351563e-5;
static const float kCoarseHalfStepsPerOctave = 512;
static const float kLargestResidualFraction = 0.99951171875;

float Resolution()
{
    return cGrid.x;
}

float InverseResolution()
{
    return cGrid.y;
}

float SlotOrigin()
{
    return cGrid.z;
}

float InverseAtlasWidth()
{
    return cGrid.w;
}

float HalfTexelsPerUnit()
{
    return cStencil.x;
}

float WideRadius()
{
    return cStencil.y;
}

float InverseWideBaseline()
{
    return cStencil.z;
}

float DeltaSeconds()
{
    return cStencil.w;
}

float3 DisplacementAt(float2 texel, float2 offset)
{
    float2 wrapped = texel + offset;
    wrapped -= Resolution() * floor(wrapped * InverseResolution());
    float2 uv = float2((SlotOrigin() + wrapped.x + 0.5) * InverseAtlasWidth(), (wrapped.y + 0.5) * InverseResolution());
    return FetchTexel(sDisplacement, uv).xyz;
}

float3 CentralDifference(float2 texel, float2 step)
{
    return (DisplacementAt(texel, step) - DisplacementAt(texel, -step)) * HalfTexelsPerUnit();
}

struct Gradients
{
    float3 alongX;
    float3 alongY;
    float2 wide;
};

Gradients GradientsAt(float2 texel)
{
    Gradients gradients;
    gradients.alongX = CentralDifference(texel, float2(1, 0));
    gradients.alongY = CentralDifference(texel, float2(0, 1));
    float2 wideAhead = float2(DisplacementAt(texel, float2(WideRadius(), 0)).z,
                              DisplacementAt(texel, float2(0, WideRadius())).z);
    float2 wideBehind = float2(DisplacementAt(texel, float2(-WideRadius(), 0)).z,
                               DisplacementAt(texel, float2(0, -WideRadius())).z);
    gradients.wide = (wideAhead - wideBehind) * InverseWideBaseline();
    return gradients;
}

float4 SurfaceMoments(Gradients gradients)
{
    float2 slope = float2(gradients.alongX.z, gradients.alongY.z);
    return float4(slope, dot(slope, slope), dot(gradients.wide, gradients.wide));
}

float Jacobian(Gradients gradients)
{
    return (1 + gradients.alongX.x) * (1 + gradients.alongY.y) - gradients.alongY.x * gradients.alongX.y;
}

float FoamStep(float previous, float jacobian, float3 biasRateDecay)
{
    float injection = max(biasRateDecay.x + 1 - jacobian, 0);
    float rate = biasRateDecay.y * injection * (1 - previous) - biasRateDecay.z * previous;
    return saturate(previous + DeltaSeconds() * rate);
}

float4 SplitForHalfStorage(float2 state)
{
    float2 step = exp2(floor(log2(max(state, kSmallestNormalHalf)))) / kCoarseHalfStepsPerOctave;
    float2 coarse = floor(state / step) * step;
    return float4(coarse, min(state - coarse, step * kLargestResidualFraction));
}

float4 NextFoam(float2 texel, float jacobian)
{
    float4 stored = FetchTexel(sPreviousFoam, (texel + 0.5) * InverseResolution());
    float2 previous = saturate(stored.xy + stored.zw);
    return SplitForHalfStorage(float2(FoamStep(previous.x, jacobian, cFoam.xyz),
                                      FoamStep(previous.y, jacobian, cOxygen.xyz)));
}

#if SURFACE && FOAM
struct SurfaceAndFoam
{
    float4 surface : COLOR0;
    float4 foam : COLOR1;
};

SurfaceAndFoam main(float2 pixel : VPOS)
{
    float2 texel = floor(pixel);
    Gradients gradients = GradientsAt(texel);
    SurfaceAndFoam maps;
    maps.surface = SurfaceMoments(gradients);
    maps.foam = NextFoam(texel, Jacobian(gradients));
    return maps;
}
#elif SURFACE
float4 main(float2 pixel : VPOS) : COLOR0
{
    return SurfaceMoments(GradientsAt(floor(pixel)));
}
#else
float4 main(float2 pixel : VPOS) : COLOR0
{
    float2 texel = floor(pixel);
    return NextFoam(texel, Jacobian(GradientsAt(texel)));
}
#endif
