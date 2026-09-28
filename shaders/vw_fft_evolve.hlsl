#include "vw_fft_common.hlsli"

float4 cGrid : register(c0);
float4 cTile : register(c1);
float4 cWind : register(c2);
float4 cClock : register(c3);

sampler2D sNoise : register(s0);

static const float kSmallestWaveNumberSquared = 1.0e-6;
static const float kFlatWaveNumber = 1.0e-16;
static const float kSmallestAlongWind = 1.0e-30;

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

float HalfResolution()
{
    return cGrid.w;
}

float WaveNumberPerTexel()
{
    return cTile.x;
}

float Amplitude()
{
    return cTile.y;
}

float Alignment()
{
    return cTile.z;
}

float InversePeakLengthSquared()
{
    return cTile.w;
}

float2 WindDirection()
{
    return cWind.xy;
}

float CoarseLoopFraction()
{
    return cClock.x;
}

float FineLoopFraction()
{
    return cClock.y;
}

float LoopFrequenciesPerRootRadius()
{
    return cClock.z;
}

float2 WaveNumber(float2 texel)
{
    return (texel - HalfResolution()) * WaveNumberPerTexel();
}

float2 SpectrumAmplitudes(float2 waveNumber)
{
    float squared = dot(waveNumber, waveNumber);
    float clamped = max(squared, kSmallestWaveNumberSquared);
    float isotropic = Amplitude() / (clamped * clamped) * exp(-InversePeakLengthSquared() / clamped);
    float alongWind = dot(waveNumber, WindDirection()) * rsqrt(clamped);
    float directional = exp2(Alignment() * log2(max(abs(alongWind), kSmallestAlongWind)));
    float2 sides = float2(alongWind > 0, alongWind < 0);
    return squared < kSmallestWaveNumberSquared ? 0 : sqrt(0.5 * isotropic * directional) * sides;
}

float2 LoopRotation(float rootRadius)
{
    float loopFrequency = floor(rootRadius * LoopFrequenciesPerRootRadius() + 0.5);
    float cycles = frac(frac(loopFrequency * CoarseLoopFraction()) + loopFrequency * FineLoopFraction());
    float2 rotation;
    sincos(cycles * kWaterTwoPi, rotation.y, rotation.x);
    return rotation;
}

float2 ForeverEvolvedMode(float3 h0, float2 rotation)
{
    return ComplexMultiply(h0.xy, rotation) + ComplexMultiply(float2(h0.z, -h0.y), Conjugate(rotation));
}

float2 EvolvedHeight(float2 texel, out float2 direction)
{
    float2 waveNumber = WaveNumber(texel);
    direction = waveNumber / max(length(waveNumber), kFlatWaveNumber);
    float4 noise = FetchTexel(sNoise, (texel + 0.5) * InverseResolution());
    return ForeverEvolvedMode(noise.xyz * SpectrumAmplitudes(waveNumber).xxy, LoopRotation(noise.w));
}

float2 FoldedHermitian(float2 atTexel, float2 atMirror)
{
    return 0.5 * (atTexel + Conjugate(atMirror));
}

float4 main(float2 pixel : VPOS) : COLOR0
{
    float2 texel = floor(pixel) - float2(SlotOrigin(), 0);
    float2 mirror = (Resolution() - texel) * (texel > 0);
    float2 direction;
    float2 mirrorDirection;
    float2 height = EvolvedHeight(texel, direction);
    float2 mirrorHeight = EvolvedHeight(mirror, mirrorDirection);
    float2 foldedHeight = FoldedHermitian(height, mirrorHeight);
    float2 foldedX = FoldedHermitian(TimesMinusI(height) * direction.x, TimesMinusI(mirrorHeight) * mirrorDirection.x);
    float2 foldedY = FoldedHermitian(TimesMinusI(height) * direction.y, TimesMinusI(mirrorHeight) * mirrorDirection.y);
    return float4(foldedHeight.x - foldedX.y, foldedHeight.y + foldedX.x, foldedY);
}
