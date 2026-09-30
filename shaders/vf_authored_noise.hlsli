#ifndef SAMPLES_AUTHORED_NOISE
#define SAMPLES_AUTHORED_NOISE 0
#endif

static const bool kSamplesAuthoredNoise = SAMPLES_AUTHORED_NOISE;
static const int kNoisyLayers = 3;
static const int kRegistersPerLayerNoise = 4;
static const int kOctave0Register = 0;
static const int kOctave1Register = 1;
static const int kFadeRegister = 2;
static const int kOctaveWeightRegister = 3;
static const float kNoiseCurveContrast = 20;
static const float kNoiseCurvePivot = 0.5;
static const float kNoiseCurveShape = (kNoiseCurveContrast - 1) / (2 - kNoiseCurveContrast);

float4 cLayerNoise[kNoisyLayers * kRegistersPerLayerNoise] : register(c36);
sampler3D sAuthoredNoise : register(s10);

float4 LayerNoiseRegister(int layer, int registerInLayer)
{
    return layer == 0 ? cLayerNoise[registerInLayer]
         : layer == 1 ? cLayerNoise[kRegistersPerLayerNoise + registerInLayer]
                      : cLayerNoise[2 * kRegistersPerLayerNoise + registerInLayer];
}

float LayerNoiseAlpha(int layer)
{
    return layer < kNoisyLayers ? LayerNoiseRegister(layer, kFadeRegister).a : 0;
}

float3 LayerNoiseFade(int layer)
{
    return LayerNoiseRegister(layer, kFadeRegister).rgb;
}

bool AnyLayerNoise()
{
    return any(float3(cLayerNoise[kFadeRegister].a, cLayerNoise[kRegistersPerLayerNoise + kFadeRegister].a,
                      cLayerNoise[2 * kRegistersPerLayerNoise + kFadeRegister].a) > 0);
}

float NoiseOctaveSample(int layer, int octaveRegister, float3 worldPosition)
{
    float4 octave = LayerNoiseRegister(layer, octaveRegister);
    float3 tileCoordinate = frac((worldPosition - octave.xyz) * octave.w);
    return tex3Dlod(sAuthoredNoise, float4(tileCoordinate, 0)).r;
}

float LayerNoiseSample(int layer, float3 worldPosition)
{
    float2 weights = LayerNoiseRegister(layer, kOctaveWeightRegister).xy;
    return weights.x * NoiseOctaveSample(layer, kOctave0Register, worldPosition) +
           weights.y * NoiseOctaveSample(layer, kOctave1Register, worldPosition);
}

float NoiseCurveBelowPivot(float x)
{
    return x * x * (kNoiseCurveShape + 1) / (x + kNoiseCurveShape / 2);
}

float NoiseDensityCurve(float x)
{
    float nearest = min(x, 1 - x);
    float curve = NoiseCurveBelowPivot(nearest);
    return x < kNoiseCurvePivot ? curve : 1 - curve;
}

float LayerNoiseDensity(int layer, float3 worldPosition)
{
    return lerp(1, NoiseDensityCurve(saturate(LayerNoiseSample(layer, worldPosition))), LayerNoiseAlpha(layer));
}

float4 LayerNoiseDensities(float3 worldPosition)
{
    float4 densities = 1;
    [branch] if (!kSamplesAuthoredNoise || !AnyLayerNoise())
        return densities;
    [loop] for (int layer = 0; layer < kNoisyLayers; ++layer)
    {
        [branch] if (LayerNoiseAlpha(layer) > 0)
            densities = layer == int4(0, 1, 2, 3) ? LayerNoiseDensity(layer, worldPosition) : densities;
    }
    return densities;
}
