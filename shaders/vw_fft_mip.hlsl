#include "vw_fft_common.hlsli"

float4 cSource : register(c0);

sampler2D sSource : register(s0);

static const float2 kQuadTaps[4] = {float2(0, 0), float2(1, 0), float2(0, 1), float2(1, 1)};

float SourceLevel()
{
    return cSource.x;
}

float InverseSourceSize()
{
    return cSource.y;
}

float SourceTexelsPerPixel()
{
    return cSource.z;
}

float TapSpread()
{
    return cSource.w;
}

float4 BoxAverage(float2 pixel)
{
    float2 first = floor(pixel) * SourceTexelsPerPixel();
    float4 sum = 0;
    [unroll] for (int tap = 0; tap < 4; tap++)
    {
        float2 uv = (first + kQuadTaps[tap] * TapSpread() + 0.5) * InverseSourceSize();
        sum += tex2Dlod(sSource, float4(uv, 0, SourceLevel()));
    }
    return 0.25 * sum;
}

#if PAIRED
struct PairedLevel
{
    float4 surface : COLOR0;
    float4 scratch : COLOR1;
};

PairedLevel main(float2 pixel : VPOS)
{
    PairedLevel level;
    level.surface = BoxAverage(pixel);
    level.scratch = level.surface;
    return level;
}
#else
float4 main(float2 pixel : VPOS) : COLOR0
{
    return BoxAverage(pixel);
}
#endif
