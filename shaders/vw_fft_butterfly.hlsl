#include "vw_fft_common.hlsli"

float4 cGrid : register(c0);
float4 cStage : register(c1);

sampler2D sSource : register(s0);
sampler2D sButterfly : register(s1);

float Resolution()
{
    return cGrid.x;
}

float InverseResolution()
{
    return cGrid.y;
}

float InverseSlots()
{
    return cGrid.w;
}

float StageCoordinate()
{
    return cStage.x;
}

float InverseReferenceArea()
{
    return cStage.y;
}

float EarlierStageCoordinate()
{
    return cStage.z;
}

float4 ButterflyAt(float stageCoordinate, float index)
{
    return FetchTexel(sButterfly, float2(stageCoordinate, index));
}

float4 Combine(float4 butterfly, float4 first, float4 second)
{
    return float4(first.xy + ComplexMultiply(butterfly.xy, second.xy),
                  first.zw + ComplexMultiply(butterfly.xy, second.zw));
}

#if COLUMNS
float SlotOf(float2 pixel)
{
    return 0;
}

float LineIndex(float2 pixel, float slot)
{
    return pixel.y;
}

float4 SourceAt(float2 pixel, float slot, float index)
{
    return FetchTexel(sSource, float2((pixel.x + 0.5) * InverseResolution() * InverseSlots(), index));
}
#else
float SlotOf(float2 pixel)
{
    return floor((pixel.x + 0.5) * InverseResolution());
}

float LineIndex(float2 pixel, float slot)
{
    return pixel.x - slot * Resolution();
}

float4 SourceAt(float2 pixel, float slot, float index)
{
    return FetchTexel(sSource, float2((slot + index) * InverseSlots(), (pixel.y + 0.5) * InverseResolution()));
}
#endif

#if FUSED_STAGES
float4 EarlierStageAt(float2 pixel, float slot, float index)
{
    float4 butterfly = ButterflyAt(EarlierStageCoordinate(), index);
    return Combine(butterfly, SourceAt(pixel, slot, butterfly.z), SourceAt(pixel, slot, butterfly.w));
}
#else
float4 EarlierStageAt(float2 pixel, float slot, float index)
{
    return SourceAt(pixel, slot, index);
}
#endif

float4 Stage(float2 pixel)
{
    float slot = SlotOf(pixel);
    float4 butterfly = ButterflyAt(StageCoordinate(), (LineIndex(pixel, slot) + 0.5) * InverseResolution());
    return Combine(butterfly, EarlierStageAt(pixel, slot, butterfly.z), EarlierStageAt(pixel, slot, butterfly.w));
}

#if ASSEMBLE
float4 Output(float2 pixel, float4 transformed)
{
    float scale = (1 - 4 * frac(0.5 * (pixel.x + pixel.y))) * InverseReferenceArea();
    return float4(transformed.y, transformed.z, -transformed.x, transformed.w) * scale;
}
#else
float4 Output(float2 pixel, float4 transformed)
{
    return transformed;
}
#endif

float4 main(float2 pixel : VPOS) : COLOR0
{
    float2 index = floor(pixel);
    return Output(index, Stage(index));
}
